# Relationship Graph

ETL inspection builds one trace-local graph from COM, OLE, RPC, DDE, process-lifecycle, and application COM-lifetime observations.

## Node and relationship sources

- Process and Thread nodes are scoped by the observed process generation when a lifecycle event can resolve it. Unknown generations remain explicitly unknown.
- Activity nodes preserve ActivityId relationships across providers. RelatedActivityId is represented as a separate relationship, not silently treated as the same identifier.
- COM calls and COM objects are connected only through captured call-trace and object identity fields. Object nodes are scoped by process generation.
- RPC calls retain their own process/thread, ActivityId, InterfaceUuid, and correlation links. A client/server edge is emitted only when the RPC timeline has a unique correlation.
- RPC-to-COM call links require a shared ActivityId and an exact match between RPC InterfaceUuid and a COM object IID. Ambiguous candidates are not linked.
- DDE conversation and transaction nodes retain their process ownership and explicit correlation IDs. Server callback observations are separate callback nodes, not fabricated transaction instances. Client transactions connect to server transactions or callback nodes only from the DDE timeline's unique exchange results and explicit correlation IDs.
- OLE operations retain their operation name, process/thread, observed timestamp window, ActivityId, and RelatedActivityId. RelatedActivityId remains a separate activity node and is linked only when a distinct primary ActivityId is also present. An OLE operation is linked to a COM object as an IDataObject only when the same primary ActivityId reaches exactly one object exposing IID_IDataObject, including across process boundaries.
- Instrumented COM lifetime objects remain distinct from native COM object observations. A lifetime object is not merged with a native OID unless the instrumentation's explicit native-identity binding establishes the relationship.

## Conservative correlation rules

The graph does not infer relationships from timestamp proximity, matching process names, reused numeric IDs, or similar event sequences. A missing identifier produces an unlinked observation. Multiple candidates produce no edge. DDE callback nodes retain their own process and thread context; they connect to a client transaction only when the timeline has a unique explicit correlation. Process ID reuse is separated when process lifecycle evidence provides a generation; otherwise the generation remains unknown.

RPC InterfaceUuid is not generally assumed to be a COM IID. It is compared with a COM IID only for the explicit RPC-to-COM link, and only as one exact piece of evidence alongside ActivityId.

The OLE DataObject relationship is deliberately narrow: it requires a captured COM object identity and the standard IDataObject IID. If the OLE provider does not expose an ActivityId that connects the drag operation to the COM call, or the COM identity does not expose that IID, ComTrace leaves the relationship unresolved.

## Output

The Relationship graph summary reports node and edge counts and prints a bounded sample of graph edges. The rpc_com_calls and ole_data_objects counters count the cross-layer links that passed the exact matching rules above. The graph is a trace-local relationship view; it is not a proof of causality when the provider did not emit a linking identifier.
