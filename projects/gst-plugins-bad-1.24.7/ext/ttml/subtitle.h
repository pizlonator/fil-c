/* GStreamer
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Author: Chris Bass <dash@rd.bbc.co.uk>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_SUBTITLE_H__
#define __GST_SUBTITLE_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/gstminiobject.h>

G_BEGIN_DECLS

typedef struct _GstSubtitleColor GstSubtitleColor;
typedef struct _GstSubtitleStyleSet GstSubtitleStyleSet;
typedef struct _GstSubtitleElement GstSubtitleElement;
typedef struct _GstSubtitleBlock GstSubtitleBlock;
typedef struct _GstSubtitleRegion GstSubtitleRegion;

/**
 * GstSubtitleWritingMode:
 * @GST_SUBTITLE_WRITING_MODE_LRTB: Text progression is left-to-right,
 * top-to-bottom.
 * @GST_SUBTITLE_WRITING_MODE_RLTB: Text progression is right-to-left,
 * top-to-bottom.
 * @GST_SUBTITLE_WRITING_MODE_TBRL: Text progression is top-to-bottom,
 * right-to-left.
 * @GST_SUBTITLE_WRITING_MODE_TBLR: Text progression is top-to-bottom,
 * left-to-right.
 *
 * Writing mode of text content. The values define the direction of progression
 * of both inline text (#GstSubtitleElements) and blocks of text
 * (#GstSubtitleBlocks).
 */
typedef enum {
    GST_SUBTITLE_WRITING_MODE_LRTB,
    GST_SUBTITLE_WRITING_MODE_RLTB,
    GST_SUBTITLE_WRITING_MODE_TBRL,
    GST_SUBTITLE_WRITING_MODE_TBLR
} GstSubtitleWritingMode;

/**
 * GstSubtitleDisplayAlign:
 * @GST_SUBTITLE_DISPLAY_ALIGN_BEFORE: Blocks should be aligned at the start of
 * the containing region.
 * @GST_SUBTITLE_DISPLAY_ALIGN_CENTER: Blocks should be aligned in the center
 * of the containing region.
 * @GST_SUBTITLE_DISPLAY_ALIGN_AFTER: Blocks should be aligned to the end of
 * the containing region.
 *
 * Defines the alignment of text blocks within a region in the direction in
 * which blocks are being stacked. For text that is written left-to-right and
 * top-to-bottom, this corresponds to the vertical alignment of text blocks.
 */
typedef enum {
    GST_SUBTITLE_DISPLAY_ALIGN_BEFORE,
    GST_SUBTITLE_DISPLAY_ALIGN_CENTER,
    GST_SUBTITLE_DISPLAY_ALIGN_AFTER
} GstSubtitleDisplayAlign;

/**
 * GstSubtitleBackgroundMode:
 * @GST_SUBTITLE_BACKGROUND_MODE_ALWAYS: Background rectangle should be visible
 * at all times.
 * @GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE: Background rectangle should be
 * visible only when text is rendered into the corresponding region.
 *
 * Defines whether the background rectangle of a region should be visible at
 * all times or only when text is rendered within it.
 */
typedef enum {
    GST_SUBTITLE_BACKGROUND_MODE_ALWAYS,
    GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE
} GstSubtitleBackgroundMode;

/**
 * GstSubtitleOverflowMode:
 * @GST_SUBTITLE_OVERFLOW_MODE_HIDDEN: If text and/or background rectangles
 * flowed into the region overflow the bounds of that region, they should
 * be clipped at the region boundary.
 * @GST_SUBTITLE_OVERFLOW_MODE_VISIBLE: If text and/or background rectangles
 * flowed into the region overflow the bounds of that region, they should be
 * allowed to overflow the region boundary.
 *
 * Defines what should happen to text that overflows its containing region.
 */
typedef enum {
    GST_SUBTITLE_OVERFLOW_MODE_HIDDEN,
    GST_SUBTITLE_OVERFLOW_MODE_VISIBLE
} GstSubtitleOverflowMode;

