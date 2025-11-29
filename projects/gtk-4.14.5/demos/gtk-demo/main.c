/* GTK Demo
 *
 * GTK Demo is a collection of useful examples to demonstrate
 * GTK widgets and features. It is a useful example in itself.
 *
 * You can select examples in the sidebar or search for them by
 * typing a search term. Double-clicking or hitting the “Run” button
 * will run the demo. The source code and other resources used in the
 * demo are shown in this area.
 *
 * You can also use the GTK Inspector, available from the menu on the
 * top right, to poke at the running demos, and see how they are put
 * together.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "demos.h"
#include "fontify.h"

#include "profile_conf.h"

static GtkWidget *info_view;
static GtkWidget *source_view;

static char *current_file = NULL;

static GtkWidget *notebook;
static GtkSingleSelection *selection;
static GtkWidget *toplevel;
static char **search_needle;

typedef struct _GtkDemo GtkDemo;
struct _GtkDemo
{
  GObject parent_instance;

  const char *name;
  const char *title;
  const char **keywords;
  const char *filename;
  GDoDemoFunc func;
  GListModel *children_model;
};

enum {
  PROP_0,
  PROP_FILENAME,
  PROP_NAME,
  PROP_TITLE,
  PROP_KEYWORDS,

  N_PROPS
};

# define GTK_TYPE_DEMO (gtk_demo_get_type ())
G_DECLARE_FINAL_TYPE (GtkDemo, gtk_demo, GTK, DEMO, GObject);

G_DEFINE_TYPE (GtkDemo, gtk_demo, G_TYPE_OBJECT);
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_demo_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkDemo *self = GTK_DEMO (object);

  switch (property_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, self->filename);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_KEYWORDS:
      g_value_set_boxed (value, self->keywords);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void gtk_demo_class_init (GtkDemoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_demo_get_property;

  properties[PROP_FILENAME] =
    g_param_spec_string ("filename",
                         "filename",
                         "filename",
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "name",
                         "name",
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "title",
                         "title",
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_KEYWORDS] =
    g_param_spec_string ("keywords",
                         "keywords",
                         "keywords",
                         NULL,
                         G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void gtk_demo_init (GtkDemo *self)
{
}

typedef struct _CallbackData CallbackData;
struct _CallbackData
{
  GtkTreeModel *model;
  GtkTreePath *path;
};

static gboolean
gtk_demo_run (GtkDemo   *self,
              GtkWidget *window)
{
  GtkWidget *result;

  if (!self->func)
    return FALSE;

  result = self->func (window);
  if (result == NULL)
    return FALSE;

  if (GTK_IS_WINDOW (result))
    {
      gtk_window_set_transient_for (GTK_WINDOW (result), GTK_WINDOW (window));
      gtk_window_set_modal (GTK_WINDOW (result), TRUE);
    }
  return TRUE;
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  const char *authors[] = {
    "The GTK Team",
    NULL
  };
  char *version;
  char *os_name;
  char *os_version;
  GString *s;

  s = g_string_new ("");

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);
  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tPango\t%s\n",
                          pango_version_string ());
  g_string_append_printf (s, "\tGTK \t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());
  g_string_append_printf (s, "\nA link can appear here: <http://www.gtk.org>");

  version = g_strdup_printf ("%s%s%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
                             g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "",
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  gtk_show_about_dialog (GTK_WINDOW (gtk_application_get_active_window (app)),
                         "program-name", g_strcmp0 (PROFILE, "devel") == 0
                                         ? "GTK Demo (Development)"
                                         : "GTK Demo",
                         "version", version,
                         "copyright", "© 1997—2021 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK widgets",
                         "authors", authors,
                         "logo-icon-name", "org.gtk.Demo4",
                         "title", "About GTK Demo",
                         "system-information", s->str,
                         NULL);

  g_string_free (s, TRUE);
  g_free (version);
  g_free (os_name);
  g_free (os_version);
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkApplication *app = user_data;
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (app);
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_window_destroy (GTK_WINDOW (win));

      list = next;
    }
}

static void
activate_inspector (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
activate_run (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       window)
{
  GtkTreeListRow *row = gtk_single_selection_get_selected_item (selection);
  GtkDemo *demo = gtk_tree_list_row_get_item (row);

  gtk_demo_run (demo, window);
}

static GtkWidget *
display_image (const char *format,
               const char *resource,
               GtkWidget  *label)
{
  GtkWidget *sw, *image;

  image = gtk_picture_new_for_resource (resource);
  gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), image);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  return sw;
}

static GtkWidget *
display_images (const char *format,
                const char *resource_dir,
                GtkWidget  *label)
{
  char **resources;
  GtkWidget *grid;
  GtkWidget *sw;
  GtkWidget *widget;
  guint i;

  resources = g_resources_enumerate_children (resource_dir, 0, NULL);
  if (resources == NULL)
    return NULL;

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  grid = gtk_flow_box_new ();
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (grid), GTK_SELECTION_NONE);
  gtk_widget_set_valign (grid, GTK_ALIGN_START);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), grid);

  for (i = 0; resources[i]; i++)
    {
      char *resource_name;
      GtkWidget *box;
      GtkWidget *image_label;

      resource_name = g_strconcat (resource_dir, "/", resources[i], NULL);

      image_label = gtk_label_new (resources[i]);
      widget = display_image (NULL, resource_name, image_label);
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (box), widget);
      gtk_box_append (GTK_BOX (box), image_label);
      gtk_flow_box_insert (GTK_FLOW_BOX (grid), box, -1);

      g_free (resource_name);
    }

  g_strfreev (resources);

  gtk_label_set_label (GTK_LABEL (label), "Images");

  gtk_accessible_update_relation (GTK_ACCESSIBLE (grid),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  return sw;
}

static GtkWidget *
display_text (const char *format,
              const char *resource,
              GtkWidget  *label)
{
  GtkTextBuffer *buffer;
  GtkWidget *textview, *sw;
  GBytes *bytes;
  const char *text;
  gsize len;

  bytes = g_resources_lookup_data (resource, 0, NULL);
  g_assert (bytes);
  text = g_bytes_get_data (bytes, &len);
  g_assert (g_utf8_validate (text, len, NULL));

  textview = gtk_text_view_new ();
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 20);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 20);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (textview), 20);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (textview), 20);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
  /* Make it a bit nicer for text. */
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
  gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (textview), 2);
  gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (textview), 2);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (textview), TRUE);

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, text, len);
  g_bytes_unref (bytes);

  if (format)
    fontify (format, buffer);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (textview),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), textview);

  return sw;
}

