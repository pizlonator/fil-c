/*
 * Copyright (c) 1997-8,2008,20-21 Andrew G. Morgan <morgan@kernel.org>
 *
 * This file deals with flipping of capabilities on internal
 * capability sets as specified by POSIX.1e (formerlly, POSIX 6).
 *
 * It also contains similar code for bit flipping cap_iab_t values.
 */

#include "libcap.h"

/*
 * Return the state of a specified capability flag.  The state is
 * returned as the contents of *raised.  The capability is from one of
 * the sets stored in cap_d as specified by set and value
 */
int cap_get_flag(cap_t cap_d, cap_value_t value, cap_flag_t set,
		 cap_flag_value_t *raised)
{
    /*
     * Do we have a set and a place to store its value?
     * Is it a known capability?
     */

    if (raised && good_cap_t(cap_d) && value >= 0 && value < __CAP_MAXBITS
	&& set >= 0 && set < NUMBER_OF_CAP_SETS) {
	_cap_mu_lock(&cap_d->mutex);
	*raised = isset_cap(cap_d,value,set) ? CAP_SET:CAP_CLEAR;
	_cap_mu_unlock(&cap_d->mutex);
	return 0;
    } else {
	_cap_debug("invalid arguments");
	errno = EINVAL;
	return -1;
    }
}

/*
 * raise/lower a selection of capabilities
 */

int cap_set_flag(cap_t cap_d, cap_flag_t set,
		 int no_values, const cap_value_t *array_values,
		 cap_flag_value_t raise)
{
    /*
     * Do we have a set and a place to store its value?
     * Is it a known capability?
     */

    if (good_cap_t(cap_d) && no_values > 0 && no_values < __CAP_MAXBITS
	&& (set >= 0) && (set < NUMBER_OF_CAP_SETS)
	&& (raise == CAP_SET || raise == CAP_CLEAR) ) {
	int i;
	_cap_mu_lock(&cap_d->mutex);
	for (i=0; i<no_values; ++i) {
	    if (array_values[i] < 0 || array_values[i] >= __CAP_MAXBITS) {
		_cap_debug("weird capability (%d) - skipped", array_values[i]);
	    } else {
		int value = array_values[i];

		if (raise == CAP_SET) {
		    cap_d->raise_cap(value,set);
		} else {
		    cap_d->lower_cap(value,set);
		}
	    }
	}
	_cap_mu_unlock(&cap_d->mutex);
	return 0;
    } else {
	_cap_debug("invalid arguments");
	errno = EINVAL;
	return -1;
    }
}

/*
 *  Reset the capability to be empty (nothing raised)
 */

int cap_clear(cap_t cap_d)
{
    if (good_cap_t(cap_d)) {
	_cap_mu_lock(&cap_d->mutex);
	memset(&(cap_d->u), 0, sizeof(cap_d->u));
	_cap_mu_unlock(&cap_d->mutex);
	return 0;
    } else {
	_cap_debug("invalid pointer");
	errno = EINVAL;
	return -1;
    }
}

/*
 *  Reset the all of the capability bits for one of the flag sets
 */

int cap_clear_flag(cap_t cap_d, cap_flag_t flag)
{
    switch (flag) {
    case CAP_EFFECTIVE:
    case CAP_PERMITTED:
    case CAP_INHERITABLE:
	if (good_cap_t(cap_d)) {
	    unsigned i;

	    _cap_mu_lock(&cap_d->mutex);
	    for (i=0; i<_LIBCAP_CAPABILITY_U32S; i++) {
		cap_d->u[i].flat[flag] = 0;
	    }
	    _cap_mu_unlock(&cap_d->mutex);
	    return 0;
	}
	/*
	 * fall through
	 */

    default:
	_cap_debug("invalid pointer");
	errno = EINVAL;
	return -1;
    }
}

/*
 * Compare two capability sets
 */
