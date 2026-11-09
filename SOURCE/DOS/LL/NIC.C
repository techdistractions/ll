/* Hardware-only native probe. No socket/ARP operation, resident memory or IRQ. */
#include "COMMON.C"
extern int __cdecl ll_nic_open(void);extern void __cdecl ll_nic_close(void);
int __cdecl ll_main(void){int result;lf=logopen("LLNIC.LOG");if(lf<0)lf=create("LLNIC.LOG");if(lf>=0)seekend(lf);
 say("LiteLink LLNIC 0.0.11r1 - hardware initialization probe");args();if(ac||!config()){say("LLNIC FAIL: use LLNIC with a valid LL.INI; no hardware operation.");if(lf>=0)cl(lf);return 1;}
 setup();result=ll_nic_open();val("NIC open result=",result);nic_report();ll_nic_close();say("NIC closed; probe process now exits completely.");say(result?"LLNIC FAIL":"LLNIC hardware initialization PASS; network reachability not tested.");if(lf>=0)cl(lf);return result?1:0;}
