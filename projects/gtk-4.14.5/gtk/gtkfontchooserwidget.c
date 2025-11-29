/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include "deprecated/gtkfontchooserwidget.h"
#include "gtkfontchooserwidgetprivate.h"

#include "gtkadjustment.h"
#include "gtkbuildable.h"
#include "gtkbox.h"
#include "gtkbinlayout.h"
#include "gtkcheckbutton.h"
#include "gtkcustomfilter.h"
#include "gtkentry.h"
#include "gtkfilter.h"
#include "gtkframe.h"
#include "gtkgrid.h"
#include "deprecated/gtkfontchooser.h"
#include "gtkfontchooserutils.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtksingleselection.h"
#include "gtkstack.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkscrolledwindow.h"
#include "gtksearchentry.h"
#include "gtkspinbutton.h"
#include "gtktextview.h"
#include "gtkwidgetprivate.h"
#include "gtksettings.h"
#include "deprecated/gtkdialog.h"
#include "gtkgestureclick.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkroot.h"
#include "gtkfilterlistmodel.h"
#include "gtkflattenlistmodel.h"
#include "gtkslicelistmodel.h"
#include "gtkmaplistmodel.h"
#include "gtklistitem.h"
#include "gtksignallistitemfactory.h"
#include "gtkstringlist.h"
#include "gtklistview.h"
#include "gtksortlistmodel.h"
#include "gtkstringsorter.h"
#include "gtkdropdown.h"
#include "gtkmultifilter.h"

#include <hb-ot.h>

#include "language-names.h"
#include "open-type-layout.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkFontChooserWidget:
 *
 * The `GtkFontChooserWidget` widget lets the user select a font.
 *
 * It is used in the `GtkFontChooserDialog` widget to provide a
 * dialog for selecting fonts.
 *
 * To set the font which is initially selected, use
 * [method@Gtk.FontChooser.set_font] or [method@Gtk.FontChooser.set_font_desc].
 *
 * To get the selected font use [method@Gtk.FontChooser.get_font] or
 * [method@Gtk.FontChooser.get_font_desc].
 *
 * To change the text which is shown in the preview area, use
 * [method@Gtk.FontChooser.set_preview_text].
 *
 * # CSS nodes
 *
 * `GtkFontChooserWidget` has a single CSS node with name fontchooser.
 *
 * Deprecated: 4.10: Direct use of `GtkFontChooserWidget` is deprecated.
 */

typedef struct _GtkFontChooserWidgetClass         GtkFontChooserWidgetClass;

struct _GtkFontChooserWidget
{
  GtkWidget parent_instance;

  GtkWidget    *stack;
  GtkWidget    *grid;
  GtkWidget    *search_entry;
  GtkWidget    *family_face_list;
  GtkWidget    *list_stack;
  GtkSingleSelection *selection;
  GtkCustomFilter      *custom_filter;
  GtkCustomFilter      *user_filter;
  GtkCustomFilter      *multi_filter;
  GtkFilterListModel   *filter_model;

  GtkWidget       *preview;
  GtkWidget       *preview2;
  GtkWidget       *font_name_label;
  char            *preview_text;
  gboolean         show_preview_entry;
  gboolean         preview_text_set;

  GtkWidget *size_label;
  GtkWidget *size_spin;
  GtkWidget *size_slider;
  GtkWidget *size_label2;
  GtkWidget *size_spin2;
  GtkWidget *size_slider2;

  GtkWidget       *axis_grid;
  GtkWidget       *feature_box;

  GtkFrame          *language_button;
  GtkFrame          *language_frame;
  GtkWidget         *language_list;
  GtkStringList     *languages;
  GHashTable        *language_table;

  PangoLanguage     *filter_language;
  gboolean           filter_by_language;
  gboolean           filter_by_monospace;

  PangoFontMap         *font_map;

  PangoFontDescription *font_desc;
  char                 *font_features;
  PangoLanguage        *language;

  GtkFontFilterFunc filter_func;
  gpointer          filter_data;
  GDestroyNotify    filter_data_destroy;

  GtkFilter *filter;

  guint last_fontconfig_timestamp;

  GtkFontChooserLevel level;

  GHashTable *axes;
  gboolean updating_variations;

  GList *feature_items;

  GAction *tweak_action;

  hb_map_t *glyphmap;
};

struct _GtkFontChooserWidgetClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_ZERO,
  PROP_TWEAK_ACTION
};

static void gtk_font_chooser_widget_set_property         (GObject         *object,
                                                          guint            prop_id,
                                                          const GValue    *value,
                                                          GParamSpec      *pspec);
static void gtk_font_chooser_widget_get_property         (GObject         *object,
                                                          guint            prop_id,
                                                          GValue          *value,
                                                          GParamSpec      *pspec);
static void gtk_font_chooser_widget_finalize             (GObject         *object);

static char    *gtk_font_chooser_widget_get_font         (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_font         (GtkFontChooserWidget *fontchooser,
                                                          const char           *fontname);

static PangoFontDescription *gtk_font_chooser_widget_get_font_desc  (GtkFontChooserWidget *fontchooser);
static void                  gtk_font_chooser_widget_merge_font_desc(GtkFontChooserWidget       *fontchooser,
                                                                     const PangoFontDescription *font_desc);
static void                  gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc);


static const char *gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser);
static void         gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                                              const char           *text);

static gboolean gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                                gboolean              show_preview_entry);

static void     gtk_font_chooser_widget_populate_features      (GtkFontChooserWidget *fontchooser);
static void                gtk_font_chooser_widget_set_level (GtkFontChooserWidget *fontchooser,
                                                              GtkFontChooserLevel   level);
static GtkFontChooserLevel gtk_font_chooser_widget_get_level (GtkFontChooserWidget *fontchooser);
static void                gtk_font_chooser_widget_set_language (GtkFontChooserWidget *fontchooser,
                                                                 const char           *language);
static void update_font_features (GtkFontChooserWidget *fontchooser);

static void gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserWidget, gtk_font_chooser_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_chooser_widget_iface_init))

