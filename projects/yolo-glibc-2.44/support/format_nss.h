/* String formatting functions for NSS- and DNS-related data.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef SUPPORT_FORMAT_NSS_H
#define SUPPORT_FORMAT_NSS_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

struct addrinfo;
struct aliasent;
struct ether_addr;
struct group;
struct hostent;
struct netent;
struct passwd;
struct protoent;
struct rpcent;
struct servent;
struct sgrp;
struct spwd;

/* The following functions format their arguments as human-readable
   strings (which can span multiple lines).  The caller must free the
   returned buffer.  For NULL pointers or failure status arguments,
   error variables such as h_errno and errno are included in the
   result.  */
char *support_format_address_family (int);
char *support_format_addrinfo (const struct addrinfo *, int ret);
char *support_format_aliasent (const struct aliasent *);
char *support_format_dns_packet (const unsigned char *buffer, size_t length);
char *support_format_ether_addr (const struct ether_addr *);
char *support_format_group (const struct group *);
char *support_format_herrno (int);
char *support_format_hostent (const struct hostent *);
char *support_format_netent (const struct netent *);
char *support_format_passwd (const struct passwd *);
char *support_format_protoent (const struct protoent *);
char *support_format_rpcent (const struct rpcent *);
char *support_format_servent (const struct servent *);
char *support_format_sgrp (const struct sgrp *);
char *support_format_spwd (const struct spwd *);

__END_DECLS

#endif  /* SUPPORT_FORMAT_NSS_H */
