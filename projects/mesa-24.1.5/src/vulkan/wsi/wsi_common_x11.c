/*
 * Copyright © 2015 Intel Corporation
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

#include <X11/Xlib-xcb.h>
#include <X11/xshmfence.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <xcb/xcb.h>
#ifdef XCB_KEYSYMS_AVAILABLE
#include <xcb/xcb_keysyms.h>
#endif
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/shm.h>

#include "util/macros.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <xf86drm.h>
#include "drm-uapi/drm_fourcc.h"
#include "util/hash_table.h"
#include "util/mesa-blake3.h"
#include "util/os_file.h"
#include "util/os_time.h"
#include "util/u_debug.h"
#include "util/u_thread.h"
#include "util/xmlconfig.h"
#include "util/timespec.h"

#include "vk_format.h"
#include "vk_instance.h"
#include "vk_physical_device.h"
#include "vk_device.h"
#include "vk_util.h"
#include "vk_enum_to_str.h"
#include "wsi_common_entrypoints.h"
#include "wsi_common_private.h"
#include "wsi_common_queue.h"

#ifdef HAVE_SYS_SHM_H
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#ifndef XCB_PRESENT_OPTION_ASYNC_MAY_TEAR
#define XCB_PRESENT_OPTION_ASYNC_MAY_TEAR 16
#endif
#ifndef XCB_PRESENT_CAPABILITY_ASYNC_MAY_TEAR
#define XCB_PRESENT_CAPABILITY_ASYNC_MAY_TEAR 8
#endif

struct wsi_x11_connection {
   bool has_dri3;
   bool has_dri3_modifiers;
   bool has_dri3_explicit_sync;
   bool has_present;
   bool is_proprietary_x11;
   bool is_xwayland;
   bool has_mit_shm;
   bool has_xfixes;
};

struct wsi_x11 {
   struct wsi_interface base;

   pthread_mutex_t                              mutex;
   /* Hash table of xcb_connection -> wsi_x11_connection mappings */
   struct hash_table *connections;
};

struct wsi_x11_vk_surface {
   union {
      VkIcdSurfaceXlib xlib;
      VkIcdSurfaceXcb xcb;
   };
   bool has_alpha;
};

/**
 * Wrapper around xcb_dri3_open. Returns the opened fd or -1 on error.
 */
static int
wsi_dri3_open(xcb_connection_t *conn,
	      xcb_window_t root,
	      uint32_t provider)
{
   xcb_dri3_open_cookie_t       cookie;
   xcb_dri3_open_reply_t        *reply;
   int                          fd;

   cookie = xcb_dri3_open(conn,
                          root,
                          provider);

   reply = xcb_dri3_open_reply(conn, cookie, NULL);
   if (!reply)
      return -1;

   /* According to DRI3 extension nfd must equal one. */
   if (reply->nfd != 1) {
      free(reply);
      return -1;
   }

   fd = xcb_dri3_open_reply_fds(conn, reply)[0];
   free(reply);
   fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

   return fd;
}

/**
 * Checks compatibility of the device wsi_dev with the device the X server
 * provides via DRI3.
 *
 * This returns true when no device could be retrieved from the X server or when
 * the information for the X server device indicate that it is the same device.
 */
static bool
wsi_x11_check_dri3_compatible(const struct wsi_device *wsi_dev,
                              xcb_connection_t *conn)
{
   xcb_screen_iterator_t screen_iter =
      xcb_setup_roots_iterator(xcb_get_setup(conn));
   xcb_screen_t *screen = screen_iter.data;

   /* Open the DRI3 device from the X server. If we do not retrieve one we
    * assume our local device is compatible.
    */
   int dri3_fd = wsi_dri3_open(conn, screen->root, None);
   if (dri3_fd == -1)
      return true;

   bool match = wsi_device_matches_drm_fd(wsi_dev, dri3_fd);

   close(dri3_fd);

   return match;
}

static bool
wsi_x11_detect_xwayland(xcb_connection_t *conn,
                        xcb_query_extension_reply_t *randr_reply,
                        xcb_query_extension_reply_t *xwl_reply)
{
   /* Newer Xwayland exposes an X11 extension we can check for */
   if (xwl_reply && xwl_reply->present)
      return true;

   /* Older Xwayland uses the word "XWAYLAND" in the RandR output names */
   if (!randr_reply || !randr_reply->present)
      return false;

   xcb_randr_query_version_cookie_t ver_cookie =
      xcb_randr_query_version_unchecked(conn, 1, 3);
   xcb_randr_query_version_reply_t *ver_reply =
      xcb_randr_query_version_reply(conn, ver_cookie, NULL);
   bool has_randr_v1_3 = ver_reply && (ver_reply->major_version > 1 ||
                                       ver_reply->minor_version >= 3);
   free(ver_reply);

   if (!has_randr_v1_3)
      return false;

   const xcb_setup_t *setup = xcb_get_setup(conn);
   xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

   xcb_randr_get_screen_resources_current_cookie_t gsr_cookie =
      xcb_randr_get_screen_resources_current_unchecked(conn, iter.data->root);
   xcb_randr_get_screen_resources_current_reply_t *gsr_reply =
      xcb_randr_get_screen_resources_current_reply(conn, gsr_cookie, NULL);

   if (!gsr_reply || gsr_reply->num_outputs == 0) {
      free(gsr_reply);
      return false;
   }

   xcb_randr_output_t *randr_outputs =
      xcb_randr_get_screen_resources_current_outputs(gsr_reply);
   xcb_randr_get_output_info_cookie_t goi_cookie =
      xcb_randr_get_output_info(conn, randr_outputs[0], gsr_reply->config_timestamp);
   free(gsr_reply);

   xcb_randr_get_output_info_reply_t *goi_reply =
      xcb_randr_get_output_info_reply(conn, goi_cookie, NULL);
   if (!goi_reply) {
      return false;
   }

   char *output_name = (char*)xcb_randr_get_output_info_name(goi_reply);
   bool is_xwayland = output_name && strncmp(output_name, "XWAYLAND", 8) == 0;
   free(goi_reply);

   return is_xwayland;
}

static struct wsi_x11_connection *
wsi_x11_connection_create(struct wsi_device *wsi_dev,
                          xcb_connection_t *conn)
{
   xcb_query_extension_cookie_t dri3_cookie, pres_cookie, randr_cookie,
                                amd_cookie, nv_cookie, shm_cookie, sync_cookie,
                                xfixes_cookie, xwl_cookie;
   xcb_query_extension_reply_t *dri3_reply, *pres_reply, *randr_reply,
                               *amd_reply, *nv_reply, *shm_reply = NULL,
                               *xfixes_reply, *xwl_reply;
   bool wants_shm = wsi_dev->sw && !(WSI_DEBUG & WSI_DEBUG_NOSHM) &&
                    wsi_dev->has_import_memory_host;
   bool has_dri3_v1_2 = false;
   bool has_present_v1_2 = false;
   bool has_dri3_v1_4 = false;
   bool has_present_v1_4 = false;

   struct wsi_x11_connection *wsi_conn =
      vk_alloc(&wsi_dev->instance_alloc, sizeof(*wsi_conn), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!wsi_conn)
      return NULL;

   sync_cookie = xcb_query_extension(conn, 4, "SYNC");
   dri3_cookie = xcb_query_extension(conn, 4, "DRI3");
   pres_cookie = xcb_query_extension(conn, 7, "Present");
   randr_cookie = xcb_query_extension(conn, 5, "RANDR");
   xfixes_cookie = xcb_query_extension(conn, 6, "XFIXES");
   xwl_cookie = xcb_query_extension(conn, 8, "XWAYLAND");

   if (wants_shm)
      shm_cookie = xcb_query_extension(conn, 7, "MIT-SHM");

   /* We try to be nice to users and emit a warning if they try to use a
    * Vulkan application on a system without DRI3 enabled.  However, this ends
    * up spewing the warning when a user has, for example, both Intel
    * integrated graphics and a discrete card with proprietary drivers and are
    * running on the discrete card with the proprietary DDX.  In this case, we
    * really don't want to print the warning because it just confuses users.
    * As a heuristic to detect this case, we check for a couple of proprietary
    * X11 extensions.
    */
   amd_cookie = xcb_query_extension(conn, 11, "ATIFGLRXDRI");
   nv_cookie = xcb_query_extension(conn, 10, "NV-CONTROL");

   xcb_discard_reply(conn, sync_cookie.sequence);
   dri3_reply = xcb_query_extension_reply(conn, dri3_cookie, NULL);
   pres_reply = xcb_query_extension_reply(conn, pres_cookie, NULL);
   randr_reply = xcb_query_extension_reply(conn, randr_cookie, NULL);
   amd_reply = xcb_query_extension_reply(conn, amd_cookie, NULL);
   nv_reply = xcb_query_extension_reply(conn, nv_cookie, NULL);
   xfixes_reply = xcb_query_extension_reply(conn, xfixes_cookie, NULL);
   xwl_reply = xcb_query_extension_reply(conn, xwl_cookie, NULL);
   if (wants_shm)
      shm_reply = xcb_query_extension_reply(conn, shm_cookie, NULL);
   if (!dri3_reply || !pres_reply || !xfixes_reply) {
      free(dri3_reply);
      free(pres_reply);
      free(xfixes_reply);
      free(xwl_reply);
      free(randr_reply);
      free(amd_reply);
      free(nv_reply);
      if (wants_shm)
         free(shm_reply);
      vk_free(&wsi_dev->instance_alloc, wsi_conn);
      return NULL;
   }

   wsi_conn->has_dri3 = dri3_reply->present != 0;
#ifdef HAVE_DRI3_MODIFIERS
   if (wsi_conn->has_dri3) {
      xcb_dri3_query_version_cookie_t ver_cookie;
      xcb_dri3_query_version_reply_t *ver_reply;

      ver_cookie = xcb_dri3_query_version(conn, 1, 4);
      ver_reply = xcb_dri3_query_version_reply(conn, ver_cookie, NULL);
      has_dri3_v1_2 = ver_reply != NULL &&
         (ver_reply->major_version > 1 || ver_reply->minor_version >= 2);
      has_dri3_v1_4 = ver_reply != NULL &&
         (ver_reply->major_version > 1 || ver_reply->minor_version >= 4);
      free(ver_reply);
   }
#endif

   wsi_conn->has_present = pres_reply->present != 0;
#ifdef HAVE_DRI3_MODIFIERS
   if (wsi_conn->has_present) {
      xcb_present_query_version_cookie_t ver_cookie;
      xcb_present_query_version_reply_t *ver_reply;

      ver_cookie = xcb_present_query_version(conn, 1, 4);
      ver_reply = xcb_present_query_version_reply(conn, ver_cookie, NULL);
      has_present_v1_2 =
        (ver_reply->major_version > 1 || ver_reply->minor_version >= 2);
      has_present_v1_4 =
        (ver_reply->major_version > 1 || ver_reply->minor_version >= 4);
      free(ver_reply);
   }
#endif

   wsi_conn->has_xfixes = xfixes_reply->present != 0;
   if (wsi_conn->has_xfixes) {
      xcb_xfixes_query_version_cookie_t ver_cookie;
      xcb_xfixes_query_version_reply_t *ver_reply;

      ver_cookie = xcb_xfixes_query_version(conn, 6, 0);
      ver_reply = xcb_xfixes_query_version_reply(conn, ver_cookie, NULL);
      wsi_conn->has_xfixes = (ver_reply->major_version >= 2);
      free(ver_reply);
   }

   wsi_conn->is_xwayland = wsi_x11_detect_xwayland(conn, randr_reply,
                                                   xwl_reply);

   wsi_conn->has_dri3_modifiers = has_dri3_v1_2 && has_present_v1_2;
   wsi_conn->has_dri3_explicit_sync = has_dri3_v1_4 && has_present_v1_4;
   wsi_conn->is_proprietary_x11 = false;
   if (amd_reply && amd_reply->present)
      wsi_conn->is_proprietary_x11 = true;
   if (nv_reply && nv_reply->present)
      wsi_conn->is_proprietary_x11 = true;

   wsi_conn->has_mit_shm = false;
   if (wsi_conn->has_dri3 && wsi_conn->has_present && wants_shm) {
      bool has_mit_shm = shm_reply->present != 0;

      xcb_shm_query_version_cookie_t ver_cookie;
      xcb_shm_query_version_reply_t *ver_reply;

      ver_cookie = xcb_shm_query_version(conn);
      ver_reply = xcb_shm_query_version_reply(conn, ver_cookie, NULL);

      has_mit_shm = ver_reply->shared_pixmaps;
      free(ver_reply);
      xcb_void_cookie_t cookie;
      xcb_generic_error_t *error;

      if (has_mit_shm) {
         cookie = xcb_shm_detach_checked(conn, 0);
         if ((error = xcb_request_check(conn, cookie))) {
            if (error->error_code != BadRequest)
               wsi_conn->has_mit_shm = true;
            free(error);
         }
      }
   }

   free(dri3_reply);
   free(pres_reply);
   free(randr_reply);
   free(xwl_reply);
   free(amd_reply);
   free(nv_reply);
   free(xfixes_reply);
   if (wants_shm)
      free(shm_reply);

   return wsi_conn;
}

