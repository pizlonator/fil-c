/* Pango/Font Explorer
 *
 * This example demonstrates support for OpenType font features with
 * Pango attributes. The attributes can be used manually or via Pango
 * markup.
 *
 * It can also be used to explore available features in OpenType fonts
 * and their effect.
 *
 * If the selected font supports OpenType font variations, then the
 * axes are also offered for customization.
 */

#include <gtk/gtk.h>
#include <hb.h>
#include <hb-ot.h>
#include <glib/gi18n.h>

#include "open-type-layout.h"
#include "fontplane.h"
#include "script-names.h"
#include "language-names.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* {{{ ScriptLang object */

G_DECLARE_FINAL_TYPE (ScriptLang, script_lang, SCRIPT, LANG, GObject)

struct _ScriptLang
{
  GObject parent;

  char *langname;
  unsigned int script_index;
  unsigned int lang_index;
  hb_tag_t lang_tag;
};

struct _ScriptLangClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (ScriptLang, script_lang, G_TYPE_OBJECT)

static void
script_lang_init (ScriptLang *self)
{
}

static void
script_lang_finalize (GObject *object)
{
  ScriptLang *self = SCRIPT_LANG (object);

  g_free (self->langname);

  G_OBJECT_CLASS (script_lang_parent_class)->finalize (object);
}

static void
script_lang_class_init (ScriptLangClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = script_lang_finalize;
}

static ScriptLang *
script_lang_new (const char   *langname,
                 unsigned int  script_index,
                 unsigned int  lang_index,
                 hb_tag_t      lang_tag)
{
  ScriptLang *self;

  self = g_object_new (script_lang_get_type (), NULL);

  self->langname = g_strdup (langname);
  self->script_index = script_index;
  self->lang_index = lang_index;
  self->lang_tag = lang_tag;

  return self;
}

static char *
script_lang_get_langname (ScriptLang *self)
{
  return g_strdup (self->langname);
}

/* }}} */

#define MAKE_TAG(a,b,c,d) (unsigned int)(((a) << 24) | ((b) << 16) | ((c) <<  8) | (d))

typedef struct {
  unsigned int tag;
  const char *name;
  GtkWidget *icon;
  GtkWidget *dflt;
  GtkWidget *feat;
} FeatureItem;

typedef struct {
  unsigned int start;
  unsigned int end;
  PangoFontDescription *desc;
  char *features;
  PangoLanguage *language;
} Range;

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
  double default_value;
  guint tick_cb;
  guint64 start_time;
  gboolean increasing;
  GtkWidget *button;
} Axis;

typedef struct {
  GtkWidget *the_label;
  GtkWidget *settings;
  GtkWidget *description;
  GtkWidget *font;
  GtkWidget *script_lang;
  GtkWidget *feature_list;
  GtkWidget *variations_grid;
  GtkWidget *instance_combo;
  GtkWidget *stack;
  GtkWidget *entry;
  GtkWidget *plain_toggle;
  GtkWidget *waterfall_toggle;
  GtkWidget *edit_toggle;
  GtkAdjustment *size_adjustment;
  GtkAdjustment *letterspacing_adjustment;
  GtkAdjustment *line_height_adjustment;
  GtkWidget *foreground;
  GtkWidget *background;
  GtkWidget *size_scale;
  GtkWidget *size_entry;
  GtkWidget *letterspacing_entry;
  GtkWidget *line_height_entry;
  GList *feature_items;
  GList *ranges;
  GHashTable *instances;
  GHashTable *axes;
  char *text;
  GtkWidget *swin;
  GtkCssProvider *provider;
  int sample;
} FontFeaturesDemo;

static void
demo_free (gpointer data)
{
  FontFeaturesDemo *demo = data;

  g_list_free_full (demo->feature_items, g_free);
  g_list_free_full (demo->ranges, g_free);
  g_clear_pointer (&demo->instances, g_hash_table_unref);
  g_clear_pointer (&demo->axes, g_hash_table_unref);
  g_clear_pointer (&demo->text, g_free);

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                 GTK_STYLE_PROVIDER (demo->provider));
  g_object_unref (demo->provider);

  g_free (demo);
}

static FontFeaturesDemo *demo;

static void update_display (void);

static void
font_features_toggle_plain (void)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (demo->plain_toggle)) ||
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (demo->waterfall_toggle)))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (demo->stack), "label");
      update_display ();
    }
}

static void
font_features_notify_waterfall (void)
{
  gboolean can_change_size;

  can_change_size = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (demo->waterfall_toggle));
  gtk_widget_set_sensitive (demo->size_scale, can_change_size);
  gtk_widget_set_sensitive (demo->size_entry, can_change_size);
}

typedef struct {
  GtkAdjustment *adjustment;
  GtkEntry *entry;
} BasicData;

static gboolean
update_in_idle (gpointer data)
{
  BasicData *bd = data;
  char *str;

  str = g_strdup_printf ("%g", gtk_adjustment_get_value (bd->adjustment));
  gtk_editable_set_text (GTK_EDITABLE (bd->entry), str);
  g_free (str);

  update_display ();

  g_free (bd);

  return G_SOURCE_REMOVE;
}

static void
basic_value_changed (GtkAdjustment *adjustment,
                     gpointer       data)
{
  BasicData *bd;

  bd = g_new (BasicData, 1);
  bd->adjustment = adjustment;
  bd->entry = GTK_ENTRY (data);

  g_idle_add (update_in_idle, bd);
}

static void
basic_entry_activated (GtkEntry *entry,
                       gpointer  data)
{
  GtkAdjustment *adjustment = data;

  double value;
  char *err = NULL;

  value = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    gtk_adjustment_set_value (adjustment, value);
}

static void
color_set_cb (void)
{
  update_display ();
}

static void
swap_colors (void)
{
  GdkRGBA fg;
  GdkRGBA bg;

  fg = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (demo->foreground));
  bg = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (demo->background));
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (demo->foreground), &bg);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (demo->background), &fg);
}

static void
font_features_reset_basic (void)
{
  gtk_adjustment_set_value (demo->size_adjustment, 20);
  gtk_adjustment_set_value (demo->letterspacing_adjustment, 0);
  gtk_adjustment_set_value (demo->line_height_adjustment, 1);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (demo->foreground), &(GdkRGBA){0.,0.,0.,1.});
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (demo->background), &(GdkRGBA){1.,1.,1.,1.});
}

