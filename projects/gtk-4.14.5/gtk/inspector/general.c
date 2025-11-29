/*
 * Copyright (c) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "general.h"
#include "window.h"

#include "gtkdebug.h"
#include "gtklabel.h"
#include "gtkscale.h"
#include "gtkswitch.h"
#include "gtklistbox.h"
#include "gtkprivate.h"
#include "gtksizegroup.h"
#include "gtkimage.h"
#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkbinlayout.h"
#include "gtkmediafileprivate.h"
#include "gtkimmoduleprivate.h"

#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdisplayprivate.h"

#include "profile_conf.h"

#include <epoxy/gl.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <epoxy/glx.h>
#include <epoxy/egl.h>
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#include "gdkglcontextprivate.h"
#include <epoxy/wgl.h>
#ifdef GDK_WIN32_ENABLE_EGL
#include <epoxy/egl.h>
#endif
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include <epoxy/egl.h>
#include <xkbcommon/xkbcommon.h>
#include "wayland/gdkdisplay-wayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

struct _GtkInspectorGeneral
{
  GtkWidget parent;

  GtkWidget *swin;
  GtkWidget *box;
  GtkWidget *version_box;
  GtkWidget *env_box;
  GtkWidget *display_box;
  GtkWidget *display_extensions_row;
  GtkWidget *display_extensions_box;
  GtkWidget *monitor_box;
  GtkWidget *gl_box;
  GtkWidget *gl_extensions_row;
  GtkWidget *gl_extensions_box;
  GtkWidget *vulkan_box;
  GtkWidget *vulkan_extensions_row;
  GtkWidget *vulkan_extensions_box;
  GtkWidget *device_box;
  GtkWidget *gtk_version;
  GtkWidget *gdk_backend;
  GtkWidget *gsk_renderer;
  GtkWidget *pango_fontmap;
  GtkWidget *media_backend;
  GtkWidget *im_module;
  GtkWidget *gl_backend_version;
  GtkWidget *gl_backend_version_row;
  GtkWidget *gl_backend_vendor;
  GtkWidget *gl_backend_vendor_row;
  GtkWidget *gl_error;
  GtkWidget *gl_error_row;
  GtkWidget *gl_version;
  GtkWidget *gl_vendor;
  GtkWidget *gl_vendor_row;
  GtkWidget *gl_renderer;
  GtkWidget *gl_renderer_row;
  GtkWidget *gl_full_version;
  GtkWidget *gl_full_version_row;
  GtkWidget *glsl_version;
  GtkWidget *glsl_version_row;
  GtkWidget *vk_device;
  GtkWidget *vk_api_version;
  GtkWidget *vk_driver_version;
  GtkWidget *app_id_box;
  GtkWidget *app_id;
  GtkWidget *resource_path;
  GtkWidget *prefix;
  GtkWidget *xdg_data_home;
  GtkWidget *xdg_data_dirs;
  GtkWidget *gtk_path;
  GtkWidget *gtk_exe_prefix;
  GtkWidget *gtk_data_prefix;
  GtkWidget *gsettings_schema_dir;
  GtkWidget *display_name;
  GtkWidget *display_rgba;
  GtkWidget *display_composited;

  GdkDisplay *display;
};

typedef struct _GtkInspectorGeneralClass
{
  GtkWidgetClass parent_class;
} GtkInspectorGeneralClass;

G_DEFINE_TYPE (GtkInspectorGeneral, gtk_inspector_general, GTK_TYPE_WIDGET)

static void
init_version (GtkInspectorGeneral *gen)
{
  const char *backend;
  GdkSurface *surface;
  GskRenderer *gsk_renderer;
  const char *renderer;

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gen->display))
    backend = "X11";
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gen->display))
    backend = "Wayland";
  else
#endif
#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (gen->display))
    backend = "Broadway";
  else
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (gen->display))
    backend = "Windows";
  else
#endif
#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (gen->display))
    backend = "MacOS";
  else
#endif
    backend = "Unknown";

  surface = gdk_surface_new_toplevel (gen->display);
  gsk_renderer = gsk_renderer_new_for_surface (surface);
  if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskVulkanRenderer") == 0)
    renderer = "Vulkan";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskGLRenderer") == 0)
    renderer = "GL";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskCairoRenderer") == 0)
    renderer = "Cairo";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskNglRenderer") == 0)
    renderer = "GL (new)";
  else
    renderer = "Unknown";

  gsk_renderer_unrealize (gsk_renderer);
  g_object_unref (gsk_renderer);
  gdk_surface_destroy (surface);

  if (g_strcmp0 (PROFILE, "devel") == 0)
    {
      char *version = g_strdup_printf ("%s-%s", GTK_VERSION, VCS_TAG);
      gtk_label_set_text (GTK_LABEL (gen->gtk_version), version);
      g_free (version);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (gen->gtk_version), GTK_VERSION);
    }
  gtk_label_set_text (GTK_LABEL (gen->gdk_backend), backend);
  gtk_label_set_text (GTK_LABEL (gen->gsk_renderer), renderer);
}

static void
init_app_id (GtkInspectorGeneral *gen)
{
  GApplication *app;

  app = g_application_get_default ();
  if (!app)
    {
      gtk_widget_set_visible (gen->app_id_box, FALSE);
      return;
    }

  gtk_label_set_text (GTK_LABEL (gen->app_id),
                      g_application_get_application_id (app));
  gtk_label_set_text (GTK_LABEL (gen->resource_path),
                      g_application_get_resource_base_path (app));
}

static G_GNUC_UNUSED void
add_check_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const char          *name,
               gboolean             value,
               int                  indent)
{
  GtkWidget *row, *box, *label, *check;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin-start", indent, NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  check = gtk_image_new_from_icon_name ("object-select-symbolic");
  gtk_widget_set_halign (check, GTK_ALIGN_END);
  gtk_widget_set_valign (check, GTK_ALIGN_BASELINE_FILL);
  gtk_widget_set_opacity (check, value ? 1.0 : 0.0);
  gtk_box_append (GTK_BOX (box), check);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (list, row, -1);
}

static void
add_label_row (GtkInspectorGeneral *gen,
               GtkListBox          *list,
               const char          *name,
               const char          *value,
               int                  indent)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *row;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin-start", indent, NULL);

  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new (value);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 25);
  gtk_box_append (GTK_BOX (box), label);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_widget_set_hexpand (box, FALSE);
  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
}

static void
append_gl_extension_row (GtkInspectorGeneral *gen,
                         const char          *ext)
{
  add_check_row (gen, GTK_LIST_BOX (gen->gl_extensions_box), ext, epoxy_has_gl_extension (ext), 0);
}

#ifdef GDK_WINDOWING_X11
static void
append_glx_extension_row (GtkInspectorGeneral *gen,
                          Display             *dpy,
                          const char          *ext)
{
  add_check_row (gen, GTK_LIST_BOX (gen->gl_extensions_box), ext, epoxy_has_glx_extension (dpy, 0, ext), 0);
}
#endif

#ifdef GDK_WINDOWING_WIN32
static void
append_wgl_extension_row (GtkInspectorGeneral *gen,
                          const char          *ext)
{
  HDC hdc = 0;

  add_check_row (gen, GTK_LIST_BOX (gen->gl_extensions_box), ext, epoxy_has_wgl_extension (hdc, ext), 0);
}
#endif

#if defined(GDK_WINDOWING_WAYLAND) || defined (GDK_WINDOWING_X11) || (defined (GDK_WINDOWING_WIN32) && defined(GDK_WIN32_ENABLE_EGL))
static void
append_egl_extension_row (GtkInspectorGeneral *gen,
                          EGLDisplay          dpy,
                          const char          *ext)
{
  add_check_row (gen, GTK_LIST_BOX (gen->gl_extensions_box), ext, epoxy_has_egl_extension (dpy, ext), 0);
}

static EGLDisplay
get_egl_display (GdkDisplay *display)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return gdk_wayland_display_get_egl_display (display);
  else
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    return gdk_x11_display_get_egl_display (display);
  else
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    return gdk_win32_display_get_egl_display (display);
  else
#endif
   return NULL;
}
#endif

static void
init_gl (GtkInspectorGeneral *gen)
{
  GdkGLContext *context;
  GError *error = NULL;
  int major, minor;
  char *s;

  if (!gdk_display_prepare_gl (gen->display, &error))
    {
      gtk_label_set_text (GTK_LABEL (gen->gl_version), C_("GL version", "None"));
      gtk_widget_set_visible (gen->gl_error_row, TRUE);
      gtk_widget_set_visible (gen->gl_backend_version_row, FALSE);
      gtk_widget_set_visible (gen->gl_backend_vendor_row, FALSE);
      gtk_widget_set_visible (gen->gl_renderer_row, FALSE);
      gtk_widget_set_visible (gen->gl_vendor_row, FALSE);
      gtk_widget_set_visible (gen->gl_full_version_row, FALSE);
      gtk_widget_set_visible (gen->glsl_version_row, FALSE);
      gtk_widget_set_visible (gen->gl_extensions_row, FALSE);
      gtk_label_set_text (GTK_LABEL (gen->gl_error), error->message);
      g_error_free (error);
      return;
    }

 gdk_gl_context_make_current (gdk_display_get_gl_context (gen->display));
 append_gl_extension_row (gen, "GL_OES_rgb8_rgba8");
 append_gl_extension_row (gen, "GL_EXT_abgr");
 append_gl_extension_row (gen, "GL_EXT_texture_format_BGRA8888");
 append_gl_extension_row (gen, "GL_EXT_texture_norm16");
 append_gl_extension_row (gen, "GL_OES_texture_half_float");
 append_gl_extension_row (gen, "GL_EXT_color_buffer_half_float");
 append_gl_extension_row (gen, "GL_OES_texture_half_float_linear");
 append_gl_extension_row (gen, "GL_OES_vertex_half_float");
 append_gl_extension_row (gen, "GL_OES_texture_float");
 append_gl_extension_row (gen, "GL_EXT_color_buffer_float");
 append_gl_extension_row (gen, "GL_OES_texture_float_linear");

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND) || (defined(GDK_WINDOWING_WIN32) && defined(GDK_WIN32_ENABLE_EGL))
  EGLDisplay egl_display = get_egl_display (gen->display);
  if (egl_display)
    {
      char *version;

      version = g_strconcat ("EGL ", eglQueryString (egl_display, EGL_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), version);
      g_free (version);

      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), eglQueryString (egl_display, EGL_VENDOR));

      append_egl_extension_row (gen, egl_display, "EGL_KHR_create_context");
      append_egl_extension_row (gen, egl_display, "EGL_EXT_buffer_age");
      append_egl_extension_row (gen, egl_display, "EGL_EXT_swap_buffers_with_damage");
      append_egl_extension_row (gen, egl_display, "EGL_KHR_surfaceless_context");
      append_egl_extension_row (gen, egl_display, "EGL_KHR_no_config_context");
      append_egl_extension_row (gen, egl_display, "EGL_EXT_image_dma_buf_import_modifiers");
      append_egl_extension_row (gen, egl_display, "EGL_MESA_image_dma_buf_export");
    }
  else
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gen->display))
    {
      Display *dpy = GDK_DISPLAY_XDISPLAY (gen->display);
      int error_base, event_base;
      char *version;

      if (!glXQueryExtension (dpy, &error_base, &event_base))
        return;

      version = g_strconcat ("GLX ", glXGetClientString (dpy, GLX_VERSION), NULL);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), version);
      g_free (version);
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), glXGetClientString (dpy, GLX_VENDOR));

      append_glx_extension_row (gen, dpy, "GLX_ARB_create_context_profile");
      append_glx_extension_row (gen, dpy, "GLX_SGI_swap_control");
      append_glx_extension_row (gen, dpy, "GLX_EXT_texture_from_pixmap");
      append_glx_extension_row (gen, dpy, "GLX_SGI_video_sync");
      append_glx_extension_row (gen, dpy, "GLX_EXT_buffer_age");
      append_glx_extension_row (gen, dpy, "GLX_OML_sync_control");
      append_glx_extension_row (gen, dpy, "GLX_ARB_multisample");
      append_glx_extension_row (gen, dpy, "GLX_EXT_visual_rating");
    }
  else
#endif
#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (gen->display) &&
      gdk_gl_backend_can_be_used (GDK_GL_WGL, NULL))
    {
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_vendor), "Microsoft WGL");
      gtk_widget_set_visible (gen->gl_backend_version, FALSE);

      append_wgl_extension_row (gen, "WGL_EXT_create_context");
      append_wgl_extension_row (gen, "WGL_EXT_swap_control");
      append_wgl_extension_row (gen, "WGL_OML_sync_control");
      append_wgl_extension_row (gen, "WGL_ARB_pixel_format");
      append_wgl_extension_row (gen, "WGL_ARB_multisample");
    }
  else
#endif
    {
      gtk_label_set_text (GTK_LABEL (gen->gl_backend_version), C_("GL version", "Unknown"));
      gtk_widget_set_visible (gen->gl_backend_vendor_row, FALSE);
    }

  context = gdk_display_get_gl_context (gen->display);
  gdk_gl_context_make_current (context);
  gdk_gl_context_get_version (context, &major, &minor);
  s = g_strdup_printf ("%s %u.%u",
                       gdk_gl_context_get_use_es (context) ? "GLES " : "OpenGL ", 
                       major, minor);
  gtk_label_set_text (GTK_LABEL (gen->gl_version), s);
  g_free (s);
  gtk_label_set_text (GTK_LABEL (gen->gl_vendor), (const char *) glGetString (GL_VENDOR));
  gtk_label_set_text (GTK_LABEL (gen->gl_renderer), (const char *) glGetString (GL_RENDERER));
  gtk_label_set_text (GTK_LABEL (gen->gl_full_version), (const char *) glGetString (GL_VERSION));
  gtk_label_set_text (GTK_LABEL (gen->glsl_version), (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION));
}

#ifdef GDK_RENDERING_VULKAN
static gboolean
has_debug_extension (void)
{
  uint32_t i;
  uint32_t n_extensions;
  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, NULL);
  VkExtensionProperties *extensions = g_newa (VkExtensionProperties, n_extensions);
  vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, extensions);

  for (i = 0; i < n_extensions; i++)
    {
      if (g_str_equal (extensions[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        return TRUE;
    }

  return FALSE;
}

static gboolean
has_validation_layer (void)
{
  uint32_t i;
  uint32_t n_layers;
  vkEnumerateInstanceLayerProperties (&n_layers, NULL);
  VkLayerProperties *layers = g_newa (VkLayerProperties, n_layers);
  vkEnumerateInstanceLayerProperties (&n_layers, layers);

  for (i = 0; i < n_layers; i++)
    {
      if (g_str_equal (layers[i].layerName, "VK_LAYER_KHRONOS_validation"))
        return TRUE;
    }

  return FALSE;
}
#endif

static void
init_vulkan (GtkInspectorGeneral *gen)
{
#ifdef GDK_RENDERING_VULKAN
  if (gdk_display_get_debug_flags (gen->display) & GDK_DEBUG_VULKAN_DISABLE)
    {
      gtk_label_set_text (GTK_LABEL (gen->vk_device), C_("Vulkan device", "Disabled"));
      gtk_label_set_text (GTK_LABEL (gen->vk_api_version), C_("Vulkan version", "Disabled"));
      gtk_label_set_text (GTK_LABEL (gen->vk_driver_version), C_("Vulkan version", "Disabled"));
      return;
    }

  if (gen->display->vk_device)
    {
      VkPhysicalDevice vk_device;
      VkPhysicalDeviceProperties props;
      char *device_name;
      char *api_version;
      char *driver_version;
      const char *types[] = { "other", "integrated GPU", "discrete GPU", "virtual GPU", "CPU" };

      vk_device = gen->display->vk_physical_device;
      vkGetPhysicalDeviceProperties (vk_device, &props);

      device_name = g_strdup_printf ("%s (%s)", props.deviceName, types[props.deviceType]);
      api_version = g_strdup_printf ("%d.%d.%d",
                                     VK_VERSION_MAJOR (props.apiVersion),
                                     VK_VERSION_MINOR (props.apiVersion),
                                     VK_VERSION_PATCH (props.apiVersion));
      driver_version = g_strdup_printf ("%d.%d.%d",
                                        VK_VERSION_MAJOR (props.driverVersion),
                                        VK_VERSION_MINOR (props.driverVersion),
                                        VK_VERSION_PATCH (props.driverVersion));

      gtk_label_set_text (GTK_LABEL (gen->vk_device), device_name);
      gtk_label_set_text (GTK_LABEL (gen->vk_api_version), api_version);
      gtk_label_set_text (GTK_LABEL (gen->vk_driver_version), driver_version);

      g_free (device_name);
      g_free (api_version);
      g_free (driver_version);

      add_check_row (gen, GTK_LIST_BOX (gen->vulkan_extensions_box), VK_KHR_SURFACE_EXTENSION_NAME, TRUE, 0);
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (gen->display))
        add_check_row (gen, GTK_LIST_BOX (gen->vulkan_extensions_box), "VK_KHR_xlib_surface", TRUE, 0);
#endif
#ifdef GDK_WINDOWING_WAYLAND
      if (GDK_IS_WAYLAND_DISPLAY (gen->display))
        add_check_row (gen, GTK_LIST_BOX (gen->vulkan_extensions_box), "VK_KHR_wayland_surface", TRUE, 0);
#endif
      add_check_row (gen, GTK_LIST_BOX (gen->vulkan_extensions_box), VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                     has_debug_extension (), 0);
      add_check_row (gen, GTK_LIST_BOX (gen->vulkan_extensions_box), "VK_LAYER_KHRONOS_validation",
                     has_validation_layer (), 0);
    }
  else
#endif
    {
      gtk_label_set_text (GTK_LABEL (gen->vk_device), C_("Vulkan device", "None"));
      gtk_label_set_text (GTK_LABEL (gen->vk_api_version), C_("Vulkan version", "None"));
      gtk_label_set_text (GTK_LABEL (gen->vk_driver_version), C_("Vulkan version", "None"));
    }
}

static void
set_monospace_font (GtkWidget *w)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));
  pango_attr_list_insert (attrs, pango_attr_family_new ("Monospace"));
  gtk_label_set_attributes (GTK_LABEL (w), attrs);
  pango_attr_list_unref (attrs);
}

static void
set_path_label (GtkWidget   *w,
                const char *var)
{
  const char *v;

  v = g_getenv (var);
  if (v != NULL)
    {
      set_monospace_font (w);
      gtk_label_set_text (GTK_LABEL (w), v);
    }
  else
    {
       GtkWidget *r;
       r = gtk_widget_get_ancestor (w, GTK_TYPE_LIST_BOX_ROW);
       gtk_widget_set_visible (r, FALSE);
    }
}

static void
init_env (GtkInspectorGeneral *gen)
{
  set_monospace_font (gen->prefix);
  gtk_label_set_text (GTK_LABEL (gen->prefix), _gtk_get_data_prefix ());
  set_path_label (gen->xdg_data_home, "XDG_DATA_HOME");
  set_path_label (gen->xdg_data_dirs, "XDG_DATA_DIRS");
  set_path_label (gen->gtk_path, "GTK_PATH");
  set_path_label (gen->gtk_exe_prefix, "GTK_EXE_PREFIX");
  set_path_label (gen->gtk_data_prefix, "GTK_DATA_PREFIX");
  set_path_label (gen->gsettings_schema_dir, "GSETTINGS_SCHEMA_DIR");
}

static const char *
translate_subpixel_layout (GdkSubpixelLayout subpixel)
{
  switch (subpixel)
    {
    case GDK_SUBPIXEL_LAYOUT_NONE: return "none";
    case GDK_SUBPIXEL_LAYOUT_UNKNOWN: return "unknown";
    case GDK_SUBPIXEL_LAYOUT_HORIZONTAL_RGB: return "horizontal rgb";
    case GDK_SUBPIXEL_LAYOUT_HORIZONTAL_BGR: return "horizontal bgr";
    case GDK_SUBPIXEL_LAYOUT_VERTICAL_RGB: return "vertical rgb";
    case GDK_SUBPIXEL_LAYOUT_VERTICAL_BGR: return "vertical bgr";
    default: g_assert_not_reached (); return "none";
    }
}

#ifdef GDK_WINDOWING_WAYLAND
static void
append_wayland_protocol_row (GtkInspectorGeneral *gen,
                             struct wl_proxy     *proxy)
{
  const char *id;
  unsigned int version;
  char buf[64];
  GtkListBox *list;

  if (proxy == NULL)
    return;

  list = GTK_LIST_BOX (gen->display_extensions_box);

  id = wl_proxy_get_class (proxy);
  version = wl_proxy_get_version (proxy);

  g_snprintf (buf, sizeof (buf), "%u", version);

  add_label_row (gen, list, id, buf, 10);
}

static void
add_wayland_protocols (GdkDisplay          *display,
                       GtkInspectorGeneral *gen)
{
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      GdkWaylandDisplay *d = (GdkWaylandDisplay *) display;

      append_wayland_protocol_row (gen, (struct wl_proxy *)d->compositor);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->shm);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->linux_dmabuf);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_wm_base);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->zxdg_shell_v6);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->gtk_shell);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->data_device_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->subcompositor);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->pointer_gestures);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->primary_selection_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->tablet_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_exporter);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_exporter_v2);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_importer);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_importer_v2);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->keyboard_shortcuts_inhibit);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->server_decoration_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_output_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->idle_inhibit_manager);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->xdg_activation);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->fractional_scale);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->viewporter);
      append_wayland_protocol_row (gen, (struct wl_proxy *)d->presentation);
    }
}
#endif

static void
populate_display (GdkDisplay *display, GtkInspectorGeneral *gen)
{
  GList *children, *l;
  GtkWidget *child;
  GtkListBox *list;

  list = GTK_LIST_BOX (gen->display_box);
  children = NULL;
  for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_LIST_BOX_ROW (child))
        children = g_list_prepend (children, child);
    }
  for (l = children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_is_ancestor (gen->display_name, child) ||
          gtk_widget_is_ancestor (gen->display_rgba, child) ||
          gtk_widget_is_ancestor (gen->display_composited, child) ||
          child == gen->display_extensions_row)
        continue;

      gtk_list_box_remove (list, child);
    }
  g_list_free (children);

  gtk_label_set_label (GTK_LABEL (gen->display_name), gdk_display_get_name (display));

  gtk_widget_set_visible (gen->display_rgba,
                          gdk_display_is_rgba (display));
  gtk_widget_set_visible (gen->display_composited,
                          gdk_display_is_composited (display));

  list = GTK_LIST_BOX (gen->display_extensions_box);
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (list))) != NULL)
    gtk_list_box_remove (list, child);

#ifdef GDK_WINDOWING_WAYLAND
  add_wayland_protocols (display, gen);
#endif

  gtk_widget_set_visible (gen->display_extensions_row,
                          gtk_widget_get_first_child (gen->display_extensions_box) != NULL);
}

static void
add_monitor (GtkInspectorGeneral *gen,
             GdkMonitor          *monitor,
             guint                i)
{
  GtkListBox *list;
  char *value;
  GdkRectangle rect;
  double scale;
  char *name;
  char *scale_str = NULL;
  const char *manufacturer;
  const char *model;

  list = GTK_LIST_BOX (gen->monitor_box);

  name = g_strdup_printf ("Monitor %u", i);
  manufacturer = gdk_monitor_get_manufacturer (monitor);
  model = gdk_monitor_get_model (monitor);
  value = g_strdup_printf ("%s%s%s",
                           manufacturer ? manufacturer : "",
                           manufacturer || model ? " " : "",
                           model ? model : "");
  add_label_row (gen, list, name, value, 0);
  g_free (value);
  g_free (name);

  add_label_row (gen, list, "Description", gdk_monitor_get_description (monitor), 10);
  add_label_row (gen, list, "Connector", gdk_monitor_get_connector (monitor), 10);

  gdk_monitor_get_geometry (monitor, &rect);
  scale = gdk_monitor_get_scale (monitor);
  if (scale != 1.0)
    scale_str = g_strdup_printf (" @ %.2f", scale);

  value = g_strdup_printf ("%d × %d%s at %d, %d",
                           rect.width, rect.height,
                           scale_str ? scale_str : "",
                           rect.x, rect.y);
  add_label_row (gen, list, "Geometry", value, 10);
  g_free (value);
  g_free (scale_str);

  value = g_strdup_printf ("%d × %d",
                           (int) (rect.width * scale),
                           (int) (rect.height * scale));
  add_label_row (gen, list, "Pixels", value, 10);
  g_free (value);

  value = g_strdup_printf ("%d × %d mm²",
                           gdk_monitor_get_width_mm (monitor),
                           gdk_monitor_get_height_mm (monitor));
  add_label_row (gen, list, "Size", value, 10);
  g_free (value);

  if (gdk_monitor_get_refresh_rate (monitor) != 0)
    {
      value = g_strdup_printf ("%.2f Hz",
                               0.001 * gdk_monitor_get_refresh_rate (monitor));
      add_label_row (gen, list, "Refresh rate", value, 10);
      g_free (value);
    }

  if (gdk_monitor_get_subpixel_layout (monitor) != GDK_SUBPIXEL_LAYOUT_UNKNOWN)
    {
      add_label_row (gen, list, "Subpixel layout",
                     translate_subpixel_layout (gdk_monitor_get_subpixel_layout (monitor)),
                     10);
    }
}

static void
populate_monitors (GdkDisplay          *display,
                   GtkInspectorGeneral *gen)
{
  GtkWidget *child;
  GListModel *list;

  while ((child = gtk_widget_get_first_child (gen->monitor_box)))
    gtk_list_box_remove (GTK_LIST_BOX (gen->monitor_box), child);

  list = gdk_display_get_monitors (gen->display);

  for (guint i = 0; i < g_list_model_get_n_items (list); i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (list, i);
      add_monitor (gen, monitor, i);
      g_object_unref (monitor);
    }
}

static void
populate_display_notify_cb (GdkDisplay          *display,
                            GParamSpec          *pspec,
                            GtkInspectorGeneral *gen)
{
  populate_display (display, gen);
}

static void
monitors_changed_cb (GListModel          *monitors,
                     guint                position,
                     guint                removed,
                     guint                added,
                     GtkInspectorGeneral *gen)
{
  populate_monitors (gen->display, gen);
}

static void
init_display (GtkInspectorGeneral *gen)
{
  g_signal_connect (gen->display, "notify", G_CALLBACK (populate_display_notify_cb), gen);
  g_signal_connect (gdk_display_get_monitors (gen->display), "items-changed",
                    G_CALLBACK (monitors_changed_cb), gen);

  populate_display (gen->display, gen);
  populate_monitors (gen->display, gen);
}

static void
init_pango (GtkInspectorGeneral *gen)
{
  PangoFontMap *fontmap;
  const char *type;
  const char *name;

  fontmap = pango_cairo_font_map_get_default ();
  type = G_OBJECT_TYPE_NAME (fontmap);
  if (strcmp (type, "PangoCairoFcFontMap") == 0)
    name = "fontconfig";
  else if (strcmp (type, "PangoCairoCoreTextFontMap") == 0)
    name = "coretext";
  else if (strcmp (type, "PangoCairoWin32FontMap") == 0)
    name = "win32";
  else
    name = type;

  gtk_label_set_label (GTK_LABEL (gen->pango_fontmap), name);
}

static void
init_media (GtkInspectorGeneral *gen)
{
  GIOExtension *e;
  const char *name;

  e = gtk_media_file_get_extension ();
  name = g_io_extension_get_name (e);
  gtk_label_set_label (GTK_LABEL (gen->media_backend), name);
}

static void
im_module_changed (GtkSettings         *settings,
                   GParamSpec          *pspec,
                   GtkInspectorGeneral *gen)
{
  if (!gen->display)
    return;

  gtk_label_set_label (GTK_LABEL (gen->im_module),
                       _gtk_im_module_get_default_context_id (gen->display));
}

static void
init_im_module (GtkInspectorGeneral *gen)
{
  GtkSettings *settings = gtk_settings_get_for_display (gen->display);
  const char *default_context_id = _gtk_im_module_get_default_context_id (gen->display);

  gtk_label_set_label (GTK_LABEL (gen->im_module), default_context_id);

  if (g_getenv ("GTK_IM_MODULE") != NULL)
    {
      /* This can't update if GTK_IM_MODULE envvar is set */
      gtk_widget_set_tooltip_text (gen->im_module,
                                   _("IM Context is hardcoded by GTK_IM_MODULE"));
      gtk_widget_set_sensitive (gen->im_module, FALSE);
      return;
    }

  g_signal_connect_object (settings,
                           "notify::gtk-im-module",
                           G_CALLBACK (im_module_changed),
                           gen, 0);
}