/**
 * GstSubtitleColor:
 * @r: Red value.
 * @g: Green value.
 * @b: Blue value.
 * @a: Alpha value (0 = totally transparent; 255 = totally opaque).
 *
 * Describes an RGBA color.
 */
struct _GstSubtitleColor {
  guint8 r;
  guint8 g;
  guint8 b;
  guint8 a;
};

/**
 * GstSubtitleTextDirection:
 * @GST_SUBTITLE_TEXT_DIRECTION_LTR: Text direction is left-to-right.
 * @GST_SUBTITLE_TEXT_DIRECTION_RTL: Text direction is right-to-left.
 *
 * Defines the progression direction of unicode text that is being treated by
 * the unicode bidirectional algorithm as embedded or overridden (see
 * http://unicode.org/reports/tr9/ for more details of the unicode
 * bidirectional algorithm).
 */
typedef enum {
  GST_SUBTITLE_TEXT_DIRECTION_LTR,
  GST_SUBTITLE_TEXT_DIRECTION_RTL
} GstSubtitleTextDirection;

/**
 * GstSubtitleTextAlign:
 * @GST_SUBTITLE_TEXT_ALIGN_START: Text areas should be rendered at the
 * start of the block area, with respect to the direction in which text is
 * being rendered. For text that is rendered left-to-right this corresponds to
 * the left of the block area; for text that is rendered right-to-left this
 * corresponds to the right of the block area.
 * @GST_SUBTITLE_TEXT_ALIGN_LEFT: Text areas should be rendered at the left of
 * the block area.
 * @GST_SUBTITLE_TEXT_ALIGN_CENTER: Text areas should be rendered at the center
 * of the block area.
 * @GST_SUBTITLE_TEXT_ALIGN_RIGHT: Text areas should be rendered at the right
 * of the block area.
 * @GST_SUBTITLE_TEXT_ALIGN_END: Text areas should be rendered at the end of
 * the block area, with respect to the direction in which text is being
 * rendered. For text that is rendered left-to-right this corresponds to the
 * right of the block area; for text that is rendered right-to-left this
 * corresponds to end of the block area.
 *
 * Defines how inline text areas within a block should be aligned within the
 * block area.
 */
typedef enum {
  GST_SUBTITLE_TEXT_ALIGN_START,
  GST_SUBTITLE_TEXT_ALIGN_LEFT,
  GST_SUBTITLE_TEXT_ALIGN_CENTER,
  GST_SUBTITLE_TEXT_ALIGN_RIGHT,
  GST_SUBTITLE_TEXT_ALIGN_END
} GstSubtitleTextAlign;

/**
 * GstSubtitleFontStyle:
 * @GST_SUBTITLE_FONT_STYLE_NORMAL: Normal font style.
 * @GST_SUBTITLE_FONT_STYLE_ITALIC: Italic font style.
 *
 * Defines styling that should be applied to the glyphs of a font used to
 * render text within an inline text element.
 */
typedef enum {
  GST_SUBTITLE_FONT_STYLE_NORMAL,
  GST_SUBTITLE_FONT_STYLE_ITALIC
} GstSubtitleFontStyle;

/**
 * GstSubtitleFontWeight:
 * @GST_SUBTITLE_FONT_WEIGHT_NORMAL: Normal weight.
 * @GST_SUBTITLE_FONT_WEIGHT_BOLD: Bold weight.
 *
 * Defines the font weight that should be applied to the glyphs of a font used
 * to render text within an inline text element.
 */
typedef enum {
  GST_SUBTITLE_FONT_WEIGHT_NORMAL,
  GST_SUBTITLE_FONT_WEIGHT_BOLD
} GstSubtitleFontWeight;

/**
 * GstSubtitleTextDecoration:
 * @GST_SUBTITLE_TEXT_DECORATION_NONE: Text should not be decorated.
 * @GST_SUBTITLE_TEXT_DECORATION_UNDERLINE: Text should be underlined.
 *
 * Defines the decoration that should be applied to the glyphs of a font used
 * to render text within an inline text element.
 */
