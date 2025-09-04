/* Support for dynamic linking code in static libc.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
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

/* This file defines some things that for the dynamic linker are defined in
   rtld.c and dl-sysdep.c in ways appropriate to bootstrap dynamic linking.  */

#include <string.h>
/* Mark symbols hidden in static PIE for early self relocation to work.
   Note: string.h may have ifuncs which cannot be hidden on i686.  */
#if BUILD_PIE_DEFAULT
# pragma GCC visibility push(hidden)
#endif
#include <errno.h>
#include <libintl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdint.h>
#include <ldsodefs.h>
#include <dl-machine.h>
#include <libc-lock.h>
#include <dl-cache.h>
#include <dl-procinfo.h>
#include <unsecvars.h>
#include <hp-timing.h>
#include <stackinfo.h>
#include <dl-vdso.h>
#include <dl-vdso-setup.h>
#include <dl-auxv.h>
#include <dl-find_object.h>
#include <array_length.h>
#include <dl-symbol-redir-ifunc.h>

extern char *__progname;
char **_dl_argv = &__progname;	/* This is checked for some error messages.  */

/* Name of the architecture.  */
const char *_dl_platform;
size_t _dl_platformlen;

int _dl_debug_mask;
int _dl_lazy;
int _dl_dynamic_weak;

/* If nonzero print warnings about problematic situations.  */
int _dl_verbose;

/* Names of shared object for which the RUNPATHs and RPATHs should be
   ignored.  */
const char *_dl_inhibit_rpath;

/* The map for the object we will profile.  */
struct link_map *_dl_profile_map;

/* This is the address of the last stack address ever used.  */
void *__libc_stack_end;

/* Path where the binary is found.  */
const char *_dl_origin_path;

/* Nonzero if runtime lookup should not update the .got/.plt.  */
int _dl_bind_not;

/* A dummy link map for the executable, used by dlopen to access the global
   scope.  We don't export any symbols ourselves, so this can be minimal.  */
static struct link_map _dl_main_map =
  {
    .l_name = (char *) "",
    .l_real = &_dl_main_map,
    .l_ns = LM_ID_BASE,
    .l_libname = &(struct libname_list) { .name = "", .dont_free = 1 },
    .l_searchlist =
      {
	.r_list = &(struct link_map *) { &_dl_main_map },
	.r_nlist = 1,
      },
    .l_symbolic_searchlist = { .r_list = &(struct link_map *) { NULL } },
    .l_type = lt_executable,
    .l_scope_mem = { &_dl_main_map.l_searchlist },
    .l_scope_max = (sizeof (_dl_main_map.l_scope_mem)
		    / sizeof (_dl_main_map.l_scope_mem[0])),
    .l_scope = _dl_main_map.l_scope_mem,
    .l_local_scope = { &_dl_main_map.l_searchlist },
    .l_used = 1,
    .l_tls_offset = NO_TLS_OFFSET,
    .l_serial = 1,
  };

/* Namespace information.  */
struct link_namespaces _dl_ns[DL_NNS] =
  {
    [LM_ID_BASE] =
      {
	._ns_loaded = &_dl_main_map,
	._ns_nloaded = 1,
	._ns_main_searchlist = &_dl_main_map.l_searchlist,
      }
  };
size_t _dl_nns = 1;

/* Incremented whenever something may have been added to dl_loaded. */
unsigned long long _dl_load_adds = 1;

/* Fake scope of the main application.  */
struct r_scope_elem _dl_initial_searchlist =
  {
    .r_list = &(struct link_map *) { &_dl_main_map },
    .r_nlist = 1,
  };

#ifndef HAVE_INLINED_SYSCALLS
/* Nonzero during startup.  */
int _dl_starting_up = 1;
#endif

/* Random data provided by the kernel.  */
void *_dl_random;

/* Get architecture specific initializer.  */
#include <dl-procruntime.c>
#include <dl-procinfo.c>

size_t _dl_pagesize = EXEC_PAGESIZE;

size_t _dl_minsigstacksize = CONSTANT_MINSIGSTKSZ;

int _dl_inhibit_cache;

