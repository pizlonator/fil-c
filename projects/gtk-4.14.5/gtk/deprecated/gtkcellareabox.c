/* gtkcellareabox.c
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


/**
 * GtkCellAreaBox:
 *
 * A cell area that renders GtkCellRenderers into a row or a column
 *
 * The `GtkCellAreaBox` renders cell renderers into a row or a column
 * depending on its `GtkOrientation`.
 *
 * GtkCellAreaBox uses a notion of packing. Packing
 * refers to adding cell renderers with reference to a particular position
 * in a `GtkCellAreaBox`. There are two reference positions: the
 * start and the end of the box.
 * When the `GtkCellAreaBox` is oriented in the %GTK_ORIENTATION_VERTICAL
 * orientation, the start is defined as the top of the box and the end is
 * defined as the bottom. In the %GTK_ORIENTATION_HORIZONTAL orientation
 * start is defined as the left side and the end is defined as the right
 * side.
 *
 * Alignments of `GtkCellRenderer`s rendered in adjacent rows can be
 * configured by configuring the `GtkCellAreaBox` align child cell property
 * with gtk_cell_area_cell_set_property() or by specifying the "align"
 * argument to gtk_cell_area_box_pack_start() and gtk_cell_area_box_pack_end().
 *
 * Deprecated: 4.10: List views use widgets for displaying their
 *   contents
 */

#include "config.h"
#include "gtkorientable.h"
#include "deprecated/gtkcelllayout.h"
#include "gtkcellareabox.h"
#include "gtkcellareaboxcontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* GObjectClass */
static void      gtk_cell_area_box_finalize                       (GObject              *object);
static void      gtk_cell_area_box_dispose                        (GObject              *object);
static void      gtk_cell_area_box_set_property                   (GObject              *object,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void      gtk_cell_area_box_get_property                   (GObject              *object,
                                                                   guint                 prop_id,
                                                                   GValue               *value,
                                                                   GParamSpec           *pspec);

/* GtkCellAreaClass */
static void      gtk_cell_area_box_add                            (GtkCellArea          *area,
                                                                   GtkCellRenderer      *renderer);
static void      gtk_cell_area_box_remove                         (GtkCellArea          *area,
                                                                   GtkCellRenderer      *renderer);
static void      gtk_cell_area_box_foreach                        (GtkCellArea          *area,
                                                                   GtkCellCallback       callback,
                                                                   gpointer              callback_data);
static void      gtk_cell_area_box_foreach_alloc                  (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context,
                                                                   GtkWidget            *widget,
                                                                   const GdkRectangle   *cell_area,
                                                                   const GdkRectangle   *background_area,
                                                                   GtkCellAllocCallback  callback,
                                                                   gpointer              callback_data);
static void      gtk_cell_area_box_apply_attributes               (GtkCellArea          *area,
								   GtkTreeModel         *tree_model,
								   GtkTreeIter          *iter,
								   gboolean              is_expander,
								   gboolean              is_expanded);
static void      gtk_cell_area_box_set_cell_property              (GtkCellArea          *area,
                                                                   GtkCellRenderer      *renderer,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void      gtk_cell_area_box_get_cell_property              (GtkCellArea          *area,
                                                                   GtkCellRenderer      *renderer,
                                                                   guint                 prop_id,
                                                                   GValue               *value,
                                                                   GParamSpec           *pspec);
static GtkCellAreaContext *gtk_cell_area_box_create_context       (GtkCellArea          *area);
static GtkCellAreaContext *gtk_cell_area_box_copy_context         (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context);
static GtkSizeRequestMode  gtk_cell_area_box_get_request_mode     (GtkCellArea          *area);
static void      gtk_cell_area_box_get_preferred_width            (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context,
                                                                   GtkWidget            *widget,
                                                                   int                  *minimum_width,
                                                                   int                  *natural_width);
static void      gtk_cell_area_box_get_preferred_height           (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context,
                                                                   GtkWidget            *widget,
                                                                   int                  *minimum_height,
                                                                   int                  *natural_height);
static void      gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context,
                                                                   GtkWidget            *widget,
                                                                   int                   width,
                                                                   int                  *minimum_height,
                                                                   int                  *natural_height);
static void      gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea          *area,
                                                                   GtkCellAreaContext   *context,
                                                                   GtkWidget            *widget,
                                                                   int                   height,
                                                                   int                  *minimum_width,
                                                                   int                  *natural_width);
static gboolean  gtk_cell_area_box_focus                          (GtkCellArea          *area,
                                                                   GtkDirectionType      direction);

/* GtkCellLayoutIface */
static void      gtk_cell_area_box_cell_layout_init               (GtkCellLayoutIface *iface);
static void      gtk_cell_area_box_layout_pack_start              (GtkCellLayout      *cell_layout,
                                                                   GtkCellRenderer    *renderer,
                                                                   gboolean            expand);
static void      gtk_cell_area_box_layout_pack_end                (GtkCellLayout      *cell_layout,
                                                                   GtkCellRenderer    *renderer,
                                                                   gboolean            expand);
static void      gtk_cell_area_box_layout_reorder                 (GtkCellLayout      *cell_layout,
                                                                   GtkCellRenderer    *renderer,
                                                                   int                 position);
static void      gtk_cell_area_box_focus_changed                  (GtkCellArea        *area,
                                                                   GParamSpec         *pspec,
                                                                   GtkCellAreaBox     *box);


/* CellInfo/CellGroup metadata handling and convenience functions */
typedef struct {
  GtkCellRenderer *renderer;

  guint            expand : 1; /* Whether the cell expands */
  guint            pack   : 1; /* Whether it is packed from the start or end */
  guint            align  : 1; /* Whether to align its position with adjacent rows */
  guint            fixed  : 1; /* Whether to require the same size for all rows */
} CellInfo;

typedef struct {
  GList *cells;

  guint  id           : 8;
  guint  n_cells      : 8;
  guint  expand_cells : 8;
  guint  align        : 1;
  guint  visible      : 1;
} CellGroup;

typedef struct {
  GtkCellRenderer *renderer;

  int              position;
  int              size;
} AllocatedCell;

static CellInfo      *cell_info_new          (GtkCellRenderer       *renderer,
                                              GtkPackType            pack,
                                              gboolean               expand,
                                              gboolean               align,
					      gboolean               fixed);
static void           cell_info_free         (CellInfo              *info);
static int            cell_info_find         (CellInfo              *info,
                                              GtkCellRenderer       *renderer);

static AllocatedCell *allocated_cell_new     (GtkCellRenderer       *renderer,
                                              int                    position,
                                              int                    size);
static void           allocated_cell_free    (AllocatedCell         *cell);
static GList         *list_consecutive_cells (GtkCellAreaBox        *box);
static int            count_expand_groups    (GtkCellAreaBox        *box);
static void           context_weak_notify    (GtkCellAreaBox        *box,
                                              GtkCellAreaBoxContext *dead_context);
static void           reset_contexts         (GtkCellAreaBox        *box);
static void           init_context_groups    (GtkCellAreaBox        *box);
static void           init_context_group     (GtkCellAreaBox        *box,
                                              GtkCellAreaBoxContext *context);
static GSList        *get_allocated_cells    (GtkCellAreaBox        *box,
                                              GtkCellAreaBoxContext *context,
                                              GtkWidget             *widget,
                                              int                    width,
                                              int                    height);

