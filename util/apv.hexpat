#pragma description Advanced Professional Video
#pragma pattern for Advanced Professional Video (*.apv)

#pragma magic [ 61 50 76 31 ] @ 0x04
#pragma endian big

import std.io;
import std.mem;
import std.string;
import type.guid;

/* set true for detailed console output */
bool verbose = false;

/* summary counters */
u32 pbu_total = 0;
u32 frm_total = 0;
u32 md_total = 0;
u32 md_t35 = 0;
u32 md_mdcv = 0;
u32 md_cll = 0;
u32 md_filler = 0;
u32 md_icc = 0;
u32 md_gainmap = 0;
u32 md_user = 0;
u32 md_undef = 0;
u32 raw_count = 0;
str frame_cfg_list = "";

/* frame info tracking, keyed by (pbu_type, group_id) */
u32 au_frm_idx = 0;
u32 slot_cnt = 0;
bool frm_slot_new = false;
bool slot_ovf = false;
u32 slot_gid[32] = { 0 };
u32 slot_type[32] = { 0 };
u32 slot_w[32] = { 0 };
u32 slot_h[32] = { 0 };
u32 slot_c[32] = { 0 };
u32 slot_b[32] = { 0 };
u32 slot_p[32] = { 0 };
u32 slot_u[32] = { 0 };

/* PBU types */
enum PbuType : u8 {
    FRM_PRI = 1,
    FRM_NONPRI = 2,
    FRM_PREVIEW = 25,
    FRM_DEPTH = 26,
    FRM_ALPHA = 27,
    FRM_GAINMAP = 28,
    AUI = 65,
    METADATA = 66,
    FILLER = 67
};

enum ProfileIdc : u8 {
    PROFILE_422_10 = 33,
    PROFILE_422_10_UNCONST = 43,
    PROFILE_422_12 = 44,
    PROFILE_422_12_UNCONST = 54,
    PROFILE_444_10 = 55,
    PROFILE_444_10_UNCONST = 65,
    PROFILE_444_12 = 66,
    PROFILE_444_12_UNCONST = 76,
    PROFILE_4444_10 = 77,
    PROFILE_4444_10_UNCONST = 87,
    PROFILE_4444_12 = 88,
    PROFILE_4444_12_UNCONST = 98,
    PROFILE_400_10 = 99,
    PROFILE_400_10_UNCONST = 109,
    PROFILE_444_16C12 = 140,
    PROFILE_4444_16C12 = 144
};

enum ChromaFormatIdc : u8 {
    MONO_400 = 0,
    CHROMA_422 = 2,
    CHROMA_444 = 3,
    CHROMA_4444 = 4
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

fn get_profile_str(auto idc) {
    if(idc == 33) return "422-10";
    if(idc == 43) return "422-10-UNCONST";
    if(idc == 44) return "422-12";
    if(idc == 54) return "422-12-UNCONST";
    if(idc == 55) return "444-10";
    if(idc == 65) return "444-10-UNCONST";
    if(idc == 66) return "444-12";
    if(idc == 76) return "444-12-UNCONST";
    if(idc == 77) return "4444-10";
    if(idc == 87) return "4444-10-UNCONST";
    if(idc == 88) return "4444-12";
    if(idc == 98) return "4444-12-UNCONST";
    if(idc == 99) return "400-10";
    if(idc == 109) return "400-10-UNCONST";
    if(idc == 140) return "444-16C12";
    if(idc == 144) return "4444-16C12";
    return "unknown";
};

fn get_chroma_str(auto idc) {
    if(idc == 0) return "4:0:0";
    if(idc == 2) return "4:2:2";
    if(idc == 3) return "4:4:4";
    if(idc == 4) return "4:4:4:4";
    return "unknown";
};

/* find or allocate a tracking slot for a (pbu_type, group_id) frame identity */
fn find_frame_slot(auto gid, auto ptype) {
    u32 i = 0;
    while(i < slot_cnt) {
        if(slot_gid[i] == gid && slot_type[i] == ptype) {
            frm_slot_new = false;
            return i;
        }
        i += 1;
    }
    if(slot_cnt < 32) {
        slot_gid[slot_cnt] = gid;
        slot_type[slot_cnt] = ptype;
        slot_cnt += 1;
        frm_slot_new = true;
        return slot_cnt - 1;
    }
    return 9999;
};

/* current frame context, shared with Tile */
u32 num_comps = 3;
u32 aui_frm_count = 0;

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

