/*
 * Copyright © 2018 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#define IR(num)    VTE_CHARSET_ISO_2375_IR_##num
#define DEC(name)  VTE_CHARSET_DEC_##name
#define NRCS(name) VTE_CHARSET_##name##_NRCS
#define NA         VTE_CHARSET_NONE
#define RET        VTE_CHARSET_RETURN

/* 94-character graphic character sets:
 * G0: ESC 2/8 F
 * G1: ESC 2/9 F
 * G2: ESC 2/10 F
 * G3: ESC 2/11 F
 * C0: -
 * C1: -
 */
/* NOTE: 4/8 'H' for IR(11) (SWEDISH_NRCS) conflicts with the
 * primary choice on ISO_HEBREW_SUPPLEMENTAL.
 * VT510 always chooses HEBREW; the table below prefers IR #11.
 * Also note that ARIB STD-B24 defines charsets with 03/01..03/08
 * which conflict with the DEC and NRCS charsets below; so they
 * have not been added here.
 */
static uint8_t const charset_graphic_94[] = {
        /* 3/0..3/15 */
        DEC(SPECIAL_GRAPHIC), NA, NA, NA, NRCS(DUTCH), NRCS(FINNISH), NRCS(NORWEGIAN_DANISH), NRCS(SWEDISH),
        NA, NRCS(FRENCH_CANADIAN), NA, NA, DEC(UPSS), NRCS(SWISS), DEC(TECHNICAL), NA,
        /* 4/0..4/15 */
        IR(2), IR(4), IR(6), IR(8_1), IR(8_2), IR(9_1), IR(9_2), IR(10),
        IR(11), IR(13), IR(14), IR(21), IR(16), IR(39), IR(37), IR(38),
        /* 5/0..5/15 */
        IR(53), IR(54), IR(25), IR(55), IR(57), IR(27), IR(47), IR(49),
        IR(31), IR(15), IR(17), IR(18), IR(19), IR(50), IR(51), IR(59),
        /* 6/0..6/15 */
        IR(60), IR(61), IR(70), IR(71), IR(72_OR_173), IR(68), IR(69), IR(84),
        IR(85), IR(86), IR(88), IR(89), IR(90), IR(91), IR(92), IR(93),
        /* 7/0..7/13 */
        IR(94), IR(95), IR(96), IR(98), IR(99), IR(102), IR(103), IR(121),
        IR(122), IR(137), IR(141), IR(146), IR(128), IR(147),
};

/* 94-character graphic character sets, with second intermediate byte 2/1:
 * G0: ESC 2/8 2/1 F
 * G1: ESC 2/9 2/1 F
 * G2: ESC 2/10 2/1 F
 * G3: ESC 2/11 2/1 F
 * C0: -
 * C1: -
 */
static uint8_t const charset_graphic_94_with_2_1[] = {
        /* 4/0..4/6 */
        IR(150), IR(151), IR(170), IR(207), IR(230), IR(231), IR(232)
};

/* 94-character graphic character sets, with second intermediate byte 2/2:
 * G0: ESC 2/8 2/2 F
 * G1: ESC 2/9 2/2 F
 * G2: ESC 2/10 2/2 F
 * G3: ESC 2/11 2/2 F
 * C0: -
 * C1: -
 */
static uint8_t const charset_graphic_94_with_2_2[] = {
        /* 3/0..3/15 */
        NA, NA, NA, NA, DEC(HEBREW), NA, NA, NA,
        NA, NA, NA, NA, NA, NA, NRCS(GREEK), DEC(GREEK),
};

/* 94-character graphic character sets, with second intermediate byte 2/5:
 * G0: ESC 2/8 2/5 F
 * G1: ESC 2/9 2/5 F
 * G2: ESC 2/10 2/5 F
 * G3: ESC 2/11 2/5 F
 * C0: -
 * C1: -
 */
static uint8_t const charset_graphic_94_with_2_5[] = {
        /* 3/0..3/15 */
        DEC(TURKISH), NA, NRCS(TURKISH), NRCS(SOFT), NA, DEC(SUPPLEMENTAL_GRAPHIC), NRCS(PORTUGUESE), NA,
        NA, NA, NA, NA, NA, NRCS(HEBREW), NA, NA,
};

/* 94-character graphic character sets, with second intermediate byte 2/6:
 * G0: ESC 2/8 2/6 F
 * G1: ESC 2/9 2/6 F
 * G2: ESC 2/10 2/6 F
 * G3: ESC 2/11 2/6 F
 * C0: -
 * C1: -
 */
static uint8_t const charset_graphic_94_with_2_6[] = {
        /* 3/0..3/15 */
        NA, NA, NA, DEC(THAI), DEC(CYRILLIC), NRCS(RUSSIAN), NA, NA,
        NA, NA, NA, NA, NA, NA, NA, NA,
};

/* 96-characters graphic character sets:
 * G0: -
 * G1: ESC 2/13 F
 * G2: ESC 2/14 F
 * G3: ESC 2/15 F
 * C0: -
 * C1: -
 */
