/*
 * slabinfo.c - slab pools related definitions for libproc2
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2004-2006 Albert Cahalan
 * Copyright © 2003      Chris Rivera
 * Copyright © 2003      Fabian Frederick
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "procps-private.h"
#include "slabinfo.h"


#define SLABINFO_FILE        "/proc/slabinfo"
#define SLABINFO_LINE_LEN    2048
#define SLABINFO_NAME_LEN    128

#define STACKS_INCR          128         // amount reap stack allocations grow

/* ---------------------------------------------------------------------------- +
   this #define will be used to help ensure that our Item_table is synchronized |
   with all the enumerators found in the associated header file. It is intended |
   to only be defined locally (and temporarily) at some point prior to release! | */
// #define ITEMTABLE_DEBUG //-------------------------------------------------- |
// ---------------------------------------------------------------------------- +

/*
   Because 'select' could, at most, return only node[0] values and since 'reap' |
   would be forced to duplicate global slabs stuff in every node results stack, |
   the following #define can be used to enforce strictly logical return values. |
      select: allow only SLABINFO & SLABS items
      reap:   allow only SLABINFO & SLAB items
   Without the #define, these functions always return something even if just 0. |
      get:    return only SLABS results, else 0
      select: return only SLABINFO & SLABS results, else zero
      reap:   return any requested, even when duplicated in each cache's stack */
//#define ENFORCE_LOGICAL  // ensure only logical items accepted by select/reap


struct slabs_summ {
    unsigned int  nr_objs;           // number of objects, among all caches
    unsigned int  nr_active_objs;    // number of active objects, among all caches
    unsigned int  nr_pages;          // number of pages consumed by all objects
    unsigned int  nr_slabs;          // number of slabs, among all caches
    unsigned int  nr_active_slabs;   // number of active slabs, among all caches
    unsigned int  nr_caches;         // number of caches
    unsigned int  nr_active_caches;  // number of active caches
    unsigned int  avg_obj_size;      // average object size
    unsigned int  min_obj_size;      // size of smallest object
    unsigned int  max_obj_size;      // size of largest object
    unsigned long active_size;       // size of all active objects
    unsigned long total_size;        // size of all objects
};

struct slabs_node {
    char name[SLABINFO_NAME_LEN+1];  // name of this cache
    unsigned long cache_size;        // size of entire cache
    unsigned int  nr_objs;           // number of objects in this cache
    unsigned int  nr_active_objs;    // number of active objects
    unsigned int  obj_size;          // size of each object
    unsigned int  objs_per_slab;     // number of objects per slab
    unsigned int  pages_per_slab;    // number of pages per slab
    unsigned int  nr_slabs;          // number of slabs in this cache
    unsigned int  nr_active_slabs;   // number of active slabs
    unsigned int  use;               // percent full: total / active
};

struct slabs_hist {
    struct slabs_summ new;
    struct slabs_summ old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct slabinfo_stack **stacks;
};

struct ext_support {
    int numitems;                    // includes 'logical_end' delimiter
    enum slabinfo_item *items;       // includes 'logical_end' delimiter
    struct stacks_extent *extents;   // anchor for these extents
#ifdef ENFORCE_LOGICAL
    enum slabinfo_item lowest;       // range of allowable enums
    enum slabinfo_item highest;
#endif
};

struct fetch_support {
    struct slabinfo_stack **anchor;  // fetch consolidated extents
    int n_alloc;                     // number of above pointers allocated
    int n_inuse;                     // number of above pointers occupied
    int n_alloc_save;                // last known reap.stacks allocation
    struct slabinfo_reaped results;  // count + stacks for return to caller
};

struct slabinfo_info {
    int refcount;
    FILE *slabinfo_fp;
    int nodes_alloc;                 // nodes alloc()ed
    int nodes_used;                  // nodes using alloced memory
    struct slabs_node *nodes;        // first slabnode of this list
    struct slabs_hist slabs;         // new/old slabs_summ data
    struct ext_support select_ext;   // supports concurrent select/reap
    struct ext_support fetch_ext;    // supports concurrent select/reap
    struct fetch_support fetch;      // support for procps_slabinfo_reap
    struct slabs_node nul_node;      // used by slabinfo_get/select
    struct slabinfo_result get_this; // used by slabinfo_get
    time_t sav_secs;                 // used by slabinfo_get
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct slabinfo_result *R, struct slabs_hist *S, struct slabs_node *N)

