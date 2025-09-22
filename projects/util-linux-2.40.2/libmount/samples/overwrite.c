/*
 * Copyright (C) 2023 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * This sample reads the mountpoint entry from /etc/fstab and mounts it to the
 * different (on the command line specified) mountpoint. The mount options
 * settings are read from fstab.
 */
#include <stdlib.h>

#include "c.h"

#include "libmount.h"


int main(int argc, char *argv[])
{
	char *target, *fstab_target;
        struct libmnt_table *tab;
        struct libmnt_fs *fs;
        struct libmnt_context *cxt;
	int rc;

	if (argc != 3)
		errx(EXIT_FAILURE, "usage: %s <mnt-from-fstab> <target>",
				program_invocation_short_name);

	fstab_target = argv[1];
	target = argv[2];

	printf("Mounting %s from fstab to %s\n", fstab_target, target);

	tab = mnt_new_table_from_file("/etc/fstab");
	if (!tab)
		err(EXIT_FAILURE, "failed to parse fstab");

	fs = mnt_table_find_target(tab, fstab_target, MNT_ITER_FORWARD);
	if (!fs)
		err(EXIT_FAILURE, "cannot found %s in fstab", argv[1]);

        cxt = mnt_new_context();
	if (!cxt)
		err(EXIT_FAILURE, "cannot allocate context");

	mnt_context_set_fs(cxt, fs);
	mnt_context_set_target(cxt, target);

	rc = mnt_context_mount(cxt);

	printf("Done: rc=%d status=%d\n", rc, mnt_context_get_status(cxt));

	mnt_free_context(cxt);
	mnt_unref_table(tab);
	return rc == 0 && mnt_context_get_status(cxt) == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