static void populate_seats (GtkInspectorGeneral *gen);

static void
add_tool (GtkInspectorGeneral *gen,
          GdkDeviceTool       *tool)
{
  GdkAxisFlags axes;
  GString *str;
  char *val;
  int i;
  GEnumClass *eclass;
  GEnumValue *evalue;
  GFlagsClass *fclass;
  GFlagsValue *fvalue;

  val = g_strdup_printf ("Serial %" G_GUINT64_FORMAT, gdk_device_tool_get_serial (tool));
  add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Tool", val, 10);
  g_free (val);

  eclass = g_type_class_ref (GDK_TYPE_DEVICE_TOOL_TYPE);
  evalue = g_enum_get_value (eclass, gdk_device_tool_get_tool_type (tool));
  add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Type", evalue->value_nick, 20);
  g_type_class_unref (eclass);

  fclass = g_type_class_ref (GDK_TYPE_AXIS_FLAGS);
  str = g_string_new ("");
  axes = gdk_device_tool_get_axes (tool);
  for (i = GDK_AXIS_X; i < GDK_AXIS_LAST; i++)
    {
      if ((axes & (1 << i)) != 0)
        {
          fvalue = g_flags_get_first_value (fclass, i);
          if (str->len > 0)
            g_string_append (str, ", ");
          g_string_append (str, fvalue->value_nick);
        }
    }
  g_type_class_unref (fclass);

  if (str->len > 0)
    add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Axes", str->str, 20);

  g_string_free (str, TRUE);
}

