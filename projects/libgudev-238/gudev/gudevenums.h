/* -*- Mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * SPDX-FileCopyrightText: 2008 David Zeuthen <davidz@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (_GUDEV_COMPILATION) && !defined(_GUDEV_INSIDE_GUDEV_H)
#error "Only <gudev/gudev.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __G_UDEV_ENUMS_H__
#define __G_UDEV_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GUdevDeviceType:
 * @G_UDEV_DEVICE_TYPE_NONE: Device does not have a device file.
 * @G_UDEV_DEVICE_TYPE_BLOCK: Device is a block device.
 * @G_UDEV_DEVICE_TYPE_CHAR: Device is a character device.
 *
 * Enumeration used to specify a the type of a device.
 */
typedef enum
{
  G_UDEV_DEVICE_TYPE_NONE = 0,
  G_UDEV_DEVICE_TYPE_BLOCK = 'b',
  G_UDEV_DEVICE_TYPE_CHAR = 'c',
} GUdevDeviceType;

G_END_DECLS

#endif /* __G_UDEV_ENUMS_H__ */
