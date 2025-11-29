/* gtkaccessibletext.h: Interface for accessible objects containing text
 *
 * SPDX-FileCopyrightText: 2023  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkaccessible.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSIBLE_TEXT (gtk_accessible_text_get_type ())

/**
 * GtkAccessibleText:
 *
 * An interface for accessible objects containing formatted text.
 *
 * The `GtkAccessibleText` interfaces is meant to be implemented by accessible
 * objects that have text formatted with attributes, or non-trivial text contents.
 *
 * You should use the [enum@Gtk.AccessibleProperty.LABEL] or the
 * [enum@Gtk.AccessibleProperty.DESCRIPTION] properties for accessible
 * objects containing simple, unformatted text.
 *
 * Since: 4.14
 */
GDK_AVAILABLE_IN_4_14
G_DECLARE_INTERFACE (GtkAccessibleText, gtk_accessible_text, GTK, ACCESSIBLE_TEXT, GtkAccessible)

/**
 * GtkAccessibleTextRange:
 * @start: the start of the range, in characters
 * @length: the length of the range, in characters
 *
 * A range inside the text of an accessible object.
 *
 * Since: 4.14
 */
typedef struct {
  gsize start;
  gsize length;
} GtkAccessibleTextRange;

/**
 * GtkAccessibleTextGranularity:
 * @GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER: Use the boundary between
 *   characters (including non-printing characters)
 * @GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD: Use the boundary between words,
 *   starting from the beginning of the current word and ending at the
 *   beginning of the next word
 * @GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE: Use the boundary between
 *   sentences, starting from the beginning of the current sentence and
 *   ending at the beginning of the next sentence
 * @GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE: Use the boundary between lines,
 *   starting from the beginning of the current line and ending at the
 *   beginning of the next line
 * @GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH: Use the boundary between
 *   paragraphs, starting from the beginning of the current paragraph and
 *   ending at the beginning of the next paragraph
 *
 * The granularity for queries about the text contents of a [iface@Gtk.AccessibleText]
 * implementation.
 *
 * Since: 4.14
 */
typedef enum {
  GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER,
  GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD,
  GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE,
  GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE,
  GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH
} GtkAccessibleTextGranularity;

/**
 * GtkAccessibleTextContentChange:
 * @GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT: contents change as the result of
 *   an insert operation
 * @GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE: contents change as the result of
 *   a remove operation
 *
 * The type of contents change operation.
 *
 * Since: 4.14
 */
typedef enum {
  GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
  GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE
} GtkAccessibleTextContentChange;

/**
 * GtkAccessibleTextInterface:
 *
 * The interface vtable for accessible objects containing text.
 *
 * Since: 4.14
 */
