/* Copyright (C) 1994-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <mach.h>
#include <mach/mig_support.h>
#include <tls.h>

/* These functions are called by MiG-generated code.  */

#if !defined (SHARED) || IS_IN (rtld)
mach_port_t __hurd_reply_port0;
#endif

static mach_port_t
get_reply_port (void)
{
#if !defined (SHARED) || IS_IN (rtld)
  if (__LIBC_NO_TLS ())
    return __hurd_reply_port0;
#endif
  return THREAD_GETMEM (THREAD_SELF, reply_port);
}

static void
set_reply_port (mach_port_t port)
{
#if !defined (SHARED) || IS_IN (rtld)
  if (__LIBC_NO_TLS ())
    __hurd_reply_port0 = port;
  else
#endif
    THREAD_SETMEM (THREAD_SELF, reply_port, port);
}

/* Called by MiG to get a reply port.  */
mach_port_t
__mig_get_reply_port (void)
{
  mach_port_t port = get_reply_port ();
  if (__glibc_unlikely (port == MACH_PORT_NULL))
    {
      port = __mach_reply_port ();
      set_reply_port (port);
    }
  return port;
}
weak_alias (__mig_get_reply_port, mig_get_reply_port)
libc_hidden_def (__mig_get_reply_port)

/* Called by MiG to deallocate the reply port.  */
void
__mig_dealloc_reply_port (mach_port_t arg)
{
  error_t err;
  mach_port_t port = get_reply_port ();

  set_reply_port (MACH_PORT_NULL);	/* So the mod_refs RPC won't use it.  */
  assert (port == arg);
  if (!MACH_PORT_VALID (port))
    return;

  err = __mach_port_mod_refs (__mach_task_self (), port,
                              MACH_PORT_RIGHT_RECEIVE, -1);
  if (err == KERN_INVALID_RIGHT)
    /* It could be that during signal handling, the receive right had been
       replaced with a dead name.  */
    err = __mach_port_mod_refs (__mach_task_self (), port,
                                MACH_PORT_RIGHT_DEAD_NAME, -1);

  assert_perror (err);
}
weak_alias (__mig_dealloc_reply_port, mig_dealloc_reply_port)
libc_hidden_def (__mig_dealloc_reply_port)

/* Called by mig interfaces when done with a port.  Used to provide the
   same interface as needed when a custom allocator is used.  */
void
__mig_put_reply_port(mach_port_t port)
{
  /* Do nothing.  */
}
weak_alias (__mig_put_reply_port, mig_put_reply_port)

/* Called at startup with STACK == NULL.  When per-thread variables are set
   up, this is called again with STACK set to the new stack being switched
   to, where per-thread variables should be set up.  */
void
__mig_init (void *stack)
{
  /* Do nothing.  */
}
weak_alias (__mig_init, mig_init)
libc_hidden_def (__mig_init)
