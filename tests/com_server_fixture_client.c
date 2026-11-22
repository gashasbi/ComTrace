#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <objbase.h>
#include <stdio.h>
#include <wchar.h>
#include "com_server_fixture.h"
static const WCHAR logger_name[] = L"ComTrace COM Server Fixture";
static const GUID logger_guid = {0x2a8d1f74,0x3b65,0x4c90,{0xa1,0x27,0x58,0x6b,0x7c,0x8d,0x9e,0xaf}};
static const GUID provider_guid = {0x6f3b5e21,0x7c14,0x4a90,{0x9d,0x42,0x81,0x31,0x6c,0xe2,0xa7,0x50}};
static const wchar_t registry_path[] = L"Software\\Classes\\CLSID\\{7A2C9E41-6D53-4F80-9125-384B5C6D7E8F}\\InprocServer32";
typedef struct { IUnknown iface; IUnknown *inner; volatile LONG refs; } FIXTURE_OUTER;
static HRESULT STDMETHODCALLTYPE outer_query(IUnknown *s, REFIID iid, void **r)
{
 FIXTURE_OUTER *o = CONTAINING_RECORD(s,FIXTURE_OUTER,iface);
 if (iid == NULL || r == NULL) return E_POINTER;
 *r = NULL;
 if (IsEqualIID(iid,&IID_IUnknown)) { *r=s; (void)InterlockedIncrement(&o->refs); return S_OK; }
 if (o->inner == NULL) return E_UNEXPECTED;
 return o->inner->lpVtbl->QueryInterface(o->inner,iid,r);
}
static ULONG STDMETHODCALLTYPE outer_addref(IUnknown *s)
{
 return (ULONG)InterlockedIncrement(&CONTAINING_RECORD(s,FIXTURE_OUTER,iface)->refs);
}
static ULONG STDMETHODCALLTYPE outer_release(IUnknown *s)
{
 LONG c=InterlockedDecrement(&CONTAINING_RECORD(s,FIXTURE_OUTER,iface)->refs);
 return c<0?0U:(ULONG)c;
}
static const IUnknownVtbl outer_vtable={outer_query,outer_addref,outer_release};
static int register_server(const wchar_t *dll)
{
 HKEY key=NULL; DWORD disp=0U,size; static const wchar_t threading[]=L"Both";
 LONG status=RegCreateKeyExW(HKEY_CURRENT_USER,registry_path,0U,NULL,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,NULL,&key,&disp);
 if(status!=ERROR_SUCCESS)return 0;
 size=(DWORD)((wcslen(dll)+1U)*sizeof(wchar_t));
 status=RegSetValueExW(key,NULL,0U,REG_SZ,(const BYTE*)dll,size);
 if(status==ERROR_SUCCESS) status=RegSetValueExW(key,L"ThreadingModel",0U,REG_SZ,(const BYTE*)threading,(DWORD)sizeof(threading));
 (void)RegCloseKey(key); return status==ERROR_SUCCESS;
}
static void unregister_server(void)
{
 static const wchar_t path[]=L"Software\\Classes\\CLSID\\{7A2C9E41-6D53-4F80-9125-384B5C6D7E8F}";
 (void)RegDeleteTreeW(HKEY_CURRENT_USER,path);
}
static int start_trace(const wchar_t *etl,TRACEHANDLE *h,EVENT_TRACE_PROPERTIES **out)
{
 SIZE_T lc=sizeof(logger_name)/sizeof(WCHAR),pc=wcslen(etl)+1U;
 SIZE_T total=sizeof(EVENT_TRACE_PROPERTIES)+(lc+pc)*sizeof(WCHAR);
 EVENT_TRACE_PROPERTIES *p=(EVENT_TRACE_PROPERTIES*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,total);
 ULONG status;
 if(p==NULL)return 0;
 p->Wnode.BufferSize=(ULONG)total;p->Wnode.Flags=WNODE_FLAG_TRACED_GUID;p->Wnode.Guid=logger_guid;
 p->Wnode.ClientContext=1U;p->BufferSize=64U;p->MinimumBuffers=2U;p->MaximumBuffers=16U;p->MaximumFileSize=32U;
 p->LogFileMode=EVENT_TRACE_FILE_MODE_SEQUENTIAL;p->LoggerNameOffset=(ULONG)sizeof(EVENT_TRACE_PROPERTIES);
 p->LogFileNameOffset=p->LoggerNameOffset+(ULONG)(lc*sizeof(WCHAR));
 CopyMemory((BYTE*)p+p->LoggerNameOffset,logger_name,lc*sizeof(WCHAR));
 CopyMemory((BYTE*)p+p->LogFileNameOffset,etl,pc*sizeof(WCHAR));
 status=StartTraceW(h,logger_name,p);
 if(status!=ERROR_SUCCESS){fwprintf(stderr,L"StartTraceW failed: %lu\n",(unsigned long)status);(void)HeapFree(GetProcessHeap(),0U,p);return 0;}
 status=EnableTraceEx2(*h,&provider_guid,EVENT_CONTROL_CODE_ENABLE_PROVIDER,TRACE_LEVEL_VERBOSE,1ULL,0U,0U,NULL);
 if(status!=ERROR_SUCCESS){fwprintf(stderr,L"EnableTraceEx2 failed: %lu\n",(unsigned long)status);(void)ControlTraceW(*h,logger_name,p,EVENT_TRACE_CONTROL_STOP);(void)HeapFree(GetProcessHeap(),0U,p);*h=0U;return 0;}
 *out=p;return 1;
}
static int stop_trace(TRACEHANDLE h,EVENT_TRACE_PROPERTIES *p)
{
 ULONG s=ControlTraceW(h,logger_name,p,EVENT_TRACE_CONTROL_STOP);
 if(s!=ERROR_SUCCESS){fwprintf(stderr,L"ControlTraceW failed: %lu\n",(unsigned long)s);return 0;}return 1;
}
static int test_regular(void)
{
 COMTRACE_PRIMARY *a=NULL;COMTRACE_SECONDARY *b=NULL;IUnknown *ia=NULL,*ib=NULL;LONG v=0;int ok=0;
 if(FAILED(CoCreateInstance(&COMTRACE_FIXTURE_CLSID,NULL,CLSCTX_INPROC_SERVER,&COMTRACE_FIXTURE_IID_PRIMARY,(void**)&a)))goto done;
 if(FAILED(a->lpVtbl->SetValue(a,40))||FAILED(a->lpVtbl->QueryInterface(a,&COMTRACE_FIXTURE_IID_SECONDARY,(void**)&b))||
    FAILED(b->lpVtbl->AddToValue(b,2))||FAILED(a->lpVtbl->QueryInterface(a,&IID_IUnknown,(void**)&ia))||
    FAILED(b->lpVtbl->QueryInterface(b,&IID_IUnknown,(void**)&ib))||ia!=ib||
    FAILED(a->lpVtbl->GetValue(a,&v))||v!=42)goto done;
 ok=1;
done:
 if(ib)(void)ib->lpVtbl->Release(ib);if(ia)(void)ia->lpVtbl->Release(ia);
 if(b)(void)b->lpVtbl->Release(b);if(a)(void)a->lpVtbl->Release(a);return ok;
}
static int test_aggregation(void)
{
 FIXTURE_OUTER o;IUnknown *inner=NULL,*ia=NULL,*ib=NULL;COMTRACE_PRIMARY *a=NULL;COMTRACE_SECONDARY *b=NULL;LONG v=0;int ok=0;
 ZeroMemory(&o,sizeof(o));o.iface.lpVtbl=&outer_vtable;o.refs=1;
 if(FAILED(CoCreateInstance(&COMTRACE_FIXTURE_CLSID,&o.iface,CLSCTX_INPROC_SERVER,&IID_IUnknown,(void**)&inner)))goto done;
 o.inner=inner;
 if(FAILED(o.iface.lpVtbl->QueryInterface(&o.iface,&COMTRACE_FIXTURE_IID_PRIMARY,(void**)&a))||
    FAILED(a->lpVtbl->QueryInterface(a,&COMTRACE_FIXTURE_IID_SECONDARY,(void**)&b))||
    FAILED(a->lpVtbl->SetValue(a,70))||FAILED(b->lpVtbl->AddToValue(b,5))||
    FAILED(a->lpVtbl->QueryInterface(a,&IID_IUnknown,(void**)&ia))||
    FAILED(b->lpVtbl->QueryInterface(b,&IID_IUnknown,(void**)&ib))||
    ia!=&o.iface||ib!=&o.iface||FAILED(a->lpVtbl->GetValue(a,&v))||v!=75||o.refs<5)goto done;
 ok=1;
done:
 if(ib)(void)ib->lpVtbl->Release(ib);if(ia)(void)ia->lpVtbl->Release(ia);
 if(b)(void)b->lpVtbl->Release(b);if(a)(void)a->lpVtbl->Release(a);
 if(inner){if(o.refs!=1){fwprintf(stderr,L"Aggregated outer references were not balanced.\n");ok=0;}
  (void)inner->lpVtbl->Release(inner);o.inner=NULL;}
 if(o.refs!=0)(void)o.iface.lpVtbl->Release(&o.iface);
 return ok;
}
int wmain(int argc,wchar_t **argv)
{
 int result=EXIT_FAILURE,reg=0,com=0;TRACEHANDLE h=0U;EVENT_TRACE_PROPERTIES *p=NULL;
 if(argc!=3||argv[1]==NULL||argv[2]==NULL||argv[1][0]==L'\0'||argv[2][0]==L'\0'){
  fwprintf(stderr,L"Usage: ComTraceComServerClient <server.dll> <output.etl>\n");return EXIT_FAILURE;}
 if(!start_trace(argv[2],&h,&p))goto done;
 if(!register_server(argv[1])){fwprintf(stderr,L"Unable to register the COM class.\n");goto done;}reg=1;
 {HRESULT s=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);if(FAILED(s)){fwprintf(stderr,L"CoInitializeEx failed.\n");goto done;}com=1;}
 if(!test_regular()||!test_aggregation()){fwprintf(stderr,L"COM activation fixture checks failed.\n");goto done;}
 CoFreeUnusedLibraries();result=EXIT_SUCCESS;
done:
 if(com)CoUninitialize();if(reg)unregister_server();
 if(h&&p&&!stop_trace(h,p))result=EXIT_FAILURE;if(p)(void)HeapFree(GetProcessHeap(),0U,p);
 if(result==EXIT_SUCCESS)wprintf(L"Real COM server activation, multiple interfaces, and aggregation passed.\n");
 return result;
}
