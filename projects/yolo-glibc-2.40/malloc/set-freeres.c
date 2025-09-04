/* Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <atomic.h>
#include <stdlib.h>
#include <libc-internal.h>
#include <unwind-link.h>
#include <ldsodefs.h>
#include <set-freeres.h>
#include <set-freeres-system.h>

#ifndef SHARED
# pragma weak __nss_module_freeres
# pragma weak __nss_action_freeres
# pragma weak __nss_database_freeres
# pragma weak __dl_libc_freemem
# pragma weak __hdestroy
# pragma weak __gconv_cache_freemem
# pragma weak __gconv_conf_freemem
# pragma weak __gconv_db_freemem
# pragma weak __gconv_dl_freemem
# pragma weak __intl_freemem
# pragma weak __libio_freemem
# pragma weak __libc_fstab_freemem
#ifdef USE_NSCD
# pragma weak __nscd_gr_map_freemem
# pragma weak __nscd_hst_map_freemem
# pragma weak __nscd_pw_map_freemem
# pragma weak __nscd_serv_map_freemem
# pragma weak __nscd_group_map_freemem
#endif
# pragma weak __libc_regcomp_freemem
# pragma weak __libc_atfork_freemem
# pragma weak __res_thread_freeres
# pragma weak __libc_resolv_conf_freemem
# pragma weak __libc_printf_freemem
# pragma weak __libc_fmtmsg_freemem
# pragma weak __libc_setenv_freemem
# if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_31)
#  pragma weak __rpc_freemem
#  pragma weak __rpc_thread_destroy
# endif
# pragma weak __libc_getaddrinfo_freemem
# pragma weak __libc_tzset_freemem
# pragma weak __libc_localealias_freemem
# pragma weak __gai_freemem
# pragma weak __aio_freemem
# pragma weak __libpthread_freeres
# pragma weak __libc_dlerror_result_free
# pragma weak __check_pf_freemem
# pragma weak __libc_fgetgrent_freemem_ptr
# pragma weak __libc_fgetsgent_freeres_ptr
# pragma weak __libc_getnetgrent_freemem_ptr
# pragma weak __libc_rcmd_freemem_ptr
# pragma weak __libc_rexec_freemem_ptr
# pragma weak __libc_mntent_freemem_ptr
# pragma weak __libc_fgetpwent_freemem_ptr
# pragma weak __libc_getspent_freemem_ptr
# pragma weak __libc_resolv_res_hconf_freemem_ptr
# pragma weak __libc_fgetspent_freemem_ptr
# pragma weak __libc_tzfile_freemem_ptr
# pragma weak __libc_getnameinfo_freemem_ptr
# pragma weak __libc_getutent_freemem_ptr
# pragma weak __libc_getutid_freemem_ptr
# pragma weak __libc_getutline_freemem_ptr
# pragma weak __libc_reg_printf_freemem_ptr
# pragma weak __libc_reg_type_freemem_ptr
# pragma weak __libc_getgrgid_freemem_ptr
# pragma weak __libc_getgrnam_freemem_ptr
# pragma weak __libc_getpwnam_freemem_ptr
# pragma weak __libc_getpwuid_freemem_ptr
# pragma weak __libc_getspnam_freemem_ptr
# pragma weak __libc_getaliasbyname_freemem_ptr
# pragma weak __libc_gethostbyaddr_freemem_ptr
# pragma weak __libc_gethostbyname_freemem_ptr
# pragma weak __libc_gethostbyname2_freemem_ptr
# pragma weak __libc_getnetbyaddr_freemem_ptr
# pragma weak __libc_getnetbyname_freemem_ptr
# pragma weak __libc_getprotobynumber_freemem_ptr
# pragma weak __libc_getprotobyname_freemem_ptr
# pragma weak __libc_getrpcbyname_freemem_ptr
# pragma weak __libc_getrpcbynumber_freemem_ptr
# pragma weak __libc_getservbyname_freemem_ptr
# pragma weak __libc_getservbyport_freemem_ptr
# pragma weak __libc_getgrent_freemem_ptr
# pragma weak __libc_getpwent_freemem_ptr
# pragma weak __libc_getaliasent_freemem_ptr
# pragma weak __libc_gethostent_freemem_ptr
# pragma weak __libc_getnetent_freemem_ptr
# pragma weak __libc_getprotoent_freemem_ptr
# pragma weak __libc_getrpcent_freemem_ptr
# pragma weak __libc_getservent_freemem_ptr
# pragma weak __libc_efgcvt_freemem_ptr
# pragma weak __libc_qefgcvt_freemem_ptr
# pragma weak __libc_qefgcvt_freemem_ptr
# pragma weak __ttyname_freemem_ptr
#endif

#ifdef SHARED
# define call_free_static_weak(__ptr)				\
   free (__ptr)
#else
# define call_free_static_weak(__ptr)				\
  if (&__ptr != NULL)						\
    free (__ptr);
#endif

void
__libc_freeres (void)
{
  /* This function might be called from different places.  So better
     protect for multiple executions since these are fatal.  */
  static long int already_called;

  if (!atomic_compare_and_exchange_bool_acq (&already_called, 1, 0))
    {
      call_function_static_weak (__nss_module_freeres);
      call_function_static_weak (__nss_action_freeres);
      call_function_static_weak (__nss_database_freeres);

      _IO_cleanup ();

      /* We run the resource freeing after IO cleanup.  */
      call_function_static_weak (__dl_libc_freemem);
      call_function_static_weak (__hdestroy);
      call_function_static_weak (__gconv_cache_freemem);
      call_function_static_weak (__gconv_conf_freemem);
      call_function_static_weak (__gconv_db_freemem);
      call_function_static_weak (__gconv_dl_freemem);
      call_function_static_weak (__intl_freemem);
      call_function_static_weak (__libio_freemem);
      call_function_static_weak (__libc_fstab_freemem);

#ifdef USE_NSCD
      call_function_static_weak (__nscd_gr_map_freemem);
      call_function_static_weak (__nscd_hst_map_freemem);
      call_function_static_weak (__nscd_pw_map_freemem);
      call_function_static_weak (__nscd_serv_map_freemem);
      call_function_static_weak (__nscd_group_map_freemem);
#endif

      call_function_static_weak (__libc_regcomp_freemem);
      call_function_static_weak (__libc_atfork_freemem);
      /* __res_thread_freeres deallocates the per-thread resolv_context);
	 which in turn drop the reference count of the current global object.
	 So it need to be before __libc_resolv_conf_freemem.  */
      call_function_static_weak (__res_thread_freeres);
      call_function_static_weak (__libc_resolv_conf_freemem);
      call_function_static_weak (__libc_printf_freemem);
      call_function_static_weak (__libc_fmtmsg_freemem);
      call_function_static_weak (__libc_setenv_freemem);
#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_31)
      call_function_static_weak (__rpc_freemem);
      call_function_static_weak (__rpc_thread_destroy);
#endif
      call_function_static_weak (__libc_getaddrinfo_freemem);
      call_function_static_weak (__libc_tzset_freemem);
      call_function_static_weak (__libc_localealias_freemem);

#if PTHREAD_IN_LIBC
      call_function_static_weak (__gai_freemem);

      call_function_static_weak (__aio_freemem);
#endif

      call_function_static_weak (__libpthread_freeres);

#ifdef SHARED
      __libc_unwind_link_freeres ();
#endif

      call_function_static_weak (__libc_dlerror_result_free);

#ifdef SHARED
      GLRO (dl_libc_freeres) ();
#endif

      call_free_static_weak (__libc_fgetgrent_freemem_ptr);
      call_free_static_weak (__libc_fgetsgent_freeres_ptr);
      call_free_static_weak (__libc_getnetgrent_freemem_ptr);
      call_free_static_weak (__libc_rcmd_freemem_ptr);
      call_free_static_weak (__libc_rexec_freemem_ptr);
      call_free_static_weak (__libc_mntent_freemem_ptr);
      call_free_static_weak (__libc_fgetpwent_freemem_ptr);
      call_free_static_weak (__libc_resolv_res_hconf_freemem_ptr);
      call_free_static_weak (__libc_fgetspent_freemem_ptr);
      call_free_static_weak (__libc_tzfile_freemem_ptr);
      call_free_static_weak (__libc_getnameinfo_freemem_ptr);
      call_free_static_weak (__libc_getutent_freemem_ptr);
      call_free_static_weak (__libc_getutid_freemem_ptr);
      call_free_static_weak (__libc_getutline_freemem_ptr);
      call_free_static_weak (__libc_reg_printf_freemem_ptr);
      call_free_static_weak (__libc_reg_type_freemem_ptr);

      call_free_static_weak (__libc_getgrgid_freemem_ptr);
      call_free_static_weak (__libc_getgrnam_freemem_ptr);
      call_free_static_weak (__libc_getpwnam_freemem_ptr);
      call_free_static_weak (__libc_getpwuid_freemem_ptr);
      call_free_static_weak (__libc_getspnam_freemem_ptr);
      call_free_static_weak (__libc_getaliasbyname_freemem_ptr);
      call_free_static_weak (__libc_gethostbyaddr_freemem_ptr);
      call_free_static_weak (__libc_gethostbyname_freemem_ptr);
      call_free_static_weak (__libc_gethostbyname2_freemem_ptr);
      call_free_static_weak (__libc_getnetbyaddr_freemem_ptr);
      call_free_static_weak (__libc_getnetbyname_freemem_ptr);
      call_free_static_weak (__libc_getprotobynumber_freemem_ptr);
      call_free_static_weak (__libc_getprotobyname_freemem_ptr);
      call_free_static_weak (__libc_getrpcbyname_freemem_ptr);
      call_free_static_weak (__libc_getrpcbynumber_freemem_ptr);
      call_free_static_weak (__libc_getservbyname_freemem_ptr);
      call_free_static_weak (__libc_getservbyport_freemem_ptr);

      call_free_static_weak (__libc_getgrent_freemem_ptr);
      call_free_static_weak (__libc_getpwent_freemem_ptr);
      call_free_static_weak (__libc_getspent_freemem_ptr);
      call_free_static_weak (__libc_getaliasent_freemem_ptr);
      call_free_static_weak (__libc_gethostent_freemem_ptr);
      call_free_static_weak (__libc_getnetent_freemem_ptr);
      call_free_static_weak (__libc_getprotoent_freemem_ptr);
      call_free_static_weak (__libc_getrpcent_freemem_ptr);
      call_free_static_weak (__libc_getservent_freemem_ptr);

      call_free_static_weak (__libc_efgcvt_freemem_ptr);
      call_free_static_weak (__libc_qefgcvt_freemem_ptr);

      call_freeres_system_funcs;
    }
}
libc_hidden_def (__libc_freeres)
