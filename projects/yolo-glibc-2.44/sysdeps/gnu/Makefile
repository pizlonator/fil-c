# Copyright (C) 1996-2026 Free Software Foundation, Inc.
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

ifeq ($(subdir),login)
sysdep_routines += \
  endutxent \
  getutmp \
  getutmpx \
  getutxent \
  getutxid \
  getutxline \
  pututxline \
  setutxent \
  updwtmpx \
  utmpxname \
  # sysdep_routines

sysdep_headers += \
  bits/utmpx.h \
  utmpx.h \
  # sysdep_headers
endif


ifeq ($(subdir),inet)
sysdep_headers += \
  netinet/ip_icmp.h \
  netinet/udp.h \
  # sysdep_headers
endif


ifeq ($(subdir),misc)
sysdep_headers += \
  sys/mtio.h \
  # sysdep_headers
endif


ifeq ($(subdir),csu)
routines += \
  unwind-resume \
  # routines
shared-only-routines += \
  unwind-resume \
  # shared-only-routines
CFLAGS-unwind-resume.c += -fexceptions -fasynchronous-unwind-tables
endif


ifeq ($(subdir),rt)
librt-sysdep_routines += \
  rt-unwind-resume \
  # librt-sysdep_routines
librt-shared-only-routines += \
  rt-unwind-resume \
  # librt-shared-only-routines
CFLAGS-rt-unwind-resume.c += -fexceptions -fasynchronous-unwind-tables
endif


ifeq ($(subdir),sysvipc)
sysdep_headers += \
  bits/ipc-perm.h \
  bits/types/struct_msqid_ds.h \
  bits/types/struct_shmid_ds.h
  # sysdep_headers
endif

ifeq ($(subdir),socket)
sysdep_headers += net/if_arp.h
endif
