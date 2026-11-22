#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include "comtrace_lifetime.h"
#include "com_server_fixture.h"

typedef struct {
 COMTRACE_PRIMARY primary;
 COMTRACE_SECONDARY secondary;
 IUnknown inner;
 volatile LONG refs;
 IUnknown *outer;
 ULONGLONG id;
 volatile LONG value;
} FIXTURE_OBJECT;

static SRWLOCK provider_lock = SRWLOCK_INIT;
static volatile LONG provider_ready;
static volatile LONG factory_refs;
static volatile LONG object_count;
static volatile LONG server_locks;

static HRESULT provider_register_locked(void)
{
 ULONG status = ERROR_SUCCESS;
 if (!provider_ready) {
  status = comtrace_lifetime_initialize();
  if (status == ERROR_SUCCESS) provider_ready = 1;
 }
 return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}
static void provider_unregister_if_idle_locked(void)
{
 if (!object_count && !factory_refs && !server_locks && provider_ready) {
  provider_ready = 0;
  comtrace_lifetime_shutdown();
 }
}
static void object_destroy(FIXTURE_OBJECT *o)
{
 comtrace_lifetime_object_destroyed(o->id, &IID_IUnknown);
 (void)HeapFree(GetProcessHeap(), 0U, o);
 (void)InterlockedDecrement(&object_count);
}
static ULONG object_addref(FIXTURE_OBJECT *o, const IID *iid, BOOL delegate)
{
 LONG count;
 if (delegate && o->outer != NULL) {
  ULONG result = o->outer->lpVtbl->AddRef(o->outer);
  comtrace_lifetime_add_ref(o->id, iid, result);
  return result;
 }
 count = InterlockedIncrement(&o->refs);
 comtrace_lifetime_add_ref(o->id, iid, (ULONG)count);
 return (ULONG)count;
}
static ULONG object_release(FIXTURE_OBJECT *o, const IID *iid, BOOL delegate)
{
 LONG count;
 if (delegate && o->outer != NULL) {
  ULONG result = o->outer->lpVtbl->Release(o->outer);
  comtrace_lifetime_release(o->id, iid, result);
  return result;
 }
 count = InterlockedDecrement(&o->refs);
 comtrace_lifetime_release(o->id, iid, (ULONG)count);
 if (count == 0) object_destroy(o);
 return count < 0 ? 0U : (ULONG)count;
}
static HRESULT inner_query_object(FIXTURE_OBJECT *o, REFIID iid, void **result)
{
 const IID *reported;
 if (iid == NULL || result == NULL) return E_POINTER;
 *result = NULL;
 if (IsEqualIID(iid, &IID_IUnknown)) {
  *result = &o->inner;
  (void)object_addref(o, &IID_IUnknown, FALSE);
  return S_OK;
 }
 if (IsEqualIID(iid, &COMTRACE_FIXTURE_IID_PRIMARY)) {
  *result = &o->primary;
  reported = &COMTRACE_FIXTURE_IID_PRIMARY;
 } else if (IsEqualIID(iid, &COMTRACE_FIXTURE_IID_SECONDARY)) {
  *result = &o->secondary;
  reported = &COMTRACE_FIXTURE_IID_SECONDARY;
 } else return E_NOINTERFACE;
 (void)object_addref(o, reported, TRUE);
 return S_OK;
}
static HRESULT STDMETHODCALLTYPE inner_query(IUnknown *s, REFIID iid, void **r)
{
 return inner_query_object(CONTAINING_RECORD(s, FIXTURE_OBJECT, inner), iid, r);
}
static ULONG STDMETHODCALLTYPE inner_addref(IUnknown *s)
{
 return object_addref(CONTAINING_RECORD(s, FIXTURE_OBJECT, inner), &IID_IUnknown, FALSE);
}
static ULONG STDMETHODCALLTYPE inner_release(IUnknown *s)
{
 return object_release(CONTAINING_RECORD(s, FIXTURE_OBJECT, inner), &IID_IUnknown, FALSE);
}
static HRESULT STDMETHODCALLTYPE primary_query(COMTRACE_PRIMARY *s, REFIID iid, void **r)
{
 FIXTURE_OBJECT *o = CONTAINING_RECORD(s, FIXTURE_OBJECT, primary);
 if (o->outer != NULL) return o->outer->lpVtbl->QueryInterface(o->outer, iid, r);
 return inner_query_object(o, iid, r);
}
static ULONG STDMETHODCALLTYPE primary_addref(COMTRACE_PRIMARY *s)
{
 return object_addref(CONTAINING_RECORD(s, FIXTURE_OBJECT, primary), &COMTRACE_FIXTURE_IID_PRIMARY, TRUE);
}
static ULONG STDMETHODCALLTYPE primary_release(COMTRACE_PRIMARY *s)
{
 return object_release(CONTAINING_RECORD(s, FIXTURE_OBJECT, primary), &COMTRACE_FIXTURE_IID_PRIMARY, TRUE);
}
static HRESULT STDMETHODCALLTYPE primary_set(COMTRACE_PRIMARY *s, LONG v)
{
 FIXTURE_OBJECT *o = CONTAINING_RECORD(s, FIXTURE_OBJECT, primary);
 (void)InterlockedExchange(&o->value, v);
 return S_OK;
}
static HRESULT STDMETHODCALLTYPE primary_get(COMTRACE_PRIMARY *s, LONG *v)
{
 FIXTURE_OBJECT *o = CONTAINING_RECORD(s, FIXTURE_OBJECT, primary);
 if (v == NULL) return E_POINTER;
 *v = InterlockedCompareExchange(&o->value, 0, 0);
 return S_OK;
}
static HRESULT STDMETHODCALLTYPE secondary_query(COMTRACE_SECONDARY *s, REFIID iid, void **r)
{
 FIXTURE_OBJECT *o = CONTAINING_RECORD(s, FIXTURE_OBJECT, secondary);
 if (o->outer != NULL) return o->outer->lpVtbl->QueryInterface(o->outer, iid, r);
 return inner_query_object(o, iid, r);
}
static ULONG STDMETHODCALLTYPE secondary_addref(COMTRACE_SECONDARY *s)
{
 return object_addref(CONTAINING_RECORD(s, FIXTURE_OBJECT, secondary), &COMTRACE_FIXTURE_IID_SECONDARY, TRUE);
}
static ULONG STDMETHODCALLTYPE secondary_release(COMTRACE_SECONDARY *s)
{
 return object_release(CONTAINING_RECORD(s, FIXTURE_OBJECT, secondary), &COMTRACE_FIXTURE_IID_SECONDARY, TRUE);
}
static HRESULT STDMETHODCALLTYPE secondary_add(COMTRACE_SECONDARY *s, LONG v)
{
 FIXTURE_OBJECT *o = CONTAINING_RECORD(s, FIXTURE_OBJECT, secondary);
 (void)InterlockedExchangeAdd(&o->value, v);
 return S_OK;
}
static IUnknownVtbl inner_vtable = {inner_query,inner_addref,inner_release};
static const COMTRACE_PRIMARY_VTBL primary_vtable = {primary_query,primary_addref,primary_release,primary_set,primary_get};
static const COMTRACE_SECONDARY_VTBL secondary_vtable = {secondary_query,secondary_addref,secondary_release,secondary_add};

