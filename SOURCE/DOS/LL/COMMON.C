/* LiteLink source guide:
 * Common LLTime/LLGet DOS runtime, config parser and transport setup.
 * Textually included by TIME.C or GET.C. Defines the direct DOS-call wrappers.
 * NETWORK selects the direct, polled NIC backend; utility-specific sections remain separate.
 */
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned long u32;
u8 netstate[5120];
u8 ll_ntp_request[48],ll_ntp_reply[48];u16 ll_ntp_port=123,ll_ntp_active,ll_ntp_received;
extern int __cdecl ll_prepare(void);extern int __cdecl ll_ntp(void);extern int __cdecl net_open(void);extern int __cdecl net_recv(void*,unsigned);extern int __cdecl net_send(const void*,unsigned);extern void __cdecl net_close(void);extern u16 __cdecl net_ticks(void);
static int op(const char*);static int rd(int,void*,unsigned);static int wr(int,const void*,unsigned);static int cl(int);static int create(const char*);static void seekend(int);static void del(const char*);static u8 pin(u16);static void pout(u16,u8);static void tail(char*);
#pragma aux op="mov ax,3d00h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
#pragma aux rd="mov ah,3fh" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
#pragma aux wr="mov ah,40h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [bx] [dx] [cx] value [ax] modify [ax];
#pragma aux cl="mov ah,3eh" "int 21h" "sbb ax,ax" parm [bx] value [ax] modify [ax];
#pragma aux create="mov ax,5b00h" "xor cx,cx" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax cx];
static int logopen(const char*);
#pragma aux logopen="mov ax,3d02h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
#pragma aux seekend="mov ax,4202h" "xor cx,cx" "xor dx,dx" "int 21h" parm [bx] modify [ax cx dx];
#pragma aux del="mov ah,41h" "int 21h" parm [dx] modify [ax];
#pragma aux pin="in al,dx" parm [dx] value [al];
#pragma aux pout="out dx,al" parm [dx] [al];
/* PSP segment is the word at DGROUP:0? Use DOS AH=62h, not loader ES. */
#pragma aux tail="push es" "mov ah,62h" "int 21h" "mov es,bx" "xor cx,cx" "mov cl,es:[80h]" "mov si,81h" "jcxz short L2" "L1:" "mov al,es:[si]" "mov [di],al" "inc si" "inc di" "loop L1" "L2:" "mov byte ptr [di],0" "pop es" parm [di] modify [ax bx cx si di];
static unsigned len(const char*s){unsigned n=0;while(s[n])n++;return n;}static void cp(char*d,const char*s){while((*d++=*s++)!=0);}static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}static char upper(char c){return c>='a'&&c<='z'?c-32:c;}static int ieq(const char*a,const char*b){while(*a&&upper(*a)==upper(*b)){a++;b++;}return upper(*a)==upper(*b);}static int prefix(const char*a,const char*b){while(*b)if(upper(*a++)!=upper(*b++))return 0;return 1;}
static u16 word(unsigned o){return netstate[o]|((u16)netstate[o+1]<<8);}static void put(unsigned o,u16 n){netstate[o]=(u8)n;netstate[o+1]=(u8)(n>>8);}static void bytes(unsigned o,const u8*s,unsigned n){while(n--)netstate[o++]=*s++;}
static int lf=-1;static char line[768],ini[4096],cmd[128],*av[8];static unsigned ac;static char macoverride[18];static u16 macmode,nictype,idport=0x110,macformat_seen;static u16 nic,port=0,timeout=30,pingwait=3,count=4;static u8 ip[4],mask[4],gate[4],dns[4];static char host[254],path[256],out[13];static u32 limit=16777216UL;static u16 retries=2,retrydelay=3,blocksize=256;static int tzoffset=0;static char timeserver[254];
static void say(const char*s){wr(2,s,len(s));wr(2,"\r\n",2);if(lf>=0){wr(lf,s,len(s));wr(lf,"\r\n",2);}}
static void num(char*s,u32 n){char b[12];unsigned i=0,j=len(s);do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)s[j++]=b[--i];s[j]=0;}static void val(const char*s,u32 n){cp(line,s);num(line,n);say(line);}static void hx(char*s,u8 n){const char*d="0123456789ABCDEF";s[0]=d[n>>4];s[1]=d[n&15];s[2]=0;}
static void macstr(char*s,const u8*p,unsigned stride){unsigned i;for(i=0;i<6;i++){hx(s,p[i*stride]);s+=2;if(i<5)*s++=':';}*s=0;}
static void addrstr(char*s,const u8*a){unsigned i;*s=0;for(i=0;i<4;i++){num(s,a[i]);if(i<3){s+=len(s);*s++='.';*s=0;}}}
static int number(const char*s,u16*out,unsigned base){u32 n=0;unsigned v;if(!*s)return 0;if(base==16&&s[0]=='0'&&upper(s[1])=='X')s+=2;if(!*s)return 0;while(*s){char c=upper(*s++);if(c>='0'&&c<='9')v=c-'0';else if(c>='A'&&c<='F')v=c-'A'+10;else return 0;if(v>=base)return 0;n=n*base+v;if(n>65535)return 0;}*out=(u16)n;return 1;}
static int address(const char*s,u8*a){unsigned i,j;u16 n;char b[4];for(i=0;i<4;i++){j=0;while(*s&&*s!='.'){if(j==3)return 0;b[j++]=*s++;}b[j]=0;if(!number(b,&n,10)||n>255)return 0;a[i]=(u8)n;if(i<3){if(*s++!='.')return 0;}else if(*s)return 0;}return 1;}
static int macconfig(const char*s){unsigned i;u16 n;char b[3];u8 any=0,first=0;if(len(s)!=17)return 0;for(i=0;i<6;i++){b[0]=s[i*3];b[1]=s[i*3+1];b[2]=0;if(!number(b,&n,16))return 0;if(i<5&&s[i*3+2]!=':')return 0;if(!i)first=(u8)n;any|=(u8)n;}return any&&!(first&1);}
static char* trim(char*s){char*e;while(*s==' '||*s=='\t')s++;e=s+len(s);while(e>s&&(e[-1]==' '||e[-1]=='\t'))*--e=0;return s;}
#define LL_OPEN op
#define LL_LEN len
#define LL_COPY cp
#include "../LLPATH.H"
static int config(void){int f,n,section=0;unsigned seen=0,hasdns=0;char*s,*e,*v;f=ll_open_master();if(f>=0){say("Master configuration:");say(ll_masterpath);}if(f<0){say("Missing LL.INI; no network operation attempted.");return 0;}n=rd(f,ini,sizeof(ini)-1);cl(f);if(n<1||n==sizeof(ini)-1)return 0;ini[n]=0;s=ini;while(*s){while(*s=='\r'||*s=='\n')s++;if(!*s)break;e=s;while(*e&&*e!='\r'&&*e!='\n')e++;if(*e)*e++=0;s=trim(s);if(!*s||*s==';'||*s=='#'){s=e;continue;}if(*s=='['){if(ieq(s,"[NETWORK]"))section=1;else if(ieq(s,"[TIME]"))section=2;else if(ieq(s,"[GET]"))section=3;else if(ieq(s,"[SHELL]")||ieq(s,"[HOST]")||ieq(s,"[EXECUTION]"))section=4;else return 0;s=e;continue;}v=s;while(*v&&*v!='=')v++;if(!*v)return 0;*v++=0;s=trim(s);v=trim(v);if(section==4){s=e;continue;}
 if(section==1){if(ieq(s,"NIC")){if(ieq(v,"NE2000"))nictype=0;else if(ieq(v,"PICOMEM"))nictype=1;else if(ieq(v,"3C509")||ieq(v,"3C509B"))nictype=2;else{say("Unknown NIC; use NE2000, PICOMEM or 3C509. No hardware touched.");return 0;}}else if(ieq(s,"IDPORT")){if(!number(v,&idport,16)||idport<0x100||idport>0x1f0||(idport&15))return 0;}else if(ieq(s,"NICBASE")){if(!number(v,&nic,16)||nic<0x200||nic>0x3e0||(nic&15))return 0;seen|=1;}else if(ieq(s,"IP")){if(!address(v,ip))return 0;seen|=2;}else if(ieq(s,"NETMASK")){if(!address(v,mask))return 0;seen|=4;}else if(ieq(s,"GATEWAY")){if(!address(v,gate))return 0;seen|=8;}else if(ieq(s,"DNS")){if(!address(v,dns))return 0;hasdns=1;}else if(ieq(s,"MAC")){if(!macconfig(v))return 0;cp(macoverride,v);}else if(ieq(s,"MACFORMAT")){macformat_seen=1;if(ieq(v,"AUTO"))macmode=0;else if(ieq(v,"PACKED"))macmode=2;else if(ieq(v,"DOUBLED"))macmode=1;else return 0;}else if(ieq(s,"TIMEOUT")){if(!number(v,&timeout,10)||timeout<2||timeout>300)return 0;}else if(ieq(s,"RETRIES")){if(!number(v,&retries,10)||retries>5)return 0;}else if(ieq(s,"RETRYDELAY")){if(!number(v,&retrydelay,10)||retrydelay>60)return 0;}else if(ieq(s,"BLOCKSIZE")){if(!number(v,&blocksize,10)||(blocksize!=128&&blocksize!=256&&blocksize!=512))return 0;}else if(ieq(s,"PINGWAIT")){if(!number(v,&pingwait,10)||!pingwait||pingwait>60)return 0;}else if(ieq(s,"PINGCOUNT")){if(!number(v,&count,10)||!count||count>20)return 0;}else if(!ieq(s,"IRQ"))return 0;}
 else if(section==2){if(ieq(s,"SERVER")){if(!*v||len(v)>253)return 0;cp(timeserver,v);}else if(ieq(s,"PORT")){if(!number(v,&ll_ntp_port,10)||!ll_ntp_port)return 0;}else if(ieq(s,"UTCOFFSET")){u16 t;int sign=1;if(*v=='-'){sign=-1;v++;}else if(*v=='+')v++;if(!number(v,&t,10)||t>840||(sign<0&&t>720))return 0;tzoffset=sign*(int)t;}else return 0;}
 else if(section==3){if(ieq(s,"MAXKB")){u16 kb;if(!number(v,&kb,10)||!kb)return 0;limit=(u32)kb*1024UL;}else return 0;}else return 0;s=e;}
 if(!hasdns){unsigned i;for(i=0;i<4;i++)dns[i]=gate[i];}if(nictype!=2&&(nic&31))return 0;if(nictype==1&&!macformat_seen)macmode=2;return seen==15;}