static GtkWidget *
display_video (const char *format,
               const char *resource,
               GtkWidget  *label)
{
  GtkWidget *video;

  video = gtk_video_new_for_resource (resource);
  gtk_video_set_loop (GTK_VIDEO (video), TRUE);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (video),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  return video;
}

static GtkWidget *
display_nothing (const char *resource)
{
  GtkWidget *widget;
  char *str;

  str = g_strdup_printf ("The contents of the resource at '%s' cannot be displayed", resource);
  widget = gtk_label_new (str);
  gtk_label_set_wrap (GTK_LABEL (widget), TRUE);

  g_free (str);

  return widget;
}

static struct {
  const char *extension;
  const char *format;
  GtkWidget * (* display_func) (const char *format,
                                const char *resource,
                                GtkWidget  *label);
} display_funcs[] = {
  { ".gif", NULL, display_image },
  { ".jpg", NULL, display_image },
  { ".png", NULL, display_image },
  { ".svg", NULL, display_image },
  { ".c", "c", display_text },
  { ".css", "css", display_text },
  { ".glsl", NULL, display_text },
  { ".h", "c", display_text },
  { ".txt", NULL, display_text },
  { ".ui", "xml", display_text },
  { ".webm", NULL, display_video },
  { "images/", NULL, display_images }
};

static void
add_data_tab (const char *demoname)
{
  char *resource_dir, *resource_name;
  char **resources;
  GtkWidget *widget, *label;
  guint i, j;

  resource_dir = g_strconcat ("/", demoname, NULL);
  resources = g_resources_enumerate_children (resource_dir, 0, NULL);
  if (resources == NULL)
    {
      g_free (resource_dir);
      return;
    }

  for (i = 0; resources[i]; i++)
    {
      resource_name = g_strconcat (resource_dir, "/", resources[i], NULL);

      for (j = 0; j < G_N_ELEMENTS (display_funcs); j++)
        {
          if (g_str_has_suffix (resource_name, display_funcs[j].extension))
            break;
        }

      label = gtk_label_new (resources[i]);

      if (j < G_N_ELEMENTS (display_funcs))
        widget = display_funcs[j].display_func (display_funcs[j].format,
                                                resource_name,
                                                label);
      else
        widget = display_nothing (resource_name);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);
      g_object_set (gtk_notebook_get_page (GTK_NOTEBOOK (notebook), widget),
                    "tab-expand", FALSE,
                    NULL);

      g_free (resource_name);
    }

  g_strfreev (resources);
  g_free (resource_dir);
}

