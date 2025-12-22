/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QuartzCore/QuartzCore.h>

#include <gst/gst.h>

#include <gst/vulkan/vulkan.h>

#include "gstvkwindow_ios.h"
#include "gstvkdisplay_ios.h"

#include "gstvkios_utils.h"

#define GET_PRIV(o) gst_vulkan_window_ios_get_instance_private (o)

#define GST_CAT_DEFAULT gst_vulkan_window_ios_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanwindowios", 0,
        "Vulkan iOS Window");
    g_once_init_leave (&_init, 1);
  }
}

enum
{
  PROP_0,
};

struct _GstVulkanWindowIosPrivate
{
  gpointer internal_view;
  gpointer internal_layer;
  gpointer external_view;

  gint preferred_width;
  gint preferred_height;

  GMutex lock;
  GCond cond;
};

#define gst_vulkan_window_ios_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanWindowIos, gst_vulkan_window_ios,
    GST_TYPE_VULKAN_WINDOW, G_ADD_PRIVATE (GstVulkanWindowIos) _init_debug ());

static VkSurfaceKHR gst_vulkan_window_ios_get_surface (GstVulkanWindow * window,
    GError ** error);
static gboolean gst_vulkan_window_ios_get_presentation_support (GstVulkanWindow
    * window, GstVulkanDevice * device, guint32 queue_family_idx);
static gboolean gst_vulkan_window_ios_open (GstVulkanWindow * window,
    GError ** error);
static void gst_vulkan_window_ios_close (GstVulkanWindow * window);
static void gst_vulkan_window_ios_set_window_handle (GstVulkanWindow * window,
    guintptr window_handle);

static void
gst_vulkan_window_ios_finalize (GObject * object)
{
  GstVulkanWindowIos *window_ios = GST_VULKAN_WINDOW_IOS (object);
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);

  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vulkan_window_ios_class_init (GstVulkanWindowIosClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  GstVulkanWindowClass *window_class = (GstVulkanWindowClass *) klass;

  obj_class->finalize = gst_vulkan_window_ios_finalize;

  window_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_window_ios_open);
  window_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_window_ios_close);
  window_class->get_surface = gst_vulkan_window_ios_get_surface;
  window_class->get_presentation_support =
      gst_vulkan_window_ios_get_presentation_support;
  window_class->set_window_handle =
      gst_vulkan_window_ios_set_window_handle;
}

static void
gst_vulkan_window_ios_init (GstVulkanWindowIos * window_ios)
{
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);

  priv->preferred_width = 320;
  priv->preferred_height = 240;

  g_mutex_init (&priv->lock);
  g_cond_init (&priv->cond);
}

/* Must be called in the gl thread */
GstVulkanWindowIos *
gst_vulkan_window_ios_new (GstVulkanDisplay * display)
{
  GstVulkanWindowIos *window;

  _init_debug ();

  if ((gst_vulkan_display_get_handle_type (display) &
          GST_VULKAN_DISPLAY_TYPE_IOS)
      == GST_VULKAN_DISPLAY_TYPE_NONE) {
    GST_INFO ("Wrong display type %u for this window type %u", display->type,
        GST_VULKAN_DISPLAY_TYPE_IOS);
    return NULL;
  }

  window = g_object_new (GST_TYPE_VULKAN_WINDOW_IOS, NULL);
  gst_object_ref_sink (window);

  return window;
}

static void
_create_window (GstVulkanWindowIos * window_ios)
{
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);
  UIView *external_view = (__bridge UIView *) priv->external_view;
  CGRect rect = CGRectMake (0, 0, external_view.frame.size.width, external_view.frame.size.height);
  GstVulkanUIView *view;

  view = [[GstVulkanUIView alloc] initWithFrame:rect];
  [view setGstWindow:window_ios];
  view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  view.contentMode = UIViewContentModeRedraw;

  g_mutex_lock (&priv->lock);

  priv->internal_view = (__bridge_retained gpointer) view;
  [external_view addSubview:view];
  priv->internal_layer = (__bridge_retained gpointer) [view layer];

  g_cond_broadcast (&priv->cond);
  g_mutex_unlock (&priv->lock);
}

gboolean
gst_vulkan_window_ios_create_window (GstVulkanWindowIos * window_ios)
{
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);

  if (!priv->external_view) {
    GST_WARNING_OBJECT(window_ios, "No external UIView provided");
    return FALSE;
  }

  _gst_vk_invoke_on_main ((GstVulkanWindowFunc) _create_window,
      gst_object_ref (window_ios), gst_object_unref);

  /* XXX: Maybe we need an async create_window/get_surface()? */
  g_mutex_lock (&priv->lock);
  while (!priv->internal_view)
    g_cond_wait (&priv->cond, &priv->lock);
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

