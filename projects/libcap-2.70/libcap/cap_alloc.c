/*
 * Copyright (c) 1997-8,2019,2021 Andrew G Morgan <morgan@kernel.org>
 *
 * This file deals with allocation and deallocation of internal
 * capability sets as specified by POSIX.1e (formerlly, POSIX 6).
 */

#include "libcap.h"

/*
 * Make start up atomic.
 */
static __u8 __libcap_mutex;

/*
 * These get set via the pre-main() executed constructor function below it.
 */
static cap_value_t _cap_max_bits;

__attribute__((visibility ("hidden")))
__attribute__((constructor (300))) void _libcap_initialize(void)
{
    int errno_saved = errno;
    _cap_mu_lock(&__libcap_mutex);
    if (!_cap_max_bits) {
	cap_set_syscall(NULL, NULL);
	_binary_search(_cap_max_bits, cap_get_bound, 0, __CAP_MAXBITS,
		       __CAP_BITS);
    }
    _cap_mu_unlock(&__libcap_mutex);
    errno = errno_saved;
}

cap_value_t cap_max_bits(void)
{
    return _cap_max_bits;
}

/*
 * capability allocation is all done in terms of this structure.
 */
struct _cap_alloc_s {
    __u32 magic;
    __u32 size;
    union {
	struct _cap_struct set;
	struct cap_iab_s iab;
	struct cap_launch_s launcher;
    } u;
};

/*
 * Obtain a blank set of capabilities
 */
cap_t cap_init(void)
{
    struct _cap_alloc_s *raw_data;
    cap_t result;

    raw_data = calloc(1, sizeof(struct _cap_alloc_s));
    if (raw_data == NULL) {
	_cap_debug("out of memory");
	errno = ENOMEM;
	return NULL;
    }
    raw_data->magic = CAP_T_MAGIC;
    raw_data->size = sizeof(struct _cap_alloc_s);

    result = &raw_data->u.set;
    result->head.version = _LIBCAP_CAPABILITY_VERSION;
    capget(&result->head, NULL);      /* load the kernel-capability version */

    switch (result->head.version) {
#ifdef _LINUX_CAPABILITY_VERSION_1
    case _LINUX_CAPABILITY_VERSION_1:
	break;
#endif
#ifdef _LINUX_CAPABILITY_VERSION_2
    case _LINUX_CAPABILITY_VERSION_2:
	break;
#endif
#ifdef _LINUX_CAPABILITY_VERSION_3
    case _LINUX_CAPABILITY_VERSION_3:
	break;
#endif
    default:                          /* No idea what to do */
	cap_free(result);
	result = NULL;
	break;
    }

    return result;
}

/*
 * This is an internal library function to duplicate a string and
 * tag the result as something cap_free can handle.
 */
__attribute__((visibility ("hidden"))) char *_libcap_strdup(const char *old)
{
    struct _cap_alloc_s *header;
    char *raw_data;
    size_t len;

    if (old == NULL) {
	errno = EINVAL;
	return NULL;
    }

    len = strlen(old);
    if ((len & 0x3fffffff) != len) {
	_cap_debug("len is too long for libcap to manage");
	errno = EINVAL;
	return NULL;
    }
    len += 1 + 2*sizeof(__u32);
    if (len < sizeof(struct _cap_alloc_s)) {
	len = sizeof(struct _cap_alloc_s);
    }

    raw_data = calloc(1, len);
    if (raw_data == NULL) {
	errno = ENOMEM;
	return NULL;
    }
    header = (void *) raw_data;
    header->magic = CAP_S_MAGIC;
    header->size = (__u32) len;

    raw_data += 2*sizeof(__u32);
    strcpy(raw_data, old);
    return raw_data;
}

/*
 * This function duplicates an internal capability set with
 * calloc()'d memory. It is the responsibility of the user to call
 * cap_free() to liberate it.
 */
cap_t cap_dup(cap_t cap_d)
{
    cap_t result;

    if (!good_cap_t(cap_d)) {
	_cap_debug("bad argument");
	errno = EINVAL;
	return NULL;
    }

    result = cap_init();
    if (result == NULL) {
	_cap_debug("out of memory");
	return NULL;
    }

    _cap_mu_lock(&cap_d->mutex);
    memcpy(result, cap_d, sizeof(*cap_d));
    _cap_mu_unlock(&cap_d->mutex);
    _cap_mu_unlock(&result->mutex);

    return result;
}

