/* GTK - The GIMP Toolkit
 *
 * gtkglarea.c: A GL drawing area
 *
 * Copyright © 2014  Emmanuele Bassi
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

#include "config.h"
#include "gtkglarea.h"
#include <glib/gi18n-lib.h>
#include "gtkmarshalers.h"
#include "gdk/gdkmarshalers.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtksnapshot.h"
#include "gtkrenderlayoutprivate.h"
#include "gtkcssnodeprivate.h"
#include "gdk/gdkgltextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <epoxy/gl.h>

/**
 * GtkGLArea:
 *
 * `GtkGLArea` is a widget that allows drawing with OpenGL.
 *
 * ![An example GtkGLArea](glarea.png)
 *
 * `GtkGLArea` sets up its own [class@Gdk.GLContext], and creates a custom
 * GL framebuffer that the widget will do GL rendering onto. It also ensures
 * that this framebuffer is the default GL rendering target when rendering.
 * The completed rendering is integrated into the larger GTK scene graph as
 * a texture.
 *
 * In order to draw, you have to connect to the [signal@Gtk.GLArea::render]
 * signal, or subclass `GtkGLArea` and override the GtkGLAreaClass.render
 * virtual function.
 *
 * The `GtkGLArea` widget ensures that the `GdkGLContext` is associated with
 * the widget's drawing area, and it is kept updated when the size and
 * position of the drawing area changes.
 *
 * ## Drawing with GtkGLArea
 *
 * The simplest way to draw using OpenGL commands in a `GtkGLArea` is to
 * create a widget instance and connect to the [signal@Gtk.GLArea::render] signal:
 *
 * The `render()` function will be called when the `GtkGLArea` is ready
 * for you to draw its content:
 *
 * The initial contents of the framebuffer are transparent.
 *
 * ```c
 * static gboolean
 * render (GtkGLArea *area, GdkGLContext *context)
 * {
 *   // inside this function it's safe to use GL; the given
 *   // GdkGLContext has been made current to the drawable
 *   // surface used by the `GtkGLArea` and the viewport has
 *   // already been set to be the size of the allocation
 *
 *   // we can start by clearing the buffer
 *   glClearColor (0, 0, 0, 0);
 *   glClear (GL_COLOR_BUFFER_BIT);
 *
 *   // draw your object
 *   // draw_an_object ();
 *
 *   // we completed our drawing; the draw commands will be
 *   // flushed at the end of the signal emission chain, and
 *   // the buffers will be drawn on the window
 *   return TRUE;
 * }
 *
 * void setup_glarea (void)
 * {
 *   // create a GtkGLArea instance
 *   GtkWidget *gl_area = gtk_gl_area_new ();
 *
 *   // connect to the "render" signal
 *   g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
 * }
 * ```
 *
 * If you need to initialize OpenGL state, e.g. buffer objects or
 * shaders, you should use the [signal@Gtk.Widget::realize] signal;
 * you can use the [signal@Gtk.Widget::unrealize] signal to clean up.
 * Since the `GdkGLContext` creation and initialization may fail, you
 * will need to check for errors, using [method@Gtk.GLArea.get_error].
 *
 * An example of how to safely initialize the GL state is:
 *
 * ```c
 * static void
 * on_realize (GtkGLarea *area)
 * {
 *   // We need to make the context current if we want to
 *   // call GL API
 *   gtk_gl_area_make_current (area);
 *
 *   // If there were errors during the initialization or
 *   // when trying to make the context current, this
 *   // function will return a GError for you to catch
 *   if (gtk_gl_area_get_error (area) != NULL)
 *     return;
 *
 *   // You can also use gtk_gl_area_set_error() in order
 *   // to show eventual initialization errors on the
 *   // GtkGLArea widget itself
 *   GError *internal_error = NULL;
 *   init_buffer_objects (&error);
 *   if (error != NULL)
 *     {
 *       gtk_gl_area_set_error (area, error);
 *       g_error_free (error);
 *       return;
 *     }
 *
 *   init_shaders (&error);
 *   if (error != NULL)
 *     {
 *       gtk_gl_area_set_error (area, error);
 *       g_error_free (error);
 *       return;
 *     }
 * }
 * ```
 *
 * If you need to change the options for creating the `GdkGLContext`
 * you should use the [signal@Gtk.GLArea::create-context] signal.
 */

typedef struct {
  GdkGLTextureBuilder *builder;
  GdkTexture *holder;
} Texture;

typedef struct {
  GdkGLContext *context;
  GError *error;

  gboolean have_buffers;

  int required_gl_version;

  guint frame_buffer;
  guint depth_stencil_buffer;
  Texture *texture;
  GList *textures;

  gboolean has_depth_buffer;
  gboolean has_stencil_buffer;

  gboolean needs_resize;
  gboolean needs_render;
  gboolean auto_render;
  gboolean use_es;
  GdkGLAPI allowed_apis;
} GtkGLAreaPrivate;

