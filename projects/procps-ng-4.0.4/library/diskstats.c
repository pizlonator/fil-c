/*
 * diskstats.c - disk I/O related definitions for libproc2
 *
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2003      Albert Cahalan
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

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "procps-private.h"
#include "diskstats.h"

/* The following define will cause the 'node_add' function to maintain our |
   nodes list in ascending alphabetical order which could be used to avoid |
   a sort on name. Without it, we default to a 'pull-up' stack at slightly |
   more effort than a simple 'push-down' list to duplicate prior behavior. | */
//#define ALPHABETIC_NODES

#define DISKSTATS_LINE_LEN  1024
#define DISKSTATS_NAME_LEN  34
#define DISKSTATS_FILE      "/proc/diskstats"
#define SYSBLOCK_DIR        "/sys/block"

#define STACKS_INCR         64           // amount reap stack allocations grow
#define STR_COMPARE         strverscmp

/* ----------------------------------------------------------------------- +
   this provision can help ensure that our Item_table remains synchronized |
   with the enumerators found in the associated header file. It's intended |
   to only be used locally (& temporarily) at some point before a release! | */
// #define ITEMTABLE_DEBUG //--------------------------------------------- |
// ----------------------------------------------------------------------- +


struct dev_data {
    unsigned long reads;
    unsigned long reads_merged;
    unsigned long read_sectors;
    unsigned long read_time;
    unsigned long writes;
    unsigned long writes_merged;
    unsigned long write_sectors;
    unsigned long write_time;
    unsigned long io_inprogress;
    unsigned long io_time;
    unsigned long io_wtime;
};

struct dev_node {
    char name[DISKSTATS_NAME_LEN+1];
    int type;
    int major;
    int minor;
    time_t stamped;
    struct dev_data new;
    struct dev_data old;
    struct dev_node *next;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct diskstats_stack **stacks;
};

struct ext_support {
    int numitems;                      // includes 'logical_end' delimiter
    enum diskstats_item *items;        // includes 'logical_end' delimiter
    struct stacks_extent *extents;     // anchor for these extents
};

struct fetch_support {
    struct diskstats_stack **anchor;   // fetch consolidated extents
    int n_alloc;                       // number of above pointers allocated
    int n_inuse;                       // number of above pointers occupied
    int n_alloc_save;                  // last known reap.stacks allocation
    struct diskstats_reaped results;   // count + stacks for return to caller
};

struct diskstats_info {
    int refcount;
    FILE *diskstats_fp;
    time_t old_stamp;                  // previous read seconds
    time_t new_stamp;                  // current read seconds
    struct dev_node *nodes;            // dev nodes anchor
    struct ext_support select_ext;     // supports concurrent select/reap
    struct ext_support fetch_ext;      // supports concurrent select/reap
    struct fetch_support fetch;        // support for procps_diskstats_reap
    struct diskstats_result get_this;  // used by procps_diskstats_get
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_diskstats_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct diskstats_result *R, struct dev_node *N)

// regular assignment
#define DEV_set(e,t,x) setDECL(e) { R->result. t = N-> x; }
#define REG_set(e,t,x) setDECL(e) { R->result. t = N->new. x; }
// delta assignment
#define HST_set(e,t,x) setDECL(e) { R->result. t = ( N->new. x - N->old. x ); }

setDECL(noop)  { (void)R; (void)N; }
setDECL(extra) { (void)N; R->result.ul_int = 0; }

DEV_set(NAME,                 str,     name)
DEV_set(TYPE,                 s_int,   type)
DEV_set(MAJOR,                s_int,   major)
DEV_set(MINOR,                s_int,   minor)

REG_set(READS,                ul_int,  reads)
REG_set(READS_MERGED,         ul_int,  reads_merged)
REG_set(READ_SECTORS,         ul_int,  read_sectors)
REG_set(READ_TIME,            ul_int,  read_time)
REG_set(WRITES,               ul_int,  writes)
REG_set(WRITES_MERGED,        ul_int,  writes_merged)
REG_set(WRITE_SECTORS,        ul_int,  write_sectors)
REG_set(WRITE_TIME,           ul_int,  write_time)
REG_set(IO_TIME,              ul_int,  io_time)
REG_set(WEIGHTED_TIME,        ul_int,  io_wtime)