    u32 pbu_size; // originally, this syntax is part of AccessUnit; excludes its own 4 bytes
    PbuType pbu_type;
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
  use_companding                                              | u(1)
  reserved_zero_7bits                                         | u(7)
 }
*/

bitfield FrmInfo {
    ProfileIdc profile_idc : 8;
    level_idc : 8;
    band_idc : 3;
    reserved_zero_5bits: 5;
    frame_width: 24;
    frame_height: 24;
    ChromaFormatIdc chroma_format_idc : 4;
    bit_depth_minus8: 4;
    capture_time_distance: 8;
    bool use_companding : 1;
    reserved_zero_7bits: 7;

    if(use_companding && profile_idc != ProfileIdc::PROFILE_444_16C12 &&
       profile_idc != ProfileIdc::PROFILE_4444_16C12) {
        std::warning("use_companding must be 0 unless profile is 444-16C12 or 4444-16C12");
    }
};

bitfield ColorInfo {
    u32 readbits = 25;

    color_primaries : 8;
    transfer_characteristics : 8;
    matrix_coefficients : 8;
    bool full_range_flag : 1;
};

/* bitfield member size is capped at 255 bits, hence the array form */
bitfield TileSizeInFh {
    unsigned value : 32;
};

bitfield QMatrixComp {
    qmatrix_00 : 8; qmatrix_01 : 8; qmatrix_02 : 8; qmatrix_03 : 8; qmatrix_04 : 8; qmatrix_05 : 8; qmatrix_06 : 8; qmatrix_07 : 8;
    qmatrix_08 : 8; qmatrix_09 : 8; qmatrix_10 : 8; qmatrix_11 : 8; qmatrix_12 : 8; qmatrix_13 : 8; qmatrix_14 : 8; qmatrix_15 : 8;
    qmatrix_16 : 8; qmatrix_17 : 8; qmatrix_18 : 8; qmatrix_19 : 8; qmatrix_20 : 8; qmatrix_21 : 8; qmatrix_22 : 8; qmatrix_23 : 8;
    qmatrix_24 : 8; qmatrix_25 : 8; qmatrix_26 : 8; qmatrix_27 : 8; qmatrix_28 : 8; qmatrix_29 : 8; qmatrix_30 : 8; qmatrix_31 : 8;
    qmatrix_32 : 8; qmatrix_33 : 8; qmatrix_34 : 8; qmatrix_35 : 8; qmatrix_36 : 8; qmatrix_37 : 8; qmatrix_38 : 8; qmatrix_39 : 8;
    qmatrix_40 : 8; qmatrix_41 : 8; qmatrix_42 : 8; qmatrix_43 : 8; qmatrix_44 : 8; qmatrix_45 : 8; qmatrix_46 : 8; qmatrix_47 : 8;
    qmatrix_48 : 8; qmatrix_49 : 8; qmatrix_50 : 8; qmatrix_51 : 8; qmatrix_52 : 8; qmatrix_53 : 8; qmatrix_54 : 8; qmatrix_55 : 8;
    qmatrix_56 : 8; qmatrix_57 : 8; qmatrix_58 : 8; qmatrix_59 : 8; qmatrix_60 : 8; qmatrix_61 : 8; qmatrix_62 : 8; qmatrix_63 : 8;
};

