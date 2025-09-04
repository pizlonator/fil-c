/* od-avrelf.c -- dump information about an AVR elf object file.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
   Written by Senthil Kumar Selvaraj, Atmel.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include <stddef.h>
#include <time.h>
#include <stdint.h>
#include "safe-ctype.h"
#include "bfd.h"
#include "objdump.h"
#include "bucomm.h"
#include "bfdlink.h"
#include "bfd.h"
#include "elf/external.h"
#include "elf/internal.h"
#include "elf32-avr.h"

/* Index of the options in the options[] array.  */
#define OPT_MEMUSAGE 0
#define OPT_AVRPROP 1
#define OPT_AVRDEVICEINFO 2

/* List of actions.  */
static struct objdump_private_option options[] =
  {
    { "mem-usage", 0 },
    { "avr-prop",  0},
    { "avr-deviceinfo",  0},
    { NULL, 0 }
  };

/* Display help.  */

static void
elf32_avr_help (FILE *stream)
{
  fprintf (stream, _("\
For AVR ELF files:\n\
  mem-usage       Display memory usage\n\
  avr-prop        Display contents of .avr.prop section\n\
  avr-deviceinfo  Display contents of .note.gnu.avr.deviceinfo section\n\
"));
}

typedef struct tagDeviceInfo
{
    uint32_t flash_start;
    uint32_t flash_size;
    uint32_t ram_start;
    uint32_t ram_size;
    uint32_t eeprom_start;
    uint32_t eeprom_size;
    char * name;
} deviceinfo;


/* Return TRUE if ABFD is handled.  */

static int
elf32_avr_filter (bfd *abfd)
{
  return bfd_get_flavour (abfd) == bfd_target_elf_flavour;
}

static char *
elf32_avr_get_note_section_contents (bfd *abfd, bfd_size_type *size)
{
  asection *section;
  bfd_byte *contents;

  section = bfd_get_section_by_name (abfd, ".note.gnu.avr.deviceinfo");
  if (section == NULL)
    return NULL;

  if (!bfd_malloc_and_get_section (abfd, section, &contents))
    {
      free (contents);
      contents = NULL;
    }

  *size = bfd_section_size (section);
  return (char *) contents;
}

static char *
elf32_avr_get_note_desc (bfd *abfd, char *contents, bfd_size_type size,
			 bfd_size_type *descsz)
{
  Elf_External_Note *xnp = (Elf_External_Note *) contents;
  Elf_Internal_Note in;

  if (offsetof (Elf_External_Note, name) > size)
    return NULL;

  in.type = bfd_get_32 (abfd, xnp->type);
  in.namesz = bfd_get_32 (abfd, xnp->namesz);
  in.namedata = xnp->name;
  if (in.namesz > contents - in.namedata + size)
    return NULL;

  if (in.namesz != 4 || strcmp (in.namedata, "AVR") != 0)
    return NULL;

  in.descsz = bfd_get_32 (abfd, xnp->descsz);
  in.descdata = in.namedata + align_power (in.namesz, 2);
  if (in.descsz < 6 * sizeof (uint32_t)
      || in.descdata >= contents + size
      || in.descsz > contents - in.descdata + size)
    return NULL;

  /* If the note has a string table, ensure it is 0 terminated.  */
  if (in.descsz > 8 * sizeof (uint32_t))
    in.descdata[in.descsz - 1] = 0;

  *descsz = in.descsz;
  return in.descdata;
}

static void
elf32_avr_get_device_info (bfd *abfd, char *description,
			   bfd_size_type desc_size, deviceinfo *device)
{
  if (description == NULL)
    return;

  const bfd_size_type memory_sizes = 6;

  memcpy (device, description, memory_sizes * sizeof (uint32_t));
  desc_size -= memory_sizes * sizeof (uint32_t);
  if (desc_size < 8)
    return;

  uint32_t *stroffset_table = (uint32_t *) description + memory_sizes;
  bfd_size_type stroffset_table_size = bfd_get_32 (abfd, stroffset_table);

  /* If the only content is the size itself, there's nothing in the table */
  if (stroffset_table_size < 8)
    return;
  if (desc_size <= stroffset_table_size)
    return;
  desc_size -= stroffset_table_size;

  /* First entry is the device name index. */
  uint32_t device_name_index = bfd_get_32 (abfd, stroffset_table + 1);
  if (device_name_index >= desc_size)
    return;

  char *str_table = (char *) stroffset_table + stroffset_table_size;
  device->name = str_table + device_name_index;
}