// regular assignment
#define REG_set(e,t,x) setDECL(e) { (void)N; R->result. t = S->new. x; }
#define NOD_set(e,t,x) setDECL(e) { (void)S; R->result. t = N-> x; }
// delta assignment
#define HST_set(e,t,x) setDECL(e) { (void)N; R->result. t = (signed long)S->new. x - S->old. x; }

setDECL(SLABINFO_noop)  { (void)R; (void)S; (void)N; }
setDECL(SLABINFO_extra) { (void)S; (void)N; R->result.ul_int = 0; }

NOD_set(SLAB_NAME,                     str,  name)
NOD_set(SLAB_NUM_OBJS,               u_int,  nr_objs)
NOD_set(SLAB_ACTIVE_OBJS,            u_int,  nr_active_objs)
NOD_set(SLAB_OBJ_SIZE,               u_int,  obj_size)
NOD_set(SLAB_OBJ_PER_SLAB,           u_int,  objs_per_slab)
NOD_set(SLAB_NUMS_SLABS,             u_int,  nr_slabs)
NOD_set(SLAB_ACTIVE_SLABS,           u_int,  nr_active_slabs)
NOD_set(SLAB_PAGES_PER_SLAB,         u_int,  pages_per_slab)
NOD_set(SLAB_PERCENT_USED,           u_int,  use)
NOD_set(SLAB_SIZE_TOTAL,            ul_int,  cache_size)

REG_set(SLABS_CACHES_TOTAL,          u_int,  nr_caches)
REG_set(SLABS_CACHES_ACTIVE,         u_int,  nr_active_caches)
REG_set(SLABS_NUM_OBJS,              u_int,  nr_objs)
REG_set(SLABS_ACTIVE_OBJS,           u_int,  nr_active_objs)
REG_set(SLABS_OBJ_SIZE_AVG,          u_int,  avg_obj_size)
REG_set(SLABS_OBJ_SIZE_MIN,          u_int,  min_obj_size)
REG_set(SLABS_OBJ_SIZE_MAX,          u_int,  max_obj_size)
REG_set(SLABS_NUMS_SLABS,            u_int,  nr_slabs)
REG_set(SLABS_ACTIVE_SLABS,          u_int,  nr_active_slabs)
REG_set(SLABS_PAGES_TOTAL,           u_int,  nr_pages)
REG_set(SLABS_SIZE_ACTIVE,          ul_int,  active_size)
REG_set(SLABS_SIZE_TOTAL,           ul_int,  total_size)

HST_set(SLABS_DELTA_CACHES_TOTAL,    s_int,  nr_caches)
HST_set(SLABS_DELTA_CACHES_ACTIVE,   s_int,  nr_active_caches)
HST_set(SLABS_DELTA_NUM_OBJS,        s_int,  nr_objs)
HST_set(SLABS_DELTA_ACTIVE_OBJS,     s_int,  nr_active_objs)
HST_set(SLABS_DELTA_OBJ_SIZE_AVG,    s_int,  avg_obj_size)
HST_set(SLABS_DELTA_OBJ_SIZE_MIN,    s_int,  min_obj_size)
HST_set(SLABS_DELTA_OBJ_SIZE_MAX,    s_int,  max_obj_size)
HST_set(SLABS_DELTA_NUMS_SLABS,      s_int,  nr_slabs)
HST_set(SLABS_DELTA_ACTIVE_SLABS,    s_int,  nr_active_slabs)
HST_set(SLABS_DELTA_PAGES_TOTAL,     s_int,  nr_pages)
HST_set(SLABS_DELTA_SIZE_ACTIVE,     s_int,  active_size)
HST_set(SLABS_DELTA_SIZE_TOTAL,      s_int,  total_size)

#undef setDECL
#undef REG_set
#undef NOD_set
#undef HST_set


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum slabinfo_sort_order order;
};

