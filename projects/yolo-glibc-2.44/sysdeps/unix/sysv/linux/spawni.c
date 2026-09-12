/* POSIX spawn interface.  Linux version.
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

#include <internal-signals.h>
#include <ldsodefs.h>
#include <local-setxid.h>
#include <not-cancel.h>
#include <paths.h>
#include <shlib-compat.h>
#include <spawn.h>
#include <spawn_int.h>
#include <sysdep.h>
#include <sys/resource.h>
#include <clone_internal.h>

/* The Linux implementation of posix_spawn{p} uses the clone syscall directly
   with CLONE_VM and CLONE_VFORK flags and an allocated stack.  The new stack
   and start function solves most the vfork limitation (possible parent
   clobber due stack spilling). The remaining issue are:

   1. That no signal handlers must run in child context, to avoid corrupting
      parent's state.
   2. The parent must ensure child's stack freeing.
   3. Child must synchronize with parent to enforce 2. and to possible
      return execv issues.

   The first issue is solved by blocking all signals in child, even
   the NPTL-internal ones (SIGCANCEL and SIGSETXID).  The second and
   third issue is done by a stack allocation in parent, and by using a
   field in struct spawn_args where the child can write an error
   code. CLONE_VFORK ensures that the parent does not run until the
   child has either exec'ed successfully or exited.  */


/* The Unix standard contains a long explanation of the way to signal
   an error after the fork() was successful.  Since no new wait status
   was wanted there is no way to signal an error using one of the
   available methods.  The committee chose to signal an error by a
   normal program exit with the exit code 127.  */
#define SPAWN_ERROR	127


struct posix_spawn_args
{
  internal_sigset_t oldmask;
  const char *file;
  int (*exec) (const char *, char *const *, char *const *);
  const posix_spawn_file_actions_t *fa;
  const posix_spawnattr_t *restrict attr;
  char *const *argv;
  ptrdiff_t argc;
  char *const *envp;
  int xflags;
  bool use_clone3;
  int err;
  int pidfd;
};

/* Older version requires that shell script without shebang definition
   to be called explicitly using /bin/sh (_PATH_BSHELL).  */
static void
maybe_script_execute (struct posix_spawn_args *args)
{
  if (SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_15)
      && (args->xflags & SPAWN_XFLAGS_TRY_SHELL) && errno == ENOEXEC)
    {
      char *const *argv = args->argv;
      ptrdiff_t argc = args->argc;

      /* Construct an argument list for the shell.  */
      char *new_argv[argc + 2];
      new_argv[0] = (char *) _PATH_BSHELL;
      new_argv[1] = (char *) args->file;
      if (argc > 1)
	memcpy (new_argv + 2, argv + 1, argc * sizeof (char *));
      else
	new_argv[2] = NULL;

      /* Execute the shell.  */
      args->exec (new_argv[0], new_argv, args->envp);
    }
}

/* Function used in the clone call to setup the signals mask, posix_spawn
   attributes, and file actions.  It run on its own stack (provided by the
   posix_spawn call).  */
