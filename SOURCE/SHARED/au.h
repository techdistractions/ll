/* LiteLink source guide:
 * Shared wire constants, bounded 8.3 validation, little-endian fields and CRC32.
 * Header-local helpers are compiled into each endpoint; no separate shared object.
 * Frame counters reset per TCP connection. Only complete valid frames may dispatch.
 */

#ifndef AU_H
#define AU_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
#define BLOCK 512
#define HELLO 1
#define WELCOME 2
#define PUT 3
#define READY 4
#define DATA 5
#define END 6
#define RESULT 7
#define GET 8
#define QUIT 9
#define REQUEST 10
#define LIST 11
#define ENTRY 12
#define CHDIR 13
#define MKDIR 14
#define RENAME 15
#define AU_DELETE 16
#define AU_RUN 17
#define EXEC_RESULT 18
#define AU_CMD 19
#define JOB_SUBMIT 20
#define JOB_ACCEPT 21
#define JOB_RESUME 22
#define JOB_RESUME_ACK 23
#define JOB_READY 24
#define LL_POLL 25
#define LL_CLIENT 26
#define LL_IDLE 27
#define LL_CANCEL 28
#define LL_UI_TEST 30
#define LL_CAP_MANAGER 4
#define EXEC_BYTES 20
#define XR_OK 0
#define XR_EXIT 1
#define XR_LAUNCH 2
#define XR_TIMEOUT 3
#define XR_DISABLED 4
#define XR_INVALID 5
#define XR_STATE 6
#define XR_SHELL 7
#define OK 0
#define INVALID 1
#define MISSING 2
#define EXISTS 3
#define IOERR 4
#define INTEGRITY 5
#define PROTOCOL 6
static unsigned slen(const char*s){unsigned n=0;while(s[n])n++;return n;}
static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void copy(void*d,const void*s,unsigned n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;}
static void zero(void*d,unsigned n){u8*a=d;while(n--)*a++=0;}
static void textcopy(char*d,const char*s){while((*d++=*s++)!=0);}
static u16 get16(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static void set16(u8*p,u16 n){p[0]=(u8)n;p[1]=(u8)(n>>8);}
static u32 get32(const u8*p){return (u32)get16(p)|((u32)get16(p+2)<<16);}
static void set32(u8*p,u32 n){set16(p,(u16)n);set16(p+2,(u16)(n>>16));}
/* IEEE CRC32, same wire values; 16-entry table avoids eight bit loops/byte. */
static u32 crcstep(u32 c,const u8*p,unsigned n){static const u32 t[]={0x00000000UL,0x1db71064UL,0x3b6e20c8UL,0x26d930acUL,0x76dc4190UL,0x6b6b51f4UL,0x4db26158UL,0x5005713cUL,0xedb88320UL,0xf00f9344UL,0xd6d6a3e8UL,0xcb61b38cUL,0x9b64c2b0UL,0x86d3d2d4UL,0xa00ae278UL,0xbdbdf21cUL};while(n--){c^=*p++;c=(c>>4)^t[(unsigned)c&15];c=(c>>4)^t[(unsigned)c&15];}return c;}
static int valid(const char*s){unsigned a=0,b=0,dot=0;char base[9];const char*p=s;
 if(!*s||slen(s)>12)return 0;zero(base,sizeof(base));
 while(*p){char c=*p++;if(c=='.'){if(dot||!a)return 0;dot=1;continue;}
 if(c>='a'&&c<='z')c-=32;
 if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')){const char*q="$%'-@~`!(){}^#&";while(*q&&*q!=c)q++;if(!*q)return 0;}
 if(!dot){if(a==8)return 0;base[a++]=c;}else if(++b>3)return 0;}
 if(dot&&!b)return 0;
 if(same(base,"CON")||same(base,"PRN")||same(base,"AUX")||same(base,"NUL")||same(base,"CLOCK$")||same(base,"AUTOUAT"))return 0;
 if((base[0]=='C'&&base[1]=='O'&&base[2]=='M')||(base[0]=='L'&&base[1]=='P'&&base[2]=='T'))if(base[3]>='0'&&base[3]<='9'&&!base[4])return 0;
 return 1;}
/* Platform hooks, exact byte-stream I/O with deadline. */
static int wireout(const u8*,unsigned);
static int wirein(u8*,unsigned);
static u8 payload[BLOCK],header[16];
static u16 plen;
static u32 txid,rxid;
static int frameout(u8 kind,const u8*p,unsigned n){if(n>BLOCK)return 0;copy(header,"AUAT",4);header[4]=1;header[5]=kind;set16(header+6,n);set32(header+8,txid++);set32(header+12,crcstep(0xffffffffUL,p,n)^0xffffffffUL);return wireout(header,16)&&(!n||wireout(p,n));}
/* Return a message kind only after exact-length, sequence and payload-CRC checks.
 * A framing failure invalidates this connection; do not scan for a new header. */
static int framein(void){u32 c;if(!wirein(header,16))return -1;if(header[0]!='A'||header[1]!='U'||header[2]!='A'||header[3]!='T'||header[4]!=1||get32(header+8)!=rxid++)return -1;plen=get16(header+6);if(plen>BLOCK||!wirein(payload,plen))return -1;c=crcstep(0xffffffffUL,payload,plen)^0xffffffffUL;if(c!=get32(header+12))return -1;return header[5];}
static int statusout(u8 kind,u16 code){u8 s[2];set16(s,code);return frameout(kind,s,2);}
static int statusin(u8 kind){if(framein()!=kind||plen!=2)return -1;return get16(payload);}
#endif