static void
gtk_font_chooser_widget_set_property (GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case GTK_FONT_CHOOSER_PROP_FONT:
      gtk_font_chooser_widget_set_font (fontchooser, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      gtk_font_chooser_widget_take_font_desc (fontchooser, g_value_dup_boxed (value));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      gtk_font_chooser_widget_set_preview_text (fontchooser, g_value_get_string (value));
      fontchooser->preview_text_set = TRUE;
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      gtk_font_chooser_widget_set_show_preview_entry (fontchooser, g_value_get_boolean (value));
      break;
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      gtk_font_chooser_widget_set_level (fontchooser, g_value_get_flags (value));
      break;
    case GTK_FONT_CHOOSER_PROP_LANGUAGE:
      gtk_font_chooser_widget_set_language (fontchooser, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_font_chooser_widget_get_property (GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TWEAK_ACTION:
      g_value_set_object (value, G_OBJECT (fontchooser->tweak_action));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT:
      g_value_take_string (value, gtk_font_chooser_widget_get_font (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, gtk_font_chooser_widget_get_font_desc (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_chooser_widget_get_preview_text (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, gtk_font_chooser_widget_get_show_preview_entry (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_LEVEL:
      g_value_set_flags (value, gtk_font_chooser_widget_get_level (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_FEATURES:
      g_value_set_string (value, fontchooser->font_features);
      break;
    case GTK_FONT_CHOOSER_PROP_LANGUAGE:
      g_value_set_string (value, pango_language_to_string (fontchooser->language));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
stop_search_cb (GtkSearchEntry       *entry,
                GtkFontChooserWidget *fc)
{
  if (gtk_editable_get_text (GTK_EDITABLE (entry))[0] != 0)
    gtk_editable_set_text (GTK_EDITABLE (entry), "");
  else
    {
      GtkWidget *dialog;
      GtkWidget *button = NULL;

      dialog = gtk_widget_get_ancestor (GTK_WIDGET (fc), GTK_TYPE_DIALOG);
      if (dialog)
        button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

      if (button)
        gtk_widget_activate (button);
    }
}

static void
size_change_cb (GtkAdjustment *adjustment,
                gpointer       user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  PangoFontDescription *font_desc;
  double size = gtk_adjustment_get_value (adjustment);

  font_desc = pango_font_description_new ();
  if (pango_font_description_get_size_is_absolute (fontchooser->font_desc))
    pango_font_description_set_absolute_size (font_desc, size * PANGO_SCALE);
  else
    pango_font_description_set_size (font_desc, size * PANGO_SCALE);

  gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

static gboolean
output_cb (GtkSpinButton *spin,
           gpointer       data)
{
  GtkAdjustment *adjustment;
  char *text;
  double value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%2.4g", value);
  gtk_editable_set_text (GTK_EDITABLE (spin), text);
  g_free (text);

  return TRUE;
}

static gboolean
user_filter_cb (gpointer item,
                gpointer data)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (data);
  PangoFontFamily *family;
  PangoFontFace *face;

  if (PANGO_IS_FONT_FAMILY (item))
    {
      family = item;
      face = pango_font_family_get_face (family, NULL);
    }
  else
    {
      face = PANGO_FONT_FACE (item);
      family = pango_font_face_get_family (face);
    }

  if (self->filter_by_monospace &&
      !pango_font_family_is_monospace (family))
    return FALSE;

  if (self->filter_by_language &&
      self->filter_language)
    {
      PangoFontDescription *desc;
      PangoContext *context;
      PangoFont *font;
      gboolean ret;
      PangoLanguage **langs;

      desc = pango_font_face_describe (face);
      pango_font_description_set_size (desc, 20);

      context = gtk_widget_get_pango_context (GTK_WIDGET (self));
      font = pango_context_load_font (context, desc);

      ret = FALSE;

      langs = pango_font_get_languages (font);
      if (langs)
        {
          for (int i = 0; langs[i]; i++)
            {
              if (langs[i] == self->filter_language)
                {
                  ret = TRUE;
                  break;
                }
            }
        }

      g_object_unref (font);
      pango_font_description_free (desc);

      return ret;
    }

  return TRUE;
}

static void
monospace_check_changed (GtkCheckButton       *check,
                         GParamSpec           *pspec,
                         GtkFontChooserWidget *self)
{
  self->filter_by_monospace = gtk_check_button_get_active (check);
  gtk_filter_changed (GTK_FILTER (self->user_filter),
                      self->filter_by_monospace ? GTK_FILTER_CHANGE_MORE_STRICT
                                                : GTK_FILTER_CHANGE_LESS_STRICT);
}

static void
language_check_changed (GtkCheckButton       *check,
                        GParamSpec           *pspec,
                        GtkFontChooserWidget *self)
{
  self->filter_by_language = gtk_check_button_get_active (check);
  gtk_filter_changed (GTK_FILTER (self->user_filter),
                      self->filter_by_language ? GTK_FILTER_CHANGE_MORE_STRICT
                                               : GTK_FILTER_CHANGE_LESS_STRICT);
}

static void
gtk_font_chooser_widget_update_marks (GtkFontChooserWidget *self)
{
  GtkAdjustment *adj, *spin_adj;
  const int *sizes;
  int *font_sizes;
  int i, n_sizes;
  double value, spin_value;
  gpointer item;

  item = gtk_single_selection_get_selected_item (self->selection);

  if (item)
    {
      PangoFontFace *face;

      if (PANGO_IS_FONT_FAMILY (item))
        face = pango_font_family_get_face (item, NULL);
      else
        face = item;

      pango_font_face_list_sizes (face, &font_sizes, &n_sizes);

      /* It seems not many fonts actually have a sane set of sizes */
      for (i = 0; i < n_sizes; i++)
        font_sizes[i] = font_sizes[i] / PANGO_SCALE;
    }
  else
    {
      font_sizes = NULL;
      n_sizes = 0;
    }

  if (n_sizes < 2)
    {
      static const int fallback_sizes[] = {
        6, 8, 9, 10, 11, 12, 13, 14, 16, 20, 24, 36, 48, 72
      };

      sizes = fallback_sizes;
      n_sizes = G_N_ELEMENTS (fallback_sizes);
    }
  else
    {
      sizes = font_sizes;
    }

  gtk_scale_clear_marks (GTK_SCALE (self->size_slider));
  gtk_scale_clear_marks (GTK_SCALE (self->size_slider2));

  adj        = gtk_range_get_adjustment (GTK_RANGE (self->size_slider));
  spin_adj   = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (self->size_spin));
  spin_value = gtk_adjustment_get_value (spin_adj);

  if (spin_value < sizes[0])
    value = (double) sizes[0];
  else if (spin_value > sizes[n_sizes - 1])
    value = (double)sizes[n_sizes - 1];
  else
    value = (double)spin_value;

  /* ensure clamping doesn't callback into font resizing code */
  g_signal_handlers_block_by_func (adj, size_change_cb, self);
  gtk_adjustment_configure (adj,
                            value,
                            sizes[0],
                            sizes[n_sizes - 1],
                            gtk_adjustment_get_step_increment (adj),
                            gtk_adjustment_get_page_increment (adj),
                            gtk_adjustment_get_page_size (adj));
  g_signal_handlers_unblock_by_func (adj, size_change_cb, self);

  for (i = 0; i < n_sizes; i++)
    {
      gtk_scale_add_mark (GTK_SCALE (self->size_slider),
                          sizes[i],
                          GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (self->size_slider2),
                          sizes[i],
                          GTK_POS_BOTTOM, NULL);
    }

  g_free (font_sizes);
}

static void
row_activated_cb (GtkWidget *view,
                  guint      pos,
                  gpointer   user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;
  char *fontname;

  fontname = gtk_font_chooser_widget_get_font (fontchooser);
  _gtk_font_chooser_font_activated (GTK_FONT_CHOOSER (fontchooser), fontname);
  g_free (fontname);
}

static void
resize_by_scroll_cb (GtkEventControllerScroll *controller,
                     double                    dx,
                     double                    dy,
                     gpointer                  user_data)
{
  GtkFontChooserWidget *self = user_data;
  GtkAdjustment *adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (self->size_spin));

  gtk_adjustment_set_value (adj,
                            gtk_adjustment_get_value (adj) +
                            gtk_adjustment_get_step_increment (adj) * dx);
}

static void
maybe_update_preview_text (GtkFontChooserWidget *self,
                           PangoFontFace        *face,
                           PangoFontDescription *desc)
{
  PangoContext *context;
  PangoFont *font;
  const char *sample;
  PangoLanguage **languages;
  GHashTable *langs = NULL;
  PangoLanguage *default_lang;
  PangoLanguage *alt_default = NULL;
  PangoLanguage *lang = NULL;
  int i;
  const char *p;

  /* If the user has typed text into the entry, we don't touch it */
  if (self->preview_text_set)
    return;

  if (self->filter_by_language && self->filter_language)
    {
      sample = pango_language_get_sample_string (self->filter_language);
      gtk_font_chooser_widget_set_preview_text (self, sample);
      return;
    }

  /* We do the work only once, and cache the result on the PangoFontFace */
  sample = (const char *)g_object_get_data (G_OBJECT (face), "gtk-sample-text");
  if (sample)
    {
      gtk_font_chooser_widget_set_preview_text (self, sample);
      return;
    }

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  font = pango_context_load_font (context, desc);

  default_lang = pango_language_get_default ();
  p = pango_language_to_string (default_lang);

  /* The default language tends to be of the form en-us.
   * Since fontconfig languages just have the language part,
   * and we want to use direct pointer comparisons, we need
   * an PangoLanguage for the shortened default language.
   */
  if (strchr (p, '-'))
    {
      char q[10];
      for (i = 0; p[i] != '-' && i < 9; i++)
        q[i] = p[i];
      q[i] = '\0';
      alt_default = pango_language_from_string (q);
    }

  languages = pango_font_get_languages (font);

  /* If the font supports the default language, just use it. */
  if (languages)
    for (i = 0; languages[i]; i++)
      {
        if (languages[i] == default_lang || languages[i] == alt_default)
          {
            lang = default_lang;
            goto found;
          }
      }

  /* Otherwise, we make a list of representative languages */
  langs = g_hash_table_new (NULL, NULL);

  if (languages)
    for (i = 0; languages[i]; i++)
      {
        const PangoScript *scripts;
        int num, j;

        scripts = pango_language_get_scripts (languages[i], &num);
        for (j = 0; j < num; j++)
          {
            lang = pango_script_get_sample_language (scripts[j]);
            if (lang)
              g_hash_table_add (langs, lang);
          }
      }

  /* ... and compare it to the users default and preferred languages */
  if (g_hash_table_contains (langs, default_lang) ||
      g_hash_table_contains (langs, alt_default))
    {
      lang = default_lang;
    }
  else
    {
      PangoLanguage **preferred;

      preferred = pango_language_get_preferred ();
      if (preferred)
        {
          for (i = 0; preferred[i]; i++)
            {
              if (g_hash_table_contains (langs, preferred[i]))
                {
                  lang = preferred[i];
                  break;
                }
            }
        }

      g_hash_table_unref (langs);

found:
      sample = pango_language_get_sample_string (lang);
      gtk_font_chooser_widget_set_preview_text (self, sample);
      g_object_set_data (G_OBJECT (face), "gtk-sample-text", (gpointer)sample);
    }

  g_object_unref (font);
}


static void
selection_changed_cb (GtkSingleSelection   *selection,
                      GParamSpec           *pspec,
                      GtkFontChooserWidget *self)
{
  gpointer item;

  item = gtk_single_selection_get_selected_item (selection);
  if (item)
    {
      PangoFontFace *face;
      PangoFontDescription *desc;

      if (PANGO_IS_FONT_FAMILY (item))
        face = pango_font_family_get_face (item, NULL);
      else
        face = item;
      desc = pango_font_face_describe (face);
      pango_font_description_set_variations (self->font_desc, NULL);
      gtk_font_chooser_widget_merge_font_desc (self, desc);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->tweak_action), TRUE);

      maybe_update_preview_text (self, face, desc);

      pango_font_description_free (desc);
    }
  else
    {
      g_simple_action_set_state (G_SIMPLE_ACTION (self->tweak_action), g_variant_new_boolean (FALSE));
      g_simple_action_set_enabled (G_SIMPLE_ACTION (self->tweak_action), FALSE);
    }

  g_object_notify (G_OBJECT (self), "font");
  g_object_notify (G_OBJECT (self), "font-desc");
}

static char *
get_font_name (GObject  *ignore,
               gpointer  item)
{
  if (item == NULL)
    return NULL;

  if (PANGO_IS_FONT_FACE (item))
    {
      return g_strconcat (pango_font_family_get_name (pango_font_face_get_family (item)),
                          " ",
                          pango_font_face_get_face_name (item),
                          NULL);
    }
  else
    {
      return g_strdup (pango_font_family_get_name (item));
    }
}

static PangoAttrList *
get_font_attributes (GObject  *ignore,
                     gpointer  item)
{
  PangoAttribute *attribute;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  if (item)
    {
      PangoFontFace *face;
      PangoFontDescription *font_desc;

      if (PANGO_IS_FONT_FAMILY (item))
        face = pango_font_family_get_face (item, NULL);
      else
        face = item;
      if (face)
        {
          font_desc = pango_font_face_describe (face);
          attribute = pango_attr_font_desc_new (font_desc);
          pango_attr_list_insert (attrs, attribute);
          pango_font_description_free (font_desc);
        }
    }

  return attrs;
}

static void
gtk_font_chooser_widget_update_preview_attributes (GtkFontChooserWidget *fontchooser)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  /* Prevent font fallback */
  pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));

  /* Force current font and features */
  pango_attr_list_insert (attrs, pango_attr_font_desc_new (fontchooser->font_desc));
  if (fontchooser->font_features)
    pango_attr_list_insert (attrs, pango_attr_font_features_new (fontchooser->font_features));
  if (fontchooser->language)
    pango_attr_list_insert (attrs, pango_attr_language_new (fontchooser->language));

  gtk_entry_set_attributes (GTK_ENTRY (fontchooser->preview), attrs);

  pango_attr_list_unref (attrs);
}

static void
rows_changed_cb (GtkFontChooserWidget *self)
{
  const char *page;

  if (g_list_model_get_n_items (G_LIST_MODEL (self->selection)) == 0 &&
      gtk_filter_list_model_get_pending (GTK_FILTER_LIST_MODEL (self->filter_model)) == 0)
    page = "empty";
  else
    page = "list";

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->list_stack)), page) != 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->list_stack), page);
}

