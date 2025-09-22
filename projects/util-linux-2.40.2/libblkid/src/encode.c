
/*
 * encode.c - string conversion routines (mostly for compatibility with
 *            udev/volume_id)
 *
 * Copyright (C) 2008 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "blkidP.h"
#include "strutils.h"

/**
 * SECTION: encode
 * @title: Encoding utils
 * @short_description: encode strings to safe udev-compatible formats
 *
 */

/* count of characters used to encode one unicode char */
static int utf8_encoded_expected_len(const char *str)
{
	unsigned char c = (unsigned char)str[0];

	if (c < 0x80)
		return 1;
	if ((c & 0xe0) == 0xc0)
		return 2;
	if ((c & 0xf0) == 0xe0)
		return 3;
	if ((c & 0xf8) == 0xf0)
		return 4;
	if ((c & 0xfc) == 0xf8)
		return 5;
	if ((c & 0xfe) == 0xfc)
		return 6;
	return 0;
}

/* decode one unicode char */
static int utf8_encoded_to_unichar(const char *str)
{
	int unichar;
	int len;
	int i;

	len = utf8_encoded_expected_len(str);
	switch (len) {
	case 1:
		return (int)str[0];
	case 2:
		unichar = str[0] & 0x1f;
		break;
	case 3:
		unichar = (int)str[0] & 0x0f;
		break;
	case 4:
		unichar = (int)str[0] & 0x07;
		break;
	case 5:
		unichar = (int)str[0] & 0x03;
		break;
	case 6:
		unichar = (int)str[0] & 0x01;
		break;
	default:
		return -1;
	}

	for (i = 1; i < len; i++) {
		if (((int)str[i] & 0xc0) != 0x80)
			return -1;
		unichar <<= 6;
		unichar |= (int)str[i] & 0x3f;
	}

	return unichar;
}

/* expected size used to encode one unicode char */
static int utf8_unichar_to_encoded_len(int unichar)
{
	if (unichar < 0x80)
		return 1;
	if (unichar < 0x800)
		return 2;
	if (unichar < 0x10000)
		return 3;
	if (unichar < 0x200000)
		return 4;
	if (unichar < 0x4000000)
		return 5;
	return 6;
}

/* check if unicode char has a valid numeric range */
static int utf8_unichar_valid_range(int unichar)
{
	if (unichar > 0x10ffff)
		return 0;
	if ((unichar & 0xfffff800) == 0xd800)
		return 0;
	if ((unichar > 0xfdcf) && (unichar < 0xfdf0))
		return 0;
	if ((unichar & 0xffff) == 0xffff)
		return 0;
	return 1;
}

/* validate one encoded unicode char and return its length */
static int utf8_encoded_valid_unichar(const char *str)
{
	int len;
	int unichar;
	int i;

	len = utf8_encoded_expected_len(str);
	if (len == 0)
		return -1;

	/* ascii is valid */
	if (len == 1)
		return 1;

	/* check if expected encoded chars are available */
	for (i = 0; i < len; i++)
		if ((str[i] & 0x80) != 0x80)
			return -1;

	unichar = utf8_encoded_to_unichar(str);

	/* check if encoded length matches encoded value */
	if (utf8_unichar_to_encoded_len(unichar) != len)
		return -1;

	/* check if value has valid range */
	if (!utf8_unichar_valid_range(unichar))
		return -1;

	return len;
}

static int is_whitelisted(char c, const char *white)
{
	if ((c >= '0' && c <= '9') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= 'a' && c <= 'z') ||
	    strchr("#+-.:=@_", c) != NULL ||
	    (white != NULL && strchr(white, c) != NULL))
		return 1;
	return 0;
}

/**
 * blkid_encode_string:
 * @str: input string to be encoded
 * @str_enc: output string to store the encoded input string
 * @len: maximum size of the output string, which may be
 *       four times as long as the input string
 *
 * Encode all potentially unsafe characters of a string to the
 * corresponding hex value prefixed by '\x'.
 *
 * Returns: 0 if the entire string was copied, non-zero otherwise.
 **/
int blkid_encode_string(const char *str, char *str_enc, size_t len)
{
	size_t i, j;

	if (!str || !str_enc || !len)
		return -1;

	for (i = 0, j = 0; str[i] != '\0'; i++) {
		int seqlen;

		seqlen = utf8_encoded_valid_unichar(&str[i]);
		if (seqlen > 1) {
			if (len-j < (size_t)seqlen)
				goto err;
			memcpy(&str_enc[j], &str[i], seqlen);
			j += seqlen;
			i += (seqlen-1);
		} else if (str[i] == '\\' || !is_whitelisted(str[i], NULL)) {
			if (len-j < 4)
				goto err;
			sprintf(&str_enc[j], "\\x%02x", (unsigned char) str[i]);
			j += 4;
		} else {
			if (len-j < 1)
				goto err;
			str_enc[j] = str[i];
			j++;
		}
		if (j+3 >= len)
			goto err;
	}
	if (len-j < 1)
		goto err;
	str_enc[j] = '\0';
	return 0;
err:
	return -1;
}

/**
 * blkid_safe_string:
 * @str: input string
 * @str_safe: output string
 * @len: size of output string
 *
 * Processing whitespace characters. Allows valid ascii,valid utf8.
 * Replace everything else with'_'
 *
 * Returns: 0 on success or -1 in case of error.
 */
int blkid_safe_string(const char *str, char *str_safe, size_t len)
{
	size_t i = 0;

	if (!str || !str_safe || !len)
		return -1;

	__normalize_whitespace(
			(const unsigned char *) str, strnlen(str, len),
			(unsigned char *) str_safe, len);

	while (i < len && str_safe[i] != '\0') {
		int seqsz;

		/* accept ASCII from ' ' to '~' */
		if (str_safe[i] > 0x20 && str_safe[i] <= 0x7E)
			i++;

		/* accept hex encoding */
		else if (str_safe[i] == '\\' && str_safe[i+1] == 'x')
			i += 2;

		/* replace whitespace */
		else if (isspace(str_safe[i]))
			str_safe[i++] = '_';

		/* accept valid utf8 */
		else if ((seqsz = utf8_encoded_valid_unichar(&str_safe[i])) >= 1)
			i += seqsz;

		/* everything else is replaced with '_' */
		else
			str_safe[i++] = '_';
	}

	str_safe[len - 1] = '\0';
	return 0;
}
