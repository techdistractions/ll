/* LiteLink source guide:
 * Unloaded-job state machine, included by AGENT.C (not a separate object).
 * Commit job/batch before JOB_ACCEPT; close NIC and exit; LLSTART CALLs the batch.
 * Resume requires the same ID/command CRC and an actual supervisor completion record.
 */

/* 0.0.3: only disk job state survives. No EXEC, hooks, TSR or waiting parent. */
extern int __cdecl dos_supervised(void);
static int resume_pending,resume_acked,job_handoff;
static u32 job_id,job_crc;
static u16 job_exit;
static char jobhome[67],rootabs[67];
static u8 job_record[200];
static int dopenrw(const char*);
#pragma aux dopenrw = "mov ax,3d02h" "int 21h" "jnc short L" "mov ax,-1" "L:" parm [dx] value [ax] modify [ax];
static int dseekend(int);
#pragma aux dseekend = "mov ax,4202h" "xor cx,cx" "xor dx,dx" "int 21h" "sbb ax,ax" parm [bx] value [ax] modify [ax cx dx];
static u16 dpsp(void);
#pragma aux dpsp = "mov ah,62h" "int 21h" "mov ax,bx" value [ax] modify [ax bx];
static u16 dpspsize(void);
#pragma aux dpspsize = "push es" "mov ah,62h" "int 21h" "dec bx" "mov es,bx" "mov ax,es:[3]" "pop es" value [ax] modify [ax bx];
static int log_open(void){int f=dopenrw("AGENT.LOG");if(f<0)return dcreate("AGENT.LOG");if(dseekend(f)<0){dclose(f);return -1;}return f;}
static char upper(char c){return c>='a'&&c<='z'?c-32:c;}
static int prefixi(const char*s,const char*p){while(*p)if(upper(*s++)!=upper(*p++))return 0;return 1;}
static void homepath(char*out,const char*leaf){unsigned n;textcopy(out,jobhome);n=slen(out);if(n&&out[n-1]!='\\')out[n++]='\\';textcopy(out+n,leaf);}
static int control_exists(const char*leaf){char p[80];homepath(p,leaf);return exists(p);}
static int control_delete(const char*leaf){char p[80];homepath(p,leaf);if(!exists(p))return 1;return ddelete(p)==0;}
static int writebytes(int f,const void*p,unsigned n){return dwrite(f,p,n)==n;}
static int writeline(int f,const char*s){return writebytes(f,s,slen(s))&&writebytes(f,"\r\n",2);}
static void hexjob(char*out,u32 id){u8 b[4];int i;const char*h="0123456789ABCDEF";set32(b,id);for(i=3;i>=0;i--){*out++=h[b[i]>>4];*out++=h[b[i]&15];}*out=0;}
static void numtext(char*out,unsigned n){char b[6];unsigned i=0,j=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)out[j++]=b[--i];out[j]=0;}
static int job_home(void){unsigned n;jobhome[0]=(char)('A'+originaldrive);jobhome[1]=':';textcopy(jobhome+2,original);n=slen(jobhome);while(n>3&&jobhome[n-1]=='\\')jobhome[--n]=0;return n<64;}
static int job_start(void){char p[80],line[40],hex[9];int f,n;u16 code;u8 ps[8];unsigned i;
 rootabs[0]=(char)('A'+dgetdrive());rootabs[1]=':';rootabs[2]='\\';if(dgetdir(rootabs+3)<0)return 0;
 copy(ps,"PSP3",4);set16(ps+4,dpsp());set16(ps+6,dpspsize());homepath(p,"AUPSP.DAT");f=dcreate(p);if(f<0)return 0;n=writebytes(f,ps,8);if(dclose(f)<0)n=0;if(!n)return 0;
 homepath(p,"AUJOB.DAT");f=dopen(p);if(f<0)return 1;n=dread(f,job_record,200);dclose(f);if(n!=200||!prefixi((char*)job_record,"AUJ3"))return 0;
 if(job_record[76]<1||job_record[76]>120||get32(job_record+8)!=(crcstep(0xffffffffUL,job_record+77,job_record[76])^0xffffffffUL))return 0;
 job_id=get32(job_record+4);job_crc=get32(job_record+8);hexjob(hex,job_id);
 homepath(p,"AUDONE.TXT");f=dopen(p);if(f<0){logline("JOB has no supervisor completion record");return 0;}n=dread(f,line,39);dclose(f);if(n<11)return 0;line[n]=0;
 for(i=0;i<8;i++)if(line[i]!=hex[i])return 0;if(line[8]!=' ')return 0;i=9;while(line[i]>='0'&&line[i]<='9')i++;line[i]=0;if(!number(line+9,&code,0)||code>255)return 0;
 job_exit=code;resume_pending=1;screenpart("AUTOUAT: Command completed with exit code ");screenvalue(job_exit);screenpart("; reconnecting...\r\n");logline("JOB completion found");logline(hex);return 1;}