static void
update_key_capture (GtkWidget *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkWidget *capture_widget;

  if (gtk_widget_get_mapped (chooser) &&
      g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (fontchooser->stack)), "list"))
    {
      GtkWidget *toplevel;
      GtkWidget *focus;

      toplevel = GTK_WIDGET (gtk_widget_get_root (chooser));
      focus = gtk_root_get_focus (GTK_ROOT (toplevel));

      if (GTK_IS_EDITABLE (focus) && focus != fontchooser->search_entry)
        capture_widget = NULL;
      else
        capture_widget = chooser;
    }
  else
    capture_widget = NULL;

  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (fontchooser->search_entry),
                                           capture_widget);
}

static void
gtk_font_chooser_widget_map (GtkWidget *widget)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);

  gtk_editable_set_text (GTK_EDITABLE (fontchooser->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->stack), "list");
  g_simple_action_set_state (G_SIMPLE_ACTION (fontchooser->tweak_action), g_variant_new_boolean (FALSE));

  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->map (widget);

  update_key_capture (widget);
}

static void
gtk_font_chooser_widget_unmap (GtkWidget *widget)
{
  update_key_capture (widget);

  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->unmap (widget);
}

static void
gtk_font_chooser_widget_root (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->root (widget);

  g_signal_connect_swapped (gtk_widget_get_root (widget), "notify::focus-widget",
                            G_CALLBACK (update_key_capture), widget);
}

static void
gtk_font_chooser_widget_unroot (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget),
                                        update_key_capture, widget);

  GTK_WIDGET_CLASS (gtk_font_chooser_widget_parent_class)->unroot (widget);
}

static void
gtk_font_chooser_widget_dispose (GObject *object)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (object);

  g_signal_handlers_disconnect_by_func (self->selection, rows_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->filter_model, rows_changed_cb, self);

  self->filter_func = NULL;
  g_clear_pointer (&self->filter_data, self->filter_data_destroy);

  g_clear_pointer (&self->stack, gtk_widget_unparent);
  g_clear_pointer (&self->language_table, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_font_chooser_widget_class_init (GtkFontChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  g_type_ensure (G_TYPE_THEMED_ICON);

  widget_class->root = gtk_font_chooser_widget_root;
  widget_class->unroot = gtk_font_chooser_widget_unroot;
  widget_class->map = gtk_font_chooser_widget_map;
  widget_class->unmap = gtk_font_chooser_widget_unmap;

  gobject_class->finalize = gtk_font_chooser_widget_finalize;
  gobject_class->dispose = gtk_font_chooser_widget_dispose;
  gobject_class->set_property = gtk_font_chooser_widget_set_property;
  gobject_class->get_property = gtk_font_chooser_widget_get_property;

  /**
   * GtkFontChooserWidget:tweak-action:
   *
   * A toggle action that can be used to switch to the tweak page
   * of the font chooser widget, which lets the user tweak the
   * OpenType features and variation axes of the selected font.
   *
   * The action will be enabled or disabled depending on whether
   * the selected font has any features or axes.
   */
  pspec = g_param_spec_object ("tweak-action", NULL, NULL,
                               G_TYPE_ACTION,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_TWEAK_ACTION, pspec);

  _gtk_font_chooser_install_properties (gobject_class);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkfontchooserwidget.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, family_face_list);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, list_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, filter_model);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, selection);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, custom_filter);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, user_filter);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, multi_filter);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, preview);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, preview2);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_label);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_spin);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_slider);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_label2);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_spin2);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, size_slider2);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, grid);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, font_name_label);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, feature_box);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, axis_grid);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, language_button);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, language_frame);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserWidget, language_list);
  gtk_widget_class_bind_template_callback (widget_class, get_font_name);
  gtk_widget_class_bind_template_callback (widget_class, get_font_attributes);
  gtk_widget_class_bind_template_callback (widget_class, stop_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, rows_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, size_change_cb);
  gtk_widget_class_bind_template_callback (widget_class, output_cb);
  gtk_widget_class_bind_template_callback (widget_class, selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, resize_by_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, monospace_check_changed);
  gtk_widget_class_bind_template_callback (widget_class, language_check_changed);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("fontchooser"));
}

static void
change_tweak (GSimpleAction *action,
              GVariant      *state,
              gpointer       data)
{
  GtkFontChooserWidget *fontchooser = data;
  gboolean tweak = g_variant_get_boolean (state);

  if (tweak)
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (fontchooser->preview2));
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->stack), "tweaks");
    }
  else
    {
      gtk_widget_grab_focus (fontchooser->search_entry);
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->stack), "list");
    }

  g_simple_action_set_state (action, state);
}

typedef struct {
  guint32 tag;
  float default_value;
  GtkAdjustment *adjustment;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *spin;
  GtkWidget *fontchooser;
} Axis;

static guint
axis_hash (gconstpointer v)
{
  const Axis *a = v;

  return a->tag;
}

static gboolean
axis_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *a1 = v1;
  const Axis *a2 = v2;

  return a1->tag == a2->tag;
}

static void
axis_remove (gpointer key,
             gpointer value,
             gpointer data)
{
  GtkFontChooserWidget *fontchooser = data;
  Axis *a = value;

  gtk_grid_remove (GTK_GRID (fontchooser->axis_grid), a->label);
  gtk_grid_remove (GTK_GRID (fontchooser->axis_grid), a->scale);
  gtk_grid_remove (GTK_GRID (fontchooser->axis_grid), a->spin);
}

static void
axis_free (gpointer v)
{
  Axis *a = v;

  g_free (a);
}

static void
select_added (GListModel *model,
              guint       position,
              guint       removed,
              guint       added,
              gpointer    data)
{
  GtkSingleSelection *selection = GTK_SINGLE_SELECTION (model);

  g_assert (removed == 0);
  g_assert (added == 1);

  gtk_single_selection_set_selected (selection, position);
}

static void
add_languages_from_font (GtkFontChooserWidget *self,
                         gpointer              item)
{
  PangoFontFace *face;
  PangoFontDescription *desc;
  PangoFont *font;
  PangoContext *context;
  GtkSelectionModel *model = gtk_list_view_get_model (GTK_LIST_VIEW (self->language_list));
  PangoLanguage *default_lang = pango_language_get_default ();
  PangoLanguage **langs;
  int i;

  if (PANGO_IS_FONT_FAMILY (item))
    face = pango_font_family_get_face (PANGO_FONT_FAMILY (item), NULL);
  else
    face = PANGO_FONT_FACE (item);

  if (!face)
    return;

  desc = pango_font_face_describe (face);
  pango_font_description_set_size (desc, 20);

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  font = pango_context_load_font (context, desc);

  langs = pango_font_get_languages (font);
  if (langs)
    {
      for (i = 0; langs[i]; i++)
        {
          if (!g_hash_table_contains (self->language_table, langs[i]))
            {
              g_hash_table_add (self->language_table, langs[i]);
              if (get_language_name (langs[i]))
                {
                  const char *l = pango_language_to_string (langs[i]);
                  gulong id = 0;

                  /* Pre-select the default language */
                  if (pango_language_matches (default_lang, l))
                    id = g_signal_connect (model, "items-changed", G_CALLBACK (select_added), NULL);

                  gtk_string_list_append (self->languages, l);

                  if (id)
                    g_signal_handler_disconnect (model, id);
                }
            }
        }
    }

  g_object_unref (font);
  pango_font_description_free (desc);
}

static gboolean
gtk_font_chooser_widget_ensure_matching_selection (GtkFontChooserWidget *self);

/* We incrementally populate our fontlist to prevent blocking
 * the font chooser for a long time with expensive FcFontSort
 * calls in pango for every row in the list).
 */
static gboolean
add_to_fontlist (GtkWidget     *widget,
                 GdkFrameClock *clock,
                 gpointer       user_data)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (widget);
  GtkSliceListModel *model = user_data;
  GListModel *child_model;
  guint i G_GNUC_UNUSED;
  guint n G_GNUC_UNUSED;

  if (gtk_filter_list_model_get_model (self->filter_model) != G_LIST_MODEL (model))
    return G_SOURCE_REMOVE;

  child_model = gtk_slice_list_model_get_model (model);

  n = gtk_slice_list_model_get_size (model);

  for (i = n; i < n + 10; i++)
    {
      gpointer item = g_list_model_get_item (child_model, i);
      if (!item)
        break;
      add_languages_from_font (self, item);
      g_object_unref (item);
    }

  n += 10;

  if (n >= g_list_model_get_n_items (child_model))
    n = G_MAXUINT;

  gtk_slice_list_model_set_size (model, n);

  if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection)) == GTK_INVALID_LIST_POSITION)
    gtk_font_chooser_widget_ensure_matching_selection (self);

  if (n == G_MAXUINT)
    return G_SOURCE_REMOVE;
  else
    return G_SOURCE_CONTINUE;
}

static void
update_fontlist (GtkFontChooserWidget *self)
{
  PangoFontMap *fontmap;
  GListModel *model;

  fontmap = self->font_map;
  if (!fontmap)
    fontmap = pango_cairo_font_map_get_default ();

  if ((self->level & GTK_FONT_CHOOSER_LEVEL_STYLE) == 0)
    model = g_object_ref (G_LIST_MODEL (fontmap));
  else
    model = G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (g_object_ref (fontmap))));

  model = G_LIST_MODEL (gtk_slice_list_model_new (model, 0, 20));
  gtk_widget_add_tick_callback (GTK_WIDGET (self), add_to_fontlist, g_object_ref (model), g_object_unref);

  gtk_filter_list_model_set_model (self->filter_model, model);
  g_object_unref (model);
}

static void
setup_lang_item (GtkSignalListItemFactory *factory,
                 gpointer                  item,
                 gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (GTK_LIST_ITEM (item), label);
}

