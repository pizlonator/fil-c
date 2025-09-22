/* Return line number information of CU.
   Copyright (C) 2004-2010, 2013, 2014, 2015, 2016, 2018 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>

#include "dwarf.h"
#include "libdwP.h"


struct filelist
{
  Dwarf_Fileinfo info;
  struct filelist *next;
};

struct linelist
{
  Dwarf_Line line;
  struct linelist *next;
  size_t sequence;
};


/* Compare by Dwarf_Line.addr, given pointers into an array of pointers.  */
static int
compare_lines (const void *a, const void *b)
{
  struct linelist *const *p1 = a;
  struct linelist *const *p2 = b;
  struct linelist *list1 = *p1;
  struct linelist *list2 = *p2;
  Dwarf_Line *line1 = &list1->line;
  Dwarf_Line *line2 = &list2->line;

  if (line1->addr != line2->addr)
    return (line1->addr < line2->addr) ? -1 : 1;

  /* An end_sequence marker precedes a normal record at the same address.  */
  if (line1->end_sequence != line2->end_sequence)
    return line2->end_sequence - line1->end_sequence;

  /* Otherwise, the linelist sequence maintains a stable sort.  */
  return (list1->sequence < list2->sequence) ? -1
    : (list1->sequence > list2->sequence) ? 1
    : 0;
}

struct line_state
{
  Dwarf_Word addr;
  unsigned int op_index;
  unsigned int file;
  int64_t line;
  unsigned int column;
  uint_fast8_t is_stmt;
  bool basic_block;
  bool prologue_end;
  bool epilogue_begin;
  unsigned int isa;
  unsigned int discriminator;
  struct linelist *linelist;
  size_t nlinelist;
  unsigned int end_sequence;
  unsigned int context;
  unsigned int function_name;
};

static inline void
run_advance_pc (struct line_state *state, unsigned int op_advance,
                uint_fast8_t minimum_instr_len, uint_fast8_t max_ops_per_instr)
{
  state->addr += minimum_instr_len * ((state->op_index + op_advance)
				      / max_ops_per_instr);
  state->op_index = (state->op_index + op_advance) % max_ops_per_instr;
}

static inline bool
add_new_line (struct line_state *state, struct linelist *new_line)
{
  /* Set the line information.  For some fields we use bitfields,
     so we would lose information if the encoded values are too large.
     Check just for paranoia, and call the data "invalid" if it
     violates our assumptions on reasonable limits for the values.  */
  new_line->next = state->linelist;
  new_line->sequence = state->nlinelist;
  state->linelist = new_line;
  ++(state->nlinelist);

  /* Set the line information.  For some fields we use bitfields,
     so we would lose information if the encoded values are too large.
     Check just for paranoia, and call the data "invalid" if it
     violates our assumptions on reasonable limits for the values.  */
#define SET(field)						      \
  do {								      \
     new_line->line.field = state->field;			      \
     if (unlikely (new_line->line.field != state->field))	      \
       return true;						      \
   } while (0)

  /* Same as above, but don't flag as "invalid" just use truncated
     value.  Used for discriminator for which llvm might use a value
     that won't fit 24 bits.  */
#define SETX(field)						      \
     new_line->line.field = state->field;			      \

  SET (addr);
  SET (op_index);
  SET (file);
  SET (line);
  SET (column);
  SET (is_stmt);
  SET (basic_block);
  SET (end_sequence);
  SET (prologue_end);
  SET (epilogue_begin);
  SET (isa);
  SETX (discriminator);
  SET (context);
  SET (function_name);

#undef SET

  return false;
}

