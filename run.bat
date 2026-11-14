@echo off
setlocal
pushd "%~dp0"
if not exist GymTrack.exe (
  call build.bat
  if errorlevel 1 (
    popd
    pause
    exit /b 1
  )
)
start "GymTrack" GymTrack.exe
popd
exit /b 0