static void
bind_lang_item (GtkSignalListItemFactory *factory,
                gpointer                  item,
                gpointer                  data)
{
  GtkWidget *label;
  gpointer obj;
  const char *str;
  PangoLanguage *language;
  const char *name;

  obj = gtk_list_item_get_item (GTK_LIST_ITEM (item));
  str = gtk_string_object_get_string (GTK_STRING_OBJECT (obj));

  language = pango_language_from_string (str);
  name = get_language_name (language);

  label = gtk_list_item_get_child (GTK_LIST_ITEM (item));
  gtk_label_set_label (GTK_LABEL (label), name);
}

static char *
get_lang_name (gpointer    this,
               const char *lang)
{
  return g_strdup (get_language_name (pango_language_from_string (lang)));
}

static void
language_selection_changed (GtkSelectionModel    *model,
                            guint                 position,
                            guint                 n_items,
                            GtkFontChooserWidget *self)
{
  gpointer obj;

  obj = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model));

  if (obj)
    self->filter_language = pango_language_from_string (gtk_string_object_get_string (obj));
  else
    self->filter_language = NULL;

  if (self->filter_by_language)
    gtk_filter_changed (GTK_FILTER (self->user_filter), GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
setup_language_list (GtkFontChooserWidget *self)
{
  GtkListItemFactory *factory;
  GtkExpression *expression;
  GListModel *model;
  GtkSelectionModel *selection;

  self->languages = gtk_string_list_new (NULL);
  self->language_table = g_hash_table_new (NULL, NULL);

  expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");
  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 1, &expression, (GCallback)get_lang_name, NULL, NULL);

  model = G_LIST_MODEL (gtk_sort_list_model_new (G_LIST_MODEL (self->languages),
                        GTK_SORTER (gtk_string_sorter_new (expression))));

  selection = GTK_SELECTION_MODEL (gtk_single_selection_new (model));
  g_signal_connect (selection, "selection-changed", G_CALLBACK (language_selection_changed), self);
  gtk_list_view_set_model (GTK_LIST_VIEW (self->language_list), selection);
  g_object_unref (selection);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_lang_item), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_lang_item), self);
  gtk_list_view_set_factory (GTK_LIST_VIEW (self->language_list), factory);
  g_object_unref (factory);

  return TRUE;
}

static void
gtk_font_chooser_widget_init (GtkFontChooserWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->axes = g_hash_table_new_full (axis_hash, axis_equal, NULL, axis_free);

  /* Default preview string  */
  self->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  self->show_preview_entry = TRUE;
  self->font_desc = pango_font_description_new ();
  self->level = GTK_FONT_CHOOSER_LEVEL_FAMILY |
                GTK_FONT_CHOOSER_LEVEL_STYLE |
                GTK_FONT_CHOOSER_LEVEL_SIZE;
  self->language = pango_language_get_default ();

  /* Set default preview text */
  gtk_editable_set_text (GTK_EDITABLE (self->preview), self->preview_text);

  gtk_font_chooser_widget_update_preview_attributes (self);

  /* Set the upper values of the spin/scale with G_MAXINT / PANGO_SCALE */
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->size_spin),
			     1.0, (double)(G_MAXINT / PANGO_SCALE));
  gtk_adjustment_set_upper (gtk_range_get_adjustment (GTK_RANGE (self->size_slider)),
			    (double)(G_MAXINT / PANGO_SCALE));

  self->tweak_action = G_ACTION (g_simple_action_new_stateful ("tweak", NULL, g_variant_new_boolean (FALSE)));
  g_signal_connect (self->tweak_action, "change-state", G_CALLBACK (change_tweak), self);

  update_fontlist (self);

  /* Load data and set initial style-dependent parameters */
  gtk_font_chooser_widget_populate_features (self);

  gtk_font_chooser_widget_take_font_desc (self, NULL);

  gtk_custom_filter_set_filter_func (self->user_filter, user_filter_cb, self, NULL);

  setup_language_list (self);
}

/**
 * gtk_font_chooser_widget_new:
 *
 * Creates a new `GtkFontChooserWidget`.
 *
 * Returns: a new `GtkFontChooserWidget`
 *
 * Deprecated: 4.10: Direct use of `GtkFontChooserWidget` is deprecated.
 */
GtkWidget *
gtk_font_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_FONT_CHOOSER_WIDGET, NULL);
}

static void
gtk_font_chooser_widget_finalize (GObject *object)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  if (fontchooser->font_desc)
    pango_font_description_free (fontchooser->font_desc);

  if (fontchooser->filter_data_destroy)
    fontchooser->filter_data_destroy (fontchooser->filter_data);

  g_free (fontchooser->preview_text);

  g_clear_object (&fontchooser->font_map);

  g_object_unref (fontchooser->tweak_action);

  g_list_free_full (fontchooser->feature_items, g_free);

  g_hash_table_unref (fontchooser->axes);

  g_free (fontchooser->font_features);

  g_clear_object (&fontchooser->filter);

  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->finalize (object);
}

static gboolean
my_pango_font_family_equal (const char *familya,
                            const char *familyb)
{
  return g_ascii_strcasecmp (familya, familyb) == 0;
}

static gboolean
gtk_font_chooser_widget_ensure_matching_selection (GtkFontChooserWidget *self)
{
  const char *desc_family;
  guint i, n;

  desc_family = pango_font_description_get_family (self->font_desc);
  if (desc_family == NULL)
    {
      gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);
      return TRUE;
    }

  n = g_list_model_get_n_items (G_LIST_MODEL (self->selection));
  for (i = 0; i < n; i++)
    {
      gpointer item;
      PangoFontFace *face;
      PangoFontFamily *family;
      PangoFontDescription *merged;

      item = g_list_model_get_item (G_LIST_MODEL (self->selection), i);
      g_object_unref (item);

      if (PANGO_IS_FONT_FAMILY (item))
        {
          family = item;
          face = pango_font_family_get_face (family, NULL);
        }
      else
        {
          face = item;
          family = pango_font_face_get_family (face);
        }
      if (!my_pango_font_family_equal (desc_family, pango_font_family_get_name (family)))
        continue;

      merged = pango_font_face_describe (face);
      pango_font_description_merge_static (merged, self->font_desc, FALSE);

      if (pango_font_description_equal (merged, self->font_desc))
        {
          pango_font_description_free (merged);
          break;
        }

      pango_font_description_free (merged);
    }

  if (i < n)
    {
      gtk_single_selection_set_selected (self->selection, i);
      return TRUE;
    }

  return FALSE;
}

static PangoFontFace *
gtk_font_chooser_widget_get_face (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (chooser);
  gpointer item;

  item = gtk_single_selection_get_selected_item (self->selection);
  if (PANGO_IS_FONT_FAMILY (item))
    return pango_font_family_get_face (item, NULL);
  else
    return item;
}

static PangoFontFamily *
gtk_font_chooser_widget_get_family (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (chooser);
  gpointer item;

  item = gtk_single_selection_get_selected_item (self->selection);
  if (item == NULL)
    return NULL;

  if (PANGO_IS_FONT_FAMILY (item))
    return item;
  else
    return pango_font_face_get_family (item);
}

static int
gtk_font_chooser_widget_get_size (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  PangoFontDescription *desc = gtk_font_chooser_widget_get_font_desc (fontchooser);

  if (desc)
    return pango_font_description_get_size (desc);

  return -1;
}

static char *
gtk_font_chooser_widget_get_font (GtkFontChooserWidget *fontchooser)
{
  PangoFontDescription *desc = gtk_font_chooser_widget_get_font_desc (fontchooser);

  if (desc)
    return pango_font_description_to_string (desc);

  return NULL;
}

static PangoFontDescription *
gtk_font_chooser_widget_get_font_desc (GtkFontChooserWidget *self)
{
  if (gtk_single_selection_get_selected_item (self->selection))
    return self->font_desc;

  return NULL;
}

static void
gtk_font_chooser_widget_set_font (GtkFontChooserWidget *fontchooser,
                                  const char           *fontname)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (fontname);
  gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

/* OpenType variations */

static void
add_font_variations (GtkFontChooserWidget *fontchooser,
                     GString              *s)
{
  GHashTableIter iter;
  Axis *axis;

  g_hash_table_iter_init (&iter, fontchooser->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      double value;
      char buf[128];

      value = gtk_adjustment_get_value (axis->adjustment);

      if (value == axis->default_value)
        continue;

      hb_variation_to_string (&(hb_variation_t) { axis->tag, value }, buf, sizeof (buf));

      if (s->len > 0)
        g_string_append_c (s, ',');
      g_string_append (s, buf);
    }
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    Axis          *axis)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (axis->fontchooser);
  PangoFontDescription *font_desc;
  GString *s;

  fontchooser->updating_variations = TRUE;

  s = g_string_new ("");
  add_font_variations (fontchooser, s);

  font_desc = pango_font_description_new ();
  pango_font_description_set_variations (font_desc, s->str);
  gtk_font_chooser_widget_take_font_desc (fontchooser, font_desc);

  g_string_free (s, TRUE);

  fontchooser->updating_variations = FALSE;
}

static gboolean
should_show_axis (hb_ot_var_axis_info_t *ax)
{
  return (ax->flags & HB_OT_VAR_AXIS_FLAG_HIDDEN) == 0;
}

static gboolean
is_named_instance (hb_font_t *font)
{
  /* FIXME */
  return FALSE;
}

static struct {
  guint32 tag;
  const char *name;
} axis_names[] = {
  { HB_OT_TAG_VAR_AXIS_WIDTH,        NC_("Font variation axis", "Width") },
  { HB_OT_TAG_VAR_AXIS_WEIGHT,       NC_("Font variation axis", "Weight") },
  { HB_OT_TAG_VAR_AXIS_ITALIC,       NC_("Font variation axis", "Italic") },
  { HB_OT_TAG_VAR_AXIS_SLANT,        NC_("Font variation axis", "Slant") },
  { HB_OT_TAG_VAR_AXIS_OPTICAL_SIZE, NC_("Font variation axis", "Optical Size") },
};

