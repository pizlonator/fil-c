#include <socket/sys/un.h>

#ifndef _ISOMAC

/* Set ADDR->sun_family to AF_UNIX and ADDR->sun_path to PATHNAME.
   Return 0 on success or -1 on failure (due to overlong PATHNAME).
   The caller should always use sizeof (struct sockaddr_un) as the
   socket address length, disregaring the length of PATHNAME.
   Only concrete (non-abstract) pathnames are supported.  */
int __sockaddr_un_set (struct sockaddr_un *addr, const char *pathname)
  attribute_hidden;

#endif /* _ISOMAC */
