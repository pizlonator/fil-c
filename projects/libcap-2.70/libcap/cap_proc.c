/*
 * Copyright (c) 1997-8,2007,11,19-21 Andrew G Morgan <morgan@kernel.org>
 *
 * This file deals with getting and setting capabilities on processes.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <grp.h>
#include <sys/prctl.h>
#include <sys/securebits.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "libcap.h"

/*
 * libcap uses this abstraction for all system calls that change
 * kernel managed capability state. This permits the user to redirect
 * it for testing and also to better implement posix semantics when
 * using pthreads.
 */

static long int _cap_syscall3(long int syscall_nr,
			      long int arg1, long int arg2, long int arg3)
{
    return syscall(syscall_nr, arg1, arg2, arg3);
}

static long int _cap_syscall6(long int syscall_nr,
			      long int arg1, long int arg2, long int arg3,
			      long int arg4, long int arg5, long int arg6)
{
    return syscall(syscall_nr, arg1, arg2, arg3, arg4, arg5, arg6);
}

/*
 * to keep the structure of the code conceptually similar in C and Go
 * implementations, we introduce this abstraction for invoking state
 * writing system calls. In psx+pthreaded code, the fork
 * implementation provided by nptl ensures that we can consistently
 * use the multithreaded syscalls even in the child after a fork().
 */
struct syscaller_s {
    long int (*three)(long int syscall_nr,
		      long int arg1, long int arg2, long int arg3);
    long int (*six)(long int syscall_nr,
		    long int arg1, long int arg2, long int arg3,
		    long int arg4, long int arg5, long int arg6);
};

/* use this syscaller for multi-threaded code */
static struct syscaller_s multithread = {
    .three = _cap_syscall3,
    .six = _cap_syscall6
};

/* use this syscaller for single-threaded code */
static struct syscaller_s singlethread = {
    .three = _cap_syscall3,
    .six = _cap_syscall6
};

/*
 * This gets reset to 0 if we are *not* linked with libpsx.
 */
static int _libcap_overrode_syscalls = 1;

/*
 * psx_load_syscalls() is weakly defined so we can have it overridden
 * by libpsx if that library is linked. Specifically, when libcap
 * calls psx_load_sycalls() it is prepared to override the default
 * values for the syscalls that libcap uses to change security state.
 * As can be seen here this present function is mostly a
 * no-op. However, if libpsx is linked, the one present in that
 * library (not being weak) will replace this one and the
 * _libcap_overrode_syscalls value isn't forced to zero.
 *
 * Note: we hardcode the prototype for the psx_load_syscalls()
 * function here so the compiler isn't worried. If we force the build
 * to include the header, we are close to requiring the optional
 * libpsx to be linked.
 */
void psx_load_syscalls(long int (**syscall_fn)(long int,
					      long int, long int, long int),
		       long int (**syscall6_fn)(long int,
						long int, long int, long int,
						long int, long int, long int));

__attribute__((weak))
void psx_load_syscalls(long int (**syscall_fn)(long int,
					       long int, long int, long int),
		       long int (**syscall6_fn)(long int,
						long int, long int, long int,
						long int, long int, long int))
{
    _libcap_overrode_syscalls = 0;
}

/*
 * cap_set_syscall overrides the state setting syscalls that libcap does.
 * Generally, you don't need to call this manually: libcap tries hard to
 * set things up appropriately.
 */
void cap_set_syscall(long int (*new_syscall)(long int,
					     long int, long int, long int),
			    long int (*new_syscall6)(long int, long int,
						     long int, long int,
						     long int, long int,
						     long int)) {
    if (new_syscall == NULL) {
	psx_load_syscalls(&multithread.three, &multithread.six);
    } else {
	multithread.three = new_syscall;
	multithread.six = new_syscall6;
    }
}

static int _libcap_capset(struct syscaller_s *sc,
			  cap_user_header_t header, const cap_user_data_t data)
{
    if (_libcap_overrode_syscalls) {
	return sc->three(SYS_capset, (long int) header, (long int) data, 0);
    }
    return capset(header, data);
}

