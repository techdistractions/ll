/* LLFS protocol 2: offset reads/writes, explicit durable acknowledgments.
 * No reconnect/replay. A failed reply leaves a client-visible uncertain outcome.
 * All writes use the configured export; this code never follows reparse points.
 * Version 1 clients remain read-only even on a write-enabled host.
 */
#define FS_SLOTS 16
static int peer_fs,fs_mounted,fs_v2,fs_writable;
static u32 fs_bytes,fs_written,fs_requests;
static HANDLE fs_files[FS_SLOTS];
static u16 fs_tokens[FS_SLOTS],fs_serial;
static u8 fs_modes[FS_SLOTS],fs_dirty[FS_SLOTS];
static char fs_search[FS_SLOTS][260];
static u16 fs_searchid[FS_SLOTS],fs_nextid;
static u8 fs_attrs[FS_SLOTS];static unsigned fs_cursor;
static u16 fs_error(void){DWORD e=GetLastError();switch(e){
 case ERROR_FILE_NOT_FOUND:return 2;case ERROR_PATH_NOT_FOUND:return 3;
 case ERROR_TOO_MANY_OPEN_FILES:return 4;case ERROR_ACCESS_DENIED:return 5;
 case ERROR_INVALID_HANDLE:return 6;case ERROR_NOT_SAME_DEVICE:return 17;
 case ERROR_WRITE_PROTECT:return 19;case ERROR_SHARING_VIOLATION:return 32;
 case ERROR_LOCK_VIOLATION:return 33;case ERROR_DISK_FULL:case ERROR_HANDLE_DISK_FULL:return 39;
 case ERROR_FILE_EXISTS:case ERROR_ALREADY_EXISTS:return 80;
 case ERROR_DIR_NOT_EMPTY:return 5;case ERROR_INVALID_PARAMETER:return 1;
 default:return 31;}}
static void fs_reset(void){unsigned i;if(fs_mounted){lognum("LLFS read bytes",fs_bytes);lognum("LLFS written bytes",fs_written);lognum("LLFS requests",fs_requests);}
 for(i=0;i<FS_SLOTS;i++){if(fs_tokens[i]){if(fs_dirty[i]&&!FlushFileBuffers(fs_files[i]))lognum("LLFS disconnect flush failed",GetLastError());if(!CloseHandle(fs_files[i]))lognum("LLFS disconnect close failed",GetLastError());}fs_tokens[i]=fs_searchid[i]=0;fs_dirty[i]=0;}
 fs_bytes=fs_written=fs_requests=0;fs_mounted=fs_writable=0;
}
static int fs_name(const WIN32_FIND_DATAA*d,char*out){const char*p=d->cFileName;if(!valid(p)){p=d->cAlternateFileName;if(!valid(p))return 0;}textcopy(out,p);return 1;}
static void fs_fcb(u8*out,const char*name){unsigned i=0;while(i<11)out[i++]=' ';i=0;while(*name&&i<11){char c=*name++;if(c=='.'){i=8;continue;}if(c>='a'&&c<='z')c-=32;out[i++]=c;}}
static void fs_dostime(const FILETIME*ft,u8*out){FILETIME local;WORD date=0,time=0;if(FileTimeToLocalFileTime(ft,&local))FileTimeToDosDateTime(&local,&date,&time);set16(out,time);set16(out+2,date);}
static void fs_meta(u8*b,const WIN32_FIND_DATAA*d,const char*name){zero(b,25);b[0]=(u8)(d->dwFileAttributes&0x37);if(!fs_writable&&!(b[0]&16))b[0]|=1;fs_fcb(b+1,name);fs_dostime(&d->ftLastWriteTime,b+12);set32(b+16,d->nFileSizeLow);}
/* Validate every component. Missing final components are allowed for create.
 * Existing reparse points, drive prefixes, dot components and devices are not.
 */
