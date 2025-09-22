/*
 * slabinfo.h - slab pools related declarations for libproc2
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PROCPS_SLABINFO_H
#define PROCPS_SLABINFO_H

#ifdef __cplusplus
extern "C "{
#endif

enum slabinfo_item {
    SLABINFO_noop,              //        ( never altered )
    SLABINFO_extra,             //        ( reset to zero )
                                //  returns        origin, see proc(5)
                                //  -------        -------------------
    SLAB_NAME,                  //      str        /proc/slabinfo
    SLAB_NUM_OBJS,              //    u_int         "
    SLAB_ACTIVE_OBJS,           //    u_int         "
    SLAB_OBJ_SIZE,              //    u_int         "
    SLAB_OBJ_PER_SLAB,          //    u_int         "
    SLAB_NUMS_SLABS,            //    u_int         "
    SLAB_ACTIVE_SLABS,          //    u_int         "
    SLAB_PAGES_PER_SLAB,        //    u_int         "
    SLAB_PERCENT_USED,          //    u_int        derived from ACTIVE_OBJS / NUM_OBJS
    SLAB_SIZE_TOTAL,            //   ul_int        derived from page size * NUMS_SLABS * PAGES_PER_SLAB

    SLABS_CACHES_TOTAL,         //    u_int        derived from all caches
    SLABS_CACHES_ACTIVE,        //    u_int         "
    SLABS_NUM_OBJS,             //    u_int         "
    SLABS_ACTIVE_OBJS,          //    u_int         "
    SLABS_OBJ_SIZE_AVG,         //    u_int         "
    SLABS_OBJ_SIZE_MIN,         //    u_int         "
    SLABS_OBJ_SIZE_MAX,         //    u_int         "
    SLABS_NUMS_SLABS,           //    u_int         "
    SLABS_ACTIVE_SLABS,         //    u_int         "
    SLABS_PAGES_TOTAL,          //    u_int         "
    SLABS_SIZE_ACTIVE,          //   ul_int         "
    SLABS_SIZE_TOTAL,           //   ul_int         "

    SLABS_DELTA_CACHES_TOTAL,   //    s_int        derived from above
    SLABS_DELTA_CACHES_ACTIVE,  //    s_int         "
    SLABS_DELTA_NUM_OBJS,       //    s_int         "
    SLABS_DELTA_ACTIVE_OBJS,    //    s_int         "
    SLABS_DELTA_OBJ_SIZE_AVG,   //    s_int         "
    SLABS_DELTA_OBJ_SIZE_MIN,   //    s_int         "
    SLABS_DELTA_OBJ_SIZE_MAX,   //    s_int         "
    SLABS_DELTA_NUMS_SLABS,     //    s_int         "
    SLABS_DELTA_ACTIVE_SLABS,   //    s_int         "
    SLABS_DELTA_PAGES_TOTAL,    //    s_int         "
    SLABS_DELTA_SIZE_ACTIVE,    //    s_int         "
    SLABS_DELTA_SIZE_TOTAL      //    s_int         "
};

enum slabinfo_sort_order {
    SLABINFO_SORT_ASCEND   = +1,
    SLABINFO_SORT_DESCEND  = -1
};


struct slabinfo_result {
    enum slabinfo_item item;
    union {
        signed int     s_int;
        unsigned int   u_int;
        unsigned long  ul_int;
        char          *str;
    } result;
};

struct slabinfo_stack {
    struct slabinfo_result *head;
};

struct slabinfo_reaped {
    int total;
    struct slabinfo_stack **stacks;
};

struct slabinfo_info;


#define SLABINFO_GET( info, actual_enum, type ) ( { \
    struct slabinfo_result *r = procps_slabinfo_get( info, actual_enum ); \
    r ? r->result . type : 0; } )

#define SLABINFO_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


int procps_slabinfo_new   (struct slabinfo_info **info);
int procps_slabinfo_ref   (struct slabinfo_info  *info);
int procps_slabinfo_unref (struct slabinfo_info **info);

struct slabinfo_result *procps_slabinfo_get (
    struct slabinfo_info *info,
    enum slabinfo_item item);

struct slabinfo_reaped *procps_slabinfo_reap (
    struct slabinfo_info *info,
    enum slabinfo_item *items,
    int numitems);

struct slabinfo_stack *procps_slabinfo_select (
    struct slabinfo_info *info,
    enum slabinfo_item *items,
    int numitems);

struct slabinfo_stack **procps_slabinfo_sort (
    struct slabinfo_info *info,
    struct slabinfo_stack *stacks[],
    int numstacked,
    enum slabinfo_item sortitem,
    enum slabinfo_sort_order order);


#ifdef XTRA_PROCPS_DEBUG
# include "xtra-procps-debug.h"
#endif
#ifdef __cplusplus
}
#endif
#endif