static int _libcap_wprctl3(struct syscaller_s *sc,
			   long int pr_cmd, long int arg1, long int arg2)
{
    if (_libcap_overrode_syscalls) {
	int result;
	result = sc->three(SYS_prctl, pr_cmd, arg1, arg2);
	if (result >= 0) {
	    return result;
	}
	errno = -result;
	return -1;
    }
    return prctl(pr_cmd, arg1, arg2, 0, 0, 0);
}

static int _libcap_wprctl6(struct syscaller_s *sc,
			   long int pr_cmd, long int arg1, long int arg2,
			   long int arg3, long int arg4, long int arg5)
{
    if (_libcap_overrode_syscalls) {
	int result;
	result = sc->six(SYS_prctl, pr_cmd, arg1, arg2, arg3, arg4, arg5);
	if (result >= 0) {
	    return result;
	}
	errno = -result;
	return -1;
    }
    return prctl(pr_cmd, arg1, arg2, arg3, arg4, arg5);
}

/*
 * cap_get_proc obtains the capability set for the current process.
 */
cap_t cap_get_proc(void)
{
    cap_t result;

    /* allocate a new capability set */
    result = cap_init();
    if (result) {
	_cap_debug("getting current process' capabilities");

	/* fill the capability sets via a system call */
	if (capget(&result->head, &result->u[0].set)) {
	    cap_free(result);
	    result = NULL;
	}
    }

    return result;
}

static int _cap_set_proc(struct syscaller_s *sc, cap_t cap_d) {
    int retval;

    if (!good_cap_t(cap_d)) {
	errno = EINVAL;
	return -1;
    }

    _cap_debug("setting process capabilities");
    _cap_mu_lock(&cap_d->mutex);
    retval = _libcap_capset(sc, &cap_d->head, &cap_d->u[0].set);
    _cap_mu_unlock(&cap_d->mutex);

    return retval;
}

int cap_set_proc(cap_t cap_d)
{
    return _cap_set_proc(&multithread, cap_d);
}

/* the following two functions are not required by POSIX */

/* read the caps on a specific process */

int capgetp(pid_t pid, cap_t cap_d)
{
    int error;

    if (!good_cap_t(cap_d)) {
	errno = EINVAL;
	return -1;
    }

    _cap_debug("getting process capabilities for proc %d", pid);

    _cap_mu_lock(&cap_d->mutex);
    cap_d->head.pid = pid;
    error = capget(&cap_d->head, &cap_d->u[0].set);
    cap_d->head.pid = 0;
    _cap_mu_unlock(&cap_d->mutex);

    return error;
}

/* allocate space for and return capabilities of target process */

cap_t cap_get_pid(pid_t pid)
{
    cap_t result;

    result = cap_init();
    if (result) {
	if (capgetp(pid, result) != 0) {
	    int my_errno;

	    my_errno = errno;
	    cap_free(result);
	    errno = my_errno;
	    result = NULL;
	}
    }

    return result;
}

/*
 * set the caps on a specific process/pg etc.. The kernel has long
 * since deprecated this asynchronous interface. DON'T EXPECT THIS TO
 * EVER WORK AGAIN.
 */

int capsetp(pid_t pid, cap_t cap_d)
{
    int error;

    if (!good_cap_t(cap_d)) {
	errno = EINVAL;
	return -1;
    }

    _cap_debug("setting process capabilities for proc %d", pid);
    _cap_mu_lock(&cap_d->mutex);
    cap_d->head.pid = pid;
    error = capset(&cap_d->head, &cap_d->u[0].set);
    cap_d->head.version = _LIBCAP_CAPABILITY_VERSION;
    cap_d->head.pid = 0;
    _cap_mu_unlock(&cap_d->mutex);

    return error;
}

/* the kernel api requires unsigned long arguments */
#define pr_arg(x) ((unsigned long) x)

/* get a capability from the bounding set */

int cap_get_bound(cap_value_t cap)
{
    return prctl(PR_CAPBSET_READ, pr_arg(cap), pr_arg(0));
}

