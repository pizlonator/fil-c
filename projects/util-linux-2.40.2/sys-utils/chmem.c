/*
 * chmem - Memory configuration tool
 *
 * Copyright IBM Corp. 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include <dirent.h>

#include "c.h"
#include "nls.h"
#include "path.h"
#include "strutils.h"
#include "strv.h"
#include "optutils.h"
#include "closestream.h"
#include "xalloc.h"

/* partial success, otherwise we return regular EXIT_{SUCCESS,FAILURE} */
#define CHMEM_EXIT_SOMEOK		64

#define _PATH_SYS_MEMORY		"/sys/devices/system/memory"

struct chmem_desc {
	struct path_cxt	*sysmem;	/* _PATH_SYS_MEMORY handler */
	struct dirent	**dirs;
	int		ndirs;
	uint64_t	block_size;
	uint64_t	start;
	uint64_t	end;
	uint64_t	size;
	unsigned int	use_blocks : 1;
	unsigned int	is_size	   : 1;
	unsigned int	verbose	   : 1;
	unsigned int	have_zones : 1;
};

enum {
	CMD_MEMORY_ENABLE = 0,
	CMD_MEMORY_DISABLE,
	CMD_NONE
};

enum zone_id {
	ZONE_DMA = 0,
	ZONE_DMA32,
	ZONE_NORMAL,
	ZONE_HIGHMEM,
	ZONE_MOVABLE,
	ZONE_DEVICE,
};

static char *zone_names[] = {
	[ZONE_DMA]	= "DMA",
	[ZONE_DMA32]	= "DMA32",
	[ZONE_NORMAL]	= "Normal",
	[ZONE_HIGHMEM]	= "Highmem",
	[ZONE_MOVABLE]	= "Movable",
	[ZONE_DEVICE]	= "Device",
};

/*
 * name must be null-terminated
 */
static int zone_name_to_id(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(zone_names); i++) {
		if (!strcasecmp(name, zone_names[i]))
			return i;
	}
	return -1;
}

static void idxtostr(struct chmem_desc *desc, uint64_t idx, char *buf, size_t bufsz)
{
	uint64_t start, end;

	start = idx * desc->block_size;
	end = start + desc->block_size - 1;
	snprintf(buf, bufsz,
		 _("Memory Block %"PRIu64" (0x%016"PRIx64"-0x%016"PRIx64")"),
		 idx, start, end);
}

static int chmem_size(struct chmem_desc *desc, int enable, int zone_id)
{
	char *name, *onoff, line[BUFSIZ], str[BUFSIZ];
	uint64_t size, index;
	const char *zn;
	int i, rc;

	size = desc->size;
	onoff = enable ? "online" : "offline";
	i = enable ? 0 : desc->ndirs - 1;

	if (enable && zone_id >= 0) {
		if (zone_id == ZONE_MOVABLE)
			onoff = "online_movable";
		else
			onoff = "online_kernel";
	}

	for (; i >= 0 && i < desc->ndirs && size; i += enable ? 1 : -1) {
		name = desc->dirs[i]->d_name;
		index = strtou64_or_err(name + 6, _("Failed to parse index"));

		if (ul_path_readf_buffer(desc->sysmem, line, sizeof(line), "%s/state", name) > 0
		    && strncmp(onoff, line, 6) == 0)
			continue;

		if (desc->have_zones) {
			ul_path_readf_buffer(desc->sysmem, line, sizeof(line), "%s/valid_zones", name);
			if (zone_id >= 0) {
				zn = zone_names[zone_id];
				if (enable && !strcasestr(line, zn))
					continue;
				if (!enable && strncasecmp(line, zn, strlen(zn)) != 0)
					continue;
			} else if (enable) {
				/* By default, use zone Movable for online, if valid */
				if (strcasestr(line, zone_names[ZONE_MOVABLE]))
					onoff = "online_movable";
				else
					onoff = "online";
			}
		}

		idxtostr(desc, index, str, sizeof(str));
		rc = ul_path_writef_string(desc->sysmem, onoff, "%s/state", name);
		if (rc != 0 && desc->verbose) {
			if (enable)
				fprintf(stdout, _("%s enable failed\n"), str);
			else
				fprintf(stdout, _("%s disable failed\n"), str);
		} else if (rc == 0 && desc->verbose) {
			if (enable)
				fprintf(stdout, _("%s enabled\n"), str);
			else
				fprintf(stdout, _("%s disabled\n"), str);
		}
		if (rc == 0)
			size--;
	}
	if (size) {
		uint64_t bytes;
		char *sizestr;

		bytes = (desc->size - size) * desc->block_size;
		sizestr = size_to_human_string(SIZE_SUFFIX_1LETTER, bytes);
		if (enable)
			warnx(_("Could only enable %s of memory"), sizestr);
		else
			warnx(_("Could only disable %s of memory"), sizestr);
		free(sizestr);
	}
	return size == 0 ? 0 : size == desc->size ? -1 : 1;
}

