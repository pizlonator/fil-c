/*
 * uuid.h -- private header file for uuids
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */
#ifndef _UUID_UUID_PRIVATE_H
#define _UUID_UUID_PRIVATE_H

#include <inttypes.h>
#include <sys/types.h>

#include "uuid.h"

#define LIBUUID_CLOCK_FILE	_PATH_LOCALSTATEDIR "/lib/libuuid/clock.txt"
#define LIBUUID_CLOCK_CONT_FILE	_PATH_LOCALSTATEDIR "/lib/libuuid/clock-cont.txt"

/*
 * Offset between 15-Oct-1582 and 1-Jan-70
 */
#define TIME_OFFSET_HIGH 0x01B21DD2
#define TIME_OFFSET_LOW  0x13814000

/*
 * Note that RFC4122 defines UUID in more details:
 *
 *     Field               Data Type     Octet  Note
 * -------------------------------------------------
 *  time_low               unsigned 32   0-3    The low field of the
 *                         bit integer          timestamp
 *
 *  time_mid               unsigned 16   4-5    The middle field of the
 *                         bit integer          timestamp
 *
 *  time_hi_and_version    unsigned 16   6-7    The high field of the
 *                         bit integer          timestamp multiplexed
 *                                              with the version number
 *
 *  clock_seq_hi_and_rese  unsigned 8    8      The high field of the
 *  rved                   bit integer          clock sequence
 *                                              multiplexed with the
 *                                              variant
 *
 *  clock_seq_low          unsigned 8    9      The low field of the
 *                         bit integer          clock sequence
 *
 *  node                   unsigned 48   10-15  The spatially unique
 *                         bit integer          node identifier
 *
 * We have clock_seq_hi_and_reserved (8bit) and clock_seq_low (8bit)
 * merged into clock_seq (16bit).
 */
struct uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint16_t	clock_seq;
	uint8_t	node[6];
};


/*
 * prototypes
 */
void uuid_pack(const struct uuid *uu, uuid_t ptr);
void uuid_unpack(const uuid_t in, struct uuid *uu);

#endif /* _UUID_UUID_PRIVATE_H */
