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

/**
 * SECTION:gstsubtitle
 * @title: GstSubtitle
 * @short_description: Library for describing sets of static subtitles.
 *
 * This library enables the description of static text scenes made up of a
 * number of regions, which may contain a number of block and inline text
 * elements. It is derived from the concepts and features defined in the Timed
 * Text Markup Language 1 (TTML1), Second Edition
 * (http://www.w3.org/TR/ttaf1-dfxp), and the EBU-TT-D profile of TTML1
 * (https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3380.pdf).
 */

#include "subtitle.h"

/**
 * gst_subtitle_style_set_free:
 * @style_set: A #GstSubtitleStyleSet.
 *
 * Free @style_set and its associated memory.
 */
static void
_gst_subtitle_style_set_free (GstSubtitleStyleSet * style_set)
{
  g_return_if_fail (style_set != NULL);
  g_free (style_set->font_family);
  g_free (style_set);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleStyleSet, gst_subtitle_style_set);

/**
 * gst_subtitle_style_set_new:
 *
 * Create a new #GstSubtitleStyleSet with default values for all properties.
 *
 * Returns: (transfer full): A newly-allocated #GstSubtitleStyleSet.
 */
GstSubtitleStyleSet *
gst_subtitle_style_set_new (void)
{
  GstSubtitleStyleSet *ret = g_new0 (GstSubtitleStyleSet, 1);
  GstSubtitleColor white = { 255, 255, 255, 255 };
  GstSubtitleColor transparent = { 0, 0, 0, 0 };

  gst_mini_object_init (GST_MINI_OBJECT_CAST (ret), 0,
      gst_subtitle_style_set_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_style_set_free);

  ret->font_family = g_strdup ("default");
  ret->font_size = 1.0;
  ret->line_height = -1;
  ret->color = white;
  ret->background_color = transparent;
  ret->line_padding = 0.0;
  ret->origin_x = ret->origin_y = 0.0;
  ret->extent_w = ret->extent_h = 0.0;
  ret->padding_start = ret->padding_end
      = ret->padding_before = ret->padding_after = 0.0;
  ret->fill_line_gap = FALSE;

  return ret;
}


static void
_gst_subtitle_element_free (GstSubtitleElement * element)
{
  g_return_if_fail (element != NULL);
  gst_subtitle_style_set_unref (element->style_set);
  g_free (element);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleElement, gst_subtitle_element);

/**
 * gst_subtitle_element_new:
 * @style_set: (transfer full): A #GstSubtitleStyleSet that defines the styling
 * and layout associated with this inline text element.
 * @text_index: The index within a #GstBuffer of the #GstMemory that contains
 * the text of this inline text element.
 *
 * Allocates a new #GstSubtitleElement.
 *
 * Returns: (transfer full): A newly-allocated #GstSubtitleElement. Unref
 * with gst_subtitle_element_unref() when no longer needed.
 */
GstSubtitleElement *
gst_subtitle_element_new (GstSubtitleStyleSet * style_set,
    guint text_index, gboolean suppress_whitespace)
{
  GstSubtitleElement *element;

  g_return_val_if_fail (style_set != NULL, NULL);

  element = g_new0 (GstSubtitleElement, 1);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (element), 0,
      gst_subtitle_element_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_element_free);

  element->style_set = style_set;
  element->text_index = text_index;
  element->suppress_whitespace = suppress_whitespace;

  return element;
}

static void
_gst_subtitle_block_free (GstSubtitleBlock * block)
{
  g_return_if_fail (block != NULL);
  gst_subtitle_style_set_unref (block->style_set);
  g_ptr_array_unref (block->elements);
  g_free (block);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleBlock, gst_subtitle_block);


/**
 * gst_subtitle_block_new:
 * @style_set: (transfer full): A #GstSubtitleStyleSet that defines the styling
 * and layout associated with this block of text elements.
 *
 * Allocates a new #GstSubtitleBlock.
 *
 * Returns: (transfer full): A newly-allocated #GstSubtitleBlock. Unref
 * with gst_subtitle_block_unref() when no longer needed.
 */
GstSubtitleBlock *
gst_subtitle_block_new (GstSubtitleStyleSet * style_set)
{
  GstSubtitleBlock *block;

  g_return_val_if_fail (style_set != NULL, NULL);

  block = g_new0 (GstSubtitleBlock, 1);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (block), 0,
      gst_subtitle_block_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_block_free);

  block->style_set = style_set;
  block->elements = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_subtitle_element_unref);

  return block;
}

