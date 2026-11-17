/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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

#define VTE_TAB_WIDTH			8
#define VTE_LINE_WIDTH			1
#define VTE_ROWS			24
#define VTE_COLUMNS			80

/*
 * R8G8B8 colors are encoded in 25 bits as follows:
 *
 * 0 .. 255:
 *   Colors set by SGR 256-color extension (38/48;5;index).
 *   These are direct indices into the color palette.
 *
 * 256 .. VTE_PALETTE_SIZE - 1 (261):
 *   Special values, such as default colors.
 *   These are direct indices into the color palette.
 *
 * VTE_LEGACY_COLORS_OFFSET (512) .. VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_FULL_COLOR_SET_SIZE - 1 (527):
 *   Colors set by legacy escapes (30..37/40..47, 90..97/100..107).
 *   These are translated to 0 .. 15 before looking up in the palette, taking bold into account.
 *
 * VTE_DIM_COLORS (2^10) .. :
 *   Dimmed version of the above, for foreground colors.
 *   Cell attributes can't have these colors.
 *
 * VTE_RGB_COLOR (2^24) .. VTE_RGB_COLOR + 16Mi - 1 (2^25 - 1):
 *   Colors set by SGR truecolor extension (38/48;2;red;green;blue)
 *   These are direct RGB values.
 *
 * R4G5B4-bit-per-component colours are encoded the same, except for
 * direct colours which are reduced to 13-bit colours and stored as
 * direct values with bit 1 << 13 set.
 */

#define VTE_LEGACY_COLORS_OFFSET	(1U << 9)
#define VTE_LEGACY_COLOR_SET_SIZE	8
#define VTE_LEGACY_FULL_COLOR_SET_SIZE	16
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_DIM_COLOR                   (1U << 10)
#define VTE_RGB_COLOR_MASK(rb,gb,bb)    (1U << ((rb) + (gb) + (bb)))
#define VTE_RGB_COLOR(bb,gb,rb,r,g,b)   (VTE_RGB_COLOR_MASK(rb,gb,bb) |   \
                                         ((((r) >> (8 - (rb))) & ((1U << (rb)) -  1U)) << ((gb) + (bb))) | \
                                         ((((g) >> (8 - (gb))) & ((1U << (gb)) -  1U)) << (bb)) | \
                                         (((b) >> (8 - (bb))) & ((1U << (bb)) -  1U)))
#define VTE_RGB_COLOR_GET_COMPONENT(packed,shift,bits) \
        ((((packed) >> (shift)) & ((1U << (bits)) - 1U)) << (8 - bits) | ((1U << (8 - bits)) >> 1))

#define VTE_DEFAULT_FG			256
#define VTE_DEFAULT_BG			257
#define VTE_BOLD_FG			258
#define VTE_HIGHLIGHT_FG		259
#define VTE_HIGHLIGHT_BG		260
#define VTE_CURSOR_BG			261
#define VTE_CURSOR_FG                   262
#define VTE_PALETTE_SIZE		263

#define VTE_SCROLLBACK_INIT		512
#define VTE_DEFAULT_CURSOR		std::string{"text"}
#define VTE_MOUSING_CURSOR		std::string{"default"}
#define VTE_HYPERLINK_CURSOR		std::string{"pointer"}
#define VTE_HYPERLINK_CURSOR_DEBUG	std::string{"crosshair"}
#define VTE_CHILD_INPUT_PRIORITY	G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_OUTPUT_PRIORITY	G_PRIORITY_HIGH
#define VTE_MAX_INPUT_READ		0x1000
#define VTE_MAX_PROCESS_TIME		100
#define VTE_CELL_BBOX_SLACK		1
#define VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH 1

#define VTE_UTF8_BPC                    (4) /* Maximum number of bytes used per UTF-8 character */

#define VTE_FONT_SCALE_MIN (.25)
#define VTE_FONT_SCALE_MAX (4.)
#define VTE_CELL_SCALE_MIN (1.)
#define VTE_CELL_SCALE_MAX (2.)

/* Minimum time between two beeps (µs) */
#define VTE_BELL_MINIMUM_TIME_DIFFERENCE (100000)

/* Maximum length of a URI in the OSC 8 escape sequences. There's no de jure limit,
 * 2000-ish the de facto standard, and Internet Explorer supports 2083.
 * See also the comment of VTE_HYPERLINK_TOTAL_LENGTH_MAX. */
#define VTE_HYPERLINK_URI_LENGTH_MAX    2083

/* Maximum number of URIs in the ring for a given screen (as in "normal" vs "alternate" screen)
 * at a time. Idx 0 is a placeholder for no hyperlink, URIs have indexes from 1 to
 * VTE_HYPERLINK_COUNT_MAX inclusive, plus one more technical idx is also required, see below.
 * This is just a safety cap because the number of URIs is bound by the number of cells in the ring
 * (excluding the stream) which should be way lower than this at sane window sizes.
 * Make sure there are enough bits to store them in VteCellAttr.hyperlink_idx.
 * Also make sure _vte_ring_hyperlink_gc() can allocate a large enough bitmap. */
#define VTE_HYPERLINK_COUNT_MAX         ((1 << 20) - 2)

/* Used when thawing a row from the stream in order to display it, to denote
 * hyperlinks whose target is currently irrelevant.
 * Make sure there are enough bits to store this in VteCellAttr.hyperlink_idx */
#define VTE_HYPERLINK_IDX_TARGET_IN_STREAM      (VTE_HYPERLINK_COUNT_MAX + 1)

/* Max length allowed in the id= parameter of an OSC 8 sequence.
 * See also the comment of VTE_HYPERLINK_TOTAL_LENGTH_MAX. */
#define VTE_HYPERLINK_ID_LENGTH_MAX     250

/* Max length of all the hyperlink data stored in the streams as a string.
 * Currently the hyperlink data is the ID and URI and a separator in between.
 * Make sure there are enough bits to store this in VteStreamCellAttr.hyperlink_length */
#define VTE_HYPERLINK_TOTAL_LENGTH_MAX  (VTE_HYPERLINK_ID_LENGTH_MAX + 1 + VTE_HYPERLINK_URI_LENGTH_MAX)

/* Max depth of title stack */
#define VTE_WINDOW_TITLE_STACK_MAX_DEPTH (8)

/* Maximum length of a paragraph, in lines, that might get proper RingView (BiDi) treatment. */
#define VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX   500

#define VTE_VERSION_NUMERIC ((VTE_MAJOR_VERSION) * 10000 + (VTE_MINOR_VERSION) * 100 + (VTE_MICRO_VERSION))

#define VTE_TERMINFO_NAME "xterm-256color"

#define VTE_MIN_CURSOR_BLINK_CYCLE (50 /* ms */)
#define VTE_MIN_CURSOR_BLINK_TIMEOUT (50 /* ms */)

#define VTE_MIN_GRID_WIDTH (2) /* space for one wide character */
#define VTE_MIN_GRID_HEIGHT (1)

#define VTE_FONT_WEIGHT_BOLDENING (PANGO_WEIGHT_BOLD - PANGO_WEIGHT_NORMAL)

#define VTE_MAX_TERMPROP_NUM (256)