typedef struct _GtkCellAreaBoxClass   GtkCellAreaBoxClass;
typedef struct _GtkCellAreaBoxPrivate GtkCellAreaBoxPrivate;

struct _GtkCellAreaBox
{
  GtkCellArea parent_instance;
};

struct _GtkCellAreaBoxClass
{
  GtkCellAreaClass parent_class;
};

struct _GtkCellAreaBoxPrivate
{
  /* We hold on to the previously focused cell when navigating
   * up and down in a horizontal box (or left and right on a vertical one)
   * this way we always re-enter the last focused cell.
   */
  GtkCellRenderer *last_focus_cell;
  gulong           focus_cell_id;

  GList           *cells;
  GArray          *groups;

  GSList          *contexts;

  GtkOrientation   orientation;
  int              spacing;

  /* We hold on to the rtl state from a widget we are requested for
   * so that we can navigate focus correctly
   */
  gboolean         rtl;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING
};

enum {
  CELL_PROP_0,
  CELL_PROP_EXPAND,
  CELL_PROP_ALIGN,
  CELL_PROP_FIXED_SIZE,
  CELL_PROP_PACK_TYPE
};

G_DEFINE_TYPE_WITH_CODE (GtkCellAreaBox, gtk_cell_area_box, GTK_TYPE_CELL_AREA,
                         G_ADD_PRIVATE (GtkCellAreaBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_cell_area_box_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_cell_area_box_init (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->groups      = g_array_new (FALSE, TRUE, sizeof (CellGroup));
  priv->cells       = NULL;
  priv->contexts    = NULL;
  priv->spacing     = 0;
  priv->rtl         = FALSE;

  /* Watch whenever focus is given to a cell, even if it's not with keynav,
   * this way we remember upon entry of the area where focus was last time
   * around
   */
  priv->focus_cell_id = g_signal_connect (box, "notify::focus-cell",
                                          G_CALLBACK (gtk_cell_area_box_focus_changed), box);
}

static void
gtk_cell_area_box_class_init (GtkCellAreaBoxClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaClass *area_class   = GTK_CELL_AREA_CLASS (class);

  /* GObjectClass */
  object_class->finalize     = gtk_cell_area_box_finalize;
  object_class->dispose      = gtk_cell_area_box_dispose;
  object_class->set_property = gtk_cell_area_box_set_property;
  object_class->get_property = gtk_cell_area_box_get_property;

  /* GtkCellAreaClass */
  area_class->add                 = gtk_cell_area_box_add;
  area_class->remove              = gtk_cell_area_box_remove;
  area_class->foreach             = gtk_cell_area_box_foreach;
  area_class->foreach_alloc       = gtk_cell_area_box_foreach_alloc;
  area_class->apply_attributes    = gtk_cell_area_box_apply_attributes;
  area_class->set_cell_property   = gtk_cell_area_box_set_cell_property;
  area_class->get_cell_property   = gtk_cell_area_box_get_cell_property;

  area_class->create_context                 = gtk_cell_area_box_create_context;
  area_class->copy_context                   = gtk_cell_area_box_copy_context;
  area_class->get_request_mode               = gtk_cell_area_box_get_request_mode;
  area_class->get_preferred_width            = gtk_cell_area_box_get_preferred_width;
  area_class->get_preferred_height           = gtk_cell_area_box_get_preferred_height;
  area_class->get_preferred_height_for_width = gtk_cell_area_box_get_preferred_height_for_width;
  area_class->get_preferred_width_for_height = gtk_cell_area_box_get_preferred_width_for_height;

  area_class->focus = gtk_cell_area_box_focus;

  /* Properties */
  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkCellAreaBox:spacing:
   *
   * The amount of space to reserve between cells.
   */
  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing", NULL, NULL,
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /* Cell Properties */
  /**
   * GtkCellAreaBox:expand:
   *
   * Whether the cell renderer should receive extra space
   * when the area receives more than its natural size.
   */
  gtk_cell_area_class_install_cell_property (area_class,
                                             CELL_PROP_EXPAND,
                                             g_param_spec_boolean
                                             ("expand", NULL, NULL,
                                              FALSE,
                                              GTK_PARAM_READWRITE));

  /**
   * GtkCellAreaBox:align:
   *
   * Whether the cell renderer should be aligned in adjacent rows.
   */
  gtk_cell_area_class_install_cell_property (area_class,
                                             CELL_PROP_ALIGN,
                                             g_param_spec_boolean
                                             ("align", NULL, NULL,
                                              FALSE,
                                              GTK_PARAM_READWRITE));

  /**
   * GtkCellAreaBox:fixed-size:
   *
   * Whether the cell renderer should require the same size
   * for all rows for which it was requested.
   */
  gtk_cell_area_class_install_cell_property (area_class,
                                             CELL_PROP_FIXED_SIZE,
                                             g_param_spec_boolean
                                             ("fixed-size", NULL, NULL,
                                              TRUE,
                                              GTK_PARAM_READWRITE));

  /**
   * GtkCellAreaBox:pack-type:
   *
   * A GtkPackType indicating whether the cell renderer is packed
   * with reference to the start or end of the area.
   */
  gtk_cell_area_class_install_cell_property (area_class,
                                             CELL_PROP_PACK_TYPE,
                                             g_param_spec_enum
                                             ("pack-type", NULL, NULL,
                                              GTK_TYPE_PACK_TYPE, GTK_PACK_START,
                                              GTK_PARAM_READWRITE));
}


/*************************************************************
 *    CellInfo/CellGroup basics and convenience functions    *
 *************************************************************/
static CellInfo *
cell_info_new  (GtkCellRenderer *renderer,
                GtkPackType      pack,
                gboolean         expand,
                gboolean         align,
		gboolean         fixed)
{
  CellInfo *info = g_slice_new (CellInfo);

  info->renderer = g_object_ref_sink (renderer);
  info->pack     = pack;
  info->expand   = expand;
  info->align    = align;
  info->fixed    = fixed;

  return info;
}

static void
cell_info_free (CellInfo *info)
{
  g_object_unref (info->renderer);

  g_slice_free (CellInfo, info);
}

static int
cell_info_find (CellInfo        *info,
                GtkCellRenderer *renderer)
{
  return (info->renderer == renderer) ? 0 : -1;
}

static AllocatedCell *
allocated_cell_new (GtkCellRenderer *renderer,
                    int              position,
                    int              size)
{
  AllocatedCell *cell = g_slice_new (AllocatedCell);

  cell->renderer = renderer;
  cell->position = position;
  cell->size     = size;

  return cell;
}

static void
allocated_cell_free (AllocatedCell *cell)
{
  g_slice_free (AllocatedCell, cell);
}

static GList *
list_consecutive_cells (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *l, *consecutive_cells = NULL, *pack_end_cells = NULL;
  CellInfo              *info;

  /* List cells in consecutive order taking their
   * PACK_START/PACK_END options into account
   */
  for (l = priv->cells; l; l = l->next)
    {
      info = l->data;

      if (info->pack == GTK_PACK_START)
        consecutive_cells = g_list_prepend (consecutive_cells, info);
    }

  for (l = priv->cells; l; l = l->next)
    {
      info = l->data;

      if (info->pack == GTK_PACK_END)
        pack_end_cells = g_list_prepend (pack_end_cells, info);
    }

  consecutive_cells = g_list_reverse (consecutive_cells);
  consecutive_cells = g_list_concat (consecutive_cells, pack_end_cells);

  return consecutive_cells;
}

static void
cell_groups_clear (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  int                    i;

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      g_list_free (group->cells);
    }

  g_array_set_size (priv->groups, 0);
}

static void
cell_groups_rebuild (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  CellGroup              group = { 0, };
  CellGroup             *group_ptr;
  GList                 *cells, *l;
  guint                  id = 0;
  gboolean               last_cell_fixed = FALSE;

  cell_groups_clear (box);

  if (!priv->cells)
    return;

  cells = list_consecutive_cells (box);

  /* First group is implied */
  g_array_append_val (priv->groups, group);
  group_ptr = &g_array_index (priv->groups, CellGroup, id);

  for (l = cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      /* A new group starts with any aligned cell, or
       * at the beginning and end of a fixed size cell.
       * the first group is implied */
      if ((info->align || info->fixed || last_cell_fixed) && l != cells)
        {
          memset (&group, 0x0, sizeof (CellGroup));
          group.id = ++id;

          g_array_append_val (priv->groups, group);
          group_ptr = &g_array_index (priv->groups, CellGroup, id);
        }

      group_ptr->cells = g_list_prepend (group_ptr->cells, info);
      group_ptr->n_cells++;

      /* Not every group is aligned, some are floating
       * fixed size cells */
      if (info->align)
	group_ptr->align = TRUE;

      /* A group expands if it contains any expand cells */
      if (info->expand)
        group_ptr->expand_cells++;

      last_cell_fixed = info->fixed;
    }

  g_list_free (cells);

  for (id = 0; id < priv->groups->len; id++)
    {
      group_ptr = &g_array_index (priv->groups, CellGroup, id);

      group_ptr->cells = g_list_reverse (group_ptr->cells);
    }

  /* Contexts need to be updated with the new grouping information */
  init_context_groups (box);
}

static int
count_visible_cells (CellGroup *group,
                     int       *expand_cells)
{
  GList *l;
  int    visible_cells = 0;
  int    n_expand_cells = 0;

  for (l = group->cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (gtk_cell_renderer_get_visible (info->renderer))
        {
          visible_cells++;

          if (info->expand)
            n_expand_cells++;
        }
    }

  if (expand_cells)
    *expand_cells = n_expand_cells;

  return visible_cells;
}

static int
count_expand_groups (GtkCellAreaBox  *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  int                    i;
  int                    expand_groups = 0;

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      if (group->expand_cells > 0)
        expand_groups++;
    }

  return expand_groups;
}

