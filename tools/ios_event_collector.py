"""Host-side IOS P0.2 event collector prototype.

This module is an isolated test prototype, not a VGK kernel component and not a
security boundary. Source handles model a trusted registration API for tests;
Python object capabilities do not authenticate hostile processes or hardware.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import json
import math
import time
from types import MappingProxyType
import copy
import hashlib
import uuid
from typing import Callable, Deque, Mapping


_ALLOWED_SEVERITY = ("info", "warning", "error", "critical")
_ALLOWED_CONFIDENCE = ("confirmed", "suspected", "unknown")
_ALLOWED_PAYLOAD_TYPES = (str, int, float, bool, type(None))
_MAX_UINT64 = (1 << 64) - 1
_DEFAULT_MAX_SOURCES = 64
_DEFAULT_MAX_EVENT_CODES = 4096
_DEFAULT_MAX_EVENT_CODES_PER_SOURCE = 128
_MAX_QUEUE_CAPACITY = 4096
_MAX_DEDUP_CAPACITY = 8192
_MAX_SINK_RECORDS = 4096
_MAX_RECORD_BYTES = 8192
_MAX_EVENTS_PER_WINDOW = 10000
_MAX_RATE_WINDOW_SECONDS = 3600.0


class CollectorError(Exception):
    """Base error for rejected or unavailable collector operations."""


class InvalidEvent(CollectorError):
    """The event is malformed or outside the registered source policy."""


class QueueFull(CollectorError):
    """The bounded queue has no free slot."""


class SinkFull(CollectorError):
    """The memory-only test sink cannot accept another record."""


class SequenceExhausted(CollectorError):
    """The collector's unsigned 64-bit receipt sequence is exhausted."""


@dataclass(frozen=True)
class SourcePolicy:
    source_id: str
    source_version: str
    event_codes: Mapping[str, str]
    max_payload_bytes: int = 1024
    max_events_per_window: int = 100
    rate_window_seconds: float = 1.0


class SourceHandle:
    """Opaque-by-convention handle returned by this process-local test registry."""

    __slots__ = ("_nonce",)

    def __init__(self, nonce: object) -> None:
        self._nonce = nonce


class SourceRegistry:
    """Process-local allow-list; intentionally not a production authenticator."""

    def __init__(
        self,
        max_sources: int = _DEFAULT_MAX_SOURCES,
        max_event_codes: int = _DEFAULT_MAX_EVENT_CODES,
        max_event_codes_per_source: int = _DEFAULT_MAX_EVENT_CODES_PER_SOURCE,
    ) -> None:
        limits = (max_sources, max_event_codes, max_event_codes_per_source)
        if any(not isinstance(v, int) or isinstance(v, bool) or v < 1 for v in limits):
            raise ValueError("registry limits must be positive integers")
        if (
            max_sources > _DEFAULT_MAX_SOURCES
            or max_event_codes > _DEFAULT_MAX_EVENT_CODES
            or max_event_codes_per_source > _DEFAULT_MAX_EVENT_CODES_PER_SOURCE
        ):
            raise ValueError("registry limits may be lowered but not raised")
        if max_event_codes_per_source > max_event_codes:
            raise ValueError("per-source event-code limit cannot exceed total limit")
        self._max_sources = max_sources
        self._max_event_codes = max_event_codes
        self._max_event_codes_per_source = max_event_codes_per_source
        self._event_code_count = 0
        self._policies: dict[SourceHandle, SourcePolicy] = {}

    @property
    def source_count(self) -> int:
        return len(self._policies)

    @property
    def event_code_count(self) -> int:
        return self._event_code_count

    def register(self, policy: SourcePolicy) -> SourceHandle:
        if not isinstance(policy.source_id, str) or not policy.source_id or len(policy.source_id) > 64:
            raise ValueError("source_id must contain 1..64 characters")
        if not isinstance(policy.source_version, str) or not policy.source_version or len(policy.source_version) > 32:
            raise ValueError("source_version must contain 1..32 characters")
        if not isinstance(policy.event_codes, Mapping) or not policy.event_codes:
            raise ValueError("at least one event code must be registered")
        if len(policy.event_codes) > self._max_event_codes_per_source:
            raise ValueError("source exceeds the event-code limit")
        if self.source_count >= self._max_sources:
            raise ValueError("source registry capacity reached")
        if self._event_code_count + len(policy.event_codes) > self._max_event_codes:
            raise ValueError("event-code catalogue capacity reached")
        if (
            not isinstance(policy.max_payload_bytes, int)
            or isinstance(policy.max_payload_bytes, bool)
            or not 1 <= policy.max_payload_bytes <= 4096
        ):
            raise ValueError("max_payload_bytes must be between 1 and 4096")
        if (
            not isinstance(policy.max_events_per_window, int)
            or isinstance(policy.max_events_per_window, bool)
            or not 1 <= policy.max_events_per_window <= _MAX_EVENTS_PER_WINDOW
        ):
            raise ValueError(
                f"max_events_per_window must be between 1 and {_MAX_EVENTS_PER_WINDOW}"
            )
        if (
            not isinstance(policy.rate_window_seconds, (int, float))
            or isinstance(policy.rate_window_seconds, bool)
            or not math.isfinite(policy.rate_window_seconds)
            or not 0 < policy.rate_window_seconds <= _MAX_RATE_WINDOW_SECONDS
        ):
            raise ValueError(
                f"rate_window_seconds must be finite and at most {_MAX_RATE_WINDOW_SECONDS}"
            )
        for code, severity in policy.event_codes.items():
            if not isinstance(code, str) or not code or len(code) > 64 or not isinstance(severity, str) or severity not in _ALLOWED_SEVERITY:
                raise ValueError("invalid event code or maximum severity")
        if any(existing.source_id == policy.source_id for existing in self._policies.values()):
            raise ValueError("source_id is already registered")
        frozen_policy = SourcePolicy(
            source_id=policy.source_id,
            source_version=policy.source_version,
            event_codes=MappingProxyType(dict(policy.event_codes)),
            max_payload_bytes=policy.max_payload_bytes,
            max_events_per_window=policy.max_events_per_window,
            rate_window_seconds=policy.rate_window_seconds,
        )
        handle = SourceHandle(object())
        self._policies[handle] = frozen_policy
        self._event_code_count += len(frozen_policy.event_codes)
        return handle

    def policy_for(self, handle: SourceHandle) -> SourcePolicy:
        try:
            return self._policies[handle]
        except (KeyError, TypeError):
            raise InvalidEvent("source handle is not registered") from None