#define srtNAME(t) sort_slabinfo_ ## t
#define srtDECL(t) static int srtNAME(t) \
    (const struct slabinfo_stack **A, const struct slabinfo_stack **B, struct sort_parms *P)

srtDECL(u_int) {
    const struct slabinfo_result *a = (*A)->head + P->offset; \
    const struct slabinfo_result *b = (*B)->head + P->offset; \
    if ( a->result.u_int > b->result.u_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.u_int < b->result.u_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(ul_int) {
    const struct slabinfo_result *a = (*A)->head + P->offset; \
    const struct slabinfo_result *b = (*B)->head + P->offset; \
    if ( a->result.ul_int > b->result.ul_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.ul_int < b->result.ul_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(str) {
    const struct slabinfo_result *a = (*A)->head + P->offset;
    const struct slabinfo_result *b = (*B)->head + P->offset;
    return P->order * strcoll(a->result.str, b->result.str);
}

srtDECL(noop) { \
    (void)A; (void)B; (void)P; \
    return 0;
}

#undef srtDECL


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct slabinfo_result *, struct slabs_hist *, struct slabs_node *);
#ifdef ITEMTABLE_DEBUG
#define RS(e) (SET_t)setNAME(e), e, STRINGIFY(e)
#else
#define RS(e) (SET_t)setNAME(e)
#endif

typedef int  (*QSR_t)(const void *, const void *, void *);
#define QS(t) (QSR_t)srtNAME(t)

#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those *enum slabinfo_item* guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
#ifdef ITEMTABLE_DEBUG
    int   enumnumb;              // enumerator (must match position!)
    char *enum2str;              // enumerator name as a char* string
#endif
    QSR_t sortfunc;              // sort cmp func for a specific type
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                        sortfunc     type2str
    ------------------------------  -----------  ---------- */
  { RS(SLABINFO_noop),              QS(noop),    TS_noop    },
  { RS(SLABINFO_extra),             QS(ul_int),  TS_noop    },

  { RS(SLAB_NAME),                  QS(str),     TS(str)    },
  { RS(SLAB_NUM_OBJS),              QS(u_int),   TS(u_int)  },
  { RS(SLAB_ACTIVE_OBJS),           QS(u_int),   TS(u_int)  },
  { RS(SLAB_OBJ_SIZE),              QS(u_int),   TS(u_int)  },
  { RS(SLAB_OBJ_PER_SLAB),          QS(u_int),   TS(u_int)  },
  { RS(SLAB_NUMS_SLABS),            QS(u_int),   TS(u_int)  },
  { RS(SLAB_ACTIVE_SLABS),          QS(u_int),   TS(u_int)  },
  { RS(SLAB_PAGES_PER_SLAB),        QS(u_int),   TS(u_int)  },
  { RS(SLAB_PERCENT_USED),          QS(u_int),   TS(u_int)  },
  { RS(SLAB_SIZE_TOTAL),            QS(ul_int),  TS(ul_int) },

  { RS(SLABS_CACHES_TOTAL),         QS(noop),    TS(u_int)  },
  { RS(SLABS_CACHES_ACTIVE),        QS(noop),    TS(u_int)  },
  { RS(SLABS_NUM_OBJS),             QS(noop),    TS(u_int)  },
  { RS(SLABS_ACTIVE_OBJS),          QS(noop),    TS(u_int)  },
  { RS(SLABS_OBJ_SIZE_AVG),         QS(noop),    TS(u_int)  },
  { RS(SLABS_OBJ_SIZE_MIN),         QS(noop),    TS(u_int)  },
  { RS(SLABS_OBJ_SIZE_MAX),         QS(noop),    TS(u_int)  },
  { RS(SLABS_NUMS_SLABS),           QS(noop),    TS(u_int)  },
  { RS(SLABS_ACTIVE_SLABS),         QS(noop),    TS(u_int)  },
  { RS(SLABS_PAGES_TOTAL),          QS(noop),    TS(u_int)  },
  { RS(SLABS_SIZE_ACTIVE),          QS(noop),    TS(ul_int) },
  { RS(SLABS_SIZE_TOTAL),           QS(noop),    TS(ul_int) },

  { RS(SLABS_DELTA_CACHES_TOTAL),   QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_CACHES_ACTIVE),  QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_NUM_OBJS),       QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_ACTIVE_OBJS),    QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_OBJ_SIZE_AVG),   QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_OBJ_SIZE_MIN),   QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_OBJ_SIZE_MAX),   QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_NUMS_SLABS),     QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_ACTIVE_SLABS),   QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_PAGES_TOTAL),    QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_SIZE_ACTIVE),    QS(noop),    TS(s_int)  },
  { RS(SLABS_DELTA_SIZE_TOTAL),     QS(noop),    TS(s_int)  },
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum slabinfo_item SLABINFO_logical_end = MAXTABLE(Item_table);

