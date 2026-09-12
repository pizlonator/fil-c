/* Networks file parser in nss_files module.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <nss.h>

#define ENTNAME		netent
#define DATABASE	"networks"
#define NEED_H_ERRNO

struct netent_data {};

#define TRAILING_LIST_MEMBER		n_aliases
#define TRAILING_LIST_SEPARATOR_P	isspace
#include "files-parse.c"
LINE_PARSER
("#",
 {
   char *addr;
   char *cp;
   int n = 1;

   STRING_FIELD (result->n_name, isspace, true);

   STRING_FIELD (addr, isspace, true);
   /* 'inet_network' does not add zeroes at the end if the network number
      does not contain four byte values.  We shift result ourselves if
      necessary.  */
   cp = strchr (addr, '.');
   if (cp != NULL)
     {
       ++n;
       cp = strchr (cp + 1, '.');
       if (cp != NULL)
	 {
	   ++n;
	   cp = strchr (cp + 1, '.');
	   if (cp != NULL)
	     ++n;
	 }
     }
   result->n_net = __inet_network (addr);
   if (result->n_net == INADDR_NONE)
     return 0;
   if (n < 4)
     result->n_net <<= 8 * (4 - n);
   result->n_addrtype = AF_INET;

 })

#include "files-XXX.c"

DB_LOOKUP (netbyname, ,,,
	   LOOKUP_NAME_CASE (n_name, n_aliases),
	   const char *name)

DB_LOOKUP (netbyaddr, ,,,
	   {
	     if ((type == AF_UNSPEC || result->n_addrtype == type)
		 && result->n_net == net)
	       /* Bingo!  */
	       break;
	   }, uint32_t net, int type)
