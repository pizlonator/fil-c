/* Support functions for nscd testing.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* The nscd support functionality assumes that a container test is
   used.  The container script must contain a su directive so that the
   container runs with UID 0.  */

#ifndef SUPPORT_NSCD_TEST_H
#define SUPPORT_NSCD_TEST_H

/* Copy the default configuration file to /etc/nscd.conf.  */
void support_nscd_copy_configuration (void);

/* Start nscd in foreground mode.  Terminates the test on failure.  */
void support_nscd_start (void);

/* Stop nscd.  */
void support_nscd_stop (void);

/* Invalidate the specified database (group, hosts, netgroup, passwd,
   services).  */
void support_nscd_invalidate (const char *database);

#endif /* SUPPORT_NSCD_TEST_H */