def _consume_utf8_budget(value: str, budget: list[int]) -> None:
    """Count UTF-8 bytes without allocating an encoded copy of untrusted text."""
    for character in value:
        codepoint = ord(character)
        budget[0] -= 1 if codepoint <= 0x7F else 2 if codepoint <= 0x7FF else 3 if codepoint <= 0xFFFF else 4
        if budget[0] < 0:
            raise ValueError(f"sink record exceeds {_MAX_RECORD_BYTES} bytes")


def _preflight_sink_value(value: object, depth: int, budget: list[int]) -> None:
    """Bound generic JSON structure before serialization can allocate a large buffer."""
    budget[1] -= 1
    if budget[1] < 0 or depth > 8:
        raise ValueError("sink record exceeds structural limits")
    if isinstance(value, dict):
        if len(value) > 32:
            raise ValueError("sink record exceeds structural limits")
        for key, item in value.items():
            if not isinstance(key, str):
                raise ValueError("sink record must contain finite JSON data")
            budget[0] -= len(key.encode("utf-8"))
            if budget[0] < 0:
                raise ValueError(f"sink record exceeds {_MAX_RECORD_BYTES} bytes")
            _preflight_sink_value(item, depth + 1, budget)
    elif isinstance(value, list):
        if len(value) > 32:
            raise ValueError("sink record exceeds structural limits")
        for item in value:
            _preflight_sink_value(item, depth + 1, budget)
    elif isinstance(value, str):
        budget[0] -= len(value.encode("utf-8"))
        if budget[0] < 0:
            raise ValueError(f"sink record exceeds {_MAX_RECORD_BYTES} bytes")
    elif value is None or isinstance(value, bool):
        return
    elif isinstance(value, int):
        if value.bit_length() > 64:
            raise ValueError("sink record integer exceeds 64-bit range")
    elif isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("sink record must contain finite JSON data")
    else:
        raise ValueError("sink record must contain finite JSON data")


class MemorySink:
    """Bounded, volatile test receiver; records disappear when the process exits."""

    def __init__(self, max_records: int = 1024) -> None:
        if (
            not isinstance(max_records, int)
            or isinstance(max_records, bool)
            or max_records < 1
        ):
            raise ValueError("max_records must be a positive integer")
        if max_records > _MAX_SINK_RECORDS:
            raise ValueError(f"max_records cannot exceed {_MAX_SINK_RECORDS}")
        self._records: list[dict] = []
        self._max_records = max_records

    @property
    def records(self) -> tuple[dict, ...]:
        return tuple(copy.deepcopy(self._records))

    def append(self, record: dict) -> None:
        if len(self._records) >= self._max_records:
            raise SinkFull("memory sink capacity reached")
        if not isinstance(record, dict):
            raise ValueError("sink record must be a dictionary")
        # Preflight bounds strings, containers, nesting and scalar types
        # before JSON serialization allocates its temporary output buffer.
        _preflight_sink_value(record, 0, [_MAX_RECORD_BYTES, 256])
        try:
            encoded = json.dumps(
                record, ensure_ascii=False, separators=(",", ":"), allow_nan=False
            ).encode("utf-8")
        except (TypeError, ValueError):
            raise ValueError("sink record must contain finite JSON data") from None
        if len(encoded) > _MAX_RECORD_BYTES:
            raise ValueError(f"sink record exceeds {_MAX_RECORD_BYTES} bytes")
        self._records.append(copy.deepcopy(record))


