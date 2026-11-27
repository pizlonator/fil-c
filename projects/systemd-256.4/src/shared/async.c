/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async.h"
#include "errno-util.h"
#include "fd-util.h"
#include "log.h"
#include "macro.h"
#include "process-util.h"
#include "signal-util.h"

int asynchronous_sync(pid_t *ret_pid) {
        int r;

        /* This forks off an invocation of fork() as a child process, in order to initiate synchronization to
         * disk. Note that we implement this as helper process rather than thread as we don't want the sync() to hang our
         * original process ever, and a thread would do that as the process can't exit with threads hanging in blocking
         * syscalls. */

        r = safe_fork("(sd-sync)", FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS|(ret_pid ? 0 : FORK_DETACH), ret_pid);
        if (r < 0)
                return r;
        if (r == 0) {
                /* Child process */
                sync();
                _exit(EXIT_SUCCESS);
        }

        return 0;
}

/* Fil-C: the close job below must not use the raw clone() machinery upstream
 * relies on.  Upstream spawns it via clone_with_nested_stack(), i.e. glibc's
 * clone() wrapper with CLONE_FILES so that the child shares our fd table and
 * closes our descriptor, but the Fil-C user libc stubs that wrapper out with
 * "clone not implemented." (raw clone() with a nested stack cannot be made
 * safe by the pizlonator), so asynchronous_close() would panic under Fil-C
 * wherever it is called.  The close job is a trivial close(fd)+exit worker: it
 * needs no mount namespace, and a plain fork() would not do the job either,
 * since a fork()ed child only closes its own copy of the descriptor - without
 * CLONE_FILES the parent's fd would stay open.  Hence the close job runs in a
 * detached thread instead: a thread shares our fd table exactly like a
 * CLONE_FILES child, it needs no reaping (no double fork, no zombies, no
 * __WCLONE wait), and the Fil-C runtime implements pthreads natively (and
 * makes fork() in a process that has threads safe).  If thread creation fails,
 * we fall back to a synchronous close, which is what upstream does when its
 * clone fails, too. */

static void *close_thread_func(void *p) {
        unsigned v = PTR_TO_UINT(p);

        (void) prctl(PR_SET_NAME, (unsigned long*) "(sd-close)");

        close((int) v); /* no assert() here, we are in a helper thread and the result would be eaten up anyway */
        return NULL;
}

int asynchronous_close(int fd) {
        pthread_t thread;
        int r;

        /* This is supposed to behave similar to safe_close(), but actually invoke close() asynchronously, so
         * that it will never block. Ideally the kernel would have an API for this, but it doesn't, so we
         * work around it, and hide this as a far away as we can. */

        if (fd < 0)
                return -EBADF; /* already invalid */

        PROTECT_ERRNO;

        r = pthread_create(&thread, NULL, close_thread_func, UINT_TO_PTR((unsigned) fd));
        if (r != 0)
                safe_close(fd); /* local fallback */
        else
                (void) pthread_detach(thread);

        return -EBADF; /* return an invalidated fd */
}

int asynchronous_rm_rf(const char *p, RemoveFlags flags) {
        int r;

        assert(p);

        /* Forks off a child that destroys the specified path. This will be best effort only, i.e. the child
         * will attempt to do its thing, but we won't wait for it or check its success. */

        r = safe_fork("(sd-rmrf)", FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS|FORK_DETACH, NULL);
        if (r != 0)
                return r;

        /* Child */

        r = rm_rf(p, flags);
        if (r < 0) {
                log_debug_errno(r, "Failed to rm -rf '%s', ignoring: %m", p);
                _exit(EXIT_FAILURE); /* This is a detached process, hence no one really cares, but who knows
                                      * maybe it's good for debugging/tracing to return an exit code
                                      * indicative of our failure here. */
        }

        _exit(EXIT_SUCCESS);
}
