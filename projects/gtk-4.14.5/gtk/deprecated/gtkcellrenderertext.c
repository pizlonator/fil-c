/* gtkcellrenderertext.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcellrenderertext.h"

#include "gtkcssnumbervalueprivate.h"
#include "gtkeditable.h"
#include "gtkentry.h"
#include "gtkentryprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "deprecated/gtkrender.h"
#include "gtkstylecontextprivate.h"
#include "deprecated/gtktreeprivate.h"

#include <stdlib.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRendererText:
 *
 * Renders text in a cell
 *
 * A `GtkCellRendererText` renders a given text in its cell, using the font, color and
 * style information provided by its properties. The text will be ellipsized if it is
 * too long and the `GtkCellRendererText:ellipsize` property allows it.
 *
 * If the `GtkCellRenderer:mode` is %GTK_CELL_RENDERER_MODE_EDITABLE,
 * the `GtkCellRendererText` allows to edit its text using an entry.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   You should use [class@Gtk.Inscription] or [class@Gtk.Label] instead
 */


static void gtk_cell_renderer_text_finalize   (GObject                  *object);

static void gtk_cell_renderer_text_get_property  (GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec);
static void gtk_cell_renderer_text_set_property  (GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec);
static void gtk_cell_renderer_text_snapshot   (GtkCellRenderer          *cell,
					       GtkSnapshot              *snapshot,
					       GtkWidget                *widget,
					       const GdkRectangle       *background_area,
					       const GdkRectangle       *cell_area,
					       GtkCellRendererState      flags);

static GtkCellEditable *gtk_cell_renderer_text_start_editing (GtkCellRenderer      *cell,
							      GdkEvent             *event,
							      GtkWidget            *widget,
							      const char           *path,
							      const GdkRectangle   *background_area,
							      const GdkRectangle   *cell_area,
							      GtkCellRendererState  flags);

static void       gtk_cell_renderer_text_get_preferred_width            (GtkCellRenderer       *cell,
                                                                         GtkWidget             *widget,
                                                                         int                   *minimal_size,
                                                                         int                   *natural_size);
static void       gtk_cell_renderer_text_get_preferred_height           (GtkCellRenderer       *cell,
                                                                         GtkWidget             *widget,
                                                                         int                   *minimal_size,
                                                                         int                   *natural_size);
static void       gtk_cell_renderer_text_get_preferred_height_for_width (GtkCellRenderer       *cell,
                                                                         GtkWidget             *widget,
                                                                         int                    width,
                                                                         int                   *minimum_height,
                                                                         int                   *natural_height);
static void       gtk_cell_renderer_text_get_aligned_area               (GtkCellRenderer       *cell,
									 GtkWidget             *widget,
									 GtkCellRendererState   flags,
									 const GdkRectangle    *cell_area,
									 GdkRectangle          *aligned_area);



enum {
  EDITED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_TEXT,
  PROP_MARKUP,
  PROP_ATTRIBUTES,
  PROP_SINGLE_PARAGRAPH_MODE,
  PROP_WIDTH_CHARS,
  PROP_MAX_WIDTH_CHARS,
  PROP_WRAP_WIDTH,
  PROP_ALIGN,
  PROP_PLACEHOLDER_TEXT,

  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_RGBA,
  PROP_FOREGROUND_RGBA,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_SCALE,
  PROP_EDITABLE,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_RISE,
  PROP_LANGUAGE,
  PROP_ELLIPSIZE,
  PROP_WRAP_MODE,

  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_SCALE_SET,
  PROP_EDITABLE_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET,
  PROP_LANGUAGE_SET,
  PROP_ELLIPSIZE_SET,
  PROP_ALIGN_SET,

  LAST_PROP
};

static guint text_cell_renderer_signals [LAST_SIGNAL];
static GParamSpec *text_cell_renderer_props [LAST_PROP];

#define GTK_CELL_RENDERER_TEXT_PATH "gtk-cell-renderer-text-path"

typedef struct _GtkCellRendererTextPrivate       GtkCellRendererTextPrivate;

struct _GtkCellRendererTextPrivate
{
  GtkWidget *entry;

  PangoAttrList        *extra_attrs;
  GdkRGBA               foreground;
  GdkRGBA               background;
  PangoAlignment        align;
  PangoEllipsizeMode    ellipsize;
  PangoFontDescription *font;
  PangoLanguage        *language;
  PangoUnderline        underline_style;
  PangoWrapMode         wrap_mode;

  char *text;
  char *placeholder_text;

  double font_scale;

  int rise;
  int fixed_height_rows;
  int width_chars;
  int max_width_chars;
  int wrap_width;

  guint in_entry_menu     : 1;
  guint strikethrough     : 1;
  guint editable          : 1;
  guint scale_set         : 1;
  guint foreground_set    : 1;
  guint background_set    : 1;
  guint underline_set     : 1;
  guint rise_set          : 1;
  guint strikethrough_set : 1;
  guint editable_set      : 1;
  guint calc_fixed_height : 1;
  guint single_paragraph  : 1;
  guint language_set      : 1;
  guint markup_set        : 1;
  guint ellipsize_set     : 1;
  guint align_set         : 1;

  gulong focus_out_id;
  gulong entry_menu_popdown_timeout;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererText, gtk_cell_renderer_text, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_text_init (GtkCellRendererText *celltext)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  GtkCellRenderer *cell = GTK_CELL_RENDERER (celltext);

  gtk_cell_renderer_set_alignment (cell, 0.0, 0.5);
  gtk_cell_renderer_set_padding (cell, 2, 2);
  priv->font_scale = 1.0;
  priv->fixed_height_rows = -1;
  priv->font = pango_font_description_new ();

  priv->width_chars = -1;
  priv->max_width_chars = -1;
  priv->wrap_width = -1;
  priv->wrap_mode = PANGO_WRAP_CHAR;
  priv->align = PANGO_ALIGN_LEFT;
  priv->align_set = FALSE;
}