static int _cap_drop_bound(struct syscaller_s *sc, cap_value_t cap)
{
    return _libcap_wprctl3(sc, PR_CAPBSET_DROP, pr_arg(cap), pr_arg(0));
}

/* drop a capability from the bounding set */

int cap_drop_bound(cap_value_t cap) {
    return _cap_drop_bound(&multithread, cap);
}

/* get a capability from the ambient set */

int cap_get_ambient(cap_value_t cap)
{
    int result;
    result = prctl(PR_CAP_AMBIENT, pr_arg(PR_CAP_AMBIENT_IS_SET),
		   pr_arg(cap), pr_arg(0), pr_arg(0));
    if (result < 0) {
	errno = -result;
	return -1;
    }
    return result;
}

static int _cap_set_ambient(struct syscaller_s *sc,
			    cap_value_t cap, cap_flag_value_t set)
{
    int val;
    switch (set) {
    case CAP_SET:
	val = PR_CAP_AMBIENT_RAISE;
	break;
    case CAP_CLEAR:
	val = PR_CAP_AMBIENT_LOWER;
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return _libcap_wprctl6(sc, PR_CAP_AMBIENT, pr_arg(val), pr_arg(cap),
			   pr_arg(0), pr_arg(0), pr_arg(0));
}

/*
 * cap_set_ambient modifies a single ambient capability value.
 */
int cap_set_ambient(cap_value_t cap, cap_flag_value_t set)
{
    return _cap_set_ambient(&multithread, cap, set);
}

static int _cap_reset_ambient(struct syscaller_s *sc)
{
    int olderrno = errno;
    cap_value_t c;
    int result = 0;

    for (c = 0; !result; c++) {
	result = cap_get_ambient(c);
	if (result == -1) {
	    errno = olderrno;
	    return 0;
	}
    }

    return _libcap_wprctl6(sc, PR_CAP_AMBIENT,
			   pr_arg(PR_CAP_AMBIENT_CLEAR_ALL),
			   pr_arg(0), pr_arg(0), pr_arg(0), pr_arg(0));
}

/*
 * cap_reset_ambient erases all ambient capabilities - this reads the
 * ambient caps before performing the erase to workaround the corner
 * case where the set is empty already but the ambient cap API is
 * locked.
 */
int cap_reset_ambient(void)
{
    return _cap_reset_ambient(&multithread);
}

/*
 * Read the security mode of the current process.
 */
unsigned cap_get_secbits(void)
{
    return (unsigned) prctl(PR_GET_SECUREBITS, pr_arg(0), pr_arg(0));
}

static int _cap_set_secbits(struct syscaller_s *sc, unsigned bits)
{
    return _libcap_wprctl3(sc, PR_SET_SECUREBITS, bits, 0);
}

/*
 * Set the secbits of the current process.
 */
int cap_set_secbits(unsigned bits)
{
    return _cap_set_secbits(&multithread, bits);
}

/*
 * Attempt to raise the no new privs prctl value.
 */
static void _cap_set_no_new_privs(struct syscaller_s *sc)
{
    (void) _libcap_wprctl6(sc, PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0, 0);
}

/*
 * cap_prctl performs a prctl() 6 argument call on the current
 * thread. Use cap_prctlw() if you want to perform a POSIX semantics
 * prctl() system call.
 */
int cap_prctl(long int pr_cmd, long int arg1, long int arg2,
	      long int arg3, long int arg4, long int arg5)
{
    return prctl(pr_cmd, arg1, arg2, arg3, arg4, arg5);
}

/*
 * cap_prctlw performs a POSIX semantics prctl() call. That is a 6 arg
 * prctl() call that executes on all available threads when libpsx is
 * linked. The suffix 'w' refers to the fact one only ever needs to
 * invoke this is if the call will write some kernel state.
 */
int cap_prctlw(long int pr_cmd, long int arg1, long int arg2,
	       long int arg3, long int arg4, long int arg5)
{
    return _libcap_wprctl6(&multithread, pr_cmd, arg1, arg2, arg3, arg4, arg5);
}

/*
 * Some predefined constants
 */
#define CAP_SECURED_BITS_BASIC                                 \
    (SECBIT_NOROOT | SECBIT_NOROOT_LOCKED |                    \
     SECBIT_NO_SETUID_FIXUP | SECBIT_NO_SETUID_FIXUP_LOCKED |  \
     SECBIT_KEEP_CAPS_LOCKED)

#define CAP_SECURED_BITS_AMBIENT  (CAP_SECURED_BITS_BASIC |    \
     SECBIT_NO_CAP_AMBIENT_RAISE | SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED)

