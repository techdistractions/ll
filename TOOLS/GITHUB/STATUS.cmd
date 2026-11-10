@echo off
setlocal EnableExtensions
for %%P in ("%~dp0..\..") do set "REPO=%%~fP"
if not defined GIT_EXE set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not exist "%GIT_EXE%" (echo Set GIT_EXE to Git for Windows git.exe. & exit /b 1)
"%GIT_EXE%" -C "%REPO%" status --short --branch
"%GIT_EXE%" -C "%REPO%" log -1 --oneline
"%GIT_EXE%" -C "%REPO%" tag --list
"%GIT_EXE%" -C "%REPO%" remote -v
