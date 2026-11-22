# COM Object Lifetime Instrumentation

ComTrace includes an optional C instrumentation library for COM implementations that need explicit object-lifetime observations. It emits self-describing TraceLogging events through ETW. The library does not hook arbitrary COM pointers and does not discover `AddRef` or `Release` calls automatically.

## Build and use

The Windows build produces:

- `ComTraceLifetimeInstrumentation.lib`
- `include/comtrace_lifetime.h`
- `ComTraceComLifetimeFixture.exe`, an end-to-end demonstration

The COM server activation fixture also builds `ComTraceComServerFixture.dll` and `ComTraceComServerClient.exe`. It exercises a real in-process `IClassFactory`, two interfaces with independent `QueryInterface` paths, `AddRef`/`Release`, `LockServer`, and COM aggregation through a controlling outer `IUnknown`.

Link the static library into one module in each process that implements the COM objects to be observed. Its object-instance counter is local to that linked module. If several DLLs in one process need instrumentation, route their calls through a shared wrapper in the module that owns this library; linking separate static copies can reuse the same instance IDs. Include `comtrace_lifetime.h`, call `comtrace_lifetime_initialize` once during application startup, and call `comtrace_lifetime_shutdown` after all tracked objects have finished emitting events.

The five event operations are:

| API | Call site | TraceLogging event |
|---|---|---|
| `comtrace_lifetime_object_created` | After the object has initialized its initial reference state | `ComObjectCreated` |
| `comtrace_lifetime_add_ref` | From the actual implementation of `IUnknown::AddRef` | `ComAddRef` |
| `comtrace_lifetime_release` | From the actual implementation of `IUnknown::Release` | `ComRelease` |
| `comtrace_lifetime_object_destroyed` | From the object's actual final-destruction path | `ComObjectDestroyed` |
| `comtrace_lifetime_bind_native_identity` | Only when the implementation has an authoritative native OID and, optionally, IPID for this exact object/interface | `ComObjectNativeIdentityBound` |

`comtrace_lifetime_object_created` returns a process-local `ObjectInstanceId`. Store it in the object and pass it to every later instrumentation call for that same logical object. For aggregated objects or objects exposing multiple interfaces, use one instance ID for the controlling object and pass the IID corresponding to the interface method being observed.

The creation event records the initial reference count as `InitialReferenceCount`. AddRef and Release events record their method return values as `ReferenceCountResult`. These values are observations only. Microsoft documents the `IUnknown::AddRef` return value as intended for test purposes, so ComTrace does not treat a numeric value by itself as a definitive lifetime boundary. Emit `comtrace_lifetime_object_destroyed` only from the implementation's real destruction path; this explicit event is the destruction observation used by downstream analysis.

`comtrace_lifetime_bind_native_identity` provides an explicit bridge to native COM ETW identity. Call it only when the application or a trusted integration layer already knows the native OID and can prove that it belongs to this exact instrumented object and interface. The IPID is optional. ComTrace records the supplied binding and exposes it in the lifetime record and relationship graph; it does not discover or infer the OID/IPID from an arbitrary COM pointer, CLSID/IID match, or nearby RPC event. A guessed binding is worse than no binding.

A typical implementation records the ID after its initial reference has been established, then emits each method result from the corresponding implementation:

```c
object->trace_instance_id = comtrace_lifetime_object_created(
    &CLSID_MyObject,
    &IID_IMyInterface,
    1U
);

ULONG STDMETHODCALLTYPE my_object_add_ref(MY_OBJECT *object)
{
    ULONG count = (ULONG)InterlockedIncrement(&object->reference_count);
    comtrace_lifetime_add_ref(object->trace_instance_id, &IID_IMyInterface, count);
    return count;
}

ULONG STDMETHODCALLTYPE my_object_release(MY_OBJECT *object)
{
    ULONG count = (ULONG)InterlockedDecrement(&object->reference_count);
    ULONGLONG instance_id = object->trace_instance_id;

    comtrace_lifetime_release(instance_id, &IID_IMyInterface, count);

    if (count == 0U) {
        comtrace_lifetime_object_destroyed(instance_id, &IID_IMyInterface);
        HeapFree(GetProcessHeap(), 0U, object);
    }

    return count;
}
```

