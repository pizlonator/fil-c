/* Convert IPv4/IPv6 addresses from binary to text form.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <errno.h>
#include <string.h>
#include <_itoa.h>

static inline char *
put_uint8 (uint8_t word, char *tp)
{
  int s = 1;
  if (word >= 10)
    {
      if (word >= 100)
	{
	  tp[2] = '0' + word % 10;
	  word /= 10;
	  s += 1;
	}

      tp[1] = '0' + word % 10;
      word /= 10;
      s += 1;
    }
  *tp = '0' + word;
  return tp + s;
}

static inline char *
put_uint16 (uint16_t word, char *tp)
{
  if (word >= 0x1000)
    *tp++ = _itoa_lower_digits[(word >> 12) & 0xf];
  if (word >= 0x100)
    *tp++ = _itoa_lower_digits[(word >> 8) & 0xf];
  if (word >= 0x10)
    *tp++ = _itoa_lower_digits[(word >> 4) & 0xf];
  *tp++ = _itoa_lower_digits[word & 0xf];
  return tp;
}

static __always_inline char *
inet_ntop4_format (const uint8_t *src, char *dst)
{
  dst = put_uint8 (src[0], dst);
  *(dst++) = '.';
  dst = put_uint8 (src[1], dst);
  *(dst++) = '.';
  dst = put_uint8 (src[2], dst);
  *(dst++) = '.';
  dst = put_uint8 (src[3], dst);
  *dst++ = '\0';
  return dst;
}

static __always_inline const char *
inet_ntop4 (const uint8_t *src, char *dst, socklen_t size)
{
  if (size >= INET_ADDRSTRLEN)
    {
      inet_ntop4_format (src, dst);
      return dst;
    }

  char tmp[INET_ADDRSTRLEN];
  char *tp = inet_ntop4_format (src, tmp);
  socklen_t tmp_s = tp - tmp;
  if (tmp_s > size)
    {
      __set_errno (ENOSPC);
      return NULL;
    }
  return memcpy (dst, tmp, tmp_s);
}

struct best_t
{
  int base;
  int len;
};

static inline uint16_t
in6_addr_addr16 (const struct in6_addr *src, int idx)
{
  const struct { uint16_t x; } __attribute__((__packed__)) *pptr =
    (typeof(pptr))(&src->s6_addr16[idx]);
  return ntohs (pptr->x);
}

static __always_inline char *
inet_ntop6_format (const struct in6_addr *src, struct best_t best, char *dst)
{
  char *tp = dst;
  for (int i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
    {
      /* Are we inside the best run of 0x00's? */
      if (best.base != -1 && i >= best.base && i < (best.base + best.len))
	{
	  if (i == best.base)
	    *tp++ = ':';
	  continue;
	}
      /* Are we following an initial run of 0x00s or any real hex? */
      if (i != 0)
	*tp++ = ':';
      /* Is this address an encapsulated IPv4? */
      if (i == 6 && best.base == 0
	  && (best.len == 6 || (best.len == 5
				&& in6_addr_addr16 (src, 5) == 0xffff)))
	{
	  if (!inet_ntop4 (src->s6_addr + 12, tp,
			   INET6_ADDRSTRLEN - (tp - dst)))
	    return NULL;
	  tp += strlen (tp);
	  break;
	}
      tp = put_uint16 (in6_addr_addr16 (src, i), tp);
    }
  /* Was it a trailing run of 0x00's? */
  if (best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ))
    *tp++ = ':';
  *tp++ = '\0';

  return tp;
}

static inline const char *
inet_ntop6 (const struct in6_addr *src, char *dst, socklen_t size)
{
  struct best_t best = { -1, 0 }, cur = { -1, 0 };

  /* ind the longest run of 0x00's in src[] for :: shorthanding.  */
  for (int i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
    {
      if (in6_addr_addr16 (src, i) == 0)
	{
	  if (cur.base == -1)
	    cur.base = i, cur.len = 1;
	  else
	    cur.len++;
	}
      else
	{
	  if (cur.base != -1)
	    {
	      if (best.base == -1 || cur.len > best.len)
		best = cur;
	      cur.base = -1;
	    }
	}
    }
  if (cur.base != -1)
    {
      if (best.base == -1 || cur.len > best.len)
	best = cur;
    }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

  if (size >= INET6_ADDRSTRLEN)
    {
      inet_ntop6_format (src, best, dst);
      return dst;
    }

  char tmp[INET6_ADDRSTRLEN];
  char *tp = inet_ntop6_format (src, best, tmp);

  socklen_t tmp_s = tp - tmp;
  if (tmp_s > size)
    {
      __set_errno (ENOSPC);
      return (NULL);
    }
  return memcpy (dst, tmp, tmp_s);
}

const char *
__inet_ntop (int af, const void *src, char *dst, socklen_t size)
{
  switch (af)
    {
    case AF_INET:
      return (inet_ntop4 (src, dst, size));
    case AF_INET6:
      return (inet_ntop6 (src, dst, size));
    default:
      __set_errno (EAFNOSUPPORT);
      return (NULL);
    }
}
libc_hidden_def (__inet_ntop)
weak_alias (__inet_ntop, inet_ntop)
