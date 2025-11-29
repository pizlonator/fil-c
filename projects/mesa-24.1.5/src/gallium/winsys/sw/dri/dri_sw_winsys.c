/**************************************************************************
 *
 * Copyright 2009, VMware, Inc.
 * All Rights Reserved.
 * Copyright 2010 George Sapountzis <gsapountzis@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifdef HAVE_SYS_SHM_H
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef __FreeBSD__
/* sys/ipc.h -> sys/_types.h -> machine/param.h
 * - defines ALIGN which clashes with our ALIGN
 */
#undef ALIGN
#endif
#endif

#include "util/compiler.h"
#include "util/format/u_formats.h"
#include "util/detect_os.h"

#if DETECT_OS_UNIX
# include <sys/stat.h>
# include <errno.h>
# include <sys/mman.h>
#endif

#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/format/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/os_file.h"

#include "frontend/sw_winsys.h"
#include "dri_sw_winsys.h"


struct dri_sw_displaytarget
{
   enum pipe_format format;
   unsigned width;
   unsigned height;
   unsigned stride;

   unsigned map_flags;
   int shmid;
   void *data;
   void *mapped;
   const void *front_private;
   /* dmabuf */
   int fd;
   int offset;
   size_t size;
   bool unbacked;
};

struct dri_sw_winsys
{
   struct sw_winsys base;

   const struct drisw_loader_funcs *lf;
};

static inline struct dri_sw_displaytarget *
dri_sw_displaytarget( struct sw_displaytarget *dt )
{
   return (struct dri_sw_displaytarget *)dt;
}

static inline struct dri_sw_winsys *
dri_sw_winsys( struct sw_winsys *ws )
{
   return (struct dri_sw_winsys *)ws;
}


static bool
dri_sw_is_displaytarget_format_supported( struct sw_winsys *ws,
                                          unsigned tex_usage,
                                          enum pipe_format format )
{
   /* TODO: check visuals or other sensible thing here */
   return true;
}

#ifdef HAVE_SYS_SHM_H
static char *
alloc_shm(struct dri_sw_displaytarget *dri_sw_dt, unsigned size)
{
   char *addr;

   /* 0600 = user read+write */
   dri_sw_dt->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
   if (dri_sw_dt->shmid < 0)
      return NULL;

   addr = (char *) shmat(dri_sw_dt->shmid, NULL, 0);
   /* mark the segment immediately for deletion to avoid leaks */
   shmctl(dri_sw_dt->shmid, IPC_RMID, NULL);

   if (addr == (char *) -1)
      return NULL;

   return addr;
}
#endif

static struct sw_displaytarget *
dri_sw_displaytarget_create(struct sw_winsys *winsys,
                            unsigned tex_usage,
                            enum pipe_format format,
                            unsigned width, unsigned height,
                            unsigned alignment,
                            const void *front_private,
                            unsigned *stride)
{
   UNUSED struct dri_sw_winsys *ws = dri_sw_winsys(winsys);
   struct dri_sw_displaytarget *dri_sw_dt;
   unsigned nblocksy, size, format_stride;

   dri_sw_dt = CALLOC_STRUCT(dri_sw_displaytarget);
   if(!dri_sw_dt)
      goto no_dt;

   dri_sw_dt->format = format;
   dri_sw_dt->width = width;
   dri_sw_dt->height = height;
   dri_sw_dt->front_private = front_private;

   format_stride = util_format_get_stride(format, width);
   dri_sw_dt->stride = align(format_stride, alignment);

   nblocksy = util_format_get_nblocksy(format, height);
   size = dri_sw_dt->stride * nblocksy;
   dri_sw_dt->size = size;

   dri_sw_dt->shmid = -1;
   dri_sw_dt->fd = -1;

#ifdef HAVE_SYS_SHM_H
   if (ws->lf->put_image_shm)
      dri_sw_dt->data = alloc_shm(dri_sw_dt, size);
#endif

   if(!dri_sw_dt->data)
      dri_sw_dt->data = align_malloc(size, alignment);

   if(!dri_sw_dt->data)
      goto no_data;

   *stride = dri_sw_dt->stride;
   return (struct sw_displaytarget *)dri_sw_dt;

no_data:
   FREE(dri_sw_dt);
no_dt:
   return NULL;
}