static gboolean
add_axis (GtkFontChooserWidget  *fontchooser,
          hb_font_t             *hb_font,
          hb_ot_var_axis_info_t *ax,
          int                    value,
          int                    row)
{
  hb_face_t *hb_face;
  Axis *axis;
  const char *name;
  char buffer[20];
  unsigned int buffer_len = 20;
  int i;

  hb_face = hb_font_get_face (hb_font);

  axis = g_new (Axis, 1);
  axis->tag = ax->tag;
  axis->default_value = ax->default_value;
  axis->fontchooser = GTK_WIDGET (fontchooser);

  hb_ot_name_get_utf8 (hb_face, ax->name_id, HB_LANGUAGE_INVALID, &buffer_len, buffer);
  name = buffer;

  for (i = 0; i < G_N_ELEMENTS (axis_names); i++)
    {
      if (axis_names[i].tag == ax->tag)
        {
          name = g_dpgettext2 (NULL, "Font variation axis", axis_names[i].name);
          break;
        }
    }

  axis->label = gtk_label_new (name);

  gtk_widget_set_halign (axis->label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis->label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (fontchooser->axis_grid), axis->label, 0, row, 1, 1);
  axis->adjustment = gtk_adjustment_new ((double)value,
                                         (double)ax->min_value,
                                         (double)ax->max_value,
                                         1.0, 10.0, 0.0);
  axis->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, axis->adjustment);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (axis->scale),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, axis->label, NULL,
                                  -1);
  gtk_scale_add_mark (GTK_SCALE (axis->scale), (double)ax->default_value, GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis->scale, GTK_ALIGN_BASELINE_FILL);
  gtk_widget_set_hexpand (axis->scale, TRUE);
  gtk_widget_set_size_request (axis->scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis->scale), FALSE);
  gtk_grid_attach (GTK_GRID (fontchooser->axis_grid), axis->scale, 1, row, 1, 1);
  axis->spin = gtk_spin_button_new (axis->adjustment, 0, 0);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (axis->spin),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, axis->label, NULL,
                                  -1);
  g_signal_connect (axis->spin, "output", G_CALLBACK (output_cb), fontchooser);
  gtk_widget_set_valign (axis->spin, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (fontchooser->axis_grid), axis->spin, 2, row, 1, 1);

  g_hash_table_add (fontchooser->axes, axis);

  adjustment_changed (axis->adjustment, axis);
  g_signal_connect (axis->adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis);
  if (is_named_instance (hb_font) || !should_show_axis (ax))
    {
      gtk_widget_set_visible (axis->label, FALSE);
      gtk_widget_set_visible (axis->scale, FALSE);
      gtk_widget_set_visible (axis->spin, FALSE);

      return FALSE;
    }

  return TRUE;
}

#if HB_VERSION_ATLEAST (3, 3, 0)
static void
get_axes_and_values (hb_font_t             *font,
                     unsigned int           n_axes,
                     hb_ot_var_axis_info_t *axes,
                     float                 *coords)
{
  const float *dcoords;
  unsigned int length = n_axes;

  hb_ot_var_get_axis_infos (hb_font_get_face (font), 0, &length, axes);

  dcoords = hb_font_get_var_coords_design (font, &length);
  if (dcoords)
    memcpy (coords, dcoords, sizeof (float) * length);
  else
    {
      for (int i = 0; i < n_axes; i++)
        {
          hb_ot_var_axis_info_t *axis = &axes[i];
          coords[axis->axis_index] = axis->default_value;
        }
    }
}

#else

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
get_axes_and_values (hb_font_t             *font,
                     unsigned int           n_axes,
                     hb_ot_var_axis_info_t *axes,
                     float                 *coords)
{
  const int *ncoords;
  unsigned int length = n_axes;

  hb_ot_var_get_axis_infos (hb_font_get_face (font), 0, &length, axes);

  ncoords = hb_font_get_var_coords_normalized (font, &length);

  for (int i = 0; i < n_axes; i++)
    {
      hb_ot_var_axis_info_t *axis = &axes[i];
      int idx = axis->axis_index;
      if (ncoords)
        coords[idx] = denorm_coord (axis, ncoords[idx]);
      else
        coords[idx] = axis->default_value;
    }
}
#endif

static gboolean
gtk_font_chooser_widget_update_font_variations (GtkFontChooserWidget *fontchooser)
{
  PangoFont *pango_font;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  unsigned int n_axes;
  hb_ot_var_axis_info_t *axes;
  float *coords;
  gboolean has_axis = FALSE;
  int i;

  if (fontchooser->updating_variations)
    return FALSE;

  g_hash_table_foreach (fontchooser->axes, axis_remove, fontchooser);
  g_hash_table_remove_all (fontchooser->axes);

  if ((fontchooser->level & GTK_FONT_CHOOSER_LEVEL_VARIATIONS) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        fontchooser->font_desc);
  hb_font = pango_font_get_hb_font (pango_font);
  hb_face = hb_font_get_face (hb_font);

  if (!hb_ot_var_has_data (hb_face))
    return FALSE;

  n_axes = hb_ot_var_get_axis_count (hb_face);
  axes = g_alloca (sizeof (hb_ot_var_axis_info_t) * n_axes);
  coords = g_alloca (sizeof (float) * n_axes);
  get_axes_and_values (hb_font, n_axes, axes, coords);

  for (i = 0; i < n_axes; i++)
    {
      if (add_axis (fontchooser, hb_font, &axes[i], coords[axes[i].axis_index], i + 4))
        has_axis = TRUE;
    }

  g_object_unref (pango_font);

  return has_axis;
}

/* OpenType features */

/* look for a lang / script combination that matches the
 * language property and is supported by the hb_face. If
 * none is found, return the default lang / script tags.
 */
static void
find_language_and_script (GtkFontChooserWidget *fontchooser,
                          hb_face_t            *hb_face,
                          hb_tag_t             *lang_tag,
                          hb_tag_t             *script_tag)
{
  int i, j, k;
  hb_tag_t scripts[80];
  unsigned int n_scripts;
  unsigned int count;
  hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
  hb_language_t lang;
  const char *langname, *p;

  langname = pango_language_to_string (fontchooser->language);
  p = strchr (langname, '-');
  lang = hb_language_from_string (langname, p ? p - langname : -1);

  n_scripts = 0;
  for (i = 0; i < 2; i++)
    {
      count = G_N_ELEMENTS (scripts);
      hb_ot_layout_table_get_script_tags (hb_face, table[i], n_scripts, &count, scripts);
      n_scripts += count;
    }

  for (j = 0; j < n_scripts; j++)
    {
      hb_tag_t languages[80];
      unsigned int n_languages;

      n_languages = 0;
      for (i = 0; i < 2; i++)
        {
          count = G_N_ELEMENTS (languages);
          hb_ot_layout_script_get_language_tags (hb_face, table[i], j, n_languages, &count, languages);
          n_languages += count;
        }

      for (k = 0; k < n_languages; k++)
        {
          if (lang == hb_ot_tag_to_language (languages[k]))
            {
              *script_tag = scripts[j];
              *lang_tag = languages[k];
              return;
            }
        }
    }

  *lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  *script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
}

typedef struct {
  hb_tag_t tag;
  const char *name;
  GtkWidget *top;
  GtkWidget *feat;
  GtkWidget *example;
} FeatureItem;

static char *
get_feature_display_name (hb_tag_t tag)
{
  int i;
  char buf[5] = { 0, };

  hb_tag_to_string (tag, buf);

  if (buf[0] == 's' && buf[1] == 's' && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Stylistic Set %d"), num);
    }

  if (buf[0] == 'c' && buf[1] == 'v' && g_ascii_isdigit (buf[2]) && g_ascii_isdigit (buf[3]))
    {
      int num = (buf[2] - '0') * 10 + (buf[3] - '0');
      return g_strdup_printf (g_dpgettext2 (NULL, "OpenType layout", "Character Variant %d"), num);
    }

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_strdup (g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name));
    }

  return NULL;
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

static char *
find_affected_text (GtkFontChooserWidget *fontchooser,
                    hb_tag_t    feature_tag,
                    hb_font_t  *hb_font,
                    hb_tag_t    script_tag,
                    hb_tag_t    lang_tag,
                    int         max_chars)
{
  hb_face_t *hb_face;
  unsigned int script_index = 0;
  unsigned int lang_index = 0;
  unsigned int feature_index = 0;
  GString *chars;

  hb_face = hb_font_get_face (hb_font);

  chars = g_string_new ("");

  hb_ot_layout_table_find_script (hb_face, HB_OT_TAG_GSUB, script_tag, &script_index);

  hb_ot_layout_script_select_language (hb_face, HB_OT_TAG_GSUB, script_index, 1, &lang_tag, &lang_index);

  if (hb_ot_layout_language_find_feature (hb_face,
                                          HB_OT_TAG_GSUB,
                                          script_index,
                                          lang_index,
                                          feature_tag,
                                          &feature_index))
    {
      unsigned int lookup_indexes[32];
      unsigned int lookup_count = 32;
      unsigned int count;
      int n_chars = 0;

      count = hb_ot_layout_feature_get_characters (hb_face,
                                                   HB_OT_TAG_GSUB,
                                                   feature_index,
                                                   0,
                                                   NULL, NULL);
      if (count > 0)
        {
          hb_codepoint_t *ch;

          ch = g_alloca (sizeof (hb_codepoint_t) * count);
          hb_ot_layout_feature_get_characters (hb_face,
                                               HB_OT_TAG_GSUB,
                                               feature_index,
                                               0,
                                               &count,
                                               ch);

          for (unsigned int i = 0; i < MIN (count, max_chars); i++)
            g_string_append_unichar (chars, ch[i]);
        }
      else
        {
          count  = hb_ot_layout_feature_get_lookups (hb_face,
                                                     HB_OT_TAG_GSUB,
                                                     feature_index,
                                                     0,
                                                     &lookup_count,
                                                     lookup_indexes);
          if (count > 0)
            {
              hb_set_t *glyphs_before = NULL;
              hb_set_t *glyphs_after = NULL;
              hb_set_t *glyphs_output = NULL;
              hb_set_t *glyphs_input;
              hb_codepoint_t gid;
              char buf[5] = { 0, };

              hb_tag_to_string (feature_tag, buf);

              glyphs_input = hb_set_create ();

              for (int i = 0; i < count; i++)
                {
                  hb_ot_layout_lookup_collect_glyphs (hb_face,
                                                      HB_OT_TAG_GSUB,
                                                      lookup_indexes[i],
                                                      glyphs_before,
                                                      glyphs_input,
                                                      glyphs_after,
                                                      glyphs_output);
                }

              if (!fontchooser->glyphmap)
                {
                  fontchooser->glyphmap = hb_map_create ();
                  for (hb_codepoint_t ch = 0; ch < 0xffff; ch++)
                    {
                      hb_codepoint_t glyph = 0;
                      if (hb_font_get_nominal_glyph (hb_font, ch, &glyph) &&
                         !hb_map_has (fontchooser->glyphmap, glyph))
                        hb_map_set (fontchooser->glyphmap, glyph, ch);
                    }
                }

              gid = HB_SET_VALUE_INVALID;
              while (hb_set_next (glyphs_input, &gid))
                {
                  hb_codepoint_t ch;

                  if (n_chars == max_chars)
                    {
                      g_string_append (chars, "…");
                      break;
                    }
                  ch = hb_map_get (fontchooser->glyphmap, gid);
                  if (ch != HB_MAP_VALUE_INVALID)
                    {
                      g_string_append_unichar (chars, (gunichar)ch);
                      n_chars++;
                    }
                }

              hb_set_destroy (glyphs_input);
            }
        }
    }

  return g_string_free (chars, FALSE);
}