static int
__spawni_child (void *arguments)
{
  struct posix_spawn_args *args = arguments;
  const posix_spawnattr_t *restrict attr = args->attr;
  const posix_spawn_file_actions_t *file_actions = args->fa;

  /* The child must ensure that no signal handler is enabled because it
     shares memory with parent, so all signal dispositions must be either
     SIG_DFL or SIG_IGN.  If clone3/CLONE_CLEAR_SIGHAND is used, there is
     only the need to set the defined signals POSIX_SPAWN_SETSIGDEF to
     SIG_DFL; otherwise, the code iterates over all signals.  */
  struct sigaction sa;
  memset (&sa, '\0', sizeof (sa));

  sigset_t hset;
  __sigprocmask (SIG_BLOCK, NULL, &hset);
  for (int sig = 1; sig < _NSIG; ++sig)
    {
      if ((attr->__flags & POSIX_SPAWN_SETSIGDEF)
	  && __sigismember (&attr->__sd, sig))
	{
	  sa.sa_handler = SIG_DFL;
	}
      else if (!args->use_clone3 && __sigismember (&hset, sig))
	{
	  if (is_internal_signal (sig))
	    sa.sa_handler = SIG_IGN;
	  else
	    {
	      __libc_sigaction (sig, NULL, &sa);
	      if (sa.sa_handler == SIG_IGN || sa.sa_handler == SIG_DFL)
		continue;
	      sa.sa_handler = SIG_DFL;
	    }
	}
      else
	continue;

      __libc_sigaction (sig, &sa, NULL);
    }

#ifdef _POSIX_PRIORITY_SCHEDULING
  /* Set the scheduling algorithm and parameters.  */
  if ((attr->__flags & (POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER))
      == POSIX_SPAWN_SETSCHEDPARAM)
    {
      if (__sched_setparam (0, &attr->__sp) == -1)
	goto fail;
    }
  else if ((attr->__flags & POSIX_SPAWN_SETSCHEDULER) != 0)
    {
      if (__sched_setscheduler (0, attr->__policy, &attr->__sp) == -1)
	goto fail;
    }
#endif

  if ((attr->__flags & POSIX_SPAWN_SETSID) != 0
      && __setsid () < 0)
    goto fail;

  /* Set the process group ID.  */
  if ((attr->__flags & POSIX_SPAWN_SETPGROUP) != 0
      && __setpgid (0, attr->__pgrp) != 0)
    goto fail;

  /* Set the effective user and group IDs.  */
  if ((attr->__flags & POSIX_SPAWN_RESETIDS) != 0
      && (local_seteuid (__getuid ()) != 0
	  || local_setegid (__getgid ()) != 0))
    goto fail;

  /* Execute the file actions.  */
  if (file_actions != NULL)
    {
      int cnt;
      struct rlimit64 fdlimit;
      bool have_fdlimit = false;

      for (cnt = 0; cnt < file_actions->__used; ++cnt)
	{
	  struct __spawn_action *action = &file_actions->__actions[cnt];

	  switch (action->tag)
	    {
	    case spawn_do_close:
	      if (__close_nocancel (action->action.close_action.fd) != 0)
		{
		  if (!have_fdlimit)
		    {
		      __getrlimit64 (RLIMIT_NOFILE, &fdlimit);
		      have_fdlimit = true;
		    }

		  /* Signal errors only for file descriptors out of range.  */
		  if (action->action.close_action.fd < 0
		      || action->action.close_action.fd >= fdlimit.rlim_cur)
		    goto fail;
		}
	      break;

	    case spawn_do_open:
	      {
		/* POSIX states that if fildes was already an open file descriptor,
		   it shall be closed before the new file is opened.  This avoid
		   potential issues when posix_spawn plus addopen action is called
		   with the process already at maximum number of file descriptor
		   opened and also for multiple actions on single-open special
		   paths (like /dev/watchdog).  */
		__close_nocancel (action->action.open_action.fd);

		int ret = __open_nocancel (action->action.open_action.path,
					   action->action.
					   open_action.oflag | O_LARGEFILE,
					   action->action.open_action.mode);

		if (ret == -1)
		  goto fail;

		int new_fd = ret;

		/* Make sure the desired file descriptor is used.  */
		if (ret != action->action.open_action.fd)
		  {
		    if (__dup2 (new_fd, action->action.open_action.fd)
			!= action->action.open_action.fd)
		      goto fail;

		    if (__close_nocancel (new_fd) != 0)
		      goto fail;
		  }
	      }
	      break;

	    case spawn_do_dup2:
	      /* Austin Group issue #411 requires adddup2 action with source
		 and destination being equal to remove close-on-exec flag.  */
	      if (action->action.dup2_action.fd
		  == action->action.dup2_action.newfd)
		{
		  int fd = action->action.dup2_action.newfd;
		  int flags = __fcntl (fd, F_GETFD, 0);
		  if (flags == -1)
		    goto fail;
		  if (__fcntl (fd, F_SETFD, flags & ~FD_CLOEXEC) == -1)
		    goto fail;
		}
	      else if (__dup2 (action->action.dup2_action.fd,
			       action->action.dup2_action.newfd)
		       != action->action.dup2_action.newfd)
		goto fail;
	      break;

	    case spawn_do_chdir:
	      if (__chdir (action->action.chdir_action.path) != 0)
		goto fail;
	      break;

	    case spawn_do_fchdir:
	      if (__fchdir (action->action.fchdir_action.fd) != 0)
		goto fail;
	      break;

	    case spawn_do_closefrom:
	      {
		int lowfd = action->action.closefrom_action.from;
	        int r = INLINE_SYSCALL_CALL (close_range, lowfd, ~0U, 0);
		if (r != 0 && !__closefrom_fallback (lowfd, false))
		  goto fail;
	      } break;

	    case spawn_do_tcsetpgrp:
	      {
		/* Check if it is possible to avoid an extra syscall.  */
		pid_t pgrp = (attr->__flags & POSIX_SPAWN_SETPGROUP) != 0
			       && attr->__pgrp != 0
			     ? attr->__pgrp : __getpgid (0);
		if (__tcsetpgrp (action->action.setpgrp_action.fd, pgrp) != 0)
		  goto fail;
	      }
	    }
	}
    }

  /* Set the initial signal mask of the child if POSIX_SPAWN_SETSIGMASK
     is set, otherwise restore the previous one.  */
  if (attr->__flags & POSIX_SPAWN_SETSIGMASK)
    __sigprocmask (SIG_SETMASK, &attr->__ss, NULL);
  else
    internal_sigprocmask (SIG_SETMASK, &args->oldmask, NULL);

  args->exec (args->file, args->argv, args->envp);

  /* This is compatibility function required to enable posix_spawn run
     script without shebang definition for older posix_spawn versions
     (2.15).  */
  maybe_script_execute (args);

fail:
  /* errno should have an appropriate non-zero value; otherwise,
     there's a bug in glibc or the kernel.  For lack of an error code
     (EINTERNALBUG) describing that, use ECHILD.  Another option would
     be to set args->err to some negative sentinel and have the parent
     abort(), but that seems needlessly harsh.  */
  args->err = errno ? : ECHILD;
  _exit (SPAWN_ERROR);
}