static void
wsi_x11_connection_destroy(struct wsi_device *wsi_dev,
                           struct wsi_x11_connection *conn)
{
   vk_free(&wsi_dev->instance_alloc, conn);
}

static bool
wsi_x11_check_for_dri3(struct wsi_x11_connection *wsi_conn)
{
  if (wsi_conn->has_dri3)
    return true;
  if (!wsi_conn->is_proprietary_x11) {
    fprintf(stderr, "vulkan: No DRI3 support detected - required for presentation\n"
                    "Note: you can probably enable DRI3 in your Xorg config\n");
  }
  return false;
}

/**
 * Get internal struct representing an xcb_connection_t.
 *
 * This can allocate the struct but the caller does not own the struct. It is
 * deleted on wsi_x11_finish_wsi by the hash table it is inserted.
 *
 * If the allocation fails NULL is returned.
 */
static struct wsi_x11_connection *
wsi_x11_get_connection(struct wsi_device *wsi_dev,
                       xcb_connection_t *conn)
{
   struct wsi_x11 *wsi =
      (struct wsi_x11 *)wsi_dev->wsi[VK_ICD_WSI_PLATFORM_XCB];

   pthread_mutex_lock(&wsi->mutex);

   struct hash_entry *entry = _mesa_hash_table_search(wsi->connections, conn);
   if (!entry) {
      /* We're about to make a bunch of blocking calls.  Let's drop the
       * mutex for now so we don't block up too badly.
       */
      pthread_mutex_unlock(&wsi->mutex);

      struct wsi_x11_connection *wsi_conn =
         wsi_x11_connection_create(wsi_dev, conn);
      if (!wsi_conn)
         return NULL;

      pthread_mutex_lock(&wsi->mutex);

      entry = _mesa_hash_table_search(wsi->connections, conn);
      if (entry) {
         /* Oops, someone raced us to it */
         wsi_x11_connection_destroy(wsi_dev, wsi_conn);
      } else {
         entry = _mesa_hash_table_insert(wsi->connections, conn, wsi_conn);
      }
   }

   pthread_mutex_unlock(&wsi->mutex);

   return entry->data;
}

static const VkFormat formats[] = {
   VK_FORMAT_R5G6B5_UNORM_PACK16,
   VK_FORMAT_B8G8R8A8_SRGB,
   VK_FORMAT_B8G8R8A8_UNORM,
   VK_FORMAT_A2R10G10B10_UNORM_PACK32,
};

static const VkPresentModeKHR present_modes[] = {
   VK_PRESENT_MODE_IMMEDIATE_KHR,
   VK_PRESENT_MODE_MAILBOX_KHR,
   VK_PRESENT_MODE_FIFO_KHR,
   VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};

static xcb_screen_t *
get_screen_for_root(xcb_connection_t *conn, xcb_window_t root)
{
   xcb_screen_iterator_t screen_iter =
      xcb_setup_roots_iterator(xcb_get_setup(conn));

   for (; screen_iter.rem; xcb_screen_next (&screen_iter)) {
      if (screen_iter.data->root == root)
         return screen_iter.data;
   }

   return NULL;
}

static xcb_visualtype_t *
screen_get_visualtype(xcb_screen_t *screen, xcb_visualid_t visual_id,
                      unsigned *depth)
{
   xcb_depth_iterator_t depth_iter =
      xcb_screen_allowed_depths_iterator(screen);

   for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
      xcb_visualtype_iterator_t visual_iter =
         xcb_depth_visuals_iterator (depth_iter.data);

      for (; visual_iter.rem; xcb_visualtype_next (&visual_iter)) {
         if (visual_iter.data->visual_id == visual_id) {
            if (depth)
               *depth = depth_iter.data->depth;
            return visual_iter.data;
         }
      }
   }

   return NULL;
}

static xcb_visualtype_t *
connection_get_visualtype(xcb_connection_t *conn, xcb_visualid_t visual_id)
{
   xcb_screen_iterator_t screen_iter =
      xcb_setup_roots_iterator(xcb_get_setup(conn));

   /* For this we have to iterate over all of the screens which is rather
    * annoying.  Fortunately, there is probably only 1.
    */
   for (; screen_iter.rem; xcb_screen_next (&screen_iter)) {
      xcb_visualtype_t *visual = screen_get_visualtype(screen_iter.data,
                                                       visual_id, NULL);
      if (visual)
         return visual;
   }

   return NULL;
}

static xcb_visualtype_t *
get_visualtype_for_window(xcb_connection_t *conn, xcb_window_t window,
                          unsigned *depth, xcb_visualtype_t **rootvis)
{
   xcb_query_tree_cookie_t tree_cookie;
   xcb_get_window_attributes_cookie_t attrib_cookie;
   xcb_query_tree_reply_t *tree;
   xcb_get_window_attributes_reply_t *attrib;

   tree_cookie = xcb_query_tree(conn, window);
   attrib_cookie = xcb_get_window_attributes(conn, window);

   tree = xcb_query_tree_reply(conn, tree_cookie, NULL);
   attrib = xcb_get_window_attributes_reply(conn, attrib_cookie, NULL);
   if (attrib == NULL || tree == NULL) {
      free(attrib);
      free(tree);
      return NULL;
   }

   xcb_window_t root = tree->root;
   xcb_visualid_t visual_id = attrib->visual;
   free(attrib);
   free(tree);

   xcb_screen_t *screen = get_screen_for_root(conn, root);
   if (screen == NULL)
      return NULL;

   if (rootvis)
      *rootvis = screen_get_visualtype(screen, screen->root_visual, depth);
   return screen_get_visualtype(screen, visual_id, depth);
}

static bool
visual_has_alpha(xcb_visualtype_t *visual, unsigned depth)
{
   uint32_t rgb_mask = visual->red_mask |
                       visual->green_mask |
                       visual->blue_mask;

   uint32_t all_mask = 0xffffffff >> (32 - depth);

   /* Do we have bits left over after RGB? */
   return (all_mask & ~rgb_mask) != 0;
}

static bool
visual_supported(xcb_visualtype_t *visual)
{
   if (!visual)
      return false;

   return visual->_class == XCB_VISUAL_CLASS_TRUE_COLOR ||
          visual->_class == XCB_VISUAL_CLASS_DIRECT_COLOR;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
wsi_GetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                               uint32_t queueFamilyIndex,
                                               xcb_connection_t *connection,
                                               xcb_visualid_t visual_id)
{
   VK_FROM_HANDLE(vk_physical_device, pdevice, physicalDevice);
   struct wsi_device *wsi_device = pdevice->wsi_device;
   if (!(wsi_device->queue_supports_blit & BITFIELD64_BIT(queueFamilyIndex)))
      return false;

   struct wsi_x11_connection *wsi_conn =
      wsi_x11_get_connection(wsi_device, connection);

   if (!wsi_conn)
      return false;

   if (!wsi_device->sw) {
      if (!wsi_x11_check_for_dri3(wsi_conn))
         return false;
   }

   if (!visual_supported(connection_get_visualtype(connection, visual_id)))
      return false;

   return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
wsi_GetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                uint32_t queueFamilyIndex,
                                                Display *dpy,
                                                VisualID visualID)
{
   return wsi_GetPhysicalDeviceXcbPresentationSupportKHR(physicalDevice,
                                                         queueFamilyIndex,
                                                         XGetXCBConnection(dpy),
                                                         visualID);
}

static xcb_connection_t*
x11_surface_get_connection(VkIcdSurfaceBase *icd_surface)
{
   if (icd_surface->platform == VK_ICD_WSI_PLATFORM_XLIB)
      return XGetXCBConnection(((VkIcdSurfaceXlib *)icd_surface)->dpy);
   else
      return ((VkIcdSurfaceXcb *)icd_surface)->connection;
}

static xcb_window_t
x11_surface_get_window(VkIcdSurfaceBase *icd_surface)
{
   if (icd_surface->platform == VK_ICD_WSI_PLATFORM_XLIB)
      return ((VkIcdSurfaceXlib *)icd_surface)->window;
   else
      return ((VkIcdSurfaceXcb *)icd_surface)->window;
}

static VkResult
x11_surface_get_support(VkIcdSurfaceBase *icd_surface,
                        struct wsi_device *wsi_device,
                        uint32_t queueFamilyIndex,
                        VkBool32* pSupported)
{
   xcb_connection_t *conn = x11_surface_get_connection(icd_surface);
   xcb_window_t window = x11_surface_get_window(icd_surface);

   struct wsi_x11_connection *wsi_conn =
      wsi_x11_get_connection(wsi_device, conn);
   if (!wsi_conn)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   if (!wsi_device->sw) {
      if (!wsi_x11_check_for_dri3(wsi_conn)) {
         *pSupported = false;
         return VK_SUCCESS;
      }
   }

   if (!visual_supported(get_visualtype_for_window(conn, window, NULL, NULL))) {
      *pSupported = false;
      return VK_SUCCESS;
   }

   *pSupported = true;
   return VK_SUCCESS;
}

static uint32_t
x11_get_min_image_count(const struct wsi_device *wsi_device, bool is_xwayland)
{
   if (wsi_device->x11.override_minImageCount)
      return wsi_device->x11.override_minImageCount;

   /* For IMMEDIATE and FIFO, most games work in a pipelined manner where the
    * can produce frames at a rate of 1/MAX(CPU duration, GPU duration), but
    * the render latency is CPU duration + GPU duration.
    *
    * This means that with scanout from pageflipping we need 3 frames to run
    * full speed:
    * 1) CPU rendering work
    * 2) GPU rendering work
    * 3) scanout
    *
    * Once we have a nonblocking acquire that returns a semaphore we can merge
    * 1 and 3. Hence the ideal implementation needs only 2 images, but games
    * cannot tellwe currently do not have an ideal implementation and that
    * hence they need to allocate 3 images. So let us do it for them.
    *
    * This is a tradeoff as it uses more memory than needed for non-fullscreen
    * and non-performance intensive applications.
    *
    * For Xwayland Venus reports four images as described in
    *   wsi_wl_surface_get_capabilities
    */
   return is_xwayland && wsi_device->x11.extra_xwayland_image ? 4 : 3;
}

static unsigned
x11_get_min_image_count_for_present_mode(struct wsi_device *wsi_device,
                                         struct wsi_x11_connection *wsi_conn,
                                         VkPresentModeKHR present_mode);

static VkResult
x11_surface_get_capabilities(VkIcdSurfaceBase *icd_surface,
                             struct wsi_device *wsi_device,
                             const VkSurfacePresentModeEXT *present_mode,
                             VkSurfaceCapabilitiesKHR *caps)
{
   xcb_connection_t *conn = x11_surface_get_connection(icd_surface);
   xcb_window_t window = x11_surface_get_window(icd_surface);
   struct wsi_x11_vk_surface *surface = (struct wsi_x11_vk_surface*)icd_surface;
   struct wsi_x11_connection *wsi_conn =
      wsi_x11_get_connection(wsi_device, conn);
   xcb_get_geometry_cookie_t geom_cookie;
   xcb_generic_error_t *err;
   xcb_get_geometry_reply_t *geom;

   geom_cookie = xcb_get_geometry(conn, window);

   geom = xcb_get_geometry_reply(conn, geom_cookie, &err);
   if (!geom)
      return VK_ERROR_SURFACE_LOST_KHR;
   {
      VkExtent2D extent = { geom->width, geom->height };
      caps->currentExtent = extent;
      caps->minImageExtent = extent;
      caps->maxImageExtent = extent;
   }
   free(err);
   free(geom);

   if (surface->has_alpha) {
      caps->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR |
                                      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
   } else {
      caps->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR |
                                      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   }

   if (present_mode) {
      caps->minImageCount = x11_get_min_image_count_for_present_mode(wsi_device, wsi_conn, present_mode->presentMode);
   } else {
      caps->minImageCount = x11_get_min_image_count(wsi_device, wsi_conn->is_xwayland);
   }

   /* There is no real maximum */
   caps->maxImageCount = 0;

   caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->maxImageArrayLayers = 1;
   caps->supportedUsageFlags = wsi_caps_get_image_usage();

   VK_FROM_HANDLE(vk_physical_device, pdevice, wsi_device->pdevice);
   if (pdevice->supported_extensions.EXT_attachment_feedback_loop_layout)
      caps->supportedUsageFlags |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

   return VK_SUCCESS;
}

