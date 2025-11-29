/*
 * Copyright © 2022 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkinscriptionprivate.h"

#include "gtkaccessibletextprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylechangeprivate.h"
#include "gtkpangoprivate.h"
#include "gtksnapshot.h"
#include "gtkrenderlayoutprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkatspipangoprivate.h"

/**
 * GtkInscription:
 *
 * `GtkInscription` is a widget to show text in a predefined area.
 *
 * You likely want to use `GtkLabel` instead as this widget is intended only
 * for a small subset of use cases. The main scenario envisaged is inside lists
 * such as `GtkColumnView`.
 *
 * While a `GtkLabel` sizes itself depending on the text that is displayed,
 * `GtkInscription` is given a size and inscribes the given text into that
 * space as well as it can.
 *
 * Users of this widget should take care to plan behaviour for the common case
 * where the text doesn't fit exactly in the allocated space.
 *
 * Since: 4.8
 */

/* 3 chars are enough to display ellipsizing "..." */
#define DEFAULT_MIN_CHARS 3
/* This means we request no natural size and fall back to min size */
#define DEFAULT_NAT_CHARS 0
/* 1 line is what people want in 90% of cases */
#define DEFAULT_MIN_LINES 1
/* This means we request no natural size and fall back to min size */
#define DEFAULT_NAT_LINES 0
/* Unlike GtkLabel, we default to not centering text */
#define DEFAULT_XALIGN 0.f
/* But just like GtkLabel, we center vertically */
#define DEFAULT_YALIGN 0.5f

struct _GtkInscription
{
  GtkWidget parent_instance;

  char *text;
  guint min_chars;
  guint nat_chars;
  guint min_lines;
  guint nat_lines;
  float xalign;
  float yalign;
  PangoAttrList *attrs;
  GtkInscriptionOverflow overflow;

  PangoLayout *layout;
};

enum
{
  PROP_0,
  PROP_ATTRIBUTES,
  PROP_MARKUP,
  PROP_MIN_CHARS,
  PROP_MIN_LINES,
  PROP_NAT_CHARS,
  PROP_NAT_LINES,
  PROP_TEXT,
  PROP_TEXT_OVERFLOW,
  PROP_WRAP_MODE,
  PROP_XALIGN,
  PROP_YALIGN,

  N_PROPS
};

static void     gtk_inscription_accessible_text_init (GtkAccessibleTextInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkInscription, gtk_inscription, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT,
                                                gtk_inscription_accessible_text_init))

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_inscription_dispose (GObject *object)
{
  GtkInscription *self = GTK_INSCRIPTION (object);

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (gtk_inscription_parent_class)->dispose (object);
}

static void
gtk_inscription_finalize (GObject *object)
{
  GtkInscription *self = GTK_INSCRIPTION (object);

  g_clear_object (&self->layout);

  G_OBJECT_CLASS (gtk_inscription_parent_class)->finalize (object);
}

