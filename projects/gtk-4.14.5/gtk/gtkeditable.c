/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * GtkEditable:
 *
 * `GtkEditable` is an interface for text editing widgets.
 *
 * Typical examples of editable widgets are [class@Gtk.Entry] and
 * [class@Gtk.SpinButton]. It contains functions for generically manipulating
 * an editable widget, a large number of action signals used for key bindings,
 * and several signals that an application can connect to modify the behavior
 * of a widget.
 *
 * As an example of the latter usage, by connecting the following handler to
 * [signal@Gtk.Editable::insert-text], an application can convert all entry
 * into a widget into uppercase.
 *
 * ## Forcing entry to uppercase.
 *
 * ```c
 * #include <ctype.h>
 *
 * void
 * insert_text_handler (GtkEditable *editable,
 *                      const char  *text,
 *                      int          length,
 *                      int         *position,
 *                      gpointer     data)
 * {
 *   char *result = g_utf8_strup (text, length);
 *
 *   g_signal_handlers_block_by_func (editable,
 *                                (gpointer) insert_text_handler, data);
 *   gtk_editable_insert_text (editable, result, length, position);
 *   g_signal_handlers_unblock_by_func (editable,
 *                                      (gpointer) insert_text_handler, data);
 *
 *   g_signal_stop_emission_by_name (editable, "insert_text");
 *
 *   g_free (result);
 * }
 * ```
 *
 * ## Implementing GtkEditable
 *
 * The most likely scenario for implementing `GtkEditable` on your own widget
 * is that you will embed a `GtkText` inside a complex widget, and want to
 * delegate the editable functionality to that text widget. `GtkEditable`
 * provides some utility functions to make this easy.
 *
 * In your class_init function, call [func@Gtk.Editable.install_properties],
 * passing the first available property ID:
 *
 * ```c
 * static void
 * my_class_init (MyClass *class)
 * {
 *   ...
 *   g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
 *   gtk_editable_install_properties (object_clas, NUM_PROPERTIES);
 *   ...
 * }
 * ```
 *
 * In your interface_init function for the `GtkEditable` interface, provide
 * an implementation for the get_delegate vfunc that returns your text widget:
 *
 * ```c
 * GtkEditable *
 * get_editable_delegate (GtkEditable *editable)
 * {
 *   return GTK_EDITABLE (MY_WIDGET (editable)->text_widget);
 * }
 *
 * static void
 * my_editable_init (GtkEditableInterface *iface)
 * {
 *   iface->get_delegate = get_editable_delegate;
 * }
 * ```
 *
 * You don't need to provide any other vfuncs. The default implementations
 * work by forwarding to the delegate that the GtkEditableInterface.get_delegate()
 * vfunc returns.
 *
 * In your instance_init function, create your text widget, and then call
 * [method@Gtk.Editable.init_delegate]:
 *
 * ```c
 * static void
 * my_widget_init (MyWidget *self)
 * {
 *   ...
 *   self->text_widget = gtk_text_new ();
 *   gtk_editable_init_delegate (GTK_EDITABLE (self));
 *   ...
 * }
 * ```
 *
 * In your dispose function, call [method@Gtk.Editable.finish_delegate] before
 * destroying your text widget:
 *
 * ```c
 * static void
 * my_widget_dispose (GObject *object)
 * {
 *   ...
 *   gtk_editable_finish_delegate (GTK_EDITABLE (self));
 *   g_clear_pointer (&self->text_widget, gtk_widget_unparent);
 *   ...
 * }
 * ```
 *
 * Finally, use [func@Gtk.Editable.delegate_set_property] in your `set_property`
 * function (and similar for `get_property`), to set the editable properties:
 *
 * ```c
 *   ...
 *   if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
 *     return;
 *
 *   switch (prop_id)
 *   ...
 * ```
 *
 * It is important to note that if you create a `GtkEditable` that uses
 * a delegate, the low level [signal@Gtk.Editable::insert-text] and
 * [signal@Gtk.Editable::delete-text] signals will be propagated from the
 * "wrapper" editable to the delegate, but they will not be propagated from
 * the delegate to the "wrapper" editable, as they would cause an infinite
 * recursion. If you wish to connect to the [signal@Gtk.Editable::insert-text]
 * and [signal@Gtk.Editable::delete-text] signals, you will need to connect
 * to them on the delegate obtained via [method@Gtk.Editable.get_delegate].
 */

