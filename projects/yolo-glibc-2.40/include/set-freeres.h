/* Macros for internal resource Freeing Hooks.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _SET_FREERES_H
#define _SET_FREERES_H 1

#include <shlib-compat.h>
#include <printf.h>
#include <time.h>
#include <resolv/resolv-internal.h>

/* Resource Freeing Hooks:

   Normally a process exits and the OS cleans up any allocated
   memory.  However, when tooling like mtrace or valgrind is monitoring
   the process we need to free all resources that are part of the
   process in order to provide the consistency required to track
   memory leaks.

   A single public API exists and is __libc_freeres, and this is used
   by applications like valgrind to free resources.

   Each free routines must be explicit listed below.  */

/* From libc.so.  */
extern void __dl_libc_freemem (void) attribute_hidden;
extern void __hdestroy (void) attribute_hidden;
extern void __gconv_cache_freemem (void) attribute_hidden;
extern void __gconv_conf_freemem (void) attribute_hidden;
extern void __gconv_db_freemem (void) attribute_hidden;
extern void __gconv_dl_freemem (void) attribute_hidden;
extern void __intl_freemem (void) attribute_hidden;
extern void __libio_freemem (void) attribute_hidden;
extern void __libc_fstab_freemem (void) attribute_hidden;
extern void __nscd_gr_map_freemem (void) attribute_hidden;
extern void __nscd_hst_map_freemem (void) attribute_hidden;
extern void __nscd_pw_map_freemem (void) attribute_hidden;
extern void __nscd_serv_map_freemem (void) attribute_hidden;
extern void __nscd_group_map_freemem (void) attribute_hidden;
extern void __libc_regcomp_freemem (void) attribute_hidden;
extern void __libc_atfork_freemem (void) attribute_hidden;
extern void __libc_resolv_conf_freemem (void) attribute_hidden;
extern void __res_thread_freeres (void) attribute_hidden;
extern void __libc_printf_freemem (void) attribute_hidden;
extern void __libc_fmtmsg_freemem (void) attribute_hidden;
extern void __libc_setenv_freemem (void) attribute_hidden;
#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_31)
extern void __rpc_freemem (void) attribute_hidden;
extern void __rpc_thread_destroy (void) attribute_hidden;
#endif
extern void __libc_getaddrinfo_freemem (void) attribute_hidden;
extern void __libc_tzset_freemem (void) attribute_hidden;
extern void __libc_localealias_freemem (void) attribute_hidden;
extern void __libc_getutent_freemem (void) attribute_hidden;
extern void __libc_getutline_freemem (void) attribute_hidden;
/* From nss/nss_module.c */
extern void __nss_module_freeres (void) attribute_hidden;
/* From nss/nss_action.c */
extern void __nss_action_freeres (void) attribute_hidden;
/* From nss/nss_database.c */
extern void __nss_database_freeres (void) attribute_hidden;
/* From libio/genops.c */
extern int _IO_cleanup (void) attribute_hidden;;
/* From dlfcn/dlerror.c */
extern void __libc_dlerror_result_free (void) attribute_hidden;

/* From either libc.so or libpthread.so  */
extern void __libpthread_freeres (void) attribute_hidden;
/* From either libc.so or libanl.so  */
#if PTHREAD_IN_LIBC
extern void __gai_freemem (void) attribute_hidden;
/* From either libc.so or librt.so  */
extern void __aio_freemem (void) attribute_hidden;
#endif

/* From libc.so  */
extern char * __libc_fgetgrent_freemem_ptr attribute_hidden;
extern char * __libc_fgetsgent_freeres_ptr attribute_hidden;
extern char * __libc_getnetgrent_freemem_ptr attribute_hidden;
extern char * __libc_rcmd_freemem_ptr attribute_hidden;
extern char * __libc_rexec_freemem_ptr attribute_hidden;
extern void * __libc_mntent_freemem_ptr attribute_hidden;
extern char * __libc_fgetpwent_freemem_ptr attribute_hidden;
extern struct netaddr * __libc_resolv_res_hconf_freemem_ptr attribute_hidden;
extern char * __libc_fgetspent_freemem_ptr attribute_hidden;
extern __time64_t * __libc_tzfile_freemem_ptr attribute_hidden;
extern char * __libc_getnameinfo_freemem_ptr attribute_hidden;
extern struct utmp * __libc_getutent_freemem_ptr attribute_hidden;
extern struct utmp * __libc_getutid_freemem_ptr attribute_hidden;
extern struct utmp * __libc_getutline_freemem_ptr attribute_hidden;
extern printf_arginfo_size_function ** __libc_reg_printf_freemem_ptr
    attribute_hidden;
extern printf_va_arg_function ** __libc_reg_type_freemem_ptr
    attribute_hidden;
/* From nss/getXXbyYY.c  */
extern char * __libc_getgrgid_freemem_ptr attribute_hidden;
extern char * __libc_getgrnam_freemem_ptr attribute_hidden;
extern char * __libc_getpwnam_freemem_ptr attribute_hidden;
extern char * __libc_getpwuid_freemem_ptr attribute_hidden;
extern char * __libc_getspnam_freemem_ptr attribute_hidden;
extern char * __libc_getaliasbyname_freemem_ptr attribute_hidden;
extern char * __libc_gethostbyaddr_freemem_ptr attribute_hidden;
extern char * __libc_gethostbyname_freemem_ptr attribute_hidden;
extern char * __libc_gethostbyname2_freemem_ptr attribute_hidden;
extern char * __libc_getnetbyaddr_freemem_ptr attribute_hidden;
extern char * __libc_getnetbyname_freemem_ptr attribute_hidden;
extern char * __libc_getprotobynumber_freemem_ptr attribute_hidden;
extern char * __libc_getprotobyname_freemem_ptr attribute_hidden;
extern char * __libc_getrpcbyname_freemem_ptr attribute_hidden;
extern char * __libc_getrpcbynumber_freemem_ptr attribute_hidden;
extern char * __libc_getservbyname_freemem_ptr attribute_hidden;
extern char * __libc_getservbyport_freemem_ptr attribute_hidden;
/* From nss/getXXent.c */
extern char * __libc_getgrent_freemem_ptr attribute_hidden;
extern char * __libc_getpwent_freemem_ptr attribute_hidden;
extern char * __libc_getspent_freemem_ptr attribute_hidden;
extern char * __libc_getaliasent_freemem_ptr attribute_hidden;
extern char * __libc_gethostent_freemem_ptr attribute_hidden;
extern char * __libc_getnetent_freemem_ptr attribute_hidden;
extern char * __libc_getprotoent_freemem_ptr attribute_hidden;
extern char * __libc_getrpcent_freemem_ptr attribute_hidden;
extern char * __libc_getservent_freemem_ptr attribute_hidden;
/* From misc/efgcvt-template.c  */
extern char * __libc_efgcvt_freemem_ptr attribute_hidden;
extern char * __libc_qefgcvt_freemem_ptr attribute_hidden;

#endif
