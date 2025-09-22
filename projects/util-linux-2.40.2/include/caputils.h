/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CAPUTILS_H
#define CAPUTILS_H

#include <linux/capability.h>

#ifndef PR_CAP_AMBIENT
# define PR_CAP_AMBIENT		47
#  define PR_CAP_AMBIENT_IS_SET	1
#  define PR_CAP_AMBIENT_RAISE	2
#  define PR_CAP_AMBIENT_LOWER	3
#endif

extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, const cap_user_data_t data);

extern int cap_last_cap(void);

extern void cap_permitted_to_ambient(void);

#endif /* CAPUTILS_H */