static VkResult
x11_surface_get_capabilities2(VkIcdSurfaceBase *icd_surface,
                              struct wsi_device *wsi_device,
                              const void *info_next,
                              VkSurfaceCapabilities2KHR *caps)
{
   assert(caps->sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR);

   const VkSurfacePresentModeEXT *present_mode = vk_find_struct_const(info_next, SURFACE_PRESENT_MODE_EXT);

   VkResult result =
      x11_surface_get_capabilities(icd_surface, wsi_device, present_mode,
                                   &caps->surfaceCapabilities);

   if (result != VK_SUCCESS)
      return result;

   vk_foreach_struct(ext, caps->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR: {
         VkSurfaceProtectedCapabilitiesKHR *protected = (void *)ext;
         protected->supportsProtected = VK_FALSE;
         break;
      }

      case VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT: {
         /* Unsupported. */
         VkSurfacePresentScalingCapabilitiesEXT *scaling = (void *)ext;
         scaling->supportedPresentScaling = 0;
         scaling->supportedPresentGravityX = 0;
         scaling->supportedPresentGravityY = 0;
         scaling->minScaledImageExtent = caps->surfaceCapabilities.minImageExtent;
         scaling->maxScaledImageExtent = caps->surfaceCapabilities.maxImageExtent;
         break;
      }

      case VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT: {
         /* All present modes are compatible with each other. */
         VkSurfacePresentModeCompatibilityEXT *compat = (void *)ext;
         if (compat->pPresentModes) {
            assert(present_mode);
            VK_OUTARRAY_MAKE_TYPED(VkPresentModeKHR, modes, compat->pPresentModes, &compat->presentModeCount);
            /* Must always return queried present mode even when truncating. */
            vk_outarray_append_typed(VkPresentModeKHR, &modes, mode) {
               *mode = present_mode->presentMode;
            }

            for (uint32_t i = 0; i < ARRAY_SIZE(present_modes); i++) {
               if (present_modes[i] != present_mode->presentMode) {
                  vk_outarray_append_typed(VkPresentModeKHR, &modes, mode) {
                     *mode = present_modes[i];
                  }
               }
            }
         } else {
            if (!present_mode)
               wsi_common_vk_warn_once("Use of VkSurfacePresentModeCompatibilityEXT "
                                       "without a VkSurfacePresentModeEXT set. This is an "
                                       "application bug.\n");

            compat->presentModeCount = ARRAY_SIZE(present_modes);
         }
         break;
      }

      default:
         /* Ignored */
         break;
      }
   }

   return result;
}

static int
format_get_component_bits(VkFormat format, int comp)
{
   return vk_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, comp);
}

static bool
rgb_component_bits_are_equal(VkFormat format, const xcb_visualtype_t* type)
{
   return format_get_component_bits(format, 0) == util_bitcount(type->red_mask) &&
          format_get_component_bits(format, 1) == util_bitcount(type->green_mask) &&
          format_get_component_bits(format, 2) == util_bitcount(type->blue_mask);
}

static bool
get_sorted_vk_formats(VkIcdSurfaceBase *surface, struct wsi_device *wsi_device,
                      VkFormat *sorted_formats, unsigned *count)
{
   xcb_connection_t *conn = x11_surface_get_connection(surface);
   xcb_window_t window = x11_surface_get_window(surface);
   xcb_visualtype_t *rootvis = NULL;
   xcb_visualtype_t *visual = get_visualtype_for_window(conn, window, NULL, &rootvis);

   if (!visual)
      return false;

   /* use the root window's visual to set the default */
   *count = 0;
   for (unsigned i = 0; i < ARRAY_SIZE(formats); i++) {
      if (rgb_component_bits_are_equal(formats[i], rootvis))
         sorted_formats[(*count)++] = formats[i];
   }

   for (unsigned i = 0; i < ARRAY_SIZE(formats); i++) {
      for (unsigned j = 0; j < *count; j++)
         if (formats[i] == sorted_formats[j])
            goto next_format;
      if (rgb_component_bits_are_equal(formats[i], visual))
         sorted_formats[(*count)++] = formats[i];
next_format:;
   }

   if (wsi_device->force_bgra8_unorm_first) {
      for (unsigned i = 0; i < *count; i++) {
         if (sorted_formats[i] == VK_FORMAT_B8G8R8A8_UNORM) {
            sorted_formats[i] = sorted_formats[0];
            sorted_formats[0] = VK_FORMAT_B8G8R8A8_UNORM;
            break;
         }
      }
   }

   return true;
}

static VkResult
x11_surface_get_formats(VkIcdSurfaceBase *surface,
                        struct wsi_device *wsi_device,
                        uint32_t *pSurfaceFormatCount,
                        VkSurfaceFormatKHR *pSurfaceFormats)
{
   VK_OUTARRAY_MAKE_TYPED(VkSurfaceFormatKHR, out,
                          pSurfaceFormats, pSurfaceFormatCount);

   unsigned count;
   VkFormat sorted_formats[ARRAY_SIZE(formats)];
   if (!get_sorted_vk_formats(surface, wsi_device, sorted_formats, &count))
      return VK_ERROR_SURFACE_LOST_KHR;

   for (unsigned i = 0; i < count; i++) {
      vk_outarray_append_typed(VkSurfaceFormatKHR, &out, f) {
         f->format = sorted_formats[i];
         f->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);
}

static VkResult
x11_surface_get_formats2(VkIcdSurfaceBase *surface,
                        struct wsi_device *wsi_device,
                        const void *info_next,
                        uint32_t *pSurfaceFormatCount,
                        VkSurfaceFormat2KHR *pSurfaceFormats)
{
   VK_OUTARRAY_MAKE_TYPED(VkSurfaceFormat2KHR, out,
                          pSurfaceFormats, pSurfaceFormatCount);

   unsigned count;
   VkFormat sorted_formats[ARRAY_SIZE(formats)];
   if (!get_sorted_vk_formats(surface, wsi_device, sorted_formats, &count))
      return VK_ERROR_SURFACE_LOST_KHR;

   for (unsigned i = 0; i < count; i++) {
      vk_outarray_append_typed(VkSurfaceFormat2KHR, &out, f) {
         assert(f->sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
         f->surfaceFormat.format = sorted_formats[i];
         f->surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      }
   }

   return vk_outarray_status(&out);
}

static VkResult
x11_surface_get_present_modes(VkIcdSurfaceBase *surface,
                              struct wsi_device *wsi_device,
                              uint32_t *pPresentModeCount,
                              VkPresentModeKHR *pPresentModes)
{
   if (pPresentModes == NULL) {
      *pPresentModeCount = ARRAY_SIZE(present_modes);
      return VK_SUCCESS;
   }

   *pPresentModeCount = MIN2(*pPresentModeCount, ARRAY_SIZE(present_modes));
   typed_memcpy(pPresentModes, present_modes, *pPresentModeCount);

   return *pPresentModeCount < ARRAY_SIZE(present_modes) ?
      VK_INCOMPLETE : VK_SUCCESS;
}

static VkResult
x11_surface_get_present_rectangles(VkIcdSurfaceBase *icd_surface,
                                   struct wsi_device *wsi_device,
                                   uint32_t* pRectCount,
                                   VkRect2D* pRects)
{
   xcb_connection_t *conn = x11_surface_get_connection(icd_surface);
   xcb_window_t window = x11_surface_get_window(icd_surface);
   VK_OUTARRAY_MAKE_TYPED(VkRect2D, out, pRects, pRectCount);

   vk_outarray_append_typed(VkRect2D, &out, rect) {
      xcb_generic_error_t *err = NULL;
      xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(conn, window);
      xcb_get_geometry_reply_t *geom =
         xcb_get_geometry_reply(conn, geom_cookie, &err);
      free(err);
      if (geom) {
         *rect = (VkRect2D) {
            .offset = { 0, 0 },
            .extent = { geom->width, geom->height },
         };
      }
      free(geom);
      if (!geom)
          return VK_ERROR_SURFACE_LOST_KHR;
   }

   return vk_outarray_status(&out);
}

VKAPI_ATTR VkResult VKAPI_CALL
wsi_CreateXcbSurfaceKHR(VkInstance _instance,
                        const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkSurfaceKHR *pSurface)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);
   struct wsi_x11_vk_surface *surface;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR);

   unsigned visual_depth;
   xcb_visualtype_t *visual =
      get_visualtype_for_window(pCreateInfo->connection, pCreateInfo->window, &visual_depth, NULL);
   if (!visual)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface = vk_alloc2(&instance->alloc, pAllocator, sizeof(struct wsi_x11_vk_surface), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (surface == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->xcb.base.platform = VK_ICD_WSI_PLATFORM_XCB;
   surface->xcb.connection = pCreateInfo->connection;
   surface->xcb.window = pCreateInfo->window;

   surface->has_alpha = visual_has_alpha(visual, visual_depth);

   *pSurface = VkIcdSurfaceBase_to_handle(&surface->xcb.base);
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
wsi_CreateXlibSurfaceKHR(VkInstance _instance,
                         const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkSurfaceKHR *pSurface)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);
   struct wsi_x11_vk_surface *surface;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR);

   unsigned visual_depth;
   xcb_visualtype_t *visual =
      get_visualtype_for_window(XGetXCBConnection(pCreateInfo->dpy), pCreateInfo->window, &visual_depth, NULL);
   if (!visual)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface = vk_alloc2(&instance->alloc, pAllocator, sizeof(struct wsi_x11_vk_surface), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (surface == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->xlib.base.platform = VK_ICD_WSI_PLATFORM_XLIB;
   surface->xlib.dpy = pCreateInfo->dpy;
   surface->xlib.window = pCreateInfo->window;

   surface->has_alpha = visual_has_alpha(visual, visual_depth);

   *pSurface = VkIcdSurfaceBase_to_handle(&surface->xlib.base);
   return VK_SUCCESS;
}

struct x11_image_pending_completion {
   uint32_t serial;
   uint64_t signal_present_id;
};

struct x11_image {
   struct wsi_image                          base;
   xcb_pixmap_t                              pixmap;
   xcb_xfixes_region_t                       update_region; /* long lived XID */
   xcb_xfixes_region_t                       update_area;   /* the above or None */
   struct xshmfence *                        shm_fence;
   uint32_t                                  sync_fence;
   xcb_shm_seg_t                             shmseg;
   int                                       shmid;
   uint8_t *                                 shmaddr;
   uint64_t                                  present_id;
   VkPresentModeKHR                          present_mode;

   /* In IMMEDIATE and MAILBOX modes, we can have multiple pending presentations per image.
    * We need to keep track of them when considering present ID. */

   /* This is arbitrarily chosen. With IMMEDIATE on a 3 deep swapchain,
    * we allow up to 48 outstanding presentations per vblank, which is more than enough
    * for any reasonable application. */
#define X11_SWAPCHAIN_MAX_PENDING_COMPLETIONS 16
   uint32_t                                  present_queued_count;
   struct x11_image_pending_completion       pending_completions[X11_SWAPCHAIN_MAX_PENDING_COMPLETIONS];
#ifdef HAVE_DRI3_EXPLICIT_SYNC
   uint32_t                                  dri3_syncobj[WSI_ES_COUNT];
#endif
};

struct x11_swapchain {
   struct wsi_swapchain                        base;

   bool                                         has_dri3_modifiers;
   bool                                         has_mit_shm;
   bool                                         has_async_may_tear;

   xcb_connection_t *                           conn;
   xcb_window_t                                 window;
   xcb_gc_t                                     gc;
   uint32_t                                     depth;
   VkExtent2D                                   extent;

   blake3_hash                                  dri3_modifier_hash;

   xcb_present_event_t                          event_id;
   xcb_special_event_t *                        special_event;
   uint64_t                                     send_sbc;
   uint64_t                                     last_present_msc;
   uint32_t                                     stamp;
   uint32_t                                     sent_image_count;

   atomic_int                                   status;
   bool                                         copy_is_suboptimal;
   struct wsi_queue                             present_queue;
   struct wsi_queue                             acquire_queue;
   pthread_t                                    queue_manager;
   pthread_t                                    event_manager;

   /* Used for communicating between event_manager and queue_manager.
    * Lock is also taken when reading and writing status.
    * When reading status in application threads,
    * x11_swapchain_read_status_atomic can be used as a wrapper function. */
   pthread_mutex_t                              thread_state_lock;
   pthread_cond_t                               thread_state_cond;

   /* Lock and condition variable for present wait.
    * Signalled by event thread and waited on by callers to PresentWaitKHR. */
   pthread_mutex_t                              present_progress_mutex;
   pthread_cond_t                               present_progress_cond;
   uint64_t                                     present_id;
   VkResult                                     present_progress_error;

   struct x11_image                             images[0];
};
VK_DEFINE_NONDISP_HANDLE_CASTS(x11_swapchain, base.base, VkSwapchainKHR,
                               VK_OBJECT_TYPE_SWAPCHAIN_KHR)

static void x11_present_complete(struct x11_swapchain *swapchain,
                                 struct x11_image *image, uint32_t index)
{
   uint64_t signal_present_id = image->pending_completions[index].signal_present_id;
   if (signal_present_id) {
      pthread_mutex_lock(&swapchain->present_progress_mutex);
      if (signal_present_id > swapchain->present_id) {
         swapchain->present_id = signal_present_id;
         pthread_cond_broadcast(&swapchain->present_progress_cond);
      }
      pthread_mutex_unlock(&swapchain->present_progress_mutex);
   }

   image->present_queued_count--;
   if (image->present_queued_count) {
      memmove(image->pending_completions + index,
              image->pending_completions + index + 1,
              (image->present_queued_count - index) *
              sizeof(image->pending_completions[0]));
   }

   pthread_cond_signal(&swapchain->thread_state_cond);
}

static void x11_notify_pending_present(struct x11_swapchain *swapchain,
                                       struct x11_image *image)
{
   pthread_cond_signal(&swapchain->thread_state_cond);
}

/* It is assumed that thread_state_lock is taken when calling this function. */
static void x11_swapchain_notify_error(struct x11_swapchain *swapchain, VkResult result)
{
   pthread_mutex_lock(&swapchain->present_progress_mutex);
   swapchain->present_id = UINT64_MAX;
   swapchain->present_progress_error = result;
   pthread_cond_broadcast(&swapchain->present_progress_cond);
   pthread_mutex_unlock(&swapchain->present_progress_mutex);
   pthread_cond_broadcast(&swapchain->thread_state_cond);
}

/**
 * Update the swapchain status with the result of an operation, and return
 * the combined status. The chain status will eventually be returned from
 * AcquireNextImage and QueuePresent.
 *
 * We make sure to 'stick' more pessimistic statuses: an out-of-date error
 * is permanent once seen, and every subsequent call will return this. If
 * this has not been seen, success will be returned.
 *
 * It is assumed that thread_state_lock is taken when calling this function.
 */
static VkResult
_x11_swapchain_result(struct x11_swapchain *chain, VkResult result,
                      const char *file, int line)
{
   if (result < 0)
      x11_swapchain_notify_error(chain, result);

   /* Prioritise returning existing errors for consistency. */
   if (chain->status < 0)
      return chain->status;

   /* If we have a new error, mark it as permanent on the chain and return. */
   if (result < 0) {
#ifndef NDEBUG
      fprintf(stderr, "%s:%d: Swapchain status changed to %s\n",
              file, line, vk_Result_to_str(result));
#endif
      chain->status = result;
      return result;
   }

   /* Return temporary errors, but don't persist them. */
   if (result == VK_TIMEOUT || result == VK_NOT_READY)
      return result;

   /* Suboptimal isn't an error, but is a status which sticks to the swapchain
    * and is always returned rather than success.
    */
   if (result == VK_SUBOPTIMAL_KHR) {
#ifndef NDEBUG
      if (chain->status != VK_SUBOPTIMAL_KHR) {
         fprintf(stderr, "%s:%d: Swapchain status changed to %s\n",
                 file, line, vk_Result_to_str(result));
      }
#endif
      chain->status = result;
      return result;
   }

   /* No changes, so return the last status. */
   return chain->status;
}
#define x11_swapchain_result(chain, result) \
   _x11_swapchain_result(chain, result, __FILE__, __LINE__)

static struct wsi_image *
x11_get_wsi_image(struct wsi_swapchain *wsi_chain, uint32_t image_index)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)wsi_chain;
   return &chain->images[image_index].base;
}