#include "config.h"
#include <string.h>

#include "gtkeditable.h"
#include "gtkentrybuffer.h"
#include "gtkmarshalers.h"
#include "gdk/gdkmarshalers.h"
#include "gtkprivate.h"

G_DEFINE_INTERFACE (GtkEditable, gtk_editable, GTK_TYPE_WIDGET)

enum {
  CHANGED,
  DELETE_TEXT,
  INSERT_TEXT,
  N_SIGNALS
};

static GQuark quark_editable_data;
static guint signals[N_SIGNALS];

static GtkEditable *
get_delegate (GtkEditable *editable)
{
  GtkEditableInterface *iface = GTK_EDITABLE_GET_IFACE (editable);

  if (iface->get_delegate)
    return iface->get_delegate (editable);

  return NULL;
}

static void
gtk_editable_default_do_insert_text (GtkEditable *editable,
                                     const char  *text,
                                     int          length,
                                     int         *position)
{
  g_signal_emit (editable, signals[INSERT_TEXT], 0, text, length, position);
}

#define warn_no_delegate(func) \
  g_critical ("GtkEditable %s: default implementation called without a delegate", func);

static void
gtk_editable_default_insert_text (GtkEditable *editable,
                                  const char  *text,
                                  int          length,
                                  int         *position)
{
  GtkEditable *delegate = get_delegate (editable);

  if (delegate)
    gtk_editable_insert_text (delegate, text, length, position);
  else
    warn_no_delegate ("insert_text");
}

static void
gtk_editable_default_do_delete_text (GtkEditable *editable,
                                     int          start_pos,
                                     int          end_pos)
{
  g_signal_emit (editable, signals[DELETE_TEXT], 0, start_pos, end_pos);
}

static void
gtk_editable_default_delete_text (GtkEditable *editable,
                                  int          start_pos,
                                  int          end_pos)
{
  GtkEditable *delegate = get_delegate (editable);

  if (delegate)
    gtk_editable_delete_text (delegate, start_pos, end_pos);
  else
    warn_no_delegate ("delete_text");
}

static const char *
gtk_editable_default_get_text (GtkEditable *editable)
{
  GtkEditable *delegate = get_delegate (editable);

  if (delegate)
    return gtk_editable_get_text (delegate);
  else
    warn_no_delegate ("get_text");

  return NULL;
}

static void
gtk_editable_default_set_selection_bounds (GtkEditable *editable,
                                           int          start_pos,
                                           int          end_pos)
{
  GtkEditable *delegate = get_delegate (editable);

  if (delegate)
    gtk_editable_select_region (delegate, start_pos, end_pos);
  else
    warn_no_delegate ("select_region");
}

static gboolean
gtk_editable_default_get_selection_bounds (GtkEditable *editable,
                                           int         *start_pos,
                                           int         *end_pos)
{
  GtkEditable *delegate = get_delegate (editable);

  if (delegate)
    return gtk_editable_get_selection_bounds (delegate, start_pos, end_pos);
  else
    warn_no_delegate ("select_region");

  return FALSE;
}

