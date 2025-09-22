#undef ADD_ROUTINE_PREFIX
#undef ADD_ROUTINE_SUFFIX

#if ELF_WRLOCK_HELD
#define CONCAT(x,y) x##y
#define ADD_ROUTINE_PREFIX(y) CONCAT(__,y)
#define ADD_ROUTINE_SUFFIX(x) x ## _wrlock
#define INTERNAL internal_function
#else
#define ADD_ROUTINE_PREFIX(y) y
#define ADD_ROUTINE_SUFFIX(x) x
#define INTERNAL
#endif

ElfW2(LIBELFBITS,Chdr) *
INTERNAL
ADD_ROUTINE_PREFIX(elfw2(LIBELFBITS, ADD_ROUTINE_SUFFIX(getchdr))) (Elf_Scn *scn)
{

  ElfW2(LIBELFBITS,Shdr) *shdr = ADD_ROUTINE_PREFIX(elfw2(LIBELFBITS, ADD_ROUTINE_SUFFIX(getshdr)))(scn);

  if (shdr == NULL)
    return NULL;

  /* Must have SHF_COMPRESSED flag set.  Allocated or no bits sections
     can never be compressed.  */
  if ((shdr->sh_flags & SHF_ALLOC) != 0)
    {
      __libelf_seterrno (ELF_E_INVALID_SECTION_FLAGS);
      return NULL;
    }

  if (shdr->sh_type == SHT_NULL
      || shdr->sh_type == SHT_NOBITS)
    {
      __libelf_seterrno (ELF_E_INVALID_SECTION_TYPE);
      return NULL;
    }

  if ((shdr->sh_flags & SHF_COMPRESSED) == 0)
    {
      __libelf_seterrno (ELF_E_NOT_COMPRESSED);
      return NULL;
    }

  /* This makes sure the data is in the correct format, so we don't
     need to swap fields. */
  Elf_Data *d  = ADD_ROUTINE_PREFIX(ADD_ROUTINE_SUFFIX(elf_getdata)) (scn, NULL);
  if (d == NULL)
    return NULL;

  if (d->d_size < sizeof (ElfW2(LIBELFBITS,Chdr)) || d->d_buf == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_DATA);
      return NULL;
    }

  return (ElfW2(LIBELFBITS,Chdr) *) d->d_buf;
}
#undef INTERNAL
#undef ELF_WRLOCK_HELD