static void
context_weak_notify (GtkCellAreaBox        *box,
                     GtkCellAreaBoxContext *dead_context)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  priv->contexts = g_slist_remove (priv->contexts, dead_context);
}

static void
init_context_group (GtkCellAreaBox        *box,
                    GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  int                   *expand_groups, *align_groups, i;

  expand_groups = g_new (gboolean, priv->groups->len);
  align_groups  = g_new (gboolean, priv->groups->len);

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      expand_groups[i] = (group->expand_cells > 0);
      align_groups[i]  = group->align;
    }

  /* This call implies resetting the request info */
  _gtk_cell_area_box_init_groups (context, priv->groups->len, expand_groups, align_groups);
  g_free (expand_groups);
  g_free (align_groups);
}

static void
init_context_groups (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GSList                *l;

  /* When the box's groups are reconstructed,
   * contexts need to be reinitialized.
   */
  for (l = priv->contexts; l; l = l->next)
    {
      GtkCellAreaBoxContext *context = l->data;

      init_context_group (box, context);
    }
}

static void
reset_contexts (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GSList                *l;

  /* When the box layout changes, contexts need to
   * be reset and sizes for the box get requested again
   */
  for (l = priv->contexts; l; l = l->next)
    {
      GtkCellAreaContext *context = l->data;

      gtk_cell_area_context_reset (context);
    }
}

/* Fall back on a completely unaligned dynamic allocation of cells
 * when not allocated for the said orientation, alignment of cells
 * is not done when each area gets a different size in the orientation
 * of the box.
 */
static GSList *
allocate_cells_manually (GtkCellAreaBox        *box,
                         GtkWidget             *widget,
                         int                    width,
                         int                    height)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                    *cells, *l;
  GSList                   *allocated_cells = NULL;
  GtkRequestedSize         *sizes;
  int                       i;
  int                       nvisible = 0, nexpand = 0, group_expand;
  int                       avail_size, extra_size, extra_extra, full_size;
  int                       position = 0, for_size;
  gboolean                  rtl;

  if (!priv->cells)
    return NULL;

  /* For vertical oriented boxes, we just let the cell renderers
   * realign themselves for rtl
   */
  rtl = (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
         gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  cells = list_consecutive_cells (box);

  /* Count the visible and expand cells */
  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      nvisible += count_visible_cells (group, &group_expand);
      nexpand  += group_expand;
    }

  if (nvisible <= 0)
    {
      g_list_free (cells);
      return NULL;
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      full_size = avail_size = width;
      for_size  = height;
    }
  else
    {
      full_size = avail_size = height;
      for_size  = width;
    }

  /* Go ahead and collect the requests on the fly */
  sizes = g_new0 (GtkRequestedSize, nvisible);
  for (l = cells, i = 0; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (!gtk_cell_renderer_get_visible (info->renderer))
        continue;

      gtk_cell_area_request_renderer (GTK_CELL_AREA (box), info->renderer,
                                      priv->orientation,
                                      widget, for_size,
                                      &sizes[i].minimum_size,
                                      &sizes[i].natural_size);

      avail_size -= sizes[i].minimum_size;

      sizes[i].data = info;

      i++;
    }

  /* Naturally distribute the allocation */
  avail_size -= (nvisible - 1) * priv->spacing;
  if (avail_size > 0)
    avail_size = gtk_distribute_natural_allocation (avail_size, nvisible, sizes);
  else
    avail_size = 0;

  /* Calculate/distribute expand for cells */
  if (nexpand > 0)
    {
      extra_size  = avail_size / nexpand;
      extra_extra = avail_size % nexpand;
    }
  else
    extra_size = extra_extra = 0;

  /* Create the allocated cells */
  for (i = 0; i < nvisible; i++)
    {
      CellInfo      *info = sizes[i].data;
      AllocatedCell *cell;

      if (info->expand)
        {
          sizes[i].minimum_size += extra_size;
          if (extra_extra)
            {
              sizes[i].minimum_size++;
              extra_extra--;
            }
        }

      if (rtl)
        cell = allocated_cell_new (info->renderer,
                                   full_size - (position + sizes[i].minimum_size),
                                   sizes[i].minimum_size);
      else
        cell = allocated_cell_new (info->renderer, position, sizes[i].minimum_size);

      allocated_cells = g_slist_prepend (allocated_cells, cell);

      position += sizes[i].minimum_size;
      position += priv->spacing;
    }

  g_free (sizes);
  g_list_free (cells);

  /* Note it might not be important to reverse the list here at all,
   * we have the correct positions, no need to allocate from left to right
   */
  return g_slist_reverse (allocated_cells);
}