enum {
  PROP_0,

  PROP_CONTEXT,
  PROP_HAS_DEPTH_BUFFER,
  PROP_HAS_STENCIL_BUFFER,
  PROP_USE_ES,
  PROP_ALLOWED_APIS,
  PROP_API,

  PROP_AUTO_RENDER,

  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum {
  RENDER,
  RESIZE,
  CREATE_CONTEXT,

  LAST_SIGNAL
};

static void gtk_gl_area_allocate_buffers (GtkGLArea *area);
static void gtk_gl_area_allocate_texture (GtkGLArea *area);

static guint area_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGLArea, gtk_gl_area, GTK_TYPE_WIDGET)

static void
gtk_gl_area_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkGLArea *self = GTK_GL_AREA (gobject);

  switch (prop_id)
    {
    case PROP_AUTO_RENDER:
      gtk_gl_area_set_auto_render (self, g_value_get_boolean (value));
      break;

    case PROP_HAS_DEPTH_BUFFER:
      gtk_gl_area_set_has_depth_buffer (self, g_value_get_boolean (value));
      break;

    case PROP_HAS_STENCIL_BUFFER:
      gtk_gl_area_set_has_stencil_buffer (self, g_value_get_boolean (value));
      break;

    case PROP_USE_ES:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_gl_area_set_use_es (self, g_value_get_boolean (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_ALLOWED_APIS:
      gtk_gl_area_set_allowed_apis (self, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_gl_area_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (GTK_GL_AREA (gobject));

  switch (prop_id)
    {
    case PROP_AUTO_RENDER:
      g_value_set_boolean (value, priv->auto_render);
      break;

    case PROP_HAS_DEPTH_BUFFER:
      g_value_set_boolean (value, priv->has_depth_buffer);
      break;

    case PROP_HAS_STENCIL_BUFFER:
      g_value_set_boolean (value, priv->has_stencil_buffer);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
      break;

    case PROP_USE_ES:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_boolean (value, gtk_gl_area_get_use_es (GTK_GL_AREA (gobject)));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_ALLOWED_APIS:
      g_value_set_flags (value, priv->allowed_apis);
      break;

    case PROP_API:
      g_value_set_flags (value, gtk_gl_area_get_api (GTK_GL_AREA (gobject)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_gl_area_realize (GtkWidget *widget)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->realize (widget);

  g_clear_error (&priv->error);
  priv->context = NULL;
  g_signal_emit (area, area_signals[CREATE_CONTEXT], 0, &priv->context);

  /* In case the signal failed, but did not set an error */
  if (priv->context == NULL && priv->error == NULL)
    g_set_error_literal (&priv->error, GDK_GL_ERROR,
                         GDK_GL_ERROR_NOT_AVAILABLE,
                         _("OpenGL context creation failed"));

  priv->needs_resize = TRUE;
}

static void
gtk_gl_area_notify (GObject    *object,
                    GParamSpec *pspec)
{
  if (strcmp (pspec->name, "scale-factor") == 0)
    {
      GtkGLArea *area = GTK_GL_AREA (object);
      GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

      priv->needs_resize = TRUE;
    }

  if (G_OBJECT_CLASS (gtk_gl_area_parent_class)->notify)
    G_OBJECT_CLASS (gtk_gl_area_parent_class)->notify (object, pspec);
}

static GdkGLContext *
gtk_gl_area_real_create_context (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);
  GError *error = NULL;
  GdkGLContext *context;

  context = gdk_surface_create_gl_context (gtk_native_get_surface (gtk_widget_get_native (widget)), &error);
  if (error != NULL)
    {
      gtk_gl_area_set_error (area, error);
      g_clear_object (&context);
      g_clear_error (&error);
      return NULL;
    }

  gdk_gl_context_set_allowed_apis (context, priv->allowed_apis);
  gdk_gl_context_set_required_version (context,
                                       priv->required_gl_version / 10,
                                       priv->required_gl_version % 10);

  gdk_gl_context_realize (context, &error);
  if (error != NULL)
    {
      gtk_gl_area_set_error (area, error);
      g_clear_object (&context);
      g_clear_error (&error);
      return NULL;
    }

  return context;
}

static void
gtk_gl_area_resize (GtkGLArea *area, int width, int height)
{
  glViewport (0, 0, width, height);
}

/*
 * Creates all the buffer objects needed for rendering the scene
 */
static void
gtk_gl_area_ensure_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);

  gtk_widget_realize (widget);

  if (priv->context == NULL)
    return;

  if (priv->have_buffers)
    return;

  priv->have_buffers = TRUE;

  glGenFramebuffers (1, &priv->frame_buffer);

  if ((priv->has_depth_buffer || priv->has_stencil_buffer))
    {
      if (priv->depth_stencil_buffer == 0)
        glGenRenderbuffers (1, &priv->depth_stencil_buffer);
    }
  else if (priv->depth_stencil_buffer != 0)
    {
      /* Delete old depth/stencil buffer */
      glDeleteRenderbuffers (1, &priv->depth_stencil_buffer);
      priv->depth_stencil_buffer = 0;
    }

  gtk_gl_area_allocate_buffers (area);
}

static void
delete_one_texture (gpointer data)
{
  Texture *texture = data;
  guint id;

  if (texture->holder)
    gdk_gl_texture_release (GDK_GL_TEXTURE (texture->holder));

  id = gdk_gl_texture_builder_get_id (texture->builder);
  if (id != 0)
    glDeleteTextures (1, &id);

  g_object_unref (texture->builder);

  g_free (texture);
}

static void
gtk_gl_area_ensure_texture (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);

  gtk_widget_realize (widget);

  if (priv->context == NULL)
    return;

  if (priv->texture == NULL)
    {
      GList *l, *link;

      l = priv->textures;
      while (l)
        {
          Texture *texture = l->data;
          link = l;
          l = l->next;

          if (texture->holder)
            continue;

          priv->textures = g_list_delete_link (priv->textures, link);

          if (priv->texture == NULL)
            priv->texture = texture;
          else
            delete_one_texture (texture);
        }
    }

  if (priv->texture == NULL)
    {
      GLuint id;

      priv->texture = g_new (Texture, 1);
      priv->texture->holder = NULL;

      priv->texture->builder = gdk_gl_texture_builder_new ();
      gdk_gl_texture_builder_set_context (priv->texture->builder, priv->context);
      if (gdk_gl_context_get_api (priv->context) == GDK_GL_API_GLES)
        gdk_gl_texture_builder_set_format (priv->texture->builder, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
      else
        gdk_gl_texture_builder_set_format (priv->texture->builder, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED);

      glGenTextures (1, &id);
      gdk_gl_texture_builder_set_id (priv->texture->builder, id);
    }

  gtk_gl_area_allocate_texture (area);
}

/*
 * Allocates space of the right type and size for all the buffers
 */
static void
gtk_gl_area_allocate_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);
  int scale, width, height;

  if (priv->context == NULL)
    return;

  scale = gtk_widget_get_scale_factor (widget);
  width = gtk_widget_get_width (widget) * scale;
  height = gtk_widget_get_height (widget) * scale;

  if (priv->has_depth_buffer || priv->has_stencil_buffer)
    {
      glBindRenderbuffer (GL_RENDERBUFFER, priv->depth_stencil_buffer);
      if (priv->has_stencil_buffer)
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
      else
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    }

  priv->needs_render = TRUE;
}

static void
gtk_gl_area_allocate_texture (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);
  int scale, width, height;

  if (priv->context == NULL)
    return;

  if (priv->texture == NULL)
    return;

  g_assert (priv->texture->holder == NULL);

  scale = gtk_widget_get_scale_factor (widget);
  width = gtk_widget_get_width (widget) * scale;
  height = gtk_widget_get_height (widget) * scale;

  if (gdk_gl_texture_builder_get_width (priv->texture->builder) != width ||
      gdk_gl_texture_builder_get_height (priv->texture->builder) != height)
    {
      glBindTexture (GL_TEXTURE_2D, gdk_gl_texture_builder_get_id (priv->texture->builder));
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      if (gdk_gl_context_get_api (priv->context) == GDK_GL_API_GLES)
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      else
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

      gdk_gl_texture_builder_set_width (priv->texture->builder, width);
      gdk_gl_texture_builder_set_height (priv->texture->builder, height);
    }
}

