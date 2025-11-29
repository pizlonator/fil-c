/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkstylecontextprivate.h"

#include <gdk/gdk.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsstransientnodeprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtksettingsprivate.h"
#include "deprecated/gtkrender.h"


/**
 * GtkStyleContext:
 *
 * `GtkStyleContext` stores styling information affecting a widget.
 *
 * In order to construct the final style information, `GtkStyleContext`
 * queries information from all attached `GtkStyleProviders`. Style
 * providers can be either attached explicitly to the context through
 * [method@Gtk.StyleContext.add_provider], or to the display through
 * [func@Gtk.StyleContext.add_provider_for_display]. The resulting
 * style is a combination of all providers’ information in priority order.
 *
 * For GTK widgets, any `GtkStyleContext` returned by
 * [method@Gtk.Widget.get_style_context] will already have a `GdkDisplay`
 * and RTL/LTR information set. The style context will also be updated
 * automatically if any of these settings change on the widget.
 *
 * ## Style Classes
 *
 * Widgets can add style classes to their context, which can be used to associate
 * different styles by class. The documentation for individual widgets lists
 * which style classes it uses itself, and which style classes may be added by
 * applications to affect their appearance.
 *
 * # Custom styling in UI libraries and applications
 *
 * If you are developing a library with custom widgets that render differently
 * than standard components, you may need to add a `GtkStyleProvider` yourself
 * with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK priority, either a
 * `GtkCssProvider` or a custom object implementing the `GtkStyleProvider`
 * interface. This way themes may still attempt to style your UI elements in
 * a different way if needed so.
 *
 * If you are using custom styling on an applications, you probably want then
 * to make your style information prevail to the theme’s, so you must use
 * a `GtkStyleProvider` with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority, keep in mind that the user settings in
 * `XDG_CONFIG_HOME/gtk-4.0/gtk.css` will
 * still take precedence over your changes, as it uses the
 * %GTK_STYLE_PROVIDER_PRIORITY_USER priority.
 *
 * Deprecated: 4.10: The relevant API has been moved to [class@Gtk.Widget]
 *   where applicable; otherwise, there is no replacement for querying the
 *   style machinery. Stylable UI elements should use widgets.
 */

#define CURSOR_ASPECT_RATIO (0.04)

struct _GtkStyleContextPrivate
{
  GdkDisplay *display;

  guint cascade_changed_id;
  GtkStyleCascade *cascade;
  GtkCssNode *cssnode;
  GSList *saved_nodes;
};
typedef struct _GtkStyleContextPrivate GtkStyleContextPrivate;

enum {
  PROP_0,
  PROP_DISPLAY,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

static void gtk_style_context_finalize (GObject *object);

static void gtk_style_context_impl_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
static void gtk_style_context_impl_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);

static GtkCssNode * gtk_style_context_get_root (GtkStyleContext *context);

G_DEFINE_TYPE_WITH_PRIVATE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

  properties[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_style_context_pop_style_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (priv->saved_nodes != NULL);

  if (GTK_IS_CSS_TRANSIENT_NODE (priv->cssnode))
    gtk_css_node_set_parent (priv->cssnode, NULL);
  g_object_unref (priv->cssnode);
  priv->cssnode = priv->saved_nodes->data;
  priv->saved_nodes = g_slist_remove (priv->saved_nodes, priv->cssnode);
}

static void
gtk_style_context_cascade_changed (GtkStyleCascade *cascade,
                                   GtkStyleContext *context)
{
  gtk_css_node_invalidate_style_provider (gtk_style_context_get_root (context));
}

static void
gtk_style_context_set_cascade (GtkStyleContext *context,
                               GtkStyleCascade *cascade)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  if (priv->cascade == cascade)
    return;

  if (priv->cascade)
    {
      g_signal_handler_disconnect (priv->cascade, priv->cascade_changed_id);
      priv->cascade_changed_id = 0;
      g_object_unref (priv->cascade);
    }

  if (cascade)
    {
      g_object_ref (cascade);
      priv->cascade_changed_id = g_signal_connect (cascade,
                                                   "gtk-private-changed",
                                                   G_CALLBACK (gtk_style_context_cascade_changed),
                                                   context);
    }

  priv->cascade = cascade;

  if (cascade && priv->cssnode != NULL)
    gtk_style_context_cascade_changed (cascade, context);
}

