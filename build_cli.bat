@echo off
setlocal
pushd "%~dp0"
call toolchain.bat
if errorlevel 1 goto fail
if not exist build mkdir build
gcc -std=c11 -Wall -Wextra -O2 -static-libgcc original\main.c original\input.c original\workout.c original\features.c original\storage.c -o GymTrackConsole.exe -lm
if errorlevel 1 goto fail
echo Original console program built: GymTrackConsole.exe
echo Run it from this project folder to use workouts.txt here.
popd
exit /b 0
:fail
popd
exit /b 1