static void
gtk_editable_default_init (GtkEditableInterface *iface)
{
  quark_editable_data = g_quark_from_static_string ("GtkEditable-data");

  iface->insert_text = gtk_editable_default_insert_text;
  iface->delete_text = gtk_editable_default_delete_text;
  iface->get_text = gtk_editable_default_get_text;
  iface->do_insert_text = gtk_editable_default_do_insert_text;
  iface->do_delete_text = gtk_editable_default_do_delete_text;
  iface->get_selection_bounds = gtk_editable_default_get_selection_bounds;
  iface->set_selection_bounds = gtk_editable_default_set_selection_bounds;

  /**
   * GtkEditable::insert-text:
   * @editable: the object which received the signal
   * @text: the new text to insert
   * @length: the length of the new text, in bytes,
   *     or -1 if new_text is nul-terminated
   * @position: (inout) (type int): the position, in characters,
   *     at which to insert the new text. this is an in-out
   *     parameter.  After the signal emission is finished, it
   *     should point after the newly inserted text.
   *
   * Emitted when text is inserted into the widget by the user.
   *
   * The default handler for this signal will normally be responsible
   * for inserting the text, so by connecting to this signal and then
   * stopping the signal with g_signal_stop_emission(), it is possible
   * to modify the inserted text, or prevent it from being inserted entirely.
   */
  signals[INSERT_TEXT] =
    g_signal_new (I_("insert-text"),
                  GTK_TYPE_EDITABLE,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEditableInterface, insert_text),
                  NULL, NULL,
                  _gtk_marshal_VOID__STRING_INT_POINTER,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_INT,
                  G_TYPE_POINTER);
  g_signal_set_va_marshaller (signals[INSERT_TEXT],
                              G_TYPE_FROM_INTERFACE (iface),
                              _gtk_marshal_VOID__STRING_INT_POINTERv);

  /**
   * GtkEditable::delete-text:
   * @editable: the object which received the signal
   * @start_pos: the starting position
   * @end_pos: the end position
   *
   * Emitted when text is deleted from the widget by the user.
   *
   * The default handler for this signal will normally be responsible for
   * deleting the text, so by connecting to this signal and then stopping
   * the signal with g_signal_stop_emission(), it is possible to modify the
   * range of deleted text, or prevent it from being deleted entirely.
   *
   * The @start_pos and @end_pos parameters are interpreted as for
   * [method@Gtk.Editable.delete_text].
   */
  signals[DELETE_TEXT] =
    g_signal_new (I_("delete-text"),
                  GTK_TYPE_EDITABLE,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEditableInterface, delete_text),
                  NULL, NULL,
                  _gdk_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[DELETE_TEXT],
                              G_TYPE_FROM_INTERFACE (iface),
                              _gdk_marshal_VOID__INT_INTv);

  /**
   * GtkEditable::changed:
   * @editable: the object which received the signal
   *
   * Emitted at the end of a single user-visible operation on the
   * contents.
   *
   * E.g., a paste operation that replaces the contents of the
   * selection will cause only one signal emission (even though it
   * is implemented by first deleting the selection, then inserting
   * the new content, and may cause multiple ::notify::text signals
   * to be emitted).
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  GTK_TYPE_EDITABLE,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEditableInterface, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEditable:text: (attributes org.gtk.Property.get=gtk_editable_get_text org.gtk.Property.set=gtk_editable_set_text)
   *
   * The contents of the entry.
   */
  g_object_interface_install_property (iface,
      g_param_spec_string ("text", NULL, NULL,
                           "",
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkEditable:cursor-position: (attributes org.gtk.Property.get=gtk_editable_get_position org.gtk.Property.set=gtk_editable_set_position)
   *
   * The current position of the insertion cursor in chars.
   */
  g_object_interface_install_property (iface,
      g_param_spec_int ("cursor-position", NULL, NULL,
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE));

  /**
   * GtkEditable:enable-undo: (attributes org.gtk.Property.get=gtk_editable_get_enable_undo org.gtk.Property.setg=gtk_editable_set_enable_undo)
   *
   * If undo/redo should be enabled for the editable.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("enable-undo", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkEditable:selection-bound:
   *
   * The position of the opposite end of the selection from the cursor in chars.
   */
  g_object_interface_install_property (iface,
      g_param_spec_int ("selection-bound", NULL, NULL,
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE));

  /**
   * GtkEditable:editable: (attributes org.gtk.Property.get=gtk_editable_get_editable org.gtk.Property.set=gtk_editable_set_editable)
   *
   * Whether the entry contents can be edited.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("editable", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkEditable:width-chars: (attributes org.gtk.Property.get=gtk_editable_get_width_chars org.gtk.Property.set=gtk_editable_set_width_chars)
   *
   * Number of characters to leave space for in the entry.
   */
  g_object_interface_install_property (iface,
      g_param_spec_int ("width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkEditable:max-width-chars: (attributes org.gtk.Property.get=gtk_editable_get_max_width_chars org.gtk.Property.set=gtk_editable_set_max_width_chars)
   *
   * The desired maximum width of the entry, in characters.
   */
  g_object_interface_install_property (iface,
      g_param_spec_int ("max-width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkEditable:xalign: (attributes org.gtk.Property.get=gtk_editable_get_alignment org.gtk.Property.set=gtk_editable_set_alignment)
   *
   * The horizontal alignment, from 0 (left) to 1 (right).
   *
   * Reversed for RTL layouts.
   */
  g_object_interface_install_property (iface,
      g_param_spec_float ("xalign", NULL, NULL,
                          0.0, 1.0,
                          0.0,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

/**
 * gtk_editable_insert_text: (virtual do_insert_text)
 * @editable: a `GtkEditable`
 * @text: the text to insert
 * @length: the length of the text in bytes, or -1
 * @position: (inout): location of the position text will be inserted at
 *
 * Inserts @length bytes of @text into the contents of the
 * widget, at position @position.
 *
 * Note that the position is in characters, not in bytes.
 * The function updates @position to point after the newly
 * inserted text.
 */
void
gtk_editable_insert_text (GtkEditable *editable,
                          const char  *text,
                          int          length,
                          int         *position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (text != NULL);
  g_return_if_fail (length >= -1);
  g_return_if_fail (position != NULL);

  if (text == NULL)
    text = "";

  if (length < 0)
    length = strlen (text);

  GTK_EDITABLE_GET_IFACE (editable)->do_insert_text (editable, text, length, position);
}

/**
 * gtk_editable_delete_text: (virtual do_delete_text)
 * @editable: a `GtkEditable`
 * @start_pos: start position
 * @end_pos: end position
 *
 * Deletes a sequence of characters.
 *
 * The characters that are deleted are those characters at positions
 * from @start_pos up to, but not including @end_pos. If @end_pos is
 * negative, then the characters deleted are those from @start_pos to
 * the end of the text.
 *
 * Note that the positions are specified in characters, not bytes.
 */
void
gtk_editable_delete_text (GtkEditable *editable,
                          int          start_pos,
                          int          end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (start_pos >= 0);
  g_return_if_fail (end_pos == -1 || end_pos >= start_pos);

  GTK_EDITABLE_GET_IFACE (editable)->do_delete_text (editable, start_pos, end_pos);
}

/**
 * gtk_editable_get_chars:
 * @editable: a `GtkEditable`
 * @start_pos: start of text
 * @end_pos: end of text
 *
 * Retrieves a sequence of characters.
 *
 * The characters that are retrieved are those characters at positions
 * from @start_pos up to, but not including @end_pos. If @end_pos is negative,
 * then the characters retrieved are those characters from @start_pos to
 * the end of the text.
 *
 * Note that positions are specified in characters, not bytes.
 *
 * Returns: (transfer full): a pointer to the contents of the widget as a
 *   string. This string is allocated by the `GtkEditable` implementation
 *   and should be freed by the caller.
 */
char *
gtk_editable_get_chars (GtkEditable *editable,
                        int          start_pos,
                        int          end_pos)
{
  const char *text;
  int length;
  int start_index,end_index;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);
  g_return_val_if_fail (start_pos >= 0, NULL);
  g_return_val_if_fail (end_pos == -1 || end_pos >= start_pos, NULL);

  text = GTK_EDITABLE_GET_IFACE (editable)->get_text (editable);
  length = g_utf8_strlen (text, -1);

  if (end_pos < 0)
    end_pos = length;

  start_pos = MIN (length, start_pos);
  end_pos = MIN (length, end_pos);

  start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
  end_index = g_utf8_offset_to_pointer (text, end_pos) - text;

  return g_strndup (text + start_index, end_index - start_index);
}

/**
 * gtk_editable_get_text: (attributes org.gtk.Method.get_property=text)
 * @editable: a `GtkEditable`
 *
 * Retrieves the contents of @editable.
 *
 * The returned string is owned by GTK and must not be modified or freed.
 *
 * Returns: (transfer none): a pointer to the contents of the editable
 */
const char *
gtk_editable_get_text (GtkEditable *editable)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  return GTK_EDITABLE_GET_IFACE (editable)->get_text (editable);
}

/**
 * gtk_editable_set_text: (attributes org.gtk.Method.set_property=text)
 * @editable: a `GtkEditable`
 * @text: the text to set
 *
 * Sets the text in the editable to the given value.
 *
 * This is replacing the current contents.
 */
void
gtk_editable_set_text (GtkEditable *editable,
                       const char  *text)
{
  int pos;

  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (text != NULL);

  g_object_freeze_notify (G_OBJECT (editable));
  gtk_editable_delete_text (editable, 0, -1);
  pos = 0;
  gtk_editable_insert_text (editable, text, -1, &pos);
  g_object_thaw_notify (G_OBJECT (editable));
}

/**
 * gtk_editable_set_position: (attributes org.gtk.Method.set_property=cursor-position)
 * @editable: a `GtkEditable`
 * @position: the position of the cursor
 *
 * Sets the cursor position in the editable to the given value.
 *
 * The cursor is displayed before the character with the given (base 0)
 * index in the contents of the editable. The value must be less than
 * or equal to the number of characters in the editable. A value of -1
 * indicates that the position should be set after the last character
 * of the editable. Note that @position is in characters, not in bytes.
 */
void
gtk_editable_set_position (GtkEditable *editable,
                           int          position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_IFACE (editable)->set_selection_bounds (editable, position, position);
}

/**
 * gtk_editable_get_position: (attributes org.gtk.Method.get_property=cursor-position)
 * @editable: a `GtkEditable`
 *
 * Retrieves the current position of the cursor relative
 * to the start of the content of the editable.
 *
 * Note that this position is in characters, not in bytes.
 *
 * Returns: the cursor position
 */
int
gtk_editable_get_position (GtkEditable *editable)
{
  int start, end;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  GTK_EDITABLE_GET_IFACE (editable)->get_selection_bounds (editable, &start, &end);

  return end;
}

/**
 * gtk_editable_get_selection_bounds:
 * @editable: a `GtkEditable`
 * @start_pos: (out) (optional): location to store the starting position
 * @end_pos: (out) (optional): location to store the end position
 *
 * Retrieves the selection bound of the editable.
 *
 * @start_pos will be filled with the start of the selection and
 * @end_pos with end. If no text was selected both will be identical
 * and %FALSE will be returned.
 *
 * Note that positions are specified in characters, not bytes.
 *
 * Returns: %TRUE if there is a non-empty selection, %FALSE otherwise
 */
gboolean
gtk_editable_get_selection_bounds (GtkEditable *editable,
                                   int         *start_pos,
                                   int         *end_pos)
{
  int tmp_start, tmp_end;
  gboolean result;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  result = GTK_EDITABLE_GET_IFACE (editable)->get_selection_bounds (editable, &tmp_start, &tmp_end);

  if (start_pos)
    *start_pos = MIN (tmp_start, tmp_end);
  if (end_pos)
    *end_pos = MAX (tmp_start, tmp_end);

  return result;
}

/**
 * gtk_editable_delete_selection:
 * @editable: a `GtkEditable`
 *
 * Deletes the currently selected text of the editable.
 *
 * This call doesn’t do anything if there is no selected text.
 */
void
gtk_editable_delete_selection (GtkEditable *editable)
{
  int start, end;

  g_return_if_fail (GTK_IS_EDITABLE (editable));

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);
}

/**
 * gtk_editable_select_region: (virtual set_selection_bounds)
 * @editable: a `GtkEditable`
 * @start_pos: start of region
 * @end_pos: end of region
 *
 * Selects a region of text.
 *
 * The characters that are selected are those characters at positions
 * from @start_pos up to, but not including @end_pos. If @end_pos is
 * negative, then the characters selected are those characters from
 * @start_pos to  the end of the text.
 *
 * Note that positions are specified in characters, not bytes.
 */
void
gtk_editable_select_region (GtkEditable *editable,
                            int          start_pos,
                            int          end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_IFACE (editable)->set_selection_bounds (editable, start_pos, end_pos);
}

/**
 * gtk_editable_set_editable: (attributes org.gtk.Method.set_property=editable)
 * @editable: a `GtkEditable`
 * @is_editable: %TRUE if the user is allowed to edit the text
 *   in the widget
 *
 * Determines if the user can edit the text in the editable widget.
 */
void
gtk_editable_set_editable (GtkEditable *editable,
                           gboolean     is_editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "editable", is_editable, NULL);
}

/**
 * gtk_editable_get_editable: (attributes org.gtk.Method.get_property=editable)
 * @editable: a `GtkEditable`
 *
 * Retrieves whether @editable is editable.
 *
 * Returns: %TRUE if @editable is editable.
 */
gboolean
gtk_editable_get_editable (GtkEditable *editable)
{
  gboolean is_editable;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  g_object_get (editable, "editable", &is_editable, NULL);

  return is_editable;
}


/**
 * gtk_editable_get_alignment: (attributes org.gtk.Method.get_property=xalign)
 * @editable: a `GtkEditable`
 *
 * Gets the alignment of the editable.
 *
 * Returns: the alignment
 */
float
gtk_editable_get_alignment (GtkEditable *editable)
{
  float xalign;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "xalign", &xalign, NULL);

  return xalign;
}

/**
 * gtk_editable_set_alignment: (attributes org.gtk.Method.set_property=xalign)
 * @editable: a `GtkEditable`
 * @xalign: The horizontal alignment, from 0 (left) to 1 (right).
 *   Reversed for RTL layouts
 *
 * Sets the alignment for the contents of the editable.
 *
 * This controls the horizontal positioning of the contents when
 * the displayed text is shorter than the width of the editable.
 */
void
gtk_editable_set_alignment (GtkEditable *editable,
                            float        xalign)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "xalign", xalign, NULL);
}

/**
 * gtk_editable_get_width_chars: (attributes org.gtk.Method.get_property=width-chars)
 * @editable: a `GtkEditable`
 *
 * Gets the number of characters of space reserved
 * for the contents of the editable.
 *
 * Returns: number of chars to request space for, or negative if unset
 */
int
gtk_editable_get_width_chars (GtkEditable *editable)
{
  int width_chars;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "width-chars", &width_chars, NULL);

  return width_chars;
}