REG_set(IO_INPROGRESS,        s_int,   io_inprogress)

HST_set(DELTA_READS,          s_int,   reads)
HST_set(DELTA_READS_MERGED,   s_int,   reads_merged)
HST_set(DELTA_READ_SECTORS,   s_int,   read_sectors)
HST_set(DELTA_READ_TIME,      s_int,   read_time)
HST_set(DELTA_WRITES,         s_int,   writes)
HST_set(DELTA_WRITES_MERGED,  s_int,   writes_merged)
HST_set(DELTA_WRITE_SECTORS,  s_int,   write_sectors)
HST_set(DELTA_WRITE_TIME,     s_int,   write_time)
HST_set(DELTA_IO_TIME,        s_int,   io_time)
HST_set(DELTA_WEIGHTED_TIME,  s_int,   io_wtime)

#undef setDECL
#undef DEV_set
#undef REG_set
#undef HST_set


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum diskstats_sort_order order;
};

#define srtNAME(t) sort_diskstats_ ## t
#define srtDECL(t) static int srtNAME(t) \
    (const struct diskstats_stack **A, const struct diskstats_stack **B, struct sort_parms *P)

srtDECL(s_int) {
    const struct diskstats_result *a = (*A)->head + P->offset; \
    const struct diskstats_result *b = (*B)->head + P->offset; \
    return P->order * (a->result.s_int - b->result.s_int);
}