/* Returns an allocation for each cell in the orientation of the box,
 * used in ->render()/->event() implementations to get a straight-forward
 * list of allocated cells to operate on.
 */
static GSList *
get_allocated_cells (GtkCellAreaBox        *box,
                     GtkCellAreaBoxContext *context,
                     GtkWidget             *widget,
                     int                    width,
                     int                    height)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaBoxAllocation *group_allocs;
  GtkCellArea              *area = GTK_CELL_AREA (box);
  GList                    *cell_list;
  GSList                   *allocated_cells = NULL;
  int                       i, j, n_allocs, position;
  int                       for_size, full_size;
  gboolean                  rtl;

  group_allocs = _gtk_cell_area_box_context_get_orientation_allocs (context, &n_allocs);
  if (!group_allocs)
    return allocate_cells_manually (box, widget, width, height);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      full_size = width;
      for_size  = height;
    }
  else
    {
      full_size = height;
      for_size  = width;
    }

  /* For vertical oriented boxes, we just let the cell renderers
   * realign themselves for rtl
   */
  rtl = (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
         gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  for (position = 0, i = 0; i < n_allocs; i++)
    {
      /* We dont always allocate all groups, sometimes the requested
       * group has only invisible cells for every row, hence the usage
       * of group_allocs[i].group_idx here
       */
      CellGroup *group = &g_array_index (priv->groups, CellGroup, group_allocs[i].group_idx);

      /* Exception for single cell groups */
      if (group->n_cells == 1)
        {
          CellInfo      *info = group->cells->data;
          AllocatedCell *cell;
	  int            cell_position, cell_size;

	  if (!gtk_cell_renderer_get_visible (info->renderer))
	    continue;

	  /* If were not aligned, place the cell after the last cell */
	  if (info->align)
	    position = cell_position = group_allocs[i].position;
	  else
	    cell_position = position;

	  /* If not a fixed size, use only the requested size for this row */
	  if (info->fixed)
	    cell_size = group_allocs[i].size;
	  else
	    {
	      int dummy;
              gtk_cell_area_request_renderer (area, info->renderer,
                                              priv->orientation,
                                              widget, for_size,
                                              &dummy,
                                              &cell_size);
	      cell_size = MIN (cell_size, group_allocs[i].size);
	    }

          if (rtl)
            cell = allocated_cell_new (info->renderer,
                                       full_size - (cell_position + cell_size), cell_size);
          else
            cell = allocated_cell_new (info->renderer, cell_position, cell_size);

	  position += cell_size;
          position += priv->spacing;

          allocated_cells = g_slist_prepend (allocated_cells, cell);
        }
      else
        {
          GtkRequestedSize *sizes;
          int               avail_size, cell_position;
          int               visible_cells, expand_cells;
          int               extra_size, extra_extra;

          visible_cells = count_visible_cells (group, &expand_cells);

          /* If this row has no visible cells in this group, just
           * skip the allocation
           */
          if (visible_cells == 0)
            continue;

	  /* If were not aligned, place the cell after the last cell
	   * and eat up the extra space
	   */
	  if (group->align)
	    {
	      avail_size = group_allocs[i].size;
	      position   = cell_position = group_allocs[i].position;
	    }
	  else
	    {
	      avail_size    = group_allocs[i].size + (group_allocs[i].position - position);
	      cell_position = position;
	    }

          sizes = g_new (GtkRequestedSize, visible_cells);

          for (j = 0, cell_list = group->cells; cell_list; cell_list = cell_list->next)
            {
              CellInfo *info = cell_list->data;

              if (!gtk_cell_renderer_get_visible (info->renderer))
                continue;

              gtk_cell_area_request_renderer (area, info->renderer,
                                              priv->orientation,
                                              widget, for_size,
                                              &sizes[j].minimum_size,
                                              &sizes[j].natural_size);

              sizes[j].data = info;
              avail_size   -= sizes[j].minimum_size;

              j++;
            }

          /* Distribute cells naturally within the group */
          avail_size -= (visible_cells - 1) * priv->spacing;
          if (avail_size > 0)
            avail_size = gtk_distribute_natural_allocation (avail_size, visible_cells, sizes);
          else
            avail_size = 0;

          /* Calculate/distribute expand for cells */
          if (expand_cells > 0)
            {
              extra_size  = avail_size / expand_cells;
              extra_extra = avail_size % expand_cells;
            }
          else
            extra_size = extra_extra = 0;

          /* Create the allocated cells (loop only over visible cells here) */
          for (j = 0; j < visible_cells; j++)
            {
              CellInfo      *info = sizes[j].data;
              AllocatedCell *cell;

              if (info->expand)
                {
                  sizes[j].minimum_size += extra_size;
                  if (extra_extra)
                    {
                      sizes[j].minimum_size++;
                      extra_extra--;
                    }
                }

              if (rtl)
                cell = allocated_cell_new (info->renderer,
                                           full_size - (cell_position + sizes[j].minimum_size),
                                           sizes[j].minimum_size);
              else
                cell = allocated_cell_new (info->renderer, cell_position, sizes[j].minimum_size);

              allocated_cells = g_slist_prepend (allocated_cells, cell);

              cell_position += sizes[j].minimum_size;
              cell_position += priv->spacing;
            }

          g_free (sizes);

	  position = cell_position;
        }
    }

  g_free (group_allocs);

  /* Note it might not be important to reverse the list here at all,
   * we have the correct positions, no need to allocate from left to right
   */
  return g_slist_reverse (allocated_cells);
}


static void
gtk_cell_area_box_focus_changed (GtkCellArea        *area,
                                 GParamSpec         *pspec,
                                 GtkCellAreaBox     *box)
{
  GtkCellAreaBoxPrivate *priv       = gtk_cell_area_box_get_instance_private (box);
  GtkCellRenderer       *focus_cell = gtk_cell_area_get_focus_cell (area);

  if  (focus_cell)
    priv->last_focus_cell = focus_cell;
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_finalize (GObject *object)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (object);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GSList                *l;

  /* Unref/free the context list */
  for (l = priv->contexts; l; l = l->next)
    g_object_weak_unref (G_OBJECT (l->data), (GWeakNotify)context_weak_notify, box);

  g_slist_free (priv->contexts);
  priv->contexts = NULL;

  /* Free the cell grouping info */
  cell_groups_clear (box);
  g_array_free (priv->groups, TRUE);

  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->finalize (object);
}

static void
gtk_cell_area_box_dispose (GObject *object)
{
  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->dispose (object);
}

static void
gtk_cell_area_box_set_property (GObject       *object,
                                guint          prop_id,
                                const GValue  *value,
                                GParamSpec    *pspec)
{
  GtkCellAreaBox *box = GTK_CELL_AREA_BOX (object);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);
          /* Notify that size needs to be requested again */
          reset_contexts (box);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SPACING:
      gtk_cell_area_box_set_spacing (box, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_box_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkCellAreaBox *box = GTK_CELL_AREA_BOX (object);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_SPACING:
      g_value_set_int (value, gtk_cell_area_box_get_spacing (box));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static void
gtk_cell_area_box_add (GtkCellArea        *area,
                       GtkCellRenderer    *renderer)
{
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area),
                                renderer, FALSE, FALSE, TRUE);
}