/* Get the size of section *SECNAME, truncated to a reasonable value in
   order to catch PR 27285 and dysfunctional binaries.  */

static bfd_size_type
elf32_avr_get_truncated_size (bfd *abfd, const char *secname)
{
  /* Max size of around 1 MiB is more than any reasonable AVR will
     ever be able to swallow.  And it's small enough so that we won't
     get overflows / UB as demonstrated in PR 27285.  */
  const bfd_size_type max_size = 1000000;
  bfd_size_type size = 0;
  asection *section;

  section = bfd_get_section_by_name (abfd, secname);

  if (section != NULL)
    {
      size = bfd_section_size (section);
      if (size > INT32_MAX)
	{
	  fprintf (stderr, _("Warning: section %s has a negative size of"
			     " %ld bytes, saturating to 0 bytes\n"),
		   secname, (long) (int32_t) size);
	  size = 0;
	}
      else if (size > max_size)
	{
	  fprintf (stderr, _("Warning: section %s has an impossible size of"
			     " %lu bytes, truncating to %lu bytes\n"),
		   secname, (unsigned long) size, (unsigned long) max_size);
	  size = max_size;
	}
    }

  return size;
}

static void
elf32_avr_get_memory_usage (bfd *abfd,
			    bfd_size_type *text_usage,
			    bfd_size_type *data_usage,
			    bfd_size_type *eeprom_usage)
{
  bfd_size_type avr_textsize = elf32_avr_get_truncated_size (abfd, ".text");
  bfd_size_type avr_datasize = elf32_avr_get_truncated_size (abfd, ".data");;
  bfd_size_type avr_bsssize  = elf32_avr_get_truncated_size (abfd, ".bss");
  bfd_size_type noinitsize = elf32_avr_get_truncated_size (abfd, ".noinit");
  bfd_size_type rodatasize = elf32_avr_get_truncated_size (abfd, ".rodata");
  bfd_size_type eepromsize = elf32_avr_get_truncated_size (abfd, ".eeprom");
  bfd_size_type bootloadersize = elf32_avr_get_truncated_size (abfd,
							       ".bootloader");

  *text_usage = avr_textsize + avr_datasize + rodatasize + bootloadersize;
  *data_usage = avr_datasize + avr_bsssize + noinitsize;
  *eeprom_usage = eepromsize;
}

static void
elf32_avr_dump_mem_usage (bfd *abfd)
{
  char *description = NULL;
  bfd_size_type sec_size, desc_size;

  deviceinfo device = { 0, 0, 0, 0, 0, 0, NULL };
  device.name = "Unknown";

  bfd_size_type data_usage = 0;
  bfd_size_type text_usage = 0;
  bfd_size_type eeprom_usage = 0;

  char *contents = elf32_avr_get_note_section_contents (abfd, &sec_size);

  if (contents != NULL)
    {
      description = elf32_avr_get_note_desc (abfd, contents, sec_size,
					     &desc_size);
      elf32_avr_get_device_info (abfd, description, desc_size, &device);
    }

  elf32_avr_get_memory_usage (abfd, &text_usage, &data_usage,
			      &eeprom_usage);

  printf ("AVR Memory Usage\n"
          "----------------\n"
          "Device: %s\n\n", device.name);

  /* Text size */
  printf ("Program:%8" PRIu64 " bytes", (uint64_t) text_usage);
  if (device.flash_size > 0)
    printf (" (%2.1f%% Full)", (double) text_usage / device.flash_size * 100);

  printf ("\n(.text + .data + .rodata + .bootloader)\n\n");

  /* Data size */
  printf ("Data:   %8" PRIu64 " bytes", (uint64_t) data_usage);
  if (device.ram_size > 0)
    printf (" (%2.1f%% Full)", (double) data_usage / device.ram_size * 100);

  printf ("\n(.data + .bss + .noinit)\n\n");

  /* EEPROM size */
  if (eeprom_usage > 0)
    {
      printf ("EEPROM: %8" PRIu64 " bytes", (uint64_t) eeprom_usage);
      if (device.eeprom_size > 0)
	printf (" (%2.1f%% Full)",
		(double) eeprom_usage / device.eeprom_size * 100);

      printf ("\n(.eeprom)\n\n");
    }

  if (contents != NULL)
    free (contents);

}

