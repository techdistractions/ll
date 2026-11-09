/* LiteLink source guide:
 * Bootstrap diagnostics, LL.INI/options and Winsock 1.1 listener setup.
 * Included by HOST.C. Keep log names 8.3 and use generic-write/seek for Win32s logs.
 * Trumpet misparses the zero IPv4 literal: construct INADDR_ANY directly.
 */

/* Shared native/Win32s startup. Diagnostics precede argument/config parsing. */
typedef struct {char root[260],log[260],bind[32],script[260];u16 port;int quiet,minimized,selftest,listenonly,defaultroot;} HostOptions;
static HostOptions options;
static HANDLE bootlog=INVALID_HANDLE_VALUE;
static char exedir[260],bootpath[260],inipath[260],listen_address[32],listen_text[96];
static char argstorage[4096],*startargv[32];
static unsigned startargc;
static int sockets_started;
static void diaguint(char*s,u32 n){char b[12];unsigned i=0,j=slen(s);do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)s[j++]=b[--i];s[j]=0;}
static void rawlog(HANDLE h,const char*s){DWORD n;if(h!=INVALID_HANDLE_VALUE){WriteFile(h,s,slen(s),&n,0);WriteFile(h,"\r\n",2,&n,0);FlushFileBuffers(h);}}
static void stage_record(const char*state,const char*stage,const char*domain,u32 code,const char*detail){char b[640];textcopy(b,"STARTUP pid=");diaguint(b,GetCurrentProcessId());textcopy(b+slen(b)," ");textcopy(b+slen(b),state);textcopy(b+slen(b)," stage=");textcopy(b+slen(b),stage);textcopy(b+slen(b)," domain=");textcopy(b+slen(b),domain);textcopy(b+slen(b)," code=");diaguint(b,code);if(detail&&*detail){textcopy(b+slen(b)," detail=");if(slen(detail)<400)textcopy(b+slen(b),detail);}
 rawlog(bootlog,b);if(logfile!=INVALID_HANDLE_VALUE)rawlog(logfile,b);OutputDebugStringA(b);}
static void startup_fail(const char*stage,const char*domain,u32 code,const char*detail){char b[1000];HANDLE err;DWORD n;stage_record("FAIL",stage,domain,code,detail);textcopy(b,APP_STARTFAIL);textcopy(b+slen(b),stage);textcopy(b+slen(b),"\r\nError source: ");textcopy(b+slen(b),domain);textcopy(b+slen(b),"\r\nError code: ");diaguint(b,code);textcopy(b+slen(b),"\r\n");if(detail&&slen(detail)<400)textcopy(b+slen(b),detail);textcopy(b+slen(b),"\r\nStartup log: ");textcopy(b+slen(b),bootpath);
 err=GetStdHandle(STD_ERROR_HANDLE);if(err&&err!=INVALID_HANDLE_VALUE)WriteFile(err,b,slen(b),&n,0);
#ifdef GUI
 if(!options.quiet)MessageBoxA(0,b,APP_TITLE,MB_OK|MB_ICONERROR);
#endif
 if(peer!=INVALID_SOCKET)closesocket(peer);if(listener!=INVALID_SOCKET)closesocket(listener);if(sockets_started)WSACleanup();if(logfile!=INVALID_HANDLE_VALUE)CloseHandle(logfile);if(bootlog!=INVALID_HANDLE_VALUE)CloseHandle(bootlog);ExitProcess(2);}
