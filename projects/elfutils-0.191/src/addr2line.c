/* Locate source files and line information for given addresses
   Copyright (C) 2005-2010, 2012, 2013, 2015 Red Hat, Inc.
   Copyright (C) 2022, 2023 Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdwfl.h>
#include <dwarf.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <system.h>
#include <printversion.h>


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


/* Values for the parameters which have no short form.  */
#define OPT_DEMANGLER 0x100
#define OPT_PRETTY    0x101  /* 'p' is already used to select the process.  */
#define OPT_RELATIVE  0x102  /* 'r' is something else in binutils addr2line.  */

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Input format options:"), 2 },
  { "section", 'j', "NAME", 0,
    N_("Treat addresses as offsets relative to NAME section."), 0 },

  { NULL, 0, NULL, 0, N_("Output format options:"), 3 },
  { "addresses", 'a', NULL, 0, N_("Print address before each entry"), 0 },
  { "basenames", 's', NULL, 0, N_("Show only base names of source files"), 0 },
  { "absolute", 'A', NULL, 0,
    N_("Show absolute file names using compilation directory (default)"), 0 },
  { "functions", 'f', NULL, 0, N_("Also show function names"), 0 },
  { "symbols", 'S', NULL, 0, N_("Also show symbol or section names"), 0 },
  { "symbols-sections", 'x', NULL, 0, N_("Also show symbol and the section names"), 0 },
  { "flags", 'F', NULL, 0, N_("Also show line table flags"), 0 },
  { "inlines", 'i', NULL, 0,
    N_("Show all source locations that caused inline expansion of subroutines at the address."),
    0 },
  { "demangle", OPT_DEMANGLER, "ARG", OPTION_ARG_OPTIONAL,
    N_("Show demangled symbols (ARG is always ignored)"), 0 },
  { NULL, 'C', NULL, 0, N_("Show demangled symbols"), 0 },
  { "pretty-print", OPT_PRETTY, NULL, 0,
    N_("Print all information on one line, and indent inlines"), 0 },
  { "relative", OPT_RELATIVE, NULL, 0,
    N_("Show relative file names without compilation directory"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  /* Unsupported options.  */
  { "target", 'b', "ARG", OPTION_HIDDEN, NULL, 0 },
  { "demangler", OPT_DEMANGLER, "ARG", OPTION_HIDDEN, NULL, 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Locate source files and line information for ADDRs (in a.out by default).");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[ADDR...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp_child argp_children[2]; /* [0] is set in main.  */

/* Data structure to communicate with argp functions.  */
static const struct argp argp =
{
  options, parse_opt, args_doc, doc, argp_children, NULL, NULL
};


/* Handle ADDR.  */
static int handle_address (const char *addr, Dwfl *dwfl);

/* True when we should print the address for each entry.  */
static bool print_addresses;

/* True if only base names of files should be shown.  */
static bool only_basenames;

/* True if absolute file names based on DW_AT_comp_dir should be shown.  */
static bool use_comp_dir = true;

/* True if line flags should be shown.  */
static bool show_flags;

/* True if function names should be shown.  */
static bool show_functions;

/* True if ELF symbol or section info should be shown.  */
static bool show_symbols;

/* True if section associated with a symbol address should be shown.  */
static bool show_symbol_sections;

/* If non-null, take address parameters as relative to named section.  */
static const char *just_section;

/* True if all inlined subroutines of the current address should be shown.  */
static bool show_inlines;

/* True if all names need to be demangled.  */
static bool demangle;

/* True if all information should be printed on one line.  */
static bool pretty;

#ifdef USE_DEMANGLE
static size_t demangle_buffer_len = 0;
static char *demangle_buffer = NULL;
#endif

int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  This includes opening the modules.  */
  argp_children[0].argp = dwfl_standard_argp ();
  argp_children[0].group = 1;
  Dwfl *dwfl = NULL;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);

  /* Now handle the addresses.  In case none are given on the command
     line, read from stdin.  */
  if (remaining == argc)
    {
      /* We use no threads here which can interfere with handling a stream.  */
      (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);

      char *buf = NULL;
      size_t len = 0;
      ssize_t chars;
      while (!feof_unlocked (stdin))
	{
	  if ((chars = getline (&buf, &len, stdin)) < 0)
	    break;

	  if (buf[chars - 1] == '\n')
	    buf[chars - 1] = '\0';

	  result = handle_address (buf, dwfl);
	  fflush (stdout);
	}

      free (buf);
    }
  else
    {
      do
	result = handle_address (argv[remaining], dwfl);
      while (++remaining < argc);
    }

  dwfl_end (dwfl);

#ifdef USE_DEMANGLE
  free (demangle_buffer);
#endif

  return result;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = state->input;
      break;

    case 'a':
      print_addresses = true;
      break;

    /* Ignore --target=bfdname.  */
    case 'b':
      break;

    case 'C':
    case OPT_DEMANGLER:
      demangle = true;
      break;

    case 's':
      only_basenames = true;
      break;

    case 'A':
      use_comp_dir = true;
      break;

    case OPT_RELATIVE:
      use_comp_dir = false;
      break;

    case 'f':
      show_functions = true;
      break;

    case 'F':
      show_flags = true;
      break;

    case 'S':
      show_symbols = true;
      break;

    case 'x':
      show_symbols = true;
      show_symbol_sections = true;
      break;

    case 'j':
      just_section = arg;
      break;

    case 'i':
      show_inlines = true;
      break;

    case OPT_PRETTY:
      pretty = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const char *
symname (const char *name)
{
#ifdef USE_DEMANGLE
  // Require GNU v3 ABI by the "_Z" prefix.
  if (demangle && name[0] == '_' && name[1] == 'Z')
    {
      int status = -1;
      char *dsymname = __cxa_demangle (name, demangle_buffer,
				       &demangle_buffer_len, &status);
      if (status == 0)
	name = demangle_buffer = dsymname;
    }
#endif
  return name;
}

static const char *
get_diename (Dwarf_Die *die)
{
  Dwarf_Attribute attr;
  const char *name;

  name = dwarf_formstring (dwarf_attr_integrate (die, DW_AT_MIPS_linkage_name,
						 &attr)
			   ?: dwarf_attr_integrate (die, DW_AT_linkage_name,
						    &attr));

  if (name == NULL)
    name = dwarf_diename (die) ?: "??";

  return name;
}

static bool
print_dwarf_function (Dwfl_Module *mod, Dwarf_Addr addr)
{
  Dwarf_Addr bias = 0;
  Dwarf_Die *cudie = dwfl_module_addrdie (mod, addr, &bias);

  Dwarf_Die *scopes;
  int nscopes = dwarf_getscopes (cudie, addr - bias, &scopes);
  if (nscopes <= 0)
    return false;

  bool res = false;
  for (int i = 0; i < nscopes; ++i)
    switch (dwarf_tag (&scopes[i]))
      {
      case DW_TAG_subprogram:
	{
	  const char *name = get_diename (&scopes[i]);
	  if (name == NULL)
	    goto done;
	  printf ("%s%c", symname (name), pretty ? ' ' : '\n');
	  res = true;
	  goto done;
	}

      case DW_TAG_inlined_subroutine:
	{
	  const char *name = get_diename (&scopes[i]);
	  if (name == NULL)
	    goto done;

	  /* When using --pretty-print we only show inlines on their
	     own line.  Just print the first subroutine name.  */
	  if (pretty)
	    {
	      printf ("%s ", symname (name));
	      res = true;
	      goto done;
	    }
	  else
	    printf ("%s inlined", symname (name));

	  Dwarf_Files *files;
	  if (dwarf_getsrcfiles (cudie, &files, NULL) == 0)
	    {
	      Dwarf_Attribute attr_mem;
	      Dwarf_Word val;
	      if (dwarf_formudata (dwarf_attr (&scopes[i],
					       DW_AT_call_file,
					       &attr_mem), &val) == 0)
		{
		  const char *file = dwarf_filesrc (files, val, NULL, NULL);
		  unsigned int lineno = 0;
		  unsigned int colno = 0;
		  if (dwarf_formudata (dwarf_attr (&scopes[i],
						   DW_AT_call_line,
						   &attr_mem), &val) == 0)
		    lineno = val;
		  if (dwarf_formudata (dwarf_attr (&scopes[i],
						   DW_AT_call_column,
						   &attr_mem), &val) == 0)
		    colno = val;

		  const char *comp_dir = "";
		  const char *comp_dir_sep = "";

		  if (file == NULL)
		    file = "???";
		  else if (only_basenames)
		    file = xbasename (file);
		  else if (use_comp_dir && file[0] != '/')
		    {
		      const char *const *dirs;
		      size_t ndirs;
		      if (dwarf_getsrcdirs (files, &dirs, &ndirs) == 0
			  && dirs[0] != NULL)
			{
			  comp_dir = dirs[0];
			  comp_dir_sep = "/";
			}
		    }

		  if (lineno == 0)
		    printf (" from %s%s%s",
			    comp_dir, comp_dir_sep, file);
		  else if (colno == 0)
		    printf (" at %s%s%s:%u",
			    comp_dir, comp_dir_sep, file, lineno);
		  else
		    printf (" at %s%s%s:%u:%u",
			    comp_dir, comp_dir_sep, file, lineno, colno);
		}
	    }
	  printf (" in ");
	  continue;
	}
      }

done:
  free (scopes);
  return res;
}

static void
print_addrsym (Dwfl_Module *mod, GElf_Addr addr)
{
  GElf_Sym s;
  GElf_Off off;
  const char *name = dwfl_module_addrinfo (mod, addr, &off, &s,
					   NULL, NULL, NULL);
  if (name == NULL)
    {
      /* No symbol name.  Get a section name instead.  */
      int i = dwfl_module_relocate_address (mod, &addr);
      if (i >= 0)
	name = dwfl_module_relocation_info (mod, i, NULL);
      if (name == NULL)
	printf ("??%c", pretty ? ' ': '\n');
      else
	printf ("(%s)+%#" PRIx64 "%c", name, addr, pretty ? ' ' : '\n');
    }
  else
    {
      name = symname (name);
      if (off == 0)
	printf ("%s", name);
      else
	printf ("%s+%#" PRIx64 "", name, off);

      // Also show section name for address.
      if (show_symbol_sections)
	{
	  Dwarf_Addr ebias;
	  Elf_Scn *scn = dwfl_module_address_section (mod, &addr, &ebias);
	  if (scn != NULL)
	    {
	      GElf_Shdr shdr_mem;
	      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	      if (shdr != NULL)
		{
		  Elf *elf = dwfl_module_getelf (mod, &ebias);
		  size_t shstrndx;
		  if (elf_getshdrstrndx (elf, &shstrndx) >= 0)
		    printf (" (%s)", elf_strptr (elf, shstrndx,
						 shdr->sh_name));
		}
	    }
	}
      printf ("%c", pretty ? ' ' : '\n');
    }
}

static int
see_one_module (Dwfl_Module *mod,
		void **userdata __attribute__ ((unused)),
		const char *name __attribute__ ((unused)),
		Dwarf_Addr start __attribute__ ((unused)),
		void *arg)
{
  Dwfl_Module **result = arg;
  if (*result != NULL)
    return DWARF_CB_ABORT;
  *result = mod;
  return DWARF_CB_OK;
}

static int
find_symbol (Dwfl_Module *mod,
	     void **userdata __attribute__ ((unused)),
	     const char *name __attribute__ ((unused)),
	     Dwarf_Addr start __attribute__ ((unused)),
	     void *arg)
{
  const char *looking_for = ((void **) arg)[0];
  GElf_Sym *symbol = ((void **) arg)[1];
  GElf_Addr *value = ((void **) arg)[2];

  int n = dwfl_module_getsymtab (mod);
  for (int i = 1; i < n; ++i)
    {
      const char *symbol_name = dwfl_module_getsym_info (mod, i, symbol,
							 value, NULL, NULL,
							 NULL);
      if (symbol_name == NULL || symbol_name[0] == '\0')
	continue;
      switch (GELF_ST_TYPE (symbol->st_info))
	{
	case STT_SECTION:
	case STT_FILE:
	case STT_TLS:
	  break;
	default:
	  if (!strcmp (symbol_name, looking_for))
	    {
	      ((void **) arg)[0] = NULL;
	      return DWARF_CB_ABORT;
	    }
	}
    }

  return DWARF_CB_OK;
}

static bool
adjust_to_section (const char *name, uintmax_t *addr, Dwfl *dwfl)
{
  /* It was (section)+offset.  This makes sense if there is
     only one module to look in for a section.  */
  Dwfl_Module *mod = NULL;
  if (dwfl_getmodules (dwfl, &see_one_module, &mod, 0) != 0
      || mod == NULL)
    error_exit (0, _("Section syntax requires exactly one module"));

  int nscn = dwfl_module_relocations (mod);
  for (int i = 0; i < nscn; ++i)
    {
      GElf_Word shndx;
      const char *scn = dwfl_module_relocation_info (mod, i, &shndx);
      if (unlikely (scn == NULL))
	break;
      if (!strcmp (scn, name))
	{
	  /* Found the section.  */
	  GElf_Shdr shdr_mem;
	  GElf_Addr shdr_bias;
	  GElf_Shdr *shdr = gelf_getshdr
	    (elf_getscn (dwfl_module_getelf (mod, &shdr_bias), shndx),
	     &shdr_mem);
	  if (unlikely (shdr == NULL))
	    break;

	  if (*addr >= shdr->sh_size)
	    error (0, 0,
		   _("offset %#" PRIxMAX " lies outside"
			    " section '%s'"),
		   *addr, scn);

	  *addr += shdr->sh_addr + shdr_bias;
	  return true;
	}
    }

  return false;
}

static void
print_src (const char *src, int lineno, int linecol, Dwarf_Die *cu)
{
  const char *comp_dir = "";
  const char *comp_dir_sep = "";

  if (only_basenames)
    src = xbasename (src);
  else if (use_comp_dir && src[0] != '/')
    {
      Dwarf_Attribute attr;
      comp_dir = dwarf_formstring (dwarf_attr (cu, DW_AT_comp_dir, &attr));
      if (comp_dir != NULL)
	comp_dir_sep = "/";
    }

  if (linecol != 0)
    printf ("%s%s%s:%d:%d",
	    comp_dir, comp_dir_sep, src, lineno, linecol);
  else
    printf ("%s%s%s:%d",
	    comp_dir, comp_dir_sep, src, lineno);
}

static int
get_addr_width (Dwfl_Module *mod)
{
  // Try to find the address width if possible.
  static int width = 0;
  if (width == 0 && mod != NULL)
    {
      Dwarf_Addr bias;
      Elf *elf = dwfl_module_getelf (mod, &bias);
      if (elf != NULL)
        {
	  GElf_Ehdr ehdr_mem;
	  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
	  if (ehdr != NULL)
	    width = ehdr->e_ident[EI_CLASS] == ELFCLASS32 ? 8 : 16;
	}
    }
  if (width == 0)
    width = 16;

  return width;
}

static inline void
show_note (int (*get) (Dwarf_Line *, bool *),
	   Dwarf_Line *info,
	   const char *note)
{
  bool flag;
  if ((*get) (info, &flag) == 0 && flag)
    fputs (note, stdout);
}

static inline void
show_int (int (*get) (Dwarf_Line *, unsigned int *),
	  Dwarf_Line *info,
	  const char *name)
{
  unsigned int val;
  if ((*get) (info, &val) == 0 && val != 0)
    printf (" (%s %u)", name, val);
}

static int
handle_address (const char *string, Dwfl *dwfl)
{
  char *endp;
  uintmax_t addr = strtoumax (string, &endp, 16);
  if (endp == string || *endp != '\0')
    {
      bool parsed = false;
      int i, j;
      char *name = NULL;
      if (sscanf (string, "(%m[^)])%" PRIiMAX "%n", &name, &addr, &i) == 2
	  && string[i] == '\0')
	parsed = adjust_to_section (name, &addr, dwfl);
      switch (sscanf (string, "%m[^-+]%n%" PRIiMAX "%n", &name, &i, &addr, &j))
	{
	default:
	  break;
	case 1:
	  addr = 0;
	  j = i;
	  FALLTHROUGH;
	case 2:
	  if (string[j] != '\0')
	    break;

	  /* It was symbol[+offset].  */
	  GElf_Sym sym;
	  GElf_Addr value = 0;
	  void *arg[3] = { name, &sym, &value };
	  (void) dwfl_getmodules (dwfl, &find_symbol, arg, 0);
	  if (arg[0] != NULL)
	    error (0, 0, _("cannot find symbol '%s'"), name);
	  else
	    {
	      if (sym.st_size != 0 && addr >= sym.st_size)
		error (0, 0,
		       _("offset %#" PRIxMAX " lies outside"
				" contents of '%s'"),
		       addr, name);
	      addr += value;
	      parsed = true;
	    }
	  break;
	}

      free (name);
      if (!parsed)
	return 1;
    }
  else if (just_section != NULL
	   && !adjust_to_section (just_section, &addr, dwfl))
    return 1;

  Dwfl_Module *mod = dwfl_addrmodule (dwfl, addr);

  if (print_addresses)
    {
      int width = get_addr_width (mod);
      printf ("0x%.*" PRIx64 "%s", width, addr, pretty ? ": " : "\n");
    }

  if (show_functions)
    {
      /* First determine the function name.  Use the DWARF information if
	 possible.  */
      if (! print_dwarf_function (mod, addr) && !show_symbols)
	{
	  const char *name = dwfl_module_addrname (mod, addr);
	  name = name != NULL ? symname (name) : "??";
	  printf ("%s%c", name, pretty ? ' ' : '\n');
	}
    }

  if (show_symbols)
    print_addrsym (mod, addr);

  if ((show_functions || show_symbols) && pretty)
    printf ("at ");

  Dwfl_Line *line = dwfl_module_getsrc (mod, addr);

  const char *src;
  int lineno, linecol;

  if (line != NULL && (src = dwfl_lineinfo (line, &addr, &lineno, &linecol,
					    NULL, NULL)) != NULL)
    {
      print_src (src, lineno, linecol, dwfl_linecu (line));
      if (show_flags)
	{
	  Dwarf_Addr bias;
	  Dwarf_Line *info = dwfl_dwarf_line (line, &bias);
	  assert (info != NULL);

	  show_note (&dwarf_linebeginstatement, info, " (is_stmt)");
	  show_note (&dwarf_lineblock, info, " (basic_block)");
	  show_note (&dwarf_lineprologueend, info, " (prologue_end)");
	  show_note (&dwarf_lineepiloguebegin, info, " (epilogue_begin)");
	  show_int (&dwarf_lineisa, info, "isa");
	  show_int (&dwarf_linediscriminator, info, "discriminator");
	}
      putchar ('\n');
    }
  else
    puts ("??:0");

  if (show_inlines)
    {
      Dwarf_Addr bias = 0;
      Dwarf_Die *cudie = dwfl_module_addrdie (mod, addr, &bias);

      Dwarf_Die *scopes = NULL;
      int nscopes = dwarf_getscopes (cudie, addr - bias, &scopes);
      if (nscopes < 0)
	return 1;

      if (nscopes > 0)
	{
	  Dwarf_Die subroutine;
	  Dwarf_Off dieoff = dwarf_dieoffset (&scopes[0]);
	  dwarf_offdie (dwfl_module_getdwarf (mod, &bias),
			dieoff, &subroutine);
	  free (scopes);
	  scopes = NULL;

	  nscopes = dwarf_getscopes_die (&subroutine, &scopes);
	  if (nscopes > 1)
	    {
	      Dwarf_Die cu;
	      Dwarf_Files *files;
	      if (dwarf_diecu (&scopes[0], &cu, NULL, NULL) != NULL
		  && dwarf_getsrcfiles (cudie, &files, NULL) == 0)
		{
		  for (int i = 0; i < nscopes - 1; i++)
		    {
		      Dwarf_Word val;
		      Dwarf_Attribute attr;
		      Dwarf_Die *die = &scopes[i];
		      if (dwarf_tag (die) != DW_TAG_inlined_subroutine)
			continue;

		      if (pretty)
			printf (" (inlined by) ");

		      if (show_functions)
			{
			  /* Search for the parent inline or function.  It
			     might not be directly above this inline -- e.g.
			     there could be a lexical_block in between.  */
			  for (int j = i + 1; j < nscopes; j++)
			    {
			      Dwarf_Die *parent = &scopes[j];
			      int tag = dwarf_tag (parent);
			      if (tag == DW_TAG_inlined_subroutine
				  || tag == DW_TAG_entry_point
				  || tag == DW_TAG_subprogram)
				{
				  printf ("%s%s",
					  symname (get_diename (parent)),
					  pretty ? " at " : "\n");
				  break;
				}
			    }
			}

		      src = NULL;
		      lineno = 0;
		      linecol = 0;
		      if (dwarf_formudata (dwarf_attr (die, DW_AT_call_file,
						       &attr), &val) == 0)
			src = dwarf_filesrc (files, val, NULL, NULL);

		      if (dwarf_formudata (dwarf_attr (die, DW_AT_call_line,
						       &attr), &val) == 0)
			lineno = val;

		      if (dwarf_formudata (dwarf_attr (die, DW_AT_call_column,
						       &attr), &val) == 0)
			linecol = val;

		      if (src != NULL)
			{
			  print_src (src, lineno, linecol, &cu);
			  putchar ('\n');
			}
		      else
			puts ("??:0");
		    }
		}
	    }
	}
      free (scopes);
    }

  return 0;
}


#include "debugpred.h"