static void
gtk_cell_renderer_text_class_init (GtkCellRendererTextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = gtk_cell_renderer_text_finalize;

  object_class->get_property = gtk_cell_renderer_text_get_property;
  object_class->set_property = gtk_cell_renderer_text_set_property;

  cell_class->snapshot = gtk_cell_renderer_text_snapshot;
  cell_class->start_editing = gtk_cell_renderer_text_start_editing;
  cell_class->get_preferred_width = gtk_cell_renderer_text_get_preferred_width;
  cell_class->get_preferred_height = gtk_cell_renderer_text_get_preferred_height;
  cell_class->get_preferred_height_for_width = gtk_cell_renderer_text_get_preferred_height_for_width;
  cell_class->get_aligned_area = gtk_cell_renderer_text_get_aligned_area;

  text_cell_renderer_props[PROP_TEXT] =
      g_param_spec_string ("text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_MARKUP] =
      g_param_spec_string ("markup", NULL, NULL,
                           NULL,
                           GTK_PARAM_WRITABLE);

  text_cell_renderer_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes", NULL, NULL,
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_SINGLE_PARAGRAPH_MODE] =
      g_param_spec_boolean ("single-paragraph-mode", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  text_cell_renderer_props[PROP_BACKGROUND] =
      g_param_spec_string ("background", NULL, NULL,
                           NULL,
                           GTK_PARAM_WRITABLE);

  /**
   * GtkCellRendererText:background-rgba:
   *
   * Background color as a `GdkRGBA`
   */
  text_cell_renderer_props[PROP_BACKGROUND_RGBA] =
      g_param_spec_boxed ("background-rgba", NULL, NULL,
                          GDK_TYPE_RGBA,
                          GTK_PARAM_READWRITE);
  text_cell_renderer_props[PROP_FOREGROUND] =
      g_param_spec_string ("foreground", NULL, NULL,
                           NULL,
                           GTK_PARAM_WRITABLE);

  /**
   * GtkCellRendererText:foreground-rgba:
   *
   * Foreground color as a `GdkRGBA`
   */
  text_cell_renderer_props[PROP_FOREGROUND_RGBA] =
      g_param_spec_boxed ("foreground-rgba", NULL, NULL,
                          GDK_TYPE_RGBA,
                          GTK_PARAM_READWRITE);


  text_cell_renderer_props[PROP_EDITABLE] =
      g_param_spec_boolean ("editable", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_FONT] =
      g_param_spec_string ("font", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", NULL, NULL,
                          PANGO_TYPE_FONT_DESCRIPTION,
                          GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_FAMILY] =
      g_param_spec_string ("family", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_STYLE] =
      g_param_spec_enum ("style", NULL, NULL,
                         PANGO_TYPE_STYLE,
                         PANGO_STYLE_NORMAL,
                         GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_VARIANT] =
      g_param_spec_enum ("variant", NULL, NULL,
                         PANGO_TYPE_VARIANT,
                         PANGO_VARIANT_NORMAL,
                         GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_WEIGHT] =
      g_param_spec_int ("weight", NULL, NULL,
                        0, G_MAXINT,
                        PANGO_WEIGHT_NORMAL,
                        GTK_PARAM_READWRITE);

   text_cell_renderer_props[PROP_STRETCH] =
       g_param_spec_enum ("stretch", NULL, NULL,
                          PANGO_TYPE_STRETCH,
                          PANGO_STRETCH_NORMAL,
                          GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_SIZE] =
      g_param_spec_int ("size", NULL, NULL,
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_SIZE_POINTS] =
      g_param_spec_double ("size-points", NULL, NULL,
                           0.0, G_MAXDOUBLE,
                           0.0,
                           GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_SCALE] =
      g_param_spec_double ("scale", NULL, NULL,
                           0.0, G_MAXDOUBLE,
                           1.0,
                           GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_RISE] =
      g_param_spec_int ("rise", NULL, NULL,
                        -G_MAXINT, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE);


  text_cell_renderer_props[PROP_STRIKETHROUGH] =
      g_param_spec_boolean ("strikethrough", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_UNDERLINE] =
      g_param_spec_enum ("underline", NULL, NULL,
                         PANGO_TYPE_UNDERLINE,
                         PANGO_UNDERLINE_NONE,
                         GTK_PARAM_READWRITE);

  text_cell_renderer_props[PROP_LANGUAGE] =
      g_param_spec_string ("language", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkCellRendererText:ellipsize:
   *
   * Specifies the preferred place to ellipsize the string, if the cell renderer
   * does not have enough room to display the entire string. Setting it to
   * %PANGO_ELLIPSIZE_NONE turns off ellipsizing. See the wrap-width property
   * for another way of making the text fit into a given width.
   */
  text_cell_renderer_props[PROP_ELLIPSIZE] =
      g_param_spec_enum ("ellipsize", NULL, NULL,
                         PANGO_TYPE_ELLIPSIZE_MODE,
                         PANGO_ELLIPSIZE_NONE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:width-chars:
   *
   * The desired width of the cell, in characters. If this property is set to
   * -1, the width will be calculated automatically, otherwise the cell will
   * request either 3 characters or the property value, whichever is greater.
   **/
  text_cell_renderer_props[PROP_WIDTH_CHARS] =
      g_param_spec_int ("width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:max-width-chars:
   *
   * The desired maximum width of the cell, in characters. If this property
   * is set to -1, the width will be calculated automatically.
   *
   * For cell renderers that ellipsize or wrap text; this property
   * controls the maximum reported width of the cell. The
   * cell should not receive any greater allocation unless it is
   * set to expand in its `GtkCellLayout` and all of the cell's siblings
   * have received their natural width.
   **/
  text_cell_renderer_props[PROP_MAX_WIDTH_CHARS] =
      g_param_spec_int ("max-width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:wrap-mode:
   *
   * Specifies how to break the string into multiple lines, if the cell
   * renderer does not have enough room to display the entire string.
   * This property has no effect unless the wrap-width property is set.
   */
  text_cell_renderer_props[PROP_WRAP_MODE] =
      g_param_spec_enum ("wrap-mode", NULL, NULL,
                         PANGO_TYPE_WRAP_MODE,
                         PANGO_WRAP_CHAR,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:wrap-width:
   *
   * Specifies the minimum width at which the text is wrapped. The wrap-mode property can
   * be used to influence at what character positions the line breaks can be placed.
   * Setting wrap-width to -1 turns wrapping off.
   */
  text_cell_renderer_props[PROP_WRAP_WIDTH] =
      g_param_spec_int ("wrap-width", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:alignment:
   *
   * Specifies how to align the lines of text with respect to each other.
   *
   * Note that this property describes how to align the lines of text in
   * case there are several of them. The "xalign" property of `GtkCellRenderer`,
   * on the other hand, sets the horizontal alignment of the whole text.
   */
  text_cell_renderer_props[PROP_ALIGN] =
      g_param_spec_enum ("alignment", NULL, NULL,
                         PANGO_TYPE_ALIGNMENT,
                         PANGO_ALIGN_LEFT,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCellRendererText:placeholder-text:
   *
   * The text that will be displayed in the `GtkCellRenderer` if
   * `GtkCellRendererText:editable` is %TRUE and the cell is empty.
   */
  text_cell_renderer_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) text_cell_renderer_props[propval] = g_param_spec_boolean (propname, nick, blurb, FALSE, GTK_PARAM_READWRITE)

  ADD_SET_PROP ("background-set", PROP_BACKGROUND_SET, NULL, NULL);

  ADD_SET_PROP ("foreground-set", PROP_FOREGROUND_SET, NULL, NULL);

  ADD_SET_PROP ("editable-set", PROP_EDITABLE_SET, NULL, NULL);

  ADD_SET_PROP ("family-set", PROP_FAMILY_SET, NULL, NULL);

  ADD_SET_PROP ("style-set", PROP_STYLE_SET, NULL, NULL);

  ADD_SET_PROP ("variant-set", PROP_VARIANT_SET, NULL, NULL);

  ADD_SET_PROP ("weight-set", PROP_WEIGHT_SET, NULL, NULL);

  ADD_SET_PROP ("stretch-set", PROP_STRETCH_SET, NULL, NULL);

  ADD_SET_PROP ("size-set", PROP_SIZE_SET, NULL, NULL);

  ADD_SET_PROP ("scale-set", PROP_SCALE_SET, NULL, NULL);

  ADD_SET_PROP ("rise-set", PROP_RISE_SET, NULL, NULL);

  ADD_SET_PROP ("strikethrough-set", PROP_STRIKETHROUGH_SET, NULL, NULL);

  ADD_SET_PROP ("underline-set", PROP_UNDERLINE_SET, NULL, NULL);

  ADD_SET_PROP ("language-set", PROP_LANGUAGE_SET, NULL, NULL);

  ADD_SET_PROP ("ellipsize-set", PROP_ELLIPSIZE_SET, NULL, NULL);

  ADD_SET_PROP ("align-set", PROP_ALIGN_SET, NULL, NULL);

  g_object_class_install_properties (object_class, LAST_PROP, text_cell_renderer_props);

  /**
   * GtkCellRendererText::edited:
   * @renderer: the object which received the signal
   * @path: the path identifying the edited cell
   * @new_text: the new text
   *
   * This signal is emitted after @renderer has been edited.
   *
   * It is the responsibility of the application to update the model
   * and store @new_text at the position indicated by @path.
   */
  text_cell_renderer_signals [EDITED] =
    g_signal_new (I_("edited"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkCellRendererTextClass, edited),
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
		  G_TYPE_STRING);
  g_signal_set_va_marshaller (text_cell_renderer_signals [EDITED],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gtk_marshal_VOID__STRING_STRINGv);
}

static void
gtk_cell_renderer_text_finalize (GObject *object)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  pango_font_description_free (priv->font);

  g_free (priv->text);
  g_free (priv->placeholder_text);

  if (priv->extra_attrs)
    pango_attr_list_unref (priv->extra_attrs);

  if (priv->language)
    g_object_unref (priv->language);

  g_clear_object (&priv->entry);

  G_OBJECT_CLASS (gtk_cell_renderer_text_parent_class)->finalize (object);
}

static PangoFontMask
get_property_font_set_mask (guint prop_id)
{
  switch (prop_id)
    {
    case PROP_FAMILY_SET:
      return PANGO_FONT_MASK_FAMILY;
    case PROP_STYLE_SET:
      return PANGO_FONT_MASK_STYLE;
    case PROP_VARIANT_SET:
      return PANGO_FONT_MASK_VARIANT;
    case PROP_WEIGHT_SET:
      return PANGO_FONT_MASK_WEIGHT;
    case PROP_STRETCH_SET:
      return PANGO_FONT_MASK_STRETCH;
    case PROP_SIZE_SET:
      return PANGO_FONT_MASK_SIZE;
    default:
      return 0;
    }
}

static void
gtk_cell_renderer_text_get_property (GObject        *object,
				     guint           param_id,
				     GValue         *value,
				     GParamSpec     *pspec)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  switch (param_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, priv->extra_attrs);
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
      g_value_set_boolean (value, priv->single_paragraph);
      break;

    case PROP_BACKGROUND_RGBA:
      g_value_set_boxed (value, &priv->background);
      break;

    case PROP_FOREGROUND_RGBA:
      g_value_set_boxed (value, &priv->foreground);
      break;

    case PROP_FONT:
        g_value_take_string (value, pango_font_description_to_string (priv->font));
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, priv->font);
      break;

    case PROP_FAMILY:
      g_value_set_string (value, pango_font_description_get_family (priv->font));
      break;

    case PROP_STYLE:
      g_value_set_enum (value, pango_font_description_get_style (priv->font));
      break;

    case PROP_VARIANT:
      g_value_set_enum (value, pango_font_description_get_variant (priv->font));
      break;

    case PROP_WEIGHT:
      g_value_set_int (value, pango_font_description_get_weight (priv->font));
      break;

    case PROP_STRETCH:
      g_value_set_enum (value, pango_font_description_get_stretch (priv->font));
      break;

    case PROP_SIZE:
      g_value_set_int (value, pango_font_description_get_size (priv->font));
      break;

    case PROP_SIZE_POINTS:
      g_value_set_double (value, ((double)pango_font_description_get_size (priv->font)) / (double)PANGO_SCALE);
      break;

    case PROP_SCALE:
      g_value_set_double (value, priv->font_scale);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, priv->strikethrough);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, priv->underline_style);
      break;

    case PROP_RISE:
      g_value_set_int (value, priv->rise);
      break;

    case PROP_LANGUAGE:
      g_value_set_static_string (value, pango_language_to_string (priv->language));
      break;

    case PROP_ELLIPSIZE:
      g_value_set_enum (value, priv->ellipsize);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, priv->wrap_mode);
      break;

    case PROP_WRAP_WIDTH:
      g_value_set_int (value, priv->wrap_width);
      break;

    case PROP_ALIGN:
      g_value_set_enum (value, priv->align);
      break;

    case PROP_BACKGROUND_SET:
      g_value_set_boolean (value, priv->background_set);
      break;

    case PROP_FOREGROUND_SET:
      g_value_set_boolean (value, priv->foreground_set);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      {
	PangoFontMask mask = get_property_font_set_mask (param_id);
	g_value_set_boolean (value, (pango_font_description_get_set_fields (priv->font) & mask) != 0);

	break;
      }

    case PROP_SCALE_SET:
      g_value_set_boolean (value, priv->scale_set);
      break;

    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, priv->editable_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, priv->strikethrough_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, priv->underline_set);
      break;

    case  PROP_RISE_SET:
      g_value_set_boolean (value, priv->rise_set);
      break;

    case PROP_LANGUAGE_SET:
      g_value_set_boolean (value, priv->language_set);
      break;

    case PROP_ELLIPSIZE_SET:
      g_value_set_boolean (value, priv->ellipsize_set);
      break;

    case PROP_ALIGN_SET:
      g_value_set_boolean (value, priv->align_set);
      break;

    case PROP_WIDTH_CHARS:
      g_value_set_int (value, priv->width_chars);
      break;

    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, priv->max_width_chars);
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, priv->placeholder_text);
      break;

    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    case PROP_MARKUP:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