static char *
get_name (hb_face_t       *hb_face,
          hb_ot_name_id_t  id)
{
  unsigned int len;
  char *name;

  len = hb_ot_name_get_utf8 (hb_face, id, HB_LANGUAGE_INVALID, NULL, NULL);
  len++;
  name = g_new (char, len);
  hb_ot_name_get_utf8 (hb_face, id, HB_LANGUAGE_INVALID, &len, name);

  return name;
}

static void
update_feature_label (GtkFontChooserWidget *fontchooser,
                      FeatureItem          *item,
                      hb_font_t            *hb_font,
                      hb_tag_t              script_tag,
                      hb_tag_t              lang_tag)
{
  hb_face_t *hb_face;
  unsigned int script_index, lang_index, feature_index;
  hb_ot_name_id_t label_id, first_param_id;
  unsigned int num_params;
  const char *feat[] = { "salt", "swsh", "nalt", NULL };

  hb_face = hb_font_get_face (hb_font);

  if (!g_strv_contains (feat, item->name) &&
      (!(g_str_has_prefix (item->name, "ss") || g_str_has_prefix (item->name, "cv")) ||
       !g_ascii_isdigit (item->name[2]) || !g_ascii_isdigit (item->name[3])))
    return;

  hb_ot_layout_table_find_script (hb_face, HB_OT_TAG_GSUB, script_tag, &script_index);

  hb_ot_layout_script_select_language (hb_face, HB_OT_TAG_GSUB, script_index, 1, &lang_tag, &lang_index);

  if (hb_ot_layout_language_find_feature (hb_face, HB_OT_TAG_GSUB, script_index, lang_index, item->tag, &feature_index))
    {
      char *label;

      if (!hb_ot_layout_feature_get_name_ids (hb_face, HB_OT_TAG_GSUB, feature_index, &label_id, NULL, NULL, &num_params, &first_param_id))
        {
          label_id = HB_OT_NAME_ID_INVALID;
          num_params = 0;
        }

      if (label_id != HB_OT_NAME_ID_INVALID)
        label = get_name (hb_face, label_id);
      else
        label = get_feature_display_name (item->tag);

      if (GTK_IS_CHECK_BUTTON (item->feat))
        gtk_check_button_set_label (GTK_CHECK_BUTTON (item->feat), label);
      else
        {
          GtkWidget *l = gtk_widget_get_prev_sibling (item->feat);
          gtk_label_set_label (GTK_LABEL (l), label);
        }

      g_free (label);

      if (GTK_IS_DROP_DOWN (item->feat))
        {
          unsigned int n_lookups;
          GtkStringList *strings;
          unsigned int *lookups;
          unsigned int n_alternates;

          n_lookups = hb_ot_layout_feature_get_lookups (hb_face,
                                                        HB_OT_TAG_GSUB,
                                                        feature_index,
                                                        0,
                                                        NULL, NULL);

          lookups = g_alloca (sizeof (unsigned int) * n_lookups);
          hb_ot_layout_feature_get_lookups (hb_face,
                                            HB_OT_TAG_GSUB,
                                            feature_index,
                                            0,
                                            &n_lookups,
                                            lookups);

          n_alternates = 0;
          for (unsigned int l = 0; l < n_lookups; l++)
            {
              hb_set_t *glyphs;
              hb_codepoint_t glyph_index;
              unsigned int lookup = lookups[l];

              glyphs = hb_set_create ();

              hb_ot_layout_lookup_collect_glyphs (hb_face,
                                                  HB_OT_TAG_GSUB,
                                                  lookup,
                                                  NULL,
                                                  glyphs,
                                                  NULL, NULL);

              glyph_index = HB_SET_VALUE_INVALID;
              while (hb_set_next (glyphs, &glyph_index))
                n_alternates = MAX (n_alternates,
                                    hb_ot_layout_lookup_get_glyph_alternates (hb_face,
                                                                              lookup,
                                                                              glyph_index,
                                                                              0, NULL, NULL));

              hb_set_destroy (glyphs);
            }

          strings = gtk_string_list_new (NULL);
          gtk_string_list_append (strings, C_("Font feature value", "Default"));
          for (unsigned int i = 0; i < num_params; i++)
            {
              char *name;
              name = get_name (hb_face, first_param_id + i);
              gtk_string_list_append (strings, name);
              g_free (name);
            }

          for (int i = num_params; i < n_alternates; i++)
            {
              char buf[64];
              g_snprintf (buf, sizeof (buf), "%d", i + 1);
              gtk_string_list_append (strings, buf);
            }

          if (g_list_model_get_n_items (G_LIST_MODEL (strings)) == 1)
            gtk_string_list_append (strings, C_("Font feature value", "Enable"));

          gtk_drop_down_set_model (GTK_DROP_DOWN (item->feat), G_LIST_MODEL (strings));
          g_object_unref (strings);
        }
    }
  else
    {
      char *label = get_feature_display_name (item->tag);
      gtk_check_button_set_label (GTK_CHECK_BUTTON (item->feat), label);
      g_free (label);
    }
}

static void
update_feature_example (GtkFontChooserWidget *fontchooser,
                        FeatureItem          *item,
                        hb_font_t            *hb_font,
                        hb_tag_t              script_tag,
                        hb_tag_t              lang_tag,
                        PangoFontDescription *font_desc)
{
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxnc", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxns", "pnum", "tnum", NULL };
  const char *fraction[] = { "xxnf", "frac", "afrc", NULL };
  const char *char_variants[] = {
    "zero", "nalt",
    "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
    "cv01", "cv02", "cv03", "cv04", "cv05", "cv06", "cv07", "cv08", "cv09", "cv10",
    "cv11", "cv12", "cv13", "cv14", "cv15", "cv16", "cv17", "cv18", "cv19", "cv20",
    "cv21", "cv22", "cv23", "cv24", "cv25", "cv26", "cv27", "cv28", "cv29", "cv30",
    "cv31", "cv32", "cv33", "cv34", "cv35", "cv36", "cv37", "cv38", "cv39", "cv40",
    "cv41", "cv42", "cv43", "cv44", "cv45", "cv46", "cv47", "cv48", "cv49", "cv50",
    "cv51", "cv52", "cv53", "cv54", "cv55", "cv56", "cv57", "cv58", "cv59", "cv60",
    "cv61", "cv62", "cv63", "cv64", "cv65", "cv66", "cv67", "cv68", "cv69", "cv70",
    "cv71", "cv72", "cv73", "cv74", "cv75", "cv76", "cv77", "cv78", "cv79", "cv80",
    "cv81", "cv82", "cv83", "cv84", "cv85", "cv86", "cv87", "cv88", "cv89", "cv90",
    "cv91", "cv92", "cv93", "cv94", "cv95", "cv96", "cv97", "cv98", "cv99",
    NULL };

  if (g_strv_contains (number_case, item->name) ||
      g_strv_contains (number_spacing, item->name) ||
      g_strv_contains (fraction, item->name))
    {
      PangoAttrList *attrs;
      PangoAttribute *attr;
      PangoFontDescription *desc;
      char *str;

      attrs = pango_attr_list_new ();

      desc = pango_font_description_copy (font_desc);
      pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
      pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
      pango_font_description_free (desc);
      str = g_strconcat (item->name, " 1", NULL);
      attr = pango_attr_font_features_new (str);
      pango_attr_list_insert (attrs, attr);

      if (g_strv_contains (fraction, item->name))
        gtk_label_set_text (GTK_LABEL (item->example), "1/2 2/3 7/8");
      else
        gtk_label_set_text (GTK_LABEL (item->example), "0123456789");
      gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

      pango_attr_list_unref (attrs);
    }
  else if (g_strv_contains (letter_case, item->name) ||
           g_strv_contains (char_variants, item->name))
    {
      char *input = NULL;
      char *text;

      if (strcmp (item->name, "case") == 0)
        input = g_strdup ("A-B[Cq]");
      else if (g_strv_contains (letter_case, item->name))
        input = g_strdup ("AaBbCc…");
      else if (strcmp (item->name, "zero") == 0)
        input = g_strdup ("0");
      else
        input = find_affected_text (fontchooser, item->tag, hb_font, script_tag, lang_tag, 10);

      if (input[0] != '\0')
        {
          PangoAttrList *attrs;
          PangoAttribute *attr;
          PangoFontDescription *desc;
          char *str;

          text = g_strconcat (input, " → ", input, NULL);

          attrs = pango_attr_list_new ();

          desc = pango_font_description_copy (font_desc);
          pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
          pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
          pango_font_description_free (desc);
          str = g_strconcat (item->name, " 0", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = 0;
          attr->end_index = strlen (input);
          pango_attr_list_insert (attrs, attr);
          attr = pango_attr_fallback_new (FALSE);
          attr->start_index = 0;
          attr->end_index = strlen (input);
          pango_attr_list_insert (attrs, attr);
          g_free (str);
          str = g_strconcat (item->name, " 1", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = strlen (input) + strlen (" → ");
          attr->end_index = attr->start_index + strlen (input);
          pango_attr_list_insert (attrs, attr);
          attr = pango_attr_fallback_new (FALSE);
          attr->start_index = strlen (input) + strlen (" → ");
          attr->end_index = attr->start_index + strlen (input);
          pango_attr_list_insert (attrs, attr);
          g_free (str);

          gtk_label_set_text (GTK_LABEL (item->example), text);
          gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

          g_free (text);
          pango_attr_list_unref (attrs);
        }
      else
        gtk_label_set_markup (GTK_LABEL (item->example), "");
      g_free (input);
    }
}

static void
font_feature_toggled_cb (GtkCheckButton *check_button,
                         gpointer        user_data)
{
  GtkFontChooserWidget *fontchooser = user_data;

  set_inconsistent (check_button, FALSE);
  update_font_features (fontchooser);
}

static void
add_check_group (GtkFontChooserWidget  *fontchooser,
                 const char            *title,
                 const char           **tags,
                 unsigned int           n_tags)
{
  GtkWidget *label;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (group), label);

  for (i = 0; i < n_tags; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkGesture *gesture;
      GtkWidget *box;
      GtkWidget *example;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);

      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name);
      g_free (name);
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
      g_signal_connect (feat, "toggled", G_CALLBACK (font_feature_toggled_cb), fontchooser);
      g_signal_connect_swapped (feat, "notify::inconsistent", G_CALLBACK (update_font_features), fontchooser);

      gesture = gtk_gesture_click_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_signal_connect (gesture, "pressed", G_CALLBACK (feat_pressed), feat);
      gtk_widget_add_controller (feat, GTK_EVENT_CONTROLLER (gesture));

      example = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_box_append (GTK_BOX (box), feat);
      gtk_box_append (GTK_BOX (box), example);
      gtk_box_append (GTK_BOX (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      fontchooser->feature_items = g_list_prepend (fontchooser->feature_items, item);
    }

  gtk_box_append (GTK_BOX (fontchooser->feature_box), group);
}

