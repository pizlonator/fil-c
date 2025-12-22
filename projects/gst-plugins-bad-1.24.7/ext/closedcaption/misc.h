/*
 *  libzvbi -- Miscellaneous cows and chickens
 *
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
 *  Copyright (C) 2002-2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: misc.h,v 1.24 2013-07-02 02:32:31 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>		/* (u)intXX_t */
#include <sys/types.h>		/* (s)size_t */
#include <float.h>		/* DBL_MAX */
#include <limits.h>		/* (S)SIZE_MAX */
#include <assert.h>
#include <glib.h>
#include <gst/gst.h>

#include "macros.h"

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
/* Expect expression usually true/false, schedule accordingly. */
#  define likely(expr) (expr)
#  define unlikely(expr) (expr)
#else
#  define likely(expr) __builtin_expect(expr, 1)
#  define unlikely(expr) __builtin_expect(expr, 0)
#endif

#undef __i386__
#undef __i686__
/* FIXME #cpu is deprecated
#if #cpu (i386)
#  define __i386__ 1
#endif
#if #cpu (i686)
#  define __i686__ 1
#endif
*/

/* &x == PARENT (&x.tm_min, struct tm, tm_min),
   safer than &x == (struct tm *) &x.tm_min. A NULL _ptr is safe and
   will return NULL, not -offsetof(_member). */
#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((_type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

/* Like PARENT(), to be used with const _ptr. */
#define CONST_PARENT(_ptr, _type, _member) ({				\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (const _type *)(((const char *) _p) - offsetof	\
	 (const _type, _member)) : (const _type *) 0;			\
})

/* Note the following macros have no side effects only when you
   compile with GCC, so don't expect this. */

/* Absolute value of int, long or long long without a branch.
   Note ABS (INT_MIN) -> INT_MAX + 1. */
#undef ABS
#define ABS(n) ({							\
	register __typeof__ (n) _n = (n), _t = _n;			\
	if (-1 == (-1 >> 1)) { /* do we have signed shifts? */		\
		_t >>= sizeof (_t) * 8 - 1;				\
		_n ^= _t;						\
		_n -= _t;						\
	} else if (_n < 0) { /* also warns if n is unsigned type */	\
		_n = -_n;						\
	}								\
	/* return */ _n;						\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* warn if types do not match */		\
	/* return */ (_x < _y) ? _x : _y;				\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* warn if types do not match */		\
	/* return */ (_x > _y) ? _x : _y;				\
})

/* Note other compilers may swap only int, long or pointer. */
#undef SWAP
#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#ifdef __i686__ /* has conditional move */
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* warn if types do not match */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	/* return */ _n;						\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* warn if types do not match */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	/* return */ _n;						\
})
#endif

#else /* !__GNUC__ */

#define likely(expr) (expr)
#define unlikely(expr) (expr)
#undef __i386__
#undef __i686__

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (0 == p) ? ((char *) 0) : p - offset; }

static const char *
CONST_PARENT_HELPER (const char *p, unsigned int offset)
{ return (0 == p) ? ((char *) 0) : p - offset; }

#define PARENT(_ptr, _type, _member)					\
	((0 == offsetof (_type, _member)) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))
#define CONST_PARENT(_ptr, _type, _member)				\
	((0 == offsetof (const _type, _member)) ? (const _type *)(_ptr)	\
	 : (const _type *) CONST_PARENT_HELPER ((const char *)(_ptr),	\
	  offsetof (const _type, _member)))

#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))

#undef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#undef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#undef SWAP
#define SWAP(x, y)							\
do {									\
	long _x = x;							\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#define SATURATE(n, min, max) MIN (MAX (min, n), max)

#endif /* !__GNUC__ */

/* 32 bit constant byte reverse, e.g. 0xAABBCCDD -> 0xDDCCBBAA. */
#define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0xFF0000) >> 8)					\
	 + (((m) & 0xFF00) << 8)					\
	 + (((m) & 0xFF) << 24))

#ifdef HAVE_BUILTIN_POPCOUNT
#  define popcnt(x) __builtin_popcount ((uint32_t)(x))
#else
#  define popcnt(x) _vbi_popcnt (x)
#endif

extern unsigned int
_vbi_popcnt			(uint32_t		x);

/* NB GCC inlines and optimizes these functions when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))

#define CLEAR(var) memset (&(var), 0, sizeof (var))

/* Useful to copy arrays, otherwise use assignment. */
#define COPY(d, s)							\
	(assert (sizeof (d) == sizeof (s)), memcpy (d, s, sizeof (d)))

/* Copy string const into char array. */
#define STRACPY(array, s)						\
do {									\
	/* Complain if s is no string const or won't fit. */		\
	const char t_[sizeof (array) - 1] _vbi_unused = s;		\
									\
	memcpy (array, s, sizeof (s));					\
} while (0)

/* Copy bits through mask. */
#define COPY_SET_MASK(dest, from, mask)					\
	(dest ^= (from) ^ (dest & (mask)))

