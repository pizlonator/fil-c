/* Internal definitions for libdwfl.
   Copyright (C) 2005-2015, 2018 Red Hat, Inc.
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

#ifndef _LIBDWFLP_H
#define _LIBDWFLP_H	1

#include <libdwfl.h>
#include <libebl.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "libdwP.h"	/* We need its INTDECLs.  */
#include "libdwelfP.h"

typedef struct Dwfl_Process Dwfl_Process;

#define DWFL_ERRORS							      \
  DWFL_ERROR (NOERROR, N_("no error"))					      \
  DWFL_ERROR (UNKNOWN_ERROR, N_("unknown error"))			      \
  DWFL_ERROR (NOMEM, N_("out of memory"))				      \
  DWFL_ERROR (ERRNO, N_("See errno"))					      \
  DWFL_ERROR (LIBELF, N_("See elf_errno"))				      \
  DWFL_ERROR (LIBDW, N_("See dwarf_errno"))				      \
  DWFL_ERROR (LIBEBL, N_("See ebl_errno (XXX missing)"))		      \
  DWFL_ERROR (ZLIB, N_("gzip decompression failed"))			      \
  DWFL_ERROR (BZLIB, N_("bzip2 decompression failed"))			      \
  DWFL_ERROR (LZMA, N_("LZMA decompression failed"))			      \
  DWFL_ERROR (ZSTD, N_("zstd decompression failed"))			      \
  DWFL_ERROR (UNKNOWN_MACHINE, N_("no support library found for machine"))    \
  DWFL_ERROR (NOREL, N_("Callbacks missing for ET_REL file"))		      \
  DWFL_ERROR (BADRELTYPE, N_("Unsupported relocation type"))		      \
  DWFL_ERROR (BADRELOFF, N_("r_offset is bogus"))			      \
  DWFL_ERROR (BADSTROFF, N_("offset out of range"))			      \
  DWFL_ERROR (RELUNDEF, N_("relocation refers to undefined symbol"))	      \
  DWFL_ERROR (CB, N_("Callback returned failure"))			      \
  DWFL_ERROR (NO_DWARF, N_("No DWARF information found"))		      \
  DWFL_ERROR (NO_SYMTAB, N_("No symbol table found"))			      \
  DWFL_ERROR (NO_PHDR, N_("No ELF program headers"))			      \
  DWFL_ERROR (OVERLAP, N_("address range overlaps an existing module"))	      \
  DWFL_ERROR (ADDR_OUTOFRANGE, N_("address out of range"))		      \
  DWFL_ERROR (NO_MATCH, N_("no matching address range"))		      \
  DWFL_ERROR (TRUNCATED, N_("image truncated"))				      \
  DWFL_ERROR (ALREADY_ELF, N_("ELF file opened"))			      \
  DWFL_ERROR (BADELF, N_("not a valid ELF file"))			      \
  DWFL_ERROR (WEIRD_TYPE, N_("cannot handle DWARF type description"))	      \
  DWFL_ERROR (WRONG_ID_ELF, N_("ELF file does not match build ID"))	      \
  DWFL_ERROR (BAD_PRELINK, N_("corrupt .gnu.prelink_undo section data"))      \
  DWFL_ERROR (LIBEBL_BAD, N_("Internal error due to ebl"))		      \
  DWFL_ERROR (CORE_MISSING, N_("Missing data in core file"))		      \
  DWFL_ERROR (INVALID_REGISTER, N_("Invalid register"))			      \
  DWFL_ERROR (REGISTER_VAL_UNKNOWN, N_("Unknown register value"))			      \
  DWFL_ERROR (PROCESS_MEMORY_READ, N_("Error reading process memory"))	      \
  DWFL_ERROR (PROCESS_NO_ARCH, N_("Couldn't find architecture of any ELF"))   \
  DWFL_ERROR (PARSE_PROC, N_("Error parsing /proc filesystem"))		      \
  DWFL_ERROR (INVALID_DWARF, N_("Invalid DWARF"))			      \
  DWFL_ERROR (UNSUPPORTED_DWARF, N_("Unsupported DWARF"))		      \
  DWFL_ERROR (NEXT_THREAD_FAIL, N_("Unable to find more threads"))	      \
  DWFL_ERROR (ATTACH_STATE_CONFLICT, N_("Dwfl already has attached state"))   \
  DWFL_ERROR (NO_ATTACH_STATE, N_("Dwfl has no attached state"))	      \
  DWFL_ERROR (NO_UNWIND, N_("Unwinding not supported for this architecture")) \
  DWFL_ERROR (INVALID_ARGUMENT, N_("Invalid argument"))			      \
  DWFL_ERROR (NO_CORE_FILE, N_("Not an ET_CORE ELF file"))