static cap_value_t raise_cap_setpcap[] = {CAP_SETPCAP};

static int _cap_set_mode(struct syscaller_s *sc, cap_mode_t flavor)
{
    int ret;
    unsigned secbits = CAP_SECURED_BITS_AMBIENT;
    cap_t working = cap_get_proc();

    if (working == NULL) {
	_cap_debug("getting current process' capabilities failed");
	return -1;
    }

    ret = cap_set_flag(working, CAP_EFFECTIVE, 1, raise_cap_setpcap, CAP_SET) |
	_cap_set_proc(sc, working);
    if (ret == 0) {
	cap_flag_t c;

	switch (flavor) {
	case CAP_MODE_NOPRIV:
	    /* fall through */
	case CAP_MODE_PURE1E_INIT:
	    (void) cap_clear_flag(working, CAP_INHERITABLE);
	    /* fall through */
	case CAP_MODE_PURE1E:
	    if (!CAP_AMBIENT_SUPPORTED()) {
		secbits = CAP_SECURED_BITS_BASIC;
	    } else {
		ret = _cap_reset_ambient(sc);
		if (ret) {
		    break; /* ambient dropping failed */
		}
	    }
	    ret = _cap_set_secbits(sc, secbits);
	    if (flavor != CAP_MODE_NOPRIV) {
		break;
	    }

	    /* just for "case CAP_MODE_NOPRIV:" */

	    for (c = 0; cap_get_bound(c) >= 0; c++) {
		(void) _cap_drop_bound(sc, c);
	    }
	    (void) cap_clear_flag(working, CAP_PERMITTED);

	    /* for good measure */
	    _cap_set_no_new_privs(sc);
	    break;
	case CAP_MODE_HYBRID:
	    ret = _cap_set_secbits(sc, 0);
	    break;
	default:
	    errno = EINVAL;
	    ret = -1;
	    break;
	}
    }

    (void) cap_clear_flag(working, CAP_EFFECTIVE);
    ret = _cap_set_proc(sc, working) | ret;
    (void) cap_free(working);
    return ret;
}

/*
 * cap_set_mode locks the overarching capability framework of the
 * present process and thus its children to a predefined flavor. Once
 * set, these modes cannot be undone by the affected process tree and
 * can only be done by "cap_setpcap" permitted processes. Note, a side
 * effect of this function, whether it succeeds or fails, is to clear
 * at least the CAP_EFFECTIVE flags for the current process.
 */
int cap_set_mode(cap_mode_t flavor)
{
    return _cap_set_mode(&multithread, flavor);
}

/*
 * cap_get_mode attempts to determine what the current capability mode
 * is. If it can find no match in the libcap pre-defined modes, it
 * returns CAP_MODE_UNCERTAIN.
 */
