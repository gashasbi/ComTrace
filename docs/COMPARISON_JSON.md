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
| `object_observations` | object | All observed COM objects and their per-input observation counts and timestamp bounds |
| `process_lifetimes` | object | Process generations and their observed lifecycle boundaries for each input |
| `process_lifecycle_summary` | object | Process event counts and decode-failure counts for each input |
| `timeline` | object | Per-operation timing summaries for each input |
| `lifetime_summary` | object | Application-instrumented COM object lifetime totals and integrity counters |
| `object_lifetimes` | object | Full lifetime records for each input trace |
| `object_lifetime_changes` | array | Added, removed, and changed instrumented lifetime objects |

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
- `lifetime_summary.added`, `lifetime_summary.removed`, and `lifetime_summary.changed`: application-instrumented lifetime-object change totals.

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
| `process_generation` | number or null | Resolved one-based Process generation within this trace; null when no unique lifecycle interval matches |
| `oid` | string | Object ID as a hexadecimal string with a `0x` prefix |
| `object_generation` | null | Unknown; native COM ETW observations do not provide a verified per-OID lifetime boundary |
| `left_present` | boolean | Whether the object is present in the left input |
| `right_present` | boolean | Whether the object is present in the right input |
| `changed_fields` | string | Hexadecimal bit mask of changed identity fields |

The `changed_fields` bits are:

| Bit | Meaning |
|---:|---|
| `0x1` | CLSID changed |
| `0x2` | IID set changed |
| `0x4` | IPID set changed |

Added or removed objects have one presence flag set to false and a zero changed-fields mask. Objects are matched by process ID, resolved Process generation, and OID. An unresolved generation is kept distinct from a resolved generation. Process generation values are one-based ordinals local to each trace, not globally stable process identifiers; comparing captures with different lifecycle coverage can therefore produce added/removed objects even when the underlying process is the same. Identity comparison reports only fields available from the trace schema; missing values are not guessed from arbitrary payload bytes. The inspect graph also records the number of observations and the minimum/maximum raw ETW clock timestamps for each observed COM object and IPID. These bounds are observation times, not object creation or release times. The currently mapped COM events still do not provide a verified object-release signal, so `object_generation` is explicitly `null`: a reused OID within one Process generation cannot be separated into distinct COM object generations without inventing a boundary.

## COM object observations

The `object_observations` object contains `left` and `right` arrays. Each array lists every COM object node observed in that input, including objects whose identity fields did not change and therefore do not appear in `object_changes`.

| Field | Type | Description |
|---|---|---|
| `process_id` | number | Process ID associated with the object |
| `process_generation` | number or null | Resolved one-based Process generation within this trace; null when no unique lifecycle interval matches |
| `oid` | string | Object ID as a hexadecimal string with a `0x` prefix |
| `object_generation` | null | Unknown; native COM ETW observations do not provide a verified per-OID lifetime boundary |
| `observations` | number | Number of event records that exposed this process/OID identity |
| `first_seen_ticks` | string or null | Minimum raw ETW timestamp observed for the object |
| `last_seen_ticks` | string or null | Maximum raw ETW timestamp observed for the object |

Timestamp values are strings to preserve their full 64-bit precision. They use the trace's clock units and are only observation bounds; they do not indicate object creation or release. Missing timestamp bounds are represented as `null`.

## Process lifetimes

The `process_lifetimes` object contains `left` and `right` arrays. A generation is created from a Kernel Process `Start` event or a `DCStart` rundown observation. `End` closes the most recent open generation for the same PID whose first observation is not later than the end timestamp. An `End` without a matching start is retained as an incomplete generation rather than being assigned an invented start time.

Each `process_lifetimes` entry contains:

| Field | Type | Description |
|---|---|---|
| `process_id` | number | PID read from the event payload's `ProcessId` field |
| `generation` | number | One-based generation number for this PID within the trace |
| `parent_process_id` | number or null | Parent PID from the start/rundown event, when available |
| `start_kind` | string | `start`, `dcstart`, or `unknown` |
| `first_seen_ticks` | string or null | Timestamp of the actual Start or DCStart observation |
| `end_ticks` | string or null | Timestamp of an actual End event |
| `running_at_trace_end` | boolean | Whether a DCEnd rundown observed this process at trace end |
| `trace_end_observed_ticks` | string or null | Timestamp of that DCEnd observation |

