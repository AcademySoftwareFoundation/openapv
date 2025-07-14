#pragma pattern for Advanced Professional Video (*.apv)

#pragma magic [ 61 50 76 31 ] @ 0x04
#pragma endian big

import std.io;
import std.mem;

/* PBU types */
enum PbuType : u8 {
    FRM_PRI = 1,
    FRM_NONPRI = 2,
    FRM_PREVIEW = 25,
    FRM_DEPTH = 26,
    FRM_ALPHA = 27,
    AUI = 65,
    METADATA = 66,
    FILLER = 67
};

fn get_0xff_ext_var(auto addr) {
    u32 read = 1;
    u32 var = 0;
    u8 ext = std::mem::read_unsigned(addr, 1);

    while (ext == 0xFF) {
        var += 0xFF;
        ext = std::mem::read_unsigned(addr + read, 1);
        read += 1;
    }
    var += ext;
    return var;
};

fn get_0xff_ext_var_bytes(auto addr) {
    u32 read = 1;
    u8 ext = std::mem::read_unsigned(addr, 1);

    while (ext == 0xFF) {
        ext = std::mem::read_unsigned(addr + read, 1);
        read += 1;
    }
    return read;
};

fn get_num_comp(auto chroma_format_idc) {
    u32 nc = 0;

    if(chroma_format_idc == 0) nc = 1;
    else if(chroma_format_idc == 4) nc = 4;
    else nc = 3;

    return nc;
};

struct PbuBase {

    u32 read = 0;
    str ptype_str = "";

    /*
    syntax code                                                   | type
    --------------------------------------------------------------|-----
    pbu_header(){                                                 |
        pbu_type                                                  | u(8)
        group_id                                                  | u(16)
        reserved_zero_8bits                                       | u(8)
    }
    */

    u32 pbu_size; // originally, this syntax is part of AccessUnit
    u8 pbu_type;
    u16 group_id;
    u8 reserved_zero_8bits;
    read += 4;
};

/*
syntax code                                                   | type
--------------------------------------------------------------|-----
frame_info(){                                                 |
  profile_idc                                                 | u(8)
  level_idc                                                   | u(8)
  band_idc                                                    | u(3)
  reserved_zero_5bits                                         | u(5)
  frame_width                                                 | u(24)
  frame_height                                                | u(24)
  chroma_format_idc                                           | u(4)
  bit_depth_minus8                                            | u(4)
  capture_time_distance                                       | u(8)
  reserved_zero_8bits                                         | u(8)
}
*/

bitfield FrmInfo {
    profile_idc : 8;
    level_idc : 8;
    band_idc : 3;
    reserved_zero_5bits: 5;
    frame_width: 24;
    frame_height: 24;
    chroma_format_idc: 4;
    bit_depth_minus8: 4;
    capture_time_distance: 8;
    reserved_zero_8bits: 8;
};

bitfield ColorInfo {
    u32 readbits = 25;

    color_primaries : 8;
    transfer_characteristics : 8;
    matrix_coefficients : 8;
    bool full_range_flag : 1;
};

bitfield FrmHeader {
    u32 readbits = 0;
    u32 ncomps = 0;

    FrmInfo finfo [[name("frame_info()")]];
    readbits += (12 * 8);

    ncomps = get_num_comp(finfo.chroma_format_idc);
    std::print("    NumComps = {:d}", ncomps);

    reserved_zero_8bits : 8;
    readbits += 8;

    bool color_description_present_flag : 1;
    readbits += 1;
    if(color_description_present_flag) {
        ColorInfo color_description;
        readbits += color_description.readbits;
    }
    bool use_q_matrix : 1;
    readbits += 1;
    if(use_q_matrix) {
        // need to implement
        std::print("    ERR: not implemented yet!!!\n");
    }
};

struct PbuFrm:PbuBase {
    u32 frmh_bits = 0;
    u32 NumComps = 0;
    u32 NumTiles = 0;

    if(pbu_type == PbuType::FRM_PRI) ptype_str = "Frm(Pri)";
    else if(pbu_type == PbuType::FRM_NONPRI) ptype_str = "Frm(Nonpri)";
    else if(pbu_type == PbuType::FRM_PREVIEW) ptype_str = "Frm(Preview)";
    else if(pbu_type == PbuType::FRM_DEPTH) ptype_str = "Frm(Depth)";
    else if(pbu_type == PbuType::FRM_ALPHA) ptype_str = "Frm(Alpha)";
    else ptype_str = "Frm(Unknown)";