static void
remove_data_tabs (void)
{
  int i;

  for (i = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) - 1; i > 1; i--)
    gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), i);
}

void
load_file (const char *demoname,
           const char *filename)
{
  GtkTextBuffer *info_buffer, *source_buffer;
  GtkTextIter start, end;
  char *resource_filename;
  GError *err = NULL;
  int state = 0;
  gboolean in_para = 0;
  char **lines;
  GBytes *bytes;
  int i;

  if (!g_strcmp0 (current_file, filename))
    return;

  remove_data_tabs ();

  add_data_tab (demoname);

  g_free (current_file);
  current_file = g_strdup (filename);

  info_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_create_tag (info_buffer, "title",
                              "size", 18 * 1024,
                              "pixels-below-lines", 10,
                              NULL);

  source_buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_begin_irreversible_action (info_buffer);
  gtk_text_buffer_begin_irreversible_action (source_buffer);

  resource_filename = g_strconcat ("/sources/", filename, NULL);
  bytes = g_resources_lookup_data (resource_filename, 0, &err);
  g_free (resource_filename);

  if (bytes == NULL)
    {
      g_warning ("Cannot open source for %s: %s", filename, err->message);
      g_error_free (err);
      return;
    }

  lines = g_strsplit (g_bytes_get_data (bytes, NULL), "\n", -1);
  g_bytes_unref (bytes);

  gtk_text_buffer_get_iter_at_offset (info_buffer, &start, 0);
  for (i = 0; lines[i] != NULL; i++)
    {
      char *p;
      char *q;
      char *r;

      /* Make sure \r is stripped at the end for the poor windows people */
      lines[i] = g_strchomp (lines[i]);

      p = lines[i];
      switch (state)
        {
        case 0:
          /* Reading title */
          while (*p == '/' || *p == '*' || g_ascii_isspace (*p))
            p++;
          r = p;
          while (*r != '\0')
            {
              while (*r != '/' && *r != ':' && *r != '\0')
                r++;
              if (*r == '/')
                {
                  r++;
                  p = r;
                }
              if (r[0] == ':' && r[1] == ':')
                *r = '\0';
            }
          q = p + strlen (p);
          while (q > p && g_ascii_isspace (*(q - 1)))
            q--;


          if (q > p)
            {
              int len_chars = g_utf8_pointer_to_offset (p, q);

              end = start;

              g_assert (strlen (p) >= q - p);
              gtk_text_buffer_insert (info_buffer, &end, p, q - p);
              start = end;

              gtk_text_iter_backward_chars (&start, len_chars);
              gtk_text_buffer_apply_tag_by_name (info_buffer, "title", &start, &end);

              start = end;

              while (*p && *p != '\n') p++;

              state++;
            }
          break;

        case 1:
          /* Reading body of info section */
          while (g_ascii_isspace (*p))
            p++;
          if (*p == '*' && *(p + 1) == '/')
            {
              gtk_text_buffer_get_iter_at_offset (source_buffer, &start, 0);
              state++;
            }
          else
            {
              int len;

              while (*p == '*' || g_ascii_isspace (*p))
                p++;

              len = strlen (p);
              while (g_ascii_isspace (*(p + len - 1)))
                len--;

              if (*p == '#')
                break;

              if (len > 0)
                {
                  if (in_para)
                    gtk_text_buffer_insert (info_buffer, &start, " ", 1);

                  g_assert (strlen (p) >= len);
                  gtk_text_buffer_insert (info_buffer, &start, p, len);
                  in_para = 1;
                }
              else
                {
                  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
                  in_para = 0;
                }
            }
          break;

        case 2:
          /* Skipping blank lines */
          while (g_ascii_isspace (*p))
            p++;

          if (!*p)
            break;

          p = lines[i];
          state++;
          G_GNUC_FALLTHROUGH;

        case 3:
          /* Reading program body */
          gtk_text_buffer_insert (source_buffer, &start, p, -1);
          if (lines[i+1] != NULL)
            gtk_text_buffer_insert (source_buffer, &start, "\n", 1);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  g_strfreev (lines);

  fontify ("c", source_buffer);

  gtk_text_buffer_end_irreversible_action (source_buffer);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), source_buffer);
  g_object_unref (source_buffer);

  gtk_text_buffer_end_irreversible_action (info_buffer);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (info_view), info_buffer);
  g_object_unref (info_buffer);
}