static void
font_enum_feature_changed_cb (GObject    *object,
                              GParamSpec *pspec,
                              gpointer    data)
{
  GtkFontChooserWidget *fontchooser = data;

  update_font_features (fontchooser);
}

static void
add_enum_group (GtkFontChooserWidget  *fontchooser,
                const char            *title,
                const char           **tags,
                unsigned int           n_tags)
{
  GtkWidget *label = NULL;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (group), 6);
  gtk_grid_set_column_spacing (GTK_GRID (group), 12);

  if (title)
    {
      label = gtk_label_new (title);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
      gtk_label_set_attributes (GTK_LABEL (label), attrs);
      pango_attr_list_unref (attrs);
      gtk_grid_attach (GTK_GRID (group), label, 0, -1, 3, 1);
    }

  for (i = 0; i < n_tags; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkWidget *example;
      char *name;

      tag = hb_tag_from_string (tags[i], -1);

      name = get_feature_display_name (tag);
      label = gtk_label_new (name);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_grid_attach (GTK_GRID (group), label, 0, i, 1, 1);
      g_free (name);

      feat = gtk_drop_down_new (NULL, NULL);
      gtk_grid_attach (GTK_GRID (group), feat, 1, i, 1, 1);

      gtk_label_set_mnemonic_widget (GTK_LABEL (label), feat);

      g_signal_connect (feat, "notify::selected", G_CALLBACK (font_enum_feature_changed_cb), fontchooser);

      example = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      gtk_grid_attach (GTK_GRID (group), example, 2, i, 1, 1);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = NULL;
      item->feat = feat;
      item->example = example;

      fontchooser->feature_items = g_list_prepend (fontchooser->feature_items, item);
    }

  gtk_box_append (GTK_BOX (fontchooser->feature_box), group);
}

static void
add_radio_group (GtkFontChooserWidget  *fontchooser,
                 const char            *title,
                 const char           **tags,
                 unsigned int           n_tags)
{
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;
  PangoAttrList *attrs;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (group), label);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (group),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  for (i = 0; i < n_tags; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      char *name;
      GtkWidget *box;
      GtkWidget *example;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);
      feat = gtk_check_button_new_with_label (name ? name : _("Default"));
      g_free (name);
      if (group_button == NULL)
        group_button = feat;
      else
        gtk_check_button_set_group (GTK_CHECK_BUTTON (feat), GTK_CHECK_BUTTON (group_button));

      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_font_features), fontchooser);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      example = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_box_append (GTK_BOX (box), feat);
      gtk_box_append (GTK_BOX (box), example);
      gtk_box_append (GTK_BOX (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      fontchooser->feature_items = g_list_prepend (fontchooser->feature_items, item);
    }

  gtk_box_append (GTK_BOX (fontchooser->feature_box), group);
}

static void
gtk_font_chooser_widget_populate_features (GtkFontChooserWidget *fontchooser)
{
  const char *ligatures[] = { "liga", "dlig", "hlig", "clig" };
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case" };
  const char *number_case[] = { "xxnc", "lnum", "onum" };
  const char *number_spacing[] = { "xxns", "pnum", "tnum" };
  const char *fractions[] = { "xxnf", "frac", "afrc" };
  const char *style_variants[] = {
    "zero", "cswh", "calt", "falt", "hist", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
  };
  const char *style_variants2[] = {
    "swsh", "salt", "nalt"
  };
  const char *char_variants[] = {
    "cv01", "cv02", "cv03", "cv04", "cv05", "cv06", "cv07", "cv08", "cv09", "cv10",
    "cv11", "cv12", "cv13", "cv14", "cv15", "cv16", "cv17", "cv18", "cv19", "cv20",
    "cv21", "cv22", "cv23", "cv24", "cv25", "cv26", "cv27", "cv28", "cv29", "cv30",
    "cv31", "cv32", "cv33", "cv34", "cv35", "cv36", "cv37", "cv38", "cv39", "cv40",
    "cv41", "cv42", "cv43", "cv44", "cv45", "cv46", "cv47", "cv48", "cv49", "cv50",
    "cv51", "cv52", "cv53", "cv54", "cv55", "cv56", "cv57", "cv58", "cv59", "cv60",
    "cv61", "cv62", "cv63", "cv64", "cv65", "cv66", "cv67", "cv68", "cv69", "cv70",
    "cv71", "cv72", "cv73", "cv74", "cv75", "cv76", "cv77", "cv78", "cv79", "cv80",
    "cv81", "cv82", "cv83", "cv84", "cv85", "cv86", "cv87", "cv88", "cv89", "cv90",
    "cv91", "cv92", "cv93", "cv94", "cv95", "cv96", "cv97", "cv98", "cv99",
  };

  add_check_group (fontchooser, _("Ligatures"), ligatures, G_N_ELEMENTS (ligatures));
  add_check_group (fontchooser, _("Letter Case"), letter_case, G_N_ELEMENTS (letter_case));
  add_radio_group (fontchooser, _("Number Case"), number_case, G_N_ELEMENTS (number_case));
  add_radio_group (fontchooser, _("Number Spacing"), number_spacing, G_N_ELEMENTS (number_spacing));
  add_radio_group (fontchooser, _("Fractions"), fractions, G_N_ELEMENTS (fractions));
  add_check_group (fontchooser, _("Style Variations"), style_variants, G_N_ELEMENTS (style_variants));
  add_enum_group (fontchooser, NULL, style_variants2, G_N_ELEMENTS (style_variants2));
  add_enum_group (fontchooser, _("Character Variations"), char_variants, G_N_ELEMENTS (char_variants));

  update_font_features (fontchooser);
}

