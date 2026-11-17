// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// The following data was created from https://gitlab.freedesktop.org/xorg/app/rgb
// (commit 0d2caecebf0e2a10994c22960921f366dd98d19a) by running this command:
// $ showrgb | sort -k4 | awk -f program
// with the AWK programs indicated below before each data block.
// The rgb.txt file licence is as follows:
//
// Copyright 1985, 1989, 1998  The Open Group
//
// Permission to use, copy, modify, distribute, and sell this software and its
// documentation for any purpose is hereby granted without fee, provided that
// the above copyright notice appear in all copies and that both that
// copyright notice and this permission notice appear in supporting
// documentation.
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of The Open Group shall
// not be used in advertising or otherwise to promote the sale, use or
// other dealings in this Software without prior written authorization
// from The Open Group.
//
// Copyright (c) 1994, 2008, Oracle and/or its affiliates.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#pragma once

namespace vte::color {

        static constinit char const color_names_string[] =
                // BEGIN { first = 1 }
                // {
                //     if (NF > 4) { next }
                //     printf "\"%s%s\"\n", (first ? "" : "\\0"), tolower($4)
                //     first = 0
                // }
                "aliceblue"
                "\0antiquewhite"
                "\0antiquewhite1"
                "\0antiquewhite2"
                "\0antiquewhite3"
                "\0antiquewhite4"
                "\0aqua"
                "\0aquamarine"
                "\0aquamarine1"
                "\0aquamarine2"
                "\0aquamarine3"
                "\0aquamarine4"
                "\0azure"
                "\0azure1"
                "\0azure2"
                "\0azure3"
                "\0azure4"
                "\0beige"
                "\0bisque"
                "\0bisque1"
                "\0bisque2"
                "\0bisque3"
                "\0bisque4"
                "\0black"
                "\0blanchedalmond"
                "\0blue"
                "\0blue1"
                "\0blue2"
                "\0blue3"
                "\0blue4"
                "\0blueviolet"
                "\0brown"
                "\0brown1"
                "\0brown2"
                "\0brown3"
                "\0brown4"
                "\0burlywood"
                "\0burlywood1"
                "\0burlywood2"
                "\0burlywood3"
                "\0burlywood4"
                "\0cadetblue"
                "\0cadetblue1"
                "\0cadetblue2"
                "\0cadetblue3"
                "\0cadetblue4"
                "\0chartreuse"
                "\0chartreuse1"
                "\0chartreuse2"
                "\0chartreuse3"
                "\0chartreuse4"
                "\0chocolate"
                "\0chocolate1"
                "\0chocolate2"
                "\0chocolate3"
                "\0chocolate4"
                "\0coral"
                "\0coral1"
                "\0coral2"
                "\0coral3"
                "\0coral4"
                "\0cornflowerblue"
                "\0cornsilk"
                "\0cornsilk1"
                "\0cornsilk2"
                "\0cornsilk3"
                "\0cornsilk4"
                "\0crimson"
                "\0cyan"
                "\0cyan1"
                "\0cyan2"
                "\0cyan3"
                "\0cyan4"
                "\0darkblue"
                "\0darkcyan"
                "\0darkgoldenrod"
                "\0darkgoldenrod1"
                "\0darkgoldenrod2"
                "\0darkgoldenrod3"
                "\0darkgoldenrod4"
                "\0darkgray"
                "\0darkgreen"
                "\0darkgrey"
                "\0darkkhaki"
                "\0darkmagenta"
                "\0darkolivegreen"
                "\0darkolivegreen1"
                "\0darkolivegreen2"
                "\0darkolivegreen3"
                "\0darkolivegreen4"
                "\0darkorange"
                "\0darkorange1"
                "\0darkorange2"
                "\0darkorange3"
                "\0darkorange4"
                "\0darkorchid"
                "\0darkorchid1"
                "\0darkorchid2"
                "\0darkorchid3"
                "\0darkorchid4"
                "\0darkred"
                "\0darksalmon"
                "\0darkseagreen"
                "\0darkseagreen1"
                "\0darkseagreen2"
                "\0darkseagreen3"
                "\0darkseagreen4"
                "\0darkslateblue"
                "\0darkslategray"
                "\0darkslategray1"
                "\0darkslategray2"
                "\0darkslategray3"
                "\0darkslategray4"
                "\0darkslategrey"
                "\0darkturquoise"
                "\0darkviolet"
                "\0deeppink"
                "\0deeppink1"
                "\0deeppink2"
                "\0deeppink3"
                "\0deeppink4"
                "\0deepskyblue"
                "\0deepskyblue1"
                "\0deepskyblue2"
                "\0deepskyblue3"
                "\0deepskyblue4"
                "\0dimgray"
                "\0dimgrey"
                "\0dodgerblue"
                "\0dodgerblue1"
                "\0dodgerblue2"
                "\0dodgerblue3"
                "\0dodgerblue4"
                "\0firebrick"
                "\0firebrick1"
                "\0firebrick2"
                "\0firebrick3"
                "\0firebrick4"
                "\0floralwhite"
                "\0forestgreen"
                "\0fuchsia"
                "\0gainsboro"
                "\0ghostwhite"
                "\0gold"
                "\0gold1"
                "\0gold2"
                "\0gold3"
                "\0gold4"
                "\0goldenrod"
                "\0goldenrod1"
                "\0goldenrod2"
                "\0goldenrod3"
                "\0goldenrod4"
                "\0gray"
                "\0gray0"
                "\0gray1"
                "\0gray10"
                "\0gray100"
                "\0gray11"
                "\0gray12"
                "\0gray13"
                "\0gray14"
                "\0gray15"
                "\0gray16"
                "\0gray17"
                "\0gray18"
                "\0gray19"
                "\0gray2"
                "\0gray20"
                "\0gray21"
                "\0gray22"
                "\0gray23"
                "\0gray24"
                "\0gray25"
                "\0gray26"
                "\0gray27"
                "\0gray28"
                "\0gray29"
                "\0gray3"
                "\0gray30"
                "\0gray31"
                "\0gray32"
                "\0gray33"
                "\0gray34"
                "\0gray35"
                "\0gray36"
                "\0gray37"
                "\0gray38"
                "\0gray39"
                "\0gray4"
                "\0gray40"
                "\0gray41"
                "\0gray42"
                "\0gray43"
                "\0gray44"
                "\0gray45"
                "\0gray46"
                "\0gray47"
                "\0gray48"
                "\0gray49"
                "\0gray5"
                "\0gray50"
                "\0gray51"
                "\0gray52"
                "\0gray53"
                "\0gray54"
                "\0gray55"
                "\0gray56"
                "\0gray57"
                "\0gray58"
                "\0gray59"
                "\0gray6"
                "\0gray60"
                "\0gray61"
                "\0gray62"
                "\0gray63"
                "\0gray64"
                "\0gray65"
                "\0gray66"
                "\0gray67"
                "\0gray68"
                "\0gray69"
                "\0gray7"
                "\0gray70"
                "\0gray71"
                "\0gray72"
                "\0gray73"
                "\0gray74"
                "\0gray75"
                "\0gray76"
                "\0gray77"
                "\0gray78"
                "\0gray79"
                "\0gray8"
                "\0gray80"
                "\0gray81"
                "\0gray82"
                "\0gray83"
                "\0gray84"
                "\0gray85"
                "\0gray86"
                "\0gray87"
                "\0gray88"
                "\0gray89"
                "\0gray9"
                "\0gray90"
                "\0gray91"
                "\0gray92"
                "\0gray93"
                "\0gray94"
                "\0gray95"
                "\0gray96"
                "\0gray97"
                "\0gray98"
                "\0gray99"
                "\0green"
                "\0green1"
                "\0green2"
                "\0green3"
                "\0green4"
                "\0greenyellow"
                "\0grey"
                "\0grey0"
                "\0grey1"
                "\0grey10"
                "\0grey100"
                "\0grey11"
                "\0grey12"
                "\0grey13"
                "\0grey14"
                "\0grey15"
                "\0grey16"
                "\0grey17"
                "\0grey18"
                "\0grey19"
                "\0grey2"
                "\0grey20"
                "\0grey21"
                "\0grey22"
                "\0grey23"
                "\0grey24"
                "\0grey25"
                "\0grey26"
                "\0grey27"
                "\0grey28"
                "\0grey29"
                "\0grey3"
                "\0grey30"
                "\0grey31"
                "\0grey32"
                "\0grey33"
                "\0grey34"
                "\0grey35"
                "\0grey36"
                "\0grey37"
                "\0grey38"
                "\0grey39"
                "\0grey4"
                "\0grey40"
                "\0grey41"
                "\0grey42"
                "\0grey43"
                "\0grey44"
                "\0grey45"
                "\0grey46"
                "\0grey47"
                "\0grey48"
                "\0grey49"
                "\0grey5"
                "\0grey50"
                "\0grey51"
                "\0grey52"
                "\0grey53"
                "\0grey54"
                "\0grey55"
                "\0grey56"
                "\0grey57"
                "\0grey58"
                "\0grey59"
                "\0grey6"
                "\0grey60"
                "\0grey61"
                "\0grey62"
                "\0grey63"
                "\0grey64"
                "\0grey65"
                "\0grey66"
                "\0grey67"
                "\0grey68"
                "\0grey69"
                "\0grey7"
                "\0grey70"
                "\0grey71"
                "\0grey72"
                "\0grey73"
                "\0grey74"
                "\0grey75"
                "\0grey76"
                "\0grey77"
                "\0grey78"
                "\0grey79"
                "\0grey8"
                "\0grey80"
                "\0grey81"
                "\0grey82"
                "\0grey83"
                "\0grey84"
                "\0grey85"
                "\0grey86"
                "\0grey87"
                "\0grey88"
                "\0grey89"
                "\0grey9"
                "\0grey90"
                "\0grey91"
                "\0grey92"
                "\0grey93"
                "\0grey94"
                "\0grey95"
                "\0grey96"
                "\0grey97"
                "\0grey98"
                "\0grey99"
                "\0honeydew"
                "\0honeydew1"
                "\0honeydew2"
                "\0honeydew3"
                "\0honeydew4"
                "\0hotpink"
                "\0hotpink1"
                "\0hotpink2"
                "\0hotpink3"
                "\0hotpink4"
                "\0indianred"
                "\0indianred1"
                "\0indianred2"
                "\0indianred3"
                "\0indianred4"
                "\0indigo"
                "\0ivory"
                "\0ivory1"
                "\0ivory2"
                "\0ivory3"
                "\0ivory4"
                "\0khaki"
                "\0khaki1"
                "\0khaki2"
                "\0khaki3"
                "\0khaki4"
                "\0lavender"
                "\0lavenderblush"
                "\0lavenderblush1"
                "\0lavenderblush2"
                "\0lavenderblush3"
                "\0lavenderblush4"
                "\0lawngreen"
                "\0lemonchiffon"
                "\0lemonchiffon1"
                "\0lemonchiffon2"
                "\0lemonchiffon3"
                "\0lemonchiffon4"
                "\0lightblue"
                "\0lightblue1"
                "\0lightblue2"
                "\0lightblue3"
                "\0lightblue4"
                "\0lightcoral"
                "\0lightcyan"
                "\0lightcyan1"
                "\0lightcyan2"
                "\0lightcyan3"
                "\0lightcyan4"
                "\0lightgoldenrod"
                "\0lightgoldenrod1"
                "\0lightgoldenrod2"
                "\0lightgoldenrod3"
                "\0lightgoldenrod4"
                "\0lightgoldenrodyellow"
                "\0lightgray"
                "\0lightgreen"
                "\0lightgrey"
                "\0lightpink"
                "\0lightpink1"
                "\0lightpink2"
                "\0lightpink3"
                "\0lightpink4"
                "\0lightsalmon"
                "\0lightsalmon1"
                "\0lightsalmon2"
                "\0lightsalmon3"
                "\0lightsalmon4"
                "\0lightseagreen"
                "\0lightskyblue"
                "\0lightskyblue1"
                "\0lightskyblue2"
                "\0lightskyblue3"
                "\0lightskyblue4"
                "\0lightslateblue"
                "\0lightslategray"
                "\0lightslategrey"
                "\0lightsteelblue"
                "\0lightsteelblue1"
                "\0lightsteelblue2"
                "\0lightsteelblue3"
                "\0lightsteelblue4"
                "\0lightyellow"
                "\0lightyellow1"
                "\0lightyellow2"
                "\0lightyellow3"
                "\0lightyellow4"
                "\0lime"
                "\0limegreen"
                "\0linen"
                "\0magenta"
                "\0magenta1"
                "\0magenta2"
                "\0magenta3"
                "\0magenta4"
                "\0maroon"
                "\0maroon1"
                "\0maroon2"
                "\0maroon3"
                "\0maroon4"
                "\0mediumaquamarine"
                "\0mediumblue"
                "\0mediumorchid"
                "\0mediumorchid1"
                "\0mediumorchid2"
                "\0mediumorchid3"
                "\0mediumorchid4"
                "\0mediumpurple"
                "\0mediumpurple1"
                "\0mediumpurple2"
                "\0mediumpurple3"
                "\0mediumpurple4"
                "\0mediumseagreen"
                "\0mediumslateblue"
                "\0mediumspringgreen"
                "\0mediumturquoise"
                "\0mediumvioletred"
                "\0midnightblue"
                "\0mintcream"
                "\0mistyrose"
                "\0mistyrose1"
                "\0mistyrose2"
                "\0mistyrose3"
                "\0mistyrose4"
                "\0moccasin"
                "\0navajowhite"
                "\0navajowhite1"
                "\0navajowhite2"
                "\0navajowhite3"
                "\0navajowhite4"
                "\0navy"
                "\0navyblue"
                "\0oldlace"
                "\0olive"
                "\0olivedrab"
                "\0olivedrab1"
                "\0olivedrab2"
                "\0olivedrab3"
                "\0olivedrab4"
                "\0orange"
                "\0orange1"
                "\0orange2"
                "\0orange3"
                "\0orange4"
                "\0orangered"
                "\0orangered1"
                "\0orangered2"
                "\0orangered3"
                "\0orangered4"
                "\0orchid"
                "\0orchid1"
                "\0orchid2"
                "\0orchid3"
                "\0orchid4"
                "\0palegoldenrod"
                "\0palegreen"
                "\0palegreen1"
                "\0palegreen2"
                "\0palegreen3"
                "\0palegreen4"
                "\0paleturquoise"
                "\0paleturquoise1"
                "\0paleturquoise2"
                "\0paleturquoise3"
                "\0paleturquoise4"
                "\0palevioletred"
                "\0palevioletred1"
                "\0palevioletred2"
                "\0palevioletred3"
                "\0palevioletred4"
                "\0papayawhip"
                "\0peachpuff"
                "\0peachpuff1"
                "\0peachpuff2"
                "\0peachpuff3"
                "\0peachpuff4"
                "\0peru"
                "\0pink"
                "\0pink1"
                "\0pink2"
                "\0pink3"
                "\0pink4"
                "\0plum"
                "\0plum1"
                "\0plum2"
                "\0plum3"
                "\0plum4"
                "\0powderblue"
                "\0purple"
                "\0purple1"
                "\0purple2"
                "\0purple3"
                "\0purple4"
                "\0rebeccapurple"
                "\0red"
                "\0red1"
                "\0red2"
                "\0red3"
                "\0red4"
                "\0rosybrown"
                "\0rosybrown1"
                "\0rosybrown2"
                "\0rosybrown3"
                "\0rosybrown4"
                "\0royalblue"
                "\0royalblue1"
                "\0royalblue2"
                "\0royalblue3"
                "\0royalblue4"
                "\0saddlebrown"
                "\0salmon"
                "\0salmon1"
                "\0salmon2"
                "\0salmon3"
                "\0salmon4"
                "\0sandybrown"
                "\0seagreen"
                "\0seagreen1"
                "\0seagreen2"
                "\0seagreen3"
                "\0seagreen4"
                "\0seashell"
                "\0seashell1"
                "\0seashell2"
                "\0seashell3"
                "\0seashell4"
                "\0sienna"
                "\0sienna1"
                "\0sienna2"
                "\0sienna3"
                "\0sienna4"
                "\0silver"
                "\0skyblue"
                "\0skyblue1"
                "\0skyblue2"
                "\0skyblue3"
                "\0skyblue4"
                "\0slateblue"
                "\0slateblue1"
                "\0slateblue2"
                "\0slateblue3"
                "\0slateblue4"
                "\0slategray"
                "\0slategray1"
                "\0slategray2"
                "\0slategray3"
                "\0slategray4"
                "\0slategrey"
                "\0snow"
                "\0snow1"
                "\0snow2"
                "\0snow3"
                "\0snow4"
                "\0springgreen"
                "\0springgreen1"
                "\0springgreen2"
                "\0springgreen3"
                "\0springgreen4"
                "\0steelblue"
                "\0steelblue1"
                "\0steelblue2"
                "\0steelblue3"
                "\0steelblue4"
                "\0tan"
                "\0tan1"
                "\0tan2"
                "\0tan3"
                "\0tan4"
                "\0teal"
                "\0thistle"
                "\0thistle1"
                "\0thistle2"
                "\0thistle3"
                "\0thistle4"
                "\0tomato"
                "\0tomato1"
                "\0tomato2"
                "\0tomato3"
                "\0tomato4"
                "\0turquoise"
                "\0turquoise1"
                "\0turquoise2"
                "\0turquoise3"
                "\0turquoise4"
                "\0violet"
                "\0violetred"
                "\0violetred1"
                "\0violetred2"
                "\0violetred3"
                "\0violetred4"
                "\0webgray"
                "\0webgreen"
                "\0webgrey"
                "\0webmaroon"
                "\0webpurple"
                "\0wheat"
                "\0wheat1"
                "\0wheat2"
                "\0wheat3"
                "\0wheat4"
                "\0white"
                "\0whitesmoke"
                "\0x11gray"
                "\0x11green"
                "\0x11grey"
                "\0x11maroon"
                "\0x11purple"
                "\0yellow"
                "\0yellow1"
                "\0yellow2"
                "\0yellow3"
                "\0yellow4"
                "\0yellowgreen";

