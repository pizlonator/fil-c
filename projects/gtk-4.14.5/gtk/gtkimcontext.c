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
#include "gtkimcontext.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"

#include "gdk/gdkeventsprivate.h"

/**
 * GtkIMContext:
 *
 * `GtkIMContext` defines the interface for GTK input methods.
 *
 * `GtkIMContext` is used by GTK text input widgets like `GtkText`
 * to map from key events to Unicode character strings.
 *
 * An input method may consume multiple key events in sequence before finally
 * outputting the composed result. This is called *preediting*, and an input
 * method may provide feedback about this process by displaying the intermediate
 * composition states as preedit text. To do so, the `GtkIMContext` will emit
 * [signal@Gtk.IMContext::preedit-start], [signal@Gtk.IMContext::preedit-changed]
 * and [signal@Gtk.IMContext::preedit-end] signals.
 *
 * For instance, the built-in GTK input method [class@Gtk.IMContextSimple]
 * implements the input of arbitrary Unicode code points by holding down the
 * <kbd>Control</kbd> and <kbd>Shift</kbd> keys and then typing <kbd>u</kbd>
 * followed by the hexadecimal digits of the code point. When releasing the
 * <kbd>Control</kbd> and <kbd>Shift</kbd> keys, preediting ends and the
 * character is inserted as text. For example,
 *
 *     Ctrl+Shift+u 2 0 A C
 *
 * results in the € sign.
 *
 * Additional input methods can be made available for use by GTK widgets as
 * loadable modules. An input method module is a small shared library which
 * provides a `GIOExtension` for the extension point named "gtk-im-module".
 *
 * To connect a widget to the users preferred input method, you should use
 * [class@Gtk.IMMulticontext].
 */

enum {
  PREEDIT_START,
  PREEDIT_END,
  PREEDIT_CHANGED,
  COMMIT,
  RETRIEVE_SURROUNDING,
  DELETE_SURROUNDING,
  LAST_SIGNAL
};

enum {
  PROP_INPUT_PURPOSE = 1,
  PROP_INPUT_HINTS,
  LAST_PROPERTY
};

static guint im_context_signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

typedef struct _GtkIMContextPrivate GtkIMContextPrivate;
struct _GtkIMContextPrivate {
  GtkInputPurpose purpose;
  GtkInputHints hints;
};

static void     gtk_im_context_real_get_preedit_string (GtkIMContext   *context,
							char          **str,
							PangoAttrList **attrs,
							int            *cursor_pos);
static gboolean gtk_im_context_real_filter_keypress    (GtkIMContext   *context,
							GdkEvent       *event);

static gboolean gtk_im_context_real_get_surrounding_with_selection
                                                       (GtkIMContext   *context,
                                                        char          **text,
                                                        int            *cursor_index,
                                                        int            *selection_bound);
static void     gtk_im_context_real_set_surrounding_with_selection
                                                       (GtkIMContext   *context,
                                                        const char     *text,
                                                        int             len,
                                                        int             cursor_index,
                                                        int             selection_bound);

static void     gtk_im_context_get_property            (GObject        *obj,
                                                        guint           property_id,
                                                        GValue         *value,
                                                        GParamSpec     *pspec);
static void     gtk_im_context_set_property            (GObject        *obj,
                                                        guint           property_id,
                                                        const GValue   *value,
                                                        GParamSpec     *pspec);


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkIMContext, gtk_im_context, G_TYPE_OBJECT)

