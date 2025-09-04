/* Host and service name lookups using Name Service Switch modules.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
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

/* The Inner Net License, Version 2.00

  The author(s) grant permission for redistribution and use in source and
binary forms, with or without modification, of the software and documentation
provided that the following conditions are met:

0. If you receive a version of the software that is specifically labelled
   as not being for redistribution (check the version message and/or README),
   you are not permitted to redistribute that version of the software in any
   way or form.
1. All terms of the all other applicable copyrights and licenses must be
   followed.
2. Redistributions of source code must retain the authors' copyright
   notice(s), this list of conditions, and the following disclaimer.
3. Redistributions in binary form must reproduce the authors' copyright
   notice(s), this list of conditions, and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
4. [The copyright holder has authorized the removal of this clause.]
5. Neither the name(s) of the author(s) nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ITS AUTHORS AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  If these license terms cause you a real problem, contact the author.  */

/* This software is Copyright 1996 by Craig Metz, All Rights Reserved.  */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <nss.h>
#include <resolv/resolv-internal.h>
#include <resolv/resolv_context.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <nsswitch.h>
#include <libc-lock.h>
#include <not-cancel.h>
#include <nscd/nscd-client.h>
#include <nscd/nscd_proto.h>
#include <scratch_buffer.h>
#include <inet/net-internal.h>

/* Former AI_IDN_ALLOW_UNASSIGNED and AI_IDN_USE_STD3_ASCII_RULES
   flags, now ignored.  */
#define DEPRECATED_AI_IDN 0x300

#if IS_IN (libc)
# define feof_unlocked(fp) __feof_unlocked (fp)
#endif

struct gaih_service
  {
    const char *name;
    int num;
  };

struct gaih_servtuple
  {
    int socktype;
    int protocol;
    int port;
    bool set;
  };


struct gaih_typeproto
  {
    int socktype;
    int protocol;
    uint8_t protoflag;
    bool defaultflag;
    char name[8];
  };

struct gaih_result
{
  struct gaih_addrtuple *at;
  char *canon;
  char *h_name;
  bool free_at;
  bool got_ipv6;
};

/* Values for `protoflag'.  */
#define GAI_PROTO_NOSERVICE	1
#define GAI_PROTO_PROTOANY	2

static const struct gaih_typeproto gaih_inet_typeproto[] =
{
  { 0, 0, 0, false, "" },
  { SOCK_STREAM, IPPROTO_TCP, 0, true, "tcp" },
  { SOCK_DGRAM, IPPROTO_UDP, 0, true, "udp" },
#if defined SOCK_DCCP && defined IPPROTO_DCCP
  { SOCK_DCCP, IPPROTO_DCCP, 0, false, "dccp" },
#endif
#ifdef IPPROTO_UDPLITE
  { SOCK_DGRAM, IPPROTO_UDPLITE, 0, false, "udplite" },
#endif
#ifdef IPPROTO_SCTP
  { SOCK_STREAM, IPPROTO_SCTP, 0, false, "sctp" },
  { SOCK_SEQPACKET, IPPROTO_SCTP, 0, false, "sctp" },
#endif
  { SOCK_RAW, 0, GAI_PROTO_PROTOANY|GAI_PROTO_NOSERVICE, true, "raw" },
  { 0, 0, 0, false, "" }
};

static const struct addrinfo default_hints =
  {
    .ai_flags = AI_DEFAULT,
    .ai_family = PF_UNSPEC,
    .ai_socktype = 0,
    .ai_protocol = 0,
    .ai_addrlen = 0,
    .ai_addr = NULL,
    .ai_canonname = NULL,
    .ai_next = NULL
  };

static void
gaih_result_reset (struct gaih_result *res)
{
  if (res->free_at)
    free (res->at);
  free (res->canon);
  free (res->h_name);
  memset (res, 0, sizeof (*res));
}

static int
gaih_inet_serv (const char *servicename, const struct gaih_typeproto *tp,
		const struct addrinfo *req, struct gaih_servtuple *st,
		struct scratch_buffer *tmpbuf)
{
  struct servent *s;
  struct servent ts;
  int r;

  do
    {
      r = __getservbyname_r (servicename, tp->name, &ts,
			     tmpbuf->data, tmpbuf->length, &s);
      if (r != 0 || s == NULL)
	{
	  if (r == ERANGE)
	    {
	      if (!scratch_buffer_grow (tmpbuf))
		return -EAI_MEMORY;
	    }
	  else
	    return -EAI_SERVICE;
	}
    }
  while (r);

  st->socktype = tp->socktype;
  st->protocol = ((tp->protoflag & GAI_PROTO_PROTOANY)
		  ? req->ai_protocol : tp->protocol);
  st->port = s->s_port;
  st->set = true;

  return 0;
}

/* Convert struct hostent to a list of struct gaih_addrtuple objects.  The new
   addresses are appended to the tuple array in RES.  */
static bool
convert_hostent_to_gaih_addrtuple (const struct addrinfo *req, int family,
				   struct hostent *h, struct gaih_result *res)
{
  /* Count the number of addresses in h->h_addr_list.  */
  size_t count = 0;
  for (char **p = h->h_addr_list; *p != NULL; ++p)
    ++count;

  /* Report no data if no addresses are available, or if the incoming
     address size is larger than what we can store.  */
  if (count == 0 || h->h_length > sizeof (((struct gaih_addrtuple) {}).addr))
    return true;

  struct gaih_addrtuple *array = res->at;
  size_t old = 0;

  while (array != NULL)
    {
      old++;
      array = array->next;
    }

  array = realloc (res->at, (old + count) * sizeof (*array));

  if (array == NULL)
    return false;

  res->got_ipv6 = family == AF_INET6;
  res->at = array;
  res->free_at = true;

  /* Duplicate h_name because it may get reclaimed when the underlying storage
     is freed.  */
  if (res->h_name == NULL)
    {
      res->h_name = __strdup (h->h_name);
      if (res->h_name == NULL)
	return false;
    }

  /* Update the next pointers on reallocation.  */
  for (size_t i = 0; i < old; i++)
    array[i].next = array + i + 1;

  array += old;

  memset (array, 0, count * sizeof (*array));

  for (size_t i = 0; i < count; ++i)
    {
      if (family == AF_INET && req->ai_family == AF_INET6)
	{
	  /* Perform address mapping. */
	  array[i].family = AF_INET6;
	  memcpy(array[i].addr + 3, h->h_addr_list[i], sizeof (uint32_t));
	  array[i].addr[2] = htonl (0xffff);
	}
      else
	{
	  array[i].family = family;
	  memcpy (array[i].addr, h->h_addr_list[i], h->h_length);
	}
      array[i].next = array + i + 1;
    }
  array[count - 1].next = NULL;

  return true;
}

static int
gethosts (nss_gethostbyname3_r fct, int family, const char *name,
	  const struct addrinfo *req, struct scratch_buffer *tmpbuf,
	  struct gaih_result *res, enum nss_status *statusp, int *no_datap)
{
  struct hostent th;
  char *localcanon = NULL;
  enum nss_status status;

  *no_datap = 0;
  while (1)
    {
      *statusp = status = DL_CALL_FCT (fct, (name, family, &th,
					     tmpbuf->data, tmpbuf->length,
					     &errno, &h_errno, NULL,
					     &localcanon));
      if (status != NSS_STATUS_TRYAGAIN || h_errno != NETDB_INTERNAL
	  || errno != ERANGE)
	break;
      if (!scratch_buffer_grow (tmpbuf))
	return -EAI_MEMORY;
    }
  if (status == NSS_STATUS_NOTFOUND
      || status == NSS_STATUS_TRYAGAIN || status == NSS_STATUS_UNAVAIL)
    {
      if (h_errno == NETDB_INTERNAL)
	return -EAI_SYSTEM;
      if (h_errno == TRY_AGAIN)
	*no_datap = EAI_AGAIN;
      else
	*no_datap = h_errno == NO_DATA;
    }
  else if (status == NSS_STATUS_SUCCESS)
    {
      if (!convert_hostent_to_gaih_addrtuple (req, family, &th, res))
	return -EAI_MEMORY;

      if (localcanon != NULL && res->canon == NULL)
	{
	  char *canonbuf = __strdup (localcanon);
	  if (canonbuf == NULL)
	    return  -EAI_MEMORY;
	  res->canon = canonbuf;
	}
    }

  return 0;
}

/* This function is called if a canonical name is requested, but if
   the service function did not provide it.  It tries to obtain the
   name using getcanonname_r from the same service NIP.  If the name
   cannot be canonicalized, return a copy of NAME.  Return NULL on
   memory allocation failure.  The returned string is allocated on the
   heap; the caller has to free it.  */
static char *
getcanonname (nss_action_list nip, const char *hname, const char *name)
{
  nss_getcanonname_r *cfct = __nss_lookup_function (nip, "getcanonname_r");
  char *s = (char *) name;
  if (cfct != NULL)
    {
      char buf[256];
      if (DL_CALL_FCT (cfct, (hname ?: name, buf, sizeof (buf), &s, &errno,
			      &h_errno)) != NSS_STATUS_SUCCESS)
	/* If the canonical name cannot be determined, use the passed
	   string.  */
	s = (char *) name;
    }
  return __strdup (s);
}

/* Process looked up canonical name and if necessary, decode to IDNA.  Result
   is a new string written to CANONP and the earlier string is freed.  */

static int
process_canonname (const struct addrinfo *req, const char *orig_name,
		   struct gaih_result *res)
{
  char *canon = res->canon;

