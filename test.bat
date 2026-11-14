@echo off
setlocal
pushd "%~dp0"
call toolchain.bat
if errorlevel 1 goto fail
if not exist build mkdir build
if not exist build\test-data mkdir build\test-data
gcc -std=c11 -Wall -Wextra -Werror -O2 -static-libgcc -Ioriginal -Igui tests\test_bridge.c gui\tracker_bridge.c -o build\test_bridge.exe -lm
if errorlevel 1 goto fail
pushd build\test-data
..\test_bridge.exe .
set "HT_RESULT=%ERRORLEVEL%"
popd
if not "%HT_RESULT%"=="0" goto fail
echo All bridge tests passed.
popd
exit /b 0
:fail
echo Test build or execution failed.
popd
exit /b 1