/* Spawn a new process executing PATH with the attributes describes in *ATTRP.
   Before running the process perform the actions described in FILE-ACTIONS. */
static int
__spawnix (int *pid, const char *file,
	   const posix_spawn_file_actions_t * file_actions,
	   const posix_spawnattr_t * attrp, char *const argv[],
	   char *const envp[], int xflags,
	   int (*exec) (const char *, char *const *, char *const *))
{
  pid_t new_pid;
  struct posix_spawn_args args;
  int ec;

  bool use_pidfd = xflags & SPAWN_XFLAGS_RET_PIDFD;

  /* For CLONE_PIDFD, older kernels might not fail with unsupported flags or
     some versions might not support waitid (P_PIDFD).  So to avoid the need
     to handle the error on the helper process, check for full pidfd
     support.
     ENOSYS is returned because without proper waitid support, pidfd_spawn
     can not be used proporly independently of its arguments.  */
  if (use_pidfd && !__clone_pidfd_supported ())
    return ENOSYS;

  /* To avoid imposing hard limits on posix_spawn{p} the total number of
     arguments is first calculated to allocate a mmap to hold all possible
     values.  */
  ptrdiff_t argc = 0;
  /* Linux allows at most max (0x7FFFFFFF, 1/4 stack size) arguments
     to be used in a execve call.  We limit to INT_MAX minus one due the
     compatibility code that may execute a shell script (maybe_script_execute)
     where it will construct another argument list with an additional
     argument.  */
  ptrdiff_t limit = INT_MAX - 1;
  while (argv[argc++] != NULL)
    if (argc == limit)
      {
	errno = E2BIG;
	return errno;
      }

  /* Add a slack area for child's stack.  */
  size_t argv_size = (argc * sizeof (void *)) + 512;
  /* We need at least a few pages in case the compiler's stack checking is
     enabled.  In some configs, it is known to use at least 24KiB.  We use
     32KiB to be "safe" from anything the compiler might do.  Besides, the
     extra pages won't actually be allocated unless they get used.
     It also acts the slack for spawn_closefrom (including MIPS64 getdents64
     where it might use about 1k extra stack space).  */
  argv_size += (32 * 1024);
  size_t stack_size = ALIGN_UP (argv_size, GLRO(dl_pagesize));
  void *stack = __mmap (NULL, stack_size, GL (dl_stack_prot_flags),
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (__glibc_unlikely (stack == MAP_FAILED))
    return errno;

  /* Disable asynchronous cancellation.  */
  int state;
  __pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);

  /* Child must set args.err to something non-negative - we rely on
     the parent and child sharing VM.  */
  args.err = 0;
  args.file = file;
  args.exec = exec;
  args.fa = file_actions;
  args.attr = attrp ? attrp : &(const posix_spawnattr_t) { 0 };
  args.argv = argv;
  args.argc = argc;
  args.envp = envp;
  args.pidfd = 0;
  args.xflags = xflags;

  /* Avoid the potential issues if caller sets a SIG_IGN for SIGABRT, calls
     abort, and another thread issues posix_spawn just after the sigaction
     returns.  With default options (not setting POSIX_SPAWN_SETSIGDEF), the
     process can still see SIG_DFL for SIGABRT, where it should be SIG_IGN.  */
  __abort_lock_rdlock (&args.oldmask);

  /* The clone flags used will create a new child that will run in the same
     memory space (CLONE_VM) and the execution of calling thread will be
     suspend until the child calls execve or _exit.

     Also since the calling thread execution will be suspend, there is not
     need for CLONE_SETTLS.  Although parent and child share the same TLS
     namespace, there will be no concurrent access for TLS variables (errno
     for instance).  */
  bool set_cgroup = attrp ? (attrp->__flags & POSIX_SPAWN_SETCGROUP) : false;
  struct clone_args clone_args =
    {
      /* Unsupported flags like CLONE_CLEAR_SIGHAND will be cleared up by
	 __clone_internal_fallback.  */
      .flags = (set_cgroup ? CLONE_INTO_CGROUP : 0)
	       | (use_pidfd ? CLONE_PIDFD : 0)
	       | CLONE_CLEAR_SIGHAND
	       | CLONE_VM
	       | CLONE_VFORK,
      .exit_signal = SIGCHLD,
      .stack = (uintptr_t) stack,
      .stack_size = stack_size,
      .cgroup = (set_cgroup ? attrp->__cgroup : 0),
      .pidfd = use_pidfd ? (uintptr_t) &args.pidfd : 0,
      /* This is require for clone fallback, where pidfd is returned
	 on parent_tid.  */
      .parent_tid = use_pidfd ? (uintptr_t) &args.pidfd : 0,
    };
#ifdef HAVE_CLONE3_WRAPPER
  args.use_clone3 = true;
  new_pid = __clone3 (&clone_args, sizeof (clone_args), __spawni_child,
		      &args);
  /* clone3 was added in 5.3 and CLONE_CLEAR_SIGHAND in 5.5.  */
  if (new_pid == -1 && (errno == ENOSYS || errno == EINVAL))
#endif
    {
      args.use_clone3 = false;
      if (!set_cgroup)
	new_pid = __clone_internal_fallback (&clone_args, __spawni_child,
					     &args);
      else
	{
	  /* No fallback for POSIX_SPAWN_SETCGROUP if clone3 is not
	     supported.  */
	  new_pid = -1;
#ifdef HAVE_CLONE3_WRAPPER
	  if (errno == ENOSYS)
#endif
	    errno = ENOTSUP;
	}
    }

  /* It needs to collect the case where the auxiliary process was created
     but failed to execute the file (due either any preparation step or
     for execve itself).  */
  if (new_pid > 0)
    {
      /* Also, it handles the unlikely case where the auxiliary process was
	 terminated before calling execve as if it was successfully.  The
	 args.err is set to 0 as default and changed to a positive value
	 only in case of failure, so in case of premature termination
	 due a signal args.err will remain zeroed and it will be up to
	 caller to actually collect it.  */
      ec = args.err;
      if (ec > 0)
	{
	  /* There still an unlikely case where the child is cancelled after
	     setting args.err, due to a positive error value.  Also there is
	     possible pid reuse race (where the kernel allocated the same pid
	     to an unrelated process).  Unfortunately due synchronization
	     issues where the kernel might not have the process collected
	     the waitpid below can not use WNOHANG.  */
	  __waitid (use_pidfd ? P_PIDFD : P_PID,
		    use_pidfd ? args.pidfd : new_pid,
		    NULL,
		    WEXITED);
	  /* For pidfd we need to also close the file descriptor for the case
	     where execve fails.  */
	  if (use_pidfd)
	    __close_nocancel_nostatus (args.pidfd);
	}
    }
  else
    ec = errno;

  __munmap (stack, stack_size);

  if ((ec == 0) && (pid != NULL))
    *pid = use_pidfd ? args.pidfd : new_pid;

  __abort_lock_unlock (&args.oldmask);

  __pthread_setcancelstate (state, NULL);

  return ec;
}

/* Spawn a new process executing PATH with the attributes describes in *ATTRP.
   Before running the process perform the actions described in FILE-ACTIONS. */
int
__spawni (pid_t * pid, const char *file,
	  const posix_spawn_file_actions_t * acts,
	  const posix_spawnattr_t * attrp, char *const argv[],
	  char *const envp[], int xflags)
{
  /* It uses __execvpex to avoid run ENOEXEC in non compatibility mode (it
     will be handled by maybe_script_execute).  */
  return __spawnix (pid, file, acts, attrp, argv, envp, xflags,
		    xflags & SPAWN_XFLAGS_USE_PATH ? __execvpex :__execve);
}
