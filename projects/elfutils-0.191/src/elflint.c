/* Pedantic checking of ELF files compliance with gABI/psABI spec.
   Copyright (C) 2001-2015, 2017, 2018 Red Hat, Inc.
   Copyright (C) 2023 Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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
#include <byteswap.h>
#include <endian.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <elf-knowledge.h>
#include <libeu.h>
#include <system.h>
#include <printversion.h>
#include "../libelf/libelfP.h"
#include "../libelf/common.h"
#include "../libebl/libeblP.h"
#include "../libdw/libdwP.h"
#include "../libdwfl/libdwflP.h"
#include "../libdw/memory-access.h"


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

#define ARGP_strict	300
#define ARGP_gnuld	301

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { "strict", ARGP_strict, NULL, 0,
    N_("Be extremely strict, flag level 2 features."), 0 },
  { "quiet", 'q', NULL, 0, N_("Do not print anything if successful"), 0 },
  { "debuginfo", 'd', NULL, 0, N_("Binary is a separate debuginfo file"), 0 },
  { "gnu-ld", ARGP_gnuld, NULL, 0,
    N_("Binary has been created with GNU ld and is therefore known to be \
broken in certain ways"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Pedantic checking of ELF files compliance with gABI/psABI spec.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE...");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};


/* Declarations of local functions.  */
static void process_file (int fd, Elf *elf, const char *prefix,
			  const char *suffix, const char *fname, size_t size,
			  bool only_one);
static void process_elf_file (Elf *elf, const char *prefix, const char *suffix,
			      const char *fname, size_t size, bool only_one);
static void check_note_section (Ebl *ebl, GElf_Ehdr *ehdr,
				GElf_Shdr *shdr, int idx);


/* Report an error.  */
#define ERROR(str, args...) \
  do {									      \
    printf (str, ##args);						      \
    ++error_count;							      \
  } while (0)
static unsigned int error_count;

/* True if we should perform very strict testing.  */
static bool be_strict;

/* True if no message is to be printed if the run is successful.  */
static bool be_quiet;

/* True if binary is from strip -f, not a normal ELF file.  */
static bool is_debuginfo;

/* True if binary is assumed to be generated with GNU ld.  */
static bool gnuld;

/* Index of section header string table.  */
static uint32_t shstrndx;

/* Array to count references in section groups.  */
static int *scnref;

/* Numbers of sections and program headers.  */
static unsigned int shnum;
static unsigned int phnum;


int
main (int argc, char *argv[])
{
  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Initialize the message catalog.  */
  textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Before we start tell the ELF library which version we are using.  */
  elf_version (EV_CURRENT);

  /* Now process all the files given at the command line.  */
  bool only_one = remaining + 1 == argc;
  do
    {
      /* Open the file.  */
      int fd = open (argv[remaining], O_RDONLY);
      if (fd == -1)
	{
	  error (0, errno, _("cannot open input file '%s'"), argv[remaining]);
	  continue;
	}

      /* Create an `Elf' descriptor.  */
      Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
      if (elf == NULL)
	ERROR (_("cannot generate Elf descriptor for '%s': %s\n"),
	       argv[remaining], elf_errmsg (-1));
      else
	{
	  unsigned int prev_error_count = error_count;
	  struct stat st;

	  if (fstat (fd, &st) != 0)
	    {
	      printf ("cannot stat '%s': %m\n", argv[remaining]);
	      close (fd);
	      continue;
	    }

	  process_file (fd, elf, NULL, NULL, argv[remaining], st.st_size,
			only_one);

	  /* Now we can close the descriptor.  */
	  if (elf_end (elf) != 0)
	    ERROR (_("error while closing Elf descriptor: %s\n"),
		   elf_errmsg (-1));

	  if (prev_error_count == error_count && !be_quiet)
	    puts (_("No errors"));
	}

      close (fd);
    }
  while (++remaining < argc);

  return error_count != 0;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg __attribute__ ((unused)),
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case ARGP_strict:
      be_strict = true;
      break;

    case 'q':
      be_quiet = true;
      break;

    case 'd':
      is_debuginfo = true;
      break;

    case ARGP_gnuld:
      gnuld = true;
      break;

    case ARGP_KEY_NO_ARGS:
      fputs (_("Missing file name.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE, program_invocation_short_name);
      exit (EXIT_FAILURE);

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


/* Process one file.  */
static void
process_file (int fd, Elf *elf, const char *prefix, const char *suffix,
	      const char *fname, size_t size, bool only_one)
{
  /* We can handle two types of files: ELF files and archives.  */
  Elf_Kind kind = elf_kind (elf);

  switch (kind)
    {
    case ELF_K_ELF:
      /* Yes!  It's an ELF file.  */
      process_elf_file (elf, prefix, suffix, fname, size, only_one);
      break;

    case ELF_K_AR:
      {
	Elf *subelf;
	Elf_Cmd cmd = ELF_C_READ_MMAP;
	size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
	size_t fname_len = strlen (fname) + 1;
	char new_prefix[prefix_len + 1 + fname_len];
	char new_suffix[(suffix == NULL ? 0 : strlen (suffix)) + 2];
	char *cp = new_prefix;

	/* Create the full name of the file.  */
	if (prefix != NULL)
	  {
	    cp = mempcpy (cp, prefix, prefix_len);
	    *cp++ = '(';
	    strcpy (stpcpy (new_suffix, suffix), ")");
	  }
	else
	  new_suffix[0] = '\0';
	memcpy (cp, fname, fname_len);

	/* It's an archive.  We process each file in it.  */
	while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
	  {
	    kind = elf_kind (subelf);

	    /* Call this function recursively.  */
	    if (kind == ELF_K_ELF || kind == ELF_K_AR)
	      {
		Elf_Arhdr *arhdr = elf_getarhdr (subelf);
		assert (arhdr != NULL);

		process_file (fd, subelf, new_prefix, new_suffix,
			      arhdr->ar_name, arhdr->ar_size, false);
	      }

	    /* Get next archive element.  */
	    cmd = elf_next (subelf);
	    if (elf_end (subelf) != 0)
	      ERROR (_(" error while freeing sub-ELF descriptor: %s\n"),
		     elf_errmsg (-1));
	  }
      }
      break;

    default:
      /* We cannot do anything.  */
      ERROR (_("\
Not an ELF file - it has the wrong magic bytes at the start\n"));
      break;
    }
}


static const char *
section_name (Ebl *ebl, int idx)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  const char *ret;

  if ((unsigned int) idx > shnum)
    return "<invalid>";

  shdr = gelf_getshdr (elf_getscn (ebl->elf, idx), &shdr_mem);
  if (shdr == NULL)
    return "<invalid>";

  ret = elf_strptr (ebl->elf, shstrndx, shdr->sh_name);
  if (ret == NULL)
    return "<invalid>";
  return ret;
}


static const int valid_e_machine[] =
  {
    EM_M32, EM_SPARC, EM_386, EM_68K, EM_88K, EM_860, EM_MIPS, EM_S370,
    EM_MIPS_RS3_LE, EM_PARISC, EM_VPP500, EM_SPARC32PLUS, EM_960, EM_PPC,
    EM_PPC64, EM_S390, EM_V800, EM_FR20, EM_RH32, EM_RCE, EM_ARM,
    EM_FAKE_ALPHA, EM_SH, EM_SPARCV9, EM_TRICORE, EM_ARC, EM_H8_300,
    EM_H8_300H, EM_H8S, EM_H8_500, EM_IA_64, EM_MIPS_X, EM_COLDFIRE,
    EM_68HC12, EM_MMA, EM_PCP, EM_NCPU, EM_NDR1, EM_STARCORE, EM_ME16,
    EM_ST100, EM_TINYJ, EM_X86_64, EM_PDSP, EM_FX66, EM_ST9PLUS, EM_ST7,
    EM_68HC16, EM_68HC11, EM_68HC08, EM_68HC05, EM_SVX, EM_ST19, EM_VAX,
    EM_CRIS, EM_JAVELIN, EM_FIREPATH, EM_ZSP, EM_MMIX, EM_HUANY, EM_PRISM,
    EM_AVR, EM_FR30, EM_D10V, EM_D30V, EM_V850, EM_M32R, EM_MN10300,
    EM_MN10200, EM_PJ, EM_OPENRISC, EM_ARC_A5, EM_XTENSA, EM_ALPHA,
    EM_TILEGX, EM_TILEPRO, EM_AARCH64, EM_BPF, EM_RISCV, EM_CSKY, EM_LOONGARCH,
    EM_ARCV2
  };
#define nvalid_e_machine \
  (sizeof (valid_e_machine) / sizeof (valid_e_machine[0]))


static void
check_elf_header (Ebl *ebl, GElf_Ehdr *ehdr, size_t size)
{
  char buf[512];
  size_t cnt;

  /* Check e_ident field.  */
  if (ehdr->e_ident[EI_MAG0] != ELFMAG0)
    ERROR ("e_ident[%d] != '%c'\n", EI_MAG0, ELFMAG0);
  if (ehdr->e_ident[EI_MAG1] != ELFMAG1)
    ERROR ("e_ident[%d] != '%c'\n", EI_MAG1, ELFMAG1);
  if (ehdr->e_ident[EI_MAG2] != ELFMAG2)
    ERROR ("e_ident[%d] != '%c'\n", EI_MAG2, ELFMAG2);
  if (ehdr->e_ident[EI_MAG3] != ELFMAG3)
    ERROR ("e_ident[%d] != '%c'\n", EI_MAG3, ELFMAG3);

  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32
      && ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    ERROR (_("e_ident[%d] == %d is no known class\n"),
	   EI_CLASS, ehdr->e_ident[EI_CLASS]);

  if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB
      && ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
    ERROR (_("e_ident[%d] == %d is no known data encoding\n"),
	   EI_DATA, ehdr->e_ident[EI_DATA]);

  if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
    ERROR (_("unknown ELF header version number e_ident[%d] == %d\n"),
	   EI_VERSION, ehdr->e_ident[EI_VERSION]);

  /* We currently don't handle any OS ABIs other than Linux and the
     kFreeBSD variant of Debian.  */
  if (ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE
      && ehdr->e_ident[EI_OSABI] != ELFOSABI_LINUX
      && ehdr->e_ident[EI_OSABI] != ELFOSABI_FREEBSD)
    ERROR (_("unsupported OS ABI e_ident[%d] == '%s'\n"),
	   EI_OSABI,
	   ebl_osabi_name (ebl, ehdr->e_ident[EI_OSABI], buf, sizeof (buf)));

  /* No ABI versions other than zero are supported either.  */
  if (ehdr->e_ident[EI_ABIVERSION] != 0)
    ERROR (_("unsupported ABI version e_ident[%d] == %d\n"),
	   EI_ABIVERSION, ehdr->e_ident[EI_ABIVERSION]);

  for (cnt = EI_PAD; cnt < EI_NIDENT; ++cnt)
    if (ehdr->e_ident[cnt] != 0)
      ERROR (_("e_ident[%zu] is not zero\n"), cnt);

  /* Check the e_type field.  */
  if (ehdr->e_type != ET_REL && ehdr->e_type != ET_EXEC
      && ehdr->e_type != ET_DYN && ehdr->e_type != ET_CORE)
    ERROR (_("unknown object file type %d\n"), ehdr->e_type);

  /* Check the e_machine field.  */
  for (cnt = 0; cnt < nvalid_e_machine; ++cnt)
    if (valid_e_machine[cnt] == ehdr->e_machine)
      break;
  if (cnt == nvalid_e_machine)
    ERROR (_("unknown machine type %d\n"), ehdr->e_machine);

  /* Check the e_version field.  */
  if (ehdr->e_version != EV_CURRENT)
    ERROR (_("unknown object file version\n"));

  /* Check the e_phoff and e_phnum fields.  */
  if (ehdr->e_phoff == 0)
    {
      if (ehdr->e_phnum != 0)
	ERROR (_("invalid program header offset\n"));
      else if (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)
	ERROR (_("\
executables and DSOs cannot have zero program header offset\n"));
    }
  else if (ehdr->e_phnum == 0)
    ERROR (_("invalid number of program header entries\n"));

  /* Check the e_shoff field.  */
  shnum = ehdr->e_shnum;
  shstrndx = ehdr->e_shstrndx;
  if (ehdr->e_shoff == 0)
    {
      if (ehdr->e_shnum != 0)
	ERROR (_("invalid section header table offset\n"));
      else if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN
	       && ehdr->e_type != ET_CORE)
	ERROR (_("section header table must be present\n"));
    }
  else
    {
      if (ehdr->e_shnum == 0)
	{
	  /* Get the header of the zeroth section.  The sh_size field
	     might contain the section number.  */
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
	  if (shdr != NULL)
	    {
	      /* The error will be reported later.  */
	      if (shdr->sh_size == 0)
		ERROR (_("\
invalid number of section header table entries\n"));
	      else
		shnum = shdr->sh_size;
	    }
	}

      if (ehdr->e_shstrndx == SHN_XINDEX)
	{
	  /* Get the header of the zeroth section.  The sh_size field
	     might contain the section number.  */
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
	  if (shdr != NULL && shdr->sh_link < shnum)
	    shstrndx = shdr->sh_link;
	}
      else if (shstrndx >= shnum)
	ERROR (_("invalid section header index\n"));
    }

  /* Check the shdrs actually exist.  And uncompress them before
     further checking.  Indexes between sections reference the
     uncompressed data.  */
  unsigned int scnt;
  Elf_Scn *scn = NULL;
  for (scnt = 1; scnt < shnum; ++scnt)
     {
	scn = elf_nextscn (ebl->elf, scn);
	if (scn == NULL)
	  break;
	/* If the section wasn't compressed this does nothing, but
	   returns an error.  We don't care.  */
	if (elf_compress (scn, 0, 0) < 0) { ; }
     }
  if (scnt < shnum)
    ERROR (_("Can only check %u headers, shnum was %u\n"), scnt, shnum);
  shnum = scnt;

  phnum = ehdr->e_phnum;
  if (ehdr->e_phnum == PN_XNUM)
    {
      /* Get the header of the zeroth section.  The sh_info field
	 might contain the phnum count.  */
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
      if (shdr != NULL)
	{
	  /* The error will be reported later.  */
	  if (shdr->sh_info < PN_XNUM)
	    ERROR (_("\
invalid number of program header table entries\n"));
	  else
	    phnum = shdr->sh_info;
	}
    }

  /* Check the phdrs actually exist. */
  unsigned int pcnt;
  for (pcnt = 0; pcnt < phnum; ++pcnt)
     {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr = gelf_getphdr (ebl->elf, pcnt, &phdr_mem);
	if (phdr == NULL)
	  break;
     }
  if (pcnt < phnum)
    ERROR (_("Can only check %u headers, phnum was %u\n"), pcnt, phnum);
  phnum = pcnt;

  /* Check the e_flags field.  */
  if (!ebl_machine_flag_check (ebl, ehdr->e_flags))
    ERROR (_("invalid machine flags: %s\n"),
	   ebl_machine_flag_name (ebl, ehdr->e_flags, buf, sizeof (buf)));

  /* Check e_ehsize, e_phentsize, and e_shentsize fields.  */
  if (gelf_getclass (ebl->elf) == ELFCLASS32)
    {
      if (ehdr->e_ehsize != 0 && ehdr->e_ehsize != sizeof (Elf32_Ehdr))
	ERROR (_("invalid ELF header size: %hd\n"), ehdr->e_ehsize);

      if (ehdr->e_phentsize != 0 && ehdr->e_phentsize != sizeof (Elf32_Phdr))
	ERROR (_("invalid program header size: %hd\n"),
	       ehdr->e_phentsize);
      else if (ehdr->e_phoff + phnum * ehdr->e_phentsize > size)
	ERROR (_("invalid program header position or size\n"));

      if (ehdr->e_shentsize != 0 && ehdr->e_shentsize != sizeof (Elf32_Shdr))
	ERROR (_("invalid section header size: %hd\n"),
	       ehdr->e_shentsize);
      else if (ehdr->e_shoff + shnum * ehdr->e_shentsize > size)
	ERROR (_("invalid section header position or size\n"));
    }
  else if (gelf_getclass (ebl->elf) == ELFCLASS64)
    {
      if (ehdr->e_ehsize != 0 && ehdr->e_ehsize != sizeof (Elf64_Ehdr))
	ERROR (_("invalid ELF header size: %hd\n"), ehdr->e_ehsize);

      if (ehdr->e_phentsize != 0 && ehdr->e_phentsize != sizeof (Elf64_Phdr))
	ERROR (_("invalid program header size: %hd\n"),
	       ehdr->e_phentsize);
      else if (ehdr->e_phoff + phnum * ehdr->e_phentsize > size)
	ERROR (_("invalid program header position or size\n"));

      if (ehdr->e_shentsize != 0 && ehdr->e_shentsize != sizeof (Elf64_Shdr))
	ERROR (_("invalid section header size: %hd\n"),
	       ehdr->e_shentsize);
      else if (ehdr->e_shoff + shnum * ehdr->e_shentsize > size)
	ERROR (_("invalid section header position or size\n"));
    }
}


/* Check that there is a section group section with index < IDX which
   contains section IDX and that there is exactly one.  */
static void
check_scn_group (Ebl *ebl, int idx)
{
  if (scnref[idx] == 0)
    {
      /* No reference so far.  Search following sections, maybe the
	 order is wrong.  */
      size_t cnt;

      for (cnt = idx + 1; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = elf_getscn (ebl->elf, cnt);
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    /* We cannot get the section header so we cannot check it.
	       The error to get the section header will be shown
	       somewhere else.  */
	    continue;

	  if (shdr->sh_type != SHT_GROUP)
	    continue;

	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL || data->d_buf == NULL
	      || data->d_size < sizeof (Elf32_Word))
	    /* Cannot check the section.  */
	    continue;

	  Elf32_Word *grpdata = (Elf32_Word *) data->d_buf;
	  for (size_t inner = 1; inner < data->d_size / sizeof (Elf32_Word);
	       ++inner)
	    if (grpdata[inner] == (Elf32_Word) idx)
	      goto out;
	}

    out:
      if (cnt == shnum)
	ERROR (_("\
section [%2d] '%s': section with SHF_GROUP flag set not part of a section group\n"),
	       idx, section_name (ebl, idx));
      else
	ERROR (_("\
section [%2d] '%s': section group [%2zu] '%s' does not precede group member\n"),
	       idx, section_name (ebl, idx),
	       cnt, section_name (ebl, cnt));
    }
}


static void
check_symtab (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  bool no_xndx_warned = false;
  int no_pt_tls = 0;
  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link),
				     &strshdr_mem);
  if (strshdr == NULL)
    return;

  if (strshdr->sh_type != SHT_STRTAB)
    {
      ERROR (_("section [%2d] '%s': referenced as string table for section [%2d] '%s' but type is not SHT_STRTAB\n"),
	     shdr->sh_link, section_name (ebl, shdr->sh_link),
	     idx, section_name (ebl, idx));
      strshdr = NULL;
    }

  /* Search for an extended section index table section.  */
  Elf_Data *xndxdata = NULL;
  Elf32_Word xndxscnidx = 0;
  bool found_xndx = false;
  for (size_t cnt = 1; cnt < shnum; ++cnt)
    if (cnt != (size_t) idx)
      {
	Elf_Scn *xndxscn = elf_getscn (ebl->elf, cnt);
	GElf_Shdr xndxshdr_mem;
	GElf_Shdr *xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);
	if (xndxshdr == NULL)
	  continue;

	if (xndxshdr->sh_type == SHT_SYMTAB_SHNDX
	    && xndxshdr->sh_link == (GElf_Word) idx)
	  {
	    if (found_xndx)
	      ERROR (_("\
section [%2d] '%s': symbol table cannot have more than one extended index section\n"),
		     idx, section_name (ebl, idx));

	    xndxdata = elf_getdata (xndxscn, NULL);
	    xndxscnidx = elf_ndxscn (xndxscn);
	    found_xndx = true;
	  }
      }

  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_SYM, 1, EV_CURRENT);
  if (shdr->sh_entsize != sh_entsize)
    ERROR (_("\
section [%2u] '%s': entry size is does not match ElfXX_Sym\n"),
	   idx, section_name (ebl, idx));
  else if (shdr->sh_info > shdr->sh_size / sh_entsize)
    ERROR (_("\
section [%2u] '%s': number of local entries in 'st_info' larger than table size\n"),
	   idx, section_name (ebl, idx));

  /* Test the zeroth entry.  */
  GElf_Sym sym_mem;
  Elf32_Word xndx;
  GElf_Sym *sym = gelf_getsymshndx (data, xndxdata, 0, &sym_mem, &xndx);
  if (sym == NULL)
      ERROR (_("section [%2d] '%s': cannot get symbol %d: %s\n"),
	     idx, section_name (ebl, idx), 0, elf_errmsg (-1));
  else
    {
      if (sym->st_name != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_name");
      if (sym->st_value != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_value");
      if (sym->st_size != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_size");
      if (sym->st_info != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_info");
      if (sym->st_other != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_other");
      if (sym->st_shndx != 0)
	ERROR (_("section [%2d] '%s': '%s' in zeroth entry not zero\n"),
	       idx, section_name (ebl, idx), "st_shndx");
      if (xndxdata != NULL && xndx != 0)
	ERROR (_("\
section [%2d] '%s': XINDEX for zeroth entry not zero\n"),
	       xndxscnidx, section_name (ebl, xndxscnidx));
    }

  for (size_t cnt = 1; cnt < shdr->sh_size / sh_entsize; ++cnt)
    {
      sym = gelf_getsymshndx (data, xndxdata, cnt, &sym_mem, &xndx);
      if (sym == NULL)
	{
	  ERROR (_("section [%2d] '%s': cannot get symbol %zu: %s\n"),
		 idx, section_name (ebl, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      const char *name = "<invalid>";
      if (strshdr == NULL)
	name = "";
      else if (sym->st_name >= strshdr->sh_size)
	ERROR (_("\
section [%2d] '%s': symbol %zu: invalid name value\n"),
	       idx, section_name (ebl, idx), cnt);
      else
	{
	  name = elf_strptr (ebl->elf, shdr->sh_link, sym->st_name);
	  if (name == NULL)
	    name = "";
	}

      if (sym->st_shndx == SHN_XINDEX)
	{
	  if (xndxdata == NULL)
	    {
	      if (!no_xndx_warned)
		ERROR (_("\
section [%2d] '%s': symbol %zu (%s): too large section index but no extended section index section\n"),
		       idx, section_name (ebl, idx), cnt, name);
	      no_xndx_warned = true;
	    }
	  else if (xndx < SHN_LORESERVE)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): XINDEX used for index which would fit in st_shndx (%" PRIu32 ")\n"),
		   xndxscnidx, section_name (ebl, xndxscnidx), cnt, name,
		   xndx);
	}
      else if ((sym->st_shndx >= SHN_LORESERVE
		// && sym->st_shndx <= SHN_HIRESERVE    always true
		&& sym->st_shndx != SHN_ABS
		&& sym->st_shndx != SHN_COMMON)
	       || (sym->st_shndx >= shnum
		   && (sym->st_shndx < SHN_LORESERVE
		       /* || sym->st_shndx > SHN_HIRESERVE  always false */)))
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): invalid section index\n"),
	       idx, section_name (ebl, idx), cnt, name);
      else
	xndx = sym->st_shndx;

      if (GELF_ST_TYPE (sym->st_info) >= STT_NUM
	  && !ebl_symbol_type_name (ebl, GELF_ST_TYPE (sym->st_info), NULL, 0))
	ERROR (_("section [%2d] '%s': symbol %zu (%s): unknown type\n"),
	       idx, section_name (ebl, idx), cnt, name);

      if (GELF_ST_BIND (sym->st_info) >= STB_NUM
	  && !ebl_symbol_binding_name (ebl, GELF_ST_BIND (sym->st_info), NULL,
				       0))
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): unknown symbol binding\n"),
	       idx, section_name (ebl, idx), cnt, name);
      if (GELF_ST_BIND (sym->st_info) == STB_GNU_UNIQUE
	  && GELF_ST_TYPE (sym->st_info) != STT_OBJECT)
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): unique symbol not of object type\n"),
	       idx, section_name (ebl, idx), cnt, name);

      if (xndx == SHN_COMMON)
	{
	  /* Common symbols can only appear in relocatable files.  */
	  if (ehdr->e_type != ET_REL)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): COMMON only allowed in relocatable files\n"),
		   idx, section_name (ebl, idx), cnt, name);
	  if (cnt < shdr->sh_info)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): local COMMON symbols are nonsense\n"),
		   idx, section_name (ebl, idx), cnt, name);
	  if (GELF_R_TYPE (sym->st_info) == STT_FUNC)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): function in COMMON section is nonsense\n"),
		   idx, section_name (ebl, idx), cnt, name);
	}
      else if (xndx > 0 && xndx < shnum)
	{
	  GElf_Shdr destshdr_mem;
	  GElf_Shdr *destshdr;

	  destshdr = gelf_getshdr (elf_getscn (ebl->elf, xndx), &destshdr_mem);
	  if (destshdr != NULL)
	    {
	      GElf_Addr sh_addr = (ehdr->e_type == ET_REL ? 0
				   : destshdr->sh_addr);
	      GElf_Addr st_value;
	      if (GELF_ST_TYPE (sym->st_info) == STT_FUNC
		  || (GELF_ST_TYPE (sym->st_info) == STT_GNU_IFUNC))
		st_value = sym->st_value & ebl_func_addr_mask (ebl);
	      else
		st_value = sym->st_value;
	      if (GELF_ST_TYPE (sym->st_info) != STT_TLS)
		{
		  if (! ebl_check_special_symbol (ebl, sym, name,
						  destshdr))
		    {
		      if (st_value - sh_addr > destshdr->sh_size)
			{
			  /* GNU ld has severe bugs.  When it decides to remove
			     empty sections it leaves symbols referencing them
			     behind.  These are symbols in .symtab or .dynsym
			     and for the named symbols have zero size.  See
			     sourceware PR13621.  */
			  if (!gnuld
			      || (strcmp (section_name (ebl, idx), ".symtab")
			          && strcmp (section_name (ebl, idx),
					     ".dynsym"))
			      || sym->st_size != 0
			      || (strcmp (name, "__preinit_array_start") != 0
				  && strcmp (name, "__preinit_array_end") != 0
				  && strcmp (name, "__init_array_start") != 0
				  && strcmp (name, "__init_array_end") != 0
				  && strcmp (name, "__fini_array_start") != 0
				  && strcmp (name, "__fini_array_end") != 0
				  && strcmp (name, "__bss_start") != 0
				  && strcmp (name, "__bss_start__") != 0
				  && strcmp (name, "__TMC_END__") != 0
				  && strcmp (name, ".TOC.") != 0
				  && strcmp (name, "_edata") != 0
				  && strcmp (name, "__edata") != 0
				  && strcmp (name, "_end") != 0
				  && strcmp (name, "__end") != 0))
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): st_value out of bounds\n"),
				   idx, section_name (ebl, idx), cnt, name);
			}
		      else if ((st_value - sh_addr
				+ sym->st_size) > destshdr->sh_size)
			ERROR (_("\
section [%2d] '%s': symbol %zu (%s) does not fit completely in referenced section [%2d] '%s'\n"),
			       idx, section_name (ebl, idx), cnt, name,
			       (int) xndx, section_name (ebl, xndx));
		    }
		}
	      else
		{
		  if ((destshdr->sh_flags & SHF_TLS) == 0)
		    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): referenced section [%2d] '%s' does not have SHF_TLS flag set\n"),
			   idx, section_name (ebl, idx), cnt, name,
			   (int) xndx, section_name (ebl, xndx));

		  if (ehdr->e_type == ET_REL)
		    {
		      /* For object files the symbol value must fall
			 into the section.  */
		      if (st_value > destshdr->sh_size)
			ERROR (_("\
section [%2d] '%s': symbol %zu (%s): st_value out of bounds of referenced section [%2d] '%s'\n"),
			       idx, section_name (ebl, idx), cnt, name,
			       (int) xndx, section_name (ebl, xndx));
		      else if (st_value + sym->st_size
			       > destshdr->sh_size)
			ERROR (_("\
section [%2d] '%s': symbol %zu (%s) does not fit completely in referenced section [%2d] '%s'\n"),
			       idx, section_name (ebl, idx), cnt, name,
			       (int) xndx, section_name (ebl, xndx));
		    }
		  else
		    {
		      GElf_Phdr phdr_mem;
		      GElf_Phdr *phdr = NULL;
		      unsigned int pcnt;

		      for (pcnt = 0; pcnt < phnum; ++pcnt)
			{
			  phdr = gelf_getphdr (ebl->elf, pcnt, &phdr_mem);
			  if (phdr != NULL && phdr->p_type == PT_TLS)
			    break;
			}

		      if (pcnt == phnum)
			{
			  if (no_pt_tls++ == 0)
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): TLS symbol but no TLS program header entry\n"),
				   idx, section_name (ebl, idx), cnt, name);
			}
		      else if (phdr == NULL)
			{
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): TLS symbol but couldn't get TLS program header entry\n"),
				   idx, section_name (ebl, idx), cnt, name);
			}
		      else if (!is_debuginfo)
			{
			  if (st_value
			      < destshdr->sh_offset - phdr->p_offset)
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): st_value short of referenced section [%2d] '%s'\n"),
				   idx, section_name (ebl, idx), cnt, name,
				   (int) xndx, section_name (ebl, xndx));
			  else if (st_value
				   > (destshdr->sh_offset - phdr->p_offset
				      + destshdr->sh_size))
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): st_value out of bounds of referenced section [%2d] '%s'\n"),
				   idx, section_name (ebl, idx), cnt, name,
				   (int) xndx, section_name (ebl, xndx));
			  else if (st_value + sym->st_size
				   > (destshdr->sh_offset - phdr->p_offset
				      + destshdr->sh_size))
			    ERROR (_("\
section [%2d] '%s': symbol %zu (%s) does not fit completely in referenced section [%2d] '%s'\n"),
				   idx, section_name (ebl, idx), cnt, name,
				   (int) xndx, section_name (ebl, xndx));
			}
		    }
		}
	    }
	}

      if (GELF_ST_BIND (sym->st_info) == STB_LOCAL)
	{
	  if (cnt >= shdr->sh_info)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): local symbol outside range described in sh_info\n"),
		   idx, section_name (ebl, idx), cnt, name);
	}
      else
	{
	  if (cnt < shdr->sh_info)
	    ERROR (_("\
section [%2d] '%s': symbol %zu (%s): non-local symbol outside range described in sh_info\n"),
		   idx, section_name (ebl, idx), cnt, name);
	}

      if (GELF_ST_TYPE (sym->st_info) == STT_SECTION
	  && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): non-local section symbol\n"),
	       idx, section_name (ebl, idx), cnt, name);

      if (name != NULL)
	{
	  if (strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
	    {
	      /* Check that address and size match the global offset table.  */

	      GElf_Shdr destshdr_mem;
	      GElf_Shdr *destshdr = gelf_getshdr (elf_getscn (ebl->elf, xndx),
						  &destshdr_mem);

	      if (destshdr == NULL && xndx == SHN_ABS)
		{
		  /* In a DSO, we have to find the GOT section by name.  */
		  Elf_Scn *gotscn = NULL;
		  Elf_Scn *gscn = NULL;
		  while ((gscn = elf_nextscn (ebl->elf, gscn)) != NULL)
		    {
		      destshdr = gelf_getshdr (gscn, &destshdr_mem);
		      assert (destshdr != NULL);
		      const char *sname = elf_strptr (ebl->elf,
						      shstrndx,
						      destshdr->sh_name);
		      if (sname != NULL)
			{
			  if (strcmp (sname, ".got.plt") == 0)
			    break;
			  if (strcmp (sname, ".got") == 0)
			    /* Do not stop looking.
			       There might be a .got.plt section.  */
			    gotscn = gscn;
			}

		      destshdr = NULL;
		    }

		  if (destshdr == NULL && gotscn != NULL)
		    destshdr = gelf_getshdr (gotscn, &destshdr_mem);
		}

	      const char *sname = ((destshdr == NULL || xndx == SHN_UNDEF)
				   ? NULL
				   : elf_strptr (ebl->elf, shstrndx,
						 destshdr->sh_name));
	      if (sname == NULL)
		{
		  if (xndx != SHN_UNDEF || ehdr->e_type != ET_REL)
		    ERROR (_("\
section [%2d] '%s': _GLOBAL_OFFSET_TABLE_ symbol refers to \
bad section [%2d]\n"),
			   idx, section_name (ebl, idx), xndx);
		}
	      else if (strcmp (sname, ".got.plt") != 0
		       && strcmp (sname, ".got") != 0)
		ERROR (_("\
section [%2d] '%s': _GLOBAL_OFFSET_TABLE_ symbol refers to \
section [%2d] '%s'\n"),
		       idx, section_name (ebl, idx), xndx, sname);

	      if (destshdr != NULL)
		{
		  /* Found it.  */
		  if (!ebl_check_special_symbol (ebl, sym, name,
						 destshdr))
		    {
		      if (ehdr->e_type != ET_REL
			  && sym->st_value != destshdr->sh_addr)
			/* This test is more strict than the psABIs which
			   usually allow the symbol to be in the middle of
			   the .got section, allowing negative offsets.  */
			ERROR (_("\
section [%2d] '%s': _GLOBAL_OFFSET_TABLE_ symbol value %#" PRIx64 " does not match %s section address %#" PRIx64 "\n"),
			       idx, section_name (ebl, idx),
			       (uint64_t) sym->st_value,
			       sname, (uint64_t) destshdr->sh_addr);

		      if (!gnuld && sym->st_size != destshdr->sh_size)
			ERROR (_("\
section [%2d] '%s': _GLOBAL_OFFSET_TABLE_ symbol size %" PRIu64 " does not match %s section size %" PRIu64 "\n"),
			       idx, section_name (ebl, idx),
			       (uint64_t) sym->st_size,
			       sname, (uint64_t) destshdr->sh_size);
		    }
		}
	      else
		ERROR (_("\
section [%2d] '%s': _GLOBAL_OFFSET_TABLE_ symbol present, but no .got section\n"),
		       idx, section_name (ebl, idx));
	    }
	  else if (strcmp (name, "_DYNAMIC") == 0)
	    /* Check that address and size match the dynamic section.
	       We locate the dynamic section via the program header
	       entry.  */
	    for (unsigned int pcnt = 0; pcnt < phnum; ++pcnt)
	      {
		GElf_Phdr phdr_mem;
		GElf_Phdr *phdr = gelf_getphdr (ebl->elf, pcnt, &phdr_mem);

		if (phdr != NULL && phdr->p_type == PT_DYNAMIC)
		  {
		    if (sym->st_value != phdr->p_vaddr)
		      ERROR (_("\
section [%2d] '%s': _DYNAMIC_ symbol value %#" PRIx64 " does not match dynamic segment address %#" PRIx64 "\n"),
			     idx, section_name (ebl, idx),
			     (uint64_t) sym->st_value,
			     (uint64_t) phdr->p_vaddr);

		    if (!gnuld && sym->st_size != phdr->p_memsz)
		      ERROR (_("\
section [%2d] '%s': _DYNAMIC symbol size %" PRIu64 " does not match dynamic segment size %" PRIu64 "\n"),
			     idx, section_name (ebl, idx),
			     (uint64_t) sym->st_size,
			     (uint64_t) phdr->p_memsz);

		    break;
		  }
	    }
	}

      if (GELF_ST_VISIBILITY (sym->st_other) != STV_DEFAULT
	  && shdr->sh_type == SHT_DYNSYM)
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): symbol in dynamic symbol table with non-default visibility\n"),
	       idx, section_name (ebl, idx), cnt, name);
      if (! ebl_check_st_other_bits (ebl, sym->st_other))
	ERROR (_("\
section [%2d] '%s': symbol %zu (%s): unknown bit set in st_other\n"),
	       idx, section_name (ebl, idx), cnt, name);

    }
}


