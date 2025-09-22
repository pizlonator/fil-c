/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PT_MBR_H
#define UTIL_LINUX_PT_MBR_H

#include <assert.h>

struct dos_partition {
	unsigned char boot_ind;		/* 0x80 - active */
	unsigned char bh, bs, bc;	/* begin CHS */
	unsigned char sys_ind;
	unsigned char eh, es, ec;	/* end CHS */
	unsigned char start_sect[4];
	unsigned char nr_sects[4];
} __attribute__((packed));

#define MBR_PT_OFFSET		0x1be
#define MBR_PT_BOOTBITS_SIZE	440

static inline struct dos_partition *mbr_get_partition(const unsigned char *mbr, int i)
{
	return (struct dos_partition *)
		(mbr + MBR_PT_OFFSET + (i * sizeof(struct dos_partition)));
}

/* assemble badly aligned little endian integer */
static inline uint32_t __dos_assemble_4le(const unsigned char *p)
{
	uint32_t last_byte = p[3];

	return p[0] | (p[1] << 8) | (p[2] << 16) | (last_byte << 24);
}

static inline void __dos_store_4le(unsigned char *p, unsigned int val)
{
	assert(!(p == NULL));
	p[0] = (val & 0xff);
	p[1] = ((val >> 8) & 0xff);
	p[2] = ((val >> 16) & 0xff);
	p[3] = ((val >> 24) & 0xff);
}

static inline unsigned int dos_partition_get_start(const struct dos_partition *p)
{
	return __dos_assemble_4le(&(p->start_sect[0]));
}

static inline void dos_partition_set_start(struct dos_partition *p, unsigned int n)
{
	__dos_store_4le(p->start_sect, n);
}

static inline unsigned int dos_partition_get_size(const struct dos_partition *p)
{
	return __dos_assemble_4le(&(p->nr_sects[0]));
}

static inline void dos_partition_set_size(struct dos_partition *p, unsigned int n)
{
	__dos_store_4le(p->nr_sects, n);
}

static inline void dos_partition_sync_chs(struct dos_partition *p, unsigned long long int part_offset, unsigned int geom_sectors, unsigned int geom_heads)
{
	unsigned long long int start = part_offset + dos_partition_get_start(p);
	unsigned long long int stop = start + dos_partition_get_size(p) - 1;
	unsigned int spc = geom_heads * geom_sectors;

	if (start / spc > 1023)
		start = spc * 1024 - 1;
	if (stop / spc > 1023)
		stop = spc * 1024 - 1;

	p->bc = (start / spc) & 0xff;
	p->bh = (start / geom_sectors) % geom_heads;
	p->bs = ((start % geom_sectors + 1) & 0x3f) |
		(((start / spc) >> 2) & 0xc0);

	p->ec = (stop / spc) & 0xff;
	p->eh = (stop / geom_sectors) % geom_heads;
	p->es = ((stop % geom_sectors + 1) & 0x3f) |
		(((stop / spc) >> 2) & 0xc0);
}

static inline int mbr_is_valid_magic(const unsigned char *mbr)
{
	return mbr[510] == 0x55 && mbr[511] == 0xaa ? 1 : 0;
}

static inline void mbr_set_magic(unsigned char *b)
{
	b[510] = 0x55;
	b[511] = 0xaa;
}

static inline unsigned int mbr_get_id(const unsigned char *mbr)
{
	return __dos_assemble_4le(&mbr[440]);
}

static inline void mbr_set_id(unsigned char *b, unsigned int id)
{
	__dos_store_4le(&b[440], id);
}

