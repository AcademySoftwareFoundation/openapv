@echo off
REM Build script for OpenAPV encoder and decoder

echo Building OpenAPV encoder and decoder...

REM Check if build directory exists
if not exist build (
    echo Creating build directory...
    mkdir build
)

REM Configure with CMake (Visual Studio 2019/2022)
echo Configuring with CMake...
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -S . -B build
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b %errorlevel%
)

REM Build the encoder
echo Building encoder...
cmake --build build --config Release --target oapv_app_enc
if %errorlevel% neq 0 (
    echo Encoder build failed!
    exit /b %errorlevel%
)

REM Build the decoder
echo Building decoder...
cmake --build build --config Release --target oapv_app_dec
if %errorlevel% neq 0 (
    echo Decoder build failed!
    exit /b %errorlevel%
)

echo.
echo Build completed successfully!
echo Encoder: build\bin\Release\oapv_app_enc.exe
echo Decoder: build\bin\Release\oapv_app_dec.exe