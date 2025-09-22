/*
 * pwcache.c - memory cache passwd file handling
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2002      Albert Cahalan
 *
 * Older version:
 * Copyright © 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Note: most likely none of his code remains
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

#ifndef PROCPS_PROC_PWCACHE_H
#define PROCPS_PROC_PWCACHE_H

#include <sys/types.h>

// used in pwcache and in readproc to set size of username or groupname
#define P_G_SZ 33

char *pwcache_get_user(uid_t uid);
char *pwcache_get_group(gid_t gid);

#endif
