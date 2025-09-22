/* Print contents of object file note.
   Copyright (C) 2002, 2007, 2009, 2011, 2015, 2016, 2018 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libeblP.h>

#include "common.h"
#include "libelfP.h"
#include "libdwP.h"
#include "memory-access.h"


void
ebl_object_note (Ebl *ebl, uint32_t namesz, const char *name, uint32_t type,
		 uint32_t descsz, const char *desc)
{
  if (! ebl->object_note (name, type, descsz, desc))
    {
      /* The machine specific function did not know this type.  */

      if (strcmp ("stapsdt", name) == 0)
	{
	  if (type != 3)
	    {
	      printf (_("unknown SDT version %u\n"), type);
	      return;
	    }

	  /* Descriptor starts with three addresses, pc, base ref and
	     semaphore.  Then three zero terminated strings provider,
	     name and arguments.  */

	  union
	  {
	    Elf64_Addr a64[3];
	    Elf32_Addr a32[3];
	  } addrs;

	  size_t addrs_size = gelf_fsize (ebl->elf, ELF_T_ADDR, 3, EV_CURRENT);
	  if (descsz < addrs_size + 3)
	    {
	    invalid_sdt:
	      printf (_("invalid SDT probe descriptor\n"));
	      return;
	    }

	  Elf_Data src =
	    {
	      .d_type = ELF_T_ADDR, .d_version = EV_CURRENT,
	      .d_buf = (void *) desc, .d_size = addrs_size
	    };

	  Elf_Data dst =
	    {
	      .d_type = ELF_T_ADDR, .d_version = EV_CURRENT,
	      .d_buf = &addrs, .d_size = addrs_size
	    };

	  if (gelf_xlatetom (ebl->elf, &dst, &src,
			     elf_getident (ebl->elf, NULL)[EI_DATA]) == NULL)
	    {
	      printf ("%s\n", elf_errmsg (-1));
	      return;
	    }

	  const char *provider = desc + addrs_size;
	  const char *pname = memchr (provider, '\0', desc + descsz - provider);
	  if (pname == NULL)
	    goto invalid_sdt;

	  ++pname;
	  const char *args = memchr (pname, '\0', desc + descsz - pname);
	  if (args == NULL ||
	      memchr (++args, '\0', desc + descsz - pname) != desc + descsz - 1)
	    goto invalid_sdt;

	  GElf_Addr pc;
	  GElf_Addr base;
	  GElf_Addr sem;
	  if (gelf_getclass (ebl->elf) == ELFCLASS32)
	    {
	      pc = addrs.a32[0];
	      base = addrs.a32[1];
	      sem = addrs.a32[2];
	    }
	  else
	    {
	      pc = addrs.a64[0];
	      base = addrs.a64[1];
	      sem = addrs.a64[2];
	    }

	  printf (_("    PC: "));
	  printf ("%#" PRIx64 ",", pc);
	  printf (_(" Base: "));
	  printf ("%#" PRIx64 ",", base);
	  printf (_(" Semaphore: "));
	  printf ("%#" PRIx64 "\n", sem);
	  printf (_("    Provider: "));
	  printf ("%s,", provider);
	  printf (_(" Name: "));
	  printf ("%s,", pname);
	  printf (_(" Args: "));
	  printf ("'%s'\n", args);
	  return;
	}

      if (strncmp (name, ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX,
		   strlen (ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX)) == 0
	  && (type == NT_GNU_BUILD_ATTRIBUTE_OPEN
	      || type == NT_GNU_BUILD_ATTRIBUTE_FUNC))
	{
	  /* There might or might not be a pair of addresses in the desc.  */
	  if (descsz > 0)
	    {
	      printf ("    Address Range: ");

	      union
	      {
		Elf64_Addr a64[2];
		Elf32_Addr a32[2];
	      } addrs;

	      size_t addr_size = gelf_fsize (ebl->elf, ELF_T_ADDR,
					     1, EV_CURRENT);
	      if (descsz != addr_size * 2)
		printf ("<unknown data>\n");
	      else
		{
		  Elf_Data src =
		    {
		     .d_type = ELF_T_ADDR, .d_version = EV_CURRENT,
		     .d_buf = (void *) desc, .d_size = descsz
		    };

		  Elf_Data dst =
		    {
		     .d_type = ELF_T_ADDR, .d_version = EV_CURRENT,
		     .d_buf = &addrs, .d_size = descsz
		    };

		  if (gelf_xlatetom (ebl->elf, &dst, &src,
				     elf_getident (ebl->elf,
						   NULL)[EI_DATA]) == NULL)
		    printf ("%s\n", elf_errmsg (-1));
		  else
		    {
		      if (addr_size == 4)
			printf ("%#" PRIx32 " - %#" PRIx32 "\n",
				addrs.a32[0], addrs.a32[1]);
		      else
			printf ("%#" PRIx64 " - %#" PRIx64 "\n",
				addrs.a64[0], addrs.a64[1]);
		    }
		}
	    }

	  /* Most data actually is inside the name.
	     https://fedoraproject.org/wiki/Toolchain/Watermark  */

	  /* We need at least 2 chars of data to describe the
	     attribute and value encodings.  */
	  const char *data = (name
			      + strlen (ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX));
	  if (namesz < 2)
	    {
	      printf ("<insufficient data>\n");
	      return;
	    }

	  printf ("    ");

	  /* In most cases the value comes right after the encoding bytes.  */
	  const char *value = &data[2];
	  switch (data[1])
	    {
	    case GNU_BUILD_ATTRIBUTE_VERSION:
	      printf ("VERSION: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_STACK_PROT:
	      printf ("STACK_PROT: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_RELRO:
	      printf ("RELRO: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_STACK_SIZE:
	      printf ("STACK_SIZE: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_TOOL:
	      printf ("TOOL: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_ABI:
	      printf ("ABI: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_PIC:
	      printf ("PIC: ");
	      break;
	    case GNU_BUILD_ATTRIBUTE_SHORT_ENUM:
	      printf ("SHORT_ENUM: ");
	      break;
	    case 32 ... 126:
	      printf ("\"%s\": ", &data[1]);
	      value += strlen (&data[1]) + 1;
	      break;
	    default:
	      printf ("<unknown>: ");
	      break;
	    }

	  switch (data[0])
	    {
	    case GNU_BUILD_ATTRIBUTE_TYPE_NUMERIC:
	      {
		/* Any numbers are always in (unsigned) little endian.  */
		static const Dwarf dbg
		  = { .other_byte_order = MY_ELFDATA != ELFDATA2LSB };
		size_t bytes = namesz - (value - name);
		uint64_t val;
		if (bytes == 1)
		  val = *(unsigned char *) value;
		else if (bytes == 2)
		  val = read_2ubyte_unaligned (&dbg, value);
		else if (bytes == 4)
		  val = read_4ubyte_unaligned (&dbg, value);
		else if (bytes == 8)
		  val = read_8ubyte_unaligned (&dbg, value);
		else
		  goto unknown;
		printf ("%" PRIx64, val);
	      }
	      break;
	    case GNU_BUILD_ATTRIBUTE_TYPE_STRING:
	      printf ("\"%s\"", value);
	      break;
	    case GNU_BUILD_ATTRIBUTE_TYPE_BOOL_TRUE:
	      printf ("TRUE");
	      break;
	    case GNU_BUILD_ATTRIBUTE_TYPE_BOOL_FALSE:
	      printf ("FALSE");
	      break;
	    default:
	      {
	      unknown:
		printf ("<unknown>");
	      }
	      break;
	    }

	  printf ("\n");

	  return;
	}

      /* NT_VERSION doesn't have any info.  All data is in the name.  */
      if (descsz == 0 && type == NT_VERSION)
	return;

      if (strcmp ("FDO", name) == 0 && type == NT_FDO_PACKAGING_METADATA
	  && descsz > 0 && desc[descsz - 1] == '\0')
	printf("    Packaging Metadata: %.*s\n", (int) descsz, desc);

      /* Everything else should have the "GNU" owner name.  */
      if (strcmp ("GNU", name) != 0)
	return;

      switch (type)
	{
	case NT_GNU_BUILD_ID:
	  if (strcmp (name, "GNU") == 0 && descsz > 0)
	    {
	      printf (_("    Build ID: "));
	      uint_fast32_t i;
	      for (i = 0; i < descsz - 1; ++i)
		printf ("%02" PRIx8, (uint8_t) desc[i]);
	      printf ("%02" PRIx8 "\n", (uint8_t) desc[i]);
	    }
	  break;

	case NT_GNU_GOLD_VERSION:
	  if (strcmp (name, "GNU") == 0 && descsz > 0)
	    /* A non-null terminated version string.  */
	    printf (_("    Linker version: %.*s\n"),
		    (int) descsz, desc);
	  break;

	case NT_GNU_PROPERTY_TYPE_0:
	  if (strcmp (name, "GNU") == 0 && descsz > 0)
	    {
	      /* There are at least 2 words. type and datasz.  */
	      while (descsz >= 8)
		{
		  struct pr_prop
		  {
		    GElf_Word pr_type;
		    GElf_Word pr_datasz;
		  } prop;

		  Elf_Data in =
		    {
		      .d_version = EV_CURRENT,
		      .d_type = ELF_T_WORD,
		      .d_size = 8,
		      .d_buf = (void *) desc
		    };
		  Elf_Data out =
		    {
		      .d_version = EV_CURRENT,
		      .d_type = ELF_T_WORD,
		      .d_size = descsz,
		      .d_buf = (void *) &prop
		    };

		  if (gelf_xlatetom (ebl->elf, &out, &in,
				     elf_getident (ebl->elf,
						   NULL)[EI_DATA]) == NULL)
		    {
		      printf ("%s\n", elf_errmsg (-1));
		      return;
		    }

		  desc += 8;
		  descsz -= 8;

		  if (prop.pr_datasz > descsz)
		    {
		      printf ("BAD property datasz: %" PRId32 "\n",
			      prop.pr_datasz);
		      return;
		    }

		  int elfclass = gelf_getclass (ebl->elf);
		  char *elfident = elf_getident (ebl->elf, NULL);
		  GElf_Ehdr ehdr;
		  gelf_getehdr (ebl->elf, &ehdr);

		  /* Prefix.  */
		  printf ("    ");
		  if (prop.pr_type == GNU_PROPERTY_STACK_SIZE)
		    {
		      printf ("STACK_SIZE ");
		      union
			{
			  Elf64_Addr a64;
			  Elf32_Addr a32;
			} addr;
		      if ((elfclass == ELFCLASS32 && prop.pr_datasz == 4)
			  || (elfclass == ELFCLASS64 && prop.pr_datasz == 8))
			{
			  in.d_type = ELF_T_ADDR;
			  out.d_type = ELF_T_ADDR;
			  in.d_size = prop.pr_datasz;
			  out.d_size = prop.pr_datasz;
			  in.d_buf = (void *) desc;
			  out.d_buf = (elfclass == ELFCLASS32
				       ? (void *) &addr.a32
				       : (void *) &addr.a64);

			  if (gelf_xlatetom (ebl->elf, &out, &in,
					     elfident[EI_DATA]) == NULL)
			    {
			      printf ("%s\n", elf_errmsg (-1));
			      return;
			    }
			  if (elfclass == ELFCLASS32)
			    printf ("%#" PRIx32 "\n", addr.a32);
			  else
			    printf ("%#" PRIx64 "\n", addr.a64);
			}
		      else
			printf (" (garbage datasz: %" PRIx32 ")\n",
				prop.pr_datasz);
		    }
		  else if (prop.pr_type == GNU_PROPERTY_NO_COPY_ON_PROTECTED)
		    {
		      printf ("NO_COPY_ON_PROTECTION");
		      if (prop.pr_datasz == 0)
			printf ("\n");
		      else
			printf (" (garbage datasz: %" PRIx32 ")\n",
				prop.pr_datasz);
		    }
		  else if (prop.pr_type >= GNU_PROPERTY_LOPROC
		      && prop.pr_type <= GNU_PROPERTY_HIPROC
		      && (ehdr.e_machine == EM_386
			  || ehdr.e_machine == EM_X86_64))
		    {
		      printf ("X86 ");
		      if (prop.pr_type == GNU_PROPERTY_X86_FEATURE_1_AND)
			{
			  printf ("FEATURE_1_AND: ");

			  if (prop.pr_datasz == 4)
			    {
			      GElf_Word data;
			      in.d_type = ELF_T_WORD;
			      out.d_type = ELF_T_WORD;
			      in.d_size = 4;
			      out.d_size = 4;
			      in.d_buf = (void *) desc;
			      out.d_buf = (void *) &data;

			      if (gelf_xlatetom (ebl->elf, &out, &in,
						 elfident[EI_DATA]) == NULL)
				{
				  printf ("%s\n", elf_errmsg (-1));
				  return;
				}
			      printf ("%08" PRIx32 " ", data);

			      if ((data & GNU_PROPERTY_X86_FEATURE_1_IBT)
				  != 0)
				{
				  printf ("IBT");
				  data &= ~GNU_PROPERTY_X86_FEATURE_1_IBT;
				  if (data != 0)
				    printf (" ");
				}

			      if ((data & GNU_PROPERTY_X86_FEATURE_1_SHSTK)
				  != 0)
				{
				  printf ("SHSTK");
				  data &= ~GNU_PROPERTY_X86_FEATURE_1_SHSTK;
				  if (data != 0)
				    printf (" ");
				}

			      if (data != 0)
				printf ("UNKNOWN");
			    }
			  else
			    printf ("<bad datasz: %" PRId32 ">",
				    prop.pr_datasz);

			  printf ("\n");
			}
		      else
			{
			  printf ("%#" PRIx32, prop.pr_type);
			  if (prop.pr_datasz > 0)
			    {
			      printf (" data: ");
			      size_t i;
			      for (i = 0; i < prop.pr_datasz - 1; i++)
				printf ("%02" PRIx8 " ", (uint8_t) desc[i]);
			      printf ("%02" PRIx8 "\n", (uint8_t) desc[i]);
			    }
			}
		    }
		  else if (prop.pr_type >= GNU_PROPERTY_LOPROC
			   && prop.pr_type <= GNU_PROPERTY_HIPROC
			   && ehdr.e_machine == EM_AARCH64)
		    {
		      printf ("AARCH64 ");
		      if (prop.pr_type == GNU_PROPERTY_AARCH64_FEATURE_1_AND)
			{
			  printf ("FEATURE_1_AND: ");

			  if (prop.pr_datasz == 4)
			    {
			      GElf_Word data;
			      in.d_type = ELF_T_WORD;
			      out.d_type = ELF_T_WORD;
			      in.d_size = 4;
			      out.d_size = 4;
			      in.d_buf = (void *) desc;
			      out.d_buf = (void *) &data;

			      if (gelf_xlatetom (ebl->elf, &out, &in,
						 elfident[EI_DATA]) == NULL)
				{
				  printf ("%s\n", elf_errmsg (-1));
				  return;
				}
			      printf ("%08" PRIx32 " ", data);

			      if ((data & GNU_PROPERTY_AARCH64_FEATURE_1_BTI)
				  != 0)
				{
				  printf ("BTI");
				  data &= ~GNU_PROPERTY_AARCH64_FEATURE_1_BTI;
				  if (data != 0)
				    printf (" ");
				}

			      if ((data & GNU_PROPERTY_AARCH64_FEATURE_1_PAC)
				  != 0)
				{
				  printf ("PAC");
				  data &= ~GNU_PROPERTY_AARCH64_FEATURE_1_PAC;
				  if (data != 0)
				    printf (" ");
				}

			      if (data != 0)
				printf ("UNKNOWN");
			    }
			  else
			    printf ("<bad datasz: %" PRId32 ">",
				    prop.pr_datasz);

			  printf ("\n");
			}
		      else
			{
			  printf ("%#" PRIx32, prop.pr_type);
			  if (prop.pr_datasz > 0)
			    {
			      printf (" data: ");
			      size_t i;
			      for (i = 0; i < prop.pr_datasz - 1; i++)
				printf ("%02" PRIx8 " ", (uint8_t) desc[i]);
			      printf ("%02" PRIx8 "\n", (uint8_t) desc[i]);
			    }
			}
		    }
		  else
		    {
		      if (prop.pr_type >= GNU_PROPERTY_LOPROC
			  && prop.pr_type <= GNU_PROPERTY_HIPROC)
			printf ("proc_type %#" PRIx32, prop.pr_type);
		      else if (prop.pr_type >= GNU_PROPERTY_LOUSER
			  && prop.pr_type <= GNU_PROPERTY_HIUSER)
			printf ("app_type %#" PRIx32, prop.pr_type);
		      else
			printf ("unknown_type %#" PRIx32, prop.pr_type);

		      if (prop.pr_datasz > 0)
			{
			  printf (" data: ");
			  size_t i;
			  for (i = 0; i < prop.pr_datasz - 1; i++)
			    printf ("%02" PRIx8 " ", (uint8_t) desc[i]);
			  printf ("%02" PRIx8 "\n", (uint8_t) desc[i]);
			}
		    }

		  if (elfclass == ELFCLASS32)
		    prop.pr_datasz = NOTE_ALIGN4 (prop.pr_datasz);
		  else
		    prop.pr_datasz = NOTE_ALIGN8 (prop.pr_datasz);

		  desc += prop.pr_datasz;
		  if (descsz > prop.pr_datasz)
		    descsz -= prop.pr_datasz;
		  else
		    descsz = 0;
		}
	    }
	  break;

	case NT_GNU_ABI_TAG:
	  if (descsz >= 8 && descsz % 4 == 0)
	    {
	      Elf_Data in =
		{
		  .d_version = EV_CURRENT,
		  .d_type = ELF_T_WORD,
		  .d_size = descsz,
		  .d_buf = (void *) desc
		};
	      /* Normally NT_GNU_ABI_TAG is just 4 words (16 bytes).  If it
		 is much (4*) larger dynamically allocate memory to convert.  */
#define FIXED_TAG_BYTES 16
	      uint32_t sbuf[FIXED_TAG_BYTES];
	      uint32_t *buf;
	      if (unlikely (descsz / 4 > FIXED_TAG_BYTES))
		{
		  buf = malloc (descsz);
		  if (unlikely (buf == NULL))
		    return;
		}
	      else
		buf = sbuf;
	      Elf_Data out =
		{
		  .d_version = EV_CURRENT,
		  .d_type = ELF_T_WORD,
		  .d_size = descsz,
		  .d_buf = buf
		};

	      if (elf32_xlatetom (&out, &in, ebl->data) != NULL)
		{
		  const char *os;
		  switch (buf[0])
		    {
		    case ELF_NOTE_OS_LINUX:
		      os = "Linux";
		      break;

		    case ELF_NOTE_OS_GNU:
		      os = "GNU";
		      break;

		    case ELF_NOTE_OS_SOLARIS2:
		      os = "Solaris";
		      break;

		    case ELF_NOTE_OS_FREEBSD:
		      os = "FreeBSD";
		      break;

		    default:
		      os = "???";
		      break;
		    }

		  printf (_("    OS: %s, ABI: "), os);
		  for (size_t cnt = 1; cnt < descsz / 4; ++cnt)
		    {
		      if (cnt > 1)
			putchar_unlocked ('.');
		      printf ("%" PRIu32, buf[cnt]);
		    }
		  putchar_unlocked ('\n');
		}
	      if (descsz / 4 > FIXED_TAG_BYTES)
		free (buf);
	      break;
	    }
	  FALLTHROUGH;

	default:
	  /* Unknown type.  */
	  break;
	}
    }
}
