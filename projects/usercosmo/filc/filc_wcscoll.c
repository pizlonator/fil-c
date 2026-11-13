/* Pizlonated wcscoll for the usercosmo flavor.
 *
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cosmo implements strcoll() (libc/str/strcoll.c) as plain strcmp() - the C
 * locale ordering - but has no wcscoll() at all, while its wide API surface
 * otherwise mirrors the byte one (wcscmp, wcsxfrm, ...).  bash's wide
 * pattern matcher (lib/glob/smatch.c) calls wcscoll() unconditionally for
 * its collation-ordered range and equivalence comparisons, so without this
 * the pizlonated libc is missing pizlonated_wcscoll and nothing that links
 * bash can resolve it.  Mirror cosmo's strcoll(): compare in the C locale,
 * which for wide characters is plain wcscmp().
 */

#include "libc/str/str.h"

/**
 * Compares wide strings in the C locale.
 */
int wcscoll(const wchar_t *p, const wchar_t *q) {
  return wcscmp(p, q);
}
