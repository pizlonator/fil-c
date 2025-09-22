/* Copyright (C) 1998-2002, 2004, 2006, 2012, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <config.h>

#include <dwarf.h>
#include <inttypes.h>
#include <libelf.h>
#include ELFUTILS_HEADER(dw)
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../libdw/known-dwarf.h"
#include "../lib/system.h"

static const char *
dwarf_tag_string (unsigned int tag)
{
  switch (tag)
    {
#define DWARF_ONE_KNOWN_DW_TAG(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_TAG
#undef DWARF_ONE_KNOWN_DW_TAG
    default:
      return NULL;
    }
}

static const char *
dwarf_attr_string (unsigned int attrnum)
{
  switch (attrnum)
    {
#define DWARF_ONE_KNOWN_DW_AT(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_AT
#undef DWARF_ONE_KNOWN_DW_AT
    default:
      return NULL;
    }
}


void
handle (Dwarf *dbg, Dwarf_Die *die, int n)
{
  Dwarf_Die child;
  unsigned int tag;
  const char *str;
  char buf[30];
  const char *name;
  Dwarf_Off off;
  Dwarf_Off cuoff;
  size_t cnt;
  Dwarf_Addr addr;
  int i;

  tag = dwarf_tag (die);
  if (tag != DW_TAG_invalid)
    {
      str = dwarf_tag_string (tag);
      if (str == NULL)
	{
	  snprintf (buf, sizeof buf, "%#x", tag);
	  str = buf;
	}
    }
  else
    str = "* NO TAG *";

  name = dwarf_diename (die);
  if (name == 0)
    name = "* NO NAME *";

  off = dwarf_dieoffset (die);
  cuoff = dwarf_cuoffset (die);

  printf ("%*sDW_TAG_%s\n", n * 5, "", str);
  printf ("%*s Name      : %s\n", n * 5, "", name);
  printf ("%*s Offset    : %lld\n", n * 5, "", (long long int) off);
  printf ("%*s CU offset : %lld\n", n * 5, "", (long long int) cuoff);

  printf ("%*s Attrs     :", n * 5, "");
  for (cnt = 0; cnt < 0xffff; ++cnt)
    if (dwarf_hasattr (die, cnt))
      printf (" %s", dwarf_attr_string (cnt) ?: "<unknown>");
  puts ("");

  if (dwarf_hasattr (die, DW_AT_low_pc) && dwarf_lowpc (die, &addr) == 0)
    {
      Dwarf_Attribute attr;
      Dwarf_Addr addr2;
      printf ("%*s low PC    : %#llx\n",
	      n * 5, "", (unsigned long long int) addr);

      if (dwarf_attr (die, DW_AT_low_pc, &attr) == NULL
	  || dwarf_formaddr (&attr, &addr2) != 0
	  || addr != addr2)
	puts ("************* DW_AT_low_pc verify failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_addr))
	puts ("************* DW_AT_low_pc form failed ************");
      else if (dwarf_whatform (&attr) != DW_FORM_addr)
	puts ("************* DW_AT_low_pc form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_low_pc)
	puts ("************* DW_AT_low_pc attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_high_pc) && dwarf_highpc (die, &addr) == 0)
    {
      Dwarf_Attribute attr;
      Dwarf_Addr addr2;
      printf ("%*s high PC   : %#llx\n",
	      n * 5, "", (unsigned long long int) addr);
      if (dwarf_attr (die, DW_AT_high_pc, &attr) == NULL
	  || dwarf_formaddr (&attr, &addr2) != 0
	  || addr != addr2)
	puts ("************* DW_AT_high_pc verify failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_addr))
	puts ("************* DW_AT_high_pc form failed ************");
      else if (dwarf_whatform (&attr) != DW_FORM_addr)
	puts ("************* DW_AT_high_pc form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_high_pc)
	puts ("************* DW_AT_high_pc attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_byte_size) && (i = dwarf_bytesize (die)) != -1)
    {
      Dwarf_Attribute attr;
      Dwarf_Word u2;
      unsigned int u;
      printf ("%*s byte size : %d\n", n * 5, "", i);
      if (dwarf_attr (die, DW_AT_byte_size, &attr) == NULL
	  || dwarf_formudata (&attr, &u2) != 0
	  || i != (int) u2)
	puts ("************* DW_AT_byte_size verify failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_data1)
	       && ! dwarf_hasform (&attr, DW_FORM_data2)
	       && ! dwarf_hasform (&attr, DW_FORM_data4)
	       && ! dwarf_hasform (&attr, DW_FORM_data8)
	       && ! dwarf_hasform (&attr, DW_FORM_sdata)
	       && ! dwarf_hasform (&attr, DW_FORM_udata))
	puts ("************* DW_AT_byte_size form failed ************");
      else if ((u = dwarf_whatform (&attr)) == 0
	       || (u != DW_FORM_data1
		   && u != DW_FORM_data2
		   && u != DW_FORM_data4
		   && u != DW_FORM_data8
		   && u != DW_FORM_sdata
		   && u != DW_FORM_udata))
	puts ("************* DW_AT_byte_size form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_byte_size)
	puts ("************* DW_AT_byte_size attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_bit_size) && (i = dwarf_bitsize (die)) != -1)
    {
      Dwarf_Attribute attr;
      Dwarf_Word u2;
      unsigned int u;
      printf ("%*s bit size  : %d\n", n * 5, "", i);
      if (dwarf_attr (die, DW_AT_bit_size, &attr) == NULL
	  || dwarf_formudata (&attr, &u2) != 0
	  || i != (int) u2)
	puts ("************* DW_AT_bit_size test failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_data1)
	       && ! dwarf_hasform (&attr, DW_FORM_data2)
	       && ! dwarf_hasform (&attr, DW_FORM_data4)
	       && ! dwarf_hasform (&attr, DW_FORM_data8)
	       && ! dwarf_hasform (&attr, DW_FORM_sdata)
	       && ! dwarf_hasform (&attr, DW_FORM_udata))
	puts ("************* DW_AT_bit_size form failed ************");
      else if ((u = dwarf_whatform (&attr)) == 0
	       || (u != DW_FORM_data1
		   && u != DW_FORM_data2
		   && u != DW_FORM_data4
		   && u != DW_FORM_data8
		   && u != DW_FORM_sdata
		   && u != DW_FORM_udata))
	puts ("************* DW_AT_bit_size form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_bit_size)
	puts ("************* DW_AT_bit_size attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_bit_offset)
      && (i = dwarf_bitoffset (die)) != -1)
    {
      Dwarf_Attribute attr;
      Dwarf_Word u2;
      unsigned int u;
      printf ("%*s bit offset: %d\n", n * 5, "", i);
      if (dwarf_attr (die, DW_AT_bit_offset, &attr) == NULL
	  || dwarf_formudata (&attr, &u2) != 0
	  || i != (int) u2)
	puts ("************* DW_AT_bit_offset test failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_data1)
	       && ! dwarf_hasform (&attr, DW_FORM_data2)
	       && ! dwarf_hasform (&attr, DW_FORM_data4)
	       && ! dwarf_hasform (&attr, DW_FORM_data8)
	       && ! dwarf_hasform (&attr, DW_FORM_sdata)
	       && ! dwarf_hasform (&attr, DW_FORM_udata))
	puts ("************* DW_AT_bit_offset form failed ************");
      else if ((u = dwarf_whatform (&attr)) == 0
	       || (u != DW_FORM_data1
		   && u != DW_FORM_data2
		   && u != DW_FORM_data4
		   && u != DW_FORM_data8
		   && u != DW_FORM_sdata
		   && u != DW_FORM_udata))
	puts ("************* DW_AT_bit_offset form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_bit_offset)
	puts ("************* DW_AT_bit_offset attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_language) && (i = dwarf_srclang (die)) != -1)
    {
      Dwarf_Attribute attr;
      Dwarf_Word u2;
      unsigned int u;
      printf ("%*s language  : %d\n", n * 5, "", i);
      if (dwarf_attr (die, DW_AT_language, &attr) == NULL
	  || dwarf_formudata (&attr, &u2) != 0
	  || i != (int) u2)
	puts ("************* DW_AT_language test failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_data1)
	       && ! dwarf_hasform (&attr, DW_FORM_data2)
	       && ! dwarf_hasform (&attr, DW_FORM_data4)
	       && ! dwarf_hasform (&attr, DW_FORM_data8)
	       && ! dwarf_hasform (&attr, DW_FORM_sdata)
	       && ! dwarf_hasform (&attr, DW_FORM_udata))
	puts ("************* DW_AT_language form failed ************");
      else if ((u = dwarf_whatform (&attr)) == 0
	       || (u != DW_FORM_data1
		   && u != DW_FORM_data2
		   && u != DW_FORM_data4
		   && u != DW_FORM_data8
		   && u != DW_FORM_sdata
		   && u != DW_FORM_udata))
	puts ("************* DW_AT_language form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_language)
	puts ("************* DW_AT_language attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_ordering)
      && (i = dwarf_arrayorder (die)) != -1)
    {
      Dwarf_Attribute attr;
      Dwarf_Word u2;
      unsigned int u;
      printf ("%*s ordering  : %d\n", n * 5, "", i);
      if (dwarf_attr (die, DW_AT_ordering, &attr) == NULL
	  || dwarf_formudata (&attr, &u2) != 0
	  || i != (int) u2)
	puts ("************* DW_AT_ordering test failed ************");
      else if (! dwarf_hasform (&attr, DW_FORM_data1)
	       && ! dwarf_hasform (&attr, DW_FORM_data2)
	       && ! dwarf_hasform (&attr, DW_FORM_data4)
	       && ! dwarf_hasform (&attr, DW_FORM_data8)
	       && ! dwarf_hasform (&attr, DW_FORM_sdata)
	       && ! dwarf_hasform (&attr, DW_FORM_udata))
	puts ("************* DW_AT_ordering failed ************");
      else if ((u = dwarf_whatform (&attr)) == 0
	       || (u != DW_FORM_data1
		   && u != DW_FORM_data2
		   && u != DW_FORM_data4
		   && u != DW_FORM_data8
		   && u != DW_FORM_sdata
		   && u != DW_FORM_udata))
	puts ("************* DW_AT_ordering form (2) failed ************");
      else if (dwarf_whatattr (&attr) != DW_AT_ordering)
	puts ("************* DW_AT_ordering attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_comp_dir))
    {
      Dwarf_Attribute attr;
      if (dwarf_attr (die, DW_AT_comp_dir, &attr) == NULL
	  || (name = dwarf_formstring (&attr)) == NULL)
	puts ("************* DW_AT_comp_dir attr failed ************");
      else
	printf ("%*s directory : %s\n", n * 5, "", name);
    }

  if (dwarf_hasattr (die, DW_AT_producer))
    {
      Dwarf_Attribute attr;
      if (dwarf_attr (die, DW_AT_producer, &attr) == NULL
	  || (name = dwarf_formstring (&attr)) == NULL)
	puts ("************* DW_AT_comp_dir attr failed ************");
      else
	printf ("%*s producer  : %s\n", n * 5, "", name);
    }

  if (dwarf_haschildren (die) != 0 && dwarf_child (die, &child) == 0)
    handle (dbg, &child, n + 1);
  if (dwarf_siblingof (die, die) == 0)
    handle (dbg, die, n);
}


int
main (int argc, char *argv[])
{
 int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf *dbg;

      printf ("file: %s\n", xbasename (argv[cnt]));

      dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  close (fd);
	  continue;
	}

      Dwarf_Off off = 0;
      Dwarf_Off old_off = 0;
      size_t hsize;
      Dwarf_Off abbrev;
      uint8_t addresssize;
      uint8_t offsetsize;
      while (dwarf_nextcu (dbg, off, &off, &hsize, &abbrev, &addresssize,
			   &offsetsize) == 0)
	{
	  printf ("New CU: off = %llu, hsize = %zu, ab = %llu, as = %" PRIu8
		  ", os = %" PRIu8 "\n",
		  (unsigned long long int) old_off, hsize,
		  (unsigned long long int) abbrev, addresssize,
		  offsetsize);

	  Dwarf_Die die;
	  if (dwarf_offdie (dbg, old_off + hsize, &die) != NULL)
	    handle (dbg, &die, 1);

	  old_off = off;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return 0;
}