srtDECL(ul_int) {
    const struct diskstats_result *a = (*A)->head + P->offset; \
    const struct diskstats_result *b = (*B)->head + P->offset; \
    if ( a->result.ul_int > b->result.ul_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.ul_int < b->result.ul_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(str) {
    const struct diskstats_result *a = (*A)->head + P->offset;
    const struct diskstats_result *b = (*B)->head + P->offset;
    return P->order * STR_COMPARE(a->result.str, b->result.str);
}

srtDECL(noop) { \
    (void)A; (void)B; (void)P; \
    return 0;
}

#undef srtDECL


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct diskstats_result *, struct dev_node *);
#ifdef ITEMTABLE_DEBUG
#define RS(e) (SET_t)setNAME(e), DISKSTATS_ ## e, STRINGIFY(DISKSTATS_ ## e)
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
         * those *enum diskstats_item* guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
#ifdef ITEMTABLE_DEBUG
    int   enumnumb;              // enumerator (must match position!)
    char *enum2str;              // enumerator name as a char* string
#endif
    QSR_t sortfunc;              // sort cmp func for a specific type
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                  sortfunc     type2str
    ------------------------  -----------  ---------- */
  { RS(noop),                 QS(noop),    TS_noop    },
  { RS(extra),                QS(ul_int),  TS_noop    },

  { RS(NAME),                 QS(str),     TS(str)    },
  { RS(TYPE),                 QS(s_int),   TS(s_int)  },
  { RS(MAJOR),                QS(s_int),   TS(s_int)  },
  { RS(MINOR),                QS(s_int),   TS(s_int)  },

  { RS(READS),                QS(ul_int),  TS(ul_int) },
  { RS(READS_MERGED),         QS(ul_int),  TS(ul_int) },
  { RS(READ_SECTORS),         QS(ul_int),  TS(ul_int) },
  { RS(READ_TIME),            QS(ul_int),  TS(ul_int) },
  { RS(WRITES),               QS(ul_int),  TS(ul_int) },
  { RS(WRITES_MERGED),        QS(ul_int),  TS(ul_int) },
  { RS(WRITE_SECTORS),        QS(ul_int),  TS(ul_int) },
  { RS(WRITE_TIME),           QS(ul_int),  TS(ul_int) },
  { RS(IO_TIME),              QS(ul_int),  TS(ul_int) },
  { RS(WEIGHTED_TIME),        QS(ul_int),  TS(ul_int) },

  { RS(IO_INPROGRESS),        QS(s_int),   TS(s_int)  },

  { RS(DELTA_READS),          QS(s_int),   TS(s_int)  },
  { RS(DELTA_READS_MERGED),   QS(s_int),   TS(s_int)  },
  { RS(DELTA_READ_SECTORS),   QS(s_int),   TS(s_int)  },
  { RS(DELTA_READ_TIME),      QS(s_int),   TS(s_int)  },
  { RS(DELTA_WRITES),         QS(s_int),   TS(s_int)  },
  { RS(DELTA_WRITES_MERGED),  QS(s_int),   TS(s_int)  },
  { RS(DELTA_WRITE_SECTORS),  QS(s_int),   TS(s_int)  },
  { RS(DELTA_WRITE_TIME),     QS(s_int),   TS(s_int)  },
  { RS(DELTA_IO_TIME),        QS(s_int),   TS(s_int)  },
  { RS(DELTA_WEIGHTED_TIME),  QS(s_int),   TS(s_int)  },
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum diskstats_item DISKSTATS_logical_end = MAXTABLE(Item_table);

#undef setNAME
#undef srtNAME
#undef RS
#undef QS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- dev_node specific support ----------------------------------------------

static struct dev_node *node_add (
        struct diskstats_info *info,
        struct dev_node *this)
{
    struct dev_node *prev, *walk;

#ifdef ALPHABETIC_NODES
    if (!info->nodes
    || (STR_COMPARE(this->name, info->nodes->name) < 0)) {
        this->next = info->nodes;
        info->nodes = this;
        return this;
    }
    prev = info->nodes;
    walk = info->nodes->next;
    while (walk) {
        if (STR_COMPARE(this->name, walk->name) < 0)
            break;
        prev = walk;
        walk = walk->next;
    }
    prev->next = this;
    this->next = walk;
#else
    if (!info->nodes)
        info->nodes = this;
    else {
        walk = info->nodes;
        do {
            prev = walk;
            walk = walk->next;
        } while (walk);
        prev->next = this;
    }
#endif
    return this;
} // end: node_add


static void node_classify (
        struct dev_node *this)
{
    DIR *dirp;
    struct dirent *dent;

    /* all disks start off as partitions. this function
       checks /sys/block and changes a device found there
       into a disk. if /sys/block cannot have the directory
       read, all devices are then treated as disks. */
    this->type = DISKSTATS_TYPE_PARTITION;

    if (!(dirp = opendir(SYSBLOCK_DIR))) {
        this->type = DISKSTATS_TYPE_DISK;
        return;
    }
    while ((dent = readdir(dirp))) {
        if (strcmp(this->name, dent->d_name) == 0) {
            this->type = DISKSTATS_TYPE_DISK;
            break;
        }
    }
    closedir(dirp);
} // end: node_classify


static struct dev_node *node_cut (
        struct diskstats_info *info,
        struct dev_node *this)
{
    struct dev_node *node = info->nodes;

    if (this) {
        if (this == node) {
            info->nodes = node->next;
            return this;
        }
        do {
            if (this == node->next) {
                node->next = node->next->next;
                return this;
            }
            node = node->next;
        } while (node);
    }
    return NULL;
} // end: node_cut


static struct dev_node *node_get (
        struct diskstats_info *info,
        const char *name)
{
    struct dev_node *node = info->nodes;

    while (node) {
        if (strcmp(name, node->name) == 0)
            break;
        node = node->next;
    }
    if (node) {
        /* if this disk or partition has somehow gotten stale, we'll lose
           it and then pretend it was never actually found ...
         [ we test against both stamps in case a 'read' was avoided ] */
        if (node->stamped != info->old_stamp
        && (node->stamped != info->new_stamp)) {
            free(node_cut(info, node));
            node = NULL;
        }
    }
    return node;
} // end: node_get


static int node_update (
        struct diskstats_info *info,
        struct dev_node *source)
{
    struct dev_node *target = node_get(info, source->name);

    if (!target) {
        if (!(target = malloc(sizeof(struct dev_node))))
            return 0;
        memcpy(target, source, sizeof(struct dev_node));
        // let's not distort the deltas when a new node is created ...
        memcpy(&target->old, &target->new, sizeof(struct dev_data));
        node_classify(target);
        node_add(info, target);
        return 1;
    }
    // remember history from last time around ...
    memcpy(&source->old, &target->new, sizeof(struct dev_data));
    // preserve some stuff from the existing node struct ...
    source->type = target->type;
    source->next = target->next;
    // finally 'update' the existing node struct ...
    memcpy(target, source, sizeof(struct dev_node));
    return 1;
} // end: node_update


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- generalized support ----------------------------------------------------

static inline void diskstats_assign_results (
        struct diskstats_stack *stack,
        struct dev_node *node)
{
    struct diskstats_result *this = stack->head;

    for (;;) {
        enum diskstats_item item = this->item;
        if (item >= DISKSTATS_logical_end)
            break;
        Item_table[item].setsfunc(this, node);
        ++this;
    }
    return;
} // end: diskstats_assign_results


static void diskstats_extents_free_all (
        struct ext_support *this)
{
    while (this->extents) {
        struct stacks_extent *p = this->extents;
        this->extents = this->extents->next;
        free(p);
    };
} // end: diskstats_extents_free_all


static inline struct diskstats_result *diskstats_itemize_stack (
        struct diskstats_result *p,
        int depth,
        enum diskstats_item *items)
{
    struct diskstats_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        ++p;
    }
    return p_sav;
} // end: diskstats_itemize_stack


