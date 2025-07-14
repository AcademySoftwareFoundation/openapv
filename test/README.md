# Materials for APV codec testing

## Test bitstream
"bitstream" folder has the encoded bitstreams for decoder conformance testing.

| No. | Bitstream Name | Description                                                  | Profile&nbsp;&nbsp; | Level | Band | Frame Rate | Resolution | # of Frame | MD5 sum of bitstream             |
|-----|----------------|--------------------------------------------------------------|---------------------|-------|------|------------|------------|------------|----------------------------------|
| 1   | tile_A         | one-tile per   one-picture                                   | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | c5b2f4c4ec9804f0292b2f12bd558dc5 |
| 2   | tile_B         | Tile size = min size   tile (256x128)                        | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | 7d626cea95f8d7a4b3f1f6e3d10e923c |
| 3   | tile_C         | # of Tiles: max num   tile (20x20)                           | 422-10              | 5     | 0    | 30 fps     | 7680x4320  | 3          | 758377994717d15999f53341eb5d6038 |
| 4   | tile_D         | tile dummy data test                                         | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | e124625d4ad310e2e60e366a63f669c9 |
| 5   | tile_E         | tile_size_present_in_fh_flag=on                              | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | 77cd01a8821cd17c2188fca033edc726 |
| 6   | qp_A           | QP matrix enabled                                            | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | 1ade0aed96ddf0aab286a082c17701d7 |
| 7   | qp_B           | Tile QP   variation in a frame                               | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | c7cac366f29dc6571bc814923cadeb4b |
| 8   | qp_C           | Set all the QPs in a   frame equal to min. QP (=0)           | 422-10              | 6     | 2    | 60 fps     | 3840x2160  | 3          | 6e2928f315e1670b6842955b0e7b4ad8 |
| 9   | qp_D           | Set all the QPs in a   frame equal to max. QP (=51)          | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | c7a3e5d7f1c987a064a7bdb08944901f |
| 10  | qp_E           | Set different QP  between luma and chroma                    | 422-10              | 4.1   | 2    | 60 fps     | 3840x2160  | 3          | 7d626cea95f8d7a4b3f1f6e3d10e923c |
| 11  | syn_A          | Exercise a synthetic   image with QP = 0 and QP = 51         | 422-10              | 4.1   | 2    | 60 fps     | 1920x1080  | 2          | 7b0cc8fdffdfca860dcee9b69b051053 |
| 12  | syn_B          | Exercise a synthetic   image with Tile QP variation in Frame | 422-10              | 4.1   | 2    | 60 fps     | 1920x1080  | 2          | b87a59443b009e9241393e6e1a927d61 |

## Test sequence
"sequence" folder has the uncompressed video sequence for encoder testing.