static void
gtk_cell_area_box_remove (GtkCellArea        *area,
                          GtkCellRenderer    *renderer)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *node;

  if (priv->last_focus_cell == renderer)
    priv->last_focus_cell = NULL;

  node = g_list_find_custom (priv->cells, renderer,
                             (GCompareFunc)cell_info_find);

  if (node)
    {
      CellInfo *info = node->data;

      cell_info_free (info);

      priv->cells = g_list_delete_link (priv->cells, node);

      /* Reconstruct cell groups */
      cell_groups_rebuild (box);
    }
  else
    g_warning ("Trying to remove a cell renderer that is not present GtkCellAreaBox");
}

static void
gtk_cell_area_box_foreach (GtkCellArea        *area,
                           GtkCellCallback     callback,
                           gpointer            callback_data)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *list;

  for (list = priv->cells; list; list = list->next)
    {
      CellInfo *info = list->data;

      if (callback (info->renderer, callback_data))
        break;
    }
}

static void
gtk_cell_area_box_foreach_alloc (GtkCellArea          *area,
                                 GtkCellAreaContext   *context,
                                 GtkWidget            *widget,
                                 const GdkRectangle   *cell_area,
                                 const GdkRectangle   *background_area,
                                 GtkCellAllocCallback  callback,
                                 gpointer              callback_data)
{
  GtkCellAreaBox        *box      = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GSList                *allocated_cells, *l;
  GdkRectangle           cell_alloc, cell_background;
  gboolean               rtl;

  rtl = (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
         gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  cell_alloc = *cell_area;

  /* Get a list of cells with allocation sizes decided regardless
   * of alignments and pack order etc.
   */
  allocated_cells = get_allocated_cells (box, box_context, widget,
                                         cell_area->width, cell_area->height);

  for (l = allocated_cells; l; l = l->next)
    {
      AllocatedCell *cell = l->data;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          cell_alloc.x     = cell_area->x + cell->position;
          cell_alloc.width = cell->size;
        }
      else
        {
          cell_alloc.y      = cell_area->y + cell->position;
          cell_alloc.height = cell->size;
        }

      /* Stop iterating over cells if they flow out of the render
       * area, this can happen because the render area can actually
       * be smaller than the requested area (treeview columns can
       * be user resizable and can be resized to be smaller than
       * the actual requested area).
       */
      if (cell_alloc.x > cell_area->x + cell_area->width ||
          cell_alloc.x + cell_alloc.width < cell_area->x ||
          cell_alloc.y > cell_area->y + cell_area->height)
        break;

      /* Special case for the last cell (or first cell in rtl)...
       * let the last cell consume the remaining space in the area
       * (the last cell is allowed to consume the remaining space if
       * the space given for rendering is actually larger than allocation,
       * this can happen in the expander GtkTreeViewColumn where only the
       * deepest depth column receives the allocation... shallow columns
       * receive more width). */
      if (!l->next)
        {
          if (rtl)
            {
              /* Fill the leading space for the first cell in the area
               * (still last in the list)
               */
              cell_alloc.width = (cell_alloc.x - cell_area->x) + cell_alloc.width;
              cell_alloc.x     = cell_area->x;
            }
          else
            {
              cell_alloc.width  = cell_area->x + cell_area->width  - cell_alloc.x;
              cell_alloc.height = cell_area->y + cell_area->height - cell_alloc.y;
            }
        }
      else
        {
          /* If the cell we are rendering doesn't fit into the remaining space,
           * clip it so that the underlying renderer has a chance to deal with
           * it (for instance text renderers get a chance to ellipsize).
           */
          if (cell_alloc.x + cell_alloc.width > cell_area->x + cell_area->width)
            cell_alloc.width = cell_area->x + cell_area->width - cell_alloc.x;

          if (cell_alloc.y + cell_alloc.height > cell_area->y + cell_area->height)
            cell_alloc.height = cell_area->y + cell_area->height - cell_alloc.y;
        }

      /* Add portions of the background_area to the cell_alloc
       * to create the cell_background
       */
      cell_background = cell_alloc;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (l == allocated_cells)
            {
              /* Add the depth to the first cell */
              if (rtl)
                {
                  cell_background.width += background_area->width - cell_area->width;
                  cell_background.x      = background_area->x + background_area->width - cell_background.width;
                }
              else
                {
                  cell_background.width += cell_area->x - background_area->x;
                  cell_background.x      = background_area->x;
                }
            }

          if (l->next == NULL)
            {
              /* Grant this cell the remaining space */
              int remain = cell_background.x - background_area->x;

              if (rtl)
                cell_background.x -= remain;
              else
                cell_background.width = background_area->width - remain;
            }

          cell_background.y      = background_area->y;
          cell_background.height = background_area->height;
        }
      else
        {
          if (l == allocated_cells)
            {
              cell_background.height += cell_background.y - background_area->y;
              cell_background.y       = background_area->y;
            }

          if (l->next == NULL)
              cell_background.height =
                background_area->height - (cell_background.y - background_area->y);

          cell_background.x     = background_area->x;
          cell_background.width = background_area->width;
        }

      if (callback (cell->renderer, &cell_alloc, &cell_background, callback_data))
        break;
    }

  g_slist_free_full (allocated_cells, (GDestroyNotify)allocated_cell_free);
}

static void
gtk_cell_area_box_apply_attributes (GtkCellArea  *area,
				    GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    gboolean      is_expander,
				    gboolean      is_expanded)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  int                    i;

  /* Call the parent class to apply the attributes */
  GTK_CELL_AREA_CLASS
    (gtk_cell_area_box_parent_class)->apply_attributes (area, tree_model, iter,
							is_expander, is_expanded);

  /* Update visible state for cell groups */
  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);
      GList     *list;

      group->visible = FALSE;

      for (list = group->cells; list && group->visible == FALSE; list = list->next)
	{
          CellInfo *info = list->data;

          if (gtk_cell_renderer_get_visible (info->renderer))
	    group->visible = TRUE;
	}
    }
}

static void
gtk_cell_area_box_set_cell_property (GtkCellArea        *area,
                                     GtkCellRenderer    *renderer,
                                     guint               prop_id,
                                     const GValue       *value,
                                     GParamSpec         *pspec)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *node;
  CellInfo              *info;
  gboolean               rebuild = FALSE;
  gboolean               val;
  GtkPackType            pack_type;

  node = g_list_find_custom (priv->cells, renderer,
                             (GCompareFunc)cell_info_find);
  if (!node)
    return;

  info = node->data;

  switch (prop_id)
    {
    case CELL_PROP_EXPAND:
      val = g_value_get_boolean (value);

      if (info->expand != val)
        {
          info->expand = val;
          rebuild      = TRUE;
        }
      break;

    case CELL_PROP_ALIGN:
      val = g_value_get_boolean (value);

      if (info->align != val)
        {
          info->align = val;
          rebuild     = TRUE;
        }
      break;

    case CELL_PROP_FIXED_SIZE:
      val = g_value_get_boolean (value);

      if (info->fixed != val)
        {
          info->fixed = val;
          rebuild     = TRUE;
        }
      break;

    case CELL_PROP_PACK_TYPE:
      pack_type = g_value_get_enum (value);

      if (info->pack != pack_type)
        {
          info->pack = pack_type;
          rebuild    = TRUE;
        }
      break;
    default:
      GTK_CELL_AREA_WARN_INVALID_CELL_PROPERTY_ID (area, prop_id, pspec);
      break;
    }

  /* Groups need to be rebuilt */
  if (rebuild)
    cell_groups_rebuild (box);
}