static int
read_srclines (Dwarf *dbg,
	       const unsigned char *linep, const unsigned char *lineendp,
	       const char *comp_dir, unsigned address_size,
	       Dwarf_Lines **linesp, Dwarf_Files **filesp)
{
  int res = -1;

  struct filelist *filelist = NULL;
  size_t nfilelist = 0;
  size_t ndirlist = 0;

  /* If there are a large number of lines, files or dirs don't blow up
     the stack.  Stack allocate some entries, only dynamically malloc
     when more than MAX.  */
#define MAX_STACK_ALLOC 4096
#define MAX_STACK_LINES (MAX_STACK_ALLOC / 2)
#define MAX_STACK_FILES (MAX_STACK_ALLOC / 4)
#define MAX_STACK_DIRS  (MAX_STACK_ALLOC / 16)

  /* Initial statement program state (except for stmt_list, see below).  */
  struct line_state state =
    {
      .linelist = NULL,
      .nlinelist = 0,
      .addr = 0,
      .op_index = 0,
      .file = 1,
      /* We only store int but want to check for overflow (see SET above).  */
      .line = 1,
      .column = 0,
      .basic_block = false,
      .prologue_end = false,
      .epilogue_begin = false,
      .isa = 0,
      .discriminator = 0,
      .context = 0,
      .function_name = 0
    };

  /* The dirs normally go on the stack, but if there are too many
     we alloc them all.  Set up stack storage early, so we can check on
     error if we need to free them or not.  */
  struct dirlist
  {
    const char *dir;
    size_t len;
  };
  struct dirlist dirstack[MAX_STACK_DIRS];
  struct dirlist *dirarray = dirstack;

  if (unlikely (linep + 4 > lineendp))
    {
    invalid_data:
      __libdw_seterrno (DWARF_E_INVALID_DEBUG_LINE);
      goto out;
    }

  Dwarf_Word unit_length = read_4ubyte_unaligned_inc (dbg, linep);
  unsigned int length = 4;
  if (unlikely (unit_length == DWARF3_LENGTH_64_BIT))
    {
      if (unlikely (linep + 8 > lineendp))
	goto invalid_data;
      unit_length = read_8ubyte_unaligned_inc (dbg, linep);
      length = 8;
    }

  /* Check whether we have enough room in the section.  */
  if (unlikely (unit_length > (size_t) (lineendp - linep)))
    goto invalid_data;
  lineendp = linep + unit_length;

  /* The next element of the header is the version identifier.  */
  if ((size_t) (lineendp - linep) < 2)
    goto invalid_data;
  uint_fast16_t version = read_2ubyte_unaligned_inc (dbg, linep);
  if (unlikely (version < 2) || unlikely (version > 5))
    {
      __libdw_seterrno (DWARF_E_VERSION);
      goto out;
    }

  /* DWARF5 explicitly lists address and segment_selector sizes.  */
  if (version >= 5)
    {
      if ((size_t) (lineendp - linep) < 2)
	goto invalid_data;
      size_t line_address_size = *linep++;
      size_t segment_selector_size = *linep++;
      if (line_address_size != address_size || segment_selector_size != 0)
	goto invalid_data;
    }

  /* Next comes the header length.  */
  Dwarf_Word header_length;
  if (length == 4)
    {
      if ((size_t) (lineendp - linep) < 4)
	goto invalid_data;
      header_length = read_4ubyte_unaligned_inc (dbg, linep);
    }
  else
    {
      if ((size_t) (lineendp - linep) < 8)
	goto invalid_data;
      header_length = read_8ubyte_unaligned_inc (dbg, linep);
    }
  const unsigned char *header_start = linep;

  /* Next the minimum instruction length.  */
  uint_fast8_t minimum_instr_len = *linep++;

  /* Next the maximum operations per instruction, in version 4 format.  */
  uint_fast8_t max_ops_per_instr = 1;
  if (version >= 4)
    {
      if (unlikely ((size_t) (lineendp - linep) < 1))
	goto invalid_data;
      max_ops_per_instr = *linep++;
      if (unlikely (max_ops_per_instr == 0))
	goto invalid_data;
    }

  /* 4 more bytes, is_stmt, line_base, line_range and opcode_base.  */
  if ((size_t) (lineendp - linep) < 4)
    goto invalid_data;

  /* Then the flag determining the default value of the is_stmt
     register.  */
  uint_fast8_t default_is_stmt = *linep++;

  /* Now the line base.  */
  int_fast8_t line_base = (int8_t) *linep++;

  /* And the line range.  */
  uint_fast8_t line_range = *linep++;

  /* The opcode base.  */
  uint_fast8_t opcode_base = *linep++;

  /* Remember array with the standard opcode length (-1 to account for
     the opcode with value zero not being mentioned).  */
  const uint8_t *standard_opcode_lengths = linep - 1;
  if (unlikely (lineendp - linep < opcode_base - 1))
    goto invalid_data;
  linep += opcode_base - 1;

  /* To read DWARF5 dir and file lists we need to know the forms.  For
     now we skip everything, except the DW_LNCT_path and
     DW_LNCT_directory_index.  */
  uint16_t forms[256];
  unsigned char nforms = 0;
  unsigned char form_path = -1; /* Which forms is DW_LNCT_path.  */
  unsigned char form_idx = -1;  /* And which is DW_LNCT_directory_index.  */

  /* To read/skip form data.  */
  Dwarf_CU fake_cu = {
    .dbg = dbg,
    .sec_idx = IDX_debug_line,
    .version = 5,
    .offset_size = length,
    .address_size = address_size,
    .startp = (void *) linep,
    .endp = (void *) lineendp,
  };

  /* First count the entries.  */
  size_t ndirs = 0;
  if (version < 5)
    {
      const unsigned char *dirp = linep;
      while (dirp < lineendp && *dirp != 0)
	{
	  uint8_t *endp = memchr (dirp, '\0', lineendp - dirp);
	  if (endp == NULL)
	    goto invalid_data;
	  ++ndirs;
	  dirp = endp + 1;
	}
      if (dirp >= lineendp || *dirp != '\0')
	goto invalid_data;
      ndirs = ndirs + 1; /* There is always the "unknown" dir.  */
    }
  else
    {
      if ((size_t) (lineendp - linep) < 1)
	goto invalid_data;
      nforms = *linep++;
      for (int i = 0; i < nforms; i++)
	{
	  uint16_t desc, form;
	  if ((size_t) (lineendp - linep) < 1)
	    goto invalid_data;
	  get_uleb128 (desc, linep, lineendp);
	  if ((size_t) (lineendp - linep) < 1)
	    goto invalid_data;
	  get_uleb128 (form, linep, lineendp);

	  if (! libdw_valid_user_form (form))
	    goto invalid_data;

	  forms[i] = form;
	  if (desc == DW_LNCT_path)
	    form_path = i;
	}

      if (nforms > 0 && form_path == (unsigned char) -1)
	goto invalid_data;

      if ((size_t) (lineendp - linep) < 1)
	goto invalid_data;
      get_uleb128 (ndirs, linep, lineendp);

      if (nforms == 0 && ndirs != 0)
	goto invalid_data;

      /* Assume there is at least 1 byte needed per form to describe
	 the directory.  Filters out insanely large ndirs.  */
      if (nforms != 0 && ndirs > (size_t) (lineendp - linep) / nforms)
	goto invalid_data;
    }

  /* Arrange the list in array form.  */
  ndirlist = ndirs;
  if (ndirlist >= MAX_STACK_DIRS)
    {
      if (ndirlist > SIZE_MAX / sizeof (*dirarray))
	goto no_mem;
      dirarray = malloc (ndirlist * sizeof (*dirarray));
      if (unlikely (dirarray == NULL))
	{
	no_mem:
	  __libdw_seterrno (DWARF_E_NOMEM);
	  goto out;
	}
    }

  /* Entry zero is implicit for older versions, but explicit for 5+.  */
  struct dirlist comp_dir_elem;
  if (version < 5)
    {
      /* First comes the list of directories.  Add the compilation
	 directory first since the index zero is used for it.  */
      comp_dir_elem.dir = comp_dir;
      comp_dir_elem.len = comp_dir ? strlen (comp_dir) : 0,
      dirarray[0] = comp_dir_elem;
      for (unsigned int n = 1; n < ndirlist; n++)
	{
	  dirarray[n].dir = (char *) linep;
	  uint8_t *endp = memchr (linep, '\0', lineendp - linep);
	  assert (endp != NULL); // Checked above when calculating ndirlist.
	  dirarray[n].len = endp - linep;
	  linep = endp + 1;
	}
      /* Skip the final NUL byte.  */
      assert (*linep == '\0'); // Checked above when calculating ndirlist.
      ++linep;
    }
  else
    {
      Dwarf_Attribute attr;
      attr.code = DW_AT_name;
      attr.cu = &fake_cu;
      for (unsigned int n = 0; n < ndirlist; n++)
	{
	  const char *dir = NULL;
	  for (unsigned char m = 0; m < nforms; m++)
	    {
	      if (m == form_path)
		{
		  attr.form = forms[m];
		  attr.valp = (void *) linep;
		  dir = dwarf_formstring (&attr);
		}

	      size_t len = __libdw_form_val_len (&fake_cu, forms[m], linep);
	      if ((size_t) (lineendp - linep) < len)
		goto invalid_data;

	      linep += len;
	    }

	  if (dir == NULL)
	    goto invalid_data;

	  dirarray[n].dir = dir;
	  dirarray[n].len = strlen (dir);
	}
    }

  /* File index zero doesn't exist for DWARF < 5.  Files are indexed
     starting from 1.  But for DWARF5 they are indexed starting from
     zero, but the default index is still 1.  In both cases the
     "first" file is special and refers to the main compile unit file,
     equal to the DW_AT_name of the DW_TAG_compile_unit.  */
  struct filelist null_file =
    {
      .info =
      {
	.name = "???",
	.mtime = 0,
	.length = 0
      },
      .next = NULL
    };
  filelist = &null_file;
  nfilelist = 1;

  /* Allocate memory for a new file.  For the first MAX_STACK_FILES
     entries just return a slot in the preallocated stack array.
     This is slightly complicated because in DWARF < 5 new files could
     be defined with DW_LNE_define_file after the normal file list was
     read.  */
  struct filelist flstack[MAX_STACK_FILES];
#define NEW_FILE() ({							\
  struct filelist *fl = (nfilelist < MAX_STACK_FILES			\
			   ? &flstack[nfilelist]			\
			   : malloc (sizeof (struct filelist)));	\
  if (unlikely (fl == NULL))						\
    goto no_mem;							\
  ++nfilelist;								\
  fl->next = filelist;							\
  filelist = fl;							\
  fl; })

  /* Now read the files.  */
  if (version < 5)
    {
      if (unlikely (linep >= lineendp))
	goto invalid_data;
      while (linep < lineendp && *linep != '\0')
	{
	  struct filelist *new_file = NEW_FILE ();

	  /* First comes the file name.  */
	  char *fname = (char *) linep;
	  uint8_t *endp = memchr (fname, '\0', lineendp - linep);
	  if (endp == NULL)
	    goto invalid_data;
	  size_t fnamelen = endp - (uint8_t *) fname;
	  linep = endp + 1;

	  /* Then the index.  */
	  Dwarf_Word diridx;
	  if (unlikely (linep >= lineendp))
	    goto invalid_data;
	  get_uleb128 (diridx, linep, lineendp);
	  if (unlikely (diridx >= ndirlist))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DIR_IDX);
	      goto out;
	    }

	  if (*fname == '/')
	    /* It's an absolute path.  */
	    new_file->info.name = fname;
	  else
	    {
	      new_file->info.name = libdw_alloc (dbg, char, 1,
						 dirarray[diridx].len + 1
						 + fnamelen + 1);
	      char *cp = new_file->info.name;

	      if (dirarray[diridx].dir != NULL)
		{
		  /* This value could be NULL in case the DW_AT_comp_dir
		     was not present.  We cannot do much in this case.
		     Just keep the file relative.  */
		  cp = stpcpy (cp, dirarray[diridx].dir);
		  *cp++ = '/';
		}
	      strcpy (cp, fname);
	      assert (strlen (new_file->info.name)
		      < dirarray[diridx].len + 1 + fnamelen + 1);
	    }

	  /* Next comes the modification time.  */
	  if (unlikely (linep >= lineendp))
	    goto invalid_data;
	  get_uleb128 (new_file->info.mtime, linep, lineendp);

	  /* Finally the length of the file.  */
	  if (unlikely (linep >= lineendp))
	    goto invalid_data;
	  get_uleb128 (new_file->info.length, linep, lineendp);
	}
      if (linep >= lineendp || *linep != '\0')
	goto invalid_data;
      /* Skip the final NUL byte.  */
      ++linep;
    }
  else
    {
      if ((size_t) (lineendp - linep) < 1)
	goto invalid_data;
      nforms = *linep++;
      form_path = form_idx = -1;
      for (int i = 0; i < nforms; i++)
	{
	  uint16_t desc, form;
	  if ((size_t) (lineendp - linep) < 1)
	    goto invalid_data;
	  get_uleb128 (desc, linep, lineendp);
	  if ((size_t) (lineendp - linep) < 1)
	    goto invalid_data;
	  get_uleb128 (form, linep, lineendp);

	  if (! libdw_valid_user_form (form))
	    goto invalid_data;

	  forms[i] = form;
	  if (desc == DW_LNCT_path)
	    form_path = i;
	  else if (desc == DW_LNCT_directory_index)
	    form_idx = i;
	}

      if (nforms > 0 && (form_path == (unsigned char) -1
			 || form_idx == (unsigned char) -1))
	goto invalid_data;

      size_t nfiles;
      if ((size_t) (lineendp - linep) < 1)
	goto invalid_data;
      get_uleb128 (nfiles, linep, lineendp);

      if (nforms == 0 && nfiles != 0)
	goto invalid_data;

      /* Assume there is at least 1 byte needed per form to describe
	 the file.  Filters out insanely large nfiles.  */
      if (nforms != 0 && nfiles > (size_t) (lineendp - linep) / nforms)
	goto invalid_data;

      Dwarf_Attribute attr;
      attr.cu = &fake_cu;
      for (unsigned int n = 0; n < nfiles; n++)
	{
	  const char *fname = NULL;
	  Dwarf_Word diridx = (Dwarf_Word) -1;
	  for (unsigned char m = 0; m < nforms; m++)
	    {
	      if (m == form_path)
		{
		  attr.code = DW_AT_name;
		  attr.form = forms[m];
		  attr.valp = (void *) linep;
		  fname = dwarf_formstring (&attr);
		}
	      else if (m == form_idx)
		{
		  attr.code = DW_AT_decl_file; /* Close enough.  */
		  attr.form = forms[m];
		  attr.valp = (void *) linep;
		  if (dwarf_formudata (&attr, &diridx) != 0)
		    diridx = (Dwarf_Word) -1;
		}

	      size_t len = __libdw_form_val_len (&fake_cu, forms[m], linep);
	      if ((size_t) (lineendp - linep) < len)
		goto invalid_data;

	      linep += len;
	    }

	  if (fname == NULL || diridx == (Dwarf_Word) -1)
	    goto invalid_data;

	  size_t fnamelen = strlen (fname);

	  if (unlikely (diridx >= ndirlist))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DIR_IDX);
	      goto out;
	    }

	  /* Yes, weird.  Looks like an off-by-one in the spec.  */
	  struct filelist *new_file = n == 0 ? &null_file : NEW_FILE ();

	  /* We follow the same rules as above for DWARF < 5, even
	     though the standard doesn't explicitly mention absolute
	     paths and ignoring the dir index.  */
	  if (*fname == '/')
	    /* It's an absolute path.  */
	    new_file->info.name = (char *) fname;
	  else
	    {
	      new_file->info.name = libdw_alloc (dbg, char, 1,
						 dirarray[diridx].len + 1
						 + fnamelen + 1);
	      char *cp = new_file->info.name;

	      /* In the DWARF >= 5 case, dir can never be NULL.  */
	      cp = stpcpy (cp, dirarray[diridx].dir);
	      *cp++ = '/';
	      strcpy (cp, fname);
	      assert (strlen (new_file->info.name)
		      < dirarray[diridx].len + 1 + fnamelen + 1);
	    }

	  /* For now we just ignore the modification time and file length.  */
	  new_file->info.mtime = 0;
	  new_file->info.length = 0;
	}
    }

  unsigned int debug_str_offset = 0;
  if (unlikely (linep == header_start + header_length - 4))
    {
      /* CUBINs contain an unsigned 4-byte offset */
      debug_str_offset = read_4ubyte_unaligned_inc (dbg, linep);
    }

  /* Consistency check.  */
  if (unlikely (linep != header_start + header_length))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      goto out;
    }

  /* We are about to process the statement program.  Most state machine
     registers have already been initialize above.  Just add the is_stmt
     default. See 6.2.2 in the v2.1 specification.  */
  state.is_stmt = default_is_stmt;

  /* Apply the "operation advance" from a special opcode or
     DW_LNS_advance_pc (as per DWARF4 6.2.5.1).  */