static void args(void){char*s;tail(cmd);s=cmd;while(*s&&ac<8){while(*s==' '||*s=='\t')s++;if(!*s)break;av[ac++]=s;while(*s&&*s!=' '&&*s!='\t')s++;if(*s)*s++=0;}}
static void setup(void){unsigned i;for(i=0;i<sizeof(netstate);i++)netstate[i]=0;put(4,port);put(6,timeout*18);put(18,len(host));bytes(20,(u8*)host,len(host));put(3912,nic);bytes(3978,(u8*)macoverride,len(macoverride)+1);put(4050,macmode);put(4052,nictype);put(4054,idport);bytes(3996,ip,4);bytes(4000,mask,4);bytes(4004,gate,4);bytes(4008,dns,4);ll_ntp_active=ll_ntp_received=0;}
static void waitretry(unsigned attempt){u16 ticks=(u16)(retrydelay*18*(attempt+1)),begin=net_ticks();if(ticks>1080)ticks=1080;val("Retry backoff ticks=",ticks);while((u16)(net_ticks()-begin)<ticks);}

/* Stable state offsets are documented by NNETDEF.INC. No live register pokes. */
static void nic_report(void){
 say(nictype==2?"NIC=3C509B native byte-I/O polling":(nictype==1?"NIC=PICOMEM native NE2000 polling":"NIC=NE2000 native polling"));
 cp(line,"NICBASE=0x");hx(line+len(line),(u8)(nic>>8));hx(line+len(line),(u8)nic);say(line);
 cp(line,"Station MAC=");macstr(line+len(line),netstate+4040,1);say(line);
 if(nictype==2){val("NIC initialization/last stage=",word(4062));val("NIC last status (decimal)=",word(4060));val("Product ID (decimal)=",word(4058));val("EEPROM capabilities (decimal)=",word(4072));
 if(word(4062)>=4){say(word(4056)==3?"Transceiver=BNC/10Base2":(word(4056)==1?"Transceiver=AUI":"Transceiver=TP/10BaseT"));val("Configured hardware IRQ (not installed)=",word(4074));if(word(4062)>=6&&word(4056)==0)say((word(4082)&0x800)?"TP link beat detected":"TP link beat not detected (or disabled in EEPROM)");}
 val("NIC TX completions=",word(4064));val("NIC TX errors/timeouts=",word(4066));val("NIC RX frames=",word(4068));val("NIC RX discarded errors=",word(4070));
 if(word(3884)==430)say("Card active or resident packet driver present: clean boot without the driver.");
 if(word(3884)==432)say("Remove the PicoMEM MAC= override for NIC=3C509; use the card EEPROM MAC.");
 if(word(3884)==433)say("8-bit XT operation requires the B-generation 3C509B.");
 if(word(3884)==437)say("AUTO SELECT media is unsupported; configure a fixed BNC/TP/AUI connector first.");
 if(word(3884)==434)say("NICBASE differs from the card EEPROM; no base reconfiguration attempted.");
 }
 say("Direct polling; no LiteLink IRQ handler, resident driver or TSR.");
}