static bool
is_rel_dyn (Ebl *ebl, const GElf_Ehdr *ehdr, int idx, const GElf_Shdr *shdr,
	    bool is_rela)
{
  /* If this is no executable or DSO it cannot be a .rel.dyn section.  */
  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    return false;

  /* Check the section name.  Unfortunately necessary.  */
  if (strcmp (section_name (ebl, idx), is_rela ? ".rela.dyn" : ".rel.dyn"))
    return false;

  /* When a .rel.dyn section is used a DT_RELCOUNT dynamic section
     entry can be present as well.  */
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr rcshdr_mem;
      const GElf_Shdr *rcshdr = gelf_getshdr (scn, &rcshdr_mem);

      if (rcshdr == NULL)
	break;

      if (rcshdr->sh_type == SHT_DYNAMIC && rcshdr->sh_entsize != 0)
	{
	  /* Found the dynamic section.  Look through it.  */
	  Elf_Data *d = elf_getdata (scn, NULL);
	  size_t cnt;

	  if (d == NULL)
	    ERROR (_("\
section [%2d] '%s': cannot get section data.\n"),
		   idx, section_name (ebl, idx));

	  for (cnt = 1; cnt < rcshdr->sh_size / rcshdr->sh_entsize; ++cnt)
	    {
	      GElf_Dyn dyn_mem;
	      GElf_Dyn *dyn = gelf_getdyn (d, cnt, &dyn_mem);

	      if (dyn == NULL)
		break;

	      if (dyn->d_tag == DT_RELCOUNT)
		{
		  /* Found it.  Does the type match.  */
		  if (is_rela)
		    ERROR (_("\
section [%2d] '%s': DT_RELCOUNT used for this RELA section\n"),
			   idx, section_name (ebl, idx));
		  else
		    {
		      /* Does the number specified number of relative
			 relocations exceed the total number of
			 relocations?  */
		      if (shdr->sh_entsize != 0
			  && dyn->d_un.d_val > (shdr->sh_size
						/ shdr->sh_entsize))
			ERROR (_("\
section [%2d] '%s': DT_RELCOUNT value %d too high for this section\n"),
			       idx, section_name (ebl, idx),
			       (int) dyn->d_un.d_val);

		      /* Make sure the specified number of relocations are
			 relative.  */
		      Elf_Data *reldata = elf_getdata (elf_getscn (ebl->elf,
								   idx), NULL);
		      if (reldata != NULL && shdr->sh_entsize != 0)
			for (size_t inner = 0;
			     inner < shdr->sh_size / shdr->sh_entsize;
			     ++inner)
			  {
			    GElf_Rel rel_mem;
			    GElf_Rel *rel = gelf_getrel (reldata, inner,
							 &rel_mem);
			    if (rel == NULL)
			      /* The problem will be reported elsewhere.  */
			      break;

			    if (ebl_relative_reloc_p (ebl,
						      GELF_R_TYPE (rel->r_info)))
			      {
				if (inner >= dyn->d_un.d_val)
				  ERROR (_("\
section [%2d] '%s': relative relocations after index %d as specified by DT_RELCOUNT\n"),
					 idx, section_name (ebl, idx),
					 (int) dyn->d_un.d_val);
			      }
			    else if (inner < dyn->d_un.d_val)
			      ERROR (_("\
section [%2d] '%s': non-relative relocation at index %zu; DT_RELCOUNT specified %d relative relocations\n"),
				     idx, section_name (ebl, idx),
				     inner, (int) dyn->d_un.d_val);
			  }
		    }
		}

	      if (dyn->d_tag == DT_RELACOUNT)
		{
		  /* Found it.  Does the type match.  */
		  if (!is_rela)
		    ERROR (_("\
section [%2d] '%s': DT_RELACOUNT used for this REL section\n"),
			   idx, section_name (ebl, idx));
		  else
		    {
		      /* Does the number specified number of relative
			 relocations exceed the total number of
			 relocations?  */
		      if (shdr->sh_entsize != 0
			  && dyn->d_un.d_val > shdr->sh_size / shdr->sh_entsize)
			ERROR (_("\
section [%2d] '%s': DT_RELCOUNT value %d too high for this section\n"),
			       idx, section_name (ebl, idx),
			       (int) dyn->d_un.d_val);

		      /* Make sure the specified number of relocations are
			 relative.  */
		      Elf_Data *reldata = elf_getdata (elf_getscn (ebl->elf,
								   idx), NULL);
		      if (reldata != NULL && shdr->sh_entsize != 0)
			for (size_t inner = 0;
			     inner < shdr->sh_size / shdr->sh_entsize;
			     ++inner)
			  {
			    GElf_Rela rela_mem;
			    GElf_Rela *rela = gelf_getrela (reldata, inner,
							    &rela_mem);
			    if (rela == NULL)
			      /* The problem will be reported elsewhere.  */
			      break;

			    if (ebl_relative_reloc_p (ebl,
						      GELF_R_TYPE (rela->r_info)))
			      {
				if (inner >= dyn->d_un.d_val)
				  ERROR (_("\
section [%2d] '%s': relative relocations after index %d as specified by DT_RELCOUNT\n"),
					 idx, section_name (ebl, idx),
					 (int) dyn->d_un.d_val);
			      }
			    else if (inner < dyn->d_un.d_val)
			      ERROR (_("\
section [%2d] '%s': non-relative relocation at index %zu; DT_RELCOUNT specified %d relative relocations\n"),
				     idx, section_name (ebl, idx),
				     inner, (int) dyn->d_un.d_val);
			  }
		    }
		}
	    }

	  break;
	}
    }

  return true;
}


struct loaded_segment
{
  GElf_Addr from;
  GElf_Addr to;
  bool read_only;
  struct loaded_segment *next;
};


/* Check whether binary has text relocation flag set.  */
static bool textrel;

/* Keep track of whether text relocation flag is needed.  */
static bool needed_textrel;


