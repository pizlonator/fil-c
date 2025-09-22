/*
 * SPDX-FileCopyrightText:  Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif
#include "bit.h"
#include "defines.h"
#include "prototypes.h"
#include "shadowlog.h"
#include "sizeof.h"


static uint32_t csrand_uniform32(uint32_t n);
static unsigned long csrand_uniform_slow(unsigned long n);


/*
 * Return a uniformly-distributed CS random u_long value.
 */
unsigned long
csrand(void)
{
	FILE           *fp;
	unsigned long  r;

#ifdef HAVE_GETENTROPY
	/* getentropy may exist but lack kernel support.  */
	if (getentropy(&r, sizeof(r)) == 0)
		return r;
#endif

#ifdef HAVE_GETRANDOM
	/* Likewise getrandom.  */
	if (getrandom(&r, sizeof(r), 0) == sizeof(r))
		return r;
#endif

#ifdef HAVE_ARC4RANDOM_BUF
	/* arc4random_buf can never fail.  */
	arc4random_buf(&r, sizeof(r));
	return r;
#endif

	/* Use /dev/urandom as a last resort.  */
	fp = fopen("/dev/urandom", "r");
	if (NULL == fp) {
		goto fail;
	}

	if (fread(&r, sizeof(r), 1, fp) != 1) {
		fclose(fp);
		goto fail;
	}

	fclose(fp);
	return r;

fail:
	fprintf(log_get_logfd(), _("Unable to obtain random bytes.\n"));
	exit(1);
}


/*
 * Return a uniformly-distributed CS random value in the interval [0, n-1].
 */
unsigned long
csrand_uniform(unsigned long n)
{
	if (n == 0 || n > UINT32_MAX)
		return csrand_uniform_slow(n);

	return csrand_uniform32(n);
}


/*
 * Return a uniformly-distributed CS random value in the interval [min, max].
 */
unsigned long
csrand_interval(unsigned long min, unsigned long max)
{
	return csrand_uniform(max - min + 1) + min;
}


/*
 * Fast Random Integer Generation in an Interval
 * ACM Transactions on Modeling and Computer Simulation 29 (1), 2019
 * <https://arxiv.org/abs/1805.10941>
 */
static uint32_t
csrand_uniform32(uint32_t n)
{
	uint32_t  bound, rem;
	uint64_t  r, mult;

	if (n == 0)
		return csrand();

	bound = -n % n;  // analogous to `2^32 % n`, since `x % y == (x-y) % y`

	do {
		r = csrand();
		mult = r * n;
		rem = mult;  // analogous to `mult % 2^32`
	} while (rem < bound);  // p = (2^32 % n) / 2^32;  W.C.: n=2^31+1, p=0.5

	r = mult >> WIDTHOF(n);  // analogous to `mult / 2^32`

	return r;
}


static unsigned long
csrand_uniform_slow(unsigned long n)
{
	unsigned long  r, max, mask;

	max = n - 1;
	mask = bit_ceil_wrapul(n) - 1;

	do {
		r = csrand();
		r &= mask;  // optimization
	} while (r > max);  // p = ((mask+1) % n) / (mask+1);  W.C.: p=0.5

	return r;
}