static void
update_basic (void)
{
  PangoFontDescription *desc;

  desc = gtk_font_dialog_button_get_font_desc (GTK_FONT_DIALOG_BUTTON (demo->font));

  gtk_adjustment_set_value (demo->size_adjustment,
                            pango_font_description_get_size (desc) / (double) PANGO_SCALE);
}

static void add_font_variations (GString *s);

static void
free_range (gpointer data)
{
  Range *range = data;

  if (range->desc)
    pango_font_description_free (range->desc);
  g_free (range->features);
  g_free (range);
}

static int
compare_range (gconstpointer a, gconstpointer b)
{
  const Range *ra = a;
  const Range *rb = b;

  if (ra->start < rb->start)
    return -1;
  else if (ra->start > rb->start)
    return 1;
  else if (ra->end < rb->end)
    return 1;
  else if (ra->end > rb->end)
    return -1;

  return 0;
}

static void
ensure_range (unsigned int          start,
              unsigned int          end,
              PangoFontDescription *desc,
              const char           *features,
              PangoLanguage        *language)
{
  GList *l;
  Range *range;

  for (l = demo->ranges; l; l = l->next)
    {
      Range *r = l->data;

      if (r->start == start && r->end == end)
        {
          range = r;
          goto set;
        }
    }

  range = g_new0 (Range, 1);
  range->start = start;
  range->end = end;

  demo->ranges = g_list_insert_sorted (demo->ranges, range, compare_range);

set:
  if (range->desc)
    pango_font_description_free (range->desc);
  if (desc)
    range->desc = pango_font_description_copy (desc);
  g_free (range->features);
  range->features = g_strdup (features);
  range->language = language;
}

static char *
get_feature_display_name (unsigned int tag)
{
  int i;
  static char buf[5] = { 0, };

  if (tag == MAKE_TAG ('x', 'x', 'x', 'x'))
    return g_strdup (_("Default"));

  hb_tag_to_string (tag, buf);
  if (g_str_has_prefix (buf, "ss") && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Stylistic Set %d"), num);
    }
  else if (g_str_has_prefix (buf, "cv") && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Character Variant %d"), num);
    }

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_strdup (g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name));
    }

  g_warning ("unknown OpenType layout feature tag: %s", buf);

  return g_strdup (buf);
}

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), inconsistent);
  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
feat_pressed (GtkGestureClick *gesture,
              int              n_press,
              double           x,
              double           y,
              GtkWidget       *feat)
{
  const guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      g_signal_handlers_block_by_func (feat, feat_pressed, NULL);

      if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat)))
        {
          set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
          gtk_check_button_set_active (GTK_CHECK_BUTTON (feat), TRUE);
        }

      g_signal_handlers_unblock_by_func (feat, feat_pressed, NULL);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      gboolean inconsistent = gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (feat));
      set_inconsistent (GTK_CHECK_BUTTON (feat), !inconsistent);
    }
}

static void
feat_toggled_cb (GtkCheckButton *check_button,
                 gpointer        data)
{
  set_inconsistent (check_button, FALSE);
}

static void
add_check_group (GtkWidget   *box,
                 const char  *title,
                 const char **tags)
{
  GtkWidget *label;
  GtkWidget *group;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  gtk_widget_add_css_class (label, "heading");
  gtk_box_append (GTK_BOX (group), label);

  for (i = 0; tags[i]; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkGesture *gesture;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);

      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name);
      g_free (name);
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);

      g_signal_connect (feat, "notify::active", G_CALLBACK (update_display), NULL);
      g_signal_connect (feat, "notify::inconsistent", G_CALLBACK (update_display), NULL);
      g_signal_connect (feat, "toggled", G_CALLBACK (feat_toggled_cb), NULL);

      gesture = gtk_gesture_click_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_signal_connect (gesture, "pressed", G_CALLBACK (feat_pressed), feat);
      gtk_widget_add_controller (feat, GTK_EVENT_CONTROLLER (gesture));

      gtk_box_append (GTK_BOX (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->icon = NULL;
      item->dflt = NULL;
      item->feat = feat;

      demo->feature_items = g_list_prepend (demo->feature_items, item);
    }

  gtk_box_append (GTK_BOX (box), group);
}

static void
add_radio_group (GtkWidget *box,
                 const char  *title,
                 const char **tags)
{
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_START);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  gtk_widget_add_css_class (label, "heading");
  gtk_box_append (GTK_BOX (group), label);

  for (i = 0; tags[i]; i++)
    {
      unsigned int tag;
      GtkWidget *feat;
      FeatureItem *item;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name ? name : _("Default"));
      g_free (name);
      if (group_button == NULL)
        group_button = feat;
      else
        gtk_check_button_set_group (GTK_CHECK_BUTTON (feat), GTK_CHECK_BUTTON (group_button));

      g_signal_connect (feat, "notify::active", G_CALLBACK (update_display), NULL);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      gtk_box_append (GTK_BOX (group), feat);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->icon = NULL;
      item->dflt = NULL;
      item->feat = feat;

      demo->feature_items = g_list_prepend (demo->feature_items, item);
    }

  gtk_box_append (GTK_BOX (box), group);
}

