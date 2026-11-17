/*
 * Copyright © 2013 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "vteutils.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>

/* Temporary define until glibc release catches up */
#ifdef __linux__

#include <sys/ioctl.h>
#include <linux/fs.h>

#endif /* __linux__ */

#include "missing.hh"

int
_vte_mkstemp (void)
{
        int fd;
        gchar *file_name;

#ifdef O_TMPFILE
        fd = open (g_get_tmp_dir (),
                   O_TMPFILE | O_EXCL | O_RDWR | O_NOATIME | O_CLOEXEC,
                   0600);
        if (fd != -1)
                goto done;

        /* Try again with g_file_open_tmp */
#endif

        fd = g_file_open_tmp ("vteXXXXXX", &file_name, NULL);
        if (fd == -1)
                return -1;

        unlink (file_name);
        g_free (file_name);

#ifdef O_NOATIME
        do { } while (fcntl (fd, F_SETFL, O_NOATIME) == -1 && errno == EINTR);
#endif

#ifdef O_TMPFILE
 done:
#endif

#ifdef __linux__
{
        /* Mark the tmpfile as no-cow on file systems that support it.
         *
         * (Note that the definition of the ioctls make you think @flags would
         * be long instead of int, but it turns out that this is not the case;
         * see http://lwn.net/Articles/575846/ ).
         */
        int flags;

        if (ioctl (fd, FS_IOC_GETFLAGS, &flags) == 0) {
                flags |= FS_SECRM_FL | FS_NOATIME_FL | FS_NOCOMP_FL | FS_NOCOW_FL | FS_NODUMP_FL;

                ioctl (fd, FS_IOC_SETFLAGS, &flags);
        }
}
#endif /* __linux__ */

        return fd;
}
