/* Public LLFS read/write verifier. Use a fresh dedicated export.
 * Refuses existing RWTREE/RWFINAL; no emulator or boot-drive assumptions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <direct.h>
#include <process.h>
#include <errno.h>
static int errors;static unsigned char data[4096];
#define CHECK(x) do{if(!(x)){printf("FAIL %u: %s (errno=%d)\n",__LINE__,#x,errno);errors++;}}while(0)
static int trunc0(int);
#pragma aux trunc0="mov ah,40h" "xor cx,cx" "int 21h" "sbb ax,ax" parm [bx] value [ax] modify [cx];
static int lockrange(int,unsigned,long,long);
#pragma aux lockrange="mov ah,5ch" "int 21h" "sbb ax,ax" parm [bx] [al] [cx dx] [si di] value [ax];
static void path(char*out,char drive,const char*tail){sprintf(out,"%c:\\%s",drive,tail);}
int main(int argc,char**argv){char p[128],q[128],root[4],oldcwd[128];char drive;int olddrive;int h=0,h2=0;unsigned d=0,t=0,a,j,n;unsigned long pos;int f,r,dupfd;struct diskfree_t freeinfo;
 if(argc<2||argc>3){puts("Usage: LLRWTEST drive [path-to-LLFS.EXE]");return 2;}
 drive=argv[1][0];if(drive>='a'&&drive<='z')drive-=32;
 if(drive<'C'||drive>'Z'||(argv[1][1]&&(argv[1][1]!=':'||argv[1][2]))){puts("Invalid target drive.");return 2;}
 olddrive=_getdrive();if(olddrive==drive-'A'+1||!getcwd(oldcwd,sizeof(oldcwd))){puts("Run from a local drive, not the tested drive.");return 2;}
 sprintf(root,"%c:\\",drive);
 path(p,drive,"RWTREE");r=_dos_getfileattr(p,&a);if(r!=2&&r!=3){puts("RWTREE exists or is inaccessible; use a fresh test export.");return 2;}
 path(p,drive,"RWFINAL");r=_dos_getfileattr(p,&a);if(r!=2&&r!=3){puts("RWFINAL exists or is inaccessible; use a fresh test export.");return 2;}
 CHECK(_dos_getdiskfree(drive-'A'+1,&freeinfo)==0);CHECK(freeinfo.avail_clusters>0);printf("Space: %u free clusters, %u bytes/sector\n",freeinfo.avail_clusters,freeinfo.bytes_per_sector);
 path(p,drive,"RWTREE");if(mkdir(p)){puts("Cannot create test directory.");return 1;}path(q,drive,"RWTREE\\INNER");if(mkdir(q)){puts("Cannot create nested test directory.");return 1;}CHECK(rmdir(p)!=0);
 CHECK(_chdrive(drive-'A'+1)==0);CHECK(chdir(q)==0);CHECK(chdir("..")==0);CHECK(rmdir(p)!=0);CHECK(chdir(root)==0);CHECK(_chdrive(olddrive)==0);CHECK(chdir(oldcwd)==0);
 path(p,drive,"RWTREE\\DATA.BIN");f=_sopen(p,O_CREAT|O_TRUNC|O_RDWR|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE);CHECK(f>=0);
 if(f>=0){
  pos=0;while(pos<11397){n=(unsigned)(11397-pos);if(n>sizeof(data))n=sizeof(data);for(j=0;j<n;j++)data[j]=(unsigned char)((pos+j)*29+7);r=write(f,data,n);CHECK(r==n);if(r<=0)break;pos+=r;}
  CHECK(lseek(f,511,SEEK_SET)==511);CHECK(write(f,"PATCHED-BOUNDARY!",17)==17);CHECK(lseek(f,0,SEEK_END)==11397);CHECK(write(f,"TAIL",4)==4);CHECK(filelength(f)==11401);
  CHECK(_dos_commit(f)==0);dupfd=dup(f);CHECK(dupfd>=0);if(dupfd>=0){CHECK(close(dupfd)==0);CHECK(lseek(f,100,SEEK_SET)==100);CHECK(read(f,data,1)==1);CHECK(data[0]==(unsigned char)(100*29+7));}
  r=spawnlp(P_WAIT,argc==3?argv[2]:"LLFS.EXE","LLFS","/U",NULL);CHECK(r==1);
  h2=(unsigned)_sopen(p,O_RDWR|O_BINARY,SH_DENYNO);CHECK((int)h2>=0);
  CHECK(lockrange(f,0,2,4)==0);CHECK(lockrange(f,0,2,4)!=0);
  if((int)h2>=0){CHECK(lseek(h2,3,SEEK_SET)==3);CHECK(write(h2,"X",1)==-1);CHECK(close(h2)==0);}
  CHECK(lockrange(f,1,2,4)==0);
  CHECK(lseek(f,4099,SEEK_SET)==4099);CHECK(trunc0(f)==0);CHECK(filelength(f)==4099);
  CHECK(lseek(f,65530L,SEEK_SET)==65530L);CHECK(write(f,"LONG-OFFSET!",12)==12);CHECK(filelength(f)==65542L);CHECK(lseek(f,65530L,SEEK_SET)==65530L);CHECK(read(f,data,12)==12);CHECK(!memcmp(data,"LONG-OFFSET!",12));
  CHECK(lseek(f,4099,SEEK_SET)==4099);CHECK(trunc0(f)==0);
  d=((2024-1980)<<9)|(5<<5)|6;t=(7<<11)|(8<<5)|5;CHECK(_dos_setftime(f,d,t)==0);CHECK(_dos_commit(f)==0);CHECK(_dos_getftime(f,&a,&n)==0);CHECK(a==d&&n==t);CHECK(close(f)==0);
 }
 CHECK(_dos_creatnew(p,0,&h)==80);CHECK(_dos_open(p,O_RDONLY,&h)==0);CHECK(_dos_getftime(h,&a,&n)==0);CHECK(a==d&&n==t);
 if(!errors){CHECK(_dos_read(h,data,4096,&n)==0);CHECK(n==4096);for(j=0;j<n;j++){unsigned char expected=(j>=511&&j<528)?(unsigned char)"PATCHED-BOUNDARY!"[j-511]:(unsigned char)(j*29+7);if(data[j]!=expected){printf("Mismatch at %u\n",j);errors++;break;}}}CHECK(_dos_close(h)==0);
 path(q,drive,"RWTREE\\PERSIST.BIN");CHECK(rename(p,q)==0);path(p,drive,"RWTREE\\READONLY.TXT");CHECK(_dos_creat(p,1,&h)==0);CHECK(_dos_write(h,"R",1,&n)==0&&n==1);CHECK(_dos_close(h)==0);CHECK(_dos_getfileattr(p,&a)==0&&(a&1));CHECK(_dos_open(p,O_WRONLY,&h)!=0);CHECK(_dos_setfileattr(p,0)==0);CHECK(unlink(p)==0);
 path(p,drive,"RWTREE\\ONE.TMP");CHECK(_dos_creat(p,0,&h)==0);CHECK(_dos_close(h)==0);path(p,drive,"RWTREE\\TWO.TMP");CHECK(_dos_creat(p,0,&h)==0);CHECK(_dos_close(h)==0);
 path(p,drive,"RWTREE\\INNER");CHECK(rmdir(p)==0);path(p,drive,"RWTREE");path(q,drive,"RWFINAL");CHECK(rename(p,q)==0);
 printf("READ/WRITE DOS API checks: %s (%d errors)\n",errors?"FAIL":"PASS",errors);return errors?1:0;}