set_bg_color (GtkCellRendererText *celltext,
              GdkRGBA             *rgba)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  if (rgba)
    {
      if (!priv->background_set)
        {
          priv->background_set = TRUE;
          g_object_notify_by_pspec (G_OBJECT (celltext), text_cell_renderer_props[PROP_BACKGROUND_SET]);
        }

      priv->background = *rgba;
    }
  else
    {
      if (priv->background_set)
        {
          priv->background_set = FALSE;
          g_object_notify_by_pspec (G_OBJECT (celltext), text_cell_renderer_props[PROP_BACKGROUND_SET]);
        }
    }
}

static void
set_fg_color (GtkCellRendererText *celltext,
              GdkRGBA             *rgba)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  if (rgba)
    {
      if (!priv->foreground_set)
        {
          priv->foreground_set = TRUE;
          g_object_notify_by_pspec (G_OBJECT (celltext), text_cell_renderer_props[PROP_FOREGROUND_SET]);
        }

      priv->foreground = *rgba;
    }
  else
    {
      if (priv->foreground_set)
        {
          priv->foreground_set = FALSE;
          g_object_notify_by_pspec (G_OBJECT (celltext), text_cell_renderer_props[PROP_FOREGROUND_SET]);
        }
    }
}

static PangoFontMask
set_font_desc_fields (PangoFontDescription *desc,
		      PangoFontMask         to_set)
{
  PangoFontMask changed_mask = 0;

  if (to_set & PANGO_FONT_MASK_FAMILY)
    {
      const char *family = pango_font_description_get_family (desc);
      if (!family)
	{
	  family = "sans";
	  changed_mask |= PANGO_FONT_MASK_FAMILY;
	}

      pango_font_description_set_family (desc, family);
    }
  if (to_set & PANGO_FONT_MASK_STYLE)
    pango_font_description_set_style (desc, pango_font_description_get_style (desc));
  if (to_set & PANGO_FONT_MASK_VARIANT)
    pango_font_description_set_variant (desc, pango_font_description_get_variant (desc));
  if (to_set & PANGO_FONT_MASK_WEIGHT)
    pango_font_description_set_weight (desc, pango_font_description_get_weight (desc));
  if (to_set & PANGO_FONT_MASK_STRETCH)
    pango_font_description_set_stretch (desc, pango_font_description_get_stretch (desc));
  if (to_set & PANGO_FONT_MASK_SIZE)
    {
      int size = pango_font_description_get_size (desc);
      if (size <= 0)
	{
	  size = 10 * PANGO_SCALE;
	  changed_mask |= PANGO_FONT_MASK_SIZE;
	}

      pango_font_description_set_size (desc, size);
    }

  return changed_mask;
}

