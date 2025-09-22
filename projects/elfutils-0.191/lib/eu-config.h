/* Configuration definitions.
   Copyright (C) 2008, 2009 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef EU_CONFIG_H
#define EU_CONFIG_H	1

#ifdef USE_LOCKS
# include <pthread.h>
# include <assert.h>
# define rwlock_define(class,name)	class pthread_rwlock_t name
# define once_define(class,name)  class pthread_once_t name = PTHREAD_ONCE_INIT
# define RWLOCK_CALL(call)		\
  ({ int _err = pthread_rwlock_ ## call; assert_perror (_err); })
# define ONCE_CALL(call)  \
  ({ int _err = pthread_ ## call; assert_perror (_err); })
# define rwlock_init(lock)		RWLOCK_CALL (init (&lock, NULL))
# define rwlock_fini(lock)		RWLOCK_CALL (destroy (&lock))
# define rwlock_rdlock(lock)		RWLOCK_CALL (rdlock (&lock))
# define rwlock_wrlock(lock)		RWLOCK_CALL (wrlock (&lock))
# define rwlock_unlock(lock)		RWLOCK_CALL (unlock (&lock))
# define once(once_control, init_routine)  \
  ONCE_CALL (once (&once_control, init_routine))
#else
/* Eventually we will allow multi-threaded applications to use the
   libraries.  Therefore we will add the necessary locking although
   the macros used expand to nothing for now.  */
# define rwlock_define(class,name) class int name
# define rwlock_init(lock) ((void) (lock))
# define rwlock_fini(lock) ((void) (lock))
# define rwlock_rdlock(lock) ((void) (lock))
# define rwlock_wrlock(lock) ((void) (lock))
# define rwlock_unlock(lock) ((void) (lock))
# define once_define(class,name)
# define once(once_control, init_routine)	init_routine()
#endif	/* USE_LOCKS */

#include <libintl.h>
/* gettext helper macros.  */
#define N_(Str) Str
#define _(Str) dgettext ("elfutils", Str)

/* Compiler-specific definitions.  */
#define strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

#ifdef __i386__
# define internal_function __attribute__ ((regparm (3), stdcall))
#else
# define internal_function /* nothing */
#endif

#define internal_strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name))) internal_function;

#ifdef HAVE_VISIBILITY
#define attribute_hidden \
  __attribute__ ((visibility ("hidden")))
#else
#define attribute_hidden /* empty */
#endif

#ifdef HAVE_GCC_STRUCT
#define attribute_packed \
  __attribute__ ((packed, gcc_struct))
#else
#define attribute_packed \
  __attribute__ ((packed))
#endif

/* Define ALLOW_UNALIGNED if the architecture allows operations on
   unaligned memory locations.  */
#define SANITIZE_UNDEFINED 1
#if (defined __i386__ || defined __x86_64__) && ! CHECK_UNDEFINED
# define ALLOW_UNALIGNED	1
#else
# define ALLOW_UNALIGNED	0
#endif

#if DEBUGPRED
# ifdef __x86_64__
asm (".section predict_data, \"aw\"; .previous\n"
     ".section predict_line, \"a\"; .previous\n"
     ".section predict_file, \"a\"; .previous");
#  ifndef PIC
#   define debugpred__(e, E) \
  ({ long int _e = !!(e); \
     asm volatile (".pushsection predict_data; ..predictcnt%=: .quad 0; .quad 0\n" \
                   ".section predict_line; .quad %c1\n" \
                   ".section predict_file; .quad %c2; .popsection\n" \
                   "addq $1,..predictcnt%=(,%0,8)" \
                   : : "r" (_e == E), "i" (__LINE__), "i" (__FILE__)); \
    __builtin_expect (_e, E); \
  })
#  endif
# elif defined __i386__
asm (".section predict_data, \"aw\"; .previous\n"
     ".section predict_line, \"a\"; .previous\n"
     ".section predict_file, \"a\"; .previous");
#  ifndef PIC
#   define debugpred__(e, E) \
  ({ long int _e = !!(e); \
     asm volatile (".pushsection predict_data; ..predictcnt%=: .long 0; .long 0\n" \
                   ".section predict_line; .long %c1\n" \
                   ".section predict_file; .long %c2; .popsection\n" \
                   "incl ..predictcnt%=(,%0,8)" \
                   : : "r" (_e == E), "i" (__LINE__), "i" (__FILE__)); \
    __builtin_expect (_e, E); \
  })
#  endif
# endif
# ifdef debugpred__
#  define unlikely(e) debugpred__ (e,0)
#  define likely(e) debugpred__ (e,1)
# endif
#endif
#ifndef likely
# define unlikely(expr) __builtin_expect (!!(expr), 0)
# define likely(expr) __builtin_expect (!!(expr), 1)
#endif

#define obstack_calloc(ob, size) \
  ({ size_t _s = (size); memset (obstack_alloc (ob, _s), '\0', _s); })
#define obstack_strdup(ob, str) \
  ({ const char *_s = (str); obstack_copy0 (ob, _s, strlen (_s)); })
