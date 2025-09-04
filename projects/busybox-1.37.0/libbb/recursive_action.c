/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#undef DEBUG_RECURS_ACTION

/*
 * Walk down all the directories under the specified
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * Unfortunately, while nftw(3) could replace this and reduce
 * code size a bit, nftw() wasn't supported before GNU libc 2.1,
 * and so isn't sufficiently portable to take over since glibc2.1
 * is so stinking huge.
 */

static int FAST_FUNC true_action(struct recursive_state *state UNUSED_PARAM,
		const char *fileName UNUSED_PARAM,
		struct stat *statbuf UNUSED_PARAM)
{
	return TRUE;
}

/* fileName is (l)stat'ed (depending on ACTION_FOLLOWLINKS[_L0]).
 *
 * If it is a file: fileAction in run on it, its return value is returned.
 *
 * In case we are in a recursive invocation (see below):
 * normally, fileAction should return 1 (TRUE) to indicate that
 * everything is okay and processing should continue.
 * fileAction return value of 0 (FALSE) on any file in directory will make
 * recursive_action() also return 0, but it doesn't stop directory traversal
 * (fileAction/dirAction will be called on each file).
 *
 * [TODO: maybe introduce -1 to mean "stop traversal NOW and return"]
 *
 * If it is a directory:
 *
 * If !ACTION_RECURSE, dirAction is called and its
 * return value is returned from recursive_action(). No recursion.
 *
 * If ACTION_RECURSE, directory is opened, and recursive_action() is called
 * on each file/subdirectory.
 * If any one of these calls returns 0, current recursive_action() returns 0.
 *
 * If !ACTION_DEPTHFIRST, dirAction is called before recurse.
 * Return value of 0 (FALSE) is an error: prevents recursion,
 * the warning is printed (unless ACTION_QUIET) and recursive_action() returns 0.
 * Return value of 2 (SKIP) prevents recursion, instead recursive_action()
 * returns 1 (TRUE, no error).
 *
 * If ACTION_DEPTHFIRST, dirAction is called after recurse.
 * If it returns 0, the warning is printed and recursive_action() returns 0.
 *
 * ACTION_FOLLOWLINKS mainly controls handling of links to dirs.
 * 0: lstat(statbuf). Calls fileAction on link name even if points to dir.
 * 1: stat(statbuf). Calls dirAction and optionally recurse on link to dir.
 */

static int recursive_action1(recursive_state_t *state, const char *fileName)
{
	struct stat statbuf;
	unsigned follow;
	int status;
	DIR *dir;
	struct dirent *next;

	follow = ACTION_FOLLOWLINKS;
	if (state->depth == 0)
		follow = ACTION_FOLLOWLINKS | ACTION_FOLLOWLINKS_L0;
	follow &= state->flags;
	status = (follow ? stat : lstat)(fileName, &statbuf);
	if (status < 0) {
#ifdef DEBUG_RECURS_ACTION
		bb_error_msg("status=%d flags=%x", status, state->flags);
#endif
		if ((state->flags & ACTION_DANGLING_OK)
		 && errno == ENOENT
		 && lstat(fileName, &statbuf) == 0
		) {
			/* Dangling link */
			return state->fileAction(state, fileName, &statbuf);
		}
		goto done_nak_warn;
	}

	/* If S_ISLNK(m), then we know that !S_ISDIR(m).
	 * Then we can skip checking first part: if it is true, then
	 * (!dir) is also true! */
	if ( /* (!(state->flags & ACTION_FOLLOWLINKS) && S_ISLNK(statbuf.st_mode)) || */
	 !S_ISDIR(statbuf.st_mode)
	) {
		return state->fileAction(state, fileName, &statbuf);
	}

	/* It's a directory (or a link to one, and followLinks is set) */

	if (!(state->flags & ACTION_RECURSE)) {
		return state->dirAction(state, fileName, &statbuf);
	}

	if (!(state->flags & ACTION_DEPTHFIRST)) {
		status = state->dirAction(state, fileName, &statbuf);
		if (status == FALSE)
			goto done_nak_warn;
		if (status == SKIP)
			return TRUE;
	}

	dir = opendir(fileName);
	if (!dir) {
		/* findutils-4.1.20 reports this */
		/* (i.e. it doesn't silently return with exit code 1) */
		/* To trigger: "find -exec rm -rf {} \;" */
		goto done_nak_warn;
	}
	status = TRUE;
	while ((next = readdir(dir)) != NULL) {
		char *nextFile;
		int s;

		nextFile = concat_subpath_file(fileName, next->d_name);
		if (nextFile == NULL)
			continue;

		/* process every file (NB: ACTION_RECURSE is set in flags) */
		state->depth++;
		s = recursive_action1(state, nextFile);
		if (s == FALSE)
			status = FALSE;
		free(nextFile);
		state->depth--;

//#define RECURSE_RESULT_ABORT -1
//		if (s == RECURSE_RESULT_ABORT) {
//			closedir(dir);
//			return s;
//		}
	}
	closedir(dir);

	if (state->flags & ACTION_DEPTHFIRST) {
		if (!state->dirAction(state, fileName, &statbuf))
			goto done_nak_warn;
	}

	return status;

 done_nak_warn:
	if (!(state->flags & ACTION_QUIET))
		bb_simple_perror_msg(fileName);
	return FALSE;
}

int FAST_FUNC recursive_action(const char *fileName,
		unsigned flags,
		int FAST_FUNC (*fileAction)(struct recursive_state *state, const char *fileName, struct stat* statbuf),
		int FAST_FUNC  (*dirAction)(struct recursive_state *state, const char *fileName, struct stat* statbuf),
		void *userData)
{
	/* Keeping a part of variables of recusive descent in a "state structure"
	 * instead of passing ALL of them down as parameters of recursive_action1()
	 * relieves register pressure, both in recursive_action1()
	 * and in every file/dirAction().
	 */
	recursive_state_t state;
	state.flags = flags;
	state.depth = 0;
	state.userData = userData;
	state.fileAction = fileAction ? fileAction : true_action;
	state.dirAction  =  dirAction ?  dirAction : true_action;

	return recursive_action1(&state, fileName);
}