static void
notify_set_changed (GObject       *object,
		    PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FAMILY_SET]);
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_STYLE_SET]);
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_VARIANT_SET]);
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_WEIGHT_SET]);
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_STRETCH_SET]);
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_SIZE_SET]);
}

static void
notify_fields_changed (GObject       *object,
		       PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FAMILY]);
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_STYLE]);
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_VARIANT]);
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_WEIGHT]);
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_STRETCH]);
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    {
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_SIZE]);
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_SIZE_POINTS]);
    }
}

static void
set_font_description (GtkCellRendererText  *celltext,
                      PangoFontDescription *font_desc)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  GObject *object = G_OBJECT (celltext);
  PangoFontDescription *new_font_desc;
  PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;

  if (font_desc)
    new_font_desc = pango_font_description_copy (font_desc);
  else
    new_font_desc = pango_font_description_new ();

  old_mask = pango_font_description_get_set_fields (priv->font);
  new_mask = pango_font_description_get_set_fields (new_font_desc);

  changed_mask = old_mask | new_mask;
  set_changed_mask = old_mask ^ new_mask;

  pango_font_description_free (priv->font);
  priv->font = new_font_desc;

  g_object_freeze_notify (object);

  g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FONT_DESC]);
  g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FONT]);

  notify_fields_changed (object, changed_mask);
  notify_set_changed (object, set_changed_mask);

  g_object_thaw_notify (object);
}