static bool
wsi_x11_swapchain_query_dri3_modifiers_changed(struct x11_swapchain *chain);

static VkResult
x11_wait_for_explicit_sync_release_submission(struct x11_swapchain *chain,
                                              uint64_t rel_timeout_ns,
                                              uint32_t *image_index)
{
   STACK_ARRAY(struct wsi_image*, images, chain->base.image_count);
   for (uint32_t i = 0; i < chain->base.image_count; i++)
      images[i] = &chain->images[i].base;

   VkResult result;
#ifdef HAVE_LIBDRM
   result = wsi_drm_wait_for_explicit_sync_release(&chain->base,
                                                   chain->base.image_count,
                                                   images,
                                                   rel_timeout_ns,
                                                   image_index);
#else
   result = VK_ERROR_FEATURE_NOT_PRESENT;
#endif
   STACK_ARRAY_FINISH(images);
   return result;
}

/* XXX this belongs in presentproto */
#ifndef PresentWindowDestroyed
#define PresentWindowDestroyed (1 << 0)
#endif
/**
 * Process an X11 Present event. Does not update chain->status.
 */
static VkResult
x11_handle_dri3_present_event(struct x11_swapchain *chain,
                              xcb_present_generic_event_t *event)
{
   switch (event->evtype) {
   case XCB_PRESENT_CONFIGURE_NOTIFY: {
      xcb_present_configure_notify_event_t *config = (void *) event;
      if (config->pixmap_flags & PresentWindowDestroyed)
         return VK_ERROR_SURFACE_LOST_KHR;

      struct wsi_device *wsi_device = (struct wsi_device *)chain->base.wsi;
      if (!wsi_device->x11.ignore_suboptimal) {
         if (config->width != chain->extent.width ||
             config->height != chain->extent.height)
            return VK_SUBOPTIMAL_KHR;
      }

      break;
   }

   case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
      xcb_present_idle_notify_event_t *idle = (void *) event;

      assert(!chain->base.image_info.explicit_sync);
      for (unsigned i = 0; i < chain->base.image_count; i++) {
         if (chain->images[i].pixmap == idle->pixmap) {
            chain->sent_image_count--;
            assert(chain->sent_image_count >= 0);
            wsi_queue_push(&chain->acquire_queue, i);
            break;
         }
      }

      break;
   }

   case XCB_PRESENT_EVENT_COMPLETE_NOTIFY: {
      xcb_present_complete_notify_event_t *complete = (void *) event;
      if (complete->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP) {
         unsigned i, j;
         for (i = 0; i < chain->base.image_count; i++) {
            struct x11_image *image = &chain->images[i];
            for (j = 0; j < image->present_queued_count; j++) {
               if (image->pending_completions[j].serial == complete->serial) {
                  x11_present_complete(chain, image, j);
               }
            }
         }
         chain->last_present_msc = complete->msc;
      }

      VkResult result = VK_SUCCESS;

      struct wsi_device *wsi_device = (struct wsi_device *)chain->base.wsi;
      if (wsi_device->x11.ignore_suboptimal)
         return result;

      switch (complete->mode) {
      case XCB_PRESENT_COMPLETE_MODE_COPY:
         if (chain->copy_is_suboptimal)
            result = VK_SUBOPTIMAL_KHR;
         break;
      case XCB_PRESENT_COMPLETE_MODE_FLIP:
         /* If we ever go from flipping to copying, the odds are very likely
          * that we could reallocate in a more optimal way if we didn't have
          * to care about scanout, so we always do this.
          */
         chain->copy_is_suboptimal = true;
         break;
#ifdef HAVE_DRI3_MODIFIERS
      case XCB_PRESENT_COMPLETE_MODE_SUBOPTIMAL_COPY:
         /* The winsys is now trying to flip directly and cannot due to our
          * configuration. Request the user reallocate.
          */

         /* Sometimes, this complete mode is spurious, and a false positive.
          * Xwayland may report SUBOPTIMAL_COPY even if there are no changes in the modifiers.
          * https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/26616 for more details. */
         if (chain->status == VK_SUCCESS &&
             wsi_x11_swapchain_query_dri3_modifiers_changed(chain)) {
            result = VK_SUBOPTIMAL_KHR;
         }
         break;
#endif
      default:
         break;
      }

      return result;
   }

   default:
      break;
   }

   return VK_SUCCESS;
}

/**
 * Send image to X server via Present extension.
 */
static VkResult
x11_present_to_x11_dri3(struct x11_swapchain *chain, uint32_t image_index,
                        uint64_t target_msc, VkPresentModeKHR present_mode)
{
   struct x11_image *image = &chain->images[image_index];

   assert(image_index < chain->base.image_count);

   uint32_t options = XCB_PRESENT_OPTION_NONE;

   int64_t divisor = 0;
   int64_t remainder = 0;

   struct wsi_x11_connection *wsi_conn =
      wsi_x11_get_connection((struct wsi_device*)chain->base.wsi, chain->conn);
   if (!wsi_conn)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ||
       (present_mode == VK_PRESENT_MODE_MAILBOX_KHR &&
        wsi_conn->is_xwayland) ||
       present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
      options |= XCB_PRESENT_OPTION_ASYNC;

   if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR
      && chain->has_async_may_tear)
      options |= XCB_PRESENT_OPTION_ASYNC_MAY_TEAR;

#ifdef HAVE_DRI3_MODIFIERS
   if (chain->has_dri3_modifiers)
      options |= XCB_PRESENT_OPTION_SUBOPTIMAL;
#endif

   xshmfence_reset(image->shm_fence);

   if (!chain->base.image_info.explicit_sync) {
      ++chain->sent_image_count;
      assert(chain->sent_image_count <= chain->base.image_count);
   }

   ++chain->send_sbc;
   uint32_t serial = (uint32_t)chain->send_sbc;

   assert(image->present_queued_count < ARRAY_SIZE(image->pending_completions));
   image->pending_completions[image->present_queued_count++] =
      (struct x11_image_pending_completion) {
         .signal_present_id = image->present_id,
         .serial = serial,
      };

   xcb_void_cookie_t cookie;
#ifdef HAVE_DRI3_EXPLICIT_SYNC
   if (chain->base.image_info.explicit_sync) {
      uint64_t acquire_point = image->base.explicit_sync[WSI_ES_ACQUIRE].timeline;
      uint64_t release_point = image->base.explicit_sync[WSI_ES_RELEASE].timeline;
      cookie = xcb_present_pixmap_synced(
         chain->conn,
         chain->window,
         image->pixmap,
         serial,
         0,                                   /* valid */
         image->update_area,                  /* update */
         0,                                   /* x_off */
         0,                                   /* y_off */
         XCB_NONE,                            /* target_crtc */
         image->dri3_syncobj[WSI_ES_ACQUIRE], /* acquire_syncobj */
         image->dri3_syncobj[WSI_ES_RELEASE], /* release_syncobj */
         acquire_point,
         release_point,
         options,
         target_msc,
         divisor,
         remainder, 0, NULL);
   } else
#endif
   {
      cookie = xcb_present_pixmap(chain->conn,
                                  chain->window,
                                  image->pixmap,
                                  serial,
                                  0,                  /* valid */
                                  image->update_area, /* update */
                                  0,                  /* x_off */
                                  0,                  /* y_off */
                                  XCB_NONE,           /* target_crtc */
                                  XCB_NONE,
                                  image->sync_fence,
                                  options,
                                  target_msc,
                                  divisor,
                                  remainder, 0, NULL);
   }
   xcb_discard_reply(chain->conn, cookie.sequence);
   xcb_flush(chain->conn);
   return x11_swapchain_result(chain, VK_SUCCESS);
}

/**
 * Send image to X server unaccelerated (software drivers).
 */
static VkResult
x11_present_to_x11_sw(struct x11_swapchain *chain, uint32_t image_index)
{
   assert(!chain->base.image_info.explicit_sync);
   struct x11_image *image = &chain->images[image_index];

   /* Begin querying this before submitting the frame for improved async performance.
    * In this _sw() mode we're expecting network round-trip delay, not just UNIX socket delay. */
   xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(chain->conn, chain->window);

   xcb_void_cookie_t cookie;
   void *myptr = image->base.cpu_map;
   size_t hdr_len = sizeof(xcb_put_image_request_t);
   int stride_b = image->base.row_pitches[0];
   size_t size = (hdr_len + stride_b * chain->extent.height) >> 2;
   uint64_t max_req_len = xcb_get_maximum_request_length(chain->conn);

   if (size < max_req_len) {
      cookie = xcb_put_image(chain->conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                             chain->window,
                             chain->gc,
                             image->base.row_pitches[0] / 4,
                             chain->extent.height,
                             0,0,0,chain->depth,
                             image->base.row_pitches[0] * chain->extent.height,
                             image->base.cpu_map);
      xcb_discard_reply(chain->conn, cookie.sequence);
   } else {
      int num_lines = ((max_req_len << 2) - hdr_len) / stride_b;
      int y_start = 0;
      int y_todo = chain->extent.height;
      while (y_todo) {
         int this_lines = MIN2(num_lines, y_todo);
         cookie = xcb_put_image(chain->conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                                chain->window,
                                chain->gc,
                                image->base.row_pitches[0] / 4,
                                this_lines,
                                0,y_start,0,chain->depth,
                                this_lines * stride_b,
                                (const uint8_t *)myptr + (y_start * stride_b));
         xcb_discard_reply(chain->conn, cookie.sequence);
         y_start += this_lines;
         y_todo -= this_lines;
      }
   }

   xcb_flush(chain->conn);

   /* We don't have queued present here.
    * Immediately let application acquire again, but query geometry first so
    * we can report OUT_OF_DATE on resize. */
   xcb_generic_error_t *err;

   xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(chain->conn, geom_cookie, &err);
   VkResult result = VK_SUCCESS;
   if (geom) {
      if (chain->extent.width != geom->width ||
          chain->extent.height != geom->height)
         result = VK_ERROR_OUT_OF_DATE_KHR;
   } else {
      result = VK_ERROR_SURFACE_LOST_KHR;
   }
   free(err);
   free(geom);

   wsi_queue_push(&chain->acquire_queue, image_index);
   return result;
}