#define advance_pc(op_advance) \
  run_advance_pc (&state, op_advance, minimum_instr_len, max_ops_per_instr)

  /* Process the instructions.  */

  /* Adds a new line to the matrix.  For the first MAX_STACK_LINES
     entries just return a slot in the preallocated stack array.  */
  struct linelist llstack[MAX_STACK_LINES];
#define NEW_LINE(end_seq)						\
  do {								\
    struct linelist *ll = (state.nlinelist < MAX_STACK_LINES	\
			   ? &llstack[state.nlinelist]		\
			   : malloc (sizeof (struct linelist)));	\
    if (unlikely (ll == NULL))					\
      goto no_mem;						\
    state.end_sequence = end_seq;				\
    if (unlikely (add_new_line (&state, ll)))			\
      goto invalid_data;						\
  } while (0)

  while (linep < lineendp)
    {
      unsigned int opcode;
      unsigned int u128;
      int s128;

      /* Read the opcode.  */
      opcode = *linep++;

      /* Is this a special opcode?  */
      if (likely (opcode >= opcode_base))
	{
	  if (unlikely (line_range == 0))
	    goto invalid_data;

	  /* Yes.  Handling this is quite easy since the opcode value
	     is computed with

	     opcode = (desired line increment - line_base)
		       + (line_range * address advance) + opcode_base
	  */
	  int line_increment = (line_base
				+ (opcode - opcode_base) % line_range);

	  /* Perform the increments.  */
	  state.line += line_increment;
	  advance_pc ((opcode - opcode_base) / line_range);

	  /* Add a new line with the current state machine values.  */
	  NEW_LINE (0);

	  /* Reset the flags.  */
	  state.basic_block = false;
	  state.prologue_end = false;
	  state.epilogue_begin = false;
	  state.discriminator = 0;
	}
      else if (opcode == 0)
	{
	  /* This an extended opcode.  */
	  if (unlikely (lineendp - linep < 2))
	    goto invalid_data;

	  /* The length.  */
	  uint_fast8_t len = *linep++;

	  if (unlikely ((size_t) (lineendp - linep) < len))
	    goto invalid_data;

	  /* The sub-opcode.  */
	  opcode = *linep++;

	  switch (opcode)
	    {
	    case DW_LNE_end_sequence:
	      /* Add a new line with the current state machine values.
		 The is the end of the sequence.  */
	      NEW_LINE (1);

	      /* Reset the registers.  */
	      state.addr = 0;
	      state.op_index = 0;
	      state.file = 1;
	      state.line = 1;
	      state.column = 0;
	      state.is_stmt = default_is_stmt;
	      state.basic_block = false;
	      state.prologue_end = false;
	      state.epilogue_begin = false;
	      state.isa = 0;
	      state.discriminator = 0;
	      state.context = 0;
	      state.function_name = 0;
	      break;

	    case DW_LNE_set_address:
	      /* The value is an address.  The size is defined as
		 appropriate for the target machine.  We use the
		 address size field from the CU header.  */
	      state.op_index = 0;
	      if (unlikely (lineendp - linep < (uint8_t) address_size))
		goto invalid_data;
	      if (__libdw_read_address_inc (dbg, IDX_debug_line, &linep,
					    address_size, &state.addr))
		goto out;
	      break;

	    case DW_LNE_define_file:
	      {
		char *fname = (char *) linep;
		uint8_t *endp = memchr (linep, '\0', lineendp - linep);
		if (endp == NULL)
		  goto invalid_data;
		size_t fnamelen = endp - linep;
		linep = endp + 1;

		unsigned int diridx;
		if (unlikely (linep >= lineendp))
		  goto invalid_data;
		get_uleb128 (diridx, linep, lineendp);
		if (unlikely (diridx >= ndirlist))
		  {
		    __libdw_seterrno (DWARF_E_INVALID_DIR_IDX);
		    goto invalid_data;
		  }
		Dwarf_Word mtime;
		if (unlikely (linep >= lineendp))
		  goto invalid_data;
		get_uleb128 (mtime, linep, lineendp);
		Dwarf_Word filelength;
		if (unlikely (linep >= lineendp))
		  goto invalid_data;
		get_uleb128 (filelength, linep, lineendp);

		struct filelist *new_file = NEW_FILE ();
		if (fname[0] == '/')
		  new_file->info.name = fname;
		else
		  {
		    new_file->info.name =
		      libdw_alloc (dbg, char, 1, (dirarray[diridx].len + 1
						  + fnamelen + 1));
		    char *cp = new_file->info.name;

		    if (dirarray[diridx].dir != NULL)
		      /* This value could be NULL in case the
			 DW_AT_comp_dir was not present.  We
			 cannot do much in this case.  Just
			 keep the file relative.  */
		      {
			cp = stpcpy (cp, dirarray[diridx].dir);
			*cp++ = '/';
		      }
		    strcpy (cp, fname);
		  }

		new_file->info.mtime = mtime;
		new_file->info.length = filelength;
	      }
	      break;

	    case DW_LNE_set_discriminator:
	      /* Takes one ULEB128 parameter, the discriminator.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (state.discriminator, linep, lineendp);
	      break;

	    case DW_LNE_NVIDIA_inlined_call:
	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (state.context, linep, lineendp);
	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (state.function_name, linep, lineendp);
	      state.function_name += debug_str_offset;
	      break;

	    case DW_LNE_NVIDIA_set_function_name:
	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (state.function_name, linep, lineendp);
	      state.function_name += debug_str_offset;
	      break;

	    default:
	      /* Unknown, ignore it.  */
	      if (unlikely ((size_t) (lineendp - (linep - 1)) < len))
		goto invalid_data;
	      linep += len - 1;
	      break;
	    }
	}
      else if (opcode <= DW_LNS_set_isa)
	{
	  /* This is a known standard opcode.  */
	  switch (opcode)
	    {
	    case DW_LNS_copy:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      /* Add a new line with the current state machine values.  */
	      NEW_LINE (0);

	      /* Reset the flags.  */
	      state.basic_block = false;
	      state.prologue_end = false;
	      state.epilogue_begin = false;
	      state.discriminator = 0;
	      break;

	    case DW_LNS_advance_pc:
	      /* Takes one uleb128 parameter which is added to the
		 address.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (u128, linep, lineendp);
	      advance_pc (u128);
	      break;

	    case DW_LNS_advance_line:
	      /* Takes one sleb128 parameter which is added to the
		 line.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_sleb128 (s128, linep, lineendp);
	      state.line += s128;
	      break;

	    case DW_LNS_set_file:
	      /* Takes one uleb128 parameter which is stored in file.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (u128, linep, lineendp);
	      state.file = u128;
	      break;

	    case DW_LNS_set_column:
	      /* Takes one uleb128 parameter which is stored in column.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (u128, linep, lineendp);
	      state.column = u128;
	      break;

	    case DW_LNS_negate_stmt:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      state.is_stmt = 1 - state.is_stmt;
	      break;

	    case DW_LNS_set_basic_block:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      state.basic_block = true;
	      break;

	    case DW_LNS_const_add_pc:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      if (unlikely (line_range == 0))
		goto invalid_data;

	      advance_pc ((255 - opcode_base) / line_range);
	      break;

	    case DW_LNS_fixed_advance_pc:
	      /* Takes one 16 bit parameter which is added to the
		 address.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1)
		  || unlikely (lineendp - linep < 2))
		goto invalid_data;

	      state.addr += read_2ubyte_unaligned_inc (dbg, linep);
	      state.op_index = 0;
	      break;

	    case DW_LNS_set_prologue_end:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      state.prologue_end = true;
	      break;

	    case DW_LNS_set_epilogue_begin:
	      /* Takes no argument.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 0))
		goto invalid_data;

	      state.epilogue_begin = true;
	      break;

	    case DW_LNS_set_isa:
	      /* Takes one uleb128 parameter which is stored in isa.  */
	      if (unlikely (standard_opcode_lengths[opcode] != 1))
		goto invalid_data;

	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (state.isa, linep, lineendp);
	      break;
	    }
	}
      else
	{
	  /* This is a new opcode the generator but not we know about.
	     Read the parameters associated with it but then discard
	     everything.  Read all the parameters for this opcode.  */
	  for (int n = standard_opcode_lengths[opcode]; n > 0; --n)
	    {
	      if (unlikely (linep >= lineendp))
		goto invalid_data;
	      get_uleb128 (u128, linep, lineendp);
	    }

	  /* Next round, ignore this opcode.  */
	  continue;
	}
    }

  /* Put all the files in an array.  */
  Dwarf_Files *files = libdw_alloc (dbg, Dwarf_Files,
				    sizeof (Dwarf_Files)
				    + nfilelist * sizeof (Dwarf_Fileinfo)
				    + (ndirlist + 1) * sizeof (char *),
				    1);
  const char **dirs = (void *) &files->info[nfilelist];

  struct filelist *fileslist = filelist;
  files->nfiles = nfilelist;
  for (size_t n = nfilelist; n > 0; n--)
    {
      files->info[n - 1] = fileslist->info;
      fileslist = fileslist->next;
    }
  assert (fileslist == NULL);

  /* Put all the directory strings in an array.  */
  files->ndirs = ndirlist;
  for (unsigned int i = 0; i < ndirlist; ++i)
    dirs[i] = dirarray[i].dir;
  dirs[ndirlist] = NULL;

  /* Pass the file data structure to the caller.  */
  if (filesp != NULL)
    *filesp = files;

  size_t buf_size = (sizeof (Dwarf_Lines)
		     + (sizeof (Dwarf_Line) * state.nlinelist));
  void *buf = libdw_alloc (dbg, Dwarf_Lines, buf_size, 1);

  /* First use the buffer for the pointers, and sort the entries.
     We'll write the pointers in the end of the buffer, and then
     copy into the buffer from the beginning so the overlap works.  */
  assert (sizeof (Dwarf_Line) >= sizeof (struct linelist *));
  struct linelist **sortlines = (buf + buf_size
				 - sizeof (struct linelist **) * state.nlinelist);

  /* The list is in LIFO order and usually they come in clumps with
     ascending addresses.  So fill from the back to probably start with
     runs already in order before we sort.  */
  struct linelist *lineslist = state.linelist;
  for (size_t i = state.nlinelist; i-- > 0; )
    {
      sortlines[i] = lineslist;
      lineslist = lineslist->next;
    }
  assert (lineslist == NULL);

  /* Sort by ascending address.  */
  qsort (sortlines, state.nlinelist, sizeof sortlines[0], &compare_lines);

  /* Now that they are sorted, put them in the final array.
     The buffers overlap, so we've clobbered the early elements
     of SORTLINES by the time we're reading the later ones.  */
  Dwarf_Lines *lines = buf;
  lines->nlines = state.nlinelist;
  for (size_t i = 0; i < state.nlinelist; ++i)
    {
      lines->info[i] = sortlines[i]->line;
      lines->info[i].files = files;
    }

  /* Make sure the highest address for the CU is marked as end_sequence.
     This is required by the DWARF spec, but some compilers forget and
     dwfl_module_getsrc depends on it.  */
  if (state.nlinelist > 0)
    lines->info[state.nlinelist - 1].end_sequence = 1;

  /* Pass the line structure back to the caller.  */
  if (linesp != NULL)
    *linesp = lines;

  /* Success.  */
  res = 0;

 out:
  /* Free malloced line records, if any.  */
  for (size_t i = MAX_STACK_LINES; i < state.nlinelist; i++)
    {
      struct linelist *ll = state.linelist->next;
      free (state.linelist);
      state.linelist = ll;
    }
  if (dirarray != dirstack)
    free (dirarray);
  for (size_t i = MAX_STACK_FILES; i < nfilelist; i++)
    {
      struct filelist *fl = filelist->next;
      free (filelist);
      filelist = fl;
    }

  return res;
}

