/*
 * misc.h - miscellaneous declarations for libproc2
 *
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2021-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 1998-2003 Albert Cahalan
 * Copyright © 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Copyright © 1996      Charles Blake <cblake@bbn.com>
 * Copyright © 1993      J. Cowley
 * Copyright © 1995      Martin Schulze <joey@infodrom.north.de>
 * Copyright © ????      Larry Greenfield <greenfie@gauss.rutgers.edu>
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

#ifndef PROC_MISC_H
#define PROC_MISC_H
#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif


// //////////////////////////////////////////////////////////////////
// Platform Particulars /////////////////////////////////////////////

long procps_cpu_count (void);
long procps_hertz_get (void);
unsigned int procps_pid_length (void);

   // Convenience macros for composing/decomposing version codes
#define LINUX_VERSION(x,y,z)   (0x10000*((x)&0x7fff) + 0x100*((y)&0xff) + ((z)&0xff))
#define LINUX_VERSION_MAJOR(x) (((x)>>16) & 0xFF)
#define LINUX_VERSION_MINOR(x) (((x)>> 8) & 0xFF)
#define LINUX_VERSION_PATCH(x) ( (x)      & 0xFF)

int procps_linux_version(void);


// //////////////////////////////////////////////////////////////////
// Runtime Particulars //////////////////////////////////////////////

int   procps_loadavg (double *av1, double *av5, double *av15);
int   procps_uptime (double *uptime_secs, double *idle_secs);
char *procps_uptime_sprint (void);
char *procps_uptime_sprint_short (void);


// //////////////////////////////////////////////////////////////////
// Namespace Particulars ////////////////////////////////////////////

enum namespace_type {
    PROCPS_NS_CGROUP,
    PROCPS_NS_IPC,
    PROCPS_NS_MNT,
    PROCPS_NS_NET,
    PROCPS_NS_PID,
    PROCPS_NS_TIME,
    PROCPS_NS_USER,
    PROCPS_NS_UTS,
    PROCPS_NS_COUNT  // total namespaces (fencepost)
};

struct procps_ns {
    unsigned long ns[PROCPS_NS_COUNT];
};

const char *procps_ns_get_name (const int id);
int procps_ns_get_id (const char *name);
int procps_ns_read_pid (const int pid, struct procps_ns *nsp);


#ifdef __cplusplus
}
#endif
#endif