fn get_num_tiles (auto tile_w_mb, auto tile_h_mb, auto w, auto h) {
    u32 startMb = 0;
    u32 w_mb = 0;
    u32 h_mb = 0;
    u32 TileCols = 0;
    u32 TileRows = 0;

    if(tile_w_mb == 0 || tile_h_mb == 0) return 0;

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
        QMatrixComp q_matrix[NumComps];
        readbits += (NumComps * 8 * 8) * 8;
    }
    // tile_info()
    tile_width_in_mbs  : 20;
    tile_height_in_mbs : 20;
    bool tile_size_present_in_fh_flag : 1;
    readbits += 41;

    u32 NumTiles = get_num_tiles(tile_width_in_mbs, tile_height_in_mbs, finfo.frame_width, finfo.frame_height) [[export]];
    if(tile_size_present_in_fh_flag) {
        TileSizeInFh tile_size_in_fh[NumTiles] [[name("tile_size_in_fh")]];
        readbits += 32 * NumTiles;
    }

	reserved_zero_8bits_1 : 8;
	readbits += 8;

	// byte_alignment(): 'bitfield' will align to byte at the end, automatically
};

struct Tile {
    u32 tile_size;
    u16 tile_header_size;
    u16 tile_index;
    u32 tile_data_size[num_comps];
    u8 tile_qp[num_comps];
    u8 reserved_zero_8bits;

    u32 th_fixed = 5 + num_comps * 5;

    if(tile_header_size < th_fixed) {
        std::warning(std::format("tile[{}] header size ({}) is smaller than expected ({})", tile_index, tile_header_size, th_fixed));
    }
    else if(tile_header_size > th_fixed) {
        u8 tile_header_extra[tile_header_size - th_fixed] [[sealed]];
    }

    // tile_size includes tile_header, component data and dummy bytes
    if(tile_size < tile_header_size) {
        std::warning(std::format("tile[{}] size ({}) is smaller than tile_header_size ({})", tile_index, tile_size, tile_header_size));
    }
    else {
        u8 tile_data[tile_size - tile_header_size] [[sealed]];
    }
} [[name(std::format("Tile[{}]", tile_index))]];

/*
FrmHeader is wrapped in an empty bitfield so that ImHex treats it as a
bitfield member: only members with a parent bitfield get a bit-exact size
display (e.g. "19 bytes, 3 bits"), while top-level bitfields are rounded
up to whole bytes. The wrapper is inlined so it does not show up in the
pattern tree.
*/
bitfield FrmHeaderWrap {
    FrmHeader fh [[name("frame_header()")]];
};

struct PbuFrm:PbuBase {
    frm_total += 1;

    if(pbu_type == PbuType::FRM_PRI) ptype_str = "Frm(Pri)";
    else if(pbu_type == PbuType::FRM_NONPRI) ptype_str = "Frm(Nonpri)";
    else if(pbu_type == PbuType::FRM_PREVIEW) ptype_str = "Frm(Preview)";
    else if(pbu_type == PbuType::FRM_DEPTH) ptype_str = "Frm(Depth)";
    else if(pbu_type == PbuType::FRM_ALPHA) ptype_str = "Frm(Alpha)";
    else if(pbu_type == PbuType::FRM_GAINMAP) ptype_str = "Frm(Gainmap)";
    else ptype_str = "Frm(Unknown)";

    FrmHeaderWrap fhw [[inline]];

    /* collect distinct frame configurations; brackets keep contains() exact */
    u32 w = fhw.fh.finfo.frame_width;
    u32 h = fhw.fh.finfo.frame_height;
    u32 c = u32(fhw.fh.finfo.chroma_format_idc);
    u32 bd = fhw.fh.finfo.bit_depth_minus8 + 8;
    u32 p = u32(fhw.fh.finfo.profile_idc);
    u32 uc = fhw.fh.finfo.use_companding;

    str cfg = std::format("[{}x{}, {}, {}-bit, {}", w, h, get_chroma_str(c), bd, get_profile_str(p));
    if(uc) {
        cfg += ", companded";
    }
    cfg += "]";
    if(!std::string::contains(frame_cfg_list, cfg)) {
        frame_cfg_list += cfg;
    }