static void
gtk_cell_renderer_text_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  switch (param_id)
    {
    case PROP_TEXT:
      g_free (priv->text);

      if (priv->markup_set)
        {
          if (priv->extra_attrs)
            pango_attr_list_unref (priv->extra_attrs);
          priv->extra_attrs = NULL;
          priv->markup_set = FALSE;
        }

      priv->text = g_value_dup_string (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_ATTRIBUTES:
      if (priv->extra_attrs)
	pango_attr_list_unref (priv->extra_attrs);

      priv->extra_attrs = g_value_get_boxed (value);
      if (priv->extra_attrs)
        pango_attr_list_ref (priv->extra_attrs);
      break;
    case PROP_MARKUP:
      {
	const char *str;
	char *text = NULL;
	GError *error = NULL;
	PangoAttrList *attrs = NULL;

	str = g_value_get_string (value);
	if (str && !pango_parse_markup (str, -1, 0, &attrs, &text, NULL, &error))
	  {
	    g_warning ("Failed to set text from markup due to error parsing markup: %s",
		       error->message);
	    g_error_free (error);
	    return;
	  }

	g_free (priv->text);

	if (priv->extra_attrs)
	  pango_attr_list_unref (priv->extra_attrs);

	priv->text = text;
	priv->extra_attrs = attrs;
        priv->markup_set = TRUE;
      }
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
      if (priv->single_paragraph != g_value_get_boolean (value))
        {
          priv->single_paragraph = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_BACKGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_bg_color (celltext, NULL);       /* reset to background_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_bg_color (celltext, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));
      }
      break;

    case PROP_FOREGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_fg_color (celltext, NULL);       /* reset to foreground_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_fg_color (celltext, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));
      }
      break;

    case PROP_BACKGROUND_RGBA:
      set_bg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FOREGROUND_RGBA:
      set_fg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const char *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (celltext, font_desc);

	pango_font_description_free (font_desc);

	if (priv->fixed_height_rows != -1)
	  priv->calc_fixed_height = TRUE;
      }
      break;

    case PROP_FONT_DESC:
      set_font_description (celltext, g_value_get_boxed (value));

      if (priv->fixed_height_rows != -1)
	priv->calc_fixed_height = TRUE;
      break;

    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
      {
	PangoFontMask old_set_mask = pango_font_description_get_set_fields (priv->font);

	switch (param_id)
	  {
	  case PROP_FAMILY:
	    pango_font_description_set_family (priv->font,
					       g_value_get_string (value));
	    break;
	  case PROP_STYLE:
	    pango_font_description_set_style (priv->font,
					      g_value_get_enum (value));
	    break;
	  case PROP_VARIANT:
	    pango_font_description_set_variant (priv->font,
						g_value_get_enum (value));
	    break;
	  case PROP_WEIGHT:
	    pango_font_description_set_weight (priv->font,
					       g_value_get_int (value));
	    break;
	  case PROP_STRETCH:
	    pango_font_description_set_stretch (priv->font,
						g_value_get_enum (value));
	    break;
	  case PROP_SIZE:
	    pango_font_description_set_size (priv->font,
					     g_value_get_int (value));
	    g_object_notify_by_pspec (object, pspec);
	    break;
	  case PROP_SIZE_POINTS:
	    pango_font_description_set_size (priv->font,
					     g_value_get_double (value) * PANGO_SCALE);
	    g_object_notify_by_pspec (object, pspec);
	    break;
          default:
            break;
	  }

	if (priv->fixed_height_rows != -1)
	  priv->calc_fixed_height = TRUE;

	notify_set_changed (object, old_set_mask & pango_font_description_get_set_fields (priv->font));
	g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FONT_DESC]);
	g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_FONT]);

	break;
      }

    case PROP_SCALE:
      priv->font_scale = g_value_get_double (value);
      priv->scale_set = TRUE;
      if (priv->fixed_height_rows != -1)
	priv->calc_fixed_height = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_SCALE_SET]);
      break;

    case PROP_EDITABLE:
      priv->editable = g_value_get_boolean (value);
      priv->editable_set = TRUE;
      if (priv->editable)
        g_object_set (celltext, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
      else
        g_object_set (celltext, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_EDITABLE_SET]);
      break;

    case PROP_STRIKETHROUGH:
      priv->strikethrough = g_value_get_boolean (value);
      priv->strikethrough_set = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_STRIKETHROUGH_SET]);
      break;

    case PROP_UNDERLINE:
      priv->underline_style = g_value_get_enum (value);
      priv->underline_set = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_UNDERLINE_SET]);

      break;

    case PROP_RISE:
      priv->rise = g_value_get_int (value);
      priv->rise_set = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_RISE_SET]);
      if (priv->fixed_height_rows != -1)
	priv->calc_fixed_height = TRUE;
      break;

    case PROP_LANGUAGE:
      priv->language_set = TRUE;
      if (priv->language)
        g_object_unref (priv->language);
      priv->language = pango_language_from_string (g_value_get_string (value));
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_LANGUAGE_SET]);
      break;

    case PROP_ELLIPSIZE:
      priv->ellipsize = g_value_get_enum (value);
      priv->ellipsize_set = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_ELLIPSIZE_SET]);
      break;

    case PROP_WRAP_MODE:
      if (priv->wrap_mode != g_value_get_enum (value))
        {
          priv->wrap_mode = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_WRAP_WIDTH:
      if (priv->wrap_width != g_value_get_int (value))
        {
          priv->wrap_width = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_WIDTH_CHARS:
      if (priv->width_chars != g_value_get_int (value))
        {
          priv->width_chars  = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_MAX_WIDTH_CHARS:
      if (priv->max_width_chars != g_value_get_int (value))
        {
          priv->max_width_chars  = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_ALIGN:
      if (priv->align != g_value_get_enum (value))
        {
          priv->align = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      priv->align_set = TRUE;
      g_object_notify_by_pspec (object, text_cell_renderer_props[PROP_ALIGN_SET]);
      break;

    case PROP_BACKGROUND_SET:
      priv->background_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
      priv->foreground_set = g_value_get_boolean (value);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      if (!g_value_get_boolean (value))
	{
	  pango_font_description_unset_fields (priv->font,
					       get_property_font_set_mask (param_id));
	}
      else
	{
	  PangoFontMask changed_mask;

	  changed_mask = set_font_desc_fields (priv->font,
					       get_property_font_set_mask (param_id));
	  notify_fields_changed (G_OBJECT (celltext), changed_mask);
	}
      break;

    case PROP_SCALE_SET:
      priv->scale_set = g_value_get_boolean (value);
      break;

    case PROP_EDITABLE_SET:
      priv->editable_set = g_value_get_boolean (value);
      break;

    case PROP_STRIKETHROUGH_SET:
      priv->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_UNDERLINE_SET:
      priv->underline_set = g_value_get_boolean (value);
      break;

    case PROP_RISE_SET:
      priv->rise_set = g_value_get_boolean (value);
      break;

    case PROP_LANGUAGE_SET:
      priv->language_set = g_value_get_boolean (value);
      break;

    case PROP_ELLIPSIZE_SET:
      priv->ellipsize_set = g_value_get_boolean (value);
      break;

    case PROP_ALIGN_SET:
      priv->align_set = g_value_get_boolean (value);
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_free (priv->placeholder_text);
      priv->placeholder_text = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_text_new:
 *
 * Creates a new `GtkCellRendererText`. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with `GtkTreeViewColumn`,
 * you can bind a property to a value in a `GtkTreeModel`. For example,
 * you can bind the “text” property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the `GtkTreeView`.
 *
 * Returns: the new cell renderer
 *
 * Deprecated: 4.10
 **/
GtkCellRenderer *
gtk_cell_renderer_text_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, NULL);
}

static inline gboolean
show_placeholder_text (GtkCellRendererText *celltext)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);

  return priv->editable && priv->placeholder_text &&
    (!priv->text || !priv->text[0]);
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;

  pango_attr_list_insert (attr_list, attr);
}

