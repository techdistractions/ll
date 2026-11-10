/* LiteLink source guide:
 * Classic USER controls, responsive VGA layout and message dispatch.
 * Included by HOST.C; PEER.C/QUEUE.C share its state.
 * CRITICAL: busy must remain set through every complete protocol transaction,
 * including post-command LIST, Backspace navigation and normal QUIT. waitio pumps
 * messages, so clearing busy too early allows WM_TIMER to corrupt a reply stream.
 */

/* Classic USER32 controls; no threads, common-controls DLL or modern CRT. */
#include "LLTREE.H"
#define IDC_LOCAL 101
#define IDC_REMOTE 102
#define IDC_COPY 103
#define IDC_RENAME 104
#define IDC_MKDIR 105
#define IDC_DELETE 106
#define IDC_REFRESH 107
#define IDC_NAME 108
#define IDC_QUIT 109
#define IDC_RUN 110
#define IDC_CMDTEXT 112
#define IDC_COMMAND 113
#define IDC_RETRY 114
#define IDC_CANCEL 115
#define IDC_NAMELABEL 119
#define IDC_CMDLABEL 120
#define WM_READY (WM_USER+1)
#define ENTRIES 256
typedef struct {char name[13];u8 attr;u32 size;u16 date,time;} Item;
static Item locals[ENTRIES],remotes[ENTRIES];
static unsigned nl,nr;
static HWND commandedit;
static HWND window,leftbox,rightbox,nameedit,statusbar,leftpath,rightpath,listener_label;
static HINSTANCE instance;
static char localrel[64],remoterel[64],localbase[260];
static int busy,active,selftest,lastaction,cancel_queue;
#ifdef LL_TEST
static u32 test_cancel_threshold;
#endif
static void tree_findclose(void);
static DWORD lastprogress;static int peer_state_changed;static void pump_gui(void);
static void gui_status(const char*s){SetWindowTextA(statusbar,s);UpdateWindow(statusbar);}
static void localpath(char*out){textcopy(out,localbase);if(localrel[0]){textcopy(out+slen(out),"\\");textcopy(out+slen(out),localrel);}}
static void numberstr(char*out,u32 n){char b[12];unsigned i=0,j=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)out[j++]=b[--i];out[j]=0;}
static void fixednum(char*out,unsigned n,unsigned width){while(width){out[--width]=(char)('0'+n%10);n/=10;}}
#include "STYLE.INC"
static void displayitem(HWND box,Item*i){char text[96],*t;unsigned y,m,d;zero(text,sizeof(text));textcopy(text,i->name);t=text+slen(text);*t++='\t';if(i->attr&16)textcopy(t,"<DIR>");else numberstr(t,i->size);t=text+slen(text);*t++='\t';if(i->date){y=1980+(i->date>>9);m=(i->date>>5)&15;d=i->date&31;fixednum(t,y,4);t+=4;*t++='-';fixednum(t,m,2);t+=2;*t++='-';fixednum(t,d,2);t+=2;*t++=' ';fixednum(t,i->time>>11,2);t+=2;*t++=':';fixednum(t,(i->time>>5)&63,2);t+=2;*t=0;}SendMessageA(box,LB_ADDSTRING,0,(LPARAM)text);}
static void parentitem(HWND box,Item*items,unsigned*n){zero(items,2*sizeof(Item));textcopy(items[0].name,".");textcopy(items[1].name,"..");items[0].attr=items[1].attr=16;*n=2;}
static void sortitems(HWND box,Item*items,unsigned n){unsigned i,j;Item v;for(i=1;i<n;i++){v=items[i];j=i;while(j&&itemorder(v.name,v.attr,items[j-1].name,items[j-1].attr)<0){items[j]=items[j-1];j--;}items[j]=v;}SendMessageA(box,LB_RESETCONTENT,0,0);for(i=0;i<n;i++)displayitem(box,items+i);}
static void keep_selection(HWND,Item*,unsigned);static void restore_selection(HWND,Item*,unsigned);
static char last_local_folder[260],last_remote_folder[64];static u32 last_remote_ip;
static void localrefresh(void){WIN32_FIND_DATAA f;HANDLE h;char q[260];localpath(q);if(same(q,last_local_folder))keep_selection(leftbox,locals,nl);else keep_selection(leftbox,locals,0);textcopy(last_local_folder,q);nl=0;SendMessageA(leftbox,LB_RESETCONTENT,0,0);ui_pathlabel(leftpath,"Local  |  ",q);textcopy(basedir,q);textcopy(q+slen(q),"\\*.*");parentitem(leftbox,locals,&nl);h=FindFirstFileA(q,&f);if(h!=INVALID_HANDLE_VALUE){do{if(valid(f.cFileName)&&!(f.dwFileAttributes&0x400)&&nl<ENTRIES){Item*i=locals+nl++;zero(i,sizeof(*i));textcopy(i->name,f.cFileName);i->attr=(u8)f.dwFileAttributes;i->size=f.nFileSizeLow;FileTimeToDosDateTime(&f.ftLastWriteTime,&i->date,&i->time);displayitem(leftbox,i);}}while(FindNextFileA(h,&f));FindClose(h);}sortitems(leftbox,locals,nl);restore_selection(leftbox,locals,nl);}
static int remoterefresh(void){int k;unsigned overflow=0;if(peer_fs){gui_status("LLFS connected - filesystem service");return 1;}if(same(remoterel,last_remote_folder)&&last_remote_ip==peer_ip)keep_selection(rightbox,remotes,nr);else keep_selection(rightbox,remotes,0);textcopy(last_remote_folder,remoterel);last_remote_ip=peer_ip;nr=0;SendMessageA(rightbox,LB_RESETCONTENT,0,0);ui_pathlabel(rightpath,"Peer  |  ",remoterel[0]?remoterel:"\\");if(peer==INVALID_SOCKET)return 1;parentitem(rightbox,remotes,&nr);if(!frameout(LIST,0,0))return 0;while((k=framein())==ENTRY){Item*i;if(plen!=22||payload[12]||!valid((char*)payload))return 0;if(nr>=ENTRIES){overflow=1;continue;}i=remotes+nr++;zero(i,sizeof(*i));textcopy(i->name,(char*)payload);i->size=get32(payload+13);i->attr=payload[17];i->date=get16(payload+18);i->time=get16(payload+20);displayitem(rightbox,i);}sortitems(rightbox,remotes,nr);restore_selection(rightbox,remotes,nr);if(overflow)gui_status("Directory exceeds 256 displayed entries");if(k==RESULT&&plen==2&&get16(payload)==OK){lognum("GUI directory listing entries",nr);return 1;}return 0;}
static void up(char*path){unsigned n=slen(path);while(n&&path[n-1]!='\\')n--;if(n)n--;path[n]=0;}
static int descend(char*path,const char*name){unsigned n=slen(path);if(same(name,"."))return 1;if(same(name,"..")){if(!n)return 1;up(path);return 1;}if(!valid(name)||n+slen(name)+2>=64)return 0;if(n)path[n++]='\\';textcopy(path+n,name);return 1;}
static Item*selection(void){LRESULT n=SendMessageA(active?rightbox:leftbox,LB_GETCARETINDEX,0,0);if(n==LB_ERR||n<0||(unsigned)n>=(active?nr:nl))return 0;return (active?remotes:locals)+n;}
static void lost(void){if(window)SetTimer(window,1,250,0);fs_reset();peer_fs=0;tree_findclose();if(peer!=INVALID_SOCKET)closesocket(peer);peer=INVALID_SOCKET;nr=0;remoterel[0]=0;SendMessageA(rightbox,LB_RESETCONTENT,0,0);gui_status("Waiting for connection");}
static void browse(void){Item*i=selection();char name[13],save[64];if(peer_fs||!i||!(i->attr&16))return;textcopy(name,i->name);if(same(name,".")){if(active){if(!remoterefresh())lost();}else localrefresh();return;}if(same(name,"..")&&!(active?remoterel[0]:localrel[0]))return;if(active){textcopy(save,remoterel);if(!descend(remoterel,name))return;if(!remoteop(CHDIR,name,0,OK)){textcopy(remoterel,save);gui_status("Remote directory unavailable");return;}if(!remoterefresh())lost();}else{if(!descend(localrel,name))return;localrefresh();}}
static void progress(u32 done,u32 total){char b[100],n[12];DWORD now=GetTickCount();
#ifdef LL_TEST
 if(test_cancel_threshold&&done>=test_cancel_threshold)cancel_queue=1;
#endif
 if(!window||now-lastprogress<150&&done!=total)return;lastprogress=now;textcopy(b,"Transfer ");numberstr(n,done);textcopy(b+slen(b),n);textcopy(b+slen(b)," / ");numberstr(n,total);textcopy(b+slen(b),n);textcopy(b+slen(b)," bytes");gui_status(b);UpdateWindow(window);if(busy)pump_gui();}