static void
gtk_style_context_init (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  priv->display = gdk_display_get_default ();

  if (priv->display == NULL)
    g_error ("Can't create a GtkStyleContext without a display connection");

  gtk_style_context_set_cascade (context,
                                 _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display), 1));
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  while (priv->saved_nodes)
    gtk_style_context_pop_style_node (context);

  gtk_style_context_set_cascade (context, NULL);

  if (priv->cssnode)
    g_object_unref (priv->cssnode);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
gtk_style_context_impl_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_style_context_set_display (context, g_value_get_object (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_context_impl_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkStyleContext *context = GTK_STYLE_CONTEXT (object);
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* returns TRUE if someone called gtk_style_context_save() but hasn’t
 * called gtk_style_context_restore() yet.
 * In those situations we don’t invalidate the context when somebody
 * changes state/classes.
 */
static gboolean
gtk_style_context_is_saved (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return priv->saved_nodes != NULL;
}

static GtkCssNode *
gtk_style_context_get_root (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  if (priv->saved_nodes != NULL)
    return g_slist_last (priv->saved_nodes)->data;
  else
    return priv->cssnode;
}

GtkStyleProvider *
gtk_style_context_get_style_provider (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return GTK_STYLE_PROVIDER (priv->cascade);
}

static gboolean
gtk_style_context_has_custom_cascade (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkSettings *settings = gtk_settings_get_for_display (priv->display);

  return priv->cascade != _gtk_settings_get_style_cascade (settings, _gtk_style_cascade_get_scale (priv->cascade));
}

GtkCssStyle *
gtk_style_context_lookup_style (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  /* Code will recreate style if it was changed */
  return gtk_css_node_get_style (priv->cssnode);
}

GtkCssNode*
gtk_style_context_get_node (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  return priv->cssnode;
}

GtkStyleContext *
gtk_style_context_new_for_node (GtkCssNode *node)
{
  GtkStyleContext *context;
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CSS_NODE (node), NULL);

  context = g_object_new (GTK_TYPE_STYLE_CONTEXT, NULL);
  priv = gtk_style_context_get_instance_private (context);
  priv->cssnode = g_object_ref (node);

  return context;
}

/**
 * gtk_style_context_add_provider:
 * @context: a `GtkStyleContext`
 * @provider: a `GtkStyleProvider`
 * @priority: the priority of the style provider. The lower
 *   it is, the earlier it will be used in the style construction.
 *   Typically this will be in the range between
 *   %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *   %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a style provider to @context, to be used in style construction.
 *
 * Note that a style provider added by this function only affects
 * the style of the widget to which @context belongs. If you want
 * to affect the style of all widgets, use
 * [func@Gtk.StyleContext.add_provider_for_display].
 *
 * Note: If both priorities are the same, a `GtkStyleProvider`
 * added through this function takes precedence over another added
 * through [func@Gtk.StyleContext.add_provider_for_display].
 *
 * Deprecated: 4.10: Use style classes instead
 */
void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (!gtk_style_context_has_custom_cascade (context))
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_style_cascade_new ();
      _gtk_style_cascade_set_scale (new_cascade, _gtk_style_cascade_get_scale (priv->cascade));
      _gtk_style_cascade_set_parent (new_cascade,
                                     _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display), 1));
      _gtk_style_cascade_add_provider (new_cascade, provider, priority);
      gtk_style_context_set_cascade (context, new_cascade);
      g_object_unref (new_cascade);
    }
  else
    {
      _gtk_style_cascade_add_provider (priv->cascade, provider, priority);
    }
}

/**
 * gtk_style_context_remove_provider:
 * @context: a `GtkStyleContext`
 * @provider: a `GtkStyleProvider`
 *
 * Removes @provider from the style providers list in @context.
 *
 * Deprecated: 4.10
 */
void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  if (!gtk_style_context_has_custom_cascade (context))
    return;

  _gtk_style_cascade_remove_provider (priv->cascade, provider);
}

/**
 * gtk_style_context_set_state:
 * @context: a `GtkStyleContext`
 * @flags: state to represent
 *
 * Sets the state to be used for style matching.
 *
 * Deprecated: 4.10: You should not use this api
 */
void
gtk_style_context_set_state (GtkStyleContext *context,
                             GtkStateFlags    flags)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_node_set_state (priv->cssnode, flags);
}