static void
gtk_inscription_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkInscription *self = GTK_INSCRIPTION (object);

  switch (property_id)
    {
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, self->attrs);
      break;

    case PROP_MIN_CHARS:
      g_value_set_uint (value, self->min_chars);
      break;

    case PROP_MIN_LINES:
      g_value_set_uint (value, self->min_lines);
      break;

    case PROP_NAT_CHARS:
      g_value_set_uint (value, self->nat_chars);
      break;

    case PROP_NAT_LINES:
      g_value_set_uint (value, self->nat_lines);
      break;

    case PROP_TEXT:
      g_value_set_string (value, self->text);
      break;

    case PROP_TEXT_OVERFLOW:
      g_value_set_enum (value, self->overflow);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, pango_layout_get_wrap (self->layout));
      break;

    case PROP_XALIGN:
      g_value_set_float (value, self->xalign);
      break;

    case PROP_YALIGN:
      g_value_set_float (value, self->yalign);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_inscription_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkInscription *self = GTK_INSCRIPTION (object);

  switch (property_id)
    {
    case PROP_ATTRIBUTES:
      gtk_inscription_set_attributes (self, g_value_get_boxed (value));
      break;

    case PROP_MARKUP:
      gtk_inscription_set_markup (self, g_value_get_string (value));
      break;

    case PROP_MIN_CHARS:
      gtk_inscription_set_min_chars (self, g_value_get_uint (value));
      break;

    case PROP_MIN_LINES:
      gtk_inscription_set_min_lines (self, g_value_get_uint (value));
      break;

    case PROP_NAT_CHARS:
      gtk_inscription_set_nat_chars (self, g_value_get_uint (value));
      break;

    case PROP_NAT_LINES:
      gtk_inscription_set_nat_lines (self, g_value_get_uint (value));
      break;

    case PROP_TEXT:
      gtk_inscription_set_text (self, g_value_get_string (value));
      break;

    case PROP_TEXT_OVERFLOW:
      gtk_inscription_set_text_overflow (self, g_value_get_enum (value));
      break;

    case PROP_WRAP_MODE:
      gtk_inscription_set_wrap_mode (self, g_value_get_enum (value));
      break;

    case PROP_XALIGN:
      gtk_inscription_set_xalign (self, g_value_get_float (value));
      break;

    case PROP_YALIGN:
      gtk_inscription_set_yalign (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
update_pango_alignment (GtkInscription *self)
{
  PangoAlignment align;
  gboolean ltr;

  ltr = _gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL;

  if (self->xalign < 0.33)
    align = ltr ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
  else if (self->xalign < 0.67)
    align = PANGO_ALIGN_CENTER;
  else
    align = ltr ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;

  pango_layout_set_alignment (self->layout, align);
}

static void
gtk_inscription_update_layout_attributes (GtkInscription *self,
                                          PangoAttrList  *css_attrs)
{
  PangoAttrList *new_attrs;

  if (css_attrs == NULL)
    css_attrs = gtk_css_style_get_pango_attributes (gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (self))));

  new_attrs = css_attrs;

  new_attrs = _gtk_pango_attr_list_merge (new_attrs, self->attrs);

  pango_layout_set_attributes (self->layout, new_attrs);
  pango_attr_list_unref (new_attrs);
}

static void
gtk_inscription_css_changed (GtkWidget         *widget,
                             GtkCssStyleChange *change)
{
  GtkInscription *self = GTK_INSCRIPTION (widget);

  GTK_WIDGET_CLASS (gtk_inscription_parent_class)->css_changed (widget, change);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_ATTRS))
    {
      gtk_inscription_update_layout_attributes (self,
          gtk_css_style_get_pango_attributes (gtk_css_style_change_get_new_style (change)));

      gtk_widget_queue_draw (widget);
    }
}

static void
gtk_inscription_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  GtkInscription *self = GTK_INSCRIPTION (widget);

  GTK_WIDGET_CLASS (gtk_inscription_parent_class)->direction_changed (widget, previous_direction);

  update_pango_alignment (self);
}

static PangoFontMetrics *
gtk_inscription_get_font_metrics (GtkInscription *self)
{
  PangoContext *context;

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));

  return pango_context_get_metrics (context, NULL, NULL);
}

static int
get_char_pixels (GtkInscription *self)
{
  int char_width, digit_width;
  PangoFontMetrics *metrics;

  metrics = gtk_inscription_get_font_metrics (self);
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  return MAX (char_width, digit_width);
}

static void
gtk_inscription_measure_width (GtkInscription *self,
                               int            *minimum,
                               int            *natural)
{
  int char_pixels = get_char_pixels (self);

  if (self->min_chars == 0 && self->nat_chars == 0)
    return;

  *minimum = self->min_chars * char_pixels;
  *natural = MAX (self->min_chars, self->nat_chars) * char_pixels;
}

static int
get_line_pixels (GtkInscription *self,
                 int            *baseline)
{
  PangoFontMetrics *metrics;
  int ascent, descent;

  metrics = gtk_inscription_get_font_metrics (self);
  ascent = pango_font_metrics_get_ascent (metrics);
  descent = pango_font_metrics_get_descent (metrics);
  pango_font_metrics_unref (metrics);

  if (baseline)
    *baseline = ascent;

  return ascent + descent;
}

static void
gtk_inscription_measure_height (GtkInscription *self,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  int line_pixels, baseline;

  if (self->min_lines == 0 && self->nat_lines == 0)
    return;

  line_pixels = get_line_pixels (self, &baseline);

  *minimum = self->min_lines * line_pixels;
  *natural = MAX (self->min_lines, self->nat_lines) * line_pixels;

  if (minimum_baseline)
    *minimum_baseline = self->min_lines ? baseline : 0;
  if (natural_baseline)
    *natural_baseline = (self->min_lines || self->nat_lines) ? baseline : 0;
}