/**
 * GtkIMContextClass:
 * @preedit_start: Default handler of the [signal@Gtk.IMContext::preedit-start] signal.
 * @preedit_end: Default handler of the [signal@Gtk.IMContext::preedit-end] signal.
 * @preedit_changed: Default handler of the [signal@Gtk.IMContext::preedit-changed]
 *   signal.
 * @commit: Default handler of the [signal@Gtk.IMContext::commit] signal.
 * @retrieve_surrounding: Default handler of the
 *   [signal@Gtk.IMContext::retrieve-surrounding] signal.
 * @delete_surrounding: Default handler of the
 *   [signal@Gtk.IMContext::delete-surrounding] signal.
 * @set_client_widget: Called via [method@Gtk.IMContext.set_client_widget] when
 *   the input window where the entered text will appear changes. Override this
 *   to keep track of the current input window, for instance for the purpose of
 *   positioning a status display of your input method.
 * @get_preedit_string: Called via [method@Gtk.IMContext.get_preedit_string]
 *   to retrieve the text currently being preedited for display at the cursor
 *   position. Any input method which composes complex characters or any
 *   other compositions from multiple sequential key presses should override
 *   this method to provide feedback.
 * @filter_keypress: Called via [method@Gtk.IMContext.filter_keypress] on every
 *   key press or release event. Every non-trivial input method needs to
 *   override this in order to implement the mapping from key events to text.
 *   A return value of %TRUE indicates to the caller that the event was
 *   consumed by the input method. In that case, the [signal@Gtk.IMContext::commit]
 *   signal should be emitted upon completion of a key sequence to pass the
 *   resulting text back to the input widget. Alternatively, %FALSE may be
 *   returned to indicate that the event wasn’t handled by the input method.
 *   If a builtin mapping exists for the key, it is used to produce a
 *   character.
 * @focus_in: Called via [method@Gtk.IMContext.focus_in] when the input widget
 *   has gained focus. May be overridden to keep track of the current focus.
 * @focus_out: Called via [method@Gtk.IMContext.focus_out] when the input widget
 *   has lost focus. May be overridden to keep track of the current focus.
 * @reset: Called via [method@Gtk.IMContext.reset] to signal a change such as a
 *   change in cursor position. An input method that implements preediting
 *   should override this method to clear the preedit state on reset.
 * @set_cursor_location: Called via [method@Gtk.IMContext.set_cursor_location]
 *   to inform the input method of the current cursor location relative to
 *   the client window. May be overridden to implement the display of popup
 *   windows at the cursor position.
 * @set_use_preedit: Called via [method@Gtk.IMContext.set_use_preedit] to control
 *   the use of the preedit string. Override this to display feedback by some
 *   other means if turned off.
 * @set_surrounding: Called via [method@Gtk.IMContext.set_surrounding] in
 *   response to [signal@Gtk.IMContext::retrieve-surrounding] signal to update
 *   the input method’s idea of the context around the cursor. It is not necessary
 *   to override this method even with input methods which implement
 *   context-dependent behavior. The base implementation is sufficient for
 *   [method@Gtk.IMContext.get_surrounding] to work.
 * @get_surrounding: Called via [method@Gtk.IMContext.get_surrounding] to update
 *   the context around the cursor location. It is not necessary to override this
 *   method even with input methods which implement context-dependent behavior.
 *   The base implementation emits [signal@Gtk.IMContext::retrieve-surrounding]
 *   and records the context received by the subsequent invocation of
 *   [vfunc@Gtk.IMContext.get_surrounding].
 * @set_surrounding_with_selection: Called via
 *   [method@Gtk.IMContext.set_surrounding_with_selection] in response to the
 *   [signal@Gtk.IMContext::retrieve-surrounding] signal to update the input
 *   method’s idea of the context around the cursor. It is not necessary to
 *   override this method even with input methods which implement
 *   context-dependent behavior. The base implementation is sufficient for
 *   [method@Gtk.IMContext.get_surrounding] to work.
 * @get_surrounding_with_selection: Called via
 *   [method@Gtk.IMContext.get_surrounding_with_selection] to update the
 *   context around the cursor location. It is not necessary to override
 *   this method even with input methods which implement context-dependent
 *   behavior. The base implementation emits
 *   [signal@Gtk.IMContext::retrieve-surrounding] and records the context
 *   received by the subsequent invocation of [vfunc@Gtk.IMContext.get_surrounding].
 */