/* All known directories in sorted order.  */
struct r_search_path_elem *_dl_all_dirs;

/* All directories after startup.  */
struct r_search_path_elem *_dl_init_all_dirs;

/* The object to be initialized first.  */
struct link_map *_dl_initfirst;

/* Descriptor to write debug messages to.  */
int _dl_debug_fd = STDERR_FILENO;

ElfW(auxv_t) *_dl_auxv;
const ElfW(Phdr) *_dl_phdr;
size_t _dl_phnum;
uint64_t _dl_hwcap;
uint64_t _dl_hwcap2;
uint64_t _dl_hwcap3;
uint64_t _dl_hwcap4;

enum dso_sort_algorithm _dl_dso_sort_algo;

/* The value of the FPU control word the kernel will preset in hardware.  */
fpu_control_t _dl_fpu_control = _FPU_DEFAULT;

/* Prevailing state of the stack.  Generally this includes PF_X, indicating it's
 * executable but this isn't true for all platforms.  */
ElfW(Word) _dl_stack_flags = DEFAULT_STACK_PERMS;

#if PTHREAD_IN_LIBC
list_t _dl_stack_used;
list_t _dl_stack_user;
list_t _dl_stack_cache;
size_t _dl_stack_cache_actsize;
uintptr_t _dl_in_flight_stack;
int _dl_stack_cache_lock;
#else
/* If loading a shared object requires that we make the stack executable
   when it was not, we do it by calling this function.
   It returns an errno code or zero on success.  */
int (*_dl_make_stack_executable_hook) (void **) = _dl_make_stack_executable;
void (*_dl_init_static_tls) (struct link_map *) = &_dl_nothread_init_static_tls;
#endif
struct dl_scope_free_list *_dl_scope_free_list;

#ifdef NEED_DL_SYSINFO
/* Needed for improved syscall handling on at least x86/Linux.  NB: Don't
   initialize it here to avoid RELATIVE relocation in static PIE.  */
uintptr_t _dl_sysinfo;
#endif
#ifdef NEED_DL_SYSINFO_DSO
/* Address of the ELF headers in the vsyscall page.  */
const ElfW(Ehdr) *_dl_sysinfo_dso;

struct link_map *_dl_sysinfo_map;

# include "get-dynamic-info.h"
#endif
#include "setup-vdso.h"
/* Define the vDSO function pointers.  */
#include <dl-vdso-setup.c>

/* During the program run we must not modify the global data of
   loaded shared object simultaneously in two threads.  Therefore we
   protect `_dl_open' and `_dl_close' in dl-close.c.

   This must be a recursive lock since the initializer function of
   the loaded object might as well require a call to this function.
   At this time it is not anymore a problem to modify the tables.  */
__rtld_lock_define_initialized_recursive (, _dl_load_lock)
/* This lock is used to keep __dl_iterate_phdr from inspecting the
   list of loaded objects while an object is added to or removed from
   that list.  */
__rtld_lock_define_initialized_recursive (, _dl_load_write_lock)
  /* This lock protects global and module specific TLS related data.
     E.g. it is held in dlopen and dlclose when GL(dl_tls_generation),
     GL(dl_tls_max_dtv_idx) or GL(dl_tls_dtv_slotinfo_list) are
     accessed and when TLS related relocations are processed for a
     module.  It was introduced to keep pthread_create accessing TLS
     state that is being set up.  */
__rtld_lock_define_initialized_recursive (, _dl_load_tls_lock)


#ifdef HAVE_AUX_VECTOR
#include <dl-parse_auxv.h>

int _dl_clktck;