#define DWFL_ERROR(name, text) DWFL_E_##name,
typedef enum { DWFL_ERRORS DWFL_E_NUM } Dwfl_Error;
#undef	DWFL_ERROR

#define OTHER_ERROR(name)	((unsigned int) DWFL_E_##name << 16)
#define DWFL_E(name, errno)	(OTHER_ERROR (name) | (errno))

extern int __libdwfl_canon_error (Dwfl_Error) internal_function;
extern void __libdwfl_seterrno (Dwfl_Error) internal_function;

/* Resources we might keep for the user about the core file that the
   Dwfl might have been created from.  Can currently only be set
   through std-argp.  */
struct Dwfl_User_Core
{
  char *executable_for_core;	/* --executable if --core was specified.  */
  Elf *core;                    /* non-NULL if we need to free it.  */
  int fd;                       /* close if >= 0.  */
};

struct Dwfl
{
  const Dwfl_Callbacks *callbacks;
#ifdef ENABLE_LIBDEBUGINFOD
  debuginfod_client *debuginfod;
#endif
  Dwfl_Module *modulelist;    /* List in order used by full traversals.  */

  Dwfl_Process *process;
  Dwfl_Error attacherr;      /* Previous error attaching process.  */

  GElf_Addr offline_next_address;

  GElf_Addr segment_align;	/* Smallest granularity of segments.  */

  /* Binary search table in three parallel malloc'd arrays.  */
  size_t lookup_elts;		/* Elements in use.  */
  size_t lookup_alloc;		/* Elements allococated.  */
  GElf_Addr *lookup_addr;	/* Start address of segment.  */
  Dwfl_Module **lookup_module;	/* Module associated with segment, or null.  */
  int *lookup_segndx;		/* User segment index, or -1.  */
  int next_segndx;

  struct Dwfl_User_Core *user_core;
};

#define OFFLINE_REDZONE		0x10000

struct dwfl_file
{
  char *name;
  int fd;
  bool valid;			/* The build ID note has been matched.  */
  bool relocated;		/* Partial relocation of all sections done.  */

  Elf *elf;

  /* This is the lowest p_vaddr in this ELF file, aligned to p_align.
     For a file without phdrs, this is zero.  */
  GElf_Addr vaddr;

  /* This is an address chosen for synchronization between the main file
     and the debug file.  See dwfl_module_getdwarf.c for how it's chosen.  */
  GElf_Addr address_sync;
};

struct Dwfl_Module
{
  Dwfl *dwfl;
  struct Dwfl_Module *next;	/* Link on Dwfl.modulelist.  */

  void *userdata;

  char *name;			/* Iterator name for this module.  */
  GElf_Addr low_addr, high_addr;

  struct dwfl_file main, debug, aux_sym;
  GElf_Addr main_bias;
  Ebl *ebl;
  GElf_Half e_type;		/* GElf_Ehdr.e_type cache.  */
  Dwfl_Error elferr;		/* Previous failure to open main file.  */

  struct dwfl_relocation *reloc_info; /* Relocatable sections.  */

  struct dwfl_file *symfile;	/* Either main or debug.  */
  Elf_Data *symdata;		/* Data in the ELF symbol table section.  */
  Elf_Data *aux_symdata;	/* Data in the auxiliary ELF symbol table.  */
  size_t syments;		/* sh_size / sh_entsize of that section.  */
  size_t aux_syments;		/* sh_size / sh_entsize of aux_sym section.  */
  int first_global;		/* Index of first global symbol of table.  */
  int aux_first_global;		/* Index of first global of aux_sym table.  */
  Elf_Data *symstrdata;		/* Data for its string table.  */
  Elf_Data *aux_symstrdata;	/* Data for aux_sym string table.  */
  Elf_Data *symxndxdata;	/* Data in the extended section index table. */
  Elf_Data *aux_symxndxdata;	/* Data in the extended auxiliary table. */

  char *elfpath;		/* The path where we found the main Elf.  */

  Dwarf *dw;			/* libdw handle for its debugging info.  */
  Dwarf *alt;			/* Dwarf used for dwarf_setalt, or NULL.  */
  int alt_fd; 			/* descriptor, only valid when alt != NULL.  */
  Elf *alt_elf; 		/* Elf for alt Dwarf.  */

