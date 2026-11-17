/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright © 2018, 2019 Christian Persch
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

/* The interfaces in this file are subject to change at any time. */

#pragma once

#ifdef __clang__
/* Clang generates lots of warnings for the code below. However while
 * the pointer in the VteCellAttr struct is indeed only 4-byte aligned,
 * the VteCellAttr is only used when a member of a VteCell, where it is
 * at offset 4, resulting in a sufficient overall alignment. So we can
 * safely ignore the warning.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Waddress-of-packed-member"
#endif /* __clang__ */

#include <string.h>

#include "vteunistr.h"
#include "vtemacros.h"
#include "vtedefines.hh"

#include "attr.hh"
#include "color-triple.hh"

#define VTE_TAB_WIDTH_MAX		((1 << VTE_ATTR_COLUMNS_BITS) - 1)

#define VTE_CELL_ATTR_COMMON_BYTES      12  /* The number of common bytes in VteCellAttr and VteStreamCellAttr */

typedef struct VteCellAttr VteCellAttr;

/*
 * VteCellAttrReverseMask: A class that stores SGR attributes and
 * stores a mask that when applied to a VteCellAttr reverses its
 * attributes.
 *
 * When adding new attributes, keep in sync with VteCellAttr.
 */

#define CELL_ATTR_BOOL(lname,uname) \
        inline constexpr void set_##lname(bool value) \
        { \
                attr ^= value ? VTE_ATTR_##uname##_MASK : 0;      \
        }

