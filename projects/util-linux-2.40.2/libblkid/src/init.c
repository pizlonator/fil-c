/*
 * Copyright (C) 2008-2013 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: init
 * @title: Library initialization
 * @short_description: initialize debugging
 */

#include <stdarg.h>

#include "blkidP.h"

UL_DEBUG_DEFINE_MASK(libblkid);
UL_DEBUG_DEFINE_MASKNAMES(libblkid) =
{
	{ "all", BLKID_DEBUG_ALL,	"info about all subsystems" },
	{ "cache", BLKID_DEBUG_CACHE,	"blkid tags cache" },
	{ "config", BLKID_DEBUG_CONFIG, "config file utils" },
	{ "dev", BLKID_DEBUG_DEV,       "device utils" },
	{ "devname", BLKID_DEBUG_DEVNAME, "/proc/partitions evaluation" },
	{ "devno", BLKID_DEBUG_DEVNO,	"conversions to device name" },
	{ "evaluate", BLKID_DEBUG_EVALUATE, "tags resolving" },
	{ "help", BLKID_DEBUG_HELP,	"this help" },
	{ "lowprobe", BLKID_DEBUG_LOWPROBE, "superblock/raids/partitions probing" },
	{ "buffer", BLKID_DEBUG_BUFFER, "low-probing buffers" },
	{ "probe", BLKID_DEBUG_PROBE,	"devices verification" },
	{ "read", BLKID_DEBUG_READ,	"cache parsing" },
	{ "save", BLKID_DEBUG_SAVE,	"cache writing" },
	{ "tag", BLKID_DEBUG_TAG,	"tags utils" },
	{ NULL, 0, NULL }
};

/**
 * blkid_init_debug:
 * @mask: debug mask (0xffff to enable full debugging)
 *
 * If the @mask is not specified then this function reads
 * LIBBLKID_DEBUG environment variable to get the mask.
 *
 * Already initialized debugging stuff cannot be changed. It does not
 * have effect to call this function twice.
 */
void blkid_init_debug(int mask)
{
	if (libblkid_debug_mask)
		return;

	__UL_INIT_DEBUG_FROM_ENV(libblkid, BLKID_DEBUG_, mask, LIBBLKID_DEBUG);

	if (libblkid_debug_mask != BLKID_DEBUG_INIT
	    && libblkid_debug_mask != (BLKID_DEBUG_HELP|BLKID_DEBUG_INIT)) {
		const char *ver = NULL;
		const char *date = NULL;

		blkid_get_library_version(&ver, &date);
		DBG(INIT, ul_debug("library debug mask: 0x%04x", libblkid_debug_mask));
		DBG(INIT, ul_debug("library version: %s [%s]", ver, date));

	}
	ON_DBG(HELP, ul_debug_print_masks("LIBBLKID_DEBUG",
				UL_DEBUG_MASKNAMES(libblkid)));
}

static void __attribute__ ((constructor)) blkid_init_default_debug(void)
{
	blkid_init_debug(0);
}
