/*
 * Copyright (c) 2010 Tomas Mraz <tmraz@redhat.com>
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

/*
 * faillock.h - authentication failure data file record structure
 *
 * Each record in the file represents an instance of login failure of
 * the user at the recorded time.
 */


#ifndef _FAILLOCK_H
#define _FAILLOCK_H

#include <stdint.h>
#include <sys/types.h>

#define TALLY_STATUS_VALID     0x1       /* the tally file entry is valid */
#define TALLY_STATUS_RHOST     0x2       /* the source is rhost */
#define TALLY_STATUS_TTY       0x4       /* the source is tty */
/* If neither TALLY_FLAG_RHOST nor TALLY_FLAG_TTY are set the source is service. */

struct	tally {
	char		source[52];	/* rhost or tty of the login failure */
					/* (not necessarily NULL terminated) */
	uint16_t	reserved;	/* reserved for future use */
	uint16_t	status;		/* record status  */
	uint64_t	time;		/* time of the login failure */
};
/* 64 bytes per entry */

struct tally_data {
	struct tally *records;		/* array of tallies */
	unsigned int count;		/* number of records */
};

#define FAILLOCK_DEFAULT_TALLYDIR "/var/run/faillock"

int open_tally(const char *dir, const char *user, uid_t uid, int create);
int read_tally(int fd, struct tally_data *tallies);
int update_tally(int fd, struct tally_data *tallies);
#endif