static PangoLayout*
get_layout (GtkCellRendererText *celltext,
            GtkWidget           *widget,
            const GdkRectangle  *cell_area,
            GtkCellRendererState flags)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  PangoAttrList *attr_list;
  PangoLayout *layout;
  PangoUnderline uline;
  int xpad;
  gboolean placeholder_layout = show_placeholder_text (celltext);

  layout = gtk_widget_create_pango_layout (widget, placeholder_layout ?
                                           priv->placeholder_text : priv->text);

  gtk_cell_renderer_get_padding (GTK_CELL_RENDERER (celltext), &xpad, NULL);

  if (priv->extra_attrs)
    attr_list = pango_attr_list_copy (priv->extra_attrs);
  else
    attr_list = pango_attr_list_new ();

  pango_layout_set_single_paragraph_mode (layout, priv->single_paragraph);

  if (!placeholder_layout && cell_area)
    {
      /* Add options that affect appearance but not size */

      /* note that background doesn't go here, since it affects
       * background_area not the PangoLayout area
       */

      if (priv->foreground_set
	  && (flags & GTK_CELL_RENDERER_SELECTED) == 0)
        {
          PangoColor color;
          guint16 alpha;

          color.red = CLAMP (priv->foreground.red * 65535. + 0.5, 0, 65535);
          color.green = CLAMP (priv->foreground.green * 65535. + 0.5, 0, 65535);
          color.blue = CLAMP (priv->foreground.blue * 65535. + 0.5, 0, 65535);
          alpha = CLAMP (priv->foreground.alpha * 65535. + 0.5, 0, 65535);

          add_attr (attr_list,
                    pango_attr_foreground_new (color.red, color.green, color.blue));

          add_attr (attr_list, pango_attr_foreground_alpha_new (alpha));
        }

      if (priv->strikethrough_set)
        add_attr (attr_list, pango_attr_strikethrough_new (priv->strikethrough));
    }
  else if (placeholder_layout)
    {
      PangoColor color;
      guint16 alpha;
      GtkStyleContext *context;
      GdkRGBA fg = { 0.5, 0.5, 0.5, 1.0 };

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_lookup_color (context, "placeholder_text_color", &fg);

      color.red = CLAMP (fg.red * 65535. + 0.5, 0, 65535);
      color.green = CLAMP (fg.green * 65535. + 0.5, 0, 65535);
      color.blue = CLAMP (fg.blue * 65535. + 0.5, 0, 65535);
      alpha = CLAMP (fg.alpha * 65535. + 0.5, 0, 65535);

      add_attr (attr_list,
                pango_attr_foreground_new (color.red, color.green, color.blue));

      add_attr (attr_list, pango_attr_foreground_alpha_new (alpha));
    }

  add_attr (attr_list, pango_attr_font_desc_new (priv->font));

  if (priv->scale_set &&
      priv->font_scale != 1.0)
    add_attr (attr_list, pango_attr_scale_new (priv->font_scale));

  if (priv->underline_set)
    uline = priv->underline_style;
  else
    uline = PANGO_UNDERLINE_NONE;

  if (priv->language_set)
    add_attr (attr_list, pango_attr_language_new (priv->language));

  if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
    {
      switch (uline)
        {
        case PANGO_UNDERLINE_NONE:
          uline = PANGO_UNDERLINE_SINGLE;
          break;

        case PANGO_UNDERLINE_SINGLE:
          uline = PANGO_UNDERLINE_DOUBLE;
          break;

        case PANGO_UNDERLINE_SINGLE_LINE:
          uline = PANGO_UNDERLINE_DOUBLE_LINE;
          break;

        case PANGO_UNDERLINE_DOUBLE_LINE:
        case PANGO_UNDERLINE_ERROR_LINE:
          break;

        case PANGO_UNDERLINE_DOUBLE:
        case PANGO_UNDERLINE_LOW:
        case PANGO_UNDERLINE_ERROR:
        default:
          break;
        }
    }

  if (uline != PANGO_UNDERLINE_NONE)
    add_attr (attr_list, pango_attr_underline_new (priv->underline_style));

  if (priv->rise_set)
    add_attr (attr_list, pango_attr_rise_new (priv->rise));

  /* Now apply the attributes as they will effect the outcome
   * of pango_layout_get_extents() */
  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  if (priv->ellipsize_set)
    pango_layout_set_ellipsize (layout, priv->ellipsize);
  else
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_NONE);

  if (priv->wrap_width != -1)
    {
      PangoRectangle rect;
      int            width, text_width;

      pango_layout_get_extents (layout, NULL, &rect);
      text_width = rect.width;

      if (cell_area)
	width = (cell_area->width - xpad * 2) * PANGO_SCALE;
      else
	width = priv->wrap_width * PANGO_SCALE;

      width = MIN (width, text_width);

      pango_layout_set_width (layout, width);
      pango_layout_set_wrap (layout, priv->wrap_mode);
    }
  else
    {
      pango_layout_set_width (layout, -1);
      pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
    }

  if (priv->align_set)
    pango_layout_set_alignment (layout, priv->align);
  else
    {
      PangoAlignment align;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	align = PANGO_ALIGN_RIGHT;
      else
	align = PANGO_ALIGN_LEFT;

      pango_layout_set_alignment (layout, align);
    }

  return layout;
}


