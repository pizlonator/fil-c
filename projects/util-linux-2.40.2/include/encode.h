/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_ENCODE_H
#define UTIL_LINUX_ENCODE_H

extern size_t ul_encode_to_utf8(int enc, unsigned char *dest, size_t len,
				const unsigned char *src, size_t count)
			__attribute__((nonnull));

enum {
	UL_ENCODE_UTF16BE = 0,
	UL_ENCODE_UTF16LE,
	UL_ENCODE_LATIN1
};

#endif
