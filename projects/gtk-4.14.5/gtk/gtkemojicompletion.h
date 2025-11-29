/* gtkemojicompletion.h: An Emoji picker widget
 * Copyright 2017, Red Hat, Inc.
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktext.h>

G_BEGIN_DECLS

#define GTK_TYPE_EMOJI_COMPLETION                 (gtk_emoji_completion_get_type ())
#define GTK_EMOJI_COMPLETION(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EMOJI_COMPLETION, GtkEmojiCompletion))
#define GTK_EMOJI_COMPLETION_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_EMOJI_COMPLETION, GtkEmojiCompletionClass))
#define GTK_IS_EMOJI_COMPLETION(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EMOJI_COMPLETION))
#define GTK_IS_EMOJI_COMPLETION_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_EMOJI_COMPLETION))
#define GTK_EMOJI_COMPLETION_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EMOJI_COMPLETION, GtkEmojiCompletionClass))

typedef struct _GtkEmojiCompletion      GtkEmojiCompletion;
typedef struct _GtkEmojiCompletionClass GtkEmojiCompletionClass;

GType      gtk_emoji_completion_get_type (void) G_GNUC_CONST;
GtkWidget *gtk_emoji_completion_new      (GtkText *text);

G_END_DECLS
