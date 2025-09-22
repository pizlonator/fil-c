/*
  File: walk_tree.c

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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "walk_tree.h"

struct entry_handle {
	struct entry_handle *prev, *next;
	dev_t dev;
	ino_t ino;
	DIR *stream;
	long pos;
};

struct walk_tree_args {
	char path[FILENAME_MAX];
	int walk_flags;
	int (*func)(const char *, const struct stat *, int, void *);
	void *arg;
	int depth;
	struct entry_handle dirs;
	struct entry_handle *closed;
	unsigned int num_dir_handles;
	struct stat st;
	dev_t dev;
};

static int walk_tree_visited(struct entry_handle *dirs, dev_t dev, ino_t ino)
{
	struct entry_handle *i;

	for (i = dirs->next; i != dirs; i = i->next)
		if (i->dev == dev && i->ino == ino)
			return 1;
	return 0;
}

static int walk_tree_rec(struct walk_tree_args *args)
{
	int follow_symlinks = (args->walk_flags & WALK_TREE_LOGICAL) ||
			      ((args->walk_flags & WALK_TREE_DEREFERENCE) &&
			       !(args->walk_flags & WALK_TREE_PHYSICAL) &&
			       args->depth == 0);
	int have_dir_stat = 0, flags = args->walk_flags, err;
	struct entry_handle dir;

	/*
	 * If (walk_flags & WALK_TREE_PHYSICAL), do not traverse symlinks.
	 * If (walk_flags & WALK_TREE_LOGICAL), traverse all symlinks.
	 * Otherwise, traverse only top-level symlinks.
	 */
	if (args->depth == 0)
		flags |= WALK_TREE_TOPLEVEL;

	if (lstat(args->path, &args->st) != 0)
		return args->func(args->path, NULL, flags | WALK_TREE_FAILED,
				  args->arg);

	if (flags & WALK_TREE_ONE_FILESYSTEM) {
		if (args->dev == 0)
			args->dev = args->st.st_dev;
		else if (args->st.st_dev != args->dev)
			return 0;
	}

	if (S_ISLNK(args->st.st_mode)) {
		flags |= WALK_TREE_SYMLINK;
		if ((flags & WALK_TREE_DEREFERENCE) ||
		    ((flags & WALK_TREE_TOPLEVEL) &&
		     (flags & WALK_TREE_DEREFERENCE_TOPLEVEL))) {
			if (stat(args->path, &args->st) != 0)
				return args->func(args->path, NULL,
						  flags | WALK_TREE_FAILED,
						  args->arg);
			dir.dev = args->st.st_dev;
			dir.ino = args->st.st_ino;
			have_dir_stat = 1;
		}
	} else if (S_ISDIR(args->st.st_mode)) {
		dir.dev = args->st.st_dev;
		dir.ino = args->st.st_ino;
		have_dir_stat = 1;
	}
	err = args->func(args->path, &args->st, flags, args->arg);

	/*
	 * Recurse if WALK_TREE_RECURSIVE and the path is:
	 *      a dir not from a symlink
	 *      a link and follow_symlinks
	 */
        if ((flags & WALK_TREE_RECURSIVE) &&
	    ((!(flags & WALK_TREE_SYMLINK) && S_ISDIR(args->st.st_mode)) ||
	     ((flags & WALK_TREE_SYMLINK) && follow_symlinks))) {
		struct dirent *entry;

		/*
		 * Check if we have already visited this directory to break
		 * endless loops.
		 *
		 * If we haven't stat()ed the file yet, do an opendir() for
		 * figuring out whether we have a directory, and check whether
		 * the directory has been visited afterwards. This saves a
		 * system call for each non-directory found.
		 */
		if (have_dir_stat && walk_tree_visited(&args->dirs, dir.dev, dir.ino))
			return err;

		if (args->num_dir_handles == 0 && args->closed->prev != &args->dirs) {
close_another_dir:
			/* Close the topmost directory handle still open. */
			args->closed = args->closed->prev;
			args->closed->pos = telldir(args->closed->stream);
			closedir(args->closed->stream);
			args->closed->stream = NULL;
			args->num_dir_handles++;
		}

		dir.stream = opendir(args->path);
		if (!dir.stream) {
			if (errno == ENFILE && args->closed->prev != &args->dirs) {
				/* Ran out of file descriptors. */
				args->num_dir_handles = 0;
				goto close_another_dir;
			}

			/*
			 * PATH may be a symlink to a regular file, or a dead
			 * symlink which we didn't follow above.
			 */
			if (errno != ENOTDIR && errno != ENOENT)
				err += args->func(args->path, NULL, flags |
						 WALK_TREE_FAILED, args->arg);
			return err;
		}

		/* See walk_tree_visited() comment above... */
		if (!have_dir_stat) {
			if (stat(args->path, &args->st) != 0)
				goto skip_dir;
			dir.dev = args->st.st_dev;
			dir.ino = args->st.st_ino;
			if (walk_tree_visited(&args->dirs, dir.dev, dir.ino))
				goto skip_dir;
		}

		/* Insert into the list of handles. */
		dir.next = args->dirs.next;
		dir.prev = &args->dirs;
		dir.prev->next = &dir;
		dir.next->prev = &dir;
		args->num_dir_handles--;

		while ((entry = readdir(dir.stream)) != NULL) {
			char *path_end;

			if (!strcmp(entry->d_name, ".") ||
			    !strcmp(entry->d_name, ".."))
				continue;
			path_end = strchr(args->path, 0);
			if ((path_end - args->path) + strlen(entry->d_name) + 1 >=
			    FILENAME_MAX) {
				errno = ENAMETOOLONG;
				err += args->func(args->path, NULL,
						  flags | WALK_TREE_FAILED,
						  args->arg);
				continue;
			}
			*path_end++ = '/';
			strcpy(path_end, entry->d_name);
			args->depth++;
			err += walk_tree_rec(args);
			args->depth--;
			*--path_end = 0;
			if (!dir.stream) {
				/* Reopen the directory handle. */
				dir.stream = opendir(args->path);
				if (!dir.stream)
					return err + args->func(args->path,
								NULL,
								flags | WALK_TREE_FAILED,
								args->arg);
				seekdir(dir.stream, dir.pos);

				args->closed = args->closed->next;
				args->num_dir_handles--;
			}
		}

		/* Remove from the list of handles. */
		dir.prev->next = dir.next;
		dir.next->prev = dir.prev;
		args->num_dir_handles++;

	skip_dir:
		if (closedir(dir.stream) != 0)
			err += args->func(args->path, NULL,
					 flags | WALK_TREE_FAILED, args->arg);
	}
	return err;
}

int walk_tree(const char *path, int walk_flags, unsigned int num,
	      int (*func)(const char *, const struct stat *, int, void *),
	      void *arg)
{
	struct walk_tree_args args;

	args.num_dir_handles = num;
	if (args.num_dir_handles < 1) {
		struct rlimit rlimit;

		args.num_dir_handles = 1;
		if (getrlimit(RLIMIT_NOFILE, &rlimit) == 0 &&
		    rlimit.rlim_cur >= 2)
			args.num_dir_handles = rlimit.rlim_cur / 2;
	}
	args.dirs.next = &args.dirs;
	args.dirs.prev = &args.dirs;
	args.closed = &args.dirs;
	if (strlen(path) >= FILENAME_MAX) {
		errno = ENAMETOOLONG;
		return func(path, NULL, WALK_TREE_FAILED, arg);
	}
	strcpy(args.path, path);
	args.walk_flags = walk_flags;
	args.func = func;
	args.arg = arg;
	args.depth = 0;
	args.dev = 0;

	return walk_tree_rec(&args);
}