static void
update_display (void)
{
  GString *s;
  char *text;
  gboolean has_feature;
  PangoFontDescription *desc;
  GList *l;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  int ins, bound;
  guint start, end;
  PangoLanguage *lang;
  char *font_desc;
  char *features;
  double value;
  int text_len;
  gboolean do_waterfall;
  GString *waterfall;

  {
    GtkTextBuffer *buffer;
    GtkTextIter start_iter, end_iter;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (demo->entry));
    gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    text = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
    text_len = strlen (text);
  }

  do_waterfall = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (demo->waterfall_toggle));

  gtk_label_set_wrap (GTK_LABEL (demo->the_label), !do_waterfall);

  if (do_waterfall)
    {
      start = PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING;
      end = PANGO_ATTR_INDEX_TO_TEXT_END;
    }
  else if (gtk_label_get_selection_bounds (GTK_LABEL (demo->the_label), &ins, &bound))
    {
      start = g_utf8_offset_to_pointer (text, ins) - text;
      end = g_utf8_offset_to_pointer (text, bound) - text;
    }
  else
    {
      start = PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING;
      end = PANGO_ATTR_INDEX_TO_TEXT_END;
    }

  desc = gtk_font_dialog_button_get_font_desc (GTK_FONT_DIALOG_BUTTON (demo->font));

  value = gtk_adjustment_get_value (demo->size_adjustment);
  pango_font_description_set_size (desc, value * PANGO_SCALE);

  s = g_string_new ("");
  add_font_variations (s);
  if (s->len > 0)
    {
      pango_font_description_set_variations (desc, s->str);
    }
  g_string_free (s, TRUE);

  font_desc = pango_font_description_to_string (desc);

  s = g_string_new ("");

  has_feature = FALSE;
  for (l = demo->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (g_object_get_data (G_OBJECT (item->feat), "default"))
            {
              if (gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat)) &&
                  strcmp (item->name, "xxxx") != 0)
                {
                  if (has_feature)
                    g_string_append (s, ", ");
                  g_string_append (s, item->name);
                  g_string_append (s, " 1");
                  has_feature = TRUE;
                }
            }
          else
            {
              if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (item->feat)))
                continue;

              if (has_feature)
                g_string_append (s, ", ");
              g_string_append (s, item->name);
              if (gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat)))
                g_string_append (s, " 1");
              else
                g_string_append (s, " 0");
              has_feature = TRUE;
            }
        }
    }

  features = g_string_free (s, FALSE);

  if (gtk_drop_down_get_selected (GTK_DROP_DOWN (demo->script_lang)) != 0)
    {
      ScriptLang *selected;

      selected = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (demo->script_lang));

      lang = pango_language_from_string (hb_language_to_string (hb_ot_tag_to_language (selected->lang_tag)));
    }
  else
    lang = NULL;

  attrs = pango_attr_list_new ();

  if (gtk_adjustment_get_value (demo->letterspacing_adjustment) != 0.)
    {
      attr = pango_attr_letter_spacing_new (gtk_adjustment_get_value (demo->letterspacing_adjustment));
      attr->start_index = start;
      attr->end_index = end;
      pango_attr_list_insert (attrs, attr);
    }

  if (gtk_adjustment_get_value (demo->line_height_adjustment) != 1.)
    {
      attr = pango_attr_line_height_new (gtk_adjustment_get_value (demo->line_height_adjustment));
      attr->start_index = start;
      attr->end_index = end;
      pango_attr_list_insert (attrs, attr);
    }

    {
      GdkRGBA rgba;
      char *fg, *bg, *css;

      rgba = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (demo->foreground));
      attr = pango_attr_foreground_new (65535 * rgba.red,
                                        65535 * rgba.green,
                                        65535 * rgba.blue);
      attr->start_index = start;
      attr->end_index = end;
      pango_attr_list_insert (attrs, attr);
      attr = pango_attr_foreground_alpha_new (65535 * rgba.alpha);
      attr->start_index = start;
      attr->end_index = end;
      pango_attr_list_insert (attrs, attr);

      fg = gdk_rgba_to_string (&rgba);
      rgba = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (demo->background));
      bg = gdk_rgba_to_string (&rgba);
      css = g_strdup_printf (".font_features_background { caret-color: %s; background-color: %s; }", fg, bg);
      gtk_css_provider_load_from_data (demo->provider, css, strlen (css));
      g_free (css);
      g_free (fg);
      g_free (bg);
    }

  if (do_waterfall)
    {
      attr = pango_attr_font_desc_new (desc);
      pango_attr_list_insert (attrs, attr);
      attr = pango_attr_font_features_new (features);
      pango_attr_list_insert (attrs, attr);
      attr = pango_attr_language_new (lang);
      pango_attr_list_insert (attrs, attr);
    }
  else
    {
      ensure_range (start, end, desc, features, lang);

      for (l = demo->ranges; l; l = l->next)
        {
          Range *range = l->data;

          attr = pango_attr_font_desc_new (range->desc);
          attr->start_index = range->start;
          attr->end_index = range->end;
          pango_attr_list_insert (attrs, attr);

          attr = pango_attr_font_features_new (range->features);
          attr->start_index = range->start;
          attr->end_index = range->end;
          pango_attr_list_insert (attrs, attr);

          if (range->language)
            {
              attr = pango_attr_language_new (range->language);
              attr->start_index = range->start;
              attr->end_index = range->end;
              pango_attr_list_insert (attrs, attr);
            }
        }
    }

  gtk_label_set_text (GTK_LABEL (demo->description), font_desc);
  gtk_label_set_text (GTK_LABEL (demo->settings), features);

  if (do_waterfall)
    {
      waterfall = g_string_new ("");
      int sizes[] = { 7, 8, 9, 10, 12, 14, 16, 20, 24, 30, 40, 50, 60, 70, 90 };
      start = 0;
      for (int i = 0; i < G_N_ELEMENTS (sizes); i++)
        {
          g_string_append (waterfall, text);
          g_string_append_c (waterfall, '\n');

          attr = pango_attr_size_new (sizes[i] * PANGO_SCALE);
          attr->start_index = start;
          attr->end_index = start + text_len;
          pango_attr_list_insert (attrs, attr);

          start += text_len + 1;
        }
      gtk_label_set_text (GTK_LABEL (demo->the_label), waterfall->str);
      g_string_free (waterfall, TRUE);
    }
  else
    gtk_label_set_text (GTK_LABEL (demo->the_label), text);

  gtk_label_set_attributes (GTK_LABEL (demo->the_label), attrs);

  g_free (font_desc);
  g_free (features);
  pango_attr_list_unref (attrs);
  g_free (text);
}

static PangoFont *
get_pango_font (void)
{
  PangoFontDescription *desc;
  PangoContext *context;

  desc = gtk_font_dialog_button_get_font_desc (GTK_FONT_DIALOG_BUTTON (demo->font));
  context = gtk_widget_get_pango_context (demo->font);

  return pango_context_load_font (context, desc);
}

typedef struct {
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  unsigned int script_index;
  unsigned int lang_index;
} TagPair;

static guint
tag_pair_hash (gconstpointer data)
{
  const TagPair *pair = data;

  return pair->script_tag + pair->lang_tag;
}

static gboolean
tag_pair_equal (gconstpointer a, gconstpointer b)
{
  const TagPair *pair_a = a;
  const TagPair *pair_b = b;

  return pair_a->script_tag == pair_b->script_tag && pair_a->lang_tag == pair_b->lang_tag;
}


static GtkOrdering
script_sort (const void *item1,
             const void *item2,
             void       *data)
{
  ScriptLang *a = (ScriptLang *)item1;
  ScriptLang *b = (ScriptLang *)item2;

  return strcmp (a->langname, b->langname);
}