/**
 * gtk_gl_area_attach_buffers:
 * @area: a `GtkGLArea`
 *
 * Binds buffers to the framebuffer.
 *
 * Ensures that the @area framebuffer object is made the current draw
 * and read target, and that all the required buffers for the @area
 * are created and bound to the framebuffer.
 *
 * This function is automatically called before emitting the
 * [signal@Gtk.GLArea::render] signal, and doesn't normally need to be
 * called by application code.
 */
void
gtk_gl_area_attach_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  if (priv->context == NULL)
    return;

  gtk_gl_area_make_current (area);

  if (priv->texture == NULL)
    gtk_gl_area_ensure_texture (area);
  else if (priv->needs_resize)
    gtk_gl_area_allocate_texture (area);

  if (!priv->have_buffers)
    gtk_gl_area_ensure_buffers (area);
  else if (priv->needs_resize)
    gtk_gl_area_allocate_buffers (area);

  glBindFramebuffer (GL_FRAMEBUFFER, priv->frame_buffer);

  if (priv->texture != NULL)
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, gdk_gl_texture_builder_get_id (priv->texture->builder), 0);

  if (priv->depth_stencil_buffer)
    {
      if (priv->has_depth_buffer)
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_RENDERBUFFER, priv->depth_stencil_buffer);
      if (priv->has_stencil_buffer)
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, priv->depth_stencil_buffer);
    }
}

