/*
 * SPDX-FileCopyrightText: 2022 Red Hat Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Benjamin Berg <bberg@redhat.com>
 */

#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include <libudev.h>
#include "gudev/gudev.h"
#include "gudev/gudevprivate.h"

static void
test_tags ()
{
	const char *expected_tags[] = { "tag1", "tag2", "tag3", NULL };
	const char *expected_current_tags[] = { "tag2", "tag3", NULL };
	struct udev *udev = NULL;
	struct udev_device *udev_device = NULL;
	g_autoptr(GUdevDevice) dev = NULL;

	/* Push device information into environment. */
	g_setenv ("DEVPATH", "/devices/dev1", TRUE);
	g_setenv ("SUBSYSTEM", "subsystem", TRUE);
	g_setenv ("ACTION", "add", TRUE);
	g_setenv ("SEQNUM", "1", TRUE);
	g_setenv ("TAGS", "tag1:tag2:tag3", TRUE);
	g_setenv ("CURRENT_TAGS", "tag2:tag3", TRUE);
	g_setenv ("UDEV_DATABASE_VERSION", "1", TRUE);

	udev = udev_new ();
	udev_device = udev_device_new_from_environment (udev);
	g_message ("error is: %d, %m", errno);
	g_message ("tags list entry form udev device %p: %p", udev_device, udev_device_get_tags_list_entry (udev_device));

	dev = _g_udev_device_new (udev_device);

	g_assert_nonnull (g_udev_device_get_tags (dev));
	g_assert_cmpstrv (expected_tags, g_udev_device_get_tags (dev));

	g_assert_nonnull (g_udev_device_get_current_tags (dev));
	g_assert_cmpstrv (expected_current_tags, g_udev_device_get_current_tags (dev));

	g_clear_pointer (&udev, udev_unref);
	g_clear_pointer (&udev_device, udev_device_unref);
}

int main(int argc, char **argv)
{
	setlocale (LC_ALL, NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/gudev/tags", test_tags);

	return g_test_run ();
}