void
_dl_aux_init (ElfW(auxv_t) *av)
{
#ifdef NEED_DL_SYSINFO
  /* NB: Avoid RELATIVE relocation in static PIE.  */
  GL(dl_sysinfo) = DL_SYSINFO_DEFAULT;
#endif

  _dl_auxv = av;
  dl_parse_auxv_t auxv_values;
  /* Use an explicit initialization loop here because memset may not
     be available yet.  */
  for (int i = 0; i < array_length (auxv_values); ++i)
    auxv_values[i] = 0;
  _dl_parse_auxv (av, auxv_values);

  _dl_phdr = (void*) auxv_values[AT_PHDR];
  _dl_phnum = auxv_values[AT_PHNUM];

  if (_dl_phdr == NULL)
    {
      /* Starting from binutils-2.23, the linker will define the
         magic symbol __ehdr_start to point to our own ELF header
         if it is visible in a segment that also includes the phdrs.
         So we can set up _dl_phdr and _dl_phnum even without any
         information from auxv.  */

      extern const ElfW(Ehdr) __ehdr_start attribute_hidden;
      assert (__ehdr_start.e_phentsize == sizeof *GL(dl_phdr));
      _dl_phdr = (const void *) &__ehdr_start + __ehdr_start.e_phoff;
      _dl_phnum = __ehdr_start.e_phnum;
    }

  assert (_dl_phdr != NULL);
}
#endif


void
_dl_non_dynamic_init (void)
{
  _dl_main_map.l_origin = _dl_get_origin ();
  _dl_main_map.l_phdr = GL(dl_phdr);
  _dl_main_map.l_phnum = GL(dl_phnum);

  /* Set up the data structures for the system-supplied DSO early,
     so they can influence _dl_init_paths.  */
  setup_vdso (NULL, NULL);

  /* With vDSO setup we can initialize the function pointers.  */
  setup_vdso_pointers ();

  if (__libc_enable_secure)
    {
      static const char unsecure_envvars[] =
	UNSECURE_ENVVARS
	;
      const char *cp = unsecure_envvars;

      while (cp < unsecure_envvars + sizeof (unsecure_envvars))
	{
	  __unsetenv (cp);
	  cp = strchr (cp, '\0') + 1;
	}
    }

  _dl_verbose = *(getenv ("LD_WARN") ?: "") == '\0' ? 0 : 1;

  /* Initialize the data structures for the search paths for shared
     objects.  */
  _dl_init_paths (getenv ("LD_LIBRARY_PATH"), "LD_LIBRARY_PATH",
		  /* No glibc-hwcaps selection support in statically
		     linked binaries.  */
		  NULL, NULL);

  /* Remember the last search directory added at startup.  */
  _dl_init_all_dirs = GL(dl_all_dirs);

  _dl_lazy = *(getenv ("LD_BIND_NOW") ?: "") == '\0';

  _dl_bind_not = *(getenv ("LD_BIND_NOT") ?: "") != '\0';

  _dl_dynamic_weak = *(getenv ("LD_DYNAMIC_WEAK") ?: "") == '\0';

#ifdef DL_PLATFORM_INIT
  DL_PLATFORM_INIT;
#endif

  /* Now determine the length of the platform string.  */
  if (_dl_platform != NULL)
    _dl_platformlen = strlen (_dl_platform);

  for (const ElfW(Phdr) *ph = _dl_phdr; ph < &_dl_phdr[_dl_phnum]; ++ph)
    switch (ph->p_type)
      {
      /* Check if the stack is nonexecutable.  */
      case PT_GNU_STACK:
	_dl_stack_flags = ph->p_flags;
	break;

      case PT_GNU_RELRO:
	_dl_main_map.l_relro_addr = ph->p_vaddr;
	_dl_main_map.l_relro_size = ph->p_memsz;
	break;
      }

  call_function_static_weak (_dl_find_object_init);

  /* Setup relro on the binary itself.  */
  if (_dl_main_map.l_relro_size != 0)
    _dl_protect_relro (&_dl_main_map);
}

#ifdef DL_SYSINFO_IMPLEMENTATION
DL_SYSINFO_IMPLEMENTATION
#endif

/* Since relocation to hidden _dl_main_map causes relocation overflow on
   aarch64, a function is used to get the address of _dl_main_map.  */

struct link_map *
_dl_get_dl_main_map (void)
{
  return &_dl_main_map;
}

/* This is used by _dl_runtime_profile, not used on static code.  */
void
DL_ARCH_FIXUP_ATTRIBUTE
_dl_audit_pltexit (struct link_map *l, ElfW(Word) reloc_arg,
		   const void *inregs, void *outregs)
{
}