static uint8_t const charset_graphic_96[] = {
        /* 3/0..3/15 */
        NA, NA, NA, NA, NA, NA, NA, NA,
        NA, NA, NA, NA, DEC(UPSS), NA, NA, NA,
        /* 4/0..4/15 */
        IR(111), IR(100), IR(101), IR(109), IR(110), IR(123), IR(126), IR(127),
        IR(138), IR(139), IR(142), IR(143), IR(144), IR(148), IR(152), IR(153),
        /* 5/0..5/15 */
        IR(154), IR(155), IR(156), IR(164), IR(166), IR(167), IR(157), NA,
        IR(158), IR(179), IR(180), IR(181), IR(182), IR(197), IR(198), IR(199),
        /* 6/0..6/15 */
        IR(200), IR(201), IR(203), IR(204), IR(205), IR(206), IR(226), IR(208),
        IR(209), IR(227), IR(234), NA, NA, NA, NA, NA,
        /* 7/0..7/13 */
        NA, NA, NA, NA, NA, NA, NA, NA,
        NA, NA, NA, NA, NA, IR(129),
};

/* Multibyte graphic character sets:
 * G0: ESC 2/4 2/8 F
 * G1: ESC 2/4 2/9 F
 * G2: ESC 2/4 2/10 F
 * G3: ESC 2/4 2/11 F
 * C0: -
 * C1: -
 *
 * Note that exceptionally, ESC 2/4 4/0, ESC 2/4 4/1 and ESC 2/4 4/2 designate
 * G0 sets for compatibility with an earlier version of ISO-2022.
 * Also note that ARIB STD-B24 defines 2-byte charsets with 03/09..03/11
 * which have not been added here.
 */
static uint8_t const charset_graphic_94_n[] = {
        /* 3/0..3/15 */
        NA, DEC(KANJI_1978), NA, DEC(KANJI_1983), NA, NA, NA, NA,
        NA, NA, NA, NA, NA, NA, NA, NA,
        /* 4/0..4/15 */
        IR(42), IR(58), IR(87_OR_168), IR(149), IR(159), IR(165), IR(169), IR(171),
        IR(172), IR(183), IR(184), IR(185), IR(186), IR(187), IR(202), IR(228),
        /* 5/0..5/1 */
        IR(229), IR(233),
};

/* Multibyte graphic character sets, with third intermediate byte 2/1:
 * G0: ESC 2/4 2/8 2/1 F
 * G1: ESC 2/4 2/9 2/1 F
 * G2: ESC 2/4 2/10 2/1 F
 * G3: ESC 2/4 2/11 2/1 F
 * C0: -
 * C1: -
 *
 * Note that these are not registed in ISO-IR.
 *
 * [Source: ecma35lib/ecma35/data/graphdata.py]
 */
static uint8_t const charset_graphic_94_n_with_2_1[] = {
        /* 3/0..3/15 */
        NA, VTE_CHARSET_EUCTW_G2, VTE_CHARSET_HKCS_EXT, VTE_CHARSET_MS_950_UTC_EXT
};

/* C0 control character sets:
 * G0: -
 * G1: -
 * G2: -
 * G3: -
 * C0: ESC 2/1 F
 * C1: -
 */
static uint8_t const charset_control_c0[] = {
        /* 4/0..4/12 */
        IR(1), IR(7), IR(48), IR(26), IR(36), IR(106), IR(74), IR(104),
        IR(130), IR(132), IR(134), IR(135), IR(140),
};

/* C1 control character sets:
 * G0: -
 * G1: -
 * G2: -
 * G3: -
 * C0: -
 * C1: ESC 2/2 F
 */
static uint8_t const charset_control_c1[] = {
        /* 4/0..4/8 */
        IR(56), IR(73), IR(67_OR_124), IR(77), IR(133), IR(40), IR(136), IR(105),
        IR(107)
};

#if 0
/* Single control functions as two-byte escape sequences
 * ESC F
 */
static uint8_t const charset_control_single[] = {
        /* 6/0..6/15 */
        IR(32), IR(33), IR(34), IR(35), IR(189), NA, NA, NA,
        NA, NA, NA, NA, NA, NA, IR(62), IR(63),
        /* 7/0..7/14 */
        NA, NA, NA, NA, NA, NA, NA, NA,
        NA, NA, NA, NA, IR(64), IR(65), IR(66)
};
#endif

/* Non-ISO-2022 coding systems, with standard return:
 * ESC 2/5 F
 */
static uint8_t const charset_ocs[] = {
        /* 3/0..3/15 */
        NA, NA, NA, NA, NA, NA, NA, NA,
        DEC(HPPCL), NA, NA, NA, NA, DEC(IBM_PROPRINTER), NA, NA,
        /* 4/0..4/8 */
        RET, IR(108), IR(178), IR(131), IR(145), IR(160), IR(161), IR(196),
        IR(188)
};

/* Non-ISO-2022 coding systems, with standard return:
 * ESC 2/5 SP F
 */
static uint8_t const charset_ocs_with_2_0[] = {
        /* 03/00 */
        DEC(BARCODE)
};

/* Non-ISO-2022 coding systems, without standard return:
 * ESC 2/5 2/15 F
 */
static uint8_t const charset_ocs_with_2_15[] = {
        /* 4/0..4/12 */
        IR(162), IR(163), IR(125), IR(174), IR(175), IR(176), IR(177), IR(190),
        IR(191), IR(192), IR(193), IR(194), IR(195)
};

#undef IR
#undef DEC
#undef NRCS
#undef NA
#undef RET