/**
 * gtk_style_context_get_state:
 * @context: a `GtkStyleContext`
 *
 * Returns the state used for style matching.
 *
 * This method should only be used to retrieve the `GtkStateFlags`
 * to pass to `GtkStyleContext` methods, like
 * [method@Gtk.StyleContext.get_padding].
 * If you need to retrieve the current state of a `GtkWidget`, use
 * [method@Gtk.Widget.get_state_flags].
 *
 * Returns: the state flags
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.get_state_flags] instead
 **/
GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return gtk_css_node_get_state (priv->cssnode);
}

/**
 * gtk_style_context_set_scale:
 * @context: a `GtkStyleContext`
 * @scale: scale
 *
 * Sets the scale to use when getting image assets for the style.
 *
 * Deprecated: 4.10: You should not use this api
 **/
void
gtk_style_context_set_scale (GtkStyleContext *context,
                             int              scale)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (scale == _gtk_style_cascade_get_scale (priv->cascade))
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      _gtk_style_cascade_set_scale (priv->cascade, scale);
    }
  else
    {
      GtkStyleCascade *new_cascade;

      new_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (priv->display),
                                                     scale);
      gtk_style_context_set_cascade (context, new_cascade);
    }
}

/**
 * gtk_style_context_get_scale:
 * @context: a `GtkStyleContext`
 *
 * Returns the scale used for assets.
 *
 * Returns: the scale
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.get_scale_factor] instead
 **/
int
gtk_style_context_get_scale (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return _gtk_style_cascade_get_scale (priv->cascade);
}

/*
 * gtk_style_context_save_to_node:
 * @context: a `GtkStyleContext`
 * @node: the node to save to
 *
 * Saves the @context state to a node.
 *
 * This allows temporary modifications done through
 * [method@Gtk.StyleContext.add_class],
 * [method@Gtk.StyleContext.remove_class],
 * [method@Gtk.StyleContext.set_state] etc.
 *
 * Rendering using [func@Gtk.render_background] or similar
 * functions are done using the given @node.
 *
 * To undo, call [method@Gtk.StyleContext.restore].
 * The matching call to [method@Gtk.StyleContext.restore]
 * must be done before GTK returns to the main loop.
 */
void
gtk_style_context_save_to_node (GtkStyleContext *context,
                                GtkCssNode      *node)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_CSS_NODE (node));

  priv->saved_nodes = g_slist_prepend (priv->saved_nodes, priv->cssnode);
  priv->cssnode = g_object_ref (node);
}

/**
 * gtk_style_context_save:
 * @context: a `GtkStyleContext`
 *
 * Saves the @context state.
 *
 * This allows temporary modifications done through
 * [method@Gtk.StyleContext.add_class],
 * [method@Gtk.StyleContext.remove_class],
 * [method@Gtk.StyleContext.set_state] to be quickly
 * reverted in one go through [method@Gtk.StyleContext.restore].
 *
 * The matching call to [method@Gtk.StyleContext.restore]
 * must be done before GTK returns to the main loop.
 *
 * Deprecated: 4.10: This API will be removed in GTK 5
 **/
void
gtk_style_context_save (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssNode *cssnode;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));


  /* Make sure we have the style existing. It is the
   * parent of the new saved node after all.
   */
  if (!gtk_style_context_is_saved (context))
    gtk_style_context_lookup_style (context);

  cssnode = gtk_css_transient_node_new (priv->cssnode);
  gtk_css_node_set_parent (cssnode, gtk_style_context_get_root (context));
  gtk_style_context_save_to_node (context, cssnode);

  g_object_unref (cssnode);
}

/**
 * gtk_style_context_restore:
 * @context: a `GtkStyleContext`
 *
 * Restores @context state to a previous stage.
 *
 * See [method@Gtk.StyleContext.save].
 *
 * Deprecated: 4.10: This API will be removed in GTK 5
 **/
void
gtk_style_context_restore (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (priv->saved_nodes == NULL)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");
      return;
    }

  gtk_style_context_pop_style_node (context);
}

/**
 * gtk_style_context_add_class:
 * @context: a `GtkStyleContext`
 * @class_name: class name to use in styling
 *
 * Adds a style class to @context, so later uses of the
 * style context will make use of this new class for styling.
 *
 * In the CSS file format, a `GtkEntry` defining a “search”
 * class, would be matched by:
 *
 * ```css
 * entry.search { ... }
 * ```
 *
 * While any widget defining a “search” class would be
 * matched by:
 * ```css
 * .search { ... }
 * ```
 * Deprecated: 4.10: Use [method@Gtk.Widget.add_css_class] instead
 */
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const char      *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_from_string (class_name);

  gtk_css_node_add_class (priv->cssnode, class_quark);
}

