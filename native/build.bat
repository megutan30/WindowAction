@echo off
setlocal

rem 第1引数に"dev"を指定すると、タイトル画面の「Test」ボタンから入れる
rem ステージエディター(自由配置のテストステージ)を含む開発ビルドになる。
rem 引数なし(通常のbuild.bat)は今まで通りの本番ビルドで、エディター関連の
rem コードは#ifdef ENABLE_STAGE_EDITORによりコンパイル自体されない。
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

cl.exe /nologo /W3 /O1 /GL /MT /std:c17 /DWIN32_LEAN_AND_MEAN %DEVDEFINE% ^
    src\main.c src\gamewindow.c src\player.c src\zorder.c src\noentry.c ^
    src\collision.c src\hierarchy.c src\strategy.c src\stage.c src\windowquery.c ^
    src\animation.c src\gamefont.c src\editor.c ^
    /Fo:build\ /Fe:build\WindowAction.exe ^
    /link /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /SUBSYSTEM:WINDOWS ^
    build\resource.res ^
    user32.lib gdi32.lib kernel32.lib

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo Build succeeded (%BUILD_LABEL%): build\WindowAction.exe
for %%F in (build\WindowAction.exe) do echo Size: %%~zF bytes

endlocal
