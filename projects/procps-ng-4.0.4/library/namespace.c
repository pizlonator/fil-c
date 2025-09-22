/*
 * namespace.c - Library API for Linux namespaces
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "misc.h"
#include "procps-private.h"

#define NSPATHLEN 64

static const char *ns_names[] = {
    [PROCPS_NS_CGROUP] = "cgroup",
    [PROCPS_NS_IPC]    = "ipc",
    [PROCPS_NS_MNT]    = "mnt",
    [PROCPS_NS_NET]    = "net",
    [PROCPS_NS_PID]    = "pid",
    [PROCPS_NS_TIME]   = "time",
    [PROCPS_NS_USER]   = "user",
    [PROCPS_NS_UTS]    = "uts"
};


/*
 * procps_ns_get_name:
 *
 * Find the name of the namespace with the given ID
 *
 * @id: The ID of the required namespace, see
 * namespace_type
 *
 * Returns: static string of the namespace
 */
PROCPS_EXPORT const char *procps_ns_get_name(const int id)
{
    if (id >= PROCPS_NS_COUNT || id < 0)
        return NULL;
    return ns_names[id];
}

/*
 * procps_ns_get_id:
 *
 * Find the namespace ID that matches the given
 * name.
 *
 * @name: the name of the required namespace
 *
 * Returns: ID of found name
 *   < 0 means error
 */
PROCPS_EXPORT int procps_ns_get_id(const char *name)
{
    int i;

    if (name == NULL)
        return -EINVAL;
    for (i=0; i < PROCPS_NS_COUNT; i++)
        if (!strcmp(ns_names[i], name))
            return i;
    return -EINVAL;
}

/*
 * procs_ns_read_pid:
 *
 * Find all namespaces for the given process.
 * @pid: Process ID for required process
 * @nsp: Pointer to the struct procps_ns
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
PROCPS_EXPORT int procps_ns_read_pid(
        const int pid,
        struct procps_ns *nsp)
{
    char path[NSPATHLEN+1];
    struct stat st;
    int i;

    if (nsp == NULL)
        return -EINVAL;
    if (pid < 1)
        return -EINVAL;

    for (i=0; i < PROCPS_NS_COUNT; i++) {
        snprintf(path, NSPATHLEN, "/proc/%d/ns/%s", pid, ns_names[i]);
        if (0 == stat(path, &st))
            nsp->ns[i] = (unsigned long)st.st_ino;
        else
            nsp->ns[i] = 0;
    }
    return 0;
}
