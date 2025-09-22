#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2003, 2004, 2005 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh

testfiles testfile testfile2

testrun_compare ${abs_builddir}/show-abbrev testfile testfile2 <<\EOF
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 0
abbrev[0]: attr[1]: code = 18, form = 1, offset = 2
abbrev[0]: attr[2]: code = 17, form = 1, offset = 4
abbrev[0]: attr[3]: code = 3, form = 8, offset = 6
abbrev[0]: attr[4]: code = 27, form = 8, offset = 8
abbrev[0]: attr[5]: code = 37, form = 8, offset = 10
abbrev[0]: attr[6]: code = 19, form = 11, offset = 12
abbrev[19]: code = 2, tag = 46, children = 1
abbrev[19]: attr[0]: code = 1, form = 19, offset = 19
abbrev[19]: attr[1]: code = 63, form = 12, offset = 21
abbrev[19]: attr[2]: code = 3, form = 8, offset = 23
abbrev[19]: attr[3]: code = 58, form = 11, offset = 25
abbrev[19]: attr[4]: code = 59, form = 11, offset = 27
abbrev[19]: attr[5]: code = 39, form = 12, offset = 29
abbrev[19]: attr[6]: code = 73, form = 19, offset = 31
abbrev[19]: attr[7]: code = 17, form = 1, offset = 33
abbrev[19]: attr[8]: code = 18, form = 1, offset = 35
abbrev[19]: attr[9]: code = 64, form = 10, offset = 37
abbrev[44]: code = 3, tag = 46, children = 1
abbrev[44]: attr[0]: code = 1, form = 19, offset = 44
abbrev[44]: attr[1]: code = 63, form = 12, offset = 46
abbrev[44]: attr[2]: code = 3, form = 8, offset = 48
abbrev[44]: attr[3]: code = 58, form = 11, offset = 50
abbrev[44]: attr[4]: code = 59, form = 11, offset = 52
abbrev[44]: attr[5]: code = 73, form = 19, offset = 54
abbrev[44]: attr[6]: code = 60, form = 12, offset = 56
abbrev[63]: code = 4, tag = 24, children = 0
abbrev[68]: code = 5, tag = 46, children = 1
abbrev[68]: attr[0]: code = 63, form = 12, offset = 68
abbrev[68]: attr[1]: code = 3, form = 8, offset = 70
abbrev[68]: attr[2]: code = 58, form = 11, offset = 72
abbrev[68]: attr[3]: code = 59, form = 11, offset = 74
abbrev[68]: attr[4]: code = 73, form = 19, offset = 76
abbrev[68]: attr[5]: code = 60, form = 12, offset = 78
abbrev[85]: code = 6, tag = 36, children = 0
abbrev[85]: attr[0]: code = 3, form = 8, offset = 85
abbrev[85]: attr[1]: code = 11, form = 11, offset = 87
abbrev[85]: attr[2]: code = 62, form = 11, offset = 89
abbrev[96]: code = 7, tag = 52, children = 0
abbrev[96]: attr[0]: code = 3, form = 8, offset = 96
abbrev[96]: attr[1]: code = 58, form = 11, offset = 98
abbrev[96]: attr[2]: code = 59, form = 11, offset = 100
abbrev[96]: attr[3]: code = 73, form = 19, offset = 102
abbrev[96]: attr[4]: code = 63, form = 12, offset = 104
abbrev[96]: attr[5]: code = 2, form = 10, offset = 106
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 114
abbrev[0]: attr[1]: code = 18, form = 1, offset = 116
abbrev[0]: attr[2]: code = 17, form = 1, offset = 118
abbrev[0]: attr[3]: code = 3, form = 8, offset = 120
abbrev[0]: attr[4]: code = 27, form = 8, offset = 122
abbrev[0]: attr[5]: code = 37, form = 8, offset = 124
abbrev[0]: attr[6]: code = 19, form = 11, offset = 126
abbrev[19]: code = 2, tag = 46, children = 0
abbrev[19]: attr[0]: code = 63, form = 12, offset = 133
abbrev[19]: attr[1]: code = 3, form = 8, offset = 135
abbrev[19]: attr[2]: code = 58, form = 11, offset = 137
abbrev[19]: attr[3]: code = 59, form = 11, offset = 139
abbrev[19]: attr[4]: code = 39, form = 12, offset = 141
abbrev[19]: attr[5]: code = 73, form = 19, offset = 143
abbrev[19]: attr[6]: code = 17, form = 1, offset = 145
abbrev[19]: attr[7]: code = 18, form = 1, offset = 147
abbrev[19]: attr[8]: code = 64, form = 10, offset = 149
abbrev[42]: code = 3, tag = 36, children = 0
abbrev[42]: attr[0]: code = 3, form = 8, offset = 156
abbrev[42]: attr[1]: code = 11, form = 11, offset = 158
abbrev[42]: attr[2]: code = 62, form = 11, offset = 160
abbrev[53]: code = 4, tag = 22, children = 0
abbrev[53]: attr[0]: code = 3, form = 8, offset = 167
abbrev[53]: attr[1]: code = 58, form = 11, offset = 169
abbrev[53]: attr[2]: code = 59, form = 11, offset = 171
abbrev[53]: attr[3]: code = 73, form = 19, offset = 173
abbrev[66]: code = 5, tag = 15, children = 0
abbrev[66]: attr[0]: code = 11, form = 11, offset = 180
abbrev[73]: code = 6, tag = 15, children = 0
abbrev[73]: attr[0]: code = 11, form = 11, offset = 187
abbrev[73]: attr[1]: code = 73, form = 19, offset = 189
abbrev[82]: code = 7, tag = 19, children = 1
abbrev[82]: attr[0]: code = 1, form = 19, offset = 196
abbrev[82]: attr[1]: code = 11, form = 11, offset = 198
abbrev[82]: attr[2]: code = 58, form = 11, offset = 200
abbrev[82]: attr[3]: code = 59, form = 11, offset = 202
abbrev[95]: code = 8, tag = 13, children = 0
abbrev[95]: attr[0]: code = 3, form = 8, offset = 209
abbrev[95]: attr[1]: code = 58, form = 11, offset = 211
abbrev[95]: attr[2]: code = 59, form = 11, offset = 213
abbrev[95]: attr[3]: code = 73, form = 19, offset = 215
abbrev[95]: attr[4]: code = 56, form = 10, offset = 217
abbrev[110]: code = 9, tag = 1, children = 1
abbrev[110]: attr[0]: code = 1, form = 19, offset = 224
abbrev[110]: attr[1]: code = 73, form = 19, offset = 226
abbrev[119]: code = 10, tag = 33, children = 0
abbrev[119]: attr[0]: code = 73, form = 19, offset = 233
abbrev[119]: attr[1]: code = 47, form = 11, offset = 235
abbrev[128]: code = 11, tag = 19, children = 1
abbrev[128]: attr[0]: code = 1, form = 19, offset = 242
abbrev[128]: attr[1]: code = 3, form = 8, offset = 244
abbrev[128]: attr[2]: code = 11, form = 11, offset = 246
abbrev[128]: attr[3]: code = 58, form = 11, offset = 248
abbrev[128]: attr[4]: code = 59, form = 11, offset = 250
abbrev[143]: code = 12, tag = 19, children = 0
abbrev[143]: attr[0]: code = 3, form = 8, offset = 257
abbrev[143]: attr[1]: code = 60, form = 12, offset = 259
abbrev[152]: code = 13, tag = 13, children = 0
abbrev[152]: attr[0]: code = 3, form = 8, offset = 266
abbrev[152]: attr[1]: code = 58, form = 11, offset = 268
abbrev[152]: attr[2]: code = 59, form = 5, offset = 270
abbrev[152]: attr[3]: code = 73, form = 19, offset = 272
abbrev[152]: attr[4]: code = 56, form = 10, offset = 274
abbrev[167]: code = 14, tag = 22, children = 0
abbrev[167]: attr[0]: code = 3, form = 8, offset = 281
abbrev[167]: attr[1]: code = 58, form = 11, offset = 283
abbrev[167]: attr[2]: code = 59, form = 5, offset = 285
abbrev[167]: attr[3]: code = 73, form = 19, offset = 287
abbrev[180]: code = 15, tag = 23, children = 1
abbrev[180]: attr[0]: code = 1, form = 19, offset = 294
abbrev[180]: attr[1]: code = 11, form = 11, offset = 296
abbrev[180]: attr[2]: code = 58, form = 11, offset = 298
abbrev[180]: attr[3]: code = 59, form = 11, offset = 300
abbrev[193]: code = 16, tag = 13, children = 0
abbrev[193]: attr[0]: code = 3, form = 8, offset = 307
abbrev[193]: attr[1]: code = 58, form = 11, offset = 309
abbrev[193]: attr[2]: code = 59, form = 11, offset = 311
abbrev[193]: attr[3]: code = 73, form = 19, offset = 313
abbrev[206]: code = 17, tag = 4, children = 1
abbrev[206]: attr[0]: code = 1, form = 19, offset = 320
abbrev[206]: attr[1]: code = 11, form = 11, offset = 322
abbrev[206]: attr[2]: code = 58, form = 11, offset = 324
abbrev[206]: attr[3]: code = 59, form = 11, offset = 326
abbrev[219]: code = 18, tag = 40, children = 0
abbrev[219]: attr[0]: code = 3, form = 8, offset = 333
abbrev[219]: attr[1]: code = 28, form = 11, offset = 335
abbrev[228]: code = 19, tag = 38, children = 0
abbrev[228]: attr[0]: code = 73, form = 19, offset = 342
abbrev[235]: code = 20, tag = 21, children = 1
abbrev[235]: attr[0]: code = 1, form = 19, offset = 349
abbrev[235]: attr[1]: code = 39, form = 12, offset = 351
abbrev[235]: attr[2]: code = 73, form = 19, offset = 353
abbrev[246]: code = 21, tag = 5, children = 0
abbrev[246]: attr[0]: code = 73, form = 19, offset = 360
abbrev[253]: code = 22, tag = 21, children = 1
abbrev[253]: attr[0]: code = 1, form = 19, offset = 367
abbrev[253]: attr[1]: code = 39, form = 12, offset = 369
abbrev[262]: code = 23, tag = 33, children = 0
abbrev[262]: attr[0]: code = 73, form = 19, offset = 376
abbrev[262]: attr[1]: code = 47, form = 6, offset = 378
abbrev[271]: code = 24, tag = 22, children = 0
abbrev[271]: attr[0]: code = 3, form = 8, offset = 385
abbrev[271]: attr[1]: code = 58, form = 11, offset = 387
abbrev[271]: attr[2]: code = 59, form = 11, offset = 389
abbrev[282]: code = 25, tag = 4, children = 1
abbrev[282]: attr[0]: code = 1, form = 19, offset = 396
abbrev[282]: attr[1]: code = 3, form = 8, offset = 398
abbrev[282]: attr[2]: code = 11, form = 11, offset = 400
abbrev[282]: attr[3]: code = 58, form = 11, offset = 402
abbrev[282]: attr[4]: code = 59, form = 11, offset = 404
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 412
abbrev[0]: attr[1]: code = 18, form = 1, offset = 414
abbrev[0]: attr[2]: code = 17, form = 1, offset = 416
abbrev[0]: attr[3]: code = 3, form = 8, offset = 418
abbrev[0]: attr[4]: code = 27, form = 8, offset = 420
abbrev[0]: attr[5]: code = 37, form = 8, offset = 422
abbrev[0]: attr[6]: code = 19, form = 11, offset = 424
abbrev[19]: code = 2, tag = 46, children = 0
abbrev[19]: attr[0]: code = 63, form = 12, offset = 431
abbrev[19]: attr[1]: code = 3, form = 8, offset = 433
abbrev[19]: attr[2]: code = 58, form = 11, offset = 435
abbrev[19]: attr[3]: code = 59, form = 11, offset = 437
abbrev[19]: attr[4]: code = 39, form = 12, offset = 439
abbrev[19]: attr[5]: code = 73, form = 19, offset = 441
abbrev[19]: attr[6]: code = 17, form = 1, offset = 443
abbrev[19]: attr[7]: code = 18, form = 1, offset = 445
abbrev[19]: attr[8]: code = 64, form = 10, offset = 447
abbrev[42]: code = 3, tag = 36, children = 0
abbrev[42]: attr[0]: code = 3, form = 8, offset = 454
abbrev[42]: attr[1]: code = 11, form = 11, offset = 456
abbrev[42]: attr[2]: code = 62, form = 11, offset = 458
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 0
abbrev[0]: attr[1]: code = 18, form = 1, offset = 2
abbrev[0]: attr[2]: code = 17, form = 1, offset = 4
abbrev[0]: attr[3]: code = 3, form = 8, offset = 6
abbrev[0]: attr[4]: code = 27, form = 8, offset = 8
abbrev[0]: attr[5]: code = 37, form = 8, offset = 10
abbrev[0]: attr[6]: code = 19, form = 11, offset = 12
abbrev[19]: code = 2, tag = 46, children = 0
abbrev[19]: attr[0]: code = 63, form = 12, offset = 19
abbrev[19]: attr[1]: code = 3, form = 8, offset = 21
abbrev[19]: attr[2]: code = 58, form = 11, offset = 23
abbrev[19]: attr[3]: code = 59, form = 11, offset = 25
abbrev[19]: attr[4]: code = 39, form = 12, offset = 27
abbrev[19]: attr[5]: code = 73, form = 19, offset = 29
abbrev[19]: attr[6]: code = 17, form = 1, offset = 31
abbrev[19]: attr[7]: code = 18, form = 1, offset = 33
abbrev[19]: attr[8]: code = 64, form = 10, offset = 35
abbrev[42]: code = 3, tag = 36, children = 0
abbrev[42]: attr[0]: code = 3, form = 8, offset = 42
abbrev[42]: attr[1]: code = 11, form = 11, offset = 44
abbrev[42]: attr[2]: code = 62, form = 11, offset = 46
abbrev[53]: code = 4, tag = 22, children = 0
abbrev[53]: attr[0]: code = 3, form = 8, offset = 53
abbrev[53]: attr[1]: code = 58, form = 11, offset = 55
abbrev[53]: attr[2]: code = 59, form = 11, offset = 57
abbrev[53]: attr[3]: code = 73, form = 19, offset = 59
abbrev[66]: code = 5, tag = 1, children = 1
abbrev[66]: attr[0]: code = 1, form = 19, offset = 66
abbrev[66]: attr[1]: code = 3, form = 8, offset = 68
abbrev[66]: attr[2]: code = 73, form = 19, offset = 70
abbrev[77]: code = 6, tag = 33, children = 0
abbrev[77]: attr[0]: code = 73, form = 19, offset = 77
abbrev[77]: attr[1]: code = 47, form = 11, offset = 79
abbrev[86]: code = 7, tag = 19, children = 1
abbrev[86]: attr[0]: code = 1, form = 19, offset = 86
abbrev[86]: attr[1]: code = 3, form = 8, offset = 88
abbrev[86]: attr[2]: code = 11, form = 11, offset = 90
abbrev[86]: attr[3]: code = 58, form = 11, offset = 92
abbrev[86]: attr[4]: code = 59, form = 11, offset = 94
abbrev[101]: code = 8, tag = 13, children = 0
abbrev[101]: attr[0]: code = 3, form = 8, offset = 101
abbrev[101]: attr[1]: code = 58, form = 11, offset = 103
abbrev[101]: attr[2]: code = 59, form = 11, offset = 105
abbrev[101]: attr[3]: code = 73, form = 19, offset = 107
abbrev[101]: attr[4]: code = 56, form = 10, offset = 109
abbrev[116]: code = 9, tag = 15, children = 0
abbrev[116]: attr[0]: code = 11, form = 11, offset = 116
abbrev[123]: code = 10, tag = 15, children = 0
abbrev[123]: attr[0]: code = 11, form = 11, offset = 123
abbrev[123]: attr[1]: code = 73, form = 19, offset = 125
abbrev[132]: code = 11, tag = 19, children = 1
abbrev[132]: attr[0]: code = 1, form = 19, offset = 132
abbrev[132]: attr[1]: code = 11, form = 11, offset = 134
abbrev[132]: attr[2]: code = 58, form = 11, offset = 136
abbrev[132]: attr[3]: code = 59, form = 11, offset = 138
abbrev[145]: code = 12, tag = 1, children = 1
abbrev[145]: attr[0]: code = 1, form = 19, offset = 145
abbrev[145]: attr[1]: code = 73, form = 19, offset = 147
abbrev[154]: code = 13, tag = 22, children = 0
abbrev[154]: attr[0]: code = 3, form = 8, offset = 154
abbrev[154]: attr[1]: code = 58, form = 11, offset = 156
abbrev[154]: attr[2]: code = 59, form = 5, offset = 158
abbrev[154]: attr[3]: code = 73, form = 19, offset = 160
abbrev[167]: code = 14, tag = 19, children = 0
abbrev[167]: attr[0]: code = 3, form = 8, offset = 167
abbrev[167]: attr[1]: code = 60, form = 12, offset = 169
abbrev[176]: code = 15, tag = 22, children = 0
abbrev[176]: attr[0]: code = 3, form = 8, offset = 176
abbrev[176]: attr[1]: code = 58, form = 11, offset = 178
abbrev[176]: attr[2]: code = 59, form = 11, offset = 180
abbrev[187]: code = 16, tag = 21, children = 1
abbrev[187]: attr[0]: code = 1, form = 19, offset = 187
abbrev[187]: attr[1]: code = 39, form = 12, offset = 189
abbrev[187]: attr[2]: code = 73, form = 19, offset = 191
abbrev[198]: code = 17, tag = 5, children = 0
abbrev[198]: attr[0]: code = 73, form = 19, offset = 198
abbrev[205]: code = 18, tag = 38, children = 0
abbrev[205]: attr[0]: code = 73, form = 19, offset = 205
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 213
abbrev[0]: attr[1]: code = 18, form = 1, offset = 215
abbrev[0]: attr[2]: code = 17, form = 1, offset = 217
abbrev[0]: attr[3]: code = 3, form = 8, offset = 219
abbrev[0]: attr[4]: code = 27, form = 8, offset = 221
abbrev[0]: attr[5]: code = 37, form = 8, offset = 223
abbrev[0]: attr[6]: code = 19, form = 11, offset = 225
abbrev[19]: code = 2, tag = 46, children = 0
abbrev[19]: attr[0]: code = 63, form = 12, offset = 232
abbrev[19]: attr[1]: code = 3, form = 8, offset = 234
abbrev[19]: attr[2]: code = 58, form = 11, offset = 236
abbrev[19]: attr[3]: code = 59, form = 11, offset = 238
abbrev[19]: attr[4]: code = 39, form = 12, offset = 240
abbrev[19]: attr[5]: code = 73, form = 19, offset = 242
abbrev[19]: attr[6]: code = 17, form = 1, offset = 244
abbrev[19]: attr[7]: code = 18, form = 1, offset = 246
abbrev[19]: attr[8]: code = 64, form = 10, offset = 248
abbrev[42]: code = 3, tag = 36, children = 0
abbrev[42]: attr[0]: code = 3, form = 8, offset = 255
abbrev[42]: attr[1]: code = 11, form = 11, offset = 257
abbrev[42]: attr[2]: code = 62, form = 11, offset = 259
abbrev[0]: code = 1, tag = 17, children = 1
abbrev[0]: attr[0]: code = 16, form = 6, offset = 267
abbrev[0]: attr[1]: code = 18, form = 1, offset = 269
abbrev[0]: attr[2]: code = 17, form = 1, offset = 271
abbrev[0]: attr[3]: code = 3, form = 8, offset = 273
abbrev[0]: attr[4]: code = 27, form = 8, offset = 275
abbrev[0]: attr[5]: code = 37, form = 8, offset = 277
abbrev[0]: attr[6]: code = 19, form = 11, offset = 279
abbrev[19]: code = 2, tag = 46, children = 1
abbrev[19]: attr[0]: code = 1, form = 19, offset = 286
abbrev[19]: attr[1]: code = 63, form = 12, offset = 288
abbrev[19]: attr[2]: code = 3, form = 8, offset = 290
abbrev[19]: attr[3]: code = 58, form = 11, offset = 292
abbrev[19]: attr[4]: code = 59, form = 11, offset = 294
abbrev[19]: attr[5]: code = 39, form = 12, offset = 296
abbrev[19]: attr[6]: code = 73, form = 19, offset = 298
abbrev[19]: attr[7]: code = 17, form = 1, offset = 300
abbrev[19]: attr[8]: code = 18, form = 1, offset = 302
abbrev[19]: attr[9]: code = 64, form = 10, offset = 304
abbrev[44]: code = 3, tag = 46, children = 1
abbrev[44]: attr[0]: code = 1, form = 19, offset = 311
abbrev[44]: attr[1]: code = 63, form = 12, offset = 313
abbrev[44]: attr[2]: code = 3, form = 8, offset = 315
abbrev[44]: attr[3]: code = 58, form = 11, offset = 317
abbrev[44]: attr[4]: code = 59, form = 11, offset = 319
abbrev[44]: attr[5]: code = 73, form = 19, offset = 321
abbrev[44]: attr[6]: code = 60, form = 12, offset = 323
abbrev[63]: code = 4, tag = 24, children = 0
abbrev[68]: code = 5, tag = 46, children = 1
abbrev[68]: attr[0]: code = 63, form = 12, offset = 335
abbrev[68]: attr[1]: code = 3, form = 8, offset = 337
abbrev[68]: attr[2]: code = 58, form = 11, offset = 339
abbrev[68]: attr[3]: code = 59, form = 11, offset = 341
abbrev[68]: attr[4]: code = 73, form = 19, offset = 343
abbrev[68]: attr[5]: code = 60, form = 12, offset = 345
abbrev[85]: code = 6, tag = 36, children = 0
abbrev[85]: attr[0]: code = 3, form = 8, offset = 352
abbrev[85]: attr[1]: code = 11, form = 11, offset = 354
abbrev[85]: attr[2]: code = 62, form = 11, offset = 356
abbrev[96]: code = 7, tag = 52, children = 0
abbrev[96]: attr[0]: code = 3, form = 8, offset = 363
abbrev[96]: attr[1]: code = 58, form = 11, offset = 365
abbrev[96]: attr[2]: code = 59, form = 11, offset = 367
abbrev[96]: attr[3]: code = 73, form = 19, offset = 369
abbrev[96]: attr[4]: code = 63, form = 12, offset = 371
abbrev[96]: attr[5]: code = 2, form = 10, offset = 373
EOF

exit 0