#undef setNAME
#undef srtNAME
#undef RS
#undef QS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- slabnode specific support ----------------------------------------------

/* Alloc up more slabnode memory, if required
 */
static int alloc_slabnodes (
        struct slabinfo_info *info)
{
    struct slabs_node *new_nodes;
    int new_count;

    if (info->nodes_used < info->nodes_alloc)
        return 1;
    /* Increment the allocated number of slabs */
    new_count = info->nodes_alloc * 5/4+30;

    new_nodes = realloc(info->nodes, sizeof(struct slabs_node) * new_count);
    if (!new_nodes)
        return 0;
    info->nodes = new_nodes;
    info->nodes_alloc = new_count;
    return 1;
} // end: alloc_slabnodes


/*
 * get_slabnode - allocate slab_info structures using a free list
 *
 * In the fast path, we simply return a node off the free list.  In the slow
 * list, we malloc() a new node.  The free list is never automatically reaped,
 * both for simplicity and because the number of slab caches is fairly
 * constant.
 */
static int get_slabnode (
        struct slabinfo_info *info,
        struct slabs_node **node)
{
    if (info->nodes_used == info->nodes_alloc) {
        if (!alloc_slabnodes(info))
            return 0;        // here, errno was set to ENOMEM
    }
    *node = &(info->nodes[info->nodes_used++]);
    return 1;
} // end: get_slabnode


/* parse_slabinfo20:
 *
 * Actual parse routine for slabinfo 2.x (2.6 kernels)
 * Note: difference between 2.0 and 2.1 is in the ": globalstat" part where version 2.1
 * has extra column <nodeallocs>. We don't use ": globalstat" part in both versions.
 *
 * Formats (we don't use "statistics" extensions)
 *
 *  slabinfo - version: 2.1
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail>
 *
 *  slabinfo - version: 2.1 (statistics)
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail> \
 *  : globalstat <listallocs> <maxobjs> <grown> <reaped> <error> <maxfreeable> <freelimit> <nodeallocs> \
 *  : cpustat <allochit> <allocmiss> <freehit> <freemiss>
 *
 *  slabinfo - version: 2.0
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail>
 *
 *  slabinfo - version: 2.0 (statistics)
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail> \
 *  : globalstat <listallocs> <maxobjs> <grown> <reaped> <error> <maxfreeable> <freelimit> \
 *  : cpustat <allochit> <allocmiss> <freehit> <freemiss>
 */
static int parse_slabinfo20 (
        struct slabinfo_info *info)
{
    struct slabs_node *node;
    char buffer[SLABINFO_LINE_LEN];
    int page_size = getpagesize();
    struct slabs_summ *slabs = &(info->slabs.new);

    slabs->min_obj_size = INT_MAX;
    slabs->max_obj_size = 0;