static void
gtk_inscription_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkInscription *self = GTK_INSCRIPTION (widget);

  /* We split this into 2 functions explicitly, so nobody gets the idea
   * of adding height-for-width to this.
   * This is meant to be fast, so that is a big no-no.
   */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_inscription_measure_width (self, minimum, natural);
  else
    gtk_inscription_measure_height (self, minimum, natural, minimum_baseline, natural_baseline);

  *minimum = PANGO_PIXELS_CEIL (*minimum);
  *natural = PANGO_PIXELS_CEIL (*natural);
  if (*minimum_baseline > 0)
    *minimum_baseline = PANGO_PIXELS_CEIL (*minimum_baseline);
  if (*natural_baseline > 0)
    *natural_baseline = PANGO_PIXELS_CEIL (*natural_baseline);
}

static void
gtk_inscription_get_layout_location (GtkInscription *self,
                                     float          *x_out,
                                     float          *y_out)
{
  GtkWidget *widget = GTK_WIDGET (self);
  const int widget_width = gtk_widget_get_width (widget);
  const int widget_height = gtk_widget_get_height (widget);
  PangoRectangle logical;
  float xalign;
  int baseline;
  float x, y;

  g_assert (x_out);
  g_assert (y_out);

  xalign = self->xalign;
  if (_gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    xalign = 1.0 - xalign;

  pango_layout_get_pixel_extents (self->layout, NULL, &logical);
  if (pango_layout_get_width (self->layout) > 0)
    x = 0.f;
  else
    x = floor ((xalign * (widget_width - logical.width)) - logical.x);

  baseline = gtk_widget_get_baseline (widget);
  if (baseline != -1)
    {
      int layout_baseline = pango_layout_get_baseline (self->layout) / PANGO_SCALE;
      /* yalign is 0 because we can't support yalign while baseline aligning */
      y = baseline - layout_baseline;
    }
  else if (pango_layout_is_ellipsized (self->layout))
    {
      y = 0.f;
    }
  else
    {
      y = floor ((widget_height - logical.height) * self->yalign);
      if (y < 0)
        y = 0.f;
    }

  *x_out = x;
  *y_out = y;
}

static void
gtk_inscription_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkInscription *self = GTK_INSCRIPTION (widget);

  pango_layout_set_width (self->layout, width * PANGO_SCALE);

  switch (self->overflow)
    {
    case GTK_INSCRIPTION_OVERFLOW_CLIP:
      pango_layout_set_height (self->layout, -1);
      /* figure out if we're single line (clip horizontally)
       * or multiline (clip vertically):
       * If we can't fit 2 rows, we're single line.
       */
      {
        PangoLayoutIter *iter = pango_layout_get_iter (self->layout);
        if (pango_layout_iter_next_line (iter))
          {
            PangoRectangle rect;
            pango_layout_iter_get_line_extents (iter, NULL, &rect);
            if (rect.y + rect.height > height * PANGO_SCALE)
              {
                while (!pango_layout_line_is_paragraph_start (pango_layout_iter_get_line_readonly (iter)))
                  {
                    if (!pango_layout_iter_next_line (iter))
                      break;
                  }
                if (!pango_layout_line_is_paragraph_start (pango_layout_iter_get_line_readonly (iter)))
                  {
                    pango_layout_set_width (self->layout, -1);
                  }
              }
          }
        pango_layout_iter_free (iter);
      }
      break;

    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_START:
    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_MIDDLE:
    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_END:
      pango_layout_set_height (self->layout, height * PANGO_SCALE);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

static void
gtk_inscription_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkInscription *self = GTK_INSCRIPTION (widget);
  float lx, ly;
  GtkCssBoxes boxes;

  if (!self->text || (*self->text == '\0'))
    return;

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT(0, 0, gtk_widget_get_width (widget), gtk_widget_get_height (widget)));
  gtk_inscription_get_layout_location (self, &lx, &ly);

  gtk_css_boxes_init (&boxes, widget);
  gtk_css_style_snapshot_layout (&boxes, snapshot, lx, ly, self->layout);

  gtk_snapshot_pop (snapshot);
}

