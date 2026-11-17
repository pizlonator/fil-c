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

#define IR_NAME(num) ISO_2375_IR_##num
#define DEC_NAME(name) DEC_##name
#define NRCS_NAME(name) name##_NRCS
#define SUPPLEMENTAL_NAME(name) ISO_##name##_SUPPLEMENTAL

#define IR(num) _VTE_CHARSET(IR_NAME(num))
#define DEC(name) _VTE_CHARSET(DEC_NAME(name))
#define NRCS(name) _VTE_CHARSET(NRCS_NAME(name))
#define ALIAS(name1,name2) _VTE_CHARSET_ALIAS(name1,name2)

_VTE_CHARSET(NONE)

/* See ECMA-35 § 14.4 for the meaning of this */
_VTE_CHARSET(DRCS)

/* See ECMA-35 § 14.1 (and ECMA-6 § 9.2) for the meaning of this */
_VTE_CHARSET(EMPTY)

/* Return to ISO-2022 */
_VTE_CHARSET(RETURN)

/* ISO 2375 IR sets */

IR(1)
IR(2)
IR(4)
IR(6)
IR(7)
IR(8_1)
IR(8_2)
IR(9_1)
IR(9_2)
IR(10)
IR(11)
IR(13)
IR(14)
IR(15)
IR(16)
IR(17)
IR(18)
IR(19)
IR(21)
IR(25)
IR(26)
IR(27)
IR(31)
IR(32)
IR(33)
IR(34)
IR(35)
IR(36)
IR(37)
IR(38)
IR(39)
IR(40)
IR(42)
IR(47)
IR(48)
IR(49)
IR(50)
IR(51)
IR(53)
IR(54)
IR(55)
IR(56)
IR(57)
IR(58)
IR(59)
IR(60)
IR(61)
IR(62)
IR(63)
IR(64)
IR(65)
IR(66)
IR(67)
IR(68)
IR(69)
IR(70)
IR(71)
IR(72)
IR(73)
IR(74)
IR(77)
IR(84)
IR(85)
IR(86)
IR(87)
IR(88)
IR(89)
IR(90)
IR(91)
IR(92)
IR(93)
IR(94)
IR(95)
IR(96)
IR(98)
IR(99)
IR(100)
IR(101)
IR(102)
IR(103)
IR(104)
IR(105)
IR(106)
IR(107)
IR(108)
IR(109)
IR(110)
IR(111)
IR(121)
IR(122)
IR(123)
IR(124)
IR(125)
IR(126)
IR(127)
IR(128)
IR(129)
IR(130)
IR(131)
IR(132)
IR(133)
IR(134)
IR(135)
IR(136)
IR(137)
IR(138)
IR(139)
IR(140)
IR(141)
IR(142)
IR(143)
IR(144)
IR(145)
IR(146)
IR(147)
IR(148)
IR(149)
IR(150)
IR(151)
IR(152)
IR(153)
IR(154)
IR(155)
IR(156)
IR(157)
IR(158)
IR(159)
IR(160)
IR(161)
IR(162)
IR(163)
IR(164)
IR(165)
IR(166)
IR(167)
IR(168)
IR(169)
IR(170)
IR(171)
IR(172)
IR(173)
IR(174)
IR(175)
IR(176)
IR(177)
IR(178)
IR(179)
IR(180)
IR(181)
IR(182)
IR(183)
IR(184)
IR(185)
IR(186)
IR(187)
IR(188)
IR(189)
IR(190)
IR(191)
IR(192)
IR(193)
IR(194)
IR(195)
IR(196)
IR(197)
IR(198)
IR(199)
IR(200)
IR(201)
IR(202)
IR(203)
IR(204)
IR(205)
IR(206)
IR(207)
IR(208)
IR(209)
IR(226)
IR(227)
IR(228)
IR(229)
IR(230)
IR(231)
IR(232)
IR(233)
IR(234)

/* Use IRR to choose between them */
IR(67_OR_124)
IR(72_OR_173)
IR(87_OR_168)

/* DEC 94-sets */

