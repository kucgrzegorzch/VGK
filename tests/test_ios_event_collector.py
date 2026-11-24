import unittest

from tools.ios_event_collector import (
    EventCollector,
    InvalidEvent,
    MemorySink,
    QueueFull,
    SequenceExhausted,
    SinkFull,
    SourcePolicy,
    SourceRegistry,
)


class FakeClock:
    def __init__(self):
        self.value = 10.0

    def __call__(self):
        return self.value

    def advance(self, seconds):
        self.value += seconds


def make_message(**overrides):
    message = {
        "schema_version": 1,
        "source_sequence": 1,
        "event_code": "MEMORY_PRESSURE",
        "category": "memory",
        "severity": "warning",
        "confidence": "confirmed",
        "payload": {"free_pages": 12},
    }
    message.update(overrides)
    return message


class SourceRegistryLimitTests(unittest.TestCase):
    def test_registry_limits_are_validated(self):
        for limits in ((0, 1, 1), (1, 0, 1), (1, 1, 0), (1, 1, 2),
                       (True, 1, 1), (1, 1.5, 1),
                       (65, 4096, 128), (64, 4097, 128),
                       (64, 4096, 129)):
            with self.subTest(limits=limits):
                with self.assertRaises(ValueError):
                    SourceRegistry(*limits)

    def test_source_count_limit_accepts_boundary_and_rejects_next_source(self):
        registry = SourceRegistry(max_sources=2, max_event_codes=4,
                                  max_event_codes_per_source=2)
        registry.register(SourcePolicy("source.one", "1", {"A": "info"}))
        registry.register(SourcePolicy("source.two", "1", {"B": "info"}))
        with self.assertRaisesRegex(ValueError, "source registry capacity"):
            registry.register(SourcePolicy("source.three", "1", {"C": "info"}))
        self.assertEqual(registry.source_count, 2)
        self.assertEqual(registry.event_code_count, 2)

    def test_per_source_event_code_limit_accepts_boundary_and_rejects_next(self):
        registry = SourceRegistry(max_sources=2, max_event_codes=4,
                                  max_event_codes_per_source=2)
        registry.register(SourcePolicy("source.one", "1",
                                        {"A": "info", "B": "warning"}))
        with self.assertRaisesRegex(ValueError, "event-code limit"):
            registry.register(SourcePolicy("source.two", "1",
                                            {"C": "info", "D": "warning",
                                             "E": "error"}))
        self.assertEqual(registry.source_count, 1)
        self.assertEqual(registry.event_code_count, 2)

    def test_total_event_code_catalogue_limit_is_enforced(self):
        registry = SourceRegistry(max_sources=3, max_event_codes=3,
                                  max_event_codes_per_source=2)
        registry.register(SourcePolicy("source.one", "1",
                                        {"A": "info", "B": "warning"}))
        registry.register(SourcePolicy("source.two", "1", {"C": "info"}))
        with self.assertRaisesRegex(ValueError, "catalogue capacity"):
            registry.register(SourcePolicy("source.three", "1", {"D": "info"}))
        self.assertEqual(registry.source_count, 2)
        self.assertEqual(registry.event_code_count, 3)


