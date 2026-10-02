@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist dist\obj mkdir dist\obj

echo Compiling resources...
rc.exe /nologo /fo dist\TaskManager.res TaskManager.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo Compiling TaskManager...
cl.exe /nologo /O2 /MT /EHsc /W4 /utf-8 /std:c++17 /DUNICODE /D_UNICODE /Isrc /Fodist\obj\ src\*.cpp dist\TaskManager.res /link /SUBSYSTEM:WINDOWS /OUT:TaskManager.exe user32.lib gdi32.lib shell32.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib secur32.lib comdlg32.lib winmm.lib
if %ERRORLEVEL% NEQ 0 (
    echo C++ compilation failed.
    exit /b %ERRORLEVEL%
)

echo Build succeeded!
exit /b 0
