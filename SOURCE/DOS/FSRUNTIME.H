/* Installer-only DOS/config helpers. No 4 KiB retained INI buffer.
 * Parsing streams 32-byte chunks into the wire buffer before TCP connects.
 * Resident requests/replies share one 512-byte buffer, never simultaneously.
 */
#include <i86.h>
/* Same 512-byte TCP RX/TX and 1514-byte Ethernet frame caches.
 * 1536 unused bytes from the original Win16 NETWORK layout are removed. */
#define FS_OFFSET(o) ((o)>=1920?(o)-1536:(o))
u8 netstate[3584];
extern int __cdecl net_open(void);extern int __cdecl net_recv(void*,unsigned);extern int __cdecl net_send(const void*,unsigned);extern void __cdecl net_close(void);extern u16 __cdecl net_ticks(void);
#define request payload
#define cp textcopy
#define len slen
static u16 timeout=10;
static u16 word(unsigned o){return get16(netstate+FS_OFFSET(o));}
static void put(unsigned o,u16 v){set16(netstate+FS_OFFSET(o),v);}
#pragma code_seg ("FSINIT", "INITCODE")
static int op(const char*);static int rd(int,void*,unsigned);static int wr(int,const void*,unsigned);static int cl(int);static int create(const char*);static int logopen(const char*);static void seekend(int);
#pragma aux op="mov ax,3d00h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
#pragma aux rd="mov ah,3fh" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
#pragma aux wr="mov ah,40h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
#pragma aux cl="mov ah,3eh" "int 21h" "sbb ax,ax" parm [bx] value [ax] modify [ax];
#pragma aux create="mov ax,5b00h" "xor cx,cx" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax cx];
#pragma aux logopen="mov ax,3d02h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
#pragma aux seekend="mov ax,4202h" "xor cx,cx" "xor dx,dx" "int 21h" parm [bx] modify [ax cx dx];
static int lf=-1;static char *argument;static unsigned ac;
static void say(const char*s){wr(2,s,len(s));wr(2,"\r\n",2);if(lf>=0){wr(lf,s,len(s));wr(lf,"\r\n",2);}}
static void val(const char*s,u16 n){char b[8];unsigned i=0;say(s);do{b[i++]='0'+n%10;n/=10;}while(n);while(i){char c=b[--i];wr(2,&c,1);if(lf>=0)wr(lf,&c,1);}wr(2,"\r\n",2);if(lf>=0)wr(lf,"\r\n",2);}
static char upper(char c){return c>='a'&&c<='z'?c-32:c;}
static int ieq(const char*a,const char*b){while(*a&&upper(*a)==upper(*b)){a++;b++;}return upper(*a)==upper(*b);}
static int number(const char*s,u16*out,unsigned base){u16 n=0,v;if(base==16&&s[0]=='0'&&upper(s[1])=='X')s+=2;if(!*s)return 0;while(*s){char c=upper(*s++);if(c>='0'&&c<='9')v=c-'0';else if(c>='A'&&c<='F')v=c-'A'+10;else return 0;if(v>=base||n>(65535u-v)/base)return 0;n=n*base+v;}*out=n;return 1;}
static char*trim(char*s){char*e;while(*s==' '||*s=='\t')s++;e=s+len(s);while(e>s&&(e[-1]==' '||e[-1]=='\t'))*--e=0;return s;}
static int addr(const char*s,u8*out){u16 n;unsigned i,j;char b[4];for(i=0;i<4;i++){j=0;while(*s&&*s!='.'){if(j==3)return 0;b[j++]=*s++;}b[j]=0;if(!number(b,&n,10)||n>255)return 0;out[i]=n;if(i<3){if(*s++!='.')return 0;}else if(*s)return 0;}return 1;}
static int exepath(char*);
#pragma aux exepath="push es" "mov ah,62h" "int 21h" "mov es,bx" "mov ax,es:[2ch]" "or ax,ax" "jz short EF" "mov es,ax" "xor si,si" "mov cx,32760" "E0:" "mov al,es:[si]" "inc si" "dec cx" "jz short EF" "or al,al" "jnz E0" "cmp byte ptr es:[si],0" "jne E0" "inc si" "cmp word ptr es:[si],0" "je short EF" "add si,2" "mov cx,127" "E1:" "mov al,es:[si]" "inc si" "mov [di],al" "inc di" "or al,al" "jz short EO" "loop E1" "EF:" "xor ax,ax" "jmp short ED" "EO:" "mov ax,1" "ED:" "pop es" parm [di] value [ax] modify [ax bx cx si di];
static int master(void){char*p=(char*)payload;unsigned n;int f;if(exepath(p)){n=len(p);while(n&&p[n-1]!='\\')n--;if(n&&n+7<128){cp(p+n,"LL.INI");f=op(p);if(f>=0){say(p);return f;}}}say("LL.INI");return op("LL.INI");}
static u16 getpsp(void);
static void args(void){u16 p;char far*s;unsigned i;p=getpsp();s=MK_FP(p,0x80);i=(u8)*s++;if(i>127)i=127;fcopy(payload+256,s,i);payload[256+i]=0;argument=trim((char*)payload+256);ac=*argument?1:0;}
/* Resident packet data are initialized directly; no duplicate address/config arrays. */
static int fsconfig(void){int f,z=0;unsigned used=0,pos=0,n,section=0,seen=0,hasdns=0,shareok=0,macmode=0;char*s,*v;u16 t,kind=0;u8 first,any;unsigned i;char b[3];
 put(6,180);put(4054,0x110);f=master();if(f<0)return 0;
 for(;;){n=0;for(;;){if(pos==used){z=rd(f,payload+480,32);if(z<0){cl(f);return 0;}used=z;pos=0;if(!used)break;}t=payload[480+pos++];if(t=='\r')continue;if(t=='\n')break;if(n==479){cl(f);return 0;}payload[n++]=(u8)t;}if(!n&&!used)break;payload[n]=0;s=trim((char*)payload);if(!*s||*s==';'||*s=='#')continue;
 if(*s=='['){section=ieq(s,"[NETWORK]")?1:ieq(s,"[SHELL]")?2:ieq(s,"[FILESYSTEM]")?3:ieq(s,"[ISO]")?4:0;continue;}
 if(!section)continue;v=s;while(*v&&*v!='=')v++;if(!*v){cl(f);return 0;}*v++=0;s=trim(s);v=trim(v);
 if(section==1){
  if(ieq(s,"NIC")){if(ieq(v,"NE2000"))kind=0;else if(ieq(v,"PICOMEM"))kind=1;else if(ieq(v,"3C509")||ieq(v,"3C509B"))kind=2;else goto bad;put(4052,kind);}
  else if(ieq(s,"NICBASE")){if(!number(v,&t,16)||t<0x200||t>0x3e0||(t&15))goto bad;put(3912,t);seen|=1;}
  else if(ieq(s,"IP")){if(!addr(v,netstate+FS_OFFSET(3996)))goto bad;seen|=2;}
  else if(ieq(s,"NETMASK")){if(!addr(v,netstate+FS_OFFSET(4000)))goto bad;seen|=4;}
  else if(ieq(s,"GATEWAY")){if(!addr(v,netstate+FS_OFFSET(4004)))goto bad;seen|=8;}
  else if(ieq(s,"DNS")){if(!addr(v,netstate+FS_OFFSET(4008)))goto bad;hasdns=1;}
  else if(ieq(s,"IDPORT")){if(!number(v,&t,16)||t<0x100||t>0x1f0||(t&15))goto bad;put(4054,t);}
  else if(ieq(s,"MACFORMAT")){macmode=1;if(ieq(v,"AUTO"))put(4050,0);else if(ieq(v,"PACKED"))put(4050,2);else if(ieq(v,"DOUBLED"))put(4050,1);else goto bad;}
  else if(ieq(s,"MAC")){if(len(v)!=17)goto bad;any=first=0;for(i=0;i<6;i++){b[0]=v[i*3];b[1]=v[i*3+1];b[2]=0;if(!number(b,&t,16)||(i<5&&v[i*3+2]!=':'))goto bad;any|=t;if(!i)first=t;}if(!any||(first&1))goto bad;cp((char*)netstate+FS_OFFSET(3978),v);}
  else if(ieq(s,"TIMEOUT")){if(!number(v,&timeout,10)||timeout<2||timeout>60)goto bad;}
 }
 if(section==2){if(ieq(s,"HOST")){if(!*v||len(v)>253)goto bad;cp((char*)netstate+20,v);put(18,len(v));}else if(ieq(s,"PORT")){if(!number(v,&t,10)||!t)goto bad;put(4,t);}}
 if(section==3){if(ieq(s,"DRIVE")){if(!v[0]||(v[1]&&(v[1]!=':'||v[2]))||upper(v[0])<'C'||upper(v[0])>'Z')goto bad;drive=upper(v[0])-'A';}else if(ieq(s,"SHARE")){if(!ieq(v,"FILES"))goto bad;shareok=1;}else if(ieq(s,"READONLY")){if(ieq(v,"OFF"))writable=1;else if(ieq(v,"ON"))writable=0;else goto bad;}else if(ieq(s,"TIMEOUT")){if(!number(v,&timeout,10)||timeout<2||timeout>60)goto bad;}else goto bad;}
 if(section==4&&ieq(s,"ENABLED")&&!ieq(v,"OFF")){say("ISO support follows the folder-drive prototype.");goto bad;}
 }
 cl(f);if(!hasdns)copy(netstate+FS_OFFSET(4008),netstate+FS_OFFSET(4004),4);if(kind!=2&&(word(3912)&31))return 0;if(kind==1&&!macmode)put(4050,2);put(6,timeout*18);return seen==15&&shareok&&drive!=0xffff&&word(18)&&word(4);
 bad:cl(f);return 0;
}

#pragma code_seg ()
