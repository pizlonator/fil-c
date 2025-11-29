/* gtktexttag.c - text tag object
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */

/**
 * GtkTextTag:
 *
 * A tag that can be applied to text contained in a `GtkTextBuffer`.
 *
 * You may wish to begin by reading the
 * [text widget conceptual overview](section-text-widget.html),
 * which gives an overview of all the objects and data types
 * related to the text widget and how they work together.
 *
 * Tags should be in the [class@Gtk.TextTagTable] for a given
 * `GtkTextBuffer` before using them with that buffer.
 *
 * [method@Gtk.TextBuffer.create_tag] is the best way to create tags.
 * See “gtk4-demo” for numerous examples.
 *
 * For each property of `GtkTextTag`, there is a “set” property, e.g.
 * “font-set” corresponds to “font”. These “set” properties reflect
 * whether a property has been set or not.
 *
 * They are maintained by GTK and you should not set them independently.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gtktexttag.h"
#include "gtktexttypesprivate.h"
#include "gtktexttagtable.h"
#include "gtktexttagtableprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

enum {
  PROP_0,
  /* Construct args */
  PROP_NAME,

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
  PROP_PIXELS_ABOVE_LINES,
  PROP_PIXELS_BELOW_LINES,
  PROP_PIXELS_INSIDE_WRAP,
  PROP_LINE_HEIGHT,
  PROP_EDITABLE,
  PROP_WRAP_MODE,
  PROP_JUSTIFICATION,
  PROP_DIRECTION,
  PROP_LEFT_MARGIN,
  PROP_INDENT,
  PROP_STRIKETHROUGH,
  PROP_STRIKETHROUGH_RGBA,
  PROP_RIGHT_MARGIN,
  PROP_UNDERLINE,
  PROP_UNDERLINE_RGBA,
  PROP_OVERLINE,
  PROP_OVERLINE_RGBA,
  PROP_RISE,
  PROP_BACKGROUND_FULL_HEIGHT,
  PROP_LANGUAGE,
  PROP_TABS,
  PROP_INVISIBLE,
  PROP_PARAGRAPH_BACKGROUND,
  PROP_PARAGRAPH_BACKGROUND_RGBA,
  PROP_FALLBACK,
  PROP_LETTER_SPACING,
  PROP_FONT_FEATURES,
  PROP_ALLOW_BREAKS,
  PROP_SHOW_SPACES,
  PROP_INSERT_HYPHENS,
  PROP_TEXT_TRANSFORM,
  PROP_WORD,
  PROP_SENTENCE,

  /* Behavior args */
  PROP_ACCUMULATIVE_MARGIN,

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
  PROP_PIXELS_ABOVE_LINES_SET,
  PROP_PIXELS_BELOW_LINES_SET,
  PROP_PIXELS_INSIDE_WRAP_SET,
  PROP_LINE_HEIGHT_SET,
  PROP_EDITABLE_SET,
  PROP_WRAP_MODE_SET,
  PROP_JUSTIFICATION_SET,
  PROP_LEFT_MARGIN_SET,
  PROP_INDENT_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_STRIKETHROUGH_RGBA_SET,
  PROP_RIGHT_MARGIN_SET,
  PROP_UNDERLINE_SET,
  PROP_UNDERLINE_RGBA_SET,
  PROP_OVERLINE_SET,
  PROP_OVERLINE_RGBA_SET,
  PROP_RISE_SET,
  PROP_BACKGROUND_FULL_HEIGHT_SET,
  PROP_LANGUAGE_SET,
  PROP_TABS_SET,
  PROP_INVISIBLE_SET,
  PROP_PARAGRAPH_BACKGROUND_SET,
  PROP_FALLBACK_SET,
  PROP_LETTER_SPACING_SET,
  PROP_FONT_FEATURES_SET,
  PROP_ALLOW_BREAKS_SET,
  PROP_SHOW_SPACES_SET,
  PROP_INSERT_HYPHENS_SET,
  PROP_TEXT_TRANSFORM_SET,
  PROP_WORD_SET,
  PROP_SENTENCE_SET,

  LAST_ARG
};
static void gtk_text_tag_finalize     (GObject         *object);
static void gtk_text_tag_set_property (GObject         *object,
                                       guint            prop_id,
                                       const GValue    *value,
                                       GParamSpec      *pspec);
static void gtk_text_tag_get_property (GObject         *object,
                                       guint            prop_id,
                                       GValue          *value,
                                       GParamSpec      *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GtkTextTag, gtk_text_tag, G_TYPE_OBJECT)

