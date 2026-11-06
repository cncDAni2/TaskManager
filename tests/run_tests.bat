@echo off
setlocal

cd /d "%~dp0.."

set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist dist\test mkdir dist\test

cl.exe /nologo /MP /EHsc /W3 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE /Isrc /Fodist\test\ /Fedist\test\test_core.exe ^
    tests\test_core.cpp src\Task.cpp src\TaskLocalFile.cpp src\TaskSync.cpp src\TaskUndo.cpp src\TaskRecurring.cpp ^
    src\SyncFiles.cpp src\JsonLite.cpp src\TaskUtils.cpp src\Recurring.cpp src\VersionHint.cpp src\WorkHistory.cpp ^
    src\ExcludedApps.cpp ^
    /link user32.lib secur32.lib advapi32.lib rpcrt4.lib
if %ERRORLEVEL% NEQ 0 (
    echo Test build failed.
    exit /b %ERRORLEVEL%
)

dist\test\test_core.exe
exit /b %ERRORLEVEL%