#define CELL_ATTR_UINT(lname,uname) \
        inline constexpr void set_##lname(unsigned int value) \
        { \
                attr ^= value ? VTE_ATTR_##uname(1) : 0;  \
        } \
        \
        inline constexpr uint32_t lname() const \
        { \
                return vte_attr_get_value(attr, VTE_ATTR_##uname##_VALUE_MASK, VTE_ATTR_##uname##_SHIFT); \
        }

typedef struct _VTE_GNUC_PACKED VteCellAttrReverseMask {

        uint32_t attr{0u};
        // Colours cannot be 'reversed' so don't bother storing them

        /* Methods */

        explicit constexpr operator bool() const noexcept
        {
                return attr != 0;
        }

        inline constexpr void unset(uint32_t mask)
        {
                // no-op
        }

#define CELL_ATTR_COLOR(name,mask) \
        inline void set_##name(uint32_t value) \
        { \
        } \
        \
        inline constexpr uint32_t name() const \
        { \
                return 0; \
        }

        CELL_ATTR_COLOR(fore, VTE_COLOR_TRIPLE_FORE_MASK)
        CELL_ATTR_COLOR(back, VTE_COLOR_TRIPLE_BACK_MASK)
        CELL_ATTR_COLOR(deco, VTE_COLOR_TRIPLE_DECO_MASK)
#undef CELL_ATTR_COLOR

        CELL_ATTR_BOOL(bold, BOLD)
        CELL_ATTR_BOOL(italic, ITALIC)
        CELL_ATTR_UINT(underline, UNDERLINE)
        CELL_ATTR_BOOL(strikethrough, STRIKETHROUGH)
        CELL_ATTR_BOOL(overline, OVERLINE)
        CELL_ATTR_BOOL(reverse, REVERSE)
        CELL_ATTR_BOOL(blink, BLINK)
        CELL_ATTR_BOOL(dim, DIM)
        CELL_ATTR_BOOL(invisible, INVISIBLE)

        inline constexpr void reset_sgr_attributes() noexcept
        {
                attr ^= VTE_ATTR_ALL_SGR_MASK;
        }

} VteCellAttrReverseMask;

#undef CELL_ATTR_BOOL
#undef CELL_ATTR_UINT

/*
 * VteCellAttr: A single cell style attributes
 *
 * When adding new attributes, keep in sync with VteStreamCellAttr and
 * update VTE_CELL_ATTR_COMMON_BYTES accordingly.
 * Also don't forget to update basic_cell below!
 */

#define CELL_ATTR_BOOL(lname,uname) \
        inline constexpr void set_##lname(bool value) \
        { \
                vte_attr_set_bool(&attr, VTE_ATTR_##uname##_MASK, value); \
        } \
        \
        inline constexpr bool lname() const \
        { \
                return vte_attr_get_bool(attr, VTE_ATTR_##uname##_SHIFT); \
        }

#define CELL_ATTR_UINT(lname,uname) \
        inline constexpr void set_##lname(unsigned int value) \
        { \
                vte_attr_set_value(&attr, VTE_ATTR_##uname##_MASK, VTE_ATTR_##uname##_SHIFT, value); \
        } \
        \
        inline constexpr uint32_t lname() const \
        { \
                return vte_attr_get_value(attr, VTE_ATTR_##uname##_VALUE_MASK, VTE_ATTR_##uname##_SHIFT); \
        }

struct _VTE_GNUC_PACKED VteCellAttr {
        uint32_t attr;

	/* 4-byte boundary (8-byte boundary in VteCell) */
        uint64_t m_colors;                     /* fore, back and deco (underline) colour */

        /* 12-byte boundary (16-byte boundary in VteCell) */
        uint32_t hyperlink_idx; /* a unique hyperlink index at a time for the ring's cells,
                                   0 means not a hyperlink, VTE_HYPERLINK_IDX_TARGET_IN_STREAM
                                   means the target is irrelevant/unknown at the moment.
                                   If bitpacking, choose a size big enough to hold a different idx
                                   for every cell in the ring but not yet in the stream
                                   (currently the height rounded up to the next power of two, times width)
                                   for supported VTE sizes, and update VTE_HYPERLINK_IDX_TARGET_IN_STREAM. */

        /* Methods */

        inline constexpr uint64_t colors() const { return m_colors; }

        inline void copy_colors(VteCellAttr const& other)
        {
                m_colors = vte_color_triple_copy(other.colors());
        }

#define CELL_ATTR_COLOR(name) \
        inline void set_##name(uint32_t value) \
        { \
                vte_color_triple_set_##name(&m_colors, value); \
        } \
        \
        inline constexpr uint32_t name() const \
        { \
                return vte_color_triple_get_##name(m_colors); \
        }

        CELL_ATTR_COLOR(fore)
        CELL_ATTR_COLOR(back)
        CELL_ATTR_COLOR(deco)
#undef CELL_ATTR_COLOR

        inline constexpr bool has_any(uint32_t mask) const
        {
                return !!(attr & mask);
        }

        inline constexpr bool has_all(uint32_t mask) const
        {
                return (attr & mask) == mask;
        }

        inline constexpr bool has_none(uint32_t mask) const
        {
                return !(attr & mask);
        }

        inline void unset(uint32_t mask)
        {
                attr &= ~mask;
        }

        CELL_ATTR_UINT(columns, COLUMNS)
        CELL_ATTR_BOOL(fragment, FRAGMENT)
        CELL_ATTR_BOOL(bold, BOLD)
        CELL_ATTR_BOOL(italic, ITALIC)
        CELL_ATTR_UINT(underline, UNDERLINE)
        CELL_ATTR_BOOL(strikethrough, STRIKETHROUGH)
        CELL_ATTR_BOOL(overline, OVERLINE)
        CELL_ATTR_BOOL(reverse, REVERSE)
        CELL_ATTR_BOOL(blink, BLINK)
        CELL_ATTR_BOOL(dim, DIM)
        CELL_ATTR_BOOL(invisible, INVISIBLE)
        CELL_ATTR_UINT(shellintegration, SHELLINTEGRATION)
        /* ATTR_BOOL(boxed, BOXED) */

        inline void reset_sgr_attributes()
        {
                vte_attr_set_value(&attr, VTE_ATTR_ALL_SGR_MASK, 0 /* shift */, 0 /* value */);
                m_colors = vte_color_triple_init();
        }

}; // class VteCellAttr

static_assert(sizeof (VteCellAttr) == 16, "VteCellAttr has wrong size");
static_assert(offsetof (VteCellAttr, hyperlink_idx) == VTE_CELL_ATTR_COMMON_BYTES, "VteCellAttr layout is wrong");

/*
 * VteStreamCellAttr: Variant of VteCellAttr to be stored in attr_stream.
 *
 * When adding new attributes, keep in sync with VteCellAttr and
 * update VTE_CELL_ATTR_COMMON_BYTES accordingly.
 */

typedef struct _VTE_GNUC_PACKED _VteStreamCellAttr {
        uint32_t attr; /* Same as VteCellAttr. We only access columns
                        * and fragment, however.
                        */
        /* 4-byte boundary */
        uint64_t colors;
        /* 12-byte boundary */
        guint16 hyperlink_length;       /* make sure it fits VTE_HYPERLINK_TOTAL_LENGTH_MAX */

        /* Methods */
        CELL_ATTR_UINT(columns, COLUMNS)
        CELL_ATTR_BOOL(fragment, FRAGMENT)
} VteStreamCellAttr;
static_assert(sizeof (VteStreamCellAttr) == 14, "VteStreamCellAttr has wrong size");
static_assert(offsetof (VteStreamCellAttr, hyperlink_length) == VTE_CELL_ATTR_COMMON_BYTES, "VteStreamCellAttr layout is wrong");

#undef CELL_ATTR_BOOL
#undef CELL_ATTR_UINT

/*
 * VteCell: A single cell's data
 */

typedef struct _VTE_GNUC_PACKED _VteCell {
	vteunistr c;
	VteCellAttr attr;
} VteCell;

static_assert(sizeof (VteCell) == 20, "VteCell has wrong size");

static const VteCell basic_cell = {
	0,
	{
                VTE_ATTR_DEFAULT, /* attr */
                VTE_COLOR_TRIPLE_INIT_DEFAULT, /* colors */
                0, /* hyperlink_idx */
	}
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif
