#include "common.h"

/* These functions convert a while section, one Chdr plus compression data.  */

static void
Elf32_cvt_chdr (void *dest, const void *src, size_t len, int encode)
{
  if (len == 0)
    return;

  /* Move everything over, if necessary, we only need to xlate the
     header, not the compressed data following it.  */
  if (dest != src)
    memmove (dest, src, len);

  if (len >= sizeof (Elf32_Chdr))
    Elf32_cvt_Chdr (dest, src, sizeof (Elf32_Chdr), encode);
}

static void
Elf64_cvt_chdr (void *dest, const void *src, size_t len, int encode)
{
  if (len == 0)
    return;

  /* Move everything over, if necessary, we only need to xlate the
     header, not the compressed data following it.  */
  if (dest != src)
    memmove (dest, src, len);

  if (len >= sizeof (Elf64_Chdr))
    Elf64_cvt_Chdr (dest, src, sizeof (Elf64_Chdr), encode);
}