  if ((req->ai_flags & AI_CANONNAME) != 0)
    {
      bool do_idn = req->ai_flags & AI_CANONIDN;
      if (do_idn)
	{
	  char *out;
	  int rc = __idna_from_dns_encoding (canon ?: orig_name, &out);
	  if (rc == 0)
	    {
	      free (canon);
	      canon = out;
	    }
	  else if (rc == EAI_IDN_ENCODE)
	    /* Use the punycode name as a fallback.  */
	    do_idn = false;
	  else
	    return -rc;
	}
      if (!do_idn && canon == NULL && (canon = __strdup (orig_name)) == NULL)
	return -EAI_MEMORY;
    }

  res->canon = canon;
  return 0;
}

static int
get_servtuples (const struct gaih_service *service, const struct addrinfo *req,
		struct gaih_servtuple *st, struct scratch_buffer *tmpbuf)
{
  int i;
  const struct gaih_typeproto *tp = gaih_inet_typeproto;

  if (req->ai_protocol || req->ai_socktype)
    {
      ++tp;

      while (tp->name[0]
	     && ((req->ai_socktype != 0 && req->ai_socktype != tp->socktype)
		 || (req->ai_protocol != 0
		     && !(tp->protoflag & GAI_PROTO_PROTOANY)
		     && req->ai_protocol != tp->protocol)))
	++tp;

      if (! tp->name[0])
	{
	  if (req->ai_socktype)
	    return -EAI_SOCKTYPE;
	  else
	    return -EAI_SERVICE;
	}
    }

  if (service != NULL && (tp->protoflag & GAI_PROTO_NOSERVICE) != 0)
    return -EAI_SERVICE;

  if (service == NULL || service->num >= 0)
    {
      int port = service != NULL ? htons (service->num) : 0;

      if (req->ai_socktype || req->ai_protocol)
	{
	  st[0].socktype = tp->socktype;
	  st[0].protocol = ((tp->protoflag & GAI_PROTO_PROTOANY)
			  ? req->ai_protocol : tp->protocol);
	  st[0].port = port;
	  st[0].set = true;

	  return 0;
	}

      /* Neither socket type nor protocol is set.  Return all socket types
	 we know about.  */
      for (i = 0, ++tp; tp->name[0]; ++tp)
	if (tp->defaultflag)
	  {
	    st[i].socktype = tp->socktype;
	    st[i].protocol = tp->protocol;
	    st[i].port = port;
	    st[i++].set = true;
	  }

      return 0;
    }

  if (tp->name[0])
    return gaih_inet_serv (service->name, tp, req, st, tmpbuf);

  for (i = 0, tp++; tp->name[0]; tp++)
    {
      if ((tp->protoflag & GAI_PROTO_NOSERVICE) != 0)
	continue;

      if (req->ai_socktype != 0
	  && req->ai_socktype != tp->socktype)
	continue;
      if (req->ai_protocol != 0
	  && !(tp->protoflag & GAI_PROTO_PROTOANY)
	  && req->ai_protocol != tp->protocol)
	continue;

      if (gaih_inet_serv (service->name,
			  tp, req, &st[i], tmpbuf) != 0)
	continue;

      i++;
    }

  if (!st[0].set)
    return -EAI_SERVICE;

  return 0;
}

#ifdef USE_NSCD
/* Query addresses from nscd cache, returning a non-zero value on error.
   RES members have the lookup result; RES->AT is NULL if there were no errors
   but also no results.  */

static int
get_nscd_addresses (const char *name, const struct addrinfo *req,
		    struct gaih_result *res)
{
  if (__nss_not_use_nscd_hosts > 0
      && ++__nss_not_use_nscd_hosts > NSS_NSCD_RETRY)
    __nss_not_use_nscd_hosts = 0;

  res->at = NULL;

  if (__nss_not_use_nscd_hosts || __nss_database_custom[NSS_DBSIDX_hosts])
    return 0;

  /* Try to use nscd.  */
  struct nscd_ai_result *air = NULL;
  int err = __nscd_getai (name, &air, &h_errno);

  if (__glibc_unlikely (air == NULL))
    {
      /* The database contains a negative entry.  */
      if (err == 0)
	return -EAI_NONAME;
      if (__nss_not_use_nscd_hosts == 0)
	{
	  if (h_errno == NETDB_INTERNAL && errno == ENOMEM)
	    return -EAI_MEMORY;
	  if (h_errno == TRY_AGAIN)
	    return -EAI_AGAIN;
	  return -EAI_SYSTEM;
	}
      return 0;
    }

  /* Transform into gaih_addrtuple list.  */
  int result = 0;
  char *addrs = air->addrs;

  struct gaih_addrtuple *at = calloc (air->naddrs, sizeof (*at));
  if (at == NULL)
    {
      result = -EAI_MEMORY;
      goto out;
    }

  res->free_at = true;

  int count = 0;
  for (int i = 0; i < air->naddrs; ++i)
    {
      socklen_t size = (air->family[i] == AF_INET
			? INADDRSZ : IN6ADDRSZ);

      if (!((air->family[i] == AF_INET
	     && req->ai_family == AF_INET6
	     && (req->ai_flags & AI_V4MAPPED) != 0)
	    || req->ai_family == AF_UNSPEC
	    || air->family[i] == req->ai_family))
	{
	  /* Skip over non-matching result.  */
	  addrs += size;
	  continue;
	}

      if (air->family[i] == AF_INET && req->ai_family == AF_INET6
	  && (req->ai_flags & AI_V4MAPPED))
	{
	  at[count].family = AF_INET6;
	  at[count].addr[3] = *(uint32_t *) addrs;
	  at[count].addr[2] = htonl (0xffff);
	}
      else if (req->ai_family == AF_UNSPEC
	       || air->family[i] == req->ai_family)
	{
	  at[count].family = air->family[i];
	  memcpy (at[count].addr, addrs, size);
	  if (air->family[i] == AF_INET6)
	    res->got_ipv6 = true;
	}
      at[count].next = at + count + 1;
      count++;
      addrs += size;
    }

  if ((req->ai_flags & AI_CANONNAME) && air->canon != NULL)
    {
      char *canonbuf = __strdup (air->canon);
      if (canonbuf == NULL)
	{
	  result = -EAI_MEMORY;
	  goto out;
	}
      res->canon = canonbuf;
    }

  if (count == 0)
    {
      result = -EAI_NONAME;
      goto out;
    }

  at[count - 1].next = NULL;

  res->at = at;

out:
  free (air);
  if (result != 0)
    {
      free (at);
      res->free_at = false;
    }

  return result;
}
#endif

static int
get_nss_addresses (const char *name, const struct addrinfo *req,
		   struct scratch_buffer *tmpbuf, struct gaih_result *res)
{
  int no_data = 0;
  int no_inet6_data = 0;
  nss_action_list nip;
  enum nss_status inet6_status = NSS_STATUS_UNAVAIL;
  enum nss_status status = NSS_STATUS_UNAVAIL;
  int no_more;
  struct resolv_context *res_ctx = NULL;
  bool do_merge = false;
  int result = 0;

  no_more = !__nss_database_get (nss_database_hosts, &nip);

  /* If we are looking for both IPv4 and IPv6 address we don't
     want the lookup functions to automatically promote IPv4
     addresses to IPv6 addresses, so we use the no_inet6
     function variant.  */
  res_ctx = __resolv_context_get ();
  if (res_ctx == NULL)
    {
      if (errno == ENOMEM)
	{
	  result = -EAI_MEMORY;
	  goto out;
	}
      no_more = 1;
    }