#include "TREE.C"
#include "PEER.C"
#include "QUEUE.C"
static void action(unsigned id){Item*i;char name[13],other[13],src[260],dst[260],q[13];int ok=0,st;unsigned saved=active;if(id==IDC_CANCEL){cancel_queue=1;gui_status("Cancel requested - finishing the current file.");return;}if(busy)return;if(peer_fs&&id!=IDC_QUIT){gui_status("LLFS owns this connection; unmount it before file manager operations");return;}if(id==IDC_QUIT){SendMessageA(window,WM_CLOSE,0,0);return;}if(id==IDC_CANCEL){cancel_queue=1;gui_status("Cancel requested - finishing the current file.");return;}if(id==IDC_REFRESH){busy=1;localrefresh();if(!remoterefresh())lost();busy=0;return;}if(id==IDC_RETRY){busy=1;lastaction=queue_run(1);busy=0;return;}if(id==IDC_COPY){busy=1;lastaction=queue_run(0);busy=0;return;}if(id==IDC_RUN){i=selection();if(active&&i&&!(i->attr&16))SetWindowTextA(commandedit,i->name);SetFocus(commandedit);return;}
 i=selection();if(i)textcopy(name,i->name);else name[0]=0;GetWindowTextA(nameedit,other,13);busy=1;gui_status("Working...");
 if(id==IDC_COPY){if(peer==INVALID_SOCKET||!i||(i->attr&16))goto done;if(active)ok=download(name,name,OK);else ok=sendfile(name,name,0,0)==OK;}
 else if(id==IDC_MKDIR){if(!valid(other))goto done;if(active){if(peer!=INVALID_SOCKET)ok=remoteop(MKDIR,other,0,OK);}else{if(path(dst,other))ok=CreateDirectoryA(dst,0);}}
 else if(id==IDC_RENAME){if(!i||navname(name)||!valid(other))goto done;if(active){if(peer!=INVALID_SOCKET)ok=remoteop(RENAME,name,other,OK);}else if(path(src,name)&&path(dst,other))ok=MoveFileA(src,dst);}
 else if(id==IDC_COMMAND){char command[121];GetWindowTextA(commandedit,command,121);if(!command[0]||peer==INVALID_SOCKET)goto done;gui_status("Running DOS command; peer unloads while it executes...");transport_failed=0;if(jobcommand(command)){remoterel[0]=0;ok=get16(last_execution)==XR_OK;}else if(transport_failed){lost();goto done;}}
 else if(id==IDC_DELETE){if(!i||(i->attr&16))goto done;textcopy(q,name);if(MessageBoxA(window,q,"Delete selected file?",MB_YESNO|MB_ICONQUESTION)!=IDYES)goto done;if(active){if(peer!=INVALID_SOCKET)ok=remoteop(AU_DELETE,name,0,OK);}else if(path(src,name))ok=DeleteFileA(src);}
 done:lastaction=ok;/* Hold the wire owner through the post-command listing. */localrefresh();if(peer!=INVALID_SOCKET&&!remoterefresh())lost();else if(id==IDC_COMMAND)gui_status(exec_description);else gui_status(ok?"Connected - operation complete":"Operation unavailable or failed; existing files are preserved");active=saved;busy=0;SetFocus(active?rightbox:leftbox);}
