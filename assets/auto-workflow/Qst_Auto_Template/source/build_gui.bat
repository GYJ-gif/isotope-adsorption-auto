@echo off
setlocal EnableExtensions EnableDelayedExpansion
if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)
cd /d "%~dp0"
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    cl /O2 /utf-8 /EHsc /std:c++17 /wd4819 /Fe:qst_calc_gui.exe qst_gui.cpp /link gdiplus.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib
    if !ERRORLEVEL! NEQ 0 goto build_failed
    cl /O2 /utf-8 /EHsc /std:c++17 /wd4819 /Fe:qst_calc_cli.exe qst_cli.cpp
) else (
    set "GPP=%~dp0..\..\.tools\winlibs-gcc-16.1.0-ucrt-r3\mingw64\bin\g++.exe"
    set "TEMP=%~dp0..\..\.tmp\gcc"
    set "TMP=%~dp0..\..\.tmp\gcc"
    if not exist "!TEMP!" mkdir "!TEMP!"
    if not exist "!GPP!" (
        echo C++ compiler not found: !GPP!
        exit /b 1
    )
    "!GPP!" -O2 -std=c++17 -static -static-libgcc -static-libstdc++ -mwindows -municode -o qst_calc_gui.exe qst_gui.cpp -lgdiplus -lgdi32 -luser32 -lcomctl32 -lcomdlg32
    if !ERRORLEVEL! NEQ 0 goto build_failed
    "!GPP!" -O2 -std=c++17 -static -static-libgcc -static-libstdc++ -o qst_calc_cli.exe qst_cli.cpp
)
if %ERRORLEVEL% EQU 0 (
    echo Build OK: qst_calc_gui.exe
    copy /Y qst_calc_gui.exe ..\qst_calc_gui.exe >nul
    copy /Y qst_calc_cli.exe ..\qst_calc_cli.exe >nul
    echo Deployed to ..\qst_calc_gui.exe
    echo Deployed to ..\qst_calc_cli.exe
) else (
    goto build_failed
)
exit /b 0

:build_failed
echo BUILD FAILED
exit /b 1
