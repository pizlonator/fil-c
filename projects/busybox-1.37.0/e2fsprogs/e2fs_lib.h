/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/* Constants and structures */
#include "bb_e2fs_defs.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

/* Print file attributes on an ext2 file system */
void print_e2flags_long(unsigned flags);
void print_e2flags(unsigned flags);

extern const uint32_t e2attr_flags_value[];
extern const char e2attr_flags_sname[];

/* If you plan to ENABLE_COMPRESSION, see e2fs_lib.c and chattr.c - */
/* make sure that chattr doesn't accept bad options! */
#ifdef ENABLE_COMPRESSION
#define e2attr_flags_value_chattr (&e2attr_flags_value[5])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[5])
#else
#define e2attr_flags_value_chattr (&e2attr_flags_value[1])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[1])
#endif

POP_SAVED_FUNCTION_VISIBILITY
