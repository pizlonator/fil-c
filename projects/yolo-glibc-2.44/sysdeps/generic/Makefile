# Copyright (C) 1992-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

ifeq ($(subdir),string)
sysdep_routines += \
  tls-internal \
  # sysdep_routines
CFLAGS-wordcopy.c += -Wno-uninitialized
endif

ifeq ($(subdir),elf)
ifeq ($(enable-gsframe),yes)
sysdep_routines += \
  sframe \
  sframe-read \
  # sysdep_routines
endif
ifeq (yes:yes,$(build-shared):$(unwind-find-fde))
# This is needed to support g++ v2 and v3.
sysdep_routines += \
  framestate \
  unwind-pe \
  # sysdep_routines
shared-only-routines += \
  framestate \
  unwind-pe \
  # shared-only-routines
endif
endif

ifeq ($(subdir),malloc)
sysdep_malloc_debug_routines += \
  hugepages \
  # sysdep_malloc_debug_routines
endif

ifeq ($(subdir),misc)
sysdep_routines += \
  hugepages \
  # sysdep_routines
endif

ifeq ($(subdir),support)
libsupport-sysdep_routines += \
  support-address-diff \
  support-use-after-free \
  # libsupport-sysdep_routines
endif
