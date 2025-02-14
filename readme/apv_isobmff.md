ISOBMFF binding for APV
==============

# Introduction

This document specifies methods to store data encoded with Advanced Professional Video (APV) codec in ISO Base Media File Format (ISOBMFF) files. APV is a mezzanine video codec for storage, exchange and editing of professional quality video. To support extensive, repeated editing including multiple rounds of decompression and compression all the necessary information for decoding, frame header and metadata for processing of decoded video and presentation are put together for fast and simple access. For example, frame header data are repeated in each frame even if the information in frame header are exactly identical for series of frames. When APV bitstream is stored in a file, to avoid such inefficiency codec configuration box indicates whether header information is identical to entire frames stored in a track. In this file format, for efficient access of portions of a frame, a method to identify the location of tiles are supported.

# ISOBMFF binding for APV

## APV Sample Entry

### Definition

+ Sample Entry Type: apv1

+ Container: Sample Description Box ('stsd')

+ Mandatory: Yes

+ Quantity: One or more

### Description

The sample entry with APV1SampleEntry type specifies that the track contains APV coded video data samples. This type of sample entry shall use APVCodecConfigurationBox.


### Syntax
~~~~
class APV1SampleEntry extends VisualSampleEntry('apv1'){
	APVCodecConfigurationBox	config;
}
~~~~

### Semantics

The largest one among the values of the frame_width field and frame_height field of the APVCodecConfigurationBox shall be used for the value of width and height fields of the VisualSampleEntry, respectively.

When the sample entry name is 'apv1', the stream to which this sample entry applies shall be a compliant APV stream as viewed by an APV decoder operating under the configuration (including profile, level, and so on) given in the APVCodecConfigurationBox.

The compressorname field of the VisualSampleEntry shall have '\012APV Coding'. The sample entry with APV1SampleEntry type specifies that the track contains APV coded video data samples. This type of sample entry shall use APVCodecConfigurationBox.

## APV Codec Configuration Box

### Definition

+ Box Type: apvC

+ Container: APV Sample Entry ('apv1')

+ Mandatory: Yes

+ Quantity: Exactly One

### Description

The box with APVCodecConfigurationBox shall contains APVDecoderConfigurationRecord as defined in {{APVDecoderConfigurationRecord}}

### Syntax

~~~~
aligned(8) class APVDecoderConfigurationBox extends FullBox('apvC',version=0, flags) {
    APVDecoderConfigurationRecord apvConfig;
}
~~~~

## APV Decoder Configuration Record {#APVDecoderConfigurationRecord}		

The APVDecoderConfigurationRecord contains the information for initial configuration of a decoder which consumes the samples references the sample entry type of apv1. The information in this record is extracted from frame_header() of the bitstream stored in the track containing this record.

All variation of information required to decide appropriate resource for decoding, e.g. the profiles a decoder compliant to, are carried so that the client can decide whether it has appropriate resources to completely decode the AUs in that track.

### Syntax

~~~~
aligned(8) class APVDecoderConfigurationRecord {
    unsigned int(8) configurationVersion = 1;
    unsigned int(8) number_of_configuration_entry;
    for (i=0; i<number_of_configuration_entry; i++) {
        unsigned int(8) pbu_type[i];
        unsigned int(8) number_of_frame_info[i];
        for (j=0; j<number_of_frame_info[i]; j++) {
            reserved_zero_6bits;
            unsigned int(1) color_description_present_flag[i][j];
            unsigned int(1) capture_time_distance_ignored[i][j];
            unsigned int(8) profile_idc[i][j];
            unsigned int(8) level_idc[i][j];
            unsigned int(8) band_idc[i][j];
            unsigned int(32) frame_width[i][j];
            unsigned int(32) frame_height[i][j];
            unsigned int(4) chroma_format_idc[i][j];
            unsigned int(4) bit_depth_minus8[i][j];
            unsigned int(8) capture_time_distance[i][j];
            if (color_description_present_flag[i][j]) {
                unsigned int(8) color_primaries[i][j];
                unsigned int(8) transfer_characteristics[i][j];
                unsigned int(8) matrix_coefficients[i][j];
                unsigned int(1) full_range_flag[i][j];
                reserved_zero_7bits;
            }
        }
    }
}
~~~~