    while (fgets(buffer, SLABINFO_LINE_LEN, info->slabinfo_fp )) {
        if (buffer[0] == '#')
            continue;

        if (!get_slabnode(info, &node))
            return 1;        // here, errno was set to ENOMEM

        if (sscanf(buffer,
                   "%" STRINGIFY(SLABINFO_NAME_LEN) "s" \
                   "%u %u %u %u %u : tunables %*u %*u %*u : slabdata %u %u %*u",
                   node->name,
                   &node->nr_active_objs, &node->nr_objs,
                   &node->obj_size, &node->objs_per_slab,
                   &node->pages_per_slab, &node->nr_active_slabs,
                   &node->nr_slabs) < 8) {
            errno = ERANGE;
            return 1;
        }

        if (!node->name[0])
            snprintf(node->name, sizeof(node->name), "%s", "unknown");

        if (node->obj_size < slabs->min_obj_size)
            slabs->min_obj_size = node->obj_size;
        if (node->obj_size > slabs->max_obj_size)
            slabs->max_obj_size = node->obj_size;

        /* cache_size is not accurate, it's the upper limit of memory used by this slab.
         * When system using slub(most common case) is under high memory pressure, there
         * are slab order fallbacks, which means pages_per_slab is not constant and may decrease.
         */
        node->cache_size = (unsigned long)node->nr_slabs * node->pages_per_slab * page_size;

        if (node->nr_objs) {
            node->use = (unsigned int)(100 * ((float)node->nr_active_objs / node->nr_objs));
            slabs->nr_active_caches++;
        } else
            node->use = 0;

        slabs->nr_objs += node->nr_objs;
        slabs->nr_active_objs += node->nr_active_objs;
        slabs->total_size += (unsigned long)node->nr_objs * node->obj_size;
        slabs->active_size += (unsigned long)node->nr_active_objs * node->obj_size;
        slabs->nr_pages += node->nr_slabs * node->pages_per_slab;
        slabs->nr_slabs += node->nr_slabs;
        slabs->nr_active_slabs += node->nr_active_slabs;
        slabs->nr_caches++;
    }

    if (slabs->nr_objs)
        slabs->avg_obj_size = slabs->total_size / slabs->nr_objs;

    return 0;
} // end: parse_slabinfo20


/* slabinfo_read_failed():
 *
 * Read the data out of /proc/slabinfo putting the information
 * into the supplied info container
 *
 * Returns: 0 on success, 1 on error
 */
static int slabinfo_read_failed (
        struct slabinfo_info *info)
{
    char line[SLABINFO_LINE_LEN];
    int major, minor;

    memcpy(&info->slabs.old, &info->slabs.new, sizeof(struct slabs_summ));
    memset(&(info->slabs.new), 0, sizeof(struct slabs_summ));
    if (!alloc_slabnodes(info))
        return 1;            // here, errno was set to ENOMEM

    memset(info->nodes, 0, sizeof(struct slabs_node)*info->nodes_alloc);
    info->nodes_used = 0;

    if (NULL == info->slabinfo_fp
    && (info->slabinfo_fp = fopen(SLABINFO_FILE, "r")) == NULL)
        return 1;

    if (fseek(info->slabinfo_fp, 0L, SEEK_SET) < 0)
        return 1;

    /* Parse the version string */
    if (!fgets(line, SLABINFO_LINE_LEN, info->slabinfo_fp))
        return 1;

    if (2 != sscanf(line, "slabinfo - version: %d.%d", &major, &minor)
    || (major != 2)) {
        errno = ERANGE;
        return 1;
    }

    return parse_slabinfo20(info);
} // end: slabinfo_read_failed


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- generalized support ----------------------------------------------------

static inline void slabinfo_assign_results (
        struct slabinfo_stack *stack,
        struct slabs_hist *summ,
        struct slabs_node *node)
{
    struct slabinfo_result *this = stack->head;

    for (;;) {
        enum slabinfo_item item = this->item;
        if (item >= SLABINFO_logical_end)
            break;
        Item_table[item].setsfunc(this, summ, node);
        ++this;
    }
    return;
} // end: slabinfo_assign_results


static void slabinfo_extents_free_all (
        struct ext_support *this)
{
    while (this->extents) {
        struct stacks_extent *p = this->extents;
        this->extents = this->extents->next;
        free(p);
    };
} // end: slabinfo_extents_free_all


static inline struct slabinfo_result *slabinfo_itemize_stack (
        struct slabinfo_result *p,
        int depth,
        enum slabinfo_item *items)
{
    struct slabinfo_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        ++p;
    }
    return p_sav;
} // end: slabinfo_itemize_stack


