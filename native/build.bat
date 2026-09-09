@echo off
setlocal

rem pass "dev" as arg1 for a dev build with the stage editor (ENABLE_STAGE_EDITOR); no arg = release build.
set DEVDEFINE=
set BUILD_LABEL=release
if /i "%~1"=="dev" (
    set DEVDEFINE=/DENABLE_STAGE_EDITOR
    set BUILD_LABEL=dev
)

set ROOT=%~dp0

set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARS% (
    echo vcvarsall.bat not found at %VCVARS%
    exit /b 1
)

call %VCVARS% x64
if errorlevel 1 (
    echo Failed to initialize MSVC environment
    exit /b 1
)

cd /d "%ROOT%"
if not exist build mkdir build

rc.exe /nologo /fo build\resource.res src\resource.rc
if errorlevel 1 (
    echo Resource compile failed
    exit /b 1
)

rem /utf-8: interpret sources as UTF-8 (avoids C4819 warnings and mojibake in generated output)
rem /std:c++17: native/ is now compiled as C++ instead of C (see the plan for the C++ migration)
cl.exe /nologo /utf-8 /W3 /O1 /GL /MT /std:c++17 /DWIN32_LEAN_AND_MEAN %DEVDEFINE% ^
    src\main.cpp src\gamewindow.cpp src\player.cpp src\zorder.cpp src\noentry.cpp ^
    src\collision.cpp src\hierarchy.cpp src\strategy.cpp src\stage.cpp src\windowquery.cpp ^
    src\animation.cpp src\gamefont.cpp src\editor.cpp src\desktopicon.cpp ^
    /Fo:build\ /Fe:build\WindowAction.exe ^
    /link /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /SUBSYSTEM:WINDOWS ^
    build\resource.res ^
    user32.lib gdi32.lib kernel32.lib dwmapi.lib advapi32.lib

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo Build succeeded (%BUILD_LABEL%): build\WindowAction.exe
for %%F in (build\WindowAction.exe) do echo Size: %%~zF bytes

endlocal
