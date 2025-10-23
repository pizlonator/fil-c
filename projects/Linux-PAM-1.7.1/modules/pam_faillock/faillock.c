/*
 * Copyright (c) 2010 Tomas Mraz <tmraz@redhat.com>
 * Copyright (c) 2010, 2016, 2017 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <security/pam_modutil.h>

#include "faillock.h"
#include "pam_inline.h"

#define ignore_return(x) if (1==((int)(x))) {;}

int
open_tally (const char *dir, const char *user, uid_t uid, int create)
{
	char *path;
	int flags = O_RDWR;
	int fd;

	if (dir == NULL || strstr(user, "../") != NULL)
	/* just a defensive programming as the user must be a
	 * valid user on the system anyway
	 */
		return -1;
	if (*dir && dir[strlen(dir) - 1] != '/')
		path = pam_asprintf("%s/%s", dir, user);
	else
		path = pam_asprintf("%s%s", dir, user);
	if (path == NULL)
		return -1;

	if (create) {
		flags |= O_CREAT;
		if (access(dir, F_OK) != 0) {
			mkdir(dir, 0755);
		}
	}

	fd = open(path, flags, 0660);

	free(path);

	if (fd != -1) {
		struct stat st;

		while (flock(fd, LOCK_EX) == -1 && errno == EINTR);
		if (fstat(fd, &st) == 0) {
			if (st.st_uid != uid) {
				ignore_return(fchown(fd, uid, -1));
			}

			/*
			 * If umask is set to 022, as will probably in most systems, then the
			 * group will not be able to write to the file. So, change the file
			 * permissions just in case.
			 * Note: owners of this file are user:root, so if the permissions are
			 * not changed the root process writing to this file will require
			 * CAP_DAC_OVERRIDE.
			 */
			if (!(st.st_mode & S_IWGRP)) {
				ignore_return(fchmod(fd, 0660));
			}
		}
	}

	return fd;
}

#define CHUNK_SIZE (64 * sizeof(struct tally))
#define MAX_RECORDS 1024

int
read_tally(int fd, struct tally_data *tallies)
{
	void *data = NULL, *newdata;
	unsigned int count = 0;
	ssize_t chunk = 0;

	do {
		newdata = realloc(data, count * sizeof(struct tally) + CHUNK_SIZE);
		if (newdata == NULL) {
			free(data);
			return -1;
		}

		data = newdata;

		chunk = pam_modutil_read(fd, (char *)data + count * sizeof(struct tally), CHUNK_SIZE);
		if (chunk < 0) {
			free(data);
			return -1;
		}

		count += chunk/sizeof(struct tally);

		if (count >= MAX_RECORDS)
			break;
	}
	while (chunk == CHUNK_SIZE);

	tallies->records = data;
	tallies->count = count;

	return 0;
}

int
update_tally(int fd, struct tally_data *tallies)
{
	void *data = tallies->records;
	unsigned int count = tallies->count;
	ssize_t chunk;

	if (tallies->count > MAX_RECORDS) {
		data = tallies->records + (count - MAX_RECORDS);
		count = MAX_RECORDS;
	}

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		return -1;
	}

	chunk = pam_modutil_write(fd, data, count * sizeof(struct tally));

	if (chunk != (ssize_t)(count * sizeof(struct tally))) {
		return -1;
	}

	if (ftruncate(fd, count * sizeof(struct tally)) == -1)
		return -1;

	return 0;
}
