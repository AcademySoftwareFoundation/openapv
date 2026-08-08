# OpenAPV Programmer's Guide

This guide shows how to write encoding and decoding code with the OpenAPV
library (`liboapv`). The examples are pseudo code that follows the structure
of the sample applications under `app/`; refer to them for complete working
code. All types and functions are declared in `oapv.h`.

## Common types

Instance handles. Each is an opaque pointer returned by its create function
and released by the matching delete function. An instance is not internally
serialized, so use one instance per thread that encodes or decodes:

- `oapve_t` — an encoder instance, from `oapve_create()` with an
  `oapve_cdesc_t`, released with `oapve_delete()`. It holds the encoding
  parameters of every frame slot, the worker threads, and the rate control
  state that carries across access units.
- `oapvd_t` — a decoder instance, from `oapvd_create()` with an
  `oapvd_cdesc_t`, released with `oapvd_delete()`. It holds the worker
  threads and the state needed while decoding one access unit.
- `oapvm_t` — a metadata container, from `oapvm_create()` with an
  `oapvm_cdesc_t`, released with `oapvm_delete()`. It carries the metadata
  of one access unit in either direction: the encoder reads what the
  application put in it and writes it into the bitstream, and the decoder
  fills it with what it finds. Clear it with `oapvm_rem_all()` between
  access units.

Creation descriptors, which configure an instance at create time:

- `oapve_cdesc_t` — encoder settings that cannot change later: the number of
  frames per access unit (`max_num_frms`), the bitstream buffer capacity
  (`max_bs_buf_size`), the thread count, the custom allocator (`ops_mem`),
  and an `oapve_param_t` for each frame slot.
- `oapvd_cdesc_t` — decoder settings: the thread count and the custom
  allocator.
- `oapvm_cdesc_t` — metadata container settings: the custom allocator.

Encoding parameters:

- `oapve_param_t` — the coding settings of one frame slot: resolution, frame
  rate, profile, level and band, quantization parameter or bitrate, tile
  size, quantization matrix, and the color description. Start from
  `oapve_param_default()` and change what you need. Most of these can also
  be changed per frame at run time with `oapve_config()`.

Data buffers, all owned by the application:

- `oapv_imgb_t` — an image buffer holding the planes of one frame, together
  with the color space, the plane strides, and the buffer sizes. It carries
  `addref` / `release` callbacks for reference counting.
- `oapv_bitb_t` — a bitstream buffer. `addr` points to application memory,
  `bsize` is its capacity for encoding, and `ssize` is the size of the data
  to read for decoding.
- `oapv_frm_t` / `oapv_frms_t` — one frame and a set of frames making up an
  access unit. Each entry pairs an image buffer with a `pbu_type`
  (`OAPV_PBU_TYPE_PRIMARY_FRAME`, non-primary, preview, depth, alpha) and a
  `group_id` that ties frames and their metadata together.

Result and information types, filled by the library:

- `oapve_stat_t` / `oapvd_stat_t` — the outcome of one call: how many bytes
  were written or read, the per-frame sizes, and an `oapv_au_info_t`.
- `oapv_au_info_t` — the frames of an access unit, each described by an
  `oapv_frm_info_t`.
- `oapv_frm_info_t` — the format of one frame: resolution, color space,
  profile, level and band, bit depth, the tile grid, the quantization
  matrix, and the color description.
- `oapv_tile_pos_t` — the position of one tile in the frame, and its
  location in the bitstream when the frame header carries the tile sizes.
- `oapvm_payload_t` — one metadata payload: its type, its group, its data,
  and a UUID for user-defined payloads.

## Writing an encoder

```
// create an encoder
cdesc = {0}
param = &cdesc.param[0]
oapve_param_default(param)
param.w = width, param.h = height, param.fps_num/fps_den = frame rate
param.qp = qp                        // or set profile, bitrate, tile size, ...
cdesc.max_num_frms = 1               // frames per AU
cdesc.threads = OAPV_CDESC_THREADS_AUTO
cdesc.max_bs_buf_size = big_enough   // capacity for one coded AU
eid = oapve_create(&cdesc, &err)

// create a metadata container
mdesc = {0}
mid = oapvm_create(&mdesc, &err)

// prepare buffers
bitb.addr = malloc(cdesc.max_bs_buf_size)
bitb.bsize = cdesc.max_bs_buf_size
imgb = create_image_buffer(width, height, color_space)

// encoding loop
while(read_one_frame(input, imgb) == OK) {
    ifrms.num_frms = 1
    ifrms.frm[0].imgb = imgb
    ifrms.frm[0].pbu_type = OAPV_PBU_TYPE_PRIMARY_FRAME
    ifrms.frm[0].group_id = 1

    oapve_encode(eid, &ifrms, mid, &bitb, &stat, NULL)

    write(output, bitb.addr, stat.write)  // one coded AU
    oapvm_rem_all(mid)                    // clear metadata for the next AU
}

// cleanup
oapvm_delete(mid)
oapve_delete(eid)
imgb.release(imgb)
free(bitb.addr)
```