/**
 * gtk_editable_set_width_chars: (attributes org.gtk.Method.set_property=width-chars)
 * @editable: a `GtkEditable`
 * @n_chars: width in chars
 *
 * Changes the size request of the editable to be about the
 * right size for @n_chars characters.
 *
 * Note that it changes the size request, the size can still
 * be affected by how you pack the widget into containers.
 * If @n_chars is -1, the size reverts to the default size.
 */
void
gtk_editable_set_width_chars (GtkEditable *editable,
                              int          n_chars)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "width-chars", n_chars, NULL);
}

/**
 * gtk_editable_get_max_width_chars: (attributes org.gtk.Method.get_property=max-width-chars)
 * @editable: a `GtkEditable`
 *
 * Retrieves the desired maximum width of @editable, in characters.
 *
 * Returns: the maximum width of the entry, in characters
 */
int
gtk_editable_get_max_width_chars (GtkEditable *editable)
{
  int max_width_chars;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "max-width-chars", &max_width_chars, NULL);

  return max_width_chars;
}

/**
 * gtk_editable_set_max_width_chars: (attributes org.gtk.Method.set_property=max-width-chars)
 * @editable: a `GtkEditable`
 * @n_chars: the new desired maximum width, in characters
 *
 * Sets the desired maximum width in characters of @editable.
 */
void
gtk_editable_set_max_width_chars (GtkEditable *editable,
                                  int          n_chars)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "max-width-chars", n_chars, NULL);
}

/**
 * gtk_editable_get_enable_undo: (attributes org.gtk.Method.get_property=enable-undo)
 * @editable: a `GtkEditable`
 *
 * Gets if undo/redo actions are enabled for @editable
 *
 * Returns: %TRUE if undo is enabled
 */
gboolean
gtk_editable_get_enable_undo (GtkEditable *editable)
{
  gboolean enable_undo;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "enable-undo", &enable_undo, NULL);

  return enable_undo;
}

/**
 * gtk_editable_set_enable_undo: (attributes org.gtk.Method.set_property=enable-undo)
 * @editable: a `GtkEditable`
 * @enable_undo: if undo/redo should be enabled
 *
 * If enabled, changes to @editable will be saved for undo/redo
 * actions.
 *
 * This results in an additional copy of text changes and are not
 * stored in secure memory. As such, undo is forcefully disabled
 * when [property@Gtk.Text:visibility] is set to %FALSE.
 */
void
gtk_editable_set_enable_undo (GtkEditable *editable,
                              gboolean     enable_undo)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "enable-undo", enable_undo, NULL);
}

/**
 * gtk_editable_install_properties:
 * @object_class: a `GObjectClass`
 * @first_prop: property ID to use for the first property
 *
 * Overrides the `GtkEditable` properties for @class.
 *
 * This is a helper function that should be called in class_init,
 * after installing your own properties.
 *
 * Note that your class must have "text", "cursor-position",
 * "selection-bound", "editable", "width-chars", "max-width-chars",
 * "xalign" and "enable-undo" properties for this function to work.
 *
 * To handle the properties in your set_property and get_property
 * functions, you can either use [func@Gtk.Editable.delegate_set_property]
 * and [func@Gtk.Editable.delegate_get_property] (if you are using
 * a delegate), or remember the @first_prop offset and add it to the
 * values in the [enum@Gtk.EditableProperties] enumeration to get the
 * property IDs for these properties.
 *
 * Returns: the number of properties that were installed
 */