cap_mode_t cap_get_mode(void)
{
    unsigned secbits = cap_get_secbits();

    if (secbits == 0) {
	return CAP_MODE_HYBRID;
    }
    if ((secbits & CAP_SECURED_BITS_BASIC) != CAP_SECURED_BITS_BASIC) {
	return CAP_MODE_UNCERTAIN;
    }

    /* validate ambient is not set */
    int olderrno = errno;
    int ret = 0, cf;
    cap_value_t c;
    for (c = 0; !ret; c++) {
	ret = cap_get_ambient(c);
	if (ret == -1) {
	    errno = olderrno;
	    if (c && secbits != CAP_SECURED_BITS_AMBIENT) {
		return CAP_MODE_UNCERTAIN;
	    }
	    ret = 0;
	    break;
	}
	if (ret) {
	    return CAP_MODE_UNCERTAIN;
	}
    }

    /*
     * Explore how capabilities differ from empty.
     */
    cap_t working = cap_get_proc();
    cap_t empty = cap_init();
    if (working == NULL || empty == NULL) {
	_cap_debug("working=%p, empty=%p - need both non-NULL", working, empty);
	ret = -1;
    } else {
	cf = cap_compare(empty, working);
    }
    cap_free(empty);
    cap_free(working);
    if (ret != 0) {
	return CAP_MODE_UNCERTAIN;
    }

    if (CAP_DIFFERS(cf, CAP_INHERITABLE)) {
	return CAP_MODE_PURE1E;
    }
    if (CAP_DIFFERS(cf, CAP_PERMITTED) || CAP_DIFFERS(cf, CAP_EFFECTIVE)) {
	return CAP_MODE_PURE1E_INIT;
    }

    for (c = 0; ; c++) {
	int v = cap_get_bound(c);
	if (v == -1) {
	    break;
	}
	if (v) {
	    return CAP_MODE_PURE1E_INIT;
	}
    }

    return CAP_MODE_NOPRIV;
}

static int _cap_setuid(struct syscaller_s *sc, uid_t uid)
{
    const cap_value_t raise_cap_setuid[] = {CAP_SETUID};
    cap_t working = cap_get_proc();
    if (working == NULL) {
	return -1;
    }

    (void) cap_set_flag(working, CAP_EFFECTIVE,
			1, raise_cap_setuid, CAP_SET);
    /*
     * Note, we are cognizant of not using glibc's setuid in the case
     * that we've modified the way libcap is doing setting
     * syscalls. This is because prctl needs to be working in a POSIX
     * compliant way for the code below to work, so we are either
     * all-broken or not-broken and don't allow for "sort of working".
     */
    (void) _libcap_wprctl3(sc, PR_SET_KEEPCAPS, 1, 0);
    int ret = _cap_set_proc(sc, working);
    if (ret == 0) {
	if (_libcap_overrode_syscalls) {
	    ret = sc->three(SYS_setuid, (long int) uid, 0, 0);
	    if (ret < 0) {
		errno = -ret;
		ret = -1;
	    }
	} else {
	    ret = setuid(uid);
	}
    }
    int olderrno = errno;
    (void) _libcap_wprctl3(sc, PR_SET_KEEPCAPS, 0, 0);
    (void) cap_clear_flag(working, CAP_EFFECTIVE);
    (void) _cap_set_proc(sc, working);
    (void) cap_free(working);

    errno = olderrno;
    return ret;
}

/*
 * cap_setuid attempts to set the uid of the process without dropping
 * any permitted capabilities in the process. A side effect of a call
 * to this function is that the effective set will be cleared by the
 * time the function returns.
 */
int cap_setuid(uid_t uid)
{
    return _cap_setuid(&multithread, uid);
}

#if defined(__arm__) || defined(__i386__) || \
    defined(__i486__) || defined(__i586__) || defined(__i686__)
#define sys_setgroups_variant  SYS_setgroups32
#else
#define sys_setgroups_variant  SYS_setgroups
#endif

static int _cap_setgroups(struct syscaller_s *sc,
			  gid_t gid, size_t ngroups, const gid_t groups[])
{
    const cap_value_t raise_cap_setgid[] = {CAP_SETGID};
    cap_t working = cap_get_proc();
    if (working == NULL) {
	return -1;
    }

    (void) cap_set_flag(working, CAP_EFFECTIVE,
			1, raise_cap_setgid, CAP_SET);
    /*
     * Note, we are cognizant of not using glibc's setgid etc in the
     * case that we've modified the way libcap is doing setting
     * syscalls. This is because prctl needs to be working in a POSIX
     * compliant way for the other functions of this file so we are
     * all-broken or not-broken and don't allow for "sort of working".
     */
    int ret = _cap_set_proc(sc, working);
    if (_libcap_overrode_syscalls) {
	if (ret == 0) {
	    ret = sc->three(SYS_setgid, (long int) gid, 0, 0);
	}
	if (ret == 0) {
	    ret = sc->three(sys_setgroups_variant, (long int) ngroups,
			    (long int) groups, 0);
	}
	if (ret < 0) {
	    errno = -ret;
	    ret = -1;
	}
    } else {
	if (ret == 0) {
	    ret = setgid(gid);
	}
	if (ret == 0) {
	    ret = setgroups(ngroups, groups);
	}
    }
    int olderrno = errno;

    (void) cap_clear_flag(working, CAP_EFFECTIVE);
    (void) _cap_set_proc(sc, working);
    (void) cap_free(working);

    errno = olderrno;
    return ret;
}

