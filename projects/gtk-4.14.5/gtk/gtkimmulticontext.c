/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include <string.h>
#include <locale.h>

#include "gtkimmulticontext.h"
#include "gtkimmoduleprivate.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtksettings.h"


/**
 * GtkIMMulticontext:
 *
 * `GtkIMMulticontext` is an input method context supporting multiple,
 * switchable input methods.
 *
 * Text widgets such as `GtkText` or `GtkTextView` use a `GtkIMMultiContext`
 * to implement their `im-module` property for switching between different
 * input methods.
 */


struct _GtkIMMulticontextPrivate
{
  GtkIMContext          *delegate;

  GtkWidget             *client_widget;
  GdkRectangle           cursor_location;

  char                  *context_id;
  char                  *context_id_aux;

  guint                  use_preedit          : 1;
  guint                  have_cursor_location : 1;
  guint                  focus_in             : 1;
};

static void     gtk_im_multicontext_notify             (GObject                 *object,
                                                        GParamSpec              *pspec);
static void     gtk_im_multicontext_finalize           (GObject                 *object);

static void     gtk_im_multicontext_set_delegate       (GtkIMMulticontext       *multicontext,
							GtkIMContext            *delegate,
							gboolean                 finalizing);

static void     gtk_im_multicontext_set_client_widget  (GtkIMContext            *context,
							GtkWidget               *widget);
static void     gtk_im_multicontext_get_preedit_string (GtkIMContext            *context,
							char                   **str,
							PangoAttrList          **attrs,
							int                    *cursor_pos);
static gboolean gtk_im_multicontext_filter_keypress    (GtkIMContext            *context,
							GdkEvent                *event);
static void     gtk_im_multicontext_focus_in           (GtkIMContext            *context);
static void     gtk_im_multicontext_focus_out          (GtkIMContext            *context);
static void     gtk_im_multicontext_reset              (GtkIMContext            *context);
static void     gtk_im_multicontext_set_cursor_location (GtkIMContext            *context,
							GdkRectangle		*area);
static void     gtk_im_multicontext_set_use_preedit    (GtkIMContext            *context,
							gboolean                 use_preedit);
static gboolean gtk_im_multicontext_get_surrounding_with_selection
                                                       (GtkIMContext            *context,
                                                        char                   **text,
                                                        int                     *cursor_index,
                                                        int                     *anchor_index);
static void     gtk_im_multicontext_set_surrounding_with_selection
                                                       (GtkIMContext            *context,
                                                        const char              *text,
                                                        int                      len,
                                                        int                      cursor_index,
                                                        int                      anchor_index);

static void     gtk_im_multicontext_preedit_start_cb        (GtkIMContext      *delegate,
							     GtkIMMulticontext *multicontext);
static void     gtk_im_multicontext_preedit_end_cb          (GtkIMContext      *delegate,
							     GtkIMMulticontext *multicontext);
static void     gtk_im_multicontext_preedit_changed_cb      (GtkIMContext      *delegate,
							     GtkIMMulticontext *multicontext);
static void     gtk_im_multicontext_commit_cb               (GtkIMContext      *delegate,
							     const char        *str,
							     GtkIMMulticontext *multicontext);
static gboolean gtk_im_multicontext_retrieve_surrounding_cb (GtkIMContext      *delegate,
							     GtkIMMulticontext *multicontext);
static gboolean gtk_im_multicontext_delete_surrounding_cb   (GtkIMContext      *delegate,
							     int                offset,
							     int                n_chars,
							     GtkIMMulticontext *multicontext);
static gboolean gtk_im_multicontext_activate_osk_with_event (GtkIMContext *context,
                                                             GdkEvent     *event);

static void propagate_purpose (GtkIMMulticontext *context);

G_DEFINE_TYPE_WITH_PRIVATE (GtkIMMulticontext, gtk_im_multicontext, GTK_TYPE_IM_CONTEXT)

static void
gtk_im_multicontext_class_init (GtkIMMulticontextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);

  gobject_class->notify = gtk_im_multicontext_notify;

  im_context_class->set_client_widget = gtk_im_multicontext_set_client_widget;
  im_context_class->get_preedit_string = gtk_im_multicontext_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_multicontext_filter_keypress;
  im_context_class->focus_in = gtk_im_multicontext_focus_in;
  im_context_class->focus_out = gtk_im_multicontext_focus_out;
  im_context_class->reset = gtk_im_multicontext_reset;
  im_context_class->set_cursor_location = gtk_im_multicontext_set_cursor_location;
  im_context_class->set_use_preedit = gtk_im_multicontext_set_use_preedit;
  im_context_class->set_surrounding_with_selection = gtk_im_multicontext_set_surrounding_with_selection;
  im_context_class->get_surrounding_with_selection = gtk_im_multicontext_get_surrounding_with_selection;
  im_context_class->activate_osk_with_event = gtk_im_multicontext_activate_osk_with_event;

  gobject_class->finalize = gtk_im_multicontext_finalize;
}

