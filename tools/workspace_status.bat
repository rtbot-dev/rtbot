@echo off
setlocal enabledelayedexpansion

:: Read version from package.json (simple grep approach)
set "VERSION=0.1.0"
for /f "tokens=2 delims=:, " %%a in ('findstr /C:"\"version\"" package.json') do (
    set "VERSION=%%~a"
)

set "GIT_COMMIT=nogit"

:: Check if we're in a git repo
git rev-parse --is-inside-work-tree >nul 2>&1
if %errorlevel% equ 0 (
    for /f "delims=" %%c in ('git rev-parse --short HEAD 2^>nul') do set "GIT_COMMIT=%%c"

    :: Check for exact tag
    for /f "delims=" %%t in ('git describe --tags --exact-match HEAD 2^>nul') do (
        set "RAW_TAG=%%t"
        :: Strip leading v
        set "TAG_VER=!RAW_TAG!"
        if "!TAG_VER:~0,1!"=="v" set "TAG_VER=!TAG_VER:~1!"
        :: Replace hyphens with dots
        set "TAG_VER=!TAG_VER:-=.!"
        set "VERSION=!TAG_VER!"
    )
)

echo RTBOT_VERSION %VERSION%
echo STABLE_GIT_COMMIT %GIT_COMMIT%
echo BUILD_SCM_REVISION %GIT_COMMIT%
