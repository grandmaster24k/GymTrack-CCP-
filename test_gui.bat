@echo off
setlocal
pushd "%~dp0"
call toolchain.bat
if errorlevel 1 goto fail
where windres >nul 2>nul
if errorlevel 1 goto fail
if not exist build mkdir build
if not exist build\gui-test mkdir build\gui-test
:choose_directory
set "HT_GUI_RUN=build\gui-test\run_%RANDOM%_%RANDOM%"
if exist "%HT_GUI_RUN%" goto choose_directory
mkdir "%HT_GUI_RUN%"
if errorlevel 1 goto fail
windres -I . gui\app.rc -O coff -o "%HT_GUI_RUN%\app_res.o"
if errorlevel 1 goto fail
gcc -std=c11 -Wall -Wextra -Werror -O2 -DHT_GUI_TEST -mwindows -static-libgcc -Ioriginal -Igui gui\gymtrack_gui.c gui\tracker_bridge.c "%HT_GUI_RUN%\app_res.o" -o "%HT_GUI_RUN%\GymTrackGuiTest.exe" -lcomctl32 -lgdi32 -luser32 -lcomdlg32 -ldwmapi -luxtheme -lmsimg32 -ld2d1 -luuid -lwinmm -lm
if errorlevel 1 goto fail
echo Running native GUI integration checks in an isolated test folder...
start "" /wait "%HT_GUI_RUN%\GymTrackGuiTest.exe"
set "HT_GUI_RESULT=%ERRORLEVEL%"
if exist "%HT_GUI_RUN%\gui-test-results.txt" type "%HT_GUI_RUN%\gui-test-results.txt"
if not "%HT_GUI_RESULT%"=="0" goto fail
if not exist "%HT_GUI_RUN%\gui-test-results.txt" goto fail
findstr /C:"PASSED:" "%HT_GUI_RUN%\gui-test-results.txt" >nul
if errorlevel 1 goto fail
echo Native GUI integration checks passed.
echo Results are in %HT_GUI_RUN%\gui-test-results.txt
popd
exit /b 0
:fail
echo Native GUI integration checks failed. See the first error or result log.
popd
exit /b 1
