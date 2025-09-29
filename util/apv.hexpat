#pragma pattern for Advanced Professional Video (*.apv)

#pragma magic [ 61 50 76 31 ] @ 0x04
#pragma endian big

import std.io;
import std.mem;
import type.guid;

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

fn get_num_tiles (auto tile_w_mb, auto tile_h_mb, auto w, auto h) {
    u32 startMb = 0;
    u32 w_mb = 0;
    u32 h_mb = 0;
    u32 TileCols = 0;
    u32 TileRows = 0;

    w_mb = (w + 15) >> 4;
    h_mb = (h + 15) >> 4;

    TileCols = 0;
    startMb = 0;
    while(startMb < w_mb){
        startMb += tile_w_mb;
        TileCols += 1;
    }
    TileRows = 0;
    startMb = 0;
    while(startMb < h_mb){
        startMb += tile_h_mb;
        TileRows += 1;
    }
    return (TileCols * TileRows);
};

bitfield FrmHeader {
    u32 readbits = 0;

    FrmInfo finfo [[name("frame_info()")]];
    readbits += (12 * 8);
    reserved_zero_8bits_0 : 8;
    readbits += 8;

    u8 NumComps = get_num_comp(finfo.chroma_format_idc) [[export]];

    bool color_description_present_flag : 1;
    readbits += 1;
    if(color_description_present_flag) {
        ColorInfo color_description;
        readbits += color_description.readbits;
    }
    bool use_q_matrix : 1;
    readbits += 1;
    if(use_q_matrix) { // quantization_matrix()
        qmatrix0_00 : 8; qmatrix0_01 : 8; qmatrix0_02 : 8; qmatrix0_03 : 8; qmatrix0_04 : 8; qmatrix0_05 : 8; qmatrix0_06 : 8; qmatrix0_07 : 8;
        qmatrix0_08 : 8; qmatrix0_09 : 8; qmatrix0_10 : 8; qmatrix0_11 : 8; qmatrix0_12 : 8; qmatrix0_13 : 8; qmatrix0_14 : 8; qmatrix0_15 : 8;
        qmatrix0_16 : 8; qmatrix0_17 : 8; qmatrix0_18 : 8; qmatrix0_19 : 8; qmatrix0_20 : 8; qmatrix0_21 : 8; qmatrix0_22 : 8; qmatrix0_23 : 8;
        qmatrix0_24 : 8; qmatrix0_25 : 8; qmatrix0_26 : 8; qmatrix0_27 : 8; qmatrix0_28 : 8; qmatrix0_29 : 8; qmatrix0_30 : 8; qmatrix0_31 : 8;
        qmatrix0_32 : 8; qmatrix0_33 : 8; qmatrix0_34 : 8; qmatrix0_35 : 8; qmatrix0_36 : 8; qmatrix0_37 : 8; qmatrix0_38 : 8; qmatrix0_39 : 8;
        qmatrix0_40 : 8; qmatrix0_41 : 8; qmatrix0_42 : 8; qmatrix0_43 : 8; qmatrix0_44 : 8; qmatrix0_45 : 8; qmatrix0_46 : 8; qmatrix0_47 : 8;
        qmatrix0_48 : 8; qmatrix0_49 : 8; qmatrix0_50 : 8; qmatrix0_51 : 8; qmatrix0_52 : 8; qmatrix0_53 : 8; qmatrix0_54 : 8; qmatrix0_55 : 8;
        qmatrix0_56 : 8; qmatrix0_57 : 8; qmatrix0_58 : 8; qmatrix0_59 : 8; qmatrix0_60 : 8; qmatrix0_61 : 8; qmatrix0_62 : 8; qmatrix0_63 : 8;

        if(NumComps > 1) {
        qmatrix1_00 : 8; qmatrix1_01 : 8; qmatrix1_02 : 8; qmatrix1_03 : 8; qmatrix1_04 : 8; qmatrix1_05 : 8; qmatrix1_06 : 8; qmatrix1_07 : 8;
        qmatrix1_08 : 8; qmatrix1_09 : 8; qmatrix1_10 : 8; qmatrix1_11 : 8; qmatrix1_12 : 8; qmatrix1_13 : 8; qmatrix1_14 : 8; qmatrix1_15 : 8;
        qmatrix1_16 : 8; qmatrix1_17 : 8; qmatrix1_18 : 8; qmatrix1_19 : 8; qmatrix1_20 : 8; qmatrix1_21 : 8; qmatrix1_22 : 8; qmatrix1_23 : 8;
        qmatrix1_24 : 8; qmatrix1_25 : 8; qmatrix1_26 : 8; qmatrix1_27 : 8; qmatrix1_28 : 8; qmatrix1_29 : 8; qmatrix1_30 : 8; qmatrix1_31 : 8;
        qmatrix1_32 : 8; qmatrix1_33 : 8; qmatrix1_34 : 8; qmatrix1_35 : 8; qmatrix1_36 : 8; qmatrix1_37 : 8; qmatrix1_38 : 8; qmatrix1_39 : 8;
        qmatrix1_40 : 8; qmatrix1_41 : 8; qmatrix1_42 : 8; qmatrix1_43 : 8; qmatrix1_44 : 8; qmatrix1_45 : 8; qmatrix1_46 : 8; qmatrix1_47 : 8;
        qmatrix1_48 : 8; qmatrix1_49 : 8; qmatrix1_50 : 8; qmatrix1_51 : 8; qmatrix1_52 : 8; qmatrix1_53 : 8; qmatrix1_54 : 8; qmatrix1_55 : 8;
        qmatrix1_56 : 8; qmatrix1_57 : 8; qmatrix1_58 : 8; qmatrix1_59 : 8; qmatrix1_60 : 8; qmatrix1_61 : 8; qmatrix1_62 : 8; qmatrix1_63 : 8;
        }
        if(NumComps > 2) {
        qmatrix2_00 : 8; qmatrix2_01 : 8; qmatrix2_02 : 8; qmatrix2_03 : 8; qmatrix2_04 : 8; qmatrix2_05 : 8; qmatrix2_06 : 8; qmatrix2_07 : 8;
        qmatrix2_08 : 8; qmatrix2_09 : 8; qmatrix2_10 : 8; qmatrix2_11 : 8; qmatrix2_12 : 8; qmatrix2_13 : 8; qmatrix2_14 : 8; qmatrix2_15 : 8;
        qmatrix2_16 : 8; qmatrix2_17 : 8; qmatrix2_18 : 8; qmatrix2_19 : 8; qmatrix2_20 : 8; qmatrix2_21 : 8; qmatrix2_22 : 8; qmatrix2_23 : 8;
        qmatrix2_24 : 8; qmatrix2_25 : 8; qmatrix2_26 : 8; qmatrix2_27 : 8; qmatrix2_28 : 8; qmatrix2_29 : 8; qmatrix2_30 : 8; qmatrix2_31 : 8;
        qmatrix2_32 : 8; qmatrix2_33 : 8; qmatrix2_34 : 8; qmatrix2_35 : 8; qmatrix2_36 : 8; qmatrix2_37 : 8; qmatrix2_38 : 8; qmatrix2_39 : 8;
        qmatrix2_40 : 8; qmatrix2_41 : 8; qmatrix2_42 : 8; qmatrix2_43 : 8; qmatrix2_44 : 8; qmatrix2_45 : 8; qmatrix2_46 : 8; qmatrix2_47 : 8;
        qmatrix2_48 : 8; qmatrix2_49 : 8; qmatrix2_50 : 8; qmatrix2_51 : 8; qmatrix2_52 : 8; qmatrix2_53 : 8; qmatrix2_54 : 8; qmatrix2_55 : 8;
        qmatrix2_56 : 8; qmatrix2_57 : 8; qmatrix2_58 : 8; qmatrix2_59 : 8; qmatrix2_60 : 8; qmatrix2_61 : 8; qmatrix2_62 : 8; qmatrix2_63 : 8;
        }
        if(NumComps > 3) {
        qmatrix3_00 : 8; qmatrix3_01 : 8; qmatrix3_02 : 8; qmatrix3_03 : 8; qmatrix3_04 : 8; qmatrix3_05 : 8; qmatrix3_06 : 8; qmatrix3_07 : 8;
        qmatrix3_08 : 8; qmatrix3_09 : 8; qmatrix3_10 : 8; qmatrix3_11 : 8; qmatrix3_12 : 8; qmatrix3_13 : 8; qmatrix3_14 : 8; qmatrix3_15 : 8;
        qmatrix3_16 : 8; qmatrix3_17 : 8; qmatrix3_18 : 8; qmatrix3_19 : 8; qmatrix3_20 : 8; qmatrix3_21 : 8; qmatrix3_22 : 8; qmatrix3_23 : 8;
        qmatrix3_24 : 8; qmatrix3_25 : 8; qmatrix3_26 : 8; qmatrix3_27 : 8; qmatrix3_28 : 8; qmatrix3_29 : 8; qmatrix3_30 : 8; qmatrix3_31 : 8;
        qmatrix3_32 : 8; qmatrix3_33 : 8; qmatrix3_34 : 8; qmatrix3_35 : 8; qmatrix3_36 : 8; qmatrix3_37 : 8; qmatrix3_38 : 8; qmatrix3_39 : 8;
        qmatrix3_40 : 8; qmatrix3_41 : 8; qmatrix3_42 : 8; qmatrix3_43 : 8; qmatrix3_44 : 8; qmatrix3_45 : 8; qmatrix3_46 : 8; qmatrix3_47 : 8;
        qmatrix3_48 : 8; qmatrix3_49 : 8; qmatrix3_50 : 8; qmatrix3_51 : 8; qmatrix3_52 : 8; qmatrix3_53 : 8; qmatrix3_54 : 8; qmatrix3_55 : 8;
        qmatrix3_56 : 8; qmatrix3_57 : 8; qmatrix3_58 : 8; qmatrix3_59 : 8; qmatrix3_60 : 8; qmatrix3_61 : 8; qmatrix3_62 : 8; qmatrix3_63 : 8;
        }
        readbits += (NumComps * 8 * 8) * 8;
    }
    // tile_info()
    tile_width_in_mbs  : 20;
    tile_height_in_mbs : 20;
    bool tile_size_present_in_fh_flag : 1;
    readbits += 41;

    u32 NumTiles = get_num_tiles(tile_width_in_mbs, tile_height_in_mbs, finfo.frame_width, finfo.frame_height) [[export]];
    if(tile_size_present_in_fh_flag) {
        tile_size_in_fh : 32 * NumTiles;
        readbits += 32 * NumTiles;
    }

	reserved_zero_8bits_1 : 8;
	readbits += 8;

	// byte_alignment(): 'bitfield' will align to byte at the end, automatically
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

/* Metadata payload types */
enum MetadataPayloadType : u8 {
    ITU_T_T35 = 4,
    MDCV = 5,
    CLL = 6,
    FILLER = 10,
    USER_DEFINED = 170
};

struct MetadataPayload_Base {
    str mtype_str = "";
    u32 read = 0;

    u32 payloadType = get_0xff_ext_var($) [[export]];
    read += get_0xff_ext_var_bytes($);
    $ += get_0xff_ext_var_bytes($); // update current reading point

    u32 payloadSize = get_0xff_ext_var($) [[export]];
    read += get_0xff_ext_var_bytes($);
    $ += get_0xff_ext_var_bytes($); // update current reading point

    u64 endOffset = $ + payloadSize;
};

struct MetadataPayload_T35:MetadataPayload_Base {
    mtype_str ="ITU-T T.35";
    u8 itu_t_t35_country_code;
    if (itu_t_t35_country_code == 0xFF) {
        u8 itu_t_t35_country_code_extension;
        u8 itu_t_t35_payload[payloadSize - 2][[sealed]];
    } else {
        u8 itu_t_t35_payload[payloadSize - 1][[sealed]];
    }
};

struct MetadataPayload_Mdcv:MetadataPayload_Base {
    mtype_str ="MDCV";
    u16 primary_chromaticity_x0;
    u16 primary_chromaticity_y0;
    u16 primary_chromaticity_x1;
    u16 primary_chromaticity_y1;
    u16 primary_chromaticity_x2;
    u16 primary_chromaticity_y2;
    u16 white_point_chromaticity_x;
    u16 white_point_chromaticity_y;
    u32 max_mastering_luminance;
    u32 min_mastering_luminance;
};

struct MetadataPayload_Cll:MetadataPayload_Base {
    mtype_str ="CLL";
    u16 max_cll;
    u16 max_fall;
};

struct MetadataPayload_Filler:MetadataPayload_Base {
    mtype_str ="Filler";
    u8 ff_byte[payloadSize] [[sealed]];
};

struct MetadataPayload_Userdefined:MetadataPayload_Base {
    mtype_str ="User defined";
    type::GUID uuid;
    u8 user_defined_data_payload[payloadSize - 16] [[sealed]];
};

struct MetadataPayload_Undefined:MetadataPayload_Base {
    mtype_str ="Undefined";
    u8 payloadData[payloadSize] [[sealed]];
};

u32 metadata_payload_count = 0;

struct MetadataPayload {
    u32 payloadType = get_0xff_ext_var($); // peek payloadType

    match (payloadType) {
        (MetadataPayloadType::ITU_T_T35) : MetadataPayload_T35 Pay [[inline]];
        (MetadataPayloadType::MDCV) :  MetadataPayload_Mdcv Pay [[inline]];
        (MetadataPayloadType::CLL): MetadataPayload_Cll Pay [[inline]];
        (MetadataPayloadType::FILLER):  MetadataPayload_Filler Pay [[inline]];
        (MetadataPayloadType::USER_DEFINED):  MetadataPayload_Userdefined Pay [[inline]];
        (_) : MetadataPayload_Undefined Pay [[inline]];
    }
    // check read size
    if($ != Pay.endOffset) {
        std::warning("Mismatch b/w Metadata payloadSize and actual read size");
    }

    std::print("    metadata payload[{:d}] type = {:d} ({}), size = {:d}", metadata_payload_count, Pay.payloadType, Pay.mtype_str, Pay.payloadSize);
    metadata_payload_count += 1;

} [[name(std::format("Payload[{}]:{}", (metadata_payload_count - 1), Pay.mtype_str))]];

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
