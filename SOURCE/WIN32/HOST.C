/* LiteLink source guide:
 * Universal x86 LLShell32 transport, files, durable RPC journal and script driver.
 * Compile GUI + LITELINK + LEGACY_WIN32S for the ONE EXE supported on both OSes.
 * LEGACY_WIN32S selects the shared API subset, not a second modern-Windows edition.
 * Includes STARTUP.INC and GUI.INC; the latter includes PEER.C and QUEUE.C.
 */

#define WIN32_LEAN_AND_MEAN
#ifdef LEGACY_WIN32S
#include <windows.h>
#include <winsock.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include "au.h"
#if defined(LITELINK) && defined(LEGACY_WIN32S)
#define LLHOSTVER "0.0.14"
#else
#define LLHOSTVER "0.0.9"
#endif
#ifdef LITELINK
#define APP_STARTFAIL "LiteLink LLShell32 " LLHOSTVER " startup failed\r\nStage: "
#define APP_TITLE "LiteLink startup error"
#define APP_BOOT "LiteLink LLShell32 " LLHOSTVER "; Windows socket startup"
#else
#define APP_STARTFAIL "AUTOUAT 0.0.4 startup failed\r\nStage: "
#define APP_TITLE "AUTOUAT startup error"
#define APP_BOOT "AUTOUAT 0.0.4; native Windows socket startup"
#endif

static SOCKET peer=INVALID_SOCKET,listener=INVALID_SOCKET;
static HANDLE logfile=INVALID_HANDLE_VALUE;
static char basedir[260];
static u32 timeoutms=30000;
static u8 filebuf[BLOCK];static u16 transferblock=BLOCK;
static int failures,request_pending,remote_resume,remote_interactive,peer_tree,transport_failed,last_transfer_status;static char peerfolder[64];static u32 peer_ip;
static u16 accept_timeout=30,resume_wait=300;
static u32 next_job;
#ifdef GUI
static void progress(u32,u32);
#define PROGRESS(a,b) progress(a,b)
#else
#define PROGRESS(a,b)
#endif
static void logline(const char*s){DWORD n;if(logfile!=INVALID_HANDLE_VALUE){WriteFile(logfile,s,slen(s),&n,0);WriteFile(logfile,"\r\n",2,&n,0);FlushFileBuffers(logfile);}}
static void lognum(const char*s,u32 n){char b[12];unsigned i=0;logline(s);do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i){DWORD z;WriteFile(logfile,b+--i,1,&z,0);}logline("");}
#include "STARTUP.INC"
static int path(char*out,const char*name){if(!valid(name)||slen(basedir)+slen(name)+2>=260)return 0;textcopy(out,basedir);textcopy(out+slen(out),"\\");textcopy(out+slen(out),name);return 1;}
#ifdef GUI
static void pump_gui(void);
#endif
static int waitio(int sending){fd_set f;struct timeval tv;DWORD start=GetTickCount();int r;do{FD_ZERO(&f);FD_SET(peer,&f);tv.tv_sec=0;tv.tv_usec=250000;r=select(0,sending?0:&f,sending?&f:0,0,&tv);if(r>0)return 1;if(r<0)return 0;
#ifdef GUI
 pump_gui();
#endif
 }while(GetTickCount()-start<timeoutms);return 0;}