    u32 si = find_frame_slot(group_id, u32(pbu_type));
    if(si == 9999) {
        if(!slot_ovf) {
            std::warning("too many distinct (pbu_type, group_id) frame identities; change tracking disabled");
            slot_ovf = true;
        }
    }
    else {
        if(frm_slot_new) {
            std::print("AU[{}] frame[{}] {} group {}: {}", raw_count, au_frm_idx, ptype_str, group_id, std::string::substr(cfg, 1, std::string::length(cfg) - 2));
        }
        else {
            str changes = "";
            if(slot_w[si] != w || slot_h[si] != h) {
                changes += std::format("resolution {}x{} -> {}x{}, ", slot_w[si], slot_h[si], w, h);
            }
            if(slot_c[si] != c) {
                changes += std::format("chroma format {} -> {}, ", get_chroma_str(slot_c[si]), get_chroma_str(c));
            }
            if(slot_b[si] != bd) {
                changes += std::format("bit depth {} -> {}, ", slot_b[si], bd);
            }
            if(slot_p[si] != p) {
                changes += std::format("profile {} -> {}, ", get_profile_str(slot_p[si]), get_profile_str(p));
            }
            if(slot_u[si] != uc) {
                changes += std::format("companding {} -> {}, ", slot_u[si], uc);
            }
            if(changes != "") {
                std::print("AU[{}] frame[{}] {} group {}: {}", raw_count, au_frm_idx, ptype_str, group_id, std::string::substr(changes, 0, std::string::length(changes) - 2));
            }
        }
        slot_w[si] = w;
        slot_h[si] = h;
        slot_c[si] = c;
        slot_b[si] = bd;
        slot_p[si] = p;
        slot_u[si] = uc;
    }
    au_frm_idx += 1;

    read += (fhw.fh.readbits + 7) >> 3;
    u64 pbu_end = $ + pbu_size - read;

    if(pbu_end < $) {
        std::warning(std::format("PBU[{}] pbu_size ({}) is smaller than its frame header", pbu_count, pbu_size));
    }
    else if(fhw.fh.NumTiles == 0) {
        std::warning(std::format("PBU[{}] invalid tile_info, skipping frame data", pbu_count));
        u8 frame_data[pbu_end - $] [[sealed]];
    }
    else {
        num_comps = fhw.fh.NumComps;
        Tile tile[fhw.fh.NumTiles] [[inline]];

        if($ < pbu_end) {
            u8 filler[pbu_end - $] [[sealed]];
        }
    }
};

struct AuInfoFrame {
    PbuType pbu_type;
    u16 group_id;
    u8 reserved_zero_8bits;
    FrmInfo finfo [[name("frame_info()")]];

    aui_frm_count += 1;
} [[name(std::format("frame[{}]", aui_frm_count - 1))]];

struct PbuAui:PbuBase {
    ptype_str = "aui";

    u64 pbu_end = $ + pbu_size - read;

    u16 num_frames;
    u32 frames_to_read = num_frames;
    // each entry: pbu_type(1) + group_id(2) + reserved(1) + frame_info(12)
    if($ + num_frames * 16 + 1 > pbu_end) {
        std::warning(std::format("au_info num_frames ({}) overruns pbu_size ({})", num_frames, pbu_size));
        frames_to_read = 0;
    }
    aui_frm_count = 0;
    AuInfoFrame frame[frames_to_read] [[inline]];
    u8 reserved_zero_8bits_aui;

    if($ < pbu_end) {
        u8 filler[pbu_end - $] [[sealed]];
    }
};

