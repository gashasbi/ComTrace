# COM Rundown Instrumentation Events

ComTrace can capture the Microsoft-Windows-COM-RundownInstrumentation provider separately from the COM Performance and OLE Performance providers.

- Provider GUID: {2957313D-FCAA-5D4A-2F69-32CE5F0AC44E}
- Keyword used by Capture: RundownInstrumentation (0x1)
- Event IDs: 1–25
- Opcode: 0
- Versions: events 7, 10, and 11 use version 1; the other listed events use version 0.

The manifest defines named payload fields for OID and, in some events, IID and IPID. These named fields are useful for associating rundown observations with COM identities. The manifest does not provide descriptive event messages or semantic enum maps for the numeric fields. The event symbols are generic RundownInstrumentationN names.

## Event payloads

The following payload layouts are defined by the Win10 17134 manifest. The schema registered on the target Windows installation may differ; ComTrace uses the event's TDH metadata when decoding a trace.

| Event ID | Version | Named payload fields |
|---:|---:|---|
| 1 | 0 | OID, IID, IPID, IsFastRundown |
| 2 | 0 | OID, IID, IPID, ProxyManagerIUnknown |
| 3–6 | 0 | OID |
| 7 | 1 | OID, TargetInterface, TargetMethod, IPID, ProxyManagerIUnknown, ImplementedInterface1, ImplementedInterface2, ImplementedInterface3 |
| 8 | 0 | OID, TargetInterface, TargetMethod, IPID, ProxyManagerIUnknown |
| 9 | 0 | OID, Flags, HRESULT |
| 10 | 1 | OID, ClientProcessId, WhereDetected |
| 11 | 1 | OID, ClientProcessId, TokenModifiedLuid, SetIdIfKnown |
| 12, 14, 15, 24 | 0 | OID, ClientProcessId |
| 13 | 0 | OID, ClientProcessId, Error |
| 16 | 0 | OID, TokenModifiedLuid, SetId, CurrentSequenceNumber, LastSequenceNumber |
| 17 | 0 | OID, TokenModifiedLuid, SetId, Error |
| 18, 20 | 0 | OID, TokenModifiedLuid, SetId |
| 19 | 0 | OID, TokenModifiedLuid, SetIdIfKnown |
| 21 | 0 | OID, WhereIssued |
| 22, 23 | 0 | OID, DisconnectionType |
| 25 | 0 | OID, ClientProcessId, TokenModifiedLuid, SetIdIfKnown, Reason |

ProxyManagerIUnknown is a pointer field. IsFastRundown, Flags, WhereDetected, Error, WhereIssued, DisconnectionType, and Reason are preserved as typed payload values; this manifest does not define enum meanings for them. ComTrace does not assign lifecycle semantics to these values.

## How ComTrace handles these events

ComTrace.exe --capture attempts to enable this provider with the rundown keyword. Failure to enable it is a warning and does not stop the normal COM/OLE capture. A successful enable only confirms that the provider was enabled; it does not prove that the provider emitted any events.

Inspect an existing trace by provider GUID:

ComTrace.exe --search trace.etl --provider {2957313D-FCAA-5D4A-2F69-32CE5F0AC44E} --max-events 10000

Recognized records are labeled com_rundown_event and their available OID, IID, and IPID fields are displayed with com_rundown_ prefixes. Other available payload properties are shown through TDH.

These records are kept separate from COM Performance activation/call timelines and the relationship graph. The manifest establishes payload structure, but does not establish that any particular event means an object was created, released, or destroyed. In particular, a rundown record, a disconnection-related field, or an event timestamp is not treated as an object-lifetime boundary. ComTrace therefore does not yet split reuse of the same OID within one Process generation into separate object generations.

## Current real-trace evidence

In the Windows Server 2022 CI ETL captured for this change, COM Performance event 39 (COM_ClientSyncCall start) and event 43 (COM_ServerSyncCall start) exposed OID, IPID, TargetInterface, and CallTraceId. Their stop counterparts (events 40 and 44) exposed CallTraceId and result fields, but not OID/IPID. The same trace contained COM_CreateInstance start/stop events (IDs 5 and 6), but the runtime schema did not expose object identity fields for those records.

The COM-RundownInstrumentation provider was successfully enabled in that capture, as shown by the Capture output, but emitted zero events in the resulting ETL. The real-trace validator records this as NOT_EMITTED rather than treating provider enablement as event production.

These are observations from one Windows Server 2022 runner and do not guarantee that another Windows build exposes the same fields. They provide no verified create/release boundary. OID and IPID values seen on call events remain observations of objects participating in those calls.

## Manifest reference and decoding

The event layouts above were cross-checked against the Win10 17134 manifest snapshot in repnz/etw-providers-docs (https://github.com/repnz/etw-providers-docs/blob/master/Manifests-Win10-17134/Microsoft-Windows-COM-RundownInstrumentation.xml). This is a community-maintained manifest collection, not a Microsoft contract. On the target Windows machine, the installed provider schema and TDH metadata are the runtime decoding source. See Microsoft's documentation for event metadata (https://learn.microsoft.com/en-us/windows/win32/etw/event-metadata-overview), retrieving event data with TDH (https://learn.microsoft.com/en-us/windows/win32/etw/retrieving-event-data-using-tdh), and EVENT_DESCRIPTOR provider/ID/version identity rules (https://learn.microsoft.com/en-us/windows/win32/api/evntprov/ns-evntprov-event_descriptor).