guint
gtk_editable_install_properties (GObjectClass *object_class,
                                 guint         first_prop)
{
  g_type_set_qdata (G_TYPE_FROM_CLASS (object_class),
                    quark_editable_data,
                    GUINT_TO_POINTER (first_prop));

  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_TEXT, "text");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_CURSOR_POSITION, "cursor-position");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_SELECTION_BOUND, "selection-bound");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_EDITABLE, "editable");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_WIDTH_CHARS, "width-chars");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS, "max-width-chars");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_XALIGN, "xalign");
  g_object_class_override_property (object_class, first_prop + GTK_EDITABLE_PROP_ENABLE_UNDO, "enable-undo");

  return GTK_EDITABLE_NUM_PROPERTIES;
}

static void
delegate_changed (GtkEditable *delegate,
                  gpointer     editable)
{
  g_signal_emit (editable, signals[CHANGED], 0);
}

static void
delegate_notify (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)), gtk_editable_get_type ());
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (data, pspec->name);
}

/**
 * gtk_editable_get_delegate:
 * @editable: a `GtkEditable`
 *
 * Gets the `GtkEditable` that @editable is delegating its
 * implementation to.
 *
 * Typically, the delegate is a [class@Gtk.Text] widget.
 *
 * Returns: (nullable) (transfer none): the delegate `GtkEditable`
 */
GtkEditable *
gtk_editable_get_delegate (GtkEditable *editable)
{
  return get_delegate (editable);
}

/**
 * gtk_editable_init_delegate:
 * @editable: a `GtkEditable`
 *
 * Sets up a delegate for `GtkEditable`.
 *
 * This is assuming that the get_delegate vfunc in the `GtkEditable`
 * interface has been set up for the @editable's type.
 *
 * This is a helper function that should be called in instance init,
 * after creating the delegate object.
 */
void
gtk_editable_init_delegate (GtkEditable *editable)
{
  GtkEditable *delegate = get_delegate (editable);
  g_signal_connect (delegate, "notify", G_CALLBACK (delegate_notify), editable);
  g_signal_connect (delegate, "changed", G_CALLBACK (delegate_changed), editable);
}

/**
 * gtk_editable_finish_delegate:
 * @editable: a `GtkEditable`
 *
 * Undoes the setup done by [method@Gtk.Editable.init_delegate].
 *
 * This is a helper function that should be called from dispose,
 * before removing the delegate object.
 */
void
gtk_editable_finish_delegate (GtkEditable *editable)
{
  GtkEditable *delegate = get_delegate (editable);
  g_signal_handlers_disconnect_by_func (delegate, delegate_notify, editable);
  g_signal_handlers_disconnect_by_func (delegate, delegate_changed, editable);
}

/**
 * gtk_editable_delegate_set_property:
 * @object: a `GObject`
 * @prop_id: a property ID
 * @value: value to set
 * @pspec: the `GParamSpec` for the property
 *
 * Sets a property on the `GtkEditable` delegate for @object.
 *
 * This is a helper function that should be called in the `set_property`
 * function of your `GtkEditable` implementation, before handling your
 * own properties.
 *
 * Returns: %TRUE if the property was found
 */
gboolean
gtk_editable_delegate_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkEditable *delegate = get_delegate (GTK_EDITABLE (object));
  GType type = G_TYPE_FROM_INSTANCE (object);
  guint first_prop;

  do {
    first_prop = GPOINTER_TO_UINT (g_type_get_qdata (type, quark_editable_data));
    type = g_type_parent (type);
  } while (first_prop == 0 && type != 0);

  if (prop_id < first_prop)
    return FALSE;

  switch (prop_id - first_prop)
    {
    case GTK_EDITABLE_PROP_TEXT:
      gtk_editable_set_text (delegate, g_value_get_string (value));
      break;

    case GTK_EDITABLE_PROP_EDITABLE:
      gtk_editable_set_editable (delegate, g_value_get_boolean (value));
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      gtk_editable_set_width_chars (delegate, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      gtk_editable_set_max_width_chars (delegate, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      gtk_editable_set_alignment (delegate, g_value_get_float (value));
      break;

    case GTK_EDITABLE_PROP_ENABLE_UNDO:
      gtk_editable_set_enable_undo (delegate, g_value_get_boolean (value));
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_editable_delegate_get_property:
 * @object: a `GObject`
 * @prop_id: a property ID
 * @value: value to set
 * @pspec: the `GParamSpec` for the property
 *
 * Gets a property of the `GtkEditable` delegate for @object.
 *
 * This is helper function that should be called in the `get_property`
 * function of your `GtkEditable` implementation, before handling your
 * own properties.
 *
 * Returns: %TRUE if the property was found
 */
gboolean
gtk_editable_delegate_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkEditable *delegate = get_delegate (GTK_EDITABLE (object));
  int cursor_position, selection_bound;
  GType type = G_TYPE_FROM_INSTANCE (object);
  guint first_prop;

  do {
    first_prop = GPOINTER_TO_UINT (g_type_get_qdata (type, quark_editable_data));
    type = g_type_parent (type);
  } while (first_prop == 0 && type != 0);

  if (prop_id < first_prop)
    return FALSE;

  switch (prop_id - first_prop)
    {
    case GTK_EDITABLE_PROP_TEXT:
      g_value_set_string (value, gtk_editable_get_text (delegate));
      break;

    case GTK_EDITABLE_PROP_CURSOR_POSITION:
      gtk_editable_get_selection_bounds (delegate, &cursor_position, &selection_bound);
      g_value_set_int (value, cursor_position);
      break;

    case GTK_EDITABLE_PROP_SELECTION_BOUND:
      gtk_editable_get_selection_bounds (delegate, &cursor_position, &selection_bound);
      g_value_set_int (value, selection_bound);
      break;

    case GTK_EDITABLE_PROP_EDITABLE:
      g_value_set_boolean (value, gtk_editable_get_editable (delegate));
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      g_value_set_int (value, gtk_editable_get_width_chars (delegate));
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_editable_get_max_width_chars (delegate));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      g_value_set_float (value, gtk_editable_get_alignment (delegate));
      break;

    case GTK_EDITABLE_PROP_ENABLE_UNDO:
      g_value_set_boolean (value, gtk_editable_get_enable_undo (delegate));
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_editable_delegate_get_accessible_platform_state:
 * @editable: a `GtkEditable` implementation
 * @state: what kind of accessible state to retrieve
 *
 * Retrieves the accessible platform state from the editable delegate.
 *
 * This is an helper function to retrieve the accessible state for
 * `GtkEditable` interface implementations using a delegate pattern.
 *
 * You should call this function in your editable widget implementation
 * of the [vfunc@Gtk.Accessible.get_platform_state] virtual function, for
 * instance:
 *
 * ```c
 * static void
 * accessible_interface_init (GtkAccessibleInterface *iface)
 * {
 *   iface->get_platform_state = your_editable_get_accessible_platform_state;
 * }
 *
 * static gboolean
 * your_editable_get_accessible_platform_state (GtkAccessible *accessible,
 *                                              GtkAccessiblePlatformState state)
 * {
 *   return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (accessible), state);
 * }
 * ```
 *
 * Since: 4.10
 */
gboolean
gtk_editable_delegate_get_accessible_platform_state (GtkEditable                *editable,
                                                     GtkAccessiblePlatformState  state)
{
  GtkWidget *delegate = GTK_WIDGET (gtk_editable_get_delegate (editable));

  switch (state)
    {
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE:
      return gtk_widget_get_focusable (delegate);
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED:
      return gtk_widget_has_focus (delegate);
    case GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE:
      return FALSE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}