static void
gtk_inscription_class_init (GtkInscriptionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = gtk_inscription_dispose;
  gobject_class->finalize = gtk_inscription_finalize;
  gobject_class->get_property = gtk_inscription_get_property;
  gobject_class->set_property = gtk_inscription_set_property;

  widget_class->css_changed = gtk_inscription_css_changed;
  widget_class->direction_changed = gtk_inscription_direction_changed;
  widget_class->measure = gtk_inscription_measure;
  widget_class->size_allocate = gtk_inscription_allocate;
  widget_class->snapshot = gtk_inscription_snapshot;

  /**
   * GtkInscription:attributes: (attributes org.gtk.Property.get=gtk_inscription_get_attributes org.gtk.Property.set=gtk_inscription_set_attributes)
   *
   * A list of style attributes to apply to the text of the inscription.
   *
   * Since: 4.8
   */
  properties[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes", NULL, NULL,
                          PANGO_TYPE_ATTR_LIST,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:markup: (attributes org.gtk.Property.set=gtk_inscription_set_markup)
   *
   * Utility property that sets both the [property@Gtk.Inscription:text] and
   * [property@Gtk.Inscription:attributes] properties, mainly intended for use in
   * GtkBuilder ui files to ease translation support and bindings.
   *
   * This function uses [func@Pango.parse_markup] to parse the markup into text and
   * attributes. The markup must be valid. If you cannot ensure that, consider using
   * [func@Pango.parse_markup] and setting the two properties yourself.
   *
   * Since: 4.8
   */
  properties[PROP_MARKUP] =
    g_param_spec_string ("markup", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:min-chars: (attributes org.gtk.Property.get=gtk_inscription_get_min_chars org.gtk.Property.set=gtk_inscription_set_min_chars)
   *
   * The number of characters that should fit into the inscription at minimum.
   *
   * This influences the requested width, not the width actually given to the widget,
   * which might turn out to be larger.
   *
   * Note that this is an approximate character width, so some characters might be
   * wider and some might be thinner, so do not expect the number of characters to
   * exactly match.
   *
   * If you set this property to 0, the inscription will not request any width at all
   * and its width will be determined entirely by its surroundings.
   *
   * Since: 4.8
   */
  properties[PROP_MIN_CHARS] =
      g_param_spec_uint ("min-chars", NULL, NULL,
                         0, G_MAXUINT,
                         DEFAULT_MIN_CHARS,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:min-lines: (attributes org.gtk.Property.get=gtk_inscription_get_min_lines org.gtk.Property.set=gtk_inscription_set_min_lines)
   *
   * The number of lines that should fit into the inscription at minimum.
   *
   * This influences the requested height, not the height actually given to the widget,
   * which might turn out to be larger.
   *
   * Note that this is an approximate line height, so if the text uses things like fancy
   * Unicode or attribute that influence the height, the text might not fit.
   *
   * If you set this property to 0, the inscription will not request any height at all
   * and its height will be determined entirely by its surroundings.
   *
   * Since: 4.8
   */
  properties[PROP_MIN_LINES] =
      g_param_spec_uint ("min-lines", NULL, NULL,
                         0, G_MAXUINT,
                         DEFAULT_MIN_LINES,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:nat-chars: (attributes org.gtk.Property.get=gtk_inscription_get_nat_chars org.gtk.Property.set=gtk_inscription_set_nat_chars)
   *
   * The number of characters that should ideally fit into the inscription.
   *
   * This influences the requested width, not the width actually given to the widget.
   * The widget might turn out larger as well as smaller.
   *
   * If this property is set to a value smaller than [property@Gtk.Inscription:min-chars],
   * that value will be used. In particular, for the default value of 0, this will always
   * be the case.
   *
   * Since: 4.8
   */
  properties[PROP_NAT_CHARS] =
      g_param_spec_uint ("nat-chars", NULL, NULL,
                         0, G_MAXUINT,
                         DEFAULT_NAT_CHARS,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:nat-lines: (attributes org.gtk.Property.get=gtk_inscription_get_nat_lines org.gtk.Property.set=gtk_inscription_set_nat_lines)
   *
   * The number of lines that should ideally fit into the inscription.
   *
   * This influences the requested height, not the height actually given to the widget.
   * The widget might turn out larger as well as smaller.
   *
   * If this property is set to a value smaller than [property@Gtk.Inscription:min-lines],
   * that value will be used. In particular, for the default value of 0, this will always
   * be the case.
   *
   * Since: 4.8
   */
  properties[PROP_NAT_LINES] =
      g_param_spec_uint ("nat-lines", NULL, NULL,
                         0, G_MAXUINT,
                         DEFAULT_NAT_LINES,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:text: (attributes org.gtk.Property.get=gtk_inscription_get_text org.gtk.Property.set=gtk_inscription_set_text)
   *
   * The displayed text.
   *
   * Since: 4.8
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:text-overflow: (attributes org.gtk.Property.get=gtk_inscription_get_text_overflow org.gtk.Property.set=gtk_inscription_set_text_overflow)
   *
   * The overflow method to use for the text.
   *
   * Since: 4.8
   */
  properties[PROP_TEXT_OVERFLOW] =
    g_param_spec_enum ("text-overflow", NULL, NULL,
                       GTK_TYPE_INSCRIPTION_OVERFLOW,
                       GTK_INSCRIPTION_OVERFLOW_CLIP,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:wrap-mode: (attributes org.gtk.Property.get=gtk_inscription_get_wrap_mode org.gtk.Property.set=gtk_inscription_set_wrap_mode)
   *
   * Controls how the line wrapping is done.
   *
   * Note that unlike `GtkLabel`, the default here is %PANGO_WRAP_WORD_CHAR.
   *
   * Since: 4.8
   */
  properties[PROP_WRAP_MODE] =
      g_param_spec_enum ("wrap-mode", NULL, NULL,
                         PANGO_TYPE_WRAP_MODE,
                         PANGO_WRAP_WORD_CHAR,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:xalign: (attributes org.gtk.Property.get=gtk_inscription_get_xalign org.gtk.Property.set=gtk_inscription_set_xalign)
   *
   * The horizontal alignment of the text inside the allocated size.
   *
   * Compare this to [property@Gtk.Widget:halign], which determines how the
   * inscription's size allocation is positioned in the available space.
   *
   * Since: 4.8
   */
  properties[PROP_XALIGN] =
      g_param_spec_float ("xalign", NULL, NULL,
                          0.0, 1.0,
                          DEFAULT_XALIGN,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInscription:yalign: (attributes org.gtk.Property.get=gtk_inscription_get_yalign org.gtk.Property.set=gtk_inscription_set_yalign)
   *
   * The vertical alignment of the text inside the allocated size.
   *
   * Compare this to [property@Gtk.Widget:valign], which determines how the
   * inscription's size allocation is positioned in the available space.
   *
   * Since: 4.8
   */
  properties[PROP_YALIGN] =
      g_param_spec_float ("yalign", NULL, NULL,
                          0.0, 1.0,
                          DEFAULT_YALIGN,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "label");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LABEL);
}

static void
gtk_inscription_init (GtkInscription *self)
{
  self->min_chars = DEFAULT_MIN_CHARS;
  self->nat_chars = DEFAULT_NAT_CHARS;
  self->min_lines = DEFAULT_MIN_LINES;
  self->nat_lines = DEFAULT_NAT_LINES;
  self->xalign = DEFAULT_XALIGN;
  self->yalign = DEFAULT_YALIGN;

  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
  pango_layout_set_wrap (self->layout, PANGO_WRAP_WORD_CHAR);
  update_pango_alignment (self);
}

/* for a11y */
PangoLayout *
gtk_inscription_get_layout (GtkInscription *self)
{
  return self->layout;
}

/**
 * gtk_inscription_new:
 * @text: (nullable): The text to display.
 *
 * Creates a new `GtkInscription` with the given text.
 *
 * Returns: a new `GtkInscription`
 *
 * Since: 4.8
 */
GtkWidget *
gtk_inscription_new (const char *text)
{
  return g_object_new (GTK_TYPE_INSCRIPTION,
                       "text", text,
                       NULL);
}

/**
 * gtk_inscription_set_text: (attributes org.gtk.Method.set_property=text)
 * @self: a `GtkInscription`
 * @text: (nullable): The text to display
 *
 * Sets the text to be displayed.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_text (GtkInscription *self,
                          const char  *text)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_free (self->text);
  self->text = g_strdup (text);

  pango_layout_set_text (self->layout,
                         self->text ? self->text : "",
                         -1);

  /* This here not being a gtk_widget_queue_resize() is why this widget exists */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  text,
                                  -1);
}

/**
 * gtk_inscription_get_text: (attributes org.gtk.Method.get_property=text)
 * @self: a `GtkInscription`
 *
 * Gets the text that is displayed.
 *
 * Returns: (nullable) (transfer none): The displayed text
 *
 * Since: 4.8
 */
const char *
gtk_inscription_get_text (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), NULL);

  return self->text;
}

/**
 * gtk_inscription_set_min_chars: (attributes org.gtk.Method.set_property=min-chars)
 * @self: a `GtkInscription`
 * @min_chars: the minimum number of characters that should fit, approximately
 *
 * Sets the `min-chars` of the inscription.
 *
 * See the [property@Gtk.Inscription:min-chars] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_min_chars (GtkInscription *self,
                               guint           min_chars)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->min_chars == min_chars)
    return;

  self->min_chars = min_chars;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_CHARS]);
}

/**
 * gtk_inscription_get_min_chars: (attributes org.gtk.Method.get_property=min-chars)
 * @self: a `GtkInscription`
 *
 * Gets the `min-chars` of the inscription.
 *
 * See the [property@Gtk.Inscription:min-chars] property.
 *
 * Returns: the min-chars property
 *
 * Since: 4.8
 */
guint
gtk_inscription_get_min_chars (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_MIN_CHARS);

  return self->min_chars;
}

/**
 * gtk_inscription_set_nat_chars: (attributes org.gtk.Method.set_property=nat-chars)
 * @self: a `GtkInscription`
 * @nat_chars: the number of characters that should ideally fit, approximately
 *
 * Sets the `nat-chars` of the inscription.
 *
 * See the [property@Gtk.Inscription:nat-chars] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_nat_chars (GtkInscription *self,
                               guint           nat_chars)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->nat_chars == nat_chars)
    return;

  self->nat_chars = nat_chars;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAT_CHARS]);
}

/**
 * gtk_inscription_get_nat_chars: (attributes org.gtk.Method.get_property=nat-chars)
 * @self: a `GtkInscription`
 *
 * Gets the `nat-chars` of the inscription.
 *
 * See the [property@Gtk.Inscription:nat-chars] property.
 *
 * Returns: the nat-chars property
 *
 * Since: 4.8
 */
guint
gtk_inscription_get_nat_chars (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_NAT_CHARS);

  return self->nat_chars;
}

/**
 * gtk_inscription_set_min_lines: (attributes org.gtk.Method.set_property=min-lines)
 * @self: a `GtkInscription`
 * @min_lines: the minimum number of lines that should fit, approximately
 *
 * Sets the `min-lines` of the inscription.
 *
 * See the [property@Gtk.Inscription:min-lines] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_min_lines (GtkInscription *self,
                               guint           min_lines)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->min_lines == min_lines)
    return;

  self->min_lines = min_lines;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_LINES]);
}

/**
 * gtk_inscription_get_min_lines: (attributes org.gtk.Method.get_property=min-lines)
 * @self: a `GtkInscription`
 *
 * Gets the `min-lines` of the inscription.
 *
 * See the [property@Gtk.Inscription:min-lines] property.
 *
 * Returns: the min-lines property
 *
 * Since: 4.8
 */
guint
gtk_inscription_get_min_lines (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_MIN_LINES);

  return self->min_lines;
}

/**
 * gtk_inscription_set_nat_lines: (attributes org.gtk.Method.set_property=nat-lines)
 * @self: a `GtkInscription`
 * @nat_lines: the number of lines that should ideally fit
 *
 * Sets the `nat-lines` of the inscription.
 *
 * See the [property@Gtk.Inscription:nat-lines] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_nat_lines (GtkInscription *self,
                               guint           nat_lines)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->nat_lines == nat_lines)
    return;

  self->nat_lines = nat_lines;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAT_LINES]);
}

/**
 * gtk_inscription_get_nat_lines: (attributes org.gtk.Method.get_property=nat-lines)
 * @self: a `GtkInscription`
 *
 * Gets the `nat-lines` of the inscription.
 *
 * See the [property@Gtk.Inscription:nat-lines] property.
 *
 * Returns: the nat-lines property
 *
 * Since: 4.8
 */
guint
gtk_inscription_get_nat_lines (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_NAT_LINES);

  return self->nat_lines;
}