## Trace capture and inspection

`ComTrace.exe --capture <seconds> <trace.etl>` enables the `ComTrace.COMLifetime` provider in addition to the Windows COM/RPC providers. An event appears only when an instrumented process registers the provider and executes the corresponding instrumentation call.

Inspect the resulting trace with:

```cmd
ComTrace.exe --inspect trace.etl
```

Lifetime records are displayed as `com_lifetime_event`, `com_lifetime_object_instance_id`, `com_lifetime_clsid`, `com_lifetime_iid`, `com_lifetime_initial_reference_count`, and, where present, `com_lifetime_reference_count_result`. Explicit native bindings are displayed as `com_lifetime_native_object_id` and, when supplied, `com_lifetime_ipid`. Activity bindings are displayed as `activity_bindings`, `activity_id`, and `related_activity_id`. Process ID and thread ID come from the ETW event header. Combine the process ID with the instance ID; the instance ID is not a system-wide COM OID or IPID and is not stable across processes or separate process runs.

The `--compare --json` output includes an `activity_ids` array on each lifetime record. A change in that set is reported in the lifetime object's `changed_fields` bit mask as `0x80`.

The provider GUID is `{6F3B5E21-7C14-4A90-9D42-81316CE2A750}`. Its TraceLogging events carry their own metadata and are decoded by TDH without installing a separate event manifest.

TraceLoggingWrite also records the current thread's ETW ActivityId. ComTrace retains ActivityId and RelatedActivityId on lifetime events and links an instrumented lifetime object to the corresponding activity node in the relationship graph. This creates a useful bridge when the object operation runs on a thread that already carries the OLE/COM activity ID. The library does not create or propagate activity IDs automatically; a zero ActivityId or work moved to a thread without the activity context produces no link. Applications that manage their own activities should follow the ETW activity scoping rules and restore the previous thread ActivityId after the operation.

## Scope and limitations

This is explicit application instrumentation, not transparent system-wide COM interception. It observes only code paths that call the library. If the instrumented pointer is a COM proxy, the observations describe that local proxy implementation; they do not automatically describe the remote server object's reference count. Instrumentation must be placed in the implementation whose lifetime is being studied.

The library does not infer lifetime from RPC call counts, RPCSS endpoint registration, COM rundown observations, pointer values, or time gaps. The native Windows providers remain useful context, but they do not replace explicit instrumentation of the object's actual reference-management and destruction paths.


## Object lifetime analysis in Inspect

Inspect groups the four event types into an object instance keyed by process ID, process generation when resolvable, and `ObjectInstanceId`. Each record keeps the creation and explicit-destruction timestamps, AddRef and Release event counts, the initial and last observed reference-count values, and the observed CLSID/IID. Durations use the ETL trace clock and are shown in milliseconds when its frequency is available; otherwise raw ticks are shown.

The relationship graph uses a distinct `COM_LIFETIME_OBJECT` node type. These nodes connect to their owning process generation, CLSID, observed IID, and any captured ETW ActivityId. They are deliberately separate from native OID-based COM object nodes because the application-generated instance ID is not an OID. When an explicit native identity binding is present, the graph adds a `lifetime_object_maps_to_native_object` edge and links any supplied IPID and IID. No such edge is created from heuristic CLSID/IID or timing matches.

The timeline reports missing creation/destruction events, duplicate creation/destruction records, events after an explicit destruction, and Release-zero return observations without an explicit destruction event. A zero return value does not automatically mark an object destroyed. The explicit `ComObjectDestroyed` event remains the only lifetime end marker in this instrumentation model.
