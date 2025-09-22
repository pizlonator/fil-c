/*
 * SPDX-FileCopyrightText: 2023, Christian Göttsche <cgzones@googlemail.com>
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <unistd.h>

#include "prototypes.h"


/*
 * SYNOPSIS
 *	int write_full(int fd, const void *buf, size_t count);
 *
 * ARGUMENTS
 *	fd	File descriptor.
 *	buf	Source buffer to write(2) into 'fd'.
 *	count	Size of 'buf'.
 *
 * DESCRIPTION
 *	Write 'count' bytes from the buffer starting at 'buf' to the
 *	file referred to by 'fd'.
 *
 *	This function is similar to write(2), except that it retries
 *	in case of a short write.
 *
 *	Since this function either performs a full write, or fails, the
 *	return value is simpler than for write(2).
 *
 * RETURN VALUE
 *	0	On success.
 *	-1	On error.
 *
 * ERRORS
 *	See write(2).
 *
 * CAVEATS
 *	This function can still perform partial writes: if the function
 *	fails in the loop after one or more write(2) calls have
 *	succeeded, it will report a failure, but some data may have been
 *	written.  In such a case, it's the caller's responsibility to
 *	make sure that the partial write is not problematic, and
 *	remediate it if it is --maybe by trying to remove the file--.
 */


int
write_full(int fd, const void *buf, size_t count)
{
	ssize_t              w;
	const unsigned char  *p;

	p = buf;

	while (count > 0) {
		w = write(fd, p, count);
		if (w == -1) {
			if (errno == EINTR)
				continue;

			return -1;
		}

		p += w;
		count -= w;
	}

	return 0;
}
