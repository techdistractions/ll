@echo off
rem LiteLink log collector 0.0.1 - DOS 3.3+ COMMAND.COM, no external shell.
rem Run from the LiteLink installation directory; no INI or driver changes.
if "%1"=="" goto USAGE
if "%2"=="" goto USAGE
if not exist LL.INI goto MISSING
if not exist LLNIC.EXE goto MISSING
if not exist NETDIAG.EXE goto MISSING
:TRY01
set LLDBG=LLDB01
if exist LLDB01\NUL goto TRY02
if exist LLDB01 goto TRY02
goto MAKE
:TRY02
set LLDBG=LLDB02
if exist LLDB02\NUL goto TRY03
if exist LLDB02 goto TRY03
goto MAKE
:TRY03
set LLDBG=LLDB03
if exist LLDB03\NUL goto TRY04
if exist LLDB03 goto TRY04
goto MAKE
:TRY04
set LLDBG=LLDB04
if exist LLDB04\NUL goto TRY05
if exist LLDB04 goto TRY05
goto MAKE
:TRY05
set LLDBG=LLDB05
if exist LLDB05\NUL goto TRY06
if exist LLDB05 goto TRY06
goto MAKE
:TRY06
set LLDBG=LLDB06
if exist LLDB06\NUL goto TRY07
if exist LLDB06 goto TRY07
goto MAKE
:TRY07
set LLDBG=LLDB07
if exist LLDB07\NUL goto TRY08
if exist LLDB07 goto TRY08
goto MAKE
:TRY08
set LLDBG=LLDB08
if exist LLDB08\NUL goto TRY09
if exist LLDB08 goto TRY09
goto MAKE
:TRY09
set LLDBG=LLDB09
if exist LLDB09\NUL goto TRY10
if exist LLDB09 goto TRY10
goto MAKE
:TRY10
set LLDBG=LLDB10
if exist LLDB10\NUL goto TRY11
if exist LLDB10 goto TRY11
goto MAKE
:TRY11
set LLDBG=LLDB11
if exist LLDB11\NUL goto TRY12
if exist LLDB11 goto TRY12
goto MAKE
:TRY12
set LLDBG=LLDB12
if exist LLDB12\NUL goto TRY13
if exist LLDB12 goto TRY13
goto MAKE
:TRY13
set LLDBG=LLDB13
if exist LLDB13\NUL goto TRY14
if exist LLDB13 goto TRY14
goto MAKE
:TRY14
set LLDBG=LLDB14
if exist LLDB14\NUL goto TRY15
if exist LLDB14 goto TRY15
goto MAKE
:TRY15
set LLDBG=LLDB15
if exist LLDB15\NUL goto TRY16
if exist LLDB15 goto TRY16
goto MAKE
:TRY16
set LLDBG=LLDB16
if exist LLDB16\NUL goto TRY17
if exist LLDB16 goto TRY17
goto MAKE
:TRY17
set LLDBG=LLDB17
if exist LLDB17\NUL goto TRY18
if exist LLDB17 goto TRY18
goto MAKE
:TRY18
set LLDBG=LLDB18
if exist LLDB18\NUL goto TRY19
if exist LLDB18 goto TRY19
goto MAKE
:TRY19
set LLDBG=LLDB19
if exist LLDB19\NUL goto TRY20
if exist LLDB19 goto TRY20
goto MAKE
:TRY20
set LLDBG=LLDB20
if exist LLDB20\NUL goto FULL
if exist LLDB20 goto FULL
goto MAKE
:MAKE
md %LLDBG%
if not exist %LLDBG%\NUL goto DIRFAIL
cd %LLDBG%
echo LiteLink debug collection 0.0.1 > LLDEBUG.TXT
echo Target IP: %1 >> LLDEBUG.TXT
echo Target TCP port: %2 >> LLDEBUG.TXT
echo Collection directory: >> LLDEBUG.TXT
cd >> LLDEBUG.TXT
ver >> LLDEBUG.TXT
echo. >> LLDEBUG.TXT
echo ===== MASTER LL.INI - unchanged ===== >> LLDEBUG.TXT
type ..\LL.INI >> LLDEBUG.TXT
echo. >> LLDEBUG.TXT
echo ===== INSTALLED FILE SIZES / DATES ===== >> LLDEBUG.TXT
dir ..\LL*.EXE >> LLDEBUG.TXT
dir ..\NETDIAG.EXE >> LLDEBUG.TXT
dir ..\LLSTART.BAT >> LLDEBUG.TXT
echo. >> LLDEBUG.TXT
echo ===== PREVIOUS AGENT.LOG - before this collection ===== >> LLDEBUG.TXT
if exist ..\AGENT.LOG type ..\AGENT.LOG >> LLDEBUG.TXT
if not exist ..\AGENT.LOG echo No previous AGENT.LOG found. >> LLDEBUG.TXT
echo. >> LLDEBUG.TXT
echo ===== PREVIOUS SUPERV.LOG ===== >> LLDEBUG.TXT
if exist ..\SUPERV.LOG type ..\SUPERV.LOG >> LLDEBUG.TXT
if not exist ..\SUPERV.LOG echo No previous SUPERV.LOG found. >> LLDEBUG.TXT
echo. >> LLDEBUG.TXT
echo Collecting diagnostics in %LLDBG%.
echo Tests continue after failures. Network timeouts may take several minutes.
echo [1/8] MAC before any LLNIC probe
echo ===== STEP 1: MAC before any LLNIC probe ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE MAC >> LLDEBUG.TXT
..\NETDIAG.EXE MAC
if errorlevel 1 goto MAC1BAD
echo Program exit status: zero >> LLDEBUG.TXT
goto MAC1LOG
:MAC1BAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:MAC1LOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG MAC1.LOG
echo. >> LLDEBUG.TXT
echo [2/8] Hardware initialization and close
echo ===== STEP 2: Hardware initialization and close ===== >> LLDEBUG.TXT
echo Command: LLNIC.EXE >> LLDEBUG.TXT
..\LLNIC.EXE
if errorlevel 1 goto NICBAD
echo Program exit status: zero >> LLDEBUG.TXT
goto NICLOG
:NICBAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:NICLOG
if exist LLNIC.LOG type LLNIC.LOG >> LLDEBUG.TXT
if not exist LLNIC.LOG echo Diagnostic did not produce LLNIC.LOG. >> LLDEBUG.TXT
if exist LLNIC.LOG ren LLNIC.LOG NIC.LOG
echo. >> LLDEBUG.TXT
echo [3/8] MAC after LLNIC closed
echo ===== STEP 3: MAC after LLNIC closed ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE MAC >> LLDEBUG.TXT
..\NETDIAG.EXE MAC
if errorlevel 1 goto MAC2BAD
echo Program exit status: zero >> LLDEBUG.TXT
goto MAC2LOG
:MAC2BAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:MAC2LOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG MAC2.LOG
echo. >> LLDEBUG.TXT
echo [4/8] Internal NIC loopback - NE2000 only
echo ===== STEP 4: Internal NIC loopback - NE2000 only ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE LOOP >> LLDEBUG.TXT
..\NETDIAG.EXE LOOP
if errorlevel 1 goto LOOPBAD
echo Program exit status: zero >> LLDEBUG.TXT
goto LOOPLOG
:LOOPBAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:LOOPLOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG LOOP.LOG
echo. >> LLDEBUG.TXT
echo [5/8] First ARP request
echo ===== STEP 5: First ARP request ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE ARP %1 >> LLDEBUG.TXT
..\NETDIAG.EXE ARP %1
if errorlevel 1 goto ARP1BAD
echo Program exit status: zero >> LLDEBUG.TXT
goto ARP1LOG
:ARP1BAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:ARP1LOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG ARP1.LOG
echo. >> LLDEBUG.TXT
echo [6/8] Repeated ARP after close and reopen
echo ===== STEP 6: Repeated ARP after close and reopen ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE ARP %1 >> LLDEBUG.TXT
..\NETDIAG.EXE ARP %1
if errorlevel 1 goto ARP2BAD
echo Program exit status: zero >> LLDEBUG.TXT
goto ARP2LOG
:ARP2BAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:ARP2LOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG ARP2.LOG
echo. >> LLDEBUG.TXT
echo [7/8] ICMP echo
echo ===== STEP 7: ICMP echo ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE PING %1 >> LLDEBUG.TXT
..\NETDIAG.EXE PING %1
if errorlevel 1 goto PINGBAD
echo Program exit status: zero >> LLDEBUG.TXT
goto PINGLOG
:PINGBAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:PINGLOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG PING.LOG
echo. >> LLDEBUG.TXT
echo [8/8] Native TCP listener connection
echo ===== STEP 8: Native TCP listener connection ===== >> LLDEBUG.TXT
echo Command: NETDIAG.EXE TCP %1 %2 >> LLDEBUG.TXT
..\NETDIAG.EXE TCP %1 %2
if errorlevel 1 goto TCPBAD
echo Program exit status: zero >> LLDEBUG.TXT
goto TCPLOG
:TCPBAD
echo Program exit status: nonzero - inspect diagnostic lines below >> LLDEBUG.TXT
:TCPLOG
if exist NETDIAG.LOG type NETDIAG.LOG >> LLDEBUG.TXT
if not exist NETDIAG.LOG echo Diagnostic did not produce NETDIAG.LOG. >> LLDEBUG.TXT
if exist NETDIAG.LOG ren NETDIAG.LOG TCP.LOG
echo. >> LLDEBUG.TXT
echo ===== COLLECTION FINISHED ===== >> LLDEBUG.TXT
echo This marks completed log collection, not a networking PASS. >> LLDEBUG.TXT
echo Individual failures remain recorded above. No LL.INI settings changed. >> LLDEBUG.TXT
copy LLDEBUG.TXT ..\LLDEBUG.TXT > NUL
if errorlevel 1 goto COPYFAIL
cd ..
echo.
echo Finished. Send LLDEBUG.TXT from this directory.
echo A preserved copy and separate raw logs are in %LLDBG%.
goto CLEAN
:COPYFAIL
cd ..
echo Could not make the convenience copy. Send %LLDBG%\LLDEBUG.TXT.
goto CLEAN
:DIRFAIL
echo Cannot create the fresh log directory. No diagnostics were run.
goto CLEAN
:FULL
echo LLDB01 through LLDB20 already exist. Preserve or move old logs first.
goto CLEAN
:MISSING
echo Run this from the directory containing LL.INI, LLNIC.EXE and NETDIAG.EXE.
echo Example: CD \LITELINK
goto END
:USAGE
echo Usage: LLDEBUG target-IP target-TCP-port
echo Example: LLDEBUG 192.168.0.10 21210
echo Run from the LiteLink directory with LLShell stopped.
echo Keep the Windows LLShell32 listener running for the TCP test.
goto END
:CLEAN
set LLDBG=
:END