  Dwfl_Error symerr;		/* Previous failure to load symbols.  */
  Dwfl_Error dwerr;		/* Previous failure to load DWARF.  */

  /* Known CU's in this module.  */
  struct dwfl_cu *first_cu, **cu;

  void *lazy_cu_root;		/* Table indexed by Dwarf_Off of CU.  */

  struct dwfl_arange *aranges;	/* Mapping of addresses in module to CUs.  */

  void *build_id_bits;		/* malloc'd copy of build ID bits.  */
  GElf_Addr build_id_vaddr;	/* Address where they reside, 0 if unknown.  */
  int build_id_len;		/* -1 for prior failure, 0 if unset.  */

  unsigned int ncu;
  unsigned int lazycu;		/* Possible users, deleted when none left.  */
  unsigned int naranges;

  Dwarf_CFI *dwarf_cfi;		/* Cached DWARF CFI for this module.  */
  Dwarf_CFI *eh_cfi;		/* Cached EH CFI for this module.  */

  int segment;			/* Index of first segment table entry.  */
  bool gc;			/* Mark/sweep flag.  */
  bool is_executable;		/* Use Dwfl::executable_for_core?  */
};

/* This holds information common for all the threads/tasks/TIDs of one process
   for backtraces.  */

struct Dwfl_Process
{
  struct Dwfl *dwfl;
  pid_t pid;
  const Dwfl_Thread_Callbacks *callbacks;
  void *callbacks_arg;
  struct ebl *ebl;
  bool ebl_close:1;
};

/* See its typedef in libdwfl.h.  */

struct Dwfl_Thread
{
  Dwfl_Process *process;
  pid_t tid;
  /* Bottom (innermost) frame while we're initializing, NULL afterwards.  */
  Dwfl_Frame *unwound;
  void *callbacks_arg;
};

/* See its typedef in libdwfl.h.  */

struct Dwfl_Frame
{
  Dwfl_Thread *thread;
  /* Previous (outer) frame.  */
  Dwfl_Frame *unwound;
  bool signal_frame : 1;
  bool initial_frame : 1;
  enum
  {
    /* This structure is still being initialized or there was an error
       initializing it.  */
    DWFL_FRAME_STATE_ERROR,
    /* PC field is valid.  */
    DWFL_FRAME_STATE_PC_SET,
    /* PC field is undefined, this means the next (inner) frame was the
       outermost frame.  */
    DWFL_FRAME_STATE_PC_UNDEFINED
  } pc_state;
  /* Either initialized from appropriate REGS element or on some archs
     initialized separately as the return address has no DWARF register.  */
  Dwarf_Addr pc;
  /* (1 << X) bitmask where 0 <= X < ebl_frame_nregs.  */
  uint64_t regs_set[3];
  /* REGS array size is ebl_frame_nregs.
     REGS_SET tells which of the REGS are valid.  */
  Dwarf_Addr regs[];
};

/* Fetch value from Dwfl_Frame->regs indexed by DWARF REGNO.  The
   function returns 0 on success, -1 on error (invalid DWARF register
   number), 1 if the value of the register in the frame is unknown.
   Even on error, no error code is set.  */
int __libdwfl_frame_reg_get (Dwfl_Frame *state, unsigned regno,
			      Dwarf_Addr *val)
  internal_function;

/* Store value to Dwfl_Frame->regs indexed by DWARF REGNO.
   No error code is set if the function returns FALSE.  */
bool __libdwfl_frame_reg_set (Dwfl_Frame *state, unsigned regno,
			      Dwarf_Addr val)
  internal_function;

/* Information cached about each CU in Dwfl_Module.dw.  */
struct dwfl_cu
{
  /* This caches libdw information about the CU.  It's also the
     address passed back to users, so we take advantage of the
     fact that it's placed first to cast back.  */
  Dwarf_Die die;

  Dwfl_Module *mod;		/* Pointer back to containing module.  */

  struct dwfl_cu *next;		/* CU immediately following in the file.  */

  struct Dwfl_Lines *lines;
};

struct Dwfl_Lines
{
  struct dwfl_cu *cu;

