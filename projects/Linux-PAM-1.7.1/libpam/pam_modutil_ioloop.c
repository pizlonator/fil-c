/*
 * $Id$
 *
 * These functions provide common methods to ensure a complete read or
 * write occurs. They handle EINTR and partial read/write returns.
 */

#include "pam_modutil_private.h"

#include <unistd.h>
#include <errno.h>

int
pam_modutil_read(int fd, char *buffer, int count)
{
       int block, offset = 0;

       if (count < 0) {
               errno = EINVAL;
               return -1;
       }

       while (count > 0) {
               block = read(fd, &buffer[offset], count);

               if (block < 0) {
                       if (errno == EINTR) continue;
                       return block;
               }
               if (block == 0) return offset;

               offset += block;
               count -= block;
       }

       return offset;
}

int
pam_modutil_write(int fd, const char *buffer, int count)
{
       int block, offset = 0;

       if (count < 0) {
               errno = EINVAL;
               return -1;
       }

       while (count > 0) {
               block = write(fd, &buffer[offset], count);

               if (block < 0) {
                       if (errno == EINTR) continue;
                       return block;
               }
               if (block == 0) return offset;

               offset += block;
               count -= block;
       }

       return offset;
}
