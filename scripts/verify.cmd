@echo off
rem Usage: scripts\verify.cmd [--quick]
setlocal
call "%~dp0env.cmd" || exit /b 1
"%QTD_PYTHON%" "%~dp0verify.py" %*
exit /b %ERRORLEVEL%
