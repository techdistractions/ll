@echo off
setlocal EnableExtensions
for %%P in ("%~dp0..\..") do set "REPO=%%~fP"
set "DIST=%REPO%\RELEASES\0.0.13"
set "REPO_NAME=LiteLink"
if not "%~1"=="" set "REPO_NAME=%~1"
echo.
echo LiteLink 0.0.13 - GitHub publication
 echo ====================================
if not exist "%DIST%\READY-TO-PUBLISH.flag" (
    echo HOLD: Documentation and release archives are being reviewed.
    echo The final package must be rebuilt and checked before publication.
    echo Do not create the flag until those checks are complete.
    goto :failed
)
where git.exe >NUL 2>&1
if errorlevel 1 (echo ERROR: Git for Windows missing. & goto :failed)
where gh.exe >NUL 2>&1
if errorlevel 1 (echo ERROR: GitHub CLI missing. https://cli.github.com/ & goto :failed)
if not exist "%REPO%\.git" (echo ERROR: Git repository missing. & goto :failed)
for %%F in (LITELINK-0.0.13-BINARY.ZIP LITELINK-0.0.13-SOURCE.ZIP RELEASE-NOTES.md SHA256SUMS.TXT) do (
    if not exist "%DIST%\%%F" (echo ERROR: Missing %%F & goto :failed)
)
git -C "%REPO%" show-ref --verify --quiet refs/heads/main
if errorlevel 1 (echo ERROR: Branch main missing. & goto :failed)
git -C "%REPO%" rev-parse --verify refs/tags/v0.0.13 >NUL 2>&1
if errorlevel 1 (echo ERROR: Local release tag missing. & goto :failed)
for /f "delims=" %%G in ('git -C "%REPO%" status --porcelain') do (
    echo ERROR: Uncommitted changes found. Review and commit them first.
    goto :failed
)
for /f "delims=" %%G in ('git -C "%REPO%" rev-parse HEAD') do set "HEADSHA=%%G"
for /f "delims=" %%G in ('git -C "%REPO%" rev-list -n 1 v0.0.13') do set "TAGSHA=%%G"
if /I not "%HEADSHA%"=="%TAGSHA%" (
    echo ERROR: v0.0.13 tag is not at HEAD.
    echo Retag only after docs and assets are synchronized.
    goto :failed
)
gh auth status >NUL 2>&1
if errorlevel 1 (echo ERROR: Run gh auth login first. & goto :failed)
git -C "%REPO%" remote get-url origin >NUL 2>&1
if errorlevel 1 (
    echo.
    echo Create PUBLIC GitHub repo "%REPO_NAME%" and push source?
    choice /C YN /M "Continue"
    if errorlevel 2 goto :cancel
    gh repo create "%REPO_NAME%" --public --source "%REPO%" --remote origin --push --description "LiteLink: networking tools for DOS and Windows"
    if errorlevel 1 goto :failed
) else (
    echo.
    git -C "%REPO%" remote get-url origin
    choice /C YN /M "Push main to the existing origin"
    if errorlevel 2 goto :cancel
    git -C "%REPO%" push -u origin main
    if errorlevel 1 goto :failed
)
git -C "%REPO%" push origin v0.0.13
if errorlevel 1 goto :failed
pushd "%REPO%"
if errorlevel 1 goto :failed
gh release view v0.0.13 >NUL 2>&1
if not errorlevel 1 (
    echo Release already exists. No archive replacement attempted.
    popd
    goto :success
)
gh release create v0.0.13 "%DIST%\LITELINK-0.0.13-BINARY.ZIP" "%DIST%\LITELINK-0.0.13-SOURCE.ZIP" "%DIST%\SHA256SUMS.TXT" --verify-tag --title "LiteLink 0.0.13" --notes-file "%DIST%\RELEASE-NOTES.md"
if errorlevel 1 (popd & goto :failed)
popd
:success
 echo SUCCESS: Review GitHub repository and Release v0.0.13.
goto :end
:cancel
 echo Cancelled.
goto :end
:failed
 echo Publication did not complete.
:end
 echo.
pause