cap_iab_t cap_iab_init(void)
{
    struct _cap_alloc_s *base = calloc(1, sizeof(struct _cap_alloc_s));
    if (base == NULL) {
	_cap_debug("out of memory");
	return NULL;
    }
    base->magic = CAP_IAB_MAGIC;
    base->size = sizeof(struct _cap_alloc_s);
    return &base->u.iab;
}

/*
 * This function duplicates an internal iab tuple with calloc()'d
 * memory. It is the responsibility of the user to call cap_free() to
 * liberate it.
 */
cap_iab_t cap_iab_dup(cap_iab_t iab)
{
    cap_iab_t result;

    if (!good_cap_iab_t(iab)) {
	_cap_debug("bad argument");
	errno = EINVAL;
	return NULL;
    }

    result = cap_iab_init();
    if (result == NULL) {
	_cap_debug("out of memory");
	return NULL;
    }

    _cap_mu_lock(&iab->mutex);
    memcpy(result, iab, sizeof(*iab));
    _cap_mu_unlock(&iab->mutex);
    _cap_mu_unlock(&result->mutex);

    return result;
}

/*
 * cap_new_launcher allocates some memory for a launcher and
 * initializes it.  To actually launch a program with this launcher,
 * use cap_launch(). By default, the launcher is a no-op from a
 * security perspective and will act just as fork()/execve()
 * would. Use cap_launcher_setuid() etc to override this.
 */
cap_launch_t cap_new_launcher(const char *arg0, const char * const *argv,
			      const char * const *envp)
{
    struct _cap_alloc_s *data = calloc(1, sizeof(struct _cap_alloc_s));
    if (data == NULL) {
	_cap_debug("out of memory");
	return NULL;
    }
    data->magic = CAP_LAUNCH_MAGIC;
    data->size = sizeof(struct _cap_alloc_s);

    struct cap_launch_s *attr = &data->u.launcher;
    attr->arg0 = arg0;
    attr->argv = argv;
    attr->envp = envp;
    return attr;
}

/*
 * cap_func_launcher allocates some memory for a launcher and
 * initializes it. The purpose of this launcher, unlike one created
 * with cap_new_launcher(), is to execute some function code from a
 * forked copy of the program. The forked process will exit when the
 * callback function, func, returns.
 */
cap_launch_t cap_func_launcher(int (callback_fn)(void *detail))
{
    struct _cap_alloc_s *data = calloc(1, sizeof(struct _cap_alloc_s));
    if (data == NULL) {
	_cap_debug("out of memory");
	return NULL;
    }
    data->magic = CAP_LAUNCH_MAGIC;
    data->size = sizeof(struct _cap_alloc_s);

    struct cap_launch_s *attr = &data->u.launcher;
    attr->custom_setup_fn = callback_fn;
    return attr;
}

/*
 * Scrub and then liberate the recognized allocated object.
 */
int cap_free(void *data_p)
{
    if (!data_p) {
	return 0;
    }

    /* confirm alignment */
    if ((sizeof(uintptr_t)-1) & (uintptr_t) data_p) {
	_cap_debug("whatever we're cap_free()ing it isn't aligned right: %p",
		   data_p);
	errno = EINVAL;
	return -1;
    }

    void *base = (void *) (-2 + (__u32 *) data_p);
    struct _cap_alloc_s *data = base;
    switch (data->magic) {
    case CAP_T_MAGIC:
	_cap_mu_lock(&data->u.set.mutex);
	break;
    case CAP_S_MAGIC:
    case CAP_IAB_MAGIC:
	break;
    case CAP_LAUNCH_MAGIC:
	if (data->u.launcher.iab != NULL) {
	    _cap_mu_unlock(&data->u.launcher.iab->mutex);
	    if (cap_free(data->u.launcher.iab) != 0) {
		return -1;
	    }
	}
	data->u.launcher.iab = NULL;
	if (cap_free(data->u.launcher.chroot) != 0) {
	    return -1;
	}
	data->u.launcher.chroot = NULL;
	break;
    default:
	_cap_debug("don't recognize what we're supposed to liberate");
	errno = EINVAL;
	return -1;
    }

    /*
     * operate here with respect to base, to avoid tangling with the
     * automated buffer overflow detection.
     */
    memset(base, 0, data->size);
    free(base);
    data_p = NULL;
    data = NULL;
    base = NULL;
    return 0;
}