/* Set bits if cond is TRUE, clear if FALSE. */
#define COPY_SET_COND(dest, bits, cond)					\
	 ((cond) ? (dest |= (bits)) : (dest &= ~(bits)))

/* Set and clear bits. */
#define COPY_SET_CLEAR(dest, set, clear)				\
	(dest = (dest & ~(clear)) | (set))

/* For applications, debugging and fault injection during unit tests. */

#define vbi_malloc malloc
#define vbi_realloc realloc
#define vbi_strdup strdup
#define vbi_free free

#define vbi_cache_malloc vbi_malloc
#define vbi_cache_free vbi_free

/* Helper functions. */

_vbi_inline int
_vbi_to_ascii			(int			c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

typedef struct {
	const char *		key;
	int			value;
} _vbi_key_value_pair;

extern vbi_bool
_vbi_keyword_lookup		(int *			value,
				 const char **		inout_s,
				 const _vbi_key_value_pair * table,
				 unsigned int		n_pairs)
  _vbi_nonnull ((1, 2, 3));

extern void
_vbi_shrink_vector_capacity	(void **		vector,
				 size_t *		capacity,
				 size_t			min_capacity,
				 size_t			element_size)
  _vbi_nonnull ((1, 2));
extern vbi_bool
_vbi_grow_vector_capacity	(void **		vector,
				 size_t *		capacity,
				 size_t			min_capacity,
				 size_t			element_size)
  _vbi_nonnull ((1, 2));

GST_DEBUG_CATEGORY_EXTERN (libzvbi_debug);

#ifndef GST_DISABLE_GST_DEBUG
/* Logging stuff. */
#ifdef G_HAVE_ISO_VARARGS
#define VBI_CAT_LEVEL_LOG(level,object,...) G_STMT_START{		\
  if (G_UNLIKELY ((level) <= GST_LEVEL_MAX && (level) <= _gst_debug_min)) {						\
	  gst_debug_log (libzvbi_debug, (level), __FILE__, GST_FUNCTION, __LINE__, \
        (GObject *) (object), __VA_ARGS__);				\
  }									\
}G_STMT_END
#else /* G_HAVE_GNUC_VARARGS */
#ifdef G_HAVE_GNUC_VARARGS
#define VBI_CAT_LEVEL_LOG(level,object,args...) G_STMT_START{	\
  if (G_UNLIKELY ((level) <= GST_LEVEL_MAX && (level) <= _gst_debug_min)) {						\
	  gst_debug_log (libzvbi_debug, (level), __FILE__, GST_FUNCTION, __LINE__, \
        (GObject *) (object), ##args );					\
  }									\
}G_STMT_END
#else /* no variadic macros, use inline */
static inline void
VBI_CAT_LEVEL_LOG_valist (GstDebugCategory * cat,
    GstDebugLevel level, gpointer object, const char *format, va_list varargs)
{
  if (G_UNLIKELY ((level) <= GST_LEVEL_MAX && (level) <= _gst_debug_min)) {
    gst_debug_log_valist (cat, level, "", "", 0, (GObject *) object, format,
        varargs);
  }
}

static inline void
VBI_CAT_LEVEL_LOG (GstDebugLevel level,
    gpointer object, const char *format, ...)
{
  va_list varargs;

  va_start (varargs, format);
  GST_CAT_LEVEL_LOG_valist (libzvbi_debug, level, object, format, varargs);
  va_end (varargs);
}
#endif
#endif /* G_HAVE_ISO_VARARGS */
#else
static inline void
VBI_CAT_LEVEL_LOG (GstDebugLevel level,
    gpointer object, const char *format, ...)
{
}
#endif	/* GST_DISABLE_GST_DEBUG */

#ifdef G_HAVE_GNUC_VARARGS
#define error(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_ERROR, NULL, templ , ##args)
#define warn(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_WARNING, NULL, templ , ##args)
#define notice(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_INFO, NULL, templ , ##args)
#define info(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_INFO, NULL, templ , ##args)
#define debug1(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_DEBUG, NULL, templ , ##args)
#define debug2(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_LOG, NULL, templ , ##args)
#define debug3(hook, templ, args...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_TRACE, NULL, templ , ##args)
#elif defined(G_HAVE_ISO_VARARGS)
#define error(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_ERROR, NULL, templ, __VA_ARGS__)
#define warn(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_WARNING, NULL, templ, __VA_ARGS__)
#define notice(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_INFO, NULL, templ, __VA_ARGS__)
#define info(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_INFO, NULL, templ, __VA_ARGS__)
#define debug1(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_DEBUG, NULL, templ, __VA_ARGS__)
#define debug2(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_LOG, NULL, templ, __VA_ARGS__)
#define debug3(hook, templ, ...)					\
	VBI_CAT_LEVEL_LOG (GST_LEVEL_TRACE, NULL, templ, __VA_ARGS__)
#else
/* if someone needs this, they can implement the inline functions for it */
#error "variadic macros are required"
#endif