  /* This is what the opaque Dwfl_Line * pointers we pass to users are.
     We need to recover pointers to our struct dwfl_cu and a record in
     libdw's Dwarf_Line table.  To minimize the memory used in addition
     to libdw's Dwarf_Lines buffer, we just point to our own index in
     this table, and have one pointer back to the CU.  The indices here
     match those in libdw's Dwarf_CU.lines->info table.  */
  struct Dwfl_Line
  {
    unsigned int idx;		/* My index in the dwfl_cu.lines table.  */
  } idx[0];
};

static inline struct dwfl_cu *
dwfl_linecu_inline (const Dwfl_Line *line)
{
  const struct Dwfl_Lines *lines = ((const void *) line
				    - offsetof (struct Dwfl_Lines,
						idx[line->idx]));
  return lines->cu;
}
#define dwfl_linecu dwfl_linecu_inline

static inline GElf_Addr
dwfl_adjusted_address (Dwfl_Module *mod, GElf_Addr addr)
{
  return addr + mod->main_bias;
}

static inline GElf_Addr
dwfl_deadjust_address (Dwfl_Module *mod, GElf_Addr addr)
{
  return addr - mod->main_bias;
}

static inline Dwarf_Addr
dwfl_adjusted_dwarf_addr (Dwfl_Module *mod, Dwarf_Addr addr)
{
  return dwfl_adjusted_address (mod, (addr
				      - mod->debug.address_sync
				      + mod->main.address_sync));
}

static inline Dwarf_Addr
dwfl_deadjust_dwarf_addr (Dwfl_Module *mod, Dwarf_Addr addr)
{
  return (dwfl_deadjust_address (mod, addr)
	  - mod->main.address_sync
	  + mod->debug.address_sync);
}

static inline Dwarf_Addr
dwfl_adjusted_aux_sym_addr (Dwfl_Module *mod, Dwarf_Addr addr)
{
  return dwfl_adjusted_address (mod, (addr
				      - mod->aux_sym.address_sync
				      + mod->main.address_sync));
}

static inline Dwarf_Addr
dwfl_deadjust_aux_sym_addr (Dwfl_Module *mod, Dwarf_Addr addr)
{
  return (dwfl_deadjust_address (mod, addr)
	  - mod->main.address_sync
	  + mod->aux_sym.address_sync);
}

static inline GElf_Addr
dwfl_adjusted_st_value (Dwfl_Module *mod, Elf *symelf, GElf_Addr addr)
{
  if (symelf == mod->main.elf)
    return dwfl_adjusted_address (mod, addr);
  if (symelf == mod->debug.elf)
    return dwfl_adjusted_dwarf_addr (mod, addr);
  return dwfl_adjusted_aux_sym_addr (mod, addr);
}

static inline GElf_Addr
dwfl_deadjust_st_value (Dwfl_Module *mod, Elf *symelf, GElf_Addr addr)
{
  if (symelf == mod->main.elf)
    return dwfl_deadjust_address (mod, addr);
  if (symelf == mod->debug.elf)
    return dwfl_deadjust_dwarf_addr (mod, addr);
  return dwfl_deadjust_aux_sym_addr (mod, addr);
}

/* This describes a contiguous address range that lies in a single CU.
   We condense runs of Dwarf_Arange entries for the same CU into this.  */
struct dwfl_arange
{
  struct dwfl_cu *cu;
  size_t arange;		/* Index in Dwarf_Aranges.  */
};

#define __LIBDWFL_REMOTE_MEM_CACHE_SIZE 4096
/* Structure for caching remote memory reads as used by __libdwfl_pid_arg.  */
struct __libdwfl_remote_mem_cache
{
  Dwarf_Addr addr; /* Remote address.  */
  Dwarf_Off len;   /* Zero if cleared, otherwise likely 4K. */
  unsigned char buf[__LIBDWFL_REMOTE_MEM_CACHE_SIZE]; /* The actual cache.  */
};

/* Structure used for keeping track of ptrace attaching a thread.
   Shared by linux-pid-attach and linux-proc-maps.  If it has been setup
   then get the instance through __libdwfl_get_pid_arg.  */
struct __libdwfl_pid_arg
{
  /* /proc/PID/task/.  */
  DIR *dir;
  /* Elf for /proc/PID/exe.  Set to NULL if it couldn't be opened.  */
  Elf *elf;
  /* Remote memory cache, NULL if there is no memory cached.
     Should be cleared on detachment (because that makes the thread
     runnable and the cache invalid).  */
  struct __libdwfl_remote_mem_cache *mem_cache;
  /* fd for /proc/PID/exe.  Set to -1 if it couldn't be opened.  */
  int elf_fd;
  /* It is 0 if not used.  */
  pid_t tid_attached;
  /* Valid only if TID_ATTACHED is not zero.  */
  bool tid_was_stopped;
  /* True if threads are ptrace stopped by caller.  */
  bool assume_ptrace_stopped;
};

