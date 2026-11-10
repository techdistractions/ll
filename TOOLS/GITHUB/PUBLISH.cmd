@echo off
rem Publication only AFTER package approval and creation of a new approved tag.
rem This helper never creates, moves, deletes or force-pushes a tag.
setlocal EnableExtensions
for %%P in ("%~dp0..\..") do set "REPO=%%~fP"
set /p VERSION=<"%REPO%\VERSION.TXT"
if not defined VERSION (echo Missing VERSION.TXT. & exit /b 1)
set "DIST=%REPO%\RELEASES\%VERSION%"
if not defined GIT_EXE set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not exist "%GIT_EXE%" (echo Set GIT_EXE to Git for Windows git.exe. & exit /b 1)
if not exist "%DIST%\READY-TO-PUBLISH.flag" (
  echo HOLD: Final package approval has not been recorded.
  exit /b 1
)
for %%F in (LITELINK-%VERSION%-BINARY.ZIP LITELINK-%VERSION%-SOURCE.ZIP RELEASE-NOTES.md SHA256SUMS.TXT) do (
  if not exist "%DIST%\%%F" (echo Missing %%F. & exit /b 1)
)
for /f "delims=" %%G in ('"%GIT_EXE%" -C "%REPO%" status --porcelain') do (
  echo Commit intended changes first.
  exit /b 1
)
for /f "delims=" %%G in ('"%GIT_EXE%" -C "%REPO%" rev-parse HEAD') do set "HEADSHA=%%G"
"%GIT_EXE%" -C "%REPO%" rev-parse --verify "refs/tags/v%VERSION%" >NUL 2>&1
if errorlevel 1 (echo HOLD: An approved version tag does not exist. & exit /b 1)
for /f "delims=" %%G in ('"%GIT_EXE%" -C "%REPO%" rev-list -n 1 "v%VERSION%"') do set "TAGSHA=%%G"
if /I not "%HEADSHA%"=="%TAGSHA%" (echo HOLD: Tag does not match the prepared source commit. & exit /b 1)
where gh.exe >NUL 2>&1
if errorlevel 1 (echo GitHub CLI is required for publication. & exit /b 1)
gh auth status >NUL 2>&1
if errorlevel 1 (echo Authenticate GitHub CLI before publishing. & exit /b 1)
"%GIT_EXE%" -C "%REPO%" push origin main
if errorlevel 1 exit /b 1
"%GIT_EXE%" -C "%REPO%" push origin "v%VERSION%"
if errorlevel 1 exit /b 1
pushd "%REPO%"
gh release view "v%VERSION%" >NUL 2>&1
if not errorlevel 1 (echo Release exists; no assets replaced. & popd & exit /b 0)
gh release create "v%VERSION%" "%DIST%\LITELINK-%VERSION%-BINARY.ZIP" "%DIST%\LITELINK-%VERSION%-SOURCE.ZIP" "%DIST%\SHA256SUMS.TXT" --verify-tag --title "Lite Link %VERSION%" --notes-file "%DIST%\RELEASE-NOTES.md"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