static gboolean
gtk_font_chooser_widget_update_font_features (GtkFontChooserWidget *fontchooser)
{
  PangoFont *pango_font;
  hb_font_t *hb_font;
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  guint script_index = 0;
  guint lang_index = 0;
  int i, j;
  GList *l;
  gboolean has_feature = FALSE;

  for (l = fontchooser->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      if (item->top)
        {
          gtk_widget_set_visible (item->top, FALSE);
          gtk_widget_set_visible (gtk_widget_get_parent (item->top), FALSE);
        }
      else
        {
          gtk_widget_set_visible (gtk_widget_get_parent (item->feat), FALSE);
          gtk_widget_set_visible (item->feat, FALSE);
          gtk_widget_set_visible (gtk_widget_get_prev_sibling (item->feat), FALSE);
          gtk_widget_set_visible (item->example, FALSE);
        }
    }

  if ((fontchooser->level & GTK_FONT_CHOOSER_LEVEL_FEATURES) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        fontchooser->font_desc);
  hb_font = pango_font_get_hb_font (pango_font);

  if (hb_font)
    {
      hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      hb_tag_t features[80];
      unsigned int count;
      unsigned int n_features;
      hb_tag_t *feat;

      hb_face = hb_font_get_face (hb_font);

      find_language_and_script (fontchooser, hb_face, &lang_tag, &script_tag);

      n_features = 0;
      for (i = 0; i < G_N_ELEMENTS (table); i++)
        {
          hb_ot_layout_table_find_script (hb_face, table[i], script_tag, &script_index);

          hb_ot_layout_script_select_language (hb_face, table[i], script_index, 1, &lang_tag, &lang_index);

          feat = features + n_features;
          count = G_N_ELEMENTS (features) - n_features;
          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  table[i],
                                                  script_index,
                                                  lang_index,
                                                  n_features,
                                                  &count,
                                                  feat);
          n_features += count;
        }

      for (j = 0; j < n_features; j++)
        {
          for (l = fontchooser->feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;

              if (strncmp (item->name, "xx", 2) == 0)
                {
                  update_feature_example (fontchooser, item, hb_font, script_tag, lang_tag, fontchooser->font_desc);
                  continue;
                }

              if (item->tag != features[j])
                continue;

              has_feature = TRUE;
              if (item->top)
                {
                  gtk_widget_set_visible (item->top, TRUE);
                  gtk_widget_set_visible (gtk_widget_get_parent (item->top), TRUE);
                }
              else
                {
                  gtk_widget_set_visible (gtk_widget_get_parent (item->feat), TRUE);
                  gtk_widget_set_visible (item->feat, TRUE);
                  gtk_widget_set_visible (gtk_widget_get_prev_sibling (item->feat), TRUE);
                  gtk_widget_set_visible (item->example, TRUE);
                }

              update_feature_label (fontchooser, item, hb_font, script_tag, lang_tag);
              update_feature_example (fontchooser, item, hb_font, script_tag, lang_tag, fontchooser->font_desc);

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

      if (fontchooser->glyphmap)
        {
          hb_map_destroy (fontchooser->glyphmap);
          fontchooser->glyphmap = NULL;
        }
    }

  g_object_unref (pango_font);

  return has_feature;
}

static void
update_font_features (GtkFontChooserWidget *fontchooser)
{
  GString *s;
  GList *l;
  char buf[128];

  s = g_string_new ("");

  for (l = fontchooser->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_CHECK_BUTTON (item->feat) && g_object_get_data (G_OBJECT (item->feat), "default"))
        {
          if (gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat)) &&
              strncmp (item->name, "xx", 2) != 0)
            {
              hb_feature_to_string (&(hb_feature_t) { item->tag, 1, 0, -1 }, buf, sizeof (buf));
              if (s->len > 0)
                g_string_append_c (s, ',');
              g_string_append (s, buf);
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          guint32 value;

          if (gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (item->feat)))
            continue;

          value = gtk_check_button_get_active (GTK_CHECK_BUTTON (item->feat));
          hb_feature_to_string (&(hb_feature_t) { item->tag, value, 0, -1 }, buf, sizeof (buf));
          if (s->len > 0)
            g_string_append_c (s, ',');
          g_string_append (s, buf);
        }
      else if (GTK_IS_DROP_DOWN (item->feat))
        {
          guint value;

          value = gtk_drop_down_get_selected (GTK_DROP_DOWN (item->feat));
          if (value == 0 || value == GTK_INVALID_LIST_POSITION)
            continue;

          hb_feature_to_string (&(hb_feature_t) { item->tag, value, 0, -1 }, buf, sizeof (buf));
          if (s->len > 0)
            g_string_append_c (s, ',');
          g_string_append (s, buf);
        }
    }

  if (g_strcmp0 (fontchooser->font_features, s->str) != 0)
    {
      g_free (fontchooser->font_features);
      fontchooser->font_features = g_string_free (s, FALSE);
      g_object_notify (G_OBJECT (fontchooser), "font-features");
    }
  else
    g_string_free (s, TRUE);

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);
}

static void
gtk_font_chooser_widget_merge_font_desc (GtkFontChooserWidget       *fontchooser,
                                         const PangoFontDescription *font_desc)
{
  PangoFontMask mask;

  g_assert (font_desc != NULL);
  /* iter may be NULL if the font doesn't exist on the list */

  mask = pango_font_description_get_set_fields (font_desc);

  /* sucky test, because we can't restrict the comparison to
   * only the parts that actually do get merged */
  if (pango_font_description_equal (font_desc, fontchooser->font_desc))
    return;

  pango_font_description_merge (fontchooser->font_desc, font_desc, TRUE);

  if (mask & PANGO_FONT_MASK_SIZE)
    {
      double font_size = (double) pango_font_description_get_size (fontchooser->font_desc) / PANGO_SCALE;
      /* XXX: This clamps, which can cause it to reloop into here, do we need
       * to block its signal handler?
       */
      gtk_range_set_value (GTK_RANGE (fontchooser->size_slider), font_size);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (fontchooser->size_spin), font_size);
    }
  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      gboolean has_tweak = FALSE;

      gtk_font_chooser_widget_update_marks (fontchooser);

      if (gtk_font_chooser_widget_update_font_features (fontchooser))
        has_tweak = TRUE;
      if (gtk_font_chooser_widget_update_font_variations (fontchooser))
        has_tweak = TRUE;

      g_simple_action_set_enabled (G_SIMPLE_ACTION (fontchooser->tweak_action), has_tweak);
    }

  if (mask & PANGO_FONT_MASK_VARIATIONS)
    {
      if (pango_font_description_get_variations (fontchooser->font_desc)[0] == '\0')
        pango_font_description_unset_fields (fontchooser->font_desc, PANGO_FONT_MASK_VARIANT);
    }

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");
}

static void
gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                        PangoFontDescription *font_desc)
{
  PangoFontMask mask;

  if (font_desc == NULL)
    font_desc = pango_font_description_from_string (GTK_FONT_CHOOSER_DEFAULT_FONT_NAME);

  mask = pango_font_description_get_set_fields (font_desc);
  gtk_font_chooser_widget_merge_font_desc (fontchooser, font_desc);

  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      gtk_single_selection_set_selected (fontchooser->selection, GTK_INVALID_LIST_POSITION);
      gtk_font_chooser_widget_ensure_matching_selection (fontchooser);
    }

  pango_font_description_free (font_desc);
}

static const char *
gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser)
{

  return fontchooser->preview_text;
}

static void
gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                          const char           *text)
{
  if (fontchooser->preview_text == text)
    return;

  g_free (fontchooser->preview_text);
  fontchooser->preview_text = g_strdup (text);

  gtk_editable_set_text (GTK_EDITABLE (fontchooser->preview), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");
}

static gboolean
gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser)
{
  return fontchooser->show_preview_entry;
}

static void
gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                gboolean              show_preview_entry)
{
  if (fontchooser->show_preview_entry != show_preview_entry)
    {
      fontchooser->show_preview_entry = show_preview_entry;

      gtk_widget_set_visible (fontchooser->preview, show_preview_entry);

      g_object_notify (G_OBJECT (fontchooser), "show-preview-entry");
    }
}

static void
gtk_font_chooser_widget_set_font_map (GtkFontChooser *chooser,
                                      PangoFontMap   *fontmap)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  if (g_set_object (&fontchooser->font_map, fontmap))
    {
      PangoContext *context;

      if (!fontmap)
        fontmap = pango_cairo_font_map_get_default ();

      context = gtk_widget_get_pango_context (fontchooser->family_face_list);
      pango_context_set_font_map (context, fontmap);

      context = gtk_widget_get_pango_context (fontchooser->preview);
      pango_context_set_font_map (context, fontmap);

      update_fontlist (fontchooser);
    }
}

static PangoFontMap *
gtk_font_chooser_widget_get_font_map (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);

  return fontchooser->font_map;
}

static gboolean
gtk_font_chooser_widget_filter_cb (gpointer item,
                                   gpointer data)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (data);
  PangoFontFamily *family;
  PangoFontFace *face;

  if (PANGO_IS_FONT_FAMILY (item))
    {
      family = item;
      face = pango_font_family_get_face (family, NULL);
    }
  else
    {
      face = item;
      family = pango_font_face_get_family (face);
    }

  return self->filter_func (family, face, self->filter_data);
}

static void
gtk_font_chooser_widget_set_filter_func (GtkFontChooser   *chooser,
                                         GtkFontFilterFunc filter,
                                         gpointer          data,
                                         GDestroyNotify    destroy)
{
  GtkFontChooserWidget *self = GTK_FONT_CHOOSER_WIDGET (chooser);

  if (self->filter_data_destroy)
    self->filter_data_destroy (self->filter_data);

  self->filter_func = filter;
  self->filter_data = data;
  self->filter_data_destroy = destroy;

  if (filter)
    {
      gtk_custom_filter_set_filter_func (self->custom_filter,
                                         gtk_font_chooser_widget_filter_cb,
                                         self,
                                         NULL);
    }
  else
    {
      gtk_custom_filter_set_filter_func (self->custom_filter, NULL, NULL, NULL);
    }
}

static void
gtk_font_chooser_widget_set_level (GtkFontChooserWidget *fontchooser,
                                   GtkFontChooserLevel   level)
{
  gboolean show_size;

  if (fontchooser->level == level)
    return;

  fontchooser->level = level;

  show_size = (level & GTK_FONT_CHOOSER_LEVEL_SIZE) != 0;

  gtk_widget_set_visible (fontchooser->size_label, show_size);
  gtk_widget_set_visible (fontchooser->size_slider, show_size);
  gtk_widget_set_visible (fontchooser->size_spin, show_size);
  gtk_widget_set_visible (fontchooser->size_label2, show_size);
  gtk_widget_set_visible (fontchooser->size_slider2, show_size);
  gtk_widget_set_visible (fontchooser->size_spin2, show_size);

  update_fontlist (fontchooser);

  g_object_notify (G_OBJECT (fontchooser), "level");
}

static GtkFontChooserLevel
gtk_font_chooser_widget_get_level (GtkFontChooserWidget *fontchooser)
{
  return fontchooser->level;
}

static void
gtk_font_chooser_widget_set_language (GtkFontChooserWidget *fontchooser,
                                      const char           *language)
{
  PangoLanguage *lang;

  lang = pango_language_from_string (language);
  if (fontchooser->language == lang)
    return;

  fontchooser->language = lang;
  g_object_notify (G_OBJECT (fontchooser), "language");

  gtk_font_chooser_widget_update_preview_attributes (fontchooser);
}

static void
gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_chooser_widget_get_family;
  iface->get_font_face = gtk_font_chooser_widget_get_face;
  iface->get_font_size = gtk_font_chooser_widget_get_size;
  iface->set_filter_func = gtk_font_chooser_widget_set_filter_func;
  iface->set_font_map = gtk_font_chooser_widget_set_font_map;
  iface->get_font_map = gtk_font_chooser_widget_get_font_map;
}

GAction *
gtk_font_chooser_widget_get_tweak_action (GtkWidget *widget)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (widget);

  return fontchooser->tweak_action;
}

void
gtk_font_chooser_widget_set_filter (GtkFontChooserWidget *widget,
                                    GtkFilter            *filter)
{
  if (widget->filter)
    gtk_multi_filter_remove (GTK_MULTI_FILTER (widget->multi_filter), 3);
  g_set_object (&widget->filter, filter);
  if (widget->filter)
    gtk_multi_filter_append (GTK_MULTI_FILTER (widget->multi_filter), g_object_ref (filter));
}