static void
gtk_cell_area_box_get_cell_property (GtkCellArea        *area,
                                     GtkCellRenderer    *renderer,
                                     guint               prop_id,
                                     GValue             *value,
                                     GParamSpec         *pspec)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *node;
  CellInfo              *info;

  node = g_list_find_custom (priv->cells, renderer,
                             (GCompareFunc)cell_info_find);
  if (!node)
    return;

  info = node->data;

  switch (prop_id)
    {
    case CELL_PROP_EXPAND:
      g_value_set_boolean (value, info->expand);
      break;

    case CELL_PROP_ALIGN:
      g_value_set_boolean (value, info->align);
      break;

    case CELL_PROP_FIXED_SIZE:
      g_value_set_boolean (value, info->fixed);
      break;

    case CELL_PROP_PACK_TYPE:
      g_value_set_enum (value, info->pack);
      break;
    default:
      GTK_CELL_AREA_WARN_INVALID_CELL_PROPERTY_ID (area, prop_id, pspec);
      break;
    }
}


static GtkCellAreaContext *
gtk_cell_area_box_create_context (GtkCellArea *area)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaContext    *context =
    (GtkCellAreaContext *)g_object_new (GTK_TYPE_CELL_AREA_BOX_CONTEXT,
                                     "area", area, NULL);

  priv->contexts = g_slist_prepend (priv->contexts, context);

  g_object_weak_ref (G_OBJECT (context), (GWeakNotify)context_weak_notify, box);

  /* Tell the new group about our cell layout */
  init_context_group (box, GTK_CELL_AREA_BOX_CONTEXT (context));

  return context;
}

static GtkCellAreaContext *
gtk_cell_area_box_copy_context (GtkCellArea        *area,
                                GtkCellAreaContext *context)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaContext    *copy =
    (GtkCellAreaContext *)_gtk_cell_area_box_context_copy (GTK_CELL_AREA_BOX (area),
                                                          GTK_CELL_AREA_BOX_CONTEXT (context));

  priv->contexts = g_slist_prepend (priv->contexts, copy);

  g_object_weak_ref (G_OBJECT (copy), (GWeakNotify)context_weak_notify, box);

  return copy;
}

static GtkSizeRequestMode
gtk_cell_area_box_get_request_mode (GtkCellArea *area)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  return (priv->orientation) == GTK_ORIENTATION_HORIZONTAL ?
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH :
    GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
compute_size (GtkCellAreaBox        *box,
              GtkOrientation         orientation,
              GtkCellAreaBoxContext *context,
              GtkWidget             *widget,
              int                    for_size,
              int                   *minimum_size,
              int                   *natural_size)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellArea           *area = GTK_CELL_AREA (box);
  GList                 *list;
  int                    i;
  int                    min_size = 0;
  int                    nat_size = 0;

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);
      int        group_min_size = 0;
      int        group_nat_size = 0;

      for (list = group->cells; list; list = list->next)
        {
          CellInfo *info = list->data;
          int       renderer_min_size, renderer_nat_size;

          if (!gtk_cell_renderer_get_visible (info->renderer))
              continue;

          gtk_cell_area_request_renderer (area, info->renderer, orientation, widget, for_size,
                                          &renderer_min_size, &renderer_nat_size);

          if (orientation == priv->orientation)
            {
              if (min_size > 0)
                {
                  min_size += priv->spacing;
                  nat_size += priv->spacing;
                }

              if (group_min_size > 0)
                {
                  group_min_size += priv->spacing;
                  group_nat_size += priv->spacing;
                }

              min_size       += renderer_min_size;
              nat_size       += renderer_nat_size;
              group_min_size += renderer_min_size;
              group_nat_size += renderer_nat_size;
            }
          else
            {
              min_size       = MAX (min_size, renderer_min_size);
              nat_size       = MAX (nat_size, renderer_nat_size);
              group_min_size = MAX (group_min_size, renderer_min_size);
              group_nat_size = MAX (group_nat_size, renderer_nat_size);
            }
        }

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (for_size < 0)
            _gtk_cell_area_box_context_push_group_width (context, group->id, group_min_size, group_nat_size);
          else
            _gtk_cell_area_box_context_push_group_width_for_height (context, group->id, for_size,
                                                                   group_min_size, group_nat_size);
        }
      else
        {
          if (for_size < 0)
            _gtk_cell_area_box_context_push_group_height (context, group->id, group_min_size, group_nat_size);
          else
            _gtk_cell_area_box_context_push_group_height_for_width (context, group->id, for_size,
                                                                   group_min_size, group_nat_size);
        }
    }

  *minimum_size = min_size;
  *natural_size = nat_size;

  /* Update rtl state for focus navigation to work */
  priv->rtl = (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
               gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
}

static GtkRequestedSize *
get_group_sizes (GtkCellArea    *area,
                 CellGroup      *group,
                 GtkOrientation  orientation,
                 GtkWidget      *widget,
                 int            *n_sizes)
{
  GtkRequestedSize *sizes;
  GList            *l;
  int               i;

  *n_sizes = count_visible_cells (group, NULL);
  sizes    = g_new (GtkRequestedSize, *n_sizes);

  for (l = group->cells, i = 0; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (!gtk_cell_renderer_get_visible (info->renderer))
        continue;

      sizes[i].data = info;

      gtk_cell_area_request_renderer (area, info->renderer,
                                      orientation, widget, -1,
                                      &sizes[i].minimum_size,
                                      &sizes[i].natural_size);

      i++;
    }

  return sizes;
}

static void
compute_group_size_for_opposing_orientation (GtkCellAreaBox     *box,
                                             CellGroup          *group,
                                             GtkWidget          *widget,
                                             int                 for_size,
                                             int                *minimum_size,
                                             int                *natural_size)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellArea           *area = GTK_CELL_AREA (box);

  /* Exception for single cell groups */
  if (group->n_cells == 1)
    {
      CellInfo *info = group->cells->data;

      gtk_cell_area_request_renderer (area, info->renderer,
                                      OPPOSITE_ORIENTATION (priv->orientation),
                                      widget, for_size, minimum_size, natural_size);
    }
  else
    {
      GtkRequestedSize *orientation_sizes;
      CellInfo         *info;
      int               n_sizes, i;
      int               avail_size     = for_size;
      int               extra_size, extra_extra;
      int               min_size = 0, nat_size = 0;

      orientation_sizes = get_group_sizes (area, group, priv->orientation, widget, &n_sizes);

      /* First naturally allocate the cells in the group into the for_size */
      avail_size -= (n_sizes - 1) * priv->spacing;
      for (i = 0; i < n_sizes; i++)
        avail_size -= orientation_sizes[i].minimum_size;

      if (avail_size > 0)
        avail_size = gtk_distribute_natural_allocation (avail_size, n_sizes, orientation_sizes);
      else
        avail_size = 0;

      /* Calculate/distribute expand for cells */
      if (group->expand_cells > 0)
        {
          extra_size  = avail_size / group->expand_cells;
          extra_extra = avail_size % group->expand_cells;
        }
      else
        extra_size = extra_extra = 0;

      for (i = 0; i < n_sizes; i++)
        {
          int cell_min, cell_nat;

          info = orientation_sizes[i].data;

          if (info->expand)
            {
              orientation_sizes[i].minimum_size += extra_size;
              if (extra_extra)
                {
                  orientation_sizes[i].minimum_size++;
                  extra_extra--;
                }
            }

          gtk_cell_area_request_renderer (area, info->renderer,
                                          OPPOSITE_ORIENTATION (priv->orientation),
                                          widget,
                                          orientation_sizes[i].minimum_size,
                                          &cell_min, &cell_nat);

          min_size = MAX (min_size, cell_min);
          nat_size = MAX (nat_size, cell_nat);
        }

      *minimum_size = min_size;
      *natural_size = nat_size;

      g_free (orientation_sizes);
    }
}