static int fs_path(char*out,const char*in,int wild){char part[13];unsigned n=0,i;DWORD a;textcopy(out,options.root);a=GetFileAttributesA(out);if(a==0xffffffffUL||!(a&16)||(a&0x400))return 0;if(*in!='\\'||slen(in)>126)return 0;in++;if(!*in)return 1;
 while(*in){n=0;while(*in&&*in!='\\'){if(n==12)return 0;part[n++]=*in++;}part[n]=0;
  if(!valid(part)){if(!wild||*in||!n)return 0;for(i=0;i<n;i++)if(!((part[i]>='A'&&part[i]<='Z')||(part[i]>='a'&&part[i]<='z')||(part[i]>='0'&&part[i]<='9')||part[i]=='*'||part[i]=='?'||part[i]=='.'||part[i]=='_'||part[i]=='-'))return 0;if(same(part,".")||same(part,".."))return 0;}
  if(slen(out)+n+2>=260)return 0;textcopy(out+slen(out),"\\");textcopy(out+slen(out),part);
  a=GetFileAttributesA(out);if(a!=0xffffffffUL&&(a&0x400))return 0;if(*in&&(a==0xffffffffUL||!(a&16)))return 0;
  if(*in){in++;if(!*in)return 0;}
 }return 1;
}
static int fs_string(unsigned start){return start<plen&&payload[plen-1]==0&&slen((char*)payload+start)==plen-start-1;}
static int fs_scan(unsigned slot,u16 after,u8*out){WIN32_FIND_DATAA d;HANDLE h;char name[13];u16 index=0;int ok=0;h=FindFirstFileA(fs_search[slot],&d);if(h==INVALID_HANDLE_VALUE)return 18;
 do{if(index==65535)break;index++;if(index<=after)continue;if(d.dwFileAttributes&0x400)continue;if((d.dwFileAttributes&6)&~fs_attrs[slot])continue;if((d.dwFileAttributes&16)&&!(fs_attrs[slot]&16))continue;if(d.nFileSizeHigh||d.nFileSizeLow>0x7fffffffUL||!fs_name(&d,name))continue;
  fs_meta(out,&d,name);set16(out+20,fs_searchid[slot]);set16(out+22,index);ok=1;break;
 }while(FindNextFileA(h,&d));FindClose(h);return ok?0:18;
}
static u16 fs_token(void){unsigned i;int found;do{if(++fs_serial==0)++fs_serial;found=0;for(i=0;i<FS_SLOTS;i++)if(fs_tokens[i]==fs_serial)found=1;}while(found);return fs_serial;}
static u16 fs_slot(u16 token){unsigned i;if(!token)return FS_SLOTS;for(i=0;i<FS_SLOTS;i++)if(fs_tokens[i]==token)return i;return FS_SLOTS;}
static u16 fs_open(const char*full,unsigned mode,int creating,u16 attr,u8*out){HANDLE h;DWORD access,sharing,disp,size;WIN32_FIND_DATAA d;unsigned i;char name[13];FILETIME ft;
 if((mode&3)>2||((mode>>4)&7)>4)return 1;
 if(!fs_writable&&(creating||(mode&3)))return 5;
 for(i=0;i<FS_SLOTS&&fs_tokens[i];i++);if(i==FS_SLOTS)return 4;
 access=(mode&3)==0?GENERIC_READ:(mode&3)==1?GENERIC_WRITE:GENERIC_READ|GENERIC_WRITE;
 switch((mode>>4)&7){case 0:sharing=(mode&3)?0:FILE_SHARE_READ;break;case 1:sharing=0;break;case 2:sharing=FILE_SHARE_READ;break;case 3:sharing=FILE_SHARE_WRITE;break;default:sharing=FILE_SHARE_READ|FILE_SHARE_WRITE;}
 disp=creating?(attr&0x100?CREATE_NEW:CREATE_ALWAYS):OPEN_EXISTING;
 if(creating&&(attr&0xd8))return 5;
 h=CreateFileA(full,access,sharing,0,disp,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return fs_error();
 size=GetFileSize(h,0);if(size>0x7fffffffUL){CloseHandle(h);return 5;}
 zero(&d,sizeof(d));d.dwFileAttributes=GetFileAttributesA(full);d.nFileSizeLow=size;
 if(!GetFileTime(h,0,0,&ft)){u16 e=fs_error();CloseHandle(h);return e;}d.ftLastWriteTime=ft;
 /* Path was already bounded/validated; use its final DOS component. */
 {const char*p=full,*last=p;while(*p){if(*p=='\\')last=p+1;p++;}if(!valid(last)){CloseHandle(h);return 5;}textcopy(name,last);}
 if(creating){if(!SetFileAttributesA(full,(attr&0x27)|FILE_ATTRIBUTE_ARCHIVE)||!FlushFileBuffers(h)){u16 e=fs_error();CloseHandle(h);return e;}d.dwFileAttributes=(attr&0x27)|FILE_ATTRIBUTE_ARCHIVE;}
 fs_files[i]=h;fs_modes[i]=(u8)mode;fs_dirty[i]=0;fs_tokens[i]=fs_token();fs_meta(out,&d,name);set16(out+20,fs_tokens[i]);out[24]=(u8)mode;return 0;
}
static u16 fs_settime(unsigned slot,const u8*p){FILETIME local,utc;if(!DosDateTimeToFileTime(get16(p+2),get16(p),&local)||!LocalFileTimeToFileTime(&local,&utc)||!SetFileTime(fs_files[slot],0,0,&utc))return fs_error();fs_dirty[slot]=1;return 0;}
/* Durable boundary. A write is acknowledged only after a successful flush. */
static u16 fs_stampflush(unsigned slot,u8*out){SYSTEMTIME st;FILETIME ft;GetSystemTime(&st);if(!SystemTimeToFileTime(&st,&ft)||!SetFileTime(fs_files[slot],0,0,&ft)||!FlushFileBuffers(fs_files[slot]))return fs_error();fs_dirty[slot]=0;fs_dostime(&ft,out);return 0;}
static u16 fs_space(u8*out){char root[260];unsigned i;DWORD spc,bps,free,total,unit,factor;
 textcopy(root,options.root);if(root[0]&&root[1]==':'){root[2]='\\';root[3]=0;}else if(root[0]=='\\'&&root[1]=='\\'){i=2;while(root[i]&&root[i]!='\\')i++;if(!root[i])return 15;i++;while(root[i]&&root[i]!='\\')i++;root[i++]='\\';root[i]=0;}else return 15;
 if(!GetDiskFreeSpaceA(root,&spc,&bps,&free,&total))return fs_error();if(!bps||!spc||bps>65536||spc>65536)return 31;unit=spc*bps;if(!unit)return 31;
 if(unit<=32768){factor=32768/unit;if(!factor)factor=1;free/=factor;total/=factor;}else{factor=unit/32768;if(!factor)factor=1;free=free>65535/factor?65535:free*factor;total=total>65535/factor?65535:total*factor;}
 if(total>65535)total=65535;if(free>total)free=total;
 set16(out,64);set16(out+2,(u16)total);set16(out+4,512);set16(out+6,fs_writable?(u16)free:0);return 0;
}
static u16 fs_delete(const char*full){WIN32_FIND_DATAA d;HANDLE h;char path[260],name[13];unsigned n;u16 e=2;h=FindFirstFileA(full,&d);if(h==INVALID_HANDLE_VALUE)return fs_error();textcopy(path,full);n=slen(path);while(n&&path[n-1]!='\\')n--;
 do{if(d.dwFileAttributes&(16|0x400))continue;if(!fs_name(&d,name))continue;if(d.dwFileAttributes&(1|2|4)){e=5;break;}textcopy(path+n,name);if(!DeleteFileA(path)){e=fs_error();break;}e=0;}while(FindNextFileA(h,&d));FindClose(h);return e;}
static int fs_service(void){u8 out[512],op,mode;unsigned n=0,i,slot,j;u16 err=0,token,count,flags;u32 offset;char full[260],other[260];WIN32_FIND_DATAA d;HANDLE h;DWORD got;const char*p;
 i=framein();if(i==QUIT&&!plen){fs_reset();return 0;}if(i!=40||plen<2||payload[1]!=0){logline("LLFS invalid request envelope");lognum("Frame return",(u32)i);lognum("Transport failed",transport_failed);lognum("Frame bytes",get16(header+6));lognum("Frame seq",get32(header+8));lognum("Expected seq",rxid-1);lognum("Header CRC",get32(header+12));if(plen<=512)lognum("Payload CRC",crcstep(0xffffffffUL,payload,plen)^0xffffffffUL);transport_failed=1;return 0;}op=payload[0];fs_requests++;zero(out,sizeof(out));
 if(op==0){if(fs_mounted)err=5;else if(fs_v2){if(plen!=10||payload[2]!=2||payload[3]>1||!same((char*)payload+4,"FILES"))err=53;else if(payload[3]&&options.fs_readonly)err=5;else fs_writable=payload[3];}else if(plen!=9||payload[2]!=1||!same((char*)payload+3,"FILES"))err=53;
  if(!err){if(!fs_path(full,"\\",0))err=5;else{fs_mounted=1;logline(fs_writable?"LLFS mounted FILES read/write":"LLFS mounted FILES read-only");}}}
 else if(!fs_mounted)err=53;
 else switch(op){
 case 1:case 3:case 0x0e:case 0x13:
  if(!fs_writable){err=5;break;}i=op==0x0e?3:2;
  if(!fs_string(i)||!fs_path(full,(char*)payload+i,op==0x13)){err=3;break;}
  if(!payload[i+1]){err=5;break;}
  if(op==1){if(!RemoveDirectoryA(full))err=fs_error();}
  else if(op==3){if(!CreateDirectoryA(full,0)){err=fs_error();if(err==80)err=5;}}
  else if(op==0x0e){flags=payload[2];if(flags&~0x27){err=5;break;}got=GetFileAttributesA(full);if(got==0xffffffffUL){err=fs_error();break;}if(!SetFileAttributesA(full,flags|(got&16)))err=fs_error();}
  else err=fs_delete(full);break;
 case 0x11:
  if(!fs_writable){err=5;break;}p=(char*)payload+2;i=2;while(i<plen&&payload[i])i++;
  if(i==plen||i+1>=plen||!fs_string(i+1)||!fs_path(full,p,0)||!fs_path(other,(char*)payload+i+1,0)||!p[1]||!payload[i+2]){err=3;break;}
  if(!MoveFileA(full,other))err=fs_error();break;
 case 5:case 0x0f:case 0x16:case 0x17:
  i=(op==0x16||op==0x17)&&fs_v2?5:2;
  if(!fs_string(i)||!fs_path(full,(char*)payload+i,0)){err=3;break;}
  if(op==0x16||op==0x17){mode=fs_v2?payload[2]:0;flags=fs_v2?get16(payload+3):0;logline(op==0x17?"LLFS CREATE":"LLFS OPEN");logline(full);err=fs_open(full,mode,op==0x17,flags,out+2);if(!err)n=25;break;}
  h=FindFirstFileA(full,&d);if(h==INVALID_HANDLE_VALUE){err=fs_error();break;}FindClose(h);
  if(op==5){if(!(d.dwFileAttributes&16))err=3;break;}
  if(d.nFileSizeHigh||d.nFileSizeLow>0x7fffffffUL){err=5;break;}
  fs_dostime(&d.ftLastWriteTime,out+2);set32(out+6,d.nFileSizeLow);out[10]=(u8)(d.dwFileAttributes&0x37);if(!fs_writable&&!(out[10]&16))out[10]|=1;n=9;break;
 case 6:case 7:
  if(plen!=(fs_v2?9:4)){err=1;break;}slot=fs_slot(get16(payload+2));if(slot==FS_SLOTS){err=6;break;}
  if(fs_v2&&payload[4]){if(!fs_writable){err=5;break;}err=fs_settime(slot,payload+5);}
  if(!err&&(fs_modes[slot]&3)&&!FlushFileBuffers(fs_files[slot]))err=fs_error();
  if(!err)fs_dirty[slot]=0;
  if(op==6){if(!CloseHandle(fs_files[slot])&&!err)err=fs_error();fs_tokens[slot]=0;}
  break;
 case 8:case 9:
  if(plen<10){err=1;break;}offset=get32(payload+2);slot=fs_slot(get16(payload+6));count=get16(payload+8);
  if(slot==FS_SLOTS){err=6;break;}if(count>480||offset>0x7fffffffUL||(u32)count>0x7fffffffUL-offset||plen!=(op==8?10:10+count)){err=1;break;}
  if(op==9&&(!fs_writable||!(fs_modes[slot]&3))){err=5;break;}if(op==8&&(fs_modes[slot]&3)==1){err=5;break;}
  if(SetFilePointer(fs_files[slot],offset,0,FILE_BEGIN)==0xffffffffUL){err=fs_error();break;}got=0;
  if(op==8){if(!ReadFile(fs_files[slot],out+2,count,&got,0)){err=fs_error();break;}n=got;fs_bytes+=got;}
  else{fs_dirty[slot]=1;if(!count){if(!SetEndOfFile(fs_files[slot])){err=fs_error();break;}}else if(!WriteFile(fs_files[slot],payload+10,count,&got,0)){err=fs_error();break;}
   err=fs_stampflush(slot,out+4);if(!err){set16(out+2,(u16)got);n=6;fs_written+=got;}}
  break;
 case 10:case 11:
  if(plen<7||payload[2]>1){err=1;break;}slot=fs_slot(get16(payload+3));count=get16(payload+5);if(slot==FS_SLOTS){err=6;break;}if(count>63||plen!=7+count*8){err=1;break;}
  for(j=0;j<count;j++){offset=get32(payload+7+j*8);got=get32(payload+11+j*8);if(!(payload[2]?UnlockFile(fs_files[slot],offset,0,got,0):LockFile(fs_files[slot],offset,0,got,0))){err=fs_error();break;}}
  if(err&&!payload[2])while(j--){offset=get32(payload+7+j*8);got=get32(payload+11+j*8);UnlockFile(fs_files[slot],offset,0,got,0);}break;
 case 0x0c:err=fs_space(out+2);if(!err)n=8;break;
 case 0x1b:
  if(plen<5||!fs_string(3)||!fs_path(full,(char*)payload+3,1)){err=3;break;}
  slot=fs_cursor++%FS_SLOTS;if(++fs_nextid==0)++fs_nextid;fs_searchid[slot]=fs_nextid;fs_attrs[slot]=payload[2];textcopy(fs_search[slot],full);
  if(payload[2]&8){err=18;break;}err=fs_scan(slot,0,out+2);if(!err)n=24;else err=2;break;
 case 0x1c:
  if(plen!=18){err=1;break;}token=get16(payload+2);for(slot=0;slot<FS_SLOTS&&fs_searchid[slot]!=token;slot++);if(!token||slot==FS_SLOTS){err=18;break;}err=fs_scan(slot,get16(payload+4),out+2);if(!err)n=24;break;
 default:err=1;
 }
 set16(out,err);if(op!=8&&op!=9)lognum("LLFS operation",op);if(err)lognum("LLFS DOS error",err);return frameout(41,out,err?2:n+2);
}
static int fs_serve(void){int ok=1,r;fd_set f;struct timeval tv;if(!peer_fs)return 0;do{FD_ZERO(&f);FD_SET(peer,&f);tv.tv_sec=0;tv.tv_usec=250000;r=select(0,&f,0,0,&tv);if(r<0){transport_failed=1;break;}if(r>0)ok=fs_service();}while(ok);fs_reset();if(peer!=INVALID_SOCKET)closesocket(peer);peer=INVALID_SOCKET;return !transport_failed;}
