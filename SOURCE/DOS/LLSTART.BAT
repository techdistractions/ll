@echo off
SET AUSUP=1
if exist AUMON.COM AUMON.COM MEMBASE.BIN
:AUAGAIN
LLSHELL.EXE
if errorlevel 1 goto AUFAIL
if not exist AUJOB.BAT goto AUEND
call AUJOB.BAT
del AUJOB.BAT
goto AUAGAIN
:AUFAIL
echo LiteLink supervisor stopped after agent failure.
:AUEND
SET AUSUP=
