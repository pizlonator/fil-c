/* -*- Mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * SPDX-FileCopyrightText: 2008 David Zeuthen <davidz@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (_GUDEV_COMPILATION) && !defined(_GUDEV_INSIDE_GUDEV_H)
#error "Only <gudev/gudev.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __G_UDEV_PRIVATE_H__
#define __G_UDEV_PRIVATE_H__

#include <gudev/gudevtypes.h>

#include <libudev.h>

G_BEGIN_DECLS

GUdevDevice *
_g_udev_device_new (struct udev_device *udevice);

struct udev_enumerate *_g_udev_client_new_enumerate (GUdevClient *client);

G_END_DECLS

#endif /* __G_UDEV_PRIVATE_H__ */