static void
x11_capture_trace(struct x11_swapchain *chain)
{
#ifdef XCB_KEYSYMS_AVAILABLE
   VK_FROM_HANDLE(vk_device, device, chain->base.device);
   if (!device->physical->instance->trace_mode)
      return;

   xcb_query_keymap_cookie_t keys_cookie = xcb_query_keymap(chain->conn);

   xcb_generic_error_t *error = NULL;
   xcb_query_keymap_reply_t *keys = xcb_query_keymap_reply(chain->conn, keys_cookie, &error);
   if (error) {
      free(error);
      return;
   }

   xcb_key_symbols_t *key_symbols = xcb_key_symbols_alloc(chain->conn);
   xcb_keycode_t *keycodes = xcb_key_symbols_get_keycode(key_symbols, XK_F1);
   if (keycodes) {
      xcb_keycode_t keycode = keycodes[0];
      free(keycodes);

      simple_mtx_lock(&device->trace_mtx);
      bool capture_key_pressed = keys->keys[keycode / 8] & (1u << (keycode % 8));
      device->trace_hotkey_trigger = capture_key_pressed && (capture_key_pressed != chain->base.capture_key_pressed);
      chain->base.capture_key_pressed = capture_key_pressed;
      simple_mtx_unlock(&device->trace_mtx);
   }

   xcb_key_symbols_free(key_symbols);
   free(keys);
#endif
}

/* Use a trivial helper here to make it easier to read in code
 * where we're intending to access chain->status outside the thread lock. */
static VkResult x11_swapchain_read_status_atomic(struct x11_swapchain *chain)
{
   return chain->status;
}

/**
 * Decides if an early wait on buffer fences before buffer submission is required.
 * That is for mailbox mode, as otherwise the latest image in the queue might not be fully rendered at
 * present time, which could lead to missing a frame. This is an Xorg issue.
 *
 * On Wayland compositors, this used to be a problem as well, but not anymore,
 * and this check assumes that Mesa is running on a reasonable compositor.
 * The wait behavior can be forced by setting the 'vk_xwayland_wait_ready' DRIConf option to true.
 * Some drivers, like e.g. Venus may still want to require wait_ready by default,
 * so the option is kept around for now.
 *
 * On Wayland, we don't know at this point if tearing protocol is/can be used by Xwl,
 * so we have to make the MAILBOX assumption.
 */
static bool
x11_needs_wait_for_fences(const struct wsi_device *wsi_device,
                          struct wsi_x11_connection *wsi_conn,
                          VkPresentModeKHR present_mode)
{
   if (wsi_conn->is_xwayland && !wsi_device->x11.xwaylandWaitReady) {
      return false;
   }

   switch (present_mode) {
      case VK_PRESENT_MODE_MAILBOX_KHR:
         return true;
      case VK_PRESENT_MODE_IMMEDIATE_KHR:
         return wsi_conn->is_xwayland;
      default:
         return false;
   }
}

/* This matches Wayland. */
#define X11_SWAPCHAIN_MAILBOX_IMAGES 4

static bool
x11_requires_mailbox_image_count(const struct wsi_device *device,
                                 struct wsi_x11_connection *wsi_conn,
                                 VkPresentModeKHR present_mode)
{
   /* If we're resorting to wait for fences, we're assuming a MAILBOX-like model,
    * and we should allocate accordingly.
    *
    * One potential concern here is IMMEDIATE mode on Wayland.
    * This situation could arise:
    * - Fullscreen FLIP mode
    * - Compositor does not support tearing protocol (we cannot know this here)
    *
    * With 3 images, during the window between latch and flip, there is only one image left to app,
    * so peak FPS may not be reached if the window between latch and flip is large,
    * but tests on contemporary compositors suggest this effect is minor.
    * Frame rate in the thousands can easily be reached.
    *
    * There are pragmatic reasons to expose 3 images for IMMEDIATE on Xwl.
    * - minImageCount is not intended as a tool to tune performance, its intent is to signal forward progress.
    *   Our X11 and WL implementations do so for pragmatic reasons due to sync acquire interacting poorly with 2 images.
    *   A jump from 3 to 4 is at best a minor improvement which only affects applications
    *   running at extremely high frame rates, way beyond the monitor refresh rate.
    *   On the other hand, lowering minImageCount to 2 would break the fundamental idea of MAILBOX
    *   (and IMMEDIATE without tear), since FPS > refresh rate would not be possible.
    *
    * - Several games developed for other platforms and other Linux WSI implementations
    *   do not expect that image counts arbitrarily change when changing present mode,
    *   and will crash when Mesa does so.
    *   There are several games using the strict_image_count drirc to work around this,
    *   and it would be good to be friendlier in the first place, so we don't have to work around more games.
    *   IMMEDIATE is a common presentation mode on those platforms, but MAILBOX is more Wayland-centric in nature,
    *   so increasing image count for that mode is more reasonable.
    *
    * - IMMEDIATE expects tearing, and when tearing, 3 images are more than enough.
    *
    * - With EXT_swapchain_maintenance1, toggling between FIFO / IMMEDIATE (used extensively by D3D layering)
    *   would require application to allocate >3 images which is unfortunate for memory usage,
    *   and potentially disastrous for latency unless KHR_present_wait is used.
    */
   return x11_needs_wait_for_fences(device, wsi_conn, present_mode) ||
          present_mode == VK_PRESENT_MODE_MAILBOX_KHR;
}

/**
 * Send image to the X server for presentation at target_msc.
 */
static VkResult
x11_present_to_x11(struct x11_swapchain *chain, uint32_t image_index,
                   uint64_t target_msc, VkPresentModeKHR present_mode)
{
   x11_capture_trace(chain);

   VkResult result;
   if (chain->base.wsi->sw && !chain->has_mit_shm)
      result = x11_present_to_x11_sw(chain, image_index);
   else
      result = x11_present_to_x11_dri3(chain, image_index, target_msc, present_mode);

   if (result < 0)
      x11_swapchain_notify_error(chain, result);
   else
      x11_notify_pending_present(chain, &chain->images[image_index]);

   return result;
}

static VkResult
x11_release_images(struct wsi_swapchain *wsi_chain,
                   uint32_t count, const uint32_t *indices)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)wsi_chain;
   if (chain->status == VK_ERROR_SURFACE_LOST_KHR)
      return chain->status;

   /* If we're using implicit sync, push images to the acquire queue */
   if (!chain->base.image_info.explicit_sync) {
      for (uint32_t i = 0; i < count; i++) {
         uint32_t index = indices[i];
         assert(index < chain->base.image_count);
         wsi_queue_push(&chain->acquire_queue, index);
      }
   }

   return VK_SUCCESS;
}

static void
x11_set_present_mode(struct wsi_swapchain *wsi_chain,
                     VkPresentModeKHR mode)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)wsi_chain;
   chain->base.present_mode = mode;
}

/**
 * Acquire a ready-to-use image from the swapchain.
 *
 * This means usually that the image is not waiting on presentation and that the
 * image has been released by the X server to be used again by the consumer.
 */
static VkResult
x11_acquire_next_image(struct wsi_swapchain *anv_chain,
                       const VkAcquireNextImageInfoKHR *info,
                       uint32_t *image_index)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)anv_chain;
   uint64_t timeout = info->timeout;

   /* If the swapchain is in an error state, don't go any further. */
   VkResult result = x11_swapchain_read_status_atomic(chain);
   if (result < 0)
      return result;

   if (chain->base.image_info.explicit_sync) {
      result = x11_wait_for_explicit_sync_release_submission(chain, timeout,
                                                             image_index);
   } else {
      result = wsi_queue_pull(&chain->acquire_queue,
                              image_index, timeout);
   }

   if (result == VK_TIMEOUT)
      return info->timeout ? VK_TIMEOUT : VK_NOT_READY;

   if (result < 0) {
      pthread_mutex_lock(&chain->thread_state_lock);
      result = x11_swapchain_result(chain, result);
      pthread_mutex_unlock(&chain->thread_state_lock);
   } else {
      result = x11_swapchain_read_status_atomic(chain);
   }

   if (result < 0)
      return result;

   assert(*image_index < chain->base.image_count);
   if (chain->images[*image_index].shm_fence &&
       !chain->base.image_info.explicit_sync)
      xshmfence_await(chain->images[*image_index].shm_fence);

   return result;
}

#define MAX_DAMAGE_RECTS 64

/**
 * Queue a new presentation of an image that was previously acquired by the
 * consumer.
 *
 * Note that in immediate presentation mode this does not really queue the
 * presentation but directly asks the X server to show it.
 */
static VkResult
x11_queue_present(struct wsi_swapchain *anv_chain,
                  uint32_t image_index,
                  uint64_t present_id,
                  const VkPresentRegionKHR *damage)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)anv_chain;
   xcb_xfixes_region_t update_area = 0;

   /* If the swapchain is in an error state, don't go any further. */
   VkResult status = x11_swapchain_read_status_atomic(chain);
   if (status < 0)
      return status;

   if (damage && damage->pRectangles && damage->rectangleCount > 0 &&
      damage->rectangleCount <= MAX_DAMAGE_RECTS) {
      xcb_rectangle_t rects[MAX_DAMAGE_RECTS];

      update_area = chain->images[image_index].update_region;
      for (unsigned i = 0; i < damage->rectangleCount; i++) {
         const VkRectLayerKHR *rect = &damage->pRectangles[i];
         assert(rect->layer == 0);
         rects[i].x = rect->offset.x;
         rects[i].y = rect->offset.y;
         rects[i].width = rect->extent.width;
         rects[i].height = rect->extent.height;
      }
      xcb_xfixes_set_region(chain->conn, update_area, damage->rectangleCount, rects);
   }
   chain->images[image_index].update_area = update_area;
   chain->images[image_index].present_id = present_id;
   /* With EXT_swapchain_maintenance1, the present mode can change per present. */
   chain->images[image_index].present_mode = chain->base.present_mode;

   wsi_queue_push(&chain->present_queue, image_index);
   return x11_swapchain_read_status_atomic(chain);
}

/**
 * The number of images that are not owned by X11:
 *  (1) in the ownership of the app, or
 *  (2) app to take ownership through an acquire, or
 *  (3) in the present queue waiting for the FIFO thread to present to X11.
 */
static unsigned x11_driver_owned_images(const struct x11_swapchain *chain)
{
   return chain->base.image_count - chain->sent_image_count;
}

/* This thread is responsible for pumping PRESENT replies.
 * This is done in a separate thread from the X11 presentation thread
 * to be able to support non-blocking modes like IMMEDIATE and MAILBOX.
 * Frame completion events can happen at any time, and we need to handle
 * the events as soon as they come in to have a quality implementation.
 * The presentation thread may go to sleep waiting for new presentation events to come in,
 * and it cannot wait for both X events and application events at the same time.
 * If we only cared about FIFO, this thread wouldn't be very useful.
 * Earlier implementation of X11 WSI had a single FIFO thread that blocked on X events after presenting.
 * For IMMEDIATE and MAILBOX, the application thread pumped the event queue, which caused a lot of pain
 * when trying to deal with present wait.
 */
