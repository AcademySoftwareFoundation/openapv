@echo off
REM OpenAPV 4K Test Runner
REM Runs 4K UHD decoder tests (koala frame) and generates PNG visualizations

REM Set paths as variables
set BUILD_DIR=..\build
set DECODER_EXE=%BUILD_DIR%\Release\decoder_test.exe
set MEDIA_4K=media\koala_tiled\koala_tiled_0000.apv1

echo ========================================
echo OpenAPV 4K UHD Test Runner
echo ========================================
echo.

REM Check if test media exists
if not exist %MEDIA_4K% (
    echo ERROR: 4K test media not found!
    echo Expected: %MEDIA_4K%
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
cmake --build %BUILD_DIR% --config Release --target decoder_test
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
echo.

REM Run 4K UHD decoder tests (explicit test list)
echo ========================================
echo Running 4K UHD decoder tests...
echo ========================================

REM Single tile tests
echo Running single tile tests...
%DECODER_EXE% %MEDIA_4K% single_tile_mip0_origin
%DECODER_EXE% %MEDIA_4K% single_tile_mip1_center

REM Multi-tile performance tests
echo Running multi-tile tests...
%DECODER_EXE% %MEDIA_4K% multi_2x2_contiguous
%DECODER_EXE% %MEDIA_4K% multi_4x4_contiguous
%DECODER_EXE% %MEDIA_4K% multi_middle_6x4_scaling
%DECODER_EXE% %MEDIA_4K% multi_horizontal_strip
%DECODER_EXE% %MEDIA_4K% multi_vertical_strip
%DECODER_EXE% %MEDIA_4K% multi_sparse_corners
%DECODER_EXE% %MEDIA_4K% multi_single_center

REM Full frame validation tests
echo Running full frame validation tests...
%DECODER_EXE% %MEDIA_4K% multi_full_frame_validation
%DECODER_EXE% %MEDIA_4K% multi_all_mip0

REM Mip level tests
echo Running mip level tests...
%DECODER_EXE% %MEDIA_4K% multi_all_mip1
%DECODER_EXE% %MEDIA_4K% multi_some_mip1
%DECODER_EXE% %MEDIA_4K% multi_all_mip2
%DECODER_EXE% %MEDIA_4K% multi_some_mip2
%DECODER_EXE% %MEDIA_4K% multi_all_mip3
%DECODER_EXE% %MEDIA_4K% multi_all_mip4
%DECODER_EXE% %MEDIA_4K% multi_all_mip5
%DECODER_EXE% %MEDIA_4K% multi_all_mip6
%DECODER_EXE% %MEDIA_4K% multi_all_mip7
%DECODER_EXE% %MEDIA_4K% multi_mip1_single
%DECODER_EXE% %MEDIA_4K% multi_mip1_first_row

REM Multi-mip selective decode tests
echo Running multi-mip selective decode tests...
%DECODER_EXE% %MEDIA_4K% multi_all_mip9
%DECODER_EXE% %MEDIA_4K% multimip_single_center
%DECODER_EXE% %MEDIA_4K% multimip_2x2_blocks
%DECODER_EXE% %MEDIA_4K% multimip_sparse
%DECODER_EXE% %MEDIA_4K% multimip_performance_comparison

REM Single mip1 tile tests
echo Running individual mip1 tile tests...
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_0_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_1_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_2_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_3_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_4_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_5_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_6_0
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_7_0

REM More single mip1 tile tests (rows 1-4)
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_0_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_1_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_2_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_3_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_4_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_5_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_6_1
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_7_1

%DECODER_EXE% %MEDIA_4K% single_mip1_tile_0_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_1_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_2_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_3_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_4_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_5_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_6_2
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_7_2

%DECODER_EXE% %MEDIA_4K% single_mip1_tile_0_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_1_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_2_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_3_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_4_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_5_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_6_3
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_7_3

%DECODER_EXE% %MEDIA_4K% single_mip1_tile_0_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_1_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_2_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_3_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_4_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_5_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_6_4
%DECODER_EXE% %MEDIA_4K% single_mip1_tile_7_4

REM Check for test failures
if errorlevel 1 (
    echo ERROR: One or more 4K tests failed!
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
echo 4K Test Summary
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
echo All 4K UHD tests completed successfully!
echo ========================================
echo.
echo To view results:
echo   explorer output
echo.