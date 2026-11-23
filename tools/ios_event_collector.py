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
import uuid
from typing import Callable, Deque, Mapping


_ALLOWED_SEVERITY = ("info", "warning", "error", "critical")
_ALLOWED_CONFIDENCE = ("confirmed", "suspected", "unknown")
_ALLOWED_PAYLOAD_TYPES = (str, int, float, bool, type(None))


class CollectorError(Exception):
    """Base error for rejected or unavailable collector operations."""


class InvalidEvent(CollectorError):
    """The event is malformed or outside the registered source policy."""


class QueueFull(CollectorError):
    """The bounded queue has no free slot."""


class SinkFull(CollectorError):
    """The memory-only test sink cannot accept another record."""


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

    def __init__(self) -> None:
        self._policies: dict[SourceHandle, SourcePolicy] = {}

    def register(self, policy: SourcePolicy) -> SourceHandle:
        if not isinstance(policy.source_id, str) or not policy.source_id or len(policy.source_id) > 64:
            raise ValueError("source_id must contain 1..64 characters")
        if not isinstance(policy.source_version, str) or not policy.source_version or len(policy.source_version) > 32:
            raise ValueError("source_version must contain 1..32 characters")
        if not isinstance(policy.event_codes, Mapping) or not policy.event_codes:
            raise ValueError("at least one event code must be registered")
        if (
            not isinstance(policy.max_payload_bytes, int)
            or isinstance(policy.max_payload_bytes, bool)
            or not 1 <= policy.max_payload_bytes <= 4096
        ):
            raise ValueError("max_payload_bytes must be between 1 and 4096")
        if (
            not isinstance(policy.max_events_per_window, int)
            or isinstance(policy.max_events_per_window, bool)
            or policy.max_events_per_window < 1
        ):
            raise ValueError("max_events_per_window must be positive")
        if (
            not isinstance(policy.rate_window_seconds, (int, float))
            or isinstance(policy.rate_window_seconds, bool)
            or not math.isfinite(policy.rate_window_seconds)
            or policy.rate_window_seconds <= 0
        ):
            raise ValueError("rate_window_seconds must be finite and positive")
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
        return handle

    def policy_for(self, handle: SourceHandle) -> SourcePolicy:
        try:
            return self._policies[handle]
        except (KeyError, TypeError):
            raise InvalidEvent("source handle is not registered") from None


class MemorySink:
    """Bounded, volatile test receiver; records disappear when the process exits."""

    def __init__(self, max_records: int = 1024) -> None:
        if max_records < 1:
            raise ValueError("max_records must be positive")
        self._records: list[dict] = []
        self._max_records = max_records

    @property
    def records(self) -> tuple[dict, ...]:
        return tuple(copy.deepcopy(self._records))

    def append(self, record: dict) -> None:
        if len(self._records) >= self._max_records:
            raise SinkFull("memory sink capacity reached")
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
        if capacity < 1 or dedup_capacity < 1:
            raise ValueError("capacities must be positive")
        self._registry = registry
        self._queue: Deque[dict] = deque()
        self._capacity = capacity
        self._dedup_capacity = dedup_capacity
        self._seen: set[tuple[str, str, str, int]] = set()
        self._seen_order: Deque[tuple[str, str, str, int]] = deque()
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
        ):
            raise InvalidEvent("source_sequence must be a non-negative integer")

        payload = _validate_payload(message.get("payload", {}))
        try:
            encoded_payload = json.dumps(
                payload, ensure_ascii=False, separators=(",", ":"), allow_nan=False
            ).encode("utf-8")
        except (TypeError, ValueError):
            raise InvalidEvent("payload is not valid finite JSON data") from None
        if len(encoded_payload) > policy.max_payload_bytes:
            raise InvalidEvent("payload exceeds the source byte limit")

        dedup_key = (
            (policy.source_id, self._boot_session_id, code, source_sequence)
            if source_sequence is not None else None
        )
        if dedup_key is not None and dedup_key in self._seen:
            return False

        now = self._clock()
        window_start, count = self._rate_state.get(policy.source_id, (now, 0))
        if now - window_start >= policy.rate_window_seconds:
            window_start, count = now, 0
        if count >= policy.max_events_per_window:
            self._dropped += 1
            raise QueueFull("source rate limit reached")
        if len(self._queue) >= self._capacity:
            self._dropped += 1
            raise QueueFull("collector queue capacity reached")

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
        if dedup_key is not None:
            self._seen.add(dedup_key)
            self._seen_order.append(dedup_key)
            if len(self._seen_order) > self._dedup_capacity:
                expired = self._seen_order.popleft()
                self._seen.discard(expired)
        return True

    def flush_to(self, sink: MemorySink, limit: int | None = None) -> int:
        """Transfer records in order; retain the head record if the sink rejects it."""
        transferred = 0
        while self._queue and (limit is None or transferred < limit):
            record = self._queue[0]
            sink.append(record)
            self._queue.popleft()
            transferred += 1
        return transferred
