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

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * VteStream: Abstract base stream class
 */

struct _VteStream {
	GObject parent;
};

typedef struct _VteStreamClass {
	GObjectClass parent_class;

	void (*reset) (VteStream *stream, gsize offset);
	gboolean (*read) (VteStream *stream, gsize offset, char *data, gsize len);
	void (*append) (VteStream *stream, const char *data, gsize len);
	void (*truncate) (VteStream *stream, gsize offset);
	void (*advance_tail) (VteStream *stream, gsize offset);
	gsize (*tail) (VteStream *stream);
	gsize (*head) (VteStream *stream);
} VteStreamClass;

static GType _vte_stream_get_type (void);
#define VTE_TYPE_STREAM _vte_stream_get_type ()
#define VTE_STREAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_STREAM, VteStreamClass))

G_DEFINE_ABSTRACT_TYPE (VteStream, _vte_stream, G_TYPE_OBJECT)

static void
_vte_stream_class_init (VteStreamClass *klass G_GNUC_UNUSED)
{
}

static void
_vte_stream_init (VteStream *stream G_GNUC_UNUSED)
{
}

void
_vte_stream_reset (VteStream *stream, gsize offset)
{
	VTE_STREAM_GET_CLASS (stream)->reset (stream, offset);
}

gboolean
_vte_stream_read (VteStream *stream, gsize offset, char *data, gsize len)
{
	return VTE_STREAM_GET_CLASS (stream)->read (stream, offset, data, len);
}

void
_vte_stream_append (VteStream *stream, const char *data, gsize len)
{
	VTE_STREAM_GET_CLASS (stream)->append (stream, data, len);
}

void
_vte_stream_truncate (VteStream *stream, gsize offset)
{
	VTE_STREAM_GET_CLASS (stream)->truncate (stream, offset);
}

void
_vte_stream_advance_tail (VteStream *stream, gsize offset)
{
	VTE_STREAM_GET_CLASS (stream)->advance_tail (stream, offset);
}

gsize
_vte_stream_tail (VteStream *stream)
{
	return VTE_STREAM_GET_CLASS (stream)->tail (stream);
}

gsize
_vte_stream_head (VteStream *stream)
{
	return VTE_STREAM_GET_CLASS (stream)->head (stream);
}

G_END_DECLS

