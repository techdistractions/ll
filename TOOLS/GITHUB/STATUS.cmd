@echo off
setlocal EnableExtensions
for %%P in ("%~dp0..\..") do set "REPO=%%~fP"
if not exist "%REPO%\.git" (
  echo No Git repository at "%REPO%".
  goto :end
)
git -C "%REPO%" status --short --branch
 echo.
git -C "%REPO%" log -1 --oneline
 echo.
git -C "%REPO%" tag --list
 echo.
git -C "%REPO%" remote -v
:end
 echo.
pause