static inline int diskstats_items_check_failed (
        enum diskstats_item *items,
        int numitems)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum diskstats_item *'
     * my_stack = procps_diskstats_select(info, DISKSTATS_noop, num);
     *                                          ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * DISKSTATS_logical_end))
        return 1;

    for (i = 0; i < numitems; i++) {
        // a diskstats_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return 1;
        if (items[i] >= DISKSTATS_logical_end)
            return 1;
    }

    return 0;
} // end: diskstats_items_check_failed


/*
 * diskstats_read_failed:
 *
 * @info: info structure created at procps_diskstats_new
 *
 * Read the data out of /proc/diskstats putting the information
 * into the supplied info structure
 *
 * Returns: 0 on success, 1 on error
 */
static int diskstats_read_failed (
        struct diskstats_info *info)
{
    static const char *fmtstr = "%d %d %" STRINGIFY(DISKSTATS_NAME_LEN) \
        "s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu";
    char buf[DISKSTATS_LINE_LEN];
    struct dev_node node;
    int rc;

    if (!info->diskstats_fp
    && (!(info->diskstats_fp = fopen(DISKSTATS_FILE, "r"))))
        return 1;

    if (fseek(info->diskstats_fp, 0L, SEEK_SET) == -1)
        return 1;

    info->old_stamp = info->new_stamp;
    info->new_stamp = time(NULL);

    while (fgets(buf, DISKSTATS_LINE_LEN, info->diskstats_fp)) {
        // clear out the soon to be 'current'values
        memset(&node, 0, sizeof(struct dev_node));

        rc = sscanf(buf, fmtstr
            , &node.major
            , &node.minor
            , &node.name[0]
            , &node.new.reads
            , &node.new.reads_merged
            , &node.new.read_sectors
            , &node.new.read_time
            , &node.new.writes
            , &node.new.writes_merged
            , &node.new.write_sectors
            , &node.new.write_time
            , &node.new.io_inprogress
            , &node.new.io_time
            , &node.new.io_wtime);

        if (rc != 14) {
            errno = ERANGE;
            return 1;
        }
        node.stamped = info->new_stamp;
        if (!node_update(info, &node))
            return 1;        // here, errno was set to ENOMEM
    }

    return 0;
} // end: diskstats_read_failed