static void
activate_cb (GtkWidget *widget,
             guint      position,
             gpointer   window)
{
  GListModel *model = G_LIST_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (widget)));
  GtkTreeListRow *row = g_list_model_get_item (model, position);
  GtkDemo *demo = gtk_tree_list_row_get_item (row);

  gtk_demo_run (demo, window);

  g_object_unref (row);
}

static void
selection_cb (GtkSingleSelection *sel,
              GParamSpec         *pspec,
              gpointer            user_data)
{
  GtkTreeListRow *row = gtk_single_selection_get_selected_item (sel);
  GtkDemo *demo;
  GAction *action;

  gtk_widget_set_sensitive (GTK_WIDGET (notebook), !!row);

  if (!row)
    {
      gtk_window_set_title (GTK_WINDOW (toplevel), "No match");

      return;
    }

  demo = gtk_tree_list_row_get_item (row);

  if (demo->filename)
    load_file (demo->name, demo->filename);

  action = g_action_map_lookup_action (G_ACTION_MAP (toplevel), "run");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), demo->func != NULL);

  gtk_window_set_title (GTK_WINDOW (toplevel), demo->title);
}

static gboolean
filter_demo (GtkDemo *demo)
{
  int i;

  /* Show only if the name matches every needle */
  for (i = 0; search_needle[i]; i++)
    {
      if (!demo->title)
        return FALSE;

      if (g_str_match_string (search_needle[i], demo->title, TRUE))
        continue;

      if (demo->keywords)
        {
          int j;
          gboolean found = FALSE;

          for (j = 0; !found && demo->keywords[j]; j++)
            {
              if (strstr (demo->keywords[j], search_needle[i]))
                found = TRUE;
            }

          if (found)
            continue;
        }

      return FALSE;
    }

  return TRUE;
}

static gboolean
demo_filter_by_name (gpointer item,
                     gpointer user_data)
{
  GtkTreeListRow *row = item;
  GListModel *children;
  GtkDemo *demo;
  guint i, n;
  GtkTreeListRow *parent;

  /* Show all items if search is empty */
  if (!search_needle || !search_needle[0] || !*search_needle[0])
    return TRUE;

  g_assert (GTK_IS_TREE_LIST_ROW (row));
  g_assert (GTK_IS_FILTER_LIST_MODEL (user_data));

  /* Show a row if itself of any parent matches */
  for (parent = row; parent; parent = gtk_tree_list_row_get_parent (parent))
    {
      demo = gtk_tree_list_row_get_item (parent);
      g_assert (GTK_IS_DEMO (demo));

      if (filter_demo (demo))
        return TRUE;
    }

  /* Show a row if any child matches */
  children = gtk_tree_list_row_get_children (row);
  if (children)
    {
      n = g_list_model_get_n_items (children);
      for (i = 0; i < n; i++)
        {
          demo = g_list_model_get_item (children, i);
          g_assert (GTK_IS_DEMO (demo));

          if (filter_demo (demo))
            {
              g_object_unref (demo);
              return TRUE;
            }
          g_object_unref (demo);
        }
    }

  return FALSE;
}