static void
gtk_im_multicontext_init (GtkIMMulticontext *multicontext)
{
  GtkIMMulticontextPrivate *priv;
  
  multicontext->priv = gtk_im_multicontext_get_instance_private (multicontext);
  priv = multicontext->priv;

  priv->delegate = NULL;
  priv->use_preedit = TRUE;
  priv->have_cursor_location = FALSE;
  priv->focus_in = FALSE;
}

/**
 * gtk_im_multicontext_new:
 *
 * Creates a new `GtkIMMulticontext`.
 *
 * Returns: a new `GtkIMMulticontext`.
 */
GtkIMContext *
gtk_im_multicontext_new (void)
{
  return g_object_new (GTK_TYPE_IM_MULTICONTEXT, NULL);
}

static void
gtk_im_multicontext_finalize (GObject *object)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (object);
  GtkIMMulticontextPrivate *priv = multicontext->priv;

  gtk_im_multicontext_set_delegate (multicontext, NULL, TRUE);
  g_free (priv->context_id);
  g_free (priv->context_id_aux);

  G_OBJECT_CLASS (gtk_im_multicontext_parent_class)->finalize (object);
}

static void
gtk_im_multicontext_set_delegate (GtkIMMulticontext *multicontext,
			          GtkIMContext      *delegate,
			          gboolean           finalizing)
{
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  gboolean need_preedit_changed = FALSE;
  
  if (priv->delegate)
    {
      if (!finalizing)
	gtk_im_context_reset (priv->delegate);
      
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_preedit_start_cb,
					    multicontext);
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_preedit_end_cb,
					    multicontext);
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_preedit_changed_cb,
					    multicontext);
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_commit_cb,
					    multicontext);
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_retrieve_surrounding_cb,
					    multicontext);
      g_signal_handlers_disconnect_by_func (priv->delegate,
					    gtk_im_multicontext_delete_surrounding_cb,
					    multicontext);

      if (priv->client_widget)
        gtk_im_context_set_client_widget (priv->delegate, NULL);

      g_object_unref (priv->delegate);
      priv->delegate = NULL;

      if (!finalizing)
	need_preedit_changed = TRUE;
    }

  priv->delegate = delegate;

  if (priv->delegate)
    {
      g_object_ref (priv->delegate);

      propagate_purpose (multicontext);

      g_signal_connect (priv->delegate, "preedit-start",
			G_CALLBACK (gtk_im_multicontext_preedit_start_cb),
			multicontext);
      g_signal_connect (priv->delegate, "preedit-end",
			G_CALLBACK (gtk_im_multicontext_preedit_end_cb),
			multicontext);
      g_signal_connect (priv->delegate, "preedit-changed",
			G_CALLBACK (gtk_im_multicontext_preedit_changed_cb),
			multicontext);
      g_signal_connect (priv->delegate, "commit",
			G_CALLBACK (gtk_im_multicontext_commit_cb),
			multicontext);
      g_signal_connect (priv->delegate, "retrieve-surrounding",
			G_CALLBACK (gtk_im_multicontext_retrieve_surrounding_cb),
			multicontext);
      g_signal_connect (priv->delegate, "delete-surrounding",
			G_CALLBACK (gtk_im_multicontext_delete_surrounding_cb),
			multicontext);

      if (!priv->use_preedit)	/* Default is TRUE */
	gtk_im_context_set_use_preedit (delegate, FALSE);
      if (priv->client_widget)
	gtk_im_context_set_client_widget (delegate, priv->client_widget);
      if (priv->have_cursor_location)
	gtk_im_context_set_cursor_location (delegate, &priv->cursor_location);
      if (priv->focus_in)
	gtk_im_context_focus_in (delegate);
    }

  if (need_preedit_changed)
    g_signal_emit_by_name (multicontext, "preedit-changed");
}

static const char *
get_effective_context_id (GtkIMMulticontext *multicontext)
{
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  GdkDisplay *display;

  if (priv->context_id_aux)
    return priv->context_id_aux;

  if (priv->client_widget)
    display = gtk_widget_get_display (priv->client_widget);
  else
    display = gdk_display_get_default ();

  return _gtk_im_module_get_default_context_id (display);
}

