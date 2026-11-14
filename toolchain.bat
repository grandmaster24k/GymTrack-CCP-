@echo off
rem Called by build scripts. Changes are local to the caller's SETLOCAL scope.
if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist "C:\msys64\ucrt64\bin\gcc.exe" if exist "C:\MinGW\bin\gcc.exe" set "PATH=C:\MinGW\bin;%PATH%"
where gcc >nul 2>nul
if errorlevel 1 (
  echo GCC was not found. Install the MSYS2 UCRT64 GCC toolchain or add its bin folder to PATH.
  echo See docs\Implementation_Guide.pdf for the exact installation steps.
  exit /b 1
)
exit /b 0