/*
 * cap_setgroups combines setting the gid with changing the set of
 * supplemental groups for a user into one call that raises the needed
 * capabilities to do it for the duration of the call. A side effect
 * of a call to this function is that the effective set will be
 * cleared by the time the function returns.
 */
int cap_setgroups(gid_t gid, size_t ngroups, const gid_t groups[])
{
    return _cap_setgroups(&multithread, gid, ngroups, groups);
}

/*
 * cap_iab_get_proc returns a cap_iab_t value initialized by the
 * current process state related to these iab bits.
 */
cap_iab_t cap_iab_get_proc(void)
{
    cap_iab_t iab;
    cap_t current;

    iab = cap_iab_init();
    if (iab == NULL) {
	_cap_debug("no memory for IAB tuple");
	return NULL;
    }

    current = cap_get_proc();
    if (current == NULL) {
	_cap_debug("no memory for cap_t");
	cap_free(iab);
	return NULL;
    }

    cap_iab_fill(iab, CAP_IAB_INH, current, CAP_INHERITABLE);
    cap_free(current);

    cap_value_t c;
    for (c = cap_max_bits(); c; ) {
	--c;
	int o = c >> 5;
	__u32 mask = 1U << (c & 31);
	if (cap_get_bound(c) == 0) {
	    iab->nb[o] |= mask;
	}
	if (cap_get_ambient(c) == 1) {
	    iab->a[o] |= mask;
	}
    }
    return iab;
}

/*
 * _cap_iab_set_proc sets the iab collection using the requested syscaller.
 * The iab value is locked by the caller.
 */
static int _cap_iab_set_proc(struct syscaller_s *sc, cap_iab_t iab)
{
    int ret, i, raising = 0;
    cap_value_t c;
    cap_t working, temp = cap_get_proc();

    if (temp == NULL) {
	return -1;
    }

    for (i = 0; i < _LIBCAP_CAPABILITY_U32S; i++) {
	__u32 newI = iab->i[i];
	__u32 oldIP = temp->u[i].flat[CAP_INHERITABLE] |
	    temp->u[i].flat[CAP_PERMITTED];
	raising |= (newI & ~oldIP) | iab->a[i] | iab->nb[i];
	temp->u[i].flat[CAP_INHERITABLE] = newI;

    }

    working = cap_dup(temp);
    if (working == NULL) {
	ret = -1;
	goto defer;
    }
    if (raising) {
	ret = cap_set_flag(working, CAP_EFFECTIVE,
			   1, raise_cap_setpcap, CAP_SET);
	if (ret) {
	    goto defer;
	}
    }
    if ((ret = _cap_set_proc(sc, working))) {
	goto defer;
    }
    if ((ret = _cap_reset_ambient(sc))) {
	goto done;
    }

    for (c = cap_max_bits(); c-- != 0; ) {
	unsigned offset = c >> 5;
	__u32 mask = 1U << (c & 31);
	if (iab->a[offset] & mask) {
	    ret = _cap_set_ambient(sc, c, CAP_SET);
	    if (ret) {
		goto done;
	    }
	}
	if (iab->nb[offset] & mask) {
	    /* drop the bounding bit */
	    ret = _cap_drop_bound(sc, c);
	    if (ret) {
		goto done;
	    }
	}
    }

done:
    (void) cap_set_proc(temp);

defer:
    cap_free(working);
    cap_free(temp);

    return ret;
}

/*
 * cap_iab_set_proc sets the iab capability vectors of the current
 * process.
 */