static struct sw_displaytarget *
dri_sw_displaytarget_create_mapped(struct sw_winsys *winsys,
                                   unsigned tex_usage,
                                   enum pipe_format format,
                                   unsigned width, unsigned height,
                                   unsigned stride,
                                   void *data)
{
   UNUSED struct dri_sw_winsys *ws = dri_sw_winsys(winsys);
   struct dri_sw_displaytarget *dri_sw_dt;
   unsigned nblocksy, size;

   dri_sw_dt = CALLOC_STRUCT(dri_sw_displaytarget);
   if(!dri_sw_dt)
      return NULL;

   dri_sw_dt->format = format;
   dri_sw_dt->width = width;
   dri_sw_dt->height = height;

   dri_sw_dt->stride = stride;

   nblocksy = util_format_get_nblocksy(format, height);
   size = dri_sw_dt->stride * nblocksy;
   dri_sw_dt->size = size;

   dri_sw_dt->shmid = -1;
   dri_sw_dt->fd = -1;
   dri_sw_dt->unbacked = true;

   dri_sw_dt->data = data;
   dri_sw_dt->mapped = data;

   return (struct sw_displaytarget *)dri_sw_dt;
}

static void
dri_sw_displaytarget_destroy(struct sw_winsys *ws,
                             struct sw_displaytarget *dt)
{
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(dt);

   if (dri_sw_dt->unbacked) {}
   else if (dri_sw_dt->fd >= 0) {
      if (dri_sw_dt->mapped)
         ws->displaytarget_unmap(ws, dt);
      close(dri_sw_dt->fd);
   } else if (dri_sw_dt->shmid >= 0) {
#ifdef HAVE_SYS_SHM_H
      shmdt(dri_sw_dt->data);
      shmctl(dri_sw_dt->shmid, IPC_RMID, NULL);
#endif
   } else {
      align_free(dri_sw_dt->data);
   }

   FREE(dri_sw_dt);
}

static void *
dri_sw_displaytarget_map(struct sw_winsys *ws,
                         struct sw_displaytarget *dt,
                         unsigned flags)
{
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(dt);
   dri_sw_dt->map_flags = flags;
   if (dri_sw_dt->unbacked)
      return dri_sw_dt->mapped;
#if DETECT_OS_UNIX
   if (dri_sw_dt->fd > -1) {
      bool success = false;
      if (!success) {
         /* if this fails, it's a dmabuf that wasn't exported by us,
          * so it doesn't have the header that we're looking for
          */
         off_t size = lseek(dri_sw_dt->fd, 0, SEEK_END);
         lseek(dri_sw_dt->fd, 0, SEEK_SET);
         if (size < 1) {
            fprintf(stderr, "dmabuf import failed: fd has no data\n");
            return NULL;
         }
         unsigned prot = 0;
         if (flags & PIPE_MAP_READ)
            prot |= PROT_READ;
         if (flags & PIPE_MAP_WRITE)
            prot |= PROT_WRITE;
         dri_sw_dt->size = size;
         dri_sw_dt->data = mmap(NULL, dri_sw_dt->size, prot, MAP_SHARED, dri_sw_dt->fd, 0);
         if (dri_sw_dt->data == MAP_FAILED) {
            dri_sw_dt->data = NULL;
            fprintf(stderr, "dmabuf import failed to mmap: %s\n", strerror(errno));
         } else
            dri_sw_dt->mapped = ((uint8_t*)dri_sw_dt->data) + dri_sw_dt->offset;
      } else
         dri_sw_dt->mapped = ((uint8_t*)dri_sw_dt->data) + dri_sw_dt->offset;
   } else
#endif
   if (dri_sw_dt->front_private && (flags & PIPE_MAP_READ)) {
      struct dri_sw_winsys *dri_sw_ws = dri_sw_winsys(ws);
      dri_sw_ws->lf->get_image((void *)dri_sw_dt->front_private, 0, 0, dri_sw_dt->width, dri_sw_dt->height, dri_sw_dt->stride, dri_sw_dt->data);
      dri_sw_dt->mapped = dri_sw_dt->data;
   } else {
      dri_sw_dt->mapped = dri_sw_dt->data;
   }
   return dri_sw_dt->mapped;
}

static void
dri_sw_displaytarget_unmap(struct sw_winsys *ws,
                           struct sw_displaytarget *dt)
{
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(dt);

   if (dri_sw_dt->unbacked) {
      dri_sw_dt->map_flags = 0;
      return;
   }
#if DETECT_OS_UNIX
   if (dri_sw_dt->fd > -1) {
      munmap(dri_sw_dt->data, dri_sw_dt->size);
      dri_sw_dt->data = NULL;
   } else
#endif
   if (dri_sw_dt->front_private && (dri_sw_dt->map_flags & PIPE_MAP_WRITE)) {
      struct dri_sw_winsys *dri_sw_ws = dri_sw_winsys(ws);
      dri_sw_ws->lf->put_image2((void *)dri_sw_dt->front_private, dri_sw_dt->data, 0, 0, dri_sw_dt->width, dri_sw_dt->height, dri_sw_dt->stride);
   }
   dri_sw_dt->map_flags = 0;
   dri_sw_dt->mapped = NULL;
}

