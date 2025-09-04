/* Implement suppression of AAAA queries.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <resolv.h>
#include <string.h>
#include <resolv-internal.h>
#include <resolv_context.h>
#include <arpa/nameser.h>

/* Returns true if the question type at P matches EXPECTED, and the
   class is IN.  */
static bool
qtype_matches (const unsigned char *p, int expected)
{
  /* This assumes that T_A/C_IN constants are less than 256, which
     they are.  */
  return p[0] == 0 && p[1] == expected && p[2] == 0 && p[3] == C_IN;
}

/* Handle RES_NOAAAA translation of AAAA queries.  To produce a Name
   Error (NXDOMAIN) response for domain names that do not exist, it is
   still necessary to send a query.  Using question type A is a
   conservative choice.  In the returned answer, it is necessary to
   switch back the question type to AAAA.  */
bool
__res_handle_no_aaaa (struct resolv_context *ctx,
                      const unsigned char *buf, int buflen,
                      unsigned char *ans, int anssiz, int *result)
{
  /* AAAA mode is not active, or the query looks invalid (will not be
     able to be parsed).  */
  if ((ctx->resp->options & RES_NOAAAA) == 0
      || buflen <= sizeof (HEADER))
    return false;

  /* The replacement A query is produced here.  */
  struct
  {
    HEADER header;
    unsigned char question[NS_MAXCDNAME + 4];
  } replacement;
  memcpy (&replacement.header, buf, sizeof (replacement.header));

  if (replacement.header.qr
      || replacement.header.opcode != 0
      || replacement.header.rcode != 0
      || ntohs (replacement.header.qdcount) != 1
      || ntohs (replacement.header.ancount) != 0
      || ntohs (replacement.header.nscount) != 0)
    /* Not a well-formed question.  Let the core resolver code produce
       the proper error.  */
    return false;

  /* Disable EDNS0.  */
  replacement.header.arcount = htons (0);

  /* Extract the QNAME.  */
  int ret = __ns_name_unpack (buf, buf + buflen, buf + sizeof (HEADER),
                              replacement.question, NS_MAXCDNAME);
  if (ret < 0)
    /* Format error.  */
    return false;

  /* Compute the end of the question name.  */
  const unsigned char *after_question = buf + sizeof (HEADER) + ret;

  /* Check that we are dealing with an AAAA query.  */
  if (buf + buflen - after_question < 4
      || !qtype_matches (after_question, T_AAAA))
    return false;

  /* Find the place to store the type/class data in the replacement
     query.  */
  after_question = replacement.question;
  /* This cannot fail because __ns_name_unpack above produced a valid
     domain name.  */
  (void) __ns_name_skip (&after_question, &replacement.question[NS_MAXCDNAME]);
  unsigned char *start_of_query = (unsigned char *) &replacement;
  const unsigned char *end_of_query = after_question + 4;

  /* Produce an A/IN query.  */
  {
    unsigned char *p = (unsigned char *) after_question;
    p[0] = 0;
    p[1] = T_A;
    p[2] = 0;
    p[3] = C_IN;
  }

  /* Clear the output buffer, to avoid reading undefined data when
     rewriting the result from A to AAAA.  */
  memset (ans, 0, anssiz);

  /* Always perform the message translation, independent of the error
     code.  */
  ret = __res_context_send (ctx,
                            start_of_query, end_of_query - start_of_query,
                            NULL, 0, ans, anssiz,
                            NULL, NULL, NULL, NULL, NULL);

  /* Patch in the AAAA question type if there is room and the A query
     type was received.  */
  after_question = ans + sizeof (HEADER);
  if (__ns_name_skip (&after_question, ans + anssiz) == 0
      && ans + anssiz - after_question >= 4
      && qtype_matches (after_question, T_A))
    {
      ((unsigned char *) after_question)[1] = T_AAAA;

      /* Create an aligned copy of the header.  Hide all data except
         the question from the response.  Put back the header.  There is
         no need to change the response code.  The zero answer count turns
         a positive response with data into a no-data response.  */
      memcpy (&replacement.header, ans, sizeof (replacement.header));
      replacement.header.ancount = htons (0);
      replacement.header.nscount = htons (0);
      replacement.header.arcount = htons (0);
      memcpy (ans, &replacement.header, sizeof (replacement.header));

      /* Truncate the reply.  */
      if (ret <= 0)
        *result = ret;
      else
        *result = after_question - ans + 4;
    }

  return true;
}