## Runtime configuration

Options can be queried and changed after creation with `oapve_config()` and
`oapvd_config()`. Both take a config id, a value buffer, and its size:

```
value = 1
size = sizeof(int)
oapve_config(eid, OAPV_CFG_FRM(OAPV_CFG_SET_USE_FRM_HASH, 0), &value, &size)
```

Encoder config ids use the `OAPV_CFG_SET_*` / `OAPV_CFG_GET_*` values in
`oapv.h`, e.g. `OAPV_CFG_SET_QP`, `OAPV_CFG_SET_BPS`,
`OAPV_CFG_SET_USE_FRM_HASH`, or `OAPV_CFG_SET_TILE_SIZE_IN_FH`. Most
encoder configs apply to one frame slot of the AU; wrap the id with
`OAPV_CFG_FRM(cfg, frm_idx)` to select the frame, where the plain id means
frame 0. AU-level configs such as `OAPV_CFG_SET_AU_BS_FMT` apply to the
whole instance and ignore the frame index.

The decoder takes plain config ids without the `OAPV_CFG_FRM()` wrapper. It
supports `OAPV_CFG_SET_USE_FRM_HASH` (verify the frame hash metadata during
decoding) and `OAPV_CFG_SET_DISABLE_COMPANDING`:

```
value = 1
size = sizeof(int)
oapvd_config(did, OAPV_CFG_SET_USE_FRM_HASH, &value, &size)
```

## Writing a decoder

The decoder offers two API sets. API set 0 decodes a whole AU in one call.
API set 1 walks the bitstream PBU by PBU, which gives the application
control over each frame and enables partial decoding.

### API set 0: access unit decoding

```
// create a decoder and a metadata container
cdesc = {0}
cdesc.threads = OAPV_CDESC_THREADS_AUTO
did = oapvd_create(&cdesc, &err)
mdesc = {0}
mid = oapvm_create(&mdesc, &err)

// decoding loop
while(read_one_au(input, au_buf, &au_size) == OK) {
    // query the frames in this AU and prepare output buffers
    oapvd_info(au_buf, au_size, &aui)
    for(i = 0; i < aui.num_frms; i++) {
        finfo = &aui.frm_info[i]
        ofrms.frm[i].imgb = create_image_buffer(finfo.w, finfo.h, finfo.cs)
    }
    ofrms.num_frms = aui.num_frms

    bitb.addr = au_buf
    bitb.ssize = au_size
    oapvd_decode(did, &bitb, &ofrms, mid, &stat)

    for(i = 0; i < ofrms.num_frms; i++) {
        write_frame(output, ofrms.frm[i].imgb)
    }
    // read the collected metadata of this AU, if needed
    oapvm_get_all(mid, plds, &num_plds)
    oapvm_rem_all(mid)
}

oapvm_delete(mid)
oapvd_delete(did)
```

### API set 1: PBU-based decoding

An AU in the raw bitstream format is laid out as:

```
au_size(4) | signature 'aPv1'(4) | pbu_size(4) | pbu() | pbu_size(4) | pbu() | ...
```

The application reads each PBU and decides what to do with it:

```
did = oapvd_create(&cdesc, &err)
mid = oapvm_create(&mdesc, &err)

while(read_u32(input, &au_size) == OK) {
    check_signature(input)                       // 'aPv1'
    remained = au_size - 4
    while(remained > 0) {
        read_u32(input, &pbu_size)
        read(input, pbu_buf, pbu_size)
        remained -= 4 + pbu_size

        oapvd_info_pbu(pbu_buf, pbu_size, &pbu_info)

        if(pbu_info.pbu_type == OAPV_PBU_TYPE_PRIMARY_FRAME) {
            // query frame format and prepare an output buffer
            oapvd_info_frame(pbu_buf, pbu_size, &finfo)
            imgb = create_image_buffer(finfo.w, finfo.h, finfo.cs)

            bitb.addr = pbu_buf
            bitb.ssize = pbu_size
            oapvd_decode_frame(did, &bitb, imgb, &stat, 0, NULL)  // all tiles

            write_frame(output, imgb)
        }
        else if(pbu_info.pbu_type == OAPV_PBU_TYPE_AU_INFO) {
            oapvd_decode_auinfo(did, &bitb, &aui)
        }
        // handle other PBU types (metadata, non-primary frames, ...) as needed
    }
}
```