static int command_line(char*out,const char*command){unsigned at=0;while(command[at]&&command[at]!=' ')at++;
#ifdef LITELINK
 if((at==6&&prefixi(command,"LLTIME"))||(at==10&&prefixi(command,"LLTIME.EXE"))){homepath(out,"LLTIME.EXE");textcopy(out+slen(out),command+at);}else if((at==5&&prefixi(command,"LLGET"))||(at==9&&prefixi(command,"LLGET.EXE"))){homepath(out,"LLGET.EXE");textcopy(out+slen(out),command+at);}else
#endif
 if(at>=4&&prefixi(command+at-4,".BAT")){textcopy(out,"CALL ");textcopy(out+5,command);}else textcopy(out,command);return slen(out)<=126;}
static int make_batch(const char*command,const char*cwd){char path[80],line[160],id[9],num[6],done[80],trace[80],toolcmd[190];int f,i,ok=1;hexjob(id,job_id);homepath(path,"AUJOB.NEW");homepath(done,"AUDONE.TXT");homepath(trace,"SUPERV.LOG");f=dcreate(path);if(f<0)return 0;
 #define LINE(s) if(ok&&!writeline(f,s))ok=0
 LINE("@echo off");textcopy(line,"echo JOB ");textcopy(line+slen(line),id);textcopy(line+slen(line)," AGENT-RETURNED>>");textcopy(line+slen(line),trace);LINE(line);
 line[0]=cwd[0];line[1]=':';line[2]=0;LINE(line);textcopy(line,"cd ");textcopy(line+3,cwd);LINE(line);
 if(jobmem){homepath(line,"AUMON.COM");textcopy(line+slen(line)," ");homepath(path,"MEMPRE.BIN");textcopy(line+slen(line),path);LINE(line);}

 if(!command_line(toolcmd,command))ok=0;LINE(toolcmd);

 for(i=255;i>0;i--){numtext(num,i);textcopy(line,"if errorlevel ");textcopy(line+slen(line),num);textcopy(line+slen(line)," goto AUR");textcopy(line+slen(line),num);LINE(line);}LINE("goto AUR0");
 for(i=0;i<256;i++){numtext(num,i);textcopy(line,":AUR");textcopy(line+4,num);LINE(line);textcopy(line,"echo ");textcopy(line+5,id);textcopy(line+slen(line)," ");textcopy(line+slen(line),num);textcopy(line+slen(line),">");textcopy(line+slen(line),done);LINE(line);LINE("goto AUFINISH");}
 LINE(":AUFINISH");textcopy(line,"echo JOB ");textcopy(line+slen(line),id);textcopy(line+slen(line)," COMMAND-RETURNED>>");textcopy(line+slen(line),trace);LINE(line);
 if(jobmem){homepath(line,"AUMON.COM");textcopy(line+slen(line)," ");homepath(path,"MEMPOST.BIN");textcopy(line+slen(line),path);LINE(line);}
 line[0]=jobhome[0];line[1]=':';line[2]=0;LINE(line);textcopy(line,"cd ");textcopy(line+3,jobhome);LINE(line);
 #undef LINE
 if(dclose(f)<0)ok=0;homepath(path,"AUJOB.NEW");if(ok){char final[80];homepath(final,"AUJOB.BAT");if(drename(path,final)<0)ok=0;}if(!ok)control_delete("AUJOB.NEW");return ok;}
/* On success the batch and record are durable before the acceptance is sent.
 * The caller must hand off by terminating, never by EXEC from this process. */
