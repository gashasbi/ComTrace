# Comparison JSON Report

## Command

Generate a JSON report by adding `--json` to a comparison:

```text
ComTrace.exe --compare before.etl after.ctra --json
```

Both inputs may be ETL or CTRA files. The report is written to standard output. A successful comparison exits with code 0; a failed input or comparison returns a nonzero exit code.

## Schema version

The top-level `schema_version` field is currently `1`. Consumers should check this value before interpreting the report. New fields may be added without changing the schema version when the existing field meanings and types remain compatible.

## Top-level fields

| Field | Type | Description |
|---|---|---|
| `schema_version` | number | JSON report schema version |
| `left` | object | Summary of the first input |
| `right` | object | Summary of the second input |
| `summary` | object | Event-type and COM-object change totals |
| `timeline_summary` | object | COM and OLE timeline totals for both inputs |
| `event_type_changes` | array | Event types added, removed, or with changed counts |
| `object_changes` | array | COM objects added, removed, or with changed identity fields |
| `timeline` | object | Per-operation timing summaries for each input |

Each input object contains:

| Field | Type | Description |
|---|---|---|
| `path` | string | Input file path supplied to the command |
| `events` | number | Total events read from the input |
| `event_types` | number | Number of distinct provider/event-descriptor combinations |

The `summary` object contains numeric totals:

- `event_types_added`: event types present only in the right input.
- `event_types_removed`: event types present only in the left input.
- `event_types_count_changed`: event types present in both inputs whose event counts differ.
- `objects_added`, `objects_removed`, and `objects_changed`: COM-object change totals.

## Event type changes

Each `event_type_changes` entry contains the complete event identity and both input counts.

| Field | Type | Description |
|---|---|---|
| `provider_id` | string | Provider GUID |
| `event_id` | number | `EVENT_DESCRIPTOR.Id` |
| `version` | number | `EVENT_DESCRIPTOR.Version` |
| `channel` | number | `EVENT_DESCRIPTOR.Channel` |
| `level` | number | `EVENT_DESCRIPTOR.Level` |
| `opcode` | number | `EVENT_DESCRIPTOR.Opcode` |
| `task` | number | `EVENT_DESCRIPTOR.Task` |
| `keyword` | string | `EVENT_DESCRIPTOR.Keyword` as a hexadecimal string with a `0x` prefix |
| `left_count` | number | Number of matching events in the left input |
| `right_count` | number | Number of matching events in the right input |

The event identity uses the provider GUID and all fields of the Windows event descriptor. Counts are zero on the side where an event type is absent. The keyword is a string so a 64-bit value is not silently rounded by JSON consumers that represent all numbers as IEEE-754 doubles.

## COM object changes

Each `object_changes` entry contains:

| Field | Type | Description |
|---|---|---|
| `process_id` | number | Process ID associated with the object |
| `oid` | string | Object ID as a hexadecimal string with a `0x` prefix |
| `left_present` | boolean | Whether the object is present in the left input |
| `right_present` | boolean | Whether the object is present in the right input |
| `changed_fields` | string | Hexadecimal bit mask of changed identity fields |

The `changed_fields` bits are:

| Bit | Meaning |
|---:|---|
| `0x1` | CLSID changed |
| `0x2` | IID set changed |
| `0x4` | IPID set changed |

Added or removed objects have one presence flag set to false and a zero changed-fields mask. Objects are matched by process ID and OID. Identity comparison reports only fields available from the trace schema; missing values are not guessed from arbitrary payload bytes. The inspect graph also records the number of observations and the minimum/maximum raw ETW clock timestamps for each observed COM object and IPID. These bounds are observation times, not object creation or release times. The standard capture does not enable kernel process start/exit events, and the currently mapped COM events do not provide a verified object-release signal. Consequently, a reused numeric PID or a reused OID within one process cannot yet be separated into distinct object generations; matching remains scoped to process ID and OID.

## Timeline summaries

The `timeline_summary` object contains `left_com`, `right_com`, `left_ole`, and `right_ole`.

COM timeline entries contain numeric `matched`, `matched_by_call_trace_id`, `matched_by_activity_id`, `matched_by_process_thread`, `incomplete`, `unmatched`, `missing_call_trace_ids`, `missing_activity_ids`, and `failed` counts. All three `matched_by_*` values are subsets of `matched`. The missing-ID values count COM start/stop event records, not calls.

ComTrace first matches events that both expose the same `CallTraceId`, process ID, thread ID, and operation. When both events lack `CallTraceId`, it uses an identical non-empty `ActivityId` with the same process, thread, and operation. When both events lack both identifiers, it can use the same process ID, thread ID, and operation as a weaker fallback. This third method is reported separately as `matched_by_process_thread`; it is not equivalent in confidence to an identifier-based match. Pending starts are paired in last-in-first-out order to preserve nested call order. If only one event has `CallTraceId` or `ActivityId`, or any available identifier differs, the events are not paired. Related Activity IDs are retained as context but are not used as a call key.

OLE timeline entries contain numeric `matched`, `matched_by_activity_id`, `matched_by_process_thread`, `incomplete`, `unmatched`, `activity_sequences`, and `missing_activity_ids` counts. Both match-source counters are subsets of `matched`. OLE events with matching non-empty Activity IDs are paired using Activity ID, process ID, thread ID, and operation. When both events lack Activity ID, the existing fallback pairs by process ID, thread ID, and operation and is reported separately as `matched_by_process_thread`; this fallback is weaker than Activity ID correlation and should be interpreted accordingly. If only one event has Activity ID or the available Activity IDs differ, the events are not paired. Matching uses last-in-first-out order for nested operations.

The `timeline.left` and `timeline.right` arrays contain one entry per operation with at least one completed start/stop pair:

| Field | Type | Description |
|---|---|---|
| `operation` | string | Operation name |
| `calls` | number | Number of completed calls included in the timing summary |
| `failed` | number | Number of failed COM calls, where a negative HRESULT was available |
| `average_duration_ms` | number | Average duration in milliseconds |
| `maximum_duration_ms` | number | Maximum duration in milliseconds |

Duration calculations use the trace performance frequency. A trace without usable frequency metadata cannot provide meaningful duration values.

## Interpretation notes

- Event type identity includes the complete provider GUID and event descriptor, not only the event ID.
- COM objects are matched using process ID and OID. Process IDs and OIDs can be reused across different trace sessions, so compare traces from compatible capture contexts.
- An added or removed event type reflects a difference in the captured inputs; it does not by itself indicate a system fault.
- Timing and identity summaries depend on information present in the ETW event schemas and trace metadata.
- The report is descriptive trace data. It does not determine whether an operation is malicious or vulnerable.
