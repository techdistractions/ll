/* LLFS experimental DOS 3.3 filesystem redirector.
 * Installer may call DOS. Resident dispatch NEVER calls DOS or writes logs.
 * All on-wire requests are bounded, sequenced, CRC checked and never replayed.
 * NIC ownership persists until /U; current released LLShell owns its own NIC.
 */
#define LLFS_COALESCE 1
#include "AU.H"
#include "DOS3.H"
#include <i86.h>
struct FSRegs {u16 ax,bx,cx,dx,bp,si,di,es,ds,ip,cs,flags;};
struct FSRegs fs_regs;
u16 fs_done,fs_keep,fs_paras,fs_psp,fs_arg,fs_info[8],fs_vector[4];
/* 140-byte measured high-water; reserve margin for IRQs and both backends. */
u8 fs_stack[512];
extern void __cdecl fs_install(void);
extern void __cdecl fs_control(u16);
#pragma aux fs_control parm [ax] modify [ax];
extern void __cdecl fs_restore(void);
static struct sdastruct far *sda;
static struct cdsstruct far *cds;
static struct cdsstruct saved_cds;
static u16 drive=0xffff,opens,live,lastdrv,writable,uncertain;
/* fcopy is shared with the installer. */
static void fcopy(void far*,const void far*,u16);
#include "FSRUNTIME.H"
static u16 dosver(void);
#pragma aux dosver="mov ah,30h" "int 21h" value [ax] modify [bx cx];
static u16 getpsp(void);
#pragma aux getpsp="mov ah,62h" "int 21h" value [bx] modify [ax];
static u16 curdrive(void);
#pragma aux curdrive="mov ah,19h" "int 21h" "xor ah,ah" value [ax];
static void far *getsda(void);
#pragma aux getsda="push ds" "mov ax,5d06h" "int 21h" "mov dx,ds" "mov ax,si" "pop ds" value [dx ax] modify [bx cx si];
static u8 far *getlist(void);
#pragma aux getlist="mov ah,52h" "int 21h" value [es bx] modify [ax];
static u16 release(u16);
#pragma aux release="push es" "mov es,ax" "mov ah,49h" "int 21h" "sbb ax,ax" "pop es" parm [ax] value [ax];
static u32 vector(void);
#pragma aux vector="push es" "mov ax,352fh" "int 21h" "mov dx,es" "mov ax,bx" "pop es" value [dx ax] modify [bx];
static void fcopy(void far*d,const void far*s,u16 n){u8 far*a=d;const u8 far*b=s;while(n--)*a++=*b++;}
static void fzero(void far*d,u16 n){u8 far*a=d;while(n--)*a++=0;}
static int wireout(const u8*p,unsigned n){unsigned z;while(n){z=n>512?512:n;if(net_send(p,z))return 0;p+=z;n-=z;}return 1;}
static int wirein(u8*p,unsigned n){int z;u16 begin=net_ticks();while(n){z=net_recv(p,n);if(z<0)return 0;if(!z){if((u16)(net_ticks()-begin)>=timeout*18)return 0;continue;}p+=z;n-=z;begin=net_ticks();}return 1;}
/* Each mutation is sent ONCE. A broken reply permanently faults this session.
 * TCP may retransmit bytes; frame sequence checks prevent duplicate dispatch.
 */
