/*
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

#pragma once

#include <cstdint>

#include "debug.h"

enum ShellIntegrationMode {
        /* Command output, selected by OSC 133;C ST.
         * This is the default mode, the one reset operations reset to,
         * so it's safer and easier to make this number 0.
         * Also presumably attr_stream compresses better this way. */
        eNORMAL = 0,
        /* Shell prompt, selected by OSC 133;A ST. */
        ePROMPT,
        /* The command typed by the user, selected by OSC 133;B ST. */
        eCOMMAND,
};

#define VTE_ATTR_VALUE_MASK(bits)      ((1U << (bits)) - 1U)
#define VTE_ATTR_MASK(shift,bits)      (VTE_ATTR_VALUE_MASK(bits) << (shift))

/* Number of visible columns (as determined by g_unichar_iswide(c)).
 * Also (ab)used for tabs; bug 353610.
 */
#define VTE_ATTR_COLUMNS_SHIFT         (0)
#define VTE_ATTR_COLUMNS_BITS          (4) /* Has to be able to store the value of 8. */
#define VTE_ATTR_COLUMNS_MASK          (VTE_ATTR_MASK(VTE_ATTR_COLUMNS_SHIFT, VTE_ATTR_COLUMNS_BITS))
#define VTE_ATTR_COLUMNS_VALUE_MASK    (VTE_ATTR_VALUE_MASK(VTE_ATTR_COLUMNS_BITS))
#define VTE_ATTR_COLUMNS(v)            ((v) << VTE_ATTR_COLUMNS_SHIFT)

/* A continuation cell */
#define VTE_ATTR_FRAGMENT_SHIFT        (VTE_ATTR_COLUMNS_SHIFT + VTE_ATTR_COLUMNS_BITS)
#define VTE_ATTR_FRAGMENT_BITS         (1)
#define VTE_ATTR_FRAGMENT_MASK         (VTE_ATTR_MASK(VTE_ATTR_FRAGMENT_SHIFT, VTE_ATTR_FRAGMENT_BITS))
#define VTE_ATTR_FRAGMENT              (1U << VTE_ATTR_FRAGMENT_SHIFT)

#define VTE_ATTR_BOLD_SHIFT            (VTE_ATTR_FRAGMENT_SHIFT + VTE_ATTR_FRAGMENT_BITS)
#define VTE_ATTR_BOLD_BITS             (1)
#define VTE_ATTR_BOLD_MASK             (VTE_ATTR_MASK(VTE_ATTR_BOLD_SHIFT, VTE_ATTR_BOLD_BITS))
#define VTE_ATTR_BOLD                  (1U << VTE_ATTR_BOLD_SHIFT)

#define VTE_ATTR_ITALIC_SHIFT          (VTE_ATTR_BOLD_SHIFT + VTE_ATTR_BOLD_BITS)
#define VTE_ATTR_ITALIC_BITS           (1)
#define VTE_ATTR_ITALIC_MASK           (VTE_ATTR_MASK(VTE_ATTR_ITALIC_SHIFT, VTE_ATTR_ITALIC_BITS))
#define VTE_ATTR_ITALIC                (1U << VTE_ATTR_ITALIC_SHIFT)

/* 0: none, 1: single, 2: double, 3: curly, 4: dotted, 5: dashed */
#define VTE_ATTR_UNDERLINE_SHIFT       (VTE_ATTR_ITALIC_SHIFT + VTE_ATTR_ITALIC_BITS)
#define VTE_ATTR_UNDERLINE_BITS        (3)
#define VTE_ATTR_UNDERLINE_MASK        (VTE_ATTR_MASK(VTE_ATTR_UNDERLINE_SHIFT, VTE_ATTR_UNDERLINE_BITS))
#define VTE_ATTR_UNDERLINE_VALUE_MASK  (VTE_ATTR_VALUE_MASK(VTE_ATTR_UNDERLINE_BITS))
#define VTE_ATTR_UNDERLINE(v)          ((v) << VTE_ATTR_UNDERLINE_SHIFT)

#define VTE_ATTR_STRIKETHROUGH_SHIFT   (VTE_ATTR_UNDERLINE_SHIFT + VTE_ATTR_UNDERLINE_BITS)
#define VTE_ATTR_STRIKETHROUGH_BITS    (1)
#define VTE_ATTR_STRIKETHROUGH_MASK    (VTE_ATTR_MASK(VTE_ATTR_STRIKETHROUGH_SHIFT, VTE_ATTR_STRIKETHROUGH_BITS))
#define VTE_ATTR_STRIKETHROUGH         (1U << VTE_ATTR_STRIKETHROUGH_SHIFT)

#define VTE_ATTR_OVERLINE_SHIFT        (VTE_ATTR_STRIKETHROUGH_SHIFT + VTE_ATTR_STRIKETHROUGH_BITS)
#define VTE_ATTR_OVERLINE_BITS         (1)
#define VTE_ATTR_OVERLINE_MASK         (VTE_ATTR_MASK(VTE_ATTR_OVERLINE_SHIFT, VTE_ATTR_OVERLINE_BITS))
#define VTE_ATTR_OVERLINE              (1U << VTE_ATTR_OVERLINE_SHIFT)

