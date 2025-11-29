/* gtkcellareaboxcontext.c
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
#include "gtkcellareabox.h"
#include "gtkcellareaboxcontextprivate.h"
#include "gtkorientable.h"

#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* GObjectClass */
static void      _gtk_cell_area_box_context_finalize              (GObject               *object);

/* GtkCellAreaContextClass */
static void      _gtk_cell_area_box_context_reset                 (GtkCellAreaContext    *context);
static void      _gtk_cell_area_box_context_get_preferred_height_for_width (GtkCellAreaContext *context,
                                                                           int                 width,
                                                                           int                *minimum_height,
                                                                           int                *natural_height);
static void      _gtk_cell_area_box_context_get_preferred_width_for_height (GtkCellAreaContext *context,
                                                                           int                 height,
                                                                           int                *minimum_width,
                                                                           int                *natural_width);



/* Internal functions */
static void      _gtk_cell_area_box_context_sum                  (GtkCellAreaBoxContext  *context,
                                                                 GtkOrientation          orientation,
                                                                 int                     for_size,
                                                                 int                    *minimum_size,
                                                                 int                    *natural_size);
static void      free_cache_array                                (GArray                *array);
static GArray   *group_array_new                                 (GtkCellAreaBoxContext *context);
static GArray   *get_array                                       (GtkCellAreaBoxContext *context,
                                                                  GtkOrientation         orientation,
                                                                  int                    for_size);
static gboolean  group_expands                                   (GtkCellAreaBoxContext *context,
                                                                  int                    group_idx);
static int       count_expand_groups                             (GtkCellAreaBoxContext *context);


/* CachedSize management */
typedef struct {
  int      min_size;
  int      nat_size;
} CachedSize;

struct _GtkCellAreaBoxContextPrivate
{
  /* Table of per renderer CachedSizes */
  GArray *base_widths;
  GArray *base_heights;

  /* Table of per height/width hash tables of per renderer CachedSizes */
  GHashTable *widths;
  GHashTable *heights;

  /* Whether each group expands */
  gboolean  *expand;

  /* Whether each group is aligned */
  gboolean  *align;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellAreaBoxContext, _gtk_cell_area_box_context, GTK_TYPE_CELL_AREA_CONTEXT)

static void
free_cache_array (GArray *array)
{
  g_array_free (array, TRUE);
}

static GArray *
group_array_new (GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GArray *group_array;

  group_array = g_array_new (FALSE, TRUE, sizeof (CachedSize));
  g_array_set_size (group_array, priv->base_widths->len);

  return group_array;
}

static GArray *
get_array (GtkCellAreaBoxContext *context,
           GtkOrientation         orientation,
           int                    for_size)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GArray                       *array;

  if (for_size < 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        array = priv->base_widths;
      else
        array = priv->base_heights;
    }
  else
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_size));

          if (!array)
            array = priv->base_widths;
        }
      else
        {
          array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_size));

          if (!array)
            array = priv->base_heights;
        }
    }

  return array;
}

static gboolean
group_expands (GtkCellAreaBoxContext *context,
               int                    group_idx)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;

  g_assert (group_idx >= 0 && group_idx < priv->base_widths->len);

  return priv->expand[group_idx];
}

static int
count_expand_groups (GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  int i, expand = 0;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      if (priv->expand[i])
        expand++;
    }

  return expand;
}

static void
_gtk_cell_area_box_context_init (GtkCellAreaBoxContext *box_context)
{
  GtkCellAreaBoxContextPrivate *priv;

  box_context->priv = _gtk_cell_area_box_context_get_instance_private (box_context);
  priv = box_context->priv;

  priv->base_widths  = g_array_new (FALSE, TRUE, sizeof (CachedSize));
  priv->base_heights = g_array_new (FALSE, TRUE, sizeof (CachedSize));

  priv->widths       = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                              NULL, (GDestroyNotify)free_cache_array);
  priv->heights      = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                              NULL, (GDestroyNotify)free_cache_array);
}

static void
_gtk_cell_area_box_context_class_init (GtkCellAreaBoxContextClass *class)
{
  GObjectClass            *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaContextClass *context_class   = GTK_CELL_AREA_CONTEXT_CLASS (class);

  /* GObjectClass */
  object_class->finalize = _gtk_cell_area_box_context_finalize;

  context_class->reset                          = _gtk_cell_area_box_context_reset;
  context_class->get_preferred_height_for_width = _gtk_cell_area_box_context_get_preferred_height_for_width;
  context_class->get_preferred_width_for_height = _gtk_cell_area_box_context_get_preferred_width_for_height;
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
_gtk_cell_area_box_context_finalize (GObject *object)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (object);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;

  g_array_free (priv->base_widths, TRUE);
  g_array_free (priv->base_heights, TRUE);
  g_hash_table_destroy (priv->widths);
  g_hash_table_destroy (priv->heights);

  g_free (priv->expand);
  g_free (priv->align);

  G_OBJECT_CLASS (_gtk_cell_area_box_context_parent_class)->finalize (object);
}

/*************************************************************
 *                    GtkCellAreaContextClass                *
 *************************************************************/
static void
_gtk_cell_area_box_context_reset (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  CachedSize                   *size;
  int                           i;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      size = &g_array_index (priv->base_widths, CachedSize, i);

      size->min_size = 0;
      size->nat_size = 0;

      size = &g_array_index (priv->base_heights, CachedSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  /* Reset context sizes as well */
  g_hash_table_remove_all (priv->widths);
  g_hash_table_remove_all (priv->heights);

  GTK_CELL_AREA_CONTEXT_CLASS
    (_gtk_cell_area_box_context_parent_class)->reset (context);
}

static void
_gtk_cell_area_box_context_sum (GtkCellAreaBoxContext *context,
                               GtkOrientation         orientation,
                               int                    for_size,
                               int                   *minimum_size,
                               int                   *natural_size)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GtkCellAreaBox *area;
  GtkOrientation  box_orientation;
  GArray         *array;
  int             spacing, i, last_aligned_group_idx;
  int             min_size = 0, nat_size = 0;

  area            = (GtkCellAreaBox *)gtk_cell_area_context_get_area (GTK_CELL_AREA_CONTEXT (context));
  spacing         = gtk_cell_area_box_get_spacing (area);
  box_orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
  array           = get_array (context, orientation, for_size);

  /* Get the last visible aligned group
   * (we need to get space at least up till this group) */
  for (i = array->len - 1; i >= 0; i--)
    {
      if (priv->align[i] &&
          _gtk_cell_area_box_group_visible (area, i))
        break;
    }
  last_aligned_group_idx = i >= 0 ? i : 0;

  for (i = 0; i < array->len; i++)
    {
      CachedSize *size = &g_array_index (array, CachedSize, i);

      if (box_orientation == orientation)
        {
          if (i > last_aligned_group_idx &&
              !_gtk_cell_area_box_group_visible (area, i))
            continue;

          /* Dont add spacing for 0 size groups, they can be 0 size because
           * they contain only invisible cells for this round of requests
           */
          if (min_size > 0 && size->nat_size > 0)
            {
              min_size += spacing;
              nat_size += spacing;
            }

          min_size += size->min_size;
          nat_size += size->nat_size;
        }
      else
        {
          min_size = MAX (min_size, size->min_size);
          nat_size = MAX (nat_size, size->nat_size);
        }
    }

  if (for_size < 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_cell_area_context_push_preferred_width (GTK_CELL_AREA_CONTEXT (context), min_size, nat_size);
      else
        gtk_cell_area_context_push_preferred_height (GTK_CELL_AREA_CONTEXT (context), min_size, nat_size);
    }

  if (minimum_size)
    *minimum_size = min_size;
  if (natural_size)
    *natural_size = nat_size;
}

static void
_gtk_cell_area_box_context_get_preferred_height_for_width (GtkCellAreaContext *context,
                                                          int                 width,
                                                          int                *minimum_height,
                                                          int                *natural_height)
{
  _gtk_cell_area_box_context_sum (GTK_CELL_AREA_BOX_CONTEXT (context), GTK_ORIENTATION_VERTICAL,
                                 width, minimum_height, natural_height);
}

static void
_gtk_cell_area_box_context_get_preferred_width_for_height (GtkCellAreaContext *context,
                                                          int                 height,
                                                          int                *minimum_width,
                                                          int                *natural_width)
{
  _gtk_cell_area_box_context_sum (GTK_CELL_AREA_BOX_CONTEXT (context), GTK_ORIENTATION_HORIZONTAL,
                                 height, minimum_width, natural_width);
}

/*************************************************************
 *                            API                            *
 *************************************************************/
static void
copy_size_array (GArray *src_array,
                 GArray *dest_array)
{
  int i;

  for (i = 0; i < src_array->len; i++)
    {
      CachedSize *src  = &g_array_index (src_array, CachedSize, i);
      CachedSize *dest = &g_array_index (dest_array, CachedSize, i);

      memcpy (dest, src, sizeof (CachedSize));
    }
}

static void
for_size_copy (gpointer    key,
               GArray     *size_array,
               GHashTable *dest_hash)
{
  GArray *new_array;

  new_array = g_array_new (FALSE, TRUE, sizeof (CachedSize));
  g_array_set_size (new_array, size_array->len);

  copy_size_array (size_array, new_array);

  g_hash_table_insert (dest_hash, key, new_array);
}

GtkCellAreaBoxContext *
_gtk_cell_area_box_context_copy (GtkCellAreaBox        *box,
                                GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxContext *copy;

  copy = g_object_new (GTK_TYPE_CELL_AREA_BOX_CONTEXT,
                       "area", box, NULL);

  _gtk_cell_area_box_init_groups (copy,
                                  context->priv->base_widths->len,
                                  context->priv->expand,
                                  context->priv->align);

  /* Copy the base arrays */
  copy_size_array (context->priv->base_widths,
                   copy->priv->base_widths);
  copy_size_array (context->priv->base_heights,
                   copy->priv->base_heights);

  /* Copy each for size */
  g_hash_table_foreach (context->priv->heights,
                        (GHFunc)for_size_copy, copy->priv->heights);
  g_hash_table_foreach (context->priv->widths,
                        (GHFunc)for_size_copy, copy->priv->widths);


  return copy;
}

void
_gtk_cell_area_box_init_groups (GtkCellAreaBoxContext *box_context,
                                guint                  n_groups,
                                gboolean              *expand_groups,
                                gboolean              *align_groups)
{
  GtkCellAreaBoxContextPrivate *priv;
  gsize groups_size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));
  g_return_if_fail (n_groups == 0 || expand_groups != NULL);

  /* When the group dimensions change, all info must be reset
   * Note this already clears the min/nat values on the CachedSizes
   */
  gtk_cell_area_context_reset (GTK_CELL_AREA_CONTEXT (box_context));

  priv = box_context->priv;
  g_array_set_size (priv->base_widths,  n_groups);
  g_array_set_size (priv->base_heights, n_groups);

  groups_size = n_groups * sizeof (gboolean);

  g_free (priv->expand);
  priv->expand = g_memdup2 (expand_groups, groups_size);

  g_free (priv->align);
  priv->align = g_memdup2 (align_groups, groups_size);
}

void
_gtk_cell_area_box_context_push_group_width (GtkCellAreaBoxContext *box_context,
                                            int                    group_idx,
                                            int                    minimum_width,
                                            int                    natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;
  gboolean                      grew = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, CachedSize, group_idx);
  if (minimum_width > size->min_size)
    {
      size->min_size = minimum_width;
      grew = TRUE;
    }
  if (natural_width > size->nat_size)
    {
      size->nat_size = natural_width;
      grew = TRUE;
    }

  if (grew)
    _gtk_cell_area_box_context_sum (box_context, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL);
}

void
_gtk_cell_area_box_context_push_group_height_for_width  (GtkCellAreaBoxContext *box_context,
                                                        int                    group_idx,
                                                        int                    for_width,
                                                        int                    minimum_height,
                                                        int                    natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));
  if (!group_array)
    {
      group_array = group_array_new (box_context);
      g_hash_table_insert (priv->heights, GINT_TO_POINTER (for_width), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_height);
  size->nat_size = MAX (size->nat_size, natural_height);
}

void
_gtk_cell_area_box_context_push_group_height (GtkCellAreaBoxContext *box_context,
                                             int                    group_idx,
                                             int                    minimum_height,
                                             int                    natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;
  gboolean                      grew = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, CachedSize, group_idx);
  if (minimum_height > size->min_size)
    {
      size->min_size = minimum_height;
      grew = TRUE;
    }
  if (natural_height > size->nat_size)
    {
      size->nat_size = natural_height;
      grew = TRUE;
    }

  if (grew)
    _gtk_cell_area_box_context_sum (box_context, GTK_ORIENTATION_VERTICAL, -1, NULL, NULL);
}

void
_gtk_cell_area_box_context_push_group_width_for_height (GtkCellAreaBoxContext *box_context,
                                                       int                    group_idx,
                                                       int                    for_height,
                                                       int                    minimum_width,
                                                       int                    natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv        = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));
  if (!group_array)
    {
      group_array = group_array_new (box_context);
      g_hash_table_insert (priv->widths, GINT_TO_POINTER (for_height), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_width);
  size->nat_size = MAX (size->nat_size, natural_width);
}

void
_gtk_cell_area_box_context_get_group_width (GtkCellAreaBoxContext *box_context,
                                           int                    group_idx,
                                           int                   *minimum_width,
                                           int                   *natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, CachedSize, group_idx);

  if (minimum_width)
    *minimum_width = size->min_size;

  if (natural_width)
    *natural_width = size->nat_size;
}

void
_gtk_cell_area_box_context_get_group_height_for_width (GtkCellAreaBoxContext *box_context,
                                                      int                    group_idx,
                                                      int                    for_width,
                                                      int                   *minimum_height,
                                                      int                   *natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                    *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv        = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_height)
        *minimum_height = size->min_size;

      if (natural_height)
        *natural_height = size->nat_size;
    }
  else
    {
      if (minimum_height)
        *minimum_height = -1;

      if (natural_height)
        *natural_height = -1;
    }
}

void
_gtk_cell_area_box_context_get_group_height (GtkCellAreaBoxContext *box_context,
                                            int                    group_idx,
                                            int                   *minimum_height,
                                            int                   *natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  CachedSize                   *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, CachedSize, group_idx);

  if (minimum_height)
    *minimum_height = size->min_size;

  if (natural_height)
    *natural_height = size->nat_size;
}

void
_gtk_cell_area_box_context_get_group_width_for_height (GtkCellAreaBoxContext *box_context,
                                                      int                    group_idx,
                                                      int                    for_height,
                                                      int                   *minimum_width,
                                                      int                   *natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  GArray                       *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_width)
        *minimum_width = size->min_size;

      if (natural_width)
        *natural_width = size->nat_size;
    }
  else
    {
      if (minimum_width)
        *minimum_width = -1;

      if (natural_width)
        *natural_width = -1;
    }
}

static GtkRequestedSize *
_gtk_cell_area_box_context_get_requests (GtkCellAreaBoxContext *box_context,
                                        GtkCellAreaBox        *area,
                                        GtkOrientation         orientation,
                                        int                    for_size,
                                        int                   *n_requests)
{
  GtkCellAreaBoxContextPrivate *priv = box_context->priv;
  GtkRequestedSize             *requests;
  GArray                       *array;
  CachedSize                   *size;
  int                           visible_groups = 0;
  int                           last_aligned_group_idx = 0;
  int                           i, j;

  /* Get the last visible aligned group
   * (we need to get space at least up till this group) */
  for (i = priv->base_widths->len - 1; i >= 0; i--)
    {
      if (priv->align[i] &&
          _gtk_cell_area_box_group_visible (area, i))
        break;
    }
  last_aligned_group_idx = i >= 0 ? i : 0;

  priv  = box_context->priv;
  array = get_array (box_context, orientation, for_size);

  for (i = 0; i < array->len; i++)
    {
      size = &g_array_index (array, CachedSize, i);

      if (size->nat_size > 0 &&
          (i <= last_aligned_group_idx ||
           _gtk_cell_area_box_group_visible (area, i)))
        visible_groups++;
    }

  requests = g_new (GtkRequestedSize, visible_groups);

  for (j = 0, i = 0; i < array->len; i++)
    {
      size = &g_array_index (array, CachedSize, i);

      if (size->nat_size > 0 &&
          (i <= last_aligned_group_idx ||
           _gtk_cell_area_box_group_visible (area, i)))
        {
          requests[j].data         = GINT_TO_POINTER (i);
          requests[j].minimum_size = size->min_size;
          requests[j].natural_size = size->nat_size;
          j++;
        }
    }

  if (n_requests)
    *n_requests = visible_groups;

  return requests;
}

static GtkCellAreaBoxAllocation *
allocate_for_orientation (GtkCellAreaBoxContext *context,
                          GtkCellAreaBox        *area,
                          GtkOrientation         orientation,
                          int                    spacing,
                          int                    size,
                          int                    for_size,
                          int                   *n_allocs)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GtkCellAreaBoxAllocation     *allocs;
  GtkRequestedSize             *sizes;
  int                           n_expand_groups = 0;
  int                           i, n_groups, position, vis_position;
  int                           extra_size, extra_extra;
  int                           avail_size = size;

  sizes           = _gtk_cell_area_box_context_get_requests (context, area, orientation, for_size, &n_groups);
  n_expand_groups = count_expand_groups (context);

  /* First start by naturally allocating space among groups */
  avail_size -= (n_groups - 1) * spacing;
  for (i = 0; i < n_groups; i++)
    avail_size -= sizes[i].minimum_size;

  if (avail_size > 0)
    avail_size = gtk_distribute_natural_allocation (avail_size, n_groups, sizes);
  else
    avail_size = 0;

  /* Calculate/distribute expand for groups */
  if (n_expand_groups > 0)
    {
      extra_size  = avail_size / n_expand_groups;
      extra_extra = avail_size % n_expand_groups;
    }
  else
    extra_size = extra_extra = 0;

  allocs = g_new (GtkCellAreaBoxAllocation, n_groups);

  for (vis_position = 0, position = 0, i = 0; i < n_groups; i++)
    {
      allocs[i].group_idx = GPOINTER_TO_INT (sizes[i].data);

      if (priv->align[allocs[i].group_idx])
        vis_position = position;

      allocs[i].position  = vis_position;
      allocs[i].size      = sizes[i].minimum_size;

      if (group_expands (context, allocs[i].group_idx))
        {
          allocs[i].size += extra_size;
          if (extra_extra)
            {
              allocs[i].size++;
              extra_extra--;
            }
        }

      position += allocs[i].size;
      position += spacing;

      if (_gtk_cell_area_box_group_visible (area, allocs[i].group_idx))
        {
          vis_position += allocs[i].size;
          vis_position += spacing;
        }
    }

  if (n_allocs)
    *n_allocs = n_groups;

  g_free (sizes);

  return allocs;
}

GtkRequestedSize *
_gtk_cell_area_box_context_get_widths (GtkCellAreaBoxContext *box_context,
                                      int                   *n_widths)
{
  GtkCellAreaBox *area = (GtkCellAreaBox *)gtk_cell_area_context_get_area (GTK_CELL_AREA_CONTEXT (box_context));

  return _gtk_cell_area_box_context_get_requests (box_context, area, GTK_ORIENTATION_HORIZONTAL, -1, n_widths);
}

GtkRequestedSize *
_gtk_cell_area_box_context_get_heights (GtkCellAreaBoxContext *box_context,
                                       int                   *n_heights)
{
  GtkCellAreaBox *area = (GtkCellAreaBox *)gtk_cell_area_context_get_area (GTK_CELL_AREA_CONTEXT (box_context));

  return _gtk_cell_area_box_context_get_requests (box_context, area, GTK_ORIENTATION_VERTICAL, -1, n_heights);
}

GtkCellAreaBoxAllocation *
_gtk_cell_area_box_context_get_orientation_allocs (GtkCellAreaBoxContext *context,
                                                  int                   *n_allocs)
{
  GtkCellAreaContext       *ctx  = GTK_CELL_AREA_CONTEXT (context);
  GtkCellAreaBox           *area;
  GtkOrientation            orientation;
  int                       spacing, width, height, alloc_count = 0;
  GtkCellAreaBoxAllocation *allocs = NULL;

  area        = (GtkCellAreaBox *)gtk_cell_area_context_get_area (ctx);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
  spacing     = gtk_cell_area_box_get_spacing (area);

  gtk_cell_area_context_get_allocation (ctx, &width, &height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && width > 0)
    allocs = allocate_for_orientation (context, area, orientation,
                                       spacing, width, height,
                                       &alloc_count);
  else if (orientation == GTK_ORIENTATION_VERTICAL && height > 0)
    allocs = allocate_for_orientation (context, area, orientation,
                                       spacing, height, width,
                                       &alloc_count);

  *n_allocs = alloc_count;

  return allocs;
}