static int wireout(const u8*p,unsigned n){int r;while(n){if(!waitio(1)){transport_failed=1;return 0;}r=send(peer,(const char*)p,n,0);if(r<=0){transport_failed=1;return 0;}p+=r;n-=r;}return 1;}
static int wirein(u8*p,unsigned n){int r;unsigned original=n;while(n){if(!waitio(0)){lognum("wirein timeout remaining",n);lognum("wirein original",original);transport_failed=1;return 0;}r=recv(peer,(char*)p,n,0);if(r<=0){transport_failed=1;return 0;}p+=r;n-=r;}return 1;}
static int writeall(HANDLE f,const u8*p,DWORD n){DWORD z;return WriteFile(f,p,n,&z,0)&&z==n;}
static int recvfile(const char*local){char name[13],dest[260],tmp[260];u32 size,want,total=0,c=0xffffffffUL;HANDLE f;int k,err=OK;
 last_transfer_status=INVALID;if(plen!=21)return statusout(READY,INVALID);copy(name,payload,13);if(name[12]||!valid(name)||!path(dest,local?local:name))return statusout(READY,INVALID);
 size=get32(payload+13);want=get32(payload+17);if(GetFileAttributesA(dest)!=0xffffffffUL){last_transfer_status=EXISTS;return statusout(READY,EXISTS);}
 textcopy(tmp,basedir);textcopy(tmp+slen(tmp),"\\AUTOUAT.$$$");f=CreateFileA(tmp,GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE){last_transfer_status=IOERR;return statusout(READY,IOERR);}
 if(!statusout(READY,OK)){CloseHandle(f);DeleteFileA(tmp);return 0;}
 for(;;){k=framein();if(k==END&&!plen)break;if(k!=DATA||!plen){err=PROTOCOL;break;}if(total>size||(u32)plen>size-total){err=INTEGRITY;break;}if(!writeall(f,payload,plen)){err=IOERR;break;}total+=plen;c=crcstep(c,payload,plen);PROGRESS(total,size);}
 if(!FlushFileBuffers(f))err=IOERR;if(!CloseHandle(f))err=IOERR;if(total!=size||(c^0xffffffffUL)!=want)err=INTEGRITY;
 if(err==OK&&!MoveFileA(tmp,dest))err=IOERR;if(err!=OK)DeleteFileA(tmp);
 last_transfer_status=err;lognum(name,err);if(k<0||k==DATA){transport_failed=1;return 0;}return statusout(RESULT,err)&&err==OK;}