#define obstack_strndup(ob, str, n) \
  ({ const char *_s = (str); obstack_copy0 (ob, _s, strnlen (_s, n)); })

#if __STDC_VERSION__ >= 199901L
# define flexarr_size /* empty */
#else
# define flexarr_size 0
#endif

/* Calling conventions.  */
#ifdef __i386__
# define CALLING_CONVENTION regparm (3), stdcall
# define AND_CALLING_CONVENTION , regparm (3), stdcall
#else
# define CALLING_CONVENTION
# define AND_CALLING_CONVENTION
#endif

/* Avoid PLT entries.  */
#ifdef PIC
# define INTUSE(name) _INTUSE(name)
# define _INTUSE(name) __##name##_internal
# define INTDEF(name) _INTDEF(name)
# define _INTDEF(name) \
  extern __typeof__ (name) __##name##_internal __attribute__ ((alias (#name)));
# define INTDECL(name) _INTDECL(name)
# define _INTDECL(name) \
  extern __typeof__ (name) __##name##_internal attribute_hidden;
#else
# define INTUSE(name) name
# define INTDEF(name) /* empty */
# define INTDECL(name) /* empty */
#endif

/* This macro is used by the tests conditionalize for standalone building.  */
#define ELFUTILS_HEADER(name) <lib##name.h>

/* Don't reorder with global asm blocks or optimize away. (Doesn't reliably
   keep it in the same LTO partition, though; -flto-partition=none may be
   still needed for some gcc versions < 10.) */
#ifdef __has_attribute
# if __has_attribute(no_reorder)
#  define used_in_asm __attribute__ ((externally_visible, no_reorder))
# endif
#endif
#ifndef used_in_asm
# define used_in_asm /* empty */
#endif

#ifdef SYMBOL_VERSIONING
# define NEW_INTDEF(name) __typeof (name) INTUSE(name) \
  __attribute__ ((alias ("_new." #name))) attribute_hidden;
# ifdef __has_attribute
#  if __has_attribute(symver)
#   define NEW_VERSION(name, version) \
  __typeof (name) name __asm__ ("_new." #name) \
    __attribute__ ((symver (#name "@@" #version)));
#   define OLD_VERSION(name, version) _OLD_VERSION1(name, __COUNTER__, version)
#   define _OLD_VERSION1(name, num, version) _OLD_VERSION2(name, num, version)
#   define _OLD_VERSION2(name, num, version) \
  __typeof (name) _compat_old##num##_##name \
    __asm__ ("_compat." #version "." #name) \
    __attribute__ ((alias ("_new." #name), symver (#name "@" #version)));
#   define COMPAT_VERSION_NEWPROTO(name, version, prefix) \
  __typeof (_compat_##prefix##_##name) _compat_##prefix##_##name \
    __asm__ ("_compat." #version "." #name) \
    __attribute__ ((symver (#name "@" #version)));
#   define COMPAT_VERSION(name, version, prefix) \
  asm (".symver _compat." #version "." #name "," #name "@" #version); \
  __typeof (name) _compat_##prefix##_##name \
    __asm__ ("_compat." #version "." #name) \
    __attribute__ ((symver (#name "@" #version)));
#  endif
# endif
# ifndef NEW_VERSION
#  define OLD_VERSION(name, version) \
  asm (".globl _compat." #version "." #name "\n\t" \
       "_compat." #version "." #name " = _new." #name "\n\t" \
       ".symver _compat." #version "." #name "," #name "@" #version);
#  define NEW_VERSION(name, version) \
  __typeof (name) name __asm__ ("_new." #name) used_in_asm; \
  asm (".symver _new." #name ", " #name "@@" #version);
#  define COMPAT_VERSION_NEWPROTO(name, version, prefix) \
  __typeof (_compat_##prefix##_##name) _compat_##prefix##_##name \
    __asm__ ("_compat." #version "." #name) used_in_asm; \
  asm (".symver _compat." #version "." #name ", " #name "@" #version);
#  define COMPAT_VERSION(name, version, prefix) \
  __typeof (name) _compat_##prefix##_##name \
    __asm__ ("_compat." #version "." #name) used_in_asm; \
  asm (".symver _compat." #version "." #name ", " #name "@" #version);
# endif
#else
# define NEW_INTDEF(name) INTDEF(name)
# define OLD_VERSION(name, version) /* Nothing for static linking.  */
# define NEW_VERSION(name, version) /* Nothing for static linking.  */
# define COMPAT_VERSION_NEWPROTO(name, version, prefix) \
  error "should use #ifdef SYMBOL_VERSIONING"
# define COMPAT_VERSION(name, version, prefix) \
  error "should use #ifdef SYMBOL_VERSIONING"
#endif

#ifndef FALLTHROUGH
# ifdef HAVE_FALLTHROUGH
#  define FALLTHROUGH __attribute__ ((fallthrough))
# else
#  define FALLTHROUGH ((void) 0)
# endif
#endif

#endif	/* eu-config.h */