def _validate_payload(value: object, depth: int = 0) -> object:
    if depth > 4:
        raise InvalidEvent("payload nesting exceeds four levels")
    if isinstance(value, dict):
        if len(value) > 32:
            raise InvalidEvent("payload object has too many fields")
        clean: dict[str, object] = {}
        for key, item in value.items():
            if not isinstance(key, str) or not key or len(key) > 64:
                raise InvalidEvent("payload keys must be short non-empty strings")
            clean[key] = _validate_payload(item, depth + 1)
        return clean
    if isinstance(value, list):
        if len(value) > 32:
            raise InvalidEvent("payload array has too many items")
        return [_validate_payload(item, depth + 1) for item in value]
    if isinstance(value, _ALLOWED_PAYLOAD_TYPES):
        if isinstance(value, str) and len(value) > 256:
            raise InvalidEvent("payload string exceeds 256 characters")
        if isinstance(value, float):
            # json.dumps with allow_nan=False performs the finite-value check.
            return value
        return value
    raise InvalidEvent("payload contains a non-JSON value")


class EventCollector:
    """Bounded in-memory queue with source policy, validation and deduplication."""

    def __init__(
        self,
        registry: SourceRegistry,
        capacity: int = 64,
        dedup_capacity: int = 256,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        if (
            not isinstance(capacity, int)
            or isinstance(capacity, bool)
            or capacity < 1
            or not isinstance(dedup_capacity, int)
            or isinstance(dedup_capacity, bool)
            or dedup_capacity < 1
        ):
            raise ValueError("capacities must be positive integers")
        if capacity > _MAX_QUEUE_CAPACITY:
            raise ValueError(f"queue capacity cannot exceed {_MAX_QUEUE_CAPACITY}")
        if dedup_capacity > _MAX_DEDUP_CAPACITY:
            raise ValueError(f"dedup capacity cannot exceed {_MAX_DEDUP_CAPACITY}")
        self._registry = registry
        self._queue: Deque[dict] = deque()
        self._capacity = capacity
        self._dedup_capacity = dedup_capacity
        self._dedup_index: dict[tuple[str, ...], str] = {}
        self._dedup_order: Deque[tuple[str, ...]] = deque()
        self._clock = clock
        self._rate_state: dict[str, tuple[float, int]] = {}
        self._sequence = 0
        self._boot_session_id = str(uuid.uuid4())
        self._dropped = 0

    @property
    def queue_size(self) -> int:
        return len(self._queue)

    @property
    def dropped_count(self) -> int:
        return self._dropped

    @property
    def next_sequence(self) -> int:
        return self._sequence + 1

    def submit(self, handle: SourceHandle, message: Mapping[str, object]) -> bool:
        policy = self._registry.policy_for(handle)
        if not isinstance(message, Mapping):
            raise InvalidEvent("event must be a mapping")
        allowed_fields = {
            "schema_version", "source_sequence", "event_code", "category",
            "severity", "confidence", "payload",
        }
        # Reject oversized mappings before iterating their keys. The schema
        # has only seven permitted top-level fields, so no valid event can
        # require a larger mapping.
        if len(message) > len(allowed_fields):
            raise InvalidEvent("event contains too many fields")
        if set(message) - allowed_fields:
            raise InvalidEvent("event contains unknown fields")
        schema_version = message.get("schema_version")
        if not isinstance(schema_version, int) or isinstance(schema_version, bool) or schema_version != 1:
            raise InvalidEvent("unsupported schema_version")
        code = message.get("event_code")
        category = message.get("category")
        severity = message.get("severity")
        confidence = message.get("confidence")
        if not isinstance(code, str) or code not in policy.event_codes:
            raise InvalidEvent("event_code is not registered for this source")
        if not isinstance(category, str) or category not in {
            "kernel", "cpu", "memory", "device", "storage", "integrity",
            "security", "service", "recovery",
        }:
            raise InvalidEvent("category is not supported")
        if severity not in _ALLOWED_SEVERITY:
            raise InvalidEvent("invalid severity")
        if _ALLOWED_SEVERITY.index(severity) > _ALLOWED_SEVERITY.index(policy.event_codes[code]):
            raise InvalidEvent("source attempted to exceed its maximum severity")
        if confidence not in _ALLOWED_CONFIDENCE:
            raise InvalidEvent("invalid confidence")

        source_sequence = message.get("source_sequence")
        if source_sequence is not None and (
            not isinstance(source_sequence, int) or isinstance(source_sequence, bool)
            or source_sequence < 0
            or source_sequence > _MAX_UINT64
        ):
            raise InvalidEvent("source_sequence must be an unsigned 64-bit integer")

        payload = _validate_payload(message.get("payload", {}))
        try:
            encoded_payload = json.dumps(
                payload, ensure_ascii=False, separators=(",", ":"), allow_nan=False
            ).encode("utf-8")
        except (TypeError, ValueError):
            raise InvalidEvent("payload is not valid finite JSON data") from None
        if len(encoded_payload) > policy.max_payload_bytes:
            raise InvalidEvent("payload exceeds the source byte limit")

        # A fingerprint is only a hint for review, never a reason to discard
        # an observation. Include all validated event content, but not local
        # receipt sequence/time or the optional source sequence.
        fingerprint_input = {
            "source_version": policy.source_version,
            "event_code": code,
            "category": category,
            "severity": severity,
            "confidence": confidence,
            "payload": payload,
        }
        canonical = json.dumps(
            fingerprint_input, ensure_ascii=False, sort_keys=True,
            separators=(",", ":"), allow_nan=False,
        ).encode("utf-8")
        fingerprint = hashlib.sha256(canonical).hexdigest()
        content_key = (
            "content", policy.source_id, self._boot_session_id, fingerprint
        )
        sequence_key = (
            "sequence", policy.source_id, self._boot_session_id, code,
            str(source_sequence),
        ) if source_sequence is not None else None

        prior_event_id = (
            self._dedup_index.get(sequence_key) if sequence_key is not None else None
        )
        detection_status = "first_seen"
        if prior_event_id is not None:
            detection_status = "source_sequence_reused"
        elif source_sequence is None:
            prior_event_id = self._dedup_index.get(content_key)
            if prior_event_id is not None:
                detection_status = "content_match"

        now = self._clock()
        window_start, count = self._rate_state.get(policy.source_id, (now, 0))
        if now - window_start >= policy.rate_window_seconds:
            window_start, count = now, 0
        if count >= policy.max_events_per_window:
            self._dropped = min(self._dropped + 1, _MAX_UINT64)
            raise QueueFull("source rate limit reached")
        if len(self._queue) >= self._capacity:
            self._dropped = min(self._dropped + 1, _MAX_UINT64)
            raise QueueFull("collector queue capacity reached")

        if self._sequence >= _MAX_UINT64:
            raise SequenceExhausted("collector receipt sequence exhausted")
        self._sequence += 1
        record = {
            "schema_version": 1,
            "event_id": str(uuid.uuid4()),
            "sequence": self._sequence,
            "boot_session_id": self._boot_session_id,
            "event_type": "observation",
            "source": {
                "source_id": policy.source_id,
                "source_version": policy.source_version,
            },
            "source_sequence": source_sequence,
            "duplicate_detection": {
                "status": detection_status,
                "possible_match_event_id": prior_event_id,
                "fingerprint": fingerprint,
            },
            "event_code": code,
            "category": category,
            "severity": severity,
            "confidence": confidence,
            "timestamp": None,
            "monotonic_time": self._clock(),
            "time_status": "unavailable",
            "payload": payload,
        }
        self._queue.append(record)
        self._rate_state[policy.source_id] = (window_start, count + 1)
        # Index both forms, but retain every accepted observation. The bounded
        # index only limits how far back a possible match can be found.
        # Use exactly one index key per accepted event. Sequenced sources are
        # compared by their sequence; content matching is reserved for sources
        # that do not provide a source sequence.
        keys_to_remember = [sequence_key] if sequence_key is not None else [content_key]
        for key in keys_to_remember:
            if key not in self._dedup_index:
                self._dedup_index[key] = record["event_id"]
                self._dedup_order.append(key)
                if len(self._dedup_order) > self._dedup_capacity:
                    expired = self._dedup_order.popleft()
                    self._dedup_index.pop(expired, None)
        return True

    def flush_to(self, sink: MemorySink, limit: int | None = None) -> int:
        """Transfer records in order; retain the head record if the sink rejects it."""
        if limit is not None and (
            not isinstance(limit, int) or isinstance(limit, bool) or limit < 0
        ):
            raise ValueError("limit must be a non-negative integer or None")
        transferred = 0
        while self._queue and (limit is None or transferred < limit):
            record = self._queue[0]
            sink.append(record)
            self._queue.popleft()
            transferred += 1
        return transferred
