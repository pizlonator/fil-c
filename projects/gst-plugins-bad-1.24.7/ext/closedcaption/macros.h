/*
 *  libzvbi -- Useful macros
 *
 *  Copyright (C) 2002, 2003, 2004, 2007 Michael H. Schimek
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

/* $Id: macros.h,v 1.12 2013-07-10 23:11:18 mschimek Exp $ */

#ifndef __ZVBI_MACROS_H__
#define __ZVBI_MACROS_H__

#ifdef __cplusplus
#  define VBI_BEGIN_DECLS extern "C" {
#  define VBI_END_DECLS }
#else
#  define VBI_BEGIN_DECLS
#  define VBI_END_DECLS
#endif

VBI_BEGIN_DECLS

/* Public */

#if __GNUC__ >= 4
#  define _vbi_sentinel __attribute__ ((__sentinel__(0)))
#  define _vbi_deprecated __attribute__ ((__deprecated__))
#else
#  define _vbi_sentinel
#  define _vbi_deprecated
#  define __restrict__
#endif

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#  define _vbi_nonnull(params) __attribute__ ((__nonnull__ params))
#  define _vbi_format(params) __attribute__ ((__format__ params))
#else
#  define _vbi_nonnull(params)
#  define _vbi_format(params)
#endif

#if __GNUC__ >= 3
#  define _vbi_pure __attribute__ ((__pure__))
#  define _vbi_alloc __attribute__ ((__malloc__))
#else
#  define _vbi_pure
#  define _vbi_alloc
#endif

#if __GNUC__ >= 2
#  define _vbi_unused __attribute__ ((__unused__))
#  define _vbi_const __attribute__ ((__const__))
#  define _vbi_inline static __inline__
#else
#  define _vbi_unused
#  define _vbi_const
#  define _vbi_inline static
#endif

/**
 * @ingroup Basic
 * @name Boolean type
 * @{
 */
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

typedef int vbi_bool;
/** @} */

#ifndef NULL
#  ifdef __cplusplus
#    define NULL (0L)
#  else
#    define NULL ((void *) 0)
#  endif
#endif

/* XXX Document me - for variadic funcs. */
#define VBI_END ((void *) 0)

#if 0
typedef void
vbi_lock_fn			(void *			user_data);
typedef void
vbi_unlock_fn			(void *			user_data);
#endif

/**
 * @ingroup Basic
 * @{
 */
typedef enum {
	/** External error causes, for example lack of memory. */
	VBI_LOG_ERROR		= 1 << 3,

	/**
	 * Invalid parameters and similar problems which suggest
	 * a bug in the application using the library.
	 */
	VBI_LOG_WARNING		= 1 << 4,

	/**
	 * Causes of possibly undesired results, for example when a
	 * data service cannot be decoded with the current video
	 * standard setting.
	 */
	VBI_LOG_NOTICE		= 1 << 5,

	/** Progress messages. */
	VBI_LOG_INFO		= 1 << 6,

	/** Information useful to debug the library. */
	VBI_LOG_DEBUG		= 1 << 7,

	/** Driver responses (strace). Not implemented yet. */
	VBI_LOG_DRIVER		= 1 << 8,

	/** More detailed debugging information. */
	VBI_LOG_DEBUG2		= 1 << 9,
	VBI_LOG_DEBUG3		= 1 << 10
} vbi_log_mask;

typedef void
vbi_log_fn			(vbi_log_mask		level,
				 const char *		context,
				 const char *		message,
				 void *			user_data);

extern vbi_log_fn		vbi_log_on_stderr;
/** @} */

/* Private */

typedef struct {
	vbi_log_fn *		fn;
	void *			user_data;
	vbi_log_mask		mask;
} _vbi_log_hook;

VBI_END_DECLS

#endif /* __ZVBI_MACROS_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