static void
gtk_im_context_class_init (GtkIMContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_im_context_get_property;
  object_class->set_property = gtk_im_context_set_property;

  klass->get_preedit_string = gtk_im_context_real_get_preedit_string;
  klass->filter_keypress = gtk_im_context_real_filter_keypress;
  klass->get_surrounding_with_selection = gtk_im_context_real_get_surrounding_with_selection;
  klass->set_surrounding_with_selection = gtk_im_context_real_set_surrounding_with_selection;

  /**
   * GtkIMContext::preedit-start:
   * @context: the object on which the signal is emitted
   *
   * The ::preedit-start signal is emitted when a new preediting sequence
   * starts.
   */
  im_context_signals[PREEDIT_START] =
    g_signal_new (I_("preedit-start"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkIMContextClass, preedit_start),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkIMContext::preedit-end:
   * @context: the object on which the signal is emitted
   *
   * The ::preedit-end signal is emitted when a preediting sequence
   * has been completed or canceled.
   */
  im_context_signals[PREEDIT_END] =
    g_signal_new (I_("preedit-end"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkIMContextClass, preedit_end),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkIMContext::preedit-changed:
   * @context: the object on which the signal is emitted
   *
   * The ::preedit-changed signal is emitted whenever the preedit sequence
   * currently being entered has changed.
   *
   * It is also emitted at the end of a preedit sequence, in which case
   * [method@Gtk.IMContext.get_preedit_string] returns the empty string.
   */
  im_context_signals[PREEDIT_CHANGED] =
    g_signal_new (I_("preedit-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkIMContextClass, preedit_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkIMContext::commit:
   * @context: the object on which the signal is emitted
   * @str: the completed character(s) entered by the user
   *
   * The ::commit signal is emitted when a complete input sequence
   * has been entered by the user.
   *
   * If the commit comes after a preediting sequence, the
   * ::commit signal is emitted after ::preedit-end.
   *
   * This can be a single character immediately after a key press or
   * the final result of preediting.
   */
  im_context_signals[COMMIT] =
    g_signal_new (I_("commit"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkIMContextClass, commit),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  /**
   * GtkIMContext::retrieve-surrounding:
   * @context: the object on which the signal is emitted
   *
   * The ::retrieve-surrounding signal is emitted when the input method
   * requires the context surrounding the cursor.
   *
   * The callback should set the input method surrounding context by
   * calling the [method@Gtk.IMContext.set_surrounding] method.
   *
   * Returns: %TRUE if the signal was handled.
   */
  im_context_signals[RETRIEVE_SURROUNDING] =
    g_signal_new (I_("retrieve-surrounding"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkIMContextClass, retrieve_surrounding),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (im_context_signals[RETRIEVE_SURROUNDING],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /**
   * GtkIMContext::delete-surrounding:
   * @context: the object on which the signal is emitted
   * @offset: the character offset from the cursor position of the text
   *   to be deleted. A negative value indicates a position before
   *   the cursor.
   * @n_chars: the number of characters to be deleted
   *
   * The ::delete-surrounding signal is emitted when the input method
   * needs to delete all or part of the context surrounding the cursor.
   *
   * Returns: %TRUE if the signal was handled.
   */
  im_context_signals[DELETE_SURROUNDING] =
    g_signal_new (I_("delete-surrounding"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkIMContextClass, delete_surrounding),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__INT_INT,
                  G_TYPE_BOOLEAN, 2,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (im_context_signals[DELETE_SURROUNDING],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__INT_INTv);

  /**
   * GtkIMContext:input-purpose:
   *
   * The purpose of the text field that the `GtkIMContext is connected to.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   */
  properties[PROP_INPUT_PURPOSE] =
    g_param_spec_enum ("input-purpose", NULL, NULL,
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkIMContext:input-hints:
   *
   * Additional hints that allow input methods to fine-tune
   * their behaviour.
   */
  properties[PROP_INPUT_HINTS] =
    g_param_spec_flags ("input-hints", NULL, NULL,
                         GTK_TYPE_INPUT_HINTS,
                         GTK_INPUT_HINT_NONE,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}

static void
gtk_im_context_init (GtkIMContext *im_context)
{
}

static void
gtk_im_context_real_get_preedit_string (GtkIMContext       *context,
					char              **str,
					PangoAttrList     **attrs,
					int                *cursor_pos)
{
  if (str)
    *str = g_strdup ("");
  if (attrs)
    *attrs = pango_attr_list_new ();
  if (cursor_pos)
    *cursor_pos = 0;
}

static gboolean
gtk_im_context_real_filter_keypress (GtkIMContext *context,
				     GdkEvent     *event)
{
  return FALSE;
}

typedef struct
{
  char *text;
  int cursor_index;
  int selection_bound;
} SurroundingInfo;

static void
gtk_im_context_real_set_surrounding_with_selection (GtkIMContext  *context,
                                                    const char    *text,
                                                    int            len,
                                                    int            cursor_index,
                                                    int            selection_bound)
{
  SurroundingInfo *info = g_object_get_data (G_OBJECT (context),
                                             "gtk-im-surrounding-info");

  if (info)
    {
      g_free (info->text);
      info->text = g_strndup (text, len);
      info->cursor_index = cursor_index;
      info->selection_bound = selection_bound;
    }
}

static gboolean
gtk_im_context_real_get_surrounding_with_selection (GtkIMContext *context,
                                                    char        **text,
                                                    int          *cursor_index,
                                                    int          *selection_bound)
{
  gboolean result;
  gboolean info_is_local = FALSE;
  SurroundingInfo local_info = { NULL, 0 };
  SurroundingInfo *info;
  
  info = g_object_get_data (G_OBJECT (context), "gtk-im-surrounding-info");
  if (!info)
    {
      info = &local_info;
      g_object_set_data (G_OBJECT (context), I_("gtk-im-surrounding-info"), info);
      info_is_local = TRUE;
    }
  
  g_signal_emit (context,
		 im_context_signals[RETRIEVE_SURROUNDING], 0,
		 &result);

  if (result)
    {
      *text = g_strdup (info->text ? info->text : "");
      *cursor_index = info->cursor_index;
      *selection_bound = info->selection_bound;
    }
  else
    {
      *text = NULL;
      *cursor_index = 0;
      *selection_bound = 0;
    }

  if (info_is_local)
    {
      g_free (info->text);
      g_object_set_data (G_OBJECT (context), I_("gtk-im-surrounding-info"), NULL);
    }
  
  return result;
}

/**
 * gtk_im_context_set_client_widget:
 * @context: a `GtkIMContext`
 * @widget: (nullable): the client widget. This may be %NULL to indicate
 *   that the previous client widget no longer exists.
 *
 * Set the client widget for the input context.
 *
 * This is the `GtkWidget` holding the input focus. This widget is
 * used in order to correctly position status windows, and may
 * also be used for purposes internal to the input method.
 */
void
gtk_im_context_set_client_widget (GtkIMContext *context,
                                  GtkWidget    *widget)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_client_widget)
    klass->set_client_widget (context, widget);
}

/**
 * gtk_im_context_get_preedit_string:
 * @context: a `GtkIMContext`
 * @str: (out) (transfer full): location to store the retrieved
 *   string. The string retrieved must be freed with g_free().
 * @attrs: (out) (transfer full): location to store the retrieved
 *   attribute list. When you are done with this list, you
 *   must unreference it with [method@Pango.AttrList.unref].
 * @cursor_pos: (out): location to store position of cursor
 *   (in characters) within the preedit string.
 *
 * Retrieve the current preedit string for the input context,
 * and a list of attributes to apply to the string.
 *
 * This string should be displayed inserted at the insertion point.
 */
void
gtk_im_context_get_preedit_string (GtkIMContext   *context,
				   char          **str,
				   PangoAttrList **attrs,
				   int            *cursor_pos)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  klass->get_preedit_string (context, str, attrs, cursor_pos);
  g_return_if_fail (str == NULL || g_utf8_validate (*str, -1, NULL));
}

/**
 * gtk_im_context_filter_keypress:
 * @context: a `GtkIMContext`
 * @event: the key event
 *
 * Allow an input method to internally handle key press and release
 * events.
 *
 * If this function returns %TRUE, then no further processing
 * should be done for this key event.
 *
 * Returns: %TRUE if the input method handled the key event.
 */
gboolean
gtk_im_context_filter_keypress (GtkIMContext *context,
				GdkEvent     *key)
{
  GtkIMContextClass *klass;
  
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  return klass->filter_keypress (context, key);
}

/**
 * gtk_im_context_filter_key:
 * @context: a `GtkIMContext`
 * @press: whether to forward a key press or release event
 * @surface: the surface the event is for
 * @device: the device that the event is for
 * @time: the timestamp for the event
 * @keycode: the keycode for the event
 * @state: modifier state for the event
 * @group: the active keyboard group for the event
 *
 * Allow an input method to forward key press and release events
 * to another input method without necessarily having a `GdkEvent`
 * available.
 *
 * Returns: %TRUE if the input method handled the key event.
 */
gboolean
gtk_im_context_filter_key (GtkIMContext    *context,
                           gboolean         press,
                           GdkSurface      *surface,
                           GdkDevice       *device,
                           guint32          time,
                           guint            keycode,
                           GdkModifierType  state,
                           int              group)
{
  GdkTranslatedKey translated, no_lock;
  GdkEvent *key;
  gboolean ret;
  guint keyval;
  int layout;
  int level;
  GdkModifierType consumed;

  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);

  if (!gdk_display_translate_key (gdk_surface_get_display (surface),
                                  keycode,
                                  state,
                                  group,
                                  &keyval,
                                  &layout,
                                  &level,
                                  &consumed))
    return FALSE;

  translated.keyval = keyval;
  translated.layout = layout;
  translated.level = level;
  translated.consumed = consumed;

  if (!gdk_display_translate_key (gdk_surface_get_display (surface),
                                  keycode,
                                  state & ~GDK_LOCK_MASK,
                                  group,
                                  &keyval,
                                  &layout,
                                  &level,
                                  &consumed))
    return FALSE;

  no_lock.keyval = keyval;
  no_lock.layout = layout;
  no_lock.level = level;
  no_lock.consumed = consumed;

  key = gdk_key_event_new (press ? GDK_KEY_PRESS : GDK_KEY_RELEASE,
                           surface,
                           device,
                           time,
                           keycode,
                           state,
                           FALSE, /* FIXME */
                           &translated,
                           &no_lock,
                           NULL);

  ret = GTK_IM_CONTEXT_GET_CLASS (context)->filter_keypress (context, key);

  gdk_event_unref (key);

  return ret;
}

/**
 * gtk_im_context_focus_in:
 * @context: a `GtkIMContext`
 *
 * Notify the input method that the widget to which this
 * input context corresponds has gained focus.
 *
 * The input method may, for example, change the displayed
 * feedback to reflect this change.
 */
void
gtk_im_context_focus_in (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->focus_in)
    klass->focus_in (context);
}

/**
 * gtk_im_context_focus_out:
 * @context: a `GtkIMContext`
 *
 * Notify the input method that the widget to which this
 * input context corresponds has lost focus.
 *
 * The input method may, for example, change the displayed
 * feedback or reset the contexts state to reflect this change.
 */
void
gtk_im_context_focus_out (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->focus_out)
    klass->focus_out (context);
}

/**
 * gtk_im_context_reset:
 * @context: a `GtkIMContext`
 *
 * Notify the input method that a change such as a change in cursor
 * position has been made.
 *
 * This will typically cause the input method to clear the preedit state.
 */
void
gtk_im_context_reset (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->reset)
    klass->reset (context);
}


/**
 * gtk_im_context_set_cursor_location:
 * @context: a `GtkIMContext`
 * @area: new location
 *
 * Notify the input method that a change in cursor
 * position has been made.
 *
 * The location is relative to the client widget.
 */
void
gtk_im_context_set_cursor_location (GtkIMContext       *context,
				    const GdkRectangle *area)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_cursor_location)
    klass->set_cursor_location (context, (GdkRectangle *) area);
}

/**
 * gtk_im_context_set_use_preedit:
 * @context: a `GtkIMContext`
 * @use_preedit: whether the IM context should use the preedit string.
 *
 * Sets whether the IM context should use the preedit string
 * to display feedback.
 *
 * If @use_preedit is %FALSE (default is %TRUE), then the IM context
 * may use some other method to display feedback, such as displaying
 * it in a child of the root window.
 */
void
gtk_im_context_set_use_preedit (GtkIMContext *context,
				gboolean      use_preedit)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_use_preedit)
    klass->set_use_preedit (context, use_preedit);
}

/**
 * gtk_im_context_set_surrounding:
 * @context: a `GtkIMContext`
 * @text: text surrounding the insertion point, as UTF-8.
 *   the preedit string should not be included within @text
 * @len: the length of @text, or -1 if @text is nul-terminated
 * @cursor_index: the byte index of the insertion cursor within @text.
 *
 * Sets surrounding context around the insertion point and preedit
 * string.
 *
 * This function is expected to be called in response to the
 * [signal@Gtk.IMContext::retrieve-surrounding] signal, and will
 * likely have no effect if called at other times.
 *
 * Deprecated: 4.2: Use [method@Gtk.IMContext.set_surrounding_with_selection] instead
 */
void
gtk_im_context_set_surrounding (GtkIMContext  *context,
                                const char    *text,
                                int            len,
                                int            cursor_index)
{
  gtk_im_context_set_surrounding_with_selection (context, text, len, cursor_index, cursor_index);
}

/**
 * gtk_im_context_set_surrounding_with_selection:
 * @context: a `GtkIMContext`
 * @text: text surrounding the insertion point, as UTF-8.
 *   the preedit string should not be included within @text
 * @len: the length of @text, or -1 if @text is nul-terminated
 * @cursor_index: the byte index of the insertion cursor within @text
 * @anchor_index: the byte index of the selection bound within @text
 *
 * Sets surrounding context around the insertion point and preedit
 * string. This function is expected to be called in response to the
 * [signal@Gtk.IMContext::retrieve_surrounding] signal, and will likely
 * have no effect if called at other times.
 *
 * Since: 4.2
 */
