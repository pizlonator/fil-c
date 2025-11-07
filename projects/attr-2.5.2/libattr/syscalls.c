/*
  Copyright (C) 2015  Dmitry V. Levin <ldv@altlinux.org>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * These dumb wrappers are for backwards compatibility only.
 * Actual syscall wrappers are long gone to libc.
 */

#include "config.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/xattr.h>

/*
 * Versioning of compat symbols:
 * prefer symver attribute if available (since gcc 10),
 * fall back to traditional .symver asm directive otherwise.
 */
#ifdef __has_attribute
# if __has_attribute(__symver__)
#  define SYMVER(cn, vn) __typeof(cn) cn __attribute__((__symver__(vn)))
# elif __has_attribute(__no_reorder__)
   /*
    * Avoid wrong partitioning with older gcc and LTO. May not work reliably
    * with all versions; use -flto-partition=none if you encounter problems.
    */
#  define SYMVER(cn, vn) __typeof(cn) cn __attribute__((__no_reorder__)); \
			 __asm__(".symver " #cn "," vn)
# endif
#endif
#ifndef SYMVER
#  define SYMVER(cn, vn) __asm__(".symver " #cn "," vn)
#endif

#ifdef HAVE_VISIBILITY_ATTRIBUTE
# pragma GCC visibility push(default)
#endif

int libattr_setxattr(const char *path, const char *name,
		     void *value, size_t size, int flags)
{
	return syscall(__NR_setxattr, path, name, value, size, flags);
}
SYMVER(libattr_setxattr, "setxattr@ATTR_1.0");

int libattr_lsetxattr(const char *path, const char *name,
		      void *value, size_t size, int flags)
{
	return syscall(__NR_lsetxattr, path, name, value, size, flags);
}
SYMVER(libattr_lsetxattr, "lsetxattr@ATTR_1.0");

int libattr_fsetxattr(int filedes, const char *name,
		      void *value, size_t size, int flags)
{
	return syscall(__NR_fsetxattr, filedes, name, value, size, flags);
}
SYMVER(libattr_fsetxattr, "fsetxattr@ATTR_1.0");

ssize_t libattr_getxattr(const char *path, const char *name,
			 void *value, size_t size)
{
	return syscall(__NR_getxattr, path, name, value, size);
}
SYMVER(libattr_getxattr, "getxattr@ATTR_1.0");

ssize_t libattr_lgetxattr(const char *path, const char *name,
			  void *value, size_t size)
{
	return syscall(__NR_lgetxattr, path, name, value, size);
}
SYMVER(libattr_lgetxattr, "lgetxattr@ATTR_1.0");

ssize_t libattr_fgetxattr(int filedes, const char *name,
			  void *value, size_t size)
{
	return syscall(__NR_fgetxattr, filedes, name, value, size);
}
SYMVER(libattr_fgetxattr, "fgetxattr@ATTR_1.0");

ssize_t libattr_listxattr(const char *path, char *list, size_t size)
{
	return syscall(__NR_listxattr, path, list, size);
}
SYMVER(libattr_listxattr, "listxattr@ATTR_1.0");

ssize_t libattr_llistxattr(const char *path, char *list, size_t size)
{
	return syscall(__NR_llistxattr, path, list, size);
}
SYMVER(libattr_llistxattr, "llistxattr@ATTR_1.0");

ssize_t libattr_flistxattr(int filedes, char *list, size_t size)
{
	return syscall(__NR_flistxattr, filedes, list, size);
}
SYMVER(libattr_flistxattr, "flistxattr@ATTR_1.0");

int libattr_removexattr(const char *path, const char *name)
{
	return syscall(__NR_removexattr, path, name);
}
SYMVER(libattr_removexattr, "removexattr@ATTR_1.0");

int libattr_lremovexattr(const char *path, const char *name)
{
	return syscall(__NR_lremovexattr, path, name);
}
SYMVER(libattr_lremovexattr, "lremovexattr@ATTR_1.0");

int libattr_fremovexattr(int filedes, const char *name)
{
	return syscall(__NR_fremovexattr, filedes, name);
}
SYMVER(libattr_fremovexattr, "fremovexattr@ATTR_1.0");

#ifdef HAVE_VISIBILITY_ATTRIBUTE
# pragma GCC visibility pop
#endif
