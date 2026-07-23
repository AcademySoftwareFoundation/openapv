![OAPV](/readme/img/oapv_logo_black_bar_256.png#gh-light-mode-only) ![OAPV](/readme/img/oapv_logo_white_bar_256.png#gh-dark-mode-only)
# OpenAPV (Open Advanced Professional Video Codec)

[![Build and test](https://github.com/AcademySoftwareFoundation/openapv/actions/workflows/build.yml/badge.svg)](https://github.com/AcademySoftwareFoundation/openapv/actions/workflows/build.yml)

OpenAPV provides the reference implementation of the [APV codec](#apv-codec) which can be used to record professional-grade video and associated metadata without quality degradation. OpenAPV is free and open source software provided by [LICENSE](#license).

The OpenAPV supports the following features:

- fully compliant with 422-10, 422-12, 444-10, 444-12, 4444-10, 4444-12, and 400-10 profile of [APV codec](#apv-codec)
- Low complexity by optimization for ARM NEON and x86(64bit) SEE/AVX CPU
- Tile-based multi-threading
- Various metadata including HDR10/10+ and user-defined format
- Constant QP (CQP) and average bitrate (ABR) rate control algorithms
- [APV Family](/readme/apv_family.md) configurations for typical target bitrate setting of encoder


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
- APV Codec (bitstream): [https://datatracker.ietf.org/doc/draft-lim-apv/](https://datatracker.ietf.org/doc/draft-lim-apv/)
  - Scope of OpenAPV project
- APV ISO based media file format: [APV-ISOBMFF](/readme/apv_isobmff.md)
- APV RTP payload format: [https://datatracker.ietf.org/doc/draft-lim-rtp-apv/](https://datatracker.ietf.org/doc/draft-lim-rtp-apv/)

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

Encoding:

    oapv_app_enc -i input_1920x1080_yuv422_10bit.yuv -w 1920 -h 1080 -d 10 -z 30 --input-csp 2 -o encoded.apv
    oapv_app_enc -i input.y4m -o encoded.apv

### Decoder

Decoder output can be in yuv or y4m formats.

Displaying help:

    oapv_app_dec --help

Decoding:

    oapv_app_dec -i encoded.apv -o output.y4m

## Custom memory allocator

By default the library allocates with the standard C library. An application
can instead supply its own allocator **per codec instance** through the
`ops_mem` field of the encoder/decoder descriptor (`oapve_cdesc_t` /
`oapvd_cdesc_t`). This is useful for integrating with a host memory manager
(e.g. a game-engine allocator) or for memory tracking. There is no global
allocator state.

The interface is `oapv_ops_mem_t` (declared in `oapv.h`):

```c
typedef struct oapv_ops_mem {
    unsigned int magic;   // set to OAPV_OPS_MAGIC_CODE_MEM when used
    void *(*malloc) (void *udata, unsigned int size);
    void *(*calloc) (void *udata, unsigned int count, unsigned int size);
    void *(*realloc)(void *udata, void *ptr, unsigned int size);
    void  (*free)   (void *udata, void *ptr);
    void  *udata;         // opaque pointer passed back to every callback
} oapv_ops_mem_t;
```

Rules:
- Provide **all four** function pointers (custom), or **none** — leave
  `ops_mem` as `NULL` to use the standard C library. A partially-filled
  interface is rejected with `OAPV_ERR_INVALID_ARGUMENT`.
- When providing custom allocators, set `magic` to `OAPV_OPS_MAGIC_CODE_MEM`.
- `udata` is passed unchanged to every callback; the object it points to must
  stay valid for the lifetime of the codec instance.
- `free` must accept a `NULL` pointer (like standard `free`).
- Zero-initialize the descriptor so `ops_mem` defaults to `NULL` when not used.

Example (encoder; the decoder is identical via `oapvd_cdesc_t.ops_mem`):

```c
static void *my_malloc(void *u, unsigned int s)
{
    return my_alloc((MyHeap *)u, s);
}

static void *my_calloc(void *u, unsigned int c, unsigned int s)
{
    return my_zalloc((MyHeap *)u, c, s);
}

static void *my_realloc(void *u, void *p, unsigned int s)
{
    return my_realloc_((MyHeap *)u, p, s);
}

static void my_free(void *u, void *p)
{
    my_free_((MyHeap *)u, p);
}

int main(void)
{
    MyHeap heap = /* ... */;
    oapv_ops_mem_t ops = {
        OAPV_OPS_MAGIC_CODE_MEM,
        my_malloc, my_calloc, my_realloc, my_free,
        &heap
    };

    oapve_cdesc_t cdesc;
    memset(&cdesc, 0, sizeof(cdesc)); // ops_mem defaults to NULL
    /* ... set other cdesc fields ... */
    cdesc.ops_mem = &ops;             // opt in to the custom allocator

    int err;
    oapve_t eid = oapve_create(&cdesc, &err);
}
```

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

## License

See [LICENSE](LICENSE) file for details.

## Graphic logo
### logo
![OAPV](/readme/img/oapv_logo_bar_64.png) ![OAPV](/readme/img/oapv_logo_bar_128.png) ![OAPV](/readme/img/oapv_logo_bar_256.png) ![OAPV](/readme/img/oapv_logo_bar_512.png)

### black color logo for light background
![OAPV](/readme/img/oapv_logo_black_bar_64.png) ![OAPV](/readme/img/oapv_logo_black_bar_128.png) ![OAPV](/readme/img/oapv_logo_black_bar_256.png) ![OAPV](/readme/img/oapv_logo_black_bar_512.png)

### white color logo for dark background
![OAPV](/readme/img/oapv_logo_white_bar_64.png) ![OAPV](/readme/img/oapv_logo_white_bar_128.png) ![OAPV](/readme/img/oapv_logo_white_bar_256.png) ![OAPV](/readme/img/oapv_logo_white_bar_512.png)
