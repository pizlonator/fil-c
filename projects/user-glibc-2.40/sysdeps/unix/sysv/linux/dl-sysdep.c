/* Dynamic linker system dependencies for Linux.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <_itoa.h>
#include <assert.h>
#include <dl-auxv.h>
#include <dl-osinfo.h>
#include <dl-parse_auxv.h>
#include <dl-procinfo.h>
#include <dl-tunables.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <ldsodefs.h>
#include <libc-internal.h>
#include <libintl.h>
#include <not-cancel.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <tls.h>
#include <unistd.h>
#include <dl-symbol-redir-ifunc.h>

#include <dl-machine.h>
#include <dl-hwcap-check.h>