static void
elf32_avr_dump_avr_prop (bfd *abfd)
{
  struct avr_property_record_list *r_list;
  unsigned int i;

  r_list = avr_elf32_load_property_records (abfd);
  if (r_list == NULL)
    return;

  printf ("\nContents of `%s' section:\n\n", r_list->section->name);

  printf ("  Version: %d\n", r_list->version);
  printf ("  Flags:   %#x\n\n", r_list->flags);

  for (i = 0; i < r_list->record_count; ++i)
    {
      printf ("   %d %s @ %s + %#08" PRIx64" (%#08" PRIx64 ")\n",
	      i,
	      avr_elf32_property_record_name (&r_list->records [i]),
	      r_list->records [i].section->name,
	      (uint64_t) r_list->records [i].offset,
	      ((uint64_t) bfd_section_vma (r_list->records [i].section)
	       + r_list->records [i].offset));
      switch (r_list->records [i].type)
        {
        case RECORD_ORG:
          /* Nothing else to print.  */
          break;
        case RECORD_ORG_AND_FILL:
          printf ("     Fill: %#08lx\n",
                  r_list->records [i].data.org.fill);
          break;
        case RECORD_ALIGN:
          printf ("     Align: %#08lx\n",
                  r_list->records [i].data.align.bytes);
          break;
        case RECORD_ALIGN_AND_FILL:
          printf ("     Align: %#08lx, Fill: %#08lx\n",
                  r_list->records [i].data.align.bytes,
                  r_list->records [i].data.align.fill);
          break;
        }
    }

  free (r_list);
}

static void
elf32_avr_dump_avr_deviceinfo (bfd *abfd)
{
  char *description = NULL;
  bfd_size_type sec_size, desc_size;

  deviceinfo dinfo = { 0, 0, 0, 0, 0, 0, NULL };
  dinfo.name = "Unknown";

  char *contents = elf32_avr_get_note_section_contents (abfd, &sec_size);

  if (contents == NULL)
    return;

  description = elf32_avr_get_note_desc (abfd, contents, sec_size, &desc_size);
  elf32_avr_get_device_info (abfd, description, desc_size, &dinfo);

  printf ("AVR Device Info\n"
	  "----------------\n"
	  "Device: %s\n\n", dinfo.name);

  printf ("Memory     Start      Size      Start      Size\n");

  printf ("Flash  %9" PRIu32 " %9" PRIu32 "  %#9" PRIx32 " %#9" PRIx32 "\n",
	  dinfo.flash_start, dinfo.flash_size,
	  dinfo.flash_start, dinfo.flash_size);

  /* FIXME: There are devices like ATtiny11 without RAM, and where the
     avr/io*.h header has defines like
	 #define RAMSTART    0x60
	 // Last memory addresses
	 #define RAMEND	     0x1F
     which results in a negative RAM size.  The correct display would be to
     show a size of 0, however we also want to show what's actually in the
     note section as precise as possible.  Hence, display the decimal size
     as %d, not as %u.	*/
  printf ("RAM    %9" PRIu32 " %9" PRId32 "  %#9" PRIx32 " %#9" PRIx32 "\n",
	  dinfo.ram_start, dinfo.ram_size,
	  dinfo.ram_start, dinfo.ram_size);

  printf ("EEPROM %9" PRIu32 " %9" PRIu32 "  %#9" PRIx32 " %#9" PRIx32 "\n",
	  dinfo.eeprom_start, dinfo.eeprom_size,
	  dinfo.eeprom_start, dinfo.eeprom_size);

  free (contents);
}

static void
elf32_avr_dump (bfd *abfd)
{
  if (options[OPT_MEMUSAGE].selected)
    elf32_avr_dump_mem_usage (abfd);
  if (options[OPT_AVRPROP].selected)
    elf32_avr_dump_avr_prop (abfd);
  if (options[OPT_AVRDEVICEINFO].selected)
    elf32_avr_dump_avr_deviceinfo (abfd);
}

const struct objdump_private_desc objdump_private_desc_elf32_avr =
  {
    elf32_avr_help,
    elf32_avr_filter,
    elf32_avr_dump,
    options
  };