int cap_compare(cap_t a, cap_t b)
{
    unsigned i;
    int result;

    if (!(good_cap_t(a) && good_cap_t(b))) {
	_cap_debug("invalid arguments");
	errno = EINVAL;
	return -1;
    }

    /*
     * To avoid a deadlock corner case, we operate on an unlocked
     * private copy of b
     */
    b = cap_dup(b);
    if (b == NULL) {
	return -1;
    }
    _cap_mu_lock(&a->mutex);
    for (i=0, result=0; i<_LIBCAP_CAPABILITY_U32S; i++) {
	result |=
	    ((a->u[i].flat[CAP_EFFECTIVE] != b->u[i].flat[CAP_EFFECTIVE])
	     ? LIBCAP_EFF : 0)
	    | ((a->u[i].flat[CAP_INHERITABLE] != b->u[i].flat[CAP_INHERITABLE])
	       ? LIBCAP_INH : 0)
	    | ((a->u[i].flat[CAP_PERMITTED] != b->u[i].flat[CAP_PERMITTED])
	       ? LIBCAP_PER : 0);
    }
    _cap_mu_unlock(&a->mutex);
    cap_free(b);
    return result;
}

/*
 * cap_fill_flag copies a bit-vector of capability state in one cap_t from one
 * flag to another flag of another cap_t.
 */
int cap_fill_flag(cap_t cap_d, cap_flag_t to, cap_t ref, cap_flag_t from)
{
    int i;
    cap_t orig;

    if (!good_cap_t(cap_d) || !good_cap_t(ref)) {
	errno = EINVAL;
	return -1;
    }

    if (to < CAP_EFFECTIVE || to > CAP_INHERITABLE ||
	from < CAP_EFFECTIVE || from > CAP_INHERITABLE) {
	errno = EINVAL;
	return -1;
    }

    orig = cap_dup(ref);
    if (orig == NULL) {
	return -1;
    }

    _cap_mu_lock(&cap_d->mutex);
    for (i = 0; i < _LIBCAP_CAPABILITY_U32S; i++) {
	cap_d->u[i].flat[to] = orig->u[i].flat[from];
    }
    _cap_mu_unlock(&cap_d->mutex);

    cap_free(orig);
    return 0;
}

/*
 * cap_fill copies a bit-vector of capability state in a cap_t from
 * one flag to another.
 */
int cap_fill(cap_t cap_d, cap_flag_t to, cap_flag_t from)
{
    return cap_fill_flag(cap_d, to, cap_d, from);
}

/*
 * cap_iab_get_vector reads the single bit value from an IAB vector set.
 */
cap_flag_value_t cap_iab_get_vector(cap_iab_t iab, cap_iab_vector_t vec,
				    cap_value_t bit)
{
    if (!good_cap_iab_t(iab) || bit >= cap_max_bits()) {
	return 0;
    }

    unsigned o = (bit >> 5);
    __u32 mask = 1u << (bit & 31);
    cap_flag_value_t ret;

    _cap_mu_lock(&iab->mutex);
    switch (vec) {
    case CAP_IAB_INH:
	ret = !!(iab->i[o] & mask);
	break;
    case CAP_IAB_AMB:
	ret = !!(iab->a[o] & mask);
	break;
    case CAP_IAB_BOUND:
	ret = !!(iab->nb[o] & mask);
	break;
    default:
	ret = 0;
    }
    _cap_mu_unlock(&iab->mutex);

    return ret;
}

/*
 * cap_iab_set_vector sets the bits in an IAB to the value
 * raised. Note, setting A implies setting I too, lowering I implies
 * lowering A too.  The B bits are, however, independently settable.
 */
