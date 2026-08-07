# APV Profile Extensions

This document defines additional APV profiles which are specified by the
OpenAPV project on top of [RFC 9924](https://www.rfc-editor.org/rfc/rfc9924.html).

These profiles are **not** part of RFC 9924. Their `profile_idc` values are
taken from the value space that RFC 9924 reserves for future use, so a
bitstream using one of these profiles does not conform to RFC 9924 and can
only be decoded by implementations that support the corresponding extension.
For the profiles defined by RFC 9924 (422-10, 422-12, 444-10, 444-12,
4444-10, 4444-12, and 400-10), see [Section 9.3 of the RFC](https://www.rfc-editor.org/rfc/rfc9924.html#section-9.3).

## Defined extension profiles

| Profile          | profile_idc | chroma_format_idc | bit_depth_minus8 | Description                        |
|------------------|-------------|-------------------|------------------|------------------------------------|
| 422-10-UNCONST   | 43          | 2                 | 2                | Relaxed conformance constraints    |
| 422-12-UNCONST   | 54          | 2                 | 2 to 4           | Relaxed conformance constraints    |
| 444-10-UNCONST   | 65          | 2 to 3            | 2                | Relaxed conformance constraints    |
| 444-12-UNCONST   | 76          | 2 to 3            | 2 to 4           | Relaxed conformance constraints    |
| 4444-10-UNCONST  | 87          | 2 to 4            | 2                | Relaxed conformance constraints    |
| 4444-12-UNCONST  | 98          | 2 to 4            | 2 to 4           | Relaxed conformance constraints    |
| 400-10-UNCONST   | 109         | 0                 | 2                | Relaxed conformance constraints    |
| 444-16C12        | 140         | 3                 | 4                | 16-bit source companded to 12-bit  |
| 4444-16C12       | 144         | 3 or 4            | 4                | 16-bit source companded to 12-bit  |

## UNCONST profiles

The UNCONST profiles relax conformance constraints of the corresponding
RFC 9924 profiles. Conformance of a coded frame to an UNCONST profile is
indicated by the `profile_idc` value in the table above.

Each UNCONST profile follows all constraints of its corresponding RFC 9924
profile as specified in Sections 9.3.2 to 9.3.8 of the RFC (see the
`chroma_format_idc` and `bit_depth_minus8` columns above; `pbu_type` MUST be
equal to 1), except for the constraints relaxed below. Further relaxations
may be added to this section in the future.

The tile constraints of Section 9.4.1 of RFC 9924 do not apply. Instead,
the following applies:

- The value of `tile_width_in_mbs` MUST be greater than or equal to 1.
- The value of `tile_height_in_mbs` MUST be greater than or equal to 1.
- There is no limit on the values of TileCols and TileRows beyond what the
  syntax can express.

In other words, the minimum tile size is one MB (16x16 luma samples), and a
frame may be partitioned into as many tiles as its dimensions allow. All
other constraints, including the levels and bands constraints of Section 9.4
(luma sample rate and coded data rate), apply as specified in RFC 9924.

For example, a 16K (15360x8640) frame can be coded with 256x256 pixel tiles
(60x34 tiles), which the 20x20 tile limit of RFC 9924 does not allow.

## 444-16C12 and 4444-16C12 profiles

Conformance of a coded frame to the 444-16C12 or 4444-16C12 profile is
indicated by `profile_idc` equal to 140 or 144, respectively.

These profiles carry 16-bit source video in a 12-bit coded representation:
the encoder compands each 16-bit sample to 12 bits before coding, and the
decoder expands the reconstructed samples back to 16 bits. Companding is
signaled by the `use_companding` flag in the frame info, which occupies the
first bit of the `reserved_zero_8bits` field of RFC 9924 and MUST be 0 for
all other profiles.

Coded frames conforming to these profiles MUST obey the following
constraints:

- 444-16C12: `chroma_format_idc` MUST be equal to 3.
- 4444-16C12: `chroma_format_idc` MUST be equal to 3 or 4.
- `bit_depth_minus8` MUST be equal to 4.
- `pbu_type` MUST be equal to 1.

The levels and bands constraints of Section 9.4 of RFC 9924 apply.