/* Metadata payload types */
enum MetadataPayloadType : u8 {
    ITU_T_T35 = 4,
    MDCV = 5,
    CLL = 6,
    FILLER = 10,
    ICC_PROFILE = 20,
    GAINMAP = 21,
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
    md_t35 += 1;
    if(payloadSize < 1) {
        std::warning("ITU-T T.35 payload too small");
    } else {
        u8 itu_t_t35_country_code;
        if (payloadSize >= 2 && itu_t_t35_country_code == 0xFF) {
            u8 itu_t_t35_country_code_extension;
            u8 itu_t_t35_payload[payloadSize - 2][[sealed]];
        } else {
            u8 itu_t_t35_payload[payloadSize - 1][[sealed]];
        }
    }
};

struct MetadataPayload_Mdcv:MetadataPayload_Base {
    mtype_str ="MDCV";
    md_mdcv += 1;
    if(payloadSize < 24) {
        std::warning("MDCV payload too small");
        u8 payload_data[payloadSize][[sealed]];
    } else {
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
    }
};

struct MetadataPayload_Cll:MetadataPayload_Base {
    mtype_str ="CLL";
    md_cll += 1;
    if(payloadSize < 4) {
        std::warning("CLL payload too small");
        u8 payload_data[payloadSize][[sealed]];
    } else {
        u16 max_cll;
        u16 max_fall;
    }
};

struct MetadataPayload_Filler:MetadataPayload_Base {
    mtype_str ="Filler";
    md_filler += 1;
    u8 ff_byte[payloadSize] [[sealed]];
};

struct MetadataPayload_IccProfile:MetadataPayload_Base {
    mtype_str ="ICC profile";
    md_icc += 1;
    u8 icc_profile_structure[payloadSize] [[sealed]];
};

struct MetadataPayload_Gainmap:MetadataPayload_Base {
    mtype_str ="Gainmap";
    md_gainmap += 1;
    u8 gainmap_metadata[payloadSize] [[sealed]];
};

struct MetadataPayload_Userdefined:MetadataPayload_Base {
    mtype_str ="User defined";
    md_user += 1;
    if(payloadSize < 16) {
        std::warning("user-defined payload too small for UUID");
        u8 user_defined_data_payload[payloadSize] [[sealed]];
    } else {
        type::GUID uuid;
        u8 user_defined_data_payload[payloadSize - 16] [[sealed]];
    }
};

struct MetadataPayload_Undefined:MetadataPayload_Base {
    mtype_str ="Undefined";
    md_undef += 1;
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
        (MetadataPayloadType::ICC_PROFILE): MetadataPayload_IccProfile Pay [[inline]];
        (MetadataPayloadType::GAINMAP): MetadataPayload_Gainmap Pay [[inline]];
        (MetadataPayloadType::USER_DEFINED): MetadataPayload_Userdefined Pay [[inline]];
        (_) : MetadataPayload_Undefined Pay [[inline]];
    }
    // consume any bytes the payload struct did not read
    if($ < Pay.endOffset) {
        u8 payload_remainder[Pay.endOffset - $] [[sealed]];
    }
    // check read size
    if($ != Pay.endOffset) {
        std::warning("Mismatch b/w Metadata payloadSize and actual read size");
    }

    if(verbose) {
        std::print("    metadata payload[{:d}] type = {:d} ({}), size = {:d}", metadata_payload_count, Pay.payloadType, Pay.mtype_str, Pay.payloadSize);
    }
    metadata_payload_count += 1;

} [[name(std::format("Payload[{}]:{}", (metadata_payload_count - 1), Pay.mtype_str))]];

struct PbuMetadata:PbuBase {
    u64 endOffset = 0;
    ptype_str = "Metadata";
    metadata_payload_count = 0; // reset number of metadata payload
    md_total += 1;

    u64 pbu_end = $ + pbu_size - read;

    u32 metadata_size; // syntax

    endOffset = $ + metadata_size;

    if(endOffset > pbu_end) {
        std::warning(std::format("metadata_size ({}) overruns pbu_size ({})", metadata_size, pbu_size));
    }

    MetadataPayload pay[while($ < endOffset && $ < pbu_end)] [[inline]];

