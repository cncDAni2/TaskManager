@echo off
setlocal

cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
set "VSINSTALL="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)
if not defined VSINSTALL (
    echo Visual Studio with the C++ x64 toolset was not found.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if %ERRORLEVEL% NEQ 0 (
    echo Failed to initialize the MSVC x64 environment.
    exit /b %ERRORLEVEL%
)

if not exist dist\obj mkdir dist\obj

echo Compiling resources...
rc.exe /nologo /fo dist\TaskManager.res TaskManager.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo Compiling TaskManager...
cl.exe /nologo /O2 /MT /EHsc /W4 /utf-8 /std:c++17 /DUNICODE /D_UNICODE /Isrc /Fodist\obj\ src\*.cpp dist\TaskManager.res /link /SUBSYSTEM:WINDOWS /OUT:TaskManager.exe user32.lib gdi32.lib shell32.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib secur32.lib rpcrt4.lib comdlg32.lib winmm.lib delayimp.lib /DELAYLOAD:comdlg32.dll /DELAYLOAD:winmm.dll
if %ERRORLEVEL% NEQ 0 (
    echo C++ compilation failed.
    exit /b %ERRORLEVEL%
)

echo Build succeeded!
exit /b 0
