/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2024 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include <sched.h>
#include "libbb.h"

unsigned long* FAST_FUNC get_malloc_cpu_affinity(int pid, unsigned *sz)
{
	unsigned long *mask = NULL;
	unsigned sz_in_bytes = *sz;

	for (;;) {
		mask = xrealloc(mask, sz_in_bytes);
		if (sched_getaffinity(pid, sz_in_bytes, (void*)mask) == 0)
			break; /* got it */
		sz_in_bytes *= 2;
		if (errno == EINVAL && (int)sz_in_bytes > 0)
			continue;
		bb_perror_msg_and_die("can't %cet pid %d's affinity", 'g', pid);
	}
	//bb_error_msg("get mask[0]:%lx sz_in_bytes:%d", mask[0], sz_in_bytes);
	*sz = sz_in_bytes;
	return mask;
}
