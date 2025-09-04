/* Return point to next ancillary data entry in message header.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

#include <stddef.h>
#include <sys/socket.h>


struct cmsghdr *
__cmsg_nxthdr (struct msghdr *mhdr, struct cmsghdr *cmsg)
{
  /* We may safely assume that cmsg lies between mhdr->msg_control and
     mhdr->msg_controllen because the user is required to obtain the first
     cmsg via CMSG_FIRSTHDR, set its length, then obtain subsequent cmsgs
     via CMSG_NXTHDR, setting lengths along the way.  However, we don't yet
     trust the value of cmsg->cmsg_len and therefore do not use it in any
     pointer arithmetic until we check its value.  */

  unsigned char * msg_control_ptr = (unsigned char *) mhdr->msg_control;
  unsigned char * cmsg_ptr = (unsigned char *) cmsg;

  size_t size_needed = sizeof (struct cmsghdr)
                       + __CMSG_PADDING (cmsg->cmsg_len);

  /* The current header is malformed, too small to be a full header.  */
  if ((size_t) cmsg->cmsg_len < sizeof (struct cmsghdr))
    return (struct cmsghdr *) 0;

  /* There isn't enough space between cmsg and the end of the buffer to
  hold the current cmsg *and* the next one.  */
  if (((size_t)
         (msg_control_ptr + mhdr->msg_controllen - cmsg_ptr)
       < size_needed)
      || ((size_t)
            (msg_control_ptr + mhdr->msg_controllen - cmsg_ptr
             - size_needed)
          < cmsg->cmsg_len))

    return (struct cmsghdr *) 0;

  /* Now, we trust cmsg_len and can use it to find the next header.  */
  cmsg = (struct cmsghdr *) ((unsigned char *) cmsg
			     + CMSG_ALIGN (cmsg->cmsg_len));
  return cmsg;
}
libc_hidden_def (__cmsg_nxthdr)
