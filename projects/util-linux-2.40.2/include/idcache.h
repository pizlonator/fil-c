/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_IDCACHE_H
#define UTIL_LINUX_IDCACHE_H

#include <sys/types.h>
#include <pwd.h>

#define IDCACHE_FLAGS_NAMELEN	(1 << 1)

struct identry {
	unsigned long int	id;
	char			*name;
	struct identry		*next;
};

struct idcache {
	struct identry	*ent;	/* first entry */
	int		width;	/* name width */
};


extern struct idcache *new_idcache(void);
extern void add_gid(struct idcache *cache, unsigned long int id);
extern void add_uid(struct idcache *cache, unsigned long int id);

extern void free_idcache(struct idcache *ic);
extern struct identry *get_id(struct idcache *ic, unsigned long int id);

#endif /* UTIL_LINUX_IDCACHE_H */
