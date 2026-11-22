# COM Object Lifetime Instrumentation

ComTrace includes an optional C instrumentation library for COM implementations that need explicit object-lifetime observations. It emits self-describing TraceLogging events through ETW. The library does not hook arbitrary COM pointers and does not discover `AddRef` or `Release` calls automatically.

## Build and use

The Windows build produces:

- `ComTraceLifetimeInstrumentation.lib`
- `include/comtrace_lifetime.h`
- `ComTraceComLifetimeFixture.exe`, an end-to-end demonstration

Link the static library into the process that implements the COM objects to be observed. Include `comtrace_lifetime.h`, call `comtrace_lifetime_initialize` once during application startup, and call `comtrace_lifetime_shutdown` after all tracked objects have finished emitting events.

The four event operations are:

| API | Call site | TraceLogging event |
|---|---|---|
| `comtrace_lifetime_object_created` | After the object has initialized its initial reference state | `ComObjectCreated` |
| `comtrace_lifetime_add_ref` | From the actual implementation of `IUnknown::AddRef` | `ComAddRef` |
| `comtrace_lifetime_release` | From the actual implementation of `IUnknown::Release` | `ComRelease` |
| `comtrace_lifetime_object_destroyed` | From the object's actual final-destruction path | `ComObjectDestroyed` |

`comtrace_lifetime_object_created` returns a process-local `ObjectInstanceId`. Store it in the object and pass it to every later instrumentation call for that same logical object. For aggregated objects or objects exposing multiple interfaces, use one instance ID for the controlling object and pass the IID corresponding to the interface method being observed.

The API records the class ID, interface ID, and reference-count result supplied by the instrumented implementation. Reference-count return values are observations only. Microsoft documents the `IUnknown::AddRef` return value as intended for test purposes, so ComTrace does not treat a numeric value by itself as a definitive lifetime boundary. Emit `comtrace_lifetime_object_destroyed` only from the implementation's real destruction path; this explicit event is the destruction observation used by downstream analysis.

## Trace capture and inspection

`ComTrace.exe --capture <seconds> <trace.etl>` enables the `ComTrace.COMLifetime` provider in addition to the Windows COM/RPC providers. An event appears only when an instrumented process registers the provider and executes the corresponding instrumentation call.

Inspect the resulting trace with:

```cmd
ComTrace.exe --inspect trace.etl
```

Lifetime records are displayed as `com_lifetime_event`, `com_lifetime_object_instance_id`, `com_lifetime_clsid`, `com_lifetime_iid`, and, where present, `com_lifetime_reference_count_result`. Process ID and thread ID come from the ETW event header. Combine the process ID with the instance ID; the instance ID is not a system-wide COM OID or IPID and is not stable across processes or separate process runs.

The provider GUID is `{6F3B5E21-7C14-4A90-9D42-81316CE2A750}`. Its TraceLogging events carry their own metadata and are decoded by TDH without installing a separate event manifest.

## Scope and limitations

This is explicit application instrumentation, not transparent system-wide COM interception. It observes only code paths that call the library. If the instrumented pointer is a COM proxy, the observations describe that local proxy implementation; they do not automatically describe the remote server object's reference count. Instrumentation must be placed in the implementation whose lifetime is being studied.

The library does not infer lifetime from RPC call counts, RPCSS endpoint registration, COM rundown observations, pointer values, or time gaps. The native Windows providers remain useful context, but they do not replace explicit instrumentation of the object's actual reference-management and destruction paths.
