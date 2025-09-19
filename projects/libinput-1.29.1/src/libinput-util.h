/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBINPUT_UTIL_H
#define LIBINPUT_UTIL_H

#include "config.h"

#ifdef NDEBUG
#warning "libinput relies on assert(). #defining NDEBUG is not recommended"
#endif

#include "util-bits.h"
#include "util-list.h"
#include "util-macros.h"
#include "util-matrix.h"
#include "util-mem.h"
#include "util-multivalue.h"
#include "util-prop-parsers.h"
#include "util-range.h"
#include "util-ratelimit.h"
#include "util-strings.h"
#include "util-time.h"

#include "libinput.h"

#define VENDOR_ID_APPLE 0x5ac
#define VENDOR_ID_CHICONY 0x4f2
#define VENDOR_ID_LOGITECH 0x46d
#define VENDOR_ID_WACOM 0x56a
#define VENDOR_ID_SYNAPTICS_SERIAL 0x002
#define PRODUCT_ID_APPLE_KBD_TOUCHPAD 0x273
#define PRODUCT_ID_APPLE_APPLETOUCH 0x21a
#define PRODUCT_ID_SYNAPTICS_SERIAL 0x007
#define PRODUCT_ID_WACOM_EKR 0x0331

/* The HW DPI rate we normalize to before calculating pointer acceleration */
#define DEFAULT_MOUSE_DPI 1000
#define DEFAULT_TRACKPOINT_SENSITIVITY 128

#define _trace(file, ...) \
	do { \
	char buf_[1024]; \
	snprintf(buf_, sizeof(buf_), __VA_ARGS__); \
	fprintf(file, ANSI_BLUE "%30s():%4d - " ANSI_RED "%s" ANSI_NORMAL "\n", __func__, __LINE__, buf_); \
	} while (0)

#define trace(...) _trace(stdout, __VA_ARGS__)
#define etrace(...) _trace(stderr, __VA_ARGS__)

#define LIBINPUT_EXPORT __attribute__ ((visibility("default")))

/* Commonly-used cleanup  */
#ifdef udev_list_entry_foreach
DEFINE_UNREF_CLEANUP_FUNC(udev);
DEFINE_UNREF_CLEANUP_FUNC(udev_device);
DEFINE_UNREF_CLEANUP_FUNC(udev_enumerate);
DEFINE_UNREF_CLEANUP_FUNC(udev_monitor);
#endif
#ifdef LIBEVDEV_ATTRIBUTE_PRINTF
DEFINE_FREE_CLEANUP_FUNC(libevdev);
#endif
DEFINE_UNREF_CLEANUP_FUNC(libinput);
DEFINE_UNREF_CLEANUP_FUNC(libinput_device);
DEFINE_UNREF_CLEANUP_FUNC(libinput_tablet_tool);
DEFINE_UNREF_CLEANUP_FUNC(libinput_seat);
DEFINE_DESTROY_CLEANUP_FUNC(libinput_event);
DEFINE_DESTROY_CLEANUP_FUNC(libinput_config_accel);

#endif /* LIBINPUT_UTIL_H */
