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

Start records expose fields including `InterfaceUuid`, `ProcNum`, `Protocol`, `NetworkAddress`, `Endpoint`, and authentication settings. Stop records expose status; event 1 has a different error-oriented payload. ComTrace labels mapped records with `rpc_event` and `phase`, and explicitly prints `rpc_interface_uuid` when that field exists.

These RPC records describe RPC calls and interface/procedure metadata. An RPC interface UUID is not a COM IID, an RPC call is not a COM object lifetime, and RPC start/stop events are not used to create or close COM object generations. The client/server records are not assumed to be a one-to-one correlated pair.

The RPC manifest also defines Debug and FunctionTrace records with internal pointers and object-like fields. ComTrace does not interpret those pointers as OID, IPID, or COM object identities.

## Microsoft-Windows-RPCSS

The Win10 17134 manifest defines two endpoint-mapper registration records:

| Event ID | Version | Opcode | Name | Keyword | Named payload fields |
|---:|---:|---:|---|---:|---|
| 2 | 1 | 0 | Interface registration | 0x200 | InterfaceUUID, ObjectUUID, Protocol, EndPoint |
| 3 | 1 | 0 | Interface unregistration | 0x400 | InterfaceUUID, ObjectUUID, Protocol, EndPoint |

ComTrace labels them with `rpcss_event` and prints `rpcss_interface_uuid` and `rpcss_object_uuid` when the values can be decoded. These are RPCSS endpoint-registration fields. In particular, `ObjectUUID` is not treated as the COM Performance provider's OID, and interface unregistration is not treated as COM object destruction.

## Current real-trace evidence

A 30-second Windows Server 2022 CI capture used for this change successfully enabled all six capture providers and completed with 59 buffers, zero lost events, and zero lost buffers. The ETL contained 2,471 events across 59 event types. COM Performance produced 843 events.

The RPC provider produced 1,010 records. ComTrace classified 980 of them: 245 client-call starts, 245 client-call stops, 245 server-call starts, and 245 server-call stops. The start records exposed 17 distinct `InterfaceUuid` values in this trace. The remaining 30 RPC records were retained as raw TDH events but are not mapped by the current classifier.

COMRuntime produced zero events, RPCSS produced zero events, and COM-RundownInstrumentation produced zero events in this particular CI environment. Their absence did not fail validation. OLE-Perf also emitted no events in this headless runner.

The ETL converted to CTRA with 2,471 records; archive CRC32 validation passed and the ETL/CTRA comparison was identical. These measurements describe one runner and one capture window only. The observed RPC client/server start/stop counts are separate provider observations; ComTrace does not assume each client event is a matching server event, nor does it relate the RPC interface UUIDs to COM OID/IPID values.
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