static void *
x11_manage_event_queue(void *state)
{
   struct x11_swapchain *chain = state;
   u_thread_setname("WSI swapchain event");

   /* While there is an outstanding IDLE we should wait for it.
    * In FLIP modes at most one image will not be driver owned eventually.
    * In BLIT modes, we expect that all images will eventually be driver owned,
    * but we don't know which mode is being used. */
   unsigned forward_progress_guaranteed_acquired_images = chain->base.image_count - 1;

   pthread_mutex_lock(&chain->thread_state_lock);

   while (chain->status >= 0) {
      /* This thread should only go sleep waiting for X events when we know there are pending events.
       * We expect COMPLETION events when there is at least one image marked as present_queued.
       * We also expect IDLE events, but we only consider waiting for them when all images are busy,
       * and application has fewer than N images acquired. */

      bool assume_forward_progress = false;

      for (uint32_t i = 0; i < chain->base.image_count; i++) {
         if (chain->images[i].present_queued_count != 0) {
            /* We must pump through a present wait and unblock FIFO thread if using FIFO mode. */
            assume_forward_progress = true;
            break;
         }
      }

      if (!assume_forward_progress && !chain->base.image_info.explicit_sync) {
         /* If true, application expects acquire (IDLE) to happen in finite time. */
         assume_forward_progress = x11_driver_owned_images(chain) <
                                   forward_progress_guaranteed_acquired_images;
      }

      if (assume_forward_progress) {
         /* Only yield lock when blocking on X11 event. */
         pthread_mutex_unlock(&chain->thread_state_lock);
         xcb_generic_event_t *event =
               xcb_wait_for_special_event(chain->conn, chain->special_event);
         pthread_mutex_lock(&chain->thread_state_lock);

         /* Re-check status since we dropped the lock while waiting for X. */
         VkResult result = chain->status;

         if (result >= 0) {
            if (event) {
               /* Queue thread will be woken up if anything interesting happened in handler.
                * Queue thread blocks on:
                * - Presentation events completing
                * - Presentation requests from application
                * - WaitForFence workaround if applicable */
               result = x11_handle_dri3_present_event(chain, (void *) event);
            } else {
               result = VK_ERROR_SURFACE_LOST_KHR;
            }
         }

         /* Updates chain->status and wakes up threads as necessary on error. */
         x11_swapchain_result(chain, result);
         free(event);
      } else {
         /* Nothing important to do, go to sleep until queue thread wakes us up. */
         pthread_cond_wait(&chain->thread_state_cond, &chain->thread_state_lock);
      }
   }

   pthread_mutex_unlock(&chain->thread_state_lock);
   return NULL;
}

/**
 * Presentation thread.
 *
 * Runs in a separate thread, blocks and reacts to queued images on the
 * present-queue
 *
 * This must be a thread since we have to block in two cases:
 * - FIFO:
 *     We must wait for previous presentation to complete
 *     in some way so we can compute the target MSC.
 * - WaitForFence workaround:
 *     In some cases, we need to wait for image to complete rendering before submitting it to X.
 */
static void *
x11_manage_present_queue(void *state)
{
   struct x11_swapchain *chain = state;
   struct wsi_x11_connection *wsi_conn =
         wsi_x11_get_connection((struct wsi_device*)chain->base.wsi, chain->conn);
   VkResult result = VK_SUCCESS;

   u_thread_setname("WSI swapchain queue");

   uint64_t target_msc = 0;

   while (x11_swapchain_read_status_atomic(chain) >= 0) {
      uint32_t image_index = 0;
      {
         MESA_TRACE_SCOPE("pull present queue");
         result = wsi_queue_pull(&chain->present_queue, &image_index, INT64_MAX);
         assert(result != VK_TIMEOUT);
      }

      /* The status can change underneath us if the swapchain is destroyed
       * from another thread. */
      if (result >= 0)
         result = x11_swapchain_read_status_atomic(chain);
      if (result < 0)
         break;

      VkPresentModeKHR present_mode = chain->images[image_index].present_mode;

      if (x11_needs_wait_for_fences(chain->base.wsi, wsi_conn,
                                    present_mode) &&
          /* not necessary with explicit sync */
          !chain->base.image_info.explicit_sync) {
         MESA_TRACE_SCOPE("wait fence");
         result = chain->base.wsi->WaitForFences(chain->base.device, 1,
                                                 &chain->base.fences[image_index],
                                                 true, UINT64_MAX);
         if (result != VK_SUCCESS) {
            result = VK_ERROR_OUT_OF_DATE_KHR;
            break;
         }
      }

      pthread_mutex_lock(&chain->thread_state_lock);

      /* In IMMEDIATE and MAILBOX modes, there is a risk that we have exhausted the presentation queue,
       * since IDLE could return multiple times before observing a COMPLETE. */
      while (chain->status >= 0 &&
             chain->images[image_index].present_queued_count ==
             ARRAY_SIZE(chain->images[image_index].pending_completions)) {
         pthread_cond_wait(&chain->thread_state_cond, &chain->thread_state_lock);
      }

      if (chain->status < 0) {
         pthread_mutex_unlock(&chain->thread_state_lock);
         break;
      }

      result = x11_present_to_x11(chain, image_index, target_msc, present_mode);

      if (result < 0) {
         pthread_mutex_unlock(&chain->thread_state_lock);
         break;
      }

      if (present_mode == VK_PRESENT_MODE_FIFO_KHR ||
          present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
         MESA_TRACE_SCOPE("wait present");

         while (chain->status >= 0 && chain->images[image_index].present_queued_count != 0) {
            /* In FIFO mode, we need to make sure we observe a COMPLETE before queueing up
             * another present. */
            pthread_cond_wait(&chain->thread_state_cond, &chain->thread_state_lock);
         }

         /* If next present is not FIFO, we still need to ensure we don't override that
          * present. If FIFO, we need to ensure MSC is larger than the COMPLETED frame. */
         target_msc = chain->last_present_msc + 1;
      }

      pthread_mutex_unlock(&chain->thread_state_lock);
   }

   pthread_mutex_lock(&chain->thread_state_lock);
   x11_swapchain_result(chain, result);
   if (!chain->base.image_info.explicit_sync)
      wsi_queue_push(&chain->acquire_queue, UINT32_MAX);
   pthread_mutex_unlock(&chain->thread_state_lock);

   return NULL;
}

static uint8_t *
alloc_shm(struct wsi_image *imagew, unsigned size)
{
#ifdef HAVE_SYS_SHM_H
   struct x11_image *image = (struct x11_image *)imagew;
   image->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
   if (image->shmid < 0)
      return NULL;

   uint8_t *addr = (uint8_t *)shmat(image->shmid, 0, 0);
   /* mark the segment immediately for deletion to avoid leaks */
   shmctl(image->shmid, IPC_RMID, 0);

   if (addr == (uint8_t *) -1)
      return NULL;

   image->shmaddr = addr;
   return addr;
#else
   return NULL;
#endif
}

static VkResult
x11_image_init(VkDevice device_h, struct x11_swapchain *chain,
               const VkSwapchainCreateInfoKHR *pCreateInfo,
               const VkAllocationCallbacks* pAllocator,
               struct x11_image *image)
{
   xcb_void_cookie_t cookie;
   xcb_generic_error_t *error = NULL;
   VkResult result;
   uint32_t bpp = 32;
   int fence_fd;

   result = wsi_create_image(&chain->base, &chain->base.image_info,
                             &image->base);
   if (result != VK_SUCCESS)
      return result;

   image->update_region = xcb_generate_id(chain->conn);
   xcb_xfixes_create_region(chain->conn, image->update_region, 0, NULL);

   if (chain->base.wsi->sw) {
      if (!chain->has_mit_shm) {
         return VK_SUCCESS;
      }

      image->shmseg = xcb_generate_id(chain->conn);

      xcb_shm_attach(chain->conn,
                     image->shmseg,
                     image->shmid,
                     0);
      image->pixmap = xcb_generate_id(chain->conn);
      cookie = xcb_shm_create_pixmap_checked(chain->conn,
                                             image->pixmap,
                                             chain->window,
                                             image->base.row_pitches[0] / 4,
                                             pCreateInfo->imageExtent.height,
                                             chain->depth,
                                             image->shmseg, 0);
      xcb_discard_reply(chain->conn, cookie.sequence);
      goto out_fence;
   }
   image->pixmap = xcb_generate_id(chain->conn);

#ifdef HAVE_DRI3_MODIFIERS
   if (image->base.drm_modifier != DRM_FORMAT_MOD_INVALID) {
      /* If the image has a modifier, we must have DRI3 v1.2. */
      assert(chain->has_dri3_modifiers);

      /* XCB requires an array of file descriptors but we only have one */
      int fds[4] = { -1, -1, -1, -1 };
      for (int i = 0; i < image->base.num_planes; i++) {
         fds[i] = os_dupfd_cloexec(image->base.dma_buf_fd);
         if (fds[i] == -1) {
            for (int j = 0; j < i; j++)
               close(fds[j]);

            return VK_ERROR_OUT_OF_HOST_MEMORY;
         }
      }

      cookie =
         xcb_dri3_pixmap_from_buffers_checked(chain->conn,
                                              image->pixmap,
                                              chain->window,
                                              image->base.num_planes,
                                              pCreateInfo->imageExtent.width,
                                              pCreateInfo->imageExtent.height,
                                              image->base.row_pitches[0],
                                              image->base.offsets[0],
                                              image->base.row_pitches[1],
                                              image->base.offsets[1],
                                              image->base.row_pitches[2],
                                              image->base.offsets[2],
                                              image->base.row_pitches[3],
                                              image->base.offsets[3],
                                              chain->depth, bpp,
                                              image->base.drm_modifier,
                                              fds);
   } else
#endif
   {
      /* Without passing modifiers, we can't have multi-plane RGB images. */
      assert(image->base.num_planes == 1);

      /* XCB will take ownership of the FD we pass it. */
      int fd = os_dupfd_cloexec(image->base.dma_buf_fd);
      if (fd == -1)
         return VK_ERROR_OUT_OF_HOST_MEMORY;

      cookie =
         xcb_dri3_pixmap_from_buffer_checked(chain->conn,
                                             image->pixmap,
                                             chain->window,
                                             image->base.sizes[0],
                                             pCreateInfo->imageExtent.width,
                                             pCreateInfo->imageExtent.height,
                                             image->base.row_pitches[0],
                                             chain->depth, bpp, fd);
   }

   error = xcb_request_check(chain->conn, cookie);
   if (error != NULL) {
      free(error);
      goto fail_image;
   }

#ifdef HAVE_DRI3_EXPLICIT_SYNC
   if (chain->base.image_info.explicit_sync) {
      for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
         image->dri3_syncobj[i] = xcb_generate_id(chain->conn);
         int fd = dup(image->base.explicit_sync[i].fd);
         if (fd < 0)
            goto fail_image;

         cookie = xcb_dri3_import_syncobj_checked(chain->conn,
                                                  image->dri3_syncobj[i],
                                                  chain->window,
                                                  fd /* libxcb closes the fd */);
         error = xcb_request_check(chain->conn, cookie);
         if (error != NULL) {
            free(error);
            goto fail_image;
         }
      }
   }
#endif

out_fence:
   fence_fd = xshmfence_alloc_shm();
   if (fence_fd < 0)
      goto fail_pixmap;

   image->shm_fence = xshmfence_map_shm(fence_fd);
   if (image->shm_fence == NULL)
      goto fail_shmfence_alloc;

   image->sync_fence = xcb_generate_id(chain->conn);
   xcb_dri3_fence_from_fd(chain->conn,
                          image->pixmap,
                          image->sync_fence,
                          false,
                          fence_fd);

   xshmfence_trigger(image->shm_fence);

   return VK_SUCCESS;

fail_shmfence_alloc:
   close(fence_fd);

fail_pixmap:
   cookie = xcb_free_pixmap(chain->conn, image->pixmap);
   xcb_discard_reply(chain->conn, cookie.sequence);

fail_image:
   wsi_destroy_image(&chain->base, &image->base);

   return VK_ERROR_INITIALIZATION_FAILED;
}

static void
x11_image_finish(struct x11_swapchain *chain,
                 const VkAllocationCallbacks* pAllocator,
                 struct x11_image *image)
{
   xcb_void_cookie_t cookie;

   if (!chain->base.wsi->sw || chain->has_mit_shm) {
      cookie = xcb_sync_destroy_fence(chain->conn, image->sync_fence);
      xcb_discard_reply(chain->conn, cookie.sequence);
      xshmfence_unmap_shm(image->shm_fence);

      cookie = xcb_free_pixmap(chain->conn, image->pixmap);
      xcb_discard_reply(chain->conn, cookie.sequence);

      cookie = xcb_xfixes_destroy_region(chain->conn, image->update_region);
      xcb_discard_reply(chain->conn, cookie.sequence);

#ifdef HAVE_DRI3_EXPLICIT_SYNC
      if (chain->base.image_info.explicit_sync) {
         for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
            cookie = xcb_dri3_free_syncobj(chain->conn, image->dri3_syncobj[i]);
            xcb_discard_reply(chain->conn, cookie.sequence);
         }
      }
#endif
   }

   wsi_destroy_image(&chain->base, &image->base);
