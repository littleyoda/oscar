@echo off
setlocal EnableDelayedExpansion
set DIR=%~dp0
cd %DIR%

for /f %%i in ('git rev-parse --git-dir') do set GIT_DIR=%%i
if "%GIT_DIR%"=="" goto GitDone

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
if "%GIT_BRANCH%"=="HEAD" set GIT_BRANCH=""
for /f %%i in ('git rev-parse --short HEAD') do set GIT_REVISION=%%i
for /f %%i in ('git diff-index --quiet HEAD --') do set GIT_INDEX=%%i
if "%GIT_INDEX%"=="" (
  set GIT_REVISION=%GIT_REVISION%-plus
  set GIT_TAG=
  goto GitDone
)

for /f %%i in ('git describe --exact-match --tags') do set GIT_TAG=%%i

:GitDone

@echo Update_gtinfo.bat: GIT_DIR=%GIT_DIR%, GIT_BRANCH=%GIT_BRANCH%, GIT_REVISION=%GIT_REVISION%, GIT_TAG=%GIT_TAG%

echo // This is an auto generated file > %DIR%git_info.new

if "%GIT_BRANCH%"=="" goto DoRevision
  echo #define GIT_BRANCH "%GIT_BRANCH%" >> %DIR%git_info.new
:DoRevision
if "%GIT_REVISION%"=="" goto DoTag
  echo #define GIT_REVISION "%GIT_REVISION%" >> %DIR%git_info.new
:DoTag
if "%GIT_TAG%"=="" goto DefinesDone
  echo #define GIT_TAG "%GIT_TAG%" >> %DIR%git_info.new
:DefinesDone

:::type %DIR%git_info.new
:::set

fc %DIR%git_info.h %DIR%git_info.new 1>nul 2>nul && del /q %DIR%git_info.new || goto NewFile
goto AllDone

:NewFile
echo Updating %DIR%git_info.h
move /y %DIR%git_info.new %DIR%git_info.h

:AllDone