static inline int slabinfo_items_check_failed (
        struct ext_support *this,
        enum slabinfo_item *items,
        int numitems)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum slabinfo_item *'
     * my_stack = procps_slabinfo_select(info, SLABINFO_noop, num);
     *                                         ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * SLABINFO_logical_end))
        return 1;

    for (i = 0; i < numitems; i++) {
#ifdef ENFORCE_LOGICAL
        if (items[i] == SLABINFO_noop
        || (items[i] == SLABINFO_extra))
            continue;
        if (items[i] < this->lowest
        || (items[i] > this->highest))
            return 1;
#else
        // a slabinfo_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return 1;
        if (items[i] >= SLABINFO_logical_end)
            return 1;
        (void)this;
#endif
    }

    return 0;
} // end: slabinfo_items_check_failed


/*
 * slabinfo_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *slabinfo_stacks_alloc (
        struct ext_support *this,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct slabinfo_stack **p_vect;
    struct slabinfo_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    vect_size  = sizeof(void *) * maxstacks;                     // size of the addr vectors |
    vect_size += sizeof(void *);                                 // plus NULL addr delimiter |
    head_size  = sizeof(struct slabinfo_stack);                  // size of that head struct |
    list_size  = sizeof(struct slabinfo_result)*this->numitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                   // the extent anchor itself |
    blob_size += vect_size;                                      // plus room for addr vects |
    blob_size += head_size * maxstacks;                          // plus room for head thing |
    blob_size += list_size * maxstacks;                          // plus room for our stacks |

    /* note: all of our memory is allocated in one single blob, facilitating a later free(). |
             as a minimum, it is important that those result structures themselves always be |
             contiguous within each stack since they are accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = this->extents;                                // push this extent onto... |
    this->extents = p_blob;                                      // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);      // prime our vector pointer |
    p_blob->stacks = p_vect;                                     // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                         // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                   // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct slabinfo_stack *)v_head;
        p_head->head = slabinfo_itemize_stack((struct slabinfo_result *)v_list, this->numitems, this->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: slabinfo_stacks_alloc


static int slabinfo_stacks_fetch (
        struct slabinfo_info *info)
{
 #define n_alloc  info->fetch.n_alloc
 #define n_inuse  info->fetch.n_inuse
 #define n_saved  info->fetch.n_alloc_save
    struct stacks_extent *ext;

    // initialize stuff -----------------------------------
    if (!info->fetch.anchor) {
        if (!(info->fetch.anchor = calloc(sizeof(void *), STACKS_INCR)))
            return -1;
        n_alloc = STACKS_INCR;
    }
    if (!info->fetch_ext.extents) {
        if (!(ext = slabinfo_stacks_alloc(&info->fetch_ext, n_alloc)))
            return -1;       // here, errno was set to ENOMEM
        memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * n_alloc);
    }

    // iterate stuff --------------------------------------
    n_inuse = 0;
    while (n_inuse < info->nodes_used) {
        if (!(n_inuse < n_alloc)) {
            n_alloc += STACKS_INCR;
            if ((!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc)))
            || (!(ext = slabinfo_stacks_alloc(&info->fetch_ext, STACKS_INCR))))
                return -1;   // here, errno was set to ENOMEM
            memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_INCR);
        }
        slabinfo_assign_results(info->fetch.anchor[n_inuse], &info->slabs, &info->nodes[n_inuse]);
        ++n_inuse;
    }

    // finalize stuff -------------------------------------
    /* note: we go to this trouble of maintaining a duplicate of the consolidated |
             extent stacks addresses represented as our 'anchor' since these ptrs |
             are exposed to a user (um, not that we don't trust 'em or anything). |
             plus, we can NULL delimit these ptrs which we couldn't do otherwise. | */
    if (n_saved < n_inuse + 1) {
        n_saved = n_inuse + 1;
        if (!(info->fetch.results.stacks = realloc(info->fetch.results.stacks, sizeof(void *) * n_saved)))
            return -1;
    }
    memcpy(info->fetch.results.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
    info->fetch.results.stacks[n_inuse] = NULL;
    info->fetch.results.total = n_inuse;

    return n_inuse;
 #undef n_alloc
 #undef n_inuse
 #undef n_saved
} // end: slabinfo_stacks_fetch