static void
demo_search_changed_cb (GtkSearchEntry *entry,
                        GtkFilter      *filter)
{
  const char *text;

  g_assert (GTK_IS_SEARCH_ENTRY (entry));
  g_assert (GTK_IS_FILTER (filter));

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

  g_clear_pointer (&search_needle, g_strfreev);

  if (text && *text)
    search_needle = g_str_tokenize_and_fold (text, NULL, NULL);

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
demo_can_run (GtkWidget  *window,
              const char *name)
{
  if (name != NULL && strcmp (name, "gltransition") == 0)
    return GSK_IS_GL_RENDERER (gtk_native_get_renderer (GTK_NATIVE (window)));

  return TRUE;
}

static GListModel *
create_demo_model (GtkWidget *window)
{
  GListStore *store = g_list_store_new (GTK_TYPE_DEMO);
  DemoData *demo = gtk_demos;
  GtkDemo *d;

  gtk_widget_realize (window);

  d = GTK_DEMO (g_object_new (GTK_TYPE_DEMO, NULL));
  d->name = "main";
  d->title = "GTK Demo";
  d->keywords = NULL;
  d->filename = "main.c";
  d->func = NULL;

  g_list_store_append (store, d);

  while (demo->title)
    {
       DemoData *children = demo->children;

      if (demo_can_run (window, demo->name))
        {
          d = GTK_DEMO (g_object_new (GTK_TYPE_DEMO, NULL));

          d->name = demo->name;
          d->title = demo->title;
          d->keywords = demo->keywords;
          d->filename = demo->filename;
          d->func = demo->func;

          g_list_store_append (store, d);
        }

      if (children)
        {
          d->children_model = G_LIST_MODEL (g_list_store_new (GTK_TYPE_DEMO));

          while (children->title)
            {
              if (demo_can_run (window, children->name))
                {
                  GtkDemo *child = GTK_DEMO (g_object_new (GTK_TYPE_DEMO, NULL));

                  child->name = children->name;
                  child->title = children->title;
                  child->keywords = children->keywords;
                  child->filename = children->filename;
                  child->func = children->func;

                  g_list_store_append (G_LIST_STORE (d->children_model), child);
                }

              children++;
            }
        }

      demo++;
    }

  return G_LIST_MODEL (store);
}

static GListModel *
get_child_model (gpointer item,
                 gpointer user_data)
{
  GtkDemo *demo = item;

  if (demo->children_model)
    return g_object_ref (G_LIST_MODEL (demo->children_model));

  return NULL;
}

static void
clear_search (GtkSearchBar *bar)
{
  if (!gtk_search_bar_get_search_mode (bar))
    {
      GtkWidget *entry = gtk_search_bar_get_child (GTK_SEARCH_BAR (bar));
      gtk_editable_set_text (GTK_EDITABLE (entry), "");
    }
}

static void
search_results_update (GObject    *filter_model,
                       GParamSpec *pspec,
                       GtkEntry   *entry)
{
  gsize n_items = g_list_model_get_n_items (G_LIST_MODEL (filter_model));

  if (strlen (gtk_editable_get_text (GTK_EDITABLE (entry))) > 0)
    {
      char *text;

      if (n_items > 0)
        text = g_strdup_printf (ngettext ("%ld search result", "%ld search results", n_items), n_items);
      else
        text = g_strdup (_("No search results"));

      gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                      GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, text,
                                      -1);

      g_free (text);
    }
  else
    {
      gtk_accessible_reset_property (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);
    }
}

static void
activate (GApplication *app)
{
  GtkBuilder *builder;
  GListModel *listmodel;
  GtkTreeListModel *treemodel;
  GtkWidget *window, *listview, *search_entry, *search_bar;
  GtkFilterListModel *filter_model;
  GtkFilter *filter;
  GSimpleAction *action;

  builder = gtk_builder_new_from_resource ("/ui/main.ui");

  window = (GtkWidget *)gtk_builder_get_object (builder, "window");
  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (window));

  if (g_strcmp0 (PROFILE, "devel") == 0)
    gtk_widget_add_css_class (window, "devel");

  action = g_simple_action_new ("run", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (activate_run), window);
  g_action_map_add_action (G_ACTION_MAP (window), G_ACTION (action));

  notebook = GTK_WIDGET (gtk_builder_get_object (builder, "notebook"));

  info_view = GTK_WIDGET (gtk_builder_get_object (builder, "info-textview"));
  source_view = GTK_WIDGET (gtk_builder_get_object (builder, "source-textview"));
  toplevel = GTK_WIDGET (window);
  listview = GTK_WIDGET (gtk_builder_get_object (builder, "listview"));
  g_signal_connect (listview, "activate", G_CALLBACK (activate_cb), window);
  search_bar = GTK_WIDGET (gtk_builder_get_object (builder, "searchbar"));
  g_signal_connect (search_bar, "notify::search-mode-enabled", G_CALLBACK (clear_search), NULL);

  listmodel = create_demo_model (window);
  treemodel = gtk_tree_list_model_new (G_LIST_MODEL (listmodel),
                                       FALSE,
                                       TRUE,
                                       get_child_model,
                                       NULL,
                                       NULL);
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (treemodel), NULL);
  filter = GTK_FILTER (gtk_custom_filter_new (demo_filter_by_name, filter_model, NULL));
  gtk_filter_list_model_set_filter (filter_model, filter);
  g_object_unref (filter);

  search_entry = GTK_WIDGET (gtk_builder_get_object (builder, "search-entry"));
  g_signal_connect (search_entry, "search-changed", G_CALLBACK (demo_search_changed_cb), filter);
  g_signal_connect (filter_model, "notify::n-items", G_CALLBACK (search_results_update), search_entry);

  selection = gtk_single_selection_new (G_LIST_MODEL (filter_model));
  g_signal_connect (selection, "notify::selected-item", G_CALLBACK (selection_cb), NULL);
  gtk_list_view_set_model (GTK_LIST_VIEW (listview), GTK_SELECTION_MODEL (selection));

  selection_cb (selection, NULL, NULL);
  g_object_unref (selection);

  g_object_unref (builder);
}

