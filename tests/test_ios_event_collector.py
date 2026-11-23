import unittest

from tools.ios_event_collector import (
    EventCollector,
    InvalidEvent,
    MemorySink,
    QueueFull,
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

    def test_duplicate_source_sequence_is_ignored(self):
        self.assertTrue(self.collector.submit(self.handle, make_message()))
        self.assertFalse(self.collector.submit(self.handle, make_message()))
        self.assertEqual(self.collector.queue_size, 1)
        self.assertEqual(self.collector.next_sequence, 2)

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