/* If DWfl is not NULL and a Dwfl_Process has been setup that has
   Dwfl_Thread_Callbacks set to pid_thread_callbacks, then return the
   callbacks_arg, which will be a struct __libdwfl_pid_arg.  Otherwise
   returns NULL.  */
extern struct __libdwfl_pid_arg *__libdwfl_get_pid_arg (Dwfl *dwfl)
  internal_function;

/* Makes sure the given tid is attached. On success returns true and
   sets tid_was_stopped.  */
extern bool __libdwfl_ptrace_attach (pid_t tid, bool *tid_was_stoppedp)
  internal_function;

/* Detaches a tid that was attached through
   __libdwfl_ptrace_attach. Must be given the tid_was_stopped as set
   by __libdwfl_ptrace_attach.  */
extern void __libdwfl_ptrace_detach (pid_t tid, bool tid_was_stopped)
  internal_function;


/* Internal wrapper for old dwfl_module_getsym and new dwfl_module_getsym_info.
   adjust_st_value set to true returns adjusted SYM st_value, set to false
   it will not adjust SYM at all, but does match against resolved *ADDR. */
extern const char *__libdwfl_getsym (Dwfl_Module *mod, int ndx, GElf_Sym *sym,
				     GElf_Addr *addr, GElf_Word *shndxp,
				     Elf **elfp, Dwarf_Addr *biasp,
				     bool *resolved, bool adjust_st_value)
  internal_function;

extern void __libdwfl_module_free (Dwfl_Module *mod) internal_function;

/* Find the main ELF file, update MOD->elferr and/or MOD->main.elf.  */
extern void __libdwfl_getelf (Dwfl_Module *mod) internal_function;

/* Process relocations in debugging sections in an ET_REL file.
   FILE must be opened with ELF_C_READ_MMAP_PRIVATE or ELF_C_READ,
   to make it possible to relocate the data in place (or ELF_C_RDWR or
   ELF_C_RDWR_MMAP if you intend to modify the Elf file on disk).  After
   this, dwarf_begin_elf on FILE will read the relocated data.

   When DEBUG is false, apply partial relocation to all sections.  */
extern Dwfl_Error __libdwfl_relocate (Dwfl_Module *mod, Elf *file, bool debug)
  internal_function;

/* Find the section index in mod->main.elf that contains the given
   *ADDR.  Adjusts *ADDR to be section relative on success, returns
   SHN_UNDEF on failure.  */
extern size_t __libdwfl_find_section_ndx (Dwfl_Module *mod, Dwarf_Addr *addr)
  internal_function;

/* Process (simple) relocations in arbitrary section TSCN of an ET_REL file.
   RELOCSCN is SHT_REL or SHT_RELA and TSCN is its sh_info target section.  */
extern Dwfl_Error __libdwfl_relocate_section (Dwfl_Module *mod, Elf *relocated,
					      Elf_Scn *relocscn, Elf_Scn *tscn,
					      bool partial)
  internal_function;

/* Adjust *VALUE from section-relative to absolute.
   MOD->dwfl->callbacks->section_address is called to determine the actual
   address of a loaded section.  */
extern Dwfl_Error __libdwfl_relocate_value (Dwfl_Module *mod, Elf *elf,
					    size_t *shstrndx_cache,
					    Elf32_Word shndx,
					    GElf_Addr *value)
     internal_function;

/* Ensure that MOD->ebl is set up.  */
extern Dwfl_Error __libdwfl_module_getebl (Dwfl_Module *mod) internal_function;

/* Install a new Dwarf_CFI in *SLOT (MOD->eh_cfi or MOD->dwarf_cfi).  */
extern Dwarf_CFI *__libdwfl_set_cfi (Dwfl_Module *mod, Dwarf_CFI **slot,
				     Dwarf_CFI *cfi)
  internal_function;

/* Iterate through all the CU's in the module.  Start by passing a null
   LASTCU, and then pass the last *CU returned.  Success return with null
   *CU no more CUs.  */
extern Dwfl_Error __libdwfl_nextcu (Dwfl_Module *mod, struct dwfl_cu *lastcu,
				    struct dwfl_cu **cu) internal_function;