static GtkIMContext *
gtk_im_multicontext_get_delegate (GtkIMMulticontext *multicontext)
{
  GtkIMMulticontextPrivate *priv = multicontext->priv;

  if (!priv->delegate)
    {
      GtkIMContext *delegate;

      g_free (priv->context_id);

      priv->context_id = g_strdup (get_effective_context_id (multicontext));

      delegate = _gtk_im_module_create (priv->context_id);
      if (delegate)
        {
          gtk_im_multicontext_set_delegate (multicontext, delegate, FALSE);
          g_object_unref (delegate);
        }
    }

  return priv->delegate;
}

static void
im_module_setting_changed (GtkSettings       *settings, 
                           GParamSpec        *pspec,
                           GtkIMMulticontext *self)
{
  gtk_im_multicontext_set_delegate (self, NULL, FALSE);
}

static void
gtk_im_multicontext_set_client_widget (GtkIMContext *context,
				       GtkWidget    *widget)
{
  GtkIMMulticontext *self = GTK_IM_MULTICONTEXT (context);
  GtkIMMulticontextPrivate *priv = self->priv;
  GtkIMContext *delegate;
  GtkSettings *settings;

  if (priv->client_widget == widget)
    return;

  gtk_im_multicontext_set_delegate (self, NULL, TRUE);

  if (priv->client_widget != NULL)
    {
      settings = gtk_widget_get_settings (priv->client_widget);

      g_signal_handlers_disconnect_by_func (settings,
                                            im_module_setting_changed,
                                            self);
    }

  priv->client_widget = widget;

  if (widget)
    {
      settings = gtk_widget_get_settings (widget);

      g_signal_connect (settings, "notify::gtk-im-module",
                        G_CALLBACK (im_module_setting_changed),
                        self);

      delegate = gtk_im_multicontext_get_delegate (self);
      if (delegate)
        gtk_im_context_set_client_widget (delegate, widget);
    }
}

static void
gtk_im_multicontext_get_preedit_string (GtkIMContext   *context,
					char          **str,
					PangoAttrList **attrs,
					int            *cursor_pos)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  if (delegate)
    gtk_im_context_get_preedit_string (delegate, str, attrs, cursor_pos);
  else
    {
      if (str)
	*str = g_strdup ("");
      if (attrs)
	*attrs = pango_attr_list_new ();
    }
}

static gboolean
gtk_im_multicontext_filter_keypress (GtkIMContext *context,
				     GdkEvent     *event)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);
  guint keyval, state;

  if (delegate)
    {
      return gtk_im_context_filter_keypress (delegate, event);
    }
  else
    {
      GdkModifierType no_text_input_mask;

      keyval = gdk_key_event_get_keyval (event);
      state = gdk_event_get_modifier_state (event);

      no_text_input_mask = GDK_ALT_MASK|GDK_CONTROL_MASK;

      if (gdk_event_get_event_type (event) == GDK_KEY_PRESS &&
          (state & no_text_input_mask) == 0)
        {
          gunichar ch;

          ch = gdk_keyval_to_unicode (keyval);
          if (ch != 0 && !g_unichar_iscntrl (ch))
            {
              int len;
              char buf[10];

              len = g_unichar_to_utf8 (ch, buf);
              buf[len] = '\0';

              g_signal_emit_by_name (multicontext, "commit", buf);

              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
gtk_im_multicontext_focus_in (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  priv->focus_in = TRUE;

  if (delegate)
    gtk_im_context_focus_in (delegate);
}

static void
gtk_im_multicontext_focus_out (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  priv->focus_in = FALSE;

  if (delegate)
    gtk_im_context_focus_out (delegate);
}

static void
gtk_im_multicontext_reset (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  if (delegate)
    gtk_im_context_reset (delegate);
}

static void
gtk_im_multicontext_set_cursor_location (GtkIMContext   *context,
					 GdkRectangle   *area)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  priv->have_cursor_location = TRUE;
  priv->cursor_location = *area;

  if (delegate)
    gtk_im_context_set_cursor_location (delegate, area);
}

static void
gtk_im_multicontext_set_use_preedit (GtkIMContext   *context,
				     gboolean	    use_preedit)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMMulticontextPrivate *priv = multicontext->priv;
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  use_preedit = use_preedit != FALSE;

  priv->use_preedit = use_preedit;

  if (delegate)
    gtk_im_context_set_use_preedit (delegate, use_preedit);
}

static gboolean
gtk_im_multicontext_get_surrounding_with_selection (GtkIMContext  *context,
                                                    char         **text,
                                                    int           *cursor_index,
                                                    int           *anchor_index)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  if (delegate)
    return gtk_im_context_get_surrounding_with_selection (delegate, text, cursor_index, anchor_index);
  else
    {
      if (text)
        *text = NULL;
      if (cursor_index)
        *cursor_index = 0;
      if (anchor_index)
        *anchor_index = 0;

      return FALSE;
    }
}

