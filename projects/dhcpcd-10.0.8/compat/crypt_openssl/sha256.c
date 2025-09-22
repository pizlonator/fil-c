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

#include "config.h"
#include "sha256.h"

#include "openssl/evp.h"

/* SHA-256 initialization.  Begins a SHA-256 operation. */
void
dhcpcd_SHA256_Init(SHA256_CTX *ctx)
{
	ctx->c = EVP_MD_CTX_new();
	EVP_DigestInit_ex2(ctx->c, EVP_sha256(), NULL);
}

/* Add bytes into the hash */
void
dhcpcd_SHA256_Update(SHA256_CTX *ctx, const void *in, size_t len)
{
	EVP_DigestUpdate(ctx->c, in, len);
}

/*
 * SHA-256 finalization.  Pads the input data, exports the hash value,
 * and clears the context state.
 */
void
dhcpcd_SHA256_Final(unsigned char digest[32], SHA256_CTX *ctx)
{
	EVP_DigestFinal_ex(ctx->c, digest, NULL);
	EVP_MD_CTX_free(ctx->c);
}