/**
 * gtk_inscription_set_xalign: (attributes org.gtk.Method.set_property=xalign)
 * @self: a `GtkInscription`
 * @xalign: the new xalign value, between 0 and 1
 *
 * Sets the `xalign` of the inscription.
 *
 * See the [property@Gtk.Inscription:xalign] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_xalign (GtkInscription *self,
                            float           xalign)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (self->xalign == xalign)
    return;

  self->xalign = xalign;

  update_pango_alignment (self);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_XALIGN]);
}

/**
 * gtk_inscription_get_xalign: (attributes org.gtk.Method.get_property=xalign)
 * @self: a `GtkInscription`
 *
 * Gets the `xalign` of the inscription.
 *
 * See the [property@Gtk.Inscription:xalign] property.
 *
 * Returns: the xalign property
 *
 * Since: 4.8
 */
float
gtk_inscription_get_xalign (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_XALIGN);

  return self->xalign;
}

/**
 * gtk_inscription_set_yalign: (attributes org.gtk.Method.set_property=yalign)
 * @self: a `GtkInscription`
 * @yalign: the new yalign value, between 0 and 1
 *
 * Sets the `yalign` of the inscription.
 *
 * See the [property@Gtk.Inscription:yalign] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_yalign (GtkInscription *self,
                            float           yalign)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  yalign = CLAMP (yalign, 0.0, 1.0);

  if (self->yalign == yalign)
    return;

  self->yalign = yalign;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_YALIGN]);
}

/**
 * gtk_inscription_get_yalign: (attributes org.gtk.Method.get_property=yalign)
 * @self: a `GtkInscription`
 *
 * Gets the `yalign` of the inscription.
 *
 * See the [property@Gtk.Inscription:yalign] property.
 *
 * Returns: the yalign property
 *
 * Since: 4.8
 */
