/*
  File: walk_tree.h

  Copyright (C) 2007 Andreas Gruenbacher <a.gruenbacher@computer.org>

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the
  Free Software Foundation; either version 2.1 of the License, or (at
  your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __WALK_TREE_H
#define __WALK_TREE_H

#define WALK_TREE_RECURSIVE		0x01
#define WALK_TREE_PHYSICAL		0x02
#define WALK_TREE_LOGICAL		0x04
#define WALK_TREE_DEREFERENCE		0x08
#define WALK_TREE_DEREFERENCE_TOPLEVEL	0x10
#define WALK_TREE_ONE_FILESYSTEM	0x20

#define WALK_TREE_TOPLEVEL	0x100
#define WALK_TREE_SYMLINK	0x200
#define WALK_TREE_FAILED	0x400

struct stat;

extern int walk_tree(const char *path, int walk_flags, unsigned int num,
		     int (*func)(const char *, const struct stat *, int,
				 void *), void *arg);

#endif
