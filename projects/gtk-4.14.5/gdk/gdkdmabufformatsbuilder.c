/* gdkdmabufformats.c
 *
 * Copyright 2023 Red Hat, Inc.
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

#include <config.h>

#include "gdkdmabufformatsbuilderprivate.h"

#include "gdkdmabufformatsprivate.h"


#define GDK_ARRAY_NAME gdk_dmabuf_formats_builder
#define GDK_ARRAY_TYPE_NAME GdkDmabufFormatsBuilder
#define GDK_ARRAY_ELEMENT_TYPE GdkDmabufFormat
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 1024
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

/* NB: We keep duplicates in the list for ease of use. Only when creating the final
 * GdkDmabufFormats do we actually remove duplicates.
 */

GdkDmabufFormatsBuilder *
gdk_dmabuf_formats_builder_new (void)
{
  GdkDmabufFormatsBuilder *result = g_new (GdkDmabufFormatsBuilder, 1);

  gdk_dmabuf_formats_builder_init (result);

  return result;
}

static int
gdk_dmabuf_format_compare (gconstpointer data_a,
                           gconstpointer data_b)
{
  const GdkDmabufFormat *a = data_a;
  const GdkDmabufFormat *b = data_b;

  if (a->fourcc == b->fourcc)
    return (a->modifier - b->modifier) >> 8 * (sizeof (gint64) - sizeof (gint));
  else
    return a->fourcc - b->fourcc;

}

static gboolean
gdk_dmabuf_format_equal (gconstpointer data_a,
                         gconstpointer data_b)
{
  const GdkDmabufFormat *a = data_a;
  const GdkDmabufFormat *b = data_b;

  return a->fourcc == b->fourcc &&
         a->modifier == b->modifier;
}

static void
gdk_dmabuf_formats_builder_sort (GdkDmabufFormatsBuilder *self)
{
  qsort (gdk_dmabuf_formats_builder_get_data (self),
         gdk_dmabuf_formats_builder_get_size (self),
         sizeof (GdkDmabufFormat),
         gdk_dmabuf_format_compare);
}

/* list must be sorted */
static void
gdk_dmabuf_formats_builder_remove_duplicates (GdkDmabufFormatsBuilder *self)
{
  gsize i, j;

  for (i = 1, j = 0; i < gdk_dmabuf_formats_builder_get_size (self); i++)
    {
      if (gdk_dmabuf_format_equal (gdk_dmabuf_formats_builder_get (self, i),
                                   gdk_dmabuf_formats_builder_get (self, j)))
        continue;

      j++;
      if (i != j)
        *gdk_dmabuf_formats_builder_index (self, j) = *gdk_dmabuf_formats_builder_index (self, i);
    }
}

GdkDmabufFormats *
gdk_dmabuf_formats_builder_free_to_formats (GdkDmabufFormatsBuilder *self)
{
  GdkDmabufFormats *formats;

  gdk_dmabuf_formats_builder_sort (self);
  gdk_dmabuf_formats_builder_remove_duplicates (self);

  formats = gdk_dmabuf_formats_new (gdk_dmabuf_formats_builder_get_data (self),
                                    gdk_dmabuf_formats_builder_get_size (self));
  gdk_dmabuf_formats_builder_clear (self);
  g_free (self);

  return formats;
}

void
gdk_dmabuf_formats_builder_add_format (GdkDmabufFormatsBuilder *self,
                                       guint32                  fourcc,
                                       guint64                  modifier)
{
  gdk_dmabuf_formats_builder_append (self, &(GdkDmabufFormat) { fourcc, modifier });
}

void
gdk_dmabuf_formats_builder_add_formats (GdkDmabufFormatsBuilder *self,
                                        GdkDmabufFormats        *formats)
{
  gdk_dmabuf_formats_builder_splice (self,
                                     gdk_dmabuf_formats_builder_get_size (self),
                                     0,
                                     FALSE,
                                     gdk_dmabuf_formats_peek_formats (formats),
                                     gdk_dmabuf_formats_get_n_formats (formats));
}