float
gtk_inscription_get_yalign (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), DEFAULT_YALIGN);

  return self->yalign;
}

/**
 * gtk_inscription_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @self: a `GtkInscription`
 * @attrs: (nullable): a [struct@Pango.AttrList]
 *
 * Apply attributes to the inscription text.
 *
 * These attributes will not be evaluated for sizing the inscription.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_attributes (GtkInscription *self,
                                PangoAttrList  *attrs)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->attrs == attrs)
    return;

  if (attrs)
    pango_attr_list_ref (attrs);

  if (self->attrs)
    pango_attr_list_unref (self->attrs);
  self->attrs = attrs;

  gtk_inscription_update_layout_attributes (self, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTRIBUTES]);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * gtk_inscription_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @self: a `GtkInscription`
 *
 * Gets the inscription's attribute list.
 *
 * Returns: (nullable) (transfer none): the attribute list
 *
 * Since: 4.8
 */
PangoAttrList *
gtk_inscription_get_attributes (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), NULL);

  return self->attrs;
}

/**
 * gtk_inscription_set_text_overflow: (attributes org.gtk.Method.set_property=text-overflow)
 * @self: a `GtkInscription`
 * @overflow: the overflow method to use
 *
 * Sets what to do when the text doesn't fit.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_text_overflow (GtkInscription         *self,
                                   GtkInscriptionOverflow  overflow)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (self->overflow == overflow)
    return;

  self->overflow = overflow;

  switch (self->overflow)
    {
    case GTK_INSCRIPTION_OVERFLOW_CLIP:
      pango_layout_set_ellipsize (self->layout, PANGO_ELLIPSIZE_NONE);
      break;
    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_START:
      pango_layout_set_ellipsize (self->layout, PANGO_ELLIPSIZE_START);
      break;
    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_MIDDLE:
      pango_layout_set_ellipsize (self->layout, PANGO_ELLIPSIZE_MIDDLE);
      break;
    case GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_END:
      pango_layout_set_ellipsize (self->layout, PANGO_ELLIPSIZE_END);
      break;
    default:
      g_assert_not_reached();
      break;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT_OVERFLOW]);
}

/**
 * gtk_inscription_get_text_overflow: (attributes org.gtk.Method.get_property=text-overflow)
 * @self: a `GtkInscription`
 *
 * Gets the inscription's overflow method.
 *
 * Returns: the overflow method
 *
 * Since: 4.8
 */
