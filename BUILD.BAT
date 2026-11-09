@echo off
rem From the LiteLink root: BUILD (DOS), BUILD nt64, or BUILD nt.
if "%WATCOM%"=="" goto NOWATCOM
set INCLUDE=%WATCOM%\H;%WATCOM%\H\NT
if "%1"=="nt64" goto NT64
if "%1"=="nt" goto NT
%WATCOM%\binw\wmake.exe -h -s -l BUILD.LOG %1
if errorlevel 1 goto FAIL
goto END
:NT64
%WATCOM%\binnt64\wmake.exe -h -s -l BUILD.LOG HOST=nt64 %2
if errorlevel 1 goto FAIL
goto END
:NT
%WATCOM%\binnt\wmake.exe -h -s -l BUILD.LOG HOST=nt %2
if errorlevel 1 goto FAIL
goto END
:NOWATCOM
echo Set WATCOM first. Example: SET WATCOM=C:\WATCOM
goto END
:FAIL
echo Build failed. Read the first compiler or linker error above.
:END
