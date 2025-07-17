ISOBMFF binding for APV
==============

# Introduction

This document specifies methods to store data encoded with Advanced Professional Video (APV) codec in ISO Base Media File Format (ISOBMFF) files. APV is a mezzanine video codec for storage, exchange and editing of professional quality video. To support extensive, repeated editing including multiple rounds of decompression and compression all the necessary information for decoding, frame header and metadata for processing of decoded video and presentation are put together for fast and simple access. For example, frame header data are repeated in each frame even if the information in frame header are exactly identical for series of frames. When APV bitstream is stored in a file, to avoid such inefficiency codec configuration box indicates whether header information is identical to entire frames stored in a track. In this file format, for efficient access of portions of a frame, a method to identify the location of tiles are supported.

# ISOBMFF binding for APV video

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
When APV coded bitstream is encapsulated in a track with APVSampleEntry, each sample shall contain one and only one access unit of APV coded data. The format of sample shall be same as the raw_bitstream_access_unit, the length field of four bytes preceeding the access unit data, as defined in the section 12.1 of [APV codec specification](#apv-codec).

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

# Sub-parameters for the MIME tyype 'codecs' parameter

When the 'codecs' parameter of a MIME type is used for a track containing  APV bistream, as defined in IETF RFC 6381, the sub-parameters defined in this section is used.

The 'codecs' parameter string for the APV bitstream is defined as follows:

~~~
<4CC value of sample entry>.<key1><value1>.<key2><value2>...<keyN><valueN>
~~~

The keys are defined as 4CC values and the value used for each keys are defined in the table below.

|Key|Meaning|Value|
|:-:|:-:|:-:|
|'apvf'| profile | the largest value of the profile_idc in APVDecoderConfigurationRecord |
|'apvl'| level | the largest value of the level_idc in APVDecoderConfigurationRecord |
|'apvb'| band | the largest value of the band_idc in APVDecoderConfigurationRecord |

For example, codecs="apv1.apvf44.apvl210.apvb3" indicates the track is compliant to 'apv1' sample entry type and the largest value of the profile in APVDecoderConfigurationRecord of the track is 422-12 profile, the level is 7 and the band is 3.


# ISOBMFF and HEIF binding for APV image items

## APV image items - general

This section describes how to encapsulate APV encoded images and image collections into the Image File Format
specified in ISO/IEC 23008-12 (also known as "HEIF").

This format allows multiple images to be included in a single file, allowing stereo or multi-view images, and 
panoramas. Not all of the images need to be in the same format, such that an APV image could have an embedded
thumbnail in JPEG or HEIC (H.265) format. The format also allows image sequences (essentially video with
advisory timing), which are further specified [below](#isobmff-and-heif-binding-for-apv-image-sequences).

ISO/IEC 23008-12 is an extension to ISO/IEC 14496-12 (ISOBMFF), and it is valid to include combinations of
video, image and image sequences plus associated metadata in a single file. The nature of APV allows the
bitstream data for a frame to be common between the video track and image (or image sequence frame) if desired.

## APV image item data

### APV image item data definition

An item of type `apvi` consists of an APV bitstream corresponding to one and only one access unit of APV coded data.
The item content does not start with the `au_size` since this can be determined from the sum of the extents given in the
`iloc` for the item.

### APV image item data syntax

TODO

### APV image item data semantics

TODO

## APV configuration item property

+ Box type: `apvC`

+ Property type: Descriptive item property

+ Container: `ItemPropertyContainerBox`

+ Mandatory (per item): Yes, for an image item of type `apvi`

+ Quantity (per item): One for an image item of type `apvi`

Each APV image item shall have an associated property that is exactly identical to the `APVConfigurationBox` defined [above](#definition-1).

`essential` shall be equal to 1 for an `apvC` item property associated with an image item of type `apvi`.

## APV auxiliary images

EDITOR NOTE: We could consider prohibiting this, since they could just be in the AU.

Auxiliary images may be used to provide supplemental frame data such as depth images or alpha images. The URNs specified for HEVC in
ISO/IEC 23008-12:2025 Section B.2.4 may also be used with APV.

An APV coded auxiliary image uses an `item_type` value of `apvi`. An APV coded auxiliary image does not use the `aux_subtype` byte array in
`AuxiliaryTypeProperty`.

# ISOBMFF and HEIF binding for APV image sequences

TODO: To be specified.

# APV-specific brands

## APV video brand

The brand `apv1` shall be used to indicate that the file is conformant with the [APV video](#isobmff-binding-for-apv-video) section of this document.

EDITOR NOTE: We could relax this requirement and specify the parts we actually need (e.g. not requiring `stss` since its all sync samples).

The `apv1` brand requires support for the `isom` brand as defined in ISO/IEC 14496-12.

## APV image and image collection brand

### APV image brand

The brand `apvi` ("APV image") is specified in the following subclauses.

A coded image item is specified to conform to the `apvi` brand when all of the following constraints are true:

 - the item has type `avp1` and conforms to the specification for [APV image items described above](#isobmff-and-heif-binding-for-apv-image-items).
 - the item is not associated with any essential item properties other than `apvC`, `colr`, `irot`, `clap`, and `imir`.

### Requirements on files

Files shall include `mif1` among the compatible brands and hence conform to the associated requirements in ISO/IEC 23008-12:2025
Section 10.

Files including `apvi` as a compatible brand shall include an image item that is present in the file, as either the primary item
or any item from the alternate group containing the primary item, and that meets one of the following constraints:

 - the item is a coded image item conforming to the `apvi` brand specified [above](#apv-image-brand)
 - the item is a crop-rotate-mirror derived image item, and the source image item of the item is either a crop-rotate-mirror
    derived image item or a coded image item conforming to the `apvi` brand specified [above](#apv-image-brand).

### Requirements on readers

The requirements in ISO/IEC 23008-12:2025 Section 10 for `mif1` readers shall be supported.

Readers conforming to the `apvi` brand shall support displaying an item that is either the primary item or any item from the alternate group
containing the primary item, where that item meets one of the following constraints:

 - the item is a coded image item conforming to the `apvi` brand specified [above](#apv-image-brand)
 - the item is a crop-rotate-mirror derived image item, and the source image item of the item is either a crop-rotate-mirror
    derived image item or a coded image item conforming to the `apvi` brand specified [above](#apv-image-brand).

Readers conforming to the `apvi` brand are recommended to decode all profiles and levels of the APV codec, but are not required to do so.

File readers should support displaying an image with opacity information specified by an associated auxiliary image with `aux_type` equal
to `urn:mpeg:hevc:2015:auxid:1`.

## AVP image sequence brand

TODO: To be specified

# References

<a name="apv-codec"></a>
+ APV codec
> The IETF Internet-Draft of APV codec: https://datatracker.ietf.org/doc/html/draft-lim-apv.