@echo off
setlocal enabledelayedexpansion
title System Health Toolkit v3.0.0

:: ----------------------------------------------------------------
::  System Health Toolkit - Windows Launcher
::  Finds an existing build, or builds from source with CMake.
::  CMake locates Visual Studio / GCC / Clang by itself, so no
::  compiler needs to be on PATH.
:: ----------------------------------------------------------------

:: UTF-8 console so the toolkit's box-drawing output renders correctly
chcp 65001 >nul

set "SHT_DIR=%~dp0"
set "SHT_EXE="

call :find_exe
if defined SHT_EXE goto :run

echo.
echo  ================================================================
echo    System Health Toolkit - executable not found, building...
echo  ================================================================
echo.

:: --- CMake required ---------------------------------------------
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [SHT] CMake is not installed.
    echo.
    echo   Install it with:
    echo     winget install --id Kitware.CMake -e
    echo   or download from https://cmake.org/download/
    echo.
    echo   If you also need a C++ compiler:
    echo     winget install --id Microsoft.VisualStudio.2022.BuildTools -e
    echo   ^(select the "Desktop development with C++" workload^)
    echo.
    pause
    exit /b 1
)
echo [SHT] CMake found: OK

:: --- Configure + build ------------------------------------------
:: The default generator auto-detects an installed Visual Studio,
:: even when cl.exe is not on PATH.
echo [SHT] Configuring build...
if not exist "%SHT_DIR%build_cmake" mkdir "%SHT_DIR%build_cmake"
pushd "%SHT_DIR%build_cmake"
cmake ..
if errorlevel 1 (
    popd
    echo.
    echo [SHT] CMake configuration failed - no usable C++ compiler was found.
    echo.
    echo   Install one of:
    echo     winget install --id Microsoft.VisualStudio.2022.BuildTools -e
    echo       ^(select the "Desktop development with C++" workload^)
    echo     MinGW-w64 from https://winlibs.com/  ^(add its bin folder to PATH^)
    echo.
    echo   Then run this launcher again.
    pause
    exit /b 1
)

echo [SHT] Building ^(Release^)...
cmake --build . --config Release --target SystemHealthToolkit
popd

call :find_exe
if defined SHT_EXE (
    echo.
    echo [SHT] Build successful!
    goto :run
)

echo.
echo [SHT] ERROR: Build failed. Check the compiler output above.
pause
exit /b 1

:: --- Run the Toolkit --------------------------------------------
:run
net session >nul 2>&1
if %errorlevel% equ 0 (set "ELEVATED=Yes") else (set "ELEVATED=No")

echo.
echo  ================================================================
echo    SYSTEM HEALTH TOOLKIT  v3.0.0
echo  ================================================================
echo.
echo   Executable: %SHT_EXE%
echo   Elevated:   %ELEVATED%
if "%ELEVATED%"=="No" echo   Tip: run as Administrator for repairs and deeper scans.
echo.
echo [SHT] Starting System Health Toolkit...
echo.

"%SHT_EXE%" %*
set "EXITCODE=%errorlevel%"
if not "%EXITCODE%"=="0" (
    echo.
    echo [SHT] The toolkit exited with code %EXITCODE%.
    echo   - Run as Administrator for full functionality
    echo   - Check the Logs folder next to the executable for details
)
echo.
echo [SHT] Press any key to close this window...
pause >nul
exit /b %EXITCODE%

:: --- Find existing executable (newest build location first) -----
:find_exe
if exist "%SHT_DIR%build_cmake\Release\SystemHealthToolkit.exe" (
    set "SHT_EXE=%SHT_DIR%build_cmake\Release\SystemHealthToolkit.exe"
    goto :eof
)
if exist "%SHT_DIR%build\Release\SystemHealthToolkit.exe" (
    set "SHT_EXE=%SHT_DIR%build\Release\SystemHealthToolkit.exe"
    goto :eof
)
if exist "%SHT_DIR%build\SystemHealthToolkit.exe" (
    set "SHT_EXE=%SHT_DIR%build\SystemHealthToolkit.exe"
    goto :eof
)
if exist "%SHT_DIR%bin\SystemHealthToolkit.exe" (
    set "SHT_EXE=%SHT_DIR%bin\SystemHealthToolkit.exe"
    goto :eof
)
if exist "%SHT_DIR%SystemHealthToolkit.exe" (
    set "SHT_EXE=%SHT_DIR%SystemHealthToolkit.exe"
    goto :eof
)
goto :eof
