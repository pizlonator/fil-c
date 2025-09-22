/*
 * Copyright (c) 2001-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <attr/attributes.h>

#ifndef ENOATTR
# define ENOATTR ENODATA
#endif

#undef MAXNAMELEN
#define MAXNAMELEN 256
#undef MAXLISTLEN
#define MAXLISTLEN 65536

#undef roundup
#define roundup(x,y) ((((x)+((y)-1))/(y))*(y))

static const char *user_name = "user.";
static const char *secure_name = "security.";
static const char *trusted_name = "trusted.";
static const char *xfsroot_name = "xfsroot.";

/*
 * Convert IRIX API components into Linux/XFS API components,
 * and vice-versa.
 */
static int
api_convert(char *name, const char *irixname, int irixflags, int compat)
{
	if (strlen(irixname) >= MAXNAMELEN) {
		errno = EINVAL;
		return -1;
	}
	if (irixflags & ATTR_ROOT) {
		if (compat)
			strcpy(name, xfsroot_name);
		else
			strcpy(name, trusted_name);
	} else if (irixflags & ATTR_SECURE) {
		strcpy(name, secure_name);
	} else {
		strcpy(name, user_name);
	}
	strcat(name, irixname);
	return 0;
}

static int
api_unconvert(char *name, const char *linuxname, int irixflags)
{
	int type, length;

	length = strlen(user_name);
	if (strncmp(linuxname, user_name, length) == 0) {
		type = 0; /*ATTR_USER*/
		goto found;
	}
	length = strlen(secure_name);
	if (strncmp(linuxname, secure_name, length) == 0) {
		type = ATTR_SECURE;
		goto found;
	}
	length = strlen(trusted_name);
	if (strncmp(linuxname, trusted_name, length) == 0) {
		type = ATTR_ROOT;
		goto found;
	}
	length = strlen(xfsroot_name);
	if (strncmp(linuxname, xfsroot_name, length) == 0) {
		type = ATTR_ROOT;
		goto found;
	}
	return 1;

found:
	if ((irixflags & ATTR_SECURE) != 0 && (type != ATTR_SECURE))
		return 1;
	if ((irixflags & ATTR_ROOT) != 0 && (type != ATTR_ROOT))
		return 1;
	strcpy(name, linuxname + length);
	return 0;
}


int
attr_get(const char *path, const char *attrname, char *attrvalue,
	 int *valuelength, int flags)
{
	ssize_t (*get)(const char *, const char *, void *, size_t) =
		flags & ATTR_DONTFOLLOW ? lgetxattr : getxattr;
	int c, compat;
	char name[MAXNAMELEN+16];

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		c = get(path, name, attrvalue, *valuelength);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	if (c < 0 && errno == ERANGE) {
		int size = get(path, name, NULL, 0);
		if (size >= 0) {
			*valuelength = size;
			errno = E2BIG;
		}
		return c;
	}
	if (c < 0)
		return c;
	*valuelength = c;
	return 0;
}

int
attr_getf(int fd, const char *attrname, char *attrvalue,
	  int *valuelength, int flags)
{
	int c, compat;
	char name[MAXNAMELEN+16];

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		c = fgetxattr(fd, name, attrvalue, *valuelength);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	if (c < 0 && errno == ERANGE) {
		int size = fgetxattr(fd, name, NULL, 0);
		if (size >= 0) {
			*valuelength = size;
			errno = E2BIG;
		}
		return c;
	}
	if (c < 0)
		return c;
	*valuelength = c;
	return 0;
}

int
attr_set(const char *path, const char *attrname, const char *attrvalue,
	 const int valuelength, int flags)
{
	int c, compat, lflags = 0;
	char name[MAXNAMELEN+16];
	void *buffer = (void *)attrvalue;

	if (flags & ATTR_CREATE)
		lflags = XATTR_CREATE;
	else if (flags & ATTR_REPLACE)
		lflags = XATTR_REPLACE;

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		if (flags & ATTR_DONTFOLLOW)
			c = lsetxattr(path, name, buffer, valuelength, lflags);
		else
			c = setxattr(path, name, buffer, valuelength, lflags);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	return c;
}