static void
gtk_gl_area_delete_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->context == NULL)
    return;

  priv->have_buffers = FALSE;

  if (priv->depth_stencil_buffer != 0)
    {
      glDeleteRenderbuffers (1, &priv->depth_stencil_buffer);
      priv->depth_stencil_buffer = 0;
    }

  if (priv->frame_buffer != 0)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &priv->frame_buffer);
      priv->frame_buffer = 0;
    }
}

static void
gtk_gl_area_delete_textures (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->texture)
    {
      delete_one_texture (priv->texture);
      priv->texture = NULL;
    }

  /* FIXME: we need to explicitly release all outstanding
   * textures here, otherwise release_texture will get called
   * later and access freed memory.
   */
  g_list_free_full (priv->textures, delete_one_texture);
  priv->textures = NULL;
}

static void
gtk_gl_area_unrealize (GtkWidget *widget)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->context != NULL)
    {
      gtk_gl_area_make_current (area);
      gtk_gl_area_delete_buffers (area);
      gtk_gl_area_delete_textures (area);

      /* Make sure to unset the context if current */
      if (priv->context == gdk_gl_context_get_current ())
        gdk_gl_context_clear_current ();
    }

  g_clear_object (&priv->context);
  g_clear_error (&priv->error);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->unrealize (widget);
}

static void
gtk_gl_area_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->size_allocate (widget, width, height, baseline);

  if (gtk_widget_get_realized (widget))
    priv->needs_resize = TRUE;
}

static void
gtk_gl_area_draw_error_screen (GtkGLArea   *area,
                               GtkSnapshot *snapshot,
                               int          width,
                               int          height)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  PangoLayout *layout;
  int layout_height;
  GtkCssBoxes boxes;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (area), priv->error->message);
  pango_layout_set_width (layout, width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);

  gtk_css_boxes_init (&boxes, GTK_WIDGET (area));
  gtk_css_style_snapshot_layout (&boxes, snapshot, 0, (height - layout_height) / 2, layout);

  g_object_unref (layout);
}

static void
release_texture (gpointer data)
{
  Texture *texture = data;
  gpointer sync;

  sync = gdk_gl_texture_builder_get_sync (texture->builder);
  if (sync)
    {
      glDeleteSync (sync);
      gdk_gl_texture_builder_set_sync (texture->builder, NULL);
    }

  texture->holder = NULL;
}

static void
gtk_gl_area_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  gboolean unused;
  int w, h, scale;
  GLenum status;

  scale = gtk_widget_get_scale_factor (widget);
  w = gtk_widget_get_width (widget) * scale;
  h = gtk_widget_get_height (widget) * scale;

  if (w == 0 || h == 0)
    return;

  if (priv->error != NULL)
    {
      gtk_gl_area_draw_error_screen (area,
                                     snapshot,
                                     gtk_widget_get_width (widget),
                                     gtk_widget_get_height (widget));
      return;
    }

  if (priv->context == NULL)
    return;

  gtk_gl_area_make_current (area);

  gtk_gl_area_attach_buffers (area);

 if (priv->has_depth_buffer)
   glEnable (GL_DEPTH_TEST);
 else
   glDisable (GL_DEPTH_TEST);

  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
  if (status == GL_FRAMEBUFFER_COMPLETE)
    {
      Texture *texture;
      gpointer sync = NULL;

      if (priv->needs_render || priv->auto_render)
        {
          if (priv->needs_resize)
            {
              g_signal_emit (area, area_signals[RESIZE], 0, w, h, NULL);
              priv->needs_resize = FALSE;
            }

          g_signal_emit (area, area_signals[RENDER], 0, priv->context, &unused);
        }

      priv->needs_render = FALSE;

      texture = priv->texture;
      priv->texture = NULL;
      priv->textures = g_list_prepend (priv->textures, texture);

      if (gdk_gl_context_has_feature (priv->context, GDK_GL_FEATURE_SYNC))
        sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

      gdk_gl_texture_builder_set_sync (texture->builder, sync);

      texture->holder = gdk_gl_texture_builder_build (texture->builder,
                                                      release_texture,
                                                      texture);

      /* Our texture is rendered by OpenGL, so it is upside down,
       * compared to what GSK expects, so flip it back.
       */
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, gtk_widget_get_height (widget)));
      gtk_snapshot_scale (snapshot, 1, -1);
      gtk_snapshot_append_texture (snapshot,
                                   texture->holder,
                                   &GRAPHENE_RECT_INIT (0, 0,
                                                        gtk_widget_get_width (widget),
                                                        gtk_widget_get_height (widget)));
      gtk_snapshot_restore (snapshot);

      g_object_unref (texture->holder);
    }
  else
    {
      g_warning ("fb setup not supported (%x)", status);
    }
}