#ifdef HAVE_SYS_SHM_H
   if (image->shmaddr)
      shmdt(image->shmaddr);
#endif
}

static void
wsi_x11_recompute_dri3_modifier_hash(blake3_hash *hash, const struct wsi_drm_image_params *params)
{
   mesa_blake3 ctx;
   _mesa_blake3_init(&ctx);
   _mesa_blake3_update(&ctx, &params->num_modifier_lists, sizeof(params->num_modifier_lists));
   for (uint32_t i = 0; i < params->num_modifier_lists; i++) {
      _mesa_blake3_update(&ctx, &i, sizeof(i));
      _mesa_blake3_update(&ctx, params->modifiers[i],
                          params->num_modifiers[i] * sizeof(*params->modifiers[i]));
   }
   _mesa_blake3_update(&ctx, &params->same_gpu, sizeof(params->same_gpu));
   _mesa_blake3_final(&ctx, *hash);
}

static void
wsi_x11_get_dri3_modifiers(struct wsi_x11_connection *wsi_conn,
                           xcb_connection_t *conn, xcb_window_t window,
                           uint8_t depth, uint8_t bpp,
                           uint64_t **modifiers_in, uint32_t *num_modifiers_in,
                           uint32_t *num_tranches_in,
                           const VkAllocationCallbacks *pAllocator)
{
   if (!wsi_conn->has_dri3_modifiers)
      goto out;

#ifdef HAVE_DRI3_MODIFIERS
   xcb_generic_error_t *error = NULL;
   xcb_dri3_get_supported_modifiers_cookie_t mod_cookie =
      xcb_dri3_get_supported_modifiers(conn, window, depth, bpp);
   xcb_dri3_get_supported_modifiers_reply_t *mod_reply =
      xcb_dri3_get_supported_modifiers_reply(conn, mod_cookie, &error);
   free(error);

   if (!mod_reply || (mod_reply->num_window_modifiers == 0 &&
                      mod_reply->num_screen_modifiers == 0)) {
      free(mod_reply);
      goto out;
   }

   uint32_t n = 0;
   uint32_t counts[2];
   uint64_t *modifiers[2];

   if (mod_reply->num_window_modifiers) {
      counts[n] = mod_reply->num_window_modifiers;
      modifiers[n] = vk_alloc(pAllocator,
                              counts[n] * sizeof(uint64_t),
                              8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!modifiers[n]) {
         free(mod_reply);
         goto out;
      }

      memcpy(modifiers[n],
             xcb_dri3_get_supported_modifiers_window_modifiers(mod_reply),
             counts[n] * sizeof(uint64_t));
      n++;
   }

   if (mod_reply->num_screen_modifiers) {
      counts[n] = mod_reply->num_screen_modifiers;
      modifiers[n] = vk_alloc(pAllocator,
                              counts[n] * sizeof(uint64_t),
                              8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!modifiers[n]) {
	 if (n > 0)
            vk_free(pAllocator, modifiers[0]);
         free(mod_reply);
         goto out;
      }

      memcpy(modifiers[n],
             xcb_dri3_get_supported_modifiers_screen_modifiers(mod_reply),
             counts[n] * sizeof(uint64_t));
      n++;
   }

   for (int i = 0; i < n; i++) {
      modifiers_in[i] = modifiers[i];
      num_modifiers_in[i] = counts[i];
   }
   *num_tranches_in = n;

   free(mod_reply);
   return;
#endif
out:
   *num_tranches_in = 0;
}

static bool
wsi_x11_swapchain_query_dri3_modifiers_changed(struct x11_swapchain *chain)
{
   const struct wsi_device *wsi_device = chain->base.wsi;

   if (wsi_device->sw || !wsi_device->supports_modifiers)
      return false;

   struct wsi_drm_image_params drm_image_params;
   uint64_t *modifiers[2] = {NULL, NULL};
   uint32_t num_modifiers[2] = {0, 0};

   struct wsi_x11_connection *wsi_conn =
         wsi_x11_get_connection((struct wsi_device*)chain->base.wsi, chain->conn);

   xcb_get_geometry_reply_t *geometry =
         xcb_get_geometry_reply(chain->conn, xcb_get_geometry(chain->conn, chain->window), NULL);
   if (geometry == NULL)
      return false;
   uint32_t bit_depth = geometry->depth;
   free(geometry);

   drm_image_params = (struct wsi_drm_image_params){
      .base.image_type = WSI_IMAGE_TYPE_DRM,
      .same_gpu = wsi_x11_check_dri3_compatible(wsi_device, chain->conn),
      .explicit_sync = chain->base.image_info.explicit_sync,
   };

   wsi_x11_get_dri3_modifiers(wsi_conn, chain->conn, chain->window, bit_depth, 32,
                              modifiers, num_modifiers,
                              &drm_image_params.num_modifier_lists,
                              &wsi_device->instance_alloc);

   drm_image_params.num_modifiers = num_modifiers;
   drm_image_params.modifiers = (const uint64_t **)modifiers;

   blake3_hash hash;
   wsi_x11_recompute_dri3_modifier_hash(&hash, &drm_image_params);

   for (int i = 0; i < ARRAY_SIZE(modifiers); i++)
      vk_free(&wsi_device->instance_alloc, modifiers[i]);

   return memcmp(hash, chain->dri3_modifier_hash, sizeof(hash)) != 0;
}

static VkResult
x11_swapchain_destroy(struct wsi_swapchain *anv_chain,
                      const VkAllocationCallbacks *pAllocator)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)anv_chain;
   xcb_void_cookie_t cookie;

   pthread_mutex_lock(&chain->thread_state_lock);
   chain->status = VK_ERROR_OUT_OF_DATE_KHR;
   pthread_cond_broadcast(&chain->thread_state_cond);
   pthread_mutex_unlock(&chain->thread_state_lock);

   /* Push a UINT32_MAX to wake up the manager */
   wsi_queue_push(&chain->present_queue, UINT32_MAX);
   pthread_join(chain->queue_manager, NULL);
   pthread_join(chain->event_manager, NULL);

   if (!chain->base.image_info.explicit_sync)
      wsi_queue_destroy(&chain->acquire_queue);
   wsi_queue_destroy(&chain->present_queue);

   for (uint32_t i = 0; i < chain->base.image_count; i++)
      x11_image_finish(chain, pAllocator, &chain->images[i]);

   xcb_unregister_for_special_event(chain->conn, chain->special_event);
   cookie = xcb_present_select_input_checked(chain->conn, chain->event_id,
                                             chain->window,
                                             XCB_PRESENT_EVENT_MASK_NO_EVENT);
   xcb_discard_reply(chain->conn, cookie.sequence);

   pthread_mutex_destroy(&chain->present_progress_mutex);
   pthread_cond_destroy(&chain->present_progress_cond);
   pthread_mutex_destroy(&chain->thread_state_lock);
   pthread_cond_destroy(&chain->thread_state_cond);

   wsi_swapchain_finish(&chain->base);

   vk_free(pAllocator, chain);

   return VK_SUCCESS;
}

static void
wsi_x11_set_adaptive_sync_property(xcb_connection_t *conn,
                                   xcb_drawable_t drawable,
                                   uint32_t state)
{
   static char const name[] = "_VARIABLE_REFRESH";
   xcb_intern_atom_cookie_t cookie;
   xcb_intern_atom_reply_t* reply;
   xcb_void_cookie_t check;

   cookie = xcb_intern_atom(conn, 0, strlen(name), name);
   reply = xcb_intern_atom_reply(conn, cookie, NULL);
   if (reply == NULL)
      return;

   if (state)
      check = xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE,
                                          drawable, reply->atom,
                                          XCB_ATOM_CARDINAL, 32, 1, &state);
   else
      check = xcb_delete_property_checked(conn, drawable, reply->atom);

   xcb_discard_reply(conn, check.sequence);
   free(reply);
}

static VkResult x11_wait_for_present(struct wsi_swapchain *wsi_chain,
                                     uint64_t waitValue,
                                     uint64_t timeout)
{
   struct x11_swapchain *chain = (struct x11_swapchain *)wsi_chain;
   struct timespec abs_timespec;
   uint64_t abs_timeout = 0;
   if (timeout != 0)
      abs_timeout = os_time_get_absolute_timeout(timeout);

   /* Need to observe that the swapchain semaphore has been unsignalled,
    * as this is guaranteed when a present is complete. */
   VkResult result = wsi_swapchain_wait_for_present_semaphore(
         &chain->base, waitValue, timeout);
   if (result != VK_SUCCESS)
      return result;

   timespec_from_nsec(&abs_timespec, abs_timeout);

   pthread_mutex_lock(&chain->present_progress_mutex);
   while (chain->present_id < waitValue) {
      int ret = pthread_cond_timedwait(&chain->present_progress_cond,
                                       &chain->present_progress_mutex,
                                       &abs_timespec);
      if (ret == ETIMEDOUT) {
         result = VK_TIMEOUT;
         break;
      }
      if (ret) {
         result = VK_ERROR_DEVICE_LOST;
         break;
      }
   }
   if (result == VK_SUCCESS && chain->present_progress_error)
      result = chain->present_progress_error;
   pthread_mutex_unlock(&chain->present_progress_mutex);
   return result;
}

static unsigned
x11_get_min_image_count_for_present_mode(struct wsi_device *wsi_device,
                                         struct wsi_x11_connection *wsi_conn,
                                         VkPresentModeKHR present_mode)
{
   uint32_t min_image_count = x11_get_min_image_count(wsi_device, wsi_conn->is_xwayland);
   if (x11_requires_mailbox_image_count(wsi_device, wsi_conn, present_mode))
      return MAX2(min_image_count, X11_SWAPCHAIN_MAILBOX_IMAGES);
   else
      return min_image_count;
}

/**
 * Create the swapchain.
 *
 * Supports immediate, fifo and mailbox presentation mode.
 *
 */
static VkResult
x11_surface_create_swapchain(VkIcdSurfaceBase *icd_surface,
                             VkDevice device,
                             struct wsi_device *wsi_device,
                             const VkSwapchainCreateInfoKHR *pCreateInfo,
                             const VkAllocationCallbacks* pAllocator,
                             struct wsi_swapchain **swapchain_out)
{
   struct x11_swapchain *chain;
   xcb_void_cookie_t cookie;
   VkResult result;
   VkPresentModeKHR present_mode = wsi_swapchain_get_present_mode(wsi_device, pCreateInfo);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   /* Get xcb connection from the icd_surface and from that our internal struct
    * representing it.
    */
   xcb_connection_t *conn = x11_surface_get_connection(icd_surface);
   struct wsi_x11_connection *wsi_conn =
      wsi_x11_get_connection(wsi_device, conn);
   if (!wsi_conn)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* Get number of images in our swapchain. This count depends on:
    * - requested minimal image count
    * - device characteristics
    * - presentation mode.
    */
   unsigned num_images = pCreateInfo->minImageCount;
   if (!wsi_device->x11.strict_imageCount) {
      if (x11_requires_mailbox_image_count(wsi_device, wsi_conn, present_mode) ||
          wsi_device->x11.ensure_minImageCount) {
         unsigned present_mode_images = x11_get_min_image_count_for_present_mode(
               wsi_device, wsi_conn, pCreateInfo->presentMode);
         num_images = MAX2(num_images, present_mode_images);
      }
   }

   /* Check that we have a window up-front. It is an error to not have one. */
   xcb_window_t window = x11_surface_get_window(icd_surface);

   /* Get the geometry of that window. The bit depth of the swapchain will be fitted and the
    * chain's images extents should fit it for performance-optimizing flips.
    */
   xcb_get_geometry_reply_t *geometry =
      xcb_get_geometry_reply(conn, xcb_get_geometry(conn, window), NULL);
   if (geometry == NULL)
      return VK_ERROR_SURFACE_LOST_KHR;
   const uint32_t bit_depth = geometry->depth;
   const uint16_t cur_width = geometry->width;
   const uint16_t cur_height = geometry->height;
   free(geometry);

