# COM Runtime and RPC ETW Events

ComTrace captures and displays selected COM Runtime, RPC, and RPCSS events alongside COM Performance and OLE Performance events. These providers add call, stub-rundown, and RPC endpoint-registration context. They do not provide a verified COM object creation/release boundary.

## Providers enabled by Capture

| Provider | GUID | Capture level and filter | Purpose |
|---|---|---|---|
| Microsoft-Windows-COMRuntime | {BF406804-6AFA-46E7-8A48-6C357E1D6D61} | Informational; keyword 0x8 | Premature stub-rundown observations |
| Microsoft-Windows-RPC | {6AD52B32-D609-4BE9-AE07-CE8DAE937E39} | Informational; no keyword filter | RPC call start/stop records |
| Microsoft-Windows-RPCSS | {D8975F88-7DDB-4ED0-91BF-3ADF48C48E0C} | Informational; keywords 0x200 and 0x400 | RPC interface endpoint registration changes |

These providers are optional. A provider enable failure is reported as a warning and does not stop the COM/OLE capture. Capture disables each provider that it successfully enabled.

## COMRuntime: PrematureStubRundown

The Win10 18990 COMRuntime manifest defines event ID 7, version 0, opcode 0, named `PrematureStubRundown`. Its payload contains:

| Field | Type |
|---|---|
| IID | GUID |
| Method | UInt32 |
| IPID | GUID |
| CallType | UInt32 |

ComTrace prints `com_runtime_event=PrematureStubRundown` and, when available, the `IID` and `IPID` as `com_stub_iid` and `com_stub_ipid`. The generic TDH property output preserves the Method and CallType values.

The event name indicates a premature stub-rundown condition. It is not an AddRef/Release trace and is not treated as proof that the owning COM object reached reference count zero. It is retained as a diagnostic observation, not an object-lifetime boundary.

## Microsoft-Windows-RPC

The Win10 17134 manifest defines the following mapped records:

| Event ID | Version | Opcode | Event name | Displayed phase |
|---:|---:|---:|---|---|
| 1 | 1 | Stop (2) | RpcClientCallStop_V1 | RpcClientCall / stop |
| 2 | 1 | Info (0) | RpcServerCall_V1 | RpcServerCall / info |
| 5 | 1 | Start (1) | RpcClientCallStart_V1 | RpcClientCall / start |
| 6 | 1 | Start (1) | RpcServerCallStart_V1 | RpcServerCall / start |
| 7 | 1 | Stop (2) | RpcClientCallStop7_V1 | RpcClientCall / stop |
| 8 | 1 | Stop (2) | RpcServerCallStop_V1 | RpcServerCall / stop |
| 14 | 1 | Start (1) | FunctionTraceStart_V1 | FunctionTrace / start |
| 15 | 1 | Stop (2) | FunctionTraceStop_V1 | FunctionTrace / stop |
| 16 | 1 | Start (1) | FunctionTraceStart16_V1 | FunctionTraceStart16 / start |

Events 14 and 15 expose `InterfaceUuid`, `TypeMgrUuid`, `Flags`, and `MaxCalls`; event 14 also carries the `SDSize` and `SD` fields. Event 16 carries `Protocol`, `Endpoint`, `NetworkAddress`, `PendingQueueSize`, `EndpointFlags`, and `NicFlags`.

Start records expose fields including `InterfaceUuid`, `ProcNum`, `Protocol`, `NetworkAddress`, `Endpoint`, and authentication settings. Stop records expose status; event 1 has a different error-oriented payload. ComTrace labels mapped records with `rpc_event` and `phase`, and explicitly prints `rpc_interface_uuid` when that field exists.

These RPC records describe RPC calls and interface/procedure metadata. An RPC interface UUID is not a COM IID, an RPC call is not a COM object lifetime, and RPC start/stop events are not used to create or close COM object generations. The client/server records are not assumed to be a one-to-one correlated pair.

The RPC manifest also defines Debug and FunctionTrace records with internal pointers and object-like fields. ComTrace does not interpret those pointers as OID, IPID, or COM object identities.

## RPC call timelines

Inspect builds separate timelines for the classified `RpcClientCall`, `RpcServerCall`, and `FunctionTrace` start/stop records. A pair is matched by ActivityId when both records expose the same ActivityId. When both records lack ActivityId, the weaker process/thread/operation match is used and labeled separately. A one-sided or conflicting ActivityId, a different interface UUID, or a different procedure number prevents a match. Client and server records remain separate timelines; ComTrace does not invent a cross-process client/server pairing.

The timeline retains the interface UUID and procedure number when the start record exposes them, and reports raw ETW start/stop ticks, duration ticks, incomplete starts, unmatched stops, and records missing ActivityId. FunctionTraceStart16 is retained as a start observation; the current classifier has no matching stop event for that record, so it may remain incomplete. These timelines describe RPC activity and do not establish COM object lifetime or DCOM object identity.

## Microsoft-Windows-RPCSS

