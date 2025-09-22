/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_RPMATCH_H
#define UTIL_LINUX_RPMATCH_H

#ifndef HAVE_RPMATCH
#define rpmatch(r) \
	(*r == 'y' || *r == 'Y' ? 1 : *r == 'n' || *r == 'N' ? 0 : -1)
#endif

#define RPMATCH_YES	 1
#define RPMATCH_NO	 0
#define RPMATCH_INVALID	-1

#endif /* UTIL_LINUX_RPMATCH_H */
