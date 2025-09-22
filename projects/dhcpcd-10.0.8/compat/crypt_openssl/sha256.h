/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023 Canonical Ltd.
 *
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

#ifndef SHA256_H_
#define SHA256_H_

#define SHA256_DIGEST_LENGTH		32

#include "openssl/evp.h"
typedef struct dhcpcd_SHA256Context {
	EVP_MD_CTX *c;
} dhcpcd_SHA256_CTX;

void	dhcpcd_SHA256_Init(dhcpcd_SHA256_CTX *);
void	dhcpcd_SHA256_Update(dhcpcd_SHA256_CTX *, const void *, size_t);
void	dhcpcd_SHA256_Final(unsigned char [32], dhcpcd_SHA256_CTX *);

#define SHA256_Init	dhcpcd_SHA256_Init
#define SHA256_Update	dhcpcd_SHA256_Update
#define SHA256_Final	dhcpcd_SHA256_Final
#define SHA256_CTX	dhcpcd_SHA256_CTX

#endif