static void
add_device (GtkInspectorGeneral *gen,
            GdkDevice           *device)
{
  const char *name;
  guint n_touches;
  char *text;
  GEnumClass *class;
  GEnumValue *value;

  name = gdk_device_get_name (device);

  class = g_type_class_ref (GDK_TYPE_INPUT_SOURCE);
  value = g_enum_get_value (class, gdk_device_get_source (device));

  add_label_row (gen, GTK_LIST_BOX (gen->device_box), name, value->value_nick, 10);

  g_object_get (device, "num-touches", &n_touches, NULL);
  if (n_touches > 0)
    {
      text = g_strdup_printf ("%d", n_touches);
      add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Touches", text, 20);
      g_free (text);
    }

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DEVICE (device) &&
      gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      struct xkb_keymap *keymap = gdk_wayland_device_get_xkb_keymap (device);
      GString *s;

      s = g_string_new ("");
      for (int i = 0; i < xkb_keymap_num_layouts (keymap); i++)
        {
          if (s->len > 0)
            g_string_append (s, ", ");
          g_string_append (s, xkb_keymap_layout_get_name (keymap, i));
        }

      add_label_row (gen, GTK_LIST_BOX (gen->device_box), "Layouts", s->str, 20);
      g_string_free (s, TRUE);
    }