### Semantics

+ number_of_configuration_entry
> indicates the number of frame header information for a specific PBU types are stored.

+ pbu_type[i]
> indicates the value of the pbu_type field in the pbu header immediately preceding the frame data for a certain index i.

+ number_of_frame_info[i]
> indicates the number of variations of the frame header information for the frames whose value of the pbu_type field in the pbu header immediately preceding it is idendtical with the value of the pub_type[i] field for a certain index i.

+ color_description_present_flag[i][j]
>indicates whether the color description information is provided for the jth variation of frame header whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ capture_time_distance_ignored[i][j]
> indicates whether the value of the capture_time_distance field in the jth variation of frame header is used for the processing of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ profile_idc[i][j]
> indicates the value of the profile_idc field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the profile_idc field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of profile_idc field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ level_idc[i][j]
> indicates the value of the level_idc field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the level_idc field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of level_idc field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ band_idc[i][j]
> indicates the value of the band_idc field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the band_idc field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of band_idc field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ frame_width[i][j]
> indicates the value of the frame_width field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the frame_width field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of frame_width field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ frame_height[i][j]
> indicates the value of the frame_height field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the frame_height field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of frame_height field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ chroma_format_idc[i][j]
> indicates the value of the chroma_format_idc field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the chroma_format_idc field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of chroma_format_idc field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ bit_depth_minus8[i]
> indicates the value of the bit_depth_minus8 field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the bit_depth_minus8 field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of bit_depth_minus8 field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ capture_time_distance[i][j]
> indicates the value of the capture_time_distance field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the capture_time_distance field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of capture_time_distance field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ color_primaries[i][j]
> indicates the value of the color_primaries field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the color_primaries field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of color_primaries field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ transfer_characteristics[i][j]
> indicates the value of the transfer_characteristics field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the transfer_characteristics field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of transfer_characteristics field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ matrix_coefficients[i][j]
> indicates the value of the matrix_coefficients field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of the matrix_coefficients field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of matrix_cofficients field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

+ full_range_flag[i][j]
> indicates the value of the full_range_flag field in the jth variation of the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1, then the same value of this field must be used as the value of
the full_range_flag field in the frame header of the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i. If the value of number_of_frame_info[i] is 1 is greater than 1, then the frame header in each sample must provide the value of matrix_cofficients field matched with one among the values of this field for all index j for the frames whose value of the pbu_type field in the pbu header immediately preceding it is identical with the value of the pbu_type[i] field for a certain index i.

## APV Sample Description

###	Format of sample
When APV coded bitstream is encapsulated in a track with APVSamspleEntry, each sample shall contain one and only one access unit of APV coded data

###	Sync sample
Every samples of APV bitstream shall be sync samples.

###	Sub-sample for APV
For the use of the SubSampleInformationBox as defined in ISO/IEC 14496-12 in an APV stream, a sub-sample is defined on the basis of the value of the flags field of the sub-sample information box as specified below. The presence of this box is optional; however, if present in a track containing APV data, the codec_specific_parameters field in the box shall have the semantics defined here.

flags specifies the type of sub-sample information given in this box as follows:
>0:	tile-based sub-samples: A sub-sample contains one tile.
>1:	PBU-based sub-samples : A sub-sample contains one PBU
Other values of flags are reserved.

The subsample_priority field shall be set to a value in accordance with the specification of this field in ISO/IEC 14496-12.
The discardable field shall be set to 1 only if this sample would still be decodable if this sub-sample is discarded.

The codec_specific_parameters field of the SubSampleInformationBox is defined for APV as follows:

~~~~
if (flags == 0) {
    unsigned int(32) tile_index;
}
else {
    bit(32) reserved = 0;
}
~~~~

tile_index for sub-samples based on tiles, this parameter indicates the tile index in raster order in a frame.