#define VTE_ATTR_REVERSE_SHIFT         (VTE_ATTR_OVERLINE_SHIFT + VTE_ATTR_OVERLINE_BITS)
#define VTE_ATTR_REVERSE_BITS          (1)
#define VTE_ATTR_REVERSE_MASK          (VTE_ATTR_MASK(VTE_ATTR_REVERSE_SHIFT, VTE_ATTR_REVERSE_BITS))
#define VTE_ATTR_REVERSE               (1U << VTE_ATTR_REVERSE_SHIFT)

#define VTE_ATTR_BLINK_SHIFT           (VTE_ATTR_REVERSE_SHIFT + VTE_ATTR_REVERSE_BITS)
#define VTE_ATTR_BLINK_BITS            (1)
#define VTE_ATTR_BLINK_MASK            (VTE_ATTR_MASK(VTE_ATTR_BLINK_SHIFT, VTE_ATTR_BLINK_BITS))
#define VTE_ATTR_BLINK                 (1U << VTE_ATTR_BLINK_SHIFT)

/* also known as faint, half intensity etc. */
#define VTE_ATTR_DIM_SHIFT             (VTE_ATTR_BLINK_SHIFT + VTE_ATTR_BLINK_BITS)
#define VTE_ATTR_DIM_BITS              (1)
#define VTE_ATTR_DIM_MASK              (VTE_ATTR_MASK(VTE_ATTR_DIM_SHIFT, VTE_ATTR_DIM_BITS))
#define VTE_ATTR_DIM                   (1U << VTE_ATTR_DIM_SHIFT)

#define VTE_ATTR_INVISIBLE_SHIFT       (VTE_ATTR_DIM_SHIFT + VTE_ATTR_DIM_BITS)
#define VTE_ATTR_INVISIBLE_BITS        (1)
#define VTE_ATTR_INVISIBLE_MASK        (VTE_ATTR_MASK(VTE_ATTR_INVISIBLE_SHIFT, VTE_ATTR_INVISIBLE_BITS))
#define VTE_ATTR_INVISIBLE             (1U << VTE_ATTR_INVISIBLE_SHIFT)

/* Holds an enum ShellIntegrationMode */
#define VTE_ATTR_SHELLINTEGRATION_SHIFT        (VTE_ATTR_INVISIBLE_SHIFT + VTE_ATTR_INVISIBLE_BITS)
#define VTE_ATTR_SHELLINTEGRATION_BITS         (2)
#define VTE_ATTR_SHELLINTEGRATION_MASK         (VTE_ATTR_MASK(VTE_ATTR_SHELLINTEGRATION_SHIFT, VTE_ATTR_SHELLINTEGRATION_BITS))
#define VTE_ATTR_SHELLINTEGRATION_VALUE_MASK   (VTE_ATTR_VALUE_MASK(VTE_ATTR_SHELLINTEGRATION_BITS))
#define VTE_ATTR_SHELLINTEGRATION(v)           ((v) << VTE_ATTR_SHELLINTEGRATION_SHIFT)

/* Used internally only */
#define VTE_ATTR_BOXED_SHIFT           (31)
#define VTE_ATTR_BOXED_BITS            (1)
#define VTE_ATTR_BOXED_MASK            (VTE_ATTR_MASK(VTE_ATTR_BOXED_SHIFT, VTE_ATTR_BOXED_BITS))
#define VTE_ATTR_BOXED                 (1U << VTE_ATTR_BOXED_SHIFT)

/* All attributes except DIM and BOXED */
#define VTE_ATTR_ALL_MASK              (VTE_ATTR_BOLD_MASK | \
                                        VTE_ATTR_ITALIC_MASK | \
                                        VTE_ATTR_UNDERLINE_MASK | \
                                        VTE_ATTR_STRIKETHROUGH_MASK | \
                                        VTE_ATTR_OVERLINE_MASK | \
                                        VTE_ATTR_REVERSE_MASK | \
                                        VTE_ATTR_BLINK_MASK | \
                                        VTE_ATTR_INVISIBLE_MASK)

/* All SGR settable attributes */
#define VTE_ATTR_ALL_SGR_MASK          (VTE_ATTR_ALL_MASK | \
                                        VTE_ATTR_DIM_MASK)

#define VTE_ATTR_NONE                  (0U)
#define VTE_ATTR_DEFAULT               (VTE_ATTR_COLUMNS(1))

static inline constexpr void vte_attr_set_bool(uint32_t* attr,
                                               uint32_t mask,
                                               bool value)
{
        if (value)
                *attr |= mask;
        else
                *attr &= ~mask;
}

static inline constexpr void vte_attr_set_value(uint32_t* attr,
                                                uint32_t mask,
                                                unsigned int shift,
                                                uint32_t value)
{
        vte_assert_cmpuint(value << shift, <=, mask); /* assurance */
        *attr = (*attr & ~mask) | ((value << shift) & mask /* assurance */);
}

static constexpr inline bool vte_attr_get_bool(uint32_t attr,
                                               unsigned int shift)
{
        return (attr >> shift) & 1U;
}

static constexpr inline unsigned int vte_attr_get_value(uint32_t attr,
                                                        uint32_t value_mask,
                                                        unsigned int shift)
{
        return (attr >> shift) & value_mask;
}
