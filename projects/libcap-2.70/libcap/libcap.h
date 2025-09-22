/*
 * Copyright (c) 1997,2020 Andrew G Morgan <morgan@kernel.org>
 *
 * This file contains internal definitions for the various functions in
 * this small capability library.
 */

#ifndef LIBCAP_H
#define LIBCAP_H

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/capability.h>

#ifndef __u8
#define __u8    uint8_t
#endif /* __8 */

#ifndef __u32
#define __u32   uint32_t
#endif /* __u32 */

/* include the names for the caps and a definition of __CAP_BITS */
#include "cap_names.h"

#ifndef _LINUX_CAPABILITY_U32S_1
# define _LINUX_CAPABILITY_U32S_1          1
#endif /* ndef _LINUX_CAPABILITY_U32S_1 */

/*
 * Do we match the local kernel?
 */

#if !defined(_LINUX_CAPABILITY_VERSION)

# error Kernel <linux/capability.h> does not support library
# error file "libcap.h" --> fix and recompile libcap

#elif !defined(_LINUX_CAPABILITY_VERSION_2)

# warning Kernel <linux/capability.h> does not support 64-bit capabilities
# warning and libcap is being built with no support for 64-bit capabilities

# ifndef _LINUX_CAPABILITY_VERSION_1
#  define _LINUX_CAPABILITY_VERSION_1 0x19980330
# endif

# _LIBCAP_CAPABILITY_VERSION  _LINUX_CAPABILITY_VERSION_1
# _LIBCAP_CAPABILITY_U32S     _LINUX_CAPABILITY_U32S_1

#elif defined(_LINUX_CAPABILITY_VERSION_3)

# if (_LINUX_CAPABILITY_VERSION_3 != 0x20080522)
#  error Kernel <linux/capability.h> v3 does not match library
#  error file "libcap.h" --> fix and recompile libcap
# else
#  define _LIBCAP_CAPABILITY_VERSION  _LINUX_CAPABILITY_VERSION_3
#  define _LIBCAP_CAPABILITY_U32S     _LINUX_CAPABILITY_U32S_3
# endif

#elif (_LINUX_CAPABILITY_VERSION_2 != 0x20071026)

# error Kernel <linux/capability.h> does not match library
# error file "libcap.h" --> fix and recompile libcap

#else

# define _LIBCAP_CAPABILITY_VERSION  _LINUX_CAPABILITY_VERSION_2
# define _LIBCAP_CAPABILITY_U32S     _LINUX_CAPABILITY_U32S_2

#endif

#undef _LINUX_CAPABILITY_VERSION
#undef _LINUX_CAPABILITY_U32S

/*
 * This is a pointer to a struct containing three consecutive
 * capability sets in the order of the cap_flag_t type: the are
 * effective,inheritable and permitted.  This is the type that the
 * user-space routines think of as 'internal' capabilities - this is
 * the type that is passed to the kernel with the system calls related
 * to processes.
 */

#if defined(VFS_CAP_REVISION_MASK) && !defined(VFS_CAP_U32)
# define VFS_CAP_U32_1                   1
# define XATTR_CAPS_SZ_1                 (sizeof(__le32)*(1 + 2*VFS_CAP_U32_1))
# define VFS_CAP_U32                     VFS_CAP_U32_1
struct _cap_vfs_cap_data {
    __le32 magic_etc;
    struct {
	__le32 permitted;
	__le32 inheritable;
    } data[VFS_CAP_U32_1];
};
# define vfs_cap_data                    _cap_vfs_cap_data
#endif

#ifndef CAP_TO_INDEX
# define CAP_TO_INDEX(x)     ((x) >> 5)  /* 1 << 5 == bits in __u32 */
#endif /* ndef CAP_TO_INDEX */

#ifndef CAP_TO_MASK
# define CAP_TO_MASK(x)      (1 << ((x) & 31))
#endif /* ndef CAP_TO_MASK */

#define NUMBER_OF_CAP_SETS      3   /* effective, inheritable, permitted */
#define __CAP_BLKS   (_LIBCAP_CAPABILITY_U32S)
#define CAP_SET_SIZE (__CAP_BLKS * sizeof(__u32))

#define CAP_T_MAGIC 0xCA90D0
struct _cap_struct {
    __u8 mutex;
    struct __user_cap_header_struct head;
    union {
	struct __user_cap_data_struct set;
	__u32 flat[NUMBER_OF_CAP_SETS];
    } u[_LIBCAP_CAPABILITY_U32S];
    uid_t rootid;
};

/*
 * Elementary exclusive locking primatives for situations where
 * linking with pthreads needs it, but such linking is not common.
 *
 *  _cap_mu_blocked(x) attempts to lock x but if already locked, returns true
 *  _cap_mu_lock(x)    attempts to lock and waits until the lock is granted
 *  _cap_mu_unlock(x)  unconditionally unlocks the lock
 *  _cap_mu_unlock_return(x, y) unlock lock x and return value y
 */
#define _cap_mu_blocked(x)          \
    __atomic_test_and_set((void *)(x), __ATOMIC_SEQ_CST)
#define _cap_mu_lock(x)             \
    while (_cap_mu_blocked(x)) sched_yield()
#define _cap_mu_unlock(x)           \
    __atomic_clear((void *) (x), __ATOMIC_SEQ_CST)
#define _cap_mu_unlock_return(x, y) \
    do { _cap_mu_unlock(x); return (y); } while (0)

/* the maximum bits supportable */
#define __CAP_MAXBITS (__CAP_BLKS * 32)

/* string magic for cap_free */
#define CAP_S_MAGIC 0xCA95D0

/* iab set magic for cap_free */
#define CAP_IAB_MAGIC 0xCA91AB

/* launcher magic for cap_free */
#define CAP_LAUNCH_MAGIC 0xCA91AC

