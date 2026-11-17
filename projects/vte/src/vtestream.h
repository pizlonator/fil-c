/*
 * Copyright (C) 2009,2010 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef vtestream_h_included
#define vtestream_h_included

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _VteStream VteStream;

void _vte_stream_reset (VteStream *stream, gsize offset);
gboolean _vte_stream_read (VteStream *stream, gsize offset, char *data, gsize len);
void _vte_stream_append (VteStream *stream, const char *data, gsize len);
void _vte_stream_truncate (VteStream *stream, gsize offset);
void _vte_stream_advance_tail (VteStream *stream, gsize offset);
gsize _vte_stream_tail (VteStream *stream);
gsize _vte_stream_head (VteStream *stream);

/* Various streams */

VteStream *
_vte_file_stream_new (void);

G_END_DECLS

#endif