void
gtk_im_context_set_surrounding_with_selection (GtkIMContext  *context,
                                               const char    *text,
                                               int            len,
                                               int            cursor_index,
                                               int            anchor_index)
{
  GtkIMContextClass *klass;

  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  g_return_if_fail (text != NULL || len == 0);

  if (text == NULL && len == 0)
    text = "";
  if (len < 0)
    len = strlen (text);

  g_return_if_fail (cursor_index >= 0 && cursor_index <= len);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_surrounding_with_selection)
    klass->set_surrounding_with_selection (context, text, len, cursor_index, anchor_index);
  else if (klass->set_surrounding)
    klass->set_surrounding (context, text, len, cursor_index);
}

/**
 * gtk_im_context_get_surrounding:
 * @context: a `GtkIMContext`
 * @text: (out) (transfer full): location to store a UTF-8 encoded
 *   string of text holding context around the insertion point.
 *   If the function returns %TRUE, then you must free the result
 *   stored in this location with g_free().
 * @cursor_index: (out): location to store byte index of the insertion
 *   cursor within @text.
 *
 * Retrieves context around the insertion point.
 *
 * Input methods typically want context in order to constrain input text
 * based on existing text; this is important for languages such as Thai
 * where only some sequences of characters are allowed.
 *
 * This function is implemented by emitting the
 * [signal@Gtk.IMContext::retrieve-surrounding] signal on the input method;
 * in response to this signal, a widget should provide as much context as
 * is available, up to an entire paragraph, by calling
 * [method@Gtk.IMContext.set_surrounding].
 *
 * Note that there is no obligation for a widget to respond to the
 * `::retrieve-surrounding` signal, so input methods must be prepared to
 * function without context.
 *
 * Returns: `TRUE` if surrounding text was provided; in this case
 *    you must free the result stored in `text`.
 *
 * Deprecated: 4.2: Use [method@Gtk.IMContext.get_surrounding_with_selection] instead.
 */
