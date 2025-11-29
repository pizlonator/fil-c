/*
 * Copyright © 2014 Broadcom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <unistd.h>
#include <fcntl.h>

#include "util/os_file.h"
#include "util/u_screen.h"

#include "v3d_drm_public.h"

#include "v3d/v3d_screen.h"

struct pipe_screen *
v3d_drm_screen_create(int fd, const struct pipe_screen_config *config)
{
   return u_pipe_screen_lookup_or_create(os_dupfd_cloexec(fd), config,
                                         NULL, v3d_screen_create);
}

struct pipe_screen *
v3d_drm_screen_create_renderonly(int fd, struct renderonly *ro,
                                 const struct pipe_screen_config *config)
{
   return u_pipe_screen_lookup_or_create(fd, config,
                                         ro, v3d_screen_create);
}
