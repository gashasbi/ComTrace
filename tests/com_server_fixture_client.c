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
static const GUID com_perf_provider_guid = {0xb8d6861b,0xd20f,0x4eec,{0xbb,0xae,0x87,0xe0,0xdd,0x80,0x60,0x2b}};
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
static IUnknownVtbl outer_vtable={outer_query,outer_addref,outer_release};
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
 status=EnableTraceEx2(*h,&com_perf_provider_guid,EVENT_CONTROL_CODE_ENABLE_PROVIDER,TRACE_LEVEL_VERBOSE,0U,0U,0U,NULL);
 if(status!=ERROR_SUCCESS){fwprintf(stderr,L"COM Perf provider enable failed: %lu\n",(unsigned long)status);(void)ControlTraceW(*h,logger_name,p,EVENT_TRACE_CONTROL_STOP);(void)HeapFree(GetProcessHeap(),0U,p);*h=0U;return 0;}
 *out=p;return 1;
}
static int stop_trace(TRACEHANDLE h,EVENT_TRACE_PROPERTIES *p)
{
 ULONG s=ControlTraceW(h,logger_name,p,EVENT_TRACE_CONTROL_STOP);
 if(s!=ERROR_SUCCESS){fwprintf(stderr,L"ControlTraceW failed: %lu\n",(unsigned long)s);return 0;}return 1;
}
static int test_regular(void)
{
 IClassFactory *factory=NULL; COMTRACE_PRIMARY *a=NULL; COMTRACE_SECONDARY *b=NULL;
 IUnknown *ia=NULL,*ib=NULL; LONG v=0; BOOL server_locked=FALSE; int ok=0;
 HRESULT hr=S_OK; const wchar_t *step=L"CoGetClassObject";
 hr=CoGetClassObject(&COMTRACE_FIXTURE_CLSID,CLSCTX_INPROC_SERVER,NULL,&IID_IClassFactory,(void**)&factory);
 if(FAILED(hr))goto failed;
 step=L"IClassFactory::LockServer(TRUE)";
 hr=factory->lpVtbl->LockServer(factory,TRUE);
 if(FAILED(hr))goto failed;
 server_locked=TRUE;
 step=L"IClassFactory::CreateInstance";
 hr=factory->lpVtbl->CreateInstance(factory,NULL,&COMTRACE_FIXTURE_IID_PRIMARY,(void**)&a);
 if(FAILED(hr))goto failed;
 step=L"primary SetValue";
 hr=a->lpVtbl->SetValue(a,40);
 if(FAILED(hr))goto failed;
 step=L"primary QueryInterface secondary";
 hr=a->lpVtbl->QueryInterface(a,&COMTRACE_FIXTURE_IID_SECONDARY,(void**)&b);
 if(FAILED(hr))goto failed;
 step=L"secondary AddToValue";
 hr=b->lpVtbl->AddToValue(b,2);
 if(FAILED(hr))goto failed;
 step=L"primary QueryInterface IUnknown";
 hr=a->lpVtbl->QueryInterface(a,&IID_IUnknown,(void**)&ia);
 if(FAILED(hr))goto failed;
 step=L"secondary QueryInterface IUnknown";
 hr=b->lpVtbl->QueryInterface(b,&IID_IUnknown,(void**)&ib);
 if(FAILED(hr))goto failed;
 step=L"IUnknown identity comparison";
 if(ia!=ib){hr=E_FAIL;goto failed;}
 step=L"primary GetValue";
 hr=a->lpVtbl->GetValue(a,&v);
 if(FAILED(hr))goto failed;
 step=L"primary value verification";
 if(v!=42){hr=E_FAIL;goto failed;}
 ok=1;
 goto done;
failed:
 fwprintf(stderr,L"Regular COM activation failed at %ls (HRESULT=0x%08lX).\n",step,(unsigned long)hr);
done:
 if(ib)(void)ib->lpVtbl->Release(ib);
 if(ia)(void)ia->lpVtbl->Release(ia);
 if(b)(void)b->lpVtbl->Release(b);
 if(a)(void)a->lpVtbl->Release(a);
 if(server_locked&&FAILED(factory->lpVtbl->LockServer(factory,FALSE)))ok=0;
 if(factory)(void)factory->lpVtbl->Release(factory);
 return ok;
}
static int test_aggregation(void)
{
 FIXTURE_OUTER o; IUnknown *inner=NULL,*ia=NULL,*ib=NULL;
 COMTRACE_PRIMARY *a=NULL; COMTRACE_SECONDARY *b=NULL; LONG v=0; int ok=0;
 HRESULT hr=S_OK; const wchar_t *step=L"CoCreateInstance aggregation";
 ZeroMemory(&o,sizeof(o));o.iface.lpVtbl=&outer_vtable;o.refs=1;
 hr=CoCreateInstance(&COMTRACE_FIXTURE_CLSID,&o.iface,CLSCTX_INPROC_SERVER,&IID_IUnknown,(void**)&inner);
 if(FAILED(hr))goto failed;
 o.inner=inner;
 step=L"outer QueryInterface primary";
 hr=o.iface.lpVtbl->QueryInterface(&o.iface,&COMTRACE_FIXTURE_IID_PRIMARY,(void**)&a);
 if(FAILED(hr))goto failed;
 step=L"primary QueryInterface secondary";
 hr=a->lpVtbl->QueryInterface(a,&COMTRACE_FIXTURE_IID_SECONDARY,(void**)&b);
 if(FAILED(hr))goto failed;
 step=L"aggregated primary SetValue";
 hr=a->lpVtbl->SetValue(a,70);
 if(FAILED(hr))goto failed;
 step=L"aggregated secondary AddToValue";
 hr=b->lpVtbl->AddToValue(b,5);
 if(FAILED(hr))goto failed;
 step=L"aggregated primary QueryInterface IUnknown";
 hr=a->lpVtbl->QueryInterface(a,&IID_IUnknown,(void**)&ia);
 if(FAILED(hr))goto failed;
 step=L"aggregated secondary QueryInterface IUnknown";
 hr=b->lpVtbl->QueryInterface(b,&IID_IUnknown,(void**)&ib);
 if(FAILED(hr))goto failed;
 step=L"aggregated IUnknown identity comparison";
 if(ia!=&o.iface||ib!=&o.iface){hr=E_FAIL;goto failed;}
 step=L"aggregated GetValue";
 hr=a->lpVtbl->GetValue(a,&v);
 if(FAILED(hr))goto failed;
 step=L"aggregated value verification";
 if(v!=75){hr=E_FAIL;goto failed;}
 step=L"aggregated reference count verification";
 if(o.refs<5){hr=E_FAIL;goto failed;}
 ok=1;
 goto done;
failed:
 fwprintf(stderr,L"COM aggregation failed at %ls (HRESULT=0x%08lX, outer refs=%ld, value=%ld).\n",step,(unsigned long)hr,o.refs,v);
done:
 if(ib)(void)ib->lpVtbl->Release(ib);
 if(ia)(void)ia->lpVtbl->Release(ia);
 if(b)(void)b->lpVtbl->Release(b);
 if(a)(void)a->lpVtbl->Release(a);
 if(inner){if(o.refs!=1){fwprintf(stderr,L"Aggregated outer references were not balanced (refs=%ld).\n",o.refs);ok=0;}
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
 {HKEY key=NULL;LONG rs=RegOpenKeyExW(HKEY_CLASSES_ROOT,L"CLSID\\{7A2C9E41-6D53-4F80-9125-384B5C6D7E8F}\\InprocServer32",0U,KEY_QUERY_VALUE,&key);if(rs!=ERROR_SUCCESS){fwprintf(stderr,L"Registered class is not visible through HKCR (error=%ld).\\n",rs);goto done;}(void)RegCloseKey(key);}
 {HRESULT s=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);if(FAILED(s)){fwprintf(stderr,L"CoInitializeEx failed: 0x%08lX.\n",(unsigned long)s);goto done;}com=1;}
 if(!test_regular()){fwprintf(stderr,L"COM regular activation checks failed.\n");goto done;}
 if(!test_aggregation()){fwprintf(stderr,L"COM aggregation checks failed.\n");goto done;}
 CoFreeUnusedLibraries();result=EXIT_SUCCESS;
done:
 if(com)CoUninitialize();if(reg)unregister_server();
 if(h&&p&&!stop_trace(h,p))result=EXIT_FAILURE;if(p)(void)HeapFree(GetProcessHeap(),0U,p);
 if(result==EXIT_SUCCESS)wprintf(L"Real COM server activation, multiple interfaces, and aggregation passed.\n");
 return result;
}