static void
compute_size_for_opposing_orientation (GtkCellAreaBox        *box,
                                       GtkCellAreaBoxContext *context,
                                       GtkWidget             *widget,
                                       int                    for_size,
                                       int                   *minimum_size,
                                       int                   *natural_size)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  CellGroup             *group;
  GtkRequestedSize      *orientation_sizes;
  int                    n_groups, n_expand_groups, i;
  int                    avail_size = for_size;
  int                    extra_size, extra_extra;
  int                    min_size = 0, nat_size = 0;

  n_expand_groups = count_expand_groups (box);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    orientation_sizes = _gtk_cell_area_box_context_get_widths (context, &n_groups);
  else
    orientation_sizes = _gtk_cell_area_box_context_get_heights (context, &n_groups);

  /* First start by naturally allocating space among groups of cells */
  avail_size -= (n_groups - 1) * priv->spacing;
  for (i = 0; i < n_groups; i++)
    avail_size -= orientation_sizes[i].minimum_size;

  if (avail_size > 0)
    avail_size = gtk_distribute_natural_allocation (avail_size, n_groups, orientation_sizes);
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

  /* Now we need to naturally allocate sizes for cells in each group
   * and push the height-for-width for each group accordingly while
   * accumulating the overall height-for-width for this row.
   */
  for (i = 0; i < n_groups; i++)
    {
      int group_min, group_nat;
      int group_idx = GPOINTER_TO_INT (orientation_sizes[i].data);

      group = &g_array_index (priv->groups, CellGroup, group_idx);

      if (group->expand_cells > 0)
        {
          orientation_sizes[i].minimum_size += extra_size;
          if (extra_extra)
            {
              orientation_sizes[i].minimum_size++;
              extra_extra--;
            }
        }

      /* Now we have the allocation for the group,
       * request its height-for-width
       */
      compute_group_size_for_opposing_orientation (box, group, widget,
                                                   orientation_sizes[i].minimum_size,
                                                   &group_min, &group_nat);

      min_size = MAX (min_size, group_min);
      nat_size = MAX (nat_size, group_nat);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          _gtk_cell_area_box_context_push_group_height_for_width (context, group_idx, for_size,
                                                                 group_min, group_nat);
        }
      else
        {
          _gtk_cell_area_box_context_push_group_width_for_height (context, group_idx, for_size,
                                                                 group_min, group_nat);
        }
    }

  *minimum_size = min_size;
  *natural_size = nat_size;

  g_free (orientation_sizes);

  /* Update rtl state for focus navigation to work */
  priv->rtl = (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
               gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
}



static void
gtk_cell_area_box_get_preferred_width (GtkCellArea        *area,
                                       GtkCellAreaContext *context,
                                       GtkWidget          *widget,
                                       int                *minimum_width,
                                       int                *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  int                    min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);

  /* Compute the size of all renderers for current row data,
   * bumping cell alignments in the context along the way
   */
  compute_size (box, GTK_ORIENTATION_HORIZONTAL,
                box_context, widget, -1, &min_width, &nat_width);

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

static void
gtk_cell_area_box_get_preferred_height (GtkCellArea        *area,
                                        GtkCellAreaContext *context,
                                        GtkWidget          *widget,
                                        int                *minimum_height,
                                        int                *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  int                    min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);

  /* Compute the size of all renderers for current row data,
   * bumping cell alignments in the context along the way
   */
  compute_size (box, GTK_ORIENTATION_VERTICAL,
                box_context, widget, -1, &min_height, &nat_height);

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
                                                  GtkCellAreaContext *context,
                                                  GtkWidget          *widget,
                                                  int                 width,
                                                  int                *minimum_height,
                                                  int                *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  int                    min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      /* Add up vertical requests of height for width and push
       * the overall cached sizes for alignments
       */
      compute_size (box, priv->orientation, box_context, widget, width, &min_height, &nat_height);
    }
  else
    {
      /* Juice: virtually allocate cells into the for_width using the
       * alignments and then return the overall height for that width,
       * and cache it
       */
      compute_size_for_opposing_orientation (box, box_context, widget, width, &min_height, &nat_height);
    }

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
                                                  GtkCellAreaContext *context,
                                                  GtkWidget          *widget,
                                                  int                 height,
                                                  int                *minimum_width,
                                                  int                *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  int                    min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Add up horizontal requests of width for height and push
       * the overall cached sizes for alignments
       */
      compute_size (box, priv->orientation, box_context, widget, height, &min_width, &nat_width);
    }
  else
    {
      /* Juice: horizontally allocate cells into the for_height using the
       * alignments and then return the overall width for that height,
       * and cache it
       */
      compute_size_for_opposing_orientation (box, box_context, widget, height, &min_width, &nat_width);
    }

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

enum {
  FOCUS_NONE,
  FOCUS_PREV,
  FOCUS_NEXT,
  FOCUS_LAST_CELL
};

static gboolean
gtk_cell_area_box_focus (GtkCellArea      *area,
                         GtkDirectionType  direction)
{
  GtkCellAreaBox        *box   = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  int                    cycle = FOCUS_NONE;
  gboolean               cycled_focus = FALSE;
  GtkCellRenderer       *focus_cell;

  focus_cell = gtk_cell_area_get_focus_cell (area);

  /* Special case, when there is no activatable cell, focus
   * is painted around the entire area... in this case we
   * let focus leave the area directly.
   */
  if (focus_cell && !gtk_cell_area_is_activatable (area))
    {
      gtk_cell_area_set_focus_cell (area, NULL);
      return FALSE;
    }

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
      cycle = priv->rtl ? FOCUS_PREV : FOCUS_NEXT;
      break;
    case GTK_DIR_TAB_BACKWARD:
      cycle = priv->rtl ? FOCUS_NEXT : FOCUS_PREV;
      break;
    case GTK_DIR_UP:
      if (priv->orientation == GTK_ORIENTATION_VERTICAL || !priv->last_focus_cell)
        cycle = FOCUS_PREV;
      else if (!focus_cell)
        cycle = FOCUS_LAST_CELL;
      break;
    case GTK_DIR_DOWN:
      if (priv->orientation == GTK_ORIENTATION_VERTICAL || !priv->last_focus_cell)
        cycle = FOCUS_NEXT;
      else if (!focus_cell)
        cycle = FOCUS_LAST_CELL;
      break;
    case GTK_DIR_LEFT:
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL || !priv->last_focus_cell)
        cycle = priv->rtl ? FOCUS_NEXT : FOCUS_PREV;
      else if (!focus_cell)
        cycle = FOCUS_LAST_CELL;
      break;
    case GTK_DIR_RIGHT:
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL || !priv->last_focus_cell)
        cycle = priv->rtl ? FOCUS_PREV : FOCUS_NEXT;
      else if (!focus_cell)
        cycle = FOCUS_LAST_CELL;
      break;
    default:
      break;
    }

  if (cycle == FOCUS_LAST_CELL)
    {
      gtk_cell_area_set_focus_cell (area, priv->last_focus_cell);
      cycled_focus = TRUE;
    }
  else if (cycle != FOCUS_NONE)
    {
      gboolean  found_cell = FALSE;
      GList    *list;
      int       i;

      /* If there is no focused cell, focus on the first (or last) one */
      if (!focus_cell)
        found_cell = TRUE;

      for (i = (cycle == FOCUS_NEXT) ? 0 : priv->groups->len -1;
           cycled_focus == FALSE && i >= 0 && i < priv->groups->len;
           i = (cycle == FOCUS_NEXT) ? i + 1 : i - 1)
        {
          CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

          for (list = (cycle == FOCUS_NEXT) ? g_list_first (group->cells) : g_list_last (group->cells);
               cycled_focus == FALSE && list; list = (cycle == FOCUS_NEXT) ? list->next : list->prev)
            {
              CellInfo *info = list->data;

              if (info->renderer == focus_cell)
                found_cell = TRUE;
              else if (found_cell && /* Dont give focus to cells that are siblings to a focus cell */
                       gtk_cell_area_get_focus_from_sibling (area, info->renderer) == NULL)
                {
                  gtk_cell_area_set_focus_cell (area, info->renderer);
                  cycled_focus = TRUE;
                }
            }
        }
    }

  if (!cycled_focus)
    gtk_cell_area_set_focus_cell (area, NULL);

  return cycled_focus;
}


