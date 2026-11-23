# ComTrace

ComTrace is a native Windows command-line toolkit for capturing, inspecting, filtering, archiving, and comparing ETW traces related to COM and OLE activity.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++** and the Windows SDK
- CMake 3.21 or newer
- Ninja only for the VS Code/Ninja workflow

## Build and test

### Visual Studio 2022

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder:

```cmd
cmake --preset windows-vs2022-x64
cmake --build --preset windows-vs2022-x64-release
ctest --preset windows-vs2022-x64-release
```

For Debug, replace `release` with `debug`. Open `out\build\windows-vs2022-x64\ComTrace.sln` to work in Visual Studio.

### VS Code / Ninja

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder:

```cmd
cmake --preset windows-msvc-x64-release
cmake --build --preset windows-msvc-x64-release
ctest --preset windows-msvc-x64-release
```

For Debug, replace `release` with `debug` in all three commands. In VS Code, install the recommended **CMake Tools** and **C/C++** extensions and select the corresponding configure preset.

## Command-line tools

The build produces two executables:

- `ComTrace.exe` captures, inspects, filters, and compares traces.
- `ComTraceArchive.exe` converts ETL files to CTRA archives and inspects or scans CTRA archives.

### ComTrace.exe

```cmd
ComTrace.exe --help
ComTrace.exe --version
ComTrace.exe --self-test
ComTrace.exe --providers
ComTrace.exe --capture 60 trace.etl
ComTrace.exe --inspect trace.etl 100
ComTrace.exe --search trace.etl --com --pid 1234 --max-events 50
ComTrace.exe --compare before.etl after.etl
ComTrace.exe --compare before.etl after.ctra --json
```

**Capture** enables the COM Performance and OLE Performance ETW providers and attempts to enable COM-RundownInstrumentation, the COMRuntime PrematureStubRundown event, informational RPC call events, and RPCSS interface registration events. When Windows permits a private system logger, Kernel Process start/stop events are recorded in the same ETL. The additional providers are optional; enablement does not guarantee that they emit events. If the system logger is unavailable, capture falls back to the COM/OLE providers without process lifecycle events and prints a warning. The duration must be between 1 and 3600 seconds. The output path must end in `.etl` and must not already exist. Depending on the machine's ETW permissions, capture may require an elevated terminal.

**Inspect** processes the full trace and prints a bounded sample of events (100 by default; accepted range 1–10000), event-type counts, COM, OLE, and RPC client/server call timelines, Process lifetime generations, and a relationship-graph summary. Event samples and lifecycle bounds use raw ETW clock ticks (`time_ticks`); the sample limit only limits detailed event output, not full-trace analysis.

**ETL property decoding** uses TDH metadata and supports scalar values, fixed and dynamic arrays, and structure members, including dynamic arrays of structures. Nested descriptor paths are handled when runtime metadata exposes them. Dynamic array counts support signed and unsigned integer fields, including pointer-sized counts; negative or excessive counts are rejected. The decoder limits nesting to 16 struct levels, arrays to 4096 elements, event metadata to 1 MiB, and an individual property payload to 16 MiB. Value and bitmap maps are decoded from TDH map entries; inspection reports the map kind and name, resolved label, and original raw value. When TDH exposes a pattern map, its format and entries are reported as message-rendering metadata and are not applied to individual property values. TDH output formats for date/time values, reduced strings, and PKCS7-with-type-info payloads are used when supported; no-print fields remain suppressed and unknown values retain their raw bytes. WBEM XML-fragment properties retain their declared format and can be searched as text when their input type is a string. Custom-schema properties retain their raw payload and expose the schema protocol identifier and schema bytes; protocol-specific deserialization is not guessed. Nested TraceLogging structures are covered by an ETL fixture. The Windows manifest compiler rejects nested `<struct>` declarations, so that specific manifest shape cannot be used as a manifest fixture.

See [ETL_DECODER.md](docs/ETL_DECODER.md) for the detailed supported-type matrix, map behavior, resource limits, and fixture coverage.

**Search** scans an ETL file and supports these optional filters:

- `--provider <guid>`
- `--event-id <id>`
- `--pid <id>`
- `--tid <id>`
- `--com` or `--ole` (mutually exclusive)
- `--clsid <guid>`, `--iid <guid>`, `--oid <value>`
- `--lifetime-id <value>`, `--activity-id <guid>`
- `--property <name> --equals <value>` for exact value matching
- `--property <name> --contains <value>` for string or binary substring matching
- `--property <name> --greater-than <number>`
- `--property <name> --greater-or-equal <number>`
- `--property <name> --less-than <number>`
- `--property <name> --less-or-equal <number>`
- `--max-events <n>` (1–10000; default 100)

Filters can be combined and all supplied filters must match. Property filters support top-level values, indexed fixed and dynamic arrays, nested structure members, and indexed structure arrays. Supported scalar types include strings, signed and unsigned integers, floating-point values, booleans, GUIDs, hexadecimal integers, pointers, FILETIME, SYSTEMTIME, SID, IPv4/IPv6, and socket addresses. Binary and custom-schema payloads support exact matching and byte-sequence contains matching with `hex:<bytes>`. Value and bitmap maps are displayed using their TDH labels while search compares the underlying value. See [SEARCH.md](docs/SEARCH.md) for details.

**Compare** accepts ETL or CTRA input files. It reports event-type count changes, COM object changes keyed by process ID, resolved process generation, and OID, Process lifetime generations, changes to available CLSID/IID/IPID identity fields, COM and OLE timeline counts, and per-operation timing summaries. Add `--json` for the versioned JSON report (currently schema version 1); its fields and data types are documented in [COMPARISON_JSON.md](docs/COMPARISON_JSON.md).

### ComTraceArchive.exe

```cmd
ComTraceArchive.exe trace.etl trace.ctra
ComTraceArchive.exe --inspect trace.ctra
ComTraceArchive.exe --recover trace.ctra
ComTraceArchive.exe --help
ComTraceArchive.exe --version
```

The first command converts an ETL trace to the CTRA archive format. New archives use **CTRA version 2** and store a CRC-32 checksum over every byte before the checksum field, including the file header, event records, and footer metadata. `--inspect` validates the archive checksum and reports its event and trace-footer statistics. The reader remains backward-compatible with **CTRA version 1** archives, which do not contain a checksum. CRC32 detects accidental corruption; it is not a cryptographic signature or authenticity guarantee. The complete byte layout, validation rules, and version compatibility details are documented in [CTRA_FORMAT.md](docs/CTRA_FORMAT.md).

`--recover` scans recoverable records and reports the recovered count, stopping offset, and stop reason. When a version 2 checksum is invalid, recovery preserves the records that could be read but reports `checksum invalid` and does not mark the archive as complete. Recovery mode is a read-only scan; it does not write a replacement archive.

## Real ETL validation

Use [the real ETL validation workflow](docs/REAL_TRACE_VALIDATION.md) on a Windows machine to capture or supply a trace with COM and OLE activity, confirm both providers' events and operation timelines, convert ETL to CTRA, verify CRC32, and compare the original trace with the archive round trip. The script is `scripts/validate-real-trace.ps1`; new captures also run `ComTraceOleActivityFixture.exe` to generate OLE clipboard traffic, invoke `DoDragDrop` with a deterministic cancelling source, and exercise drag-drop registration. Capturing a new trace requires elevated 64-bit PowerShell; validating an existing ETL does not require elevation.

## DDE instrumentation

ComTrace includes the optional **ComTraceDdeInstrumentation** library for applications that need DDE conversation and transaction timelines. It records conversation lifecycle, transaction start/completion, server callbacks, explicit cross-process correlation IDs, unmatched lifecycle events, invalid duration order, and conflicting transaction metadata. **This is opt-in application instrumentation, not automatic interception of arbitrary DDEML calls.** The application must call the API at its DDE client/server lifecycle points and propagate correlation IDs across processes. See [DDE_INSTRUMENTATION.md](docs/DDE_INSTRUMENTATION.md).

## COM/OLE analysis and limitations