static gboolean
create_context_accumulator (GSignalInvocationHint *ihint,
                            GValue *return_accu,
                            const GValue *handler_return,
                            gpointer data)
{
  g_value_copy (handler_return, return_accu);

  /* stop after the first handler returning a valid object */
  return g_value_get_object (handler_return) == NULL;
}

static void
gtk_gl_area_class_init (GtkGLAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  klass->resize = gtk_gl_area_resize;
  klass->create_context = gtk_gl_area_real_create_context;

  widget_class->realize = gtk_gl_area_realize;
  widget_class->unrealize = gtk_gl_area_unrealize;
  widget_class->size_allocate = gtk_gl_area_size_allocate;
  widget_class->snapshot = gtk_gl_area_snapshot;

  /**
   * GtkGLArea:context:
   *
   * The `GdkGLContext` used by the `GtkGLArea` widget.
   *
   * The `GtkGLArea` widget is responsible for creating the `GdkGLContext`
   * instance. If you need to render with other kinds of buffers (stencil,
   * depth, etc), use render buffers.
   */
  obj_props[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GtkGLArea:auto-render: (attributes org.gtk.Property.get=gtk_gl_area_get_auto_render org.gtk.Property.set=gtk_gl_area_set_auto_render)
   *
   * If set to %TRUE the ::render signal will be emitted every time
   * the widget draws.
   *
   * This is the default and is useful if drawing the widget is faster.
   *
   * If set to %FALSE the data from previous rendering is kept around and will
   * be used for drawing the widget the next time, unless the window is resized.
   * In order to force a rendering [method@Gtk.GLArea.queue_render] must be called.
   * This mode is useful when the scene changes seldom, but takes a long time
   * to redraw.
   */
  obj_props[PROP_AUTO_RENDER] =
    g_param_spec_boolean ("auto-render", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:has-depth-buffer: (attributes org.gtk.Property.get=gtk_gl_area_get_has_depth_buffer org.gtk.Property.set=gtk_gl_area_set_has_depth_buffer)
   *
   * If set to %TRUE the widget will allocate and enable a depth buffer for the
   * target framebuffer.
   *
   * Setting this property will enable GL's depth testing as a side effect. If
   * you don't need depth testing, you should call `glDisable(GL_DEPTH_TEST)`
   * in your `GtkGLArea::render` handler.
   */
  obj_props[PROP_HAS_DEPTH_BUFFER] =
    g_param_spec_boolean ("has-depth-buffer", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:has-stencil-buffer: (attributes org.gtk.Property.get=gtk_gl_area_get_has_stencil_buffer org.gtk.Property.set=gtk_gl_area_set_has_stencil_buffer)
   *
   * If set to %TRUE the widget will allocate and enable a stencil buffer for the
   * target framebuffer.
   */
  obj_props[PROP_HAS_STENCIL_BUFFER] =
    g_param_spec_boolean ("has-stencil-buffer", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:use-es: (attributes org.gtk.Property.get=gtk_gl_area_get_use_es org.gtk.Property.set=gtk_gl_area_set_use_es)
   *
   * If set to %TRUE the widget will try to create a `GdkGLContext` using
   * OpenGL ES instead of OpenGL.
   *
   * Deprecated: 4.12: Use [property@Gtk.GLArea:allowed-apis]
   */
  obj_props[PROP_USE_ES] =
    g_param_spec_boolean ("use-es", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:allowed-apis: (attributes org.gtk.Property.get=gtk_gl_area_get_allowed_apis org.gtk.Property.set=gtk_gl_area_set_allowed_apis)
   *
   * The allowed APIs.
   *
   * Since: 4.12
   */
  obj_props[PROP_ALLOWED_APIS] =
    g_param_spec_flags ("allowed-apis", NULL, NULL,
                        GDK_TYPE_GL_API,
                        GDK_GL_API_GL | GDK_GL_API_GLES,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:api: (attributes org.gtk.Property.get=gtk_gl_area_get_api)
   *
   * The API currently in use.
   *
   * Since: 4.12
   */
  obj_props[PROP_API] =
    g_param_spec_flags ("api", NULL, NULL,
                        GDK_TYPE_GL_API,
                        0,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  gobject_class->set_property = gtk_gl_area_set_property;
  gobject_class->get_property = gtk_gl_area_get_property;
  gobject_class->notify = gtk_gl_area_notify;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);

  /**
   * GtkGLArea::render:
   * @area: the `GtkGLArea` that emitted the signal
   * @context: the `GdkGLContext` used by @area
   *
   * Emitted every time the contents of the `GtkGLArea` should be redrawn.
   *
   * The @context is bound to the @area prior to emitting this function,
   * and the buffers are painted to the window once the emission terminates.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  area_signals[RENDER] =
    g_signal_new (I_("render"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGLAreaClass, render),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gdk_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN, 1,
                  GDK_TYPE_GL_CONTEXT);
  g_signal_set_va_marshaller (area_signals[RENDER],
                              G_TYPE_FROM_CLASS (klass),
                              _gdk_marshal_BOOLEAN__OBJECTv);

  /**
   * GtkGLArea::resize:
   * @area: the `GtkGLArea` that emitted the signal
   * @width: the width of the viewport
   * @height: the height of the viewport
   *
   * Emitted once when the widget is realized, and then each time the widget
   * is changed while realized.
   *
   * This is useful in order to keep GL state up to date with the widget size,
   * like for instance camera properties which may depend on the width/height
   * ratio.
   *
   * The GL context for the area is guaranteed to be current when this signal
   * is emitted.
   *
   * The default handler sets up the GL viewport.
   */
  area_signals[RESIZE] =
    g_signal_new (I_("resize"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGLAreaClass, resize),
                  NULL, NULL,
                  _gdk_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  g_signal_set_va_marshaller (area_signals[RESIZE],
                              G_TYPE_FROM_CLASS (klass),
                              _gdk_marshal_VOID__INT_INTv);

  /**
   * GtkGLArea::create-context:
   * @area: the `GtkGLArea` that emitted the signal
   * @error: (nullable): location to store error information on failure
   *
   * Emitted when the widget is being realized.
   *
   * This allows you to override how the GL context is created.
   * This is useful when you want to reuse an existing GL context,
   * or if you want to try creating different kinds of GL options.
   *
   * If context creation fails then the signal handler can use
   * [method@Gtk.GLArea.set_error] to register a more detailed error
   * of how the construction failed.
   *
   * Returns: (transfer full): a newly created `GdkGLContext`;
   *     the `GtkGLArea` widget will take ownership of the returned value.
   */
  area_signals[CREATE_CONTEXT] =
    g_signal_new (I_("create-context"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGLAreaClass, create_context),
                  create_context_accumulator, NULL,
                  _gtk_marshal_OBJECT__VOID,
                  GDK_TYPE_GL_CONTEXT, 0);
  g_signal_set_va_marshaller (area_signals[CREATE_CONTEXT],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_OBJECT__VOIDv);
}

static void
gtk_gl_area_init (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  priv->auto_render = TRUE;
  priv->needs_render = TRUE;
  priv->required_gl_version = 0;
  priv->allowed_apis = GDK_GL_API_GL | GDK_GL_API_GLES;
}

/**
 * gtk_gl_area_new:
 *
 * Creates a new `GtkGLArea` widget.
 *
 * Returns: a new `GtkGLArea`
 */
GtkWidget *
gtk_gl_area_new (void)
{
  return g_object_new (GTK_TYPE_GL_AREA, NULL);
}

/**
 * gtk_gl_area_set_error:
 * @area: a `GtkGLArea`
 * @error: (nullable): a new `GError`, or %NULL to unset the error
 *
 * Sets an error on the area which will be shown instead of the
 * GL rendering.
 *
 * This is useful in the [signal@Gtk.GLArea::create-context]
 * signal if GL context creation fails.
 */
void
gtk_gl_area_set_error (GtkGLArea    *area,
                       const GError *error)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  g_clear_error (&priv->error);
  if (error)
    priv->error = g_error_copy (error);
}

/**
 * gtk_gl_area_get_error:
 * @area: a `GtkGLArea`
 *
 * Gets the current error set on the @area.
 *
 * Returns: (nullable) (transfer none): the `GError`
 */
GError *
gtk_gl_area_get_error (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), NULL);

  return priv->error;
}

/**
 * gtk_gl_area_set_use_es: (attributes org.gtk.Method.set_property=use-es)
 * @area: a `GtkGLArea`
 * @use_es: whether to use OpenGL or OpenGL ES
 *
 * Sets whether the @area should create an OpenGL or an OpenGL ES context.
 *
 * You should check the capabilities of the `GdkGLContext` before drawing
 * with either API.
 *
 * Deprecated: 4.12: Use [method@Gtk.GLArea.set_allowed_apis]
 */
void
gtk_gl_area_set_use_es (GtkGLArea *area,
                        gboolean   use_es)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));
  g_return_if_fail (!gtk_widget_get_realized (GTK_WIDGET (area)));

  if ((priv->allowed_apis == GDK_GL_API_GLES) == use_es)
    return;

  priv->allowed_apis = use_es ? GDK_GL_API_GLES : GDK_GL_API_GL;

  g_object_notify_by_pspec (G_OBJECT (area), obj_props[PROP_USE_ES]);
  g_object_notify_by_pspec (G_OBJECT (area), obj_props[PROP_ALLOWED_APIS]);
}

/**
 * gtk_gl_area_get_use_es: (attributes org.gtk.Method.get_property=use-es)
 * @area: a `GtkGLArea`
 *
 * Returns whether the `GtkGLArea` should use OpenGL ES.
 *
 * See [method@Gtk.GLArea.set_use_es].
 *
 * Returns: %TRUE if the `GtkGLArea` should create an OpenGL ES context
 *   and %FALSE otherwise
 *
 * Deprecated: 4.12: Use [method@Gtk.GLArea.get_api]
 */
gboolean
gtk_gl_area_get_use_es (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  if (priv->context)
    return gdk_gl_context_get_api (priv->context) == GDK_GL_API_GLES;
  else
    return priv->allowed_apis == GDK_GL_API_GLES;
}

/**
 * gtk_gl_area_set_allowed_apis:
 * @area: a `GtkGLArea`
 * @apis: the allowed APIs
 *
 * Sets the allowed APIs to create a context with.
 *
 * You should check [property@Gtk.GLArea:api] before drawing
 * with either API.
 *
 * By default, all APIs are allowed.
 *
 * Since: 4.12
 */
void
gtk_gl_area_set_allowed_apis (GtkGLArea *area,
                              GdkGLAPI   apis)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GdkGLAPI old_allowed_apis;

  g_return_if_fail (GTK_IS_GL_AREA (area));
  g_return_if_fail (!gtk_widget_get_realized (GTK_WIDGET (area)));

  if (priv->allowed_apis == apis)
    return;

  old_allowed_apis = priv->allowed_apis;

  priv->allowed_apis = apis;

  if ((old_allowed_apis == GDK_GL_API_GLES) != (apis == GDK_GL_API_GLES))
    g_object_notify_by_pspec (G_OBJECT (area), obj_props[PROP_USE_ES]);
  g_object_notify_by_pspec (G_OBJECT (area), obj_props[PROP_ALLOWED_APIS]);
}