static gboolean
auto_quit (gpointer data)
{
  g_application_quit (G_APPLICATION (data));
  return G_SOURCE_REMOVE;
}

static void
list_demos (void)
{
  DemoData *d, *c;

  d = gtk_demos;

  while (d->title)
    {
      c = d->children;
      if (d->name)
        g_print ("%s\n", d->name);
      d++;
      while (c && c->title)
        {
          if (c->name)
            g_print ("%s\n", c->name);
          c++;
        }
    }
}

static int
command_line (GApplication            *app,
              GApplicationCommandLine *cmdline)
{
  GVariantDict *options;
  const char *name = NULL;
  gboolean autoquit = FALSE;
  gboolean list = FALSE;
  DemoData *d, *c;
  GDoDemoFunc func = 0;
  GtkWidget *window, *demo;

  activate (app);

  options = g_application_command_line_get_options_dict (cmdline);
  g_variant_dict_lookup (options, "run", "&s", &name);
  g_variant_dict_lookup (options, "autoquit", "b", &autoquit);
  g_variant_dict_lookup (options, "list", "b", &list);

  if (list)
    {
      list_demos ();
      g_application_quit (app);
      return 0;
    }

  window = gtk_application_get_windows (GTK_APPLICATION (app))->data;

  if (name == NULL)
    goto out;

  d = gtk_demos;

  while (d->title)
    {
      c = d->children;
      if (g_strcmp0 (d->name, name) == 0)
        {
          func = d->func;
          goto out;
        }
      d++;
      while (c && c->title)
        {
          if (g_strcmp0 (c->name, name) == 0)
            {
              func = c->func;
              goto out;
            }
          c++;
        }
    }

out:
  if (func)
    {
      demo = (func) (window);

      gtk_window_set_transient_for (GTK_WINDOW (demo), GTK_WINDOW (window));

      g_signal_connect_swapped (G_OBJECT (demo), "destroy", G_CALLBACK (g_application_quit), app);
    }
  else
    gtk_window_present (GTK_WINDOW (window));

  if (autoquit)
    g_timeout_add_seconds (1, auto_quit, app);

  return 0;
}

static void
print_version (void)
{
  g_print ("gtk4-demo %s%s%s\n",
           PACKAGE_VERSION,
           g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
           g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "");
}

static int
local_options (GApplication *app,
               GVariantDict *options,
               gpointer      data)
{
  gboolean version = FALSE;

  g_variant_dict_lookup (options, "version", "b", &version);

  if (version)
    {
      print_version ();
      return 0;
    }

  return -1;
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  static GActionEntry app_entries[] = {
    { "about", activate_about, NULL, NULL, NULL },
    { "quit", activate_quit, NULL, NULL, NULL },
    { "inspector", activate_inspector, NULL, NULL, NULL },
  };
  struct {
    const char *action_and_target;
    const char *accelerators[2];
  } accels[] = {
    { "app.about", { "F1", NULL } },
    { "app.quit", { "<Control>q", NULL } },
  };
  int i;

  app = gtk_application_new ("org.gtk.Demo4", G_APPLICATION_NON_UNIQUE|G_APPLICATION_HANDLES_COMMAND_LINE);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  for (i = 0; i < G_N_ELEMENTS (accels); i++)
    gtk_application_set_accels_for_action (app, accels[i].action_and_target, accels[i].accelerators);

  g_application_add_main_option (G_APPLICATION (app), "version", 0, 0, G_OPTION_ARG_NONE, "Show program version", NULL);
  g_application_add_main_option (G_APPLICATION (app), "run", 0, 0, G_OPTION_ARG_STRING, "Run an example", "EXAMPLE");
  g_application_add_main_option (G_APPLICATION (app), "list", 0, 0, G_OPTION_ARG_NONE, "List examples", NULL);
  g_application_add_main_option (G_APPLICATION (app), "autoquit", 0, 0, G_OPTION_ARG_NONE, "Quit after a delay", NULL);

  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
  g_signal_connect (app, "handle-local-options", G_CALLBACK (local_options), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