`start_kind=start` means an actual process Start event was captured. `dcstart` is only a snapshot of a process already running when the kernel session began; `first_seen_ticks` is not its creation time. `running_at_trace_end` is based on DCEnd, which is also a snapshot and does not mean an exit. Timestamp fields are strings to preserve 64-bit precision and use the trace clock's raw units. `--inspect` also reads raw ETW timestamps and labels event sample values as `time_ticks`.

The `process_lifecycle_summary` object contains `left` and `right` event counters: `start_events`, `end_events`, `dc_start_events`, `dc_end_events`, `defunct_events`, and `decode_failures`. Decode failures count recognized Process events for which the payload's `ProcessId` could not be read. Process IDs are extracted from the named payload property, not `EVENT_HEADER.ProcessId`, because the latter can refer to the parent process for creation events. Parent PIDs are retained as event metadata but are not treated as durable process identities because Windows can reuse PIDs.

Actual process generations can only be resolved when exactly one observed lifecycle interval contains the event timestamp. Overlapping intervals, unmatched End records, missing Start events, captures that fall back to COM/OLE-only mode, traces with lost events, and older ETLs can leave the generation unknown. Graph nodes with unknown generations are not merged into a known generation.

## Timeline summaries

The `timeline_summary` object contains `left_com`, `right_com`, `left_ole`, and `right_ole`.

COM timeline entries contain numeric `matched`, `matched_by_call_trace_id`, `matched_by_activity_id`, `matched_by_process_thread`, `incomplete`, `unmatched`, `missing_call_trace_ids`, `missing_activity_ids`, and `failed` counts. All three `matched_by_*` values are subsets of `matched`. The missing-ID values count COM start/stop event records, not calls.

ComTrace first matches events that both expose the same `CallTraceId`, process ID, thread ID, and operation. When both events lack `CallTraceId`, it uses an identical non-empty `ActivityId` with the same process, thread, and operation. When both events lack both identifiers, it can use the same process ID, thread ID, and operation as a weaker fallback. This third method is reported separately as `matched_by_process_thread`; it is not equivalent in confidence to an identifier-based match. Pending starts are paired in last-in-first-out order to preserve nested call order. If only one event has `CallTraceId` or `ActivityId`, or any available identifier differs, the events are not paired. Related Activity IDs are retained as context but are not used as a call key.

OLE timeline entries contain numeric `matched`, `matched_by_activity_id`, `matched_by_related_activity_id`, `matched_by_process_thread`, `incomplete`, `unmatched`, `ambiguous_stops`, `activity_sequences`, `missing_activity_ids`, and `invalid_duration_orders` counts. All three match-source counters are subsets of `matched`. OLE events with matching non-empty primary Activity IDs are paired first. When both events lack a primary ActivityId but expose the same RelatedActivityId, that identifier is used as the second matching level. The process/thread/operation fallback is used only when both events lack both activity identifiers and is reported separately as `matched_by_process_thread`. Nested operations on the same process/thread use last-in-first-out order. A unique cross-process/thread candidate may be paired; multiple candidates are retained as ambiguous unmatched stops and counted in `ambiguous_stops`. One-sided or conflicting identifiers are not paired. Reversed start/stop timestamps are retained as invalid duration orders and counted in `invalid_duration_orders`.

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
- COM objects are matched using process ID, process generation, and OID. Process generation values are trace-local; use captures with compatible lifecycle coverage when comparing traces. OID reuse within a single Process generation is not yet separated because no verified COM object-release event is available.
- An added or removed event type reflects a difference in the captured inputs; it does not by itself indicate a system fault.
- Timing and identity summaries depend on information present in the ETW event schemas and trace metadata.
- The report is descriptive trace data. It does not determine whether an operation is malicious or vulnerable.


## Application-instrumented COM object lifetimes

The report includes a lifetime_summary, object_lifetimes.left,
object_lifetimes.right, and object_lifetime_changes.

