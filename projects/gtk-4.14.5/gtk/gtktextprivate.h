/* gtkentryprivate.h
 * Copyright (C) 2019 Red Hat, Inc.
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

#pragma once

#include "gtktext.h"

#include "gtkeventcontroller.h"
#include  "gtkimcontext.h"

G_BEGIN_DECLS

#define GTK_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT, GtkTextClass))
#define GTK_IS_TEXT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT))
#define GTK_TEXT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT, GtkTextClass))

typedef struct _GtkTextClass         GtkTextClass;

/*<private>
 * GtkTextClass:
 * @parent_class: The parent class.
 * @activate: Class handler for the `GtkText::activate` signal. The default
 *   implementation activates the gtk.activate-default action.
 * @move_cursor: Class handler for the `GtkText::move-cursor` signal. The
 *   default implementation specifies the standard `GtkText` cursor movement
 *   behavior.
 * @insert_at_cursor: Class handler for the `GtkText::insert-at-cursor` signal.
 *   The default implementation inserts text at the cursor.
 * @delete_from_cursor: Class handler for the `GtkText::delete-from-cursor`
 *   signal. The default implementation deletes the selection or the specified
 *   number of characters or words.
 * @backspace: Class handler for the `GtkText::backspace` signal. The default
 *   implementation deletes the selection or a single character or word.
 * @cut_clipboard: Class handler for the `GtkText::cut-clipboard` signal. The
 *   default implementation cuts the selection, if one exists.
 * @copy_clipboard: Class handler for the `GtkText::copy-clipboard` signal. The
 *   default implementation copies the selection, if one exists.
 * @paste_clipboard: Class handler for the `GtkText::paste-clipboard` signal.
 *   The default implementation pastes at the current cursor position or over
 *   the current selection if one exists.
 * @toggle_overwrite: Class handler for the `GtkText::toggle-overwrite` signal.
 *   The default implementation toggles overwrite mode and blinks the cursor.
 * @insert_emoji: Class handler for the `GtkText::insert-emoji` signal.
 *
 * Class structure for `GtkText`. All virtual functions have a default
 * implementation. Derived classes may set the virtual function pointers for the
 * signal handlers to %NULL, but must keep @get_text_area_size and
 * @get_frame_size non-%NULL; either use the default implementation, or provide
 * a custom one.
 */
struct _GtkTextClass
{
  GtkWidgetClass parent_class;

  /* Action signals
   */
  void (* activate)           (GtkText         *self);
  void (* move_cursor)        (GtkText         *self,
                               GtkMovementStep  step,
                               int              count,
                               gboolean         extend);
  void (* insert_at_cursor)   (GtkText         *self,
                               const char      *str);
  void (* delete_from_cursor) (GtkText         *self,
                               GtkDeleteType    type,
                               int              count);
  void (* backspace)          (GtkText         *self);
  void (* cut_clipboard)      (GtkText         *self);
  void (* copy_clipboard)     (GtkText         *self);
  void (* paste_clipboard)    (GtkText         *self);
  void (* toggle_overwrite)   (GtkText         *self);
  void (* insert_emoji)       (GtkText         *self);
  void (* undo)               (GtkText         *self);
  void (* redo)               (GtkText         *self);
};

char *              gtk_text_get_display_text   (GtkText    *entry,
                                                 int         start_pos,
                                                 int         end_pos);
void                gtk_text_enter_text         (GtkText    *entry,
                                                 const char *text);
void                gtk_text_set_positions      (GtkText    *entry,
                                                 int         current_pos,
                                                 int         selection_bound);
PangoLayout *       gtk_text_get_layout         (GtkText    *entry);
void                gtk_text_get_layout_offsets (GtkText    *entry,
                                                 int        *x,
                                                 int        *y);
void                gtk_text_reset_im_context   (GtkText    *entry);
GtkEventController *gtk_text_get_key_controller (GtkText    *entry);

G_END_DECLS