  while (!no_more)
    {
      /* Always start afresh; continue should discard previous results
	 and the hosts database does not support merge.  */
      gaih_result_reset (res);

      if (do_merge)
	{
	  __set_h_errno (NETDB_INTERNAL);
	  __set_errno (EBUSY);
	  break;
	}

      no_data = 0;
      nss_gethostbyname4_r *fct4 = NULL;

      /* gethostbyname4_r sends out parallel A and AAAA queries and
	 is thus only suitable for PF_UNSPEC.  */
      if (req->ai_family == PF_UNSPEC)
	fct4 = __nss_lookup_function (nip, "gethostbyname4_r");

      if (fct4 != NULL)
	{
	  while (1)
	    {
	      status = DL_CALL_FCT (fct4, (name, &res->at,
					   tmpbuf->data, tmpbuf->length,
					   &errno, &h_errno,
					   NULL));
	      if (status == NSS_STATUS_SUCCESS)
		break;
	      /* gethostbyname4_r may write into AT, so reset it.  */
	      res->at = NULL;
	      if (status != NSS_STATUS_TRYAGAIN
		  || errno != ERANGE || h_errno != NETDB_INTERNAL)
		{
		  if (h_errno == TRY_AGAIN)
		    no_data = EAI_AGAIN;
		  else
		    no_data = h_errno == NO_DATA;
		  break;
		}

	      if (!scratch_buffer_grow (tmpbuf))
		{
		  __resolv_context_put (res_ctx);
		  result = -EAI_MEMORY;
		  goto out;
		}
	    }

	  if (status == NSS_STATUS_SUCCESS)
	    {
	      assert (!no_data);
	      no_data = 1;

	      if ((req->ai_flags & AI_CANONNAME) != 0 && res->canon == NULL)
		{
		  char *canonbuf = __strdup (res->at->name);
		  if (canonbuf == NULL)
		    {
		      __resolv_context_put (res_ctx);
		      result = -EAI_MEMORY;
		      goto out;
		    }
		  res->canon = canonbuf;
		}

	      struct gaih_addrtuple **pat = &res->at;

	      while (*pat != NULL)
		{
		  if ((*pat)->family == AF_INET
		      && req->ai_family == AF_INET6
		      && (req->ai_flags & AI_V4MAPPED) != 0)
		    {
		      uint32_t *pataddr = (*pat)->addr;
		      (*pat)->family = AF_INET6;
		      pataddr[3] = pataddr[0];
		      pataddr[2] = htonl (0xffff);
		      pataddr[1] = 0;
		      pataddr[0] = 0;
		      pat = &((*pat)->next);
		      no_data = 0;
		    }
		  else if (req->ai_family == AF_UNSPEC
			   || (*pat)->family == req->ai_family)
		    {
		      pat = &((*pat)->next);

		      no_data = 0;
		      if (req->ai_family == AF_INET6)
			res->got_ipv6 = true;
		    }
		  else
		    *pat = ((*pat)->next);
		}
	    }

	  no_inet6_data = no_data;
	}
      else
	{
	  nss_gethostbyname3_r *fct = NULL;
	  if (req->ai_flags & AI_CANONNAME)
	    /* No need to use this function if we do not look for
	       the canonical name.  The function does not exist in
	       all NSS modules and therefore the lookup would
	       often fail.  */
	    fct = __nss_lookup_function (nip, "gethostbyname3_r");
	  if (fct == NULL)
	    /* We are cheating here.  The gethostbyname2_r
	       function does not have the same interface as
	       gethostbyname3_r but the extra arguments the
	       latter takes are added at the end.  So the
	       gethostbyname2_r code will just ignore them.  */
	    fct = __nss_lookup_function (nip, "gethostbyname2_r");

	  if (fct != NULL)
	    {
	      if (req->ai_family == AF_INET6
		  || req->ai_family == AF_UNSPEC)
		{
		  if ((result = gethosts (fct, AF_INET6, name, req, tmpbuf,
					  res, &status, &no_data)) != 0)
		    {
		      __resolv_context_put (res_ctx);
		      goto out;
		    }
		  no_inet6_data = no_data;
		  inet6_status = status;
		}
	      if (req->ai_family == AF_INET
		  || req->ai_family == AF_UNSPEC
		  || (req->ai_family == AF_INET6
		      && (req->ai_flags & AI_V4MAPPED)
		      /* Avoid generating the mapped addresses if we
			 know we are not going to need them.  */
		      && ((req->ai_flags & AI_ALL) || !res->got_ipv6)))
		{
		  if ((result = gethosts (fct, AF_INET, name, req, tmpbuf,
					  res, &status, &no_data)) != 0)
		    {
		      __resolv_context_put (res_ctx);
		      goto out;
		    }

		  if (req->ai_family == AF_INET)
		    {
		      no_inet6_data = no_data;
		      inet6_status = status;
		    }
		}

	      /* If we found one address for AF_INET or AF_INET6,
		 don't continue the search.  */
	      if (inet6_status == NSS_STATUS_SUCCESS
		  || status == NSS_STATUS_SUCCESS)
		{
		  if ((req->ai_flags & AI_CANONNAME) != 0
		      && res->canon == NULL)
		    {
		      char *canonbuf = getcanonname (nip, res->h_name, name);
		      if (canonbuf == NULL)
			{
			  __resolv_context_put (res_ctx);
			  result = -EAI_MEMORY;
			  goto out;
			}
		      res->canon = canonbuf;
		    }
		  status = NSS_STATUS_SUCCESS;
		}
	      else
		{
		  /* We can have different states for AF_INET and
		     AF_INET6.  Try to find a useful one for both.  */
		  if (inet6_status == NSS_STATUS_TRYAGAIN)
		    status = NSS_STATUS_TRYAGAIN;
		  else if (status == NSS_STATUS_UNAVAIL
			   && inet6_status != NSS_STATUS_UNAVAIL)
		    status = inet6_status;
		}
	    }
	  else
	    {
	      /* Could not locate any of the lookup functions.
		 The NSS lookup code does not consistently set
		 errno, so we need to supply our own error
		 code here.  The root cause could either be a
		 resource allocation failure, or a missing
		 service function in the DSO (so it should not
		 be listed in /etc/nsswitch.conf).  Assume the
		 former, and return EBUSY.  */
	      status = NSS_STATUS_UNAVAIL;
	      __set_h_errno (NETDB_INTERNAL);
	      __set_errno (EBUSY);
	    }
	}

      if (nss_next_action (nip, status) == NSS_ACTION_RETURN)
	break;

      /* The hosts database does not support MERGE.  */
      if (nss_next_action (nip, status) == NSS_ACTION_MERGE)
	do_merge = true;

      nip++;
      if (nip->module == NULL)
	no_more = -1;
    }

  __resolv_context_put (res_ctx);

  /* If we have a failure which sets errno, report it using
     EAI_SYSTEM.  */
  if ((status == NSS_STATUS_TRYAGAIN || status == NSS_STATUS_UNAVAIL)
      && h_errno == NETDB_INTERNAL)
    {
      result = -EAI_SYSTEM;
      goto out;
    }

  if (no_data != 0 && no_inet6_data != 0)
    {
      /* If both requests timed out report this.  */
      if (no_data == EAI_AGAIN && no_inet6_data == EAI_AGAIN)
	result = -EAI_AGAIN;
      else
	/* We made requests but they turned out no data.  The name
	   is known, though.  */
	result = -EAI_NODATA;
    }

out:
  if (result != 0)
    gaih_result_reset (res);
  return result;
}

/* Convert numeric addresses to binary into RES.  On failure, RES->AT is set to
   NULL and an error code is returned.  If AI_NUMERIC_HOST is not requested and
   the function cannot determine a result, RES->AT is set to NULL and 0
   returned.  */

static int
text_to_binary_address (const char *name, const struct addrinfo *req,
			struct gaih_result *res)
{
  struct gaih_addrtuple *at = res->at;
  int result = 0;

  assert (at != NULL);

  memset (at->addr, 0, sizeof (at->addr));
  if (__inet_aton_exact (name, (struct in_addr *) at->addr) != 0)
    {
      if (req->ai_family == AF_UNSPEC || req->ai_family == AF_INET)
	at->family = AF_INET;
      else if (req->ai_family == AF_INET6 && (req->ai_flags & AI_V4MAPPED))
	{
	  at->addr[3] = at->addr[0];
	  at->addr[2] = htonl (0xffff);
	  at->addr[1] = 0;
	  at->addr[0] = 0;
	  at->family = AF_INET6;
	}
      else
	{
	  result = -EAI_ADDRFAMILY;
	  goto out;
	}

      if (req->ai_flags & AI_CANONNAME)
	{
	  char *canonbuf = __strdup (name);
	  if (canonbuf == NULL)
	    {
	      result = -EAI_MEMORY;
	      goto out;
	    }
	  res->canon = canonbuf;
	}
      return 0;
    }

  char *scope_delim = strchr (name, SCOPE_DELIMITER);
  int e;

  if (scope_delim == NULL)
    e = inet_pton (AF_INET6, name, at->addr);
  else
    e = __inet_pton_length (AF_INET6, name, scope_delim - name, at->addr);

  if (e > 0)
    {
      if (req->ai_family == AF_UNSPEC || req->ai_family == AF_INET6)
	at->family = AF_INET6;
      else if (req->ai_family == AF_INET
	       && IN6_IS_ADDR_V4MAPPED (at->addr))
	{
	  at->addr[0] = at->addr[3];
	  at->family = AF_INET;
	}
      else
	{
	  result = -EAI_ADDRFAMILY;
	  goto out;
	}

      if (scope_delim != NULL
	  && __inet6_scopeid_pton ((struct in6_addr *) at->addr,
				   scope_delim + 1, &at->scopeid) != 0)
	{
	  result = -EAI_NONAME;
	  goto out;
	}

      if (req->ai_flags & AI_CANONNAME)
	{
	  char *canonbuf = __strdup (name);
	  if (canonbuf == NULL)
	    {
	      result = -EAI_MEMORY;
	      goto out;
	    }
	  res->canon = canonbuf;
	}
      return 0;
    }

  if ((req->ai_flags & AI_NUMERICHOST))
    result = -EAI_NONAME;

out:
  res->at = NULL;
  return result;
}

/* If possible, call the simple, old functions, which do not support IPv6 scope
   ids, nor retrieving the canonical name.  */

static int
try_simple_gethostbyname (const char *name, const struct addrinfo *req,
			  struct scratch_buffer *tmpbuf,
			  struct gaih_result *res)
{
  res->at = NULL;

  if (req->ai_family != AF_INET || (req->ai_flags & AI_CANONNAME) != 0)
    return 0;

  int rc;
  struct hostent th;
  struct hostent *h;

  while (1)
    {
      rc = __gethostbyname2_r (name, AF_INET, &th, tmpbuf->data,
			       tmpbuf->length, &h, &h_errno);
      if (rc != ERANGE || h_errno != NETDB_INTERNAL)
	break;
      if (!scratch_buffer_grow (tmpbuf))
	return -EAI_MEMORY;
    }

  if (rc == 0)
    {
      if (h != NULL)
	{
	  /* We found data, convert it.  RES->AT from the conversion will
	     either be an allocated block or NULL, both of which are safe to
	     pass to free ().  */
	  if (!convert_hostent_to_gaih_addrtuple (req, AF_INET, h, res))
	    return -EAI_MEMORY;

	  res->free_at = true;
	  return 0;
	}
      if (h_errno == NO_DATA)
	return -EAI_NODATA;

      return -EAI_NONAME;
    }

