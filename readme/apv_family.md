APV Family
==============

# Introduction

This document details the standard configurations of the APV codec, collectively termed the 'APV family'. Although the APV family is not formally defined in the APV specification, it provides a useful framework for defining typical setups and ensures uniform communication practices.

The choice of which APV family to use depends on factors like file size requirements, desired quality, and the specific editing environment. For example, 422 HQ might be preferred for preserving the high quality video, while 422 LQ might be used for lower file sizes and bandwidth usage, making it suitable for situations where storage space or network transfer is a concern.

# Description of APV family

| Usage | Family | Color Sampling |
|:-------------|:--------------:|:------:|
| High quality mezzanine                   | APV 422 HQ | 4:2:2 | 
| Standard quality mezzanine               | APV 422 SQ | 4:2:2 | 
| Editing-friendly low-data-rate workflows | APV 422 LQ | 4:2:2 | 
| Finishing                                | APV 444 UQ | 4:4:4 | 


## Target bitrates of APV family

Following table is the typical bitrates of APV family.
Actual compressed bitrate may have about 10% variations according to complexity of video signal.

|Resolution | Frame Rate | APV 422 LQ | | APV 422 SQ | | APV 422 HQ | | APV 444 UQ | |
|:---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| |  | Mb/s | GB/h | Mb/s | GB/h | Mb/s | GB/h | Mb/s | GB/h|
|qHD     960x540 | 24p | 29 | 13 | 41 | 19 | 58 | 26 | 86 | 39|
| | 25p | 31 | 14 | 43 | 19 | 60 | 27 | 90 | 41|
| | 30p | 37 | 17 | 51 | 23 | 72 | 32 | 108 | 49|
| | 50p | 61 | 28 | 86 | 39 | 120 | 54 | 180 | 81|
| | 60p | 73 | 33 | 103 | 46 | 144 | 65 | 216 | 97|
| | 120p | 147 | 66 | 206 | 93 | 288 | 130 | 432 | 194|
|HD     1280x720 | 24p | 40 | 18 | 56 | 25 | 78 | 35 | 118 | 53|
| | 25p | 42 | 19 | 58 | 26 | 82 | 37 | 123 | 55|
| | 30p | 50 | 23 | 70 | 32 | 98 | 44 | 147 | 66|
| | 50p | 83 | 38 | 117 | 53 | 163 | 74 | 245 | 110|
| | 60p | 100 | 45 | 140 | 63 | 196 | 88 | 294 | 132|
| | 120p | 200 | 90 | 280 | 126 | 392 | 176 | 588 | 265|
|FHD     1920x1080 | 24p | 81 | 36 | 113 | 51 | 158 | 71 | 238 | 107|
| | 25p | 84 | 38 | 118 | 53 | 165 | 74 | 248 | 111|
| | 30p | 101 | 45 | 141 | 64 | 198 | 89 | 297 | 134|
| | 50p | 168 | 76 | 236 | 106 | 330 | 149 | 495 | 223|
| | 60p | 202 | 91 | 283 | 127 | 396 | 178 | 594 | 267|
| | 120p | 404 | 182 | 566 | 255 | 792 | 356 | 1188 | 535|
|2K     2048x1080 | 24p | 86 | 39 | 121 | 54 | 169 | 76 | 253 | 114|
| | 25p | 90 | 40 | 126 | 57 | 176 | 79 | 264 | 119|
| | 30p | 108 | 48 | 151 | 68 | 211 | 95 | 317 | 143|
| | 50p | 180 | 81 | 251 | 113 | 352 | 158 | 528 | 238|
| | 60p | 216 | 97 | 302 | 136 | 422 | 190 | 634 | 285|
| | 120p | 431 | 194 | 603 | 272 | 845 | 380 | 1,267 | 570|
|UHD   4K     3840x2160 | 24p | 325 | 146 | 455 | 205 | 637 | 287 | 955 | 430|
| | 25p | 338 | 152 | 474 | 213 | 663 | 299 | 995 | 448|
| | 30p | 406 | 183 | 569 | 256 | 796 | 358 | 1,194 | 537|
| | 50p | 677 | 305 | 948 | 426 | 1,327 | 597 | 1,990 | 896|
| | 60p | 812 | 366 | 1,137 | 512 | 1,592 | 716 | 2,388 | 1,075|
| | 120p | 1,624 | 731 | 2,274 | 1,023 | 3,184 | 1,433 | 4,776 | 2,149|
|4K     4096x2160 | 24p | 347 | 156 | 485 | 218 | 679 | 306 | 1,019 | 458|
| | 25p | 361 | 162 | 505 | 227 | 708 | 318 | 1,061 | 478|
| | 30p | 433 | 195 | 606 | 273 | 849 | 382 | 1,274 | 573|
| | 50p | 722 | 325 | 1,011 | 455 | 1,415 | 637 | 2,123 | 955|
| | 60p | 866 | 390 | 1,213 | 546 | 1,698 | 764 | 2,547 | 1,146|
| | 120p | 1,733 | 780 | 2,426 | 1,092 | 3,396 | 1,528 | 5,094 | 2,293|
|UHD   8K     7680x4320 | 24p | 1,300 | 585 | 1,819 | 819 | 2,547 | 1,146 | 3,821 | 1,719|
| | 25p | 1,354 | 609 | 1,895 | 853 | 2,653 | 1,194 | 3,980 | 1,791|
| | 30p | 1,624 | 731 | 2,274 | 1,023 | 3,184 | 1,433 | 4,776 | 2,149|
| | 50p | 2,707 | 1,218 | 3,790 | 1,706 | 5,307 | 2,388 | 7,960 | 3,582|
| | 60p | 3,249 | 1,462 | 4,549 | 2,047 | 6,368 | 2,866 | 9,552 | 4,298|
| | 120p | 6,498 | 2,924 | 9,097 | 4,094 | 12,736 | 5,731 | 19,104 | 8,597|
|8K     8192x4320 | 24p | 1,386 | 624 | 1,941 | 873 | 2,717 | 1,223 | 4,076 | 1,834|
| | 25p | 1,444 | 650 | 2,022 | 910 | 2,830 | 1,274 | 4,245 | 1,910|
| | 30p | 1,733 | 780 | 2,426 | 1,092 | 3,396 | 1,528 | 5,094 | 2,292|
| | 50p | 2,888 | 1,300 | 4,043 | 1,819 | 5,660 | 2,547 | 8,491 | 3,821|
| | 60p | 3,466 | 1,560 | 4,852 | 2,183 | 6,793 | 3,057 | 10,189 | 4,585|
| | 120p | 6,931 | 3,119 | 9,704 | 4,367 | 13,585 | 6,113 | 20,378 | 9,170|

< table: Typical bitrates (Mega bits/sec, Giga Bytes/hour) >