#if 0				/* Replaced logging with GStreamer logging system */
extern _vbi_log_hook		_vbi_global_log;

extern void
_vbi_log_vprintf		(vbi_log_fn *		log_fn,
				 void *			user_data,
				 vbi_log_mask		level,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 va_list		ap)
  _vbi_nonnull ((1, 4, 5, 6));
extern void
_vbi_log_printf		(vbi_log_fn *		log_fn,
				 void *			user_data,
				 vbi_log_mask		level,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 ...)
  _vbi_nonnull ((1, 4, 5, 6)) _vbi_format ((printf, 6, 7));

#define _vbi_log(hook, level, templ, args...)				\
do {									\
	_vbi_log_hook *_h = hook;					\
									\
	if ((NULL != _h && 0 != (_h->mask & level))			\
	    || (_h = &_vbi_global_log, 0 != (_h->mask & level)))	\
		_vbi_log_printf (_h->fn, _h->user_data,		\
				  level, __FILE__, __FUNCTION__,	\
				  templ , ##args);			\
} while (0)

#define _vbi_vlog(hook, level, templ, ap)				\
do {									\
	_vbi_log_hook *_h = hook;					\
									\
	if ((NULL != _h && 0 != (_h->mask & level))			\
	    || (_h = &_vbi_global_log, 0 != (_h->mask & level)))	\
		_vbi_log_vprintf (_h->fn, _h->user_data,		\
				  level, __FILE__, __FUNCTION__,	\
				  templ, ap);				\
} while (0)
#define error(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_ERROR, templ , ##args)
#define warning(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_ERROR, templ , ##args)
#define notice(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_NOTICE, templ , ##args)
#define info(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_INFO, templ , ##args)
#define debug1(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_DEBUG, templ , ##args)
#define debug2(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_DEBUG2, templ , ##args)
#define debug3(hook, templ, args...)					\
	_vbi_log (hook, VBI_LOG_DEBUG3, templ , ##args)
#endif

/* Portability stuff. */

/* These should be defined in inttypes.h. */
#ifndef PRId64
#  define PRId64 "lld"
#endif
#ifndef PRIu64
#  define PRIu64 "llu"
#endif
#ifndef PRIx64
#  define PRIx64 "llx"
#endif

/* Should be defined in C99 limits.h? */
#ifndef SIZE_MAX
#  define SIZE_MAX ((size_t) -1)
#endif

#ifndef TIME_MIN
#  define TIME_MIN (_vbi_time_min ())
_vbi_inline time_t
_vbi_time_min			(void)
{
	const time_t t = (time_t) -1.25;

	if (t < -1) {
		return (time_t)((sizeof (time_t) > 4) ? DBL_MIN : FLT_MIN);
	} else if (t < 0) {
		return ((uint64_t) 1) << (sizeof (time_t) * 8 - 1);
	} else {
		return 0;
	}
}
#endif

#ifndef TIME_MAX
#  define TIME_MAX (_vbi_time_max ())
_vbi_inline time_t
_vbi_time_max			(void)
{
	const time_t t = (time_t) -1.25;

	if (t < -1) {
		return (time_t)((sizeof (time_t) > 4) ? DBL_MAX : FLT_MAX);
	} else if (t < 0) {
		/* Most likely signed 32 or 64 bit. */
		return (((uint64_t) 1) << (sizeof (time_t) * 8 - 1)) - 1;
	} else {
		return -1;
	}
}
#endif

/* __va_copy is a GNU extension. */
#ifndef __va_copy
#  define __va_copy(ap1, ap2) do { ap1 = ap2; } while (0)
#endif

#if 0
/* Use this instead of strncpy(). strlcpy() is a BSD extension. */
#ifndef HAVE_STRLCPY
#  define strlcpy _vbi_strlcpy
#endif
#undef strncpy
#define strncpy use_strlcpy_instead

extern size_t
_vbi_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			size)
  _vbi_nonnull ((1, 2));
#endif

/* /\* strndup() is a BSD/GNU extension. *\/ */
/* #ifndef HAVE_STRNDUP */
/* #  define strndup _vbi_strndup */
/* #endif */

/* extern char * */
/* _vbi_strndup			(const char *		s, */
/* 				 size_t			len) */
/*   _vbi_nonnull ((1)); */

/* vasprintf() is a GNU extension. */
#ifndef HAVE_VASPRINTF
#  define vasprintf _vbi_vasprintf
#endif

extern int
_vbi_vasprintf			(char **		dstp,
				 const char *		templ,
				 va_list		ap)
  _vbi_nonnull ((1, 2));

/* asprintf() is a GNU extension. */
#ifndef HAVE_ASPRINTF
#  define asprintf _vbi_asprintf
#endif

extern int
_vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
  _vbi_nonnull ((1, 2)) _vbi_format ((printf, 2, 3));

#undef sprintf
#define sprintf use_snprintf_or_asprintf_instead

#endif /* MISC_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
