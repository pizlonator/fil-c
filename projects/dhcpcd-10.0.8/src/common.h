/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
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

#ifndef COMMON_H
#define COMMON_H

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

/* Define eloop queues here, as other apps share eloop.h */
#define	ELOOP_DHCPCD		1 /* default queue */
#define	ELOOP_DHCP		2
#define	ELOOP_ARP		3
#define	ELOOP_IPV4LL		4
#define	ELOOP_IPV6		5
#define	ELOOP_IPV6ND		6
#define	ELOOP_IPV6RA_EXPIRE	7
#define	ELOOP_DHCP6		8
#define	ELOOP_IF		9

#ifndef HOSTNAME_MAX_LEN
#define HOSTNAME_MAX_LEN	250	/* 255 - 3 (FQDN) - 2 (DNS enc) */
#endif

#ifndef MIN
#define MIN(a,b)		((/*CONSTCOND*/(a)<(b))?(a):(b))
#define MAX(a,b)		((/*CONSTCOND*/(a)>(b))?(a):(b))
#endif

#define UNCONST(a)		((void *)(unsigned long)(const void *)(a))
#define STRINGIFY(a)		#a
#define TOSTRING(a)		STRINGIFY(a)
#define UNUSED(a)		(void)(a)

#define ROUNDUP4(a)		(1 + (((a) - 1) |  3))
#define ROUNDUP8(a)		(1 + (((a) - 1) |  7))

/* Some systems don't define timespec macros */
#ifndef timespecclear
#define timespecclear(tsp)      (tsp)->tv_sec = (time_t)((tsp)->tv_nsec = 0L)
#define timespecisset(tsp)      ((tsp)->tv_sec || (tsp)->tv_nsec)
#endif

#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# ifndef __packed
#  define __packed __attribute__((__packed__))
# endif
# ifndef __unused
#  define __unused __attribute__((__unused__))
# endif
#else
# ifndef __packed
#  define __packed
# endif
# ifndef __unused
#  define __unused
# endif
#endif

/* Needed for rbtree(3) compat */
#ifndef __RCSID
#define __RCSID(a)
#endif
#ifndef __predict_false
# if __GNUC__ > 2
#  define	__predict_true(exp)	__builtin_expect((exp) != 0, 1)
#  define	__predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
#  define	__predict_true(exp)	(exp)
#  define	__predict_false(exp)	(exp)
# endif
#endif
#ifndef __BEGIN_DECLS
# if defined(__cplusplus)
#  define	__BEGIN_DECLS		extern "C" {
#  define	__END_DECLS		};
# else /* __BEGIN_DECLS */
#  define	__BEGIN_DECLS
#  define	__END_DECLS
# endif /* __BEGIN_DECLS */
#endif /* __BEGIN_DECLS */

#ifndef __fallthrough
# if __GNUC__ >= 7
#  define __fallthrough __attribute__((fallthrough))
# else
#  define __fallthrough
# endif
#endif

/*
 * Compile Time Assertion.
 */
#ifndef __CTASSERT
# ifdef __COUNTER__
#   define	__CTASSERT(x)		__CTASSERT0(x, __ctassert, __COUNTER__)
# else
#  define	__CTASSERT(x)		__CTASSERT99(x, __INCLUDE_LEVEL__, __LINE__)
#  define	__CTASSERT99(x, a, b)	__CTASSERT0(x, __CONCAT(__ctassert,a), \
					       __CONCAT(_,b))
# endif
# define	__CTASSERT0(x, y, z)	__CTASSERT1(x, y, z)
# define	__CTASSERT1(x, y, z)	typedef char y ## z[/*CONSTCOND*/(x) ? 1 : -1] __unused
#endif

#ifndef __arraycount
#  define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

/* We don't really need this as our supported systems define __restrict
 * automatically for us, but it is here for completeness. */
#ifndef __restrict
# if defined(__lint__)
#  define __restrict
# elif __STDC_VERSION__ >= 199901L
#  define __restrict restrict
# elif !(2 < __GNUC__ || (2 == __GNU_C && 95 <= __GNUC_VERSION__))
#  define __restrict
# endif
#endif

const char *hwaddr_ntoa(const void *, size_t, char *, size_t);
size_t hwaddr_aton(uint8_t *, const char *);
ssize_t readfile(const char *, void *, size_t);
ssize_t writefile(const char *, mode_t, const void *, size_t);
int filemtime(const char *, time_t *);
char *get_line(char ** __restrict, ssize_t * __restrict);
int is_root_local(void);
#endif