/* This is referred to variously as DEC Supplemental or DEC Supplemental Graphic */
DEC(SUPPLEMENTAL_GRAPHIC) /* ESC I 2/5 3/5 */
DEC(SPECIAL_GRAPHIC) /* ESC I 3/0 */
DEC(TECHNICAL) /* ESC I 3/14 */
/* UPSS: User Preferred Supplemental Set */
DEC(UPSS) /* ESC I 3/12 */
DEC(CYRILLIC) /* ESC I 2/6 3/4 */
DEC(GREEK) /* ESC I 2/2 3/15 */
DEC(HEBREW) /* ESC I 2/2 3/4 */
DEC(THAI) /* ESC I 2/6 3/3 */
DEC(TURKISH) /* ESC I 2/5 3/0 */

/* DEC NRCS 94-sets */

/* FRENCH_CANADIAN: this has a secondary choice of ESC I 5/1 which is registered as
 * ISO IR #54 (cyrillic), so we don't implement that alias
*/
NRCS(FRENCH_CANADIAN) /* ESC I 3/9 */

/* FIXME: check if these correspond to any IR sets and make them an alias if so */
NRCS(DUTCH) /* ESC I 3/4 */
NRCS(GREEK) /* ESC I 2/2 3/14 */
NRCS(HEBREW) /* ESC I 2/5 3/13 */
NRCS(PORTUGUESE) /* ESC I 2/5 3/6 */
NRCS(RUSSIAN) /* ESC I 2/6 3/5 */
NRCS(SWISS) /* ESC I 3/13 */
NRCS(TURKISH) /* ESC I 2/5 3/2 */

NRCS(SOFT) /* ESC I 2/5 3/3 */

/* Aliases. They were identified as an alias by their ISO IR sequence.
 * Some have a secondary sequence.
 */

ALIAS(DEC_NAME(KANJI_1978), IR_NAME(42)) /* G3 only: ESC 2/4 2/11 3/1 */
ALIAS(DEC_NAME(KANJI_1983), IR_NAME(87)) /* G3 only: ESC 2/4 2/11 3/3 */

ALIAS(NRCS_NAME(AMERICAN), IR_NAME(6))
ALIAS(NRCS_NAME(BRITISH), IR_NAME(4))
ALIAS(NRCS_NAME(FINNISH), IR_NAME(8_1)) /* ESC I 3/5 */

/* ISO IR #25 is withdrawn in favour of #69; check which one DEC actually uses */
ALIAS(NRCS_NAME(FRENCH), IR_NAME(25))

ALIAS(NRCS_NAME(GERMAN), IR_NAME(21))
ALIAS(NRCS_NAME(ITALIAN), IR_NAME(15))

/* NORWEGIAN_DANISH: this has primary choice ESC I 6/0 which is ISO IR #60,
 * secondary choice ESC I 4/5 which is ISO IR #9-1 and
 * tertiary choice ESC 3/6. We link the tertiary choice to IR #9-1 since
 * the VT220 manual only lists the 4/5 and 3/6 choices.
 */
ALIAS(NRCS_NAME(NORWEGIAN_DANISH), IR_NAME(9_1))

ALIAS(NRCS_NAME(SPANISH), IR_NAME(17))
ALIAS(NRCS_NAME(SWEDISH), IR_NAME(11)) /* ESC I 3/7 */

ALIAS(SUPPLEMENTAL_NAME(GREEK), IR_NAME(126))
ALIAS(SUPPLEMENTAL_NAME(HEBREW), IR_NAME(138))
ALIAS(SUPPLEMENTAL_NAME(LATIN_1), IR_NAME(100))
ALIAS(SUPPLEMENTAL_NAME(LATIN_2), IR_NAME(101))
ALIAS(SUPPLEMENTAL_NAME(LATIN_5), IR_NAME(148))
ALIAS(SUPPLEMENTAL_NAME(LATIN_CYRILLIC), IR_NAME(144))

/* Multi-byte charsets not registered in ISO IR */

_VTE_CHARSET(EUCTW_G2) /* 4-byte */
_VTE_CHARSET(HKCS_EXT)
_VTE_CHARSET(MS_950_UTC_EXT)

/* Other coding systems */

DEC(HPPCL) /* DEC HPPCL emulation mode on DEC LJ250; ESC 2/5 3/8 */
DEC(IBM_PROPRINTER) /* DEC PPLV2; ESC 2/5 3/13 */
DEC(BARCODE) /* DEC PPLV2; ESC 2/5 2/0 3/0 */

#undef IR_NAME
#undef DEC_NAME
#undef NRCS_NAME
#undef SUPPLEMENTAL_NAME

#undef IR
#undef DEC
#undef NRCS
#undef ALIAS