int cap_iab_set_vector(cap_iab_t iab, cap_iab_vector_t vec, cap_value_t bit,
		       cap_flag_value_t raised)
{
    if (!good_cap_iab_t(iab) || (raised >> 1) || bit >= cap_max_bits()) {
	errno = EINVAL;
	return -1;
    }

    unsigned o = (bit >> 5);
    __u32 on = 1u << (bit & 31);
    __u32 mask = ~on;

    _cap_mu_lock(&iab->mutex);
    switch (vec) {
    case CAP_IAB_INH:
	iab->i[o] = (iab->i[o] & mask) | (raised ? on : 0);
	iab->a[o] &= iab->i[o];
	break;
    case CAP_IAB_AMB:
	iab->a[o] = (iab->a[o] & mask) | (raised ? on : 0);
	iab->i[o] |= iab->a[o];
	break;
    case CAP_IAB_BOUND:
	iab->nb[o] = (iab->nb[o] & mask) | (raised ? on : 0);
	break;
    default:
	errno = EINVAL;
	_cap_mu_unlock_return(&iab->mutex, -1);
    }

    _cap_mu_unlock(&iab->mutex);
    return 0;
}

/*
 * cap_iab_fill copies a bit-vector of capability state from a cap_t
 * to a cap_iab_t. Note, because the bounding bits in an iab are to be
 * dropped when applied, the copying process, when to a CAP_IAB_BOUND
 * vector involves inverting the bits. Also, adjusting I will mask
 * bits in A, and adjusting A may implicitly raise bits in I.
 */
int cap_iab_fill(cap_iab_t iab, cap_iab_vector_t vec,
		 cap_t cap_d, cap_flag_t flag)
{
    int i, ret = 0;

    if (!good_cap_t(cap_d) || !good_cap_iab_t(iab)) {
	errno = EINVAL;
	return -1;
    }

    switch (flag) {
    case CAP_EFFECTIVE:
    case CAP_INHERITABLE:
    case CAP_PERMITTED:
	break;
    default:
	errno = EINVAL;
	return -1;
    }

    /*
     * Make a private copy so we don't need to hold two locks at once
     * avoiding a recipe for a deadlock.
     */
    cap_d = cap_dup(cap_d);
    if (cap_d == NULL) {
	return -1;
    }

    _cap_mu_lock(&iab->mutex);
    for (i = 0; !ret && i < _LIBCAP_CAPABILITY_U32S; i++) {
	switch (vec) {
	case CAP_IAB_INH:
	    iab->i[i] = cap_d->u[i].flat[flag];
	    iab->a[i] &= iab->i[i];
	    break;
	case CAP_IAB_AMB:
	    iab->a[i] = cap_d->u[i].flat[flag];
	    iab->i[i] |= cap_d->u[i].flat[flag];
	    break;
	case CAP_IAB_BOUND:
	    iab->nb[i] = ~cap_d->u[i].flat[flag];
	    break;
	default:
	    errno = EINVAL;
	    ret = -1;
	    break;
	}
    }
    _cap_mu_unlock(&iab->mutex);

    cap_free(cap_d);
    return ret;
}

/*
 * cap_iab_compare compares two iab tuples.
 */
int cap_iab_compare(cap_iab_t a, cap_iab_t b)
{
    int j, result;
    if (!(good_cap_iab_t(a) && good_cap_iab_t(b))) {
	_cap_debug("invalid arguments");
	errno = EINVAL;
	return -1;
    }
    b = cap_iab_dup(b);
    if (b == NULL) {
	return -1;
    }

    _cap_mu_lock(&a->mutex);
    for (j=0, result=0; j<_LIBCAP_CAPABILITY_U32S; j++) {
	result |=
	    (a->i[j] == b->i[j] ? 0 : (1 << CAP_IAB_INH)) |
	    (a->a[j] == b->a[j] ? 0 : (1 << CAP_IAB_AMB)) |
	    (a->nb[j] == b->nb[j] ? 0 : (1 << CAP_IAB_BOUND));
    }
    _cap_mu_unlock(&a->mutex);
    cap_free(b);

    return result;
}
