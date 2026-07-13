@echo off
REM OpenAPV Frame list Test Runner

REM Set paths as variables
set DECODER_EXE=..\build\Release\decoder_test.exe
set FRAMELIST=framelist.txt

echo ========================================
echo OpenAPV Frame List Decoder Test Runner
echo ========================================
echo.

REM Check if frame list exists
if not exist %FRAMELIST% (
    echo ERROR: frame list not found!
    echo Expected: %FRAMELIST%
    exit /b 1
)

REM Build the tests
echo Building decoder tests...
cmake --build ..\build --config Release --target decoder_test
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
echo.

REM Run frame list decoder tests
echo ========================================
echo Running frame list decoder tests...
echo ========================================

%DECODER_EXE% %FRAMELIST%

REM Check for test failures
if errorlevel 1 (
    echo ERROR: One or more 4K tests failed!
    exit /b 1
)
echo.