typedef enum {
  GST_SUBTITLE_TEXT_DECORATION_NONE,
  GST_SUBTITLE_TEXT_DECORATION_UNDERLINE
} GstSubtitleTextDecoration;

/**
 * GstSubtitleUnicodeBidi:
 * @GST_SUBTITLE_UNICODE_BIDI_NORMAL: Text should progress according the the
 * default behaviour of the Unicode bidirectional algorithm.
 * @GST_SUBTITLE_UNICODE_BIDI_EMBED: Text should be treated as being embedded
 * with a specific direction (given by a #GstSubtitleTextDecoration value
 * defined elsewhere).
 * @GST_SUBTITLE_UNICODE_BIDI_OVERRIDE: Text should be forced to have a
 * specific direction (given by a #GstSubtitleTextDecoration value defined
 * elsewhere).
 *
 * Defines directional embedding or override according to the Unicode
 * bidirectional algorithm. See http://unicode.org/reports/tr9/ for more
 * details of the Unicode bidirectional algorithm.
 */
typedef enum {
  GST_SUBTITLE_UNICODE_BIDI_NORMAL,
  GST_SUBTITLE_UNICODE_BIDI_EMBED,
  GST_SUBTITLE_UNICODE_BIDI_OVERRIDE
} GstSubtitleUnicodeBidi;

/**
 * GstSubtitleWrapping:
 * @GST_SUBTITLE_WRAPPING_ON: Lines that overflow the region boundary should be
 * wrapped.
 * @GST_SUBTITLE_WRAPPING_OFF: Lines that overflow the region boundary should
 * not be wrapped.
 *
 * Defines how a renderer should treat lines of text that overflow the boundary
 * of the region into which they are being rendered.
 */
typedef enum {
  GST_SUBTITLE_WRAPPING_ON,
  GST_SUBTITLE_WRAPPING_OFF
} GstSubtitleWrapping;

/**
 * GstSubtitleMultiRowAlign:
 * @GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO: Lines should be aligned according to the
 * value of #GstSubtitleTextAlign associated with that text.
 * @GST_SUBTITLE_MULTI_ROW_ALIGN_START: Lines should be aligned at their
 * starting edge. The edge that is considered the starting edge depends upon
 * the direction of that text.
 * @GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER: Lines should be center-aligned.
 * @GST_SUBTITLE_MULTI_ROW_ALIGN_END: Lines should be aligned at their trailing
 * edge. The edge that is considered the trailing edge depends upon the
 * direction of that text.
 *
 * Defines how multiple 'rows' (i.e, lines) in a block should be aligned
 * relative to each other.
 *
 * This is based upon the ebutts:multiRowAlign attribute defined in the
 * EBU-TT-D specification.
 */
typedef enum {
  GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO,
  GST_SUBTITLE_MULTI_ROW_ALIGN_START,
  GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER,
  GST_SUBTITLE_MULTI_ROW_ALIGN_END
} GstSubtitleMultiRowAlign;