Each lifetime record is keyed by process ID, process generation when available,
module `InstrumentationId` when present, and the application-level object instance ID. The module GUID is intentionally trace-local and is not a cross-run identity. It records explicit creation and
destruction observations, AddRef/Release event counts, zero-valued Release
observations, raw ETW timestamps, CLSID, all observed interface IDs, initial
and last observed reference counts when available, and any native identity
links explicitly supplied by the instrumented application. Each record also
contains an observation_count. Missing reference-count values are JSON null.

The lifetime diff matches records using the same process, module instrumentation ID when available, and application identity key. Its changed_fields mask uses 0x1 for CLSID, 0x2 for the set of
interface IDs, 0x4 for creation/destruction state, 0x8 for lifecycle event
counts, 0x10 for explicit native identity bindings, 0x20 for a changed initial
reference count, and 0x40 for a changed last observed reference-count result.
Reference-count differences are reported only when both traces contain the
corresponding value. Native OID/IPID values
are not inferred from PID, timestamp proximity, or IID equality. Only mappings
passed through the lifetime instrumentation API are included.

Timestamps are emitted as raw ETW ticks. They are not compared across trace
files because their clock origins may differ. A Release result of zero remains
an observation; only an explicit destruction event marks an object destroyed.

The lifetime_summary object also reports left and right totals for events_seen,
native_identity_bind_events, events_without_creation, events_after_destruction,
duplicate_creations, destructions_without_creation, and duplicate_destructions.
These are observations from each trace and are not automatically interpreted
as proof of a leak or invalid COM behavior.

### Lifetime record fields

| Field | Type | Description |
|---|---|---|
| `process_id` | number | Process ID that emitted the instrumentation events |
| `process_generation` | number or null | Resolved process generation, when available |
| `instrumentation_id` | string or null | GUID identifying the linked instrumentation module instance; null for older traces and not stable across separate module loads or process runs |
| `object_instance_id` | string | Application-level object ID in hexadecimal |
| `created` / `destroyed` | boolean | Whether explicit creation/destruction events were observed in this trace |
| `create_events`, `add_ref_events`, `release_events`, `destroy_events` | number | Count of each observed lifecycle event |
| `release_zero_results` | number | Release calls whose recorded result was zero |
| `observation_count` | number | Total lifetime events contributing to this record |
| `initial_reference_count` | number or null | Initial count from a captured creation event |
| `last_reference_count_result` | number or null | Most recent captured AddRef/Release result |
| `first_seen_ticks`, `last_seen_ticks` | string | Minimum and maximum raw ETW timestamps for the record |
| `created_ticks`, `destroyed_ticks` | string or null | Timestamp of explicit creation/destruction, when observed |
| `class_id` | string or null | CLSID from the creation event, when captured |
| `interface_ids` | array of strings | Distinct interface IDs observed for the object |
| `native_identities` | array | Explicit native OID/IPID mappings supplied by the application |

A trace can begin or end in the middle of an object's lifetime. In that case,
creation metadata or the initial reference count may be unavailable, and
lifecycle events can be counted as events_without_creation or
destructions_without_creation. These counters describe the captured evidence;
they do not establish that the application violated COM lifetime rules.

### Lifetime summary fields

The `lifetime_summary` object contains:

| Field | Description |
|---|---|
| `left_objects`, `right_objects` | Number of distinct lifetime records in each input |
| `added`, `removed`, `changed` | Lifetime object differences |
| `left_events`, `right_events` | Number of lifetime events processed from each input |
| `left_native_identity_bind_events`, `right_native_identity_bind_events` | Explicit native identity binding events per input |
| `left_events_without_creation`, `right_events_without_creation` | Non-creation events observed before a creation event |
| `left_events_after_destruction`, `right_events_after_destruction` | Events observed after explicit destruction |
| `left_duplicate_creations`, `right_duplicate_creations` | Repeated creation events for an existing record |
| `left_destructions_without_creation`, `right_destructions_without_creation` | Destruction events without a captured creation event |
| `left_duplicate_destructions`, `right_duplicate_destructions` | Repeated explicit destruction events |