  if (h_errno == NETDB_INTERNAL)
    return -EAI_SYSTEM;
  if (h_errno == TRY_AGAIN)
    return -EAI_AGAIN;

  /* We made requests but they turned out no data.
     The name is known, though.  */
  return -EAI_NODATA;
}

/* Add local address information into RES.  RES->AT is assumed to have enough
   space for two tuples and is zeroed out.  */

static void
get_local_addresses (const struct addrinfo *req, struct gaih_result *res)
{
  struct gaih_addrtuple *atr = res->at;
  if (req->ai_family == AF_UNSPEC)
    res->at->next = res->at + 1;

  if (req->ai_family == AF_UNSPEC || req->ai_family == AF_INET6)
    {
      res->at->family = AF_INET6;
      if ((req->ai_flags & AI_PASSIVE) == 0)
	memcpy (res->at->addr, &in6addr_loopback, sizeof (struct in6_addr));
      atr = res->at->next;
    }

  if (req->ai_family == AF_UNSPEC || req->ai_family == AF_INET)
    {
      atr->family = AF_INET;
      if ((req->ai_flags & AI_PASSIVE) == 0)
	atr->addr[0] = htonl (INADDR_LOOPBACK);
    }
}

/* Generate results in PAI and its count in NADDRS.  Return 0 on success or an
   error code on failure.  */

static int
generate_addrinfo (const struct addrinfo *req, struct gaih_result *res,
		   const struct gaih_servtuple *st, struct addrinfo **pai,
		   unsigned int *naddrs)
{
  size_t socklen;
  sa_family_t family;

  /* Buffer is the size of an unformatted IPv6 address in printable format.  */
  for (struct gaih_addrtuple *at = res->at; at != NULL; at = at->next)
    {
      family = at->family;
      if (family == AF_INET6)
	{
	  socklen = sizeof (struct sockaddr_in6);

	  /* If we looked up IPv4 mapped address discard them here if
	     the caller isn't interested in all address and we have
	     found at least one IPv6 address.  */
	  if (res->got_ipv6
	      && (req->ai_flags & (AI_V4MAPPED|AI_ALL)) == AI_V4MAPPED
	      && IN6_IS_ADDR_V4MAPPED (at->addr))
	    continue;
	}
      else
	socklen = sizeof (struct sockaddr_in);

      for (int i = 0; st[i].set; i++)
	{
	  struct addrinfo *ai;
	  ai = *pai = malloc (sizeof (struct addrinfo) + socklen);
	  if (ai == NULL)
	    return -EAI_MEMORY;

	  ai->ai_flags = req->ai_flags;
	  ai->ai_family = family;
	  ai->ai_socktype = st[i].socktype;
	  ai->ai_protocol = st[i].protocol;
	  ai->ai_addrlen = socklen;
	  ai->ai_addr = (void *) (ai + 1);

	  /* We only add the canonical name once.  */
	  ai->ai_canonname = res->canon;
	  res->canon = NULL;

#ifdef _HAVE_SA_LEN
	  ai->ai_addr->sa_len = socklen;
#endif /* _HAVE_SA_LEN */
	  ai->ai_addr->sa_family = family;

	  /* In case of an allocation error the list must be NULL
	     terminated.  */
	  ai->ai_next = NULL;

	  if (family == AF_INET6)
	    {
	      struct sockaddr_in6 *sin6p = (struct sockaddr_in6 *) ai->ai_addr;
	      sin6p->sin6_port = st[i].port;
	      sin6p->sin6_flowinfo = 0;
	      memcpy (&sin6p->sin6_addr, at->addr, sizeof (struct in6_addr));
	      sin6p->sin6_scope_id = at->scopeid;
	    }
	  else
	    {
	      struct sockaddr_in *sinp = (struct sockaddr_in *) ai->ai_addr;
	      sinp->sin_port = st[i].port;
	      memcpy (&sinp->sin_addr, at->addr, sizeof (struct in_addr));
	      memset (sinp->sin_zero, '\0', sizeof (sinp->sin_zero));
	    }

	  pai = &(ai->ai_next);
	}

      ++*naddrs;
    }
  return 0;
}

static int
gaih_inet (const char *name, const struct gaih_service *service,
	   const struct addrinfo *req, struct addrinfo **pai,
	   unsigned int *naddrs, struct scratch_buffer *tmpbuf)
{
  struct gaih_servtuple st[sizeof (gaih_inet_typeproto)
			   / sizeof (struct gaih_typeproto)] = {0};

  const char *orig_name = name;

  int rc;
  if ((rc = get_servtuples (service, req, st, tmpbuf)) != 0)
    return rc;

  bool malloc_name = false;
  struct gaih_addrtuple *addrmem = NULL;
  int result = 0;

  struct gaih_result res = {0};
  struct gaih_addrtuple local_at[2] = {0};

  res.at = local_at;

  if (__glibc_unlikely (name == NULL))
    {
      get_local_addresses (req, &res);
      goto process_list;
    }

  if (req->ai_flags & AI_IDN)
    {
      char *out;
      result = __idna_to_dns_encoding (name, &out);
      if (result != 0)
	return -result;
      name = out;
      malloc_name = true;
    }

  if ((result = text_to_binary_address (name, req, &res)) != 0)
    goto free_and_return;
  else if (res.at != NULL)
    goto process_list;

  if ((result = try_simple_gethostbyname (name, req, tmpbuf, &res)) != 0)
    goto free_and_return;
  else if (res.at != NULL)
    goto process_list;

#ifdef USE_NSCD
  if ((result = get_nscd_addresses (name, req, &res)) != 0)
    goto free_and_return;
  else if (res.at != NULL)
    goto process_list;
#endif

  if ((result = get_nss_addresses (name, req, tmpbuf, &res)) != 0)
    goto free_and_return;
  else if (res.at != NULL)
    goto process_list;

  /* None of the lookups worked, so name not found.  */
  result = -EAI_NONAME;
  goto free_and_return;

process_list:
  /* Set up the canonical name if we need it.  */
  if ((result = process_canonname (req, orig_name, &res)) != 0)
    goto free_and_return;

  result = generate_addrinfo (req, &res, st, pai, naddrs);

free_and_return:
  if (malloc_name)
    free ((char *) name);
  free (addrmem);
  gaih_result_reset (&res);

  return result;
}


struct sort_result
{
  struct addrinfo *dest_addr;
  /* Using sockaddr_storage is for now overkill.  We only support IPv4
     and IPv6 so far.  If this changes at some point we can adjust the
     type here.  */
  struct sockaddr_in6 source_addr;
  uint8_t source_addr_len;
  bool got_source_addr;
  uint8_t source_addr_flags;
  uint8_t prefixlen;
  uint32_t index;
  int32_t native;
};

struct sort_result_combo
{
  struct sort_result *results;
  int nresults;
};


#if __BYTE_ORDER == __BIG_ENDIAN
# define htonl_c(n) n
#else
# define htonl_c(n) __bswap_constant_32 (n)
#endif

static const struct scopeentry
{
  union
  {
    char addr[4];
    uint32_t addr32;
  };
  uint32_t netmask;
  int32_t scope;
} default_scopes[] =
  {
    /* Link-local addresses: scope 2.  */
    { { { 169, 254, 0, 0 } }, htonl_c (0xffff0000), 2 },
    { { { 127, 0, 0, 0 } }, htonl_c (0xff000000), 2 },
    /* Default: scope 14.  */
    { { { 0, 0, 0, 0 } }, htonl_c (0x00000000), 14 }
  };

/* The label table.  */
static const struct scopeentry *scopes;


static int
get_scope (const struct sockaddr_in6 *in6)
{
  int scope;
  if (in6->sin6_family == PF_INET6)
    {
      if (! IN6_IS_ADDR_MULTICAST (&in6->sin6_addr))
	{
	  if (IN6_IS_ADDR_LINKLOCAL (&in6->sin6_addr)
	      /* RFC 4291 2.5.3 says that the loopback address is to be
		 treated like a link-local address.  */
	      || IN6_IS_ADDR_LOOPBACK (&in6->sin6_addr))
	    scope = 2;
	  else if (IN6_IS_ADDR_SITELOCAL (&in6->sin6_addr))
	    scope = 5;
	  else
	    /* XXX Is this the correct default behavior?  */
	    scope = 14;
	}
      else
	scope = in6->sin6_addr.s6_addr[1] & 0xf;
    }
  else if (in6->sin6_family == PF_INET)
    {
      const struct sockaddr_in *in = (const struct sockaddr_in *) in6;

      size_t cnt = 0;
      while (1)
	{
	  if ((in->sin_addr.s_addr & scopes[cnt].netmask)
	      == scopes[cnt].addr32)
	    return scopes[cnt].scope;

	  ++cnt;
	}
      /* NOTREACHED */
    }
  else
    /* XXX What is a good default?  */
    scope = 15;

  return scope;
}


struct prefixentry
{
  struct in6_addr prefix;
  unsigned int bits;
  int val;
};


/* The label table.  */
static const struct prefixentry *labels;

