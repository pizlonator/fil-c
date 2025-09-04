/* Hurdish implementation of getrandom
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#include <hurd.h>
#include <sys/random.h>
#include <fcntl.h>

__libc_rwlock_define_initialized (static, lock);
static file_t random_server, random_server_nonblock,
              urandom_server, urandom_server_nonblock;

extern char *__trivfs_server_name __attribute__((weak));

/* Write up to LENGTH bytes of randomness starting at BUFFER.
   Return the number of bytes written, or -1 on error.  */
ssize_t
__getrandom (void *buffer, size_t length, unsigned int flags)
{
  const char *random_source = "/dev/urandom";
  int open_flags = O_RDONLY;
  file_t server, *cached_server;
  error_t err;
  char *data = buffer;
  mach_msg_type_number_t nread = length;

  switch (flags)
    {
    case 0:
      cached_server = &urandom_server;
      break;
    case GRND_RANDOM:
      cached_server = &random_server;
      break;
    case GRND_NONBLOCK:
      cached_server = &urandom_server_nonblock;
      break;
    case GRND_RANDOM | GRND_NONBLOCK:
      cached_server = &random_server_nonblock;
      break;
    default:
      return __hurd_fail (EINVAL);
    }

  if (flags & GRND_RANDOM)
    random_source = "/dev/random";
  if (flags & GRND_NONBLOCK)
    open_flags |= O_NONBLOCK;
  /* No point in passing either O_NOCTTY, O_IGNORE_CTTY, or O_CLOEXEC
     to file_name_lookup, since we're not making an fd.  */

  if (&__trivfs_server_name && __trivfs_server_name
      && __trivfs_server_name[0] == 'r'
      && __trivfs_server_name[1] == 'a'
      && __trivfs_server_name[2] == 'n'
      && __trivfs_server_name[3] == 'd'
      && __trivfs_server_name[4] == 'o'
      && __trivfs_server_name[5] == 'm'
      && __trivfs_server_name[6] == '\0')
    /* We are random, don't try to read ourselves!  */
    return length;

again:
  __libc_rwlock_rdlock (lock);
  server = *cached_server;
  if (MACH_PORT_VALID (server))
    /* Attempt to read some random data using this port.  */
    err = __io_read (server, &data, &nread, -1, length);
  else
    err = MACH_SEND_INVALID_DEST;
  __libc_rwlock_unlock (lock);

  if (err == MACH_SEND_INVALID_DEST || err == MIG_SERVER_DIED)
    {
      file_t oldserver = server;
      mach_port_urefs_t urefs;

      /* Slow path: the cached port didn't work, or there was no
         cached port in the first place.  */

      __libc_rwlock_wrlock (lock);
      server = *cached_server;
      if (server != oldserver)
        {
          /* Someone else must have refetched the port while we were
             waiting for the lock. */
          __libc_rwlock_unlock (lock);
          goto again;
        }

      if (MACH_PORT_VALID (server))
        {
          /* It could be that someone else has refetched the port and
             it got the very same name.  So check whether it is a send
             right (and not a dead name).  */
          err = __mach_port_get_refs (__mach_task_self (), server,
                                      MACH_PORT_RIGHT_SEND, &urefs);
          if (!err && urefs > 0)
            {
              __libc_rwlock_unlock (lock);
              goto again;
            }

          /* Now we're sure that it's dead.  */
          __mach_port_deallocate (__mach_task_self (), server);
        }

      server = *cached_server = __file_name_lookup (random_source,
                                                    open_flags, 0);
      __libc_rwlock_unlock (lock);
      if (!MACH_PORT_VALID (server))
	{
	  if (errno == ENOENT)
	    /* No translator set up, we won't have support for it.  */
	    errno = ENOSYS;
	  /* No luck.  */
	  return -1;
	}

      goto again;
    }

  if (err)
    return __hurd_fail (err);

  if (data != buffer)
    {
      if (nread > length)
        {
          __vm_deallocate (__mach_task_self (), (vm_address_t) data, nread);
          return __hurd_fail (EGRATUITOUS);
        }
      memcpy (buffer, data, nread);
      __vm_deallocate (__mach_task_self (), (vm_address_t) data, nread);
    }

  return nread;
}

libc_hidden_def (__getrandom)
weak_alias (__getrandom, getrandom)
