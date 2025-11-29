/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Takao Fujiwara <takao.fujiwara1@gmail.com>
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

#include <glib.h>


G_BEGIN_DECLS

typedef struct _GtkComposeTable GtkComposeTable;
typedef struct _GtkComposeTableCompact GtkComposeTableCompact;

/* The layout of the data is as follows:
 *
 * The first part of the data contains rows of length max_seq_len + 1,
 * where the first element is the item of the sequence, and the
 * following elements are offsets to the data for sequences that
 * start with the first item of length 1, ..., max_seq_len.
 *
 * The second part of the data contains the rest of the sequence
 * data. It does not have a fixed stride. For each sequence, we
 * put seq[2], ..., seq[len - 1], followed by the encoded value
 * for this sequence. In particular for a sequence of length 1,
 * the offset points directly to the value.
 *
 * The values are encoded as follows:
 *
 * If the value is a single Unicode character smaller than 0x8000,
 * then we place it directly. Otherwise, we put the UTF8-encoded
 * value in the char_data array, and use offset | 0x8000 as the
 * encoded value.
 */
struct _GtkComposeTable
{
  guint16 *data;
  char *char_data;
  int max_seq_len;
  int n_index_size;
  int data_size;
  int n_chars;
  int n_sequences;
  guint32 id;
};

GtkComposeTable * gtk_compose_table_new_with_file (const char    *compose_file);
GtkComposeTable * gtk_compose_table_parse         (const char    *compose_file,
                                                   gboolean      *found_include);
GtkComposeTable * gtk_compose_table_new_with_data (const guint16 *data,
                                                   int            max_seq_len,
                                                   int            n_seqs);

typedef void (* GtkComposeSequenceCallback) (gunichar   *sequence,
                                             int         len,
                                             const char *value,
                                             gpointer    data);

void              gtk_compose_table_foreach (const GtkComposeTable      *table,
                                             GtkComposeSequenceCallback  callback,
                                             gpointer                    data);

gboolean          gtk_compose_table_check   (const GtkComposeTable *table,
                                             const guint           *compose_buffer,
                                             int                    n_compose,
                                             gboolean              *compose_finish,
                                             gboolean              *compose_match,
                                             GString               *output);

void              gtk_compose_table_get_prefix (const GtkComposeTable *table,
                                                const guint           *compose_buffer,
                                                int                    n_compose,
                                                int                   *prefix);

gboolean          gtk_check_algorithmically (const guint           *compose_buffer,
                                             int                    n_compose,
                                             GString               *output);

guint32           gtk_compose_table_data_hash (const guint16 *data,
                                               int            max_seq_len,
                                               int            n_seqs);

char *            gtk_compose_table_get_x11_compose_file_dir (void);

G_END_DECLS

