/* -*- Mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * SPDX-FileCopyrightText: 2008 David Zeuthen <davidz@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (_GUDEV_COMPILATION) && !defined(_GUDEV_INSIDE_GUDEV_H)
#error "Only <gudev/gudev.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __G_UDEV_TYPES_H__
#define __G_UDEV_TYPES_H__

#include <gudev/gudevenums.h>
#include <sys/types.h>

G_BEGIN_DECLS

typedef struct _GUdevClient GUdevClient;
typedef struct _GUdevDevice GUdevDevice;
typedef struct _GUdevEnumerator GUdevEnumerator;

/**
 * GUdevDeviceNumber:
 *
 * Corresponds to the standard #dev_t type as defined by POSIX (Until
 * bug 584517 is resolved this work-around is needed).
 */
#ifdef _GUDEV_WORK_AROUND_DEV_T_BUG
typedef guint64 GUdevDeviceNumber; /* __UQUAD_TYPE */
#else
typedef dev_t GUdevDeviceNumber;
#endif

G_END_DECLS

#endif /* __G_UDEV_TYPES_H__ */