/* Default labels.  */
static const struct prefixentry default_labels[] =
  {
    /* See RFC 3484 for the details.  */
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
      }, 128, 0 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 16, 2 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 96, 3 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 } }
      }, 96, 4 },
    /* The next two entries differ from RFC 3484.  We need to treat
       IPv6 site-local addresses special because they are never NATed,
       unlike site-locale IPv4 addresses.  If this would not happen, on
       machines which have only IPv4 and IPv6 site-local addresses, the
       sorting would prefer the IPv6 site-local addresses, causing
       unnecessary delays when trying to connect to a global IPv6 address
       through a site-local IPv6 address.  */
    { { .__in6_u
	= { .__u6_addr8 = { 0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 10, 5 },
    { { .__in6_u
	= { .__u6_addr8 = { 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 7, 6 },
    /* Additional rule for Teredo tunnels.  */
    { { .__in6_u
	= { .__u6_addr8 = { 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 32, 7 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 0, 1 }
  };


/* The precedence table.  */
static const struct prefixentry *precedence;

/* The default precedences.  */
static const struct prefixentry default_precedence[] =
  {
    /* See RFC 3484 for the details.  */
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
      }, 128, 50 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 16, 30 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 96, 20 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 } }
      }, 96, 10 },
    { { .__in6_u
	= { .__u6_addr8 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
      }, 0, 40 }
  };


static int
match_prefix (const struct sockaddr_in6 *in6,
	      const struct prefixentry *list, int default_val)
{
  int idx;
  struct sockaddr_in6 in6_mem;

  if (in6->sin6_family == PF_INET)
    {
      const struct sockaddr_in *in = (const struct sockaddr_in *) in6;

      /* Construct a V4-to-6 mapped address.  */
      in6_mem.sin6_family = PF_INET6;
      in6_mem.sin6_port = in->sin_port;
      in6_mem.sin6_flowinfo = 0;
      memset (&in6_mem.sin6_addr, '\0', sizeof (in6_mem.sin6_addr));
      in6_mem.sin6_addr.s6_addr16[5] = 0xffff;
      in6_mem.sin6_addr.s6_addr32[3] = in->sin_addr.s_addr;
      in6_mem.sin6_scope_id = 0;

      in6 = &in6_mem;
    }
  else if (in6->sin6_family != PF_INET6)
    return default_val;

  for (idx = 0; ; ++idx)
    {
      unsigned int bits = list[idx].bits;
      const uint8_t *mask = list[idx].prefix.s6_addr;
      const uint8_t *val = in6->sin6_addr.s6_addr;

      while (bits >= 8)
	{
	  if (*mask != *val)
	    break;

	  ++mask;
	  ++val;
	  bits -= 8;
	}

      if (bits < 8)
	{
	  if ((*mask & (0xff00 >> bits)) == (*val & (0xff00 >> bits)))
	    /* Match!  */
	    break;
	}
    }

  return list[idx].val;
}


static int
get_label (const struct sockaddr_in6 *in6)
{
  /* XXX What is a good default value?  */
  return match_prefix (in6, labels, INT_MAX);
}


static int
get_precedence (const struct sockaddr_in6 *in6)
{
  /* XXX What is a good default value?  */
  return match_prefix (in6, precedence, 0);
}


/* Find last bit set in a word.  */
static int
fls (uint32_t a)
{
  uint32_t mask;
  int n;
  for (n = 0, mask = 1 << 31; n < 32; mask >>= 1, ++n)
    if ((a & mask) != 0)
      break;
  return n;
}


static int
rfc3484_sort (const void *p1, const void *p2, void *arg)
{
  const size_t idx1 = *(const size_t *) p1;
  const size_t idx2 = *(const size_t *) p2;
  struct sort_result_combo *src = (struct sort_result_combo *) arg;
  struct sort_result *a1 = &src->results[idx1];
  struct sort_result *a2 = &src->results[idx2];

  /* Rule 1: Avoid unusable destinations.
     We have the got_source_addr flag set if the destination is reachable.  */
  if (a1->got_source_addr && ! a2->got_source_addr)
    return -1;
  if (! a1->got_source_addr && a2->got_source_addr)
    return 1;


  /* Rule 2: Prefer matching scope.  Only interesting if both
     destination addresses are IPv6.  */
  int a1_dst_scope
    = get_scope ((struct sockaddr_in6 *) a1->dest_addr->ai_addr);

  int a2_dst_scope
    = get_scope ((struct sockaddr_in6 *) a2->dest_addr->ai_addr);

  if (a1->got_source_addr)
    {
      int a1_src_scope = get_scope (&a1->source_addr);
      int a2_src_scope = get_scope (&a2->source_addr);

      if (a1_dst_scope == a1_src_scope && a2_dst_scope != a2_src_scope)
	return -1;
      if (a1_dst_scope != a1_src_scope && a2_dst_scope == a2_src_scope)
	return 1;
    }


  /* Rule 3: Avoid deprecated addresses.  */
  if (a1->got_source_addr)
    {
      if (!(a1->source_addr_flags & in6ai_deprecated)
	  && (a2->source_addr_flags & in6ai_deprecated))
	return -1;
      if ((a1->source_addr_flags & in6ai_deprecated)
	  && !(a2->source_addr_flags & in6ai_deprecated))
	return 1;
    }

  /* Rule 4: Prefer home addresses.  */
  if (a1->got_source_addr)
    {
      if (!(a1->source_addr_flags & in6ai_homeaddress)
	  && (a2->source_addr_flags & in6ai_homeaddress))
	return 1;
      if ((a1->source_addr_flags & in6ai_homeaddress)
	  && !(a2->source_addr_flags & in6ai_homeaddress))
	return -1;
    }

  /* Rule 5: Prefer matching label.  */
  if (a1->got_source_addr)
    {
      int a1_dst_label
	= get_label ((struct sockaddr_in6 *) a1->dest_addr->ai_addr);
      int a1_src_label = get_label (&a1->source_addr);

      int a2_dst_label
	= get_label ((struct sockaddr_in6 *) a2->dest_addr->ai_addr);
      int a2_src_label = get_label (&a2->source_addr);

      if (a1_dst_label == a1_src_label && a2_dst_label != a2_src_label)
	return -1;
      if (a1_dst_label != a1_src_label && a2_dst_label == a2_src_label)
	return 1;
    }


  /* Rule 6: Prefer higher precedence.  */
  int a1_prec
    = get_precedence ((struct sockaddr_in6 *) a1->dest_addr->ai_addr);
  int a2_prec
    = get_precedence ((struct sockaddr_in6 *) a2->dest_addr->ai_addr);

  if (a1_prec > a2_prec)
    return -1;
  if (a1_prec < a2_prec)
    return 1;


  /* Rule 7: Prefer native transport.  */
  if (a1->got_source_addr)
    {
      /* The same interface index means the same interface which means
	 there is no difference in transport.  This should catch many
	 (most?) cases.  */
      if (a1->index != a2->index)
	{
	  int a1_native = a1->native;
	  int a2_native = a2->native;

	  if (a1_native == -1 || a2_native == -1)
	    {
	      uint32_t a1_index;
	      if (a1_native == -1)
		{
		  /* If we do not have the information use 'native' as
		     the default.  */
		  a1_native = 0;
		  a1_index = a1->index;
		}
	      else
		a1_index = 0xffffffffu;

	      uint32_t a2_index;
	      if (a2_native == -1)
		{
		  /* If we do not have the information use 'native' as
		     the default.  */
		  a2_native = 0;
		  a2_index = a2->index;
		}
	      else
		a2_index = 0xffffffffu;

	      __check_native (a1_index, &a1_native, a2_index, &a2_native);

	      /* Fill in the results in all the records.  */
	      for (int i = 0; i < src->nresults; ++i)
		if (a1_index != -1 && src->results[i].index == a1_index)
		  {
		    assert (src->results[i].native == -1
			    || src->results[i].native == a1_native);
		    src->results[i].native = a1_native;
		  }
		else if (a2_index != -1 && src->results[i].index == a2_index)
		  {
		    assert (src->results[i].native == -1
			    || src->results[i].native == a2_native);
		    src->results[i].native = a2_native;
		  }
	    }

	  if (a1_native && !a2_native)
	    return -1;
	  if (!a1_native && a2_native)
	    return 1;
	}
    }


  /* Rule 8: Prefer smaller scope.  */
  if (a1_dst_scope < a2_dst_scope)
    return -1;
  if (a1_dst_scope > a2_dst_scope)
    return 1;


  /* Rule 9: Use longest matching prefix.  */
  if (a1->got_source_addr
      && a1->dest_addr->ai_family == a2->dest_addr->ai_family)
    {
      int bit1 = 0;
      int bit2 = 0;

      if (a1->dest_addr->ai_family == PF_INET)
	{
	  assert (a1->source_addr.sin6_family == PF_INET);
	  assert (a2->source_addr.sin6_family == PF_INET);

	  /* Outside of subnets, as defined by the network masks,
	     common address prefixes for IPv4 addresses make no sense.
	     So, define a non-zero value only if source and
	     destination address are on the same subnet.  */
	  struct sockaddr_in *in1_dst
	    = (struct sockaddr_in *) a1->dest_addr->ai_addr;
	  in_addr_t in1_dst_addr = ntohl (in1_dst->sin_addr.s_addr);
	  struct sockaddr_in *in1_src
	    = (struct sockaddr_in *) &a1->source_addr;
	  in_addr_t in1_src_addr = ntohl (in1_src->sin_addr.s_addr);
	  in_addr_t netmask1 = 0xffffffffu << (32 - a1->prefixlen);

	  if ((in1_src_addr & netmask1) == (in1_dst_addr & netmask1))
	    bit1 = fls (in1_dst_addr ^ in1_src_addr);

	  struct sockaddr_in *in2_dst
	    = (struct sockaddr_in *) a2->dest_addr->ai_addr;
	  in_addr_t in2_dst_addr = ntohl (in2_dst->sin_addr.s_addr);
	  struct sockaddr_in *in2_src
	    = (struct sockaddr_in *) &a2->source_addr;
	  in_addr_t in2_src_addr = ntohl (in2_src->sin_addr.s_addr);
	  in_addr_t netmask2 = 0xffffffffu << (32 - a2->prefixlen);

	  if ((in2_src_addr & netmask2) == (in2_dst_addr & netmask2))
	    bit2 = fls (in2_dst_addr ^ in2_src_addr);
	}
      else if (a1->dest_addr->ai_family == PF_INET6)
	{
	  assert (a1->source_addr.sin6_family == PF_INET6);
	  assert (a2->source_addr.sin6_family == PF_INET6);

	  struct sockaddr_in6 *in1_dst;
	  struct sockaddr_in6 *in1_src;
	  struct sockaddr_in6 *in2_dst;
	  struct sockaddr_in6 *in2_src;

	  in1_dst = (struct sockaddr_in6 *) a1->dest_addr->ai_addr;
	  in1_src = (struct sockaddr_in6 *) &a1->source_addr;
	  in2_dst = (struct sockaddr_in6 *) a2->dest_addr->ai_addr;
	  in2_src = (struct sockaddr_in6 *) &a2->source_addr;

	  int i;
	  for (i = 0; i < 4; ++i)
	    if (in1_dst->sin6_addr.s6_addr32[i]
		!= in1_src->sin6_addr.s6_addr32[i]
		|| (in2_dst->sin6_addr.s6_addr32[i]
		    != in2_src->sin6_addr.s6_addr32[i]))
	      break;

	  if (i < 4)
	    {
	      bit1 = fls (ntohl (in1_dst->sin6_addr.s6_addr32[i]
				 ^ in1_src->sin6_addr.s6_addr32[i]));
	      bit2 = fls (ntohl (in2_dst->sin6_addr.s6_addr32[i]
				 ^ in2_src->sin6_addr.s6_addr32[i]));
	    }
	}

      if (bit1 > bit2)
	return -1;
      if (bit1 < bit2)
	return 1;
    }


  /* Rule 10: Otherwise, leave the order unchanged.  To ensure this
     compare with the value indicating the order in which the entries
     have been received from the services.  NB: no two entries can have
     the same order so the test will never return zero.  */
  return idx1 < idx2 ? -1 : 1;
}


static int
in6aicmp (const void *p1, const void *p2)
{
  struct in6addrinfo *a1 = (struct in6addrinfo *) p1;
  struct in6addrinfo *a2 = (struct in6addrinfo *) p2;

  return memcmp (a1->addr, a2->addr, sizeof (a1->addr));
}


/* Name of the config file for RFC 3484 sorting (for now).  */
#define GAICONF_FNAME "/etc/gai.conf"


/* Non-zero if we are supposed to reload the config file automatically
   whenever it changed.  */
static int gaiconf_reload_flag;

/* Non-zero if gaiconf_reload_flag was ever set to true.  */
static int gaiconf_reload_flag_ever_set;

/* Last modification time.  */
#ifdef _STATBUF_ST_NSEC

static struct __timespec64 gaiconf_mtime;

static inline void
save_gaiconf_mtime (const struct __stat64_t64 *st)
{
  gaiconf_mtime = (struct __timespec64) { st->st_mtim.tv_sec,
					  st->st_mtim.tv_nsec };
}

static inline bool
check_gaiconf_mtime (const struct __stat64_t64 *st)
{
  return (st->st_mtim.tv_sec == gaiconf_mtime.tv_sec
          && st->st_mtim.tv_nsec == gaiconf_mtime.tv_nsec);
}

#else

static time_t gaiconf_mtime;

static inline void
save_gaiconf_mtime (const struct __stat64_t64 *st)
{
  gaiconf_mtime = st->st_mtime;
}

static inline bool
check_gaiconf_mtime (const struct __stat64_t64 *st)
{
  return st->st_mtime == gaiconf_mtime;
}

#endif


void
__libc_getaddrinfo_freemem (void)
{
  if (labels != default_labels)
    {
      const struct prefixentry *old = labels;
      labels = default_labels;
      free ((void *) old);
    }

  if (precedence != default_precedence)
    {
      const struct prefixentry *old = precedence;
      precedence = default_precedence;
      free ((void *) old);
    }

  if (scopes != default_scopes)
    {
      const struct scopeentry *old = scopes;
      scopes = default_scopes;
      free ((void *) old);
    }
}


struct prefixlist
{
  struct prefixentry entry;
  struct prefixlist *next;
};


struct scopelist
{
  struct scopeentry entry;
  struct scopelist *next;
};


static void
free_prefixlist (struct prefixlist *list)
{
  while (list != NULL)
    {
      struct prefixlist *oldp = list;
      list = list->next;
      free (oldp);
    }
}


static void
free_scopelist (struct scopelist *list)
{
  while (list != NULL)
    {
      struct scopelist *oldp = list;
      list = list->next;
      free (oldp);
    }
}


static int
prefixcmp (const void *p1, const void *p2)
{
  const struct prefixentry *e1 = (const struct prefixentry *) p1;
  const struct prefixentry *e2 = (const struct prefixentry *) p2;

  if (e1->bits < e2->bits)
    return 1;
  if (e1->bits == e2->bits)
    return 0;
  return -1;
}


static int
scopecmp (const void *p1, const void *p2)
{
  const struct scopeentry *e1 = (const struct scopeentry *) p1;
  const struct scopeentry *e2 = (const struct scopeentry *) p2;

  if (e1->netmask > e2->netmask)
    return -1;
  if (e1->netmask == e2->netmask)
    return 0;
  return 1;
}

static bool
add_prefixlist (struct prefixlist **listp, size_t *lenp, bool *nullbitsp,
		char *val1, char *val2, char **pos)
{
  struct in6_addr prefix;
  unsigned long int bits;
  unsigned long int val;
  char *endp;

  bits = 128;
  __set_errno (0);
  char *cp = strchr (val1, '/');
  if (cp != NULL)
    *cp++ = '\0';
  *pos = cp;
  if (inet_pton (AF_INET6, val1, &prefix)
      && (cp == NULL
	  || (bits = strtoul (cp, &endp, 10)) != ULONG_MAX
	  || errno != ERANGE)
      && *endp == '\0'
      && bits <= 128
      && ((val = strtoul (val2, &endp, 10)) != ULONG_MAX
	  || errno != ERANGE)
      && *endp == '\0'
      && val <= INT_MAX)
    {
      struct prefixlist *newp = malloc (sizeof (*newp));
      if (newp == NULL)
	return false;

      memcpy (&newp->entry.prefix, &prefix, sizeof (prefix));
      newp->entry.bits = bits;
      newp->entry.val = val;
      newp->next = *listp;
      *listp = newp;
      ++*lenp;
      *nullbitsp |= bits == 0;
    }
  return true;
}

static bool
add_scopelist (struct scopelist **listp, size_t *lenp, bool *nullbitsp,
	       const struct in6_addr *prefixp, unsigned long int bits,
	       unsigned long int val)
{
  struct scopelist *newp = malloc (sizeof (*newp));
  if (newp == NULL)
    return false;

  newp->entry.netmask = htonl (bits != 96 ? (0xffffffff << (128 - bits)) : 0);
  newp->entry.addr32 = (prefixp->s6_addr32[3] & newp->entry.netmask);
  newp->entry.scope = val;
  newp->next = *listp;
  *listp = newp;
  ++*lenp;
  *nullbitsp |= bits == 96;

  return true;
}

static void
gaiconf_init (void)
{
  struct prefixlist *labellist = NULL;
  size_t nlabellist = 0;
  bool labellist_nullbits = false;
  struct prefixlist *precedencelist = NULL;
  size_t nprecedencelist = 0;
  bool precedencelist_nullbits = false;
  struct scopelist *scopelist =  NULL;
  size_t nscopelist = 0;
  bool scopelist_nullbits = false;

  FILE *fp = fopen (GAICONF_FNAME, "rce");
  if (fp == NULL)
    goto no_file;

  struct __stat64_t64 st;
  if (__fstat64_time64 (fileno (fp), &st) != 0)
    {
      fclose (fp);
      goto no_file;
    }

  char *line = NULL;
  size_t linelen = 0;

  __fsetlocking (fp, FSETLOCKING_BYCALLER);

  while (!feof_unlocked (fp))
    {
      ssize_t n = __getline (&line, &linelen, fp);
      if (n <= 0)
	break;

      /* Handle comments.  No escaping possible so this is easy.  */
      char *cp = strchr (line, '#');
      if (cp != NULL)
	*cp = '\0';

      cp = line;
      while (isspace (*cp))
	++cp;

      char *cmd = cp;
      while (*cp != '\0' && !isspace (*cp))
	++cp;
      size_t cmdlen = cp - cmd;

      if (*cp != '\0')
	*cp++ = '\0';
      while (isspace (*cp))
	++cp;

      char *val1 = cp;
      while (*cp != '\0' && !isspace (*cp))
	++cp;
      size_t val1len = cp - cmd;

      /* We always need at least two values.  */
      if (val1len == 0)
	continue;

      if (*cp != '\0')
	*cp++ = '\0';
      while (isspace (*cp))
	++cp;

      char *val2 = cp;
      while (*cp != '\0' && !isspace (*cp))
	++cp;

      /*  Ignore the rest of the line.  */
      *cp = '\0';

      switch (cmdlen)
	{
	case 5:
	  if (strcmp (cmd, "label") == 0)
	    {
	      if (!add_prefixlist (&labellist, &nlabellist,
				   &labellist_nullbits, val1, val2, &cp))
		{
		  free (line);
		  fclose (fp);
		  goto no_file;
		}
	    }
	  break;

	case 6:
	  if (strcmp (cmd, "reload") == 0)
	    {
	      gaiconf_reload_flag = strcmp (val1, "yes") == 0;
	      if (gaiconf_reload_flag)
		gaiconf_reload_flag_ever_set = 1;
	    }
	  break;

	case 7:
	  if (strcmp (cmd, "scopev4") == 0)
	    {
	      struct in6_addr prefix;
	      unsigned long int bits;
	      unsigned long int val;
	      char *endp;

	      bits = 32;
	      __set_errno (0);
	      cp = strchr (val1, '/');
	      if (cp != NULL)
		*cp++ = '\0';
	      if (inet_pton (AF_INET6, val1, &prefix))
		{
		  bits = 128;
		  if (IN6_IS_ADDR_V4MAPPED (&prefix)
		      && (cp == NULL
			  || (bits = strtoul (cp, &endp, 10)) != ULONG_MAX
			  || errno != ERANGE)
		      && *endp == '\0'
		      && bits >= 96
		      && bits <= 128
		      && ((val = strtoul (val2, &endp, 10)) != ULONG_MAX
			  || errno != ERANGE)
		      && *endp == '\0'
		      && val <= INT_MAX)
		    {
		      if (!add_scopelist (&scopelist, &nscopelist,
					  &scopelist_nullbits, &prefix,
					  bits, val))
			{
			  free (line);
			  fclose (fp);
			  goto no_file;
			}
		    }
		}
	      else if (inet_pton (AF_INET, val1, &prefix.s6_addr32[3])
		       && (cp == NULL
			   || (bits = strtoul (cp, &endp, 10)) != ULONG_MAX
			   || errno != ERANGE)
		       && *endp == '\0'
		       && bits <= 32
		       && ((val = strtoul (val2, &endp, 10)) != ULONG_MAX
			   || errno != ERANGE)
		       && *endp == '\0'
		       && val <= INT_MAX)
		{
		  if (!add_scopelist (&scopelist, &nscopelist,
				      &scopelist_nullbits, &prefix,
				      bits + 96, val))
		    {
		      free (line);
		      fclose (fp);
		      goto no_file;
		    }
		}
	    }
	  break;

	case 10:
	  if (strcmp (cmd, "precedence") == 0)
	    {
	      if (!add_prefixlist (&precedencelist, &nprecedencelist,
				   &precedencelist_nullbits, val1, val2,
				   &cp))
		{
		  free (line);
		  fclose (fp);
		  goto no_file;
		}
	    }
	  break;
	}
    }

  free (line);

  fclose (fp);

  /* Create the array for the labels.  */
  struct prefixentry *new_labels;
  if (nlabellist > 0)
    {
      if (!labellist_nullbits)
	++nlabellist;
      new_labels = malloc (nlabellist * sizeof (*new_labels));
      if (new_labels == NULL)
	goto no_file;

      int i = nlabellist;
      if (!labellist_nullbits)
	{
	  --i;
	  memset (&new_labels[i].prefix, '\0', sizeof (struct in6_addr));
	  new_labels[i].bits = 0;
	  new_labels[i].val = 1;
	}

      struct prefixlist *l = labellist;
      while (i-- > 0)
	{
	  new_labels[i] = l->entry;
	  l = l->next;
	}
      free_prefixlist (labellist);
      labellist = NULL;

      /* Sort the entries so that the most specific ones are at
	 the beginning.  */
      qsort (new_labels, nlabellist, sizeof (*new_labels), prefixcmp);
    }
  else
    new_labels = (struct prefixentry *) default_labels;

  struct prefixentry *new_precedence;
  if (nprecedencelist > 0)
    {
      if (!precedencelist_nullbits)
	++nprecedencelist;
      new_precedence = malloc (nprecedencelist * sizeof (*new_precedence));
      if (new_precedence == NULL)
	{
	  if (new_labels != default_labels)
	    free (new_labels);
	  goto no_file;
	}

      int i = nprecedencelist;
      if (!precedencelist_nullbits)
	{
	  --i;
	  memset (&new_precedence[i].prefix, '\0',
		  sizeof (struct in6_addr));
	  new_precedence[i].bits = 0;
	  new_precedence[i].val = 40;
	}

      struct prefixlist *l = precedencelist;
      while (i-- > 0)
	{
	  new_precedence[i] = l->entry;
	  l = l->next;
	}
      free_prefixlist (precedencelist);
      precedencelist = NULL;

      /* Sort the entries so that the most specific ones are at
	 the beginning.  */
      qsort (new_precedence, nprecedencelist, sizeof (*new_precedence),
	     prefixcmp);
    }
  else
    new_precedence = (struct prefixentry *) default_precedence;

  struct scopeentry *new_scopes;
  if (nscopelist > 0)
    {
      if (!scopelist_nullbits)
	++nscopelist;
      new_scopes = malloc (nscopelist * sizeof (*new_scopes));
      if (new_scopes == NULL)
	{
	  if (new_labels != default_labels)
	    free (new_labels);
	  if (new_precedence != default_precedence)
	    free (new_precedence);
	  goto no_file;
	}

      int i = nscopelist;
      if (!scopelist_nullbits)
	{
	  --i;
	  new_scopes[i].addr32 = 0;
	  new_scopes[i].netmask = 0;
	  new_scopes[i].scope = 14;
	}

      struct scopelist *l = scopelist;
      while (i-- > 0)
	{
	  new_scopes[i] = l->entry;
	  l = l->next;
	}
      free_scopelist (scopelist);

      /* Sort the entries so that the most specific ones are at
	 the beginning.  */
      qsort (new_scopes, nscopelist, sizeof (*new_scopes),
	     scopecmp);
    }
  else
    new_scopes = (struct scopeentry *) default_scopes;

  /* Now we are ready to replace the values.  */
  const struct prefixentry *old = labels;
  labels = new_labels;
  if (old != default_labels)
    free ((void *) old);

  old = precedence;
  precedence = new_precedence;
  if (old != default_precedence)
    free ((void *) old);

  const struct scopeentry *oldscope = scopes;
  scopes = new_scopes;
  if (oldscope != default_scopes)
    free ((void *) oldscope);

  save_gaiconf_mtime (&st);
  return;

no_file:
  free_prefixlist (labellist);
  free_prefixlist (precedencelist);
  free_scopelist (scopelist);

  /* If we previously read the file but it is gone now, free the old data and
     use the builtin one.  Leave the reload flag alone.  */
  __libc_getaddrinfo_freemem ();
}


static void
gaiconf_reload (void)
{
  struct __stat64_t64 st;
  if (__stat64_time64 (GAICONF_FNAME, &st) != 0
      || !check_gaiconf_mtime (&st))
    gaiconf_init ();
}

static bool
try_connect (int *fdp, int *afp, struct sockaddr_in6 *source_addrp,
	     const struct sockaddr *addr, socklen_t addrlen, int family)
{
  int fd = *fdp;
  int af = *afp;
  socklen_t sl = sizeof (*source_addrp);

  while (true)
    {
      if (fd != -1 && __connect (fd, addr, addrlen) == 0
	  && __getsockname (fd, (struct sockaddr *) source_addrp, &sl) == 0)
	return true;

      if (errno == EAFNOSUPPORT && af == AF_INET6 && family == AF_INET)
	{
	  /* This could mean IPv6 sockets are IPv6-only.  */
	  if (fd != -1)
	    __close_nocancel_nostatus (fd);
	  *afp = af = AF_INET;
	  *fdp = fd = __socket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC,
				IPPROTO_IP);
	  continue;
	}

      return false;
    }

  __builtin_unreachable ();
}

int
getaddrinfo (const char *name, const char *service,
	     const struct addrinfo *hints, struct addrinfo **pai)
{
  int i = 0, last_i = 0;
  int nresults = 0;
  struct addrinfo *p = NULL;
  struct gaih_service gaih_service, *pservice;
  struct addrinfo local_hints;

  if (name != NULL && name[0] == '*' && name[1] == 0)
    name = NULL;

  if (service != NULL && service[0] == '*' && service[1] == 0)
    service = NULL;

  if (name == NULL && service == NULL)
    return EAI_NONAME;

  if (hints == NULL)
    hints = &default_hints;

  if (hints->ai_flags
      & ~(AI_PASSIVE|AI_CANONNAME|AI_NUMERICHOST|AI_ADDRCONFIG|AI_V4MAPPED
	  |AI_IDN|AI_CANONIDN|DEPRECATED_AI_IDN
	  |AI_NUMERICSERV|AI_ALL))
    return EAI_BADFLAGS;

  if ((hints->ai_flags & AI_CANONNAME) && name == NULL)
    return EAI_BADFLAGS;

  if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET
      && hints->ai_family != AF_INET6)
    return EAI_FAMILY;

  struct in6addrinfo *in6ai = NULL;
  size_t in6ailen = 0;
  bool seen_ipv4 = false;
  bool seen_ipv6 = false;
  bool check_pf_called = false;

  if (hints->ai_flags & AI_ADDRCONFIG)
    {
      /* We might need information about what interfaces are available.
	 Also determine whether we have IPv4 or IPv6 interfaces or both.  We
	 cannot cache the results since new interfaces could be added at
	 any time.  */
      __check_pf (&seen_ipv4, &seen_ipv6, &in6ai, &in6ailen);
      check_pf_called = true;

      /* Now make a decision on what we return, if anything.  */
      if (hints->ai_family == PF_UNSPEC && (seen_ipv4 || seen_ipv6))
	{
	  /* If we haven't seen both IPv4 and IPv6 interfaces we can
	     narrow down the search.  */
	  if (seen_ipv4 != seen_ipv6)
	    {
	      local_hints = *hints;
	      local_hints.ai_family = seen_ipv4 ? PF_INET : PF_INET6;
	      hints = &local_hints;
	    }
	}
      else if ((hints->ai_family == PF_INET && ! seen_ipv4)
	       || (hints->ai_family == PF_INET6 && ! seen_ipv6))
	{
	  /* We cannot possibly return a valid answer.  */
	  __free_in6ai (in6ai);
	  return EAI_NONAME;
	}
    }

  if (service && service[0])
    {
      char *c;
      gaih_service.name = service;
      gaih_service.num = strtoul (gaih_service.name, &c, 10);
      if (*c != '\0')
	{
	  if (hints->ai_flags & AI_NUMERICSERV)
	    {
	      __free_in6ai (in6ai);
	      return EAI_NONAME;
	    }

	  gaih_service.num = -1;
	}

      pservice = &gaih_service;
    }
  else
    pservice = NULL;

  struct addrinfo **end = &p;
  unsigned int naddrs = 0;
  struct scratch_buffer tmpbuf;

  scratch_buffer_init (&tmpbuf);
  last_i = gaih_inet (name, pservice, hints, end, &naddrs, &tmpbuf);
  scratch_buffer_free (&tmpbuf);

  if (last_i != 0)
    {
      freeaddrinfo (p);
      __free_in6ai (in6ai);

      return -last_i;
    }

  while (*end)
    {
      end = &((*end)->ai_next);
      ++nresults;
    }

  if (naddrs > 1)
    {
      /* Read the config file.  */
      __libc_once_define (static, once);
      __typeof (once) old_once = once;
      __libc_once (once, gaiconf_init);
      /* Sort results according to RFC 3484.  */
      struct sort_result *results;
      size_t *order;
      struct addrinfo *q;
      struct addrinfo *last = NULL;
      char *canonname = NULL;
      struct scratch_buffer buf;
      scratch_buffer_init (&buf);

      if (!scratch_buffer_set_array_size (&buf, nresults,
					  sizeof (*results) + sizeof (size_t)))
	{
	  __free_in6ai (in6ai);
	  return EAI_MEMORY;
	}
      results = buf.data;

      order = (size_t *) (results + nresults);

      /* Now we definitely need the interface information.  */
      if (! check_pf_called)
	__check_pf (&seen_ipv4, &seen_ipv6, &in6ai, &in6ailen);

      /* If we have information about deprecated and temporary addresses
	 sort the array now.  */
      if (in6ai != NULL)
	qsort (in6ai, in6ailen, sizeof (*in6ai), in6aicmp);

      int fd = -1;
      int af = AF_UNSPEC;

      for (i = 0, q = p; q != NULL; ++i, last = q, q = q->ai_next)
	{
	  results[i].dest_addr = q;
	  results[i].native = -1;
	  order[i] = i;

	  /* If we just looked up the address for a different
	     protocol, reuse the result.  */
	  if (last != NULL && last->ai_addrlen == q->ai_addrlen
	      && memcmp (last->ai_addr, q->ai_addr, q->ai_addrlen) == 0)
	    {
	      memcpy (&results[i].source_addr, &results[i - 1].source_addr,
		      results[i - 1].source_addr_len);
	      results[i].source_addr_len = results[i - 1].source_addr_len;
	      results[i].got_source_addr = results[i - 1].got_source_addr;
	      results[i].source_addr_flags = results[i - 1].source_addr_flags;
	      results[i].prefixlen = results[i - 1].prefixlen;
	      results[i].index = results[i - 1].index;
	    }
	  else
	    {
	      results[i].got_source_addr = false;
	      results[i].source_addr_flags = 0;
	      results[i].prefixlen = 0;
	      results[i].index = 0xffffffffu;

	      /* We overwrite the type with SOCK_DGRAM since we do not
		 want connect() to connect to the other side.  If we
		 cannot determine the source address remember this
		 fact.  */
	      if (fd == -1 || (af == AF_INET && q->ai_family == AF_INET6))
		{
		  if (fd != -1)
		    __close_nocancel_nostatus (fd);
		  af = q->ai_family;
		  fd = __socket (af, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_IP);
		}
	      else
		{
		  /* Reset the connection.  */
		  struct sockaddr sa = { .sa_family = AF_UNSPEC };
		  __connect (fd, &sa, sizeof (sa));
		}

	      if (try_connect (&fd, &af, &results[i].source_addr, q->ai_addr,
			       q->ai_addrlen, q->ai_family))
		{
		  results[i].source_addr_len = sizeof (results[i].source_addr);
		  results[i].got_source_addr = true;

		  if (in6ai != NULL)
		    {
		      /* See whether the source address is on the list of
			 deprecated or temporary addresses.  */
		      struct in6addrinfo tmp;

		      if (q->ai_family == AF_INET && af == AF_INET)
			{
			  struct sockaddr_in *sinp
			    = (struct sockaddr_in *) &results[i].source_addr;
			  tmp.addr[0] = 0;
			  tmp.addr[1] = 0;
			  tmp.addr[2] = htonl (0xffff);
			  /* Special case for lo interface, the source address
			     being possibly different than the interface
			     address. */
			  if ((ntohl(sinp->sin_addr.s_addr) & 0xff000000)
			      == 0x7f000000)
			    tmp.addr[3] = htonl(0x7f000001);
			  else
			    tmp.addr[3] = sinp->sin_addr.s_addr;
			}
		      else
			{
			  struct sockaddr_in6 *sin6p
			    = (struct sockaddr_in6 *) &results[i].source_addr;
			  memcpy (tmp.addr, &sin6p->sin6_addr, IN6ADDRSZ);
			}

		      struct in6addrinfo *found
			= bsearch (&tmp, in6ai, in6ailen, sizeof (*in6ai),
				   in6aicmp);
		      if (found != NULL)
			{
			  results[i].source_addr_flags = found->flags;
			  results[i].prefixlen = found->prefixlen;
			  results[i].index = found->index;
			}
		    }

		  if (q->ai_family == AF_INET && af == AF_INET6)
		    {
		      /* We have to convert the address.  The socket is
			 IPv6 and the request is for IPv4.  */
		      struct sockaddr_in6 *sin6
			= (struct sockaddr_in6 *) &results[i].source_addr;
		      struct sockaddr_in *sin
			= (struct sockaddr_in *) &results[i].source_addr;
		      assert (IN6_IS_ADDR_V4MAPPED (sin6->sin6_addr.s6_addr32));
		      sin->sin_family = AF_INET;
		      /* We do not have to initialize sin_port since this
			 fields has the same position and size in the IPv6
			 structure.  */
		      assert (offsetof (struct sockaddr_in, sin_port)
			      == offsetof (struct sockaddr_in6, sin6_port));
		      assert (sizeof (sin->sin_port)
			      == sizeof (sin6->sin6_port));
		      memcpy (&sin->sin_addr,
			      &sin6->sin6_addr.s6_addr32[3], INADDRSZ);
		      results[i].source_addr_len = sizeof (struct sockaddr_in);
		    }
		}
	      else
		/* Just make sure that if we have to process the same
		   address again we do not copy any memory.  */
		results[i].source_addr_len = 0;
	    }

	  /* Remember the canonical name.  */
	  if (q->ai_canonname != NULL)
	    {
	      assert (canonname == NULL);
	      canonname = q->ai_canonname;
	      q->ai_canonname = NULL;
	    }
	}

      if (fd != -1)
	__close_nocancel_nostatus (fd);

      /* We got all the source addresses we can get, now sort using
	 the information.  */
      struct sort_result_combo src
	= { .results = results, .nresults = nresults };
      if (__glibc_unlikely (gaiconf_reload_flag_ever_set))
	{
	  __libc_lock_define_initialized (static, lock);

	  __libc_lock_lock (lock);
	  if (__libc_once_get (old_once) && gaiconf_reload_flag)
	    gaiconf_reload ();
	  __qsort_r (order, nresults, sizeof (order[0]), rfc3484_sort, &src);
	  __libc_lock_unlock (lock);
	}
      else
	__qsort_r (order, nresults, sizeof (order[0]), rfc3484_sort, &src);

      /* Queue the results up as they come out of sorting.  */
      q = p = results[order[0]].dest_addr;
      for (i = 1; i < nresults; ++i)
	q = q->ai_next = results[order[i]].dest_addr;
      q->ai_next = NULL;

      /* Fill in the canonical name into the new first entry.  */
      p->ai_canonname = canonname;

      scratch_buffer_free (&buf);
    }

  __free_in6ai (in6ai);

  if (p)
    {
      *pai = p;
      return 0;
    }

  return last_i ? -last_i : EAI_NONAME;
}
libc_hidden_def (getaddrinfo)

nss_interface_function (getaddrinfo)

void
freeaddrinfo (struct addrinfo *ai)
{
  struct addrinfo *p;

  while (ai != NULL)
    {
      p = ai;
      ai = ai->ai_next;
      free (p->ai_canonname);
      free (p);
    }
}
libc_hidden_def (freeaddrinfo)
