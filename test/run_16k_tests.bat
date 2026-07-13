@echo off
REM OpenAPV 16K Test Runner
REM Runs 16K decoder tests and generates PNG visualizations

REM Set paths as variables
set DECODER_EXE=..\build\Release\decoder_test.exe
set MEDIA_16K=media\koala_16k_tiled\koala_16k_tiled_0000.apv1

echo ========================================
echo OpenAPV 16K Test Runner
echo ========================================
echo.

REM Check if test media exists
if not exist %MEDIA_16K% (
    echo ERROR: 16K test media not found!
    echo Expected: %MEDIA_16K%
    echo.
    echo Please generate 16K test media or update the path in this script.
    exit /b 1
)

REM Create output directory if it doesn't exist
if not exist output mkdir output

REM Clean previous test outputs
echo Cleaning previous test outputs...
del /Q output\*.raw 2>nul
del /Q output\*.y4m 2>nul
del /Q output\*.png 2>nul
echo.

REM Build the tests
echo Building decoder tests...
cmake --build ..\build --config Release --target decoder_test
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
echo.

REM Run 16K decoder tests (explicit test list)
echo ========================================
echo Running 16K decoder tests...
echo ========================================

REM 16K Single tile tests
echo Running 16K single tile tests...
%DECODER_EXE% %MEDIA_16K% 16k_single_corner
if errorlevel 1 (
    echo ERROR: 16k_single_corner test failed!
    exit /b 1
)

%DECODER_EXE% %MEDIA_16K% 16k_single_center
if errorlevel 1 (
    echo ERROR: 16k_single_center test failed!
    exit /b 1
)

%DECODER_EXE% %MEDIA_16K% 16k_single_edge_right
if errorlevel 1 (
    echo ERROR: 16k_single_edge_right test failed!
    exit /b 1
)

%DECODER_EXE% %MEDIA_16K% 16k_single_edge_bottom
if errorlevel 1 (
    echo ERROR: 16k_single_edge_bottom test failed!
    exit /b 1
)

REM 16K Multi-tile tests
echo Running 16K multi-tile tests...
%DECODER_EXE% %MEDIA_16K% 16k_multi_2x2_center
if errorlevel 1 (
    echo ERROR: 16k_multi_2x2_center test failed!
    exit /b 1
)

%DECODER_EXE% %MEDIA_16K% 16k_multi_10x10_center
if errorlevel 1 (
    echo ERROR: 16k_multi_10x10_center test failed!
    exit /b 1
)

%DECODER_EXE% %MEDIA_16K% 16k_tile_limit_validation
if errorlevel 1 (
    echo ERROR: 16k_tile_limit_validation test failed!
    exit /b 1
)

echo.

REM Convert all raw files to PNG
echo ========================================
echo Converting all RAW files to PNG images...
echo ========================================
setlocal enabledelayedexpansion
set count=0
for %%f in (output\*.raw) do (
    set /a count+=1
    echo [!count!] Converting %%~nf.raw to PNG...
    python src\convert_tile_to_png.py "%%f"
    if errorlevel 1 (
        echo WARNING: Failed to convert %%f
    )
)
echo Total files converted: %count%
endlocal
echo.

REM Generate summary
echo ========================================
echo 16K Test Summary
echo ========================================
echo.
echo Generated outputs:
setlocal enabledelayedexpansion
for %%e in (raw png y4m) do (
    set count=0
    for %%f in (output\*.%%e) do set /a count+=1
    if !count! gtr 0 echo   %%e files: !count!
)
endlocal
echo.

REM Display file list
echo Files generated:
echo ----------------
dir /B output\*.png 2>nul
if errorlevel 1 echo   (No PNG files found)
echo.

echo ========================================
echo All 16K tests completed successfully!
echo ========================================
echo.
echo To view results:
echo   explorer output
echo.

REM Note about test media
echo NOTE: This script expects 16K test media at:
echo   media\koala_16k_tiled\koala_16k_tiled_0000.apv1
echo.
echo If you need to generate the 16K media, run:
echo   cd media
echo   generate_koala_tiled.bat
echo.