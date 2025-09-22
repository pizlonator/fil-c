/* Conversion functions for notes.
   Copyright (C) 2007, 2009, 2014, 2018 Red Hat, Inc.
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

static void
elf_cvt_note (void *dest, const void *src, size_t len, int encode,
	      bool nhdr8)
{
  /* Note that the header is always the same size, but the padding
     differs for GNU Property notes.  */
  assert (sizeof (Elf32_Nhdr) == sizeof (Elf64_Nhdr));

  while (len >= sizeof (Elf32_Nhdr))
    {
      /* Convert the header.  */
      (1 ? Elf32_cvt_Nhdr : Elf64_cvt_Nhdr) (dest, src, sizeof (Elf32_Nhdr),
					     encode);
      const Elf32_Nhdr *n = encode ? src : dest;

      size_t note_len = sizeof *n;

      /* desc needs to be aligned.  */
      note_len += n->n_namesz;
      note_len = nhdr8 ? NOTE_ALIGN8 (note_len) : NOTE_ALIGN4 (note_len);
      if (note_len > len || note_len < sizeof *n)
	{
	  /* Header was translated, nothing else.  */
	  len -= sizeof *n;
	  src += sizeof *n;
	  dest += sizeof *n;
	  break;
	}

      /* data as a whole needs to be aligned.  */
      note_len += n->n_descsz;
      note_len = nhdr8 ? NOTE_ALIGN8 (note_len) : NOTE_ALIGN4 (note_len);
      if (note_len > len || note_len < sizeof *n)
	{
	  /* Header was translated, nothing else.  */
	  len -= sizeof *n;
	  src += sizeof *n;
	  dest += sizeof *n;
	  break;
	}

      /* Copy or skip the note data.  */
      size_t note_data_len = note_len - sizeof *n;
      src += sizeof *n;
      dest += sizeof *n;
      if (src != dest)
	memcpy (dest, src, note_data_len);

      src += note_data_len;
      dest += note_data_len;
      len -= note_len;
    }

    /* Copy over any leftover data unconverted.  Probably part of
       truncated name/desc data.  */
    if (unlikely (len > 0) && src != dest)
      memcpy (dest, src, len);
}

static void
elf_cvt_note4 (void *dest, const void *src, size_t len, int encode)
{
  elf_cvt_note (dest, src, len, encode, false);
}

static void
elf_cvt_note8 (void *dest, const void *src, size_t len, int encode)
{
  elf_cvt_note (dest, src, len, encode, true);
}