static int
files_lines_compare (const void *p1, const void *p2)
{
  const struct files_lines_s *t1 = p1;
  const struct files_lines_s *t2 = p2;

  if (t1->debug_line_offset < t2->debug_line_offset)
    return -1;
  if (t1->debug_line_offset > t2->debug_line_offset)
    return 1;

  return 0;
}

int
internal_function
__libdw_getsrclines (Dwarf *dbg, Dwarf_Off debug_line_offset,
		     const char *comp_dir, unsigned address_size,
		     Dwarf_Lines **linesp, Dwarf_Files **filesp)
{
  struct files_lines_s fake = { .debug_line_offset = debug_line_offset };
  struct files_lines_s **found = tfind (&fake, &dbg->files_lines,
					files_lines_compare);
  if (found == NULL)
    {
      Elf_Data *data = __libdw_checked_get_data (dbg, IDX_debug_line);
      if (data == NULL
	  || __libdw_offset_in_section (dbg, IDX_debug_line,
					debug_line_offset, 1) != 0)
	return -1;

      const unsigned char *linep = data->d_buf + debug_line_offset;
      const unsigned char *lineendp = data->d_buf + data->d_size;

      struct files_lines_s *node = libdw_alloc (dbg, struct files_lines_s,
						sizeof *node, 1);

      if (read_srclines (dbg, linep, lineendp, comp_dir, address_size,
			 &node->lines, &node->files) != 0)
	return -1;

      node->debug_line_offset = debug_line_offset;

      found = tsearch (node, &dbg->files_lines, files_lines_compare);
      if (found == NULL)
	{
	  __libdw_seterrno (DWARF_E_NOMEM);
	  return -1;
	}
    }

  if (linesp != NULL)
    *linesp = (*found)->lines;

  if (filesp != NULL)
    *filesp = (*found)->files;

  return 0;
}

