/* GTK - The GIMP Toolkit
 * gtktextbufferprivate.h Copyright (C) 2015 Red Hat, Inc.
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

#pragma once

#include <gtk/gtktextbuffer.h>
#include "gtktexttypesprivate.h"

G_BEGIN_DECLS

void            _gtk_text_buffer_spew                  (GtkTextBuffer      *buffer);

GtkTextBTree*   _gtk_text_buffer_get_btree             (GtkTextBuffer      *buffer);

const PangoLogAttr* _gtk_text_buffer_get_line_log_attrs (GtkTextBuffer     *buffer,
                                                         const GtkTextIter *anywhere_in_line,
                                                         int               *char_len);

void _gtk_text_buffer_notify_will_remove_tag (GtkTextBuffer *buffer,
                                              GtkTextTag    *tag);


const char *gtk_justification_to_string (GtkJustification just);
const char *gtk_text_direction_to_string (GtkTextDirection direction);
const char *gtk_wrap_mode_to_string (GtkWrapMode wrap_mode);

void gtk_text_buffer_add_run_attributes (GtkTextBuffer   *buffer,
                                         int              offset,
                                         GHashTable      *attributes,
                                         int             *start_offset,
                                         int             *end_offset);

G_END_DECLS