static int slabinfo_stacks_reconfig_maybe (
        struct ext_support *this,
        enum slabinfo_item *items,
        int numitems)
{
    if (slabinfo_items_check_failed(this, items, numitems))
        return -1;
    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (this->numitems != numitems + 1
    || memcmp(this->items, items, sizeof(enum slabinfo_item) * numitems)) {
        // allow for our SLABINFO_logical_end
        if (!(this->items = realloc(this->items, sizeof(enum slabinfo_item) * (numitems + 1))))
            return -1;
        memcpy(this->items, items, sizeof(enum slabinfo_item) * numitems);
        this->items[numitems] = SLABINFO_logical_end;
        this->numitems = numitems + 1;
        slabinfo_extents_free_all(this);
        return 1;
    }
    return 0;
} // end: slabinfo_stacks_reconfig_maybe


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_slabinfo_new():
 *
 * @info: location of returned new structure
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_slabinfo_new (
        struct slabinfo_info **info)
{
    struct slabinfo_info *p;

#ifdef ITEMTABLE_DEBUG
    int i, failed = 0;
    for (i = 0; i < MAXTABLE(Item_table); i++) {
        if (i != Item_table[i].enumnumb) {
            fprintf(stderr, "%s: enum/table error: Item_table[%d] was %s, but its value is %d\n"
                , __FILE__, i, Item_table[i].enum2str, Item_table[i].enumnumb);
            failed = 1;
        }
    }
    if (failed) _Exit(EXIT_FAILURE);
#endif

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct slabinfo_info))))
        return -ENOMEM;

#ifdef ENFORCE_LOGICAL
    p->select_ext.lowest  = SLABS_CACHES_TOTAL;
    p->select_ext.highest = SLABS_DELTA_SIZE_TOTAL;
    p->fetch_ext.lowest   = SLAB_NAME;
    p->fetch_ext.highest  = SLAB_SIZE_TOTAL;
#endif

    p->refcount = 1;

    /* do a priming read here for the following potential benefits: |
         1) see if that caller's permissions were sufficient (root) |
         2) make delta results potentially useful, even if 1st time |
         3) elimnate need for history distortions 1st time 'switch' | */
    if (slabinfo_read_failed(p)) {
        procps_slabinfo_unref(&p);
        return -errno;
    }

    *info = p;
    return 0;
} // end: procps_slabinfo_new