/*
 * diskstats_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *diskstats_stacks_alloc (
        struct ext_support *this,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct diskstats_stack **p_vect;
    struct diskstats_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    vect_size  = sizeof(void *) * maxstacks;                        // size of the addr vectors |
    vect_size += sizeof(void *);                                    // plus NULL addr delimiter |
    head_size  = sizeof(struct diskstats_stack);                    // size of that head struct |
    list_size  = sizeof(struct diskstats_result) * this->numitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                      // the extent anchor itself |
    blob_size += vect_size;                                         // plus room for addr vects |
    blob_size += head_size * maxstacks;                             // plus room for head thing |
    blob_size += list_size * maxstacks;                             // plus room for our stacks |

    /* note: all of our memory is allocated in one single blob, facilitating some later free(). |
             as a minimum, it's important that all of those result structs themselves always be |
             contiguous within every stack since they will be accessed via a relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = this->extents;                                   // push this extent onto... |
    this->extents = p_blob;                                         // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);         // prime our vector pointer |
    p_blob->stacks = p_vect;                                        // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                            // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                      // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct diskstats_stack *)v_head;
        p_head->head = diskstats_itemize_stack((struct diskstats_result *)v_list, this->numitems, this->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: diskstats_stacks_alloc


static int diskstats_stacks_fetch (
        struct diskstats_info *info)
{
 #define n_alloc  info->fetch.n_alloc
 #define n_inuse  info->fetch.n_inuse
 #define n_saved  info->fetch.n_alloc_save
    struct stacks_extent *ext;
    struct dev_node *node;

    // initialize stuff -----------------------------------
    if (!info->fetch.anchor) {
        if (!(info->fetch.anchor = calloc(sizeof(void *), STACKS_INCR)))
            return -ENOMEM;
        n_alloc = STACKS_INCR;
    }
    if (!info->fetch_ext.extents) {
        if (!(ext = diskstats_stacks_alloc(&info->fetch_ext, n_alloc)))
            return -1;       // here, errno was set to ENOMEM
        memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * n_alloc);
    }

    // iterate stuff --------------------------------------
    n_inuse = 0;
    node = info->nodes;
    while (node) {
        if (!(n_inuse < n_alloc)) {
            n_alloc += STACKS_INCR;
            if ((!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc)))
            || (!(ext = diskstats_stacks_alloc(&info->fetch_ext, STACKS_INCR))))
                return -1;   // here, errno was set to ENOMEM
            memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_INCR);
        }
        diskstats_assign_results(info->fetch.anchor[n_inuse], node);
        ++n_inuse;
        node = node->next;
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
} // end: diskstats_stacks_fetch


static int diskstats_stacks_reconfig_maybe (
        struct ext_support *this,
        enum diskstats_item *items,
        int numitems)
{
    if (diskstats_items_check_failed(items, numitems))
        return -1;
    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (this->numitems != numitems + 1
    || memcmp(this->items, items, sizeof(enum diskstats_item) * numitems)) {
        // allow for our DISKSTATS_logical_end
        if (!(this->items = realloc(this->items, sizeof(enum diskstats_item) * (numitems + 1))))
            return -1;       // here, errno was set to ENOMEM
        memcpy(this->items, items, sizeof(enum diskstats_item) * numitems);
        this->items[numitems] = DISKSTATS_logical_end;
        this->numitems = numitems + 1;
        diskstats_extents_free_all(this);
        return 1;
    }
    return 0;
} // end: diskstats_stacks_reconfig_maybe


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_diskstats_new():
 *
 * @info: location of returned new structure
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_diskstats_new (
        struct diskstats_info **info)
{
    struct diskstats_info *p;

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
    if (!(p = calloc(1, sizeof(struct diskstats_info))))
        return -ENOMEM;

    p->refcount = 1;

    /* do a priming read here for the following potential benefits: |
         1) ensure there will be no problems with subsequent access |
         2) make delta results potentially useful, even if 1st time |
         3) elimnate need for history distortions 1st time 'switch' | */
    if (diskstats_read_failed(p)) {
        procps_diskstats_unref(&p);
        return -errno;
    }

    *info = p;
    return 0;
} // end: procps_diskstats_new