class EventCollectorTests(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.registry = SourceRegistry()
        self.handle = self.registry.register(
            SourcePolicy(
                source_id="test.memory",
                source_version="1",
                event_codes={"MEMORY_PRESSURE": "warning", "MEMORY_RECOVERY": "info"},
                max_payload_bytes=128,
                max_events_per_window=10,
                rate_window_seconds=1.0,
            )
        )
        self.collector = EventCollector(
            self.registry, capacity=2, dedup_capacity=4, clock=self.clock
        )

    def test_collector_rejects_non_integer_or_non_positive_capacities(self):
        for capacity, dedup_capacity in (
            (0, 1), (-1, 1), (True, 1), (1.5, 1), (1, False), (1, 0),
        ):
            with self.subTest(capacity=capacity, dedup_capacity=dedup_capacity):
                with self.assertRaises(ValueError):
                    EventCollector(self.registry, capacity, dedup_capacity)

    def test_memory_sink_rejects_non_integer_or_non_positive_capacity(self):
        for capacity in (0, -1, True, 1.5):
            with self.subTest(capacity=capacity):
                with self.assertRaises(ValueError):
                    MemorySink(max_records=capacity)

    def test_flush_limit_must_be_a_non_negative_integer(self):
        for limit in (-1, True, 1.5):
            with self.subTest(limit=limit):
                with self.assertRaises(ValueError):
                    self.collector.flush_to(MemorySink(), limit=limit)
        self.assertEqual(self.collector.flush_to(MemorySink(), limit=0), 0)

    def test_accepts_registered_event_and_assigns_canonical_sequence(self):
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        sink = MemorySink()
        self.assertEqual(self.collector.flush_to(sink), 1)
        record = sink.records[0]
        self.assertEqual(record["sequence"], 1)
        self.assertEqual(record["event_type"], "observation")
        self.assertEqual(record["source"]["source_id"], "test.memory")
        self.assertIsNone(record["timestamp"])
        self.assertEqual(record["time_status"], "unavailable")
        self.assertEqual(record["duplicate_detection"]["status"], "first_seen")
        self.assertIsInstance(record["duplicate_detection"]["fingerprint"], str)
        self.assertEqual(len(record["duplicate_detection"]["fingerprint"]), 64)
        self.assertIsInstance(record["monotonic_time"], float)

    def test_rejects_unregistered_handle(self):
        foreign = self.registry.register(
            SourcePolicy("other.source", "1", {"OTHER": "info"})
        )
        with self.assertRaises(InvalidEvent):
            self.collector.submit(object(), make_message())
        self.assertTrue(self.collector.submit(foreign, {
            "schema_version": 1,
            "event_code": "OTHER",
            "category": "kernel",
            "severity": "info",
            "confidence": "unknown",
            "payload": {},
        }))

    def test_source_cannot_escalate_severity(self):
        with self.assertRaises(InvalidEvent):
            self.collector.submit(self.handle, make_message(severity="critical"))
        self.assertEqual(self.collector.queue_size, 0)

    def test_unknown_fields_and_unregistered_event_codes_are_rejected(self):
        with self.assertRaises(InvalidEvent):
            self.collector.submit(self.handle, make_message(source_id="forged"))
        with self.assertRaises(InvalidEvent):
            self.collector.submit(self.handle, make_message(event_code="NOT_ALLOWED"))

    def test_repeated_source_sequence_is_flagged_but_never_discarded(self):
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        self.assertEqual(self.collector.queue_size, 2)
        sink = MemorySink()
        self.assertEqual(self.collector.flush_to(sink), 2)
        first, second = sink.records
        self.assertEqual(first["duplicate_detection"]["status"], "first_seen")
        self.assertEqual(
            second["duplicate_detection"]["status"], "source_sequence_reused"
        )
        self.assertEqual(
            second["duplicate_detection"]["possible_match_event_id"],
            first["event_id"],
        )
        self.assertEqual(self.collector.next_sequence, 3)

    def test_same_source_sequence_with_different_content_is_preserved_and_flagged(self):
        self.collector.submit(self.handle, make_message())
        changed = make_message(payload={"free_pages": 99})
        self.assertTrue(self.collector.submit(self.handle, changed))
        sink = MemorySink()
        self.collector.flush_to(sink)
        self.assertEqual(len(sink.records), 2)
        self.assertEqual(
            sink.records[1]["duplicate_detection"]["status"],
            "source_sequence_reused",
        )

    def test_distinct_source_sequences_with_same_content_are_not_content_matches(self):
        self.assertTrue(self.collector.submit(
            self.handle, make_message(source_sequence=10)
        ))
        self.assertTrue(self.collector.submit(
            self.handle, make_message(source_sequence=11)
        ))
        sink = MemorySink()
        self.collector.flush_to(sink)
        self.assertEqual(len(sink.records), 2)
        self.assertEqual(
            sink.records[1]["duplicate_detection"]["status"], "first_seen"
        )
        self.assertIsNone(
            sink.records[1]["duplicate_detection"]["possible_match_event_id"]
        )

    def test_unsequenced_exact_content_match_is_flagged_but_preserved(self):
        message = make_message(source_sequence=None)
        self.assertTrue(self.collector.submit(self.handle, message))
        self.assertTrue(self.collector.submit(self.handle, message))
        self.assertEqual(self.collector.queue_size, 2)
        sink = MemorySink()
        self.collector.flush_to(sink)
        self.assertEqual(
            sink.records[1]["duplicate_detection"]["status"], "content_match"
        )
        self.assertEqual(
            sink.records[1]["duplicate_detection"]["possible_match_event_id"],
            sink.records[0]["event_id"],
        )

    def test_unsequenced_different_content_is_not_flagged_as_match(self):
        self.collector.submit(
            self.handle, make_message(source_sequence=None, payload={"free_pages": 12})
        )
        self.collector.submit(
            self.handle, make_message(source_sequence=None, payload={"free_pages": 11})
        )
        sink = MemorySink()
        self.collector.flush_to(sink)
        self.assertEqual(
            sink.records[1]["duplicate_detection"]["status"], "first_seen"
        )

    def test_same_source_sequence_with_different_event_code_is_not_deduplicated(self):
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        recovery = make_message(
            event_code="MEMORY_RECOVERY",
            severity="info",
            source_sequence=1,
        )
        self.assertTrue(self.collector.submit(self.handle, recovery))
        self.assertEqual(self.collector.queue_size, 2)

    def test_same_event_from_different_source_is_not_deduplicated(self):
        other_handle = self.registry.register(
            SourcePolicy("test.memory.secondary", "1", {"MEMORY_PRESSURE": "warning"})
        )
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        self.assertTrue(self.collector.submit(other_handle, make_message()))
        self.assertEqual(self.collector.queue_size, 2)

    def test_new_boot_session_has_an_independent_deduplication_scope(self):
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        next_session = EventCollector(self.registry, capacity=2, clock=self.clock)
        self.assertTrue(next_session.submit(self.handle, make_message()))
        self.assertNotEqual(
            self.collector._boot_session_id, next_session._boot_session_id
        )

    def test_bounded_queue_reports_overflow_without_growing(self):
        self.collector.submit(self.handle, make_message(source_sequence=1))
        self.collector.submit(self.handle, make_message(source_sequence=2))
        with self.assertRaises(QueueFull):
            self.collector.submit(self.handle, make_message(source_sequence=3))
        self.assertEqual(self.collector.queue_size, 2)
        self.assertEqual(self.collector.dropped_count, 1)

    def test_source_sequence_is_limited_to_unsigned_64_bit_range(self):
        self.assertTrue(self.collector.submit(
            self.handle, make_message(source_sequence=(1 << 64) - 1)
        ))
        for invalid in (-1, 1 << 64, True):
            with self.subTest(source_sequence=invalid):
                with self.assertRaises(InvalidEvent):
                    self.collector.submit(
                        self.handle, make_message(source_sequence=invalid)
                    )

    def test_collector_receipt_sequence_does_not_wrap(self):
        self.collector._sequence = (1 << 64) - 1
        with self.assertRaises(SequenceExhausted):
            self.collector.submit(self.handle, make_message())
        self.assertEqual(self.collector.queue_size, 0)
        self.assertEqual(self.collector.next_sequence, 1 << 64)

    def test_payload_size_is_bounded(self):
        with self.assertRaises(InvalidEvent):
            self.collector.submit(
                self.handle, make_message(payload={"text": "x" * 200})
            )

    def test_rate_limit_is_enforced_and_resets_after_window(self):
        limited_registry = SourceRegistry()
        limited_handle = limited_registry.register(
            SourcePolicy(
                "limited.source", "1", {"PING": "info"},
                max_events_per_window=1, rate_window_seconds=1.0
            )
        )
        limited = EventCollector(limited_registry, capacity=4, clock=self.clock)
        msg = {
            "schema_version": 1, "event_code": "PING", "category": "service",
            "severity": "info", "confidence": "unknown", "payload": {},
        }
        limited.submit(limited_handle, msg)
        with self.assertRaises(QueueFull):
            limited.submit(limited_handle, msg)
        self.clock.advance(1.1)
        self.assertTrue(limited.submit(limited_handle, msg))

    def test_sink_failure_does_not_discard_queued_record(self):
        self.collector.submit(self.handle, make_message())
        tiny_sink = MemorySink(max_records=1)
        tiny_sink.append({"occupied": True})
        with self.assertRaises(SinkFull):
            self.collector.flush_to(tiny_sink)
        self.assertEqual(self.collector.queue_size, 1)
        sink = MemorySink()
        self.assertEqual(self.collector.flush_to(sink), 1)
        self.assertEqual(sink.records[0]["sequence"], 1)

    def test_schema_version_rejects_boolean_and_float_aliases(self):
        with self.assertRaises(InvalidEvent):
            self.collector.submit(self.handle, make_message(schema_version=True))
        with self.assertRaises(InvalidEvent):
            self.collector.submit(self.handle, make_message(schema_version=1.0))

    def test_registered_policy_is_not_changed_by_caller_mapping_mutation(self):
        registry = SourceRegistry()
        codes = {"PING": "info"}
        handle = registry.register(SourcePolicy("immutable.source", "1", codes))
        codes["PING"] = "critical"
        collector = EventCollector(registry, clock=self.clock)
        message = {
            "schema_version": 1, "event_code": "PING", "category": "service",
            "severity": "critical", "confidence": "confirmed", "payload": {},
        }
        with self.assertRaises(InvalidEvent):
            collector.submit(handle, message)

    def test_records_are_flushed_in_sequence_order(self):
        self.collector.submit(self.handle, make_message(source_sequence=1))
        self.collector.submit(self.handle, make_message(source_sequence=2))
        sink = MemorySink()
        self.assertEqual(self.collector.flush_to(sink, limit=1), 1)
        self.assertEqual(self.collector.queue_size, 1)
        self.assertEqual(self.collector.flush_to(sink), 1)
        self.assertEqual([r["sequence"] for r in sink.records], [1, 2])


if __name__ == "__main__":
    unittest.main()