static u16 query(u8 op,unsigned n){u16 e;int mutation=op==1||op==3||op==6||op==7||op==9||op==0x0e||op==0x11||op==0x13||op==0x17;
 if(!live)return 53;request[0]=op;request[1]=0;
 if(!frameout(40,request,n+2)||framein()!=41||plen<2){live=0;if(writable&&mutation)uncertain=1;return 59;}
 e=get16(payload);if(writable&&mutation&&(e==31||e==39))uncertain=1;return e;
}
static void fail(u16 e){fs_regs.flags|=1;fs_regs.ax=e;}
static int patharg(unsigned start){unsigned i=2;while(i<128&&sda->fn1[i]){request[start+i-2]=sda->fn1[i];i++;}if(i==128)return -1;request[start+i-2]=0;return start+i-1;}
static void fcbfix(void){u16 i,j=0;u8 far*p=sda->fn1;for(i=0;i<128&&sda->fn1[i];i++)if(sda->fn1[i]=='\\')p=sda->fn1+i+1;for(i=0;i<11;i++)sda->fcb_fn1[i]=' ';while(*p&&j<11){if(*p=='.'){j=8;p++;}else if(*p=='*'){while(j<(*p? (j<8?8:11):11))sda->fcb_fn1[j++]='?';p++;}else sda->fcb_fn1[j++]=*p++;}}
void __cdecl fs_dispatch(void){
 u16 op=fs_regs.ax&255,d=255,e=0,n,total,want,take;long off;
 struct sftstruct far*sft=MK_FP(fs_regs.es,fs_regs.di);
 struct sdbstruct far*dta;u8 far*buffer;
 fs_done=0;
 if((fs_regs.ax&0xff00)==0xd700){
  if(op>1||(op&&(fs_regs.bx!=0x4c4c||fs_regs.cx!=0x4653)))return;
  fs_done=1;fs_regs.ax=0xd700|drive;fs_regs.bx=0x4c4c;fs_regs.cx=0x4653;fs_regs.dx=fs_psp;
  fs_regs.si=fs_vector[0];fs_regs.di=fs_vector[1];fs_regs.bp=fs_vector[2];fs_regs.es=fs_vector[3];
  fs_regs.flags&=~1;
  if(op==1){if(opens){fs_regs.ax=5;fs_regs.flags|=1;return;}
   if(live)frameout(QUIT,0,0);live=0;net_close();fcopy(cds,&saved_cds,sizeof(saved_cds));for(n=0;n<sizeof(fs_stack)&&fs_stack[n]==0xa5;n++);fs_regs.cx=(sizeof(fs_stack)-n)|(uncertain?0x8000:0);fs_regs.ax=0;}
  return;
 }
 if((fs_regs.ax&0xff00)!=0x1100||!op||op>0x2e)return;
 if((op>=6&&op<=11)||op==0x21||op==0x2d)d=sft->dev_info_word&63;
 else if(op==0x1c)d=sda->sdb.drv_lett&31;
 else if(op==1||op==3||op==5||op==0x0e||op==0x0f||op==0x11||op==0x13||op==0x16||op==0x17||op==0x1b||op==0x2e)d=sda->fn1[0]-'A';
 else if(op==0x0c)d=((struct cdsstruct far*)sft)->current_path[0]-'A';
 else return;
 if(d!=drive)return;
 fs_done=1;fs_regs.flags&=~1;
 switch(op){
 case 0x2e:fail(1);return; /* DOS 4 extended open is outside the DOS 3.3 ABI. */
 case 1:case 3:case 0x0e:case 0x13:
  if(!writable){fail(5);return;}
  if(op==1){for(n=0;n<67&&sda->fn1[n]&&sda->fn1[n]==cds->current_path[n];n++);if(n<67&&!sda->fn1[n]&&!cds->current_path[n]){fail(16);return;}}
  if(op==0x0e)request[2]=(u8)fs_arg;n=patharg(op==0x0e?3:2);if((int)n<0){e=3;break;}e=query(op,n-2);break;
 case 0x11:
  if(!writable){fail(5);return;}if(sda->fn2[0]!=(u8)('A'+drive)){e=17;break;}
  n=patharg(2);if((int)n<0){e=3;break;}for(total=2;total<128&&sda->fn2[total];total++)request[n++]=sda->fn2[total];
  if(total==128){e=3;break;}request[n++]=0;e=query(op,n-2);break;
 case 10:case 11:
  /* DOS 3.3: CX:DX offset, SI high length, caller's stack word low length.
   * The later DOS 4+ DS:DX record-list ABI must NOT be used here. */
  request[2]=op==11?1:0;set16(request+3,sft->start_sector);set16(request+5,1);
  set16(request+7,fs_regs.dx);set16(request+9,fs_regs.cx);set16(request+11,fs_arg);set16(request+13,fs_regs.si);e=query(op,13);break;
 case 0x2d:fs_regs.ax=2;return;
 case 0x21:
  off=(long)(((u32)fs_regs.cx<<16)|fs_regs.dx);
  if(off<0&&((u32)(-off)>sft->file_size)){fail(1);return;}
  if(off>0&&sft->file_size>0x7fffffffUL-(u32)off){fail(1);return;}
  sft->file_pos=sft->file_size+off;fs_regs.ax=(u16)sft->file_pos;fs_regs.dx=(u16)(sft->file_pos>>16);return;
 case 6:case 7:
  if(op==6&&sft->handle_count)sft->handle_count--;
  if(op==7||!sft->handle_count){set16(request+2,sft->start_sector);request[4]=(sft->dev_info_word&0x4000)!=0;set32(request+5,sft->file_time);e=query(op,7);if(op==6&&opens)opens--;}
  break;
 case 9:
  if(!writable||!(sft->open_mode&3)){fail(5);return;}
  total=0;want=fs_regs.cx;buffer=sda->curr_dta;
  do{take=want-total;if(take>480)take=480;
   if(sft->file_pos>0x7fffffffUL||(u32)take>0x7fffffffUL-sft->file_pos){e=1;break;}
   set32(request+2,sft->file_pos);set16(request+6,sft->start_sector);set16(request+8,take);fcopy(request+10,buffer+total,take);
   e=query(9,8+take);if(e)break;if(plen!=8||(n=get16(payload+2))>take){live=0;uncertain=1;e=59;break;}
   total+=n;sft->file_pos+=n;if(!want||sft->file_pos>sft->file_size)sft->file_size=sft->file_pos;
   sft->file_time=get32(payload+4);sft->dev_info_word&=~0x4040;
   if(n<take)break;
  }while(total<want);fs_regs.cx=total;break;
 case 8:
  if((sft->open_mode&3)==1){fail(5);return;}
  total=0;want=fs_regs.cx;buffer=sda->curr_dta;
  while(total<want){take=want-total;if(take>480)take=480;
   set32(request+2,sft->file_pos);set16(request+6,sft->start_sector);set16(request+8,take);
   e=query(8,8);if(e)break;n=plen-2;if(n>take){e=59;live=0;break;}
   fcopy(buffer+total,payload+2,n);total+=n;sft->file_pos+=n;if(n<take)break;
  }fs_regs.cx=total;break;
 case 0x0c:e=query(op,0);if(e)break;if(plen!=10){e=59;break;}fs_regs.ax=get16(payload+2);fs_regs.bx=get16(payload+4);fs_regs.cx=get16(payload+6);fs_regs.dx=get16(payload+8);break;
 case 0x1c:
  dta=MK_FP(fs_regs.es,fs_regs.di);set16(request+2,dta->par_clstr);set16(request+4,dta->dir_entry);request[6]=dta->srch_attr;
  fcopy(request+7,dta->srch_tmpl,11);e=query(op,16);goto found;
 case 0x1b:
  fcbfix();request[2]=sda->srch_attr;n=patharg(3);if((int)n<0){e=3;break;}dta=(struct sdbstruct far*)sda->curr_dta;
  e=query(op,n-2);
  found:
  if(e)break;if(plen!=26){e=59;break;}
  fzero(&sda->found_file,32);fcopy(sda->found_file.fname,payload+3,11);
  sda->found_file.fattr=payload[2];sda->found_file.time_lstupd=get16(payload+14);sda->found_file.date_lstupd=get16(payload+16);sda->found_file.fsize=get32(payload+18);
  if(op==0x1b){fzero(dta,21);dta->drv_lett=drive|128;fcopy(dta->srch_tmpl,sda->fcb_fn1,11);dta->srch_attr=sda->srch_attr;}
  dta->par_clstr=get16(payload+22);dta->dir_entry=get16(payload+24);
  fcopy((u8 far*)dta+21,&sda->found_file,32);break;
 case 0x16:case 0x17:
  if(!writable&&(op==0x17||(sda->open_mode&3))){e=5;break;}
  request[2]=op==0x17?2:sda->open_mode;set16(request+3,op==0x17?fs_arg:0);
  n=patharg(5);if((int)n<0){e=3;break;}e=query(op,n-2);if(e)break;
  if(plen!=27){e=59;break;}
  sft->file_attr=payload[2];sft->dev_info_word=0x8040|drive;sft->dev_drvr_ptr=0;
  sft->start_sector=get16(payload+22);sft->file_time=get32(payload+14);sft->file_size=get32(payload+18);sft->file_pos=0;
  sft->open_mode=(sft->open_mode&0xff00)|payload[26];sft->rel_sector=sft->abs_sector=0xffff;sft->dir_sector=0;sft->dir_entry_no=255;fcopy(sft->file_name,payload+3,11);opens++;break;
 case 5:case 0x0f:
  n=patharg(2);if((int)n<0){e=3;break;}e=query(op,n-2);if(e||op==5)break;
  if(plen!=11){e=59;break;}fs_regs.cx=get16(payload+2);fs_regs.dx=get16(payload+4);fs_regs.bx=get16(payload+8);fs_regs.di=get16(payload+6);fs_regs.ax=payload[10];break;
 default:fail(1);return;
 }if(e)fail(e);else if(op==1||op==3||op==6||op==7||op==0x0e||op==0x11||op==0x13||op==0x16||op==0x17)fs_regs.ax=0;
}
#pragma code_seg ("FSINIT", "INITCODE")
int __cdecl ll_main(void){u8 far*lol;u16 env,i;int k;u32 vec;
 lf=logopen("LLFS.LOG");if(lf<0)lf=create("LLFS.LOG");if(lf>=0)seekend(lf);say("LiteLink LLFS 0.0.2 - DOS network filesystem");args();fs_psp=getpsp();fs_control(0);
 if(ac==1&&ieq(argument,"/U")){
  if((fs_info[0]&0xff00)!=0xd700||fs_info[1]!=0x4c4c||fs_info[2]!=0x4653){say("LLFS is not installed.");return 1;}
  if(curdrive()==(fs_info[0]&255)){say("Switch to a local drive before LLFS /U.");return 1;}
  vec=vector();if((u16)vec!=fs_info[6]||(u16)(vec>>16)!=fs_info[7]){say("Another INT 2F owner was installed later; cannot safely unload.");return 1;}
  fs_control(1);if(fs_info[0]){say("Open files prevent unmount.");return 1;}fs_restore();if(release(fs_info[3])){say("Unmounted, but DOS memory release failed.");return 1;}say("LLFS unmounted; NIC and resident memory released.");val("Resident stack peak bytes=",fs_info[2]&0x7fff);if(fs_info[2]&0x8000)say("WARNING: write outcome uncertain; inspect affected files. No operation was replayed.");return 0;
 }
 if(ac){say("Usage: LLFS [ /U ] ; settings in LL.INI");return 1;}
 if((fs_info[0]&0xff00)==0xd700&&fs_info[1]==0x4c4c&&fs_info[2]==0x4653){say("LLFS is already installed.");return 1;}
 if(dosver()!=0x1e03){say("This experimental redirector requires actual DOS 3.30.");return 1;}
 if(!fsconfig()){say("Invalid LL.INI: require NETWORK, SHELL HOST/PORT, FILESYSTEM DRIVE/SHARE.");return 1;}
 lol=getlist();lastdrv=lol[0x21];if(drive>=lastdrv){say("Configured drive exceeds LASTDRIVE in CONFIG.SYS.");return 1;}
 cds=(struct cdsstruct far*)(*(u8 far*far*)(lol+0x16)+drive*81);
 if(cds->flags){say("Configured drive is occupied; choose a free letter in LL.INI.");return 1;}
 sda=getsda();if(!sda){say("DOS SDA unavailable.");return 1;}
 say("Connecting to configured server...");if(net_open()){val("Connect error=",word(3884));net_close();return 1;}
 live=1;zero(request,14);copy(request,"LLFS    0.002",13);
 if(!frameout(HELLO,request,14)||(k=framein())!=WELCOME||plen!=2||!(get16(payload)&1)){say("LLFS handshake failed.");net_close();return 1;}
 if(writable&&!(get16(payload)&2)){say("Host is read-only; set HOST READONLY=OFF to permit this mount.");net_close();return 1;}
 request[2]=2;request[3]=(u8)writable;copy(request+4,"FILES",6);if(query(0,8)){say("Share rejected.");net_close();return 1;}
 fcopy(&saved_cds,cds,81);fzero(cds,81);cds->current_path[0]='A'+drive;cds->current_path[1]=':';cds->current_path[2]='\\';cds->backslash_offset=2;cds->flags=0xc000;
 for(i=0;i<sizeof(fs_stack);i++)fs_stack[i]=0xa5;fs_install();say(writable?"Configured drive mounted read/write; LLFS /U unmounts.":"Configured drive mounted read-only; LLFS /U unmounts.");val("Resident paragraphs=",fs_paras);val("Resident bytes=",fs_paras<<4);
 env=*(u16 far*)MK_FP(fs_psp,0x2c);if(env){release(env);*(u16 far*)MK_FP(fs_psp,0x2c)=0;}
 if(lf>=0)cl(lf);lf=-1;for(i=0;i<5;i++)cl(i);fs_keep=1;return 0;
}
