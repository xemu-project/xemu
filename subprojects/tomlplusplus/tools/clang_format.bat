@ECHO off
SETLOCAL enableextensions enabledelayedexpansion
PUSHD .
CD /d "%~dp0\.."

REM --------------------------------------------------------------------------------------
REM 	Runs clang format on all the C++ files in the project
REM --------------------------------------------------------------------------------------

WHERE /Q clang-format
IF %ERRORLEVEL% NEQ 0 (
	ECHO Could not find clang-format
	PAUSE
	POPD
	ENDLOCAL
	EXIT /B %ERRORLEVEL%
)

CALL :RunClangFormatOnDirectories ^
	include\toml++ ^
	include\toml++\impl ^
	tests ^
	examples

POPD
@ENDLOCAL
EXIT /B 0

:RunClangFormatOnDirectories
(
	FOR %%i IN (%*) DO (
		IF EXIST "%%~i" (
			ECHO Formatting files in "%%~i"
			clang-format --style=file -i "%%~i\*.cpp" >nul 2>&1
			clang-format --style=file -i "%%~i\*.h" >nul 2>&1
			clang-format --style=file -i "%%~i\*.inl" >nul 2>&1
		)
	)
	EXIT /B
)