static void
gtk_text_tag_class_init (GtkTextTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_text_tag_set_property;
  object_class->get_property = gtk_text_tag_get_property;
  
  object_class->finalize = gtk_text_tag_finalize;

  /* Construct */
  /**
   * GtkTextTag:name:
   *
   * The name used to refer to the tag.
   *
   * %NULL for anonymous tags.
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* Style args */

  /**
   * GtkTextTag:background:
   *
   * Background color as a string.
   */
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  /**
   * GtkTextTag:background-rgba:
   *
   * Background color as a `GdkRGBA`.
   */
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_RGBA,
                                   g_param_spec_boxed ("background-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:background-full-height:
   *
   * Whether the background color fills the entire line height
   * or only the height of the tagged characters.
   */
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_FULL_HEIGHT,
                                   g_param_spec_boolean ("background-full-height", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:foreground:
   *
   * Foreground color as a string.
   */
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_string ("foreground", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  /**
   * GtkTextTag:foreground-rgba:
   *
   * Foreground color as a `GdkRGBA`.
   */
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_RGBA,
                                   g_param_spec_boxed ("foreground-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:direction:
   *
   * Text direction, e.g. right-to-left or left-to-right.
   */
  g_object_class_install_property (object_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction", NULL, NULL,
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_NONE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:editable:
   *
   * Whether the text can be modified by the user.
   */
  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:font:
   *
   * Font description as string, e.g. \"Sans Italic 12\".
   *
   * Note that the initial value of this property depends on
   * the internals of `PangoFontDescription`.
   */
  g_object_class_install_property (object_class,
                                   PROP_FONT,
                                   g_param_spec_string ("font", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:font-desc:
   *
   * Font description as a `PangoFontDescription`.
   */
  g_object_class_install_property (object_class,
                                   PROP_FONT_DESC,
                                   g_param_spec_boxed ("font-desc", NULL, NULL,
                                                       PANGO_TYPE_FONT_DESCRIPTION,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:family:
   *
   * Name of the font family, e.g. Sans, Helvetica, Times, Monospace.
   */
  g_object_class_install_property (object_class,
                                   PROP_FAMILY,
                                   g_param_spec_string ("family", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:style:
   *
   * Font style as a `PangoStyle`, e.g. %PANGO_STYLE_ITALIC.
   */
  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("style", NULL, NULL,
                                                      PANGO_TYPE_STYLE,
                                                      PANGO_STYLE_NORMAL,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:variant:
   *
   * Font variant as a `PangoVariant`, e.g. %PANGO_VARIANT_SMALL_CAPS.
   */
  g_object_class_install_property (object_class,
                                   PROP_VARIANT,
                                   g_param_spec_enum ("variant", NULL, NULL,
                                                      PANGO_TYPE_VARIANT,
                                                      PANGO_VARIANT_NORMAL,
                                                      GTK_PARAM_READWRITE));
  /**
   * GtkTextTag:weight:
   *
   * Font weight as an integer.
   */
  g_object_class_install_property (object_class,
                                   PROP_WEIGHT,
                                   g_param_spec_int ("weight", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     PANGO_WEIGHT_NORMAL,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:stretch:
   *
   * Font stretch as a `PangoStretch`, e.g. %PANGO_STRETCH_CONDENSED.
   */
  g_object_class_install_property (object_class,
                                   PROP_STRETCH,
                                   g_param_spec_enum ("stretch", NULL, NULL,
                                                      PANGO_TYPE_STRETCH,
                                                      PANGO_STRETCH_NORMAL,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:size:
   *
   * Font size in Pango units.
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:scale:
   *
   * Font size as a scale factor relative to the default font size.
   *
   * This properly adapts to theme changes, etc. so is recommended.
   * Pango predefines some scales such as %PANGO_SCALE_X_LARGE.
   */
  g_object_class_install_property (object_class,
                                   PROP_SCALE,
                                   g_param_spec_double ("scale", NULL, NULL,
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:size-points:
   *
   * Font size in points.
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE_POINTS,
                                   g_param_spec_double ("size-points", NULL, NULL,
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        GTK_PARAM_READWRITE));  

  /**
   * GtkTextTag:justification:
   *
   * Left, right, or center justification.
   */
  g_object_class_install_property (object_class,
                                   PROP_JUSTIFICATION,
                                   g_param_spec_enum ("justification", NULL, NULL,
                                                      GTK_TYPE_JUSTIFICATION,
                                                      GTK_JUSTIFY_LEFT,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:language:
   *
   * The language this text is in, as an ISO code.
   *
   * Pango can use this as a hint when rendering the text.
   * If not set, an appropriate default will be used.
   *
   * Note that the initial value of this property depends
   * on the current locale, see also [func@Gtk.get_default_language].
   */
  g_object_class_install_property (object_class,
                                   PROP_LANGUAGE,
                                   g_param_spec_string ("language", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE));  

  /**
   * GtkTextTag:left-margin:
   *
   * Width of the left margin in pixels.
   */
  g_object_class_install_property (object_class,
                                   PROP_LEFT_MARGIN,
                                   g_param_spec_int ("left-margin", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:right-margin:
   *
   * Width of the right margin, in pixels.
   */
  g_object_class_install_property (object_class,
                                   PROP_RIGHT_MARGIN,
                                   g_param_spec_int ("right-margin", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:indent:
   *
   * Amount to indent the paragraph, in pixels. 
   *
   * A negative value of indent will produce a hanging indentation.
   * That is, the first line will have the full width, and subsequent
   * lines will be indented by the absolute value of indent.
   *
   */
  g_object_class_install_property (object_class,
                                   PROP_INDENT,
                                   g_param_spec_int ("indent", NULL, NULL,
                                                     G_MININT,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:rise:
   *
   * Offset of text above the baseline, in Pango units.
   *
   * Negative values go below the baseline.
   */
  g_object_class_install_property (object_class,
                                   PROP_RISE,
                                   g_param_spec_int ("rise", NULL, NULL,
						     G_MININT,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:pixels-above-lines:
   *
   * Pixels of blank space above paragraphs.
   */
  g_object_class_install_property (object_class,
                                   PROP_PIXELS_ABOVE_LINES,
                                   g_param_spec_int ("pixels-above-lines", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:pixels-below-lines:
   *
   * Pixels of blank space below paragraphs.
   */
  g_object_class_install_property (object_class,
                                   PROP_PIXELS_BELOW_LINES,
                                   g_param_spec_int ("pixels-below-lines", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:pixels-inside-wrap:
   *
   * Pixels of blank space between wrapped lines in a paragraph.
   */
  g_object_class_install_property (object_class,
                                   PROP_PIXELS_INSIDE_WRAP,
                                   g_param_spec_int ("pixels-inside-wrap", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:line-height:
   *
   * Factor to scale line height by.
   *
   * Since: 4.6
   */
  g_object_class_install_property (object_class,
                                   PROP_LINE_HEIGHT,
                                   g_param_spec_float ("line-height", NULL, NULL,
                                                       0.0, 10.0, 0.0,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:strikethrough:
   *
   * Whether to strike through the text.
   */
  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH,
                                   g_param_spec_boolean ("strikethrough", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:underline:
   *
   * Style of underline for this text.
   */
  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE,
                                   g_param_spec_enum ("underline", NULL, NULL,
                                                      PANGO_TYPE_UNDERLINE,
                                                      PANGO_UNDERLINE_NONE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:underline-rgba:
   *
   * This property modifies the color of underlines.
   *
   * If not set, underlines will use the foreground color.
   *
   * If [property@Gtk.TextTag:underline] is set to %PANGO_UNDERLINE_ERROR,
   * an alternate color may be applied instead of the foreground. Setting
   * this property will always override those defaults.
   */
  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE_RGBA,
                                   g_param_spec_boxed ("underline-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:overline:
   *
   * Style of overline for this text.
   */
  g_object_class_install_property (object_class,
                                   PROP_OVERLINE,
                                   g_param_spec_enum ("overline", NULL, NULL,
                                                      PANGO_TYPE_OVERLINE,
                                                      PANGO_OVERLINE_NONE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:overline-rgba:
   *
   * This property modifies the color of overlines.
   *
   * If not set, overlines will use the foreground color.
   */
  g_object_class_install_property (object_class,
                                   PROP_OVERLINE_RGBA,
                                   g_param_spec_boxed ("overline-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:strikethrough-rgba:
   *
   * This property modifies the color of strikeouts.
   *
   * If not set, strikeouts will use the foreground color.
   */
  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH_RGBA,
                                   g_param_spec_boxed ("strikethrough-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:wrap-mode:
   *
   * Whether to wrap lines never, at word boundaries, or
   * at character boundaries.
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode", NULL, NULL,
                                                      GTK_TYPE_WRAP_MODE,
                                                      GTK_WRAP_NONE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:tabs:
   *
   * Custom tabs for this text.
   */
  g_object_class_install_property (object_class,
                                   PROP_TABS,
                                   g_param_spec_boxed ("tabs", NULL, NULL,
                                                       PANGO_TYPE_TAB_ARRAY,
                                                       GTK_PARAM_READWRITE));
  
  /**
   * GtkTextTag:invisible:
   *
   * Whether this text is hidden.
   *
   * Note that there may still be problems with the support for invisible
   * text, in particular when navigating programmatically inside a buffer
   * containing invisible segments.
   */
  g_object_class_install_property (object_class,
                                   PROP_INVISIBLE,
                                   g_param_spec_boolean ("invisible", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:paragraph-background:
   *
   * The paragraph background color as a string.
   */
  g_object_class_install_property (object_class,
                                   PROP_PARAGRAPH_BACKGROUND,
                                   g_param_spec_string ("paragraph-background", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  /**
   * GtkTextTag:paragraph-background-rgba:
   *
   * The paragraph background color as a `GdkRGBA`.
   */
  g_object_class_install_property (object_class,
                                   PROP_PARAGRAPH_BACKGROUND_RGBA,
                                   g_param_spec_boxed ("paragraph-background-rgba", NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:fallback:
   *
   * Whether font fallback is enabled.
   *
   * When set to %TRUE, other fonts will be substituted
   * where the current font is missing glyphs.
   */
  g_object_class_install_property (object_class,
                                   PROP_FALLBACK,
                                   g_param_spec_boolean ("fallback", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:letter-spacing:
   *
   * Extra spacing between graphemes, in Pango units.
   */
  g_object_class_install_property (object_class,
                                   PROP_LETTER_SPACING,
                                   g_param_spec_int ("letter-spacing", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:font-features:
   *
   * OpenType font features, as a string.
   */
  g_object_class_install_property (object_class,
                                   PROP_FONT_FEATURES,
                                   g_param_spec_string ("font-features", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:allow-breaks:
   *
   * Whether breaks are allowed.
   */
  g_object_class_install_property (object_class,
                                   PROP_ALLOW_BREAKS,
                                   g_param_spec_boolean ("allow-breaks", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:show-spaces:
   *
   * How to render invisible characters.
   */
  g_object_class_install_property (object_class,
                                   PROP_SHOW_SPACES,
                                   g_param_spec_flags ("show-spaces", NULL, NULL,
                                                         PANGO_TYPE_SHOW_FLAGS,
                                                         PANGO_SHOW_NONE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:insert-hyphens:
   *
   * Whether to insert hyphens at breaks.
   */
  g_object_class_install_property (object_class,
                                   PROP_INSERT_HYPHENS,
                                   g_param_spec_boolean ("insert-hyphens", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:text-transform:
   *
   * How to transform the text for display.
   *
   * Since: 4.6
   */
  g_object_class_install_property (object_class,
                                   PROP_TEXT_TRANSFORM,
                                   g_param_spec_enum ("text-transform", NULL, NULL,
                                                         PANGO_TYPE_TEXT_TRANSFORM,
                                                         PANGO_TEXT_TRANSFORM_NONE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:word:
   *
   * Whether this tag represents a single word.
   *
   * This affects line breaks and cursor movement.
   *
   * Since: 4.6
   */
  g_object_class_install_property (object_class,
                                   PROP_WORD,
                                   g_param_spec_boolean ("word", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:sentence:
   *
   * Whether this tag represents a single sentence.
   *
   * This affects cursor movement.
   *
   * Since: 4.6
   */
  g_object_class_install_property (object_class,
                                   PROP_SENTENCE,
                                   g_param_spec_boolean ("sentence", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextTag:accumulative-margin:
   *
   * Whether the margins accumulate or override each other.
   *
   * When set to %TRUE the margins of this tag are added to the margins
   * of any other non-accumulative margins present. When set to %FALSE
   * the margins override one another (the default).
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCUMULATIVE_MARGIN,
                                   g_param_spec_boolean ("accumulative-margin", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, GTK_PARAM_READWRITE))

  ADD_SET_PROP ("background-set", PROP_BACKGROUND_SET, NULL, NULL);
  
  ADD_SET_PROP ("background-full-height-set", PROP_BACKGROUND_FULL_HEIGHT_SET, NULL, NULL);

  ADD_SET_PROP ("foreground-set", PROP_FOREGROUND_SET, NULL, NULL);

  ADD_SET_PROP ("editable-set", PROP_EDITABLE_SET, NULL, NULL);

  ADD_SET_PROP ("family-set", PROP_FAMILY_SET, NULL, NULL);  

  ADD_SET_PROP ("style-set", PROP_STYLE_SET, NULL, NULL);

  ADD_SET_PROP ("variant-set", PROP_VARIANT_SET, NULL, NULL);

  ADD_SET_PROP ("weight-set", PROP_WEIGHT_SET, NULL, NULL);

  ADD_SET_PROP ("stretch-set", PROP_STRETCH_SET, NULL, NULL);

  ADD_SET_PROP ("size-set", PROP_SIZE_SET, NULL, NULL);

  ADD_SET_PROP ("scale-set", PROP_SCALE_SET, NULL, NULL);
  
  ADD_SET_PROP ("justification-set", PROP_JUSTIFICATION_SET, NULL, NULL);
  
  ADD_SET_PROP ("language-set", PROP_LANGUAGE_SET, NULL, NULL);

  ADD_SET_PROP ("left-margin-set", PROP_LEFT_MARGIN_SET, NULL, NULL);

  ADD_SET_PROP ("indent-set", PROP_INDENT_SET, NULL, NULL);

  ADD_SET_PROP ("rise-set", PROP_RISE_SET, NULL, NULL);

  ADD_SET_PROP ("pixels-above-lines-set", PROP_PIXELS_ABOVE_LINES_SET, NULL, NULL);

  ADD_SET_PROP ("pixels-below-lines-set", PROP_PIXELS_BELOW_LINES_SET, NULL, NULL);

  ADD_SET_PROP ("pixels-inside-wrap-set", PROP_PIXELS_INSIDE_WRAP_SET, NULL, NULL);

  ADD_SET_PROP ("line-height-set", PROP_LINE_HEIGHT_SET, NULL, NULL);

  ADD_SET_PROP ("strikethrough-set", PROP_STRIKETHROUGH_SET, NULL, NULL);
  
  ADD_SET_PROP ("right-margin-set", PROP_RIGHT_MARGIN_SET, NULL, NULL);

  ADD_SET_PROP ("underline-set", PROP_UNDERLINE_SET, NULL, NULL);

  /**
   * GtkTextTag:underline-rgba-set:
   *
   * If the `underline-rgba` property has been set.
   */
  ADD_SET_PROP ("underline-rgba-set", PROP_UNDERLINE_RGBA_SET, NULL, NULL);

  ADD_SET_PROP ("overline-set", PROP_OVERLINE_SET, NULL, NULL);

  ADD_SET_PROP ("overline-rgba-set", PROP_OVERLINE_RGBA_SET, NULL, NULL);

  /**
   * GtkTextTag:strikethrough-rgba-set:
   *
   * If the `strikethrough-rgba` property has been set.
   */
  ADD_SET_PROP ("strikethrough-rgba-set", PROP_STRIKETHROUGH_RGBA_SET, NULL, NULL);

  ADD_SET_PROP ("wrap-mode-set", PROP_WRAP_MODE_SET, NULL, NULL);

  ADD_SET_PROP ("tabs-set", PROP_TABS_SET, NULL, NULL);

  ADD_SET_PROP ("invisible-set", PROP_INVISIBLE_SET, NULL, NULL);

  ADD_SET_PROP ("paragraph-background-set", PROP_PARAGRAPH_BACKGROUND_SET, NULL, NULL);

  ADD_SET_PROP ("fallback-set", PROP_FALLBACK_SET, NULL, NULL);

  ADD_SET_PROP ("letter-spacing-set", PROP_LETTER_SPACING_SET, NULL, NULL);

  ADD_SET_PROP ("font-features-set", PROP_FONT_FEATURES_SET, NULL, NULL);

  ADD_SET_PROP ("allow-breaks-set", PROP_ALLOW_BREAKS_SET, NULL, NULL);

  ADD_SET_PROP ("show-spaces-set", PROP_SHOW_SPACES_SET, NULL, NULL);

  ADD_SET_PROP ("insert-hyphens-set", PROP_INSERT_HYPHENS_SET, NULL, NULL);

  ADD_SET_PROP ("text-transform-set", PROP_TEXT_TRANSFORM_SET, NULL, NULL);

  ADD_SET_PROP ("word-set", PROP_WORD_SET, NULL, NULL);

  ADD_SET_PROP ("sentence-set", PROP_WORD_SET, NULL, NULL);
}

static void
gtk_text_tag_init (GtkTextTag *text_tag)
{
  text_tag->priv = gtk_text_tag_get_instance_private (text_tag);
  text_tag->priv->values = gtk_text_attributes_new ();
}

/**
 * gtk_text_tag_new:
 * @name: (nullable): tag name
 *
 * Creates a `GtkTextTag`.
 *
 * Returns: a new `GtkTextTag`
 */
GtkTextTag*
gtk_text_tag_new (const char *name)
{
  GtkTextTag *tag;

  tag = g_object_new (GTK_TYPE_TEXT_TAG, "name", name, NULL);

  return tag;
}

static void
gtk_text_tag_finalize (GObject *object)
{
  GtkTextTag *text_tag = GTK_TEXT_TAG (object);
  GtkTextTagPrivate *priv = text_tag->priv;

  if (priv->table)
    gtk_text_tag_table_remove (priv->table, text_tag);

  g_assert (priv->table == NULL);

  gtk_text_attributes_unref (priv->values);
  priv->values = NULL;

  g_free (priv->name);
  priv->name = NULL;

  G_OBJECT_CLASS (gtk_text_tag_parent_class)->finalize (object);
}

static void
set_underline_rgba (GtkTextTag    *tag,
                    const GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->appearance.underline_rgba)
    gdk_rgba_free (priv->values->appearance.underline_rgba);
  priv->values->appearance.underline_rgba = NULL;

  if (rgba)
    {
      priv->values->appearance.underline_rgba = gdk_rgba_copy (rgba);

      if (!priv->underline_rgba_set)
        {
          priv->underline_rgba_set = TRUE;
          g_object_notify (G_OBJECT (tag), "underline-rgba-set");
        }
    }
  else
    {
      if (priv->underline_rgba_set)
        {
          priv->underline_rgba_set = FALSE;
          g_object_notify (G_OBJECT (tag), "underline-rgba-set");
        }
    }
}

static void
set_overline_rgba (GtkTextTag    *tag,
                   const GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->appearance.overline_rgba)
    gdk_rgba_free (priv->values->appearance.overline_rgba);
  priv->values->appearance.overline_rgba = NULL;

  if (rgba)
    {
      priv->values->appearance.overline_rgba = gdk_rgba_copy (rgba);

      if (!priv->overline_rgba_set)
        {
          priv->overline_rgba_set = TRUE;
          g_object_notify (G_OBJECT (tag), "overline-rgba-set");
        }
    }
  else
    {
      if (priv->overline_rgba_set)
        {
          priv->overline_rgba_set = FALSE;
          g_object_notify (G_OBJECT (tag), "overline-rgba-set");
        }
    }
}

static void
set_strikethrough_rgba (GtkTextTag    *tag,
                        const GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->appearance.strikethrough_rgba)
    gdk_rgba_free (priv->values->appearance.strikethrough_rgba);
  priv->values->appearance.strikethrough_rgba = NULL;

  if (rgba)
    {
      priv->values->appearance.strikethrough_rgba = gdk_rgba_copy (rgba);

      if (!priv->strikethrough_rgba_set)
        {
          priv->strikethrough_rgba_set = TRUE;
          g_object_notify (G_OBJECT (tag), "strikethrough-rgba-set");
        }
    }
  else
    {
      if (priv->strikethrough_rgba_set)
        {
          priv->strikethrough_rgba_set = FALSE;
          g_object_notify (G_OBJECT (tag), "strikethrough-rgba-set");
        }
    }
}

static void
set_bg_rgba (GtkTextTag *tag, GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->appearance.bg_rgba)
    gdk_rgba_free (priv->values->appearance.bg_rgba);
  priv->values->appearance.bg_rgba = NULL;

  if (rgba)
    {
      if (!priv->bg_color_set)
        {
          priv->bg_color_set = TRUE;
          g_object_notify (G_OBJECT (tag), "background-set");
        }

      priv->values->appearance.bg_rgba = gdk_rgba_copy (rgba);
    }
  else
    {
      if (priv->bg_color_set)
        {
          priv->bg_color_set = FALSE;
          g_object_notify (G_OBJECT (tag), "background-set");
        }
    }
}

static void
set_fg_rgba (GtkTextTag *tag, GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->appearance.fg_rgba)
    gdk_rgba_free (priv->values->appearance.fg_rgba);
  priv->values->appearance.fg_rgba = NULL;

  if (rgba)
    {
      if (!priv->fg_color_set)
        {
          priv->fg_color_set = TRUE;
          g_object_notify (G_OBJECT (tag), "foreground-set");
        }

      priv->values->appearance.fg_rgba = gdk_rgba_copy (rgba);
    }
  else
    {
      if (priv->fg_color_set)
        {
          priv->fg_color_set = FALSE;
          g_object_notify (G_OBJECT (tag), "foreground-set");
        }
    }
}

static void
set_pg_bg_rgba (GtkTextTag *tag, GdkRGBA *rgba)
{
  GtkTextTagPrivate *priv = tag->priv;

  if (priv->values->pg_bg_rgba)
    gdk_rgba_free (priv->values->pg_bg_rgba);
  priv->values->pg_bg_rgba = NULL;

  if (rgba)
    {
      if (!priv->pg_bg_color_set)
        {
          priv->pg_bg_color_set = TRUE;
          g_object_notify (G_OBJECT (tag), "paragraph-background-set");
        }

      priv->values->pg_bg_rgba = gdk_rgba_copy (rgba);
    }
  else
    {
      if (priv->pg_bg_color_set)
        {
          priv->pg_bg_color_set = FALSE;
          g_object_notify (G_OBJECT (tag), "paragraph-background-set");
        }
    }
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
    g_object_notify (object, "family-set");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style-set");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant-set");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight-set");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch-set");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size-set");
}

static void
notify_fields_changed (GObject       *object,
		       PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size");
}

static void
set_font_description (GtkTextTag           *text_tag,
                      PangoFontDescription *font_desc)
{
  GtkTextTagPrivate *priv = text_tag->priv;
  GObject *object = G_OBJECT (text_tag);
  PangoFontDescription *new_font_desc;
  PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;
  
  if (font_desc)
    new_font_desc = pango_font_description_copy (font_desc);
  else
    new_font_desc = pango_font_description_new ();

  if (priv->values->font)
    old_mask = pango_font_description_get_set_fields (priv->values->font);
  else
    old_mask = 0;
  
  new_mask = pango_font_description_get_set_fields (new_font_desc);

  changed_mask = old_mask | new_mask;
  set_changed_mask = old_mask ^ new_mask;

  if (priv->values->font)
    pango_font_description_free (priv->values->font);
  priv->values->font = new_font_desc;

  g_object_freeze_notify (object);

  g_object_notify (object, "font-desc");
  g_object_notify (object, "font");
  
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    {
      g_object_notify (object, "size");
      g_object_notify (object, "size-points");
    }

  notify_set_changed (object, set_changed_mask);
  
  g_object_thaw_notify (object);
}

static void
gtk_text_tag_ensure_font (GtkTextTag *text_tag)
{
  GtkTextTagPrivate *priv = text_tag->priv;

  if (!priv->values->font)
    priv->values->font = pango_font_description_new ();
}

static void
gtk_text_tag_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkTextTag *text_tag = GTK_TEXT_TAG (object);
  GtkTextTagPrivate *priv = text_tag->priv;
  gboolean size_changed = FALSE;

  switch (prop_id)
    {
    case PROP_NAME:
      g_return_if_fail (priv->name == NULL);
      priv->name = g_value_dup_string (value);
      break;

    case PROP_BACKGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_bg_rgba (text_tag, NULL);       /* reset background_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_bg_rgba (text_tag, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));
      }
      break;

    case PROP_FOREGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_fg_rgba (text_tag, NULL);       /* reset to foreground_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_fg_rgba (text_tag, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));
      }
      break;

    case PROP_BACKGROUND_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);

        set_bg_rgba (text_tag, color);
      }
      break;

    case PROP_FOREGROUND_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);

        set_fg_rgba (text_tag, color);
      }
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const char *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (text_tag, font_desc);
	if (font_desc)
	  pango_font_description_free (font_desc);
        
        size_changed = TRUE;
      }
      break;

    case PROP_FONT_DESC:
      {
        PangoFontDescription *font_desc;

        font_desc = g_value_get_boxed (value);

        set_font_description (text_tag, font_desc);

        size_changed = TRUE;
      }
      break;

    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
      {
	PangoFontMask old_set_mask;

	gtk_text_tag_ensure_font (text_tag);
	old_set_mask = pango_font_description_get_set_fields (priv->values->font);
 
	switch (prop_id)
	  {
	  case PROP_FAMILY:
	    pango_font_description_set_family (priv->values->font,
					       g_value_get_string (value));
	    break;
	  case PROP_STYLE:
	    pango_font_description_set_style (priv->values->font,
					      g_value_get_enum (value));
	    break;
	  case PROP_VARIANT:
	    pango_font_description_set_variant (priv->values->font,
						g_value_get_enum (value));
	    break;
	  case PROP_WEIGHT:
	    pango_font_description_set_weight (priv->values->font,
					       g_value_get_int (value));
	    break;
	  case PROP_STRETCH:
	    pango_font_description_set_stretch (priv->values->font,
						g_value_get_enum (value));
	    break;
	  case PROP_SIZE:
	    pango_font_description_set_size (priv->values->font,
					     g_value_get_int (value));
	    g_object_notify (object, "size-points");
	    break;
	  case PROP_SIZE_POINTS:
	    pango_font_description_set_size (priv->values->font,
					     g_value_get_double (value) * PANGO_SCALE);
	    g_object_notify (object, "size");
	    break;

          default:
            break;
	  }

	size_changed = TRUE;
	notify_set_changed (object, old_set_mask & pango_font_description_get_set_fields (priv->values->font));
	g_object_notify (object, "font-desc");
	g_object_notify (object, "font");

	break;
      }
      
    case PROP_SCALE:
      priv->values->font_scale = g_value_get_double (value);
      priv->scale_set = TRUE;
      g_object_notify (object, "scale-set");
      size_changed = TRUE;
      break;
      
    case PROP_PIXELS_ABOVE_LINES:
      priv->pixels_above_lines_set = TRUE;
      priv->values->pixels_above_lines = g_value_get_int (value);
      g_object_notify (object, "pixels-above-lines-set");
      size_changed = TRUE;
      break;

    case PROP_PIXELS_BELOW_LINES:
      priv->pixels_below_lines_set = TRUE;
      priv->values->pixels_below_lines = g_value_get_int (value);
      g_object_notify (object, "pixels-below-lines-set");
      size_changed = TRUE;
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      priv->pixels_inside_wrap_set = TRUE;
      priv->values->pixels_inside_wrap = g_value_get_int (value);
      g_object_notify (object, "pixels-inside-wrap-set");
      size_changed = TRUE;
      break;

    case PROP_LINE_HEIGHT:
      priv->line_height_set = TRUE;
      priv->values->line_height = g_value_get_float (value);
      g_object_notify (object, "line-height-set");
      size_changed = TRUE;
      break;

    case PROP_EDITABLE:
      priv->editable_set = TRUE;
      priv->values->editable = g_value_get_boolean (value);
      g_object_notify (object, "editable-set");
      break;

    case PROP_WRAP_MODE:
      priv->wrap_mode_set = TRUE;
      priv->values->wrap_mode = g_value_get_enum (value);
      g_object_notify (object, "wrap-mode-set");
      size_changed = TRUE;
      break;

    case PROP_JUSTIFICATION:
      priv->justification_set = TRUE;
      priv->values->justification = g_value_get_enum (value);
      g_object_notify (object, "justification-set");
      size_changed = TRUE;
      break;

    case PROP_DIRECTION:
      priv->values->direction = g_value_get_enum (value);
      break;

    case PROP_LEFT_MARGIN:
      priv->left_margin_set = TRUE;
      priv->values->left_margin = g_value_get_int (value);
      g_object_notify (object, "left-margin-set");
      size_changed = TRUE;
      break;

    case PROP_INDENT:
      priv->indent_set = TRUE;
      priv->values->indent = g_value_get_int (value);
      g_object_notify (object, "indent-set");
      size_changed = TRUE;
      break;

    case PROP_STRIKETHROUGH:
      priv->strikethrough_set = TRUE;
      priv->values->appearance.strikethrough = g_value_get_boolean (value);
      g_object_notify (object, "strikethrough-set");
      break;

    case PROP_STRIKETHROUGH_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);
        set_strikethrough_rgba (text_tag, color);
      }
      break;

    case PROP_RIGHT_MARGIN:
      priv->right_margin_set = TRUE;
      priv->values->right_margin = g_value_get_int (value);
      g_object_notify (object, "right-margin-set");
      size_changed = TRUE;
      break;

    case PROP_UNDERLINE:
      priv->underline_set = TRUE;
      priv->values->appearance.underline = g_value_get_enum (value);
      g_object_notify (object, "underline-set");
      break;

    case PROP_UNDERLINE_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);
        set_underline_rgba (text_tag, color);
      }
      break;

    case PROP_OVERLINE:
      priv->overline_set = TRUE;
      priv->values->appearance.overline = g_value_get_enum (value);
      g_object_notify (object, "overline-set");
      break;

    case PROP_OVERLINE_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);
        set_overline_rgba (text_tag, color);
      }
      break;

    case PROP_RISE:
      priv->rise_set = TRUE;
      priv->values->appearance.rise = g_value_get_int (value);
      g_object_notify (object, "rise-set");
      size_changed = TRUE;      
      break;

    case PROP_BACKGROUND_FULL_HEIGHT:
      priv->bg_full_height_set = TRUE;
      priv->values->bg_full_height = g_value_get_boolean (value);
      g_object_notify (object, "background-full-height-set");
      break;

    case PROP_LANGUAGE:
      priv->language_set = TRUE;
      priv->values->language = pango_language_from_string (g_value_get_string (value));
      g_object_notify (object, "language-set");
      break;

    case PROP_TABS:
      priv->tabs_set = TRUE;

      if (priv->values->tabs)
        pango_tab_array_free (priv->values->tabs);

      /* FIXME I'm not sure if this is a memleak or not */
      priv->values->tabs =
        pango_tab_array_copy (g_value_get_boxed (value));

      g_object_notify (object, "tabs-set");
      
      size_changed = TRUE;
      break;

    case PROP_INVISIBLE:
      priv->invisible_set = TRUE;
      priv->values->invisible = g_value_get_boolean (value);
      g_object_notify (object, "invisible-set");
      size_changed = TRUE;
      break;
      
    case PROP_PARAGRAPH_BACKGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_pg_bg_rgba (text_tag, NULL);       /* reset paragraph_background_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_pg_bg_rgba (text_tag, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));
      }
      break;

    case PROP_PARAGRAPH_BACKGROUND_RGBA:
      {
        GdkRGBA *color = g_value_get_boxed (value);

        set_pg_bg_rgba (text_tag, color);
      }
      break;

    case PROP_FALLBACK:
      priv->fallback_set = TRUE;
      priv->values->no_fallback = !g_value_get_boolean (value);
      g_object_notify (object, "fallback-set");
      break;

    case PROP_LETTER_SPACING:
      priv->letter_spacing_set = TRUE;
      priv->values->letter_spacing = g_value_get_int (value);
      g_object_notify (object, "letter-spacing-set");
      break;

    case PROP_FONT_FEATURES:
      priv->font_features_set = TRUE;
      priv->values->font_features = g_value_dup_string (value);
      g_object_notify (object, "font-features-set");
      break;

    case PROP_ALLOW_BREAKS:
      priv->allow_breaks_set = TRUE;
      priv->values->no_breaks = !g_value_get_boolean (value);
      g_object_notify (object, "allow-breaks-set");
      break;

    case PROP_SHOW_SPACES:
      priv->show_spaces_set = TRUE;
      priv->values->show_spaces = g_value_get_flags (value);
      g_object_notify (object, "show-spaces-set");
      break;

    case PROP_INSERT_HYPHENS:
      priv->insert_hyphens_set = TRUE;
      priv->values->no_hyphens = !g_value_get_boolean (value);
      g_object_notify (object, "insert-hyphens-set");
      break;

    case PROP_TEXT_TRANSFORM:
      priv->text_transform_set = TRUE;
      priv->values->text_transform = g_value_get_enum (value);
      g_object_notify (object, "text-transform-set");
      break;

    case PROP_WORD:
      priv->word_set = TRUE;
      priv->values->word = g_value_get_boolean (value);
      g_object_notify (object, "word-set");
      break;

    case PROP_SENTENCE:
      priv->sentence_set = TRUE;
      priv->values->sentence = g_value_get_boolean (value);
      g_object_notify (object, "sentence-set");
      break;

    case PROP_ACCUMULATIVE_MARGIN:
      priv->accumulative_margin = g_value_get_boolean (value);
      g_object_notify (object, "accumulative-margin");
      size_changed = TRUE;
      break;

      /* Whether the value should be used... */

    case PROP_BACKGROUND_SET:
      priv->bg_color_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
      priv->fg_color_set = g_value_get_boolean (value);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      if (!g_value_get_boolean (value))
	{
	  if (priv->values->font)
	    pango_font_description_unset_fields (priv->values->font,
						 get_property_font_set_mask (prop_id));
	}
      else
	{
	  PangoFontMask changed_mask;
	  
	  gtk_text_tag_ensure_font (text_tag);
	  changed_mask = set_font_desc_fields (priv->values->font,
					       get_property_font_set_mask (prop_id));
	  notify_fields_changed (G_OBJECT (text_tag), changed_mask);
	}
      break;

    case PROP_SCALE_SET:
      priv->scale_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    case PROP_PIXELS_ABOVE_LINES_SET:
      priv->pixels_above_lines_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_PIXELS_BELOW_LINES_SET:
      priv->pixels_below_lines_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_PIXELS_INSIDE_WRAP_SET:
      priv->pixels_inside_wrap_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_EDITABLE_SET:
      priv->editable_set = g_value_get_boolean (value);
      break;

    case PROP_WRAP_MODE_SET:
      priv->wrap_mode_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_JUSTIFICATION_SET:
      priv->justification_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    case PROP_LEFT_MARGIN_SET:
      priv->left_margin_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_INDENT_SET:
      priv->indent_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_STRIKETHROUGH_SET:
      priv->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_STRIKETHROUGH_RGBA_SET:
      priv->strikethrough_rgba_set = g_value_get_boolean (value);
      break;

    case PROP_RIGHT_MARGIN_SET:
      priv->right_margin_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_UNDERLINE_SET:
      priv->underline_set = g_value_get_boolean (value);
      break;

    case PROP_UNDERLINE_RGBA_SET:
      priv->underline_rgba_set = g_value_get_boolean (value);
      break;

    case PROP_OVERLINE_SET:
      priv->overline_set = g_value_get_boolean (value);
      break;

    case PROP_OVERLINE_RGBA_SET:
      priv->overline_rgba_set = g_value_get_boolean (value);
      break;

    case PROP_RISE_SET:
      priv->rise_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_BACKGROUND_FULL_HEIGHT_SET:
      priv->bg_full_height_set = g_value_get_boolean (value);
      break;

    case PROP_LANGUAGE_SET:
      priv->language_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_TABS_SET:
      priv->tabs_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_INVISIBLE_SET:
      priv->invisible_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    case PROP_PARAGRAPH_BACKGROUND_SET:
      priv->pg_bg_color_set = g_value_get_boolean (value);
      break;

    case PROP_FALLBACK_SET:
      priv->fallback_set = g_value_get_boolean (value);
      break;

    case PROP_LETTER_SPACING_SET:
      priv->letter_spacing_set = g_value_get_boolean (value);
      break;

    case PROP_FONT_FEATURES_SET:
      priv->font_features_set = g_value_get_boolean (value);
      break;

    case PROP_ALLOW_BREAKS_SET:
      priv->allow_breaks_set = g_value_get_boolean (value);
      break;

    case PROP_SHOW_SPACES_SET:
      priv->show_spaces_set = g_value_get_boolean (value);
      break;

    case PROP_INSERT_HYPHENS_SET:
      priv->insert_hyphens_set = g_value_get_boolean (value);
      break;

    case PROP_TEXT_TRANSFORM_SET:
      priv->text_transform_set = g_value_get_boolean (value);
      break;

    case PROP_WORD_SET:
      priv->word_set = g_value_get_boolean (value);
      break;

    case PROP_SENTENCE_SET:
      priv->sentence_set = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  /* The signal is emitted for each set_property(). A possible optimization is
   * to send the signal only once when several properties are set at the same
   * time with e.g. g_object_set(). The signal could be emitted when the notify
   * signal is thawed.
   */
  gtk_text_tag_changed (text_tag, size_changed);
}

static void
gtk_text_tag_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  GtkTextTag *tag = GTK_TEXT_TAG (object);
  GtkTextTagPrivate *priv = tag->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_BACKGROUND_RGBA:
      g_value_set_boxed (value, priv->values->appearance.bg_rgba);
      break;

    case PROP_FOREGROUND_RGBA:
      g_value_set_boxed (value, priv->values->appearance.fg_rgba);
      break;

    case PROP_FONT:
        {
          char *str;

	  gtk_text_tag_ensure_font (tag);

	  str = pango_font_description_to_string (priv->values->font);
          g_value_take_string (value, str);
        }
      break;

    case PROP_FONT_DESC:
      gtk_text_tag_ensure_font (tag);
      g_value_set_boxed (value, priv->values->font);
      break;

    case PROP_FAMILY:
      gtk_text_tag_ensure_font (tag);
      g_value_set_string (value, pango_font_description_get_family (priv->values->font));
      break;
      
    case PROP_STYLE:
      gtk_text_tag_ensure_font (tag);
      g_value_set_enum (value, pango_font_description_get_style (priv->values->font));
      break;
      
    case PROP_VARIANT:
      gtk_text_tag_ensure_font (tag);
      g_value_set_enum (value, pango_font_description_get_variant (priv->values->font));
      break;
      
    case PROP_WEIGHT:
      gtk_text_tag_ensure_font (tag);
      g_value_set_int (value, pango_font_description_get_weight (priv->values->font));
      break;
      
    case PROP_STRETCH:
      gtk_text_tag_ensure_font (tag);
      g_value_set_enum (value, pango_font_description_get_stretch (priv->values->font));
      break;
      
    case PROP_SIZE:
      gtk_text_tag_ensure_font (tag);
      g_value_set_int (value, pango_font_description_get_size (priv->values->font));
      break;
      
    case PROP_SIZE_POINTS:
      gtk_text_tag_ensure_font (tag);
      g_value_set_double (value, ((double)pango_font_description_get_size (priv->values->font)) / (double)PANGO_SCALE);
      break;
  
    case PROP_SCALE:
      g_value_set_double (value, priv->values->font_scale);
      break;
      
    case PROP_PIXELS_ABOVE_LINES:
      g_value_set_int (value,  priv->values->pixels_above_lines);
      break;

    case PROP_PIXELS_BELOW_LINES:
      g_value_set_int (value,  priv->values->pixels_below_lines);
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      g_value_set_int (value,  priv->values->pixels_inside_wrap);
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_float (value, priv->values->line_height);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->values->editable);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, priv->values->wrap_mode);
      break;

    case PROP_JUSTIFICATION:
      g_value_set_enum (value, priv->values->justification);
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, priv->values->direction);
      break;
      
    case PROP_LEFT_MARGIN:
      g_value_set_int (value,  priv->values->left_margin);
      break;

    case PROP_INDENT:
      g_value_set_int (value,  priv->values->indent);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, priv->values->appearance.strikethrough);
      break;

    case PROP_STRIKETHROUGH_RGBA:
      if (priv->strikethrough_rgba_set)
        g_value_set_boxed (value, priv->values->appearance.strikethrough_rgba);
      break;

    case PROP_RIGHT_MARGIN:
      g_value_set_int (value, priv->values->right_margin);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, priv->values->appearance.underline);
      break;

    case PROP_UNDERLINE_RGBA:
      if (priv->underline_rgba_set)
        g_value_set_boxed (value, priv->values->appearance.underline_rgba);
      break;

    case PROP_OVERLINE:
      g_value_set_enum (value, priv->values->appearance.overline);
      break;

    case PROP_OVERLINE_RGBA:
      if (priv->overline_rgba_set)
        g_value_set_boxed (value, priv->values->appearance.overline_rgba);
      break;

    case PROP_RISE:
      g_value_set_int (value, priv->values->appearance.rise);
      break;

    case PROP_BACKGROUND_FULL_HEIGHT:
      g_value_set_boolean (value, priv->values->bg_full_height);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, pango_language_to_string (priv->values->language));
      break;

    case PROP_TABS:
      if (priv->values->tabs)
        g_value_set_boxed (value, priv->values->tabs);
      break;

    case PROP_INVISIBLE:
      g_value_set_boolean (value, priv->values->invisible);
      break;
      

    case PROP_PARAGRAPH_BACKGROUND_RGBA:
      g_value_set_boxed (value, priv->values->pg_bg_rgba);
      break;

    case PROP_FALLBACK:
      g_value_set_boolean (value, !priv->values->no_fallback);
      break;

    case PROP_LETTER_SPACING:
      g_value_set_int (value, priv->values->letter_spacing);
      break;

    case PROP_FONT_FEATURES:
      g_value_set_string (value, priv->values->font_features);
      break;

    case PROP_ALLOW_BREAKS:
      g_value_set_boolean (value, !priv->values->no_breaks);
      break;

    case PROP_SHOW_SPACES:
      g_value_set_flags (value, priv->values->show_spaces);
      break;

    case PROP_INSERT_HYPHENS:
      g_value_set_boolean (value, !priv->values->no_hyphens);
      break;

    case PROP_TEXT_TRANSFORM:
      g_value_set_enum (value, priv->values->text_transform);
      break;

    case PROP_WORD:
      g_value_set_boolean (value, priv->values->word);
      break;

    case PROP_SENTENCE:
      g_value_set_boolean (value, priv->values->sentence);
      break;

    case PROP_ACCUMULATIVE_MARGIN:
      g_value_set_boolean (value, priv->accumulative_margin);
      break;

    case PROP_BACKGROUND_SET:
      g_value_set_boolean (value, priv->bg_color_set);
      break;

    case PROP_FOREGROUND_SET:
      g_value_set_boolean (value, priv->fg_color_set);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      {
	PangoFontMask set_mask = priv->values->font ? pango_font_description_get_set_fields (priv->values->font) : 0;
	PangoFontMask test_mask = get_property_font_set_mask (prop_id);
	g_value_set_boolean (value, (set_mask & test_mask) != 0);

	break;
      }

    case PROP_SCALE_SET:
      g_value_set_boolean (value, priv->scale_set);
      break;
      
    case PROP_PIXELS_ABOVE_LINES_SET:
      g_value_set_boolean (value, priv->pixels_above_lines_set);
      break;

    case PROP_PIXELS_BELOW_LINES_SET:
      g_value_set_boolean (value, priv->pixels_below_lines_set);
      break;

    case PROP_PIXELS_INSIDE_WRAP_SET:
      g_value_set_boolean (value, priv->pixels_inside_wrap_set);
      break;

    case PROP_LINE_HEIGHT_SET:
      g_value_set_boolean (value, priv->line_height_set);
      break;

    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, priv->editable_set);
      break;

    case PROP_WRAP_MODE_SET:
      g_value_set_boolean (value, priv->wrap_mode_set);
      break;

    case PROP_JUSTIFICATION_SET:
      g_value_set_boolean (value, priv->justification_set);
      break;
      
    case PROP_LEFT_MARGIN_SET:
      g_value_set_boolean (value, priv->left_margin_set);
      break;

    case PROP_INDENT_SET:
      g_value_set_boolean (value, priv->indent_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, priv->strikethrough_set);
      break;

    case PROP_STRIKETHROUGH_RGBA_SET:
      g_value_set_boolean (value, priv->strikethrough_rgba_set);
      break;

    case PROP_RIGHT_MARGIN_SET:
      g_value_set_boolean (value, priv->right_margin_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, priv->underline_set);
      break;

    case PROP_UNDERLINE_RGBA_SET:
      g_value_set_boolean (value, priv->underline_rgba_set);
      break;

    case PROP_OVERLINE_SET:
      g_value_set_boolean (value, priv->overline_set);
      break;

    case PROP_OVERLINE_RGBA_SET:
      g_value_set_boolean (value, priv->overline_rgba_set);
      break;

    case PROP_RISE_SET:
      g_value_set_boolean (value, priv->rise_set);
      break;

    case PROP_BACKGROUND_FULL_HEIGHT_SET:
      g_value_set_boolean (value, priv->bg_full_height_set);
      break;

    case PROP_LANGUAGE_SET:
      g_value_set_boolean (value, priv->language_set);
      break;

    case PROP_TABS_SET:
      g_value_set_boolean (value, priv->tabs_set);
      break;

    case PROP_INVISIBLE_SET:
      g_value_set_boolean (value, priv->invisible_set);
      break;
      
    case PROP_PARAGRAPH_BACKGROUND_SET:
      g_value_set_boolean (value, priv->pg_bg_color_set);
      break;

    case PROP_FALLBACK_SET:
      g_value_set_boolean (value, priv->fallback_set);
      break;

    case PROP_LETTER_SPACING_SET:
      g_value_set_boolean (value, priv->letter_spacing_set);
      break;

    case PROP_FONT_FEATURES_SET:
      g_value_set_boolean (value, priv->font_features_set);
      break;

    case PROP_ALLOW_BREAKS_SET:
      g_value_set_boolean (value, priv->allow_breaks_set);
      break;

    case PROP_SHOW_SPACES_SET:
      g_value_set_boolean (value, priv->show_spaces_set);
      break;

    case PROP_INSERT_HYPHENS_SET:
      g_value_set_boolean (value, priv->insert_hyphens_set);
      break;

    case PROP_TEXT_TRANSFORM_SET:
      g_value_set_boolean (value, priv->text_transform_set);
      break;

    case PROP_WORD_SET:
      g_value_set_boolean (value, priv->word_set);
      break;

    case PROP_SENTENCE_SET:
      g_value_set_boolean (value, priv->sentence_set);
      break;

    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    case PROP_PARAGRAPH_BACKGROUND:
      g_warning ("'foreground', 'background' and 'paragraph_background' properties are not readable, use 'foreground_gdk', 'background_gdk' and 'paragraph_background_gdk'");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*
 * Tag operations
 */

typedef struct {
  int high;
  int low;
  int delta;
} DeltaData;

static void
delta_priority_foreach (GtkTextTag *tag, gpointer user_data)
{
  GtkTextTagPrivate *priv = tag->priv;
  DeltaData *dd = user_data;

  if (priv->priority >= dd->low && priv->priority <= dd->high)
    priv->priority += dd->delta;
}

/**
 * gtk_text_tag_get_priority:
 * @tag: a `GtkTextTag`
 *
 * Get the tag priority.
 *
 * Returns: The tag’s priority.
 */
int
gtk_text_tag_get_priority (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), 0);

  return tag->priv->priority;
}

/**
 * gtk_text_tag_set_priority:
 * @tag: a `GtkTextTag`
 * @priority: the new priority
 *
 * Sets the priority of a `GtkTextTag`.
 *
 * Valid priorities start at 0 and go to one less than
 * [method@Gtk.TextTagTable.get_size]. Each tag in a table
 * has a unique priority; setting the priority of one tag shifts
 * the priorities of all the other tags in the table to maintain
 * a unique priority for each tag.
 *
 * Higher priority tags “win” if two tags both set the same text
 * attribute. When adding a tag to a tag table, it will be assigned
 * the highest priority in the table by default; so normally the
 * precedence of a set of tags is the order in which they were added
 * to the table, or created with [method@Gtk.TextBuffer.create_tag],
 * which adds the tag to the buffer’s table automatically.
 */
void
gtk_text_tag_set_priority (GtkTextTag *tag,
                           int         priority)
{
  GtkTextTagPrivate *priv;
  DeltaData dd;

  g_return_if_fail (GTK_IS_TEXT_TAG (tag));

  priv = tag->priv;

  g_return_if_fail (priv->table != NULL);
  g_return_if_fail (priority >= 0);
  g_return_if_fail (priority < gtk_text_tag_table_get_size (priv->table));

  if (priority == priv->priority)
    return;

  if (priority < priv->priority)
    {
      dd.low = priority;
      dd.high = priv->priority - 1;
      dd.delta = 1;
    }
  else
    {
      dd.low = priv->priority + 1;
      dd.high = priority;
      dd.delta = -1;
    }

  gtk_text_tag_table_foreach (priv->table,
                              delta_priority_foreach,
                              &dd);

  priv->priority = priority;
}

/**
 * gtk_text_tag_changed:
 * @tag: a `GtkTextTag`
 * @size_changed: whether the change affects the `GtkTextView` layout
 *
 * Emits the [signal@Gtk.TextTagTable::tag-changed] signal on the
 * `GtkTextTagTable` where the tag is included.
 *
 * The signal is already emitted when setting a `GtkTextTag` property.
 * This function is useful for a `GtkTextTag` subclass.
 */
void
gtk_text_tag_changed (GtkTextTag *tag,
                      gboolean    size_changed)
{
  GtkTextTagPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_TAG (tag));

  priv = tag->priv;

  /* This is somewhat weird since we emit another object's signal here, but the
   * two objects are already tightly bound. If a GtkTextTag::changed signal is
   * added, this would increase significantly the number of signal connections.
   */
  if (priv->table != NULL)
    _gtk_text_tag_table_tag_changed (priv->table, tag, size_changed);
}

static int
tag_sort_func (gconstpointer first, gconstpointer second)
{
  GtkTextTag *tag1, *tag2;

  tag1 = * (GtkTextTag **) first;
  tag2 = * (GtkTextTag **) second;
  return tag1->priv->priority - tag2->priv->priority;
}

void
_gtk_text_tag_array_sort (GPtrArray *tags)
{
  g_ptr_array_sort (tags, tag_sort_func);
}