/**
 * GstSubtitleStyleSet:
 * @text_direction: Defines the direction of text that has been declared by the
 * #GstSubtitleStyleSet:unicode_bidi attribute to be embedded or overridden.
 * Applies to both #GstSubtitleBlocks and #GstSubtitleElements.
 * @font_family: The name of the font family that should be used to render the
 * text of an inline element. Applies only to #GstSubtitleElements.
 * @font_size: The size of the font that should be used to render the text
 * of an inline element. The size is given as a multiple of the display height,
 * where 1.0 equals the height of the display. Applies only to
 * #GstSubtitleElements.
 * @line_height: The inter-baseline separation between lines generated when
 * rendering inline text elements within a block area. The height is given as a
 * multiple of the the overall display height, where 1.0 equals the height of
 * the display. Applies only to #GstSubtitleBlocks.
 * @text_align: Controls the alignent of lines of text within a block area.
 * Note that this attribute does not control the alignment of lines relative to
 * each other within a block area: that is determined by
 * #GstSubtitleStyleSet:multi_row_align. Applies only to #GstSubtitleBlocks.
 * @color: The color that should be used when rendering the text of an inline
 * element. Applies only to #GstSubtitleElements.
 * @background_color: The color of the rectangle that should be rendered behind
 * the contents of a #GstSubtitleRegion, #GstSubtitleBlock or
 * #GstSubtitleElement.
 * @font_style: The style of the font that should be used to render the text
 * of an inline element. Applies only to #GstSubtitleElements.
 * @font_weight: The weight of the font that should be used to render the text
 * of an inline element. Applies only to #GstSubtitleElements.
 * @text_decoration: The decoration that should be applied to the text of an
 * inline element. Applies only to #GstSubtitleElements.
 * @unicode_bidi: Controls how unicode text within a block or inline element
 * should be treated by the unicode bidirectional algorithm. Applies to both
 * #GstSubtitleBlocks and #GstSubtitleElements.
 * @wrap_option: Defines whether or not automatic line breaking should apply to
 * the lines generated when rendering a block of text elements. Applies only to
 * #GstSubtitleBlocks.
 * @multi_row_align: Defines how 'rows' (i.e., lines) within a text block
 * should be aligned relative to each other. Note that this attribute does not
 * determine how a block of text is aligned within that block area: that is
 * determined by @text_align. Applies only to #GstSubtitleBlocks.
 * @line_padding: Defines how much horizontal padding should be added on the
 * start and end of each rendered line; this allows the insertion of space
 * between the start/end of text lines and their background rectangles for
 * better-looking subtitles. This is based upon the ebutts:linePadding
 * attribute defined in the EBU-TT-D specification. Applies only to
 * #GstSubtitleBlocks.
 * @origin_x: The horizontal origin of a region into which text blocks may be
 * rendered. Given as a multiple of the overall display width, where 1.0 equals
 * the width of the display. Applies only to #GstSubtitleRegions.
 * @origin_y: The vertical origin of a region into which text blocks may be
 * rendered. Given as a multiple of the overall display height, where 1.0
 * equals the height of the display. Applies only to #GstSubtitleRegions.
 * @extent_w: The horizontal extent of a region into which text blocks may be
 * rendered. Given as a multiple of the overall display width, where 1.0 equals
 * the width of the display. Applies only to #GstSubtitleRegions.
 * @extent_h: The vertical extent of a region into which text blocks may be
 * rendered. Given as a multiple of the overall display height, where 1.0
 * equals the height of the display. Applies only to #GstSubtitleRegions.
 * @display_align: The alignment of generated text blocks in the direction in
 * which blocks are being stacked. For text that flows left-to-right and
 * top-to-bottom, for example, this corresponds to the vertical alignment of
 * text blocks. Applies only to #GstSubtitleRegions.
 * @padding_start: The horizontal indent of text from the leading edge of a
 * region into which blocks may be rendered. Given as a multiple of the overall
 * display width, where 1.0 equals the width of the display. Applies only to
 * #GstSubtitleRegions.
 * @padding_end: The horizontal indent of text from the trailing edge of a
 * region into which blocks may be rendered. Given as a multiple of the overall
 * display width, where 1.0 equals the width of the display. Applies only to
 * #GstSubtitleRegions.
 * @padding_before: The vertical indent of text from the top edge of a region
 * into which blocks may be rendered. Given as a multiple of the overall
 * display height, where 1.0 equals the height of the display. Applies only to
 * #GstSubtitleRegions.
 * @padding_after: The vertical indent of text from the bottom edge of a
 * region into which blocks may be rendered. Given as a multiple of the overall
 * display height, where 1.0 equals the height of the display. Applies only to
 * #GstSubtitleRegions.
 * @writing_mode: Defines the direction in which both inline elements and
 * blocks should be stacked when rendered into an on-screen region. Applies
 * only to #GstSubtitleRegions.
 * @show_background: Defines whether the background of a region should be
 * displayed at all times or only when it has text rendered into it. Applies
 * only to #GstSubtitleRegions.
 * @overflow: Defines what should happen if text and background rectangles
 * generated by rendering text blocks overflow the size of their containing
 * region. Applies only to #GstSubtitleRegions.
 * @fill_line_gap: Controls whether the rendered backgrounds of text elements
 * in a line fill the whole space between that line and adjacent lines or
 * extends only to the font height of the text in the individual elements (thus
 * this field controls whether or not there are gaps between backgrounds
 * through which the underlying video can be seen). Applies only to
 * #GstSubtitleBlocks.
 *
 * Holds a set of attributes that describes the styling and layout that apply
 * to #GstSubtitleRegion, #GstSubtitleBlock and/or #GstSubtitleElement objects.
 *
 * Note that, though each of the above object types have an associated
 * #GstSubtitleStyleSet, not all attributes in a #GstSubtitleStyleSet type
 * apply to all object types: #GstSubtitleStyleSet:overflow applies only to
 * #GstSubtitleRegions, for example, while #GstSubtitleStyleSet:font_style
 * applies only to #GstSubtitleElements. Some attributes apply to multiple
 * object types: #GstSubtitleStyleSet:background_color, for example, applies to
 * all object types. The types to which each attribute applies is given in the
 * description of that attribute below.
 */
