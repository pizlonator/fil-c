/*
 * Copyright (C) 2021 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_FEATURE_TAR_CREATE) += chksum_and_xwrite_tar_header.o
//kbuild:lib-$(CONFIG_SMEMCAP) += chksum_and_xwrite_tar_header.o

#include "libbb.h"
#include "bb_archive.h"

void FAST_FUNC chksum_and_xwrite_tar_header(int fd, struct tar_header_t *hp)
{
	/* POSIX says that checksum is done on unsigned bytes
	 * (Sun and HP-UX gets it wrong... more details in
	 * GNU tar source) */
	const unsigned char *cp;
	unsigned int chksum, size;

	strcpy(hp->magic, "ustar  ");

	/* Calculate and store the checksum (the sum of all of the bytes of
	 * the header).  The checksum field must be filled with blanks for the
	 * calculation.  The checksum field is formatted differently from the
	 * other fields: it has 6 digits, a NUL, then a space -- rather than
	 * digits, followed by a NUL like the other fields... */
	memset(hp->chksum, ' ', sizeof(hp->chksum));
	cp = (const unsigned char *) hp;
	chksum = 0;
	size = sizeof(*hp);
	do { chksum += *cp++; } while (--size);
	sprintf(hp->chksum, "%06o", chksum);

	xwrite(fd, hp, sizeof(*hp));
}