int cap_iab_set_proc(cap_iab_t iab)
{
    int retval;
    if (!good_cap_iab_t(iab)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&iab->mutex);
    retval = _cap_iab_set_proc(&multithread, iab);
    _cap_mu_unlock(&iab->mutex);
    return retval;
}

/*
 * cap_launcher_callback primes the launcher with a callback that will
 * be invoked after the fork() but before any privilege has changed
 * and before the execve(). This can be used to augment the state of
 * the child process within the cap_launch() process. You can cancel
 * any callback associated with a launcher by calling this function
 * with a callback_fn value NULL.
 *
 * If the callback function returns anything other than 0, it is
 * considered to have failed and the launch will be aborted - further,
 * errno will be communicated to the parent.
 */
int cap_launcher_callback(cap_launch_t attr, int (callback_fn)(void *detail))
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);
    attr->custom_setup_fn = callback_fn;
    _cap_mu_unlock(&attr->mutex);
    return 0;
}

/*
 * cap_launcher_setuid primes the launcher to attempt a change of uid.
 */
int cap_launcher_setuid(cap_launch_t attr, uid_t uid)
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);
    attr->uid = uid;
    attr->change_uids = 1;
    _cap_mu_unlock(&attr->mutex);
    return 0;
}

/*
 * cap_launcher_setgroups primes the launcher to attempt a change of
 * gid and groups.
 */
int cap_launcher_setgroups(cap_launch_t attr, gid_t gid,
			   int ngroups, const gid_t *groups)
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);
    attr->gid = gid;
    attr->ngroups = ngroups;
    attr->groups = groups;
    attr->change_gids = 1;
    _cap_mu_unlock(&attr->mutex);
    return 0;
}

/*
 * cap_launcher_set_mode primes the launcher to attempt a change of
 * mode.
 */
int cap_launcher_set_mode(cap_launch_t attr, cap_mode_t flavor)
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);
    attr->mode = flavor;
    attr->change_mode = 1;
    _cap_mu_unlock(&attr->mutex);
    return 0;
}

/*
 * cap_launcher_set_iab primes the launcher to attempt to change the
 * IAB values of the launched child. The launcher locks iab while it
 * is owned by the launcher: this prevents the user from
 * asynchronously changing its value while it is associated with the
 * launcher.
 */
cap_iab_t cap_launcher_set_iab(cap_launch_t attr, cap_iab_t iab)
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return NULL;
    }
    _cap_mu_lock(&attr->mutex);
    cap_iab_t old = attr->iab;
    attr->iab = iab;
    if (old != NULL) {
	_cap_mu_unlock(&old->mutex);
    }
    if (iab != NULL) {
	_cap_mu_lock(&iab->mutex);
    }
    _cap_mu_unlock(&attr->mutex);
    return old;
}

/*
 * cap_launcher_set_chroot sets the intended chroot for the launched
 * child.
 */
int cap_launcher_set_chroot(cap_launch_t attr, const char *chroot)
{
    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);
    attr->chroot = _libcap_strdup(chroot);
    _cap_mu_unlock(&attr->mutex);
    return 0;
}

static int _cap_chroot(struct syscaller_s *sc, const char *root)
{
    const cap_value_t raise_cap_sys_chroot[] = {CAP_SYS_CHROOT};
    cap_t working = cap_get_proc();
    if (working == NULL) {
	return -1;
    }

    (void) cap_set_flag(working, CAP_EFFECTIVE,
			1, raise_cap_sys_chroot, CAP_SET);
    int ret = _cap_set_proc(sc, working);
    if (ret == 0) {
	if (_libcap_overrode_syscalls) {
	    ret = sc->three(SYS_chroot, (long int) root, 0, 0);
	    if (ret < 0) {
		errno = -ret;
		ret = -1;
	    }
	} else {
	    ret = chroot(root);
	}
	if (ret == 0) {
	    ret = chdir("/");
	}
    }
    int olderrno = errno;
    (void) cap_clear_flag(working, CAP_EFFECTIVE);
    (void) _cap_set_proc(sc, working);
    (void) cap_free(working);

    errno = olderrno;
    return ret;
}