/*************************************************************
 *                    GtkCellLayoutIface                     *
 *************************************************************/
static void
gtk_cell_area_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_cell_area_box_layout_pack_start;
  iface->pack_end   = gtk_cell_area_box_layout_pack_end;
  iface->reorder    = gtk_cell_area_box_layout_reorder;
}

static void
gtk_cell_area_box_layout_pack_start (GtkCellLayout      *cell_layout,
                                     GtkCellRenderer    *renderer,
                                     gboolean            expand)
{
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (cell_layout), renderer, expand, FALSE, TRUE);
}

static void
gtk_cell_area_box_layout_pack_end (GtkCellLayout      *cell_layout,
                                   GtkCellRenderer    *renderer,
                                   gboolean            expand)
{
  gtk_cell_area_box_pack_end (GTK_CELL_AREA_BOX (cell_layout), renderer, expand, FALSE, TRUE);
}

static void
gtk_cell_area_box_layout_reorder (GtkCellLayout      *cell_layout,
                                  GtkCellRenderer    *renderer,
                                  int                 position)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (cell_layout);
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  GList                 *node;
  CellInfo              *info;

  node = g_list_find_custom (priv->cells, renderer,
                             (GCompareFunc)cell_info_find);

  if (node)
    {
      info = node->data;

      priv->cells = g_list_delete_link (priv->cells, node);
      priv->cells = g_list_insert (priv->cells, info, position);

      cell_groups_rebuild (box);
    }
}

/*************************************************************
 *       Private interaction with GtkCellAreaBoxContext      *
 *************************************************************/
gboolean
_gtk_cell_area_box_group_visible (GtkCellAreaBox  *box,
				  int              group_idx)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  CellGroup *group;

  g_assert (group_idx >= 0 && group_idx < priv->groups->len);

  group = &g_array_index (priv->groups, CellGroup, group_idx);

  return group->visible;
}


/*************************************************************
 *                            API                            *
 *************************************************************/
/**
 * gtk_cell_area_box_new:
 *
 * Creates a new `GtkCellAreaBox`.
 *
 * Returns: a newly created `GtkCellAreaBox`
 *
 * Deprecated: 4.10
 */
GtkCellArea *
gtk_cell_area_box_new (void)
{
  return (GtkCellArea *)g_object_new (GTK_TYPE_CELL_AREA_BOX, NULL);
}

/**
 * gtk_cell_area_box_pack_start:
 * @box: a `GtkCellAreaBox`
 * @renderer: the `GtkCellRenderer` to add
 * @expand: whether @renderer should receive extra space when the area receives
 * more than its natural size
 * @align: whether @renderer should be aligned in adjacent rows
 * @fixed: whether @renderer should have the same size in all rows
 *
 * Adds @renderer to @box, packed with reference to the start of @box.
 *
 * The @renderer is packed after any other `GtkCellRenderer` packed
 * with reference to the start of @box.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_box_pack_start  (GtkCellAreaBox  *box,
                               GtkCellRenderer *renderer,
                               gboolean         expand,
                               gboolean         align,
			       gboolean         fixed)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  CellInfo              *info;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  if (g_list_find_custom (priv->cells, renderer,
                          (GCompareFunc)cell_info_find))
    {
      g_warning ("Refusing to add the same cell renderer to a GtkCellAreaBox twice");
      return;
    }

  info = cell_info_new (renderer, GTK_PACK_START, expand, align, fixed);

  priv->cells = g_list_append (priv->cells, info);

  cell_groups_rebuild (box);
}

/**
 * gtk_cell_area_box_pack_end:
 * @box: a `GtkCellAreaBox`
 * @renderer: the `GtkCellRenderer` to add
 * @expand: whether @renderer should receive extra space when the area receives
 * more than its natural size
 * @align: whether @renderer should be aligned in adjacent rows
 * @fixed: whether @renderer should have the same size in all rows
 *
 * Adds @renderer to @box, packed with reference to the end of @box.
 *
 * The @renderer is packed after (away from end of) any other
 * `GtkCellRenderer` packed with reference to the end of @box.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_box_pack_end (GtkCellAreaBox  *box,
                            GtkCellRenderer *renderer,
                            gboolean         expand,
                            gboolean         align,
			    gboolean         fixed)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);
  CellInfo              *info;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  if (g_list_find_custom (priv->cells, renderer,
                          (GCompareFunc)cell_info_find))
    {
      g_warning ("Refusing to add the same cell renderer to a GtkCellArea twice");
      return;
    }

  info = cell_info_new (renderer, GTK_PACK_END, expand, align, fixed);

  priv->cells = g_list_append (priv->cells, info);

  cell_groups_rebuild (box);
}

/**
 * gtk_cell_area_box_get_spacing:
 * @box: a `GtkCellAreaBox`
 *
 * Gets the spacing added between cell renderers.
 *
 * Returns: the space added between cell renderers in @box.
 *
 * Deprecated: 4.10
 */
int
gtk_cell_area_box_get_spacing (GtkCellAreaBox  *box)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX (box), 0);

  return priv->spacing;
}

/**
 * gtk_cell_area_box_set_spacing:
 * @box: a `GtkCellAreaBox`
 * @spacing: the space to add between `GtkCellRenderer`s
 *
 * Sets the spacing to add between cell renderers in @box.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_box_set_spacing (GtkCellAreaBox  *box,
                               int              spacing)
{
  GtkCellAreaBoxPrivate *priv = gtk_cell_area_box_get_instance_private (box);

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      g_object_notify (G_OBJECT (box), "spacing");

      /* Notify that size needs to be requested again */
      reset_contexts (box);
    }
}