GtkInscriptionOverflow
gtk_inscription_get_text_overflow (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), GTK_INSCRIPTION_OVERFLOW_CLIP);

  return self->overflow;
}

/**
 * gtk_inscription_set_wrap_mode: (attributes org.gtk.Method.set_property=wrap-mode)
 * @self: a `GtkInscription`
 * @wrap_mode: the line wrapping mode
 *
 * Controls how line wrapping is done.
 *
 * Since:4.8
 */
void
gtk_inscription_set_wrap_mode (GtkInscription *self,
                               PangoWrapMode   wrap_mode)
{
  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (pango_layout_get_wrap (self->layout) == wrap_mode)
    return;

  pango_layout_set_wrap (self->layout, wrap_mode);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WRAP_MODE]);
}

/**
 * gtk_inscription_get_wrap_mode: (attributes org.gtk.Method.get_property=wrap-mode)
 * @self: a `GtkInscription`
 *
 * Returns line wrap mode used by the inscription.
 *
 * See [method@Gtk.Inscription.set_wrap_mode].
 *
 * Returns: the line wrap mode
 *
 * Since:4.8
 */
PangoWrapMode
gtk_inscription_get_wrap_mode (GtkInscription *self)
{
  g_return_val_if_fail (GTK_IS_INSCRIPTION (self), PANGO_WRAP_WORD_CHAR);

  return pango_layout_get_wrap (self->layout);
}

