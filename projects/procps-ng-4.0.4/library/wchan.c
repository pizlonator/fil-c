/*
 * wchan.c - kernel symbol handling
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 1998-2003 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wchan.h"  // to verify prototype


const char *lookup_wchan (int pid) {
   static __thread char buf[64];
   const char *ret = buf;
   ssize_t num;
   int fd;

   snprintf(buf, sizeof buf, "/proc/%d/wchan", pid);
   fd = open(buf, O_RDONLY);
   if (fd==-1) return "?";

   num = read(fd, buf, sizeof buf - 1);
   close(fd);

   if (num<1) return "?"; // allow for "0"
   buf[num] = '\0';

   if (buf[0]=='0' && buf[1]=='\0') return "-";

   // lame ppc64 has a '.' in front of every name
   if (*ret=='.') ret++;
   while(*ret=='_') ret++;

   return ret;
}