static HRESULT STDMETHODCALLTYPE factory_query(IClassFactory *, REFIID, void **);
static ULONG STDMETHODCALLTYPE factory_addref(IClassFactory *);
static ULONG STDMETHODCALLTYPE factory_release(IClassFactory *);
static HRESULT STDMETHODCALLTYPE factory_create(IClassFactory *, IUnknown *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE factory_lock(IClassFactory *, BOOL);
static IClassFactoryVtbl factory_vtable = {factory_query,factory_addref,factory_release,factory_create,factory_lock};
static IClassFactory class_factory = {&factory_vtable};

static HRESULT STDMETHODCALLTYPE factory_query(IClassFactory *s, REFIID iid, void **r)
{
 HRESULT status;
 if (iid == NULL || r == NULL) return E_POINTER;
 *r = NULL;
 if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IClassFactory)) return E_NOINTERFACE;
 AcquireSRWLockExclusive(&provider_lock);
 status = provider_register_locked();
 if (SUCCEEDED(status)) {
  (void)InterlockedIncrement(&factory_refs);
  *r = s;
 }
 ReleaseSRWLockExclusive(&provider_lock);
 return status;
}
static ULONG STDMETHODCALLTYPE factory_addref(IClassFactory *s)
{
 LONG count;
 (void)s;
 AcquireSRWLockExclusive(&provider_lock);
 count = InterlockedIncrement(&factory_refs);
 ReleaseSRWLockExclusive(&provider_lock);
 return (ULONG)count;
}
static ULONG STDMETHODCALLTYPE factory_release(IClassFactory *s)
{
 LONG count;
 (void)s;
 AcquireSRWLockExclusive(&provider_lock);
 count = InterlockedDecrement(&factory_refs);
 if (count < 0) {
  InterlockedExchange(&factory_refs, 0);
  count = 0;
 }
 ReleaseSRWLockExclusive(&provider_lock);
 return (ULONG)count;
}
static HRESULT STDMETHODCALLTYPE factory_create(IClassFactory *s, IUnknown *outer, REFIID iid, void **r)
{
 FIXTURE_OBJECT *o;
 HRESULT status;
 (void)s;
 if (iid == NULL || r == NULL) return E_POINTER;
 *r = NULL;
 if (outer != NULL && !IsEqualIID(iid, &IID_IUnknown)) return CLASS_E_NOAGGREGATION;
 o = (FIXTURE_OBJECT *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*o));
 if (o == NULL) return E_OUTOFMEMORY;
 o->primary.lpVtbl = &primary_vtable;
 o->secondary.lpVtbl = &secondary_vtable;
 o->inner.lpVtbl = &inner_vtable;
 o->refs = 1;
 o->outer = outer;
 (void)InterlockedIncrement(&object_count);
 o->id = comtrace_lifetime_object_created(&COMTRACE_FIXTURE_CLSID, outer == NULL ? iid : &IID_IUnknown, 1U);
 if (o->id == 0ULL) {
  (void)InterlockedDecrement(&object_count);
  (void)HeapFree(GetProcessHeap(), 0U, o);
  return E_FAIL;
 }
 if (outer != NULL) {
  *r = &o->inner;
  return S_OK;
 }
 status = inner_query_object(o, iid, r);
 (void)object_release(o, &IID_IUnknown, FALSE);
 return status;
}
static HRESULT STDMETHODCALLTYPE factory_lock(IClassFactory *s, BOOL lock)
{
 LONG count;
 (void)s;
 AcquireSRWLockExclusive(&provider_lock);
 count = lock ? InterlockedIncrement(&server_locks) : InterlockedDecrement(&server_locks);
 if (count < 0) {
  InterlockedExchange(&server_locks, 0);
  ReleaseSRWLockExclusive(&provider_lock);
  return E_UNEXPECTED;
 }
 ReleaseSRWLockExclusive(&provider_lock);
 return S_OK;
}
HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID iid, void **r)
{
 if (clsid == NULL || iid == NULL || r == NULL) return E_POINTER;
 *r = NULL;
 if (!IsEqualCLSID(clsid, &COMTRACE_FIXTURE_CLSID)) return CLASS_E_CLASSNOTAVAILABLE;
 return factory_query(&class_factory, iid, r);
}
HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
 HRESULT status = S_OK;
 AcquireSRWLockExclusive(&provider_lock);
 if (InterlockedCompareExchange(&object_count,0,0) != 0 ||
     InterlockedCompareExchange(&factory_refs,0,0) != 0 ||
     InterlockedCompareExchange(&server_locks,0,0) != 0) {
  status = S_FALSE;
 } else {
  provider_unregister_if_idle_locked();
 }
 ReleaseSRWLockExclusive(&provider_lock);
 return status;
}
