/* Exception handling in the dynamic linker.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.
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

#include <libintl.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <stdio.h>
#include <tls.h>

/* This structure communicates state between _dl_catch_error and
   _dl_signal_error.  */
struct rtld_catch
  {
    struct dl_exception *exception; /* The exception data is stored there.  */
    volatile int *errcode;	/* Return value of _dl_signal_error.  */
    jmp_buf env;		/* longjmp here on error.  */
  };

/* Multiple threads at once can use the `_dl_catch_error' function.
   The calls can come from `_dl_map_object_deps', `_dlerror_run', or
   from any of the libc functionality which loads dynamic objects
   (NSS, iconv).  Therefore we have to be prepared to save the state
   in thread-local memory.  We use THREAD_GETMEM and THREAD_SETMEM
   instead of ELF TLS because ELF TLS is not available in the dynamic
   loader.  Additionally, the exception handling mechanism must be
   usable before the TCB has been set up, which is why
   rtld_catch_notls is used if !__rtld_tls_init_tp_called.  This is
   not needed for static builds, where initialization completes before
   static dlopen etc. can be called.  */

#if IS_IN (rtld)
static struct rtld_catch *rtld_catch_notls;
#endif

static struct rtld_catch *
get_catch (void)
{
#if IS_IN (rtld)
  if (!__rtld_tls_init_tp_called)
    return rtld_catch_notls;
  else
#endif
    return THREAD_GETMEM (THREAD_SELF, rtld_catch);
}

static void
set_catch (struct rtld_catch *catch)
{
#if IS_IN (rtld)
  if (!__rtld_tls_init_tp_called)
    rtld_catch_notls = catch;
  else
#endif
    THREAD_SETMEM (THREAD_SELF, rtld_catch, catch);
}

/* Lossage while resolving the program's own symbols is always fatal.  */
static void
__attribute__ ((noreturn))
fatal_error (int errcode, const char *objname, const char *occasion,
	     const char *errstring)
{
  char buffer[1024];
  _dl_fatal_printf ("%s: %s: %s%s%s%s%s\n",
		    RTLD_PROGNAME,
		    occasion ?: N_("error while loading shared libraries"),
		    objname, *objname ? ": " : "",
		    errstring, errcode ? ": " : "",
		    (errcode
		     ? __strerror_r (errcode, buffer, sizeof buffer)
		     : ""));
}

void
_dl_signal_exception (int errcode, struct dl_exception *exception,
		      const char *occasion)
{
  struct rtld_catch *lcatch = get_catch ();
  if (lcatch != NULL)
    {
      *lcatch->exception = *exception;
      *lcatch->errcode = errcode;

      /* We do not restore the signal mask because none was saved.  */
      __longjmp (lcatch->env[0].__jmpbuf, 1);
    }
  else
    fatal_error (errcode, exception->objname, occasion, exception->errstring);
}
rtld_hidden_def (_dl_signal_exception)

void
_dl_signal_error (int errcode, const char *objname, const char *occasion,
		  const char *errstring)
{
  struct rtld_catch *lcatch = get_catch ();

  if (! errstring)
    errstring = N_("DYNAMIC LINKER BUG!!!");

  if (lcatch != NULL)
    {
      _dl_exception_create (lcatch->exception, objname, errstring);
      *lcatch->errcode = errcode;

      /* We do not restore the signal mask because none was saved.  */
      __longjmp (lcatch->env[0].__jmpbuf, 1);
    }
  else
    {
      if (objname == NULL)
	objname = "";
      fatal_error (errcode, objname, occasion, errstring);
    }
}
rtld_hidden_def (_dl_signal_error)

#if IS_IN (rtld)
/* This points to a function which is called when a continuable error is
   received.  Unlike the handling of `catch' this function may return.
   The arguments will be the `errstring' and `objname'.

   Since this functionality is not used in normal programs (only in ld.so)
   we do not care about multi-threaded programs here.  We keep this as a
   global variable.  */
static receiver_fct receiver;

void
_dl_signal_cexception (int errcode, struct dl_exception *exception,
		       const char *occasion)
{
  if (__builtin_expect (GLRO(dl_debug_mask)
			& ~(DL_DEBUG_STATISTICS), 0))
    _dl_debug_printf ("%s: error: %s: %s (%s)\n",
		      exception->objname, occasion,
		      exception->errstring, receiver ? "continued" : "fatal");

  if (receiver)
    {
      /* We are inside _dl_receive_error.  Call the user supplied
	 handler and resume the work.  The receiver will still be
	 installed.  */
      (*receiver) (errcode, exception->objname, exception->errstring);
    }
  else
    _dl_signal_exception (errcode, exception, occasion);
}

void
_dl_signal_cerror (int errcode, const char *objname, const char *occasion,
		   const char *errstring)
{
  if (__builtin_expect (GLRO(dl_debug_mask)
			& ~(DL_DEBUG_STATISTICS), 0))
    _dl_debug_printf ("%s: error: %s: %s (%s)\n", objname, occasion,
		      errstring, receiver ? "continued" : "fatal");

  if (receiver)
    {
      /* We are inside _dl_receive_error.  Call the user supplied
	 handler and resume the work.  The receiver will still be
	 installed.  */
      (*receiver) (errcode, objname, errstring);
    }
  else
    _dl_signal_error (errcode, objname, occasion, errstring);
}

void
_dl_receive_error (receiver_fct fct, void (*operate) (void *), void *args)
{
  struct rtld_catch *old_catch = get_catch ();
  receiver_fct old_receiver = receiver;

  /* Set the new values.  */
  set_catch (NULL);
  receiver = fct;

  (*operate) (args);

  set_catch (old_catch);
  receiver = old_receiver;
}
#endif

int
_dl_catch_exception (struct dl_exception *exception,
		     void (*operate) (void *), void *args)
{
  /* If exception is NULL, temporarily disable exception handling.
     Exceptions during operate (args) are fatal.  */
  if (exception == NULL)
    {
      struct rtld_catch *old_catch = get_catch ();
      set_catch (NULL);
      operate (args);
      /* If we get here, the operation was successful.  */
      set_catch (old_catch);
      return 0;
    }

  /* We need not handle `receiver' since setting a `catch' is handled
     before it.  */

  /* Only this needs to be marked volatile, because it is the only local
     variable that gets changed between the setjmp invocation and the
     longjmp call.  All others are just set here (before setjmp) and read
     in _dl_signal_error (before longjmp).  */
  volatile int errcode;

  struct rtld_catch c;
  /* Don't use an initializer since we don't need to clear C.env.  */
  c.exception = exception;
  c.errcode = &errcode;

  struct rtld_catch *old = get_catch ();
  set_catch (&c);

  /* Do not save the signal mask.  */
  if (__builtin_expect (__sigsetjmp (c.env, 0), 0) == 0)
    {
      (*operate) (args);
      set_catch (old);
      *exception = (struct dl_exception) { NULL };
      return 0;
    }

  /* We get here only if we longjmp'd out of OPERATE.
     _dl_signal_exception has already stored values into
     *EXCEPTION.  */
  set_catch (old);
  return errcode;
}
rtld_hidden_def (_dl_catch_exception)

int
_dl_catch_error (const char **objname, const char **errstring,
		 bool *mallocedp, void (*operate) (void *), void *args)
{
  struct dl_exception exception;
  int errorcode = _dl_catch_exception (&exception, operate, args);
  *objname = exception.objname;
  *errstring = exception.errstring;
  *mallocedp = exception.message_buffer == exception.errstring;
  return errorcode;
}