static bool
check_reloc_shdr (Ebl *ebl, const GElf_Ehdr *ehdr, const GElf_Shdr *shdr,
		  int idx, int reltype, GElf_Shdr **destshdrp,
		  GElf_Shdr *destshdr_memp, struct loaded_segment **loadedp)
{
  bool reldyn = false;

  /* Check whether the link to the section we relocate is reasonable.  */
  if (shdr->sh_info >= shnum)
    ERROR (_("section [%2d] '%s': invalid destination section index\n"),
	   idx, section_name (ebl, idx));
  else if (shdr->sh_info != 0)
    {
      *destshdrp = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_info),
				 destshdr_memp);
      if (*destshdrp != NULL)
	{
	  if(! ebl_check_reloc_target_type (ebl, (*destshdrp)->sh_type))
	    {
	      reldyn = is_rel_dyn (ebl, ehdr, idx, shdr, true);
	      if (!reldyn)
		ERROR (_("\
section [%2d] '%s': invalid destination section type\n"),
		       idx, section_name (ebl, idx));
	      else
		{
		  /* There is no standard, but we require that .rel{,a}.dyn
		     sections have a sh_info value of zero.  */
		  if (shdr->sh_info != 0)
		    ERROR (_("\
section [%2d] '%s': sh_info should be zero\n"),
			   idx, section_name (ebl, idx));
		}
	    }

	  if ((((*destshdrp)->sh_flags & SHF_MERGE) != 0)
	      && ((*destshdrp)->sh_flags & SHF_STRINGS) != 0)
	    ERROR (_("\
section [%2d] '%s': no relocations for merge-able string sections possible\n"),
		   idx, section_name (ebl, idx));
	}
    }

  size_t sh_entsize = gelf_fsize (ebl->elf, reltype, 1, EV_CURRENT);
  if (shdr->sh_entsize != sh_entsize)
    {
      if (reltype == ELF_T_RELA)
	ERROR ("\
section [%2d] '%s': section entry size does not match ElfXX_Rela\n",
	       idx, section_name (ebl, idx));
      else if (reltype == ELF_T_REL)
	ERROR ("\
section [%2d] '%s': section entry size does not match ElfXX_Rel\n",
	       idx, section_name (ebl, idx));
      else
	ERROR ("\
section [%2d] '%s': section entry size does not match ElfXX_Relr\n",
	       idx, section_name (ebl, idx));
    }

  /* In preparation of checking whether relocations are text
     relocations or not we need to determine whether the file is
     flagged to have text relocation and we need to determine a) what
     the loaded segments are and b) which are read-only.  This will
     also allow us to determine whether the same reloc section is
     modifying loaded and not loaded segments.  */
  for (unsigned int i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (ebl->elf, i, &phdr_mem);
      if (phdr == NULL)
	continue;

      if (phdr->p_type == PT_LOAD)
	{
	  struct loaded_segment *newp = xmalloc (sizeof (*newp));
	  newp->from = phdr->p_vaddr;
	  newp->to = phdr->p_vaddr + phdr->p_memsz;
	  newp->read_only = (phdr->p_flags & PF_W) == 0;
	  newp->next = *loadedp;
	  *loadedp = newp;
	}
      else if (phdr->p_type == PT_DYNAMIC)
	{
	  Elf_Scn *dynscn = gelf_offscn (ebl->elf, phdr->p_offset);
	  GElf_Shdr dynshdr_mem;
	  GElf_Shdr *dynshdr = gelf_getshdr (dynscn, &dynshdr_mem);
	  Elf_Data *dyndata = elf_getdata (dynscn, NULL);
	  if (dynshdr != NULL && dynshdr->sh_type == SHT_DYNAMIC
	      && dyndata != NULL && dynshdr->sh_entsize != 0)
	    for (size_t j = 0; j < dynshdr->sh_size / dynshdr->sh_entsize; ++j)
	      {
		GElf_Dyn dyn_mem;
		GElf_Dyn *dyn = gelf_getdyn (dyndata, j, &dyn_mem);
		if (dyn != NULL
		    && (dyn->d_tag == DT_TEXTREL
			|| (dyn->d_tag == DT_FLAGS
			    && (dyn->d_un.d_val & DF_TEXTREL) != 0)))
		  {
		    textrel = true;
		    break;
		  }
	      }
	}
    }

  /* A quick test which can be easily done here (although it is a bit
     out of place): the text relocation flag makes only sense if there
     is a segment which is not writable.  */
  if (textrel)
    {
      struct loaded_segment *seg = *loadedp;
      while (seg != NULL && !seg->read_only)
	seg = seg->next;
      if (seg == NULL)
	ERROR (_("\
text relocation flag set but there is no read-only segment\n"));
    }

  return reldyn;
}


enum load_state
  {
    state_undecided,
    state_loaded,
    state_unloaded,
    state_error
  };


static void
check_one_reloc (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *relshdr, int idx,
		 size_t cnt, const GElf_Shdr *symshdr, Elf_Data *symdata,
		 GElf_Addr r_offset, GElf_Xword r_info,
		 const GElf_Shdr *destshdr, bool reldyn,
		 struct loaded_segment *loaded, enum load_state *statep)
{
  bool known_broken = gnuld;

  if (!ebl_reloc_type_check (ebl, GELF_R_TYPE (r_info)))
    ERROR (_("section [%2d] '%s': relocation %zu: invalid type\n"),
	   idx, section_name (ebl, idx), cnt);
  else if (((ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
	    /* The executable/DSO can contain relocation sections with
	       all the relocations the linker has applied.  Those sections
	       are marked non-loaded, though.  */
	    || (relshdr->sh_flags & SHF_ALLOC) != 0)
	   && !ebl_reloc_valid_use (ebl, GELF_R_TYPE (r_info)))
    ERROR (_("\
section [%2d] '%s': relocation %zu: relocation type invalid for the file type\n"),
	   idx, section_name (ebl, idx), cnt);

  if (symshdr != NULL
      && ((GELF_R_SYM (r_info) + 1)
	  * gelf_fsize (ebl->elf, ELF_T_SYM, 1, EV_CURRENT)
	  > symshdr->sh_size))
    ERROR (_("\
section [%2d] '%s': relocation %zu: invalid symbol index\n"),
	   idx, section_name (ebl, idx), cnt);

  /* No more tests if this is a no-op relocation.  */
  if (ebl_none_reloc_p (ebl, GELF_R_TYPE (r_info)))
    return;

  if (ebl_gotpc_reloc_check (ebl, GELF_R_TYPE (r_info)))
    {
      const char *name;
      char buf[64];
      GElf_Sym sym_mem;
      GElf_Sym *sym = gelf_getsym (symdata, GELF_R_SYM (r_info), &sym_mem);
      if (sym != NULL
	  /* Get the name for the symbol.  */
	  && (name = elf_strptr (ebl->elf, symshdr->sh_link, sym->st_name))
	  && strcmp (name, "_GLOBAL_OFFSET_TABLE_") !=0 )
	ERROR (_("\
section [%2d] '%s': relocation %zu: only symbol '_GLOBAL_OFFSET_TABLE_' can be used with %s\n"),
	       idx, section_name (ebl, idx), cnt,
	       ebl_reloc_type_name (ebl, GELF_R_SYM (r_info),
				    buf, sizeof (buf)));
    }

  if (reldyn)
    {
      // XXX TODO Check .rel.dyn section addresses.
    }
  else if (!known_broken)
    {
      if (destshdr != NULL
	  && GELF_R_TYPE (r_info) != 0
	  && (r_offset - (ehdr->e_type == ET_REL ? 0
			  : destshdr->sh_addr)) >= destshdr->sh_size)
	ERROR (_("\
section [%2d] '%s': relocation %zu: offset out of bounds\n"),
	       idx, section_name (ebl, idx), cnt);
    }

  GElf_Sym sym_mem;
  GElf_Sym *sym = gelf_getsym (symdata, GELF_R_SYM (r_info), &sym_mem);

  if (ebl_copy_reloc_p (ebl, GELF_R_TYPE (r_info))
      /* Make sure the referenced symbol is an object or unspecified.  */
      && sym != NULL
      && GELF_ST_TYPE (sym->st_info) != STT_NOTYPE
      && GELF_ST_TYPE (sym->st_info) != STT_OBJECT)
    {
      char buf[64];
      ERROR (_("section [%2d] '%s': relocation %zu: copy relocation against symbol of type %s\n"),
	     idx, section_name (ebl, idx), cnt,
	     ebl_symbol_type_name (ebl, GELF_ST_TYPE (sym->st_info),
				   buf, sizeof (buf)));
    }

  if ((ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
      || (relshdr->sh_flags & SHF_ALLOC) != 0)
    {
      bool in_loaded_seg = false;
      while (loaded != NULL)
	{
	  if (r_offset < loaded->to
	      && r_offset + (sym == NULL ? 0 : sym->st_size) >= loaded->from)
	    {
	      /* The symbol is in this segment.  */
	      if  (loaded->read_only)
		{
		  if (textrel)
		    needed_textrel = true;
		  else
		    ERROR (_("section [%2d] '%s': relocation %zu: read-only section modified but text relocation flag not set\n"),
			   idx, section_name (ebl, idx), cnt);
		}

	      in_loaded_seg = true;
	    }

	  loaded = loaded->next;
	}

      if (*statep == state_undecided)
	*statep = in_loaded_seg ? state_loaded : state_unloaded;
      else if ((*statep == state_unloaded && in_loaded_seg)
	       || (*statep == state_loaded && !in_loaded_seg))
	{
	  ERROR (_("\
section [%2d] '%s': relocations are against loaded and unloaded data\n"),
		 idx, section_name (ebl, idx));
	  *statep = state_error;
	}
    }
}


static void
check_rela (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  /* Check the fields of the section header.  */
  GElf_Shdr destshdr_mem;
  GElf_Shdr *destshdr = NULL;
  struct loaded_segment *loaded = NULL;
  bool reldyn = check_reloc_shdr (ebl, ehdr, shdr, idx, ELF_T_RELA, &destshdr,
				  &destshdr_mem, &loaded);

  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  enum load_state state = state_undecided;

  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_RELA, 1, EV_CURRENT);
  for (size_t cnt = 0; cnt < shdr->sh_size / sh_entsize; ++cnt)
    {
      GElf_Rela rela_mem;
      GElf_Rela *rela = gelf_getrela (data, cnt, &rela_mem);
      if (rela == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': cannot get relocation %zu: %s\n"),
		 idx, section_name (ebl, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      check_one_reloc (ebl, ehdr, shdr, idx, cnt, symshdr, symdata,
		       rela->r_offset, rela->r_info, destshdr, reldyn, loaded,
		       &state);
    }

  while (loaded != NULL)
    {
      struct loaded_segment *old = loaded;
      loaded = loaded->next;
      free (old);
    }
}


static void
check_rel (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  /* Check the fields of the section header.  */
  GElf_Shdr destshdr_mem;
  GElf_Shdr *destshdr = NULL;
  struct loaded_segment *loaded = NULL;
  bool reldyn = check_reloc_shdr (ebl, ehdr, shdr, idx, ELF_T_REL, &destshdr,
				  &destshdr_mem, &loaded);

  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  enum load_state state = state_undecided;

  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_REL, 1, EV_CURRENT);
  for (size_t cnt = 0; cnt < shdr->sh_size / sh_entsize; ++cnt)
    {
      GElf_Rel rel_mem;
      GElf_Rel *rel = gelf_getrel (data, cnt, &rel_mem);
      if (rel == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': cannot get relocation %zu: %s\n"),
		 idx, section_name (ebl, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      check_one_reloc (ebl, ehdr, shdr, idx, cnt, symshdr, symdata,
		       rel->r_offset, rel->r_info, destshdr, reldyn, loaded,
		       &state);
    }

  while (loaded != NULL)
    {
      struct loaded_segment *old = loaded;
      loaded = loaded->next;
      free (old);
    }
}

static void
check_relr (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  /* Check the fields of the section header.  */
  GElf_Shdr destshdr_mem;
  GElf_Shdr *destshdr = NULL;
  struct loaded_segment *loaded = NULL;
  check_reloc_shdr (ebl, ehdr, shdr, idx, ELF_T_RELR, &destshdr,
		    &destshdr_mem, &loaded);

  /* Just throw them away.  */
  while (loaded != NULL)
    {
      struct loaded_segment *old = loaded;
      loaded = loaded->next;
      free (old);
    }
}

/* Number of dynamic sections.  */
static int ndynamic;


static void
check_dynamic (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  Elf_Data *data;
  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr;
  size_t cnt;
  static const bool dependencies[DT_NUM][DT_NUM] =
    {
      [DT_NEEDED] = { [DT_STRTAB] = true },
      [DT_PLTRELSZ] = { [DT_JMPREL] = true },
      [DT_HASH] = { [DT_SYMTAB] = true },
      [DT_STRTAB] = { [DT_STRSZ] = true },
      [DT_SYMTAB] = { [DT_STRTAB] = true, [DT_SYMENT] = true },
      [DT_RELA] = { [DT_RELASZ] = true, [DT_RELAENT] = true },
      [DT_RELASZ] = { [DT_RELA] = true },
      [DT_RELAENT] = { [DT_RELA] = true },
      [DT_STRSZ] = { [DT_STRTAB] = true },
      [DT_SYMENT] = { [DT_SYMTAB] = true },
      [DT_SONAME] = { [DT_STRTAB] = true },
      [DT_RPATH] = { [DT_STRTAB] = true },
      [DT_REL] = { [DT_RELSZ] = true, [DT_RELENT] = true },
      [DT_RELSZ] = { [DT_REL] = true },
      [DT_RELENT] = { [DT_REL] = true },
      [DT_JMPREL] = { [DT_PLTRELSZ] = true, [DT_PLTREL] = true },
      [DT_RUNPATH] = { [DT_STRTAB] = true },
      [DT_PLTREL] = { [DT_JMPREL] = true },
    };
  bool has_dt[DT_NUM];
  bool has_val_dt[DT_VALNUM];
  bool has_addr_dt[DT_ADDRNUM];
  static const bool level2[DT_NUM] =
    {
      [DT_RPATH] = true,
      [DT_SYMBOLIC] = true,
      [DT_TEXTREL] = true,
      [DT_BIND_NOW] = true
    };
  static const bool mandatory[DT_NUM] =
    {
      [DT_NULL] = true,
      [DT_STRTAB] = true,
      [DT_SYMTAB] = true,
      [DT_STRSZ] = true,
      [DT_SYMENT] = true
    };

  memset (has_dt, '\0', sizeof (has_dt));
  memset (has_val_dt, '\0', sizeof (has_val_dt));
  memset (has_addr_dt, '\0', sizeof (has_addr_dt));

  if (++ndynamic == 2)
    ERROR (_("more than one dynamic section present\n"));

  data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link), &strshdr_mem);
  if (strshdr != NULL && strshdr->sh_type != SHT_STRTAB)
    ERROR (_("\
section [%2d] '%s': referenced as string table for section [%2d] '%s' but type is not SHT_STRTAB\n"),
	   shdr->sh_link, section_name (ebl, shdr->sh_link),
	   idx, section_name (ebl, idx));
  else if (strshdr == NULL)
    {
      ERROR (_("\
section [%2d]: referenced as string table for section [%2d] '%s' but section link value is invalid\n"),
	   shdr->sh_link, idx, section_name (ebl, idx));
      return;
    }

  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_DYN, 1, EV_CURRENT);
  if (shdr->sh_entsize != sh_entsize)
    ERROR (_("\
section [%2d] '%s': section entry size does not match ElfXX_Dyn\n"),
	   idx, section_name (ebl, idx));

  if (shdr->sh_info != 0)
    ERROR (_("section [%2d] '%s': sh_info not zero\n"),
	   idx, section_name (ebl, idx));

  bool non_null_warned = false;
  for (cnt = 0; cnt < shdr->sh_size / sh_entsize; ++cnt)
    {
      GElf_Dyn dyn_mem;
      GElf_Dyn *dyn = gelf_getdyn (data, cnt, &dyn_mem);
      if (dyn == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': cannot get dynamic section entry %zu: %s\n"),
		 idx, section_name (ebl, idx), cnt, elf_errmsg (-1));
	  continue;
	}

      if (has_dt[DT_NULL] && dyn->d_tag != DT_NULL && ! non_null_warned)
	{
	  ERROR (_("\
section [%2d] '%s': non-DT_NULL entries follow DT_NULL entry\n"),
		 idx, section_name (ebl, idx));
	  non_null_warned = true;
	}

      if (!ebl_dynamic_tag_check (ebl, dyn->d_tag))
	ERROR (_("section [%2d] '%s': entry %zu: unknown tag\n"),
	       idx, section_name (ebl, idx), cnt);

      if (dyn->d_tag >= 0 && dyn->d_tag < DT_NUM)
	{
	  if (has_dt[dyn->d_tag]
	      && dyn->d_tag != DT_NEEDED
	      && dyn->d_tag != DT_NULL
	      && dyn->d_tag != DT_POSFLAG_1)
	    {
	      char buf[50];
	      ERROR (_("\
section [%2d] '%s': entry %zu: more than one entry with tag %s\n"),
		     idx, section_name (ebl, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag,
					   buf, sizeof (buf)));
	    }

	  if (be_strict && level2[dyn->d_tag])
	    {
	      char buf[50];
	      ERROR (_("\
section [%2d] '%s': entry %zu: level 2 tag %s used\n"),
		     idx, section_name (ebl, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag,
					   buf, sizeof (buf)));
	    }

	  has_dt[dyn->d_tag] = true;
	}
      else if (dyn->d_tag >= 0 && dyn->d_tag <= DT_VALRNGHI
	       && DT_VALTAGIDX (dyn->d_tag) < DT_VALNUM)
	has_val_dt[DT_VALTAGIDX (dyn->d_tag)] = true;
      else if (dyn->d_tag >= 0 && dyn->d_tag <= DT_ADDRRNGHI
	       && DT_ADDRTAGIDX (dyn->d_tag) < DT_ADDRNUM)
	has_addr_dt[DT_ADDRTAGIDX (dyn->d_tag)] = true;

      if (dyn->d_tag == DT_PLTREL && dyn->d_un.d_val != DT_REL
	  && dyn->d_un.d_val != DT_RELA)
	ERROR (_("\
section [%2d] '%s': entry %zu: DT_PLTREL value must be DT_REL or DT_RELA\n"),
	       idx, section_name (ebl, idx), cnt);

      /* Check that addresses for entries are in loaded segments.  */
      switch (dyn->d_tag)
	{
	  size_t n;
	case DT_STRTAB:
	  /* We require the referenced section is the same as the one
	     specified in sh_link.  */
	  if (strshdr->sh_addr != dyn->d_un.d_val)
	    {
	      ERROR (_("\
section [%2d] '%s': entry %zu: pointer does not match address of section [%2d] '%s' referenced by sh_link\n"),
		     idx, section_name (ebl, idx), cnt,
		     shdr->sh_link, section_name (ebl, shdr->sh_link));
	      break;
	    }
	  goto check_addr;

	default:
	  if (dyn->d_tag < DT_ADDRRNGLO || dyn->d_tag > DT_ADDRRNGHI)
	    /* Value is no pointer.  */
	    break;
	  FALLTHROUGH;

	case DT_AUXILIARY:
	case DT_FILTER:
	case DT_FINI:
	case DT_FINI_ARRAY:
	case DT_HASH:
	case DT_INIT:
	case DT_INIT_ARRAY:
	case DT_JMPREL:
	case DT_PLTGOT:
	case DT_REL:
	case DT_RELA:
	case DT_RELR:
	case DT_SYMBOLIC:
	case DT_SYMTAB:
	case DT_VERDEF:
	case DT_VERNEED:
	case DT_VERSYM:
	check_addr:
	  for (n = 0; n < phnum; ++n)
	    {
	      GElf_Phdr phdr_mem;
	      GElf_Phdr *phdr = gelf_getphdr (ebl->elf, n, &phdr_mem);
	      if (phdr != NULL && phdr->p_type == PT_LOAD
		  && phdr->p_vaddr <= dyn->d_un.d_ptr
		  && phdr->p_vaddr + phdr->p_memsz > dyn->d_un.d_ptr)
		break;
	    }
	  if (unlikely (n >= phnum))
	    {
	      char buf[50];
	      ERROR (_("\
section [%2d] '%s': entry %zu: %s value must point into loaded segment\n"),
		     idx, section_name (ebl, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag, buf,
					   sizeof (buf)));
	    }
	  break;

	case DT_NEEDED:
	case DT_RPATH:
	case DT_RUNPATH:
	case DT_SONAME:
	  if (dyn->d_un.d_ptr >= strshdr->sh_size)
	    {
	      char buf[50];
	      ERROR (_("\
section [%2d] '%s': entry %zu: %s value must be valid offset in section [%2d] '%s'\n"),
		     idx, section_name (ebl, idx), cnt,
		     ebl_dynamic_tag_name (ebl, dyn->d_tag, buf,
					   sizeof (buf)),
		     shdr->sh_link, section_name (ebl, shdr->sh_link));
	    }
	  break;
	}
    }

  for (cnt = 1; cnt < DT_NUM; ++cnt)
    if (has_dt[cnt])
      {
	for (int inner = 0; inner < DT_NUM; ++inner)
	  if (dependencies[cnt][inner] && ! has_dt[inner])
	    {
	      char buf1[50];
	      char buf2[50];

	      ERROR (_("\
section [%2d] '%s': contains %s entry but not %s\n"),
		     idx, section_name (ebl, idx),
		     ebl_dynamic_tag_name (ebl, cnt, buf1, sizeof (buf1)),
		     ebl_dynamic_tag_name (ebl, inner, buf2, sizeof (buf2)));
	    }
      }
    else
      {
	if (mandatory[cnt])
	  {
	    char buf[50];
	    ERROR (_("\
section [%2d] '%s': mandatory tag %s not present\n"),
		   idx, section_name (ebl, idx),
		   ebl_dynamic_tag_name (ebl, cnt, buf, sizeof (buf)));
	  }
      }