PROCPS_EXPORT int procps_diskstats_ref (
        struct diskstats_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_diskstats_ref


PROCPS_EXPORT int procps_diskstats_unref (
        struct diskstats_info **info)
{
    struct dev_node *node;

    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
        int errno_sav = errno;

        if ((*info)->diskstats_fp) {
            fclose((*info)->diskstats_fp);
            (*info)->diskstats_fp = NULL;
        }
        node = (*info)->nodes;
        while (node) {
            struct dev_node *p = node;
            node = p->next;
            free(p);
        }
        if ((*info)->select_ext.extents)
            diskstats_extents_free_all((&(*info)->select_ext));
        if ((*info)->select_ext.items)
            free((*info)->select_ext.items);

        if ((*info)->fetch.anchor)
            free((*info)->fetch.anchor);
        if ((*info)->fetch.results.stacks)
            free((*info)->fetch.results.stacks);

        if ((*info)->fetch_ext.extents)
            diskstats_extents_free_all(&(*info)->fetch_ext);
        if ((*info)->fetch_ext.items)
            free((*info)->fetch_ext.items);

        free(*info);
        *info = NULL;

        errno = errno_sav;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_diskstats_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct diskstats_result *procps_diskstats_get (
        struct diskstats_info *info,
        const char *name,
        enum diskstats_item item)
{
    struct dev_node *node;
    time_t cur_secs;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (item < 0 || item >= DISKSTATS_logical_end)
        return NULL;
    errno = 0;

    /* we will NOT read the diskstat file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - info->new_stamp) {
        if (diskstats_read_failed(info))
            return NULL;
    }

    info->get_this.item = item;
    //  with 'get', we must NOT honor the usual 'noop' guarantee
    info->get_this.result.ul_int = 0;

    if (!(node = node_get(info, name))) {
        errno = ENXIO;
        return NULL;
    }
    Item_table[item].setsfunc(&info->get_this, node);

    return &info->get_this;
} // end: procps_diskstats_get


/* procps_diskstats_reap():
 *
 * Harvest all the requested disks information providing
 * the result stacks along with the total number of harvested.
 *
 * Returns: pointer to a diskstats_reaped struct on success, NULL on error.
 */
PROCPS_EXPORT struct diskstats_reaped *procps_diskstats_reap (
        struct diskstats_info *info,
        enum diskstats_item *items,
        int numitems)
{
    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (0 > diskstats_stacks_reconfig_maybe(&info->fetch_ext, items, numitems))
        return NULL;         // here, errno may be overridden with ENOMEM
    errno = 0;

    if (diskstats_read_failed(info))
        return NULL;
    if (0 > diskstats_stacks_fetch(info))
        return NULL;

    return &info->fetch.results;
} // end: procps_diskstats_reap


/* procps_diskstats_select():
 *
 * Obtain all the requested disk/partition information then return
 * it in a single library provided results stack.
 *
 * Returns: pointer to a diskstats_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct diskstats_stack *procps_diskstats_select (
        struct diskstats_info *info,
        const char *name,
        enum diskstats_item *items,
        int numitems)
{
    struct dev_node *node;

    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (0 > diskstats_stacks_reconfig_maybe(&info->select_ext, items, numitems))
        return NULL;         // here, errno may be overridden with ENOMEM
    errno = 0;

    if (!info->select_ext.extents
    && (!diskstats_stacks_alloc(&info->select_ext, 1)))
       return NULL;

    if (diskstats_read_failed(info))
        return NULL;
    if (!(node = node_get(info, name))) {
        errno = ENXIO;
        return NULL;
    }

    diskstats_assign_results(info->select_ext.extents->stacks[0], node);

    return info->select_ext.extents->stacks[0];
} // end: procps_diskstats_select


/*
 * procps_diskstats_sort():
 *
 * Sort stacks anchored in the passed stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct diskstats_stack **procps_diskstats_sort (
        struct diskstats_info *info,
        struct diskstats_stack *stacks[],
        int numstacked,
        enum diskstats_item sortitem,
        enum diskstats_sort_order order)
{
    struct diskstats_result *p;
    struct sort_parms parms;
    int offset;

    errno = EINVAL;
    if (info == NULL || stacks == NULL)
        return NULL;
    // a diskstats_item is currently unsigned, but we'll protect our future
    if (sortitem < 0 || sortitem >= DISKSTATS_logical_end)
        return NULL;
    if (order != DISKSTATS_SORT_ASCEND && order != DISKSTATS_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (p->item >= DISKSTATS_logical_end)
            return NULL;
        ++p;
    }
    errno = 0;

    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_diskstats_sort


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) an #include of 'xtra-procps-debug.h' is used
 */

PROCPS_EXPORT struct diskstats_result *xtra_diskstats_get (
        struct diskstats_info *info,
        const char *name,
        enum diskstats_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct diskstats_result *r = procps_diskstats_get(info, name, actual_enum);

    if (actual_enum < 0 || actual_enum >= DISKSTATS_logical_end) {
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
} // end: xtra_diskstats_get_


PROCPS_EXPORT struct diskstats_result *xtra_diskstats_val (
        int relative_enum,
        const char *typestr,
        const struct diskstats_stack *stack,
        struct diskstats_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < DISKSTATS_logical_end; i++)
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
} // end: xtra_diskstats_val