static void
get_size (GtkCellRenderer    *cell,
	  GtkWidget          *widget,
	  const GdkRectangle *cell_area,
	  PangoLayout        *layout,
	  int                *x_offset,
	  int                *y_offset,
	  int                *width,
	  int                *height)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  PangoRectangle rect;
  int xpad, ypad;
  int cell_width, cell_height;
  float xalign, yalign;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  if (priv->calc_fixed_height)
    {
      GtkStyleContext *style_context;
      PangoContext *context;
      PangoFontMetrics *metrics;
      PangoFontDescription *font_desc;
      int row_height;

      style_context = gtk_widget_get_style_context (widget);

      font_desc = gtk_css_style_get_pango_font (gtk_style_context_lookup_style (style_context));
      pango_font_description_merge_static (font_desc, priv->font, TRUE);

      if (priv->scale_set)
	pango_font_description_set_size (font_desc,
					 priv->font_scale * pango_font_description_get_size (font_desc));

      context = gtk_widget_get_pango_context (widget);

      metrics = pango_context_get_metrics (context,
					   font_desc,
					   pango_context_get_language (context));
      row_height = (pango_font_metrics_get_ascent (metrics) +
		    pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);

      pango_font_description_free (font_desc);

      gtk_cell_renderer_get_fixed_size (cell, &cell_width, &cell_height);

      gtk_cell_renderer_set_fixed_size (cell,
					cell_width, 2 * ypad +
					priv->fixed_height_rows * PANGO_PIXELS (row_height));

      if (height)
	{
	  *height = cell_height;
	  height = NULL;
	}
      priv->calc_fixed_height = FALSE;
      if (width == NULL)
	return;
    }

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

  rect.height = MIN (rect.height, cell_area->height - 2 * ypad);
  rect.width  = MIN (rect.width, cell_area->width - 2 * xpad);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    *x_offset = (1.0 - xalign) * (cell_area->width - (rect.width + (2 * xpad)));
  else
    *x_offset = xalign * (cell_area->width - (rect.width + (2 * xpad)));

  if ((priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE) || priv->wrap_width != -1)
    *x_offset = MAX(*x_offset, 0);

  *y_offset = yalign * (cell_area->height - (rect.height + (2 * ypad)));
  *y_offset = MAX (*y_offset, 0);

  if (height)
    *height = ypad * 2 + rect.height;

  if (width)
    *width = xpad * 2 + rect.width;
}

static void
gtk_cell_renderer_text_snapshot (GtkCellRenderer      *cell,
			         GtkSnapshot          *snapshot,
                                 GtkWidget            *widget,
                                 const GdkRectangle   *background_area,
                                 const GdkRectangle   *cell_area,
                                 GtkCellRendererState  flags)

{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  GtkStyleContext *context;
  PangoLayout *layout;
  int x_offset = 0;
  int y_offset = 0;
  int xpad, ypad;
  PangoRectangle rect;

  layout = get_layout (celltext, widget, cell_area, flags);
  get_size (cell, widget, cell_area, layout, &x_offset, &y_offset, NULL, NULL);
  context = gtk_widget_get_style_context (widget);

  if (priv->background_set && (flags & GTK_CELL_RENDERER_SELECTED) == 0)
    {
      gtk_snapshot_append_color (snapshot,
                                 &priv->background,
                                 &GRAPHENE_RECT_INIT(
                                     background_area->x, background_area->y,
                                     background_area->width, background_area->height
                                 ));
    }

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  if (priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE)
    pango_layout_set_width (layout,
			    (cell_area->width - x_offset - 2 * xpad) * PANGO_SCALE);
  else if (priv->wrap_width == -1)
    pango_layout_set_width (layout, -1);

  pango_layout_get_pixel_extents (layout, NULL, &rect);
  x_offset = x_offset - rect.x;

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                              cell_area->x, cell_area->y,
                              cell_area->width, cell_area->height
                          ));

  gtk_snapshot_render_layout (snapshot, context,
                              cell_area->x + x_offset + xpad,
                              cell_area->y + y_offset + ypad,
                              layout);

  gtk_snapshot_pop (snapshot);

  g_object_unref (layout);
}

static void
gtk_cell_renderer_text_editing_done (GtkCellEditable *entry,
				     gpointer         data)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (data);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  const char *path;
  const char *new_text;
  gboolean canceled;

  g_clear_object (&priv->entry);

  if (priv->focus_out_id > 0)
    {
      g_signal_handler_disconnect (entry, priv->focus_out_id);
      priv->focus_out_id = 0;
    }

  if (priv->entry_menu_popdown_timeout)
    {
      g_source_remove (priv->entry_menu_popdown_timeout);
      priv->entry_menu_popdown_timeout = 0;
    }

  g_object_get (entry,
                "editing-canceled", &canceled,
                NULL);
  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (data), canceled);

  if (canceled)
    return;

  path = g_object_get_data (G_OBJECT (entry), GTK_CELL_RENDERER_TEXT_PATH);
  new_text = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_signal_emit (data, text_cell_renderer_signals[EDITED], 0, path, new_text);
}

static void
gtk_cell_renderer_text_focus_changed (GtkWidget  *entry,
                                      GParamSpec *pspec,
                                      gpointer    data)
{
  if (gtk_widget_has_focus (entry) ||
      gtk_widget_has_focus (GTK_WIDGET (gtk_entry_get_text_widget (GTK_ENTRY (entry)))))
    return;

  g_object_set (entry,
                "editing-canceled", TRUE,
                NULL);
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
}

static GtkCellEditable *
gtk_cell_renderer_text_start_editing (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const char           *path,
				      const GdkRectangle   *background_area,
				      const GdkRectangle   *cell_area,
				      GtkCellRendererState  flags)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  float xalign, yalign;

  /* If the cell isn't editable we return NULL. */
  if (!priv->editable)
    return NULL;

  gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

  priv->entry = gtk_entry_new ();
  g_object_ref_sink (G_OBJECT (priv->entry));

  gtk_entry_set_has_frame (GTK_ENTRY (priv->entry), FALSE);
  gtk_entry_set_alignment (GTK_ENTRY (priv->entry), xalign);
  gtk_editable_set_width_chars (GTK_EDITABLE (priv->entry), 5);

  if (priv->text)
    gtk_editable_set_text (GTK_EDITABLE (priv->entry), priv->text);
  g_object_set_data_full (G_OBJECT (priv->entry), I_(GTK_CELL_RENDERER_TEXT_PATH), g_strdup (path), g_free);

  gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

  priv->in_entry_menu = FALSE;
  if (priv->entry_menu_popdown_timeout)
    {
      g_source_remove (priv->entry_menu_popdown_timeout);
      priv->entry_menu_popdown_timeout = 0;
    }

  g_signal_connect (priv->entry, "editing-done",
		    G_CALLBACK (gtk_cell_renderer_text_editing_done), celltext);
  priv->focus_out_id = g_signal_connect_after (priv->entry, "notify::has-focus",
					       G_CALLBACK (gtk_cell_renderer_text_focus_changed),
					       celltext);

  return GTK_CELL_EDITABLE (priv->entry);
}

