/* Silly pointless function MiG needs.  */

#include <mach.h>
#include <string.h>

vm_size_t
__mig_strncpy (char *dst, const char *src, vm_size_t len)
{
  if (len == 0)
    return 0;

  char *end = __stpncpy (dst, src, len - 1);
  vm_size_t ret = end - dst;
  /* Null terminate the string.  */
  if (ret == len - 1)
    *end = '\0';
  return ret;
}
weak_alias (__mig_strncpy, mig_strncpy)
