@echo off
setlocal
pushd "%~dp0"
call toolchain.bat
if errorlevel 1 goto fail
where windres >nul 2>nul
if errorlevel 1 (
  echo windres was not found. Install the GCC toolchain's binutils package.
  goto fail
)
if not exist build mkdir build
set "HT_OPT=-O2"
if /I "%~1"=="debug" set "HT_OPT=-O0 -g3"
echo Building GymTrack desktop...
windres -I . gui\app.rc -O coff -o build\app_res.o
if errorlevel 1 goto fail
gcc -std=c11 -Wall -Wextra %HT_OPT% -mwindows -static-libgcc -Ioriginal -Igui gui\gymtrack_gui.c gui\tracker_bridge.c build\app_res.o -o GymTrack.exe -lcomctl32 -lgdi32 -luser32 -lcomdlg32 -ldwmapi -luxtheme -lmsimg32 -ld2d1 -luuid -lwinmm -lm
if errorlevel 1 goto fail
echo Build succeeded: GymTrack.exe
echo Run run.bat to start the app.
popd
exit /b 0
:fail
echo Build failed. Read the first error above, then see the implementation guide.
popd
exit /b 1