/**
 * gtk_cell_renderer_text_set_fixed_height_from_font:
 * @renderer: A `GtkCellRendererText`
 * @number_of_rows: Number of rows of text each cell renderer is allocated, or -1
 *
 * Sets the height of a renderer to explicitly be determined by the “font” and
 * “y_pad” property set on it.  Further changes in these properties do not
 * affect the height, so they must be accompanied by a subsequent call to this
 * function.  Using this function is inflexible, and should really only be used
 * if calculating the size of a cell is too slow (ie, a massive number of cells
 * displayed).  If @number_of_rows is -1, then the fixed height is unset, and
 * the height is determined by the properties again.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_text_set_fixed_height_from_font (GtkCellRendererText *renderer,
						   int                  number_of_rows)
{
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (renderer);
  GtkCellRenderer *cell = GTK_CELL_RENDERER (renderer);

  g_return_if_fail (GTK_IS_CELL_RENDERER_TEXT (renderer));
  g_return_if_fail (number_of_rows == -1 || number_of_rows > 0);

  if (number_of_rows == -1)
    {
      int width, height;

      gtk_cell_renderer_get_fixed_size (cell, &width, &height);
      gtk_cell_renderer_set_fixed_size (cell, width, -1);
    }
  else
    {
      priv->fixed_height_rows = number_of_rows;
      priv->calc_fixed_height = TRUE;
    }
}

static void
gtk_cell_renderer_text_get_preferred_width (GtkCellRenderer *cell,
                                            GtkWidget       *widget,
                                            int             *minimum_size,
                                            int             *natural_size)
{
  GtkCellRendererText        *celltext = GTK_CELL_RENDERER_TEXT (cell);
  GtkCellRendererTextPrivate *priv = gtk_cell_renderer_text_get_instance_private (celltext);
  PangoLayout                *layout;
  PangoContext               *context;
  PangoFontMetrics           *metrics;
  PangoRectangle              rect;
  int char_width, text_width, ellipsize_chars, xpad;
  int min_width, nat_width;

  /* "width-chars" Hard-coded minimum width:
   *    - minimum size should be MAX (width-chars, strlen ("..."));
   *    - natural size should be MAX (width-chars, strlen (label->text));
   *
   * "wrap-width" User specified natural wrap width
   *    - minimum size should be MAX (width-chars, 0)
   *    - natural size should be MIN (wrap-width, strlen (label->text))
   */
  gtk_cell_renderer_get_padding (cell, &xpad, NULL);

  layout = get_layout (celltext, widget, NULL, 0);

  /* Fetch the length of the complete unwrapped text */
  pango_layout_set_width (layout, -1);
  pango_layout_get_extents (layout, NULL, &rect);
  text_width = rect.width;

  /* Fetch the average size of a character */
  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  char_width = pango_font_metrics_get_approximate_char_width (metrics);

  pango_font_metrics_unref (metrics);
  g_object_unref (layout);

  /* enforce minimum width for ellipsized labels at ~3 chars */
  if (priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE)
    ellipsize_chars = 3;
  else
    ellipsize_chars = 0;

  if ((priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE) || priv->width_chars > 0)
    min_width = xpad * 2 +
      MIN (PANGO_PIXELS_CEIL (text_width),
           (PANGO_PIXELS (char_width) * MAX (priv->width_chars, ellipsize_chars)));
  /* If no width-chars set, minimum for wrapping text will be the wrap-width */
  else if (priv->wrap_width > -1)
    min_width = xpad * 2 + rect.x + MIN (PANGO_PIXELS_CEIL (text_width), priv->wrap_width);
  else
    min_width = xpad * 2 + rect.x + PANGO_PIXELS_CEIL (text_width);

  if (priv->width_chars > 0)
    nat_width = xpad * 2 +
      MAX ((PANGO_PIXELS (char_width) * priv->width_chars), PANGO_PIXELS_CEIL (text_width));
  else
    nat_width = xpad * 2 + PANGO_PIXELS_CEIL (text_width);

  nat_width = MAX (nat_width, min_width);

  if (priv->max_width_chars > 0)
    {
      int max_width = xpad * 2 + PANGO_PIXELS (char_width) * priv->max_width_chars;

      min_width = MIN (min_width, max_width);
      nat_width = MIN (nat_width, max_width);
    }

  if (minimum_size)
    *minimum_size = min_width;

  if (natural_size)
    *natural_size = nat_width;
}

static void
gtk_cell_renderer_text_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                       GtkWidget       *widget,
                                                       int              width,
                                                       int             *minimum_height,
                                                       int             *natural_height)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  PangoLayout         *layout;
  int                  text_height, xpad, ypad;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  layout = get_layout (celltext, widget, NULL, 0);

  pango_layout_set_width (layout, (width - xpad * 2) * PANGO_SCALE);
  pango_layout_get_pixel_size (layout, NULL, &text_height);

  if (minimum_height)
    *minimum_height = text_height + ypad * 2;

  if (natural_height)
    *natural_height = text_height + ypad * 2;

  g_object_unref (layout);
}

static void
gtk_cell_renderer_text_get_preferred_height (GtkCellRenderer *cell,
                                             GtkWidget       *widget,
                                             int             *minimum_size,
                                             int             *natural_size)
{
  int min_width;

  /* Thankfully cell renderers dont rotate, so they only have to do
   * height-for-width and not the opposite. Here we have only to return
   * the height for the base minimum width of the renderer.
   *
   * Note this code path won't be followed by GtkTreeView which is
   * height-for-width specifically.
   */
  gtk_cell_renderer_get_preferred_width (cell, widget, &min_width, NULL);
  gtk_cell_renderer_text_get_preferred_height_for_width (cell, widget, min_width,
                                                         minimum_size, natural_size);
}

static void
gtk_cell_renderer_text_get_aligned_area (GtkCellRenderer       *cell,
					 GtkWidget             *widget,
					 GtkCellRendererState   flags,
					 const GdkRectangle    *cell_area,
					 GdkRectangle          *aligned_area)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  PangoLayout *layout;
  int x_offset = 0;
  int y_offset = 0;

  layout = get_layout (celltext, widget, cell_area, flags);
  get_size (cell, widget, cell_area, layout, &x_offset, &y_offset,
	    &aligned_area->width, &aligned_area->height);

  aligned_area->x = cell_area->x + x_offset;
  aligned_area->y = cell_area->y + y_offset;

  g_object_unref (layout);
}
