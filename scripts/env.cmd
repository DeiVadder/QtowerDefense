@echo off
rem Build environment of QtowerDefense: MSVC x64, Qt, CMake and Ninja from C:\Dev\Qt\Tools.
rem Keeps the MinGW g++ and the old CMake of Strawberry out of the way. Safe to call twice.
if defined QTD_ENV_READY exit /b 0

set "QTD_QT_DIR=C:\Dev\Qt\6.12.0\msvc2022_64"
set "QTD_PYTHON=C:\Dev\Python313\python.exe"
set "QTD_LLVM_DIR=C:\dev\LLVM\clang+llvm-23.1.5-x86_64-pc-windows-msvc\bin"
set "QTD_CPPCHECK=C:\Dev\Strawberry\c\bin\cppcheck.exe"
set "QTD_OPENCPPCOVERAGE=C:\dev\OpenCppCoverage\OpenCppCoverage.exe"
set "QTD_CLANGD=C:\dev\clangd\clangd_23.1.0\bin\clangd.exe"

call "C:\Dev\VS2022BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo env.cmd: vcvarsall failed 1>&2
    exit /b 1
)
set "PATH=%QTD_QT_DIR%\bin;C:\Dev\Qt\Tools\CMake_64\bin;C:\Dev\Qt\Tools\Ninja;%QTD_LLVM_DIR%;C:\Dev\Python313;%PATH%"
set "QTD_ENV_READY=1"
exit /b 0