static struct sw_displaytarget *
dri_sw_displaytarget_from_handle(struct sw_winsys *winsys,
                                 const struct pipe_resource *templ,
                                 struct winsys_handle *whandle,
                                 unsigned *stride)
{
#if DETECT_OS_UNIX
   int fd = os_dupfd_cloexec(whandle->handle);
   struct sw_displaytarget *sw = dri_sw_displaytarget_create(winsys, templ->usage, templ->format, templ->width0, templ->height0, 64, NULL, stride);
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(sw);
   dri_sw_dt->fd = fd;
   dri_sw_dt->offset = whandle->offset;
   return sw;
#else
   assert(0);
   return NULL;
#endif
}

static bool
dri_sw_displaytarget_get_handle(struct sw_winsys *winsys,
                                struct sw_displaytarget *dt,
                                struct winsys_handle *whandle)
{
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(dt);

   if (whandle->type == WINSYS_HANDLE_TYPE_SHMID) {
      if (dri_sw_dt->shmid < 0)
         return false;
      whandle->handle = dri_sw_dt->shmid;
      return true;
   }

   return false;
}

static void
dri_sw_displaytarget_display(struct sw_winsys *ws,
                             struct sw_displaytarget *dt,
                             void *context_private,
                             unsigned nboxes,
                             struct pipe_box *box)
{
   struct dri_sw_winsys *dri_sw_ws = dri_sw_winsys(ws);
   struct dri_sw_displaytarget *dri_sw_dt = dri_sw_displaytarget(dt);
   struct dri_drawable *dri_drawable = (struct dri_drawable *)context_private;
   unsigned width, height, x = 0, y = 0;
   unsigned blsize = util_format_get_blocksize(dri_sw_dt->format);
   bool is_shm = dri_sw_dt->shmid != -1;
   /* Set the width to 'stride / cpp'.
    *
    * PutImage correctly clips to the width of the dst drawable.
    */
   if (!nboxes) {
      width = dri_sw_dt->stride / blsize;
      height = dri_sw_dt->height;
      if (is_shm)
         dri_sw_ws->lf->put_image_shm(dri_drawable, dri_sw_dt->shmid, dri_sw_dt->data, 0, 0,
                                    0, 0, width, height, dri_sw_dt->stride);
      else
         dri_sw_ws->lf->put_image(dri_drawable, dri_sw_dt->data, width, height);
      return;
   }
   for (unsigned i = 0; i < nboxes; i++) {
      unsigned offset = dri_sw_dt->stride * box[i].y;
      unsigned offset_x = box[i].x * blsize;
      char *data = dri_sw_dt->data + offset;
      x = box[i].x;
      y = box[i].y;
      width = box[i].width;
      height = box[i].height;
      if (is_shm) {
         /* don't add x offset for shm, the put_image_shm will deal with it */
         dri_sw_ws->lf->put_image_shm(dri_drawable, dri_sw_dt->shmid, dri_sw_dt->data, offset, offset_x,
                                      x, y, width, height, dri_sw_dt->stride);
      } else {
         data += offset_x;
         dri_sw_ws->lf->put_image2(dri_drawable, data,
                                   x, y, width, height, dri_sw_dt->stride);
      }
   }
}

static void
dri_destroy_sw_winsys(struct sw_winsys *winsys)
{
   FREE(winsys);
}

struct sw_winsys *
dri_create_sw_winsys(const struct drisw_loader_funcs *lf)
{
   struct dri_sw_winsys *ws;

   ws = CALLOC_STRUCT(dri_sw_winsys);
   if (!ws)
      return NULL;

   ws->lf = lf;
   ws->base.destroy = dri_destroy_sw_winsys;

   ws->base.is_displaytarget_format_supported = dri_sw_is_displaytarget_format_supported;

   /* screen texture functions */
   ws->base.displaytarget_create = dri_sw_displaytarget_create;
   ws->base.displaytarget_create_mapped = dri_sw_displaytarget_create_mapped;
   ws->base.displaytarget_destroy = dri_sw_displaytarget_destroy;
   ws->base.displaytarget_from_handle = dri_sw_displaytarget_from_handle;
   ws->base.displaytarget_get_handle = dri_sw_displaytarget_get_handle;

   /* texture functions */
   ws->base.displaytarget_map = dri_sw_displaytarget_map;
   ws->base.displaytarget_unmap = dri_sw_displaytarget_unmap;

   ws->base.displaytarget_display = dri_sw_displaytarget_display;

   return &ws->base;
}

/* vim: set sw=3 ts=8 sts=3 expandtab: */