static int chmem_range(struct chmem_desc *desc, int enable, int zone_id)
{
	char *name, *onoff, line[BUFSIZ], str[BUFSIZ];
	uint64_t index, todo;
	const char *zn;
	int i, rc;

	todo = desc->end - desc->start + 1;
	onoff = enable ? "online" : "offline";

	if (enable && zone_id >= 0) {
		if (zone_id == ZONE_MOVABLE)
			onoff = "online_movable";
		else
			onoff = "online_kernel";
	}

	for (i = 0; i < desc->ndirs; i++) {
		name = desc->dirs[i]->d_name;
		index = strtou64_or_err(name + 6, _("Failed to parse index"));
		if (index < desc->start)
			continue;
		if (index > desc->end)
			break;
		idxtostr(desc, index, str, sizeof(str));
		if (ul_path_readf_buffer(desc->sysmem, line, sizeof(line), "%s/state", name) > 0
		    && strncmp(onoff, line, 6) == 0) {
			if (desc->verbose && enable)
				fprintf(stdout, _("%s already enabled\n"), str);
			else if (desc->verbose && !enable)
				fprintf(stdout, _("%s already disabled\n"), str);
			todo--;
			continue;
		}

		if (desc->have_zones) {
			ul_path_readf_buffer(desc->sysmem, line, sizeof(line), "%s/valid_zones", name);
			if (zone_id >= 0) {
				zn = zone_names[zone_id];
				if (enable && !strcasestr(line, zn)) {
					warnx(_("%s enable failed: Zone mismatch"), str);
					continue;
				}
				if (!enable && strncasecmp(line, zn, strlen(zn)) != 0) {
					warnx(_("%s disable failed: Zone mismatch"), str);
					continue;
				}
			} else if (enable) {
				/* By default, use zone Movable for online, if valid */
				if (strcasestr(line, zone_names[ZONE_MOVABLE]))
					onoff = "online_movable";
				else
					onoff = "online";
			}
		}

		rc = ul_path_writef_string(desc->sysmem, onoff, "%s/state", name);
		if (rc != 0) {
			if (enable)
				warn(_("%s enable failed"), str);
			else
				warn(_("%s disable failed"), str);
		} else if (desc->verbose) {
			if (enable)
				fprintf(stdout, _("%s enabled\n"), str);
			else
				fprintf(stdout, _("%s disabled\n"), str);
		}
		if (rc == 0)
			todo--;
	}
	return todo == 0 ? 0 : todo == desc->end - desc->start + 1 ? -1 : 1;
}

static int filter(const struct dirent *de)
{
	if (strncmp("memory", de->d_name, 6) != 0)
		return 0;
	return isdigit_string(de->d_name + 6);
}

static void read_info(struct chmem_desc *desc)
{
	char line[128];

	desc->ndirs = scandir(_PATH_SYS_MEMORY, &desc->dirs, filter, versionsort);
	if (desc->ndirs <= 0)
		goto fail;
	ul_path_read_buffer(desc->sysmem, line, sizeof(line), "block_size_bytes");

	errno = 0;
	desc->block_size = strtoumax(line, NULL, 16);
	if (errno)
		goto fail;
	return;
fail:
	err(EXIT_FAILURE, _("Failed to read %s"), _PATH_SYS_MEMORY);
}

static void parse_single_param(struct chmem_desc *desc, char *str)
{
	if (desc->use_blocks) {
		desc->start = strtou64_or_err(str, _("Failed to parse block number"));
		desc->end = desc->start;
		return;
	}
	desc->is_size = 1;
	desc->size = strtosize_or_err(str, _("Failed to parse size"));
	if (isdigit(str[strlen(str) - 1]))
		desc->size *= 1024*1024;
	if (desc->size % desc->block_size) {
		errx(EXIT_FAILURE, _("Size must be aligned to memory block size (%s)"),
		     size_to_human_string(SIZE_SUFFIX_1LETTER, desc->block_size));
	}
	desc->size /= desc->block_size;
}