    if($ > endOffset) {
        std::warning("metadata payload overruns metadata_size");
    }

    if($ < pbu_end) { // filler()
        if(std::mem::read_unsigned($, 1) != 0xFF) {
            std::warning("metadata filler byte is not 0xFF");
        }
        u8 filler[pbu_end - $] [[sealed]];
    }
};

struct PbuFiller:PbuBase {
    ptype_str = "filler";
    if(pbu_size < read) {
        std::warning(std::format("PBU[{}] invalid pbu_size ({})", pbu_count, pbu_size));
    }
    u8 data[(pbu_size > read) ? (pbu_size - read) : 0] [[sealed]];
};

struct PbuUnknown:PbuBase {
    std::warning(std::format("Unknown PBU type ({})!!!", u32(pbu_type)));
    ptype_str = "unknown";
    if(pbu_size < read) {
        std::warning(std::format("PBU[{}] invalid pbu_size ({})", pbu_count, pbu_size));
    }
    u8 data[(pbu_size > read) ? (pbu_size - read) : 0] [[sealed]];
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
        (PbuType::FRM_GAINMAP): PbuFrm Pbu [[inline]];
        (PbuType::AUI): PbuAui Pbu [[inline]];
        (PbuType::METADATA): PbuMetadata Pbu [[inline]];
        (PbuType::FILLER): PbuFiller Pbu [[inline]];
        (_) : PbuUnknown Pbu [[inline]];
    }

    if(verbose) {
        std::print("  PBU[{:d}] size = {:d}, {}", pbu_count, pbu_size, Pbu.ptype_str);
    }

    pbu_count += 1;
    pbu_total += 1;

} [[name(std::format("PBU[{}]:{}", (pbu_count - 1), Pbu.ptype_str))]];

struct AccessUnit {
    u64 au_end = 0;

    u32 au_size; // originally this syntax is part of RawBitstream

    if(verbose) {
        std::print("AU[{:d}] size = {:d}", raw_count, au_size);
    }

    au_end = $ + au_size;

    pbu_count = 0; // reset number of PBU
    au_frm_idx = 0;

    char signature[4]; // 'aPv1'
    if(signature != "aPv1") {
        std::warning("AU signature is not 'aPv1'");
    }
    PBU pbu[while($ < au_end)] [[inline]];
};

struct RawBitstream {
    AccessUnit AU [[name(std::format("AU[{}]", raw_count))]];
    raw_count += 1;
} [[name(std::format("Raw[{}]", (raw_count - 1)))]];

struct ApvBitstream {
    RawBitstream Raw [[inline]];
}[[inline]];

std::print("APV bitstream ({} bytes)", std::mem::size());

ApvBitstream APV[while(!std::mem::eof())] @ 0x0 [[inline]];

std::print("Summary: {} AU, {} PBU ({} frame, {} metadata)", raw_count, pbu_total, frm_total, md_total);
if(frm_total > 0) {
    str cfgs = std::string::replace(frame_cfg_list, "][", " | ");
    std::print("Frame: {}", std::string::substr(cfgs, 1, std::string::length(cfgs) - 2));
}
str md_list = "";
if(md_t35 > 0) md_list += std::format("ITU-T T.35({}), ", md_t35);
if(md_mdcv > 0) md_list += std::format("MDCV({}), ", md_mdcv);
if(md_cll > 0) md_list += std::format("CLL({}), ", md_cll);
if(md_filler > 0) md_list += std::format("Filler({}), ", md_filler);
if(md_icc > 0) md_list += std::format("ICC profile({}), ", md_icc);
if(md_gainmap > 0) md_list += std::format("Gainmap({}), ", md_gainmap);
if(md_user > 0) md_list += std::format("User defined({}), ", md_user);
if(md_undef > 0) md_list += std::format("Undefined({}), ", md_undef);
if(md_list != "") {
    std::print("Metadata: {}", std::string::substr(md_list, 0, std::string::length(md_list) - 2));
}
