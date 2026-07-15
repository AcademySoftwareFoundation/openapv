REM Generate yuv
REM
REM It generates a single .yuv with the first NUMFRAMES frames (@ 30 fps) of
REM the source footage (koala.mp4 is trimmed to 10 frames to keep the repo
REM small). It is encoded in yuv422p10, so each pixel is a 10-bit luma and a
REM 10-bit chroma, but padded in 16-bit values.

@echo off

set NAME=koala
set INPUT=%NAME%.mp4
set YUV=%NAME%_3840x2160_yuv422p10le.yuv
set NUMFRAMES=10

ffmpeg -y -i "%INPUT%" -pix_fmt yuv422p10le -s 3840x2160 -f rawvideo -frames:v %NUMFRAMES% "%YUV%"

REM Generate .apv1 frames

set OAPV_ENC_EXE=..\..\build\bin\release\oapv_app_enc
set OAPV_ENC_ARGS=--color-primaries 1 --color-transfer 1 --color-matrix 1 --color-range 1

REM Create output directory
if not exist "%NAME%_tiled" mkdir "%NAME%_tiled"

%OAPV_ENC_EXE% -i "%YUV%" -w 3840 -h 2160 -d 10 -z 30 --input-csp 2 --tile-w 256 --tile-h 256 --tmv-mips %OAPV_ENC_ARGS% -o %NAME%_tiled/%NAME%_tiled

REM Generate 16K version from the first frame

echo.
echo ========================================
echo Generating 16K version from the first frame
echo ========================================

set YUV_16K=%NAME%_16k_3840x2160_yuv422p10le.yuv
set YUV_16K_UPSCALED=%NAME%_16k_15360x8640_yuv422p10le.yuv

REM Extract the first frame (the 16K tests only exercise tile addressing /
REM chroma at 16K resolution, so any frame works; frame 0 keeps the source
REM clip tiny -- see NUMFRAMES above).
echo Step 1: Extracting the first frame...
ffmpeg -y -i "%INPUT%" -pix_fmt yuv422p10le -s 3840x2160 -f rawvideo -frames:v 1 "%YUV_16K%"

if errorlevel 1 (
    echo ERROR: Failed to extract the first frame
    exit /b 1
)

REM Upscale to 16K resolution
echo Step 2: Upscaling to 16K (15360x8640)...
ffmpeg -f rawvideo -pix_fmt yuv422p10le -s 3840x2160 -i "%YUV_16K%" -vf "scale=15360:8640:flags=lanczos" -f rawvideo -frames:v 1 -pix_fmt yuv422p10le -y "%YUV_16K_UPSCALED%"

if errorlevel 1 (
    echo ERROR: Failed to upscale to 16K
    exit /b 1
)

REM Create 16K output directory
if not exist "%NAME%_16k_tiled" mkdir "%NAME%_16k_tiled"

REM Encode 16K APV1
echo Step 3: Encoding 16K APV1...
%OAPV_ENC_EXE% -i "%YUV_16K_UPSCALED%" -w 15360 -h 8640 -d 10 -z 30 --input-csp 2 --tile-w 256 --tile-h 256 --tmv-mips %OAPV_ENC_ARGS% -o %NAME%_16k_tiled/%NAME%_16k_tiled

if errorlevel 1 (
    echo ERROR: Failed to encode 16K APV1
    exit /b 1
)

REM Generate PNG preview of 16K frame
echo Step 4: Generating 16K PNG preview...
ffmpeg -f rawvideo -pix_fmt yuv422p10le -s 15360x8640 -i "%YUV_16K_UPSCALED%" -frames:v 1 -y %NAME%_16k_preview.png

if errorlevel 1 (
    echo WARNING: Failed to generate 16K PNG preview
) else (
    echo 16K PNG preview created: %NAME%_16k_preview.png
)

REM Clean up temporary YUV files
echo.
echo Cleaning up temporary files...
del /Q "%YUV%" 2>nul
del /Q "%YUV_16K%" 2>nul
del /Q "%YUV_16K_UPSCALED%" 2>nul

echo.
echo ========================================
echo Generation Complete!
echo ========================================
echo.
echo Generated files:
echo   4K APV1: %NAME%_tiled/%NAME%_tiled_0000.apv1
if exist %NAME%_16k_tiled/%NAME%_16k_tiled_0000.apv1 (
    for %%A in (%NAME%_16k_tiled/%NAME%_16k_tiled_0000.apv1) do echo   16K APV1: %NAME%_16k_tiled/%NAME%_16k_tiled_0000.apv1 (%%~zA bytes)
)
if exist %NAME%_16k_preview.png (
    for %%A in (%NAME%_16k_preview.png) do echo   16K PNG: %NAME%_16k_preview.png (%%~zA bytes)
)