/* Returns status code; -1 denotes a broken protocol stream. */
static int sendfile(const char*local,const char*remote,int badcrc,int drop){char src[260];HANDLE f;DWORD n;u32 size=0,done=0,c=0xffffffffUL;u8 meta[21];int st;
 if(!valid(remote)||!path(src,local))return INVALID;
 f=CreateFileA(src,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return MISSING;
 while(1){if(!ReadFile(f,filebuf,transferblock,&n,0)){CloseHandle(f);return IOERR;}if(!n)break;size+=n;c=crcstep(c,filebuf,n);}
 if(SetFilePointer(f,0,0,FILE_BEGIN)==0xffffffffUL){CloseHandle(f);return IOERR;}
 zero(meta,21);textcopy((char*)meta,remote);set32(meta+13,size+(badcrc==2?1:0));set32(meta+17,(c^0xffffffffUL)^(badcrc==1?1:0));
 if(!frameout(PUT,meta,21)){CloseHandle(f);return -1;}st=statusin(READY);if(st!=OK){CloseHandle(f);return st;}
 if(drop){ReadFile(f,filebuf,transferblock,&n,0);if(n)frameout(DATA,filebuf,n);CloseHandle(f);logline("Injected transport interruption");return -2;}
 while(1){if(!ReadFile(f,filebuf,transferblock,&n,0)){CloseHandle(f);return -1;}if(!n)break;if(!frameout(DATA,filebuf,(unsigned)n)){CloseHandle(f);return -1;}done+=n;PROGRESS(done,size);}
 CloseHandle(f);if(!frameout(END,0,0))return -1;st=statusin(RESULT);lognum(remote,st);return st;}
static int download(const char*remote,const char*local,int expected){int k,st;if(!frameout(GET,(const u8*)remote,slen(remote)+1))return 0;k=framein();if(k==RESULT&&plen==2){st=get16(payload);last_transfer_status=st;return st==expected;}if(k!=PUT){transport_failed=1;return 0;}if(expected!=OK)return 0;st=recvfile(local);return st&&last_transfer_status==OK;}
static int invalidprobe(const char*name,int expected){u8 meta[21];zero(meta,21);if(slen(name)>12)return 0;textcopy((char*)meta,name);if(!frameout(PUT,meta,21))return 0;return statusin(READY)==expected;}

static int remoteop(int kind,const char*a,const char*b,int expected){u8 data[26];unsigned n=slen(a)+1;if(n>13)return 0;copy(data,a,n);if(b){unsigned m=slen(b)+1;if(m>13)return 0;copy(data+n,b,m);n+=m;}return frameout(kind,data,n)&&statusin(RESULT)==expected;}
static int remotelist(void){int k;unsigned count=0;if(!frameout(LIST,0,0))return 0;while((k=framein())==ENTRY){if(plen!=22||payload[12]||!valid((char*)payload))return 0;logline((char*)payload);if(++count>4096)return 0;}return k==RESULT&&plen==2&&get16(payload)==OK;}
#include "FS.C"
static int acceptpeer(void){fd_set f;struct timeval tv;int k,n=sizeof(struct sockaddr_in);struct sockaddr_in from;char req[80];FD_ZERO(&f);FD_SET(listener,&f);tv.tv_sec=accept_timeout;tv.tv_usec=0;if(select(0,&f,0,0,&tv)<=0)return 0;peer=accept(listener,(struct sockaddr*)&from,&n);if(peer==INVALID_SOCKET){stage_record("FAIL","accept","WINSOCK",WSAGetLastError(),"Peer accept failed");return 0;}textcopy(req,"TCP ACCEPT peer=");textcopy(req+slen(req),inet_ntoa(from.sin_addr));textcopy(req+slen(req),":");diaguint(req,ntohs(from.sin_port));logline(req);peer_ip=from.sin_addr.s_addr;txid=rxid=0;k=framein();if(k!=HELLO||plen!=14||payload[13]>7)return 0;copy(req,payload,13);req[13]=0;logline(req);request_pending=payload[13]&1;remote_resume=(payload[13]&2)!=0;remote_interactive=(payload[13]&LL_CAP_MANAGER)!=0;transport_failed=0;if(!remote_resume)peerfolder[0]=0;peer_fs=same(req,"LLFS    0.001")||same(req,"LLFS    0.002");fs_reset();fs_v2=same(req,"LLFS    0.002");peer_tree=same(req,"LLSHELL 0.012");if(peer_fs){int one=1;if(setsockopt(peer,IPPROTO_TCP,TCP_NODELAY,(const char*)&one,sizeof(one))==SOCKET_ERROR){lognum("LLFS TCP_NODELAY failure",WSAGetLastError());return 0;}}if(peer_tree||peer_fs){u8 caps[2];set16(caps,peer_fs&&fs_v2&&!options.fs_readonly?3:1);if(!frameout(WELCOME,caps,2))return 0;}else if(!frameout(WELCOME,0,0))return 0;logline("HANDSHAKE PASS");return 1;}
static int requestfile(void){char req[13];int st,k;if(!request_pending)return -1;request_pending=0;k=framein();if(k!=REQUEST||plen<2||plen>13||payload[plen-1])return 0;copy(req,payload,plen);if(!valid(req))return 0;st=sendfile(req,req,0,0);if(st==MISSING||st==IOERR||st==INVALID){if(!statusout(RESULT,st))return -1;}return st;}
static char scriptbuf[8192];
static char*words[5];
static int split(char*s){int n=0;while(*s){while(*s==' '||*s=='\t')s++;if(!*s)break;if(n==5)return 0;words[n++]=s;while(*s&&*s!=' '&&*s!='\t')s++;if(*s)*s++=0;}return n;}
static u16 decimal(const char*s){u32 n=0;if(!*s)return 0;while(*s){if(*s<'0'||*s>'9')return 0;n=n*10+(*s++-'0');if(n>65535)return 0;}return (u16)n;}

static u8 last_execution[EXEC_BYTES];
static char exec_description[240];
static void appendnum(char*s,u32 n){char b[12];unsigned i=0,j=slen(s);do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)s[j++]=b[--i];s[j]=0;}