/**
 * gtk_style_context_remove_class:
 * @context: a `GtkStyleContext`
 * @class_name: class name to remove
 *
 * Removes @class_name from @context.
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.remove_css_class] instead
 */
void
gtk_style_context_remove_class (GtkStyleContext *context,
                                const char      *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return;

  gtk_css_node_remove_class (priv->cssnode, class_quark);
}

/**
 * gtk_style_context_has_class:
 * @context: a `GtkStyleContext`
 * @class_name: a class name
 *
 * Returns %TRUE if @context currently has defined the
 * given class name.
 *
 * Returns: %TRUE if @context has @class_name defined
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.has_css_class] instead
 **/
gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const char      *class_name)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);
  if (!class_quark)
    return FALSE;

  return gtk_css_node_has_class (priv->cssnode, class_quark);
}

GtkCssValue *
_gtk_style_context_peek_property (GtkStyleContext *context,
                                  guint            property_id)
{
  GtkCssStyle *values = gtk_style_context_lookup_style (context);

  return gtk_css_style_get_value (values, property_id);
}

/**
 * gtk_style_context_set_display:
 * @context: a `GtkStyleContext`
 * @display: a `GdkDisplay`
 *
 * Attaches @context to the given display.
 *
 * The display is used to add style information from “global”
 * style providers, such as the display's `GtkSettings` instance.
 *
 * If you are using a `GtkStyleContext` returned from
 * [method@Gtk.Widget.get_style_context], you do not need to
 * call this yourself.
 *
 * Deprecated: 4.10: You should not use this api
 */
void
gtk_style_context_set_display (GtkStyleContext *context,
                               GdkDisplay      *display)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkStyleCascade *display_cascade;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (priv->display == display)
    return;

  if (gtk_style_context_has_custom_cascade (context))
    {
      display_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display), 1);
      _gtk_style_cascade_set_parent (priv->cascade, display_cascade);
    }
  else
    {
      display_cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (display),
                                                        _gtk_style_cascade_get_scale (priv->cascade));
      gtk_style_context_set_cascade (context, display_cascade);
    }

  priv->display = display;

  g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_DISPLAY]);
}

/**
 * gtk_style_context_get_display:
 * @context: a `GtkStyleContext`
 *
 * Returns the `GdkDisplay` to which @context is attached.
 *
 * Returns: (transfer none): a `GdkDisplay`.
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.get_display] instead
 */
GdkDisplay *
gtk_style_context_get_display (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return priv->display;
}

static gboolean
gtk_style_context_resolve_color (GtkStyleContext    *context,
                                 GtkCssValue        *color,
                                 GdkRGBA            *result)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  val = _gtk_css_color_value_resolve (color,
                                      GTK_STYLE_PROVIDER (priv->cascade),
                                      _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR),
                                      NULL);
  if (val == NULL)
    return FALSE;

  *result = *gtk_css_color_value_get_rgba (val);
  _gtk_css_value_unref (val);
  return TRUE;
}

/**
 * gtk_style_context_lookup_color:
 * @context: a `GtkStyleContext`
 * @color_name: color name to lookup
 * @color: (out): Return location for the looked up color
 *
 * Looks up and resolves a color name in the @context color map.
 *
 * Returns: %TRUE if @color_name was found and resolved, %FALSE otherwise
 *
 * Deprecated: 4.10: This api will be removed in GTK 5
 */
gboolean
gtk_style_context_lookup_color (GtkStyleContext *context,
                                const char      *color_name,
                                GdkRGBA         *color)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GtkCssValue *value;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  value = gtk_style_provider_get_color (GTK_STYLE_PROVIDER (priv->cascade), color_name);
  if (value == NULL)
    return FALSE;

  return gtk_style_context_resolve_color (context, value, color);
}

/**
 * gtk_style_context_get_color:
 * @context: a `GtkStyleContext`
 * @color: (out): return value for the foreground color
 *
 * Gets the foreground color for a given state.
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.get_color] instead
 */
