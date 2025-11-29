/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "reftest-snapshot.h"

#include "reftest-module.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <cairo-xlib.h>
#endif

#include <string.h>

#define REFTEST_TYPE_SCOPE               (reftest_scope_get_type ())

G_DECLARE_FINAL_TYPE (ReftestScope, reftest_scope, REFTEST, SCOPE, GtkBuilderCScope)

static GtkBuilderScopeInterface *parent_scope_iface;

struct _ReftestScope
{
  GtkBuilderCScope parent_instance;

  char *directory;
};

static GClosure *
reftest_scope_create_closure (GtkBuilderScope        *scope,
                              GtkBuilder             *builder,
                              const char             *function_name,
                              GtkBuilderClosureFlags  flags,
                              GObject                *object,
                              GError                **error)
{
  ReftestScope *self = REFTEST_SCOPE (scope);
  ReftestModule *module;
  GCallback func;
  GClosure *closure;
  char **split;

  split = g_strsplit (function_name, ":", -1);

  switch (g_strv_length (split))
    {
    case 1:
      closure = parent_scope_iface->create_closure (scope, builder, split[0], flags, object, error);
      break;

    case 2:
#ifdef _MSC_VER
      char *modulefile = g_strconcat (split[0], ".", G_MODULE_SUFFIX, NULL);
      module = reftest_module_new (self->directory, modulefile);
      g_free (modulefile);
#else
      module = reftest_module_new (self->directory, split[0]);
#endif
      if (module == NULL)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_FUNCTION,
                       "Could not load module '%s' from '%s' when looking up '%s': %s", split[0], self->directory, function_name, g_module_error ());
          return NULL;
        }
      func = reftest_module_lookup (module, split[1]);
      if (!func)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_FUNCTION,
                       "failed to lookup function for name '%s' in module '%s'", split[1], split[0]);
          return NULL;
        }

      if (object)
        {
          if (flags & GTK_BUILDER_CLOSURE_SWAPPED)
            closure = g_cclosure_new_object_swap (func, object);
          else
            closure = g_cclosure_new_object (func, object);
        }
      else
        {
          if (flags & GTK_BUILDER_CLOSURE_SWAPPED)
            closure = g_cclosure_new_swap (func, NULL, NULL);
          else
            closure = g_cclosure_new (func, NULL, NULL);
        }
      
      if (module)
        g_closure_add_finalize_notifier (closure, module, (GClosureNotify) reftest_module_unref);
      break;

    default:
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_FUNCTION,
                   "Could not find function named '%s'", function_name);
      return NULL;
    }

  g_strfreev (split);

  return closure;
}

static void
reftest_scope_scope_init (GtkBuilderScopeInterface *iface)
{
  iface->create_closure = reftest_scope_create_closure;

  parent_scope_iface = g_type_interface_peek_parent (iface);
}

G_DEFINE_TYPE_WITH_CODE (ReftestScope, reftest_scope, GTK_TYPE_BUILDER_CSCOPE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDER_SCOPE,
                                                reftest_scope_scope_init))

static void
reftest_scope_finalize (GObject *object)
{
  ReftestScope *self = REFTEST_SCOPE (object);

  g_free (self->directory);

  G_OBJECT_CLASS (reftest_scope_parent_class)->finalize (object);
}

static void
reftest_scope_class_init (ReftestScopeClass *scope_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (scope_class);

  object_class->finalize = reftest_scope_finalize;
}

static void
reftest_scope_init (ReftestScope *self)
{
}

static GtkBuilderScope *
reftest_scope_new (const char *directory)
{
  ReftestScope *result;

  g_return_val_if_fail (directory != NULL, NULL);

  result = g_object_new (REFTEST_TYPE_SCOPE, NULL);

  result->directory = g_strdup (directory);

  return GTK_BUILDER_SCOPE (result);
}

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }
  
  g_slist_free (list);

  return window;
}

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static int inhibit_count;
static GMainLoop *loop;

G_MODULE_EXPORT void
reftest_inhibit_snapshot (void)
{
  inhibit_count++;
}

G_MODULE_EXPORT void
reftest_uninhibit_snapshot (void)
{
  g_assert_true (inhibit_count > 0);
  inhibit_count--;
}

static void
draw_paintable (GdkPaintable *paintable,
                gpointer      out_texture)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GdkTexture *texture;
  GskRenderer *renderer;

  if (inhibit_count > 0)
    return;

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable,
                          snapshot,
                          gdk_paintable_get_intrinsic_width (paintable),
                          gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (snapshot);

  /* If the window literally draws nothing, we assume it hasn't been mapped yet and as such
   * the invalidations were only side effects of resizes.
   */
  if (node == NULL)
    return;

  renderer = gtk_native_get_renderer (
                 gtk_widget_get_native (
                     gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (paintable))));
  texture = gsk_renderer_render_texture (renderer,
                                         node,
                                         &GRAPHENE_RECT_INIT (
                                           0, 0,
                                           gdk_paintable_get_intrinsic_width (paintable),
                                           gdk_paintable_get_intrinsic_height (paintable)
                                         ));
  g_object_set_data_full (G_OBJECT (texture),
                          "source-render-node",
                          node,
                          (GDestroyNotify) gsk_render_node_unref);

  g_signal_handlers_disconnect_by_func (paintable, draw_paintable, out_texture);

  *(GdkTexture **) out_texture = texture;

  g_idle_add (quit_when_idle, loop);
}

static GdkTexture *
snapshot_widget (GtkWidget *widget)
{
  GdkPaintable *paintable;
  GdkTexture *texture = NULL;

  g_assert_true (gtk_widget_get_realized (widget));

  loop = g_main_loop_new (NULL, FALSE);

  /* We wait until the widget is drawn for the first time.
   *
   * We also use an inhibit mechanism, to give module functions a chance
   * to delay the snapshot.
   */
  paintable = gtk_widget_paintable_new (widget);
  g_signal_connect (paintable, "invalidate-contents", G_CALLBACK (draw_paintable), &texture);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (paintable);
  gtk_window_destroy (GTK_WINDOW (widget));

  return texture;
}

GdkTexture *
reftest_snapshot_ui_file (const char *ui_file)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GtkBuilderScope *scope;
  GError *error = NULL;
  char *directory;

  if (g_getenv ("REFTEST_MODULE_DIR"))
    directory = g_strdup (g_getenv ("REFTEST_MODULE_DIR"));
  else
    directory = g_path_get_dirname (ui_file);
  scope = reftest_scope_new (directory);
  g_free (directory);

  builder = gtk_builder_new ();
  gtk_builder_set_scope (builder, scope);
  g_object_unref (scope);

  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
  g_assert_true (window);

  gtk_window_present (GTK_WINDOW (window));

  return snapshot_widget (window);
}