struct _GstSubtitleStyleSet
{
  GstMiniObject mini_object;

  GstSubtitleTextDirection text_direction;
  gchar *font_family;
  gdouble font_size;
  gdouble line_height;
  GstSubtitleTextAlign text_align;
  GstSubtitleColor color;
  GstSubtitleColor background_color;
  GstSubtitleFontStyle font_style;
  GstSubtitleFontWeight font_weight;
  GstSubtitleTextDecoration text_decoration;
  GstSubtitleUnicodeBidi unicode_bidi;
  GstSubtitleWrapping wrap_option;
  GstSubtitleMultiRowAlign multi_row_align;
  gdouble line_padding;
  gdouble origin_x, origin_y;
  gdouble extent_w, extent_h;
  GstSubtitleDisplayAlign display_align;
  gdouble padding_start, padding_end, padding_before, padding_after;
  GstSubtitleWritingMode writing_mode;
  GstSubtitleBackgroundMode show_background;
  GstSubtitleOverflowMode overflow;
  gboolean fill_line_gap;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_style_set_get_type (void);

GstSubtitleStyleSet * gst_subtitle_style_set_new (void);

/**
 * gst_subtitle_style_set_ref:
 * @style_set: A #GstSubtitleStyleSet.
 *
 * Increments the refcount of @style_set.
 *
 * Returns: (transfer full): @style_set.
 */
static inline GstSubtitleStyleSet *
gst_subtitle_style_set_ref (GstSubtitleStyleSet * style_set)
{
  return (GstSubtitleStyleSet *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (style_set));
}

/**
 * gst_subtitle_style_set_unref:
 * @style_set: (transfer full): A #GstSubtitleStyleSet.
 *
 * Decrements the refcount of @style_set. If the refcount reaches 0, @style_set
 * will be freed.
 */
static inline void
gst_subtitle_style_set_unref (GstSubtitleStyleSet * style_set)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (style_set));
}


/**
 * GstSubtitleElement:
 * @mini_object: The parent #GstMiniObject.
 * @style_set: Styling associated with this element.
 * @text_index: Index into the #GstBuffer associated with this
 * #GstSubtitleElement; the index identifies the #GstMemory within the
 * #GstBuffer that holds the #GstSubtitleElement's text.
 * @suppress_whitespace: Indicates whether or not a renderer should suppress
 * whitespace in the element's text.
 *
 * Represents an inline text element.
 *
 * In TTML this would correspond to inline text resulting from a &lt;span&gt;
 * element, an anonymous span (e.g., text within a &lt;p&gt; tag), or a
 * &lt;br&gt; element.
 */
struct _GstSubtitleElement
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;
  guint text_index;
  gboolean suppress_whitespace;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_element_get_type (void);

GstSubtitleElement * gst_subtitle_element_new (GstSubtitleStyleSet * style_set,
    guint text_index, gboolean suppress_whitespace);

