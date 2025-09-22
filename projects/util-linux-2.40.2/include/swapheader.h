/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef _SWAPHEADER_H
#define _SWAPHEADER_H

#define SWAP_VERSION 1
#define SWAP_UUID_LENGTH 16
#define SWAP_LABEL_LENGTH 16
#define SWAP_SIGNATURE "SWAPSPACE2"
#define SWAP_SIGNATURE_SZ (sizeof(SWAP_SIGNATURE) - 1)

#include <stdint.h>

struct swap_header_v1_2 {
	char	      bootbits[1024];    /* Space for disklabel etc. */
	uint32_t      version;
	uint32_t      last_page;
	uint32_t      nr_badpages;
	unsigned char uuid[SWAP_UUID_LENGTH];
	char	      volume_name[SWAP_LABEL_LENGTH];
	uint32_t      padding[117];
	uint32_t      badpages[1];
};

#endif /* _SWAPHEADER_H */