/* Find the CU by address.  */
extern Dwfl_Error __libdwfl_addrcu (Dwfl_Module *mod, Dwarf_Addr addr,
				    struct dwfl_cu **cu) internal_function;

/* Ensure that CU->lines (and CU->cu->lines) is set up.  */
extern Dwfl_Error __libdwfl_cu_getsrclines (struct dwfl_cu *cu)
  internal_function;

/* Look in ELF for an NT_GNU_BUILD_ID note.  Store it to BUILD_ID_BITS,
   its vaddr in ELF to BUILD_ID_VADDR (it is unrelocated, even if MOD is not
   NULL) and store length to BUILD_ID_LEN.  Returns -1 for errors, 1 if it was
   stored and 0 if no note is found.  MOD may be NULL, MOD must be non-NULL
   only if ELF is ET_REL.  */
extern int __libdwfl_find_elf_build_id (Dwfl_Module *mod, Elf *elf,
					const void **build_id_bits,
					GElf_Addr *build_id_elfaddr,
					int *build_id_len)
  internal_function;

/* Look in ELF for an NT_GNU_BUILD_ID note.  If SET is true, store it
   in MOD and return its length.  If SET is false, instead compare it
   to that stored in MOD and return 2 if they match, 1 if they do not.
   Returns -1 for errors, 0 if no note is found.  */
extern int __libdwfl_find_build_id (Dwfl_Module *mod, bool set, Elf *elf)
  internal_function;

/* Open a main or debuginfo file by its build ID, returns the fd.  */
extern int __libdwfl_open_mod_by_build_id (Dwfl_Module *mod, bool debug,
					   char **file_name) internal_function;

/* Same, but takes an explicit build_id, can also be used for alt debug.  */
extern int __libdwfl_open_by_build_id (Dwfl_Module *mod, bool debug,
				       char **file_name, const size_t id_len,
				       const uint8_t *id) internal_function;

extern uint32_t __libdwfl_crc32 (uint32_t crc, unsigned char *buf, size_t len)
  attribute_hidden;
extern int __libdwfl_crc32_file (int fd, uint32_t *resp) attribute_hidden;


/* Given ELF and some parameters return TRUE if the *P return value parameters
   have been successfully filled in.  Any of the *P parameters can be NULL.  */
extern bool __libdwfl_elf_address_range (Elf *elf, GElf_Addr base,
					 bool add_p_vaddr, bool sanity,
					 GElf_Addr *vaddrp,
					 GElf_Addr *address_syncp,
					 GElf_Addr *startp, GElf_Addr *endp,
					 GElf_Addr *biasp, GElf_Half *e_typep)
  internal_function;

/* Meat of dwfl_report_elf, given elf_begin just called.
   Consumes ELF on success, not on failure.  */
extern Dwfl_Module *__libdwfl_report_elf (Dwfl *dwfl, const char *name,
					  const char *file_name, int fd,
					  Elf *elf, GElf_Addr base,
					  bool add_p_vaddr, bool sanity)
  internal_function;

/* Meat of dwfl_report_offline.  */
extern Dwfl_Module *__libdwfl_report_offline (Dwfl *dwfl, const char *name,
					      const char *file_name,
					      int fd, bool closefd,
					      int (*predicate) (const char *,
								const char *))
  internal_function;

/* Free PROCESS.  Unlink and free also any structures it references.  */
extern void __libdwfl_process_free (Dwfl_Process *process)
  internal_function;

/* Update STATE->unwound for the unwound frame.
   On error STATE->unwound == NULL
   or STATE->unwound->pc_state == DWFL_FRAME_STATE_ERROR;
   in such case dwfl_errno () is set.
   If STATE->unwound->pc_state == DWFL_FRAME_STATE_PC_UNDEFINED
   then STATE was the last valid frame.  */
extern void __libdwfl_frame_unwind (Dwfl_Frame *state)
  internal_function;

/* Align segment START downwards or END upwards addresses according to DWFL.  */
extern GElf_Addr __libdwfl_segment_start (Dwfl *dwfl, GElf_Addr start)
  internal_function;
extern GElf_Addr __libdwfl_segment_end (Dwfl *dwfl, GElf_Addr end)
  internal_function;

/* Decompression wrappers: decompress whole file into memory.  */
extern Dwfl_Error __libdw_gunzip  (int fd, off_t start_offset,
				   void *mapped, size_t mapped_size,
				   void **whole, size_t *whole_size)
  internal_function;
