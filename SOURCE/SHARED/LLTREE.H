/* Shared bounded traversal and directory wire extension; no platform I/O. */
#ifndef LL_TREE_H
#define LL_TREE_H
#define LL_TREE 31
#define TREE_SKIP 7
#define TC_SCAN 1
#define TC_MKDIR 2
#define TC_GET 3
#define TC_PUT 4
#define TREE_DEPTH 16
#define TP_MKDIR 1
#define TP_SCAN 2
#define TP_FILE 3
typedef struct {char rel[64];u16 index[TREE_DEPTH],depth,phase;} TreeState;
static int navname(const char*n){return same(n,".")||same(n,"..");}
static int relvalid(const char*s){char p[13];unsigned n;if(slen(s)>63)return 0;while(*s){n=0;while(*s&&*s!='\\'){if(n==12)return 0;p[n++]=*s++;}p[n]=0;if(!valid(p))return 0;if(*s&&!*++s)return 0;}return 1;}
static int reljoin(char*out,const char*a,const char*b){unsigned n=slen(a),m=slen(b);if(!relvalid(a)||!relvalid(b)||n+m+(n&&m?1:0)>63)return 0;textcopy(out,a);if(n&&m)out[n++]='\\';textcopy(out+n,b);return 1;}
static void relup(char*p){unsigned n=slen(p);while(n&&p[n-1]!='\\')n--;if(n)n--;p[n]=0;}
static int tree_begin(TreeState*t,const char*name){if(!valid(name))return 0;zero(t,sizeof(*t));textcopy(t->rel,name);t->depth=1;t->phase=TP_MKDIR;return 1;}
static void tree_leave(TreeState*t){relup(t->rel);if(t->depth)t->depth--;t->phase=t->depth?TP_SCAN:0;}
/* Return 0 for an overlong/deep item; caller records failure and continues scan. */
static int tree_child(TreeState*t,const char*name,u8 attr){char path[64];if(t->index[t->depth-1]==65535U)return 0;t->index[t->depth-1]++;if(!reljoin(path,t->rel,name))return 0;if((attr&16)&&t->depth==TREE_DEPTH)return 0;textcopy(t->rel,path);if(attr&16){t->index[t->depth++]=0;t->phase=TP_MKDIR;}else t->phase=TP_FILE;return 1;}
static void tree_file_done(TreeState*t){relup(t->rel);t->phase=TP_SCAN;}
static void tree_pack(u8*p,TreeState*t){unsigned i;zero(p,100);textcopy((char*)p,t->rel);for(i=0;i<TREE_DEPTH;i++)set16(p+64+i*2,t->index[i]);set16(p+96,t->depth);set16(p+98,t->phase);}
static int tree_unpack(TreeState*t,const u8*p){unsigned i;if(p[63]||!relvalid((char*)p)||get16(p+96)>TREE_DEPTH||get16(p+98)>TP_FILE)return 0;zero(t,sizeof(*t));textcopy(t->rel,(char*)p);t->depth=get16(p+96);t->phase=get16(p+98);if(t->phase&&(!t->depth||!t->rel[0]))return 0;for(i=0;i<TREE_DEPTH;i++)t->index[i]=get16(p+64+i*2);return 1;}
static unsigned itemkind(const char*n,u8 attr){const char*e=n;char ext[5];unsigned i=0;if(attr&16)return 1;if(attr&6)return 5;while(*e&&*e!='.')e++;if(*e)e++;while(*e&&i<4){char c=*e++;ext[i++]=c>='a'&&c<='z'?c-32:c;}ext[i]=0;if(same(ext,"EXE")||same(ext,"COM")||same(ext,"BAT"))return 2;if(same(ext,"ZIP")||same(ext,"ARC")||same(ext,"ARJ")||same(ext,"LZH")||same(ext,"RAR"))return 3;if(same(ext,"TXT")||same(ext,"INI")||same(ext,"CFG")||same(ext,"LOG")||same(ext,"DOC"))return 4;return 0;}
static int itemorder(const char*a,u8 aa,const char*b,u8 ba){char x,y;if(navname(a)||navname(b)){if(same(a,b))return 0;if(same(a,"."))return -1;if(same(b,"."))return 1;if(same(a,".."))return -1;return 1;}if((aa&16)!=(ba&16))return aa&16?-1:1;while(*a&&*b){x=*a++;y=*b++;if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return x<y?-1:1;}return *a?1:(*b?-1:0);}
#endif