/**
 * gtk_inscription_set_markup: (attributes org.gtk.Method.set_property=markup)
 * @self: a `GtkInscription`
 * @markup: (nullable): The markup to display
 *
 * Utility function to set the text and attributes to be displayed.
 *
 * See the [property@Gtk.Inscription:markup] property.
 *
 * Since: 4.8
 */
void
gtk_inscription_set_markup (GtkInscription *self,
                            const char     *markup)
{
  PangoAttrList *attrs;
  char *text;
  GError *error = NULL;

  g_return_if_fail (GTK_IS_INSCRIPTION (self));

  if (markup == NULL)
    {
      text = NULL;
      attrs = NULL;
    }
  else if (!pango_parse_markup (markup, -1,
                           0,
                           &attrs, &text,
                           NULL,
                           &error))
    {
      g_warning ("Failed to set text '%s' from markup due to error parsing markup: %s",
                 markup, error->message);
      return;
    }

  gtk_inscription_set_text (self, text);
  gtk_inscription_set_attributes (self, attrs);

  g_clear_pointer (&text, g_free);
  g_clear_pointer (&attrs, pango_attr_list_unref);
}

/* {{{ GtkAccessibleText implementation */

static GBytes *
gtk_inscription_accessible_text_get_contents (GtkAccessibleText *self,
                                              unsigned int       start,
                                              unsigned int       end)
{
  const char *text;
  int len;
  char *string;
  gsize size;

  text = gtk_inscription_get_text (GTK_INSCRIPTION (self));
  len = g_utf8_strlen (text, -1);

  start = CLAMP (start, 0, len);
  end = CLAMP (end, 0, len);

  if (end <= start)
    {
      string = g_strdup ("");
      size = 1;
    }
  else
    {
      const char *p, *q;
      p = g_utf8_offset_to_pointer (text, start);
      q = g_utf8_offset_to_pointer (text, end);
      size = q - p + 1;
      string = g_strndup (p, q - p);
    }

  return g_bytes_new_take (string, size);
}

static GBytes *
gtk_inscription_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                                 unsigned int                  offset,
                                                 GtkAccessibleTextGranularity  granularity,
                                                 unsigned int                 *start,
                                                 unsigned int                 *end)
{
  PangoLayout *layout = gtk_inscription_get_layout (GTK_INSCRIPTION (self));
  char *string = gtk_pango_get_string_at (layout, offset, granularity, start, end);

  return g_bytes_new_take (string, strlen (string));
}

static unsigned int
gtk_inscription_accessible_text_get_caret_position (GtkAccessibleText *self)
{
  return 0;
}

static gboolean
gtk_inscription_accessible_text_get_selection (GtkAccessibleText       *self,
                                               gsize                   *n_ranges,
                                               GtkAccessibleTextRange **ranges)
{
  return FALSE;
}

static gboolean
gtk_inscription_accessible_text_get_attributes (GtkAccessibleText        *self,
                                                unsigned int              offset,
                                                gsize                    *n_ranges,
                                                GtkAccessibleTextRange  **ranges,
                                                char                   ***attribute_names,
                                                char                   ***attribute_values)
{
  PangoLayout *layout = gtk_inscription_get_layout (GTK_INSCRIPTION (self));
  unsigned int start, end;
  char **names, **values;

  gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

  *n_ranges = g_strv_length (names);
  *ranges = g_new (GtkAccessibleTextRange, *n_ranges);

  for (unsigned i = 0; i < *n_ranges; i++)
    {
      GtkAccessibleTextRange *range = &(*ranges)[i];

      range->start = start;
      range->length = end - start;
    }

  *attribute_names = names;
  *attribute_values = values;

  return TRUE;
}

static void
gtk_inscription_accessible_text_get_default_attributes (GtkAccessibleText   *self,
                                                        char              ***attribute_names,
                                                        char              ***attribute_values)
{
  PangoLayout *layout = gtk_inscription_get_layout (GTK_INSCRIPTION (self));
  char **names, **values;

  gtk_pango_get_default_attributes (layout, &names, &values);

  *attribute_names = names;
  *attribute_values = values;
}

static void
gtk_inscription_accessible_text_init (GtkAccessibleTextInterface *iface)
{
  iface->get_contents = gtk_inscription_accessible_text_get_contents;
  iface->get_contents_at = gtk_inscription_accessible_text_get_contents_at;
  iface->get_caret_position = gtk_inscription_accessible_text_get_caret_position;
  iface->get_selection = gtk_inscription_accessible_text_get_selection;
  iface->get_attributes = gtk_inscription_accessible_text_get_attributes;
  iface->get_default_attributes = gtk_inscription_accessible_text_get_default_attributes;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
