/* LiteLink source guide:
 * DOS entry point and RPC/file engine. Textually includes JOBS.C and SHELLUI.C.
 * Build with LITELINK and LL_UI for the foreground manager. No resident EXEC parent.
 * Transport hooks are implemented by SHELLNET.ASM and the NNET*.INC adapters.
 */

#include "au.h"
u8 netstate[5120];
extern int __cdecl net_open(void);
extern int __cdecl net_send(const void*,unsigned);
extern int __cdecl net_recv(void*,unsigned);
extern void __cdecl net_close(void);
extern u16 __cdecl net_ticks(void);
static int dopen(const char*);
#pragma aux dopen = "mov ax,3d00h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
static int dcreate(const char*);
#pragma aux dcreate = "mov ah,3ch" "xor cx,cx" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax cx];
static int dread(int,void*,unsigned);
#pragma aux dread = "mov ah,3fh" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
static int dwrite(int,const void*,unsigned);
#pragma aux dwrite = "mov ah,40h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
static int dclose(int);
#pragma aux dclose = "mov ah,3eh" "int 21h" "sbb ax,ax" parm [bx] value [ax] modify [ax];
static int ddelete(const char*);
#pragma aux ddelete = "mov ah,41h" "int 21h" "sbb ax,ax" parm [dx] value [ax] modify [ax];
static int drename(const char*,const char*);
#pragma aux drename = "push es" "push ds" "pop es" "mov ah,56h" "int 21h" "sbb ax,ax" "pop es" parm [dx] [di] value [ax] modify [ax];
static int dchdir(const char*);
#pragma aux dchdir = "mov ah,3bh" "int 21h" "sbb ax,ax" parm [dx] value [ax] modify [ax];
static int dgetdir(char*);
#pragma aux dgetdir = "mov ah,47h" "xor dl,dl" "int 21h" "sbb ax,ax" parm [si] value [ax] modify [ax dx];
static u16 dgetdrive(void);
#pragma aux dgetdrive = "mov ah,19h" "int 21h" "xor ah,ah" value [ax] modify [ax];
static void dsetdrive(u16);
#pragma aux dsetdrive = "mov ah,0eh" "int 21h" parm [dx] modify [ax];
static char original[65];static u16 originaldrive;
static void dsetdta(void*);
#pragma aux dsetdta = "mov ah,1ah" "int 21h" parm [dx] modify [ax];
static int dfind(const char*);
#pragma aux dfind = "mov ah,4eh" "mov cx,10h" "int 21h" "sbb ax,ax" parm [dx] value [ax] modify [ax cx];
static int dnext(void);
#pragma aux dnext = "mov ah,4fh" "int 21h" "sbb ax,ax" value [ax] modify [ax];
static int dmkdir(const char*);
#pragma aux dmkdir = "mov ah,39h" "int 21h" "sbb ax,ax" parm [dx] value [ax] modify [ax];
static u8 dta[43];static char remotecwd[64];
static u16 dosversion(void);
#pragma aux dosversion = "mov ah,30h" "int 21h" value [ax] modify [bx cx];
#ifdef LITELINK
static u16 manager_mode=1;
#else
static u16 manager_mode=0;
#endif
static int last_file_status;static void ui_idle(void);static int ui_output(const char*);static void ui_save(void);
static u16 allowrun,jobmem,macmode,nictype,idport=0x110,macformat_seen;static char macoverride[18];
static u16 logfd=0xffff,timeout,retries,retryticks,port,nic;
static char root[64],request[13],host[16];
static u8 ip[4],mask[4],gate[4];
static u8 filebuf[BLOCK];static u16 transferblock=BLOCK;
static char inibuf[2048];
static void logline(const char*s){if(logfd!=0xffff){dwrite(logfd,s,slen(s));dwrite(logfd,"\r\n",2);}}
static void lognum(const char*s,u16 n){char b[6];unsigned i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);logline(s);while(i)dwrite(logfd,&b[--i],1);dwrite(logfd,"\r\n",2);}
/* Foreground DOS handle 1 only; no timer, IRQ hook or resident screen state. */
static void screenpart(const char*s){if(!ui_output(s))dwrite(1,s,slen(s));}
static void screenline(const char*s){screenpart("LL: ");screenpart(s);screenpart("\r\n");}
static void screenvalue(u32 n){static const u32 places[]={1000000000UL,100000000UL,10000000UL,1000000UL,100000UL,10000UL,1000UL,100UL,10UL,1UL};unsigned i;int started=0;char digit;for(i=0;i<10;i++){digit='0';while(n>=places[i]){n-=places[i];digit++;}if(digit!='0'||started||i==9){char b[2];b[0]=digit;b[1]=0;screenpart(b);started=1;}}}
static void screenerror(const char*s,u16 code){screenpart("LL: ");screenpart(s);screenpart(" (code ");screenvalue(code);screenpart("). See AGENT.LOG.\r\n");}
static void transferstatus(const char*verb,const char*name,u32 done,u32 total){screenpart("LL: ");screenpart(verb);screenpart(" ");screenpart(name);screenpart(" - ");screenvalue(done);screenpart(" / ");screenvalue(total);screenpart(" bytes\r\n");}
static int number(const char*s,u16*out,int hex){u16 n=0,x;unsigned base=hex?16:10;if(!*s)return 0;if(hex&&s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2;while(*s){char c=*s++;if(c>='0'&&c<='9')x=c-'0';else if(hex&&c>='A'&&c<='F')x=c-'A'+10;else if(hex&&c>='a'&&c<='f')x=c-'a'+10;else return 0;if(x>=base||n>(65535U-x)/base)return 0;n=n*base+x;}*out=n;return 1;}
static int addr(const char*s,u8*a){unsigned i,j;u16 n;char b[4];for(i=0;i<4;i++){j=0;while(*s&&*s!='.'){if(j==3)return 0;b[j++]=*s++;}b[j]=0;if(!number(b,&n,0)||n>255)return 0;a[i]=(u8)n;if(i<3){if(*s++!='.')return 0;}else if(*s)return 0;}return 1;}
static int macconfig(const char*s){unsigned i;u16 n;char b[3];u8 any=0,first=0;if(slen(s)!=17)return 0;for(i=0;i<6;i++){b[0]=s[i*3];b[1]=s[i*3+1];b[2]=0;if(!number(b,&n,1))return 0;if(i<5&&s[i*3+2]!=':')return 0;if(!i)first=(u8)n;any|=(u8)n;}return any&&!(first&1);}
#define LL_OPEN dopen
#define LL_LEN slen
#define LL_COPY textcopy
#include "LLPATH.H"
static int config(void){int f,n;char*s,*e,*v;u16 seen=0,tmp;int skipsection=0;
#ifdef LITELINK
f=ll_open_master();
#else
f=dopen("AUTOUAT.INI");
#endif
if(f<0)return 0;n=dread(f,inibuf,sizeof(inibuf)-1);dclose(f);if(n<1||n==sizeof(inibuf)-1)return 0;inibuf[n]=0;s=inibuf;
 while(*s){while(*s=='\r'||*s=='\n')s++;if(!*s)break;e=s;while(*e&&*e!='\r'&&*e!='\n')e++;if(*e)*e++=0;
 if(*s=='['){
#ifdef LITELINK
 skipsection=!(same(s,"[NETWORK]")||same(s,"[SHELL]")||same(s,"[EXECUTION]"));
#endif
 s=e;continue;}if(*s==';'||*s=='#'||skipsection){s=e;continue;}v=s;while(*v&&*v!='=')v++;if(!*v)return 0;*v++=0;
 if(same(s,"DNS")||same(s,"IRQ")){}
 else if(same(s,"BLOCKSIZE")){if(!number(v,&transferblock,0)||(transferblock!=128&&transferblock!=256&&transferblock!=512))return 0;}
 else if(same(s,"MAC")){if(!macconfig(v))return 0;textcopy(macoverride,v);}
 else if(same(s,"MACFORMAT")){macformat_seen=1;if(same(v,"AUTO"))macmode=0;else if(same(v,"DOUBLED"))macmode=1;else if(same(v,"PACKED"))macmode=2;else return 0;}
 else if(same(s,"NIC")){if(same(v,"NE2000"))nictype=0;else if(same(v,"PICOMEM"))nictype=1;else if(same(v,"3C509")||same(v,"3C509B"))nictype=2;else return 0;}
 else if(same(s,"IDPORT")){if(!number(v,&idport,1)||idport<0x100||idport>0x1f0||(idport&15))return 0;}
 else if(same(s,"NICBASE")){if(!number(v,&nic,1)||nic<0x200||nic>0x3e0||(nic&15))return 0;seen|=1;}
 else if(same(s,"IP")){if(!addr(v,ip))return 0;seen|=2;}
 else if(same(s,"NETMASK")){if(!addr(v,mask))return 0;seen|=4;}
 else if(same(s,"GATEWAY")){if(!addr(v,gate))return 0;seen|=8;}
 else if(same(s,"HOST")){u8 a[4];if(slen(v)>15||!addr(v,a))return 0;textcopy(host,v);seen|=16;}
 else if(same(s,"PORT")){if(!number(v,&port,0)||!port)return 0;seen|=32;}
 else if(same(s,"ROOT")){if(!*v||slen(v)>63)return 0;textcopy(root,v);seen|=64;}
 else if(same(s,"TIMEOUT")){if(!number(v,&tmp,0)||tmp<2||tmp>300)return 0;timeout=tmp*18;seen|=128;}
 else if(same(s,"RETRIES")){if(!number(v,&retries,0)||retries>100)return 0;seen|=256;}
 else if(same(s,"RETRYDELAY")){if(!number(v,&tmp,0)||tmp>60)return 0;retryticks=tmp*18;seen|=512;}
 else if(same(s,"ALLOWRUN")){if(same(v,"ON"))allowrun=1;else if(same(v,"OFF"))allowrun=0;else return 0;}
 else if(same(s,"RUNTIMEOUT")){if(!number(v,&tmp,0))return 0;}
 else if(same(s,"OUTPUTLIMIT")){if(!number(v,&tmp,0))return 0;}
 else if(same(s,"JOBMEM")){if(same(v,"ON"))jobmem=1;else if(same(v,"OFF"))jobmem=0;else return 0;}
 else if(same(s,"MODE")){if(same(v,"MANAGER"))manager_mode=1;else if(same(v,"AGENT"))manager_mode=0;else return 0;}
 else if(same(s,"REQUEST")){if(*v&&!valid(v))return 0;textcopy(request,v);}
 else return 0;s=e;
 }if(nictype!=2&&(nic&31))return 0;if(nictype==1&&!macformat_seen)macmode=2;return seen==1023;}
static int wireout(const u8*p,unsigned n){return net_send(p,n)==0;}
static int wirein(u8*p,unsigned n){u16 t=net_ticks();int r;while(n){r=net_recv(p,n);if(r<0)return 0;if(r){n-=r;p+=r;t=net_ticks();}else if((u16)(net_ticks()-t)>=timeout)return 0;}return 1;}
static int exists(const char*s){int f=dopen(s);if(f<0)return 0;dclose(f);return 1;}
static int recvfile(void){char name[13];u32 size,cwant,c=0xffffffffUL,total=0;int f,k,err=OK,n;
 last_file_status=INVALID;if(plen!=21)return statusout(READY,INVALID);copy(name,payload,13);if(name[12]!=0||!valid(name))return statusout(READY,INVALID);size=get32(payload+13);cwant=get32(payload+17);
 if(exists(name)){last_file_status=EXISTS;return statusout(READY,EXISTS);}
 /* A single DOS process owns this fixed reserved staging name. It is never a valid remote name. */
 if(exists("AUTOUAT.$$$")){last_file_status=IOERR;return statusout(READY,IOERR);}
 f=dcreate("AUTOUAT.$$$");if(f<0){last_file_status=IOERR;return statusout(READY,IOERR);}
 transferstatus("Receiving",name,0,size);
 if(!statusout(READY,OK)){dclose(f);ddelete("AUTOUAT.$$$");return 0;}
 while(1){k=framein();if(k==END&&plen==0)break;if(k!=DATA||!plen){err=PROTOCOL;break;}n=plen;if(total>size||(u32)n>size-total){err=INTEGRITY;break;}if(dwrite(f,payload,n)!=n){err=IOERR;break;}total+=n;c=crcstep(c,payload,n);if(total&&!(total&8191UL)&&total<size)transferstatus("Receiving",name,total,size);}
 if(dclose(f)<0)err=IOERR;if(total!=size||(c^0xffffffffUL)!=cwant)err=INTEGRITY;
 if(err==OK){if(drename("AUTOUAT.$$$",name)<0)err=IOERR;}
 if(err!=OK){ddelete("AUTOUAT.$$$");screenerror("Receive failed",err);}else{transferstatus("Received",name,total,size);screenline("Receive complete; CRC verified. Waiting for host requests.");}last_file_status=err;lognum(name,err);if(k<0||k==DATA)return 0;return statusout(RESULT,err);}
static int sendfile(const char*name){int f,n,st;u32 size=0,sent=0,c=0xffffffffUL;u8 meta[21];
 last_file_status=INVALID;if(!valid(name))return statusout(RESULT,INVALID);f=dopen(name);if(f<0){last_file_status=MISSING;return statusout(RESULT,MISSING);}
 while((n=dread(f,filebuf,transferblock))>0){size+=n;c=crcstep(c,filebuf,n);}dclose(f);if(n<0)return statusout(RESULT,IOERR);
 zero(meta,21);textcopy((char*)meta,name);set32(meta+13,size);set32(meta+17,c^0xffffffffUL);
 transferstatus("Sending",name,0,size);
 if(!frameout(PUT,meta,21))return 0;st=statusin(READY);last_file_status=st;if(st<0)return 0;if(st!=OK)return 1;f=dopen(name);if(f<0)return 0;
 while((n=dread(f,filebuf,transferblock))>0){if(!frameout(DATA,filebuf,n)){dclose(f);return 0;}sent+=n;if(sent&&!(sent&8191UL)&&sent<size)transferstatus("Sending",name,sent,size);}dclose(f);if(n<0)return 0;
 if(!frameout(END,0,0))return 0;st=statusin(RESULT);last_file_status=st;lognum(name,(u16)st);if(st==OK){transferstatus("Sent",name,sent,size);screenline("Send complete; host verified CRC. Waiting for host requests.");}else screenerror("Send failed",(u16)st);return st>=0;}

static int listdir(void){u8 ent[22];int rc;dsetdta(dta);rc=dfind("*.*");while(rc==0){char*name=(char*)dta+30;if(valid(name)){zero(ent,22);textcopy((char*)ent,name);copy(ent+13,dta+26,4);ent[17]=dta[21];copy(ent+18,dta+24,2);copy(ent+20,dta+22,2);if(!frameout(ENTRY,ent,22))return 0;}rc=dnext();}return statusout(RESULT,OK);}
static int fileop(int kind){char a[13],b[13];int n,i,st=OK;unsigned len=slen(remotecwd);if(kind==RENAME){if(plen<4||plen>26)return statusout(RESULT,INVALID);n=0;while(n<plen&&payload[n])n++;if(n==plen||n>12||plen-n-1>13||payload[plen-1])return statusout(RESULT,INVALID);copy(a,payload,n+1);copy(b,payload+n+1,plen-n-1);if(!valid(a)||!valid(b))return statusout(RESULT,INVALID);if(exists(b))st=EXISTS;else if(drename(a,b)<0)st=IOERR;}
 else {if(plen<2||plen>13||payload[plen-1])return statusout(RESULT,INVALID);copy(a,payload,plen);
 if(kind==CHDIR&&same(a,".")){}
 else if(kind==CHDIR&&same(a,"..")){if(!len)st=OK;else if(dchdir("..")<0)st=IOERR;else{while(len&&remotecwd[len-1]!='\\')len--;if(len)len--;remotecwd[len]=0;}}
 else if(!valid(a))st=INVALID;
 else if(kind==CHDIR){if(slen(root)+len+slen(a)+2>=63)st=INVALID;else if(dchdir(a)<0)st=IOERR;else{if(len)remotecwd[len++]='\\';textcopy(remotecwd+len,a);}}
 else if(kind==MKDIR){if(slen(root)+len+slen(a)+2>=63)st=INVALID;else if(dmkdir(a)<0)st=IOERR;}
 else if(kind==AU_DELETE){if(ddelete(a)<0)st=IOERR;}}
 return statusout(RESULT,st);}

#include "JOBS.C"
#include "TREE.C"
#include "SHELLUI.C"
static int session(void){int k;char name[13];u8 hello[14];txid=rxid=0;remotecwd[0]=0;
 
#ifdef LITELINK
 copy(hello,"LLSHELL 0.012",13);
#else
 copy(hello,"AUTOUAT 0.0.7",13);
#endif
hello[13]=(resume_pending?2:(request[0]?1:0))|(manager_mode?LL_CAP_MANAGER:0);
 if(!frameout(HELLO,hello,14)||framein()!=WELCOME||(plen!=0&&plen!=2))return 0;peer_tree=plen==2?(get16(payload)&1):0;logline("HANDSHAKE PASS");screenline("Connected - waiting for host requests.");
 if(resume_pending){if(!resume_job())return 0;request[0]=0;}
 if(request[0]){if(!frameout(REQUEST,(u8*)request,slen(request)+1))return 0;k=framein();if(k==RESULT&&plen==2){lognum("REQUEST error",get16(payload));}else if(k!=PUT||!recvfile())return 0;request[0]=0;}
 while(1){k=framein();if(k>=0&&resume_acked){if(!clear_job())return 0;}if(k==QUIT&&plen==0){logline("SESSION PASS");screenline("Host closed the session.");return 1;}
 if(k==LL_POLL&&manager_mode){if(!ui_poll())return 0;if(job_handoff||ui_quitting)return 1;}
 else if(k==LL_UI_TEST){
#ifdef LL_TEST
 if(!ui_test_input())return 0;
#else
 return 0;
#endif
 }
 else if(k==JOB_SUBMIT||k==AU_RUN||k==AU_CMD){if(!jobrequest(k))return 0;if(job_handoff)return 1;}
 else if(k==LL_TREE){if(!tree_server(payload,plen))return 0;}
 else if(k==LIST&&plen==0){if(!listdir())return 0;}
 else if(k==CHDIR||k==MKDIR||k==RENAME||k==AU_DELETE){screenline("Host requested a directory or file operation.");if(!fileop(k))return 0;u_refresh=1;}
 else if(k==PUT){if(!recvfile())return 0;u_refresh=1;}
 else if(k==GET){if(!get_request())return 0;}
 else return 0;}}
int __cdecl agent_main(void){u16 attempt,ver;int ok=0;
#ifdef LITELINK
screenline("LiteLink LLShell 0.0.12");
#else
screenline("AUTOUAT 0.0.7 - starting DOS network agent.");
#endif
logfd=log_open();
#ifdef LITELINK
logline("LiteLink LLShell 0.0.12");
#else
logline("AUTOUAT 0.0.7 DOS polling agent");
#endif
ver=dosversion();lognum("DOS version (minor:major hex encoded as decimal)",ver);
 if((ver&255)<3||((ver&255)==3&&(ver>>8)<30)){logline("Requires DOS 3.3+");screenline("Requires DOS 3.3+. See AGENT.LOG.");return 2;}
 if(!config()){logline("Invalid/missing configuration INI");screenline("Invalid/missing configuration INI. See AGENT.LOG.");return 2;}logline(nictype==2?"NIC=3C509B native byte-I/O polling":(nictype==1?"NIC=PICOMEM":"NIC=NE2000"));screenline(nictype==2?"NIC=3C509B - native polling, no resident driver.":(nictype==1?"NIC=PICOMEM - native polling.":"NIC=NE2000 - native polling."));screenpart("LL: Shared directory ");screenpart(root);screenpart("\r\n");originaldrive=dgetdrive();original[0]='\\';if(dgetdir(original+1)<0)return 2;
 if(!job_home()){logline("Invalid control home");screenline("Invalid control home. See AGENT.LOG.");return 2;}
 if(root[1]==':'&&root[0]>='A'&&root[0]<='Z')dsetdrive(root[0]-'A');
 if(dchdir(root)<0){logline("Configured ROOT unavailable");screenline("Configured ROOT unavailable. See AGENT.LOG.");dsetdrive(originaldrive);dchdir(original);return 2;}
 if(!job_start()){logline("Job state startup failure");screenline("Job state startup failure. See AGENT.LOG.");dsetdrive(originaldrive);dchdir(original);return 2;}
 if(manager_mode)ui_start();
 /* Remove only our reserved orphan after a prior interrupted run. */
 ddelete("AUTOUAT.$$$");
 for(attempt=0;manager_mode||attempt<=retries;attempt++){if(ui_quitting){ok=1;break;}zero(netstate,sizeof(netstate));set16(netstate+4,port);set16(netstate+6,timeout);set16(netstate+18,slen(host));copy(netstate+20,host,slen(host));set16(netstate+3912,nic);copy(netstate+3978,macoverride,slen(macoverride)+1);set16(netstate+4050,macmode);set16(netstate+4052,nictype);set16(netstate+4054,idport);copy(netstate+3996,ip,4);copy(netstate+4000,mask,4);copy(netstate+4004,gate,4);
 screenpart("LL: Connecting to ");screenpart(host);screenpart(":");screenvalue(port);screenpart(" (attempt ");screenvalue((u16)(attempt+1));screenpart(")...\r\n");lognum("CONNECT attempt",attempt);if(net_open()==0){logline("NIC ARP TCP PASS");screenline("TCP connected; negotiating with host...");ok=session();if(!ok)screenline("Connection or protocol failed. See AGENT.LOG.");}else{lognum("NETWORK error",get16(netstate+3884));screenerror("Network connection failed",get16(netstate+3884));if(nictype==2){lognum("3Com last stage",get16(netstate+4062));lognum("3Com hardware status",get16(netstate+4060));if(get16(netstate+3884)==430)screenline("3Com active/resident driver: clean boot without packet driver.");if(get16(netstate+3884)==432)screenline("Remove MAC= override for the 3Com card.");}}net_close();if(ok)break;u_connected=0;u_dirty=1;while(remotecwd[0]){unsigned z=slen(remotecwd);dchdir("..");while(z&&remotecwd[z-1]!='\\')z--;if(z)z--;remotecwd[z]=0;}ddelete("AUTOUAT.$$$");logline("Retry after failure");if(manager_mode||attempt<retries)screenline("Waiting to retry connection...");{u16 t=net_ticks(),wait=retryticks,shift=attempt>4?4:attempt;if(manager_mode){while(shift--&&wait<540)wait*=2;if(wait>1080)wait=1080;}while((u16)(net_ticks()-t)<wait){ui_idle();if(ui_quitting)break;}}if(manager_mode&&attempt>100)attempt=100;}
 ui_shutdown();logline(ok?"EXIT PASS":"EXIT FAIL");if(!ok)screenline("Agent stopped after failure. See AGENT.LOG.");else if(!job_handoff)screenline("Agent stopped normally.");dclose(logfd);dsetdrive(originaldrive);dchdir(original);return ok?0:1;}