static void
update_script_combo (void)
{
  GListStore *store;
  GtkSortListModel *sortmodel;
  hb_font_t *hb_font;
  PangoFont *pango_font;
  GHashTable *tags;
  GHashTableIter iter;
  TagPair *pair;
  PangoLanguage *language;
  const char *lang;
  hb_tag_t active;

  language = gtk_font_dialog_button_get_language (GTK_FONT_DIALOG_BUTTON (demo->font));
  lang = pango_language_to_string (language);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  active = hb_ot_tag_from_language (hb_language_from_string (lang, -1));
  G_GNUC_END_IGNORE_DEPRECATIONS

  store = g_list_store_new (script_lang_get_type ());

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);

  tags = g_hash_table_new_full (tag_pair_hash, tag_pair_equal, g_free, NULL);

  pair = g_new (TagPair, 1);
  pair->script_tag = 0;
  pair->lang_tag = 0;
  g_hash_table_add (tags, pair);

  if (hb_font)
    {
      hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;

      hb_face = hb_font_get_face (hb_font);

      for (guint i = 0; i < 2; i++)
        {
          hb_tag_t scripts[80];
          unsigned int script_count = G_N_ELEMENTS (scripts);

          hb_ot_layout_table_get_script_tags (hb_face, tables[i], 0, &script_count, scripts);
          for (guint j = 0; j < script_count; j++)
            {
              hb_tag_t languages[80];
              unsigned int language_count = G_N_ELEMENTS (languages);

              hb_ot_layout_script_get_language_tags (hb_face, tables[i], j, 0, &language_count, languages);
              for (guint k = 0; k < language_count; k++)
                {
                  pair = g_new (TagPair, 1);
                  pair->script_tag = scripts[j];
                  pair->lang_tag = languages[k];
                  pair->script_index = j;
                  pair->lang_index = k;
                  g_hash_table_add (tags, pair);
                }
            }
        }
    }

  g_object_unref (pango_font);

  g_hash_table_iter_init (&iter, tags);
  while (g_hash_table_iter_next (&iter, (gpointer *)&pair, NULL))
    {
      const char *langname;
      char langbuf[5];

      if (pair->lang_tag == 0 && pair->script_tag == 0)
        langname = NC_("Language", "None");
      else if (pair->lang_tag == HB_OT_TAG_DEFAULT_LANGUAGE)
        langname = NC_("Language", "Default");
      else
        {
          langname = get_language_name_for_tag (pair->lang_tag);
          if (!langname)
            {
              hb_tag_to_string (pair->lang_tag, langbuf);
              langbuf[4] = 0;
              langname = langbuf;
            }
        }

      g_list_store_append (store, script_lang_new (langname,
                                                   pair->script_index,
                                                   pair->lang_index,
                                                   pair->lang_tag));
    }

  g_hash_table_destroy (tags);

  sortmodel = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                       GTK_SORTER (gtk_custom_sorter_new (script_sort, NULL, NULL)));
  gtk_drop_down_set_model (GTK_DROP_DOWN (demo->script_lang), G_LIST_MODEL (sortmodel));

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (sortmodel)); i++)
    {
      ScriptLang *item = g_list_model_get_item (G_LIST_MODEL (sortmodel), i);
      g_object_unref (item);

      if (item->lang_tag == active)
        {
          gtk_drop_down_set_selected (GTK_DROP_DOWN (demo->script_lang), i);
          break;
        }
    }

  g_object_unref (sortmodel);
}

static char *
get_name (hb_face_t       *hbface,
          hb_ot_name_id_t  id)
{
  unsigned int len;
  char *text;

  if (id == HB_OT_NAME_ID_INVALID)
    return NULL;

  len = hb_ot_name_get_utf8 (hbface, id, HB_LANGUAGE_INVALID, NULL, NULL);
  len++;
  text = g_new (char, len);
  hb_ot_name_get_utf8 (hbface, id, HB_LANGUAGE_INVALID, &len, text);

  return text;
}

static void
update_features (void)
{
  int i, j;
  PangoFont *pango_font;
  hb_font_t *hb_font;
  GList *l;
  ScriptLang *selected;

  /* set feature presence checks from the font features */

  selected = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (demo->script_lang));

  if (selected->lang_tag == 0) /* None is selected */
    {
      for (l = demo->feature_items; l; l = l->next)
        {
          FeatureItem *item = l->data;
          gtk_widget_set_visible (item->feat, TRUE);
          gtk_widget_set_visible (gtk_widget_get_parent (item->feat), TRUE);
          if (strcmp (item->name, "xxxx") == 0)
            gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), TRUE);
        }

      return;
    }

  for (l = demo->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      gtk_widget_set_visible (item->feat, FALSE);
      gtk_widget_set_visible (gtk_widget_get_parent (item->feat), FALSE);
      if (strcmp (item->name, "xxxx") == 0)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), TRUE);
    }

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);

  g_print ("language %s\n", selected->langname);

  if (hb_font)
    {
      hb_tag_t tables[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      const char *feat;

      hb_face = hb_font_get_face (hb_font);

      for (i = 0; i < 2; i++)
        {
          hb_tag_t features[80];
          unsigned int count = G_N_ELEMENTS(features);

          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  tables[i],
                                                  selected->script_index,
                                                  selected->lang_index,
                                                  0,
                                                  &count,
                                                  features);

          for (j = 0; j < count; j++)
            {
              char buf[5];
              hb_tag_to_string (features[j], buf);
              buf[4] = 0;

              if (g_str_has_prefix (buf, "ss") || g_str_has_prefix (buf, "cv"))
                {
                  unsigned int feature_index;
                  hb_ot_name_id_t label_id, tooltip_id, sample_id, first_param_id;
                  unsigned int num_params;

                  hb_ot_layout_language_find_feature (hb_face,
                                                      tables[i],
                                                      selected->script_index,
                                                      selected->lang_index,
                                                      features[j],
                                                      &feature_index);

                  if (hb_ot_layout_feature_get_name_ids (hb_face,
                                                         tables[i],
                                                         feature_index,
                                                         &label_id,
                                                         &tooltip_id,
                                                         &sample_id,
                                                         &num_params,
                                                         &first_param_id))
                    {
                      char *label = get_name (hb_face, label_id);

                      if (label)
                        {
                          for (l = demo->feature_items; l; l = l->next)
                            {
                              FeatureItem *item = l->data;

                              if (item->tag == features[j])
                                {
                                  gtk_check_button_set_label (GTK_CHECK_BUTTON (item->feat), label);
                                  break;
                                }
                            }
                        }

                      g_free (label);
                    }
                }

              for (l = demo->feature_items; l; l = l->next)
                {
                  FeatureItem *item = l->data;

                  if (item->tag == features[j])
                    {
                      gtk_widget_set_visible (item->feat, TRUE);
                      gtk_widget_set_visible (gtk_widget_get_parent (item->feat), TRUE);
                      if (GTK_IS_CHECK_BUTTON (item->feat))
                        {
                          GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
                          if (def)
                            {
                              gtk_widget_set_visible (def, TRUE);
                              gtk_widget_set_visible (gtk_widget_get_parent (def), TRUE);
                              gtk_check_button_set_active (GTK_CHECK_BUTTON (def), TRUE);
                            }
                          else
                            set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
                        }
                    }
                }
            }
        }

      feat = gtk_font_dialog_button_get_font_features (GTK_FONT_DIALOG_BUTTON (demo->font));
      if (feat)
        {
          for (l = demo->feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;
              char buf[5];
              char *p;

              hb_tag_to_string (item->tag, buf);
              buf[4] = 0;

              p = strstr (feat, buf);
              if (p)
                {
                  if (GTK_IS_CHECK_BUTTON (item->feat) && g_object_get_data (G_OBJECT (item->feat), "default"))
                    {
                      gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), p[6] == '1');
                    }
                  else if (GTK_IS_CHECK_BUTTON (item->feat))
                    {
                      set_inconsistent (GTK_CHECK_BUTTON (item->feat), FALSE);
                      gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), p[6] == '1');
                    }
                }
            }
        }
    }

  g_object_unref (pango_font);
}

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
adjustment_changed (GtkAdjustment *adjustment,
                    GtkEntry      *entry)
{
  char *str;

  str = g_strdup_printf ("%g", gtk_adjustment_get_value (adjustment));
  gtk_editable_set_text (GTK_EDITABLE (entry), str);
  g_free (str);

  update_display ();
}

