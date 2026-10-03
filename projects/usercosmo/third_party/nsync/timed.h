#ifndef NSYNC_TIMED_H_
#define NSYNC_TIMED_H_
#include "third_party/nsync/time.h"
#include "third_party/nsync/mu.h"
COSMOPOLITAN_C_START_

errno_t nsync_mu_timedlock(nsync_mu *, nsync_time);
errno_t nsync_mu_clocklock(nsync_mu *, int, nsync_time);

COSMOPOLITAN_C_END_
#endif /* NSYNC_TIMED_H_ */
