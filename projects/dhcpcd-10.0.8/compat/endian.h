/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2014 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ENDIAN_H
#define ENDIAN_H

#include <stdint.h>

inline static void
be32enc(uint8_t *buf, uint32_t u)
{

	buf[0] = (uint8_t)((u >> 24) & 0xff);
	buf[1] = (uint8_t)((u >> 16) & 0xff);
	buf[2] = (uint8_t)((u >> 8) & 0xff);
	buf[3] = (uint8_t)(u & 0xff);
}

inline static void
be64enc(uint8_t *buf, uint64_t u)
{

	be32enc(buf, (uint32_t)(u >> 32));
	be32enc(buf + sizeof(uint32_t), (uint32_t)(u & 0xffffffffULL));
}

inline static uint16_t
be16dec(const uint8_t *buf)
{

	return (uint16_t)(buf[0] << 8 | buf[1]);
}

inline static uint32_t
be32dec(const uint8_t *buf)
{

	return (uint32_t)((uint32_t)be16dec(buf) << 16 | be16dec(buf + 2));
}

inline static uint64_t
be64dec(const uint8_t *buf)
{

	return (uint64_t)((uint64_t)be32dec(buf) << 32 | be32dec(buf + 4));
}
#endif