#endif

  g_type_class_unref (class);
}

static char *
get_seat_capabilities (GdkSeat *seat)
{
  struct {
    GdkSeatCapabilities cap;
    const char *name;
  } caps[] = {
    { GDK_SEAT_CAPABILITY_POINTER,       "Pointer" },
    { GDK_SEAT_CAPABILITY_TOUCH,         "Touch" },
    { GDK_SEAT_CAPABILITY_TABLET_STYLUS, "Tablet" },
    { GDK_SEAT_CAPABILITY_KEYBOARD,      "Keyboard" },
    { 0, NULL }
  };
  GString *str;
  GdkSeatCapabilities capabilities;
  int i;

  str = g_string_new ("");
  capabilities = gdk_seat_get_capabilities (seat);
  for (i = 0; caps[i].cap != 0; i++)
    {
      if (capabilities & caps[i].cap)
        {
          if (str->len > 0)
            g_string_append (str, ", ");
          g_string_append (str, caps[i].name);
        }
    }

  return g_string_free (str, FALSE);
}

static void
add_seat (GtkInspectorGeneral *gen,
          GdkSeat             *seat,
          int                  num)
{
  char *text;
  char *caps;
  GList *list, *l;

  if (!g_object_get_data (G_OBJECT (seat), "inspector-connected"))
    {
      g_object_set_data (G_OBJECT (seat), "inspector-connected", GINT_TO_POINTER (1));
      g_signal_connect_swapped (seat, "device-added", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "device-removed", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "tool-added", G_CALLBACK (populate_seats), gen);
      g_signal_connect_swapped (seat, "tool-removed", G_CALLBACK (populate_seats), gen);
    }

  text = g_strdup_printf ("Seat %d", num);
  caps = get_seat_capabilities (seat);

  add_label_row (gen, GTK_LIST_BOX (gen->device_box), text, caps, 0);
  g_free (text);
  g_free (caps);

  list = gdk_seat_get_devices (seat, GDK_SEAT_CAPABILITY_ALL);

  for (l = list; l; l = l->next)
    add_device (gen, GDK_DEVICE (l->data));

  g_list_free (list);

  list = gdk_seat_get_tools (seat);

  for (l = list; l; l = l->next)
    add_tool (gen, l->data);

  g_list_free (list);
}

