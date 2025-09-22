/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PT_SUN_H
#define UTIL_LINUX_PT_SUN_H

#include <stdint.h>

#define SUN_LABEL_MAGIC		0xDABE

/* Supported VTOC setting */
#define SUN_VTOC_SANITY		0x600DDEEE	/* magic number */
#define SUN_VTOC_VERSION	1
#define SUN_MAXPARTITIONS	8

struct sun_disklabel {
	unsigned char label_id[128];   /* Informative text string */

	struct sun_vtoc {
		uint32_t version;     /* version */
		char	 volume_id[8];/* volume name */
		uint16_t nparts;      /* num of partitions */

		struct sun_info {        /* partition information */
			uint16_t id;     /* SUN_TAG_*  */
			uint16_t flags;  /* SUN_FLAG_* */
		} __attribute__ ((packed)) infos[8];

		uint16_t padding;      /* padding */
		uint32_t bootinfo[3];  /* info needed by mboot */
		uint32_t sanity;       /* magic number */
		uint32_t reserved[10]; /* padding */
		uint32_t timestamp[8]; /* partition timestamp */
	} __attribute__ ((packed)) vtoc;

	uint32_t write_reinstruct;     /* sectors to skip, writes */
	uint32_t read_reinstruct;      /* sectors to skip, reads */
	unsigned char spare[148];      /* padding */
	uint16_t rpm;                  /* disk rotational speed */
	uint16_t pcyl;                 /* physical cylinder count */
	uint16_t apc;                  /* extra sects per cylinder */
	uint16_t obs1;
	uint16_t obs2;
	uint16_t intrlv;               /* interleave factor */
	uint16_t ncyl;                 /* data cylinder count */
	uint16_t acyl;                 /* alt. cylinder count */
	uint16_t nhead;                /* tracks per cylinder   <---- */
	uint16_t nsect;                /* sectors per track     <---- */
	uint16_t obs3;
	uint16_t obs4;

	struct sun_partition {         /* partitions */
		uint32_t start_cylinder;
		uint32_t num_sectors;
	} __attribute__ ((packed)) partitions[8];

	uint16_t magic;                /* magic number */
	uint16_t csum;                 /* label xor'd checksum */
} __attribute__ ((packed));


#define SUN_TAG_UNASSIGNED	0x00	/* Unassigned partition */
#define SUN_TAG_BOOT		0x01	/* Boot partition	*/
#define SUN_TAG_ROOT		0x02	/* Root filesystem	*/
#define SUN_TAG_SWAP		0x03	/* Swap partition	*/
#define SUN_TAG_USR		0x04	/* /usr filesystem	*/
#define SUN_TAG_WHOLEDISK	0x05	/* Full-disk slice	*/
#define SUN_TAG_STAND		0x06	/* Stand partition	*/
#define SUN_TAG_VAR		0x07	/* /var filesystem	*/
#define SUN_TAG_HOME		0x08	/* /home filesystem	*/
#define SUN_TAG_ALTSCTR		0x09	/* Alt sector partition	*/
#define SUN_TAG_CACHE		0x0a	/* Cachefs partition	*/
#define SUN_TAG_RESERVED	0x0b	/* SMI reserved data	*/
#define SUN_TAG_LINUX_SWAP	0x82	/* Linux SWAP		*/
#define SUN_TAG_LINUX_NATIVE	0x83	/* Linux filesystem	*/
#define SUN_TAG_LINUX_LVM	0x8e	/* Linux LVM		*/
#define SUN_TAG_LINUX_RAID	0xfd	/* LInux RAID		*/

#define SUN_FLAG_UNMNT		0x01	/* Unmountable partition*/
#define SUN_FLAG_RONLY		0x10	/* Read only		*/

static inline uint16_t sun_pt_checksum(const struct sun_disklabel *label)
{
	const uint16_t *ptr = ((const uint16_t *) (label + 1)) - 1;
	uint16_t sum;

	for (sum = 0; ptr >= ((const uint16_t *) label);)
		sum ^= *ptr--;

	return sum;
}

#endif /* UTIL_LINUX_PT_SUN_H */