   /* Allocate the actual swapchain. The size depends on image count. */
   size_t size = sizeof(*chain) + num_images * sizeof(chain->images[0]);
   chain = vk_zalloc(pAllocator, size, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (chain == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   int ret = pthread_mutex_init(&chain->present_progress_mutex, NULL);
   if (ret != 0) {
      vk_free(pAllocator, chain);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   ret = pthread_mutex_init(&chain->thread_state_lock, NULL);
   if (ret != 0) {
      pthread_mutex_destroy(&chain->present_progress_mutex);
      vk_free(pAllocator, chain);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   ret = pthread_cond_init(&chain->thread_state_cond, NULL);
   if (ret != 0) {
      pthread_mutex_destroy(&chain->present_progress_mutex);
      pthread_mutex_destroy(&chain->thread_state_lock);
      vk_free(pAllocator, chain);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   bool bret = wsi_init_pthread_cond_monotonic(&chain->present_progress_cond);
   if (!bret) {
      pthread_mutex_destroy(&chain->present_progress_mutex);
      pthread_mutex_destroy(&chain->thread_state_lock);
      pthread_cond_destroy(&chain->thread_state_cond);
      vk_free(pAllocator, chain);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   uint32_t present_caps = 0;
   xcb_present_query_capabilities_cookie_t present_query_cookie;
   xcb_present_query_capabilities_reply_t *present_query_reply;
   present_query_cookie = xcb_present_query_capabilities(conn, window);
   present_query_reply = xcb_present_query_capabilities_reply(conn, present_query_cookie, NULL);
   if (present_query_reply) {
      present_caps = present_query_reply->capabilities;
      free(present_query_reply);
   }

   struct wsi_base_image_params *image_params = NULL;
   struct wsi_cpu_image_params cpu_image_params;
   struct wsi_drm_image_params drm_image_params;
   uint64_t *modifiers[2] = {NULL, NULL};
   uint32_t num_modifiers[2] = {0, 0};
   if (wsi_device->sw) {
      cpu_image_params = (struct wsi_cpu_image_params) {
         .base.image_type = WSI_IMAGE_TYPE_CPU,
         .alloc_shm = wsi_conn->has_mit_shm ? &alloc_shm : NULL,
      };
      image_params = &cpu_image_params.base;
   } else {
      drm_image_params = (struct wsi_drm_image_params) {
         .base.image_type = WSI_IMAGE_TYPE_DRM,
         .same_gpu = wsi_x11_check_dri3_compatible(wsi_device, conn),
         .explicit_sync =
#ifdef HAVE_DRI3_EXPLICIT_SYNC
            wsi_conn->has_dri3_explicit_sync &&
            (present_caps & XCB_PRESENT_CAPABILITY_SYNCOBJ) &&
            wsi_device_supports_explicit_sync(wsi_device),
#else
            false,
#endif
      };
      if (wsi_device->supports_modifiers) {
         wsi_x11_get_dri3_modifiers(wsi_conn, conn, window, bit_depth, 32,
                                    modifiers, num_modifiers,
                                    &drm_image_params.num_modifier_lists,
                                    pAllocator);
         drm_image_params.num_modifiers = num_modifiers;
         drm_image_params.modifiers = (const uint64_t **)modifiers;

         wsi_x11_recompute_dri3_modifier_hash(&chain->dri3_modifier_hash, &drm_image_params);
      }
      image_params = &drm_image_params.base;
   }

   result = wsi_swapchain_init(wsi_device, &chain->base, device, pCreateInfo,
                               image_params, pAllocator);

   for (int i = 0; i < ARRAY_SIZE(modifiers); i++)
      vk_free(pAllocator, modifiers[i]);

   if (result != VK_SUCCESS)
      goto fail_alloc;

   chain->base.destroy = x11_swapchain_destroy;
   chain->base.get_wsi_image = x11_get_wsi_image;
   chain->base.acquire_next_image = x11_acquire_next_image;
   chain->base.queue_present = x11_queue_present;
   chain->base.wait_for_present = x11_wait_for_present;
   chain->base.release_images = x11_release_images;
   chain->base.set_present_mode = x11_set_present_mode;
   chain->base.present_mode = present_mode;
   chain->base.image_count = num_images;
   chain->conn = conn;
   chain->window = window;
   chain->depth = bit_depth;
   chain->extent = pCreateInfo->imageExtent;
   chain->send_sbc = 0;
   chain->sent_image_count = 0;
   chain->last_present_msc = 0;
   chain->status = VK_SUCCESS;
   chain->has_dri3_modifiers = wsi_conn->has_dri3_modifiers;
   chain->has_mit_shm = wsi_conn->has_mit_shm;
   chain->has_async_may_tear = present_caps & XCB_PRESENT_CAPABILITY_ASYNC_MAY_TEAR;

   /* When images in the swapchain don't fit the window, X can still present them, but it won't
    * happen by flip, only by copy. So this is a suboptimal copy, because if the client would change
    * the chain extents X may be able to flip
    */
   if (!wsi_device->x11.ignore_suboptimal) {
      if (chain->extent.width != cur_width || chain->extent.height != cur_height)
         chain->status = VK_SUBOPTIMAL_KHR;
   }

   /* On a new swapchain this helper variable is set to false. Once we present it will have an
    * impact once we ever do at least one flip and go back to copying afterwards. It is presumed
    * that in this case here is a high likelihood X could do flips again if the client reallocates a
    * new swapchain.
    *
    * Note that we used to inheritted this property from 'pCreateInfo->oldSwapchain'. But when it
    * was true, and when the next present was completed with copying, we would return
    * VK_SUBOPTIMAL_KHR and hint the app to reallocate again for no good reason. If all following
    * presents on the surface were completed with copying because of some surface state change, we
    * would always return VK_SUBOPTIMAL_KHR no matter how many times the app had reallocated.
    *
    * Note also that is is questionable in general if that mechanism is really useful. It ist not
    * clear why on a change from flipping to copying we can assume a reallocation has a high chance
    * of making flips work again per se. In other words it is not clear why there is need for
    * another way to inform clients about suboptimal copies besides forwarding the
    * 'PresentOptionSuboptimal' complete mode.
    */
   chain->copy_is_suboptimal = false;

   /* For our swapchain we need to listen to following Present extension events:
    * - Configure: Window dimensions changed. Images in the swapchain might need
    *              to be reallocated.
    * - Complete: An image from our swapchain was presented on the output.
    * - Idle: An image from our swapchain is not anymore accessed by the X
    *         server and can be reused.
    */
   chain->event_id = xcb_generate_id(chain->conn);
   uint32_t event_mask = XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY |
                         XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY;
   if (!chain->base.image_info.explicit_sync)
      event_mask |= XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY;
   xcb_present_select_input(chain->conn, chain->event_id, chain->window, event_mask);

   /* Create an XCB event queue to hold present events outside of the usual
    * application event queue
    */
   chain->special_event =
      xcb_register_for_special_xge(chain->conn, &xcb_present_id,
                                   chain->event_id, NULL);

   /* Create the graphics context. */
   chain->gc = xcb_generate_id(chain->conn);
   if (!chain->gc) {
      /* FINISHME: Choose a better error. */
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail_register;
   }

   cookie = xcb_create_gc(chain->conn,
                          chain->gc,
                          chain->window,
                          XCB_GC_GRAPHICS_EXPOSURES,
                          (uint32_t []) { 0 });
   xcb_discard_reply(chain->conn, cookie.sequence);

   uint32_t image = 0;
   for (; image < chain->base.image_count; image++) {
      result = x11_image_init(device, chain, pCreateInfo, pAllocator,
                              &chain->images[image]);
      if (result != VK_SUCCESS)
         goto fail_init_images;
   }

   /* The queues have a length of base.image_count + 1 because we will
    * occasionally use UINT32_MAX to signal the other thread that an error
    * has occurred and we don't want an overflow.
    */
   ret = wsi_queue_init(&chain->present_queue, chain->base.image_count + 1);
   if (ret) {
      goto fail_init_images;
   }

   /* Acquire queue is only needed when using implicit sync */
   if (!chain->base.image_info.explicit_sync) {
      ret = wsi_queue_init(&chain->acquire_queue, chain->base.image_count + 1);
      if (ret) {
         wsi_queue_destroy(&chain->present_queue);
         goto fail_init_images;
      }

      for (unsigned i = 0; i < chain->base.image_count; i++)
         wsi_queue_push(&chain->acquire_queue, i);
   }

   ret = pthread_create(&chain->queue_manager, NULL,
                        x11_manage_present_queue, chain);
   if (ret)
      goto fail_init_fifo_queue;

   ret = pthread_create(&chain->event_manager, NULL,
                        x11_manage_event_queue, chain);
   if (ret)
      goto fail_init_event_queue;

   /* It is safe to set it here as only one swapchain can be associated with
    * the window, and swapchain creation does the association. At this point
    * we know the creation is going to succeed. */
   wsi_x11_set_adaptive_sync_property(conn, window,
                                      wsi_device->enable_adaptive_sync);

   *swapchain_out = &chain->base;

   return VK_SUCCESS;

fail_init_event_queue:
   /* Push a UINT32_MAX to wake up the manager */
   wsi_queue_push(&chain->present_queue, UINT32_MAX);
   pthread_join(chain->queue_manager, NULL);

fail_init_fifo_queue:
   wsi_queue_destroy(&chain->present_queue);
   if (!chain->base.image_info.explicit_sync)
      wsi_queue_destroy(&chain->acquire_queue);

fail_init_images:
   for (uint32_t j = 0; j < image; j++)
      x11_image_finish(chain, pAllocator, &chain->images[j]);

fail_register:
   xcb_unregister_for_special_event(chain->conn, chain->special_event);

   wsi_swapchain_finish(&chain->base);

fail_alloc:
   vk_free(pAllocator, chain);

   return result;
}

VkResult
wsi_x11_init_wsi(struct wsi_device *wsi_device,
                 const VkAllocationCallbacks *alloc,
                 const struct driOptionCache *dri_options)
{
   struct wsi_x11 *wsi;
   VkResult result;

   wsi = vk_alloc(alloc, sizeof(*wsi), 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!wsi) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail;
   }

   int ret = pthread_mutex_init(&wsi->mutex, NULL);
   if (ret != 0) {
      if (ret == ENOMEM) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
      } else {
         /* FINISHME: Choose a better error. */
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
      }

      goto fail_alloc;
   }

   wsi->connections = _mesa_hash_table_create(NULL, _mesa_hash_pointer,
                                              _mesa_key_pointer_equal);
   if (!wsi->connections) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail_mutex;
   }

   if (dri_options) {
      if (driCheckOption(dri_options, "vk_x11_override_min_image_count", DRI_INT)) {
         wsi_device->x11.override_minImageCount =
            driQueryOptioni(dri_options, "vk_x11_override_min_image_count");
      }
      if (driCheckOption(dri_options, "vk_x11_strict_image_count", DRI_BOOL)) {
         wsi_device->x11.strict_imageCount =
            driQueryOptionb(dri_options, "vk_x11_strict_image_count");
      }
      if (driCheckOption(dri_options, "vk_x11_ensure_min_image_count", DRI_BOOL)) {
         wsi_device->x11.ensure_minImageCount =
            driQueryOptionb(dri_options, "vk_x11_ensure_min_image_count");
      }
      wsi_device->x11.xwaylandWaitReady = true;
      if (driCheckOption(dri_options, "vk_xwayland_wait_ready", DRI_BOOL)) {
         wsi_device->x11.xwaylandWaitReady =
            driQueryOptionb(dri_options, "vk_xwayland_wait_ready");
      }

      if (driCheckOption(dri_options, "vk_x11_ignore_suboptimal", DRI_BOOL)) {
         wsi_device->x11.ignore_suboptimal =
            driQueryOptionb(dri_options, "vk_x11_ignore_suboptimal");
      }
   }

   wsi->base.get_support = x11_surface_get_support;
   wsi->base.get_capabilities2 = x11_surface_get_capabilities2;
   wsi->base.get_formats = x11_surface_get_formats;
   wsi->base.get_formats2 = x11_surface_get_formats2;
   wsi->base.get_present_modes = x11_surface_get_present_modes;
   wsi->base.get_present_rectangles = x11_surface_get_present_rectangles;
   wsi->base.create_swapchain = x11_surface_create_swapchain;

   wsi_device->wsi[VK_ICD_WSI_PLATFORM_XCB] = &wsi->base;
   wsi_device->wsi[VK_ICD_WSI_PLATFORM_XLIB] = &wsi->base;

   return VK_SUCCESS;

fail_mutex:
   pthread_mutex_destroy(&wsi->mutex);
fail_alloc:
   vk_free(alloc, wsi);
fail:
   wsi_device->wsi[VK_ICD_WSI_PLATFORM_XCB] = NULL;
   wsi_device->wsi[VK_ICD_WSI_PLATFORM_XLIB] = NULL;

   return result;
}

void
wsi_x11_finish_wsi(struct wsi_device *wsi_device,
                   const VkAllocationCallbacks *alloc)
{
   struct wsi_x11 *wsi =
      (struct wsi_x11 *)wsi_device->wsi[VK_ICD_WSI_PLATFORM_XCB];

   if (wsi) {
      hash_table_foreach(wsi->connections, entry)
         wsi_x11_connection_destroy(wsi_device, entry->data);

      _mesa_hash_table_destroy(wsi->connections, NULL);

      pthread_mutex_destroy(&wsi->mutex);

      vk_free(alloc, wsi);
   }
}