/**
 * gst_subtitle_element_ref:
 * @element: A #GstSubtitleElement.
 *
 * Increments the refcount of @element.
 *
 * Returns: (transfer full): @element.
 */
static inline GstSubtitleElement *
gst_subtitle_element_ref (GstSubtitleElement * element)
{
  return (GstSubtitleElement *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (element));
}

/**
 * gst_subtitle_element_unref:
 * @element: (transfer full): A #GstSubtitleElement.
 *
 * Decrements the refcount of @element. If the refcount reaches 0, @element
 * will be freed.
 */
static inline void
gst_subtitle_element_unref (GstSubtitleElement * element)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (element));
}


/**
 * GstSubtitleBlock:
 * @mini_object: The parent #GstMiniObject.
 * @style_set: Styling associated with this block.
 *
 * Represents a text block made up of one or more inline text elements (i.e.,
 * one or more #GstSubtitleElements).
 *
 * In TTML this would correspond to the block of text resulting from the inline
 * elements within a single &lt;p&gt;.
 */
struct _GstSubtitleBlock
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;

  /*< private >*/
  GPtrArray *elements;
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_block_get_type (void);

GstSubtitleBlock * gst_subtitle_block_new (GstSubtitleStyleSet * style_set);

void gst_subtitle_block_add_element (
    GstSubtitleBlock * block,
    GstSubtitleElement * element);

guint gst_subtitle_block_get_element_count (const GstSubtitleBlock * block);

GstSubtitleElement * gst_subtitle_block_get_element (
    const GstSubtitleBlock * block, guint index);

/**
 * gst_subtitle_block_ref:
 * @block: A #GstSubtitleBlock.
 *
 * Increments the refcount of @block.
 *
 * Returns: (transfer full): @block.
 */
static inline GstSubtitleBlock *
gst_subtitle_block_ref (GstSubtitleBlock * block)
{
  return (GstSubtitleBlock *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (block));
}

/**
 * gst_subtitle_block_unref:
 * @block: (transfer full): A #GstSubtitleBlock.
 *
 * Decrements the refcount of @block. If the refcount reaches 0, @block will
 * be freed.
 */
static inline void
gst_subtitle_block_unref (GstSubtitleBlock * block)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (block));
}


/**
 * GstSubtitleRegion:
 * @mini_object: The parent #GstMiniObject.
 * @style_set: Styling associated with this region.
 *
 * Represents an on-screen region in which is displayed zero or more
 * #GstSubtitleBlocks.
 *
 * In TTML this corresponds to a &lt;region&gt; into which zero or more
 * &lt;p&gt;s may be rendered. A #GstSubtitleRegion allows a background
 * rectangle to be displayed in a region area even if no text blocks are
 * rendered into it, as per the behaviour allowed by TTML regions whose
 * tts:showBackground style attribute is set to "always".
 */
struct _GstSubtitleRegion
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;

  /*< private >*/
  GPtrArray *blocks;
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_region_get_type (void);

GstSubtitleRegion * gst_subtitle_region_new (GstSubtitleStyleSet * style_set);

void gst_subtitle_region_add_block (
    GstSubtitleRegion * region,
    GstSubtitleBlock * block);

guint gst_subtitle_region_get_block_count (const GstSubtitleRegion * region);

const GstSubtitleBlock * gst_subtitle_region_get_block (
    const GstSubtitleRegion * region, guint index);

/**
 * gst_subtitle_region_ref:
 * @region: A #GstSubtitleRegion.
 *
 * Increments the refcount of @region.
 *
 * Returns: (transfer full): @region.
 */
static inline GstSubtitleRegion *
gst_subtitle_region_ref (GstSubtitleRegion * region)
{
  return (GstSubtitleRegion *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (region));
}

/**
 * gst_subtitle_region_unref:
 * @region: (transfer full): A #GstSubtitleRegion.
 *
 * Decrements the refcount of @region. If the refcount reaches 0, @region will
 * be freed.
 */
static inline void
gst_subtitle_region_unref (GstSubtitleRegion * region)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (region));
}

G_END_DECLS

#endif /* __GST_SUBTITLE_H__ */