/**
 * gtk_gl_area_get_allowed_apis:
 * @area: a `GtkGLArea`
 *
 * Gets the allowed APIs.
 *
 * See [method@Gtk.GLArea.set_allowed_apis].
 *
 * Returns: the allowed APIs
 *
 * Since: 4.12
 */
GdkGLAPI
gtk_gl_area_get_allowed_apis (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), 0);

  return priv->allowed_apis;
}

/**
 * gtk_gl_area_get_api:
 * @area: a `GtkGLArea`
 *
 * Gets the API that is currently in use.
 *
 * If the GL area has not been realized yet, 0 is returned.
 *
 * Returns: the currently used API
 *
 * Since: 4.12
 */
GdkGLAPI
gtk_gl_area_get_api (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), 0);

  if (priv->context)
    return gdk_gl_context_get_api (priv->context);
  else
    return 0;
}

/**
 * gtk_gl_area_set_required_version:
 * @area: a `GtkGLArea`
 * @major: the major version
 * @minor: the minor version
 *
 * Sets the required version of OpenGL to be used when creating
 * the context for the widget.
 *
 * This function must be called before the area has been realized.
 */
void
gtk_gl_area_set_required_version (GtkGLArea *area,
                                  int        major,
                                  int        minor)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));
  g_return_if_fail (!gtk_widget_get_realized (GTK_WIDGET (area)));

  priv->required_gl_version = major * 10 + minor;
}

