#ifndef UTIL_LINUX_EXITCODES_H
#define UTIL_LINUX_EXITCODES_H
/*
 * BE CAREFUL
 *
 * These exit codes are part of the official interface for mount,
 * fsck, mkfs, etc. wrappers.
 */

/* Exit codes used by mkfs-type programs */
#define MKFS_EX_OK	0	/* No errors */
#define MKFS_EX_ERROR	8	/* Operational error */
#define MKFS_EX_USAGE	16	/* Usage or syntax error */

/* Exit codes used by fsck-type programs */
#define FSCK_EX_OK		0	/* No errors */
#define FSCK_EX_NONDESTRUCT	1	/* File system errors corrected */
#define FSCK_EX_REBOOT		2	/* System should be rebooted */
#define FSCK_EX_DESTRUCT	FSCK_EX_REBOOT	/* Alias */
#define FSCK_EX_UNCORRECTED	4	/* File system errors left uncorrected */
#define FSCK_EX_ERROR		8	/* Operational error */
#define FSCK_EX_USAGE		16	/* Usage or syntax error */
#define FSCK_EX_LIBRARY		128	/* Shared library error */

#define EXIT_NOTSUPP 2

#endif	/* UTIL_LINUX_EXITCODES_H */