static void
entry_activated (GtkEntry *entry,
                 GtkAdjustment *adjustment)
{
  double value;
  char *err = NULL;

  value = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    gtk_adjustment_set_value (adjustment, value);
}

static void unset_instance (GtkAdjustment *adjustment);

static void start_or_stop_axis_animation (GtkButton *button,
                                          gpointer   data);

static void
font_features_reset_variations (void)
{
  GHashTableIter iter;
  Axis *axis;

  g_hash_table_iter_init (&iter, demo->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      if (axis->tick_cb)
        start_or_stop_axis_animation (GTK_BUTTON (axis->button), axis);
      gtk_adjustment_set_value (axis->adjustment, axis->default_value);
    }
}

static void
add_font_variations (GString *s)
{
  GHashTableIter iter;
  Axis *axis;
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  const char *sep = "";

  g_hash_table_iter_init (&iter, demo->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      char tag[5];
      double value;

      hb_tag_to_string (axis->tag, tag);
      tag[4] = '\0';
      value = gtk_adjustment_get_value (axis->adjustment);

      g_string_append_printf (s, "%s%s=%s", sep, tag, g_ascii_dtostr (buf, sizeof (buf), value));
      sep = ",";
    }
}

static guint
axes_hash (gconstpointer v)
{
  const Axis *p = v;

  return p->tag;
}

static gboolean
axes_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *p1 = v1;
  const Axis *p2 = v2;

  return p1->tag == p2->tag;
}

static double
ease_out_cubic (double t)
{
  double p = t - 1;

  return p * p * p + 1;
}

static gboolean
animate_axis (GtkWidget     *widget,
              GdkFrameClock *frame_clock,
              gpointer       data)
{
  Axis *axis = data;
  guint64 now;
  double upper, lower, value;

  now = g_get_monotonic_time ();

  if (now >= axis->start_time + G_TIME_SPAN_SECOND)
    {
      axis->start_time += G_TIME_SPAN_SECOND;
      axis->increasing = !axis->increasing;
    }

  value = (now - axis->start_time) / (double) G_TIME_SPAN_SECOND;

  value = ease_out_cubic (value);

  lower = gtk_adjustment_get_lower (axis->adjustment);
  upper = gtk_adjustment_get_upper (axis->adjustment);

  if (axis->increasing)
    gtk_adjustment_set_value (axis->adjustment, lower + (upper - lower) * value);
  else
    gtk_adjustment_set_value (axis->adjustment, upper - (upper - lower) * value);

  return G_SOURCE_CONTINUE;
}

static void
start_or_stop_axis_animation (GtkButton *button,
                              gpointer   data)
{
  Axis *axis = data;

  if (axis->tick_cb)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (button), axis->tick_cb);
      axis->tick_cb = 0;
      gtk_button_set_icon_name (button, "media-playback-start");
    }
  else
    {
      double value, upper, lower;

      gtk_button_set_icon_name (button, "media-playback-stop");
      axis->tick_cb = gtk_widget_add_tick_callback (GTK_WIDGET (button), animate_axis, axis, NULL);
      value = gtk_adjustment_get_value (axis->adjustment);
      lower = gtk_adjustment_get_lower (axis->adjustment);
      upper = gtk_adjustment_get_upper (axis->adjustment);
      value = value / (upper - lower);
      axis->start_time = g_get_monotonic_time () - value * G_TIME_SPAN_SECOND;
      axis->increasing = TRUE;
    }
}