/**
 * gtk_gl_area_get_required_version:
 * @area: a `GtkGLArea`
 * @major: (out): return location for the required major version
 * @minor: (out): return location for the required minor version
 *
 * Retrieves the required version of OpenGL.
 *
 * See [method@Gtk.GLArea.set_required_version].
 */
void
gtk_gl_area_get_required_version (GtkGLArea *area,
                                  int       *major,
                                  int       *minor)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  if (major != NULL)
    *major = priv->required_gl_version / 10;
  if (minor != NULL)
    *minor = priv->required_gl_version % 10;
}

/**
 * gtk_gl_area_get_has_depth_buffer: (attributes org.gtk.Method.get_property=has-depth-buffer)
 * @area: a `GtkGLArea`
 *
 * Returns whether the area has a depth buffer.
 *
 * Returns: %TRUE if the @area has a depth buffer, %FALSE otherwise
 */
gboolean
gtk_gl_area_get_has_depth_buffer (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_depth_buffer;
}

/**
 * gtk_gl_area_set_has_depth_buffer: (attributes org.gtk.Method.set_property=has-depth-buffer)
 * @area: a `GtkGLArea`
 * @has_depth_buffer: %TRUE to add a depth buffer
 *
 * Sets whether the `GtkGLArea` should use a depth buffer.
 *
 * If @has_depth_buffer is %TRUE the widget will allocate and
 * enable a depth buffer for the target framebuffer. Otherwise
 * there will be none.
 */
