#ifndef PROCPS_NG_RPMATCH_H
#define PROCPS_NG_RPMATCH_H

#ifndef HAVE_RPMATCH
#define rpmatch(r) \
	(*r == 'y' || *r == 'Y' ? 1 : *r == 'n' || *r == 'N' ? 0 : -1)
#endif

#endif /* PROCPS_NG_RPMATCH_H */