static int starts(const char*s,const char*p){while(*p)if(*s++!=*p++)return 0;return 1;}
static void optioncopy(char*d,const char*s,unsigned cap,const char*stage){if(!s||slen(s)>=cap)startup_fail(stage,"CONFIG",87,"Missing or oversized option value");textcopy(d,s);}
/* Win32s needs generic write access plus an explicit append seek. */
static HANDLE appendlog(const char*name){HANDLE h;
#ifdef LEGACY_WIN32S
 h=CreateFileA(name,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(h!=INVALID_HANDLE_VALUE){SetLastError(0);if(SetFilePointer(h,0,0,FILE_END)==0xffffffffUL&&GetLastError()!=0){DWORD error=GetLastError();CloseHandle(h);SetLastError(error);return INVALID_HANDLE_VALUE;}}
#else
 h=CreateFileA(name,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
#endif
 return h;}
static void bootstrap(void){DWORD n,error;unsigned i;zero(&options,sizeof(options));n=GetModuleFileNameA(0,exedir,sizeof(exedir));if(!n||n>=sizeof(exedir)){error=GetLastError();startup_fail("GetModuleFileNameA","WIN32",error?error:122,"Cannot locate executable directory");}i=slen(exedir);while(i&&exedir[i-1]!='\\')i--;if(!i)startup_fail("GetModuleFileNameA","CONFIG",87,"Executable path has no directory");exedir[i-1]=0;
 if(slen(exedir)>215)startup_fail("bootstrap.path","CONFIG",122,"Executable directory is too long");textcopy(bootpath,exedir);
#ifdef LEGACY_WIN32S
 textcopy(bootpath+slen(bootpath),"\\LLSTART.LOG");
#elif defined(GUI)
 textcopy(bootpath+slen(bootpath),"\\AUMGR.STARTUP.LOG");
#else
 textcopy(bootpath+slen(bootpath),"\\AUHOST.STARTUP.LOG");
#endif
 bootlog=appendlog(bootpath);if(bootlog==INVALID_HANDLE_VALUE){error=GetLastError();startup_fail("bootstrap.CreateFileA","WIN32",error,"Cannot write startup diagnostic log beside executable");}
 stage_record("OK","bootstrap","WIN32",0,APP_BOOT);}
static void parseargs(void){const char*s=GetCommandLineA();unsigned i=0;if(!s)startup_fail("GetCommandLineA","WIN32",GetLastError(),"No command line");while(*s&&i<sizeof(argstorage)-1)argstorage[i++]=*s++;argstorage[i]=0;if(*s)startup_fail("arguments","CONFIG",87,"Command line exceeds 4095 bytes");s=argstorage;startargc=0;
 while(*s){int quoted=0;while(*s==' '||*s=='\t')s++;if(!*s)break;if(startargc==32)startup_fail("arguments","CONFIG",87,"Too many arguments");if(*s=='\"'){quoted=1;s++;}startargv[startargc++]=(char*)s;while(*s&&(quoted?*s!='\"':(*s!=' '&&*s!='\t')))s++;if(*s)*(char*)s++=0;}
 for(i=1;i<startargc;i++)if(same(startargv[i],"--quiet-errors"))options.quiet=1;}
static u16 parseport(const char*s){u32 n=0;if(!*s)return 0;while(*s){if(*s<'0'||*s>'9')return 0;n=n*10+(*s++-'0');if(n>65535)return 0;}return (u16)n;}
static void inivalue(const char*key,const char*def,char*out,unsigned cap){char fallback[260];DWORD n;if(slen(def)>=260)startup_fail("INI.default","CONFIG",122,key);textcopy(fallback,def);n=GetPrivateProfileStringA("Host",key,fallback,out,cap,inipath);if(n>=cap-1)startup_fail("INI.read","CONFIG",122,key);}
static void configure(void){unsigned i;char p[24];const char*v;DWORD attrs,error;parseargs();textcopy(options.root,exedir);textcopy(options.root+slen(options.root),"\\FILES");textcopy(options.log,exedir);
#ifdef GUI
 textcopy(options.log+slen(options.log),"\\AUMGR.LOG");textcopy(options.bind,"0.0.0.0");textcopy(inipath,exedir);textcopy(inipath+slen(inipath),"\\AUMGR.INI");options.defaultroot=1;

#ifndef LITELINK
 attrs=GetFileAttributesA(inipath);if(attrs!=0xffffffffUL){if(attrs&FILE_ATTRIBUTE_DIRECTORY)startup_fail("INI.path","CONFIG",267,"AUMGR.INI is a directory");inivalue("ROOT",options.root,options.root,sizeof(options.root));inivalue("LOG",options.log,options.log,sizeof(options.log));inivalue("BIND","0.0.0.0",options.bind,sizeof(options.bind));inivalue("PORT","21210",p,sizeof(p));options.port=parseport(p);options.defaultroot=0;}else options.port=21210;
#else
 options.port=21210;
#endif

#else
 textcopy(options.log+slen(options.log),"\\AUHOST.LOG");textcopy(options.bind,"127.0.0.1");options.port=21210;
#endif

#ifdef LITELINK
 textcopy(inipath,exedir);textcopy(inipath+slen(inipath),"\\LL.INI");attrs=GetFileAttributesA(inipath);if(attrs==0xffffffffUL||attrs&FILE_ATTRIBUTE_DIRECTORY)startup_fail("LL.INI","CONFIG",2,"Master LL.INI is required beside LLShell32");
 inivalue("ROOT",options.root,options.root,sizeof(options.root));inivalue("LOG",options.log,options.log,sizeof(options.log));inivalue("BIND","0.0.0.0",options.bind,sizeof(options.bind));inivalue("PORT","21210",p,sizeof(p));options.port=parseport(p);options.defaultroot=0;
 GetPrivateProfileStringA("NETWORK","TIMEOUT","60",p,sizeof(p),inipath);i=parseport(p);if(i<2||i>300)startup_fail("LL.INI.TIMEOUT","CONFIG",87,"TIMEOUT must be 2..300 seconds");timeoutms=(u32)i*1000UL;
 GetPrivateProfileStringA("NETWORK","BLOCKSIZE","256",p,sizeof(p),inipath);transferblock=parseport(p);if(transferblock!=128&&transferblock!=256&&transferblock!=512)startup_fail("LL.INI.BLOCKSIZE","CONFIG",87,"BLOCKSIZE must be 128, 256 or 512");
 stage_record("OK","LL.INI","CONFIG",0,inipath);
#endif
 if(startargc>1&&startargv[1][0]!='-'){
#ifdef GUI
  if(startargc<4)startup_fail("arguments","CONFIG",87,"Use root log port [bind-ip], or no arguments, or --root/--log/--port/--bind");optioncopy(options.root,startargv[1],260,"arguments.root");optioncopy(options.log,startargv[2],260,"arguments.log");options.port=parseport(startargv[3]);options.defaultroot=0;i=4;
  if(startargc==6&&same(startargv[4],"--selftest")){options.selftest=1;optioncopy(options.script,startargv[5],260,"arguments.script");i=6;}else if(startargc>4&&startargv[4][0]!='-'){optioncopy(options.bind,startargv[4],32,"arguments.bind");i=5;}
#else
  if(startargc<5)startup_fail("arguments","CONFIG",87,"Use root script log port [bind-ip], or --listen-only with named options");optioncopy(options.root,startargv[1],260,"arguments.root");optioncopy(options.script,startargv[2],260,"arguments.script");optioncopy(options.log,startargv[3],260,"arguments.log");options.port=parseport(startargv[4]);options.defaultroot=0;i=5;if(startargc>5&&startargv[5][0]!='-'){optioncopy(options.bind,startargv[5],32,"arguments.bind");i=6;}
#endif
 }else i=1;
 for(;i<startargc;i++){char*t=startargv[i];v=0;if(same(t,"--quiet-errors")){options.quiet=1;continue;}if(same(t,"--minimized")){options.minimized=1;continue;}if(same(t,"--listen-only")){options.listenonly=1;continue;}
  if(starts(t,"--root=")){v=t+7;optioncopy(options.root,v,260,"arguments.root");options.defaultroot=0;continue;}if(starts(t,"--log=")){optioncopy(options.log,t+6,260,"arguments.log");continue;}if(starts(t,"--bind=")){optioncopy(options.bind,t+7,32,"arguments.bind");continue;}if(starts(t,"--port=")){options.port=parseport(t+7);continue;}
  if(i+1>=startargc)startup_fail("arguments","CONFIG",87,"Missing option value");v=startargv[++i];if(same(t,"--root")){optioncopy(options.root,v,260,"arguments.root");options.defaultroot=0;}else if(same(t,"--log"))optioncopy(options.log,v,260,"arguments.log");else if(same(t,"--bind"))optioncopy(options.bind,v,32,"arguments.bind");else if(same(t,"--port"))options.port=parseport(v);else if(same(t,"--script"))optioncopy(options.script,v,260,"arguments.script");else if(same(t,"--selftest")){options.selftest=1;optioncopy(options.script,v,260,"arguments.script");}else startup_fail("arguments","CONFIG",87,t);
 }
 if(!options.port)startup_fail("arguments.port","CONFIG",87,"TCP port must be decimal 1..65535");
#ifndef GUI
 if(!options.listenonly&&!options.script[0])startup_fail("arguments.script","CONFIG",87,"A script is required, or use --listen-only for native TCP readiness");
#endif
 if(slen(options.root)>239||!options.root[0])startup_fail("configuration.root","CONFIG",87,"Root must be a nonempty path <=239 bytes");
 attrs=GetFileAttributesA(options.root);if(attrs==0xffffffffUL&&options.defaultroot){if(!CreateDirectoryA(options.root,0)){error=GetLastError();if(error!=ERROR_ALREADY_EXISTS)startup_fail("CreateDirectoryA(root)","WIN32",error,options.root);}attrs=GetFileAttributesA(options.root);}
 if(attrs==0xffffffffUL){error=GetLastError();startup_fail("GetFileAttributesA(root)","WIN32",error,options.root);}if(!(attrs&FILE_ATTRIBUTE_DIRECTORY))startup_fail("configuration.root","CONFIG",267,"Root is not a directory");
 logfile=appendlog(options.log);if(logfile==INVALID_HANDLE_VALUE){error=GetLastError();startup_fail("CreateFileA(log)","WIN32",error,options.log);}
 textcopy(basedir,options.root);stage_record("OK","configuration","CONFIG",0,options.root);stage_record("OK","logfile","WIN32",0,options.log);
#ifdef LITELINK
 stage_record("OK","LL.INI","CONFIG",0,inipath);
#endif
}
static int ipv4literal(const char*s){unsigned i,n,digits;for(i=0;i<4;i++){n=digits=0;while(*s>='0'&&*s<='9'){n=n*10+(*s++-'0');if(++digits>3||n>255)return 0;}if(!digits)return 0;if(i<3){if(*s++!='.')return 0;}else if(*s)return 0;}return 1;}
static void start_network(void){WSADATA wd;struct sockaddr_in a;int r,accepting,len,exclusive=1;u32 error;char detail[160];
#ifdef LEGACY_WIN32S
 r=WSAStartup(0x0101,&wd);
#else
 r=WSAStartup(0x0202,&wd);
#endif
 if(r)startup_fail("WSAStartup","WINSOCK",r,"Winsock initialization failed (direct return code)");sockets_started=1;textcopy(detail,"negotiated=");diaguint(detail,wd.wVersion&255);textcopy(detail+slen(detail),".");diaguint(detail,wd.wVersion>>8);stage_record("OK","WSAStartup","WINSOCK",0,detail);
 if(!ipv4literal(options.bind)||same(options.bind,"255.255.255.255"))startup_fail("bind.address","CONFIG",87,"Bind must be a dotted IPv4 literal; 0.0.0.0 means all local IPv4 interfaces");
 listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==INVALID_SOCKET){error=WSAGetLastError();startup_fail("socket","WINSOCK",error,options.bind);}stage_record("OK","socket","WINSOCK",0,"AF_INET SOCK_STREAM IPPROTO_TCP");
#ifndef LEGACY_WIN32S
 if(setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&exclusive,sizeof(exclusive))==SOCKET_ERROR){error=WSAGetLastError();startup_fail("setsockopt(SO_EXCLUSIVEADDRUSE)","WINSOCK",error,"Exclusive ownership of listening endpoint");}
#endif
 zero(&a,sizeof(a));a.sin_family=AF_INET;/* Trumpet 1.0 Rev A inet_addr("0.0.0.0") returns INADDR_NONE. */a.sin_addr.s_addr=same(options.bind,"0.0.0.0")?0:inet_addr(options.bind);a.sin_port=htons(options.port);
 textcopy(detail,options.bind);textcopy(detail+slen(detail),":");diaguint(detail,options.port);
 if(bind(listener,(struct sockaddr*)&a,sizeof(a))==SOCKET_ERROR){error=WSAGetLastError();startup_fail("bind","WINSOCK",error,detail);}stage_record("OK","bind","WINSOCK",0,detail);
 if(listen(listener,4)==SOCKET_ERROR){error=WSAGetLastError();startup_fail("listen","WINSOCK",error,detail);}stage_record("OK","listen","WINSOCK",0,detail);
 len=sizeof(a);if(getsockname(listener,(struct sockaddr*)&a,&len)==SOCKET_ERROR){error=WSAGetLastError();startup_fail("getsockname","WINSOCK",error,detail);}textcopy(listen_address,inet_ntoa(a.sin_addr));textcopy(listen_text,"LISTENING ");textcopy(listen_text+slen(listen_text),listen_address);textcopy(listen_text+slen(listen_text),":");diaguint(listen_text,ntohs(a.sin_port));
 accepting=0;len=sizeof(accepting);if(getsockopt(listener,SOL_SOCKET,SO_ACCEPTCONN,(char*)&accepting,&len)==SOCKET_ERROR){error=WSAGetLastError();startup_fail("getsockopt(SO_ACCEPTCONN)","WINSOCK",error,detail);}if(!accepting)startup_fail("SO_ACCEPTCONN","WINSOCK",0,"listen returned without accept state");stage_record("OK","SO_ACCEPTCONN","WINSOCK",0,"value=1");logline(listen_text);}
static void finish_network(void){if(peer!=INVALID_SOCKET){closesocket(peer);peer=INVALID_SOCKET;}if(listener!=INVALID_SOCKET){closesocket(listener);listener=INVALID_SOCKET;}if(sockets_started){WSACleanup();sockets_started=0;}stage_record("OK","shutdown","WIN32",0,"normal application shutdown");if(logfile!=INVALID_HANDLE_VALUE)CloseHandle(logfile);if(bootlog!=INVALID_HANDLE_VALUE)CloseHandle(bootlog);}