extern Dwfl_Error __libdw_bunzip2 (int fd, off_t start_offset,
				   void *mapped, size_t mapped_size,
				   void **whole, size_t *whole_size)
  internal_function;
extern Dwfl_Error __libdw_unlzma (int fd, off_t start_offset,
				  void *mapped, size_t mapped_size,
				  void **whole, size_t *whole_size)
  internal_function;
extern Dwfl_Error __libdw_unzstd (int fd, off_t start_offset,
				  void *mapped, size_t mapped_size,
				  void **whole, size_t *whole_size)
  internal_function;

/* Skip the image header before a file image: updates *START_OFFSET.  */
extern Dwfl_Error __libdw_image_header (int fd, off_t *start_offset,
					void *mapped, size_t mapped_size)
  internal_function;

/* Open Elf handle on *FDP.  This handles decompression and checks
   elf_kind.  Succeed only for ELF_K_ELF, or also ELF_K_AR if ARCHIVE_OK.
   Returns DWFL_E_NOERROR and sets *ELFP on success, resets *FDP to -1 if
   it's no longer used.  Resets *FDP on failure too iff CLOSE_ON_FAIL.  */
extern Dwfl_Error __libdw_open_file (int *fdp, Elf **elfp,
				     bool close_on_fail, bool archive_ok)
  internal_function;

/* Same as __libdw_open_file, but opens Elf handle from memory region.  */
extern Dwfl_Error __libdw_open_elf_memory (char *data, size_t size, Elf **elfp,
					   bool archive_ok)
  internal_function;

/* Same as __libdw_open_file, but never closes the given file
   descriptor and ELF_K_AR is always an acceptable type.  */
extern Dwfl_Error __libdw_open_elf (int fd, Elf **elfp) internal_function;

/* Fetch PT_DYNAMIC P_VADDR from ELF and store it to *VADDRP.  Return success.
   *VADDRP is not modified if the function fails.  */
extern bool __libdwfl_dynamic_vaddr_get (Elf *elf, GElf_Addr *vaddrp)
  internal_function;

#ifdef ENABLE_LIBDEBUGINFOD
/* Internal interface to libdebuginfod (if installed).  */
int
__libdwfl_debuginfod_find_executable (Dwfl *dwfl,
				      const unsigned char *build_id_bits,
				      size_t build_id_len);
int
__libdwfl_debuginfod_find_debuginfo (Dwfl *dwfl,
				     const unsigned char *build_id_bits,
				     size_t build_id_len);
void
__libdwfl_debuginfod_end (debuginfod_client *c);
#endif


/* These are working nicely for --core, but are not ready to be
   exported interfaces quite yet.  */

/* Type of callback function ...
 */
typedef bool Dwfl_Memory_Callback (Dwfl *dwfl, int segndx,
				   void **buffer, size_t *buffer_available,
				   GElf_Addr vaddr, size_t minread, void *arg);

/* Type of callback function ...
 */
typedef bool Dwfl_Module_Callback (Dwfl_Module *mod, void **userdata,
				   const char *name, Dwarf_Addr base,
				   void **buffer, size_t *buffer_available,
				   GElf_Off cost, GElf_Off worthwhile,
				   GElf_Off whole, GElf_Off contiguous,
				   void *arg, Elf **elfp);

/* One shared library (or executable) info from DT_DEBUG link map.  */
struct r_debug_info_module
{
  struct r_debug_info_module *next;
  /* FD is -1 iff ELF is NULL.  */
  int fd;
  Elf *elf;
  GElf_Addr l_ld;
  /* START and END are both zero if not valid.  */
  GElf_Addr start, end;
  bool disk_file_has_build_id;
  char name[0];
};

/* Information gathered from DT_DEBUG by dwfl_link_map_report hinted to
   dwfl_segment_report_module.  */
struct r_debug_info
{
  struct r_debug_info_module *module;
};

/* ...
 */
extern int dwfl_segment_report_module (Dwfl *dwfl, int ndx, const char *name,
				       Dwfl_Memory_Callback *memory_callback,
				       void *memory_callback_arg,
				       Dwfl_Module_Callback *read_eagerly,
				       void *read_eagerly_arg,
				       size_t maxread,
				       const void *note_file,
				       size_t note_file_size,
				       const struct r_debug_info *r_debug_info);