/* Get the compilation directory, if any is set.  */
const char *
__libdw_getcompdir (Dwarf_Die *cudie)
{
  Dwarf_Attribute compdir_attr_mem;
  Dwarf_Attribute *compdir_attr = INTUSE(dwarf_attr) (cudie,
						      DW_AT_comp_dir,
						      &compdir_attr_mem);
  return INTUSE(dwarf_formstring) (compdir_attr);
}

int
dwarf_getsrclines (Dwarf_Die *cudie, Dwarf_Lines **lines, size_t *nlines)
{
  if (cudie == NULL)
    return -1;
  if (! is_cudie (cudie))
    {
      __libdw_seterrno (DWARF_E_NOT_CUDIE);
      return -1;
    }

  /* Get the information if it is not already known.  */
  struct Dwarf_CU *const cu = cudie->cu;
  if (cu->lines == NULL)
    {
      /* For split units always pick the lines from the skeleton.  */
      if (cu->unit_type == DW_UT_split_compile
	  || cu->unit_type == DW_UT_split_type)
	{
	  /* We tries, assume we fail...  */
	  cu->lines = (void *) -1l;

	  Dwarf_CU *skel = __libdw_find_split_unit (cu);
	  if (skel != NULL)
	    {
	      Dwarf_Die skeldie = CUDIE (skel);
	      int res = INTUSE(dwarf_getsrclines) (&skeldie, lines, nlines);
	      if (res == 0)
		{
		  cu->lines = skel->lines;
		  *lines = cu->lines;
		  *nlines = cu->lines->nlines;
		}
	      return res;
	    }

	  __libdw_seterrno (DWARF_E_NO_DEBUG_LINE);
	  return -1;
	}

      /* Failsafe mode: no data found.  */
      cu->lines = (void *) -1l;
      cu->files = (void *) -1l;

      /* The die must have a statement list associated.  */
      Dwarf_Attribute stmt_list_mem;
      Dwarf_Attribute *stmt_list = INTUSE(dwarf_attr) (cudie, DW_AT_stmt_list,
						       &stmt_list_mem);

      /* Get the offset into the .debug_line section.  NB: this call
	 also checks whether the previous dwarf_attr call failed.  */
      Dwarf_Off debug_line_offset;
      if (__libdw_formptr (stmt_list, IDX_debug_line, DWARF_E_NO_DEBUG_LINE,
			   NULL, &debug_line_offset) == NULL)
	return -1;

      if (__libdw_getsrclines (cu->dbg, debug_line_offset,
			       __libdw_getcompdir (cudie),
			       cu->address_size, &cu->lines, &cu->files) < 0)
	return -1;
    }
  else if (cu->lines == (void *) -1l)
    return -1;

  *lines = cu->lines;
  *nlines = cu->lines->nlines;

  // XXX Eventually: unlocking here.

  return 0;
}
INTDEF(dwarf_getsrclines)
