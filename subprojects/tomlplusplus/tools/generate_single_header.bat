@ECHO off
SETLOCAL enableextensions enabledelayedexpansion
PUSHD .
CD /d "%~dp0"

REM --------------------------------------------------------------------------------------
REM 	Invokes generate_single_header.py.
REM --------------------------------------------------------------------------------------

py generate_single_header.py %*
if %ERRORLEVEL% NEQ 0 (
	PAUSE
	GOTO FINISH
)

:FINISH
POPD
@ENDLOCAL
EXIT /B %ERRORLEVEL%