/* Report a module for entry in the dynamic linker's struct link_map list.
   For each link_map entry, if an existing module resides at its address,
   this just modifies that module's name and suggested file name.  If
   no such module exists, this calls dwfl_report_elf on the l_name string.

   If AUXV is not null, it points to AUXV_SIZE bytes of auxiliary vector
   data as contained in an NT_AUXV note or read from a /proc/pid/auxv
   file.  When this is available, it guides the search.  If AUXV is null
   or the memory it points to is not accessible, then this search can
   only find where to begin if the correct executable file was
   previously reported and preloaded as with dwfl_report_elf.

   Fill in R_DEBUG_INFO if it is not NULL.  It should be cleared by the
   caller, this function does not touch fields it does not need to modify.
   If R_DEBUG_INFO is not NULL then no modules get added to DWFL, caller
   has to add them from filled in R_DEBUG_INFO.

   Returns the number of modules found, or -1 for errors.  */
extern int dwfl_link_map_report (Dwfl *dwfl, const void *auxv, size_t auxv_size,
				 Dwfl_Memory_Callback *memory_callback,
				 void *memory_callback_arg,
				 struct r_debug_info *r_debug_info);


/* Avoid PLT entries.  */
INTDECL (dwfl_begin)
INTDECL (dwfl_errmsg)
INTDECL (dwfl_errno)
INTDECL (dwfl_addrmodule)
INTDECL (dwfl_addrsegment)
INTDECL (dwfl_addrdwarf)
INTDECL (dwfl_addrdie)
INTDECL (dwfl_core_file_attach)
INTDECL (dwfl_core_file_report)
INTDECL (dwfl_getmodules)
INTDECL (dwfl_module_addrdie)
INTDECL (dwfl_module_address_section)
INTDECL (dwfl_module_addrinfo)
INTDECL (dwfl_module_addrsym)
INTDECL (dwfl_module_build_id)
INTDECL (dwfl_module_getdwarf)
INTDECL (dwfl_module_getelf)
INTDECL (dwfl_module_getsym)
INTDECL (dwfl_module_getsym_info)
INTDECL (dwfl_module_getsymtab)
INTDECL (dwfl_module_getsymtab_first_global)
INTDECL (dwfl_module_getsrc)
INTDECL (dwfl_module_report_build_id)
INTDECL (dwfl_report_elf)
INTDECL (dwfl_report_begin)
INTDECL (dwfl_report_begin_add)
INTDECL (dwfl_report_module)
INTDECL (dwfl_report_segment)
INTDECL (dwfl_report_offline)
INTDECL (dwfl_report_offline_memory)
INTDECL (dwfl_report_end)
INTDECL (dwfl_build_id_find_elf)
INTDECL (dwfl_build_id_find_debuginfo)
INTDECL (dwfl_standard_find_debuginfo)
INTDECL (dwfl_link_map_report)
INTDECL (dwfl_linux_kernel_find_elf)
INTDECL (dwfl_linux_kernel_module_section_address)
INTDECL (dwfl_linux_proc_attach)
INTDECL (dwfl_linux_proc_report)
INTDECL (dwfl_linux_proc_maps_report)
INTDECL (dwfl_linux_proc_find_elf)
INTDECL (dwfl_linux_kernel_report_kernel)
INTDECL (dwfl_linux_kernel_report_modules)
INTDECL (dwfl_linux_kernel_report_offline)
INTDECL (dwfl_offline_section_address)
INTDECL (dwfl_module_relocate_address)
INTDECL (dwfl_module_dwarf_cfi)
INTDECL (dwfl_module_eh_cfi)
INTDECL (dwfl_attach_state)
INTDECL (dwfl_pid)
INTDECL (dwfl_thread_dwfl)
INTDECL (dwfl_thread_tid)
INTDECL (dwfl_frame_thread)
INTDECL (dwfl_thread_state_registers)
INTDECL (dwfl_thread_state_register_pc)
INTDECL (dwfl_getthread_frames)
INTDECL (dwfl_getthreads)
INTDECL (dwfl_thread_getframes)
INTDECL (dwfl_frame_pc)
INTDECL (dwfl_frame_reg)
INTDECL (dwfl_get_debuginfod_client)

/* Leading arguments standard to callbacks passed a Dwfl_Module.  */
#define MODCB_ARGS(mod)	(mod), &(mod)->userdata, (mod)->name, (mod)->low_addr
#define CBFAIL		(errno ? DWFL_E (ERRNO, errno) : DWFL_E_CB);


/* The default used by dwfl_standard_find_debuginfo.  */
#define DEFAULT_DEBUGINFO_PATH ":.debug:/usr/lib/debug"


#endif	/* libdwflP.h */