static void
add_axis (hb_face_t             *hb_face,
          hb_ot_var_axis_info_t *ax,
          float                  value,
          int                    i)
{
  GtkWidget *axis_label;
  GtkWidget *axis_entry;
  GtkWidget *axis_scale;
  GtkAdjustment *adjustment;
  Axis *axis;
  char name[20];
  unsigned int name_len = 20;

  hb_ot_name_get_utf8 (hb_face, ax->name_id, HB_LANGUAGE_INVALID, &name_len, name);

  axis_label = gtk_label_new (name);
  gtk_widget_set_halign (axis_label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis_label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (demo->variations_grid), axis_label, 0, i, 1, 1);
  adjustment = gtk_adjustment_new (value, ax->min_value, ax->max_value,
                                   1.0, 10.0, 0.0);
  axis_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_add_mark (GTK_SCALE (axis_scale), ax->default_value, GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis_scale, GTK_ALIGN_BASELINE_FILL);
  gtk_widget_set_hexpand (axis_scale, TRUE);
  gtk_widget_set_size_request (axis_scale, 100, -1);
  gtk_grid_attach (GTK_GRID (demo->variations_grid), axis_scale, 1, i, 1, 1);
  axis_entry = gtk_entry_new ();
  gtk_widget_set_valign (axis_entry, GTK_ALIGN_BASELINE_FILL);
  gtk_editable_set_width_chars (GTK_EDITABLE (axis_entry), 4);
  gtk_editable_set_max_width_chars (GTK_EDITABLE (axis_entry), 4);
  gtk_widget_set_hexpand (axis_entry, FALSE);
  gtk_grid_attach (GTK_GRID (demo->variations_grid), axis_entry, 2, i, 1, 1);

  axis = g_new0 (Axis, 1);
  axis->tag = ax->tag;
  axis->adjustment = adjustment;
  axis->default_value = ax->default_value;
  g_hash_table_add (demo->axes, axis);

  axis->button = gtk_button_new_from_icon_name ("media-playback-start");
  gtk_widget_add_css_class (GTK_WIDGET (axis->button), "circular");
  gtk_widget_set_valign (GTK_WIDGET (axis->button), GTK_ALIGN_CENTER);
  g_signal_connect (axis->button, "clicked", G_CALLBACK (start_or_stop_axis_animation), axis);
  gtk_grid_attach (GTK_GRID (demo->variations_grid), axis->button, 3, i, 1, 1);

  adjustment_changed (adjustment, GTK_ENTRY (axis_entry));

  g_signal_connect (adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis_entry);
  g_signal_connect (adjustment, "value-changed", G_CALLBACK (unset_instance), NULL);
  g_signal_connect (axis_entry, "activate", G_CALLBACK (entry_activated), adjustment);
}

typedef struct {
  char *name;
  unsigned int index;
} Instance;

static guint
instance_hash (gconstpointer v)
{
  const Instance *p = v;

  return g_str_hash (p->name);
}

static gboolean
instance_equal (gconstpointer v1, gconstpointer v2)
{
  const Instance *p1 = v1;
  const Instance *p2 = v2;

  return g_str_equal (p1->name, p2->name);
}

static void
free_instance (gpointer data)
{
  Instance *instance = data;

  g_free (instance->name);
  g_free (instance);
}

static void
add_instance (hb_face_t     *face,
              unsigned int   index,
              GtkStringList *strings)
{
  Instance *instance;
  hb_ot_name_id_t name_id;
  char name[20];
  unsigned int name_len = 20;

  instance = g_new0 (Instance, 1);

  name_id = hb_ot_var_named_instance_get_subfamily_name_id (face, index);
  hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &name_len, name);

  instance->name = g_strdup (name);
  instance->index = index;

  g_hash_table_add (demo->instances, instance);
  gtk_string_list_append (GTK_STRING_LIST (strings), instance->name);
}

static void
unset_instance (GtkAdjustment *adjustment)
{
  if (demo->instance_combo)
    gtk_drop_down_set_selected (GTK_DROP_DOWN (demo->instance_combo), 0);
}

static void
instance_changed (GtkDropDown *combo)
{
  const char *text;
  Instance *instance;
  Instance ikey;
  int i;
  unsigned int coords_length;
  float *coords = NULL;
  hb_ot_var_axis_info_t *ai = NULL;
  unsigned int n_axes;
  PangoFont *pango_font = NULL;
  hb_font_t *hb_font;
  hb_face_t *hb_face;

  text = gtk_string_list_get_string (GTK_STRING_LIST (gtk_drop_down_get_model (combo)),
                                     gtk_drop_down_get_selected (combo));
  if (text[0] == '\0')
    goto out;

  ikey.name = (char *) text;
  instance = g_hash_table_lookup (demo->instances, &ikey);
  if (!instance)
    goto out;

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);
  hb_face = hb_font_get_face (hb_font);

  n_axes = hb_ot_var_get_axis_infos (hb_face, 0, NULL, NULL);
  ai = g_new (hb_ot_var_axis_info_t, n_axes);
  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, ai);

  coords = g_new (float, n_axes);
  hb_ot_var_named_instance_get_design_coords (hb_face,
                                              instance->index,
                                              &coords_length,
                                              coords);

  for (i = 0; i < n_axes; i++)
    {
      Axis *axis;
      Axis akey;
      double value;

      value = coords[ai[i].axis_index];

      akey.tag = ai[i].tag;
      axis = g_hash_table_lookup (demo->axes, &akey);
      if (axis)
        {
          g_signal_handlers_block_by_func (axis->adjustment, unset_instance, NULL);
          gtk_adjustment_set_value (axis->adjustment, value);
          g_signal_handlers_unblock_by_func (axis->adjustment, unset_instance, NULL);
        }
    }

out:
  g_clear_object (&pango_font);
  g_free (ai);
  g_free (coords);
}

static gboolean
matches_instance (hb_face_t             *hb_face,
                  unsigned int           index,
                  unsigned int           n_axes,
                  float                 *coords)
{
  float *instance_coords;
  unsigned int coords_length;
  int i;

  instance_coords = g_new (float, n_axes);
  coords_length = n_axes;

  hb_ot_var_named_instance_get_design_coords (hb_face,
                                              index,
                                              &coords_length,
                                              instance_coords);

  for (i = 0; i < n_axes; i++)
    if (instance_coords[i] != coords[i])
      return FALSE;

  return TRUE;
}

static void
add_font_plane (int i)
{
  GtkWidget *plane;
  Axis *weight_axis;
  Axis *width_axis;

  Axis key;

  key.tag = MAKE_TAG('w','g','h','t');
  weight_axis = g_hash_table_lookup (demo->axes, &key);
  key.tag = MAKE_TAG('w','d','t','h');
  width_axis = g_hash_table_lookup (demo->axes, &key);

  if (weight_axis && width_axis)
    {
      plane = gtk_font_plane_new (weight_axis->adjustment,
                                  width_axis->adjustment);

      gtk_widget_set_size_request (plane, 300, 300);
      gtk_widget_set_halign (plane, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (demo->variations_grid), plane, 0, i, 3, 1);
    }
}

/* FIXME: This doesn't work if the font has an avar table */
static float
denorm_coord (hb_ot_var_axis_info_t *axis, int coord)
{
  float r = coord / 16384.0;

  if (coord < 0)
    return axis->default_value + r * (axis->default_value - axis->min_value);
  else
    return axis->default_value + r * (axis->max_value - axis->default_value);
}