static VkSurfaceKHR
gst_vulkan_window_ios_get_surface (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowIos *window_ios = GST_VULKAN_WINDOW_IOS (window);
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);
  VkIOSSurfaceCreateInfoMVK info = { 0, };
  VkSurfaceKHR ret;
  VkResult err;

  if (!priv->internal_layer) {
    g_set_error_literal (error, GST_VULKAN_ERROR,
        VK_ERROR_INITIALIZATION_FAILED,
        "No layer to retrieve surface for. Has create_window() been called?");
    return VK_NULL_HANDLE;
  }

  info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
  info.pNext = NULL;
  info.flags = 0;
  info.pView = priv->internal_layer;

  if (!window_ios->CreateIOSSurface)
    window_ios->CreateIOSSurface =
        gst_vulkan_instance_get_proc_address (window->display->instance,
        "vkCreateIOSSurfaceMVK");
  if (!window_ios->CreateIOSSurface) {
    g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_FEATURE_NOT_PRESENT,
        "Could not retrieve \"vkCreateIOSSurfaceMVK\" function pointer");
    return VK_NULL_HANDLE;
  }

  err =
      window_ios->CreateIOSSurface (window->display->instance->instance, &info,
      NULL, &ret);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateIOSSurfaceMVK") < 0)
    return VK_NULL_HANDLE;

  return ret;
}

static gboolean
gst_vulkan_window_ios_get_presentation_support (GstVulkanWindow * window,
    GstVulkanDevice * device, guint32 queue_family_idx)
{
  return TRUE;
}

static gboolean
gst_vulkan_window_ios_open (GstVulkanWindow * window, GError ** error)
{
  GstVulkanWindowIos *window_ios = GST_VULKAN_WINDOW_IOS (window);

  if (!GST_VULKAN_WINDOW_CLASS (parent_class)->open (window, error))
    return FALSE;

  return gst_vulkan_window_ios_create_window (window_ios);
}

static void
gst_vulkan_window_ios_close (GstVulkanWindow * window)
{
  GstVulkanWindowIos *window_ios = GST_VULKAN_WINDOW_IOS (window);
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);
  GstVulkanUIView *view = (__bridge GstVulkanUIView *) priv->internal_view;

  [view setGstWindow:NULL];
  CFBridgingRelease (priv->internal_view);
  priv->internal_view = NULL;
  CFBridgingRelease (priv->internal_layer);
  priv->internal_layer = NULL;

  GST_VULKAN_WINDOW_CLASS (parent_class)->close (window);
}

static void
gst_vulkan_window_ios_set_window_handle (GstVulkanWindow * window,
    guintptr window_handle)
{
  GstVulkanWindowIos *window_ios = GST_VULKAN_WINDOW_IOS (window);
  GstVulkanWindowIosPrivate *priv = GET_PRIV (window_ios);
  gpointer view = (gpointer) window_handle;

  g_return_if_fail (view != NULL);

  if (priv->external_view && priv->external_view != view) {
    GST_FIXME_OBJECT (window_ios, "View changes are not implemented");
    return;
  }

  priv->external_view = view;
}

@implementation GstVulkanUIView {
    GstVulkanWindowIos * window_ios;

    guint width;
    guint height;
};

+(Class) layerClass
{
  return [CAMetalLayer class];
}

-(void) setGstWindow:(GstVulkanWindowIos *) window
{
  window_ios = window;
}

-(void) layoutSubViews
{
  [super layoutSubviews];
  CGSize rect = self.bounds.size;
  GST_ERROR ("%ix%i", (int) rect.width, (int) rect.height);
  gboolean resize = self->width != rect.width || self->height != rect.height;
  self->width = rect.width;
  self->height = rect.height;
  if (resize && self->window_ios) {
    gst_vulkan_window_resize (GST_VULKAN_WINDOW (self->window_ios), rect.width, rect.height);
  }
}

@end

void
_gst_vk_invoke_on_main (GstVulkanWindowFunc func, gpointer data, GDestroyNotify notify)
{
  if ([NSThread isMainThread]) {
    func (data);
    if (notify)
      notify (data);
  } else {
    dispatch_async (dispatch_get_main_queue (), ^{
      func (data);
      if (notify)
        notify (data);
    });
  }
}