/* Durable single-peer job journal: acknowledged work is never submitted again. */
static void journalpath(char*p,const char*leaf){textcopy(p,exedir);textcopy(p+slen(p),"\\");textcopy(p+slen(p),leaf);}
static int journalread(const char*leaf,u32 id,u32 crc){char p[260];u8 b[24];DWORD n;HANDLE f;journalpath(p,leaf);f=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;n=0;if(!ReadFile(f,b,24,&n,0))n=0;CloseHandle(f);return n==24&&b[0]=='L'&&b[1]=='L'&&b[2]=='J'&&b[3]=='1'&&get32(b+4)==id&&get32(b+8)==crc&&get32(b+16)==peer_ip&&get32(b+20)==(crcstep(0xffffffffUL,b,20)^0xffffffffUL);}
static int journalmatch(u32 id,u32 crc){return journalread("LLJOB.DAT",id,crc)||journalread("LLJOB.NEW",id,crc)||journalread("LLJOB.BAK",id,crc);}
static int journalstore(u32 id,u32 crc,u16 code){char p[260],tmp[260],bak[260];u8 b[24];HANDLE f;int ok;DWORD n;zero(b,24);copy(b,"LLJ1",4);set32(b+4,id);set32(b+8,crc);set16(b+12,code);set32(b+16,peer_ip);set32(b+20,crcstep(0xffffffffUL,b,20)^0xffffffffUL);journalpath(p,"LLJOB.DAT");journalpath(tmp,"LLJOB.NEW");journalpath(bak,"LLJOB.BAK");f=CreateFileA(tmp,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;ok=WriteFile(f,b,24,&n,0)&&n==24&&FlushFileBuffers(f);if(!CloseHandle(f))ok=0;if(!ok)return 0;if(GetFileAttributesA(p)!=0xffffffffUL){if(GetFileAttributesA(bak)!=0xffffffffUL&&!DeleteFileA(bak))return 0;if(!MoveFileA(p,bak))return 0;}if(!MoveFileA(tmp,p)){MoveFileA(bak,p);return 0;}return 1;}
static int recover_job(void){u8 ack[4];u32 id,crc;u16 code;if(framein()!=JOB_RESUME||plen!=12||get16(payload+6)!=3)return 0;id=get32(payload);crc=get32(payload+8);code=get16(payload+4);if(code>255||!journalmatch(id,crc)){logline("JOB recovery rejected: no matching durable id/CRC/peer record");return 0;}if(!journalstore(id,crc,code))return 0;set32(ack,id);if(!frameout(JOB_RESUME_ACK,ack,4)||framein()!=JOB_READY||plen!=4||get32(payload)!=id)return 0;remote_resume=0;textcopy(exec_description,"JOB recovered id=");appendnum(exec_description,id);textcopy(exec_description+slen(exec_description)," errorlevel=");appendnum(exec_description,code);textcopy(exec_description+slen(exec_description),"; command not repeated");logline(exec_description);return 1;}
/* Journal ID/CRC/peer before submit, then wait for a newly connected completed job.
 * Acceptance alone is not success. Never automatically repeat a submitted command. */
static int jobcommand(const char*command){u8 data[128],reply[12],ack[4];u32 id,crc;unsigned n=slen(command);int k;u16 code,previous_accept;
 if(n<1||n>120||peer==INVALID_SOCKET)return 0;if(!next_job)next_job=GetTickCount();id=++next_job;crc=crcstep(0xffffffffUL,(const u8*)command,n)^0xffffffffUL;
 if(!journalstore(id,crc,65535)){stage_record("FAIL","job.journal","WIN32",GetLastError(),"Cannot durably record command; not submitted");return 0;}
 set32(data,id);copy(data+4,command,n);if(!frameout(JOB_SUBMIT,data,n+4)||framein()!=JOB_ACCEPT||plen!=12||get32(payload)!=id){logline("JOB submit/accept transport failure");return 0;}
 if(get16(payload+4)!=0){textcopy(exec_description,"JOB rejected status=");appendnum(exec_description,get16(payload+4));logline(exec_description);return 0;}
 if(get32(payload+8)!=crc||get16(payload+6)!=1)return 0;textcopy(exec_description,"JOB accepted id=");appendnum(exec_description,id);textcopy(exec_description+slen(exec_description)," command_crc32=");appendnum(exec_description,crc);logline(exec_description);logline(command);
 closesocket(peer);peer=INVALID_SOCKET;previous_accept=accept_timeout;accept_timeout=resume_wait;k=acceptpeer();accept_timeout=previous_accept;if(!k||!remote_resume){logline("JOB failed waiting for resumed agent");return 0;}
 if(framein()!=JOB_RESUME||plen!=12||get32(payload)!=id||get32(payload+8)!=crc||get16(payload+6)!=3){logline("JOB resumed connection id/CRC/completion mismatch");return 0;}copy(reply,payload,12);code=get16(reply+4);if(code>255)return 0;
 set32(ack,id);if(!frameout(JOB_RESUME_ACK,ack,4)||framein()!=JOB_READY||plen!=4||get32(payload)!=id)return 0;
 if(!journalstore(id,crc,code)){logline("JOB completion journal write failed");return 0;}remote_resume=0;
 zero(last_execution,EXEC_BYTES);set16(last_execution,code?XR_EXIT:XR_OK);last_execution[4]=(u8)code;last_execution[5]=255;set16(last_execution+6,17);
 textcopy(exec_description,"JOB completed id=");appendnum(exec_description,id);textcopy(exec_description+slen(exec_description)," errorlevel=");appendnum(exec_description,code);textcopy(exec_description+slen(exec_description)," resumed=1 command_crc32=");appendnum(exec_description,crc);logline(exec_description);return 1;}
static int remoteexec(int kind,const char*name,const char*args,u16 seconds){char command[121];unsigned n=slen(name),a=slen(args);if(seconds){logline("RUN TIMEOUT is unavailable while agent is unloaded; use JOBWAIT");return 0;}if(n+a+(a?1:0)>120)return 0;textcopy(command,name);if(a){command[n++]=' ';textcopy(command+n,args);}return jobcommand(command);}
static char*token(char**s){char*p;while(**s==' '||**s=='\t')(*s)++;if(!**s)return 0;p=*s;while(**s&&**s!=' '&&**s!='\t')(*s)++;if(**s)*(*s)++=0;return p;}
static int runline(char*line){char*s=line,*verb=token(&s),*program=token(&s),*word,*expect="0",*arguments="";u16 seconds=0,outcome,flags,error;int kind=same(verb,"CMD")?AU_CMD:AU_RUN;if(!program)return 0;
 while((word=token(&s))!=0){if(same(word,"--")){while(*s==' '||*s=='\t')s++;arguments=s;break;}if(word[0]=='E'&&word[1]=='X'&&word[2]=='P'&&word[3]=='E'&&word[4]=='C'&&word[5]=='T'&&word[6]=='=')expect=word+7;
 else if(word[0]=='T'&&word[1]=='I'&&word[2]=='M'&&word[3]=='E'&&word[4]=='O'&&word[5]=='U'&&word[6]=='T'&&word[7]=='='){seconds=decimal(word+8);if(!seconds||seconds>300)return 0;}else return 0;}
 if(!remoteexec(kind,program,arguments,seconds))return 0;outcome=get16(last_execution);error=get16(last_execution+2);flags=get16(last_execution+6);
 if(same(expect,"TIMEOUT"))return outcome==XR_TIMEOUT&&(flags&3)==3;
 if(same(expect,"DISABLED"))return outcome==XR_DISABLED&&!(flags&1);
 if(same(expect,"INVALID"))return outcome==XR_INVALID&&!(flags&1);
 if(same(expect,"SHELL"))return outcome==XR_SHELL&&!(flags&1);
 if(expect[0]=='L'&&expect[1]=='A'&&expect[2]=='U'&&expect[3]=='N'&&expect[4]=='C'&&expect[5]=='H'&&expect[6]==':')return outcome==XR_LAUNCH&&error==decimal(expect+7)&&!(flags&1);
 {u16 exitcode=decimal(expect);if(exitcode>255||(!exitcode&&!same(expect,"0")))return 0;return outcome==(exitcode?XR_EXIT:XR_OK)&&last_execution[4]==exitcode&&(flags&17)==17;}}
#ifdef GUI
static int gui_test(char**,int);
#endif
#ifdef GUI
static int peer_poll(void);
#endif
static int runscript(const char*filename){HANDLE f;DWORD n;char*s,*e;int wc,ok,st;f=CreateFileA(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;if(!ReadFile(f,scriptbuf,sizeof(scriptbuf)-1,&n,0)||n==sizeof(scriptbuf)-1){CloseHandle(f);return 0;}CloseHandle(f);scriptbuf[n]=0;s=scriptbuf;
 while(*s){while(*s=='\r'||*s=='\n')s++;if(!*s)break;e=s;while(*e&&*e!='\r'&&*e!='\n')e++;if(*e)*e++=0;if(*s=='#'){s=e;continue;}logline(s);if((s[0]=='C'&&s[1]=='M'&&s[2]=='D'&&s[3]==' ')||(s[0]=='J'&&s[1]=='O'&&s[2]=='B'&&s[3]==' ')){ok=jobcommand(s+4)&&get16(last_execution)==XR_OK;logline(ok?"PASS":"FAIL");if(!ok){failures++;return 0;}s=e;continue;}if((s[0]=='R'&&s[1]=='U'&&s[2]=='N'&&s[3]==' ')||(s[0]=='C'&&s[1]=='M'&&s[2]=='D'&&s[3]==' ')){ok=runline(s);logline(ok?"PASS":"FAIL");if(!ok){failures++;return 0;}s=e;continue;}wc=split(s);if(!wc){s=e;continue;}ok=0;
 if(same(words[0],"ACCEPT")&&wc==1)ok=acceptpeer();
#ifdef GUI
 else if(same(words[0],"POLL")&&wc==1)ok=peer_poll();
 else if(words[0][0]=='G'&&words[0][1]=='U'&&words[0][2]=='I')ok=gui_test(words,wc);
#endif
 else if(same(words[0],"FSSERVE")&&wc==1)ok=fs_serve();
 else if(same(words[0],"REQUEST")&&wc==1)ok=requestfile()==OK;
 else if(same(words[0],"REQUESTMISSING")&&wc==1)ok=requestfile()==MISSING;
 else if(same(words[0],"JOBWAIT")&&wc==2){resume_wait=decimal(words[1]);ok=resume_wait>=1&&resume_wait<=3600;}
 else if(same(words[0],"PUT")&&wc==3)ok=sendfile(words[1],words[2],0,0)==OK;
 else if(same(words[0],"BADCRC")&&wc==3)ok=sendfile(words[1],words[2],1,0)==INTEGRITY;
 else if(same(words[0],"BADLEN")&&wc==3)ok=sendfile(words[1],words[2],2,0)==INTEGRITY;
 else if(same(words[0],"DROP")&&wc==3){st=sendfile(words[1],words[2],0,1);if(st==-2){closesocket(peer);peer=INVALID_SOCKET;ok=1;}}
 else if(same(words[0],"GET")&&wc==3)ok=download(words[1],words[2],OK);
 else if(same(words[0],"MISSING")&&wc==2)ok=download(words[1],words[1],MISSING);
 else if(same(words[0],"INVALID")&&wc==2)ok=invalidprobe(words[1],INVALID);
 else if(same(words[0],"EXISTS")&&wc==2)ok=invalidprobe(words[1],EXISTS);
 else if(same(words[0],"LIST")&&wc==1)ok=remotelist();
 else if(same(words[0],"MKDIR")&&wc==2)ok=remoteop(MKDIR,words[1],0,OK);
 else if(same(words[0],"CD")&&wc==2)ok=remoteop(CHDIR,words[1],0,OK);
 else if(same(words[0],"BADCD")&&wc==2)ok=remoteop(CHDIR,words[1],0,INVALID);
 else if(same(words[0],"RENAME")&&wc==3)ok=remoteop(RENAME,words[1],words[2],OK);
 else if(same(words[0],"DELETE")&&wc==2)ok=remoteop(AU_DELETE,words[1],0,OK);
 else if(same(words[0],"QUIT")&&wc==1){ok=frameout(QUIT,0,0);shutdown(peer,1);closesocket(peer);peer=INVALID_SOCKET;}
 logline(ok?"PASS":"FAIL");if(!ok){failures++;return 0;}s=e;
 }return 1;}

#ifndef GUI
void __cdecl entry(void){int ok=0;bootstrap();configure();start_network();
 if(options.listenonly){stage_record("OK","listen-only","CONFIG",0,"Diagnostic TCP accept mode; use a script for AUTOUAT protocol operations");for(;;){struct sockaddr_in from;int n=sizeof(from);char b[100];SOCKET c=accept(listener,(struct sockaddr*)&from,&n);if(c==INVALID_SOCKET){u32 e=WSAGetLastError();stage_record("FAIL","accept","WINSOCK",e,"listen-only accept failed");break;}textcopy(b,"NATIVE TCP ACCEPT peer=");textcopy(b+slen(b),inet_ntoa(from.sin_addr));textcopy(b+slen(b),":");diaguint(b,ntohs(from.sin_port));logline(b);closesocket(c);} }
 else ok=runscript(options.script);logline(ok?"ALL PASS":"HOST FAIL");finish_network();ExitProcess(ok?0:1);}
#else
#include "GUI.INC"
#endif