#define magic_of(x)           ((x) ? *(-2 + (const __u32 *) x) : 0)
#define good_cap_t(x)         (CAP_T_MAGIC   == magic_of(x))
#define good_cap_iab_t(x)     (CAP_IAB_MAGIC == magic_of(x))
#define good_cap_launch_t(x)  (CAP_LAUNCH_MAGIC == magic_of(x))

/*
 * kernel API cap set abstraction
 */

#define raise_cap(x, set)    u[(x) >> 5].flat[set]       |=  (1u << ((x)&31))
#define lower_cap(x, set)    u[(x) >> 5].flat[set]       &= ~(1u << ((x)&31))
#define isset_cap(y, x, set) ((y)->u[(x) >> 5].flat[set] &   (1u << ((x)&31)))

/*
 * These match CAP_DIFFERS() expectations
 */
#define LIBCAP_EFF   (1 << CAP_EFFECTIVE)
#define LIBCAP_INH   (1 << CAP_INHERITABLE)
#define LIBCAP_PER   (1 << CAP_PERMITTED)

/*
 * library debugging
 */
#ifdef DEBUG

#include <stdio.h>
# define _cap_debug(f, x...)  do { \
    fprintf(stderr, "%s(%s:%d): ", __FUNCTION__, __FILE__, __LINE__); \
    fprintf(stderr, f, ## x); \
    fprintf(stderr, "\n"); \
} while (0)

# define _cap_debugcap(s, c, set) do { \
    unsigned _cap_index; \
    fprintf(stderr, "%s(%s:%d): %s", __FUNCTION__, __FILE__, __LINE__, s); \
    for (_cap_index=_LIBCAP_CAPABILITY_U32S; _cap_index-- > 0; ) { \
       fprintf(stderr, "%08x", (c).u[_cap_index].flat[set]); \
    } \
    fprintf(stderr, "\n"); \
} while (0)

#else /* !DEBUG */

# define _cap_debug(f, x...)
# define _cap_debugcap(s, c, set)

#endif /* DEBUG */

extern char *_libcap_strdup(const char *text);
extern void _libcap_initialize(void);

#define EXECABLE_INITIALIZE _libcap_initialize()

/*
 * These are semi-public prototypes, they will only be defined in
 * <sys/capability.h> if _POSIX_SOURCE is not #define'd, so we
 * place them here too.
 */

extern int capget(cap_user_header_t header, cap_user_data_t data);
extern int capgetp(pid_t pid, cap_t cap_d);
extern int capsetp(pid_t pid, cap_t cap_d);

/* prctl based API for altering character of current process */
#define PR_GET_KEEPCAPS    7
#define PR_SET_KEEPCAPS    8
#define PR_CAPBSET_READ   23
#define PR_CAPBSET_DROP   24
#define PR_GET_SECUREBITS 27
#define PR_SET_SECUREBITS 28

/*
 * The library compares sizeof() with integer return values. To avoid
 * signed/unsigned comparisons, leading to unfortunate
 * misinterpretations of -1, we provide a convenient cast-to-signed-integer
 * version of sizeof().
 */
#define ssizeof(x) ((ssize_t) sizeof(x))

/*
 * Put this here as a macro so we can unit test it.
 */
#define _binary_search(val, fn, low, high, fallback) do {	\
	cap_value_t min = low, max = high;			\
	while (min <= max) {					\
	    cap_value_t mid = (min+max) / 2;			\
	    if (fn(mid) < 0) {					\
		max = mid - 1;					\
	    } else {						\
		min = mid + 1;					\
	    }							\
	}							\
	val = min ? (min <= high ? min : fallback) : fallback;	\
    } while(0)

/*
 * cap_iab_s holds a collection of inheritable capability bits. The i
 * bits are inheritable (these are the same as those in cap_t), the a
 * bits are ambient bits (which cannot be a superset of i&p), and nb
 * are the bits that will be dropped from the bounding set when
 * applied.
 */
struct cap_iab_s {
    __u8 mutex;
    __u32 i[_LIBCAP_CAPABILITY_U32S];
    __u32 a[_LIBCAP_CAPABILITY_U32S];
    __u32 nb[_LIBCAP_CAPABILITY_U32S];
};

#define LIBCAP_IAB_I_FLAG (1U << CAP_IAB_INH)
#define LIBCAP_IAB_A_FLAG (1U << CAP_IAB_AMB)
#define LIBCAP_IAB_IA_FLAG (LIBCAP_IAB_I_FLAG | LIBCAP_IAB_A_FLAG)
#define LIBCAP_IAB_NB_FLAG (1U << CAP_IAB_BOUND)

/*
 * The following support launching another process without destroying
 * the state of the current process. This is especially useful for
 * multithreaded applications.
 */
struct cap_launch_s {
    __u8 mutex;
    /*
     * Once forked but before active privilege is changed, this
     * function (if non-NULL) is called.
     */
    int (*custom_setup_fn)(void *detail);

    /*
     * user and groups to be used by the forked child.
     */
    int change_uids;
    uid_t uid;

    int change_gids;
    gid_t gid;
    int ngroups;
    const gid_t *groups;

    /*
     * mode holds the preferred capability mode. Any non-uncertain
     * setting here will require an empty ambient set.
     */
    int change_mode;
    cap_mode_t mode;

    /*
     * i,a,[n]b caps. These bitmaps hold all of the capability sets that
     * cap_launch will affect. nb holds values to be lowered in the bounding
     * set.
     */
    struct cap_iab_s *iab;

    /* chroot holds a preferred chroot for the launched child. */
    char *chroot;

    /*
     * execve style arguments
     */
    const char *arg0;
    const char *const *argv;
    const char *const *envp;
};

#endif /* LIBCAP_H */