gboolean
gtk_im_context_get_surrounding (GtkIMContext  *context,
                                char         **text,
                                int           *cursor_index)
{
  return gtk_im_context_get_surrounding_with_selection (context,
                                                        text,
                                                        cursor_index,
                                                        NULL);
}

/**
 * gtk_im_context_get_surrounding_with_selection:
 * @context: a `GtkIMContext`
 * @text: (out) (transfer full): location to store a UTF-8 encoded
 *   string of text holding context around the insertion point.
 *   If the function returns %TRUE, then you must free the result
 *   stored in this location with g_free().
 * @cursor_index: (out): location to store byte index of the insertion
 *   cursor within @text.
 * @anchor_index: (out): location to store byte index of the selection
 *   bound within @text
 *
 * Retrieves context around the insertion point.
 *
 * Input methods typically want context in order to constrain input
 * text based on existing text; this is important for languages such
 * as Thai where only some sequences of characters are allowed.
 *
 * This function is implemented by emitting the
 * [signal@Gtk.IMContext::retrieve-surrounding] signal on the input method;
 * in response to this signal, a widget should provide as much context as
 * is available, up to an entire paragraph, by calling
 * [method@Gtk.IMContext.set_surrounding_with_selection].
 *
 * Note that there is no obligation for a widget to respond to the
 * `::retrieve-surrounding` signal, so input methods must be prepared to
 * function without context.
 *
 * Returns: `TRUE` if surrounding text was provided; in this case
 *   you must free the result stored in `text`.
 *
 * Since: 4.2
 */
