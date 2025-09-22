/* Interfaces for libdwfl.
   Copyright (C) 2005-2010, 2013 Red Hat, Inc.
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

#ifndef _LIBDWFL_H
#define _LIBDWFL_H	1

#include "libdw.h"
#include <stdio.h>

/* Handle for a session using the library.  */
typedef struct Dwfl Dwfl;

/* Handle for a module.  */
typedef struct Dwfl_Module Dwfl_Module;

/* Handle describing a line record.  */
typedef struct Dwfl_Line Dwfl_Line;

/* This holds information common for all the frames of one backtrace for
   a particular thread/task/TID.  Several threads belong to one Dwfl.  */
typedef struct Dwfl_Thread Dwfl_Thread;

/* This holds everything we know about the state of the frame at a particular
   PC location described by an FDE belonging to Dwfl_Thread.  */
typedef struct Dwfl_Frame Dwfl_Frame;

/* Handle for debuginfod-client connection.  */
#ifndef _ELFUTILS_DEBUGINFOD_CLIENT_TYPEDEF
typedef struct debuginfod_client debuginfod_client;
#define _ELFUTILS_DEBUGINFOD_CLIENT_TYPEDEF 1
#endif

/* Callbacks.  */
typedef struct
{
  int (*find_elf) (Dwfl_Module *mod, void **userdata,
		   const char *modname, Dwarf_Addr base,
		   char **file_name, Elf **elfp);

  int (*find_debuginfo) (Dwfl_Module *mod, void **userdata,
			 const char *modname, Dwarf_Addr base,
			 const char *file_name,
			 const char *debuglink_file, GElf_Word debuglink_crc,
			 char **debuginfo_file_name);

  /* Fill *ADDR with the loaded address of the section called SECNAME in
     the given module.  Use (Dwarf_Addr) -1 if this section is omitted from
     accessible memory.  This is called exactly once for each SHF_ALLOC
     section that relocations affecting DWARF data refer to, so it can
     easily be used to collect state about the sections referenced.  */
  int (*section_address) (Dwfl_Module *mod, void **userdata,
			  const char *modname, Dwarf_Addr base,
			  const char *secname,
			  GElf_Word shndx, const GElf_Shdr *shdr,
			  Dwarf_Addr *addr);

  char **debuginfo_path;	/* See dwfl_standard_find_debuginfo.  */
} Dwfl_Callbacks;