struct _GtkAccessibleTextInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  /**
   * GtkAccessibleTextInterface::get_contents:
   * @self: the accessible object
   * @start: the beginning of the range, in characters
   * @end: the end of the range, in characters
   *
   * Retrieve the current contents of the accessible object within
   * the given range.
   *
   * If @end is `G_MAXUINT`, the end of the range is the full content
   * of the accessible object.
   *
   * Returns: (transfer full): the requested slice of the contents of the
   *   accessible object, as UTF-8. Note that the slice does not have to
   *   be NUL-terminated
   *
   * Since: 4.14
   */
  GBytes * (* get_contents) (GtkAccessibleText *self,
                             unsigned int       start,
                             unsigned int       end);

  /**
   * GtkAccessibleTextInterface::get_contents_at:
   * @self: the accessible object
   * @offset: the offset, in characters
   * @granularity: the granularity of the query
   * @start: (out): the start of the range, in characters
   * @end: (out): the end of the range, in characters
   *
   * Retrieve the current contents of the accessible object starting
   * from the given offset, and using the given granularity.
   *
   * The @start and @end values contain the boundaries of the text.
   *
   * Returns: (transfer full): the requested slice of the contents of the
   *   accessible object, as UTF-8. Note that the slice does not have to
   *   be NUL-terminated
   *
   * Since: 4.14
   */
  GBytes * (* get_contents_at) (GtkAccessibleText            *self,
                                unsigned int                  offset,
                                GtkAccessibleTextGranularity  granularity,
                                unsigned int                 *start,
                                unsigned int                 *end);

  /**
   * GtkAccessibleTextInterface::get_caret_position:
   * @self: the accessible object
   *
   * Retrieves the position of the caret inside the accessible object.
   *
   * Returns: the position of the caret, in characters
   *
   * Since: 4.14
   */
  unsigned int (* get_caret_position) (GtkAccessibleText *self);

  /**
   * GtkAccessibleTextInterface::get_selection:
   * @self: the accessible object
   * @n_ranges: (out): the number of selection ranges
   * @ranges: (optional) (out) (array length=n_ranges) (transfer container): the selection ranges
   *
   * Retrieves the selection ranges in the accessible object.
   *
   * If this function returns true, `n_ranges` will be set to a value
   * greater than or equal to one, and @ranges will be set to a newly
   * allocated array of [struct#Gtk.AccessibleTextRange].
   *
   * Returns: true if there is at least a selection inside the
   *   accessible object, and false otherwise
   *
   * Since: 4.14
   */
  gboolean (* get_selection) (GtkAccessibleText *self,
                              gsize *n_ranges,
                              GtkAccessibleTextRange **ranges);

  /**
   * GtkAccessibleTextInterface::get_attributes:
   * @self: the accessible object
   * @offset: the offset, in characters
   * @n_ranges: (out): the number of attributes
   * @ranges: (out) (array length=n_ranges) (optional) (transfer container): the ranges of the attributes
   *   inside the accessible object
   * @attribute_names: (out) (array zero-terminated=1) (optional) (transfer full): the
   *   names of the attributes inside the accessible object
   * @attribute_values: (out) (array zero-terminated=1) (optional) (transfer full): the
   *   values of the attributes inside the accessible object
   *
   * Retrieves the text attributes inside the accessible object.
   *
   * Each attribute is composed by:
   *
   * - a range
   * - a name
   * - a value
   *
   * It is left to the implementation to determine the serialization format
   * of the value to a string.
   *
   * GTK provides support for various text attribute names and values, but
   * implementations of this interface are free to add their own attributes.
   *
   * If this function returns true, `n_ranges` will be set to a value
   * greater than or equal to one, @ranges will be set to a newly
   * allocated array of [struct#Gtk.AccessibleTextRange].
   *
   * Returns: true if the accessible object has at least an attribute,
   *   and false otherwise
   *
   * Since: 4.14
   */
  gboolean (* get_attributes) (GtkAccessibleText *self,
                               unsigned int offset,
                               gsize *n_ranges,
                               GtkAccessibleTextRange **ranges,
                               char ***attribute_names,
                               char ***attribute_values);

  /**
   * GtkAccessibleTextInterface::get_default_attributes:
   * @self: the accessible object
   * @attribute_names: (out) (array zero-terminated=1) (optional) (transfer full): the
   *   names of the default attributes inside the accessible object
   * @attribute_values: (out) (array zero-terminated=1) (optional) (transfer full): the
   *   values of the default attributes inside the accessible object
   *
   * Retrieves the default text attributes inside the accessible object.
   *
   * Each attribute is composed by:
   *
   * - a name
   * - a value
   *
   * It is left to the implementation to determine the serialization format
   * of the value to a string.
   *
   * GTK provides support for various text attribute names and values, but
   * implementations of this interface are free to add their own attributes.
   *
   * Since: 4.14
   */
  void (* get_default_attributes) (GtkAccessibleText *self,
                                   char ***attribute_names,
                                   char ***attribute_values);
};

GDK_AVAILABLE_IN_4_14
void gtk_accessible_text_update_caret_position (GtkAccessibleText *self);

GDK_AVAILABLE_IN_4_14
void gtk_accessible_text_update_selection_bound (GtkAccessibleText *self);