        struct color_name_index {
                uint16_t offset; // offset into color_names_string
                uint32_t color;  // packed RGBA (BE)
        };

        static constinit color_name_index const color_names_indices[] = {
                // BEGIN { offset = 0 }
                // {
                //     if (NF > 4) { next }
                //     printf "{ %4d, 0x%06x },\n", offset, $1 * 0x10000 + 0x100 * $2 + $3
                //     offset += length($4) + 1
                // }
                {    0, 0xf0f8ff },
                {   10, 0xfaebd7 },
                {   23, 0xffefdb },
                {   37, 0xeedfcc },
                {   51, 0xcdc0b0 },
                {   65, 0x8b8378 },
                {   79, 0x00ffff },
                {   84, 0x7fffd4 },
                {   95, 0x7fffd4 },
                {  107, 0x76eec6 },
                {  119, 0x66cdaa },
                {  131, 0x458b74 },
                {  143, 0xf0ffff },
                {  149, 0xf0ffff },
                {  156, 0xe0eeee },
                {  163, 0xc1cdcd },
                {  170, 0x838b8b },
                {  177, 0xf5f5dc },
                {  183, 0xffe4c4 },
                {  190, 0xffe4c4 },
                {  198, 0xeed5b7 },
                {  206, 0xcdb79e },
                {  214, 0x8b7d6b },
                {  222, 0x000000 },
                {  228, 0xffebcd },
                {  243, 0x0000ff },
                {  248, 0x0000ff },
                {  254, 0x0000ee },
                {  260, 0x0000cd },
                {  266, 0x00008b },
                {  272, 0x8a2be2 },
                {  283, 0xa52a2a },
                {  289, 0xff4040 },
                {  296, 0xee3b3b },
                {  303, 0xcd3333 },
                {  310, 0x8b2323 },
                {  317, 0xdeb887 },
                {  327, 0xffd39b },
                {  338, 0xeec591 },
                {  349, 0xcdaa7d },
                {  360, 0x8b7355 },
                {  371, 0x5f9ea0 },
                {  381, 0x98f5ff },
                {  392, 0x8ee5ee },
                {  403, 0x7ac5cd },
                {  414, 0x53868b },
                {  425, 0x7fff00 },
                {  436, 0x7fff00 },
                {  448, 0x76ee00 },
                {  460, 0x66cd00 },
                {  472, 0x458b00 },
                {  484, 0xd2691e },
                {  494, 0xff7f24 },
                {  505, 0xee7621 },
                {  516, 0xcd661d },
                {  527, 0x8b4513 },
                {  538, 0xff7f50 },
                {  544, 0xff7256 },
                {  551, 0xee6a50 },
                {  558, 0xcd5b45 },
                {  565, 0x8b3e2f },
                {  572, 0x6495ed },
                {  587, 0xfff8dc },
                {  596, 0xfff8dc },
                {  606, 0xeee8cd },
                {  616, 0xcdc8b1 },
                {  626, 0x8b8878 },
                {  636, 0xdc143c },
                {  644, 0x00ffff },
                {  649, 0x00ffff },
                {  655, 0x00eeee },
                {  661, 0x00cdcd },
                {  667, 0x008b8b },
                {  673, 0x00008b },
                {  682, 0x008b8b },
                {  691, 0xb8860b },
                {  705, 0xffb90f },
                {  720, 0xeead0e },
                {  735, 0xcd950c },
                {  750, 0x8b6508 },
                {  765, 0xa9a9a9 },
                {  774, 0x006400 },
                {  784, 0xa9a9a9 },
                {  793, 0xbdb76b },
                {  803, 0x8b008b },
                {  815, 0x556b2f },
                {  830, 0xcaff70 },
                {  846, 0xbcee68 },
                {  862, 0xa2cd5a },
                {  878, 0x6e8b3d },
                {  894, 0xff8c00 },
                {  905, 0xff7f00 },
                {  917, 0xee7600 },
                {  929, 0xcd6600 },
                {  941, 0x8b4500 },
                {  953, 0x9932cc },
                {  964, 0xbf3eff },
                {  976, 0xb23aee },
                {  988, 0x9a32cd },
                { 1000, 0x68228b },
                { 1012, 0x8b0000 },
                { 1020, 0xe9967a },
                { 1031, 0x8fbc8f },
                { 1044, 0xc1ffc1 },
                { 1058, 0xb4eeb4 },
                { 1072, 0x9bcd9b },
                { 1086, 0x698b69 },
                { 1100, 0x483d8b },
                { 1114, 0x2f4f4f },
                { 1128, 0x97ffff },
                { 1143, 0x8deeee },
                { 1158, 0x79cdcd },
                { 1173, 0x528b8b },
                { 1188, 0x2f4f4f },
                { 1202, 0x00ced1 },
                { 1216, 0x9400d3 },
                { 1227, 0xff1493 },
                { 1236, 0xff1493 },
                { 1246, 0xee1289 },
                { 1256, 0xcd1076 },
                { 1266, 0x8b0a50 },
                { 1276, 0x00bfff },
                { 1288, 0x00bfff },
                { 1301, 0x00b2ee },
                { 1314, 0x009acd },
                { 1327, 0x00688b },
                { 1340, 0x696969 },
                { 1348, 0x696969 },
                { 1356, 0x1e90ff },
                { 1367, 0x1e90ff },
                { 1379, 0x1c86ee },
                { 1391, 0x1874cd },
                { 1403, 0x104e8b },
                { 1415, 0xb22222 },
                { 1425, 0xff3030 },
                { 1436, 0xee2c2c },
                { 1447, 0xcd2626 },
                { 1458, 0x8b1a1a },
                { 1469, 0xfffaf0 },
                { 1481, 0x228b22 },
                { 1493, 0xff00ff },
                { 1501, 0xdcdcdc },
                { 1511, 0xf8f8ff },
                { 1522, 0xffd700 },
                { 1527, 0xffd700 },
                { 1533, 0xeec900 },
                { 1539, 0xcdad00 },
                { 1545, 0x8b7500 },
                { 1551, 0xdaa520 },
                { 1561, 0xffc125 },
                { 1572, 0xeeb422 },
                { 1583, 0xcd9b1d },
                { 1594, 0x8b6914 },
                { 1605, 0xbebebe },
                { 1610, 0x000000 },
                { 1616, 0x030303 },
                { 1622, 0x1a1a1a },
                { 1629, 0xffffff },
                { 1637, 0x1c1c1c },
                { 1644, 0x1f1f1f },
                { 1651, 0x212121 },
                { 1658, 0x242424 },
                { 1665, 0x262626 },
                { 1672, 0x292929 },
                { 1679, 0x2b2b2b },
                { 1686, 0x2e2e2e },
                { 1693, 0x303030 },
                { 1700, 0x050505 },
                { 1706, 0x333333 },
                { 1713, 0x363636 },
                { 1720, 0x383838 },
                { 1727, 0x3b3b3b },
                { 1734, 0x3d3d3d },
                { 1741, 0x404040 },
                { 1748, 0x424242 },
                { 1755, 0x454545 },
                { 1762, 0x474747 },
                { 1769, 0x4a4a4a },
                { 1776, 0x080808 },
                { 1782, 0x4d4d4d },
                { 1789, 0x4f4f4f },
                { 1796, 0x525252 },
                { 1803, 0x545454 },
                { 1810, 0x575757 },
                { 1817, 0x595959 },
                { 1824, 0x5c5c5c },
                { 1831, 0x5e5e5e },
                { 1838, 0x616161 },
                { 1845, 0x636363 },
                { 1852, 0x0a0a0a },
                { 1858, 0x666666 },
                { 1865, 0x696969 },
                { 1872, 0x6b6b6b },
                { 1879, 0x6e6e6e },
                { 1886, 0x707070 },
                { 1893, 0x737373 },
                { 1900, 0x757575 },
                { 1907, 0x787878 },
                { 1914, 0x7a7a7a },
                { 1921, 0x7d7d7d },
                { 1928, 0x0d0d0d },
                { 1934, 0x7f7f7f },
                { 1941, 0x828282 },
                { 1948, 0x858585 },
                { 1955, 0x878787 },
                { 1962, 0x8a8a8a },
                { 1969, 0x8c8c8c },
                { 1976, 0x8f8f8f },
                { 1983, 0x919191 },
                { 1990, 0x949494 },
                { 1997, 0x969696 },
                { 2004, 0x0f0f0f },
                { 2010, 0x999999 },
                { 2017, 0x9c9c9c },
                { 2024, 0x9e9e9e },
                { 2031, 0xa1a1a1 },
                { 2038, 0xa3a3a3 },
                { 2045, 0xa6a6a6 },
                { 2052, 0xa8a8a8 },
                { 2059, 0xababab },
                { 2066, 0xadadad },
                { 2073, 0xb0b0b0 },
                { 2080, 0x121212 },
                { 2086, 0xb3b3b3 },
                { 2093, 0xb5b5b5 },
                { 2100, 0xb8b8b8 },
                { 2107, 0xbababa },
                { 2114, 0xbdbdbd },
                { 2121, 0xbfbfbf },
                { 2128, 0xc2c2c2 },
                { 2135, 0xc4c4c4 },
                { 2142, 0xc7c7c7 },
                { 2149, 0xc9c9c9 },
                { 2156, 0x141414 },
                { 2162, 0xcccccc },
                { 2169, 0xcfcfcf },
                { 2176, 0xd1d1d1 },
                { 2183, 0xd4d4d4 },
                { 2190, 0xd6d6d6 },
                { 2197, 0xd9d9d9 },
                { 2204, 0xdbdbdb },
                { 2211, 0xdedede },
                { 2218, 0xe0e0e0 },
                { 2225, 0xe3e3e3 },
                { 2232, 0x171717 },
                { 2238, 0xe5e5e5 },
                { 2245, 0xe8e8e8 },
                { 2252, 0xebebeb },
                { 2259, 0xededed },
                { 2266, 0xf0f0f0 },
                { 2273, 0xf2f2f2 },
                { 2280, 0xf5f5f5 },
                { 2287, 0xf7f7f7 },
                { 2294, 0xfafafa },
                { 2301, 0xfcfcfc },
                { 2308, 0x00ff00 },
                { 2314, 0x00ff00 },
                { 2321, 0x00ee00 },
                { 2328, 0x00cd00 },
                { 2335, 0x008b00 },
                { 2342, 0xadff2f },
                { 2354, 0xbebebe },
                { 2359, 0x000000 },
                { 2365, 0x030303 },
                { 2371, 0x1a1a1a },
                { 2378, 0xffffff },
                { 2386, 0x1c1c1c },
                { 2393, 0x1f1f1f },
                { 2400, 0x212121 },
                { 2407, 0x242424 },
                { 2414, 0x262626 },
                { 2421, 0x292929 },
                { 2428, 0x2b2b2b },
                { 2435, 0x2e2e2e },
                { 2442, 0x303030 },
                { 2449, 0x050505 },
                { 2455, 0x333333 },
                { 2462, 0x363636 },
                { 2469, 0x383838 },
                { 2476, 0x3b3b3b },
                { 2483, 0x3d3d3d },
                { 2490, 0x404040 },
                { 2497, 0x424242 },
                { 2504, 0x454545 },
                { 2511, 0x474747 },
                { 2518, 0x4a4a4a },
                { 2525, 0x080808 },
                { 2531, 0x4d4d4d },
                { 2538, 0x4f4f4f },
                { 2545, 0x525252 },
                { 2552, 0x545454 },
                { 2559, 0x575757 },
                { 2566, 0x595959 },
                { 2573, 0x5c5c5c },
                { 2580, 0x5e5e5e },
                { 2587, 0x616161 },
                { 2594, 0x636363 },
                { 2601, 0x0a0a0a },
                { 2607, 0x666666 },
                { 2614, 0x696969 },
                { 2621, 0x6b6b6b },
                { 2628, 0x6e6e6e },
                { 2635, 0x707070 },
                { 2642, 0x737373 },
                { 2649, 0x757575 },
                { 2656, 0x787878 },
                { 2663, 0x7a7a7a },
                { 2670, 0x7d7d7d },
                { 2677, 0x0d0d0d },
                { 2683, 0x7f7f7f },
                { 2690, 0x828282 },
                { 2697, 0x858585 },
                { 2704, 0x878787 },
                { 2711, 0x8a8a8a },
                { 2718, 0x8c8c8c },
                { 2725, 0x8f8f8f },
                { 2732, 0x919191 },
                { 2739, 0x949494 },
                { 2746, 0x969696 },
                { 2753, 0x0f0f0f },
                { 2759, 0x999999 },
                { 2766, 0x9c9c9c },
                { 2773, 0x9e9e9e },
                { 2780, 0xa1a1a1 },
                { 2787, 0xa3a3a3 },
                { 2794, 0xa6a6a6 },
                { 2801, 0xa8a8a8 },
                { 2808, 0xababab },
                { 2815, 0xadadad },
                { 2822, 0xb0b0b0 },
                { 2829, 0x121212 },
                { 2835, 0xb3b3b3 },
                { 2842, 0xb5b5b5 },
                { 2849, 0xb8b8b8 },
                { 2856, 0xbababa },
                { 2863, 0xbdbdbd },
                { 2870, 0xbfbfbf },
                { 2877, 0xc2c2c2 },
                { 2884, 0xc4c4c4 },
                { 2891, 0xc7c7c7 },
                { 2898, 0xc9c9c9 },
                { 2905, 0x141414 },
                { 2911, 0xcccccc },
                { 2918, 0xcfcfcf },
                { 2925, 0xd1d1d1 },
                { 2932, 0xd4d4d4 },
                { 2939, 0xd6d6d6 },
                { 2946, 0xd9d9d9 },
                { 2953, 0xdbdbdb },
                { 2960, 0xdedede },
                { 2967, 0xe0e0e0 },
                { 2974, 0xe3e3e3 },
                { 2981, 0x171717 },
                { 2987, 0xe5e5e5 },
                { 2994, 0xe8e8e8 },
                { 3001, 0xebebeb },
                { 3008, 0xededed },
                { 3015, 0xf0f0f0 },
                { 3022, 0xf2f2f2 },
                { 3029, 0xf5f5f5 },
                { 3036, 0xf7f7f7 },
                { 3043, 0xfafafa },
                { 3050, 0xfcfcfc },
                { 3057, 0xf0fff0 },
                { 3066, 0xf0fff0 },
                { 3076, 0xe0eee0 },
                { 3086, 0xc1cdc1 },
                { 3096, 0x838b83 },
                { 3106, 0xff69b4 },
                { 3114, 0xff6eb4 },
                { 3123, 0xee6aa7 },
                { 3132, 0xcd6090 },
                { 3141, 0x8b3a62 },
                { 3150, 0xcd5c5c },
                { 3160, 0xff6a6a },
                { 3171, 0xee6363 },
                { 3182, 0xcd5555 },
                { 3193, 0x8b3a3a },
                { 3204, 0x4b0082 },
                { 3211, 0xfffff0 },
                { 3217, 0xfffff0 },
                { 3224, 0xeeeee0 },
                { 3231, 0xcdcdc1 },
                { 3238, 0x8b8b83 },
                { 3245, 0xf0e68c },
                { 3251, 0xfff68f },
                { 3258, 0xeee685 },
                { 3265, 0xcdc673 },
                { 3272, 0x8b864e },
                { 3279, 0xe6e6fa },
                { 3288, 0xfff0f5 },
                { 3302, 0xfff0f5 },
                { 3317, 0xeee0e5 },
                { 3332, 0xcdc1c5 },
                { 3347, 0x8b8386 },
                { 3362, 0x7cfc00 },
                { 3372, 0xfffacd },
                { 3385, 0xfffacd },
                { 3399, 0xeee9bf },
                { 3413, 0xcdc9a5 },
                { 3427, 0x8b8970 },
                { 3441, 0xadd8e6 },
                { 3451, 0xbfefff },
                { 3462, 0xb2dfee },
                { 3473, 0x9ac0cd },
                { 3484, 0x68838b },
                { 3495, 0xf08080 },
                { 3506, 0xe0ffff },
                { 3516, 0xe0ffff },
                { 3527, 0xd1eeee },
                { 3538, 0xb4cdcd },
                { 3549, 0x7a8b8b },
                { 3560, 0xeedd82 },
                { 3575, 0xffec8b },
                { 3591, 0xeedc82 },
                { 3607, 0xcdbe70 },
                { 3623, 0x8b814c },
                { 3639, 0xfafad2 },
                { 3660, 0xd3d3d3 },
                { 3670, 0x90ee90 },
                { 3681, 0xd3d3d3 },
                { 3691, 0xffb6c1 },
                { 3701, 0xffaeb9 },
                { 3712, 0xeea2ad },
                { 3723, 0xcd8c95 },
                { 3734, 0x8b5f65 },
                { 3745, 0xffa07a },
                { 3757, 0xffa07a },
                { 3770, 0xee9572 },
                { 3783, 0xcd8162 },
                { 3796, 0x8b5742 },
                { 3809, 0x20b2aa },
                { 3823, 0x87cefa },
                { 3836, 0xb0e2ff },
                { 3850, 0xa4d3ee },
                { 3864, 0x8db6cd },
                { 3878, 0x607b8b },
                { 3892, 0x8470ff },
                { 3907, 0x778899 },
                { 3922, 0x778899 },
                { 3937, 0xb0c4de },
                { 3952, 0xcae1ff },
                { 3968, 0xbcd2ee },
                { 3984, 0xa2b5cd },
                { 4000, 0x6e7b8b },
                { 4016, 0xffffe0 },
                { 4028, 0xffffe0 },
                { 4041, 0xeeeed1 },
                { 4054, 0xcdcdb4 },
                { 4067, 0x8b8b7a },
                { 4080, 0x00ff00 },
                { 4085, 0x32cd32 },
                { 4095, 0xfaf0e6 },
                { 4101, 0xff00ff },
                { 4109, 0xff00ff },
                { 4118, 0xee00ee },
                { 4127, 0xcd00cd },
                { 4136, 0x8b008b },
                { 4145, 0xb03060 },
                { 4152, 0xff34b3 },
                { 4160, 0xee30a7 },
                { 4168, 0xcd2990 },
                { 4176, 0x8b1c62 },
                { 4184, 0x66cdaa },
                { 4201, 0x0000cd },
                { 4212, 0xba55d3 },
                { 4225, 0xe066ff },
                { 4239, 0xd15fee },
                { 4253, 0xb452cd },
                { 4267, 0x7a378b },
                { 4281, 0x9370db },
                { 4294, 0xab82ff },
                { 4308, 0x9f79ee },
                { 4322, 0x8968cd },
                { 4336, 0x5d478b },
                { 4350, 0x3cb371 },
                { 4365, 0x7b68ee },
                { 4381, 0x00fa9a },
                { 4399, 0x48d1cc },
                { 4415, 0xc71585 },
                { 4431, 0x191970 },
                { 4444, 0xf5fffa },
                { 4454, 0xffe4e1 },
                { 4464, 0xffe4e1 },
                { 4475, 0xeed5d2 },
                { 4486, 0xcdb7b5 },
                { 4497, 0x8b7d7b },
                { 4508, 0xffe4b5 },
                { 4517, 0xffdead },
                { 4529, 0xffdead },
                { 4542, 0xeecfa1 },
                { 4555, 0xcdb38b },
                { 4568, 0x8b795e },
                { 4581, 0x000080 },
                { 4586, 0x000080 },
                { 4595, 0xfdf5e6 },
                { 4603, 0x808000 },
                { 4609, 0x6b8e23 },
                { 4619, 0xc0ff3e },
                { 4630, 0xb3ee3a },
                { 4641, 0x9acd32 },
                { 4652, 0x698b22 },
                { 4663, 0xffa500 },
                { 4670, 0xffa500 },
                { 4678, 0xee9a00 },
                { 4686, 0xcd8500 },
                { 4694, 0x8b5a00 },
                { 4702, 0xff4500 },
                { 4712, 0xff4500 },
                { 4723, 0xee4000 },
                { 4734, 0xcd3700 },
                { 4745, 0x8b2500 },
                { 4756, 0xda70d6 },
                { 4763, 0xff83fa },
                { 4771, 0xee7ae9 },
                { 4779, 0xcd69c9 },
                { 4787, 0x8b4789 },
                { 4795, 0xeee8aa },
                { 4809, 0x98fb98 },
                { 4819, 0x9aff9a },
                { 4830, 0x90ee90 },
                { 4841, 0x7ccd7c },
                { 4852, 0x548b54 },
                { 4863, 0xafeeee },
                { 4877, 0xbbffff },
                { 4892, 0xaeeeee },
                { 4907, 0x96cdcd },
                { 4922, 0x668b8b },
                { 4937, 0xdb7093 },
                { 4951, 0xff82ab },
                { 4966, 0xee799f },
                { 4981, 0xcd6889 },
                { 4996, 0x8b475d },
                { 5011, 0xffefd5 },
                { 5022, 0xffdab9 },
                { 5032, 0xffdab9 },
                { 5043, 0xeecbad },
                { 5054, 0xcdaf95 },
                { 5065, 0x8b7765 },
                { 5076, 0xcd853f },
                { 5081, 0xffc0cb },
                { 5086, 0xffb5c5 },
                { 5092, 0xeea9b8 },
                { 5098, 0xcd919e },
                { 5104, 0x8b636c },
                { 5110, 0xdda0dd },
                { 5115, 0xffbbff },
                { 5121, 0xeeaeee },
                { 5127, 0xcd96cd },
                { 5133, 0x8b668b },
                { 5139, 0xb0e0e6 },
                { 5150, 0xa020f0 },
                { 5157, 0x9b30ff },
                { 5165, 0x912cee },
                { 5173, 0x7d26cd },
                { 5181, 0x551a8b },
                { 5189, 0x663399 },
                { 5203, 0xff0000 },
                { 5207, 0xff0000 },
                { 5212, 0xee0000 },
                { 5217, 0xcd0000 },
                { 5222, 0x8b0000 },
                { 5227, 0xbc8f8f },
                { 5237, 0xffc1c1 },
                { 5248, 0xeeb4b4 },
                { 5259, 0xcd9b9b },
                { 5270, 0x8b6969 },
                { 5281, 0x4169e1 },
                { 5291, 0x4876ff },
                { 5302, 0x436eee },
                { 5313, 0x3a5fcd },
                { 5324, 0x27408b },
                { 5335, 0x8b4513 },
                { 5347, 0xfa8072 },
                { 5354, 0xff8c69 },
                { 5362, 0xee8262 },
                { 5370, 0xcd7054 },
                { 5378, 0x8b4c39 },
                { 5386, 0xf4a460 },
                { 5397, 0x2e8b57 },
                { 5406, 0x54ff9f },
                { 5416, 0x4eee94 },
                { 5426, 0x43cd80 },
                { 5436, 0x2e8b57 },
                { 5446, 0xfff5ee },
                { 5455, 0xfff5ee },
                { 5465, 0xeee5de },
                { 5475, 0xcdc5bf },
                { 5485, 0x8b8682 },
                { 5495, 0xa0522d },
                { 5502, 0xff8247 },
                { 5510, 0xee7942 },
                { 5518, 0xcd6839 },
                { 5526, 0x8b4726 },
                { 5534, 0xc0c0c0 },
                { 5541, 0x87ceeb },
                { 5549, 0x87ceff },
                { 5558, 0x7ec0ee },
                { 5567, 0x6ca6cd },
                { 5576, 0x4a708b },
                { 5585, 0x6a5acd },
                { 5595, 0x836fff },
                { 5606, 0x7a67ee },
                { 5617, 0x6959cd },
                { 5628, 0x473c8b },
                { 5639, 0x708090 },
                { 5649, 0xc6e2ff },
                { 5660, 0xb9d3ee },
                { 5671, 0x9fb6cd },
                { 5682, 0x6c7b8b },
                { 5693, 0x708090 },
                { 5703, 0xfffafa },
                { 5708, 0xfffafa },
                { 5714, 0xeee9e9 },
                { 5720, 0xcdc9c9 },
                { 5726, 0x8b8989 },
                { 5732, 0x00ff7f },
                { 5744, 0x00ff7f },
                { 5757, 0x00ee76 },
                { 5770, 0x00cd66 },
                { 5783, 0x008b45 },
                { 5796, 0x4682b4 },
                { 5806, 0x63b8ff },
                { 5817, 0x5cacee },
                { 5828, 0x4f94cd },
                { 5839, 0x36648b },
                { 5850, 0xd2b48c },
                { 5854, 0xffa54f },
                { 5859, 0xee9a49 },
                { 5864, 0xcd853f },
                { 5869, 0x8b5a2b },
                { 5874, 0x008080 },
                { 5879, 0xd8bfd8 },
                { 5887, 0xffe1ff },
                { 5896, 0xeed2ee },
                { 5905, 0xcdb5cd },
                { 5914, 0x8b7b8b },
                { 5923, 0xff6347 },
                { 5930, 0xff6347 },
                { 5938, 0xee5c42 },
                { 5946, 0xcd4f39 },
                { 5954, 0x8b3626 },
                { 5962, 0x40e0d0 },
                { 5972, 0x00f5ff },
                { 5983, 0x00e5ee },
                { 5994, 0x00c5cd },
                { 6005, 0x00868b },
                { 6016, 0xee82ee },
                { 6023, 0xd02090 },
                { 6033, 0xff3e96 },
                { 6044, 0xee3a8c },
                { 6055, 0xcd3278 },
                { 6066, 0x8b2252 },
                { 6077, 0x808080 },
                { 6085, 0x008000 },
                { 6094, 0x808080 },
                { 6102, 0x800000 },
                { 6112, 0x800080 },
                { 6122, 0xf5deb3 },
                { 6128, 0xffe7ba },
                { 6135, 0xeed8ae },
                { 6142, 0xcdba96 },
                { 6149, 0x8b7e66 },
                { 6156, 0xffffff },
                { 6162, 0xf5f5f5 },
                { 6173, 0xbebebe },
                { 6181, 0x00ff00 },
                { 6190, 0xbebebe },
                { 6198, 0xb03060 },
                { 6208, 0xa020f0 },
                { 6218, 0xffff00 },
                { 6225, 0xffff00 },
                { 6233, 0xeeee00 },
                { 6241, 0xcdcd00 },
                { 6249, 0x8b8b00 },
                { 6257, 0x9acd32 },
        };

} // namespace vte::color