PROCPS_EXPORT int procps_slabinfo_ref (
        struct slabinfo_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_slabinfo_ref


PROCPS_EXPORT int procps_slabinfo_unref (
        struct slabinfo_info **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
        int errno_sav = errno;

        if ((*info)->slabinfo_fp) {
            fclose((*info)->slabinfo_fp);
            (*info)->slabinfo_fp = NULL;
        }
        if ((*info)->select_ext.extents)
            slabinfo_extents_free_all((&(*info)->select_ext));
        if ((*info)->select_ext.items)
            free((*info)->select_ext.items);

        if ((*info)->fetch.anchor)
            free((*info)->fetch.anchor);
        if ((*info)->fetch.results.stacks)
            free((*info)->fetch.results.stacks);

        if ((*info)->fetch_ext.extents)
            slabinfo_extents_free_all(&(*info)->fetch_ext);
        if ((*info)->fetch_ext.items)
            free((*info)->fetch_ext.items);

        free((*info)->nodes);

        free(*info);
        *info = NULL;

        errno = errno_sav;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_slabinfo_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct slabinfo_result *procps_slabinfo_get (
        struct slabinfo_info *info,
        enum slabinfo_item item)
{
    time_t cur_secs;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (item < 0 || item >= SLABINFO_logical_end)
        return NULL;
    errno = 0;

    /* we will NOT read the slabinfo file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - info->sav_secs) {
        if (slabinfo_read_failed(info))
            return NULL;
        info->sav_secs = cur_secs;
    }

    info->get_this.item = item;
    //  with 'get', we must NOT honor the usual 'noop' guarantee
    info->get_this.result.ul_int = 0;
    Item_table[item].setsfunc(&info->get_this, &info->slabs, &info->nul_node);

    return &info->get_this;
} // end: procps_slabinfo_get


/* procps_slabinfo_reap():
 *
 * Harvest all the requested SLAB (individual nodes) information
 * providing the result stacks along with the total number of nodes.
 *
 * Returns: pointer to a slabinfo_reaped struct on success, NULL on error.
 */
PROCPS_EXPORT struct slabinfo_reaped *procps_slabinfo_reap (
        struct slabinfo_info *info,
        enum slabinfo_item *items,
        int numitems)
{
    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (0 > slabinfo_stacks_reconfig_maybe(&info->fetch_ext, items, numitems))
        return NULL;         // here, errno may be overridden with ENOMEM
    errno = 0;

    if (slabinfo_read_failed(info))
        return NULL;
    if (0 > slabinfo_stacks_fetch(info))
        return NULL;

    return &info->fetch.results;
} // end: procps_slabinfo_reap


/* procps_slabinfo_select():
 *
 * Obtain all the requested SLABS (global) information then return
 * it in a single library provided results stack.
 *
 * Returns: pointer to a slabinfo_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct slabinfo_stack *procps_slabinfo_select (
        struct slabinfo_info *info,
        enum slabinfo_item *items,
        int numitems)
{
    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (0 > slabinfo_stacks_reconfig_maybe(&info->select_ext, items, numitems))
        return NULL;         // here, errno may be overridden with ENOMEM
    errno = 0;

    if (!info->select_ext.extents
    && (!slabinfo_stacks_alloc(&info->select_ext, 1)))
       return NULL;

    if (slabinfo_read_failed(info))
        return NULL;
    slabinfo_assign_results(info->select_ext.extents->stacks[0], &info->slabs, &info->nul_node);

    return info->select_ext.extents->stacks[0];
} // end: procps_slabinfo_select


/*
 * procps_slabinfo_sort():
 *
 * Sort stacks anchored in the passed stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct slabinfo_stack **procps_slabinfo_sort (
        struct slabinfo_info *info,
        struct slabinfo_stack *stacks[],
        int numstacked,
        enum slabinfo_item sortitem,
        enum slabinfo_sort_order order)
{
    struct slabinfo_result *p;
    struct sort_parms parms;
    int offset;

    errno = EINVAL;
    if (info == NULL || stacks == NULL)
        return NULL;
    // a slabinfo_item is currently unsigned, but we'll protect our future
    if (sortitem < 0 || sortitem >= SLABINFO_logical_end)
        return NULL;
    if (order != SLABINFO_SORT_ASCEND && order != SLABINFO_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (p->item >= SLABINFO_logical_end)
            return NULL;
        ++p;
    }
    errno = 0;

    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_slabinfo_sort


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) an #include of 'xtra-procps-debug.h' is used
 */

PROCPS_EXPORT struct slabinfo_result *xtra_slabinfo_get (
        struct slabinfo_info *info,
        enum slabinfo_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct slabinfo_result *r = procps_slabinfo_get(info, actual_enum);

    if (actual_enum < 0 || actual_enum >= SLABINFO_logical_end) {
        fprintf(stderr, "%s line %d: invalid item = %d, type = %s\n"
            , file, lineno, actual_enum, typestr);
    }
    if (r) {
        char *str = Item_table[r->item].type2str;
        if (str[0]
        && (strcmp(typestr, str)))
            fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return r;
} // end: xtra_slabinfo_get_


PROCPS_EXPORT struct slabinfo_result *xtra_slabinfo_val (
        int relative_enum,
        const char *typestr,
        const struct slabinfo_stack *stack,
        struct slabinfo_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < SLABINFO_logical_end; i++)
        ;
    if (relative_enum < 0 || relative_enum >= i) {
        fprintf(stderr, "%s line %d: invalid relative_enum = %d, valid range = 0-%d\n"
            , file, lineno, relative_enum, i-1);
        return NULL;
    }
    str = Item_table[stack->head[relative_enum].item].type2str;
    if (str[0]
    && (strcmp(typestr, str))) {
        fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return &stack->head[relative_enum];
    (void)info;
} // end: xtra_slabinfo_val