static void
update_font_variations (void)
{
  GtkWidget *child;
  PangoFont *pango_font = NULL;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  unsigned int n_axes;
  hb_ot_var_axis_info_t *ai = NULL;
  float *design_coords = NULL;
  const int *coords;
  unsigned int length;
  int i;

  while ((child = gtk_widget_get_first_child (demo->variations_grid)))
    gtk_grid_remove (GTK_GRID (demo->variations_grid), child);

  demo->instance_combo = NULL;

  g_hash_table_remove_all (demo->axes);
  g_hash_table_remove_all (demo->instances);

  pango_font = get_pango_font ();
  hb_font = pango_font_get_hb_font (pango_font);
  hb_face = hb_font_get_face (hb_font);

  n_axes = hb_ot_var_get_axis_infos (hb_face, 0, NULL, NULL);
  if (n_axes == 0)
    goto done;

  ai = g_new0 (hb_ot_var_axis_info_t, n_axes);
  design_coords = g_new (float, n_axes);

  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, ai);
  coords = hb_font_get_var_coords_normalized (hb_font, &length);
  for (i = 0; i < length; i++)
    design_coords[i] = denorm_coord (&ai[i], coords[i]);

  if (hb_ot_var_get_named_instance_count (hb_face) > 0)
    {
      GtkWidget *label;
      GtkWidget *combo;
      GtkStringList *strings;

      label = gtk_label_new ("Instance");
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
      gtk_grid_attach (GTK_GRID (demo->variations_grid), label, 0, -1, 1, 1);

      strings = gtk_string_list_new (NULL);
      combo = gtk_drop_down_new (G_LIST_MODEL (strings), NULL);

      gtk_widget_set_halign (combo, GTK_ALIGN_START);
      gtk_widget_set_valign (combo, GTK_ALIGN_BASELINE_FILL);

      gtk_string_list_append (strings, "");

      for (i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
        add_instance (hb_face, i, strings);

      for (i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
        {
          if (matches_instance (hb_face, i, n_axes, design_coords))
            {
              gtk_drop_down_set_selected (GTK_DROP_DOWN (combo), i + 1);
              break;
            }
        }

      gtk_grid_attach (GTK_GRID (demo->variations_grid), combo, 1, -1, 3, 1);
      g_signal_connect (combo, "notify::selected", G_CALLBACK (instance_changed), NULL);
      demo->instance_combo = combo;
   }

  for (i = 0; i < n_axes; i++)
    add_axis (hb_face, &ai[i], design_coords[i], i);

  add_font_plane (n_axes);

done:
  g_clear_object (&pango_font);
  g_free (ai);
  g_free (design_coords);
}

static void
font_features_font_changed (void)
{
  update_basic ();
  update_script_combo ();
  update_features ();
  update_font_variations ();
}

static void
font_features_script_changed (void)
{
  update_features ();
  update_display ();
}

static void
font_features_reset_features (void)
{
  GList *l;

  gtk_label_select_region (GTK_LABEL (demo->the_label), 0, 0);

  g_list_free_full (demo->ranges, free_range);
  demo->ranges = NULL;

  for (l = demo->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (strcmp (item->name, "xxxx") == 0)
            gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), TRUE);
          else
            {
              gtk_check_button_set_active (GTK_CHECK_BUTTON (item->feat), FALSE);
              set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
            }
        }
    }
}

static void
font_features_toggle_edit (void)
{
  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (demo->stack)), "entry") != 0)
    {
      GtkTextBuffer *buffer;
      GtkTextIter start, end;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (demo->entry));
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      g_free (demo->text);
      demo->text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
      gtk_stack_set_visible_child_name (GTK_STACK (demo->stack), "entry");
      gtk_widget_grab_focus (demo->entry);
      gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (demo->swin)), 0);
    }
  else
    {
      g_clear_pointer (&demo->text, g_free);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (demo->plain_toggle), TRUE);
      update_display ();
    }
}

static void
font_features_stop_edit (void)
{
  g_signal_emit_by_name (demo->edit_toggle, "clicked");
}

static gboolean
entry_key_press (GtkEventController *controller,
                 guint               keyval,
                 guint               keycode,
                 GdkModifierType     modifiers,
                 GtkTextView        *entry)
{
  if (keyval == GDK_KEY_Escape)
    {
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (entry), demo->text, -1);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static const char *paragraphs[] = {
  "Grumpy wizards make toxic brew for the evil Queen and Jack. A quick movement of the enemy will jeopardize six gunboats. The job of waxing linoleum frequently peeves chintzy kids. My girl wove six dozen plaid jackets before she quit. Twelve ziggurats quickly jumped a finch box.",
  "Разъяренный чтец эгоистично бьёт пятью жердями шустрого фехтовальщика. Наш банк вчера же выплатил Ф.Я. Эйхгольду комиссию за ценные вещи. Эх, чужак, общий съём цен шляп (юфть) – вдрызг! В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!",
  "Τάχιστη αλώπηξ βαφής ψημένη γη, δρασκελίζει υπέρ νωθρού κυνός",
};

static const char *alphabets[] = {
  "abcdefghijklmnopqrstuvwxzy",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789",
  "!@#$%^&*/?;",
};

static void
set_text_alphabet (void)
{
  demo->sample++;
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (demo->entry)),
                            alphabets[demo->sample % G_N_ELEMENTS (alphabets)],
                            -1);
  update_display ();
}

static void
set_text_paragraph (void)
{
  demo->sample++;
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (demo->entry)),
                            paragraphs[demo->sample % G_N_ELEMENTS (paragraphs)],
                            -1);
  update_display ();
}