#ifdef __cplusplus
extern "C" {
#endif

/* Start a new session with the library.  */
extern Dwfl *dwfl_begin (const Dwfl_Callbacks *callbacks)
  __nonnull_attribute__ (1);


/* End a session.  */
extern void dwfl_end (Dwfl *);

/* Return implementation's version string suitable for printing.  */
extern const char *dwfl_version (Dwfl *);

/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int dwfl_errno (void);

/* Return error string for ERROR.  If ERROR is zero, return error string
   for most recent error or NULL if none occurred.  If ERROR is -1 the
   behaviour is similar to the last case except that not NULL but a legal
   string is returned.  */
extern const char *dwfl_errmsg (int err);


/* Start reporting the current set of segments and modules to the library.
   All existing segments are wiped.  Existing modules are marked to be
   deleted, and will not be found via dwfl_addrmodule et al if they are not
   re-reported before dwfl_report_end is called.  */
extern void dwfl_report_begin (Dwfl *dwfl);

/* Report that segment NDX begins at PHDR->p_vaddr + BIAS.
   If NDX is < 0, the value succeeding the last call's NDX
   is used instead (zero on the first call).  IDENT is ignored.

   If nonzero, the smallest PHDR->p_align value seen sets the
   effective page size for the address space DWFL describes.
   This is the granularity at which reported module boundary
   addresses will be considered to fall in or out of a segment.

   Returns -1 for errors, or NDX (or its assigned replacement) on success.

   Reporting segments at all is optional.  Its only benefit to the caller is to
   offer this quick lookup via dwfl_addrsegment, or use other segment-based
   calls.  */
extern int dwfl_report_segment (Dwfl *dwfl, int ndx,
				const GElf_Phdr *phdr, GElf_Addr bias,
				const void *ident);

/* Report that a module called NAME spans addresses [START, END).
   Returns the module handle, either existing or newly allocated,
   or returns a null pointer for an allocation error.  */
extern Dwfl_Module *dwfl_report_module (Dwfl *dwfl, const char *name,
					Dwarf_Addr start, Dwarf_Addr end);

/* Report a module to address BASE with start and end addresses computed
   from the ELF program headers in the given file - see the table below.
   FD may be -1 to open FILE_NAME.  On success, FD is consumed by the
   library, and the `find_elf' callback will not be used for this module.
	    ADD_P_VADDR  BASE
   ET_EXEC  ignored      ignored
   ET_DYN   false        absolute address where to place the file
	    true         start address relative to ELF's phdr p_vaddr
   ET_REL   ignored      absolute address where to place the file
   ET_CORE  ignored      ignored
   ET_DYN ELF phdr p_vaddr address can be non-zero if the shared library
   has been prelinked by tool prelink(8).  */
extern Dwfl_Module *dwfl_report_elf (Dwfl *dwfl, const char *name,
				     const char *file_name, int fd,
				     GElf_Addr base, bool add_p_vaddr);

/* Similar, but report the module for offline use.  All ET_EXEC files
   being reported must be reported before any relocatable objects.
   If this is used, dwfl_report_module and dwfl_report_elf may not be
   used in the same reporting session.  */
extern Dwfl_Module *dwfl_report_offline (Dwfl *dwfl, const char *name,
					 const char *file_name, int fd);

/* Similar, but report ELF from memory region.  */
extern Dwfl_Module *dwfl_report_offline_memory (Dwfl *dwfl, const char *name,
						const char *file_name,
						char *data, size_t size);

/* Finish reporting the current set of modules to the library.
   If REMOVED is not null, it's called for each module that
   existed before but was not included in the current report.
   Returns a nonzero return value from the callback.
   The callback may call dwfl_report_module; doing so with the
   details of the module being removed prevents its removal.
   DWFL cannot be used until this function has returned zero.  */
extern int dwfl_report_end (Dwfl *dwfl,
			    int (*removed) (Dwfl_Module *, void *,
					    const char *, Dwarf_Addr,
					    void *arg),
			    void *arg);

/* Start reporting additional modules to the library.  No calls but
   dwfl_report_* can be made on DWFL until dwfl_report_end is called.
   This is like dwfl_report_begin, but all the old modules are kept on.
   More dwfl_report_* calls can follow to add more modules.
   When dwfl_report_end is called, no old modules will be removed.  */
extern void dwfl_report_begin_add (Dwfl *dwfl);


/* Return the name of the module, and for each non-null argument store
   interesting details: *USERDATA is a location for storing your own
   pointer, **USERDATA is initially null; *START and *END give the address
   range covered by the module; *DWBIAS is the address bias for debugging
   information, and *SYMBIAS for symbol table entries (either is -1 if not
   yet accessed); *MAINFILE is the name of the ELF file, and *DEBUGFILE the
   name of the debuginfo file (might be equal to *MAINFILE; either is null
   if not yet accessed).  */
extern const char *dwfl_module_info (Dwfl_Module *mod, void ***userdata,
				     Dwarf_Addr *start, Dwarf_Addr *end,
				     Dwarf_Addr *dwbias, Dwarf_Addr *symbias,
				     const char **mainfile,
				     const char **debugfile);

/* Iterate through the modules, starting the walk with OFFSET == 0.
   Calls *CALLBACK for each module as long as it returns DWARF_CB_OK.
   When *CALLBACK returns another value, the walk stops and the
   return value can be passed as OFFSET to resume it.  Returns 0 when
   there are no more modules, or -1 for errors.  */
extern ptrdiff_t dwfl_getmodules (Dwfl *dwfl,
				  int (*callback) (Dwfl_Module *, void **,
						   const char *, Dwarf_Addr,
						   void *arg),
				  void *arg,
				  ptrdiff_t offset);

/* Find the module containing the given address.  */
extern Dwfl_Module *dwfl_addrmodule (Dwfl *dwfl, Dwarf_Addr address);

/* Find the segment, if any, and module, if any, containing ADDRESS.
   Returns a segment index returned by dwfl_report_segment, or -1
   if no segment matches the address.  Regardless of the return value,
   *MOD is always set to the module containing ADDRESS, or to null.  */
extern int dwfl_addrsegment (Dwfl *dwfl, Dwarf_Addr address, Dwfl_Module **mod);



/* Report the known build ID bits associated with a module.
   If VADDR is nonzero, it gives the absolute address where those
   bits are found within the module.  This can be called at any
   time, but is usually used immediately after dwfl_report_module.
   Once the module's main ELF file is opened, the ID note found
   there takes precedence and cannot be changed.  */
extern int dwfl_module_report_build_id (Dwfl_Module *mod,
					const unsigned char *bits, size_t len,
					GElf_Addr vaddr)
  __nonnull_attribute__ (2);

/* Extract the build ID bits associated with a module.
   Returns -1 for errors, 0 if no ID is known, or the number of ID bytes.
   When an ID is found, *BITS points to it; *VADDR is the absolute address
   at which the ID bits are found within the module, or 0 if unknown.

   This returns 0 when the module's main ELF file has not yet been loaded
   and its build ID bits were not reported.  To ensure the ID is always
   returned when determinable, call dwfl_module_getelf first.  */
extern int dwfl_module_build_id (Dwfl_Module *mod,
				 const unsigned char **bits, GElf_Addr *vaddr)
  __nonnull_attribute__ (2, 3);


/*** Standard callbacks ***/

/* These standard find_elf and find_debuginfo callbacks are
   controlled by a string specifying directories to look in.
   If `debuginfo_path' is set in the Dwfl_Callbacks structure
   and the char * it points to is not null, that supplies the
   string.  Otherwise a default path is used.

   If the first character of the string is + or - that enables or
   disables CRC32 checksum validation when it's necessary.  The
   remainder of the string is composed of elements separated by
   colons.  Each element can start with + or - to override the
   global checksum behavior.  This flag is never relevant when
   working with build IDs, but it's always parsed in the path
   string.  The remainder of the element indicates a directory.

   Searches by build ID consult only the elements naming absolute
   directory paths.  They look under those directories for a link
   named ".build-id/xx/yy" or ".build-id/xx/yy.debug", where "xxyy"
   is the lower-case hexadecimal representation of the ID bytes.

   In searches for debuginfo by name, if the remainder of the
   element is empty, the directory containing the main file is
   tried; if it's an absolute path name, the absolute directory path
   (and any subdirectory of that path) containing the main file is
   taken as a subdirectory of this path; a relative path name is taken
   as a subdirectory of the directory containing the main file.
   Hence for /usr/bin/ls, the default string ":.debug:/usr/lib/debug"
   says to look in /usr/bin, then /usr/bin/.debug, then the path subdirs
   under /usr/lib/debug, in the order /usr/lib/debug/usr/bin, then
   /usr/lib/debug/bin, and finally /usr/lib/debug, for the file name in
   the .gnu_debuglink section (or "ls.debug" if none was found).  */

/* Standard find_elf callback function working solely on build ID.
   This can be tried first by any find_elf callback, to use the
   bits passed to dwfl_module_report_build_id, if any.  */
extern int dwfl_build_id_find_elf (Dwfl_Module *, void **,
				   const char *, Dwarf_Addr,
				   char **, Elf **);

/* Standard find_debuginfo callback function working solely on build ID.
   This can be tried first by any find_debuginfo callback,
   to use the build ID bits from the main file when present.  */
extern int dwfl_build_id_find_debuginfo (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, const char *,
					 GElf_Word, char **);

/* Standard find_debuginfo callback function.
   If a build ID is available, this tries first to use that.
   If there is no build ID or no valid debuginfo found by ID,
   it searches the debuginfo path by name, as described above.
   Any file found in the path is validated by build ID if possible,
   or else by CRC32 checksum if enabled, and skipped if it does not match.  */
extern int dwfl_standard_find_debuginfo (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, const char *,
					 GElf_Word, char **);


/* This callback must be used when using dwfl_offline_* to report modules,
   if ET_REL is to be supported.  */
extern int dwfl_offline_section_address (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, GElf_Word,
					 const GElf_Shdr *,
					 Dwarf_Addr *addr);


/* Callbacks for working with kernel modules in the running Linux kernel.  */
extern int dwfl_linux_kernel_find_elf (Dwfl_Module *, void **,
				       const char *, Dwarf_Addr,
				       char **, Elf **);
extern int dwfl_linux_kernel_module_section_address (Dwfl_Module *, void **,
						     const char *, Dwarf_Addr,
						     const char *, GElf_Word,
						     const GElf_Shdr *,
						     Dwarf_Addr *addr);

/* Call dwfl_report_elf for the running Linux kernel.
   Returns zero on success, -1 if dwfl_report_module failed,
   or an errno code if opening the kernel binary failed.  */
extern int dwfl_linux_kernel_report_kernel (Dwfl *dwfl);

/* Call dwfl_report_module for each kernel module in the running Linux kernel.
   Returns zero on success, -1 if dwfl_report_module failed,
   or an errno code if reading the list of modules failed.  */
extern int dwfl_linux_kernel_report_modules (Dwfl *dwfl);

/* Report a kernel and its modules found on disk, for offline use.
   If RELEASE starts with '/', it names a directory to look in;
   if not, it names a directory to find under /lib/modules/;
   if null, /lib/modules/`uname -r` is used.
   Returns zero on success, -1 if dwfl_report_module failed,
   or an errno code if finding the files on disk failed.

   If PREDICATE is not null, it is called with each module to be reported;
   its arguments are the module name, and the ELF file name or null if unknown,
   and its return value should be zero to skip the module, one to report it,
   or -1 to cause the call to fail and return errno.  */
extern int dwfl_linux_kernel_report_offline (Dwfl *dwfl, const char *release,
					     int (*predicate) (const char *,
							       const char *));

/* Examine an ET_CORE file and report modules based on its contents.
   This can follow a dwfl_report_offline call to bootstrap the
   DT_DEBUG method of following the dynamic linker link_map chain, in
   case the core file does not contain enough of the executable's text
   segment to locate its PT_DYNAMIC in the dump.  In such case you need to
   supply non-NULL EXECUTABLE, otherwise dynamic libraries will not be loaded
   into the DWFL map.  This might call dwfl_report_elf on file names found in
   the dump if reading some link_map files is the only way to ascertain those
   modules' addresses.  Returns the number of modules reported, or -1 for
   errors.  */
extern int dwfl_core_file_report (Dwfl *dwfl, Elf *elf, const char *executable);

/* Call dwfl_report_module for each file mapped into the address space of PID.
   Returns zero on success, -1 if dwfl_report_module failed,
   or an errno code if opening the proc files failed.  */
extern int dwfl_linux_proc_report (Dwfl *dwfl, pid_t pid);

/* Similar, but reads an input stream in the format of Linux /proc/PID/maps
   files giving module layout, not the file for a live process.  */
extern int dwfl_linux_proc_maps_report (Dwfl *dwfl, FILE *);

/* Trivial find_elf callback for use with dwfl_linux_proc_report.
   This uses the module name as a file name directly and tries to open it
   if it begin with a slash, or handles the magic string "[vdso]".  */
extern int dwfl_linux_proc_find_elf (Dwfl_Module *mod, void **userdata,
				     const char *module_name, Dwarf_Addr base,
				     char **file_name, Elf **);

/* Standard argument parsing for using a standard callback set.  */
struct argp;
extern const struct argp *dwfl_standard_argp (void) __const_attribute__;


/*** Relocation of addresses from Dwfl ***/

/* Return the number of relocatable bases associated with the module,
   which is zero for ET_EXEC and one for ET_DYN.  Returns -1 for errors.  */
extern int dwfl_module_relocations (Dwfl_Module *mod);

/* Return the relocation base index associated with the *ADDRESS location,
   and adjust *ADDRESS to be an offset relative to that base.
   Returns -1 for errors.  */
extern int dwfl_module_relocate_address (Dwfl_Module *mod,
					 Dwarf_Addr *address);

/* Return the ELF section name for the given relocation base index;
   if SHNDXP is not null, set *SHNDXP to the ELF section index.
   For ET_DYN, returns "" and sets *SHNDXP to SHN_ABS; the relocation
   base is the runtime start address reported for the module.
   Returns null for errors.  */
extern const char *dwfl_module_relocation_info (Dwfl_Module *mod,
						unsigned int idx,
						GElf_Word *shndxp);

/* Validate that ADDRESS and ADDRESS+OFFSET lie in a known module
   and both within the same contiguous region for relocation purposes.
   Returns zero for success and -1 for errors.  */
extern int dwfl_validate_address (Dwfl *dwfl,
				  Dwarf_Addr address, Dwarf_Sword offset);


/*** ELF access functions ***/

/* Fetch the module main ELF file (where the allocated sections
   are found) for use with libelf.  If successful, fills in *BIAS
   with the difference between addresses within the loaded module
   and those in symbol tables or Dwarf information referring to it.  */
extern Elf *dwfl_module_getelf (Dwfl_Module *, GElf_Addr *bias)
  __nonnull_attribute__ (2);

/* Return the number of symbols in the module's symbol table,
   or -1 for errors.  */
extern int dwfl_module_getsymtab (Dwfl_Module *mod);

/* Return the index of the first global symbol in the module's symbol
   table, or -1 for errors.  In each symbol table, all symbols with
   STB_LOCAL binding precede the weak and global symbols.  This
   function returns the symbol table index one greater than the last
   local symbol.  */
extern int dwfl_module_getsymtab_first_global (Dwfl_Module *mod);

/* Fetch one entry from the module's symbol table.  On errors, returns
   NULL.  If successful, fills in *SYM and returns the string for st_name.
   This works like gelf_getsym except that st_value is always adjusted to
   an absolute value based on the module's location, when the symbol is in
   an SHF_ALLOC section.  If SHNDXP is non-null, it's set with the section
   index (whether from st_shndx or extended index table); in case of a
   symbol in a non-allocated section, *SHNDXP is instead set to -1.
   Note that since symbols can come from either the main, debug or auxiliary
   ELF symbol file (either dynsym or symtab) the section index can only
   be reliably used to compare against special section constants like
   SHN_UNDEF or SHN_ABS.  It is recommended to use dwfl_module_getsym_info
   which doesn't have these deficiencies.  */
extern const char *dwfl_module_getsym (Dwfl_Module *mod, int ndx,
				       GElf_Sym *sym, GElf_Word *shndxp)
  __nonnull_attribute__ (3);

/* Fetch one entry from the module's symbol table and the associated
   address value.  On errors, returns NULL.  If successful, fills in
   *SYM, *ADDR and returns the string for st_name.  This works like
   gelf_getsym.  *ADDR is set to the st_value adjusted to an absolute
   value based on the module's location, when the symbol is in an
   SHF_ALLOC section.  For non-ET_REL files, if the arch uses function
   descriptors, and the st_value points to one, *ADDR will be resolved
   to the actual function entry address.  The SYM->ST_VALUE itself
   isn't adjusted in any way.  Fills in ELFP, if not NULL, with the
   ELF file the symbol originally came from.  Note that symbols can
   come from either the main, debug or auxiliary ELF symbol file
   (either dynsym or symtab).  If SHNDXP is non-null, it's set with
   the section index (whether from st_shndx or extended index table);
   in case of a symbol in a non-allocated section, *SHNDXP is instead
   set to -1.  Fills in BIAS, if not NULL, with the difference between
   addresses within the loaded module and those in symbol table of the
   ELF file.  Note that the address associated with the symbol might
   be in a different section than the returned symbol.  The section in
   the main elf file in which returned ADDR falls can be found with
   dwfl_module_address_section.  */
extern const char *dwfl_module_getsym_info (Dwfl_Module *mod, int ndx,
					    GElf_Sym *sym, GElf_Addr *addr,
					    GElf_Word *shndxp,
					    Elf **elfp, Dwarf_Addr *bias)
  __nonnull_attribute__ (3, 4);

/* Find the symbol that ADDRESS lies inside, and return its name.  */
extern const char *dwfl_module_addrname (Dwfl_Module *mod, GElf_Addr address);

/* Find the symbol associated with ADDRESS.  Return its name or NULL
   when nothing was found.  If the architecture uses function
   descriptors, and symbol st_value points to one, ADDRESS will be
   matched against either the adjusted st_value or the associated
   function entry value as described in dwfl_module_getsym_info.
   OFFSET will be filled in with the difference from the start of the
   symbol (or function entry), OFFSET cannot be NULL.  SYM is filled
   in with the symbol associated with the matched ADDRESS, SYM cannot
   be NULL.  The SYM->ST_VALUE itself isn't adjusted in any way.
   Fills in ELFP, if not NULL, with the ELF file the symbol originally
   came from.  Note that symbols can come from either the main, debug
   or auxiliary ELF symbol file (either dynsym or symtab).  If SHNDXP
   is non-null, it's set with the section index (whether from st_shndx
   or extended index table).  Fills in BIAS, if not NULL, with the
   difference between addresses within the loaded module and those in
   symbol table of the ELF file.  Note that the address matched
   against the symbol might be in a different section than the
   returned symbol.  The section in the main elf file in ADDRESS falls
   can be found with dwfl_module_address_section.  */
extern const char *dwfl_module_addrinfo (Dwfl_Module *mod, GElf_Addr address,
					 GElf_Off *offset, GElf_Sym *sym,
					 GElf_Word *shndxp, Elf **elfp,
					 Dwarf_Addr *bias)
  __nonnull_attribute__ (3, 4);

/* Find the symbol that ADDRESS lies inside, and return detailed
   information as for dwfl_module_getsym (above).  Note that like
   dwfl_module_getsym this function also adjusts SYM->ST_VALUE to an
   absolute value based on the module's location.  ADDRESS is only
   matched against this adjusted SYM->ST_VALUE.  This means that
   depending on architecture this might only match symbols that
   represent function descriptor addresses (and not function entry
   addresses).  For these reasons it is recommended to use
   dwfl_module_addrinfo instead.  */
extern const char *dwfl_module_addrsym (Dwfl_Module *mod, GElf_Addr address,
					GElf_Sym *sym, GElf_Word *shndxp)
  __nonnull_attribute__ (3);

/* Find the ELF section that *ADDRESS lies inside and return it.
   On success, adjusts *ADDRESS to be relative to the section,
   and sets *BIAS to the difference between addresses used in
   the returned section's headers and run-time addresses.  */
extern Elf_Scn *dwfl_module_address_section (Dwfl_Module *mod,
					     Dwarf_Addr *address,
					     Dwarf_Addr *bias)
  __nonnull_attribute__ (2, 3);


/*** Dwarf access functions ***/

/* Fetch the module's debug information for use with libdw.
   If successful, fills in *BIAS with the difference between
   addresses within the loaded module and those  to use with libdw.  */
extern Dwarf *dwfl_module_getdwarf (Dwfl_Module *, Dwarf_Addr *bias)
     __nonnull_attribute__ (2);

/* Get the libdw handle for each module.  */
extern ptrdiff_t dwfl_getdwarf (Dwfl *,
				int (*callback) (Dwfl_Module *, void **,
						 const char *, Dwarf_Addr,
						 Dwarf *, Dwarf_Addr, void *),
				void *arg, ptrdiff_t offset);

/* Look up the module containing ADDR and return its debugging information,
   loading it if necessary.  */
extern Dwarf *dwfl_addrdwarf (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);


/* Find the CU containing ADDR and return its DIE.  */
extern Dwarf_Die *dwfl_addrdie (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);
extern Dwarf_Die *dwfl_module_addrdie (Dwfl_Module *mod,
				       Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);

/* Iterate through the CUs, start with null for LASTCU.  */
extern Dwarf_Die *dwfl_nextcu (Dwfl *dwfl, Dwarf_Die *lastcu, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);
extern Dwarf_Die *dwfl_module_nextcu (Dwfl_Module *mod,
				      Dwarf_Die *lastcu, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);

/* Return the module containing the CU DIE.  */
extern Dwfl_Module *dwfl_cumodule (Dwarf_Die *cudie);


/* Cache the source line information for the CU and return the
   number of Dwfl_Line entries it has.  */
extern int dwfl_getsrclines (Dwarf_Die *cudie, size_t *nlines);

/* Access one line number entry within the CU.  */
extern Dwfl_Line *dwfl_onesrcline (Dwarf_Die *cudie, size_t idx);

/* Get source for address.  */
extern Dwfl_Line *dwfl_module_getsrc (Dwfl_Module *mod, Dwarf_Addr addr);
extern Dwfl_Line *dwfl_getsrc (Dwfl *dwfl, Dwarf_Addr addr);

/* Get address for source.  */
extern int dwfl_module_getsrc_file (Dwfl_Module *mod,
				    const char *fname, int lineno, int column,
				    Dwfl_Line ***srcsp, size_t *nsrcs);

/* Return the module containing this line record.  */
extern Dwfl_Module *dwfl_linemodule (Dwfl_Line *line);

/* Return the CU containing this line record.  */
extern Dwarf_Die *dwfl_linecu (Dwfl_Line *line);

/* Return the source file name and fill in other information.
   Arguments may be null for unneeded fields.  */
extern const char *dwfl_lineinfo (Dwfl_Line *line, Dwarf_Addr *addr,
				  int *linep, int *colp,
				  Dwarf_Word *mtime, Dwarf_Word *length);

  /* Return the equivalent Dwarf_Line and the bias to apply to its address.  */
extern Dwarf_Line *dwfl_dwarf_line (Dwfl_Line *line, Dwarf_Addr *bias);

/* Return the compilation directory (AT_comp_dir) from this line's CU.  */
extern const char *dwfl_line_comp_dir (Dwfl_Line *line);


/*** Machine backend access functions ***/

/* Return location expression to find return value given a
   DW_TAG_subprogram, DW_TAG_subroutine_type, or similar DIE describing
   function itself (whose DW_AT_type attribute describes its return type).
   The given DIE must come from the given module.  Returns -1 for errors.
   Returns zero if the function has no return value (e.g. "void" in C).
   Otherwise, *LOCOPS gets a location expression to find the return value,
   and returns the number of operations in the expression.  The pointer is
   permanently allocated at least as long as the module is live.  */
extern int dwfl_module_return_value_location (Dwfl_Module *mod,
					      Dwarf_Die *functypedie,
					      const Dwarf_Op **locops);

/* Enumerate the DWARF register numbers and their names.
   For each register, CALLBACK gets its DWARF number, a string describing
   the register set (such as "integer" or "FPU"), a prefix used in
   assembler syntax (such as "%" or "$", may be ""), and the name for the
   register (contains identifier characters only, possibly all digits).
   The REGNAME string is valid only during the callback. */
extern int dwfl_module_register_names (Dwfl_Module *mod,
				       int (*callback) (void *arg,
							int regno,
							const char *setname,
							const char *prefix,
							const char *regname,
							int bits, int type),
				       void *arg);


/* Find the CFI for this module.  Returns NULL if there is no CFI.
   On success, fills in *BIAS with the difference between addresses
   within the loaded module and those in the CFI referring to it.
   The pointer returned can be used until the module is cleaned up.
   Calling these more than once returns the same pointers.

   dwfl_module_dwarf_cfi gets the '.debug_frame' information found with the
   rest of the DWARF information.  dwfl_module_eh_cfi gets the '.eh_frame'
   information found linked into the text.  A module might have either or
   both.  */
extern Dwarf_CFI *dwfl_module_dwarf_cfi (Dwfl_Module *mod, Dwarf_Addr *bias);
extern Dwarf_CFI *dwfl_module_eh_cfi (Dwfl_Module *mod, Dwarf_Addr *bias);


typedef struct
{
  /* Called to iterate through threads.  Returns next TID (thread ID) on
     success, a negative number on failure and zero if there are no more
     threads.  dwfl_errno () should be set if negative number has been
     returned.  *THREAD_ARGP is NULL on first call, and may be optionally
     set by the implementation. The value set by the implementation will
     be passed in on the next call to NEXT_THREAD.  THREAD_ARGP is never
     NULL.  *THREAD_ARGP will be passed to set_initial_registers or
     thread_detach callbacks together with Dwfl_Thread *thread.  This
     method must not be NULL.  */
  pid_t (*next_thread) (Dwfl *dwfl, void *dwfl_arg, void **thread_argp)
    __nonnull_attribute__ (1);

  /* Called to get a specific thread.  Returns true if there is a
     thread with the given thread id number, returns false if no such
     thread exists and will set dwfl_errno in that case.  THREAD_ARGP
     is never NULL.  *THREAD_ARGP will be passed to
     set_initial_registers or thread_detach callbacks together with
     Dwfl_Thread *thread.  This method may be NULL and will then be
     emulated using the next_thread callback. */
  bool (*get_thread) (Dwfl *dwfl, pid_t tid, void *dwfl_arg,
		      void **thread_argp)
    __nonnull_attribute__ (1);

  /* Called during unwinding to access memory (stack) state.  Returns true for
     successfully read *RESULT or false and sets dwfl_errno () on failure.
     This method may be NULL - in such case dwfl_thread_getframes will return
     only the initial frame.  */
  bool (*memory_read) (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Word *result,
                       void *dwfl_arg)
    __nonnull_attribute__ (1, 3);

  /* Called on initial unwind to get the initial register state of the first
     frame.  Should call dwfl_thread_state_registers, possibly multiple times
     for different ranges and possibly also dwfl_thread_state_register_pc, to
     fill in initial (DWARF) register values.  After this call, till at least
     thread_detach is called, the thread is assumed to be frozen, so that it is
     safe to unwind.  Returns true on success or false and sets dwfl_errno ()
     on failure.  In the case of a failure thread_detach will not be called.
     This method must not be NULL.  */
  bool (*set_initial_registers) (Dwfl_Thread *thread, void *thread_arg)
    __nonnull_attribute__ (1);

  /* Called by dwfl_end.  All thread_detach method calls have been already
     done.  This method may be NULL.  */
  void (*detach) (Dwfl *dwfl, void *dwfl_arg)
    __nonnull_attribute__ (1);

  /* Called when unwinding is done.  No callback will be called after
     this method has been called.  Iff set_initial_registers was called for
     a TID and it returned success thread_detach will be called before the
     detach method above.  This method may be NULL.  */
  void (*thread_detach) (Dwfl_Thread *thread, void *thread_arg)
    __nonnull_attribute__ (1);
} Dwfl_Thread_Callbacks;

/* PID is the process id associated with the DWFL state.  Architecture of DWFL
   modules is specified by ELF, ELF must remain valid during DWFL lifetime.
   Use NULL ELF to detect architecture from DWFL, the function will then detect
   it from arbitrary Dwfl_Module of DWFL.  DWFL_ARG is the callback backend
   state.  DWFL_ARG will be provided to the callbacks.  *THREAD_CALLBACKS
   function pointers must remain valid during lifetime of DWFL.  Function
   returns true on success, false otherwise.  */
bool dwfl_attach_state (Dwfl *dwfl, Elf *elf, pid_t pid,
                        const Dwfl_Thread_Callbacks *thread_callbacks,
			void *dwfl_arg)
  __nonnull_attribute__ (1, 4);

/* Calls dwfl_attach_state with Dwfl_Thread_Callbacks setup for extracting
   thread state from the ELF core file.  Returns the pid number extracted
   from the core file, or -1 for errors.  */
extern int dwfl_core_file_attach (Dwfl *dwfl, Elf *elf);

/* Calls dwfl_attach_state with Dwfl_Thread_Callbacks setup for extracting
   thread state from the proc file system.  Uses ptrace to attach and stop
   the thread under inspection and detaches when thread_detach is called
   and unwinding for the thread is done, unless ASSUME_PTRACE_STOPPED is
   true.  If ASSUME_PTRACE_STOPPED is true the caller should make sure that
   the thread is ptrace attached and stopped before unwinding by calling
   either dwfl_thread_getframes or dwfl_getthread_frames.  Returns zero on
   success, -1 if dwfl_attach_state failed, or an errno code if opening the
   proc files failed.  */
extern int dwfl_linux_proc_attach (Dwfl *dwfl, pid_t pid,
				   bool assume_ptrace_stopped);

/* Return PID for the process associated with DWFL.  Function returns -1 if
   dwfl_attach_state was not called for DWFL.  */
pid_t dwfl_pid (Dwfl *dwfl)
  __nonnull_attribute__ (1);

/* Return DWFL from which THREAD was created using dwfl_getthreads.  */
Dwfl *dwfl_thread_dwfl (Dwfl_Thread *thread)
  __nonnull_attribute__ (1);

/* Return positive TID (thread ID) for THREAD.  This function never fails.  */
pid_t dwfl_thread_tid (Dwfl_Thread *thread)
  __nonnull_attribute__ (1);

/* Return thread for frame STATE.  This function never fails.  */
Dwfl_Thread *dwfl_frame_thread (Dwfl_Frame *state)
  __nonnull_attribute__ (1);

/* Called by Dwfl_Thread_Callbacks.set_initial_registers implementation.
   For every known continuous block of registers <FIRSTREG..FIRSTREG+NREGS)
   (inclusive..exclusive) set their content to REGS (array of NREGS items).
   Function returns false if any of the registers has invalid number.  */
bool dwfl_thread_state_registers (Dwfl_Thread *thread, int firstreg,
                                  unsigned nregs, const Dwarf_Word *regs)
  __nonnull_attribute__ (1, 4);

/* Called by Dwfl_Thread_Callbacks.set_initial_registers implementation.
   If PC is not contained among DWARF registers passed by
   dwfl_thread_state_registers on the target architecture pass the PC value
   here.  */
void dwfl_thread_state_register_pc (Dwfl_Thread *thread, Dwarf_Word pc)
  __nonnull_attribute__ (1);

/* Iterate through the threads for a process.  Returns zero if all threads have
   been processed by the callback, returns -1 on error, or the value of the
   callback when not DWARF_CB_OK.  -1 returned on error will set dwfl_errno ().
   Keeps calling the callback with the next thread while the callback returns
   DWARF_CB_OK, till there are no more threads.  */
int dwfl_getthreads (Dwfl *dwfl,
		     int (*callback) (Dwfl_Thread *thread, void *arg),
		     void *arg)
  __nonnull_attribute__ (1, 2);

/* Iterate through the frames for a thread.  Returns zero if all frames
   have been processed by the callback, returns -1 on error, or the value of
   the callback when not DWARF_CB_OK.  -1 returned on error will
   set dwfl_errno ().  Some systems return error instead of zero on end of the
   backtrace, for cross-platform compatibility callers should consider error as
   a zero.  Keeps calling the callback with the next frame while the callback
   returns DWARF_CB_OK, till there are no more frames.  On start will call the
   set_initial_registers callback and on return will call the detach_thread
   callback of the Dwfl_Thread.  */
int dwfl_thread_getframes (Dwfl_Thread *thread,
			   int (*callback) (Dwfl_Frame *state, void *arg),
			   void *arg)
  __nonnull_attribute__ (1, 2);

/* Like dwfl_thread_getframes, but specifying the thread by its unique
   identifier number.  Returns zero if all frames have been processed
   by the callback, returns -1 on error (and when no thread with
   the given thread id number exists), or the value of the callback
   when not DWARF_CB_OK.  -1 returned on error will set dwfl_errno ().  */
int dwfl_getthread_frames (Dwfl *dwfl, pid_t tid,
			   int (*callback) (Dwfl_Frame *thread, void *arg),
			   void *arg)
  __nonnull_attribute__ (1, 3);

/* Return *PC (program counter) for thread-specific frame STATE.
   Set *ISACTIVATION according to DWARF frame "activation" definition.
   Typically you need to subtract 1 from *PC if *ACTIVATION is false to safely
   find function of the caller.  ACTIVATION may be NULL.  PC must not be NULL.
   Function returns false if it failed to find *PC.  */
bool dwfl_frame_pc (Dwfl_Frame *state, Dwarf_Addr *pc, bool *isactivation)
  __nonnull_attribute__ (1, 2);

/* Get the value of the DWARF register number in the given frame.
   Returns zero on success, -1 on error (invalid DWARF register
   number) or 1 if the value of the register in the frame is unknown.  */
int dwfl_frame_reg (Dwfl_Frame *state, unsigned regno, Dwarf_Word *val)
  __nonnull_attribute__ (1);

/* Return the internal debuginfod-client connection handle for the DWFL session.
   When the client connection has not yet been initialized, it will be done on the
   first call to this function. If elfutils is compiled without support for debuginfod,
   NULL will be returned.
 */
extern debuginfod_client *dwfl_get_debuginfod_client (Dwfl *dwfl);

#ifdef __cplusplus
}
#endif

#endif	/* libdwfl.h */
