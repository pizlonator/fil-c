/* List Box/Complex
 *
 * GtkListBox allows lists with complicated layouts, using
 * regular widgets supporting sorting and filtering.
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static GdkTexture *avatar_texture_other;
static GtkWidget *window = NULL;

#define GTK_TYPE_MESSAGE		  (gtk_message_get_type ())
#define GTK_MESSAGE(message)		  (G_TYPE_CHECK_INSTANCE_CAST ((message), GTK_TYPE_MESSAGE, GtkMessage))
#define GTK_MESSAGE_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MESSAGE, GtkMessageClass))
#define GTK_IS_MESSAGE(message)		  (G_TYPE_CHECK_INSTANCE_TYPE ((message), GTK_TYPE_MESSAGE))
#define GTK_IS_MESSAGE_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MESSAGE))
#define GTK_MESSAGE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MESSAGE, GtkMessageClass))

#define GTK_TYPE_MESSAGE_ROW		  (gtk_message_row_get_type ())
#define GTK_MESSAGE_ROW(message_row)		  (G_TYPE_CHECK_INSTANCE_CAST ((message_row), GTK_TYPE_MESSAGE_ROW, GtkMessageRow))
#define GTK_MESSAGE_ROW_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MESSAGE_ROW, GtkMessageRowClass))
#define GTK_IS_MESSAGE_ROW(message_row)		  (G_TYPE_CHECK_INSTANCE_TYPE ((message_row), GTK_TYPE_MESSAGE_ROW))
#define GTK_IS_MESSAGE_ROW_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MESSAGE_ROW))
#define GTK_MESSAGE_ROW_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MESSAGE_ROW, GtkMessageRowClass))

typedef struct _GtkMessage   GtkMessage;
typedef struct _GtkMessageClass  GtkMessageClass;
typedef struct _GtkMessageRow   GtkMessageRow;
typedef struct _GtkMessageRowClass  GtkMessageRowClass;
typedef struct _GtkMessageRowPrivate  GtkMessageRowPrivate;


struct _GtkMessage
{
  GObject parent;

  guint id;
  char *sender_name;
  char *sender_nick;
  char *message;
  gint64 time;
  guint reply_to;
  char *resent_by;
  int n_favorites;
  int n_reshares;
};

struct _GtkMessageClass
{
  GObjectClass parent_class;
};

struct _GtkMessageRow
{
  GtkListBoxRow parent;

  GtkMessageRowPrivate *priv;
};

struct _GtkMessageRowClass
{
  GtkListBoxRowClass parent_class;
};

struct _GtkMessageRowPrivate
{
  GtkMessage *message;
  GtkRevealer *details_revealer;
  GtkImage *avatar_image;
  GtkWidget *extra_buttons_box;
  GtkLabel *content_label;
  GtkLabel *source_name;
  GtkLabel *source_nick;
  GtkLabel *short_time_label;
  GtkLabel *detailed_time_label;
  GtkBox *resent_box;
  GtkLinkButton *resent_by_button;
  GtkLabel *n_favorites_label;
  GtkLabel *n_reshares_label;
  GtkButton *expand_button;
};

GType      gtk_message_get_type  (void) G_GNUC_CONST;
GType      gtk_message_row_get_type  (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkMessage, gtk_message, G_TYPE_OBJECT);

static void
gtk_message_finalize (GObject *obj)
{
  GtkMessage *msg = GTK_MESSAGE (obj);

  g_free (msg->sender_name);
  g_free (msg->sender_nick);
  g_free (msg->message);
  g_free (msg->resent_by);

  G_OBJECT_CLASS (gtk_message_parent_class)->finalize (obj);
}
static void
gtk_message_class_init (GtkMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gtk_message_finalize;
}

static void
gtk_message_init (GtkMessage *msg)
{
}

static void
gtk_message_parse (GtkMessage *msg, const char *str)
{
  char **strv;
  int i;

  strv = g_strsplit (str, "|", 0);

  i = 0;
  msg->id = strtol (strv[i++], NULL, 10);
  msg->sender_name = g_strdup (strv[i++]);
  msg->sender_nick = g_strdup (strv[i++]);
  msg->message = g_strdup (strv[i++]);
  msg->time = strtol (strv[i++], NULL, 10);
  if (strv[i])
    {
      msg->reply_to = strtol (strv[i++], NULL, 10);
      if (strv[i])
        {
          if (*strv[i])
            msg->resent_by = g_strdup (strv[i]);
          i++;
          if (strv[i])
            {
              msg->n_favorites = strtol (strv[i++], NULL, 10);
              if (strv[i])
                {
                  msg->n_reshares = strtol (strv[i++], NULL, 10);
                }

            }
        }
    }

  g_strfreev (strv);
}

static GtkMessage *
gtk_message_new (const char *str)
{
  GtkMessage *msg;
  msg = g_object_new (gtk_message_get_type (), NULL);
  gtk_message_parse (msg, str);
  return msg;
}

G_DEFINE_TYPE_WITH_PRIVATE (GtkMessageRow, gtk_message_row, GTK_TYPE_LIST_BOX_ROW);


static void
gtk_message_row_update (GtkMessageRow *row)
{
  GtkMessageRowPrivate *priv = row->priv;
  GDateTime *t;
  char *s;

  gtk_label_set_text (priv->source_name, priv->message->sender_name);
  gtk_label_set_text (priv->source_nick, priv->message->sender_nick);
  gtk_label_set_text (priv->content_label, priv->message->message);
  t = g_date_time_new_from_unix_utc (priv->message->time);
  s = g_date_time_format (t, "%e %b %y");
  gtk_label_set_text (priv->short_time_label, s);
  g_free (s);
  s = g_date_time_format (t, "%X - %e %b %Y");
  gtk_label_set_text (priv->detailed_time_label, s);
  g_free (s);
  g_date_time_unref (t);

  gtk_widget_set_visible (GTK_WIDGET(priv->n_favorites_label),
                          priv->message->n_favorites != 0);
  s = g_strdup_printf ("<b>%d</b>\nFavorites", priv->message->n_favorites);
  gtk_label_set_markup (priv->n_favorites_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET(priv->n_reshares_label),
                          priv->message->n_reshares != 0);
  s = g_strdup_printf ("<b>%d</b>\nReshares", priv->message->n_reshares);
  gtk_label_set_markup (priv->n_reshares_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET (priv->resent_box), priv->message->resent_by != NULL);
  if (priv->message->resent_by)
    gtk_button_set_label (GTK_BUTTON (priv->resent_by_button), priv->message->resent_by);

  if (strcmp (priv->message->sender_nick, "@GTKtoolkit") == 0)
    gtk_image_set_from_icon_name (priv->avatar_image, "org.gtk.Demo4");
  else
    gtk_image_set_from_paintable (priv->avatar_image, GDK_PAINTABLE (avatar_texture_other));

}

static void
gtk_message_row_expand (GtkMessageRow *row)
{
  GtkMessageRowPrivate *priv = row->priv;
  gboolean expand;

  expand = !gtk_revealer_get_reveal_child (priv->details_revealer);

  gtk_revealer_set_reveal_child (priv->details_revealer, expand);
  if (expand)
    gtk_button_set_label (priv->expand_button, "Hide");
  else
    gtk_button_set_label (priv->expand_button, "Expand");
}

static void
expand_clicked (GtkMessageRow *row,
                GtkButton *button)
{
  gtk_message_row_expand (row);
}

static void
reshare_clicked (GtkMessageRow *row,
                 GtkButton *button)
{
  GtkMessageRowPrivate *priv = row->priv;

  priv->message->n_reshares++;
  gtk_message_row_update (row);
}

static void
favorite_clicked (GtkMessageRow *row,
                  GtkButton *button)
{
  GtkMessageRowPrivate *priv = row->priv;

  priv->message->n_favorites++;
  gtk_message_row_update (row);
}

static void
gtk_message_row_state_flags_changed (GtkWidget    *widget,
                                     GtkStateFlags previous_state_flags)
{
  GtkMessageRowPrivate *priv = GTK_MESSAGE_ROW (widget)->priv;
  GtkStateFlags flags;
  gboolean visible;

  flags = gtk_widget_get_state_flags (widget);

  visible = flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED) ? TRUE : FALSE;
  gtk_widget_set_visible (priv->extra_buttons_box, visible);

  GTK_WIDGET_CLASS (gtk_message_row_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_message_row_dispose (GObject *obj)
{
  gtk_widget_dispose_template (GTK_WIDGET (obj), GTK_TYPE_MESSAGE_ROW);
  G_OBJECT_CLASS (gtk_message_row_parent_class)->dispose (obj);
}

static void
gtk_message_row_finalize (GObject *obj)
{
  GtkMessageRowPrivate *priv = GTK_MESSAGE_ROW (obj)->priv;
  g_object_unref (priv->message);
  G_OBJECT_CLASS (gtk_message_row_parent_class)->finalize (obj);
}

static void
gtk_message_row_class_init (GtkMessageRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_message_row_dispose;
  object_class->finalize = gtk_message_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/listbox/listbox.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, content_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, source_name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, source_nick);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, short_time_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, detailed_time_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, extra_buttons_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, details_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, avatar_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, resent_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, resent_by_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, n_reshares_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, n_favorites_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, expand_button);
  gtk_widget_class_bind_template_callback (widget_class, expand_clicked);
  gtk_widget_class_bind_template_callback (widget_class, reshare_clicked);
  gtk_widget_class_bind_template_callback (widget_class, favorite_clicked);

  widget_class->state_flags_changed = gtk_message_row_state_flags_changed;
}

static void
gtk_message_row_init (GtkMessageRow *row)
{
  row->priv = gtk_message_row_get_instance_private (row);

  gtk_widget_init_template (GTK_WIDGET (row));
}

static GtkMessageRow *
gtk_message_row_new (GtkMessage *message)
{
  GtkMessageRow *row;

  row = g_object_new (gtk_message_row_get_type (), NULL);
  row->priv->message = message;
  gtk_message_row_update (row);

  return row;
}

static int
gtk_message_row_sort (GtkMessageRow *a, GtkMessageRow *b, gpointer data)
{
  return b->priv->message->time - a->priv->message->time;
}

static void
row_activated (GtkListBox *listbox, GtkListBoxRow *row)
{
  gtk_message_row_expand (GTK_MESSAGE_ROW (row));
}

GtkWidget *
do_listbox (GtkWidget *do_widget)
{
  GtkWidget *scrolled, *listbox, *vbox, *label;
  GtkMessage *message;
  GtkMessageRow *row;
  GBytes *data;
  char **lines;
  int i;

  if (!window)
    {
      avatar_texture_other = gdk_texture_new_from_resource ("/listbox/apple-red.png");

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "List Box — Complex");
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_window_set_child (GTK_WINDOW (window), vbox);
      label = gtk_label_new ("Messages from GTK and friends");
      gtk_box_append (GTK_BOX (vbox), label);
      scrolled = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand (scrolled, TRUE);
      gtk_box_append (GTK_BOX (vbox), scrolled);
      listbox = gtk_list_box_new ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), listbox);

      gtk_list_box_set_sort_func (GTK_LIST_BOX (listbox), (GtkListBoxSortFunc)gtk_message_row_sort, listbox, NULL);
      gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (listbox), FALSE);
      g_signal_connect (listbox, "row-activated", G_CALLBACK (row_activated), NULL);

      data = g_resources_lookup_data ("/listbox/messages.txt", 0, NULL);
      lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

      for (i = 0; lines[i] != NULL && *lines[i]; i++)
        {
          message = gtk_message_new (lines[i]);
          row = gtk_message_row_new (message);
          gtk_list_box_insert (GTK_LIST_BOX (listbox), GTK_WIDGET (row), -1);
        }

      g_strfreev (lines);
      g_bytes_unref (data);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