static void
disconnect_seat (GtkInspectorGeneral *gen,
                 GdkSeat             *seat)
{
  g_signal_handlers_disconnect_by_func (seat, G_CALLBACK (populate_seats), gen);
}

static void
populate_seats (GtkInspectorGeneral *gen)
{
  GtkWidget *child;
  GList *list, *l;
  int i;

  while ((child = gtk_widget_get_first_child (gen->device_box)))
    gtk_list_box_remove (GTK_LIST_BOX (gen->device_box), child);

  list = gdk_display_list_seats (gen->display);

  for (l = list, i = 0; l; l = l->next, i++)
    add_seat (gen, GDK_SEAT (l->data), i);

  g_list_free (list);
}

static void
seat_added (GdkDisplay          *display,
            GdkSeat             *seat,
            GtkInspectorGeneral *gen)
{
  populate_seats (gen);
}

static void
seat_removed (GdkDisplay          *display,
              GdkSeat             *seat,
              GtkInspectorGeneral *gen)
{
  disconnect_seat (gen, seat);
  populate_seats (gen);
}

static void
init_device (GtkInspectorGeneral *gen)
{
  g_signal_connect (gen->display, "seat-added", G_CALLBACK (seat_added), gen);
  g_signal_connect (gen->display, "seat-removed", G_CALLBACK (seat_removed), gen);

  populate_seats (gen);
}

