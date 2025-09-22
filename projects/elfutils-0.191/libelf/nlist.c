/* Extract symbol list from binary.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2005, 2007, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <nlist.h>

#include "libelfP.h"


struct hashentry
{
  const char *str;
  GElf_Sym sym;
};
#define TYPE struct hashentry
/* XXX Use a better hash function some day.  */
#define HASHFCT(str, len) INTUSE(elf_hash) (str)
#define COMPARE(p1, p2) strcmp ((p1)->str, (p2)->str)
#define CLASS static
#define PREFIX nlist_
#define xcalloc(n, m) calloc (n, m)
#define next_prime(s) __libelf_next_prime (s)
#include <fixedsizehash.h>


int
nlist (const char *filename, struct nlist *nl)
{
  int fd;
  Elf *elf;
  Elf_Scn *scn = NULL;
  Elf_Scn *symscn = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = NULL;
  Elf_Data *data;
  struct nlist_fshash *table;
  size_t nsyms;
  size_t cnt;

  /* Open the file.  */
  fd = open (filename, O_RDONLY);
  if (fd == -1)
    {
      __libelf_seterrno (ELF_E_NOFILE);
      goto fail;
    }

  /* For compatibility reasons (`nlist' existed before ELF and libelf)
     we don't expect the caller to set the ELF version.  Do this here
     as if it hasn't happened yet.  */
  INTUSE(elf_version) (EV_CURRENT);

  /* Now get an ELF descriptor.  */
  elf = INTUSE(elf_begin) (fd, ELF_C_READ_MMAP, NULL);
  if (elf == NULL)
    goto fail_fd;

  /* Find a symbol table.  We prefer the real symbol table but if it
     does not exist use the dynamic symbol table.  */
  while ((scn = INTUSE(elf_nextscn) (elf, scn)) != NULL)
    {
      shdr = INTUSE(gelf_getshdr) (scn, &shdr_mem);
      if (shdr == NULL)
	goto fail_close;

      /* That is what we are looking for.  */
      if (shdr->sh_type == SHT_SYMTAB)
	{
	  symscn = scn;
	  break;
	}

      /* Better than nothing.  Remember this section.  */
      if (shdr->sh_type == SHT_DYNSYM)
	symscn = scn;
    }

  if (symscn == NULL)
    /* We haven't found anything.  Fail.  */
    goto fail_close;

  /* Re-get the section header in case we found only the dynamic symbol
     table.  */
  if (scn == NULL)
    {
      shdr = INTUSE(gelf_getshdr) (symscn, &shdr_mem);
      if (unlikely (shdr == NULL))
	goto fail_close;
    }
  /* SHDR->SH_LINK now contains the index of the string section.  */

  /* Get the data for the symbol section.  */
  data = INTUSE(elf_getdata) (symscn, NULL);
  if (data == NULL)
    goto fail_close;

  /* How many symbols are there?  */
  nsyms = (shdr->sh_size
	   / INTUSE(gelf_fsize) (elf, ELF_T_SYM, 1, EV_CURRENT));

  /* Create the hash table.  */
  table = nlist_fshash_init (nsyms);
  if (table == NULL)
    {
      __libelf_seterrno (ELF_E_NOMEM);
      goto fail_close;
    }

  /* Iterate over all the symbols in the section.  */
  for (cnt = 0; cnt < nsyms; ++cnt)
    {
      struct hashentry mem;
      GElf_Sym *sym;

      /* Get the symbol.  */
      sym = INTUSE(gelf_getsym) (data, cnt, &mem.sym);
      if (sym == NULL)
	goto fail_dealloc;

      /* Get the name of the symbol.  */
      mem.str = INTUSE(elf_strptr) (elf, shdr->sh_link, sym->st_name);
      if (mem.str == NULL)
	goto fail_dealloc;

      /* Don't allow zero-length strings.  */
      if (mem.str[0] == '\0')
	continue;

      /* And add it to the hash table.  Note that we are using the
         overwrite version.  This will ensure that
	 a) global symbols are preferred over local symbols since
	    they are all located at the end
	 b) if there are multiple local symbols with the same name
	    the last one is used.
      */
      (void) nlist_fshash_overwrite (table, mem.str, 0, &mem);
    }

  /* Now it is time to look for the symbols the user asked for.
     XXX What is a `null name/null string'?  This is what the
     standard says terminates the list.  Is it a null pointer
     or a zero-length string?  We test for both...  */
  while (nl->n_name != NULL && nl->n_name[0] != '\0')
    {
      struct hashentry search;
      const struct hashentry *found;

      /* Search for a matching entry in the hash table.  */
      search.str = nl->n_name;
      found = nlist_fshash_find (table, nl->n_name, 0, &search);

      if (found != NULL)
	{
	  /* Found it.  */
	  nl->n_value = found->sym.st_value;
	  nl->n_scnum = found->sym.st_shndx;
	  nl->n_type = GELF_ST_TYPE (found->sym.st_info);
	  /* XXX What shall we fill in the next fields?  */
	  nl->n_sclass = 0;
	  nl->n_numaux = 0;
	}
      else
	{
	  /* Not there.  */
	  nl->n_value = 0;
	  nl->n_scnum = 0;
	  nl->n_type = 0;
	  nl->n_sclass = 0;
	  nl->n_numaux = 0;
	}

      /* Next search request.  */
      ++nl;
    }

  /* Free the resources.  */
  nlist_fshash_fini (table);

  /* We do not need the ELF descriptor anymore.  */
  (void) INTUSE(elf_end) (elf);

  /* Neither the file descriptor.  */
  (void) close (fd);

  return 0;

 fail_dealloc:
  nlist_fshash_fini (table);

 fail_close:
  /* We do not need the ELF descriptor anymore.  */
  (void) INTUSE(elf_end) (elf);

 fail_fd:
  /* Neither the file descriptor.  */
  (void) close (fd);

 fail:
  /* We have to set all entries to zero.  */
  while (nl->n_name != NULL && nl->n_name[0] != '\0')
    {
      nl->n_value = 0;
      nl->n_scnum = 0;
      nl->n_type = 0;
      nl->n_sclass = 0;
      nl->n_numaux = 0;

      /* Next entry.  */
      ++nl;
    }

  return -1;
}