- COM Performance, COMRuntime, RPC, RPCSS, COM-RundownInstrumentation, and OLE Performance event classification is limited to the provider, event ID, version, and opcode mappings explicitly defined in `src/com_event.c` and `src/ole_event.c`. Provider payloads and interpretation limits are documented in [COM_RUNDOWN_EVENTS.md](docs/COM_RUNDOWN_EVENTS.md) and [COM_RPC_EVENTS.md](docs/COM_RPC_EVENTS.md).
- RPC call analysis keeps client, server, and FunctionTrace roles separate. Start/stop pairs use ActivityId when available and only fall back to same-process/thread/operation when both sides lack it. A second pass links client and server calls only with a unique shared ActivityId, InterfaceUuid, and ProcNum match; ambiguous or incomplete calls remain unlinked.
- RPC Protocol, NetworkAddress, and Endpoint values are retained. Only `ncalrpc` is classified as local and known TCP/HTTP protocol sequences as network; other transports remain unknown. RPCSS registration records are tracked by exact interface/object UUID and binding, and calls link to a registration only when the binding and event-time interval match.
- RPC calls link to COM operation records only when ActivityId and the interface GUID/IID both match exactly. This is evidence-based trace correlation, not an assumption that every RPC interface UUID is a COM IID. Missing status remains unknown, nonzero status is reported as failure, and durations with reversed ETW timestamps are marked invalid.
- CLSID, IID, IPID, OID, CallTraceId, process/thread IDs, and Activity IDs are kept as distinct fields. A value is reported only when the ETW event schema or event header provides it; ComTrace does not scan arbitrary payload bytes or infer missing identifiers.
- COM start/stop pairing uses matching CallTraceId values first. When both events lack CallTraceId, matching ActivityId, process ID, thread ID, and operation form a second level. When both identifiers are absent, process ID, thread ID, and operation form a weaker third level, reported separately. One-sided or conflicting identifiers remain unmatched.
- OLE operation pairing uses matching primary ActivityId first, then matching RelatedActivityId only when both sides lack primary ActivityId, and finally the weaker process/thread/operation fallback only when both activity identifiers are absent on both sides. One-sided or conflicting identifiers remain unmatched. Reversed timestamp order is reported as an invalid duration.
- Relationship graph Process, Thread, and COM Object nodes use `(PID, process generation)` when exactly one lifecycle interval contains the event timestamp. Call endpoints are resolved independently, and a COM object uses the generation of its valid target process when provided, otherwise the generation of the event process. If the timestamp is outside all known intervals or matches overlapping/incomplete intervals, the generation remains unknown; the graph never guesses a generation. Without usable lifecycle data, the graph retains the legacy PID-scoped grouping.
- Relationship graph call direction uses explicit `SourceProcessId`/`SourceThreadId` and `TargetProcessId`/`TargetThreadId` endpoints when both are present. When only the target endpoint is available, the event process/thread is treated as the caller; when only the source endpoint is available, the event process/thread is treated as the callee. Zero process and thread IDs do not create graph nodes.
- `ActivityId` and `RelatedActivityId` are retained independently. A related activity remains visible as a node when the event has no primary `ActivityId`, but no relationship edge is invented without both identifiers; identical activity IDs do not create self-edges.
- Kernel Process `Start` and `End` events are decoded using the payload `ProcessId` field and tracked as one-based generations within a trace. `DCStart` and `DCEnd` are kept as start/end-of-trace rundown observations; they are not treated as actual process creation or exit. An unmatched End remains an incomplete generation. Parent IDs are retained as descriptive metadata only because Windows can reuse them. Process lifecycle events are available when Capture can start its private system logger; the COM/OLE-only fallback and traces with missing events can produce incomplete lifecycle data.
- COM object and IPID nodes accumulate the number of events that exposed each identity and the minimum/maximum ETW timestamp observed for it. These values describe the observation window in the trace clock's raw ticks; they are not creation or release times. COM Object graph nodes and comparison keys now include the resolved Process generation, which separates the same PID/OID pair across observed Process lifetimes. Generation numbers are trace-local ordinals; comparisons across separately captured traces require compatible lifecycle coverage. The COM-RundownInstrumentation provider exposes named OID and, in some events, IID/IPID fields. These records are reported as rundown observations only. COMRuntime PrematureStubRundown is a diagnostic stub event, RPC start/stop records describe procedure calls and are correlated only when ActivityId, InterfaceUuid, and ProcNum provide a unique match. RPCSS InterfaceUUID/ObjectUUID/Protocol/EndPoint values describe time-bounded endpoint registration records; they are not treated as COM object lifetime boundaries. None of these has a verified per-object AddRef/Release boundary, so they are not used as object-lifetime boundaries. Reuse of the same OID within one Process generation cannot yet be split into distinct COM object generations.
- Peer process/thread IDs are read from explicitly named event properties such as `TargetProcessId`, `TargetThreadId`, `SourceProcessId`, and `SourceThreadId`. Executable paths are displayed only when Windows permits the process query.
- The OLE Performance manifest inspected on Windows Server 2022 defines 28 events across 14 Clipboard and Drag & Drop operations, but does not declare event payload templates or data fields. ComTrace also queries TDH runtime metadata for displayed events and preserves any named properties it exposes. DataObject identity is not inferred from raw payload bytes; it is only eligible for extraction when a stable runtime property and its meaning can be verified.

