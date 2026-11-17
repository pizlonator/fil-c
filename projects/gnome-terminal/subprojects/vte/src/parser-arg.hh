/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
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

#pragma once

#include <assert.h>

/*
 * vte_seq_arg_t:
 *
 * A type to hold a CSI, OSC or DCS parameter.
 *
 * Parameters can be final or nonfinal.
 *
 * Final parameters are those that occur at the end of the
 * parameter list, or the end of a subparameter list.
 *
 * Nonfinal parameters are those that have subparameters
 * after them.
 *
 * Parameters have default value or have a nondefault value.
 */
typedef int vte_seq_arg_t;

#define VTE_SEQ_ARG_FLAG_VALUE    (1 << 16)
#define VTE_SEQ_ARG_FLAG_NONFINAL (1 << 17)
#define VTE_SEQ_ARG_FLAG_MASK     (VTE_SEQ_ARG_FLAG_VALUE | VTE_SEQ_ARG_FLAG_NONFINAL)
#define VTE_SEQ_ARG_VALUE_MASK    (0xffff)

/*
 * VTE_SEQ_ARG_INIT_DEFAULT:
 *
 * Returns: a parameter with default value
 */
#define VTE_SEQ_ARG_INIT_DEFAULT (0)

/*
 * VTE_SEQ_ARG_INIT:
 * @value:
 *
 * Returns: a parameter with value @value
 */
#define VTE_SEQ_ARG_INIT(value) ((value & VTE_SEQ_ARG_VALUE_MASK) | VTE_SEQ_ARG_FLAG_VALUE)

/*
 * vte_seq_arg_init:
 * @value:
 *
 * Returns: a #vte_seq_arg_t for @value, or with default value if @value is -1
 */
static constexpr inline vte_seq_arg_t vte_seq_arg_init(int value)
{
        if (value == -1)
                return VTE_SEQ_ARG_INIT_DEFAULT;
        else
                return VTE_SEQ_ARG_INIT(value);
}

/*
 * vte_seq_arg_push:
 * @arg:
 * @c: a value between 3/0 and 3/9 ['0' .. '9']
 *
 * Multiplies @arg by 10 and adds the numeric value of @c.
 *
 * After this, @arg has a value.
 */
static inline void vte_seq_arg_push(vte_seq_arg_t* arg,
                                    uint32_t c)
{
        auto value = *arg & VTE_SEQ_ARG_VALUE_MASK;
        value = value * 10 + (c - '0');

        /*
         * VT510 tells us to clamp all values to [0, 9999], however, it
         * also allows commands with values up to 2^15-1. We simply use
         * 2^16 as maximum here to be compatible to all commands, but
         * avoid overflows in any calculations.
         */
        if (value > 0xffff)
                value = 0xffff;

        *arg = value | VTE_SEQ_ARG_FLAG_VALUE;
}

/*
 * vte_seq_arg_finish:
 * @arg:
 * @finalise:
 *
 * Finishes @arg; after this no more vte_seq_arg_push() calls
 * are allowed.
 *
 * If @nonfinal is %true, marks @arg as a nonfinal parameter, is,
 * there are more subparameters after it.
 */
static inline void vte_seq_arg_finish(vte_seq_arg_t* arg,
                                      bool nonfinal = false)
{
        if (nonfinal)
                *arg |= VTE_SEQ_ARG_FLAG_NONFINAL;
}

static inline void vte_seq_arg_refinish(vte_seq_arg_t* arg,
                                        bool nonfinal = false)
{
        if (nonfinal)
                *arg |= VTE_SEQ_ARG_FLAG_NONFINAL;
        else
                *arg &= ~VTE_SEQ_ARG_FLAG_NONFINAL;
}

/*
 * vte_seq_arg_started:
 * @arg:
 *
 * Returns: whether @arg has nondefault value
 */
static constexpr inline bool vte_seq_arg_started(vte_seq_arg_t arg)
{
        return arg & VTE_SEQ_ARG_FLAG_VALUE;
}

/*
 * vte_seq_arg_default:
 * @arg:
 *
 * Returns: whether @arg has default value
 */
static constexpr inline bool vte_seq_arg_default(vte_seq_arg_t arg)
{
        return !(arg & VTE_SEQ_ARG_FLAG_VALUE);
}

/*
 * vte_seq_arg_nonfinal:
 * @arg:
 *
 * Returns: whether @arg is a nonfinal parameter, i.e. there
 * are more subparameters after it
 */
static constexpr inline int vte_seq_arg_nonfinal(vte_seq_arg_t arg)
{
        return (arg & VTE_SEQ_ARG_FLAG_NONFINAL);
}

/*
 * vte_seq_arg_value:
 * @arg:
 * @default_value: (defaults to -1)
 *
 * Returns: the value of @arg, or @default_value if @arg has default value
 */
static constexpr inline int vte_seq_arg_value(vte_seq_arg_t arg,
                                              int default_value = -1)
{
        return (arg & VTE_SEQ_ARG_FLAG_VALUE) ? (arg & VTE_SEQ_ARG_VALUE_MASK) : default_value;
}

/*
 * vte_seq_arg_value_final:
 * @arg:
 * @default_value: (defaults to -1)
 *
 * Returns: the value of @arg, or @default_value if @arg has default value or is not final
 */
static constexpr inline int vte_seq_arg_value_final(vte_seq_arg_t arg,
                                                    int default_value = -1)
{
        return ((arg & VTE_SEQ_ARG_FLAG_MASK) == VTE_SEQ_ARG_FLAG_VALUE) ? (arg & VTE_SEQ_ARG_VALUE_MASK) : default_value;
}
