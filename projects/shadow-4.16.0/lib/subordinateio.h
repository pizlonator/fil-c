/*
 * Copyright (c) 2012- Eric W. Biederman
 */

#ifndef _SUBORDINATEIO_H
#define _SUBORDINATEIO_H

#include <config.h>

#ifdef ENABLE_SUBIDS

#include <sys/types.h>

#include "../libsubid/subid.h"

extern int sub_uid_close(void);
extern bool have_sub_uids(const char *owner, uid_t start, unsigned long count);
extern bool sub_uid_file_present (void);
extern bool local_sub_uid_assigned(const char *owner);
extern int sub_uid_lock (void);
extern int sub_uid_setdbname (const char *filename);
extern /*@observer@*/const char *sub_uid_dbname (void);
extern int sub_uid_open (int mode);
extern int sub_uid_unlock (void);
extern int sub_uid_add (const char *owner, uid_t start, unsigned long count);
extern int sub_uid_remove (const char *owner, uid_t start, unsigned long count);
extern uid_t sub_uid_find_free_range(uid_t min, uid_t max, unsigned long count);
extern int list_owner_ranges(const char *owner, enum subid_type id_type, struct subid_range **ranges);
extern bool new_subid_range(struct subordinate_range *range, enum subid_type id_type, bool reuse);
extern bool release_subid_range(struct subordinate_range *range, enum subid_type id_type);
extern int find_subid_owners(unsigned long id, enum subid_type id_type, uid_t **uids);
extern void free_subordinate_ranges(struct subordinate_range **ranges, int count);

extern int sub_gid_close(void);
extern bool have_sub_gids(const char *owner, gid_t start, unsigned long count);
extern bool sub_gid_file_present (void);
extern bool local_sub_gid_assigned(const char *owner);
extern int sub_gid_lock (void);
extern int sub_gid_setdbname (const char *filename);
extern /*@observer@*/const char *sub_gid_dbname (void);
extern int sub_gid_open (int mode);
extern int sub_gid_unlock (void);
extern int sub_gid_add (const char *owner, gid_t start, unsigned long count);
extern int sub_gid_remove (const char *owner, gid_t start, unsigned long count);
extern uid_t sub_gid_find_free_range(gid_t min, gid_t max, unsigned long count);

extern void free_subid_pointer(void *ptr);

#endif				/* ENABLE_SUBIDS */

#endif