GtkWidget *
do_font_features (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkEventController *controller;
      GtkExpression *expression;

      builder = gtk_builder_new ();

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, basic_value_changed);
      gtk_builder_cscope_add_callback (scope, basic_entry_activated);
      gtk_builder_cscope_add_callback (scope, color_set_cb);
      gtk_builder_cscope_add_callback (scope, swap_colors);
      gtk_builder_cscope_add_callback (scope, font_features_reset_basic);
      gtk_builder_cscope_add_callback (scope, font_features_reset_features);
      gtk_builder_cscope_add_callback (scope, font_features_reset_variations);
      gtk_builder_cscope_add_callback (scope, font_features_toggle_plain);
      gtk_builder_cscope_add_callback (scope, font_features_toggle_edit);
      gtk_builder_cscope_add_callback (scope, font_features_stop_edit);
      gtk_builder_cscope_add_callback (scope, font_features_font_changed);
      gtk_builder_cscope_add_callback (scope, font_features_script_changed);
      gtk_builder_cscope_add_callback (scope, font_features_notify_waterfall);
      gtk_builder_cscope_add_callback (scope, set_text_alphabet);
      gtk_builder_cscope_add_callback (scope, set_text_paragraph);
      gtk_builder_set_scope (builder, scope);

      demo = g_new0 (FontFeaturesDemo, 1);

      gtk_builder_add_from_resource (builder, "/font_features/font_features.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_set_data_full  (G_OBJECT (window), "demo", demo, demo_free);

      demo->the_label = GTK_WIDGET (gtk_builder_get_object (builder, "label"));
      demo->settings = GTK_WIDGET (gtk_builder_get_object (builder, "settings"));
      demo->description = GTK_WIDGET (gtk_builder_get_object (builder, "description"));
      demo->font = GTK_WIDGET (gtk_builder_get_object (builder, "font"));
      demo->script_lang = GTK_WIDGET (gtk_builder_get_object (builder, "script_lang"));
      g_assert (GTK_IS_DROP_DOWN (demo->script_lang));
      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, G_CALLBACK (script_lang_get_langname), NULL, NULL);
      gtk_drop_down_set_expression (GTK_DROP_DOWN (demo->script_lang), expression);
      gtk_expression_unref (expression);
      demo->feature_list = GTK_WIDGET (gtk_builder_get_object (builder, "feature_list"));
      demo->stack = GTK_WIDGET (gtk_builder_get_object (builder, "stack"));
      demo->entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));
      demo->plain_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "plain_toggle"));
      demo->waterfall_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "waterfall_toggle"));
      demo->edit_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "edit_toggle"));
      demo->size_scale = GTK_WIDGET (gtk_builder_get_object (builder, "size_scale"));
      demo->size_entry = GTK_WIDGET (gtk_builder_get_object (builder, "size_entry"));
      demo->size_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "size_adjustment"));
      demo->letterspacing_entry = GTK_WIDGET (gtk_builder_get_object (builder, "letterspacing_entry"));
      demo->letterspacing_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "letterspacing_adjustment"));
      demo->line_height_entry = GTK_WIDGET (gtk_builder_get_object (builder, "line_height_entry"));
      demo->line_height_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "line_height_adjustment"));
      demo->foreground = GTK_WIDGET (gtk_builder_get_object (builder, "foreground"));
      demo->background = GTK_WIDGET (gtk_builder_get_object (builder, "background"));
      demo->swin = GTK_WIDGET (gtk_builder_get_object (builder, "swin"));

      demo->provider = gtk_css_provider_new ();
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (demo->provider), 800);

      basic_value_changed (demo->size_adjustment, demo->size_entry);
      basic_value_changed (demo->letterspacing_adjustment, demo->letterspacing_entry);
      basic_value_changed (demo->line_height_adjustment, demo->line_height_entry);

      controller = gtk_event_controller_key_new ();
      g_signal_connect (controller, "key-pressed", G_CALLBACK (entry_key_press), demo->entry);
      gtk_widget_add_controller (demo->entry, controller);

      add_check_group (demo->feature_list, _("Kerning"),
                       (const char *[]){ "kern", NULL });
      add_check_group (demo->feature_list, _("Ligatures"),
                       (const char *[]){ "liga", "dlig", "hlig", "clig", "rlig", NULL });
      add_check_group (demo->feature_list, _("Letter Case"),
                       (const char *[]){ "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp",
                                         "case",NULL });
      add_radio_group (demo->feature_list, _("Number Case"),
                       (const char *[]){ "xxxx", "lnum", "onum", NULL });
      add_radio_group (demo->feature_list, _("Number Spacing"),
                       (const char *[]){ "xxxx", "pnum", "tnum", NULL });
      add_radio_group (demo->feature_list, _("Fractions"),
                       (const char *[]){ "xxxx", "frac", "afrc", NULL });
      add_check_group (demo->feature_list, _("Numeric Extras"),
                       (const char *[]){ "zero", "nalt", "sinf", NULL });
      add_check_group (demo->feature_list, _("Character Alternatives"),
                       (const char *[]){ "swsh", "cswh", "locl", "calt", "falt", "hist",
                                         "salt", "jalt", "titl", "rand", "subs", "sups",
                                         "ordn", "ltra", "ltrm", "rtla", "rtlm", "rclt", NULL });
      add_check_group (demo->feature_list, _("Positional Alternatives"),
                       (const char *[]){ "init", "medi", "med2", "fina", "fin2", "fin3",
                                         "isol", NULL });
      add_check_group (demo->feature_list, _("Width Variants"),
                       (const char *[]){ "fwid", "hwid", "halt", "pwid", "palt", "twid",
                                         "qwid", NULL });
      add_check_group (demo->feature_list, _("Alternative Stylistic Sets"),
                       (const char *[]){ "ss01", "ss02", "ss03", "ss04", "ss05", "ss06",
                                         "ss07", "ss08", "ss09", "ss10", "ss11", "ss12",
                                         "ss13", "ss14", "ss15", "ss16", "ss17", "ss18",
                                         "ss19", "ss20", NULL });
      add_check_group (demo->feature_list, _("Character Variants"),
                       (const char *[]){ "cv01", "cv02", "cv03", "cv04", "cv05", "cv06",
                                         "cv07", "cv08", "cv09", "cv10", "cv11", "cv12",
                                         "cv13", "cv14", "cv15", "cv16", "cv17", "cv18",
                                         "cv19", "cv20", NULL });
      add_check_group (demo->feature_list, _("Mathematical"),
                       (const char *[]){ "dtls", "flac", "mgrk", "ssty", NULL });
      add_check_group (demo->feature_list, _("Optical Bounds"),
                       (const char *[]){ "opbd", "lfbd", "rtbd", NULL });
      demo->feature_items = g_list_reverse (demo->feature_items);

      demo->variations_grid = GTK_WIDGET (gtk_builder_get_object (builder, "variations_grid"));
      if (demo->instances == NULL)
        demo->instances = g_hash_table_new_full (instance_hash, instance_equal, NULL, free_instance);
      else
        g_hash_table_remove_all (demo->instances);

      if (demo->axes == NULL)
        demo->axes = g_hash_table_new_full (axes_hash, axes_equal, NULL, g_free);
      else
        g_hash_table_remove_all (demo->axes);

      font_features_font_changed ();

      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_object_unref (builder);

      update_display ();
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}

/* vim:set foldmethod=marker expandtab: */