enum {
	MBR_EMPTY_PARTITION		= 0x00,
	MBR_FAT12_PARTITION		= 0x01,
	MBR_XENIX_ROOT_PARTITION	= 0x02,
	MBR_XENIX_USR_PARTITION		= 0x03,
	MBR_FAT16_LESS32M_PARTITION	= 0x04,
	MBR_DOS_EXTENDED_PARTITION	= 0x05,
	MBR_FAT16_PARTITION		= 0x06, /* DOS 16-bit >=32M */
	MBR_HPFS_NTFS_PARTITION		= 0x07, /* OS/2 IFS, eg, HPFS or NTFS or QNX */
	MBR_AIX_PARTITION		= 0x08, /* AIX boot (AIX -- PS/2 port) or SplitDrive */
	MBR_AIX_BOOTABLE_PARTITION	= 0x09, /* AIX data or Coherent */
	MBR_OS2_BOOTMNGR_PARTITION	= 0x0a, /* OS/2 Boot Manager */
	MBR_W95_FAT32_PARTITION		= 0x0b,
	MBR_W95_FAT32_LBA_PARTITION	= 0x0c, /* LBA really is `Extended Int 13h' */
	MBR_W95_FAT16_LBA_PARTITION	= 0x0e,
	MBR_W95_EXTENDED_PARTITION	= 0x0f,
	MBR_OPUS_PARTITION		= 0x10,
	MBR_HIDDEN_FAT12_PARTITION	= 0x11,
	MBR_COMPAQ_DIAGNOSTICS_PARTITION = 0x12,
	MBR_HIDDEN_FAT16_L32M_PARTITION	= 0x14,
	MBR_HIDDEN_FAT16_PARTITION	= 0x16,
	MBR_HIDDEN_HPFS_NTFS_PARTITION	= 0x17,
	MBR_AST_SMARTSLEEP_PARTITION	= 0x18,
	MBR_HIDDEN_W95_FAT32_PARTITION	= 0x1b,
	MBR_HIDDEN_W95_FAT32LBA_PARTITION = 0x1c,
	MBR_HIDDEN_W95_FAT16LBA_PARTITION = 0x1e,
	MBR_NEC_DOS_PARTITION		= 0x24,
	MBR_PLAN9_PARTITION		= 0x39,
	MBR_PARTITIONMAGIC_PARTITION	= 0x3c,
	MBR_VENIX80286_PARTITION	= 0x40,
	MBR_PPC_PREP_BOOT_PARTITION	= 0x41,
	MBR_SFS_PARTITION		= 0x42,
	MBR_QNX_4X_PARTITION		= 0x4d,
	MBR_QNX_4X_2ND_PARTITION	= 0x4e,
	MBR_QNX_4X_3RD_PARTITION	= 0x4f,
	MBR_DM_PARTITION		= 0x50,
	MBR_DM6_AUX1_PARTITION		= 0x51, /* (or Novell) */
	MBR_CPM_PARTITION		= 0x52, /* CP/M or Microport SysV/AT */
	MBR_DM6_AUX3_PARTITION		= 0x53,
	MBR_DM6_PARTITION		= 0x54,
	MBR_EZ_DRIVE_PARTITION		= 0x55,
	MBR_GOLDEN_BOW_PARTITION	= 0x56,
	MBR_PRIAM_EDISK_PARTITION	= 0x5c,
	MBR_SPEEDSTOR_PARTITION		= 0x61,
	MBR_GNU_HURD_PARTITION		= 0x63, /* GNU HURD or Mach or Sys V/386 (such as ISC UNIX) */
	MBR_UNIXWARE_PARTITION		= MBR_GNU_HURD_PARTITION,
	MBR_NETWARE_286_PARTITION	= 0x64,
	MBR_NETWARE_386_PARTITION	= 0x65,
	MBR_DISKSECURE_MULTIBOOT_PARTITION = 0x70,
	MBR_PC_IX_PARTITION		= 0x75,
	MBR_OLD_MINIX_PARTITION		= 0x80, /* Minix 1.4a and earlier */
	MBR_MINIX_PARTITION		= 0x81, /* Minix 1.4b and later */
	MBR_LINUX_SWAP_PARTITION	= 0x82,
	MBR_SOLARIS_X86_PARTITION	= MBR_LINUX_SWAP_PARTITION,
	MBR_LINUX_DATA_PARTITION	= 0x83,
	MBR_OS2_HIDDEN_DRIVE_PARTITION	= 0x84, /* also hibernation MS APM, Intel Rapid Start */
	MBR_INTEL_HIBERNATION_PARTITION	= MBR_OS2_HIDDEN_DRIVE_PARTITION,
	MBR_LINUX_EXTENDED_PARTITION	= 0x85,
	MBR_NTFS_VOL_SET1_PARTITION	= 0x86,
	MBR_NTFS_VOL_SET2_PARTITION	= 0x87,
	MBR_LINUX_PLAINTEXT_PARTITION	= 0x88,
	MBR_LINUX_LVM_PARTITION		= 0x8e,
	MBR_AMOEBA_PARTITION		= 0x93,
	MBR_AMOEBA_BBT_PARTITION	= 0x94, /* (bad block table) */
	MBR_BSD_OS_PARTITION		= 0x9f, /* BSDI */
	MBR_THINKPAD_HIBERNATION_PARTITION = 0xa0,
	MBR_FREEBSD_PARTITION		= 0xa5, /* various BSD flavours */
	MBR_OPENBSD_PARTITION		= 0xa6,
	MBR_NEXTSTEP_PARTITION		= 0xa7,
	MBR_DARWIN_UFS_PARTITION	= 0xa8,
	MBR_NETBSD_PARTITION		= 0xa9,
	MBR_DARWIN_BOOT_PARTITION	= 0xab,
	MBR_HFS_HFS_PARTITION		= 0xaf,
	MBR_BSDI_FS_PARTITION		= 0xb7,
	MBR_BSDI_SWAP_PARTITION		= 0xb8,
	MBR_BOOTWIZARD_HIDDEN_PARTITION	= 0xbb,
	MBR_ACRONIS_FAT32LBA_PARTITION  = 0xbc, /* Acronis Secure Zone with ipl for loader F11.SYS */
	MBR_SOLARIS_BOOT_PARTITION	= 0xbe,
	MBR_SOLARIS_PARTITION		= 0xbf,
	MBR_DRDOS_FAT12_PARTITION	= 0xc1,
	MBR_DRDOS_FAT16_L32M_PARTITION	= 0xc4,
	MBR_DRDOS_FAT16_PARTITION	= 0xc6,
	MBR_SYRINX_PARTITION		= 0xc7,
	MBR_NONFS_DATA_PARTITION	= 0xda,
	MBR_CPM_CTOS_PARTITION		= 0xdb, /* CP/M or Concurrent CP/M or Concurrent DOS or CTOS */
	MBR_DELL_UTILITY_PARTITION	= 0xde, /* Dell PowerEdge Server utilities */
	MBR_BOOTIT_PARTITION		= 0xdf, /* BootIt EMBRM */
	MBR_DOS_ACCESS_PARTITION	= 0xe1, /* DOS access or SpeedStor 12-bit FAT extended partition */
	MBR_DOS_RO_PARTITION		= 0xe3, /* DOS R/O or SpeedStor */
	MBR_SPEEDSTOR_EXTENDED_PARTITION = 0xe4, /* SpeedStor 16-bit FAT extended partition < 1024 cyl. */
	MBR_RUFUS_EXTRA_PARTITION	= 0xea, /* Rufus extra partition for alignment */
	MBR_BEOS_FS_PARTITION		= 0xeb,
	MBR_GPT_PARTITION		= 0xee, /* Intel EFI GUID Partition Table */
	MBR_EFI_SYSTEM_PARTITION	= 0xef, /* Intel EFI System Partition */
	MBR_LINUX_PARISC_BOOT_PARTITION	= 0xf0, /* Linux/PA-RISC boot loader */
	MBR_SPEEDSTOR1_PARTITION	= 0xf1,
	MBR_SPEEDSTOR2_PARTITION	= 0xf4, /* SpeedStor large partition */
	MBR_DOS_SECONDARY_PARTITION	= 0xf2, /* DOS 3.3+ secondary */
	MBR_EBBR_PROTECTIVE_PARTITION	= 0xf8, /* Arm EBBR firmware protective partition */
	MBR_VMWARE_VMFS_PARTITION	= 0xfb,
	MBR_VMWARE_VMKCORE_PARTITION	= 0xfc, /* VMware kernel dump partition */
	MBR_LINUX_RAID_PARTITION	= 0xfd, /* Linux raid partition with autodetect using persistent superblock */
	MBR_LANSTEP_PARTITION		= 0xfe, /* SpeedStor >1024 cyl. or LANstep */
	MBR_XENIX_BBT_PARTITION		= 0xff, /* Xenix Bad Block Table */
};

#endif /* UTIL_LINUX_PT_MBR_H */