static void layout(int w,int h){int half=w/2;unsigned j;int step;static unsigned ids[]={IDC_COPY,IDC_RENAME,IDC_MKDIR,IDC_DELETE,IDC_REFRESH,IDC_RUN,IDC_RETRY,IDC_CANCEL};static const char*shortlabels[]={"Copy","Rename","Folder","Delete","Refresh","Command","Resume","Cancel"};static const char*longlabels[]={"F5 Copy","F6 Rename","F7 Folder","Del Delete","F2 Refresh","F8 Cmd","F4 Resume","Esc Cancel"};MoveWindow(leftpath,12,10,half-20,24,TRUE);MoveWindow(rightpath,half+8,10,half-16,24,TRUE);MoveWindow(leftbox,8,62,half-16,h-206,TRUE);MoveWindow(rightbox,half+4,62,half-12,h-206,TRUE);MoveWindow(GetDlgItem(window,IDC_CMDLABEL),8,h-125,64,24,TRUE);MoveWindow(commandedit,76,h-125,w-194,24,TRUE);MoveWindow(GetDlgItem(window,IDC_COMMAND),w-110,h-126,102,25,TRUE);MoveWindow(GetDlgItem(window,IDC_NAMELABEL),8,h-93,38,24,TRUE);MoveWindow(nameedit,49,h-93,99,24,TRUE);MoveWindow(GetDlgItem(window,IDC_COPY),153,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_RENAME),235,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_MKDIR),317,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_DELETE),399,h-94,65,25,TRUE);MoveWindow(GetDlgItem(window,IDC_REFRESH),468,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_RUN),550,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_RETRY),632,h-94,78,25,TRUE);MoveWindow(GetDlgItem(window,IDC_CANCEL),714,h-94,78,25,TRUE);if(w<800){MoveWindow(nameedit,44,h-93,64,24,TRUE);step=(w-120)/8;for(j=0;j<8;j++){MoveWindow(GetDlgItem(window,ids[j]),112+j*step,h-94,step-3,25,TRUE);SetWindowTextA(GetDlgItem(window,ids[j]),shortlabels[j]);}}else for(j=0;j<8;j++)SetWindowTextA(GetDlgItem(window,ids[j]),longlabels[j]);MoveWindow(statusbar,8,h-55,w-16,24,TRUE);MoveWindow(listener_label,8,h-28,w-16,20,TRUE);{char p[260];localpath(p);ui_pathlabel(leftpath,"Local  |  ",p);if(peer!=INVALID_SOCKET)ui_pathlabel(rightpath,"Peer  |  ",remoterel[0]?remoterel:"\\");}InvalidateRect(window,0,TRUE);}
static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_DRAWITEM)return ui_draw((DRAWITEMSTRUCT*)lp);if(msg==WM_MEASUREITEM){MEASUREITEMSTRUCT*m=(MEASUREITEMSTRUCT*)lp;if(m->CtlType==ODT_LISTBOX){m->itemHeight=ui_row;return TRUE;}}if(msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLOREDIT){SetTextColor((HDC)wp,ui_ink);SetBkColor((HDC)wp,RGB(255,255,255));return (LRESULT)ui_white;}if(msg==WM_CTLCOLORSTATIC){SetTextColor((HDC)wp,(HWND)lp==listener_label&&!ui_legacy?RGB(86,108,132):ui_ink);if((HWND)lp==leftpath||(HWND)lp==rightpath){SetBkColor((HDC)wp,ui_legacy?RGB(255,255,255):RGB(243,246,250));SetBkMode((HDC)wp,OPAQUE);}else SetBkMode((HDC)wp,TRANSPARENT);return (LRESULT)ui_background;}if(msg==WM_PAINT&&leftbox){ui_headers(h);return 0;}if(msg==WM_GETMINMAXINFO){MINMAXINFO*m=(MINMAXINFO*)lp;m->ptMinTrackSize.x=620;m->ptMinTrackSize.y=400;return 0;}if(msg==WM_SIZE){if(window&&leftbox&&statusbar)layout(LOWORD(lp),HIWORD(lp));return 0;}if(msg==WM_COMMAND){unsigned id=LOWORD(wp),code=HIWORD(wp);if(id==IDC_LOCAL||id==IDC_REMOTE){active=id==IDC_REMOTE;if(code==LBN_DBLCLK&&!busy){busy=1;browse();busy=0;}}else if(((id>=IDC_COPY&&id<=IDC_REFRESH)||id==IDC_RUN||id==IDC_COMMAND||id==IDC_RETRY||id==IDC_CANCEL)&&code==BN_CLICKED)action(id);return 0;}if(msg==WM_TIMER&&!busy&&peer!=INVALID_SOCKET&&peer_fs){fd_set f;struct timeval tv;FD_ZERO(&f);FD_SET(peer,&f);tv.tv_sec=tv.tv_usec=0;if(select(0,&f,0,0,&tv)>0){busy=1;if(!fs_service())lost();busy=0;}return 0;}if(msg==WM_TIMER&&!busy&&peer!=INVALID_SOCKET){fd_set f;struct timeval tv;char b;int r;DWORD now=GetTickCount();FD_ZERO(&f);FD_SET(peer,&f);tv.tv_sec=tv.tv_usec=0;if(select(0,&f,0,0,&tv)>0){r=recv(peer,&b,1,MSG_PEEK);if(r<=0){lost();return 0;}}if(now-last_turn>=(remote_interactive?250UL:10000UL)){busy=1;last_turn=now;if(remote_interactive){if(!peer_poll())lost();if(peer_gui_dirty&&peer!=INVALID_SOCKET&&GetTickCount()-last_directory_update>=2000UL){peer_gui_dirty=0;last_directory_update=GetTickCount();localrefresh();if(!remoterefresh())lost();}}else if(!remoterefresh())lost();busy=0;}return 0;}
if(msg==WM_TIMER&&!busy&&peer==INVALID_SOCKET){fd_set f;struct timeval tv;FD_ZERO(&f);FD_SET(listener,&f);tv.tv_sec=tv.tv_usec=0;if(select(0,&f,0,0,&tv)>0){busy=1;if(acceptpeer()){if(peer_fs)SetTimer(window,1,10,0);if(remote_resume&&!recover_job()){lost();busy=0;return 0;}if(request_pending){int rs=requestfile();if(rs<0){lost();busy=0;return 0;}}remoterel[0]=0;if(remoterefresh())gui_status(peer_fs?"LLFS connected - filesystem service":"Connected - Tab switches panes; Enter opens a directory");else lost();}else lost();busy=0;}return 0;}if(msg==WM_CLOSE){if(busy){cancel_queue=1;gui_status("Finishing current file; press Close again when it completes.");return 0;}busy=1;if(peer!=INVALID_SOCKET)frameout(QUIT,0,0);DestroyWindow(h);return 0;}if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(h,msg,wp,lp);}
static void pump_gui(void){MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE){cancel_queue=1;continue;}if(m.message==WM_QUIT){cancel_queue=1;PostQuitMessage((int)m.wParam);break;}TranslateMessage(&m);DispatchMessageA(&m);}}
static HWND child(const char*cls,const char*title,DWORD style,int id){HWND h;char detail[100];u32 error;SetLastError(0);h=CreateWindowExA(0,cls,title,WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,(HMENU)id,instance,0);if(h&&ui_font)SendMessageA(h,WM_SETFONT,(WPARAM)ui_font,FALSE);if(!h){error=GetLastError();textcopy(detail,cls);textcopy(detail+slen(detail)," control_id=");diaguint(detail,id);startup_fail("CreateWindowExA(child)","WIN32",error,detail);}return h;}