static void parse_range_param(struct chmem_desc *desc, char *start, char *end)
{
	if (desc->use_blocks) {
		desc->start = strtou64_or_err(start, _("Failed to parse start"));
		desc->end = strtou64_or_err(end, _("Failed to parse end"));
		return;
	}
	if (strlen(start) < 2 || start[1] != 'x')
		errx(EXIT_FAILURE, _("Invalid start address format: %s"), start);
	if (strlen(end) < 2 || end[1] != 'x')
		errx(EXIT_FAILURE, _("Invalid end address format: %s"), end);
	desc->start = strtox64_or_err(start, _("Failed to parse start address"));
	desc->end = strtox64_or_err(end, _("Failed to parse end address"));
	if (desc->start % desc->block_size || (desc->end + 1) % desc->block_size) {
		errx(EXIT_FAILURE,
		     _("Start address and (end address + 1) must be aligned to "
		       "memory block size (%s)"),
		     size_to_human_string(SIZE_SUFFIX_1LETTER, desc->block_size));
	}
	desc->start /= desc->block_size;
	desc->end /= desc->block_size;
}

static void parse_parameter(struct chmem_desc *desc, char *param)
{
	char **split;

	split = strv_split(param, "-");
	if (strv_length(split) > 2)
		errx(EXIT_FAILURE, _("Invalid parameter: %s"), param);
	if (strv_length(split) == 1)
		parse_single_param(desc, split[0]);
	else
		parse_range_param(desc, split[0], split[1]);
	strv_free(split);
	if (desc->start > desc->end)
		errx(EXIT_FAILURE, _("Invalid range: %s"), param);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [SIZE|RANGE|BLOCKRANGE]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Set a particular size or range of memory online or offline.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -e, --enable       enable memory\n"), out);
	fputs(_(" -d, --disable      disable memory\n"), out);
	fputs(_(" -b, --blocks       use memory blocks\n"), out);
	fputs(_(" -z, --zone <name>  select memory zone (see below)\n"), out);
	fputs(_(" -v, --verbose      verbose output\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(20));

	fputs(_("\nSupported zones:\n"), out);
	for (i = 0; i < ARRAY_SIZE(zone_names); i++)
		fprintf(out, " %s\n", zone_names[i]);

	fprintf(out, USAGE_MAN_TAIL("chmem(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct chmem_desc _desc = { 0 }, *desc = &_desc;
	int cmd = CMD_NONE, zone_id = -1;
	char *zone = NULL;
	int c, rc;

	static const struct option longopts[] = {
		{"block",	no_argument,		NULL, 'b'},
		{"disable",	no_argument,		NULL, 'd'},
		{"enable",	no_argument,		NULL, 'e'},
		{"help",	no_argument,		NULL, 'h'},
		{"verbose",	no_argument,		NULL, 'v'},
		{"version",	no_argument,		NULL, 'V'},
		{"zone",	required_argument,	NULL, 'z'},
		{NULL,		0,			NULL, 0}
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'd','e' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	ul_path_init_debug();
	desc->sysmem = ul_new_path(_PATH_SYS_MEMORY);
	if (!desc->sysmem)
		err(EXIT_FAILURE, _("failed to initialize %s handler"), _PATH_SYS_MEMORY);

	read_info(desc);

	while ((c = getopt_long(argc, argv, "bdehvVz:", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'd':
			cmd = CMD_MEMORY_DISABLE;
			break;
		case 'e':
			cmd = CMD_MEMORY_ENABLE;
			break;
		case 'b':
			desc->use_blocks = 1;
			break;
		case 'v':
			desc->verbose = 1;
			break;
		case 'z':
			zone = xstrdup(optarg);
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if ((argc == 1) || (argc != optind + 1) || (cmd == CMD_NONE)) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	parse_parameter(desc, argv[optind]);


	/* The valid_zones sysfs attribute was introduced with kernel 3.18 */
	if (ul_path_access(desc->sysmem, F_OK, "memory0/valid_zones") == 0)
		desc->have_zones = 1;
	else if (zone)
		warnx(_("zone ignored, no valid_zones sysfs attribute present"));

	if (zone && desc->have_zones) {
		zone_id = zone_name_to_id(zone);
		if (zone_id == -1) {
			warnx(_("unknown memory zone: %s"), zone);
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (desc->is_size)
		rc = chmem_size(desc, cmd == CMD_MEMORY_ENABLE ? 1 : 0, zone_id);
	else
		rc = chmem_range(desc, cmd == CMD_MEMORY_ENABLE ? 1 : 0, zone_id);

	ul_unref_path(desc->sysmem);

	return rc == 0 ? EXIT_SUCCESS :
		rc < 0 ? EXIT_FAILURE : CHMEM_EXIT_SOMEOK;
}
