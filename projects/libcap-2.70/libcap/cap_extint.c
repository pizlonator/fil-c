/*
 * Copyright (c) 1997-8,2021 Andrew G. Morgan <morgan@kernel.org>
 *
 * This file deals with exchanging internal and external
 * representations of capability sets.
 */

#include "libcap.h"

/*
 * External representation for capabilities. (exported as a fixed
 * length)
 */
#define CAP_EXT_MAGIC "\220\302\001\121"
#define CAP_EXT_MAGIC_SIZE 4
const static __u8 external_magic[CAP_EXT_MAGIC_SIZE+1] = CAP_EXT_MAGIC;

/*
 * This is the largest size libcap can currently export.
 * cap_size() may return something smaller depending on the
 * content of its argument cap_t.
 */
struct cap_ext_struct {
    __u8 magic[CAP_EXT_MAGIC_SIZE];
    __u8 length_of_capset;
    /*
     * note, we arrange these so the caps are stacked with byte-size
     * resolution
     */
    __u8 bytes[CAP_SET_SIZE][NUMBER_OF_CAP_SETS];
};

/*
 * minimum exported flag size: libcap2 has always exported with flags
 * this size.
 */
static size_t _libcap_min_ext_flag_size = CAP_SET_SIZE < 8 ? CAP_SET_SIZE : 8;

static ssize_t _cap_size_locked(cap_t cap_d)
{
    size_t j, used;
    for (j=used=0; j<CAP_SET_SIZE; j+=sizeof(__u32)) {
	int i;
	__u32 val = 0;
	for (i=0; i<NUMBER_OF_CAP_SETS; ++i) {
	    val |= cap_d->u[j/sizeof(__u32)].flat[i];
	}
	if (val == 0) {
	    continue;
	}
	if (val > 0x0000ffff) {
	    if (val > 0x00ffffff) {
		used = j+4;
	    } else {
		used = j+3;
	    }
	} else if (val > 0x000000ff) {
	    used = j+2;
	} else {
	    used = j+1;
	}
    }
    if (used < _libcap_min_ext_flag_size) {
	used = _libcap_min_ext_flag_size;
    }
    return (ssize_t)(CAP_EXT_MAGIC_SIZE + 1+ NUMBER_OF_CAP_SETS * used);
}

/*
 * return size of external capability set
 */
ssize_t cap_size(cap_t cap_d)
{
    size_t used;
    if (!good_cap_t(cap_d)) {
	return ssizeof(struct cap_ext_struct);
    }
    _cap_mu_lock(&cap_d->mutex);
    used = _cap_size_locked(cap_d);
    _cap_mu_unlock(&cap_d->mutex);
    return used;
}

/*
 * Copy the internal (cap_d) capability set into an external
 * representation.  The external representation is portable to other
 * Linux architectures.
 */

ssize_t cap_copy_ext(void *cap_ext, cap_t cap_d, ssize_t length)
{
    struct cap_ext_struct *result = (struct cap_ext_struct *) cap_ext;
    ssize_t csz, len_set;
    int i;

    /* valid arguments? */
    if (!good_cap_t(cap_d) || cap_ext == NULL) {
	errno = EINVAL;
	return -1;
    }

    _cap_mu_lock(&cap_d->mutex);
    csz = _cap_size_locked(cap_d);
    if (csz > length) {
	errno = EINVAL;
	_cap_mu_unlock_return(&cap_d->mutex, -1);
    }
    len_set = (csz - (CAP_EXT_MAGIC_SIZE+1))/NUMBER_OF_CAP_SETS;

    /* fill external capability set */
    memcpy(&result->magic, external_magic, CAP_EXT_MAGIC_SIZE);
    result->length_of_capset = len_set;

    for (i=0; i<NUMBER_OF_CAP_SETS; ++i) {
	size_t j;
	for (j=0; j<len_set; ) {
	    __u32 val;

	    val = cap_d->u[j/sizeof(__u32)].flat[i];

	    result->bytes[j++][i] =      val        & 0xFF;
	    if (j < len_set) {
		result->bytes[j++][i] = (val >>= 8) & 0xFF;
	    }
	    if (j < len_set) {
		result->bytes[j++][i] = (val >>= 8) & 0xFF;
	    }
	    if (j < len_set) {
		result->bytes[j++][i] = (val >> 8)  & 0xFF;
	    }
	}
    }

    /* All done: return length of external representation */
    _cap_mu_unlock_return(&cap_d->mutex, csz);
}

/*
 * Import an external representation to produce an internal rep.
 * the internal rep should be liberated with cap_free().
 *
 * Note, this function assumes that cap_ext has a valid length. That
 * is, feeding garbage to this function will likely crash the program.
 */
cap_t cap_copy_int(const void *cap_ext)
{
    const struct cap_ext_struct *export =
	(const struct cap_ext_struct *) cap_ext;
    cap_t cap_d;
    int set, blen;

    /* Does the external representation make sense? */
    if ((export == NULL)
	|| memcmp(export->magic, external_magic, CAP_EXT_MAGIC_SIZE)) {
	errno = EINVAL;
	return NULL;
    }

    /* Obtain a new internal capability set */
    if (!(cap_d = cap_init()))
       return NULL;

    blen = export->length_of_capset;
    for (set=0; set<NUMBER_OF_CAP_SETS; ++set) {
	unsigned blk;
	int bno = 0;
	for (blk=0; blk<(CAP_SET_SIZE/sizeof(__u32)); ++blk) {
	    __u32 val = 0;

	    if (bno != blen)
		val  = export->bytes[bno++][set];
	    if (bno != blen)
		val |= export->bytes[bno++][set] << 8;
	    if (bno != blen)
		val |= export->bytes[bno++][set] << 16;
	    if (bno != blen)
		val |= export->bytes[bno++][set] << 24;

	    cap_d->u[blk].flat[set] = val;
	}
    }

    /* all done */
    return cap_d;
}

/*
 * This function is the same as cap_copy_int() although it requires an
 * extra argument that is the length of the cap_ext data. Before
 * running cap_copy_int() the function validates that length is
 * consistent with the stated length. It returns NULL on error.
 */
cap_t cap_copy_int_check(const void *cap_ext, ssize_t length)
{
    const struct cap_ext_struct *export =
	(const struct cap_ext_struct *) cap_ext;

    if (length < 1+CAP_EXT_MAGIC_SIZE) {
	errno = EINVAL;
	return NULL;
    }
    if (length < 1+CAP_EXT_MAGIC_SIZE + export->length_of_capset * NUMBER_OF_CAP_SETS) {
	errno = EINVAL;
	return NULL;
    }
    return cap_copy_int(cap_ext);
}
