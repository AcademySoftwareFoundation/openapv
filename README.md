![OAPV](/readme/img/oapv_logo_black_bar_256.png#gh-light-mode-only) ![OAPV](/readme/img/oapv_logo_white_bar_256.png#gh-dark-mode-only)
# OpenAPV (Open Advanced Professional Video Codec)

[![Build and test](https://github.com/AcademySoftwareFoundation/openapv/actions/workflows/build.yml/badge.svg)](https://github.com/AcademySoftwareFoundation/openapv/actions/workflows/build.yml)

OpenAPV provides the reference implementation of the [APV codec](#apv-codec) which can be used to record professional-grade video and associated metadata without quality degradation. OpenAPV is free and open source software provided by [LICENSE](#license).

The OpenAPV supports the following features:

- fully compliant with 422-10, 422-12, 444-10, 444-12, 4444-10, 4444-12, and 400-10 profile of [APV codec](#apv-codec)
- Low complexity by optimization for ARM NEON and x86(64bit) SEE/AVX CPU
- Tile-based multi-threading
- Tile-based partial decoding, which decodes only the tiles an application asks for
- Various metadata including HDR10/10+ and user-defined format
- RGB content coding with the 444 profiles through color description signalling
- Constant QP (CQP) and average bitrate (ABR) rate control algorithms
- [APV Family](/readme/apv_family.md) configurations for typical target bitrate setting of encoder
- [APV Extensions](/readme/apv_ext.md) defined by the OpenAPV project on top of the RFC 9924 profiles: the 444-16C12 and 4444-16C12 profiles for 16-bit source companded to 12-bit, and the UNCONST profiles without the tile partitioning constraints


## APV codec
The APV codec is a professional video codec, which was developed in response to the need for professional level high quality video recording and post production. The primary purpose of the APV codec is for use in professional video recording and editing workflows for various types of content.

APV codec utilizes technologies known to be over 20 years old to achieve a royalty free codec. APV builds a video codec using only conventional coding technologies, which consist of traditional methods published between the early 1980s and the end of the 1990s.

The APV codec standard has the following features:

- Perceptually lossless video quality, which is close to raw video quality
- Low complexity and high throughput intra frame only coding without pixel domain prediction
- High bit-rate range up to a few Gbps for 2K, 4K and 8K resolution content, enabled by a lightweight entropy coding scheme
- Frame tiling for immersive content and for enabling parallel encoding and decoding
- Various chroma sampling formats from 4:2:2 to 4:4:4, and bit-depths from 10 to 16
- Multiple decoding and re-encoding without severe visual quality degradation
- Multi-view video and auxiliary video like depth, alpha, and preview
- Various metadata including HDR10/10+ and user-defined format

### Related specification
- APV Codec (bitstream): [RFC 9924](https://www.rfc-editor.org/rfc/rfc9924.html)
- APV ISO based media file format: [APV-ISOBMFF](/readme/apv_isobmff.md)
- APV RTP payload format: [https://datatracker.ietf.org/doc/draft-ietf-avtcore-rtp-apv/](https://datatracker.ietf.org/doc/draft-ietf-avtcore-rtp-apv/)
- APV Family: [APV-Family](/readme/apv_family.md)
- APV Extensions: [APV-Extensions](/readme/apv_ext.md)

## How to build
- Build Requirements
  - CMake (download from [https://cmake.org/](https://cmake.org/))
  - GCC

  For ARM
  - gcc-aarch64-linux-gnu
  - binutils-aarch64-linux-gnu

  For Windows (crosscompile)
  - mingw-w64
  - mingw-w64-tools

- Build Instructions PC (Linux)
  ```
  cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
  cmake --build build
  ```

- Build Instructions ARM (Crosscompile)
  ```
  cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=arm64_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build-arm
  ```

- Build Instructions Windows (Crosscompile)
  ```
  cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=windows_x86_64_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build-windows
  ```

- Build Instructions macOS (Apple Silicon)
  ```
  cmake -S . -B build-darwin -DUNIVERSAL=FALSE -DCMAKE_OSX_ARCHITECTURES=arm64 -DARM=1 -DCMAKE_BUILD_TYPE=Release
  cmake --build build-darwin
  ```

- Build Instructions macOS (Universal)
  ```
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=macos_universal_toolchain.cmake -DUNIVERSAL=1 -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```

- Build Instructions iOS (ARM64)
  ```
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=ios_arm64_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```

- Output Location
  - Executable applications can be found under build*/bin/
  - Library files can be found under build*/lib/

## How to use applications
### Encoder

Encoder requires raw YCbCr file (422, 444), 10-bit or more, as input.

Displaying help:

    oapv_app_enc --help

Encoding a raw YCbCr file, where the format has to be given, and a Y4M file,
which carries the format in its header:

    oapv_app_enc -i input_1920x1080_yuv422_10bit.yuv -w 1920 -h 1080 -d 10 -z 30 --input-csp 2 -o encoded.apv
    oapv_app_enc -i input.y4m -o encoded.apv

Choosing the quality. A fixed quantization parameter gives constant quality,
while a target bitrate lets the rate control pick the quantization parameter.
The preset trades encoding speed for compression efficiency:

    oapv_app_enc -i input.y4m -q 25 --preset slow -o encoded.apv
    oapv_app_enc -i input.y4m --bitrate 200mbps -o encoded.apv
    oapv_app_enc -i input.y4m --family 422-HQ -o encoded.apv

Selecting a profile. The input is converted to the color space and bit depth
of the profile if needed:

    oapv_app_enc -i input_12bit.y4m --profile 422-12 -q 25 -o encoded.apv
    oapv_app_enc -i input_444_12bit.y4m --profile 444-12 -q 25 -o encoded.apv

Encoding RGB content (G/B/R planar order, coded as 444 with the identity matrix signalled in the color description; see the [Programmer's Guide](/readme/programmers_guide.md) for details):

    oapv_app_enc -i input_rgb_gbr_planar_10bit.yuv -w 1920 -h 1080 -d 10 -z 30 --input-csp 3 --profile 444-10 --color-primaries 1 --color-transfer 13 --color-matrix 0 --color-range 1 -o encoded.apv

Controlling tiles and threads. Smaller tiles give more parallelism and
finer-grained partial decoding; the [APV Extensions](/readme/apv_ext.md)
profiles lift the tile size and count limits of RFC 9924:

    oapv_app_enc -i input.y4m -q 25 --tile-w 256 --tile-h 256 -m 8 -o encoded.apv
    oapv_app_enc -i input.y4m -q 25 --profile 422-10-UNCONST --tile-w 64 --tile-h 64 -o encoded.apv

Writing the reconstructed video and embedding a frame hash, so a decoder can
verify that it reconstructs exactly the same picture:

    oapv_app_enc -i input.y4m -q 25 --hash -r recon.y4m -o encoded.apv

Encoding a part of the input, skipping the first 100 frames and coding the
next 50:

    oapv_app_enc -i input.y4m -q 25 --seek 100 --max-au 50 -o encoded.apv

### Decoder

Decoder output can be in yuv or y4m formats.

Displaying help:

    oapv_app_dec --help

Decoding to a Y4M file, which records the format, or to a raw YCbCr file:

    oapv_app_dec -i encoded.apv -o output.y4m
    oapv_app_dec -i encoded.apv -o output.yuv

Verifying the bitstream against the frame hash embedded by the encoder, and
decoding without writing any output, which is useful for timing:

    oapv_app_dec -i encoded.apv --hash -v 3 -o output.y4m
    oapv_app_dec -i encoded.apv -v 3

Converting the output. The bit depth can be changed and 422 content can be
written as P210:

    oapv_app_dec -i encoded.apv -d 10 -o output.y4m
    oapv_app_dec -i encoded.apv --output-csp 1 -o output.yuv

Decoding a limited number of access units with a fixed number of threads:

    oapv_app_dec -i encoded.apv --max-au 50 -m 8 -o output.y4m

Decoding PBU by PBU with API set 1, which also allows decoding only a subset
of the tiles of each frame (see the [Programmer's Guide](/readme/programmers_guide.md)):

    oapv_app_dec -i encoded.apv --api-set 1 -o output.y4m
    oapv_app_dec -i encoded.apv --api-set 1 --cyclic-tile-decoding 4 -o output.y4m

## Programmer's guide

See the [Programmer's Guide](/readme/programmers_guide.md) for how to encode
and decode with the library, including PBU-based decoding, tile-based
partial decoding, runtime configuration, RGB content encoding, and the
custom memory allocator interface.

## Utility

### Graphical APV bitstream parser

Pattern file of APV bitstream for [ImHex](https://github.com/WerWolv/ImHex) is provided [here](/util/apv.hexpat).
1. Install [ImHex](https://github.com/WerWolv/ImHex) application
2. Download [APV pattern file](/util/apv.hexpat) and copy it to 'patterns' directory of the ImHex application
3. Open an APV bitstream (*.apv file) file with the ImHex application
4. The APV pattern file will be selected automatically and press 'yes' to apply it

![APV_on_ImHex](/readme/img/apv_parser_on_imhex.png)

## Testing

In build directory run ``ctest``

## Code Coverage

To generate a code coverage report manually:

1.  **`lcov` is required.**

2.  **Build and Test with Coverage:**
    ```bash
    # Create build directory and configure with coverage enabled
    cmake -S . -B build -DENABLE_COVERAGE=ON

    # Build the project
    cmake --build build

    # Run tests to generate coverage data
    cd build && ctest && cd ..
    ```

3.  **Generate HTML Report:**
    ```bash
    # Capture coverage data
    lcov --capture --directory build --output-file build/coverage.info

    # Remove coverage for external files (optional)
    lcov --remove build/coverage.info '/usr/*' --output-file build/coverage.info

    # Generate the HTML report
    mkdir -p coverage_report
    genhtml build/coverage.info --output-directory coverage_report
    ```

4.  **View Report:**
    Open `coverage_report/index.html` in your web browser.

## Packaging

For generating package ready for distribution (default deb) execute in build directory ``cpack``,  or other formats (tgz, zip etc.) ``cpack -G TGZ``.

## Versioning

This project is using the following versioning scheme ``API-SET.MAJOR.MINOR.PATCH``. It's mostly based on Semantic Versioning with addition of ``API-SET`` on first place.
Project and library share a common version number.

## Contributing

Contributions are welcome through pull requests.

- Create a topic branch and keep one feature or fix per pull request
- Sign off every commit with `git commit -s` (the DCO check requires it)
- Make sure the CI builds and tests pass

When opening a pull request from a fork, please enable **"Allow edits from
maintainers"**. This lets maintainers push small fixes (typos, rebase
conflicts, style adjustments) directly to your branch instead of describing
them in comments and waiting for your update, which can shorten the review
cycle considerably.

## License

See [LICENSE](LICENSE) file for details.

## Graphic logo

The black and the white logo below are meant for the opposite background, so
one of them blends into the page you are reading this on and looks blank.
Drag over it, or open the image file directly, to see it.

### logo
![OAPV](/readme/img/oapv_logo_bar_64.png) ![OAPV](/readme/img/oapv_logo_bar_128.png) ![OAPV](/readme/img/oapv_logo_bar_256.png) ![OAPV](/readme/img/oapv_logo_bar_512.png)

### black color logo for light background
![OAPV](/readme/img/oapv_logo_black_bar_64.png) ![OAPV](/readme/img/oapv_logo_black_bar_128.png) ![OAPV](/readme/img/oapv_logo_black_bar_256.png) ![OAPV](/readme/img/oapv_logo_black_bar_512.png)

### white color logo for dark background
![OAPV](/readme/img/oapv_logo_white_bar_64.png) ![OAPV](/readme/img/oapv_logo_white_bar_128.png) ![OAPV](/readme/img/oapv_logo_white_bar_256.png) ![OAPV](/readme/img/oapv_logo_white_bar_512.png)
