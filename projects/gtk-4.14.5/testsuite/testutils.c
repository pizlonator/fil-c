/*
 * Copyright © 2021 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "testsuite/testutils.h"

char *
diff_with_file (const char  *file1,
                const char  *text,
                gssize       len,
                GError     **error)
{
  char *diff_cmd, *diff, *tmpfile;
  int fd;

  diff = NULL;

  diff_cmd = g_find_program_in_path ("diff");
  if (diff_cmd)
    {
      const char *command[] = { diff_cmd, "-u", file1, NULL, NULL };

      if (len < 0)
        len = strlen (text);

      /* write the text buffer to a temporary file */
      fd = g_file_open_tmp (NULL, &tmpfile, error);
      if (fd < 0)
        return NULL;

      if (write (fd, text, len) != (int) len)
        {
          close (fd);
          g_set_error (error,
                       G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "Could not write data to temporary file '%s'", tmpfile);
          goto done;
        }
      close (fd);
      command[3] = tmpfile;

      /* run diff command */
      g_spawn_sync (NULL,
                    (char **) command,
                    NULL,
                    0,
                    NULL, NULL,
                    &diff,
                    NULL, NULL,
                    error);

done:
      g_unlink (tmpfile);
      g_free (tmpfile);
      g_free (diff_cmd);
    }
  else
    {
      char *buf1;
      gsize len1;

      if (!g_file_get_contents (file1, &buf1, &len1, error))
        return NULL;

      if ((len != -1 && len != len1) ||
          strncmp (text, buf1, len1) != 0)
        diff = g_strdup ("Files differ.\n");

      g_free (buf1);
    }

  return diff;
}