  /* Make sure we have an hash table.  */
  if (!has_dt[DT_HASH] && !has_addr_dt[DT_ADDRTAGIDX (DT_GNU_HASH)])
    ERROR (_("\
section [%2d] '%s': no hash section present\n"),
	   idx, section_name (ebl, idx));

  /* The GNU-style hash table also needs a symbol table.  */
  if (!has_dt[DT_HASH] && has_addr_dt[DT_ADDRTAGIDX (DT_GNU_HASH)]
      && !has_dt[DT_SYMTAB])
    ERROR (_("\
section [%2d] '%s': contains %s entry but not %s\n"),
	   idx, section_name (ebl, idx),
	   "DT_GNU_HASH", "DT_SYMTAB");

  /* Check the rel/rela tags.  At least one group must be available.  */
  if ((has_dt[DT_RELA] || has_dt[DT_RELASZ] || has_dt[DT_RELAENT])
      && (!has_dt[DT_RELA] || !has_dt[DT_RELASZ] || !has_dt[DT_RELAENT]))
    ERROR (_("\
section [%2d] '%s': not all of %s, %s, and %s are present\n"),
	   idx, section_name (ebl, idx),
	   "DT_RELA", "DT_RELASZ", "DT_RELAENT");

  if ((has_dt[DT_REL] || has_dt[DT_RELSZ] || has_dt[DT_RELENT])
      && (!has_dt[DT_REL] || !has_dt[DT_RELSZ] || !has_dt[DT_RELENT]))
    ERROR (_("\
section [%2d] '%s': not all of %s, %s, and %s are present\n"),
	   idx, section_name (ebl, idx),
	   "DT_REL", "DT_RELSZ", "DT_RELENT");