void
gtk_style_context_get_color (GtkStyleContext *context,
                             GdkRGBA         *color)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  *color = *gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
}

/**
 * gtk_style_context_get_border:
 * @context: a `GtkStyleContext`
 * @border: (out): return value for the border settings
 *
 * Gets the border for a given state as a `GtkBorder`.
 *
 * Deprecated: 4.10: This api will be removed in GTK 5
 */
void
gtk_style_context_get_border (GtkStyleContext *context,
                              GtkBorder       *border)
{
  GtkCssStyle *style;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  border->top = round (_gtk_css_number_value_get (style->border->border_top_width, 100));
  border->right = round (_gtk_css_number_value_get (style->border->border_right_width, 100));
  border->bottom = round (_gtk_css_number_value_get (style->border->border_bottom_width, 100));
  border->left = round (_gtk_css_number_value_get (style->border->border_left_width, 100));
}

/**
 * gtk_style_context_get_padding:
 * @context: a `GtkStyleContext`
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a `GtkBorder`.
 *
 * Deprecated: 4.10: This api will be removed in GTK 5
 */
void
gtk_style_context_get_padding (GtkStyleContext *context,
                               GtkBorder       *padding)
{
  GtkCssStyle *style;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  padding->top = round (_gtk_css_number_value_get (style->size->padding_top, 100));
  padding->right = round (_gtk_css_number_value_get (style->size->padding_right, 100));
  padding->bottom = round (_gtk_css_number_value_get (style->size->padding_bottom, 100));
  padding->left = round (_gtk_css_number_value_get (style->size->padding_left, 100));
}

/**
 * gtk_style_context_get_margin:
 * @context: a `GtkStyleContext`
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a `GtkBorder`.
 *
 * Deprecated: 4.10: This api will be removed in GTK 5
 */
void
gtk_style_context_get_margin (GtkStyleContext *context,
                              GtkBorder       *margin)
{
  GtkCssStyle *style;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  style = gtk_style_context_lookup_style (context);

  margin->top = round (_gtk_css_number_value_get (style->size->margin_top, 100));
  margin->right = round (_gtk_css_number_value_get (style->size->margin_right, 100));
  margin->bottom = round (_gtk_css_number_value_get (style->size->margin_bottom, 100));
  margin->left = round (_gtk_css_number_value_get (style->size->margin_left, 100));
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  GtkCssStyle *style;

  style = gtk_style_context_lookup_style (context);

  if (primary_color)
    *primary_color = *gtk_css_color_value_get_rgba (style->font->caret_color ? style->font->caret_color : style->core->color);

  if (secondary_color)
    *secondary_color = *gtk_css_color_value_get_rgba (style->font->secondary_caret_color ? style->font->secondary_caret_color : style->core->color);
}

/**
 * GtkStyleContextPrintFlags:
 * @GTK_STYLE_CONTEXT_PRINT_NONE: Default value.
 * @GTK_STYLE_CONTEXT_PRINT_RECURSE: Print the entire tree of
 *   CSS nodes starting at the style context's node
 * @GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE: Show the values of the
 *   CSS properties for each node
 * @GTK_STYLE_CONTEXT_PRINT_SHOW_CHANGE: Show information about
 *   what changes affect the styles
 *
 * Flags that modify the behavior of gtk_style_context_to_string().
 *
 * New values may be added to this enumeration.
 */

/**
 * gtk_style_context_to_string:
 * @context: a `GtkStyleContext`
 * @flags: Flags that determine what to print
 *
 * Converts the style context into a string representation.
 *
 * The string representation always includes information about
 * the name, state, id, visibility and style classes of the CSS
 * node that is backing @context. Depending on the flags, more
 * information may be included.
 *
 * This function is intended for testing and debugging of the
 * CSS implementation in GTK. There are no guarantees about
 * the format of the returned string, it may change.
 *
 * Returns: a newly allocated string representing @context
 *
 * Deprecated: 4.10: This api will be removed in GTK 5
 */
char *
gtk_style_context_to_string (GtkStyleContext           *context,
                             GtkStyleContextPrintFlags  flags)
{
  GtkStyleContextPrivate *priv = gtk_style_context_get_instance_private (context);
  GString *string;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  string = g_string_new ("");

  gtk_css_node_print (priv->cssnode, (GtkCssNodePrintFlags)flags, string, 0);

  return g_string_free (string, FALSE);
}