gboolean
gtk_im_context_get_surrounding_with_selection (GtkIMContext  *context,
                                               char         **text,
                                               int           *cursor_index,
                                               int           *anchor_index)
{
  GtkIMContextClass *klass;
  char *local_text = NULL;
  int local_index;
  gboolean result = FALSE;

  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->get_surrounding_with_selection)
    result = klass->get_surrounding_with_selection
                                    (context,
                                     text ? text : &local_text,
                                     cursor_index ? cursor_index : &local_index,
                                     anchor_index ? anchor_index : &local_index);
  else if (klass->get_surrounding)
    {
      result = klass->get_surrounding (context,
                                       text ? text : &local_text,
                                       &local_index);
      if (cursor_index)
        *cursor_index = local_index;
      if (anchor_index)
        *anchor_index = local_index;
    }

  if (result)
    g_free (local_text);

  return result;
}

/**
 * gtk_im_context_delete_surrounding:
 * @context: a `GtkIMContext`
 * @offset: offset from cursor position in chars;
 *    a negative value means start before the cursor.
 * @n_chars: number of characters to delete.
 *
 * Asks the widget that the input context is attached to delete
 * characters around the cursor position by emitting the
 * `::delete_surrounding` signal.
 *
 * Note that @offset and @n_chars are in characters not in bytes
 * which differs from the usage other places in `GtkIMContext`.
 *
 * In order to use this function, you should first call
 * [method@Gtk.IMContext.get_surrounding] to get the current context,
 * and call this function immediately afterwards to make sure that you
 * know what you are deleting. You should also account for the fact
 * that even if the signal was handled, the input context might not
 * have deleted all the characters that were requested to be deleted.
 *
 * This function is used by an input method that wants to make
 * substitutions in the existing text in response to new input.
 * It is not useful for applications.
 *
 * Returns: %TRUE if the signal was handled.
 */
gboolean
gtk_im_context_delete_surrounding (GtkIMContext *context,
				   int           offset,
				   int           n_chars)
{
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);

  g_signal_emit (context,
		 im_context_signals[DELETE_SURROUNDING], 0,
		 offset, n_chars, &result);

  return result;
}

static void
gtk_im_context_get_property (GObject    *obj,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkIMContextPrivate *priv = gtk_im_context_get_instance_private (GTK_IM_CONTEXT (obj));

  switch (property_id)
    {
    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, priv->purpose);
      break;
    case PROP_INPUT_HINTS:
      g_value_set_flags (value, priv->hints);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_im_context_set_property (GObject      *obj,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkIMContextPrivate *priv = gtk_im_context_get_instance_private (GTK_IM_CONTEXT (obj));

  switch (property_id)
    {
    case PROP_INPUT_PURPOSE:
      if (priv->purpose != g_value_get_enum (value))
        {
          priv->purpose = g_value_get_enum (value);
          g_object_notify_by_pspec (obj, pspec);
        }
      break;
    case PROP_INPUT_HINTS:
      if (priv->hints != g_value_get_flags (value))
        {
          priv->hints = g_value_get_flags (value);
          g_object_notify_by_pspec (obj, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

/**
 * gtk_im_context_activate_osk:
 * @context: a `GtkIMContext`
 * @event: (nullable): a [class@Gdk.Event]
 *
 * Requests the platform to show an on-screen keyboard for user input.
 *
 * This method will return %TRUE if this request was actually performed
 * to the platform, other environmental factors may result in an on-screen
 * keyboard effectively not showing up.
 *
 * Returns: %TRUE if an on-screen keyboard could be requested to the platform.
 *
 * Since: 4.14
 **/
gboolean
gtk_im_context_activate_osk (GtkIMContext *context,
			     GdkEvent     *event)
{
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);
  g_return_val_if_fail (!event || GDK_IS_EVENT (event), FALSE);

  if (!GTK_IM_CONTEXT_GET_CLASS (context)->activate_osk_with_event)
    return FALSE;

  return GTK_IM_CONTEXT_GET_CLASS (context)->activate_osk_with_event (context, event);
}
