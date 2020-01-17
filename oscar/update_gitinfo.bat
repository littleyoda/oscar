@echo off
setlocal
set DIR=%~dp0
cd %DIR%

if ('git rev-parse --git-dir') (
  for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
  if "%GIT_BRANCH"=="HEAD" set GIT_BRANCH=""
  for /f %%i in ('git rev-parse --short HEAD') do set GIT_REVISION=%%i
  if ('git diff-index --quiet HEAD --') (
    set GIT_REVISION=%GIT_REVISION%-plus
  ) else (
    for /f %%i in ('git describe --exact-match --tags') do set GIT_TAG=%%i
  )
)

echo // This is an auto generated file > %DIR%git_info.new
if "%GIT_BRANCH"!=""  echo #define GIT_BRANCH "%GIT_BRANCH%" >> %DIR%git_info.new
if "%GIT_REVISION!="" echo #define GIT_REVISION "%GIT_REVISION%" >> %DIR%git_info.new
if "%GIT_TAG"!=""     echo #define GIT_TAG "%GIT_TAG%" >> %DIR%git_info.new

fc %DIR%git_info.h %DIR%git_info.new >nul 2>nul && del /q %DIR%git_info.new || goto NewFile
goto AllDone

:NewFile
echo Updating %DIR%git_info.h
move /y %DIR%git_info.new %DIR%git_info.h

:AllDone