  /* Check that all prelink sections are present if any of them is.  */
  if (has_val_dt[DT_VALTAGIDX (DT_GNU_PRELINKED)]
      || has_val_dt[DT_VALTAGIDX (DT_CHECKSUM)])
    {
      if (!has_val_dt[DT_VALTAGIDX (DT_GNU_PRELINKED)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in DSO marked during prelinking\n"),
	       idx, section_name (ebl, idx), "DT_GNU_PRELINKED");
      if (!has_val_dt[DT_VALTAGIDX (DT_CHECKSUM)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in DSO marked during prelinking\n"),
	       idx, section_name (ebl, idx), "DT_CHECKSUM");

      /* Only DSOs can be marked like this.  */
      if (ehdr->e_type != ET_DYN)
	ERROR (_("\
section [%2d] '%s': non-DSO file marked as dependency during prelink\n"),
	       idx, section_name (ebl, idx));
    }

  if (has_val_dt[DT_VALTAGIDX (DT_GNU_CONFLICTSZ)]
      || has_val_dt[DT_VALTAGIDX (DT_GNU_LIBLISTSZ)]
      || has_addr_dt[DT_ADDRTAGIDX (DT_GNU_CONFLICT)]
      || has_addr_dt[DT_ADDRTAGIDX (DT_GNU_LIBLIST)])
    {
      if (!has_val_dt[DT_VALTAGIDX (DT_GNU_CONFLICTSZ)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in prelinked executable\n"),
	       idx, section_name (ebl, idx), "DT_GNU_CONFLICTSZ");
      if (!has_val_dt[DT_VALTAGIDX (DT_GNU_LIBLISTSZ)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in prelinked executable\n"),
	       idx, section_name (ebl, idx), "DT_GNU_LIBLISTSZ");
      if (!has_addr_dt[DT_ADDRTAGIDX (DT_GNU_CONFLICT)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in prelinked executable\n"),
	       idx, section_name (ebl, idx), "DT_GNU_CONFLICT");
      if (!has_addr_dt[DT_ADDRTAGIDX (DT_GNU_LIBLIST)])
	ERROR (_("\
section [%2d] '%s': %s tag missing in prelinked executable\n"),
	       idx, section_name (ebl, idx), "DT_GNU_LIBLIST");
    }
}


static void
check_symtab_shndx (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  if (ehdr->e_type != ET_REL)
    {
      ERROR (_("\
section [%2d] '%s': only relocatable files can have extended section index\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  if (symshdr != NULL && symshdr->sh_type != SHT_SYMTAB)
    ERROR (_("\
section [%2d] '%s': extended section index section not for symbol table\n"),
	   idx, section_name (ebl, idx));
  else if (symshdr == NULL)
    ERROR (_("\
section [%2d] '%s': sh_link extended section index [%2d] is invalid\n"),
	   idx, section_name (ebl, idx), shdr->sh_link);
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  if (symdata == NULL)
    ERROR (_("cannot get data for symbol section\n"));

  if (shdr->sh_entsize != sizeof (Elf32_Word))
    ERROR (_("\
section [%2d] '%s': entry size does not match Elf32_Word\n"),
	   idx, section_name (ebl, idx));

  if (symshdr != NULL
      && shdr->sh_entsize != 0
      && symshdr->sh_entsize != 0
      && (shdr->sh_size / shdr->sh_entsize
	  < symshdr->sh_size / symshdr->sh_entsize))
    ERROR (_("\
section [%2d] '%s': extended index table too small for symbol table\n"),
	   idx, section_name (ebl, idx));

  if (shdr->sh_info != 0)
    ERROR (_("section [%2d] '%s': sh_info not zero\n"),
	   idx, section_name (ebl, idx));

  for (size_t cnt = idx + 1; cnt < shnum; ++cnt)
    {
      GElf_Shdr rshdr_mem;
      GElf_Shdr *rshdr = gelf_getshdr (elf_getscn (ebl->elf, cnt), &rshdr_mem);
      if (rshdr != NULL && rshdr->sh_type == SHT_SYMTAB_SHNDX
	  && rshdr->sh_link == shdr->sh_link)
	{
	  ERROR (_("\
section [%2d] '%s': extended section index in section [%2zu] '%s' refers to same symbol table\n"),
		 idx, section_name (ebl, idx),
		 cnt, section_name (ebl, cnt));
	  break;
	}
    }

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL || data->d_buf == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  if (data->d_size < sizeof (Elf32_Word)
      || *((Elf32_Word *) data->d_buf) != 0)
    ERROR (_("symbol 0 should have zero extended section index\n"));

  for (size_t cnt = 1; cnt < data->d_size / sizeof (Elf32_Word); ++cnt)
    {
      Elf32_Word xndx = ((Elf32_Word *) data->d_buf)[cnt];

      if (xndx != 0)
	{
	  GElf_Sym sym_data;
	  GElf_Sym *sym = gelf_getsym (symdata, cnt, &sym_data);
	  if (sym == NULL)
	    {
	      ERROR (_("cannot get data for symbol %zu\n"), cnt);
	      continue;
	    }

	  if (sym->st_shndx != SHN_XINDEX)
	    ERROR (_("\
extended section index is %" PRIu32 " but symbol index is not XINDEX\n"),
		   (uint32_t) xndx);
	}
    }
}


static void
check_sysv_hash (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *data, int idx,
		 GElf_Shdr *symshdr)
{
  Elf32_Word nbucket = ((Elf32_Word *) data->d_buf)[0];
  Elf32_Word nchain = ((Elf32_Word *) data->d_buf)[1];

  if (shdr->sh_size < (2ULL + nbucket + nchain) * sizeof (Elf32_Word))
    {
      ERROR (_("\
section [%2d] '%s': hash table section is too small (is %ld, expected %ld)\n"),
	     idx, section_name (ebl, idx), (long int) shdr->sh_size,
	     (long int) ((2 + nbucket + nchain) * sizeof (Elf32_Word)));
      return;
    }

  size_t maxidx = nchain;

  if (symshdr != NULL && symshdr->sh_entsize != 0)
    {
      size_t symsize = symshdr->sh_size / symshdr->sh_entsize;

      if (nchain > symshdr->sh_size / symshdr->sh_entsize)
	ERROR (_("section [%2d] '%s': chain array too large\n"),
	       idx, section_name (ebl, idx));

      maxidx = symsize;
    }

  Elf32_Word *buf = (Elf32_Word *) data->d_buf;
  Elf32_Word *end = (Elf32_Word *) ((char *) data->d_buf + shdr->sh_size);
  size_t cnt;
  for (cnt = 2; cnt < 2 + nbucket; ++cnt)
    {
      if (buf + cnt >= end)
	break;
      else if (buf[cnt] >= maxidx)
      ERROR (_("\
section [%2d] '%s': hash bucket reference %zu out of bounds\n"),
	     idx, section_name (ebl, idx), cnt - 2);
    }

  for (; cnt < 2 + nbucket + nchain; ++cnt)
    {
      if (buf + cnt >= end)
	break;
      else if (buf[cnt] >= maxidx)
      ERROR (_("\
section [%2d] '%s': hash chain reference %zu out of bounds\n"),
	     idx, section_name (ebl, idx), cnt - 2 - nbucket);
    }
}


static void
check_sysv_hash64 (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *data, int idx,
		 GElf_Shdr *symshdr)
{
  Elf64_Xword nbucket = ((Elf64_Xword *) data->d_buf)[0];
  Elf64_Xword nchain = ((Elf64_Xword *) data->d_buf)[1];

  uint64_t maxwords = shdr->sh_size / sizeof (Elf64_Xword);
  if (maxwords < 2
      || maxwords - 2 < nbucket
      || maxwords - 2 - nbucket < nchain)
    {
      ERROR (_("\
section [%2d] '%s': hash table section is too small (is %ld, expected %ld)\n"),
	     idx, section_name (ebl, idx), (long int) shdr->sh_size,
	     (long int) ((2 + nbucket + nchain) * sizeof (Elf64_Xword)));
      return;
    }

  size_t maxidx = nchain;

  if (symshdr != NULL && symshdr->sh_entsize != 0)
    {
      size_t symsize = symshdr->sh_size / symshdr->sh_entsize;

      if (nchain > symshdr->sh_size / symshdr->sh_entsize)
	ERROR (_("section [%2d] '%s': chain array too large\n"),
	       idx, section_name (ebl, idx));

      maxidx = symsize;
    }

  Elf64_Xword *buf = (Elf64_Xword *) data->d_buf;
  Elf64_Xword *end = (Elf64_Xword *) ((char *) data->d_buf + shdr->sh_size);
  size_t cnt;
  for (cnt = 2; cnt < 2 + nbucket; ++cnt)
    {
      if (buf + cnt >= end)
	break;
      else if (buf[cnt] >= maxidx)
      ERROR (_("\
section [%2d] '%s': hash bucket reference %zu out of bounds\n"),
	     idx, section_name (ebl, idx), cnt - 2);
    }

  for (; cnt < 2 + nbucket + nchain; ++cnt)
    {
      if (buf + cnt >= end)
	break;
      else if (buf[cnt] >= maxidx)
      ERROR (_("\
section [%2d] '%s': hash chain reference %" PRIu64 " out of bounds\n"),
	       idx, section_name (ebl, idx), (uint64_t) cnt - 2 - nbucket);
    }
}


static void
check_gnu_hash (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *data, int idx,
		GElf_Shdr *symshdr)
{
  if (data->d_size < 4 * sizeof (Elf32_Word))
    {
      ERROR (_("\
section [%2d] '%s': not enough data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  Elf32_Word nbuckets = ((Elf32_Word *) data->d_buf)[0];
  Elf32_Word symbias = ((Elf32_Word *) data->d_buf)[1];
  Elf32_Word bitmask_words = ((Elf32_Word *) data->d_buf)[2];

  if (bitmask_words == 0 || !powerof2 (bitmask_words))
    {
      ERROR (_("\
section [%2d] '%s': bitmask size zero or not power of 2: %u\n"),
	     idx, section_name (ebl, idx), bitmask_words);
      return;
    }

  size_t bitmask_idxmask = bitmask_words - 1;
  if (gelf_getclass (ebl->elf) == ELFCLASS64)
    bitmask_words *= 2;
  Elf32_Word shift = ((Elf32_Word *) data->d_buf)[3];

  /* Is there still room for the sym chain?
     Use uint64_t calculation to prevent 32bit overflow.  */
  uint64_t used_buf = (4ULL + bitmask_words + nbuckets) * sizeof (Elf32_Word);
  if (used_buf > data->d_size)
    {
      ERROR (_("\
section [%2d] '%s': hash table section is too small (is %ld, expected at least %ld)\n"),
	     idx, section_name (ebl, idx), (long int) shdr->sh_size,
	     (long int) used_buf);
      return;
    }

  if (shift > 31)
    {
      ERROR (_("\
section [%2d] '%s': 2nd hash function shift too big: %u\n"),
	     idx, section_name (ebl, idx), shift);
      return;
    }

  size_t maxidx = shdr->sh_size / sizeof (Elf32_Word) - (4 + bitmask_words
							 + nbuckets);

  if (symshdr != NULL && symshdr->sh_entsize != 0)
    maxidx = MIN (maxidx, symshdr->sh_size / symshdr->sh_entsize);

  /* We need the symbol section data.  */
  Elf_Data *symdata = elf_getdata (elf_getscn (ebl->elf, shdr->sh_link), NULL);

  union
  {
    Elf32_Word *p32;
    Elf64_Xword *p64;
  } bitmask = { .p32 = &((Elf32_Word *) data->d_buf)[4] },
      collected = { .p32 = xcalloc (bitmask_words, sizeof (Elf32_Word)) };

  size_t classbits = gelf_getclass (ebl->elf) == ELFCLASS32 ? 32 : 64;

  size_t cnt;
  for (cnt = 4 + bitmask_words; cnt < 4 + bitmask_words + nbuckets; ++cnt)
    {
      Elf32_Word symidx = ((Elf32_Word *) data->d_buf)[cnt];

      if (symidx == 0)
	continue;

      if (symidx < symbias)
	{
	  ERROR (_("\
section [%2d] '%s': hash chain for bucket %zu lower than symbol index bias\n"),
		 idx, section_name (ebl, idx), cnt - (4 + bitmask_words));
	  continue;
	}

      while (symidx - symbias < maxidx)
	{
	  Elf32_Word chainhash = ((Elf32_Word *) data->d_buf)[4
							      + bitmask_words
							      + nbuckets
							      + symidx
							      - symbias];

	  if (symdata != NULL)
	    {
	      /* Check that the referenced symbol is not undefined.  */
	      GElf_Sym sym_mem;
	      GElf_Sym *sym = gelf_getsym (symdata, symidx, &sym_mem);
	      if (sym != NULL && sym->st_shndx == SHN_UNDEF
		  && GELF_ST_TYPE (sym->st_info) != STT_FUNC)
		ERROR (_("\
section [%2d] '%s': symbol %u referenced in chain for bucket %zu is undefined\n"),
		       idx, section_name (ebl, idx), symidx,
		       cnt - (4 + bitmask_words));

	      const char *symname = (sym != NULL
				     ? elf_strptr (ebl->elf, symshdr->sh_link,
						   sym->st_name)
				     : NULL);
	      if (symname != NULL)
		{
		  Elf32_Word hval = elf_gnu_hash (symname);
		  if ((hval & ~1u) != (chainhash & ~1u))
		    ERROR (_("\
section [%2d] '%s': hash value for symbol %u in chain for bucket %zu wrong\n"),
			   idx, section_name (ebl, idx), symidx,
			   cnt - (4 + bitmask_words));

		  /* Set the bits in the bitmask.  */
		  size_t maskidx = (hval / classbits) & bitmask_idxmask;
		  if (maskidx >= bitmask_words)
		    {
		      ERROR (_("\
section [%2d] '%s': mask index for symbol %u in chain for bucket %zu wrong\n"),
			     idx, section_name (ebl, idx), symidx,
			     cnt - (4 + bitmask_words));
		      return;
		    }
		  if (classbits == 32)
		    {
		      collected.p32[maskidx]
			|= UINT32_C (1) << (hval & (classbits - 1));
		      collected.p32[maskidx]
			|= UINT32_C (1) << ((hval >> shift) & (classbits - 1));
		    }
		  else
		    {
		      collected.p64[maskidx]
			|= UINT64_C (1) << (hval & (classbits - 1));
		      collected.p64[maskidx]
			|= UINT64_C (1) << ((hval >> shift) & (classbits - 1));
		    }
		}
	    }

	  if ((chainhash & 1) != 0)
	    break;

	  ++symidx;
	}

      if (symidx - symbias >= maxidx)
	ERROR (_("\
section [%2d] '%s': hash chain for bucket %zu out of bounds\n"),
	       idx, section_name (ebl, idx), cnt - (4 + bitmask_words));
      else if (symshdr != NULL && symshdr->sh_entsize != 0
	       && symidx > symshdr->sh_size / symshdr->sh_entsize)
	ERROR (_("\
section [%2d] '%s': symbol reference in chain for bucket %zu out of bounds\n"),
	       idx, section_name (ebl, idx), cnt - (4 + bitmask_words));
    }

  if (memcmp (collected.p32, bitmask.p32, bitmask_words * sizeof (Elf32_Word)))
    ERROR (_("\
section [%2d] '%s': bitmask does not match names in the hash table\n"),
	   idx, section_name (ebl, idx));

  free (collected.p32);
}


static void
check_hash (int tag, Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  if (ehdr->e_type == ET_REL)
    {
      ERROR (_("\
section [%2d] '%s': relocatable files cannot have hash tables\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL || data->d_buf == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link),
				     &symshdr_mem);
  if (symshdr != NULL && symshdr->sh_type != SHT_DYNSYM)
    ERROR (_("\
section [%2d] '%s': hash table not for dynamic symbol table\n"),
	   idx, section_name (ebl, idx));
  else if (symshdr == NULL)
    ERROR (_("\
section [%2d] '%s': invalid sh_link symbol table section index [%2d]\n"),
	   idx, section_name (ebl, idx), shdr->sh_link);

  size_t expect_entsize = (tag == SHT_GNU_HASH
			   ? (gelf_getclass (ebl->elf) == ELFCLASS32
			      ? sizeof (Elf32_Word) : 0)
			   : (size_t) ebl_sysvhash_entrysize (ebl));

  if (shdr->sh_entsize != expect_entsize)
    ERROR (_("\
section [%2d] '%s': hash table entry size incorrect\n"),
	   idx, section_name (ebl, idx));

  if ((shdr->sh_flags & SHF_ALLOC) == 0)
    ERROR (_("section [%2d] '%s': not marked to be allocated\n"),
	   idx, section_name (ebl, idx));

  if (shdr->sh_size < (tag == SHT_GNU_HASH ? 4 : 2) * (expect_entsize ?: 4))
    {
      ERROR (_("\
section [%2d] '%s': hash table has not even room for initial administrative entries\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  switch (tag)
    {
    case SHT_HASH:
      if (ebl_sysvhash_entrysize (ebl) == sizeof (Elf64_Xword))
	check_sysv_hash64 (ebl, shdr, data, idx, symshdr);
      else
	check_sysv_hash (ebl, shdr, data, idx, symshdr);
      break;

    case SHT_GNU_HASH:
      check_gnu_hash (ebl, shdr, data, idx, symshdr);
      break;

    default:
      assert (! "should not happen");
    }
}


/* Compare content of both hash tables, it must be identical.  */
static void
compare_hash_gnu_hash (Ebl *ebl, GElf_Ehdr *ehdr, size_t hash_idx,
		       size_t gnu_hash_idx)
{
  Elf_Scn *hash_scn = elf_getscn (ebl->elf, hash_idx);
  Elf_Data *hash_data = elf_getdata (hash_scn, NULL);
  GElf_Shdr hash_shdr_mem;
  GElf_Shdr *hash_shdr = gelf_getshdr (hash_scn, &hash_shdr_mem);
  Elf_Scn *gnu_hash_scn = elf_getscn (ebl->elf, gnu_hash_idx);
  Elf_Data *gnu_hash_data = elf_getdata (gnu_hash_scn, NULL);
  GElf_Shdr gnu_hash_shdr_mem;
  GElf_Shdr *gnu_hash_shdr = gelf_getshdr (gnu_hash_scn, &gnu_hash_shdr_mem);

  if (hash_shdr == NULL || gnu_hash_shdr == NULL
      || hash_data == NULL || hash_data->d_buf == NULL
      || gnu_hash_data == NULL || gnu_hash_data->d_buf == NULL)
    /* None of these pointers should be NULL since we used the
       sections already.  We are careful nonetheless.  */
    return;

  /* The link must point to the same symbol table.  */
  if (hash_shdr->sh_link != gnu_hash_shdr->sh_link)
    {
      ERROR (_("\
sh_link in hash sections [%2zu] '%s' and [%2zu] '%s' not identical\n"),
	     hash_idx, elf_strptr (ebl->elf, shstrndx, hash_shdr->sh_name),
	     gnu_hash_idx,
	     elf_strptr (ebl->elf, shstrndx, gnu_hash_shdr->sh_name));
      return;
    }

  Elf_Scn *sym_scn = elf_getscn (ebl->elf, hash_shdr->sh_link);
  Elf_Data *sym_data = elf_getdata (sym_scn, NULL);
  GElf_Shdr sym_shdr_mem;
  GElf_Shdr *sym_shdr = gelf_getshdr (sym_scn, &sym_shdr_mem);

  if (sym_data == NULL || sym_data->d_buf == NULL
      || sym_shdr == NULL || sym_shdr->sh_entsize == 0)
    return;

  const char *hash_name;
  const char *gnu_hash_name;
  hash_name  = elf_strptr (ebl->elf, shstrndx, hash_shdr->sh_name);
  gnu_hash_name  = elf_strptr (ebl->elf, shstrndx, gnu_hash_shdr->sh_name);

  if (gnu_hash_data->d_size < 4 * sizeof (Elf32_Word))
    {
      ERROR (_("\
hash section [%2zu] '%s' does not contain enough data\n"),
	     gnu_hash_idx, gnu_hash_name);
      return;
    }

  uint32_t nentries = sym_shdr->sh_size / sym_shdr->sh_entsize;
  char *used = alloca (nentries);
  memset (used, '\0', nentries);

  /* First go over the GNU_HASH table and mark the entries as used.  */
  const Elf32_Word *gnu_hasharr = (Elf32_Word *) gnu_hash_data->d_buf;
  Elf32_Word gnu_nbucket = gnu_hasharr[0];
  Elf32_Word gnu_symbias = gnu_hasharr[1];
  const int bitmap_factor = ehdr->e_ident[EI_CLASS] == ELFCLASS32 ? 1 : 2;
  const Elf32_Word *gnu_bucket = (gnu_hasharr
				  + (4 + gnu_hasharr[2] * bitmap_factor));
  const Elf32_Word *gnu_chain = gnu_bucket + gnu_hasharr[0];

  if (gnu_hasharr[2] == 0)
    {
      ERROR (_("\
hash section [%2zu] '%s' has zero bit mask words\n"),
	     gnu_hash_idx, gnu_hash_name);
      return;
    }

  uint64_t used_buf = ((4ULL + gnu_hasharr[2] * bitmap_factor + gnu_nbucket)
		       * sizeof (Elf32_Word));
  uint32_t max_nsyms = (gnu_hash_data->d_size - used_buf) / sizeof (Elf32_Word);
  if (used_buf > gnu_hash_data->d_size)
    {
      ERROR (_("\
hash section [%2zu] '%s' uses too much data\n"),
	     gnu_hash_idx, gnu_hash_name);
      return;
    }

  for (Elf32_Word cnt = 0; cnt < gnu_nbucket; ++cnt)
    {
      if (gnu_bucket[cnt] != STN_UNDEF)
	{
	  Elf32_Word symidx = gnu_bucket[cnt] - gnu_symbias;
	  do
	    {
	      if (symidx >= max_nsyms || symidx + gnu_symbias >= nentries)
		{
		  ERROR (_("\
hash section [%2zu] '%s' invalid symbol index %" PRIu32 " (max_nsyms: %" PRIu32 ", nentries: %" PRIu32 "\n"),
			 gnu_hash_idx, gnu_hash_name, symidx, max_nsyms, nentries);
		  return;
		}
	      used[symidx + gnu_symbias] |= 1;
	    }
	  while ((gnu_chain[symidx++] & 1u) == 0);
	}
    }

  /* Now go over the old hash table and check that we cover the same
     entries.  */
  if (hash_shdr->sh_entsize == sizeof (Elf32_Word))
    {
      const Elf32_Word *hasharr = (Elf32_Word *) hash_data->d_buf;
      if (hash_data->d_size < 2 * sizeof (Elf32_Word))
	{
	  ERROR (_("\
hash section [%2zu] '%s' does not contain enough data\n"),
		 hash_idx, hash_name);
	  return;
	}

      Elf32_Word nbucket = hasharr[0];
      Elf32_Word nchain = hasharr[1];
      uint64_t hash_used = (2ULL + nchain + nbucket) * sizeof (Elf32_Word);
      if (hash_used > hash_data->d_size)
	{
	  ERROR (_("\
hash section [%2zu] '%s' uses too much data\n"),
		 hash_idx, hash_name);
	  return;
	}

      const Elf32_Word *bucket = &hasharr[2];
      const Elf32_Word *chain = &hasharr[2 + nbucket];

      for (Elf32_Word cnt = 0; cnt < nbucket; ++cnt)
	{
	  Elf32_Word symidx = bucket[cnt];
	  while (symidx != STN_UNDEF && symidx < nentries && symidx < nchain)
	    {
	      used[symidx] |= 2;
	      symidx = chain[symidx];
	    }
	}
    }
  else if (hash_shdr->sh_entsize == sizeof (Elf64_Xword))
    {
      const Elf64_Xword *hasharr = (Elf64_Xword *) hash_data->d_buf;
      if (hash_data->d_size < 2 * sizeof (Elf32_Word))
	{
	  ERROR (_("\
hash section [%2zu] '%s' does not contain enough data\n"),
		 hash_idx, hash_name);
	  return;
	}

      Elf64_Xword nbucket = hasharr[0];
      Elf64_Xword nchain = hasharr[1];
      uint64_t maxwords = hash_data->d_size / sizeof (Elf64_Xword);
      if (maxwords < 2
	  || maxwords - 2 < nbucket
	  || maxwords - 2 - nbucket < nchain)
	{
	  ERROR (_("\
hash section [%2zu] '%s' uses too much data\n"),
		 hash_idx, hash_name);
	  return;
	}

      const Elf64_Xword *bucket = &hasharr[2];
      const Elf64_Xword *chain = &hasharr[2 + nbucket];

      for (Elf64_Xword cnt = 0; cnt < nbucket; ++cnt)
	{
	  Elf64_Xword symidx = bucket[cnt];
	  while (symidx != STN_UNDEF && symidx < nentries && symidx < nchain)
	    {
	      used[symidx] |= 2;
	      symidx = chain[symidx];
	    }
	}
    }
  else
    {
      ERROR (_("\
hash section [%2zu] '%s' invalid sh_entsize\n"),
	     hash_idx, hash_name);
      return;
    }

  /* Now see which entries are not set in one or both hash tables
     (unless the symbol is undefined in which case it can be omitted
     in the new table format).  */
  if ((used[0] & 1) != 0)
    ERROR (_("section [%2zu] '%s': reference to symbol index 0\n"),
	   gnu_hash_idx,
	   elf_strptr (ebl->elf, shstrndx, gnu_hash_shdr->sh_name));
  if ((used[0] & 2) != 0)
    ERROR (_("section [%2zu] '%s': reference to symbol index 0\n"),
	   hash_idx, elf_strptr (ebl->elf, shstrndx, hash_shdr->sh_name));

  for (uint32_t cnt = 1; cnt < nentries; ++cnt)
    if (used[cnt] != 0 && used[cnt] != 3)
      {
	if (used[cnt] == 1)
	  ERROR (_("\
symbol %d referenced in new hash table in [%2zu] '%s' but not in old hash table in [%2zu] '%s'\n"),
		 cnt, gnu_hash_idx,
		 elf_strptr (ebl->elf, shstrndx, gnu_hash_shdr->sh_name),
		 hash_idx,
		 elf_strptr (ebl->elf, shstrndx, hash_shdr->sh_name));
	else
	  {
	    GElf_Sym sym_mem;
	    GElf_Sym *sym = gelf_getsym (sym_data, cnt, &sym_mem);

	    if (sym != NULL && sym->st_shndx != STN_UNDEF)
	      ERROR (_("\
symbol %d referenced in old hash table in [%2zu] '%s' but not in new hash table in [%2zu] '%s'\n"),
		     cnt, hash_idx,
		     elf_strptr (ebl->elf, shstrndx, hash_shdr->sh_name),
		     gnu_hash_idx,
		     elf_strptr (ebl->elf, shstrndx, gnu_hash_shdr->sh_name));
	  }
      }
}


static void
check_null (Ebl *ebl, GElf_Shdr *shdr, int idx)
{
#define TEST(name, extra) \
  if (extra && shdr->sh_##name != 0)					      \
    ERROR (_("section [%2d] '%s': nonzero sh_%s for NULL section\n"),  \
	   idx, section_name (ebl, idx), #name)

  TEST (name, 1);
  TEST (flags, 1);
  TEST (addr, 1);
  TEST (offset, 1);
  TEST (size, idx != 0);
  TEST (link, idx != 0);
  TEST (info, 1);
  TEST (addralign, 1);
  TEST (entsize, 1);
}


static void
check_group (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  if (ehdr->e_type != ET_REL)
    {
      ERROR (_("\
section [%2d] '%s': section groups only allowed in relocatable object files\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  /* Check that sh_link is an index of a symbol table.  */
  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  if (symshdr == NULL)
    ERROR (_("section [%2d] '%s': cannot get symbol table: %s\n"),
	   idx, section_name (ebl, idx), elf_errmsg (-1));
  else
    {
      if (symshdr->sh_type != SHT_SYMTAB)
	ERROR (_("\
section [%2d] '%s': section reference in sh_link is no symbol table\n"),
	       idx, section_name (ebl, idx));

      if (shdr->sh_info >= symshdr->sh_size / gelf_fsize (ebl->elf, ELF_T_SYM,
							  1, EV_CURRENT))
	ERROR (_("\
section [%2d] '%s': invalid symbol index in sh_info\n"),
	       idx, section_name (ebl, idx));

      if (shdr->sh_flags != 0)
	ERROR (_("section [%2d] '%s': sh_flags not zero\n"),
	       idx, section_name (ebl, idx));

      GElf_Sym sym_data;
      GElf_Sym *sym = gelf_getsym (elf_getdata (symscn, NULL), shdr->sh_info,
				   &sym_data);
      if (sym == NULL)
	ERROR (_("\
section [%2d] '%s': cannot get symbol for signature\n"),
	       idx, section_name (ebl, idx));
      else if (elf_strptr (ebl->elf, symshdr->sh_link, sym->st_name) == NULL)
	ERROR (_("\
section [%2d] '%s': cannot get symbol name for signature\n"),
	       idx, section_name (ebl, idx));
      else if (strcmp (elf_strptr (ebl->elf, symshdr->sh_link, sym->st_name),
		       "") == 0)
	ERROR (_("\
section [%2d] '%s': signature symbol cannot be empty string\n"),
	       idx, section_name (ebl, idx));

      if (be_strict
	  && shdr->sh_entsize != elf32_fsize (ELF_T_WORD, 1, EV_CURRENT))
	ERROR (_("section [%2d] '%s': sh_flags not set correctly\n"),
	       idx, section_name (ebl, idx));
    }

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL || data->d_buf == NULL)
    ERROR (_("section [%2d] '%s': cannot get data: %s\n"),
	   idx, section_name (ebl, idx), elf_errmsg (-1));
  else
    {
      size_t elsize = elf32_fsize (ELF_T_WORD, 1, EV_CURRENT);
      size_t cnt;
      Elf32_Word val;

      if (data->d_size % elsize != 0)
	ERROR (_("\
section [%2d] '%s': section size not multiple of sizeof(Elf32_Word)\n"),
	       idx, section_name (ebl, idx));

      if (data->d_size < elsize)
	{
	  ERROR (_("\
section [%2d] '%s': section group without flags word\n"),
	       idx, section_name (ebl, idx));
	  return;
	}
      else if (be_strict)
	{
	  if (data->d_size < 2 * elsize)
	    ERROR (_("\
section [%2d] '%s': section group without member\n"),
		   idx, section_name (ebl, idx));
	  else if (data->d_size < 3 * elsize)
	    ERROR (_("\
section [%2d] '%s': section group with only one member\n"),
		   idx, section_name (ebl, idx));
	}

#if ALLOW_UNALIGNED
      val = *((Elf32_Word *) data->d_buf);
#else
      memcpy (&val, data->d_buf, elsize);
#endif
      if ((val & ~GRP_COMDAT) != 0)
	ERROR (_("section [%2d] '%s': unknown section group flags\n"),
	       idx, section_name (ebl, idx));

      for (cnt = elsize; cnt + elsize <= data->d_size; cnt += elsize)
	{
#if ALLOW_UNALIGNED
	  val = *((Elf32_Word *) ((char *) data->d_buf + cnt));
#else
	  memcpy (&val, (char *) data->d_buf + cnt, elsize);
#endif

	  if (val > shnum)
	    ERROR (_("\
section [%2d] '%s': section index %zu out of range\n"),
		   idx, section_name (ebl, idx), cnt / elsize);
	  else
	    {
	      GElf_Shdr refshdr_mem;
	      GElf_Shdr *refshdr = gelf_getshdr (elf_getscn (ebl->elf, val),
						 &refshdr_mem);
	      if (refshdr == NULL)
		ERROR (_("\
section [%2d] '%s': cannot get section header for element %zu: %s\n"),
		       idx, section_name (ebl, idx), cnt / elsize,
		       elf_errmsg (-1));
	      else
		{
		  if (refshdr->sh_type == SHT_GROUP)
		    ERROR (_("\
section [%2d] '%s': section group contains another group [%2d] '%s'\n"),
			   idx, section_name (ebl, idx),
			   val, section_name (ebl, val));

		  if ((refshdr->sh_flags & SHF_GROUP) == 0)
		    ERROR (_("\
section [%2d] '%s': element %zu references section [%2d] '%s' without SHF_GROUP flag set\n"),
			   idx, section_name (ebl, idx), cnt / elsize,
			   val, section_name (ebl, val));
		}

	      if (val < shnum && ++scnref[val] == 2)
		ERROR (_("\
section [%2d] '%s' is contained in more than one section group\n"),
		       val, section_name (ebl, val));
	    }
	}
    }
}


static const char *
section_flags_string (GElf_Word flags, char *buf, size_t len)
{
  if (flags == 0)
    return "none";

  static const struct
  {
    GElf_Word flag;
    const char *name;
  } known_flags[] =
    {
#define NEWFLAG(name) { SHF_##name, #name }
      NEWFLAG (WRITE),
      NEWFLAG (ALLOC),
      NEWFLAG (EXECINSTR),
      NEWFLAG (MERGE),
      NEWFLAG (STRINGS),
      NEWFLAG (INFO_LINK),
      NEWFLAG (LINK_ORDER),
      NEWFLAG (OS_NONCONFORMING),
      NEWFLAG (GROUP),
      NEWFLAG (TLS),
      NEWFLAG (COMPRESSED),
      NEWFLAG (GNU_RETAIN),
      NEWFLAG (ORDERED),
      NEWFLAG (EXCLUDE)
    };
#undef NEWFLAG
  const size_t nknown_flags = sizeof (known_flags) / sizeof (known_flags[0]);

  char *cp = buf;

  for (size_t cnt = 0; cnt < nknown_flags; ++cnt)
    if (flags & known_flags[cnt].flag)
      {
	if (cp != buf && len > 1)
	  {
	    *cp++ = '|';
	    --len;
	  }

	size_t ncopy = MIN (len - 1, strlen (known_flags[cnt].name));
	cp = mempcpy (cp, known_flags[cnt].name, ncopy);
	len -= ncopy;

	flags ^= known_flags[cnt].flag;
      }

  if (flags != 0 || cp == buf)
    {
      int r = snprintf (cp, len - 1, "%s%" PRIx64,
			(cp == buf) ? "" : "|", (uint64_t) flags);
      if (r > 0)
	cp += r;
    }
  *cp = '\0';

  return buf;
}


static int
has_copy_reloc (Ebl *ebl, unsigned int symscnndx, unsigned int symndx)
{
  /* First find the relocation section for the symbol table.  */
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr != NULL
	  && (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
	  && shdr->sh_link == symscnndx)
	/* Found the section.  */
	break;
    }

  if (scn == NULL)
    return 0;

  Elf_Data *data = elf_getdata (scn, NULL);
  if (data == NULL || shdr->sh_entsize == 0)
    return 0;

  if (shdr->sh_type == SHT_REL)
    for (int i = 0; (size_t) i < shdr->sh_size / shdr->sh_entsize; ++i)
      {
	GElf_Rel rel_mem;
	GElf_Rel *rel = gelf_getrel (data, i, &rel_mem);
	if (rel == NULL)
	  continue;

	if (GELF_R_SYM (rel->r_info) == symndx
	    && ebl_copy_reloc_p (ebl, GELF_R_TYPE (rel->r_info)))
	  return 1;
      }
  else
    for (int i = 0; (size_t) i < shdr->sh_size / shdr->sh_entsize; ++i)
      {
	GElf_Rela rela_mem;
	GElf_Rela *rela = gelf_getrela (data, i, &rela_mem);
	if (rela == NULL)
	  continue;

	if (GELF_R_SYM (rela->r_info) == symndx
	    && ebl_copy_reloc_p (ebl, GELF_R_TYPE (rela->r_info)))
	  return 1;
      }

  return 0;
}


static int
in_nobits_scn (Ebl *ebl, unsigned int shndx)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (ebl->elf, shndx), &shdr_mem);
  return shdr != NULL && shdr->sh_type == SHT_NOBITS;
}


static struct version_namelist
{
  const char *objname;
  const char *name;
  GElf_Versym ndx;
  enum { ver_def, ver_need } type;
  struct version_namelist *next;
} *version_namelist;


static int
add_version (const char *objname, const char *name, GElf_Versym ndx, int type)
{
  /* Check that there are no duplications.  */
  struct version_namelist *nlp = version_namelist;
  while (nlp != NULL)
    {
      if (((nlp->objname == NULL && objname == NULL)
	   || (nlp->objname != NULL && objname != NULL
	       && strcmp (nlp->objname, objname) == 0))
	  && strcmp (nlp->name, name) == 0)
	return nlp->type == ver_def ? 1 : -1;
      nlp = nlp->next;
    }

  nlp = xmalloc (sizeof (*nlp));
  nlp->objname = objname;
  nlp->name = name;
  nlp->ndx = ndx;
  nlp->type = type;
  nlp->next = version_namelist;
  version_namelist = nlp;

  return 0;
}


static void
check_versym (Ebl *ebl, int idx)
{
  Elf_Scn *scn = elf_getscn (ebl->elf, idx);
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    /* The error has already been reported.  */
    return;

  Elf_Data *data = elf_getdata (scn, NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  if (symshdr == NULL)
    /* The error has already been reported.  */
    return;

  if (symshdr->sh_type != SHT_DYNSYM)
    {
      ERROR (_("\
section [%2d] '%s' refers in sh_link to section [%2d] '%s' which is no dynamic symbol table\n"),
	     idx, section_name (ebl, idx),
	     shdr->sh_link, section_name (ebl, shdr->sh_link));
      return;
    }

  /* The number of elements in the version symbol table must be the
     same as the number of symbols.  */
  if (shdr->sh_entsize != 0 && symshdr->sh_entsize != 0
      && (shdr->sh_size / shdr->sh_entsize
	  != symshdr->sh_size / symshdr->sh_entsize))
    ERROR (_("\
section [%2d] '%s' has different number of entries than symbol table [%2d] '%s'\n"),
	   idx, section_name (ebl, idx),
	   shdr->sh_link, section_name (ebl, shdr->sh_link));

  Elf_Data *symdata = elf_getdata (symscn, NULL);
  if (symdata == NULL || shdr->sh_entsize == 0)
    /* The error has already been reported.  */
    return;

  for (int cnt = 1; (size_t) cnt < shdr->sh_size / shdr->sh_entsize; ++cnt)
    {
      GElf_Versym versym_mem;
      GElf_Versym *versym = gelf_getversym (data, cnt, &versym_mem);
      if (versym == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': symbol %d: cannot read version data\n"),
		 idx, section_name (ebl, idx), cnt);
	  break;
	}

      GElf_Sym sym_mem;
      GElf_Sym *sym = gelf_getsym (symdata, cnt, &sym_mem);
      if (sym == NULL)
	/* Already reported elsewhere.  */
	continue;

      if (*versym == VER_NDX_GLOBAL)
	{
	  /* Global symbol.  Make sure it is not defined as local.  */
	  if (GELF_ST_BIND (sym->st_info) == STB_LOCAL)
	    ERROR (_("\
section [%2d] '%s': symbol %d: local symbol with global scope\n"),
		   idx, section_name (ebl, idx), cnt);
	}
      else if (*versym != VER_NDX_LOCAL)
	{
	  /* Versioned symbol.  Make sure it is not defined as local.  */
	  if (!gnuld && GELF_ST_BIND (sym->st_info) == STB_LOCAL)
	    ERROR (_("\
section [%2d] '%s': symbol %d: local symbol with version\n"),
		   idx, section_name (ebl, idx), cnt);

	  /* Look through the list of defined versions and locate the
	     index we need for this symbol.  */
	  struct version_namelist *runp = version_namelist;
	  while (runp != NULL)
	    if (runp->ndx == (*versym & (GElf_Versym) 0x7fff))
	      break;
	    else
	      runp = runp->next;

	  if (runp == NULL)
	    ERROR (_("\
section [%2d] '%s': symbol %d: invalid version index %d\n"),
		   idx, section_name (ebl, idx), cnt, (int) *versym);
	  else if (sym->st_shndx == SHN_UNDEF
		   && runp->type == ver_def)
	    ERROR (_("\
section [%2d] '%s': symbol %d: version index %d is for defined version\n"),
		   idx, section_name (ebl, idx), cnt, (int) *versym);
	  else if (sym->st_shndx != SHN_UNDEF
		   && runp->type == ver_need)
	    {
	      /* Unless this symbol has a copy relocation associated
		 this must not happen.  */
	      if (!has_copy_reloc (ebl, shdr->sh_link, cnt)
		  && !in_nobits_scn (ebl, sym->st_shndx))
		ERROR (_("\
section [%2d] '%s': symbol %d: version index %d is for requested version\n"),
		       idx, section_name (ebl, idx), cnt, (int) *versym);
	    }
	}
    }
}


static int
unknown_dependency_p (Elf *elf, const char *fname)
{
  GElf_Phdr phdr_mem;
  GElf_Phdr *phdr = NULL;

  unsigned int i;
  for (i = 0; i < phnum; ++i)
    if ((phdr = gelf_getphdr (elf, i, &phdr_mem)) != NULL
	&& phdr->p_type == PT_DYNAMIC)
      break;

  if (i == phnum)
    return 1;
  assert (phdr != NULL);
  Elf_Scn *scn = gelf_offscn (elf, phdr->p_offset);
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  Elf_Data *data = elf_getdata (scn, NULL);
  if (shdr != NULL && shdr->sh_type == SHT_DYNAMIC
      && data != NULL && shdr->sh_entsize != 0)
    for (size_t j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j)
      {
	GElf_Dyn dyn_mem;
	GElf_Dyn *dyn = gelf_getdyn (data, j, &dyn_mem);
	if (dyn != NULL && dyn->d_tag == DT_NEEDED)
	  {
	    const char *str = elf_strptr (elf, shdr->sh_link, dyn->d_un.d_val);
	    if (str != NULL && strcmp (str, fname) == 0)
	      /* Found it.  */
	      return 0;
	  }
      }

  return 1;
}


static unsigned int nverneed;

static void
check_verneed (Ebl *ebl, GElf_Shdr *shdr, int idx)
{
  if (++nverneed == 2)
    ERROR (_("more than one version reference section present\n"));

  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link),
				     &strshdr_mem);
  if (strshdr == NULL)
    return;
  if (strshdr->sh_type != SHT_STRTAB)
    ERROR (_("\
section [%2d] '%s': sh_link does not link to string table\n"),
	   idx, section_name (ebl, idx));

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }
  unsigned int offset = 0;
  for (Elf64_Word cnt = shdr->sh_info; cnt > 0; )
    {
      cnt--;

      /* Get the data at the next offset.  */
      GElf_Verneed needmem;
      GElf_Verneed *need = gelf_getverneed (data, offset, &needmem);
      if (need == NULL)
	break;

      unsigned int auxoffset = offset + need->vn_aux;

      if (need->vn_version != EV_CURRENT)
	ERROR (_("\
section [%2d] '%s': entry %d has wrong version %d\n"),
	       idx, section_name (ebl, idx), cnt, (int) need->vn_version);

      if (need->vn_cnt > 0 && need->vn_aux < gelf_fsize (ebl->elf, ELF_T_VNEED,
							 1, EV_CURRENT))
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has wrong offset of auxiliary data\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}

      const char *libname = elf_strptr (ebl->elf, shdr->sh_link,
					need->vn_file);
      if (libname == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has invalid file reference\n"),
		 idx, section_name (ebl, idx), cnt);
	  goto next_need;
	}

      /* Check that there is a DT_NEEDED entry for the referenced library.  */
      if (unknown_dependency_p (ebl->elf, libname))
	ERROR (_("\
section [%2d] '%s': entry %d references unknown dependency\n"),
	       idx, section_name (ebl, idx), cnt);

      for (int cnt2 = need->vn_cnt; --cnt2 >= 0; )
	{
	  GElf_Vernaux auxmem;
	  GElf_Vernaux *aux = gelf_getvernaux (data, auxoffset, &auxmem);
	  if (aux == NULL)
	    break;

	  if ((aux->vna_flags & ~VER_FLG_WEAK) != 0)
	    ERROR (_("\
section [%2d] '%s': auxiliary entry %d of entry %d has unknown flag\n"),
		   idx, section_name (ebl, idx), need->vn_cnt - cnt2, cnt);

	  const char *verstr = elf_strptr (ebl->elf, shdr->sh_link,
					   aux->vna_name);
	  if (verstr == NULL)
	    {
	      ERROR (_("\
section [%2d] '%s': auxiliary entry %d of entry %d has invalid name reference\n"),
		     idx, section_name (ebl, idx), need->vn_cnt - cnt2, cnt);
	      break;
	    }
	  else
	    {
	      GElf_Word hashval = elf_hash (verstr);
	      if (hashval != aux->vna_hash)
		ERROR (_("\
section [%2d] '%s': auxiliary entry %d of entry %d has wrong hash value: %#x, expected %#x\n"),
		       idx, section_name (ebl, idx), need->vn_cnt - cnt2,
		       cnt, (int) hashval, (int) aux->vna_hash);

	      int res = add_version (libname, verstr, aux->vna_other,
				     ver_need);
	      if (unlikely (res !=0))
		{
		  ERROR (_("\
section [%2d] '%s': auxiliary entry %d of entry %d has duplicate version name '%s'\n"),
			 idx, section_name (ebl, idx), need->vn_cnt - cnt2,
			 cnt, verstr);
		}
	    }

	  if ((aux->vna_next != 0 || cnt2 > 0)
	      && aux->vna_next < gelf_fsize (ebl->elf, ELF_T_VNAUX, 1,
					     EV_CURRENT))
	    {
	      ERROR (_("\
section [%2d] '%s': auxiliary entry %d of entry %d has wrong next field\n"),
		     idx, section_name (ebl, idx), need->vn_cnt - cnt2, cnt);
	      break;
	    }

	  auxoffset += MAX (aux->vna_next,
			    gelf_fsize (ebl->elf, ELF_T_VNAUX, 1, EV_CURRENT));
	}

      /* Find the next offset.  */
    next_need:
      offset += need->vn_next;

      if ((need->vn_next != 0 || cnt > 0)
	  && offset < auxoffset)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has invalid offset to next entry\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}

      if (need->vn_next == 0 && cnt > 0)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has zero offset to next entry, but sh_info says there are more entries\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}
    }
}


static unsigned int nverdef;

static void
check_verdef (Ebl *ebl, GElf_Shdr *shdr, int idx)
{
  if (++nverdef == 2)
    ERROR (_("more than one version definition section present\n"));

  GElf_Shdr strshdr_mem;
  GElf_Shdr *strshdr = gelf_getshdr (elf_getscn (ebl->elf, shdr->sh_link),
				     &strshdr_mem);
  if (strshdr == NULL)
    return;
  if (strshdr->sh_type != SHT_STRTAB)
    ERROR (_("\
section [%2d] '%s': sh_link does not link to string table\n"),
	   idx, section_name (ebl, idx));

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL)
    {
    no_data:
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  /* Iterate over all version definition entries.  We check that there
     is a BASE entry and that each index is unique.  To do the later
     we collection the information in a list which is later
     examined.  */
  struct namelist
  {
    const char *name;
    struct namelist *next;
  } *namelist = NULL;
  struct namelist *refnamelist = NULL;

  bool has_base = false;
  unsigned int offset = 0;
  for (Elf64_Word cnt = shdr->sh_info; cnt > 0; )
    {
      cnt--;

      /* Get the data at the next offset.  */
      GElf_Verdef defmem;
      GElf_Verdef *def = gelf_getverdef (data, offset, &defmem);
      if (def == NULL)
	goto no_data;

      if ((def->vd_flags & VER_FLG_BASE) != 0)
	{
	  if (has_base)
	    ERROR (_("\
section [%2d] '%s': more than one BASE definition\n"),
		   idx, section_name (ebl, idx));
	  if (def->vd_ndx != VER_NDX_GLOBAL)
	    ERROR (_("\
section [%2d] '%s': BASE definition must have index VER_NDX_GLOBAL\n"),
		   idx, section_name (ebl, idx));
	  has_base = true;
	}
      if ((def->vd_flags & ~(VER_FLG_BASE|VER_FLG_WEAK)) != 0)
	ERROR (_("\
section [%2d] '%s': entry %d has unknown flag\n"),
	       idx, section_name (ebl, idx), cnt);

      if (def->vd_version != EV_CURRENT)
	ERROR (_("\
section [%2d] '%s': entry %d has wrong version %d\n"),
	       idx, section_name (ebl, idx), cnt, (int) def->vd_version);

      if (def->vd_cnt > 0 && def->vd_aux < gelf_fsize (ebl->elf, ELF_T_VDEF,
						       1, EV_CURRENT))
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has wrong offset of auxiliary data\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}

      unsigned int auxoffset = offset + def->vd_aux;
      GElf_Verdaux auxmem;
      GElf_Verdaux *aux = gelf_getverdaux (data, auxoffset, &auxmem);
      if (aux == NULL)
	goto no_data;

      const char *name = elf_strptr (ebl->elf, shdr->sh_link, aux->vda_name);
      if (name == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has invalid name reference\n"),
		 idx, section_name (ebl, idx), cnt);
	  goto next_def;
	}
      GElf_Word hashval = elf_hash (name);
      if (def->vd_hash != hashval)
	ERROR (_("\
section [%2d] '%s': entry %d has wrong hash value: %#x, expected %#x\n"),
	       idx, section_name (ebl, idx), cnt, (int) hashval,
	       (int) def->vd_hash);

      int res = add_version (NULL, name, def->vd_ndx, ver_def);
      if (unlikely (res !=0))
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has duplicate version name '%s'\n"),
		 idx, section_name (ebl, idx), cnt, name);
	}

      struct namelist *newname = alloca (sizeof (*newname));
      newname->name = name;
      newname->next = namelist;
      namelist = newname;

      auxoffset += aux->vda_next;
      for (int cnt2 = 1; cnt2 < def->vd_cnt; ++cnt2)
	{
	  aux = gelf_getverdaux (data, auxoffset, &auxmem);
	  if (aux == NULL)
	    goto no_data;

	  name = elf_strptr (ebl->elf, shdr->sh_link, aux->vda_name);
	  if (name == NULL)
	    {
	      ERROR (_("\
section [%2d] '%s': entry %d has invalid name reference in auxiliary data\n"),
		     idx, section_name (ebl, idx), cnt);
	      break;
	    }
	  else
	    {
	      newname = alloca (sizeof (*newname));
	      newname->name = name;
	      newname->next = refnamelist;
	      refnamelist = newname;
	    }

	  if ((aux->vda_next != 0 || cnt2 + 1 < def->vd_cnt)
	      && aux->vda_next < gelf_fsize (ebl->elf, ELF_T_VDAUX, 1,
					     EV_CURRENT))
	    {
	      ERROR (_("\
section [%2d] '%s': entry %d has wrong next field in auxiliary data\n"),
		     idx, section_name (ebl, idx), cnt);
	      break;
	    }

	  auxoffset += MAX (aux->vda_next,
			    gelf_fsize (ebl->elf, ELF_T_VDAUX, 1, EV_CURRENT));
	}

      /* Find the next offset.  */
    next_def:
      offset += def->vd_next;

      if ((def->vd_next != 0 || cnt > 0)
	  && offset < auxoffset)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has invalid offset to next entry\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}

      if (def->vd_next == 0 && cnt > 0)
	{
	  ERROR (_("\
section [%2d] '%s': entry %d has zero offset to next entry, but sh_info says there are more entries\n"),
	         idx, section_name (ebl, idx), cnt);
	  break;
	}
    }

  if (!has_base)
    ERROR (_("section [%2d] '%s': no BASE definition\n"),
	   idx, section_name (ebl, idx));

  /* Check whether the referenced names are available.  */
  while (namelist != NULL)
    {
      struct version_namelist *runp = version_namelist;
      while (runp != NULL)
	{
	  if (runp->type == ver_def
	      && strcmp (runp->name, namelist->name) == 0)
	    break;
	  runp = runp->next;
	}

      if (runp == NULL)
	ERROR (_("\
section [%2d] '%s': unknown parent version '%s'\n"),
	       idx, section_name (ebl, idx), namelist->name);

      namelist = namelist->next;
    }
}

static inline size_t
buffer_pos (Elf_Data *data, const unsigned char *p)
{
  return p - (const unsigned char *) data->d_buf;
}

static inline size_t
buffer_left (Elf_Data *data, const unsigned char *p)
{
  return (const unsigned char *) data->d_buf + data->d_size - p;
}

static void
check_attributes (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  if (shdr->sh_size == 0)
    {
      ERROR (_("section [%2d] '%s': empty object attributes section\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  Elf_Data *data = elf_rawdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL || data->d_size == 0 || data->d_buf == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  const unsigned char *p = data->d_buf;
  if (*p++ != 'A')
    {
      ERROR (_("section [%2d] '%s': unrecognized attribute format\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  while (buffer_left (data, p) >= 4)
    {
      uint32_t len;
      memcpy (&len, p, sizeof len);

      if (len == 0)
	ERROR (_("\
section [%2d] '%s': offset %zu: zero length field in attribute section\n"),
	       idx, section_name (ebl, idx), buffer_pos (data, p));

      if (MY_ELFDATA != ehdr->e_ident[EI_DATA])
	CONVERT (len);

      if (len > buffer_left (data, p))
	{
	  ERROR (_("\
section [%2d] '%s': offset %zu: invalid length in attribute section\n"),
		 idx, section_name (ebl, idx), buffer_pos (data, p));
	  break;
	}

      const unsigned char *name = p + sizeof len;
      p += len;

      unsigned const char *q = memchr (name, '\0', len);
      if (q == NULL)
	{
	  ERROR (_("\
section [%2d] '%s': offset %zu: unterminated vendor name string\n"),
		 idx, section_name (ebl, idx), buffer_pos (data, p));
	  break;
	}
      ++q;

      if (q - name == sizeof "gnu" && !memcmp (name, "gnu", sizeof "gnu"))
	while (q < p)
	  {
	    unsigned const char *chunk = q;

	    unsigned int subsection_tag;
	    get_uleb128 (subsection_tag, q, p);

	    if (q >= p)
	      {
		ERROR (_("\
section [%2d] '%s': offset %zu: endless ULEB128 in attribute subsection tag\n"),
		       idx, section_name (ebl, idx), buffer_pos (data, chunk));
		break;
	      }

	    uint32_t subsection_len;
	    if (p - q < (ptrdiff_t) sizeof subsection_len)
	      {
		ERROR (_("\
section [%2d] '%s': offset %zu: truncated attribute section\n"),
		       idx, section_name (ebl, idx), buffer_pos (data, q));
		break;
	      }

	    memcpy (&subsection_len, q, sizeof subsection_len);
	    if (subsection_len == 0)
	      {
		ERROR (_("\
section [%2d] '%s': offset %zu: zero length field in attribute subsection\n"),
		       idx, section_name (ebl, idx), buffer_pos (data, q));

		q += sizeof subsection_len;
		continue;
	      }

	    if (MY_ELFDATA != ehdr->e_ident[EI_DATA])
	      CONVERT (subsection_len);

	    /* Don't overflow, ptrdiff_t might be 32bits, but signed.  */
	    if (p - chunk < (ptrdiff_t) subsection_len
	        || subsection_len >= (uint32_t) PTRDIFF_MAX)
	      {
		ERROR (_("\
section [%2d] '%s': offset %zu: invalid length in attribute subsection\n"),
		       idx, section_name (ebl, idx), buffer_pos (data, q));
		break;
	      }

	    const unsigned char *subsection_end = chunk + subsection_len;
	    chunk = q;
	    q = subsection_end;

	    if (subsection_tag != 1) /* Tag_File */
	      ERROR (_("\
section [%2d] '%s': offset %zu: attribute subsection has unexpected tag %u\n"),
		     idx, section_name (ebl, idx), buffer_pos (data, chunk), subsection_tag);
	    else
	      {
		chunk += sizeof subsection_len;
		while (chunk < q)
		  {
		    unsigned int tag;
		    get_uleb128 (tag, chunk, q);

		    uint64_t value = 0;
		    const unsigned char *r = chunk;
		    if (tag == 32 || (tag & 1) == 0)
		      {
			if (r >= q)
			  goto invalid_uleb;
			get_uleb128 (value, r, q);
			if (r > q)
			  {
			  invalid_uleb:
			    ERROR (_("\
section [%2d] '%s': offset %zu: endless ULEB128 in attribute tag\n"),
				   idx, section_name (ebl, idx), buffer_pos (data, chunk));
			    break;
			  }
		      }
		    if (tag == 32 || (tag & 1) != 0)
		      {
			r = memchr (r, '\0', q - r);
			if (r == NULL)
			  {
			    ERROR (_("\
section [%2d] '%s': offset %zu: unterminated string in attribute\n"),
				   idx, section_name (ebl, idx), buffer_pos (data, chunk));
			    break;
			  }
			++r;
		      }

		    const char *tag_name = NULL;
		    const char *value_name = NULL;
		    if (!ebl_check_object_attribute (ebl, (const char *) name,
						     tag, value,
						     &tag_name, &value_name))
		      ERROR (_("\
section [%2d] '%s': offset %zu: unrecognized attribute tag %u\n"),
			     idx, section_name (ebl, idx), buffer_pos (data, chunk), tag);
		    else if ((tag & 1) == 0 && value_name == NULL)
		      ERROR (_("\
section [%2d] '%s': offset %zu: unrecognized %s attribute value %" PRIu64 "\n"),
			     idx, section_name (ebl, idx), buffer_pos (data, chunk),
			     tag_name, value);

		    chunk = r;
		  }
	      }
	  }
      else
	ERROR (_("\
section [%2d] '%s': offset %zu: vendor '%s' unknown\n"),
	       idx, section_name (ebl, idx), buffer_pos (data, p), name);
    }

  if (buffer_left (data, p) != 0)
    ERROR (_("\
section [%2d] '%s': offset %zu: extra bytes after last attribute section\n"),
	   idx, section_name (ebl, idx), buffer_pos (data, p));
}

static bool has_loadable_segment;
static bool has_interp_segment;

static const struct
{
  const char *name;
  size_t namelen;
  GElf_Word type;
  enum { unused, exact, atleast, exact_or_gnuld } attrflag;
  GElf_Word attr;
  GElf_Word attr2;
} special_sections[] =
  {
    /* See figure 4-14 in the gABI.  */
    { ".bss", 5, SHT_NOBITS, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".comment", 8, SHT_PROGBITS, atleast, 0, SHF_MERGE | SHF_STRINGS },
    { ".data", 6, SHT_PROGBITS, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".data1", 7, SHT_PROGBITS, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".debug_str", 11, SHT_PROGBITS, exact_or_gnuld, SHF_MERGE | SHF_STRINGS, 0 },
    { ".debug_line_str", 16, SHT_PROGBITS, exact_or_gnuld, SHF_MERGE | SHF_STRINGS, 0 },
    { ".debug", 6, SHT_PROGBITS, exact, 0, 0 },
    { ".dynamic", 9, SHT_DYNAMIC, atleast, SHF_ALLOC, SHF_WRITE },
    { ".dynstr", 8, SHT_STRTAB, exact, SHF_ALLOC, 0 },
    { ".dynsym", 8, SHT_DYNSYM, exact, SHF_ALLOC, 0 },
    { ".fini", 6, SHT_PROGBITS, exact, SHF_ALLOC | SHF_EXECINSTR, 0 },
    { ".fini_array", 12, SHT_FINI_ARRAY, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".got", 5, SHT_PROGBITS, unused, 0, 0 }, // XXX more info?
    { ".hash", 6, SHT_HASH, exact, SHF_ALLOC, 0 },
    { ".init", 6, SHT_PROGBITS, exact, SHF_ALLOC | SHF_EXECINSTR, 0 },
    { ".init_array", 12, SHT_INIT_ARRAY, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".interp", 8, SHT_PROGBITS, atleast, 0, SHF_ALLOC }, // XXX more tests?
    { ".line", 6, SHT_PROGBITS, exact, 0, 0 },
    { ".note", 6, SHT_NOTE, atleast, 0, SHF_ALLOC },
    { ".plt", 5, SHT_PROGBITS, unused, 0, 0 }, // XXX more tests
    { ".preinit_array", 15, SHT_PREINIT_ARRAY, exact, SHF_ALLOC | SHF_WRITE, 0 },
    { ".rela", 5, SHT_RELA, atleast, 0, SHF_ALLOC | SHF_INFO_LINK }, // XXX more tests
    { ".relr", 5, SHT_RELR, atleast, 0, SHF_ALLOC }, // XXX more tests
    { ".rel", 4, SHT_REL, atleast, 0, SHF_ALLOC | SHF_INFO_LINK }, // XXX more tests
    { ".rodata", 8, SHT_PROGBITS, atleast, SHF_ALLOC, SHF_MERGE | SHF_STRINGS },
    { ".rodata1", 9, SHT_PROGBITS, atleast, SHF_ALLOC, SHF_MERGE | SHF_STRINGS },
    { ".shstrtab", 10, SHT_STRTAB, exact, 0, 0 },
    { ".strtab", 8, SHT_STRTAB, atleast, 0, SHF_ALLOC }, // XXX more tests
    { ".symtab", 8, SHT_SYMTAB, atleast, 0, SHF_ALLOC }, // XXX more tests
    { ".symtab_shndx", 14, SHT_SYMTAB_SHNDX, atleast, 0, SHF_ALLOC }, // XXX more tests
    { ".tbss", 6, SHT_NOBITS, exact, SHF_ALLOC | SHF_WRITE | SHF_TLS, 0 },
    { ".tdata", 7, SHT_PROGBITS, exact, SHF_ALLOC | SHF_WRITE | SHF_TLS, 0 },
    { ".tdata1", 8, SHT_PROGBITS, exact, SHF_ALLOC | SHF_WRITE | SHF_TLS, 0 },
    { ".text", 6, SHT_PROGBITS, exact, SHF_ALLOC | SHF_EXECINSTR, 0 },

    /* The following are GNU extensions.  */
    { ".gnu.version", 13, SHT_GNU_versym, exact, SHF_ALLOC, 0 },
    { ".gnu.version_d", 15, SHT_GNU_verdef, exact, SHF_ALLOC, 0 },
    { ".gnu.version_r", 15, SHT_GNU_verneed, exact, SHF_ALLOC, 0 },
    { ".gnu.attributes", 16, SHT_GNU_ATTRIBUTES, exact, 0, 0 },
  };
#define nspecial_sections \
  (sizeof (special_sections) / sizeof (special_sections[0]))

#define IS_KNOWN_SPECIAL(idx, string, prefix)			      \
  (special_sections[idx].namelen == sizeof string - (prefix ? 1 : 0)  \
   && !memcmp (special_sections[idx].name, string, \
	       sizeof string - (prefix ? 1 : 0)))

/* Extra section flags that might or might not be added to the section
   and have to be ignored.  */
#define EXTRA_SHFLAGS (SHF_LINK_ORDER \
		       | SHF_GNU_RETAIN \
		       | SHF_GROUP \
		       | SHF_COMPRESSED)


/* Indices of some sections we need later.  */
static size_t eh_frame_hdr_scnndx;
static size_t eh_frame_scnndx;
static size_t gcc_except_table_scnndx;


static void
check_sections (Ebl *ebl, GElf_Ehdr *ehdr)
{
  if (ehdr->e_shoff == 0)
    /* No section header.  */
    return;

  /* Allocate array to count references in section groups.  */
  scnref = xcalloc (shnum, sizeof (int));

  /* Check the zeroth section first.  It must not have any contents
     and the section header must contain nonzero value at most in the
     sh_size and sh_link fields.  */
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (ebl->elf, 0), &shdr_mem);
  if (shdr == NULL)
    ERROR (_("cannot get section header of zeroth section\n"));
  else
    {
      if (shdr->sh_name != 0)
	ERROR (_("zeroth section has nonzero name\n"));
      if (shdr->sh_type != 0)
	ERROR (_("zeroth section has nonzero type\n"));
      if (shdr->sh_flags != 0)
	ERROR (_("zeroth section has nonzero flags\n"));
      if (shdr->sh_addr != 0)
	ERROR (_("zeroth section has nonzero address\n"));
      if (shdr->sh_offset != 0)
	ERROR (_("zeroth section has nonzero offset\n"));
      if (shdr->sh_addralign != 0)
	ERROR (_("zeroth section has nonzero align value\n"));
      if (shdr->sh_entsize != 0)
	ERROR (_("zeroth section has nonzero entry size value\n"));

      if (shdr->sh_size != 0 && ehdr->e_shnum != 0)
	ERROR (_("\
zeroth section has nonzero size value while ELF header has nonzero shnum value\n"));

      if (shdr->sh_link != 0 && ehdr->e_shstrndx != SHN_XINDEX)
	ERROR (_("\
zeroth section has nonzero link value while ELF header does not signal overflow in shstrndx\n"));

      if (shdr->sh_info != 0 && ehdr->e_phnum != PN_XNUM)
	ERROR (_("\
zeroth section has nonzero link value while ELF header does not signal overflow in phnum\n"));
    }

  int *segment_flags = xcalloc (phnum, sizeof segment_flags[0]);

  bool dot_interp_section = false;

  size_t hash_idx = 0;
  size_t gnu_hash_idx = 0;

  size_t versym_scnndx = 0;
  for (size_t cnt = 1; cnt < shnum; ++cnt)
    {
      Elf_Scn *scn = elf_getscn (ebl->elf, cnt);
      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  ERROR (_("\
cannot get section header for section [%2zu] '%s': %s\n"),
		 cnt, section_name (ebl, cnt), elf_errmsg (-1));
	  continue;
	}

      const char *scnname = elf_strptr (ebl->elf, shstrndx, shdr->sh_name);

      if (scnname == NULL)
	ERROR (_("section [%2zu]: invalid name\n"), cnt);
      else
	{
	  /* Check whether it is one of the special sections defined in
	     the gABI.  */
	  size_t s;
	  for (s = 0; s < nspecial_sections; ++s)
	    if (strncmp (scnname, special_sections[s].name,
			 special_sections[s].namelen) == 0)
	      {
		char stbuf1[100];
		char stbuf2[100];
		char stbuf3[100];

		GElf_Word good_type = special_sections[s].type;
		if (IS_KNOWN_SPECIAL (s, ".plt", false)
		    && ebl_bss_plt_p (ebl))
		  good_type = SHT_NOBITS;

		/* In a debuginfo file, any normal section can be SHT_NOBITS.
		   This is only invalid for DWARF sections and .shstrtab.  */
		if (shdr->sh_type != good_type
		    && (shdr->sh_type != SHT_NOBITS
			|| !is_debuginfo
			|| IS_KNOWN_SPECIAL (s, ".debug_str", false)
			|| IS_KNOWN_SPECIAL (s, ".debug", true)
			|| IS_KNOWN_SPECIAL (s, ".shstrtab", false)))
		  ERROR (_("\
section [%2d] '%s' has wrong type: expected %s, is %s\n"),
			 (int) cnt, scnname,
			 ebl_section_type_name (ebl, special_sections[s].type,
						stbuf1, sizeof (stbuf1)),
			 ebl_section_type_name (ebl, shdr->sh_type,
						stbuf2, sizeof (stbuf2)));

		if (special_sections[s].attrflag == exact
		    || special_sections[s].attrflag == exact_or_gnuld)
		  {
		    /* Except for the link order, retain, group bit and
		       compression flag all the other bits should
		       match exactly.  */
		    if ((shdr->sh_flags & ~EXTRA_SHFLAGS)
			!= special_sections[s].attr
			&& (special_sections[s].attrflag == exact || !gnuld))
		      ERROR (_("\
section [%2zu] '%s' has wrong flags: expected %s, is %s\n"),
			     cnt, scnname,
			     section_flags_string (special_sections[s].attr,
						   stbuf1, sizeof (stbuf1)),
			     section_flags_string (shdr->sh_flags
						   & ~EXTRA_SHFLAGS,
						   stbuf2, sizeof (stbuf2)));
		  }
		else if (special_sections[s].attrflag == atleast)
		  {
		    if ((shdr->sh_flags & special_sections[s].attr)
			!= special_sections[s].attr
			|| ((shdr->sh_flags
			     & ~(EXTRA_SHFLAGS
				 | special_sections[s].attr
				 | special_sections[s].attr2))
			    != 0))
		      ERROR (_("\
section [%2zu] '%s' has wrong flags: expected %s and possibly %s, is %s\n"),
			     cnt, scnname,
			     section_flags_string (special_sections[s].attr,
						   stbuf1, sizeof (stbuf1)),
			     section_flags_string (special_sections[s].attr2,
						   stbuf2, sizeof (stbuf2)),
			     section_flags_string (shdr->sh_flags
						   & ~EXTRA_SHFLAGS,
						   stbuf3, sizeof (stbuf3)));
		  }

		if (strcmp (scnname, ".interp") == 0)
		  {
		    dot_interp_section = true;

		    if (ehdr->e_type == ET_REL)
		      ERROR (_("\
section [%2zu] '%s' present in object file\n"),
			     cnt, scnname);

		    if ((shdr->sh_flags & SHF_ALLOC) != 0
			&& !has_loadable_segment)
		      ERROR (_("\
section [%2zu] '%s' has SHF_ALLOC flag set but there is no loadable segment\n"),
			     cnt, scnname);
		    else if ((shdr->sh_flags & SHF_ALLOC) == 0
			     && has_loadable_segment)
		      ERROR (_("\
section [%2zu] '%s' has SHF_ALLOC flag not set but there are loadable segments\n"),
			     cnt, scnname);
		  }
		else
		  {
		    if (strcmp (scnname, ".symtab_shndx") == 0
			&& ehdr->e_type != ET_REL)
		      ERROR (_("\
section [%2zu] '%s' is extension section index table in non-object file\n"),
			     cnt, scnname);

		    /* These sections must have the SHF_ALLOC flag set iff
		       a loadable segment is available.

		       .relxxx
		       .strtab
		       .symtab
		       .symtab_shndx

		       Check that if there is a reference from the
		       loaded section these sections also have the
		       ALLOC flag set.  */
#if 0
		    // XXX TODO
		    if ((shdr->sh_flags & SHF_ALLOC) != 0
			&& !has_loadable_segment)
		      ERROR (_("\
section [%2zu] '%s' has SHF_ALLOC flag set but there is no loadable segment\n"),
			     cnt, scnname);
		    else if ((shdr->sh_flags & SHF_ALLOC) == 0
			     && has_loadable_segment)
		      ERROR (_("\
section [%2zu] '%s' has SHF_ALLOC flag not set but there are loadable segments\n"),
			     cnt, scnname);
#endif
		  }

		break;
	      }

	  /* Remember a few special sections for later.  */
	  if (strcmp (scnname, ".eh_frame_hdr") == 0)
	    eh_frame_hdr_scnndx = cnt;
	  else if (strcmp (scnname, ".eh_frame") == 0)
	    eh_frame_scnndx = cnt;
	  else if (strcmp (scnname, ".gcc_except_table") == 0)
	    gcc_except_table_scnndx = cnt;
	}

      if (shdr->sh_entsize != 0 && shdr->sh_size % shdr->sh_entsize)
	ERROR (_("\
section [%2zu] '%s': size not multiple of entry size\n"),
	       cnt, section_name (ebl, cnt));

      if (elf_strptr (ebl->elf, shstrndx, shdr->sh_name) == NULL)
	ERROR (_("cannot get section header\n"));

      if (shdr->sh_type >= SHT_NUM
	  && shdr->sh_type != SHT_GNU_ATTRIBUTES
	  && shdr->sh_type != SHT_GNU_LIBLIST
	  && shdr->sh_type != SHT_CHECKSUM
	  && shdr->sh_type != SHT_GNU_verdef
	  && shdr->sh_type != SHT_GNU_verneed
	  && shdr->sh_type != SHT_GNU_versym
	  && ebl_section_type_name (ebl, shdr->sh_type, NULL, 0) == NULL)
	ERROR (_("section [%2zu] '%s' has unsupported type %d\n"),
	       cnt, section_name (ebl, cnt),
	       (int) shdr->sh_type);

#define ALL_SH_FLAGS (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_MERGE \
		      | SHF_STRINGS | SHF_INFO_LINK | SHF_LINK_ORDER \
		      | SHF_OS_NONCONFORMING | SHF_GROUP | SHF_TLS \
		      | SHF_COMPRESSED | SHF_GNU_RETAIN)
      if (shdr->sh_flags & ~(GElf_Xword) ALL_SH_FLAGS)
	{
	  GElf_Xword sh_flags = shdr->sh_flags & ~(GElf_Xword) ALL_SH_FLAGS;
	  if (sh_flags & SHF_MASKPROC)
	    {
	      /* Strictly speaking SHF_EXCLUDE is a processor specific
		 section flag, but it is used generically in the GNU
		 toolchain.  */
	      if (gnuld)
		sh_flags &= ~(GElf_Xword) SHF_EXCLUDE;
	      if (!ebl_machine_section_flag_check (ebl,
						   sh_flags & SHF_MASKPROC))
		ERROR (_("section [%2zu] '%s'"
				" contains invalid processor-specific flag(s)"
				" %#" PRIx64 "\n"),
		       cnt, section_name (ebl, cnt), sh_flags & SHF_MASKPROC);
	      sh_flags &= ~(GElf_Xword) SHF_MASKPROC;
	    }
	  if (sh_flags & SHF_MASKOS)
	    if (gnuld)
	      sh_flags &= ~(GElf_Xword) SHF_GNU_RETAIN;
	  if (sh_flags != 0)
	    ERROR (_("section [%2zu] '%s' contains unknown flag(s)"
			    " %#" PRIx64 "\n"),
		   cnt, section_name (ebl, cnt), sh_flags);
	}
      if (shdr->sh_flags & SHF_TLS)
	{
	  // XXX Correct?
	  if (shdr->sh_addr != 0 && !gnuld)
	    ERROR (_("\
section [%2zu] '%s': thread-local data sections address not zero\n"),
		   cnt, section_name (ebl, cnt));

	  // XXX TODO more tests!?
	}

      if (shdr->sh_flags & SHF_COMPRESSED)
	{
	  if (shdr->sh_flags & SHF_ALLOC)
	    ERROR (_("\
section [%2zu] '%s': allocated section cannot be compressed\n"),
		   cnt, section_name (ebl, cnt));

	  if (shdr->sh_type == SHT_NOBITS)
	    ERROR (_("\
section [%2zu] '%s': nobits section cannot be compressed\n"),
		   cnt, section_name (ebl, cnt));

	  GElf_Chdr chdr;
	  if (gelf_getchdr (scn, &chdr) == NULL)
	    ERROR (_("\
section [%2zu] '%s': compressed section with no compression header: %s\n"),
		   cnt, section_name (ebl, cnt), elf_errmsg (-1));
	}

      if (shdr->sh_link >= shnum)
	ERROR (_("\
section [%2zu] '%s': invalid section reference in link value\n"),
	       cnt, section_name (ebl, cnt));

      if (SH_INFO_LINK_P (shdr) && shdr->sh_info >= shnum)
	ERROR (_("\
section [%2zu] '%s': invalid section reference in info value\n"),
	       cnt, section_name (ebl, cnt));

      if ((shdr->sh_flags & SHF_MERGE) == 0
	  && (shdr->sh_flags & SHF_STRINGS) != 0
	  && be_strict)
	ERROR (_("\
section [%2zu] '%s': strings flag set without merge flag\n"),
	       cnt, section_name (ebl, cnt));

      if ((shdr->sh_flags & SHF_MERGE) != 0 && shdr->sh_entsize == 0)
	ERROR (_("\
section [%2zu] '%s': merge flag set but entry size is zero\n"),
	       cnt, section_name (ebl, cnt));

      if (shdr->sh_flags & SHF_GROUP)
	check_scn_group (ebl, cnt);

      if (shdr->sh_flags & SHF_EXECINSTR)
	{
	  switch (shdr->sh_type)
	    {
	    case SHT_PROGBITS:
	      break;

	    case SHT_NOBITS:
	      if (is_debuginfo)
		break;
	      FALLTHROUGH;
	    default:
	      ERROR (_("\
section [%2zu] '%s' has unexpected type %d for an executable section\n"),
		     cnt, section_name (ebl, cnt), shdr->sh_type);
	      break;
	    }

	  if (shdr->sh_flags & SHF_WRITE)
	    {
	      if (is_debuginfo && shdr->sh_type != SHT_NOBITS)
		ERROR (_("\
section [%2zu] '%s' must be of type NOBITS in debuginfo files\n"),
		       cnt, section_name (ebl, cnt));

	      if (!is_debuginfo
		  && !ebl_check_special_section (ebl, cnt, shdr,
						 section_name (ebl, cnt)))
		ERROR (_("\
section [%2zu] '%s' is both executable and writable\n"),
		       cnt, section_name (ebl, cnt));
	    }
	}

      if (ehdr->e_type != ET_REL && (shdr->sh_flags & SHF_ALLOC) != 0
	  && !is_debuginfo)
	{
	  /* Make sure the section is contained in a loaded segment
	     and that the initialization part matches NOBITS sections.  */
	  unsigned int pcnt;
	  GElf_Phdr phdr_mem;
	  GElf_Phdr *phdr;

	  for (pcnt = 0; pcnt < phnum; ++pcnt)
	    if ((phdr = gelf_getphdr (ebl->elf, pcnt, &phdr_mem)) != NULL
		&& ((phdr->p_type == PT_LOAD
		     && (shdr->sh_flags & SHF_TLS) == 0)
		    || (phdr->p_type == PT_TLS
			&& (shdr->sh_flags & SHF_TLS) != 0))
		&& phdr->p_offset <= shdr->sh_offset
		&& ((shdr->sh_offset - phdr->p_offset <= phdr->p_filesz
		     && (shdr->sh_offset - phdr->p_offset < phdr->p_filesz
			 || shdr->sh_size == 0))
		    || (shdr->sh_offset - phdr->p_offset < phdr->p_memsz
			&& shdr->sh_type == SHT_NOBITS)))
	      {
		/* Found the segment.  */
		if (phdr->p_offset + phdr->p_memsz
		    < shdr->sh_offset + shdr->sh_size)
		  ERROR (_("\
section [%2zu] '%s' not fully contained in segment of program header entry %d\n"),
			 cnt, section_name (ebl, cnt), pcnt);

		if (shdr->sh_type == SHT_NOBITS)
		  {
		    if (shdr->sh_offset < phdr->p_offset + phdr->p_filesz
			&& !is_debuginfo)
		      {
			if (!gnuld)
			  ERROR (_("\
section [%2zu] '%s' has type NOBITS but is read from the file in segment of program header entry %d\n"),
				 cnt, section_name (ebl, cnt), pcnt);
			else
			  {
			    /* This is truly horrible. GNU ld might put a
			       NOBITS section in the middle of a PT_LOAD
			       segment, assuming the next gap in the file
			       actually consists of zero bits...
			       So it really is like a PROGBITS section
			       where the data is all zeros.  Check those
			       zero bytes are really there.  */
			    bool bad;
			    Elf_Data *databits;
			    databits = elf_getdata_rawchunk (ebl->elf,
							     shdr->sh_offset,
							     shdr->sh_size,
							     ELF_T_BYTE);
			    bad = (databits == NULL
				   || databits->d_size != shdr->sh_size);
			    for (size_t idx = 0;
				 ! bad && idx < databits->d_size;
				 idx++)
			      bad = ((char *) databits->d_buf)[idx] != 0;

			    if (bad)
			      ERROR (_("\
section [%2zu] '%s' has type NOBITS but is read from the file in segment of program header entry %d and file contents is non-zero\n"),
				     cnt, section_name (ebl, cnt), pcnt);
			  }
		      }
		  }
		else
		  {
		    const GElf_Off end = phdr->p_offset + phdr->p_filesz;
		    if (shdr->sh_offset > end ||
			(shdr->sh_offset == end && shdr->sh_size != 0))
		      ERROR (_("\
section [%2zu] '%s' has not type NOBITS but is not read from the file in segment of program header entry %d\n"),
			 cnt, section_name (ebl, cnt), pcnt);
		  }

		if (shdr->sh_type != SHT_NOBITS)
		  {
		    if ((shdr->sh_flags & SHF_EXECINSTR) != 0)
		      {
			segment_flags[pcnt] |= PF_X;
			if ((phdr->p_flags & PF_X) == 0)
			  ERROR (_("\
section [%2zu] '%s' is executable in nonexecutable segment %d\n"),
				 cnt, section_name (ebl, cnt), pcnt);
		      }

		    if ((shdr->sh_flags & SHF_WRITE) != 0)
		      {
			segment_flags[pcnt] |= PF_W;
			if (0	/* XXX vdso images have this */
			    && (phdr->p_flags & PF_W) == 0)
			  ERROR (_("\
section [%2zu] '%s' is writable in unwritable segment %d\n"),
				 cnt, section_name (ebl, cnt), pcnt);
		      }
		  }

		break;
	      }

	  if (pcnt == phnum)
	    ERROR (_("\
section [%2zu] '%s': alloc flag set but section not in any loaded segment\n"),
		   cnt, section_name (ebl, cnt));
	}

      if (cnt == shstrndx && shdr->sh_type != SHT_STRTAB)
	ERROR (_("\
section [%2zu] '%s': ELF header says this is the section header string table but type is not SHT_TYPE\n"),
	       cnt, section_name (ebl, cnt));

      switch (shdr->sh_type)
	{
	case SHT_DYNSYM:
	  if (ehdr->e_type == ET_REL)
	    ERROR (_("\
section [%2zu] '%s': relocatable files cannot have dynamic symbol tables\n"),
		   cnt, section_name (ebl, cnt));
	  FALLTHROUGH;
	case SHT_SYMTAB:
	  check_symtab (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_RELA:
	  check_rela (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_REL:
	  check_rel (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_RELR:
	  check_relr (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_DYNAMIC:
	  check_dynamic (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_SYMTAB_SHNDX:
	  check_symtab_shndx (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_HASH:
	  check_hash (shdr->sh_type, ebl, ehdr, shdr, cnt);
	  hash_idx = cnt;
	  break;

	case SHT_GNU_HASH:
	  check_hash (shdr->sh_type, ebl, ehdr, shdr, cnt);
	  gnu_hash_idx = cnt;
	  break;

	case SHT_NULL:
	  check_null (ebl, shdr, cnt);
	  break;

	case SHT_GROUP:
	  check_group (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_NOTE:
	  check_note_section (ebl, ehdr, shdr, cnt);
	  break;

	case SHT_GNU_versym:
	  /* We cannot process this section now since we have no guarantee
	     that the verneed and verdef sections have already been read.
	     Just remember the section index.  */
	  if (versym_scnndx != 0)
	    ERROR (_("more than one version symbol table present\n"));
	  versym_scnndx = cnt;
	  break;

	case SHT_GNU_verneed:
	  check_verneed (ebl, shdr, cnt);
	  break;

	case SHT_GNU_verdef:
	  check_verdef (ebl, shdr, cnt);
	  break;

	case SHT_GNU_ATTRIBUTES:
	  check_attributes (ebl, ehdr, shdr, cnt);
	  break;

	default:
	  /* Nothing.  */
	  break;
	}
    }

  if (has_interp_segment && !dot_interp_section)
    ERROR (_("INTERP program header entry but no .interp section\n"));

  if (!is_debuginfo)
    for (unsigned int pcnt = 0; pcnt < phnum; ++pcnt)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr = gelf_getphdr (ebl->elf, pcnt, &phdr_mem);
	if (phdr != NULL && (phdr->p_type == PT_LOAD || phdr->p_type == PT_TLS))
	  {
	    if ((phdr->p_flags & PF_X) != 0
		&& (segment_flags[pcnt] & PF_X) == 0)
	      ERROR (_("\
loadable segment [%u] is executable but contains no executable sections\n"),
		     pcnt);

	    if ((phdr->p_flags & PF_W) != 0
		&& (segment_flags[pcnt] & PF_W) == 0)
	      ERROR (_("\
loadable segment [%u] is writable but contains no writable sections\n"),
		     pcnt);
	  }
      }

  free (segment_flags);

  if (version_namelist != NULL)
    {
      if (versym_scnndx == 0)
    ERROR (_("\
no .gnu.versym section present but .gnu.versym_d or .gnu.versym_r section exist\n"));
      else
	check_versym (ebl, versym_scnndx);

      /* Check for duplicate index numbers.  */
      do
	{
	  struct version_namelist *runp = version_namelist->next;
	  while (runp != NULL)
	    {
	      if (version_namelist->ndx == runp->ndx)
		{
		  ERROR (_("duplicate version index %d\n"),
			 (int) version_namelist->ndx);
		  break;
		}
	      runp = runp->next;
	    }

	  struct version_namelist *old = version_namelist;
	  version_namelist = version_namelist->next;
	  free (old);
	}
      while (version_namelist != NULL);
    }
  else if (versym_scnndx != 0)
    ERROR (_("\
.gnu.versym section present without .gnu.versym_d or .gnu.versym_r\n"));

  if (hash_idx != 0 && gnu_hash_idx != 0)
    compare_hash_gnu_hash (ebl, ehdr, hash_idx, gnu_hash_idx);

  free (scnref);
}


static GElf_Off
check_note_data (Ebl *ebl, const GElf_Ehdr *ehdr,
		 Elf_Data *data, int shndx, int phndx, GElf_Off start)
{
  size_t offset = 0;
  size_t last_offset = 0;
  GElf_Nhdr nhdr;
  size_t name_offset;
  size_t desc_offset;
  while (offset < data->d_size
	 && (offset = gelf_getnote (data, offset,
				    &nhdr, &name_offset, &desc_offset)) > 0)
    {
      last_offset = offset;

      /* Make sure it is one of the note types we know about.  */
      if (ehdr->e_type == ET_CORE)
	switch (nhdr.n_type)
	  {
	  case NT_PRSTATUS:
	  case NT_FPREGSET:
	  case NT_PRPSINFO:
	  case NT_TASKSTRUCT:		/* NT_PRXREG on Solaris.  */
	  case NT_PLATFORM:
	  case NT_AUXV:
	  case NT_GWINDOWS:
	  case NT_ASRS:
	  case NT_PSTATUS:
	  case NT_PSINFO:
	  case NT_PRCRED:
	  case NT_UTSNAME:
	  case NT_LWPSTATUS:
	  case NT_LWPSINFO:
	  case NT_PRFPXREG:
	    /* Known type.  */
	    break;

	  default:
	    if (shndx == 0)
	      ERROR (_("\
phdr[%d]: unknown core file note type %" PRIu32 " at offset %" PRIu64 "\n"),
		     phndx, (uint32_t) nhdr.n_type, start + offset);
	    else
	      ERROR (_("\
section [%2d] '%s': unknown core file note type %" PRIu32
			      " at offset %zu\n"),
		     shndx, section_name (ebl, shndx),
		     (uint32_t) nhdr.n_type, offset);
	  }
      else
	switch (nhdr.n_type)
	  {
	  case NT_GNU_ABI_TAG:
	  case NT_GNU_HWCAP:
	  case NT_GNU_BUILD_ID:
	  case NT_GNU_GOLD_VERSION:
	  case NT_GNU_PROPERTY_TYPE_0:
	    if (nhdr.n_namesz == sizeof ELF_NOTE_GNU
		&& strcmp (data->d_buf + name_offset, ELF_NOTE_GNU) == 0)
	      break;
	    else
	      {
		/* NT_VERSION is 1, same as NT_GNU_ABI_TAG.  It has no
		   descriptor and (ab)uses the name as version string.  */
		if (nhdr.n_descsz == 0 && nhdr.n_type == NT_VERSION)
		  break;
	      }
	      goto unknown_note;

	  case NT_GNU_BUILD_ATTRIBUTE_OPEN:
	  case NT_GNU_BUILD_ATTRIBUTE_FUNC:
	    /* GNU Build Attributes store most data in the owner
	       name, which must start with the
	       ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX "GA".  */
	    if (nhdr.n_namesz >= sizeof ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX
		&& strncmp (data->d_buf + name_offset,
			    ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX,
			    strlen (ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX)) == 0)
	      break;
	    else
	      goto unknown_note;

	  case NT_FDO_PACKAGING_METADATA:
	    if (nhdr.n_namesz == sizeof ELF_NOTE_FDO
		&& strcmp (data->d_buf + name_offset, ELF_NOTE_FDO) == 0)
	      break;
	    else
	      goto unknown_note;

	  case 0:
	    /* Linux vDSOs use a type 0 note for the kernel version word.  */
	    if (nhdr.n_namesz == sizeof "Linux"
		&& !memcmp (data->d_buf + name_offset, "Linux", sizeof "Linux"))
	      break;
	    FALLTHROUGH;
	  default:
	    {
	    unknown_note:
	    if (shndx == 0)
	      ERROR (_("\
phdr[%d]: unknown object file note type %" PRIu32 " with owner name '%s' at offset %zu\n"),
		     phndx, (uint32_t) nhdr.n_type,
		     (char *) data->d_buf + name_offset, offset);
	    else
	      ERROR (_("\
section [%2d] '%s': unknown object file note type %" PRIu32
			      " with owner name '%s' at offset %zu\n"),
		     shndx, section_name (ebl, shndx),
		     (uint32_t) nhdr.n_type,
		     (char *) data->d_buf + name_offset, offset);
	    }
	  }
    }

  return last_offset;
}


static void
check_note (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Phdr *phdr, int cnt)
{
  if (ehdr->e_type != ET_CORE && ehdr->e_type != ET_REL
      && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    ERROR (_("\
phdr[%d]: no note entries defined for the type of file\n"),
	   cnt);

  if (is_debuginfo)
    /* The p_offset values in a separate debug file are bogus.  */
    return;

  if (phdr->p_filesz == 0)
    return;

  GElf_Off notes_size = 0;
  Elf_Data *data = elf_getdata_rawchunk (ebl->elf,
					 phdr->p_offset, phdr->p_filesz,
					 (phdr->p_align == 8
					  ? ELF_T_NHDR8 : ELF_T_NHDR));
  if (data != NULL && data->d_buf != NULL)
    notes_size = check_note_data (ebl, ehdr, data, 0, cnt, phdr->p_offset);

  if (notes_size == 0)
    ERROR (_("phdr[%d]: cannot get content of note section: %s\n"),
	   cnt, elf_errmsg (-1));
  else if (notes_size != phdr->p_filesz)
    ERROR (_("phdr[%d]: extra %" PRIu64 " bytes after last note\n"),
	   cnt, phdr->p_filesz - notes_size);
}


static void
check_note_section (Ebl *ebl, GElf_Ehdr *ehdr, GElf_Shdr *shdr, int idx)
{
  if (shdr->sh_size == 0)
    return;

  Elf_Data *data = elf_getdata (elf_getscn (ebl->elf, idx), NULL);
  if (data == NULL || data->d_buf == NULL)
    {
      ERROR (_("section [%2d] '%s': cannot get section data\n"),
	     idx, section_name (ebl, idx));
      return;
    }

  if (ehdr->e_type != ET_CORE && ehdr->e_type != ET_REL
      && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    ERROR (_("\
section [%2d] '%s': no note entries defined for the type of file\n"),
	     idx, section_name (ebl, idx));

  GElf_Off notes_size = check_note_data (ebl, ehdr, data, idx, 0, 0);

  if (notes_size == 0)
    ERROR (_("section [%2d] '%s': cannot get content of note section\n"),
	   idx, section_name (ebl, idx));
  else if (notes_size != shdr->sh_size)
    ERROR (_("section [%2d] '%s': extra %" PRIu64
		    " bytes after last note\n"),
	   idx, section_name (ebl, idx), shdr->sh_size - notes_size);
}


/* Index of the PT_GNU_EH_FRAME program eader entry.  */
static int pt_gnu_eh_frame_pndx;


static void
check_program_header (Ebl *ebl, GElf_Ehdr *ehdr)
{
  if (ehdr->e_phoff == 0)
    return;

  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN
      && ehdr->e_type != ET_CORE)
    ERROR (_("\
only executables, shared objects, and core files can have program headers\n"));

  int num_pt_interp = 0;
  int num_pt_tls = 0;
  int num_pt_relro = 0;

  for (unsigned int cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr;

      phdr = gelf_getphdr (ebl->elf, cnt, &phdr_mem);
      if (phdr == NULL)
	{
	  ERROR (_("cannot get program header entry %d: %s\n"),
		 cnt, elf_errmsg (-1));
	  continue;
	}

      if (phdr->p_type >= PT_NUM && phdr->p_type != PT_GNU_EH_FRAME
	  && phdr->p_type != PT_GNU_STACK && phdr->p_type != PT_GNU_RELRO
	  && phdr->p_type != PT_GNU_PROPERTY
	  /* Check for a known machine-specific type.  */
	  && ebl_segment_type_name (ebl, phdr->p_type, NULL, 0) == NULL)
	ERROR (_("\
program header entry %d: unknown program header entry type %#" PRIx64 "\n"),
	       cnt, (uint64_t) phdr->p_type);

      if (phdr->p_type == PT_LOAD)
	has_loadable_segment = true;
      else if (phdr->p_type == PT_INTERP)
	{
	  if (++num_pt_interp != 1)
	    {
	      if (num_pt_interp == 2)
		ERROR (_("\
more than one INTERP entry in program header\n"));
	    }
	  has_interp_segment = true;
	}
      else if (phdr->p_type == PT_TLS)
	{
	  if (++num_pt_tls == 2)
	    ERROR (_("more than one TLS entry in program header\n"));
	}
      else if (phdr->p_type == PT_NOTE)
	check_note (ebl, ehdr, phdr, cnt);
      else if (phdr->p_type == PT_DYNAMIC)
	{
	  if (ehdr->e_type == ET_EXEC && ! has_interp_segment)
	    ERROR (_("\
static executable cannot have dynamic sections\n"));
	  else
	    {
	      /* Check that the .dynamic section, if it exists, has
		 the same address.  */
	      Elf_Scn *scn = NULL;
	      while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
		{
		  GElf_Shdr shdr_mem;
		  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
		  if (shdr != NULL && shdr->sh_type == SHT_DYNAMIC)
		    {
		      if (phdr->p_offset != shdr->sh_offset)
			ERROR (_("\
dynamic section reference in program header has wrong offset\n"));
		      if (phdr->p_memsz != shdr->sh_size)
			ERROR (_("\
dynamic section size mismatch in program and section header\n"));
		      break;
		    }
		}
	    }
	}
      else if (phdr->p_type == PT_GNU_RELRO)
	{
	  if (++num_pt_relro == 2)
	    ERROR (_("\
more than one GNU_RELRO entry in program header\n"));
	  else
	    {
	      /* Check that the region is in a writable segment.  */
	      unsigned int inner;
	      for (inner = 0; inner < phnum; ++inner)
		{
		  GElf_Phdr phdr2_mem;
		  GElf_Phdr *phdr2;

		  phdr2 = gelf_getphdr (ebl->elf, inner, &phdr2_mem);
		  if (phdr2 == NULL)
		    continue;

		  if (phdr2->p_type == PT_LOAD
		      && phdr->p_vaddr >= phdr2->p_vaddr
		      && (phdr->p_vaddr + phdr->p_memsz
			  <= phdr2->p_vaddr + phdr2->p_memsz))
		    {
		      if ((phdr2->p_flags & PF_W) == 0)
			ERROR (_("\
loadable segment GNU_RELRO applies to is not writable\n"));
		      /* Unless fully covered, relro flags could be a
			 subset of the phdrs2 flags.  For example the load
			 segment could also have PF_X set.  */
		      if (phdr->p_vaddr == phdr2->p_vaddr
			  && (phdr->p_vaddr + phdr->p_memsz
			      == phdr2->p_vaddr + phdr2->p_memsz))
			{
			  if ((phdr2->p_flags & ~PF_W)
			      != (phdr->p_flags & ~PF_W))
			    ERROR (_("\
loadable segment [%u] flags do not match GNU_RELRO [%u] flags\n"),
				   cnt, inner);
			}
		      else
			{
			  if ((phdr->p_flags & ~phdr2->p_flags) != 0)
			    ERROR (_("\
GNU_RELRO [%u] flags are not a subset of the loadable segment [%u] flags\n"),
				   inner, cnt);
			}
		      break;
		    }
		}

	      if (inner >= phnum)
		ERROR (_("\
%s segment not contained in a loaded segment\n"), "GNU_RELRO");
	    }
	}
      else if (phdr->p_type == PT_PHDR)
	{
	  /* Check that the region is in a writable segment.  */
	  unsigned int inner;
	  for (inner = 0; inner < phnum; ++inner)
	    {
	      GElf_Phdr phdr2_mem;
	      GElf_Phdr *phdr2;

	      phdr2 = gelf_getphdr (ebl->elf, inner, &phdr2_mem);
	      if (phdr2 != NULL
		  && phdr2->p_type == PT_LOAD
		  && phdr->p_vaddr >= phdr2->p_vaddr
		  && (phdr->p_vaddr + phdr->p_memsz
		      <= phdr2->p_vaddr + phdr2->p_memsz))
		break;
	    }

	  if (inner >= phnum)
	    ERROR (_("\
%s segment not contained in a loaded segment\n"), "PHDR");

	  /* Check that offset in segment corresponds to offset in ELF
	     header.  */
	  if (phdr->p_offset != ehdr->e_phoff)
	    ERROR (_("\
program header offset in ELF header and PHDR entry do not match"));
	}
      else if (phdr->p_type == PT_GNU_EH_FRAME)
	{
	  /* If there is an .eh_frame_hdr section it must be
	     referenced by this program header entry.  */
	  Elf_Scn *scn = NULL;
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = NULL;
	  bool any = false;
	  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
	    {
	      any = true;
	      shdr = gelf_getshdr (scn, &shdr_mem);
	      if (shdr != NULL
		  && ((is_debuginfo && shdr->sh_type == SHT_NOBITS)
		      || (! is_debuginfo
			  && (shdr->sh_type == SHT_PROGBITS
			      || shdr->sh_type == SHT_X86_64_UNWIND)))
		  && elf_strptr (ebl->elf, shstrndx, shdr->sh_name) != NULL
		  && ! strcmp (".eh_frame_hdr",
			       elf_strptr (ebl->elf, shstrndx, shdr->sh_name)))
		{
		  if (! is_debuginfo)
		    {
		      if (phdr->p_offset != shdr->sh_offset)
			ERROR (_("\
call frame search table reference in program header has wrong offset\n"));
		      if (phdr->p_memsz != shdr->sh_size)
			ERROR (_("\
call frame search table size mismatch in program and section header\n"));
		    }
		  break;
		}
	    }

	  if (scn == NULL)
	    {
	      /* If there is no section header table we don't
		 complain.  But if there is one there should be an
		 entry for .eh_frame_hdr.  */
	      if (any)
		ERROR (_("\
PT_GNU_EH_FRAME present but no .eh_frame_hdr section\n"));
	    }
	  else
	    {
	      /* The section must be allocated and not be writable and
		 executable.  */
	      if ((phdr->p_flags & PF_R) == 0)
		ERROR (_("\
call frame search table must be allocated\n"));
	      else if (shdr != NULL && (shdr->sh_flags & SHF_ALLOC) == 0)
		ERROR (_("\
section [%2zu] '%s' must be allocated\n"), elf_ndxscn (scn), ".eh_frame_hdr");

	      if ((phdr->p_flags & PF_W) != 0)
		ERROR (_("\
call frame search table must not be writable\n"));
	      else if (shdr != NULL && (shdr->sh_flags & SHF_WRITE) != 0)
		ERROR (_("\
section [%2zu] '%s' must not be writable\n"),
		       elf_ndxscn (scn), ".eh_frame_hdr");

	      if ((phdr->p_flags & PF_X) != 0)
		ERROR (_("\
call frame search table must not be executable\n"));
	      else if (shdr != NULL && (shdr->sh_flags & SHF_EXECINSTR) != 0)
		ERROR (_("\
section [%2zu] '%s' must not be executable\n"),
		       elf_ndxscn (scn), ".eh_frame_hdr");
	    }

	  /* Remember which entry this is.  */
	  pt_gnu_eh_frame_pndx = cnt;
	}

      if (phdr->p_filesz > phdr->p_memsz
	  && (phdr->p_memsz != 0
	      || (phdr->p_type != PT_NOTE
		  && !(ehdr->e_machine == EM_RISCV
		       && phdr->p_type == PT_RISCV_ATTRIBUTES))))
	ERROR (_("\
program header entry %d: file size greater than memory size\n"),
	       cnt);

      if (phdr->p_align > 1)
	{
	  if (!powerof2 (phdr->p_align))
	    ERROR (_("\
program header entry %d: alignment not a power of 2\n"), cnt);
	  else if ((phdr->p_vaddr - phdr->p_offset) % phdr->p_align != 0)
	    ERROR (_("\
program header entry %d: file offset and virtual address not module of alignment\n"), cnt);
	}
    }
}


static void
check_exception_data (Ebl *ebl __attribute__ ((unused)),
		      GElf_Ehdr *ehdr __attribute__ ((unused)))
{
  if ((ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)
      && pt_gnu_eh_frame_pndx == 0 && eh_frame_hdr_scnndx != 0)
    ERROR (_("executable/DSO with .eh_frame_hdr section does not have "
		    "a PT_GNU_EH_FRAME program header entry"));
}


/* Process one file.  */
static void
process_elf_file (Elf *elf, const char *prefix, const char *suffix,
		  const char *fname, size_t size, bool only_one)
{
  /* Reset variables.  */
  ndynamic = 0;
  nverneed = 0;
  nverdef = 0;
  textrel = false;
  needed_textrel = false;
  has_loadable_segment = false;
  has_interp_segment = false;

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  Ebl *ebl;

  /* Print the file name.  */
  if (!only_one)
    {
      if (prefix != NULL)
	printf ("\n%s(%s)%s:\n", prefix, fname, suffix);
      else
	printf ("\n%s:\n", fname);
    }

  if (ehdr == NULL)
    {
      ERROR (_("cannot read ELF header: %s\n"), elf_errmsg (-1));
      return;
    }

  ebl = ebl_openbackend (elf);
  /* If there is no appropriate backend library we cannot test
     architecture and OS specific features.  Any encountered extension
     is an error.  Often we'll get a "dummy" ebl, except if something
     really bad happen, like a totally corrupted ELF file or out of
     memory situation.  */
  if (ebl == NULL)
    {
      ERROR (_("cannot create backend for ELF file\n"));
      return;
    }

  /* Go straight by the gABI, check all the parts in turn.  */
  check_elf_header (ebl, ehdr, size);

  /* Check the program header.  */
  check_program_header (ebl, ehdr);

  /* Next the section headers.  It is OK if there are no section
     headers at all.  */
  check_sections (ebl, ehdr);

  /* Check the exception handling data, if it exists.  */
  if (pt_gnu_eh_frame_pndx != 0 || eh_frame_hdr_scnndx != 0
      || eh_frame_scnndx != 0 || gcc_except_table_scnndx != 0)
    check_exception_data (ebl, ehdr);

  /* Report if no relocation section needed the text relocation flag.  */
  if (textrel && !needed_textrel)
    ERROR (_("text relocation flag set but not needed\n"));

  /* Free the resources.  */
  ebl_closebackend (ebl);
}


#include "debugpred.h"
