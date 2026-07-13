@echo off
REM OpenAPV Test Runner - Main Entry Point
REM This script helps you choose the appropriate test suite

echo ========================================
echo OpenAPV Test Runner - Main Entry Point
echo ========================================
echo.
echo This script has been updated to use specific test runners.
echo Please choose the appropriate test suite:
echo.
echo   run_4k_tests.bat   - Run 4K UHD tests (koala frame)
echo   run_16k_tests.bat  - Run 16K tests (16K frame)
echo.
echo ========================================
echo Available Test Suites:
echo ========================================
echo.
echo 1. 4K UHD Tests (run_4k_tests.bat)
echo    - Uses: media\koala_tiled\koala_tiled_0000.apv1
echo    - Tests: ~62 tests covering single/multi-tile scenarios
echo    - Includes: Performance, validation, mip levels
echo.
echo 2. 16K Tests (run_16k_tests.bat)
echo    - Uses: media\16k_frame\16k_frame_0000.apv1
echo    - Tests: 7 tests covering 16K-specific scenarios
echo    - Includes: Edge cases, high tile counts, limit validation
echo.
echo ========================================
echo Quick Start:
echo ========================================
echo.
echo For most testing, run:
echo   run_4k_tests.bat
echo.
echo For 16K validation, run:
echo   run_16k_tests.bat
echo.

run_4k_tests.bat
run_16k_tests.bat

pause