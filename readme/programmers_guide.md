# OpenAPV Programmer's Guide

This guide shows how to write encoding and decoding code with the OpenAPV
library (`liboapv`). The examples are pseudo code that follows the structure
of the sample applications under `app/`; refer to them for complete working
code. All types and functions are declared in `oapv.h`.

## Common types

- `oapv_imgb_t` — an image buffer holding the planes of one frame. The
  buffer is created and owned by the application; it carries `addref` /
  `release` callbacks for reference counting.
- `oapv_bitb_t` — a bitstream buffer. `addr` points to memory owned by the
  application and `bsize` is its capacity.
- `oapv_frms_t` — a set of frames making up one access unit (AU). Each
  entry pairs an image buffer with a `pbu_type` and a `group_id`.
- `oapvm_t` — a metadata container, created with `oapvm_create()`. It
  collects the metadata of an AU during encoding or decoding.

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

### PBU-based encoding (API set 1)

`oapve_encode()` writes a whole access unit in one call. The PBU-based APIs
instead let the application assemble the AU itself, mirroring the PBU-based
decoding API set: the application writes the raw AU framing (the 4-byte au
size and the `aPv1` signature) and appends PBUs one by one.

```
// [au_size(4)] [signature 'aPv1'(4)] [pbu_size+pbu()] [pbu_size+pbu()] ...
off = 4                                    // au size is backfilled at the end
buf[off..off+3] = 'aPv1', off += 4

// one frame -> one frame PBU; frm_idx selects the parameter/RC slot
bitb.addr = buf + off, bitb.bsize = cap - off
oapve_pbu_encode_frame(eid, &frm, frm_idx, mid, &bitb, &stat, &rfrm)
off += stat.write                          // frame hash is collected into mid

// metadata of a group -> one metadata PBU
bitb.addr = buf + off, bitb.bsize = cap - off
ret = oapve_pbu_encode_metadata(eid, mid, group_id, &bitb, &stat)
if(ret != OAPV_ERR_NOT_FOUND) off += stat.write

write_u32_be(buf, off - 4)                 // backfill the au size
write(output, buf, off)
oapvm_rem_all(mid)
```

An AU assembled this way is identical to the output of `oapve_encode()`
when the same frames and metadata are used in the same order. The reference
encoder application exposes this path with `--api-set 1`.

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

// select the tiles to decode, e.g. the ones covering a viewport
part_tile_idxs = { 3, 4, 7, 8 }
num_part_tiles = 4

imgb = create_image_buffer(finfo.w, finfo.h, finfo.cs)
oapvd_decode_frame(did, &bitb, imgb, &stat, num_part_tiles, part_tile_idxs)
// only the selected tile regions of imgb are filled
```

Passing `0, NULL` decodes every tile. The regions of unselected tiles are
left untouched, so clear or reuse the image buffer accordingly.

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