/*
 * _cap_launch is invoked in the forked child, it cannot return but is
 * required to exit, if the execve fails. It will write the errno
 * value for any failure over the filedescriptor, fd, and exit with
 * status 1.
 */
__attribute__ ((noreturn))
static void _cap_launch(int fd, cap_launch_t attr, void *detail) {
    struct syscaller_s *sc = &singlethread;
    int my_errno;

    if (attr->custom_setup_fn && attr->custom_setup_fn(detail)) {
	goto defer;
    }
    if (attr->arg0 == NULL) {
	/* handle the successful cap_func_launcher completion */
	exit(0);
    }

    if (attr->change_uids && _cap_setuid(sc, attr->uid)) {
	goto defer;
    }
    if (attr->change_gids &&
	_cap_setgroups(sc, attr->gid, attr->ngroups, attr->groups)) {
	goto defer;
    }
    if (attr->change_mode && _cap_set_mode(sc, attr->mode)) {
	goto defer;
    }
    if (attr->iab && _cap_iab_set_proc(sc, attr->iab)) {
	goto defer;
    }
    if (attr->chroot != NULL && _cap_chroot(sc, attr->chroot)) {
	goto defer;
    }

    /*
     * Some type wrangling to work around what the kernel API really
     * means: not "const char **".
     */
    const void *temp_args = attr->argv;
    const void *temp_envp = attr->envp;

    execve(attr->arg0, temp_args, temp_envp);
    /* if the exec worked, execution will not reach here */

defer:
    /*
     * getting here means an error has occurred and errno is
     * communicated to the parent
     */
    my_errno = errno;
    for (;;) {
	int n = write(fd, &my_errno, sizeof(my_errno));
	if (n < 0 && errno == EAGAIN) {
	    continue;
	}
	break;
    }
    close(fd);
    exit(1);
}

/*
 * cap_launch performs a wrapped fork+(callback and/or exec) that
 * works in both an unthreaded environment and also where libcap is
 * linked with psx+pthreads. The function supports dropping privilege
 * in the forked thread, but retaining privilege in the parent
 * thread(s).
 *
 * When applying the IAB vector inside the fork, since the ambient set
 * is fragile with respect to changes in I or P, the function
 * carefully orders setting of these inheritable characteristics, to
 * make sure they stick.
 *
 * This function will return an error of -1 setting errno if the
 * launch failed.
 */
pid_t cap_launch(cap_launch_t attr, void *detail) {
    int my_errno;
    int ps[2];
    pid_t child;

    if (!good_cap_launch_t(attr)) {
	errno = EINVAL;
	return -1;
    }
    _cap_mu_lock(&attr->mutex);

    /* The launch must have a purpose */
    if (attr->custom_setup_fn == NULL &&
	(attr->arg0 == NULL || attr->argv == NULL)) {
	errno = EINVAL;
	_cap_mu_unlock_return(&attr->mutex, -1);
    }

    if (pipe2(ps, O_CLOEXEC) != 0) {
	_cap_mu_unlock_return(&attr->mutex, -1);
    }

    child = fork();
    my_errno = errno;

    if (!child) {
	close(ps[0]);
	prctl(PR_SET_NAME, "cap-launcher", 0, 0, 0);
	_cap_launch(ps[1], attr, detail);
	/* no return from above function */
    }

    /* child has its own copy, and parent no longer needs it locked. */
    _cap_mu_unlock(&attr->mutex);
    close(ps[1]);
    if (child < 0) {
	goto defer;
    }

    /*
     * Extend this function's return codes to include setup failures
     * in the child.
     */
    for (;;) {
	int ignored;
	int n = read(ps[0], &my_errno, sizeof(my_errno));
	if (n == 0) {
	    goto defer;
	}
	if (n < 0 && errno == EAGAIN) {
	    continue;
	}
	waitpid(child, &ignored, 0);
	child = -1;
	my_errno = ECHILD;
	break;
    }

defer:
    close(ps[0]);
    errno = my_errno;
    return child;
}