static void
gtk_inspector_general_init (GtkInspectorGeneral *gen)
{
  gtk_widget_init_template (GTK_WIDGET (gen));
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorGeneral *gen)
{
  GtkWidget *next;

  if (direction == GTK_DIR_DOWN && widget == gen->version_box)
    next = gen->env_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->env_box)
    next = gen->display_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->display_box)
    next = gen->monitor_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->monitor_box)
    next = gen->device_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->device_box)
    next = gen->gl_box;
  else if (direction == GTK_DIR_DOWN && widget == gen->gl_box)
    next = gen->vulkan_box;
  else if (direction == GTK_DIR_UP && widget == gen->vulkan_box)
    next = gen->gl_box;
  else if (direction == GTK_DIR_UP && widget == gen->gl_box)
    next = gen->device_box;
  else if (direction == GTK_DIR_UP && widget == gen->device_box)
    next = gen->monitor_box;
  else if (direction == GTK_DIR_UP && widget == gen->monitor_box)
    next = gen->display_box;
  else if (direction == GTK_DIR_UP && widget == gen->display_box)
    next = gen->env_box;
  else if (direction == GTK_DIR_UP && widget == gen->env_box)
    next = gen->version_box;
  else
    next = NULL;

  if (next)
    {
      gtk_widget_child_focus (next, direction);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_inspector_general_constructed (GObject *object)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (object);

  G_OBJECT_CLASS (gtk_inspector_general_parent_class)->constructed (object);

   g_signal_connect (gen->version_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->env_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->display_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->monitor_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->gl_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->vulkan_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
   g_signal_connect (gen->device_box, "keynav-failed", G_CALLBACK (keynav_failed), gen);
}

static void
gtk_inspector_general_dispose (GObject *object)
{
  GtkInspectorGeneral *gen = GTK_INSPECTOR_GENERAL (object);
  GList *list, *l;

  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (seat_added), gen);
  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (seat_removed), gen);
  g_signal_handlers_disconnect_by_func (gen->display, G_CALLBACK (populate_display_notify_cb), gen);
  g_signal_handlers_disconnect_by_func (gdk_display_get_monitors (gen->display), G_CALLBACK (monitors_changed_cb), gen);

  list = gdk_display_list_seats (gen->display);
  for (l = list; l; l = l->next)
    disconnect_seat (gen, GDK_SEAT (l->data));
  g_list_free (list);

  gtk_widget_dispose_template (GTK_WIDGET (gen), GTK_TYPE_INSPECTOR_GENERAL);

  G_OBJECT_CLASS (gtk_inspector_general_parent_class)->dispose (object);
}