### Tile-based partial decoding

With API set 1, `oapvd_decode_frame()` can decode only a subset of tiles.
The tile layout of a frame is available from `oapvd_info_frame()` and
`oapvd_info_tile()`:

```
oapvd_info_frame(pbu_buf, pbu_size, &finfo)
// finfo.num_tiles, finfo.tile_cols/tile_rows describe the tile grid

// query the position of every tile, if needed
pos = malloc(finfo.num_tiles * sizeof(oapv_tile_pos_t))
num = finfo.num_tiles
oapvd_info_tile(pbu_buf, pbu_size, pos, &num)

// oapvd_info_tile() can report the count on its own as well, by passing a
// NULL tile array:  num = 0; oapvd_info_tile(pbu_buf, pbu_size, NULL, &num)

// select the tiles to decode, e.g. the ones covering a viewport
part_tile_idxs = { 3, 4, 7, 8 }
num_part_tiles = 4

imgb = create_image_buffer(finfo.w, finfo.h, finfo.cs)
oapvd_decode_frame(did, &bitb, imgb, &stat, num_part_tiles, part_tile_idxs)
// only the selected tile regions of imgb are filled
```

Passing `0, NULL` decodes every tile. The regions of unselected tiles are
left untouched, so clear or reuse the image buffer accordingly.

When the frame header carries the tile sizes (the encoder writes them by
default; see `OAPV_CFG_SET_TILE_SIZE_IN_FH`), `oapvd_info_tile()` also
reports where each tile lives in the PBU, so an application can seek to a
tile instead of scanning the whole PBU:

```
oapvd_info_tile(pbu_buf, pbu_size, pos, &num)

// pos[i].offset  byte offset of the tile unit from the start of the PBU
// pos[i].size    byte size of the tile data
// the tile unit spans 4 + size bytes: a 4-byte size field, then the data

read(input, tile_buf, 4 + pos[i].size, at pos[i].offset)  // one tile only
```

Both fields are zero when the frame header does not carry the tile sizes, so
a zero `size` means the location is unknown and the PBU has to be walked
sequentially. This pairs with a memory-mapped input: only the pages of the
tiles that are actually decoded are touched.

Whether the locations are available depends on the bitstream, so treat them
as optional:

- `tile_size_present_in_fh_flag` is a per-frame choice of the encoder. This
  encoder sets it by default and it can be turned off per frame with
  `OAPV_CFG_SET_TILE_SIZE_IN_FH`, and a stream from another encoder may not
  carry the sizes at all. Check `size` before relying on a location, and
  keep the sequential path for streams that report zero.
- The values are relative to the start of the frame PBU, not to the file or
  the access unit. An application that reads from a file adds the position
  where the PBU begins, which is after the 4-byte `au_size`, the `aPv1`
  signature, and the 4-byte `pbu_size` of that PBU.
- They describe only the tile data of that frame PBU. Metadata PBUs and
  other frames of the same access unit are separate PBUs, so their contents
  are not covered by these values.
- The tile order is the raster scan order of the tile grid, the same order
  as `idx`, and the tile units are contiguous, so the end of the last tile
  coincides with the end of the PBU.
- The values come from the bitstream, and the decoder rejects a frame whose
  tile sizes do not fit within the PBU, so a successful call reports
  locations that lie inside the PBU. Beyond that, an application that
  forwards them to its I/O layer should still bound-check against the size
  of the buffer or mapping it actually holds.

## Encoding RGB content

The 444 profiles do not prescribe a color space: the three planes are just
components 0/1/2, and the color space interpretation is carried by the color
description fields of the frame header. RGB content is therefore coded with a
444 profile plus an identity matrix signal — the same convention HEVC, VP9,
and AV1 use, which is why no separate RGB profile exists.

| field | value for RGB | meaning |
|---|---|---|
| `matrix_coefficients` | 0 | identity matrix, no YCbCr conversion |
| `color_primaries` | per content (e.g. sRGB/BT.709 = 1, BT.2020 = 9) | primaries |
| `transfer_characteristics` | per content (e.g. sRGB = 13, PQ = 16) | transfer function |
| `full_range_flag` | usually 1 | RGB is normally full range |

The plane order follows the ITU-T H.273 convention: G in component 0, B in
component 1, R in component 2 (the same order as ffmpeg's `gbrp` formats).

To encode RGB, feed the G/B/R planes as a 444 image and signal the color
description through the encoding parameters:

```
// image buffer: component 0 = G, 1 = B, 2 = R
imgb->cs = OAPV_CS_SET(OAPV_CF_YCBCR444, 10, 0)

oapve_param_default(&param)
param.profile_idc = OAPV_PROFILE_444_10
param.color_description_present_flag = 1
param.color_primaries = 1           // e.g. sRGB / BT.709 primaries
param.transfer_characteristics = 13 // e.g. sRGB transfer
param.matrix_coefficients = 0       // identity, no YCbCr conversion
param.full_range_flag = 1
```

The reference encoder exposes the same controls; see the encoding examples
in the README.

On the decoding side, a 444 stream with `color_description_present_flag` set
and `matrix_coefficients == 0` (reported by `oapvd_info()` and in
`oapvd_stat_t`) identifies RGB content; the decoded planes are G, B, and R
as coded, with no conversion applied.

## Zero-copy decoding input with memory-mapped files

The decoder never writes to the bitstream buffer: `oapv_bitb_t.addr` is
caller-owned memory that is only read. So instead of allocating a buffer
and copying access units into it, the application can map the input file
into memory and pass pointers into the mapping directly. No allocation, no
copy — the OS page cache performs demand paging and read-ahead.

POSIX:

```
fd = open(path, O_RDONLY)
fstat(fd, &st)
base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)
madvise(base, st.st_size, MADV_SEQUENTIAL)   // read-ahead hint

off = 0
while(off + 4 <= st.st_size) {
    au_size = read_u32_be(base + off)        // AU framing, straight from the map
    bitb.addr = base + off + 4               // no copy
    bitb.ssize = au_size
    oapvd_decode(did, &bitb, &ofrms, mid, &stat)
    off += 4 + au_size
}

munmap(base, st.st_size)
close(fd)
```

Windows:

```
hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                   OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)
GetFileSizeEx(hFile, &size)
hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)

// same AU walk as above using base and size

UnmapViewOfFile(base)
CloseHandle(hMap)
CloseHandle(hFile)
```

The same applies to PBU-based decoding (API set 1): each `pbu_size` is read
from the map and `bitb.addr` points at the PBU bytes in place, which also
suits tile-based partial decoding, since only the pages of the tiles that
are actually decoded get touched.

Notes:

- Map read-only (`PROT_READ` / `PAGE_READONLY`); the decoder requires no
  write access to the input
- Keep a read-based fallback for inputs that cannot be mapped, such as
  pipes or standard input
- If another process truncates the file while it is mapped, accessing the
  removed pages raises SIGBUS on POSIX systems; map files that are stable
  during decoding

## Custom memory allocator

By default the library allocates with the standard C library. An application
can instead supply its own allocator **per instance** through the
`ops_mem` field of the creation descriptor (`oapve_cdesc_t` / `oapvd_cdesc_t` /
`oapvm_cdesc_t`). This is useful for integrating with a host memory manager
(e.g. a game-engine allocator) or for memory tracking. There is no global
allocator state.

The interface is `oapv_ops_mem_t`:

```
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

```
my_malloc(udata, size)         { return heap_alloc(udata, size) }
my_calloc(udata, count, size)  { return heap_zalloc(udata, count, size) }
my_realloc(udata, ptr, size)   { return heap_realloc(udata, ptr, size) }
my_free(udata, ptr)            { heap_free(udata, ptr) }

heap = create_my_heap()
ops = { OAPV_OPS_MAGIC_CODE_MEM,
        my_malloc, my_calloc, my_realloc, my_free, &heap }

cdesc = {0}                  // ops_mem defaults to NULL (libc)
cdesc.ops_mem = &ops         // opt in to the custom allocator
eid = oapve_create(&cdesc, &err)
```

The decoder and the metadata container take the same interface through
`oapvd_cdesc_t.ops_mem` and `oapvm_cdesc_t.ops_mem`.
