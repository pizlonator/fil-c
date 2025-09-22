/*
 * SPDX-FileCopyrightText: 2013 Eric Biederman
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _IDMAPPING_H_
#define _IDMAPPING_H_

#include <sys/types.h>

struct map_range {
	unsigned long upper; /* first ID inside the namespace */
	unsigned long lower; /* first ID outside the namespace */
	unsigned long count; /* Length of the inside and outside ranges */
};

extern struct map_range *get_map_ranges(int ranges, int argc, char **argv);
extern void write_mapping(int proc_dir_fd, int ranges,
	const struct map_range *mappings, const char *map_file, uid_t ruid);

#endif /* _ID_MAPPING_H_ */