static int accept_job(const char*command,unsigned n,u32 id){u8 response[12];char path[80],cwd[67],checked[190];int f,err=0;unsigned i;zero(response,12);set32(response,id);
 if(!allowrun)err=1;else if(!dos_supervised())err=5;
#ifdef LITELINK
 else if(!control_exists("LLSTART.BAT"))err=5;
#else
 else if(!control_exists("AUSTART.BAT"))err=5;
#endif
else if(slen(jobhome)>45||(jobmem&&slen(jobhome)>25))err=2;else if(control_exists("AUJOB.DAT")||control_exists("AUJOB.BAT"))err=4;else if(n<1||n>120)err=2;
 for(i=0;i<n;i++)if((u8)command[i]<32||(u8)command[i]>126)err=2;if(!err&&!command_line(checked,command))err=2;
 if(!err){job_id=id;job_crc=crcstep(0xffffffffUL,(const u8*)command,n)^0xffffffffUL;cwd[0]='A'+dgetdrive();cwd[1]=':';cwd[2]='\\';if(dgetdir(cwd+3)<0)err=3;
 if(!err){zero(job_record,200);copy(job_record,"AUJ3",4);set32(job_record+4,id);set32(job_record+8,job_crc);textcopy((char*)job_record+12,cwd);job_record[76]=(u8)n;copy(job_record+77,command,n);
 if(!make_batch(command,cwd))err=3;else{homepath(path,"AUJOB.DAT");f=dcreate(path);if(f<0)err=3;else{if(!writebytes(f,job_record,200))err=3;if(dclose(f)<0)err=3;}}}
 if(err){control_delete("AUJOB.BAT");control_delete("AUJOB.DAT");}}
 set16(response+4,err);set16(response+6,err?0:1);set32(response+8,err?0:job_crc);if(!frameout(JOB_ACCEPT,response,12))return 0;
 if(!err){ui_save();job_handoff=1;screenline("Command accepted; unloading agent completely for DOS execution.");screenline(command);logline("JOB ACCEPTED: batch committed; exiting completely via DOS 4C");logline(command);}else screenerror("Command rejected",err);return 1;}
static int jobrequest(int kind){char cmd[121];u32 id;unsigned n,pn,an;if(kind==JOB_SUBMIT){if(plen<5||plen>124)return 0;id=get32(payload);n=plen-4;copy(cmd,payload+4,n);cmd[n]=0;return accept_job(cmd,n,id);}
 /* Legacy Run/Cmd framing is rejected explicitly; the 0.0.3 host uses jobs. */
 {u8 r[EXEC_BYTES];zero(r,sizeof(r));set16(r,XR_INVALID);r[4]=r[5]=255;return frameout(EXEC_RESULT,r,sizeof(r));}}
/* Report saved completion; do not resubmit or replay the command. */
static int resume_job(void){u8 r[12];set32(r,job_id);set16(r+4,job_exit);set16(r+6,3);set32(r+8,job_crc);if(!frameout(JOB_RESUME,r,12)||framein()!=JOB_RESUME_ACK||plen!=4||get32(payload)!=job_id)return 0;resume_acked=1;set32(r,job_id);logline("JOB reconnect associated with original id");screenline("Command completion acknowledged; ready for host requests.");return frameout(JOB_READY,r,4);}
static int clear_job(void){if(!control_delete("AUJOB.DAT")||!control_delete("AUDONE.TXT"))return 0;resume_pending=resume_acked=0;return 1;}
static int get_request(void){char source[64],leaf[13],dir[64],back[67];unsigned i,last=0,rn=slen(rootabs);int ok;
 if(plen<2||plen>64||payload[plen-1])return 0;copy(source,payload,plen);if(valid(source))return sendfile(source);
 if(slen(source)<4||source[1]!=':'||source[2]!='\\'||!prefixi(source,rootabs))return statusout(RESULT,INVALID);
 if(rootabs[rn-1]!='\\'&&source[rn]!='\\')return statusout(RESULT,INVALID);
 i=3;while(source[i]){unsigned j=0;while(source[i]&&source[i]!='\\'){if(j==12)return statusout(RESULT,INVALID);leaf[j++]=source[i++];}leaf[j]=0;if(!valid(leaf)&&!(source[i]=='\\'&&prefixi(leaf,"AUTOUAT")&&slen(leaf)==7))return statusout(RESULT,INVALID);if(source[i]=='\\'){last=i;i++;if(!source[i])return statusout(RESULT,INVALID);}}
 if(!last){copy(dir,source,3);dir[3]=0;}else{copy(dir,source,last);dir[last]=0;}back[0]='A'+dgetdrive();back[1]=':';back[2]='\\';if(dgetdir(back+3)<0)return 0;if(dchdir(dir)<0)return statusout(RESULT,MISSING);ok=sendfile(leaf);if(dchdir(back)<0)return 0;return ok;}
