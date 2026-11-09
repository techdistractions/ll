/* Root-relative tree operations. Each call restores the exported DOS cwd. */
#include "LLTREE.H"
static u16 peer_tree;
static int dos_tree_path(char*out,const char*base,const char*rel){char joined[64];unsigned n=slen(rootabs);if(!reljoin(joined,base,rel)||n+slen(joined)+2>=64)return 0;textcopy(out,rootabs);if(joined[0]){if(n&&out[n-1]!='\\')out[n++]='\\';textcopy(out+n,joined);}return 1;}
/* Reuse DOS find state for sequential entries; avoid rereading each folder per file. */
static u8 tree_dta[43];static char tree_findpath[67];static u16 tree_findnext;static int tree_findvalid;
static int dos_tree_scan(const char*full,u16 ordinal,u8*e){int rc,result=0;unsigned at;if(dchdir(full)<0)return -1;dsetdta(tree_dta);if(tree_findvalid&&ordinal&&ordinal==tree_findnext&&same(full,tree_findpath)){at=ordinal;rc=dnext();}else{at=0;rc=dfind("*.*");}while(rc==0){if(!navname((char*)tree_dta+30)){if(at++==ordinal){if(ordinal>=4096){result=-1;break;}if(!valid((char*)tree_dta+30)){result=-2;break;}zero(e,22);textcopy((char*)e,(char*)tree_dta+30);copy(e+13,tree_dta+26,4);e[17]=tree_dta[21];copy(e+18,tree_dta+24,2);copy(e+20,tree_dta+22,2);result=1;break;}}rc=dnext();}tree_findvalid=result>0||result==-2;if(tree_findvalid){textcopy(tree_findpath,full);tree_findnext=ordinal+1;}dsetdta(dta);return result;}
static int dos_tree_parent(char*full,char*leaf,const char*base,const char*rel){char p[64];unsigned n=slen(rel),i=n;while(i&&rel[i-1]!='\\')i--;textcopy(leaf,rel+i);if(!valid(leaf))return 0;textcopy(p,rel);if(i)p[i-1]=0;else p[0]=0;return dos_tree_path(full,base,p)&&dchdir(full)==0;}
static int dos_tree_mkdir(const char*base,const char*rel){char p[67],leaf[13];if(!dos_tree_parent(p,leaf,base,rel))return IOERR;if(dmkdir(leaf)==0)return OK;return dchdir(leaf)==0?OK:EXISTS;}
static int tree_server(const u8*data,unsigned n){char rel[64],full[67],leaf[13],back[67];u8 e[22];unsigned op,ordinal;int st=OK,ok=1,k;if(n<4||n>67||data[n-1]||slen((char*)data+3)!=n-4||!relvalid((char*)data+3))return statusout(RESULT,INVALID);op=data[0];ordinal=get16(data+1);textcopy(rel,(char*)data+3);back[0]='\\';if(dgetdir(back+1)<0)return statusout(RESULT,IOERR);
 if(op==TC_SCAN){if(!dos_tree_path(full,"",rel))ok=statusout(RESULT,INVALID);else{k=dos_tree_scan(full,ordinal,e);ok=k>0?frameout(ENTRY,e,22):statusout(RESULT,k==-2?TREE_SKIP:(k<0?IOERR:OK));}}
 else if(op==TC_MKDIR)ok=statusout(RESULT,(u16)dos_tree_mkdir("",rel));
 else if(op==TC_GET||op==TC_PUT){if(!dos_tree_parent(full,leaf,"",rel))ok=statusout(RESULT,INVALID);else if(op==TC_GET)ok=sendfile(leaf);else if(!statusout(RESULT,OK))ok=0;else{k=framein();if(k!=PUT||plen!=21||!same((char*)payload,leaf))ok=0;else ok=recvfile();}}
 else ok=statusout(RESULT,INVALID);if(dchdir(back)<0)return 0;return ok;}
