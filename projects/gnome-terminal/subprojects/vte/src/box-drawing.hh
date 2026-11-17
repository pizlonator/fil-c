/*
 * Copyright © 2014, 2023 Egmont Koblinger
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

#pragma once

/*
 * Define the user-defined literal suffix "_str2bin" which interprets a
 * byte string as binary number.
 * Even bytes (e.g. ' ', '.', '0') mean bit 0.
 * Odd  bytes (e.g. '#', '%', '1') mean bit 1.
 */
constexpr uint32_t operator""_str2bin(char const* str,
                                      std::size_t len)
{
        auto val = uint32_t{0};
        while (*str) {
                val = (val << 1) | (static_cast<unsigned char>(*str) & 1);
                ++str;
        }
        return val;
}

/*
 * Definition of most of the glyphs in the 2500..257F range as 5x5 bitmaps
 * (bits 24..0 in the obvious order), see bug 709556 and ../doc/boxes.txt
 */
static constinit uint32_t const _vte_draw_box_drawing_bitmaps[128] = {

        /* U+2500 - BOX DRAWINGS LIGHT HORIZONTAL */
        "     "
        "     "
        "#####"
        "     "
        "     "_str2bin,

        /* U+2501 - BOX DRAWINGS HEAVY HORIZONTAL */
        "     "
        "#####"
        "#####"
        "#####"
        "     "_str2bin,

        /* U+2502 - BOX DRAWINGS LIGHT VERTICAL */
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "_str2bin,

        /* U+2503 - BOX DRAWINGS HEAVY VERTICAL */
        " ### "
        " ### "
        " ### "
        " ### "
        " ### "_str2bin,

        /* U+2504 - BOX DRAWINGS LIGHT TRIPLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+2505 - BOX DRAWINGS HEAVY TRIPLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+2506 - BOX DRAWINGS LIGHT TRIPLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+2507 - BOX DRAWINGS HEAVY TRIPLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+2508 - BOX DRAWINGS LIGHT QUADRUPLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+2509 - BOX DRAWINGS HEAVY QUADRUPLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+250A - BOX DRAWINGS LIGHT QUADRUPLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+250B - BOX DRAWINGS HEAVY QUADRUPLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+250C - BOX DRAWINGS LIGHT DOWN AND RIGHT */
        "     "
        "     "
        "  ###"
        "  #  "
        "  #  "_str2bin,

        /* U+250D - BOX DRAWINGS DOWN LIGHT AND RIGHT HEAVY */
        "     "
        "  ###"
        "  ###"
        "  ###"
        "  #  "_str2bin,

        /* U+250E - BOX DRAWINGS DOWN HEAVY AND RIGHT LIGHT */
        "     "
        "     "
        " ####"
        " ### "
        " ### "_str2bin,

        /* U+250F - BOX DRAWINGS HEAVY DOWN AND RIGHT */
        "     "
        " ####"
        " ####"
        " ####"
        " ### "_str2bin,

        /* U+2510 - BOX DRAWINGS LIGHT DOWN AND LEFT */
        "     "
        "     "
        "###  "
        "  #  "
        "  #  "_str2bin,

        /* U+2511 - BOX DRAWINGS DOWN LIGHT AND LEFT HEAVY */
        "     "
        "###  "
        "###  "
        "###  "
        "  #  "_str2bin,

        /* U+2512 - BOX DRAWINGS DOWN HEAVY AND LEFT LIGHT */
        "     "
        "     "
        "#### "
        " ### "
        " ### "_str2bin,

        /* U+2513 - BOX DRAWINGS HEAVY DOWN AND LEFT */
        "     "
        "#### "
        "#### "
        "#### "
        " ### "_str2bin,

        /* U+2514 - BOX DRAWINGS LIGHT UP AND RIGHT */
        "  #  "
        "  #  "
        "  ###"
        "     "
        "     "_str2bin,

        /* U+2515 - BOX DRAWINGS UP LIGHT AND RIGHT HEAVY */
        "  #  "
        "  ###"
        "  ###"
        "  ###"
        "     "_str2bin,

        /* U+2516 - BOX DRAWINGS UP HEAVY AND RIGHT LIGHT */
        " ### "
        " ### "
        " ####"
        "     "
        "     "_str2bin,

        /* U+2517 - BOX DRAWINGS HEAVY UP AND RIGHT */
        " ### "
        " ####"
        " ####"
        " ####"
        "     "_str2bin,

        /* U+2518 - BOX DRAWINGS LIGHT UP AND LEFT */
        "  #  "
        "  #  "
        "###  "
        "     "
        "     "_str2bin,

        /* U+2519 - BOX DRAWINGS UP LIGHT AND LEFT HEAVY */
        "  #  "
        "###  "
        "###  "
        "###  "
        "     "_str2bin,

        /* U+251A - BOX DRAWINGS UP HEAVY AND LEFT LIGHT */
        " ### "
        " ### "
        "#### "
        "     "
        "     "_str2bin,

        /* U+251B - BOX DRAWINGS HEAVY UP AND LEFT */
        " ### "
        "#### "
        "#### "
        "#### "
        "     "_str2bin,

        /* U+251C - BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
        "  #  "
        "  #  "
        "  ###"
        "  #  "
        "  #  "_str2bin,

        /* U+251D - BOX DRAWINGS VERTICAL LIGHT AND RIGHT HEAVY */
        "  #  "
        "  ###"
        "  ###"
        "  ###"
        "  #  "_str2bin,

        /* U+251E - BOX DRAWINGS UP HEAVY AND RIGHT DOWN LIGHT */
        " ### "
        " ### "
        " ####"
        "  #  "
        "  #  "_str2bin,

        /* U+251F - BOX DRAWINGS DOWN HEAVY AND RIGHT UP LIGHT */
        "  #  "
        "  #  "
        " ####"
        " ### "
        " ### "_str2bin,

        /* U+2520 - BOX DRAWINGS VERTICAL HEAVY AND RIGHT LIGHT */
        " ### "
        " ### "
        " ####"
        " ### "
        " ### "_str2bin,

        /* U+2521 - BOX DRAWINGS DOWN LIGHT AND RIGHT UP HEAVY */
        " ### "
        " ####"
        " ####"
        " ####"
        "  #  "_str2bin,

        /* U+2522 - BOX DRAWINGS UP LIGHT AND RIGHT DOWN HEAVY */
        "  #  "
        " ####"
        " ####"
        " ####"
        " ### "_str2bin,

        /* U+2523 - BOX DRAWINGS HEAVY VERTICAL AND RIGHT */
        " ### "
        " ####"
        " ####"
        " ####"
        " ### "_str2bin,

        /* U+2524 - BOX DRAWINGS LIGHT VERTICAL AND LEFT */
        "  #  "
        "  #  "
        "###  "
        "  #  "
        "  #  "_str2bin,

        /* U+2525 - BOX DRAWINGS VERTICAL LIGHT AND LEFT HEAVY */
        "  #  "
        "###  "
        "###  "
        "###  "
        "  #  "_str2bin,

        /* U+2526 - BOX DRAWINGS UP HEAVY AND LEFT DOWN LIGHT */
        " ### "
        " ### "
        "#### "
        "  #  "
        "  #  "_str2bin,

        /* U+2527 - BOX DRAWINGS DOWN HEAVY AND LEFT UP LIGHT */
        "  #  "
        "  #  "
        "#### "
        " ### "
        " ### "_str2bin,

        /* U+2528 - BOX DRAWINGS VERTICAL HEAVY AND LEFT LIGHT */
        " ### "
        " ### "
        "#### "
        " ### "
        " ### "_str2bin,

        /* U+2529 - BOX DRAWINGS DOWN LIGHT AND LEFT UP HEAVY */
        " ### "
        "#### "
        "#### "
        "#### "
        "  #  "_str2bin,

        /* U+252A - BOX DRAWINGS UP LIGHT AND LEFT DOWN HEAVY */
        "  #  "
        "#### "
        "#### "
        "#### "
        " ### "_str2bin,

        /* U+252B - BOX DRAWINGS HEAVY VERTICAL AND LEFT */
        " ### "
        "#### "
        "#### "
        "#### "
        " ### "_str2bin,

        /* U+252C - BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
        "     "
        "     "
        "#####"
        "  #  "
        "  #  "_str2bin,

        /* U+252D - BOX DRAWINGS LEFT HEAVY AND RIGHT DOWN LIGHT */
        "     "
        "###  "
        "#####"
        "###  "
        "  #  "_str2bin,

        /* U+252E - BOX DRAWINGS RIGHT HEAVY AND LEFT DOWN LIGHT */
        "     "
        "  ###"
        "#####"
        "  ###"
        "  #  "_str2bin,

        /* U+252F - BOX DRAWINGS DOWN LIGHT AND HORIZONTAL HEAVY */
        "     "
        "#####"
        "#####"
        "#####"
        "  #  "_str2bin,

        /* U+2530 - BOX DRAWINGS DOWN HEAVY AND HORIZONTAL LIGHT */
        "     "
        "     "
        "#####"
        " ### "
        " ### "_str2bin,

        /* U+2531 - BOX DRAWINGS RIGHT LIGHT AND LEFT DOWN HEAVY */
        "     "
        "#### "
        "#####"
        "#### "
        " ### "_str2bin,

        /* U+2532 - BOX DRAWINGS LEFT LIGHT AND RIGHT DOWN HEAVY */
        "     "
        " ####"
        "#####"
        " ####"
        " ### "_str2bin,

        /* U+2533 - BOX DRAWINGS HEAVY DOWN AND HORIZONTAL */
        "     "
        "#####"
        "#####"
        "#####"
        " ### "_str2bin,

        /* U+2534 - BOX DRAWINGS LIGHT UP AND HORIZONTAL */
        "  #  "
        "  #  "
        "#####"
        "     "
        "     "_str2bin,

        /* U+2535 - BOX DRAWINGS LEFT HEAVY AND RIGHT UP LIGHT */
        "  #  "
        "###  "
        "#####"
        "###  "
        "     "_str2bin,

        /* U+2536 - BOX DRAWINGS RIGHT HEAVY AND LEFT UP LIGHT */
        "  #  "
        "  ###"
        "#####"
        "  ###"
        "     "_str2bin,

        /* U+2537 - BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY */
        "  #  "
        "#####"
        "#####"
        "#####"
        "     "_str2bin,

        /* U+2538 - BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT */
        " ### "
        " ### "
        "#####"
        "     "
        "     "_str2bin,

        /* U+2539 - BOX DRAWINGS RIGHT LIGHT AND LEFT UP HEAVY */
        " ### "
        "#### "
        "#####"
        "#### "
        "     "_str2bin,

        /* U+253A - BOX DRAWINGS LEFT LIGHT AND RIGHT UP HEAVY */
        " ### "
        " ####"
        "#####"
        " ####"
        "     "_str2bin,

        /* U+253B - BOX DRAWINGS HEAVY UP AND HORIZONTAL */
        " ### "
        "#####"
        "#####"
        "#####"
        "     "_str2bin,

        /* U+253C - BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
        "  #  "
        "  #  "
        "#####"
        "  #  "
        "  #  "_str2bin,

        /* U+253D - BOX DRAWINGS LEFT HEAVY AND RIGHT VERTICAL LIGHT */
        "  #  "
        "###  "
        "#####"
        "###  "
        "  #  "_str2bin,

        /* U+253E - BOX DRAWINGS RIGHT HEAVY AND LEFT VERTICAL LIGHT */
        "  #  "
        "  ###"
        "#####"
        "  ###"
        "  #  "_str2bin,

        /* U+253F - BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY */
        "  #  "
        "#####"
        "#####"
        "#####"
        "  #  "_str2bin,

        /* U+2540 - BOX DRAWINGS UP HEAVY AND DOWN HORIZONTAL LIGHT */
        " ### "
        " ### "
        "#####"
        "  #  "
        "  #  "_str2bin,

        /* U+2541 - BOX DRAWINGS DOWN HEAVY AND UP HORIZONTAL LIGHT */
        "  #  "
        "  #  "
        "#####"
        " ### "
        " ### "_str2bin,

        /* U+2542 - BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT */
        " ### "
        " ### "
        "#####"
        " ### "
        " ### "_str2bin,

        /* U+2543 - BOX DRAWINGS LEFT UP HEAVY AND RIGHT DOWN LIGHT */
        " ### "
        "#### "
        "#####"
        "#### "
        "  #  "_str2bin,

        /* U+2544 - BOX DRAWINGS RIGHT UP HEAVY AND LEFT DOWN LIGHT */
        " ### "
        " ####"
        "#####"
        " ####"
        "  #  "_str2bin,

        /* U+2545 - BOX DRAWINGS LEFT DOWN HEAVY AND RIGHT UP LIGHT */
        "  #  "
        "#### "
        "#####"
        "#### "
        " ### "_str2bin,

        /* U+2546 - BOX DRAWINGS RIGHT DOWN HEAVY AND LEFT UP LIGHT */
        "  #  "
        " ####"
        "#####"
        " ####"
        " ### "_str2bin,

        /* U+2547 - BOX DRAWINGS DOWN LIGHT AND UP HORIZONTAL HEAVY */
        " ### "
        "#####"
        "#####"
        "#####"
        "  #  "_str2bin,

        /* U+2548 - BOX DRAWINGS UP LIGHT AND DOWN HORIZONTAL HEAVY */
        "  #  "
        "#####"
        "#####"
        "#####"
        " ### "_str2bin,

        /* U+2549 - BOX DRAWINGS RIGHT LIGHT AND LEFT VERTICAL HEAVY */
        " ### "
        "#### "
        "#####"
        "#### "
        " ### "_str2bin,

        /* U+254A - BOX DRAWINGS LEFT LIGHT AND RIGHT VERTICAL HEAVY */
        " ### "
        " ####"
        "#####"
        " ####"
        " ### "_str2bin,

        /* U+254B - BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL */
        " ### "
        "#####"
        "#####"
        "#####"
        " ### "_str2bin,

        /* U+254C - BOX DRAWINGS LIGHT DOUBLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+254D - BOX DRAWINGS HEAVY DOUBLE DASH HORIZONTAL */
        0,  /* not handled here */

        /* U+254E - BOX DRAWINGS LIGHT DOUBLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+254F - BOX DRAWINGS HEAVY DOUBLE DASH VERTICAL */
        0,  /* not handled here */

        /* U+2550 - BOX DRAWINGS DOUBLE HORIZONTAL */
        "     "
        "#####"
        "     "
        "#####"
        "     "_str2bin,

        /* U+2551 - BOX DRAWINGS DOUBLE VERTICAL */
        " # # "
        " # # "
        " # # "
        " # # "
        " # # "_str2bin,

        /* U+2552 - BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE */
        "     "
        "  ###"
        "  #  "
        "  ###"
        "  #  "_str2bin,

        /* U+2553 - BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE */
        "     "
        "     "
        " ####"
        " # # "
        " # # "_str2bin,

        /* U+2554 - BOX DRAWINGS DOUBLE DOWN AND RIGHT */
        "     "
        " ####"
        " #   "
        " # ##"
        " # # "_str2bin,

        /* U+2555 - BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE */
        "     "
        "###  "
        "  #  "
        "###  "
        "  #  "_str2bin,

        /* U+2556 - BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE */
        "     "
        "     "
        "#### "
        " # # "
        " # # "_str2bin,

        /* U+2557 - BOX DRAWINGS DOUBLE DOWN AND LEFT */
        "     "
        "#### "
        "   # "
        "## # "
        " # # "_str2bin,

        /* U+2558 - BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE */
        "  #  "
        "  ###"
        "  #  "
        "  ###"
        "     "_str2bin,

        /* U+2559 - BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE */
        " # # "
        " # # "
        " ####"
        "     "
        "     "_str2bin,

        /* U+255A - BOX DRAWINGS DOUBLE UP AND RIGHT */
        " # # "
        " # ##"
        " #   "
        " ####"
        "     "_str2bin,

        /* U+255B - BOX DRAWINGS UP SINGLE AND LEFT DOUBLE */
        "  #  "
        "###  "
        "  #  "
        "###  "
        "     "_str2bin,

        /* U+255C - BOX DRAWINGS UP DOUBLE AND LEFT SINGLE */
        " # # "
        " # # "
        "#### "
        "     "
        "     "_str2bin,

        /* U+255D - BOX DRAWINGS DOUBLE UP AND LEFT */
        " # # "
        "## # "
        "   # "
        "#### "
        "     "_str2bin,

        /* U+255E - BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE */
        "  #  "
        "  ###"
        "  #  "
        "  ###"
        "  #  "_str2bin,

        /* U+255F - BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE */
        " # # "
        " # # "
        " # ##"
        " # # "
        " # # "_str2bin,

        /* U+2560 - BOX DRAWINGS DOUBLE VERTICAL AND RIGHT */
        " # # "
        " # ##"
        " #   "
        " # ##"
        " # # "_str2bin,

        /* U+2561 - BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE */
        "  #  "
        "###  "
        "  #  "
        "###  "
        "  #  "_str2bin,

        /* U+2562 - BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE */
        " # # "
        " # # "
        "## # "
        " # # "
        " # # "_str2bin,

        /* U+2563 - BOX DRAWINGS DOUBLE VERTICAL AND LEFT */
        " # # "
        "## # "
        "   # "
        "## # "
        " # # "_str2bin,

        /* U+2564 - BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE */
        "     "
        "#####"
        "     "
        "#####"
        "  #  "_str2bin,

        /* U+2565 - BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE */
        "     "
        "     "
        "#####"
        " # # "
        " # # "_str2bin,

        /* U+2566 - BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL */
        "     "
        "#####"
        "     "
        "## ##"
        " # # "_str2bin,

        /* U+2567 - BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE */
        "  #  "
        "#####"
        "     "
        "#####"
        "     "_str2bin,

        /* U+2568 - BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE */
        " # # "
        " # # "
        "#####"
        "     "
        "     "_str2bin,

        /* U+2569 - BOX DRAWINGS DOUBLE UP AND HORIZONTAL */
        " # # "
        "## ##"
        "     "
        "#####"
        "     "_str2bin,

        /* U+256A - BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE */
        "  #  "
        "#####"
        "  #  "
        "#####"
        "  #  "_str2bin,

        /* U+256B - BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE */
        " # # "
        " # # "
        "#####"
        " # # "
        " # # "_str2bin,

        /* U+256C - BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL */
        " # # "
        "## ##"
        "     "
        "## ##"
        " # # "_str2bin,

        /* U+256D - BOX DRAWINGS LIGHT ARC DOWN AND RIGHT */
        0,  /* not handled here */

        /* U+256E - BOX DRAWINGS LIGHT ARC DOWN AND LEFT */
        0,  /* not handled here */

        /* U+256F - BOX DRAWINGS LIGHT ARC UP AND LEFT */
        0,  /* not handled here */

        /* U+2570 - BOX DRAWINGS LIGHT ARC UP AND RIGHT */
        0,  /* not handled here */

        /* U+2571 - BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT */
        0,  /* not handled here */

        /* U+2572 - BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT */
        0,  /* not handled here */

        /* U+2573 - BOX DRAWINGS LIGHT DIAGONAL CROSS */
        0,  /* not handled here */

        /* U+2574 - BOX DRAWINGS LIGHT LEFT */
        "     "
        "     "
        "###  "
        "     "
        "     "_str2bin,

        /* U+2575 - BOX DRAWINGS LIGHT UP */
        "  #  "
        "  #  "
        "  #  "
        "     "
        "     "_str2bin,

        /* U+2576 - BOX DRAWINGS LIGHT RIGHT */
        "     "
        "     "
        "  ###"
        "     "
        "     "_str2bin,

        /* U+2577 - BOX DRAWINGS LIGHT DOWN */
        "     "
        "     "
        "  #  "
        "  #  "
        "  #  "_str2bin,

        /* U+2578 - BOX DRAWINGS HEAVY LEFT */
        "     "
        "###  "
        "###  "
        "###  "
        "     "_str2bin,

        /* U+2579 - BOX DRAWINGS HEAVY UP */
        " ### "
        " ### "
        " ### "
        "     "
        "     "_str2bin,

        /* U+257A - BOX DRAWINGS HEAVY RIGHT */
        "     "
        "  ###"
        "  ###"
        "  ###"
        "     "_str2bin,

        /* U+257B - BOX DRAWINGS HEAVY DOWN */
        "     "
        "     "
        " ### "
        " ### "
        " ### "_str2bin,

        /* U+257C - BOX DRAWINGS LIGHT LEFT AND HEAVY RIGHT */
        "     "
        "  ###"
        "#####"
        "  ###"
        "     "_str2bin,

        /* U+257D - BOX DRAWINGS LIGHT UP AND HEAVY DOWN */
        "  #  "
        "  #  "
        " ### "
        " ### "
        " ### "_str2bin,

        /* U+257E - BOX DRAWINGS HEAVY LEFT AND LIGHT RIGHT */
        "     "
        "###  "
        "#####"
        "###  "
        "     "_str2bin,

        /* U+257F - BOX DRAWINGS HEAVY UP AND LIGHT DOWN */
        " ### "
        " ### "
        " ### "
        "  #  "
        "  #  "_str2bin,

};