static void
gtk_im_multicontext_set_surrounding_with_selection (GtkIMContext *context,
                                                    const char   *text,
                                                    int           len,
                                                    int           cursor_index,
                                                    int           anchor_index)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  if (delegate)
    gtk_im_context_set_surrounding_with_selection (delegate, text, len, cursor_index, anchor_index);
}

static gboolean
gtk_im_multicontext_activate_osk_with_event (GtkIMContext *context,
                                             GdkEvent     *event)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *delegate = gtk_im_multicontext_get_delegate (multicontext);

  if (delegate)
    return gtk_im_context_activate_osk (delegate, event);
  else
    return FALSE;
}

static void
gtk_im_multicontext_preedit_start_cb   (GtkIMContext      *delegate,
					GtkIMMulticontext *multicontext)
{
  g_signal_emit_by_name (multicontext, "preedit-start");
}

static void
gtk_im_multicontext_preedit_end_cb (GtkIMContext      *delegate,
				    GtkIMMulticontext *multicontext)
{
  g_signal_emit_by_name (multicontext, "preedit-end");
}

static void
gtk_im_multicontext_preedit_changed_cb (GtkIMContext      *delegate,
					GtkIMMulticontext *multicontext)
{
  g_signal_emit_by_name (multicontext, "preedit-changed");
}

static void
gtk_im_multicontext_commit_cb (GtkIMContext      *delegate,
			       const char        *str,
			       GtkIMMulticontext *multicontext)
{
  g_signal_emit_by_name (multicontext, "commit", str);
}

static gboolean
gtk_im_multicontext_retrieve_surrounding_cb (GtkIMContext      *delegate,
					     GtkIMMulticontext *multicontext)
{
  gboolean result;
  
  g_signal_emit_by_name (multicontext, "retrieve-surrounding", &result);

  return result;
}

static gboolean
gtk_im_multicontext_delete_surrounding_cb (GtkIMContext      *delegate,
					   int                offset,
					   int                n_chars,
					   GtkIMMulticontext *multicontext)
{
  gboolean result;
  
  g_signal_emit_by_name (multicontext, "delete-surrounding",
			 offset, n_chars, &result);

  return result;
}

/**
 * gtk_im_multicontext_get_context_id:
 * @context: a `GtkIMMulticontext`
 *
 * Gets the id of the currently active delegate of the @context.
 *
 * Returns: the id of the currently active delegate
 */
const char *
gtk_im_multicontext_get_context_id (GtkIMMulticontext *context)
{
  GtkIMMulticontextPrivate *priv = context->priv;

  g_return_val_if_fail (GTK_IS_IM_MULTICONTEXT (context), NULL);

  if (priv->context_id == NULL)
    gtk_im_multicontext_get_delegate (context);

  return priv->context_id;
}

/**
 * gtk_im_multicontext_set_context_id:
 * @context: a `GtkIMMulticontext`
 * @context_id: (nullable): the id to use
 *
 * Sets the context id for @context.
 *
 * This causes the currently active delegate of @context to be
 * replaced by the delegate corresponding to the new context id.
 *
 * Setting this to a non-%NULL value overrides the system-wide
 * IM module setting. See the [property@Gtk.Settings:gtk-im-module]
 * property.
 */
void
gtk_im_multicontext_set_context_id (GtkIMMulticontext *context,
                                    const char        *context_id)
{
  GtkIMMulticontextPrivate *priv;

  g_return_if_fail (GTK_IS_IM_MULTICONTEXT (context));

  priv = context->priv;

  gtk_im_context_reset (GTK_IM_CONTEXT (context));
  g_free (priv->context_id_aux);
  priv->context_id_aux = g_strdup (context_id);
  gtk_im_multicontext_set_delegate (context, NULL, FALSE);
}

static void
propagate_purpose (GtkIMMulticontext *context)
{
  GtkInputPurpose purpose;
  GtkInputHints hints;

  if (context->priv->delegate == NULL)
    return;

  g_object_get (context, "input-purpose", &purpose, NULL);
  g_object_set (context->priv->delegate, "input-purpose", purpose, NULL);

  g_object_get (context, "input-hints", &hints, NULL);
  g_object_set (context->priv->delegate, "input-hints", hints, NULL);
}

static void
gtk_im_multicontext_notify (GObject      *object,
                            GParamSpec   *pspec)
{
  propagate_purpose (GTK_IM_MULTICONTEXT (object));
}