### DDE instrumentation and analysis

ComTrace provides an optional `ComTraceDdeInstrumentation` static library and the public `comtrace_dde.h` API for applications that own or can modify their DDEML integration. Applications register the provider with `comtrace_dde_initialize`, emit conversation-open/close events, transaction-start/completion events, and callback events at the corresponding DDEML boundaries, then unregister it with `comtrace_dde_shutdown`.

The capture command enables the `ComTrace.DDE` TraceLogging provider. ETL inspection decodes its named fields and builds a DDE conversation and transaction timeline, including durations, callback observations, and cross-process exchanges. Conversation and transaction IDs generated by the library are process-local. Applications must supply stable, matching `correlation_id`, `conversation_correlation_id`, and `transaction_correlation_id` values when they need to correlate activity across processes; ComTrace does not guess cross-process identity from matching names or timestamps.

This is explicit application instrumentation, not a system-wide DDEML hook. Events are available only from processes that link and call the instrumentation API. Payload contents are not copied into ETW; the provider records metadata and data sizes so that application data is not silently exposed.
- The COM Performance manifest inspected on the Windows Server 2022 CI runner defines 55 events but no templates or data fields. ComTrace therefore relies on named TDH runtime properties when available; some identifiers, including CLSID, may remain unavailable in real traces. It does not infer them from undocumented payload offsets.
- `--providers` reports seven providers in the CLI inventory: COMRuntime, COM-Perf, COM-RundownInstrumentation, RPC, RPCSS, and the optional ComTrace.COMLifetime instrumentation provider. Capture also enables OLE-Perf, which is not part of that CLI inventory.
- A registered or enabled provider is not proof that it emitted events. Validate event production using an ETL captured on the target Windows system.
- ETL processing uses Windows ETW and TDH APIs. Build and end-to-end trace validation must be performed on Windows.
- ComTrace is a static trace analysis tool. Its output does not establish that an operation is malicious or vulnerable.


### COM object lifetime instrumentation

ComTrace also ships an optional static library, `ComTraceLifetimeInstrumentation.lib`, for COM implementations that you control. Call its API from the actual object creation, `IUnknown::AddRef`, `IUnknown::Release`, and final-destruction paths to emit self-describing ETW records. Capture enables the `ComTrace.COMLifetime` provider, and Inspect decodes its object instance ID, CLSID, IID, and observed reference-count return values. This is explicit application instrumentation, not automatic system-wide COM interception. See [COM Object Lifetime Instrumentation](docs/COM_LIFETIME_INSTRUMENTATION.md).


Inspect also builds a per-object lifetime timeline from these application-generated events and adds separate instrumented-object nodes to the relationship graph, keyed by process, process generation when known, module `InstrumentationId` when present, and `ObjectInstanceId`. Native OID observations remain separate from these application-local identities unless an instrumented implementation explicitly binds a known native OID/IPID with `comtrace_lifetime_bind_native_identity`; ComTrace does not infer this mapping from CLSID/IID or timing alone.