    FrmHeader fh [[name("frame_header()")]];

    read += (fh.readbits + 7) >> 3;

    u8 frameData[pbu_size - read] [[sealed]];
};

u32 metadata_payload_count = 0;

struct MetadataPayload {
    str ptype_str = "";
    u32 read = 0;

    u32 payloadType = get_0xff_ext_var($) [[export]];
    read += get_0xff_ext_var_bytes($);
    $ += get_0xff_ext_var_bytes($); // update current reading point

    u32 payloadSize = get_0xff_ext_var($) [[export]];
    read += get_0xff_ext_var_bytes($);
    $ += get_0xff_ext_var_bytes($); // update current reading point

    u64 endOffset = $ + payloadSize;

    if (payloadType == 4) ptype_str = "itu_t_t35";
    else if (payloadType == 5) ptype_str = "mdcv";
    else if (payloadType == 6) ptype_str = "cll";
    else if (payloadType == 10) ptype_str = "filler";
    else if (payloadType == 170) ptype_str = "user_defined";
    else ptype_str = "undefined";

    std::print("    metadata payload[{:d}] type = {:d}({}), size = {:d}", metadata_payload_count, payloadType, ptype_str, payloadSize);

    u8 payloadData[while($ < endOffset)] [[sealed]];

    metadata_payload_count += 1;

} [[name(std::format("MetadataPayload[{}]:{}", (metadata_payload_count - 1), ptype_str))]];

struct PbuMetadata:PbuBase {
    u64 endOffset = 0;
    ptype_str = "Metadata";
    metadata_payload_count = 0; // reset number of metadata payload

    u32 metadata_size; // syntax

    endOffset = $ + metadata_size;

    MetadataPayload pay[while($ < endOffset)] [[inline]];
};

struct PbuAui:PbuBase {
    ptype_str = "aui";
    u8 data[pbu_size - read] [[sealed]];
};

struct PbuFiller:PbuBase {
    ptype_str = "filler";
    u8 data[pbu_size - read] [[sealed]];
};

struct PbuUnknown:PbuBase {
    std::warning(std::format("Unknown PBU type ({})!!!", pbu_type));
    ptype_str = "unknown";
    u8 data[pbu_size - read] [[sealed]];
};

u32 pbu_count = 0;

struct PBU {
    u32 pbu_size = std::mem::read_unsigned($, 4, std::mem::Endian::Big);
    u8 pbu_type = std::mem::read_unsigned($ + 4, 1, std::mem::Endian::Big);

    match (pbu_type) {
        (PbuType::FRM_PRI) : PbuFrm Pbu [[inline]];
        (PbuType::FRM_NONPRI) :  PbuFrm Pbu [[inline]];
        (PbuType::FRM_PREVIEW): PbuFrm Pbu [[inline]];
        (PbuType::FRM_DEPTH):  PbuFrm Pbu [[inline]];
        (PbuType::FRM_ALPHA): PbuFrm Pbu [[inline]];
        (PbuType::AUI): PbuAui Pbu [[inline]];
        (PbuType::METADATA): PbuMetadata Pbu [[inline]];
        (PbuType::FILLER): PbuFiller Pbu [[inline]];
        (_) : PbuUnknown Pbu [[inline]];
    }

    std::print("  PBU[{:d}] size = {:d}, {}", pbu_count, pbu_size, Pbu.ptype_str);

    pbu_count += 1;

} [[name(std::format("PBU[{}]:{}", (pbu_count - 1), Pbu.ptype_str))]];


u32 au_count = 0;

struct AccessUnit {
    u64 au_end = 0;

    u32 au_size; // originally this syntax is part of RawBitstream

    std::print("AU[{:d}] size = {:d}", au_count, au_size);

    au_end = $ + au_size;

    pbu_count = 0; // reset number of PBU

    char signature[4]; // 'aPv1'
    PBU pbu[while($ < au_end)] [[inline]];
};

u32 raw_count = 0;

struct RawBitstream {
    AccessUnit AU [[name(std::format("AU[{}]", raw_count))]];
    raw_count += 1;
} [[name(std::format("Raw[{}]", (raw_count - 1)))]];

struct ApvBitstream {
    RawBitstream Raw [[inline]];
}[[inline]];

ApvBitstream APV[while(!std::mem::eof())] @ 0x0 [[inline]];