static int gui_test(char**w,int n){unsigned i;Item*items;unsigned count;HWND box;
 if(same(w[0],"GUITAG")&&n==3){active=same(w[1],"REMOTE");items=active?remotes:locals;count=active?nr:nl;box=active?rightbox:leftbox;for(i=0;i<count;i++)if(same(items[i].name,w[2])){SendMessageA(box,LB_SETSEL,TRUE,i);return 1;}return 0;}
 if(same(w[0],"GUIBATCH")&&n==3){unsigned good=decimal(w[1]),bad=decimal(w[2]);busy=1;queue_run(0);busy=0;return qok==good&&qfail==bad&&qnext==qcount;}
 if(same(w[0],"GUIRETRY")&&n==1){busy=1;lastaction=queue_run(1);busy=0;return lastaction;}
 if(same(w[0],"GUIRUN")&&n>=2){char cmd[121];unsigned j;cmd[0]=0;for(j=1;j<(unsigned)n;j++){if(slen(cmd)+slen(w[j])+2>=121)return 0;if(j>1)textcopy(cmd+slen(cmd)," ");textcopy(cmd+slen(cmd),w[j]);}SetWindowTextA(commandedit,cmd);action(IDC_COMMAND);return lastaction;}
#ifdef LL_TEST
 if(same(w[0],"GUICANCELTREE")&&n==1){test_cancel_threshold=1;busy=1;queue_run(0);busy=0;test_cancel_threshold=0;return qok==1&&qnext==0&&qcount==1&&qtree.phase==TP_SCAN;}
 if(same(w[0],"GUITREEBAD")&&n==1){u8 req[32];req[0]=TC_SCAN;set16(req+1,0);textcopy((char*)req+3,"..\\ESCAPE");return frameout(LL_TREE,req,13)&&statusin(RESULT)==INVALID;}
 if(same(w[0],"GUICANCELBATCH")&&n==1){test_cancel_threshold=1;busy=1;queue_run(0);busy=0;test_cancel_threshold=0;return qok==1&&qnext==1&&qcount==2;}
 if(same(w[0],"GUIDROP")&&n==1){if(peer!=INVALID_SOCKET){struct linger abortive;abortive.l_onoff=1;abortive.l_linger=0;setsockopt(peer,SOL_SOCKET,SO_LINGER,(char*)&abortive,sizeof(abortive));closesocket(peer);}peer=INVALID_SOCKET;return 1;}
 if(same(w[0],"GUIKEY")&&n>=2){u8 data[8];unsigned j,k;u16 key;for(j=1;j<(unsigned)n;j++){if(slen(w[j])!=4)return 0;key=0;for(k=0;k<4;k++){char c=w[j][k];unsigned v;if(c>='0'&&c<='9')v=c-'0';else if(c>='A'&&c<='F')v=c-'A'+10;else return 0;key=(key<<4)|v;}set16(data+(j-1)*2,key);}return frameout(LL_UI_TEST,data,(n-1)*2)&&statusin(RESULT)==OK;}
 if(same(w[0],"GUIDUMP")&&n==1)return frameout(LL_UI_TEST,0,0)&&statusin(RESULT)==OK;
#endif
 if(same(w[0],"GUIREFRESH")&&n==1){localrefresh();return remoterefresh();}
 if(same(w[0],"GUISELECT")&&n==3){active=same(w[1],"REMOTE");items=active?remotes:locals;count=active?nr:nl;box=active?rightbox:leftbox;for(i=0;i<count;i++)if(same(items[i].name,w[2])){SendMessageA(box,LB_SETSEL,FALSE,-1);SendMessageA(box,LB_SETSEL,TRUE,i);SendMessageA(box,LB_SETCARETINDEX,i,FALSE);return 1;}return 0;}
 if(same(w[0],"GUICOPY")&&n==1){Item*it=selection();char name[13];char file[260];HANDLE f;int ok;if(!it)return 0;textcopy(name,it->name);SendMessageA(window,WM_COMMAND,IDC_COPY,0);if(!lastaction)return 0;if(active){if(!path(file,name))return 0;f=CreateFileA(file,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);ok=f!=INVALID_HANDLE_VALUE;if(ok)CloseHandle(f);return ok;}return 1;}
 if(same(w[0],"GUIMKDIR")&&n==2){SetWindowTextA(nameedit,w[1]);SendMessageA(window,WM_COMMAND,IDC_MKDIR,0);return lastaction;}
 if(same(w[0],"GUIRENAME")&&n==2){SetWindowTextA(nameedit,w[1]);SendMessageA(window,WM_COMMAND,IDC_RENAME,0);return lastaction;}
 if(same(w[0],"GUIENTER")&&n==1){browse();return 1;}
 return 0;}

