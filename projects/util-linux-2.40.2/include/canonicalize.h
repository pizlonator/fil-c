/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library Public License for more details.
 */
#ifndef CANONICALIZE_H
#define CANONICALIZE_H

#include "c.h"	/* for PATH_MAX */
#include "strutils.h"

extern char *canonicalize_path(const char *path);
extern char *canonicalize_path_restricted(const char *path);
extern char *canonicalize_dm_name(const char *ptname);
extern char *__canonicalize_dm_name(const char *prefix, const char *ptname);

extern char *absolute_path(const char *path);

static inline int is_relative_path(const char *path)
{
	if (!path || *path == '/')
		return 0;
	return 1;
}

#endif /* CANONICALIZE_H */