The Win10 17134 manifest defines two endpoint-mapper registration records:

| Event ID | Version | Opcode | Name | Keyword | Named payload fields |
|---:|---:|---:|---|---:|---|
| 2 | 1 | 0 | Interface registration | 0x200 | InterfaceUUID, ObjectUUID, Protocol, EndPoint |
| 3 | 1 | 0 | Interface unregistration | 0x400 | InterfaceUUID, ObjectUUID, Protocol, EndPoint |

ComTrace labels them with `rpcss_event` and prints `rpcss_interface_uuid` and `rpcss_object_uuid` when the values can be decoded. These are RPCSS endpoint-registration fields. In particular, `ObjectUUID` is not treated as the COM Performance provider's OID, and interface unregistration is not treated as COM object destruction.

## Current real-trace evidence

A follow-up 30-second Windows Server 2022 CI capture successfully enabled all six named providers and completed with 50 buffers, zero lost events, and zero lost buffers. The ETL contained 2,313 events across 60 event types. COM Performance produced 780 events.

The RPC provider produced 911 records, and the updated classifier recognized all 911:

- 219 client-call starts and 220 client-call stops
- 219 server-call starts and 219 server-call stops
- 15 FunctionTrace starts and 17 FunctionTrace stops
- 2 FunctionTraceStart16 records

The differing client/server start and stop counts are intentionally kept as independent observations; ComTrace does not pair client and server records based on counts or timing. The client and server call-start records each exposed 18 distinct `InterfaceUuid` values. FunctionTrace start/stop records also carry their own `InterfaceUuid`/`TypeMgrUuid` payload fields and are reported separately.

RPCSS produced two records, both `InterfaceUnregister` (event ID 3); no `InterfaceRegister` record was emitted in this capture. Both records had distinct `InterfaceUUID` values, `ObjectUUID` equal to the all-zero GUID, and protocol `ncalrpc`. These values describe RPC endpoint registration metadata and do not identify a COM OID or an object release.

COMRuntime produced zero events, and COM-RundownInstrumentation produced zero events. OLE-Perf also emitted no events in this headless runner. These missing optional-provider events did not fail validation.

The ETL converted to CTRA with 2,313 records; archive CRC32 validation passed and the ETL/CTRA comparison was identical. This is one Windows Server 2022 runner and one capture window only. The successful RPC classification does not establish an AddRef/Release sequence or an object-lifetime boundary.
## Inspecting a trace

To search each provider independently:

```text
ComTrace.exe --search trace.etl --provider {BF406804-6AFA-46E7-8A48-6C357E1D6D61} --max-events 10000
ComTrace.exe --search trace.etl --provider {6AD52B32-D609-4BE9-AE07-CE8DAE937E39} --max-events 10000
ComTrace.exe --search trace.etl --provider {D8975F88-7DDB-4ED0-91BF-3ADF48C48E0C} --max-events 10000
```

The real-trace validation script records total and classified event counts for all three providers. Missing events are reported as observations and do not fail validation. Provider registration or enablement does not prove that events were emitted.

## COM object lifetime boundary

COM defines `IUnknown::AddRef` and `IUnknown::Release` as reference-count operations; the object is destroyed when its reference count reaches zero. These ETW providers do not expose a general, verified sequence of AddRef/Release operations tied to every OID. The COMRuntime premature-stub-rundown event, COM-RundownInstrumentation records, RPC calls, and RPCSS interface-registration events therefore remain separate evidence categories.

ComTrace does not derive COM object generations from RPC call start/stop, RPC interface register/unregister, a stub-rundown diagnostic, an arbitrary pointer, or a time gap. Reuse of the same OID within one Process generation remains unresolved until a trustworthy per-object lifetime signal is available.

## Manifest references

Provider schemas were cross-checked against community-maintained Win10 manifest snapshots:

- COMRuntime: https://github.com/repnz/etw-providers-docs/blob/master/Manifests-Win10-18990/Microsoft-Windows-COMRuntime.xml
- RPC and RPCSS: https://github.com/repnz/etw-providers-docs/tree/master/Manifests-Win10-17134

Those snapshots are not Microsoft contracts. The target machine's installed manifest and TDH metadata remain the runtime source of truth. Microsoft's COM lifetime overview documents the reference-count role of AddRef and Release: https://learn.microsoft.com/en-us/windows/win32/learnwin32/managing-the-lifetime-of-an-object.


## Application-level COM object lifetime events

The native COMRuntime, RPC, RPCSS, and rundown providers described above do not supply a verified general-purpose per-object AddRef/Release sequence. For COM implementations under your control, ComTrace now provides an opt-in TraceLogging instrumentation library. It emits explicit object-created, AddRef, Release, and object-destroyed records from the instrumented implementation. See [COM Object Lifetime Instrumentation](COM_LIFETIME_INSTRUMENTATION.md) for the API, event fields, sample fixture, and limitations. These application-generated events are kept distinct from native Windows provider observations.