/**
 * gst_subtitle_block_add_element:
 * @block: A #GstSubtitleBlock.
 * @element: (transfer full): A #GstSubtitleElement to add.
 *
 * Adds a #GstSubtitleElement to @block.
 */
void
gst_subtitle_block_add_element (GstSubtitleBlock * block,
    GstSubtitleElement * element)
{
  g_return_if_fail (block != NULL);
  g_return_if_fail (element != NULL);

  g_ptr_array_add (block->elements, element);
}

/**
 * gst_subtitle_block_get_element_count:
 * @block: A #GstSubtitleBlock.
 *
 * Returns: The number of #GstSubtitleElements in @block.
 */
guint
gst_subtitle_block_get_element_count (const GstSubtitleBlock * block)
{
  g_return_val_if_fail (block != NULL, 0);

  return block->elements->len;
}

/**
 * gst_subtitle_block_get_element:
 * @block: A #GstSubtitleBlock.
 * @index: Index of the element to get.
 *
 * Gets the #GstSubtitleElement at @index in the array of elements held by
 * @block.
 *
 * Returns: (transfer none): The #GstSubtitleElement at @index in the array of
 * elements held by @block, or %NULL if @index is out-of-bounds. The
 * function does not return a reference; the caller should obtain a reference
 * using gst_subtitle_element_ref(), if needed.
 */
GstSubtitleElement *
gst_subtitle_block_get_element (const GstSubtitleBlock * block, guint index)
{
  g_return_val_if_fail (block != NULL, NULL);

  if (index >= block->elements->len)
    return NULL;
  else
    return g_ptr_array_index (block->elements, index);
}

static void
_gst_subtitle_region_free (GstSubtitleRegion * region)
{
  g_return_if_fail (region != NULL);
  gst_subtitle_style_set_unref (region->style_set);
  g_ptr_array_unref (region->blocks);
  g_free (region);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleRegion, gst_subtitle_region);


/**
 * gst_subtitle_region_new:
 * @style_set: (transfer full): A #GstSubtitleStyleSet that defines the styling
 * and layout associated with this region.
 *
 * Allocates a new #GstSubtitleRegion.
 *
 * Returns: (transfer full): A newly-allocated #GstSubtitleRegion. Unref
 * with gst_subtitle_region_unref() when no longer needed.
 */
GstSubtitleRegion *
gst_subtitle_region_new (GstSubtitleStyleSet * style_set)
{
  GstSubtitleRegion *region;

  g_return_val_if_fail (style_set != NULL, NULL);

  region = g_new0 (GstSubtitleRegion, 1);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (region), 0,
      gst_subtitle_region_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_region_free);

  region->style_set = style_set;
  region->blocks = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_subtitle_block_unref);

  return region;
}

/**
 * gst_subtitle_region_add_block:
 * @region: A #GstSubtitleRegion.
 * @block: (transfer full): A #GstSubtitleBlock which should be added
 * to @region's array of blocks.
 *
 * Adds a #GstSubtitleBlock to the end of the array of blocks held by @region.
 * @region will take ownership of @block, and will unref it when @region
 * is freed.
 */
void
gst_subtitle_region_add_block (GstSubtitleRegion * region,
    GstSubtitleBlock * block)
{
  g_return_if_fail (region != NULL);
  g_return_if_fail (block != NULL);

  g_ptr_array_add (region->blocks, block);
}

/**
 * gst_subtitle_region_get_block_count:
 * @region: A #GstSubtitleRegion.
 *
 * Returns: The number of blocks in @region.
 */
guint
gst_subtitle_region_get_block_count (const GstSubtitleRegion * region)
{
  g_return_val_if_fail (region != NULL, 0);

  return region->blocks->len;
}

/**
 * gst_subtitle_region_get_block:
 * @region: A #GstSubtitleRegion.
 * @index: Index of the block to get.
 *
 * Gets the block at @index in the array of blocks held by @region.
 *
 * Returns: (transfer none): The #GstSubtitleBlock at @index in the array of
 * blocks held by @region, or %NULL if @index is out-of-bounds. The
 * function does not return a reference; the caller should obtain a reference
 * using gst_subtitle_block_ref(), if needed.
 */
const GstSubtitleBlock *
gst_subtitle_region_get_block (const GstSubtitleRegion * region, guint index)
{
  g_return_val_if_fail (region != NULL, NULL);

  if (index >= region->blocks->len)
    return NULL;
  else
    return g_ptr_array_index (region->blocks, index);
}