int
attr_setf(int fd, const char *attrname,
	  const char *attrvalue, const int valuelength, int flags)
{
	int c, compat, lflags = 0;
	char name[MAXNAMELEN+16];
	void *buffer = (void *)attrvalue;

	if (flags & ATTR_CREATE)
		lflags = XATTR_CREATE;
	else if (flags & ATTR_REPLACE)
		lflags = XATTR_REPLACE;

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		c = fsetxattr(fd, name, buffer, valuelength, lflags);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	return c;
}

int
attr_remove(const char *path, const char *attrname, int flags)
{
	int c, compat;
	char name[MAXNAMELEN+16];

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		if (flags & ATTR_DONTFOLLOW)
			c = lremovexattr(path, name);
		else
			c = removexattr(path, name);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	return c;
}

int
attr_removef(int fd, const char *attrname, int flags)
{
	int c, compat;
	char name[MAXNAMELEN+16];

	for (compat = 0; compat < 2; compat++) {
		if ((c = api_convert(name, attrname, flags, compat)) < 0)
			return c;
		c = fremovexattr(fd, name);
		if (c < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		break;
	}
	return c;
}


/*
 * Helper routine for attr_list functions.
 */

static int
attr_list_pack(const char *name, const int valuelen,
		char *buffer, const int buffersize,
		int *start_offset, int *end_offset)
{
	attrlist_ent_t *aentp;
	attrlist_t *alist = (attrlist_t *)buffer;
	int size = roundup(strlen(name) + 1 + sizeof(aentp->a_valuelen), 8);

	if ((*end_offset - size) < (*start_offset + sizeof(alist->al_count))) {
		alist->al_more = 1;
		return 1;
	}

	*end_offset -= size;
	aentp = (attrlist_ent_t *)&buffer[ *end_offset ];
	aentp->a_valuelen = valuelen;
	strncpy(aentp->a_name, name, size - sizeof(aentp->a_valuelen));

	*start_offset += sizeof(alist->al_offset);
	alist->al_offset[alist->al_count] = *end_offset;
	alist->al_count++;
	return 0;
}

int
attr_list(const char *path, char *buffer, const int buffersize, int flags,
	  attrlist_cursor_t *cursor)
{
	const char *l;
	int length, vlength, count = 0;
	char lbuf[MAXLISTLEN+1];
	char name[MAXNAMELEN+16];
	int start_offset, end_offset;

	if (buffersize < sizeof(attrlist_t)) {
		errno = EINVAL;
		return -1;
	}
	memset(buffer, 0, sizeof(attrlist_t));

	if (flags & ATTR_DONTFOLLOW)
		length = llistxattr(path, lbuf, sizeof(lbuf) - 1);
	else
		length = listxattr(path, lbuf, sizeof(lbuf) - 1);
	if (length <= 0)
		return length;
	lbuf[length] = 0;  /* not supposed to be necessary */

	start_offset = sizeof(attrlist_t);
	end_offset = buffersize & ~(8-1);	/* 8 byte align */

	for (l = lbuf; l != lbuf + length; l = strchr(l, '\0') + 1) {
		if (api_unconvert(name, l, flags))
			continue;
		if (flags & ATTR_DONTFOLLOW)
			vlength = lgetxattr(path, l, NULL, 0);
		else
			vlength =  getxattr(path, l, NULL, 0);
		if (vlength < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		if (count++ < cursor->opaque[0])
			continue;
		if (attr_list_pack(name, vlength, buffer, buffersize,
				   &start_offset, &end_offset)) {
			if (cursor->opaque[0] == count - 1) {
				errno = EINVAL;
				return -1;
			}
			cursor->opaque[0] = count - 1;
			break;
		}
	}
	return 0;
}

int
attr_listf(int fd, char *buffer, const int buffersize, int flags,
	   attrlist_cursor_t *cursor)
{
	const char *l;
	int length, vlength, count = 0;
	char lbuf[MAXLISTLEN+1];
	char name[MAXNAMELEN+16];
	int start_offset, end_offset;

	if (buffersize < sizeof(attrlist_t)) {
		errno = EINVAL;
		return -1;
	}
	memset(buffer, 0, sizeof(attrlist_t));

	length = flistxattr(fd, lbuf, sizeof(lbuf) - 1);
	if (length < 0)
		return length;
	lbuf[length] = 0;  /* not supposed to be necessary */

	start_offset = sizeof(attrlist_t);
	end_offset = buffersize & ~(8-1);	/* 8 byte align */

	for (l = lbuf; l != lbuf + length; l = strchr(l, '\0') + 1) {
		if (api_unconvert(name, l, flags))
			continue;
		vlength = fgetxattr(fd, l, NULL, 0);
		if (vlength < 0 && (errno == ENOATTR || errno == ENOTSUP))
			continue;
		if (count++ < cursor->opaque[0])
			continue;
		if (attr_list_pack(name, vlength, buffer, buffersize,
				   &start_offset, &end_offset)) {
			if (cursor->opaque[0] == count - 1) {
				errno = EINVAL;
				return -1;
			}
			cursor->opaque[0] = count - 1;
			break;
		}
	}
	return 0;
}


/*
 * Helper routines for the attr_multi functions.  In IRIX, the
 * multi routines are a single syscall - in Linux, we break em
 * apart in userspace and make individual syscalls for each.
 */

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static int
attr_single(const char *path, attr_multiop_t *op, int flags)
{
	int r = -1;

	errno = EINVAL;
	flags |= op->am_flags;
	if (op->am_opcode == ATTR_OP_GET)
		r = attr_get(path, op->am_attrname, op->am_attrvalue,
				&op->am_length, flags);
	else if (op->am_opcode == ATTR_OP_SET)
		r = attr_set(path, op->am_attrname, op->am_attrvalue,
				op->am_length, flags);
	else if (op->am_opcode == ATTR_OP_REMOVE)
		r = attr_remove(path, op->am_attrname, flags);
	return r;
}

static int
attr_singlef(const int fd, attr_multiop_t *op, int flags)
{
	int r = -1;

	errno = EINVAL;
	flags |= op->am_flags;
	if (op->am_opcode == ATTR_OP_GET)
		r = attr_getf(fd, op->am_attrname, op->am_attrvalue,
				&op->am_length, flags);
	else if (op->am_opcode == ATTR_OP_SET)
		r = attr_setf(fd, op->am_attrname, op->am_attrvalue,
				op->am_length, flags);
	else if (op->am_opcode == ATTR_OP_REMOVE)
		r = attr_removef(fd, op->am_attrname, flags);
	return r;
}
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

/*
 * Operate on multiple attributes of the same object simultaneously
 *
 * From the manpage: "attr_multi will fail if ... a bit other than
 * ATTR_DONTFOLLOW was set in the flag argument." flags must be
 * checked here as they are not passed into the kernel.
 */
int
attr_multi(const char *path, attr_multiop_t *multiops, int count, int flags)
{
	int i, tmp, r = -1;

	errno = EINVAL;
	if ((flags & ATTR_DONTFOLLOW) != flags)
		return r;

	r = errno = 0;
	for (i = 0; i < count; i++) {
		tmp = attr_single(path, &multiops[i], flags);
		if (tmp) r = tmp;
	}
	return r;
}

int
attr_multif(int fd, attr_multiop_t *multiops, int count, int flags)
{
	int i, tmp, r = -1;

	errno = EINVAL;
	if ((flags & ATTR_DONTFOLLOW) != flags)
		return r;

	r = errno = 0;
	for (i = 0; i < count; i++) {
		tmp = attr_singlef(fd, &multiops[i], flags);
		if (tmp) r = tmp;
	}
	return r;
}