static void
gtk_inspector_general_class_init (GtkInspectorGeneralClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_general_constructed;
  object_class->dispose = gtk_inspector_general_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/general.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, version_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, env_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_extensions_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, monitor_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_extensions_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_extensions_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vulkan_extensions_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gtk_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gdk_backend);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gsk_renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, pango_fontmap);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, media_backend);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, im_module);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_error);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_error_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_vendor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_backend_vendor_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_vendor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_vendor_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_renderer_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_full_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gl_full_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, glsl_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, glsl_version_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_device);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_api_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, vk_driver_version);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, app_id_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, app_id);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, resource_path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, prefix);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, xdg_data_home);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, xdg_data_dirs);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gtk_path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gtk_exe_prefix);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gtk_data_prefix);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, gsettings_schema_dir);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_name);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_composited);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, display_rgba);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorGeneral, device_box);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
gtk_inspector_general_set_display (GtkInspectorGeneral *gen,
                                   GdkDisplay *display)
{
  gen->display = display;

  init_version (gen);
  init_env (gen);
  init_app_id (gen);
  init_display (gen);
  init_pango (gen);
  init_media (gen);
  init_gl (gen);
  init_vulkan (gen);
  init_device (gen);
  init_im_module (gen);
}

// vim: set et sw=2 ts=2:
