/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2019 Red Hat, Inc.
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

#include "gtktextlayoutprivate.h"

G_BEGIN_DECLS

typedef struct _GtkTextLineDisplayCache GtkTextLineDisplayCache;

GtkTextLineDisplayCache *gtk_text_line_display_cache_new                (void);
void                     gtk_text_line_display_cache_free               (GtkTextLineDisplayCache *cache);
GtkTextLineDisplay      *gtk_text_line_display_cache_get                (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLayout           *layout,
                                                                         GtkTextLine             *line,
                                                                         gboolean                 size_only);
void                     gtk_text_line_display_cache_delay_eviction     (GtkTextLineDisplayCache *cache);
void                     gtk_text_line_display_cache_set_cursor_line    (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLine             *line);
void                     gtk_text_line_display_cache_invalidate         (GtkTextLineDisplayCache *cache);
void                     gtk_text_line_display_cache_invalidate_cursors (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLine             *line);
void                     gtk_text_line_display_cache_invalidate_display (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLineDisplay      *display,
                                                                         gboolean                 cursors_only);
void                     gtk_text_line_display_cache_invalidate_line    (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLine             *line);
void                     gtk_text_line_display_cache_invalidate_range   (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLayout           *layout,
                                                                         const GtkTextIter       *begin,
                                                                         const GtkTextIter       *end,
                                                                         gboolean                 cursors_only);
void                     gtk_text_line_display_cache_invalidate_y_range (GtkTextLineDisplayCache *cache,
                                                                         GtkTextLayout           *layout,
                                                                         int                      y,
                                                                         int                      old_height,
                                                                         int                      new_height,
                                                                         gboolean                 cursors_only);
void                     gtk_text_line_display_cache_set_mru_size       (GtkTextLineDisplayCache *cache,
                                                                         guint                    mru_size);

G_END_DECLS