GDK_AVAILABLE_IN_4_14
void gtk_accessible_text_update_contents (GtkAccessibleText              *self,
                                          GtkAccessibleTextContentChange  change,
                                          unsigned int                    start,
                                          unsigned int                    end);

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_FAMILY:
 *
 * An attribute for the font family name.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_FAMILY                   "family-name"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STYLE:
 *
 * An attribute for the font style.
 *
 * Possible values are:
 *
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE_NORMAL]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE_OBLIQUE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE_ITALIC]
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STYLE                    "style"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_WEIGHT:
 *
 * An attribute for the font weight.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_WEIGHT                   "weight"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT:
 *
 * An attribute for the font variant.
 *
 * Possible values are:
 *
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_SMALL_CAPS]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_SMALL_CAPS]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_PETITE_CAPS]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_PETITE_CAPS]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_UNICASE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT_TITLE_CAPS]
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT                  "variant"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH:
 *
 * An attribute for the font stretch type.
 *
 * Possible values are:
 *
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH_ULTRA_CONDENSED]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH_EXTRA_CONDENSED]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH_CONDENSED]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH_SEMI_CONDENSED]
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH                  "stretch"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_SIZE:
 *
 * An attribute for the font size, expressed in points.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_SIZE                     "size"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_FOREGROUND:
 *
 * An attribute for the foreground color, expressed as an RGB value
 * encoded in a string using the format: `{r8},{g8},{b8}`.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_FOREGROUND               "fg-color"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_BACKGROUND:
 *
 * An attribute for the background color, expressed as an RGB value
 * encoded in a string using the format: `{r8},{g8},{b8}`.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_BACKGROUND               "bg-color"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE:
 *
 * An attribute for the underline style.
 *
 * Possible values are:
 *
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE_NONE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE_SINGLE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE_DOUBLE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE_ERROR]
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE                "underline"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE:
 *
 * An attribute for the overline style.
 *
 * Possible values are:
 *
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_OVERLINE_NONE]
 * - [const@Gtk.ACCESSIBLE_ATTRIBUTE_OVERLINE_SINGLE]
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE                 "overline"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRIKETHROUGH:
 *
 * An attribute for strikethrough text.
 *
 * Possible values are `true` or `false`.
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRIKETHROUGH            "strikethrough"

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STYLE_NORMAL:
 *
 * The "normal" style value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STYLE_NORMAL             "normal"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STYLE_OBLIQUE:
 *
 * The "oblique" style value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STYLE_OBLIQUE            "oblique"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STYLE_ITALIC:
 *
 * The "italic" style value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STYLE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STYLE_ITALIC             "italic"

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_SMALL_CAPS:
 *
 * The "small caps" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_SMALL_CAPS       "small-caps"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_SMALL_CAPS:
 *
 * The "all small caps" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_SMALL_CAPS   "all-small-caps"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_PETITE_CAPS:
 *
 * The "petite caps" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_PETITE_CAPS      "petite-caps"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_PETITE_CAPS:
 *
 * The "all petite caps" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_ALL_PETITE_CAPS  "all-petite-caps"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_UNICASE:
 *
 * The "unicase" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_UNICASE          "unicase"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_TITLE_CAPS:
 *
 * The "title caps" variant value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_VARIANT].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_VARIANT_TITLE_CAPS       "title-caps"

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_ULTRA_CONDENSED:
 *
 * The "ultra condensed" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_ULTRA_CONDENSED  "ultra_condensed"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXTRA_CONDENSED:
 *
 * The "extra condensed" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXTRA_CONDENSED  "extra_condensed"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_CONDENSED:
 *
 * The "condensed" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_CONDENSED        "condensed"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_SEMI_CONDENSED:
 *
 * The "semi condensed" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_SEMI_CONDENSED   "semi_condensed"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_NORMAL:
 *
 * The "normal" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_NORMAL           "normal"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_SEMI_EXPANDED:
 *
 * The "semi expanded" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_SEMI_EXPANDED    "semi_expanded"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXPANDED:
 *
 * The "expanded" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXPANDED         "expanded"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXTRA_EXPANDED:
 *
 * The "extra expanded" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_EXTRA_EXPANDED   "extra_expanded"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_ULTRA_EXPANDED:
 *
 * The "ultra expanded" stretch value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_STRETCH].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_STRETCH_ULTRA_EXPANDED   "ultra_expanded"

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_NONE:
 *
 * The "none" underline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_NONE           "none"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_SINGLE:
 *
 * The "single" underline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_SINGLE         "single"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_DOUBLE:
 *
 * The "double" underline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_DOUBLE         "double"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_ERROR:
 *
 * The "error" underline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_UNDERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_ERROR          "error"

/**
 * GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE_NONE:
 *
 * The "none" overline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_OVERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE_NONE            "none"
/**
 * GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE_SINGLE:
 *
 * The "single" overline value for [const@Gtk.ACCESSIBLE_ATTRIBUTE_OVERLINE].
 *
 * Since: 4.14
 */
#define GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE_SINGLE          "single"


G_END_DECLS