void __cdecl entry(void){int ok=0,r,ww=820,hh=530,x=100,y=100;WNDCLASSA wc;MSG m;RECT rect;u32 error;char caption[160];
 bootstrap();configure();if(slen(options.root)>170)startup_fail("GUI.root","CONFIG",87,"GUI root exceeds 170-byte browse-path limit");textcopy(localbase,options.root);textcopy(basedir,localbase);selftest=options.selftest;start_network();
 ui_theme();instance=GetModuleHandleA(0);if(!instance)startup_fail("GetModuleHandleA","WIN32",GetLastError(),"Application instance");zero(&wc,sizeof(wc));wc.lpfnWndProc=wndproc;wc.hInstance=instance;wc.hIcon=LoadIconA(instance,MAKEINTRESOURCE(101));if(!wc.hIcon)startup_fail("LoadIconA","WIN32",GetLastError(),"Embedded LiteLink icon");wc.hCursor=LoadCursorA(0,IDC_ARROW);if(!wc.hCursor)startup_fail("LoadCursorA","WIN32",GetLastError(),"IDC_ARROW");wc.hbrBackground=ui_background?ui_background:(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="LiteLinkManager";
 SetLastError(0);if(!RegisterClassA(&wc)){error=GetLastError();startup_fail("RegisterClassA","WIN32",error,wc.lpszClassName);}stage_record("OK","RegisterClassA","WIN32",0,wc.lpszClassName);

#ifdef LITELINK
 textcopy(caption,"LiteLink LLShell32 " LLHOSTVER " - ");
#else
 textcopy(caption,"AUTOUAT 0.0.4 - Network File Manager - ");
#endif
if(GetSystemMetrics(SM_CXSCREEN)<840){ww=GetSystemMetrics(SM_CXSCREEN)-20;x=10;}if(GetSystemMetrics(SM_CYSCREEN)<650){hh=GetSystemMetrics(SM_CYSCREEN)-30;y=10;}textcopy(caption+slen(caption),listen_text);SetLastError(0);window=CreateWindowExA(0,wc.lpszClassName,caption,WS_OVERLAPPEDWINDOW,x,y,ww,hh,0,0,instance,0);if(!window){error=GetLastError();startup_fail("CreateWindowExA(main)","WIN32",error,caption);}stage_record("OK","CreateWindowExA(main)","WIN32",0,"Main window created");
 leftpath=child("STATIC","Local",SS_LEFTNOWORDWRAP|SS_NOPREFIX,0);rightpath=child("STATIC","Peer",SS_LEFTNOWORDWRAP|SS_NOPREFIX,0);leftbox=child("LISTBOX","",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_HASSTRINGS|LBS_OWNERDRAWFIXED|LBS_EXTENDEDSEL,IDC_LOCAL);rightbox=child("LISTBOX","",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_HASSTRINGS|LBS_OWNERDRAWFIXED|LBS_EXTENDEDSEL,IDC_REMOTE);child("STATIC","Name",0,IDC_NAMELABEL);child("STATIC","Command",0,IDC_CMDLABEL);nameedit=child("EDIT","",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,IDC_NAME);SendMessageA(nameedit,EM_LIMITTEXT,12,0);
 child("BUTTON","F5 Copy",WS_TABSTOP|BS_OWNERDRAW,IDC_COPY);child("BUTTON","F6 Rename",WS_TABSTOP|BS_OWNERDRAW,IDC_RENAME);child("BUTTON","F7 Mkdir",WS_TABSTOP|BS_OWNERDRAW,IDC_MKDIR);child("BUTTON","Del Delete",WS_TABSTOP|BS_OWNERDRAW,IDC_DELETE);child("BUTTON","F2 Refresh",WS_TABSTOP|BS_OWNERDRAW,IDC_REFRESH);child("BUTTON","F8 Cmd",WS_TABSTOP|BS_OWNERDRAW,IDC_RUN);child("BUTTON","F4 Retry",WS_TABSTOP|BS_OWNERDRAW,IDC_RETRY);child("BUTTON","Esc Stop",WS_TABSTOP|BS_OWNERDRAW,IDC_CANCEL);commandedit=child("EDIT","",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,IDC_CMDTEXT);SendMessageA(commandedit,EM_LIMITTEXT,120,0);child("BUTTON","Run command",WS_TABSTOP|BS_OWNERDRAW,IDC_COMMAND);statusbar=child("STATIC","Waiting for connection",0,0);listener_label=child("STATIC",listen_text,0,111);if(ui_bold){SendMessageA(leftpath,WM_SETFONT,(WPARAM)ui_bold,FALSE);SendMessageA(rightpath,WM_SETFONT,(WPARAM)ui_bold,FALSE);}stage_record("OK","CreateWindowExA(controls)","WIN32",0,"All controls created");
 if(!GetClientRect(window,&rect))startup_fail("GetClientRect","WIN32",GetLastError(),"Initial layout");layout(rect.right,rect.bottom);localrefresh();
 if(selftest){logline("HIDDEN GUI controls created; visible desktop unchanged");ok=runscript(options.script);DestroyWindow(window);}else{
  SetLastError(0);if(!SetTimer(window,1,250,0)){error=GetLastError();startup_fail("SetTimer","WIN32",error,"Connection polling timer");}ShowWindow(window,options.minimized?SW_SHOWMINNOACTIVE:SW_SHOWNORMAL);UpdateWindow(window);if(!IsWindowVisible(window))startup_fail("IsWindowVisible","WIN32",GetLastError(),"Window did not become visible");stage_record("OK","GUI_READY","WIN32",0,listen_text);SetFocus(leftbox);
  for(;;){r=GetMessageA(&m,0,0,0);if(r==-1)startup_fail("GetMessageA","WIN32",GetLastError(),"Message loop failed");if(!r)break;
   if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE){cancel_queue=1;continue;}
   if(m.message==WM_KEYDOWN&&!busy&&!peer_fs&&m.hwnd==commandedit&&m.wParam==VK_RETURN){action(IDC_COMMAND);continue;}
   if(m.message==WM_KEYDOWN&&!busy&&!peer_fs&&m.hwnd==nameedit){if(m.wParam==VK_RETURN||m.wParam==VK_F6){action(IDC_RENAME);continue;}if(m.wParam==VK_F7){action(IDC_MKDIR);continue;}}
   if(m.message==WM_KEYDOWN&&!busy&&!peer_fs&&(m.hwnd==leftbox||m.hwnd==rightbox)){active=m.hwnd==rightbox;if(m.wParam==VK_TAB){active=!active;SetFocus(active?rightbox:leftbox);continue;}if(m.wParam==VK_RETURN){busy=1;browse();busy=0;continue;}if(m.wParam==VK_BACK){char*rel=active?remoterel:localrel;busy=1;if(*rel){if(active){if(remoteop(CHDIR,"..",0,OK)){up(remoterel);if(!remoterefresh())lost();}}else{up(localrel);localrefresh();}}busy=0;continue;}if(m.wParam==VK_SPACE||m.wParam==VK_INSERT){LRESULT at=SendMessageA(m.hwnd,LB_GETCARETINDEX,0,0);if(at>=0&&valid((active?remotes:locals)[at].name)){SendMessageA(m.hwnd,LB_SETSEL,SendMessageA(m.hwnd,LB_GETSEL,at,0)<=0,at);if((unsigned)at+1<(active?nr:nl))SendMessageA(m.hwnd,LB_SETCARETINDEX,at+1,FALSE);}continue;}if(m.wParam=='A'&&(GetKeyState(VK_CONTROL)&0x8000)){unsigned j;Item*items=active?remotes:locals;for(j=0;j<(active?nr:nl);j++)if(valid(items[j].name))SendMessageA(m.hwnd,LB_SETSEL,TRUE,j);continue;}if(m.wParam==VK_UP||m.wParam==VK_DOWN){LRESULT at=SendMessageA(m.hwnd,LB_GETCARETINDEX,0,0);if(m.wParam==VK_UP&&at>0)at--;if(m.wParam==VK_DOWN&&at+1<(active?nr:nl))at++;SendMessageA(m.hwnd,LB_SETCARETINDEX,at,FALSE);continue;}if(m.wParam==VK_F4){action(IDC_RETRY);continue;}if(m.wParam==VK_F8){action(IDC_RUN);continue;}if(m.wParam==VK_F5){action(IDC_COPY);continue;}if(m.wParam==VK_F6){SetFocus(nameedit);continue;}if(m.wParam==VK_F7){action(IDC_MKDIR);continue;}if(m.wParam==VK_DELETE){action(IDC_DELETE);continue;}if(m.wParam==VK_F2){action(IDC_REFRESH);continue;}}
   TranslateMessage(&m);DispatchMessageA(&m);
  }ok=1;
 }
 logline(ok?"GUI PASS":"GUI FAIL");tree_findclose();ui_theme_free();finish_network();ExitProcess(ok?0:1);}

