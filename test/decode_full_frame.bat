@echo off

REM Decode full frame using standard decoder (not selective tiles)
..\build\bin\release\oapv_app_dec -i media\koala_tiled\koala_tiled_0000.apv1 -o output\full_frame_decode_0000.y4m -v 3

REM Convert Y4M to PNG for visual inspection
ffmpeg -y -i output\full_frame_decode_0000.y4m output\full_frame_decode_0000.png