void
gtk_gl_area_set_has_depth_buffer (GtkGLArea *area,
                                  gboolean   has_depth_buffer)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_depth_buffer = !!has_depth_buffer;

  if (priv->has_depth_buffer != has_depth_buffer)
    {
      priv->has_depth_buffer = has_depth_buffer;

      g_object_notify (G_OBJECT (area), "has-depth-buffer");

      priv->have_buffers = FALSE;
    }
}

/**
 * gtk_gl_area_get_has_stencil_buffer: (attributes org.gtk.Method.get_property=has-stencil-buffer)
 * @area: a `GtkGLArea`
 *
 * Returns whether the area has a stencil buffer.
 *
 * Returns: %TRUE if the @area has a stencil buffer, %FALSE otherwise
 */
gboolean
gtk_gl_area_get_has_stencil_buffer (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_stencil_buffer;
}

/**
 * gtk_gl_area_set_has_stencil_buffer: (attributes org.gtk.Method.set_property=has-stencil-buffer)
 * @area: a `GtkGLArea`
 * @has_stencil_buffer: %TRUE to add a stencil buffer
 *
 * Sets whether the `GtkGLArea` should use a stencil buffer.
 *
 * If @has_stencil_buffer is %TRUE the widget will allocate and
 * enable a stencil buffer for the target framebuffer. Otherwise
 * there will be none.
 */
void
gtk_gl_area_set_has_stencil_buffer (GtkGLArea *area,
                                    gboolean   has_stencil_buffer)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_stencil_buffer = !!has_stencil_buffer;

  if (priv->has_stencil_buffer != has_stencil_buffer)
    {
      priv->has_stencil_buffer = has_stencil_buffer;

      g_object_notify (G_OBJECT (area), "has-stencil-buffer");

      priv->have_buffers = FALSE;
    }
}

/**
 * gtk_gl_area_queue_render:
 * @area: a `GtkGLArea`
 *
 * Marks the currently rendered data (if any) as invalid, and queues
 * a redraw of the widget.
 *
 * This ensures that the [signal@Gtk.GLArea::render] signal
 * is emitted during the draw.
 *
 * This is only needed when [method@Gtk.GLArea.set_auto_render] has
 * been called with a %FALSE value. The default behaviour is to
 * emit [signal@Gtk.GLArea::render] on each draw.
 */
void
gtk_gl_area_queue_render (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  priv->needs_render = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (area));
}


/**
 * gtk_gl_area_get_auto_render: (attributes org.gtk.Method.get_property=auto-render)
 * @area: a `GtkGLArea`
 *
 * Returns whether the area is in auto render mode or not.
 *
 * Returns: %TRUE if the @area is auto rendering, %FALSE otherwise
 */
gboolean
gtk_gl_area_get_auto_render (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->auto_render;
}

/**
 * gtk_gl_area_set_auto_render: (attributes org.gtk.Method.set_property=auto-render)
 * @area: a `GtkGLArea`
 * @auto_render: a boolean
 *
 * Sets whether the `GtkGLArea` is in auto render mode.
 *
 * If @auto_render is %TRUE the [signal@Gtk.GLArea::render] signal will
 * be emitted every time the widget draws. This is the default and is
 * useful if drawing the widget is faster.
 *
 * If @auto_render is %FALSE the data from previous rendering is kept
 * around and will be used for drawing the widget the next time,
 * unless the window is resized. In order to force a rendering
 * [method@Gtk.GLArea.queue_render] must be called. This mode is
 * useful when the scene changes seldom, but takes a long time to redraw.
 */
void
gtk_gl_area_set_auto_render (GtkGLArea *area,
                             gboolean   auto_render)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  auto_render = !!auto_render;

  if (priv->auto_render != auto_render)
    {
      priv->auto_render = auto_render;

      g_object_notify (G_OBJECT (area), "auto-render");

      if (auto_render)
        gtk_widget_queue_draw (GTK_WIDGET (area));
    }
}

/**
 * gtk_gl_area_get_context:
 * @area: a `GtkGLArea`
 *
 * Retrieves the `GdkGLContext` used by @area.
 *
 * Returns: (transfer none) (nullable): the `GdkGLContext`
 */
GdkGLContext *
gtk_gl_area_get_context (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), NULL);

  return priv->context;
}

/**
 * gtk_gl_area_make_current:
 * @area: a `GtkGLArea`
 *
 * Ensures that the `GdkGLContext` used by @area is associated with
 * the `GtkGLArea`.
 *
 * This function is automatically called before emitting the
 * [signal@Gtk.GLArea::render] signal, and doesn't normally need
 * to be called by application code.
 */
void
gtk_gl_area_make_current (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  if (priv->context != NULL)
    gdk_gl_context_make_current (priv->context);
}
