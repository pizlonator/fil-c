/* gtkcellarea.c
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
 * GtkCellArea:
 *
 * An abstract class for laying out `GtkCellRenderer`s
 *
 * The `GtkCellArea` is an abstract class for [iface@Gtk.CellLayout]
 * widgets (also referred to as "layouting widgets") to interface with
 * an arbitrary number of [class@Gtk.CellRenderer]s and interact with the user
 * for a given [iface@Gtk.TreeModel] row.
 *
 * The cell area handles events, focus navigation, drawing and
 * size requests and allocations for a given row of data.
 *
 * Usually users dont have to interact with the `GtkCellArea` directly
 * unless they are implementing a cell-layouting widget themselves.
 *
 * ## Requesting area sizes
 *
 * As outlined in
 * [GtkWidget’s geometry management section](class.Widget.html#height-for-width-geometry-management),
 * GTK uses a height-for-width
 * geometry management system to compute the sizes of widgets and user
 * interfaces. `GtkCellArea` uses the same semantics to calculate the
 * size of an area for an arbitrary number of `GtkTreeModel` rows.
 *
 * When requesting the size of a cell area one needs to calculate
 * the size for a handful of rows, and this will be done differently by
 * different layouting widgets. For instance a [class@Gtk.TreeViewColumn]
 * always lines up the areas from top to bottom while a [class@Gtk.IconView]
 * on the other hand might enforce that all areas received the same
 * width and wrap the areas around, requesting height for more cell
 * areas when allocated less width.
 *
 * It’s also important for areas to maintain some cell
 * alignments with areas rendered for adjacent rows (cells can
 * appear “columnized” inside an area even when the size of
 * cells are different in each row). For this reason the `GtkCellArea`
 * uses a [class@Gtk.CellAreaContext] object to store the alignments
 * and sizes along the way (as well as the overall largest minimum
 * and natural size for all the rows which have been calculated
 * with the said context).
 *
 * The [class@Gtk.CellAreaContext] is an opaque object specific to the
 * `GtkCellArea` which created it (see [method@Gtk.CellArea.create_context]).
 *
 * The owning cell-layouting widget can create as many contexts as
 * it wishes to calculate sizes of rows which should receive the
 * same size in at least one orientation (horizontally or vertically),
 * However, it’s important that the same [class@Gtk.CellAreaContext] which
 * was used to request the sizes for a given `GtkTreeModel` row be
 * used when rendering or processing events for that row.
 *
 * In order to request the width of all the rows at the root level
 * of a `GtkTreeModel` one would do the following:
 *
 * ```c
 * GtkTreeIter iter;
 * int minimum_width;
 * int natural_width;
 *
 * valid = gtk_tree_model_get_iter_first (model, &iter);
 * while (valid)
 *   {
 *     gtk_cell_area_apply_attributes (area, model, &iter, FALSE, FALSE);
 *     gtk_cell_area_get_preferred_width (area, context, widget, NULL, NULL);
 *
 *     valid = gtk_tree_model_iter_next (model, &iter);
 *   }
 *
 * gtk_cell_area_context_get_preferred_width (context, &minimum_width, &natural_width);
 * ```
 *
 * Note that in this example it’s not important to observe the
 * returned minimum and natural width of the area for each row
 * unless the cell-layouting object is actually interested in the
 * widths of individual rows. The overall width is however stored
 * in the accompanying `GtkCellAreaContext` object and can be consulted
 * at any time.
 *
 * This can be useful since `GtkCellLayout` widgets usually have to
 * support requesting and rendering rows in treemodels with an
 * exceedingly large amount of rows. The `GtkCellLayout` widget in
 * that case would calculate the required width of the rows in an
 * idle or timeout source (see [func@GLib.timeout_add]) and when the widget
 * is requested its actual width in [vfunc@Gtk.Widget.measure]
 * it can simply consult the width accumulated so far in the
 * `GtkCellAreaContext` object.
 *
 * A simple example where rows are rendered from top to bottom and
 * take up the full width of the layouting widget would look like:
 *
 * ```c
 * static void
 * foo_get_preferred_width (GtkWidget *widget,
 *                          int       *minimum_size,
 *                          int       *natural_size)
 * {
 *   Foo *self = FOO (widget);
 *   FooPrivate *priv = foo_get_instance_private (self);
 *
 *   foo_ensure_at_least_one_handfull_of_rows_have_been_requested (self);
 *
 *   gtk_cell_area_context_get_preferred_width (priv->context, minimum_size, natural_size);
 * }
 * ```
 *
 * In the above example the `Foo` widget has to make sure that some
 * row sizes have been calculated (the amount of rows that `Foo` judged
 * was appropriate to request space for in a single timeout iteration)
 * before simply returning the amount of space required by the area via
 * the `GtkCellAreaContext`.
 *
 * Requesting the height for width (or width for height) of an area is
 * a similar task except in this case the `GtkCellAreaContext` does not
 * store the data (actually, it does not know how much space the layouting
 * widget plans to allocate it for every row. It’s up to the layouting
 * widget to render each row of data with the appropriate height and
 * width which was requested by the `GtkCellArea`).
 *
 * In order to request the height for width of all the rows at the
 * root level of a `GtkTreeModel` one would do the following:
 *
 * ```c
 * GtkTreeIter iter;
 * int minimum_height;
 * int natural_height;
 * int full_minimum_height = 0;
 * int full_natural_height = 0;
 *
 * valid = gtk_tree_model_get_iter_first (model, &iter);
 * while (valid)
 *   {
 *     gtk_cell_area_apply_attributes (area, model, &iter, FALSE, FALSE);
 *     gtk_cell_area_get_preferred_height_for_width (area, context, widget,
 *                                                   width, &minimum_height, &natural_height);
 *
 *     if (width_is_for_allocation)
 *        cache_row_height (&iter, minimum_height, natural_height);
 *
 *     full_minimum_height += minimum_height;
 *     full_natural_height += natural_height;
 *
 *     valid = gtk_tree_model_iter_next (model, &iter);
 *   }
 * ```
 *
 * Note that in the above example we would need to cache the heights
 * returned for each row so that we would know what sizes to render the
 * areas for each row. However we would only want to really cache the
 * heights if the request is intended for the layouting widgets real
 * allocation.
 *
 * In some cases the layouting widget is requested the height for an
 * arbitrary for_width, this is a special case for layouting widgets
 * who need to request size for tens of thousands  of rows. For this
 * case it’s only important that the layouting widget calculate
 * one reasonably sized chunk of rows and return that height
 * synchronously. The reasoning here is that any layouting widget is
 * at least capable of synchronously calculating enough height to fill
 * the screen height (or scrolled window height) in response to a single
 * call to [vfunc@Gtk.Widget.measure]. Returning
 * a perfect height for width that is larger than the screen area is
 * inconsequential since after the layouting receives an allocation
 * from a scrolled window it simply continues to drive the scrollbar
 * values while more and more height is required for the row heights
 * that are calculated in the background.
 *
 * ## Rendering Areas
 *
 * Once area sizes have been acquired at least for the rows in the
 * visible area of the layouting widget they can be rendered at
 * [vfunc@Gtk.Widget.snapshot] time.
 *
 * A crude example of how to render all the rows at the root level
 * runs as follows:
 *
 * ```c
 * GtkAllocation allocation;
 * GdkRectangle cell_area = { 0, };
 * GtkTreeIter iter;
 * int minimum_width;
 * int natural_width;
 *
 * gtk_widget_get_allocation (widget, &allocation);
 * cell_area.width = allocation.width;
 *
 * valid = gtk_tree_model_get_iter_first (model, &iter);
 * while (valid)
 *   {
 *     cell_area.height = get_cached_height_for_row (&iter);
 *
 *     gtk_cell_area_apply_attributes (area, model, &iter, FALSE, FALSE);
 *     gtk_cell_area_render (area, context, widget, cr,
 *                           &cell_area, &cell_area, state_flags, FALSE);
 *
 *     cell_area.y += cell_area.height;
 *
 *     valid = gtk_tree_model_iter_next (model, &iter);
 *   }
 * ```
 *
 * Note that the cached height in this example really depends on how
 * the layouting widget works. The layouting widget might decide to
 * give every row its minimum or natural height or, if the model content
 * is expected to fit inside the layouting widget without scrolling, it
 * would make sense to calculate the allocation for each row at
 * the time the widget is allocated using [func@Gtk.distribute_natural_allocation].
 *
 * ## Handling Events and Driving Keyboard Focus
 *
 * Passing events to the area is as simple as handling events on any
 * normal widget and then passing them to the [method@Gtk.CellArea.event]
 * API as they come in. Usually `GtkCellArea` is only interested in
 * button events, however some customized derived areas can be implemented
 * who are interested in handling other events. Handling an event can
 * trigger the [signal@Gtk.CellArea::focus-changed] signal to fire; as well
 * as [signal@Gtk.CellArea::add-editable] in the case that an editable cell
 * was clicked and needs to start editing. You can call
 * [method@Gtk.CellArea.stop_editing] at any time to cancel any cell editing
 * that is currently in progress.
 *
 * The `GtkCellArea` drives keyboard focus from cell to cell in a way
 * similar to `GtkWidget`. For layouting widgets that support giving
 * focus to cells it’s important to remember to pass `GTK_CELL_RENDERER_FOCUSED`
 * to the area functions for the row that has focus and to tell the
 * area to paint the focus at render time.
 *
 * Layouting widgets that accept focus on cells should implement the
 * [vfunc@Gtk.Widget.focus] virtual method. The layouting widget is always
 * responsible for knowing where `GtkTreeModel` rows are rendered inside
 * the widget, so at [vfunc@Gtk.Widget.focus] time the layouting widget
 * should use the `GtkCellArea` methods to navigate focus inside the area
 * and then observe the [enum@Gtk.DirectionType] to pass the focus to adjacent
 * rows and areas.
 *
 * A basic example of how the [vfunc@Gtk.Widget.focus] virtual method
 * should be implemented:
 *
 * ```
 * static gboolean
 * foo_focus (GtkWidget       *widget,
 *            GtkDirectionType direction)
 * {
 *   Foo *self = FOO (widget);
 *   FooPrivate *priv = foo_get_instance_private (self);
 *   int focus_row = priv->focus_row;
 *   gboolean have_focus = FALSE;
 *
 *   if (!gtk_widget_has_focus (widget))
 *     gtk_widget_grab_focus (widget);
 *
 *   valid = gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, priv->focus_row);
 *   while (valid)
 *     {
 *       gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
 *
 *       if (gtk_cell_area_focus (priv->area, direction))
 *         {
 *            priv->focus_row = focus_row;
 *            have_focus = TRUE;
 *            break;
 *         }
 *       else
 *         {
 *           if (direction == GTK_DIR_RIGHT ||
 *               direction == GTK_DIR_LEFT)
 *             break;
 *           else if (direction == GTK_DIR_UP ||
 *                    direction == GTK_DIR_TAB_BACKWARD)
 *            {
 *               if (focus_row == 0)
 *                 break;
 *               else
 *                {
 *                   focus_row--;
 *                   valid = gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, focus_row);
 *                }
 *             }
 *           else
 *             {
 *               if (focus_row == last_row)
 *                 break;
 *               else
 *                 {
 *                   focus_row++;
 *                   valid = gtk_tree_model_iter_next (priv->model, &iter);
 *                 }
 *             }
 *         }
 *     }
 *     return have_focus;
 * }
 * ```
 *
 * Note that the layouting widget is responsible for matching the
 * `GtkDirectionType` values to the way it lays out its cells.
 *
 * ## Cell Properties
 *
 * The `GtkCellArea` introduces cell properties for `GtkCellRenderer`s.
 * This provides some general interfaces for defining the relationship
 * cell areas have with their cells. For instance in a [class@Gtk.CellAreaBox]
 * a cell might “expand” and receive extra space when the area is allocated
 * more than its full natural request, or a cell might be configured to “align”
 * with adjacent rows which were requested and rendered with the same
 * `GtkCellAreaContext`.
 *
 * Use [method@Gtk.CellAreaClass.install_cell_property] to install cell
 * properties for a cell area class and [method@Gtk.CellAreaClass.find_cell_property]
 * or [method@Gtk.CellAreaClass.list_cell_properties] to get information about
 * existing cell properties.
 *
 * To set the value of a cell property, use [method@Gtk.CellArea.cell_set_property],
 * [method@Gtk.CellArea.cell_set] or [method@Gtk.CellArea.cell_set_valist]. To obtain
 * the value of a cell property, use [method@Gtk.CellArea.cell_get_property]
 * [method@Gtk.CellArea.cell_get] or [method@Gtk.CellArea.cell_get_valist].
 *
 * Deprecated: 4.10: List views use widgets for displaying their
 *   contents
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "deprecated/gtkcelllayout.h"
#include "gtkcellarea.h"
#include "deprecated/gtkcellareacontext.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "deprecated/gtkrender.h"
#include "gtkstylecontext.h"
#include "gtknative.h"

#include <gobject/gvaluecollector.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* GObjectClass */
static void      gtk_cell_area_dispose                             (GObject            *object);
static void      gtk_cell_area_finalize                            (GObject            *object);
static void      gtk_cell_area_set_property                        (GObject            *object,
                                                                    guint               prop_id,
                                                                    const GValue       *value,
                                                                    GParamSpec         *pspec);
static void      gtk_cell_area_get_property                        (GObject            *object,
                                                                    guint               prop_id,
                                                                    GValue             *value,
                                                                    GParamSpec         *pspec);

/* GtkCellAreaClass */
static void      gtk_cell_area_real_add                            (GtkCellArea         *area,
								    GtkCellRenderer     *renderer);
static void      gtk_cell_area_real_remove                         (GtkCellArea         *area,
								    GtkCellRenderer     *renderer);
static void      gtk_cell_area_real_foreach                        (GtkCellArea         *area,
								    GtkCellCallback      callback,
								    gpointer             callback_data);
static void      gtk_cell_area_real_foreach_alloc                  (GtkCellArea         *area,
								    GtkCellAreaContext  *context,
								    GtkWidget           *widget,
								    const GdkRectangle  *cell_area,
								    const GdkRectangle  *background_area,
								    GtkCellAllocCallback callback,
								    gpointer             callback_data);
static int       gtk_cell_area_real_event                          (GtkCellArea          *area,
                                                                    GtkCellAreaContext   *context,
                                                                    GtkWidget            *widget,
                                                                    GdkEvent             *event,
                                                                    const GdkRectangle   *cell_area,
                                                                    GtkCellRendererState  flags);
static void      gtk_cell_area_real_snapshot                       (GtkCellArea          *area,
                                                                    GtkCellAreaContext   *context,
                                                                    GtkWidget            *widget,
                                                                    GtkSnapshot          *snapshot,
                                                                    const GdkRectangle   *background_area,
                                                                    const GdkRectangle   *cell_area,
                                                                    GtkCellRendererState  flags,
                                                                    gboolean              paint_focus);
static void      gtk_cell_area_real_apply_attributes               (GtkCellArea           *area,
                                                                    GtkTreeModel          *tree_model,
                                                                    GtkTreeIter           *iter,
                                                                    gboolean               is_expander,
                                                                    gboolean               is_expanded);

static GtkCellAreaContext *gtk_cell_area_real_create_context       (GtkCellArea           *area);
static GtkCellAreaContext *gtk_cell_area_real_copy_context         (GtkCellArea           *area,
								    GtkCellAreaContext    *context);
static GtkSizeRequestMode  gtk_cell_area_real_get_request_mode     (GtkCellArea           *area);
static void      gtk_cell_area_real_get_preferred_width            (GtkCellArea           *area,
								    GtkCellAreaContext    *context,
								    GtkWidget             *widget,
								    int                   *minimum_width,
								    int                   *natural_width);
static void      gtk_cell_area_real_get_preferred_height           (GtkCellArea           *area,
								    GtkCellAreaContext    *context,
								    GtkWidget             *widget,
								    int                   *minimum_height,
								    int                   *natural_height);
static void      gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea           *area,
                                                                    GtkCellAreaContext    *context,
                                                                    GtkWidget             *widget,
                                                                    int                    width,
                                                                    int                   *minimum_height,
                                                                    int                   *natural_height);
static void      gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea           *area,
                                                                    GtkCellAreaContext    *context,
                                                                    GtkWidget             *widget,
                                                                    int                    height,
                                                                    int                   *minimum_width,
                                                                    int                   *natural_width);
static gboolean  gtk_cell_area_real_is_activatable                 (GtkCellArea           *area);
static gboolean  gtk_cell_area_real_activate                       (GtkCellArea           *area,
                                                                    GtkCellAreaContext    *context,
                                                                    GtkWidget             *widget,
                                                                    const GdkRectangle    *cell_area,
                                                                    GtkCellRendererState   flags,
                                                                    gboolean               edit_only);
static gboolean  gtk_cell_area_real_focus                          (GtkCellArea           *area,
								    GtkDirectionType       direction);

/* GtkCellLayoutIface */
static void      gtk_cell_area_cell_layout_init              (GtkCellLayoutIface    *iface);
static void      gtk_cell_area_pack_default                  (GtkCellLayout         *cell_layout,
                                                              GtkCellRenderer       *renderer,
                                                              gboolean               expand);
static void      gtk_cell_area_clear                         (GtkCellLayout         *cell_layout);
static void      gtk_cell_area_add_attribute                 (GtkCellLayout         *cell_layout,
                                                              GtkCellRenderer       *renderer,
                                                              const char            *attribute,
                                                              int                    column);
static void      gtk_cell_area_set_cell_data_func            (GtkCellLayout         *cell_layout,
                                                              GtkCellRenderer       *cell,
                                                              GtkCellLayoutDataFunc  func,
                                                              gpointer               func_data,
                                                              GDestroyNotify         destroy);
static void      gtk_cell_area_clear_attributes              (GtkCellLayout         *cell_layout,
                                                              GtkCellRenderer       *renderer);
static void      gtk_cell_area_reorder                       (GtkCellLayout         *cell_layout,
                                                              GtkCellRenderer       *cell,
                                                              int                    position);
static GList    *gtk_cell_area_get_cells                     (GtkCellLayout         *cell_layout);
static GtkCellArea *gtk_cell_area_get_area                   (GtkCellLayout         *cell_layout);

/* GtkBuildableIface */
static void      gtk_cell_area_buildable_init                (GtkBuildableIface     *iface);
static void      gtk_cell_area_buildable_custom_tag_end      (GtkBuildable          *buildable,
                                                              GtkBuilder            *builder,
                                                              GObject               *child,
                                                              const char            *tagname,
                                                              gpointer               data);

/* Used in foreach loop to check if a child renderer is present */
typedef struct {
  GtkCellRenderer *renderer;
  gboolean         has_renderer;
} HasRendererCheck;

/* Used in foreach loop to get a cell's allocation */
typedef struct {
  GtkCellRenderer *renderer;
  GdkRectangle     allocation;
} RendererAllocationData;

/* Used in foreach loop to render cells */
typedef struct {
  GtkCellArea         *area;
  GtkWidget           *widget;
  GtkSnapshot         *snapshot;
  GdkRectangle         focus_rect;
  GtkCellRendererState render_flags;
  guint                paint_focus : 1;
  guint                focus_all   : 1;
  guint                first_focus : 1;
} CellRenderData;

/* Used in foreach loop to get a cell by position */
typedef struct {
  int              x;
  int              y;
  GtkCellRenderer *renderer;
  GdkRectangle     cell_area;
} CellByPositionData;

/* Attribute/Cell metadata */
typedef struct {
  const char *attribute;
  int          column;
} CellAttribute;

typedef struct {
  GSList          *attributes;

  GtkCellLayoutDataFunc  func;
  gpointer               data;
  GDestroyNotify         destroy;
  GtkCellLayout         *proxy;
} CellInfo;

static CellInfo       *cell_info_new       (GtkCellLayoutDataFunc  func,
                                            gpointer               data,
                                            GDestroyNotify         destroy);
static void            cell_info_free      (CellInfo              *info);
static CellAttribute  *cell_attribute_new  (GtkCellRenderer       *renderer,
                                            const char            *attribute,
                                            int                    column);
static void            cell_attribute_free (CellAttribute         *attribute);
static int             cell_attribute_find (CellAttribute         *cell_attribute,
                                            const char            *attribute);

/* Internal functions/signal emissions */
static void            gtk_cell_area_add_editable     (GtkCellArea        *area,
                                                       GtkCellRenderer    *renderer,
                                                       GtkCellEditable    *editable,
                                                       const GdkRectangle *cell_area);
static void            gtk_cell_area_remove_editable  (GtkCellArea        *area,
                                                       GtkCellRenderer    *renderer,
                                                       GtkCellEditable    *editable);
static void            gtk_cell_area_set_edit_widget  (GtkCellArea        *area,
                                                       GtkCellEditable    *editable);
static void            gtk_cell_area_set_edited_cell  (GtkCellArea        *area,
                                                       GtkCellRenderer    *renderer);


/* Struct to pass data along while looping over
 * cell renderers to apply attributes
 */
typedef struct {
  GtkCellArea  *area;
  GtkTreeModel *model;
  GtkTreeIter  *iter;
  gboolean      is_expander;
  gboolean      is_expanded;
} AttributeData;

typedef struct _GtkCellAreaPrivate       GtkCellAreaPrivate;

struct _GtkCellAreaPrivate
{
  /* The GtkCellArea bookkeeps any connected
   * attributes in this hash table.
   */
  GHashTable      *cell_info;

  /* Current path is saved as a side-effect
   * of gtk_cell_area_apply_attributes()
   */
  char            *current_path;

  /* Current cell being edited and editable widget used */
  GtkCellEditable *edit_widget;
  GtkCellRenderer *edited_cell;

  /* Signal connections to the editable widget */
  gulong           remove_widget_id;

  /* Currently focused cell */
  GtkCellRenderer *focus_cell;

  /* Tracking which cells are focus siblings of focusable cells */
  GHashTable      *focus_siblings;
};

enum {
  PROP_0,
  PROP_FOCUS_CELL,
  PROP_EDITED_CELL,
  PROP_EDIT_WIDGET
};

enum {
  SIGNAL_APPLY_ATTRIBUTES,
  SIGNAL_ADD_EDITABLE,
  SIGNAL_REMOVE_EDITABLE,
  SIGNAL_FOCUS_CHANGED,
  LAST_SIGNAL
};

/* Keep the paramspec pool internal, no need to deliver notifications
 * on cells. at least no perceived need for now
 */
static GParamSpecPool *cell_property_pool = NULL;
static guint           cell_area_signals[LAST_SIGNAL] = { 0 };

#define PARAM_SPEC_PARAM_ID(pspec)              ((pspec)->param_id)
#define PARAM_SPEC_SET_PARAM_ID(pspec, id)      ((pspec)->param_id = (id))

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkCellArea, gtk_cell_area, G_TYPE_INITIALLY_UNOWNED,
                                  G_ADD_PRIVATE (GtkCellArea)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                         gtk_cell_area_cell_layout_init)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                         gtk_cell_area_buildable_init))

static void
gtk_cell_area_init (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  priv->cell_info = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify)cell_info_free);

  priv->focus_siblings = g_hash_table_new_full (g_direct_hash,
                                                g_direct_equal,
                                                NULL,
                                                (GDestroyNotify)g_list_free);

  priv->focus_cell         = NULL;
  priv->edited_cell        = NULL;
  priv->edit_widget        = NULL;

  priv->remove_widget_id   = 0;
}

static void
gtk_cell_area_class_init (GtkCellAreaClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  /* GObjectClass */
  object_class->dispose      = gtk_cell_area_dispose;
  object_class->finalize     = gtk_cell_area_finalize;
  object_class->get_property = gtk_cell_area_get_property;
  object_class->set_property = gtk_cell_area_set_property;

  /* general */
  class->add              = gtk_cell_area_real_add;
  class->remove           = gtk_cell_area_real_remove;
  class->foreach          = gtk_cell_area_real_foreach;
  class->foreach_alloc    = gtk_cell_area_real_foreach_alloc;
  class->event            = gtk_cell_area_real_event;
  class->snapshot         = gtk_cell_area_real_snapshot;
  class->apply_attributes = gtk_cell_area_real_apply_attributes;

  /* geometry */
  class->create_context                 = gtk_cell_area_real_create_context;
  class->copy_context                   = gtk_cell_area_real_copy_context;
  class->get_request_mode               = gtk_cell_area_real_get_request_mode;
  class->get_preferred_width            = gtk_cell_area_real_get_preferred_width;
  class->get_preferred_height           = gtk_cell_area_real_get_preferred_height;
  class->get_preferred_height_for_width = gtk_cell_area_real_get_preferred_height_for_width;
  class->get_preferred_width_for_height = gtk_cell_area_real_get_preferred_width_for_height;

  /* focus */
  class->is_activatable = gtk_cell_area_real_is_activatable;
  class->activate       = gtk_cell_area_real_activate;
  class->focus          = gtk_cell_area_real_focus;

  /* Signals */
  /**
   * GtkCellArea::apply-attributes:
   * @area: the `GtkCellArea` to apply the attributes to
   * @model: the `GtkTreeModel` to apply the attributes from
   * @iter: the `GtkTreeIter` indicating which row to apply the attributes of
   * @is_expander: whether the view shows children for this row
   * @is_expanded: whether the view is currently showing the children of this row
   *
   * This signal is emitted whenever applying attributes to @area from @model
   */
  cell_area_signals[SIGNAL_APPLY_ATTRIBUTES] =
    g_signal_new (I_("apply-attributes"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCellAreaClass, apply_attributes),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_BOXED_BOOLEAN_BOOLEAN,
                  G_TYPE_NONE, 4,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (cell_area_signals[SIGNAL_APPLY_ATTRIBUTES], G_TYPE_FROM_CLASS (class),
                              _gtk_marshal_VOID__OBJECT_BOXED_BOOLEAN_BOOLEANv);

  /**
   * GtkCellArea::add-editable:
   * @area: the `GtkCellArea` where editing started
   * @renderer: the `GtkCellRenderer` that started the edited
   * @editable: the `GtkCellEditable` widget to add
   * @cell_area: the `GtkWidget` relative `GdkRectangle` coordinates
   *             where @editable should be added
   * @path: the `GtkTreePath` string this edit was initiated for
   *
   * Indicates that editing has started on @renderer and that @editable
   * should be added to the owning cell-layouting widget at @cell_area.
   */
  cell_area_signals[SIGNAL_ADD_EDITABLE] =
    g_signal_new (I_("add-editable"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, /* No class closure here */
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT_BOXED_STRING,
                  G_TYPE_NONE, 4,
                  GTK_TYPE_CELL_RENDERER,
                  GTK_TYPE_CELL_EDITABLE,
                  GDK_TYPE_RECTANGLE,
                  G_TYPE_STRING);


  /**
   * GtkCellArea::remove-editable:
   * @area: the `GtkCellArea` where editing finished
   * @renderer: the `GtkCellRenderer` that finished editeding
   * @editable: the `GtkCellEditable` widget to remove
   *
   * Indicates that editing finished on @renderer and that @editable
   * should be removed from the owning cell-layouting widget.
   */
  cell_area_signals[SIGNAL_REMOVE_EDITABLE] =
    g_signal_new (I_("remove-editable"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, /* No class closure here */
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_CELL_RENDERER,
                  GTK_TYPE_CELL_EDITABLE);

  /**
   * GtkCellArea::focus-changed:
   * @area: the `GtkCellArea` where focus changed
   * @renderer: the `GtkCellRenderer` that has focus
   * @path: the current `GtkTreePath` string set for @area
   *
   * Indicates that focus changed on this @area. This signal
   * is emitted either as a result of focus handling or event
   * handling.
   *
   * It's possible that the signal is emitted even if the
   * currently focused renderer did not change, this is
   * because focus may change to the same renderer in the
   * same cell area for a different row of data.
   */
  cell_area_signals[SIGNAL_FOCUS_CHANGED] =
    g_signal_new (I_("focus-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, /* No class closure here */
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_STRING,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_CELL_RENDERER,
                  G_TYPE_STRING);

  /* Properties */
  /**
   * GtkCellArea:focus-cell:
   *
   * The cell in the area that currently has focus
   */
  g_object_class_install_property (object_class,
                                   PROP_FOCUS_CELL,
                                   g_param_spec_object ("focus-cell", NULL, NULL,
                                                        GTK_TYPE_CELL_RENDERER,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkCellArea:edited-cell:
   *
   * The cell in the area that is currently edited
   *
   * This property is read-only and only changes as
   * a result of a call gtk_cell_area_activate_cell().
   */
  g_object_class_install_property (object_class,
                                   PROP_EDITED_CELL,
                                   g_param_spec_object ("edited-cell", NULL, NULL,
                                                        GTK_TYPE_CELL_RENDERER,
                                                        GTK_PARAM_READABLE));

  /**
   * GtkCellArea:edit-widget:
   *
   * The widget currently editing the edited cell
   *
   * This property is read-only and only changes as
   * a result of a call gtk_cell_area_activate_cell().
   */
  g_object_class_install_property (object_class,
                                   PROP_EDIT_WIDGET,
                                   g_param_spec_object ("edit-widget", NULL, NULL,
                                                        GTK_TYPE_CELL_EDITABLE,
                                                        GTK_PARAM_READABLE));

  /* Pool for Cell Properties */
  if (!cell_property_pool)
    cell_property_pool = g_param_spec_pool_new (FALSE);
}

/*************************************************************
 *                    CellInfo Basics                        *
 *************************************************************/
static CellInfo *
cell_info_new (GtkCellLayoutDataFunc  func,
               gpointer               data,
               GDestroyNotify         destroy)
{
  CellInfo *info = g_slice_new0 (CellInfo);

  info->func     = func;
  info->data     = data;
  info->destroy  = destroy;

  return info;
}

static void
cell_info_free (CellInfo *info)
{
  if (info->destroy)
    info->destroy (info->data);

  g_slist_free_full (info->attributes, (GDestroyNotify)cell_attribute_free);

  g_slice_free (CellInfo, info);
}

static CellAttribute  *
cell_attribute_new  (GtkCellRenderer       *renderer,
                     const char            *attribute,
                     int                    column)
{
  GParamSpec *pspec;

  /* Check if the attribute really exists and point to
   * the property string installed on the cell renderer
   * class (dont dup the string)
   */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), attribute);

  if (pspec)
    {
      CellAttribute *cell_attribute = g_slice_new (CellAttribute);

      cell_attribute->attribute = pspec->name;
      cell_attribute->column    = column;

      return cell_attribute;
    }

  return NULL;
}

static void
cell_attribute_free (CellAttribute *attribute)
{
  g_slice_free (CellAttribute, attribute);
}

/* GCompareFunc for g_slist_find_custom() */
static int
cell_attribute_find (CellAttribute *cell_attribute,
                     const char    *attribute)
{
  return g_strcmp0 (cell_attribute->attribute, attribute);
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_finalize (GObject *object)
{
  GtkCellArea        *area   = GTK_CELL_AREA (object);
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  /* All cell renderers should already be removed at this point,
   * just kill our (empty) hash tables here.
   */
  g_hash_table_destroy (priv->cell_info);
  g_hash_table_destroy (priv->focus_siblings);

  g_free (priv->current_path);

  G_OBJECT_CLASS (gtk_cell_area_parent_class)->finalize (object);
}


static void
gtk_cell_area_dispose (GObject *object)
{
  /* This removes every cell renderer that may be added to the GtkCellArea,
   * subclasses should be breaking references to the GtkCellRenderers
   * at this point.
   */
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (object));

  /* Remove any ref to a focused/edited cell */
  gtk_cell_area_set_focus_cell (GTK_CELL_AREA (object), NULL);
  gtk_cell_area_set_edited_cell (GTK_CELL_AREA (object), NULL);
  gtk_cell_area_set_edit_widget (GTK_CELL_AREA (object), NULL);

  G_OBJECT_CLASS (gtk_cell_area_parent_class)->dispose (object);
}

static void
gtk_cell_area_set_property (GObject       *object,
                            guint          prop_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
  GtkCellArea *area = GTK_CELL_AREA (object);

  switch (prop_id)
    {
    case PROP_FOCUS_CELL:
      gtk_cell_area_set_focus_cell (area, (GtkCellRenderer *)g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  GtkCellArea        *area = GTK_CELL_AREA (object);
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  switch (prop_id)
    {
    case PROP_FOCUS_CELL:
      g_value_set_object (value, priv->focus_cell);
      break;
    case PROP_EDITED_CELL:
      g_value_set_object (value, priv->edited_cell);
      break;
    case PROP_EDIT_WIDGET:
      g_value_set_object (value, priv->edit_widget);
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
gtk_cell_area_real_add (GtkCellArea         *area,
			GtkCellRenderer     *renderer)
{
    g_warning ("GtkCellAreaClass::add not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static void
gtk_cell_area_real_remove (GtkCellArea         *area,
			   GtkCellRenderer     *renderer)
{
    g_warning ("GtkCellAreaClass::remove not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static void
gtk_cell_area_real_foreach (GtkCellArea         *area,
			    GtkCellCallback      callback,
			    gpointer             callback_data)
{
    g_warning ("GtkCellAreaClass::foreach not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static void
gtk_cell_area_real_foreach_alloc (GtkCellArea         *area,
				  GtkCellAreaContext  *context,
				  GtkWidget           *widget,
				  const GdkRectangle  *cell_area,
				  const GdkRectangle  *background_area,
				  GtkCellAllocCallback callback,
				  gpointer             callback_data)
{
    g_warning ("GtkCellAreaClass::foreach_alloc not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static int
gtk_cell_area_real_event (GtkCellArea          *area,
                          GtkCellAreaContext   *context,
                          GtkWidget            *widget,
                          GdkEvent             *event,
                          const GdkRectangle   *cell_area,
                          GtkCellRendererState  flags)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  gboolean            retval = FALSE;
  GdkEventType        event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_KEY_PRESS && (flags & GTK_CELL_RENDERER_FOCUSED) != 0)
    {
      /* Cancel any edits in progress */
      if (priv->edited_cell &&
          gdk_key_event_get_keyval (event) == GDK_KEY_Escape)
        {
          gtk_cell_area_stop_editing (area, TRUE);
          retval = TRUE;
        }
    }
  else if (event_type == GDK_BUTTON_PRESS)
    {
      guint button;

      button = gdk_button_event_get_button (event);
      if (button == GDK_BUTTON_PRIMARY)
        {
          GtkCellRenderer *renderer = NULL;
          GtkCellRenderer *focus_renderer;
          GdkRectangle     alloc_area;
          double event_x, event_y;
          double nx, ny;
          double x, y;
          GtkNative *native;

          /* We may need some semantics to tell us the offset of the event
           * window we are handling events for (i.e. GtkTreeView has a bin_window) */
          gdk_event_get_position (event, &event_x, &event_y);

          native = gtk_widget_get_native (widget);
          gtk_native_get_surface_transform (native, &nx, &ny);
          gtk_widget_translate_coordinates (GTK_WIDGET (native), widget, event_x - nx, event_y - ny, &x, &y);
          event_x = x;
          event_y = y;

          /* Dont try to search for an event coordinate that is not in the area, that will
           * trigger a runtime warning.
           */
          if (event_x >= cell_area->x && event_x <= cell_area->x + cell_area->width &&
              event_y >= cell_area->y && event_y <= cell_area->y + cell_area->height)
            renderer =
              gtk_cell_area_get_cell_at_position (area, context, widget,
                                                  cell_area, event_x, event_y,
                                                  &alloc_area);

          if (renderer)
            {
              focus_renderer = gtk_cell_area_get_focus_from_sibling (area, renderer);
              if (!focus_renderer)
                focus_renderer = renderer;

              /* If we're already editing, cancel it and set focus */
              if (gtk_cell_area_get_edited_cell (area))
                {
                  /* XXX Was it really canceled in this case ? */
                  gtk_cell_area_stop_editing (area, TRUE);
                  gtk_cell_area_set_focus_cell (area, focus_renderer);
                  retval = TRUE;
                }
              else
                {
                  /* If we are activating via a focus sibling,
                   * we need to fetch the right cell area for the real event renderer */
                  if (focus_renderer != renderer)
                    gtk_cell_area_get_cell_allocation (area, context, widget, focus_renderer,
                                                       cell_area, &alloc_area);

                  gtk_cell_area_set_focus_cell (area, focus_renderer);
                  retval = gtk_cell_area_activate_cell (area, widget, focus_renderer,
                                                        event, &alloc_area, flags);
                }
            }
        }
    }

  return retval;
}

static gboolean
snapshot_cell (GtkCellRenderer        *renderer,
               const GdkRectangle     *cell_area,
               const GdkRectangle     *cell_background,
               CellRenderData         *data)
{
  GtkCellRenderer      *focus_cell;
  GtkCellRendererState  flags;
  GdkRectangle          inner_area;

  focus_cell = gtk_cell_area_get_focus_cell (data->area);
  flags      = data->render_flags;

  gtk_cell_area_inner_cell_area (data->area, data->widget, cell_area, &inner_area);

  if ((flags & GTK_CELL_RENDERER_FOCUSED) &&
      (data->focus_all ||
       (focus_cell &&
        (renderer == focus_cell ||
         gtk_cell_area_is_focus_sibling (data->area, focus_cell, renderer)))))
    {
      GdkRectangle cell_focus;

      gtk_cell_renderer_get_aligned_area (renderer, data->widget, flags, &inner_area, &cell_focus);

      if (data->first_focus)
        {
          data->first_focus = FALSE;
          data->focus_rect  = cell_focus;
        }
      else
        {
          gdk_rectangle_union (&data->focus_rect, &cell_focus, &data->focus_rect);
        }
    }

  gtk_cell_renderer_snapshot (renderer, data->snapshot, data->widget,
                              cell_background, &inner_area, flags);

  return FALSE;
}

static void
gtk_cell_area_real_snapshot (GtkCellArea          *area,
                             GtkCellAreaContext   *context,
                             GtkWidget            *widget,
                             GtkSnapshot          *snapshot,
                             const GdkRectangle   *background_area,
                             const GdkRectangle   *cell_area,
                             GtkCellRendererState  flags,
                             gboolean              paint_focus)
{
  CellRenderData render_data =
    {
      area,
      widget,
      snapshot,
      { 0, },
      flags,
      paint_focus,
      FALSE, TRUE
    };

  /* Make sure we dont paint a focus rectangle while there
   * is an editable widget in play
   */
  if (gtk_cell_area_get_edited_cell (area))
    render_data.paint_focus = FALSE;

  if (!gtk_widget_has_visible_focus (widget))
    render_data.paint_focus = FALSE;

  /* If no cell can activate but the caller wants focus painted,
   * then we paint focus around all cells */
  if ((flags & GTK_CELL_RENDERER_FOCUSED) != 0 && paint_focus &&
      !gtk_cell_area_is_activatable (area))
    render_data.focus_all = TRUE;

  gtk_cell_area_foreach_alloc (area, context, widget, cell_area, background_area,
                               (GtkCellAllocCallback) snapshot_cell, &render_data);

  if (render_data.paint_focus &&
      render_data.focus_rect.width != 0 &&
      render_data.focus_rect.height != 0)
    {
      GtkStyleContext *style_context;
      GtkStateFlags renderer_state = 0;

      style_context = gtk_widget_get_style_context (widget);
      gtk_style_context_save (style_context);

      renderer_state = gtk_cell_renderer_get_state (NULL, widget, flags);
      gtk_style_context_set_state (style_context, renderer_state);

      gtk_snapshot_render_focus (snapshot, style_context,
                                 render_data.focus_rect.x,     render_data.focus_rect.y,
                                 render_data.focus_rect.width, render_data.focus_rect.height);

      gtk_style_context_restore (style_context);
    }
}

static void
apply_cell_attributes (GtkCellRenderer *renderer,
                       CellInfo        *info,
                       AttributeData   *data)
{
  CellAttribute *attribute;
  GSList        *list;
  GValue         value = G_VALUE_INIT;
  gboolean       is_expander;
  gboolean       is_expanded;

  g_object_freeze_notify (G_OBJECT (renderer));

  /* Whether a row expands or is presently expanded can only be
   * provided by the view (as these states can vary across views
   * accessing the same model).
   */
  is_expander = gtk_cell_renderer_get_is_expander (renderer);
  if (is_expander != data->is_expander)
    gtk_cell_renderer_set_is_expander (renderer, data->is_expander);

  is_expanded = gtk_cell_renderer_get_is_expanded (renderer);
  if (is_expanded != data->is_expanded)
    gtk_cell_renderer_set_is_expanded (renderer, data->is_expanded);

  /* Apply the attributes directly to the renderer */
  for (list = info->attributes; list; list = list->next)
    {
      attribute = list->data;

      gtk_tree_model_get_value (data->model, data->iter, attribute->column, &value);
      g_object_set_property (G_OBJECT (renderer), attribute->attribute, &value);
      g_value_unset (&value);
    }

  /* Call any GtkCellLayoutDataFunc that may have been set by the user
   */
  if (info->func)
    info->func (info->proxy ? info->proxy : GTK_CELL_LAYOUT (data->area), renderer,
		data->model, data->iter, info->data);

  g_object_thaw_notify (G_OBJECT (renderer));
}

static void
gtk_cell_area_real_apply_attributes (GtkCellArea           *area,
                                     GtkTreeModel          *tree_model,
                                     GtkTreeIter           *iter,
                                     gboolean               is_expander,
                                     gboolean               is_expanded)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  AttributeData       data;
  GtkTreePath        *path;

  /* Feed in data needed to apply to every renderer */
  data.area        = area;
  data.model       = tree_model;
  data.iter        = iter;
  data.is_expander = is_expander;
  data.is_expanded = is_expanded;

  /* Go over any cells that have attributes or custom GtkCellLayoutDataFuncs and
   * apply the data from the treemodel */
  g_hash_table_foreach (priv->cell_info, (GHFunc)apply_cell_attributes, &data);

  /* Update the currently applied path */
  g_free (priv->current_path);
  path               = gtk_tree_model_get_path (tree_model, iter);
  priv->current_path = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);
}

static GtkCellAreaContext *
gtk_cell_area_real_create_context (GtkCellArea *area)
{
  g_warning ("GtkCellAreaClass::create_context not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (area)));

  return NULL;
}

static GtkCellAreaContext *
gtk_cell_area_real_copy_context (GtkCellArea        *area,
				 GtkCellAreaContext *context)
{
  g_warning ("GtkCellAreaClass::copy_context not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (area)));

  return NULL;
}

static GtkSizeRequestMode
gtk_cell_area_real_get_request_mode (GtkCellArea *area)
{
  /* By default cell areas are height-for-width. */
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_cell_area_real_get_preferred_width (GtkCellArea        *area,
					GtkCellAreaContext *context,
					GtkWidget          *widget,
					int                *minimum_width,
					int                *natural_width)
{
  g_warning ("GtkCellAreaClass::get_preferred_width not implemented for '%s'",
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static void
gtk_cell_area_real_get_preferred_height (GtkCellArea        *area,
					 GtkCellAreaContext *context,
					 GtkWidget          *widget,
					 int                *minimum_height,
					 int                *natural_height)
{
  g_warning ("GtkCellAreaClass::get_preferred_height not implemented for '%s'",
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static void
gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea        *area,
                                                   GtkCellAreaContext *context,
                                                   GtkWidget          *widget,
						   int                 width,
                                                   int                *minimum_height,
                                                   int                *natural_height)
{
  /* If the area doesn't do height-for-width, fallback on base preferred height */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_height (area, context, widget, minimum_height, natural_height);
}

static void
gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea        *area,
                                                   GtkCellAreaContext *context,
                                                   GtkWidget          *widget,
                                                   int                 height,
                                                   int                *minimum_width,
                                                   int                *natural_width)
{
  /* If the area doesn't do width-for-height, fallback on base preferred width */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, context, widget, minimum_width, natural_width);
}

static gboolean
get_is_activatable (GtkCellRenderer *renderer,
                    gboolean        *activatable)
{

  if (gtk_cell_renderer_is_activatable (renderer))
    *activatable = TRUE;

  return *activatable;
}

static gboolean
gtk_cell_area_real_is_activatable (GtkCellArea *area)
{
  gboolean activatable = FALSE;

  /* Checks if any renderer can focus for the currently applied
   * attributes.
   *
   * Subclasses can override this in the case that they are also
   * rendering widgets as well as renderers.
   */
  gtk_cell_area_foreach (area, (GtkCellCallback)get_is_activatable, &activatable);

  return activatable;
}

static gboolean
gtk_cell_area_real_activate (GtkCellArea         *area,
                             GtkCellAreaContext  *context,
                             GtkWidget           *widget,
                             const GdkRectangle  *cell_area,
                             GtkCellRendererState flags,
                             gboolean             edit_only)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GdkRectangle        renderer_area;
  GtkCellRenderer    *activate_cell = NULL;
  GtkCellRendererMode mode;

  if (priv->focus_cell)
    {
      g_object_get (priv->focus_cell, "mode", &mode, NULL);

      if (gtk_cell_renderer_get_visible (priv->focus_cell) &&
          (edit_only ? mode == GTK_CELL_RENDERER_MODE_EDITABLE :
           mode != GTK_CELL_RENDERER_MODE_INERT))
        activate_cell = priv->focus_cell;
    }
  else
    {
      GList *cells, *l;

      /* GtkTreeView sometimes wants to activate a cell when no
       * cells are in focus.
       */
      cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));
      for (l = cells; l && !activate_cell; l = l->next)
        {
          GtkCellRenderer *renderer = l->data;

          g_object_get (renderer, "mode", &mode, NULL);

          if (gtk_cell_renderer_get_visible (renderer) &&
              (edit_only ? mode == GTK_CELL_RENDERER_MODE_EDITABLE :
               mode != GTK_CELL_RENDERER_MODE_INERT))
            activate_cell = renderer;
        }
      g_list_free (cells);
    }

  if (activate_cell)
    {
      /* Get the allocation of the focused cell.
       */
      gtk_cell_area_get_cell_allocation (area, context, widget, activate_cell,
                                         cell_area, &renderer_area);

      /* Activate or Edit the cell
       *
       * Currently just not sending an event, renderers afaics dont use
       * the event argument anyway, worst case is we can synthesize one.
       */
      if (gtk_cell_area_activate_cell (area, widget, activate_cell, NULL,
                                       &renderer_area, flags))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_cell_area_real_focus (GtkCellArea           *area,
			  GtkDirectionType       direction)
{
  g_warning ("GtkCellAreaClass::focus not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (area)));
  return FALSE;
}

/*************************************************************
 *                   GtkCellLayoutIface                      *
 *************************************************************/
static void
gtk_cell_area_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start         = gtk_cell_area_pack_default;
  iface->pack_end           = gtk_cell_area_pack_default;
  iface->clear              = gtk_cell_area_clear;
  iface->add_attribute      = gtk_cell_area_add_attribute;
  iface->set_cell_data_func = gtk_cell_area_set_cell_data_func;
  iface->clear_attributes   = gtk_cell_area_clear_attributes;
  iface->reorder            = gtk_cell_area_reorder;
  iface->get_cells          = gtk_cell_area_get_cells;
  iface->get_area           = gtk_cell_area_get_area;
}

static void
gtk_cell_area_pack_default (GtkCellLayout         *cell_layout,
                            GtkCellRenderer       *renderer,
                            gboolean               expand)
{
  gtk_cell_area_add (GTK_CELL_AREA (cell_layout), renderer);
}

static void
gtk_cell_area_clear (GtkCellLayout *cell_layout)
{
  GtkCellArea *area = GTK_CELL_AREA (cell_layout);
  GList *l, *cells  =
    gtk_cell_layout_get_cells (cell_layout);

  for (l = cells; l; l = l->next)
    {
      GtkCellRenderer *renderer = l->data;
      gtk_cell_area_remove (area, renderer);
    }

  g_list_free (cells);
}

static void
gtk_cell_area_add_attribute (GtkCellLayout         *cell_layout,
                             GtkCellRenderer       *renderer,
                             const char            *attribute,
                             int                    column)
{
  gtk_cell_area_attribute_connect (GTK_CELL_AREA (cell_layout),
                                   renderer, attribute, column);
}

static void
gtk_cell_area_set_cell_data_func (GtkCellLayout         *cell_layout,
                                  GtkCellRenderer       *renderer,
                                  GtkCellLayoutDataFunc  func,
                                  gpointer               func_data,
                                  GDestroyNotify         destroy)
{
  GtkCellArea *area   = GTK_CELL_AREA (cell_layout);

  _gtk_cell_area_set_cell_data_func_with_proxy (area, renderer, (GFunc)func, func_data, destroy, NULL);
}

static void
gtk_cell_area_clear_attributes (GtkCellLayout         *cell_layout,
                                GtkCellRenderer       *renderer)
{
  GtkCellArea        *area = GTK_CELL_AREA (cell_layout);
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  CellInfo           *info;

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      g_slist_free_full (info->attributes, (GDestroyNotify)cell_attribute_free);
      info->attributes = NULL;
    }
}

static void
gtk_cell_area_reorder (GtkCellLayout   *cell_layout,
                       GtkCellRenderer *cell,
                       int              position)
{
  g_warning ("GtkCellLayout::reorder not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (cell_layout)));
}

static gboolean
accum_cells (GtkCellRenderer *renderer,
             GList          **accum)
{
  *accum = g_list_prepend (*accum, renderer);

  return FALSE;
}

static GList *
gtk_cell_area_get_cells (GtkCellLayout *cell_layout)
{
  GList *cells = NULL;

  gtk_cell_area_foreach (GTK_CELL_AREA (cell_layout),
                         (GtkCellCallback)accum_cells,
                         &cells);

  return g_list_reverse (cells);
}

static GtkCellArea *
gtk_cell_area_get_area (GtkCellLayout *cell_layout)
{
  return GTK_CELL_AREA (cell_layout);
}

/*************************************************************
 *                   GtkBuildableIface                       *
 *************************************************************/
static void
gtk_cell_area_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = _gtk_cell_layout_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_cell_area_buildable_custom_tag_end;
}

static void
gtk_cell_area_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const char   *tagname,
                                        gpointer      data)
{
  /* Just ignore the boolean return from here */
  _gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data);
}

/*************************************************************
 *                            API                            *
 *************************************************************/

/**
 * gtk_cell_area_add:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to add to @area
 *
 * Adds @renderer to @area with the default child cell properties.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_add (GtkCellArea        *area,
                   GtkCellRenderer    *renderer)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  GTK_CELL_AREA_GET_CLASS (area)->add (area, renderer);
}

/**
 * gtk_cell_area_remove:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to remove from @area
 *
 * Removes @renderer from @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_remove (GtkCellArea        *area,
                      GtkCellRenderer    *renderer)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GList              *renderers, *l;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  /* Remove any custom attributes and custom cell data func here first */
  g_hash_table_remove (priv->cell_info, renderer);

  /* Remove focus siblings of this renderer */
  g_hash_table_remove (priv->focus_siblings, renderer);

  /* Remove this renderer from any focus renderer's sibling list */
  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));

  for (l = renderers; l; l = l->next)
    {
      GtkCellRenderer *focus_renderer = l->data;

      if (gtk_cell_area_is_focus_sibling (area, focus_renderer, renderer))
        {
          gtk_cell_area_remove_focus_sibling (area, focus_renderer, renderer);
          break;
        }
    }

  g_list_free (renderers);

  GTK_CELL_AREA_GET_CLASS (area)->remove (area, renderer);
}

static gboolean
get_has_renderer (GtkCellRenderer  *renderer,
                  HasRendererCheck *check)
{
  if (renderer == check->renderer)
    check->has_renderer = TRUE;

  return check->has_renderer;
}

/**
 * gtk_cell_area_has_renderer:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to check
 *
 * Checks if @area contains @renderer.
 *
 * Returns: %TRUE if @renderer is in the @area.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_has_renderer (GtkCellArea     *area,
                            GtkCellRenderer *renderer)
{
  HasRendererCheck check = { renderer, FALSE };

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), FALSE);

  gtk_cell_area_foreach (area, (GtkCellCallback)get_has_renderer, &check);

  return check.has_renderer;
}

/**
 * gtk_cell_area_foreach:
 * @area: a `GtkCellArea`
 * @callback: (scope call): the `GtkCellCallback` to call
 * @callback_data: user provided data pointer
 *
 * Calls @callback for every `GtkCellRenderer` in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_foreach (GtkCellArea        *area,
                       GtkCellCallback     callback,
                       gpointer            callback_data)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (callback != NULL);

  GTK_CELL_AREA_GET_CLASS (area)->foreach (area, callback, callback_data);
}

/**
 * gtk_cell_area_foreach_alloc:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context for this row of data.
 * @widget: the `GtkWidget` that @area is rendering to
 * @cell_area: the @widget relative coordinates and size for @area
 * @background_area: the @widget relative coordinates of the background area
 * @callback: (scope call): the `GtkCellAllocCallback` to call
 * @callback_data: user provided data pointer
 *
 * Calls @callback for every `GtkCellRenderer` in @area with the
 * allocated rectangle inside @cell_area.
 */
void
gtk_cell_area_foreach_alloc (GtkCellArea          *area,
                             GtkCellAreaContext   *context,
                             GtkWidget            *widget,
                             const GdkRectangle   *cell_area,
                             const GdkRectangle   *background_area,
                             GtkCellAllocCallback  callback,
                             gpointer              callback_data)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (callback != NULL);

  GTK_CELL_AREA_GET_CLASS (area)->foreach_alloc (area, context, widget,
						 cell_area, background_area,
						 callback, callback_data);
}

/**
 * gtk_cell_area_event:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context for this row of data.
 * @widget: the `GtkWidget` that @area is rendering to
 * @event: the `GdkEvent` to handle
 * @cell_area: the @widget relative coordinates for @area
 * @flags: the `GtkCellRenderer`State for @area in this row.
 *
 * Delegates event handling to a `GtkCellArea`.
 *
 * Returns: %TRUE if the event was handled by @area.
 *
 * Deprecated: 4.10
 */
int
gtk_cell_area_event (GtkCellArea          *area,
                     GtkCellAreaContext   *context,
                     GtkWidget            *widget,
                     GdkEvent             *event,
                     const GdkRectangle   *cell_area,
                     GtkCellRendererState  flags)
{
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_CELL_AREA_CONTEXT (context), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->event)
    return class->event (area, context, widget, event, cell_area, flags);

  g_warning ("GtkCellAreaClass::event not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (area)));
  return 0;
}

/**
 * gtk_cell_area_snapshot:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context for this row of data.
 * @widget: the `GtkWidget` that @area is rendering to
 * @snapshot: the `GtkSnapshot` to draw to
 * @background_area: the @widget relative coordinates for @area’s background
 * @cell_area: the @widget relative coordinates for @area
 * @flags: the `GtkCellRenderer`State for @area in this row.
 * @paint_focus: whether @area should paint focus on focused cells for focused rows or not.
 *
 * Snapshots @area’s cells according to @area’s layout onto at
 * the given coordinates.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_snapshot (GtkCellArea          *area,
                        GtkCellAreaContext   *context,
                        GtkWidget            *widget,
                        GtkSnapshot          *snapshot,
                        const GdkRectangle   *background_area,
                        const GdkRectangle   *cell_area,
                        GtkCellRendererState  flags,
                        gboolean              paint_focus)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (background_area != NULL);
  g_return_if_fail (cell_area != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->snapshot)
    class->snapshot (area, context, widget, snapshot, background_area, cell_area, flags, paint_focus);
  else
    g_warning ("GtkCellAreaClass::snapshot not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

static gboolean
get_cell_allocation (GtkCellRenderer        *renderer,
                     const GdkRectangle     *cell_area,
                     const GdkRectangle     *cell_background,
                     RendererAllocationData *data)
{
  if (data->renderer == renderer)
    data->allocation = *cell_area;

  return (data->renderer == renderer);
}

/**
 * gtk_cell_area_get_cell_allocation:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context used to hold sizes for @area.
 * @widget: the `GtkWidget` that @area is rendering on
 * @renderer: the `GtkCellRenderer` to get the allocation for
 * @cell_area: the whole allocated area for @area in @widget
 *             for this row
 * @allocation: (out): where to store the allocation for @renderer
 *
 * Derives the allocation of @renderer inside @area if @area
 * were to be rendered in @cell_area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_get_cell_allocation (GtkCellArea          *area,
                                   GtkCellAreaContext   *context,
                                   GtkWidget            *widget,
                                   GtkCellRenderer      *renderer,
                                   const GdkRectangle   *cell_area,
                                   GdkRectangle         *allocation)
{
  RendererAllocationData data = { renderer, { 0, } };

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (allocation != NULL);

  gtk_cell_area_foreach_alloc (area, context, widget, cell_area, cell_area,
                               (GtkCellAllocCallback)get_cell_allocation, &data);

  *allocation = data.allocation;
}

static gboolean
get_cell_by_position (GtkCellRenderer     *renderer,
                      const GdkRectangle  *cell_area,
                      const GdkRectangle  *cell_background,
                      CellByPositionData  *data)
{
  if (data->x >= cell_area->x && data->x < cell_area->x + cell_area->width &&
      data->y >= cell_area->y && data->y < cell_area->y + cell_area->height)
    {
      data->renderer  = renderer;
      data->cell_area = *cell_area;
    }

  return (data->renderer != NULL);
}

/**
 * gtk_cell_area_get_cell_at_position:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context used to hold sizes for @area.
 * @widget: the `GtkWidget` that @area is rendering on
 * @cell_area: the whole allocated area for @area in @widget
 *   for this row
 * @x: the x position
 * @y: the y position
 * @alloc_area: (out) (optional): where to store the inner allocated area of the
 *   returned cell renderer
 *
 * Gets the `GtkCellRenderer` at @x and @y coordinates inside @area and optionally
 * returns the full cell allocation for it inside @cell_area.
 *
 * Returns: (transfer none): the `GtkCellRenderer` at @x and @y.
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_area_get_cell_at_position (GtkCellArea          *area,
                                    GtkCellAreaContext   *context,
                                    GtkWidget            *widget,
                                    const GdkRectangle   *cell_area,
                                    int                   x,
                                    int                   y,
                                    GdkRectangle         *alloc_area)
{
  CellByPositionData data = { x, y, NULL, { 0, } };

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);
  g_return_val_if_fail (GTK_IS_CELL_AREA_CONTEXT (context), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (cell_area != NULL, NULL);
  g_return_val_if_fail (x >= cell_area->x && x <= cell_area->x + cell_area->width, NULL);
  g_return_val_if_fail (y >= cell_area->y && y <= cell_area->y + cell_area->height, NULL);

  gtk_cell_area_foreach_alloc (area, context, widget, cell_area, cell_area,
                               (GtkCellAllocCallback)get_cell_by_position, &data);

  if (alloc_area)
    *alloc_area = data.cell_area;

  return data.renderer;
}

/*************************************************************
 *                      API: Geometry                        *
 *************************************************************/
/**
 * gtk_cell_area_create_context:
 * @area: a `GtkCellArea`
 *
 * Creates a `GtkCellArea`Context to be used with @area for
 * all purposes. `GtkCellArea`Context stores geometry information
 * for rows for which it was operated on, it is important to use
 * the same context for the same row of data at all times (i.e.
 * one should render and handle events with the same `GtkCellArea`Context
 * which was used to request the size of those rows of data).
 *
 * Returns: (transfer full): a newly created `GtkCellArea`Context which can be used with @area.
 *
 * Deprecated: 4.10
 */
GtkCellAreaContext *
gtk_cell_area_create_context (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  return GTK_CELL_AREA_GET_CLASS (area)->create_context (area);
}

/**
 * gtk_cell_area_copy_context:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context to copy
 *
 * This is sometimes needed for cases where rows need to share
 * alignments in one orientation but may be separately grouped
 * in the opposing orientation.
 *
 * For instance, `GtkIconView` creates all icons (rows) to have
 * the same width and the cells theirin to have the same
 * horizontal alignments. However each row of icons may have
 * a separate collective height. `GtkIconView` uses this to
 * request the heights of each row based on a context which
 * was already used to request all the row widths that are
 * to be displayed.
 *
 * Returns: (transfer full): a newly created `GtkCellArea`Context copy of @context.
 *
 * Deprecated: 4.10
 */
GtkCellAreaContext *
gtk_cell_area_copy_context (GtkCellArea        *area,
                            GtkCellAreaContext *context)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);
  g_return_val_if_fail (GTK_IS_CELL_AREA_CONTEXT (context), NULL);

  return GTK_CELL_AREA_GET_CLASS (area)->copy_context (area, context);
}

/**
 * gtk_cell_area_get_request_mode:
 * @area: a `GtkCellArea`
 *
 * Gets whether the area prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * Returns: The `GtkSizeRequestMode` preferred by @area.
 */
GtkSizeRequestMode
gtk_cell_area_get_request_mode (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area),
                        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);

  return GTK_CELL_AREA_GET_CLASS (area)->get_request_mode (area);
}

/**
 * gtk_cell_area_get_preferred_width:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context to perform this request with
 * @widget: the `GtkWidget` where @area will be rendering
 * @minimum_width: (out) (optional): location to store the minimum width
 * @natural_width: (out) (optional): location to store the natural width
 *
 * Retrieves a cell area’s initial minimum and natural width.
 *
 * @area will store some geometrical information in @context along the way;
 * when requesting sizes over an arbitrary number of rows, it’s not important
 * to check the @minimum_width and @natural_width of this call but rather to
 * consult gtk_cell_area_context_get_preferred_width() after a series of
 * requests.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_get_preferred_width (GtkCellArea        *area,
                                   GtkCellAreaContext *context,
                                   GtkWidget          *widget,
                                   int                *minimum_width,
                                   int                *natural_width)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, context, widget,
						       minimum_width, natural_width);
}

/**
 * gtk_cell_area_get_preferred_height_for_width:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context which has already been requested for widths.
 * @widget: the `GtkWidget` where @area will be rendering
 * @width: the width for which to check the height of this area
 * @minimum_height: (out) (optional): location to store the minimum height
 * @natural_height: (out) (optional): location to store the natural height
 *
 * Retrieves a cell area’s minimum and natural height if it would be given
 * the specified @width.
 *
 * @area stores some geometrical information in @context along the way
 * while calling gtk_cell_area_get_preferred_width(). It’s important to
 * perform a series of gtk_cell_area_get_preferred_width() requests with
 * @context first and then call gtk_cell_area_get_preferred_height_for_width()
 * on each cell area individually to get the height for width of each
 * fully requested row.
 *
 * If at some point, the width of a single row changes, it should be
 * requested with gtk_cell_area_get_preferred_width() again and then
 * the full width of the requested rows checked again with
 * gtk_cell_area_context_get_preferred_width().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
                                              GtkCellAreaContext *context,
                                              GtkWidget          *widget,
                                              int                 width,
                                              int                *minimum_height,
                                              int                *natural_height)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_height_for_width (area, context, widget, width, minimum_height, natural_height);
}


/**
 * gtk_cell_area_get_preferred_height:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context to perform this request with
 * @widget: the `GtkWidget` where @area will be rendering
 * @minimum_height: (out) (optional): location to store the minimum height
 * @natural_height: (out) (optional): location to store the natural height
 *
 * Retrieves a cell area’s initial minimum and natural height.
 *
 * @area will store some geometrical information in @context along the way;
 * when requesting sizes over an arbitrary number of rows, it’s not important
 * to check the @minimum_height and @natural_height of this call but rather to
 * consult gtk_cell_area_context_get_preferred_height() after a series of
 * requests.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_get_preferred_height (GtkCellArea        *area,
                                    GtkCellAreaContext *context,
                                    GtkWidget          *widget,
                                    int                *minimum_height,
                                    int                *natural_height)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_height (area, context, widget,
							minimum_height, natural_height);
}

/**
 * gtk_cell_area_get_preferred_width_for_height:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context which has already been requested for widths.
 * @widget: the `GtkWidget` where @area will be rendering
 * @height: the height for which to check the width of this area
 * @minimum_width: (out) (optional): location to store the minimum width
 * @natural_width: (out) (optional): location to store the natural width
 *
 * Retrieves a cell area’s minimum and natural width if it would be given
 * the specified @height.
 *
 * @area stores some geometrical information in @context along the way
 * while calling gtk_cell_area_get_preferred_height(). It’s important to
 * perform a series of gtk_cell_area_get_preferred_height() requests with
 * @context first and then call gtk_cell_area_get_preferred_width_for_height()
 * on each cell area individually to get the height for width of each
 * fully requested row.
 *
 * If at some point, the height of a single row changes, it should be
 * requested with gtk_cell_area_get_preferred_height() again and then
 * the full height of the requested rows checked again with
 * gtk_cell_area_context_get_preferred_height().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
                                              GtkCellAreaContext *context,
                                              GtkWidget          *widget,
                                              int                 height,
                                              int                *minimum_width,
                                              int                *natural_width)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_width_for_height (area, context, widget, height, minimum_width, natural_width);
}

/*************************************************************
 *                      API: Attributes                      *
 *************************************************************/

/**
 * gtk_cell_area_attribute_connect:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to connect an attribute for
 * @attribute: the attribute name
 * @column: the `GtkTreeModel` column to fetch attribute values from
 *
 * Connects an @attribute to apply values from @column for the
 * `GtkTreeModel` in use.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_attribute_connect (GtkCellArea        *area,
                                 GtkCellRenderer    *renderer,
                                 const char         *attribute,
                                 int                 column)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  CellInfo           *info;
  CellAttribute      *cell_attribute;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (gtk_cell_area_has_renderer (area, renderer));

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (!info)
    {
      info = cell_info_new (NULL, NULL, NULL);

      g_hash_table_insert (priv->cell_info, renderer, info);
    }
  else
    {
      GSList *node;

      /* Check we are not adding the same attribute twice */
      if ((node = g_slist_find_custom (info->attributes, attribute,
                                       (GCompareFunc)cell_attribute_find)) != NULL)
        {
          cell_attribute = node->data;

          g_warning ("Cannot connect attribute '%s' for cell renderer class '%s' "
                     "since '%s' is already attributed to column %d",
                     attribute,
                     G_OBJECT_TYPE_NAME (renderer),
                     attribute, cell_attribute->column);
          return;
        }
    }

  cell_attribute = cell_attribute_new (renderer, attribute, column);

  if (!cell_attribute)
    {
      g_warning ("Cannot connect attribute '%s' for cell renderer class '%s' "
                 "since attribute does not exist",
                 attribute,
                 G_OBJECT_TYPE_NAME (renderer));
      return;
    }

  info->attributes = g_slist_prepend (info->attributes, cell_attribute);
}

/**
 * gtk_cell_area_attribute_disconnect:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to disconnect an attribute for
 * @attribute: the attribute name
 *
 * Disconnects @attribute for the @renderer in @area so that
 * attribute will no longer be updated with values from the
 * model.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_attribute_disconnect (GtkCellArea        *area,
                                    GtkCellRenderer    *renderer,
                                    const char         *attribute)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  CellInfo           *info;
  CellAttribute      *cell_attribute;
  GSList             *node;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (gtk_cell_area_has_renderer (area, renderer));

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      node = g_slist_find_custom (info->attributes, attribute,
                                  (GCompareFunc)cell_attribute_find);
      if (node)
        {
          cell_attribute = node->data;

          cell_attribute_free (cell_attribute);

          info->attributes = g_slist_delete_link (info->attributes, node);
        }
    }
}

/**
 * gtk_cell_area_attribute_get_column:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer`
 * @attribute: an attribute on the renderer
 *
 * Returns the model column that an attribute has been mapped to,
 * or -1 if the attribute is not mapped.
 *
 * Returns: the model column, or -1
 *
 * Deprecated: 4.10
 */
int
gtk_cell_area_attribute_get_column (GtkCellArea     *area,
                                    GtkCellRenderer *renderer,
                                    const char      *attribute)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  CellInfo           *info;
  CellAttribute      *cell_attribute;
  GSList             *node;

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      node = g_slist_find_custom (info->attributes, attribute,
                                  (GCompareFunc)cell_attribute_find);
      if (node)
        {
          cell_attribute = node->data;
          return cell_attribute->column;
        }
    }

  return -1;
}

/**
 * gtk_cell_area_apply_attributes:
 * @area: a `GtkCellArea`
 * @tree_model: the `GtkTreeModel` to pull values from
 * @iter: the `GtkTreeIter` in @tree_model to apply values for
 * @is_expander: whether @iter has children
 * @is_expanded: whether @iter is expanded in the view and
 *               children are visible
 *
 * Applies any connected attributes to the renderers in
 * @area by pulling the values from @tree_model.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_apply_attributes (GtkCellArea  *area,
                                GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                gboolean      is_expander,
                                gboolean      is_expanded)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  g_signal_emit (area, cell_area_signals[SIGNAL_APPLY_ATTRIBUTES], 0,
                 tree_model, iter, is_expander, is_expanded);
}

/**
 * gtk_cell_area_get_current_path_string:
 * @area: a `GtkCellArea`
 *
 * Gets the current `GtkTreePath` string for the currently
 * applied `GtkTreeIter`, this is implicitly updated when
 * gtk_cell_area_apply_attributes() is called and can be
 * used to interact with renderers from `GtkCellArea`
 * subclasses.
 *
 * Returns: The current `GtkTreePath` string for the current
 * attributes applied to @area. This string belongs to the area and
 * should not be freed.
 */
const char *
gtk_cell_area_get_current_path_string (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  return priv->current_path;
}


/*************************************************************
 *                    API: Cell Properties                   *
 *************************************************************/
/**
 * gtk_cell_area_class_install_cell_property:
 * @aclass: a `GtkCellAreaClass`
 * @property_id: the id for the property
 * @pspec: the `GParamSpec` for the property
 *
 * Installs a cell property on a cell area class.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_class_install_cell_property (GtkCellAreaClass   *aclass,
                                           guint               property_id,
                                           GParamSpec         *pspec)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CLASS (aclass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (aclass->set_cell_property != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (aclass->get_cell_property != NULL);
  g_return_if_fail (property_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
  g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

  if (g_param_spec_pool_lookup (cell_property_pool, pspec->name, G_OBJECT_CLASS_TYPE (aclass), TRUE))
    {
      g_warning (G_STRLOC ": class '%s' already contains a cell property named '%s'",
                 G_OBJECT_CLASS_NAME (aclass), pspec->name);
      return;
    }
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (cell_property_pool, pspec, G_OBJECT_CLASS_TYPE (aclass));
}

/**
 * gtk_cell_area_class_find_cell_property:
 * @aclass: a `GtkCellAreaClass`
 * @property_name: the name of the child property to find
 *
 * Finds a cell property of a cell area class by name.
 *
 * Returns: (transfer none): the `GParamSpec` of the child property
 *
 * Deprecated: 4.10
 */
GParamSpec*
gtk_cell_area_class_find_cell_property (GtkCellAreaClass   *aclass,
                                        const char         *property_name)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA_CLASS (aclass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (cell_property_pool,
                                   property_name,
                                   G_OBJECT_CLASS_TYPE (aclass),
                                   TRUE);
}

/**
 * gtk_cell_area_class_list_cell_properties:
 * @aclass: a `GtkCellAreaClass`
 * @n_properties: (out): location to return the number of cell properties found
 *
 * Returns all cell properties of a cell area class.
 *
 * Returns: (array length=n_properties) (transfer container): a newly
 *     allocated %NULL-terminated array of `GParamSpec`*.  The array
 *     must be freed with g_free().
 *
 * Deprecated: 4.10
 */
GParamSpec**
gtk_cell_area_class_list_cell_properties (GtkCellAreaClass  *aclass,
                                          guint             *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (GTK_IS_CELL_AREA_CLASS (aclass), NULL);

  pspecs = g_param_spec_pool_list (cell_property_pool,
                                   G_OBJECT_CLASS_TYPE (aclass),
                                   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

/**
 * gtk_cell_area_add_with_properties:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` to be placed inside @area
 * @first_prop_name: the name of the first cell property to set
 * @...: a %NULL-terminated list of property names and values, starting
 *     with @first_prop_name
 *
 * Adds @renderer to @area, setting cell properties at the same time.
 * See gtk_cell_area_add() and gtk_cell_area_cell_set() for more details.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_add_with_properties (GtkCellArea        *area,
                                   GtkCellRenderer    *renderer,
                                   const char         *first_prop_name,
                                   ...)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->add)
    {
      va_list var_args;

      class->add (area, renderer);

      va_start (var_args, first_prop_name);
      gtk_cell_area_cell_set_valist (area, renderer, first_prop_name, var_args);
      va_end (var_args);
    }
  else
    g_warning ("GtkCellAreaClass::add not implemented for '%s'",
               g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

/**
 * gtk_cell_area_cell_set:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` which is a cell inside @area
 * @first_prop_name: the name of the first cell property to set
 * @...: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 *
 * Sets one or more cell properties for @cell in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_set (GtkCellArea        *area,
                        GtkCellRenderer    *renderer,
                        const char         *first_prop_name,
                        ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  va_start (var_args, first_prop_name);
  gtk_cell_area_cell_set_valist (area, renderer, first_prop_name, var_args);
  va_end (var_args);
}

/**
 * gtk_cell_area_cell_get:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` which is inside @area
 * @first_prop_name: the name of the first cell property to get
 * @...: return location for the first cell property, followed
 *     optionally by more name/return location pairs, followed by %NULL
 *
 * Gets the values of one or more cell properties for @renderer in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_get (GtkCellArea        *area,
                        GtkCellRenderer    *renderer,
                        const char         *first_prop_name,
                        ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  va_start (var_args, first_prop_name);
  gtk_cell_area_cell_get_valist (area, renderer, first_prop_name, var_args);
  va_end (var_args);
}

static inline void
area_get_cell_property (GtkCellArea     *area,
                        GtkCellRenderer *renderer,
                        GParamSpec      *pspec,
                        GValue          *value)
{
  GtkCellAreaClass *class = g_type_class_peek (pspec->owner_type);

  class->get_cell_property (area, renderer, PARAM_SPEC_PARAM_ID (pspec), value, pspec);
}

static inline void
area_set_cell_property (GtkCellArea     *area,
                        GtkCellRenderer *renderer,
                        GParamSpec      *pspec,
                        const GValue    *value)
{
  GValue tmp_value = G_VALUE_INIT;
  GtkCellAreaClass *class = g_type_class_peek (pspec->owner_type);

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set cell property '%s' of type '%s' from value of type '%s'",
               pspec->name,
               g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
               G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      char *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type '%s' is invalid for property '%s' of type '%s'",
                 contents,
                 G_VALUE_TYPE_NAME (value),
                 pspec->name,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      class->set_cell_property (area, renderer, PARAM_SPEC_PARAM_ID (pspec), &tmp_value, pspec);
    }
  g_value_unset (&tmp_value);
}

/**
 * gtk_cell_area_cell_set_valist:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` which inside @area
 * @first_property_name: the name of the first cell property to set
 * @var_args: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 *
 * Sets one or more cell properties for @renderer in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_set_valist (GtkCellArea        *area,
                               GtkCellRenderer    *renderer,
                               const char         *first_property_name,
                               va_list             var_args)
{
  const char *name;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  name = first_property_name;
  while (name)
    {
      GValue value = G_VALUE_INIT;
      char *error = NULL;
      GParamSpec *pspec =
        g_param_spec_pool_lookup (cell_property_pool, name,
                                  G_OBJECT_TYPE (area), TRUE);
      if (!pspec)
        {
          g_warning ("%s: cell area class '%s' has no cell property named '%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (area), name);
          break;
        }
      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: cell property '%s' of cell area class '%s' is not writable",
                     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
          break;
        }

      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* we purposely leak the value here, it might not be
           * in a sane state if an error condition occurred
           */
          break;
        }
      area_set_cell_property (area, renderer, pspec, &value);
      g_value_unset (&value);
      name = va_arg (var_args, char *);
    }
}

/**
 * gtk_cell_area_cell_get_valist:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` inside @area
 * @first_property_name: the name of the first property to get
 * @var_args: return location for the first property, followed
 *     optionally by more name/return location pairs, followed by %NULL
 *
 * Gets the values of one or more cell properties for @renderer in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_get_valist (GtkCellArea        *area,
                               GtkCellRenderer    *renderer,
                               const char         *first_property_name,
                               va_list             var_args)
{
  const char *name;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  name = first_property_name;
  while (name)
    {
      GValue value = G_VALUE_INIT;
      GParamSpec *pspec;
      char *error;

      pspec = g_param_spec_pool_lookup (cell_property_pool, name,
                                        G_OBJECT_TYPE (area), TRUE);
      if (!pspec)
        {
          g_warning ("%s: cell area class '%s' has no cell property named '%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (area), name);
          break;
        }
      if (!(pspec->flags & G_PARAM_READABLE))
        {
          g_warning ("%s: cell property '%s' of cell area class '%s' is not readable",
                     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      area_get_cell_property (area, renderer, pspec, &value);
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }
      g_value_unset (&value);
      name = va_arg (var_args, char *);
    }
}

/**
 * gtk_cell_area_cell_set_property:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` inside @area
 * @property_name: the name of the cell property to set
 * @value: the value to set the cell property to
 *
 * Sets a cell property for @renderer in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_set_property (GtkCellArea        *area,
                                 GtkCellRenderer    *renderer,
                                 const char         *property_name,
                                 const GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  pspec = g_param_spec_pool_lookup (cell_property_pool, property_name,
                                    G_OBJECT_TYPE (area), TRUE);
  if (!pspec)
    g_warning ("%s: cell area class '%s' has no cell property named '%s'",
               G_STRLOC, G_OBJECT_TYPE_NAME (area), property_name);
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    g_warning ("%s: cell property '%s' of cell area class '%s' is not writable",
               G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
  else
    {
      area_set_cell_property (area, renderer, pspec, value);
    }
}

/**
 * gtk_cell_area_cell_get_property:
 * @area: a `GtkCellArea`
 * @renderer: a `GtkCellRenderer` inside @area
 * @property_name: the name of the property to get
 * @value: a location to return the value
 *
 * Gets the value of a cell property for @renderer in @area.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_cell_get_property (GtkCellArea        *area,
                                 GtkCellRenderer    *renderer,
                                 const char         *property_name,
                                 GValue             *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  pspec = g_param_spec_pool_lookup (cell_property_pool, property_name,
                                    G_OBJECT_TYPE (area), TRUE);
  if (!pspec)
    g_warning ("%s: cell area class '%s' has no cell property named '%s'",
               G_STRLOC, G_OBJECT_TYPE_NAME (area), property_name);
  else if (!(pspec->flags & G_PARAM_READABLE))
    g_warning ("%s: cell property '%s' of cell area class '%s' is not readable",
               G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
  else
    {
      GValue *prop_value, tmp_value = G_VALUE_INIT;

      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
        {
          g_value_reset (value);
          prop_value = value;
        }
      else if (!g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
        {
          g_warning ("can't retrieve cell property '%s' of type '%s' as value of type '%s'",
                     pspec->name,
                     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                     G_VALUE_TYPE_NAME (value));
          return;
        }
      else
        {
          g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
          prop_value = &tmp_value;
        }

      area_get_cell_property (area, renderer, pspec, prop_value);

      if (prop_value != value)
        {
          g_value_transform (prop_value, value);
          g_value_unset (&tmp_value);
        }
    }
}

/*************************************************************
 *                         API: Focus                        *
 *************************************************************/

/**
 * gtk_cell_area_is_activatable:
 * @area: a `GtkCellArea`
 *
 * Returns whether the area can do anything when activated,
 * after applying new attributes to @area.
 *
 * Returns: whether @area can do anything when activated.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_is_activatable (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);

  return GTK_CELL_AREA_GET_CLASS (area)->is_activatable (area);
}

/**
 * gtk_cell_area_focus:
 * @area: a `GtkCellArea`
 * @direction: the `GtkDirectionType`
 *
 * This should be called by the @area’s owning layout widget
 * when focus is to be passed to @area, or moved within @area
 * for a given @direction and row data.
 *
 * Implementing `GtkCellArea` classes should implement this
 * method to receive and navigate focus in its own way particular
 * to how it lays out cells.
 *
 * Returns: %TRUE if focus remains inside @area as a result of this call.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_focus (GtkCellArea      *area,
                     GtkDirectionType  direction)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);

  return GTK_CELL_AREA_GET_CLASS (area)->focus (area, direction);
}

/**
 * gtk_cell_area_activate:
 * @area: a `GtkCellArea`
 * @context: the `GtkCellArea`Context in context with the current row data
 * @widget: the `GtkWidget` that @area is rendering on
 * @cell_area: the size and location of @area relative to @widget’s allocation
 * @flags: the `GtkCellRenderer`State flags for @area for this row of data.
 * @edit_only: if %TRUE then only cell renderers that are %GTK_CELL_RENDERER_MODE_EDITABLE
 *             will be activated.
 *
 * Activates @area, usually by activating the currently focused
 * cell, however some subclasses which embed widgets in the area
 * can also activate a widget if it currently has the focus.
 *
 * Returns: Whether @area was successfully activated.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_activate (GtkCellArea         *area,
                        GtkCellAreaContext  *context,
                        GtkWidget           *widget,
                        const GdkRectangle  *cell_area,
                        GtkCellRendererState flags,
                        gboolean             edit_only)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);

  return GTK_CELL_AREA_GET_CLASS (area)->activate (area, context, widget, cell_area, flags, edit_only);
}


/**
 * gtk_cell_area_set_focus_cell:
 * @area: a `GtkCellArea`
 * @renderer: (nullable): the `GtkCellRenderer` to give focus to
 *
 * Explicitly sets the currently focused cell to @renderer.
 *
 * This is generally called by implementations of
 * `GtkCellAreaClass.focus()` or `GtkCellAreaClass.event()`,
 * however it can also be used to implement functions such
 * as gtk_tree_view_set_cursor_on_cell().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_set_focus_cell (GtkCellArea     *area,
                              GtkCellRenderer *renderer)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (renderer == NULL || GTK_IS_CELL_RENDERER (renderer));

  if (priv->focus_cell != renderer)
    {
      if (priv->focus_cell)
        g_object_unref (priv->focus_cell);

      priv->focus_cell = renderer;

      if (priv->focus_cell)
        g_object_ref (priv->focus_cell);

      g_object_notify (G_OBJECT (area), "focus-cell");
    }

  /* Signal that the current focus renderer for this path changed
   * (it may be that the focus cell did not change, but the row
   * may have changed so we need to signal it) */
  g_signal_emit (area, cell_area_signals[SIGNAL_FOCUS_CHANGED], 0,
                 priv->focus_cell, priv->current_path);

}

/**
 * gtk_cell_area_get_focus_cell:
 * @area: a `GtkCellArea`
 *
 * Retrieves the currently focused cell for @area
 *
 * Returns: (transfer none) (nullable): the currently focused cell in @area.
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_area_get_focus_cell (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  return priv->focus_cell;
}


/*************************************************************
 *                    API: Focus Siblings                    *
 *************************************************************/

/**
 * gtk_cell_area_add_focus_sibling:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` expected to have focus
 * @sibling: the `GtkCellRenderer` to add to @renderer’s focus area
 *
 * Adds @sibling to @renderer’s focusable area, focus will be drawn
 * around @renderer and all of its siblings if @renderer can
 * focus for a given row.
 *
 * Events handled by focus siblings can also activate the given
 * focusable @renderer.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_add_focus_sibling (GtkCellArea     *area,
                                 GtkCellRenderer *renderer,
                                 GtkCellRenderer *sibling)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GList              *siblings;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (GTK_IS_CELL_RENDERER (sibling));
  g_return_if_fail (renderer != sibling);
  g_return_if_fail (gtk_cell_area_has_renderer (area, renderer));
  g_return_if_fail (gtk_cell_area_has_renderer (area, sibling));
  g_return_if_fail (!gtk_cell_area_is_focus_sibling (area, renderer, sibling));

  /* XXX We should also check that sibling is not in any other renderer's sibling
   * list already, a renderer can be sibling of only one focusable renderer
   * at a time.
   */

  siblings = g_hash_table_lookup (priv->focus_siblings, renderer);

  if (siblings)
    {
      G_GNUC_UNUSED GList *unused = g_list_append (siblings, sibling);
    }
  else
    {
      siblings = g_list_append (siblings, sibling);
      g_hash_table_insert (priv->focus_siblings, renderer, siblings);
    }
}

/**
 * gtk_cell_area_remove_focus_sibling:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` expected to have focus
 * @sibling: the `GtkCellRenderer` to remove from @renderer’s focus area
 *
 * Removes @sibling from @renderer’s focus sibling list
 * (see gtk_cell_area_add_focus_sibling()).
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_remove_focus_sibling (GtkCellArea     *area,
                                    GtkCellRenderer *renderer,
                                    GtkCellRenderer *sibling)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GList              *siblings;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (GTK_IS_CELL_RENDERER (sibling));
  g_return_if_fail (gtk_cell_area_is_focus_sibling (area, renderer, sibling));

  siblings = g_hash_table_lookup (priv->focus_siblings, renderer);

  siblings = g_list_copy (siblings);
  siblings = g_list_remove (siblings, sibling);

  if (!siblings)
    g_hash_table_remove (priv->focus_siblings, renderer);
  else
    g_hash_table_insert (priv->focus_siblings, renderer, siblings);
}

/**
 * gtk_cell_area_is_focus_sibling:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` expected to have focus
 * @sibling: the `GtkCellRenderer` to check against @renderer’s sibling list
 *
 * Returns whether @sibling is one of @renderer’s focus siblings
 * (see gtk_cell_area_add_focus_sibling()).
 *
 * Returns: %TRUE if @sibling is a focus sibling of @renderer
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_is_focus_sibling (GtkCellArea     *area,
                                GtkCellRenderer *renderer,
                                GtkCellRenderer *sibling)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GList              *siblings, *l;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (sibling), FALSE);

  siblings = g_hash_table_lookup (priv->focus_siblings, renderer);

  for (l = siblings; l; l = l->next)
    {
      GtkCellRenderer *a_sibling = l->data;

      if (a_sibling == sibling)
        return TRUE;
    }

  return FALSE;
}

/**
 * gtk_cell_area_get_focus_siblings:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` expected to have focus
 *
 * Gets the focus sibling cell renderers for @renderer.
 *
 * Returns: (element-type GtkCellRenderer) (transfer none): A `GList` of `GtkCellRenderer`s.
 *       The returned list is internal and should not be freed.
 *
 * Deprecated: 4.10
 */
const GList *
gtk_cell_area_get_focus_siblings (GtkCellArea     *area,
                                  GtkCellRenderer *renderer)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), NULL);

  return g_hash_table_lookup (priv->focus_siblings, renderer);
}

/**
 * gtk_cell_area_get_focus_from_sibling:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer`
 *
 * Gets the `GtkCellRenderer` which is expected to be focusable
 * for which @renderer is, or may be a sibling.
 *
 * This is handy for `GtkCellArea` subclasses when handling events,
 * after determining the renderer at the event location it can
 * then chose to activate the focus cell for which the event
 * cell may have been a sibling.
 *
 * Returns: (nullable) (transfer none): the `GtkCellRenderer`
 *   for which @renderer is a sibling
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_area_get_focus_from_sibling (GtkCellArea          *area,
                                      GtkCellRenderer      *renderer)
{
  GtkCellRenderer *ret_renderer = NULL;
  GList           *renderers, *l;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), NULL);

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));

  for (l = renderers; l; l = l->next)
    {
      GtkCellRenderer *a_renderer = l->data;
      const GList     *list;

      for (list = gtk_cell_area_get_focus_siblings (area, a_renderer);
           list; list = list->next)
        {
          GtkCellRenderer *sibling_renderer = list->data;

          if (sibling_renderer == renderer)
            {
              ret_renderer = a_renderer;
              break;
            }
        }
    }
  g_list_free (renderers);

  return ret_renderer;
}

/*************************************************************
 *              API: Cell Activation/Editing                 *
 *************************************************************/
static void
gtk_cell_area_add_editable (GtkCellArea        *area,
                            GtkCellRenderer    *renderer,
                            GtkCellEditable    *editable,
                            const GdkRectangle *cell_area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_signal_emit (area, cell_area_signals[SIGNAL_ADD_EDITABLE], 0,
                 renderer, editable, cell_area, priv->current_path);
}

static void
gtk_cell_area_remove_editable  (GtkCellArea        *area,
                                GtkCellRenderer    *renderer,
                                GtkCellEditable    *editable)
{
  g_signal_emit (area, cell_area_signals[SIGNAL_REMOVE_EDITABLE], 0, renderer, editable);
}

static void
cell_area_remove_widget_cb (GtkCellEditable *editable,
                            GtkCellArea     *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_assert (priv->edit_widget == editable);
  g_assert (priv->edited_cell != NULL);

  gtk_cell_area_remove_editable (area, priv->edited_cell, priv->edit_widget);

  /* Now that we're done with editing the widget and it can be removed,
   * remove our references to the widget and disconnect handlers */
  gtk_cell_area_set_edited_cell (area, NULL);
  gtk_cell_area_set_edit_widget (area, NULL);
}

static void
gtk_cell_area_set_edited_cell (GtkCellArea     *area,
                               GtkCellRenderer *renderer)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (renderer == NULL || GTK_IS_CELL_RENDERER (renderer));

  if (priv->edited_cell != renderer)
    {
      if (priv->edited_cell)
        g_object_unref (priv->edited_cell);

      priv->edited_cell = renderer;

      if (priv->edited_cell)
        g_object_ref (priv->edited_cell);

      g_object_notify (G_OBJECT (area), "edited-cell");
    }
}

static void
gtk_cell_area_set_edit_widget (GtkCellArea     *area,
                               GtkCellEditable *editable)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (editable == NULL || GTK_IS_CELL_EDITABLE (editable));

  if (priv->edit_widget != editable)
    {
      if (priv->edit_widget)
        {
          g_signal_handler_disconnect (priv->edit_widget, priv->remove_widget_id);

          g_object_unref (priv->edit_widget);
        }

      priv->edit_widget = editable;

      if (priv->edit_widget)
        {
          priv->remove_widget_id =
            g_signal_connect (priv->edit_widget, "remove-widget",
                              G_CALLBACK (cell_area_remove_widget_cb), area);

          g_object_ref (priv->edit_widget);
        }

      g_object_notify (G_OBJECT (area), "edit-widget");
    }
}

/**
 * gtk_cell_area_get_edited_cell:
 * @area: a `GtkCellArea`
 *
 * Gets the `GtkCellRenderer` in @area that is currently
 * being edited.
 *
 * Returns: (transfer none) (nullable): The currently edited `GtkCellRenderer`
 *
 * Deprecated: 4.10
 */
GtkCellRenderer   *
gtk_cell_area_get_edited_cell (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  return priv->edited_cell;
}

/**
 * gtk_cell_area_get_edit_widget:
 * @area: a `GtkCellArea`
 *
 * Gets the `GtkCellEditable` widget currently used
 * to edit the currently edited cell.
 *
 * Returns: (transfer none) (nullable): The currently active `GtkCellEditable` widget
 *
 * Deprecated: 4.10
 */
GtkCellEditable *
gtk_cell_area_get_edit_widget (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  return priv->edit_widget;
}

/**
 * gtk_cell_area_activate_cell:
 * @area: a `GtkCellArea`
 * @widget: the `GtkWidget` that @area is rendering onto
 * @renderer: the `GtkCellRenderer` in @area to activate
 * @event: the `GdkEvent` for which cell activation should occur
 * @cell_area: the `GdkRectangle` in @widget relative coordinates
 *             of @renderer for the current row.
 * @flags: the `GtkCellRenderer`State for @renderer
 *
 * This is used by `GtkCellArea` subclasses when handling events
 * to activate cells, the base `GtkCellArea` class activates cells
 * for keyboard events for free in its own GtkCellArea->activate()
 * implementation.
 *
 * Returns: whether cell activation was successful
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_area_activate_cell (GtkCellArea          *area,
                             GtkWidget            *widget,
                             GtkCellRenderer      *renderer,
                             GdkEvent             *event,
                             const GdkRectangle   *cell_area,
                             GtkCellRendererState  flags)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  GtkCellRendererMode mode;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), FALSE);
  g_return_val_if_fail (cell_area != NULL, FALSE);

  if (!gtk_cell_renderer_get_sensitive (renderer))
    return FALSE;

  g_object_get (renderer, "mode", &mode, NULL);

  if (mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE)
    {
      if (gtk_cell_renderer_activate (renderer,
                                      event, widget,
                                      priv->current_path,
                                      cell_area,
                                      cell_area,
                                      flags))
        return TRUE;
    }
  else if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
    {
      GtkCellEditable *editable_widget;
      GdkRectangle inner_area;

      gtk_cell_area_inner_cell_area (area, widget, cell_area, &inner_area);

      editable_widget =
        gtk_cell_renderer_start_editing (renderer,
                                         event, widget,
                                         priv->current_path,
                                         &inner_area,
                                         &inner_area,
                                         flags);

      if (editable_widget != NULL)
        {
          g_return_val_if_fail (GTK_IS_CELL_EDITABLE (editable_widget), FALSE);

          gtk_cell_area_set_edited_cell (area, renderer);
          gtk_cell_area_set_edit_widget (area, editable_widget);

          /* Signal that editing started so that callers can get
           * a handle on the editable_widget */
          gtk_cell_area_add_editable (area, priv->focus_cell, editable_widget, cell_area);

          /* If the signal was successfully handled start the editing */
          if (gtk_widget_get_parent (GTK_WIDGET (editable_widget)))
            {
              gtk_cell_editable_start_editing (editable_widget, event);
              gtk_widget_grab_focus (GTK_WIDGET (editable_widget));
            }
          else
            {
              /* Otherwise clear the editing state and fire a warning */
              gtk_cell_area_set_edited_cell (area, NULL);
              gtk_cell_area_set_edit_widget (area, NULL);

              g_warning ("GtkCellArea::add-editable fired in the dark, no cell editing was started.");
            }

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gtk_cell_area_stop_editing:
 * @area: a `GtkCellArea`
 * @canceled: whether editing was canceled.
 *
 * Explicitly stops the editing of the currently edited cell.
 *
 * If @canceled is %TRUE, the currently edited cell renderer
 * will emit the ::editing-canceled signal, otherwise the
 * the ::editing-done signal will be emitted on the current
 * edit widget.
 *
 * See gtk_cell_area_get_edited_cell() and gtk_cell_area_get_edit_widget().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_stop_editing (GtkCellArea *area,
                            gboolean     canceled)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  if (priv->edited_cell)
    {
      GtkCellEditable *edit_widget = g_object_ref (priv->edit_widget);
      GtkCellRenderer *edit_cell   = g_object_ref (priv->edited_cell);

      /* Stop editing of the cell renderer */
      gtk_cell_renderer_stop_editing (priv->edited_cell, canceled);

      /* When editing is explicitly halted either
       * the "editing-canceled" signal is emitted on the cell
       * renderer or the "editing-done" signal on the GtkCellEditable widget
       */
      if (!canceled)
	gtk_cell_editable_editing_done (edit_widget);

      /* Remove any references to the editable widget */
      gtk_cell_area_set_edited_cell (area, NULL);
      gtk_cell_area_set_edit_widget (area, NULL);

      /* Send the remove-widget signal explicitly (this is done after setting
       * the edit cell/widget NULL to avoid feedback)
       */
      gtk_cell_area_remove_editable (area, edit_cell, edit_widget);
      g_object_unref (edit_cell);
      g_object_unref (edit_widget);
    }
}

/*************************************************************
 *         API: Convenience for area implementations         *
 *************************************************************/

/**
 * gtk_cell_area_inner_cell_area:
 * @area: a `GtkCellArea`
 * @widget: the `GtkWidget` that @area is rendering onto
 * @cell_area: the @widget relative coordinates where one of @area’s cells
 *             is to be placed
 * @inner_area: (out): the return location for the inner cell area
 *
 * This is a convenience function for `GtkCellArea` implementations
 * to get the inner area where a given `GtkCellRenderer` will be
 * rendered. It removes any padding previously added by gtk_cell_area_request_renderer().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_inner_cell_area (GtkCellArea        *area,
                               GtkWidget          *widget,
                               const GdkRectangle *cell_area,
                               GdkRectangle       *inner_area)
{
  GtkBorder border;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (inner_area != NULL);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (context, &border);

  *inner_area = *cell_area;

  if (border.left + border.right > cell_area->width)
    {
      border.left = cell_area->width / 2;
      border.right = (cell_area->width + 1) / 2;
    }
  inner_area->x += border.left;
  inner_area->width -= border.left + border.right;
  if (border.top + border.bottom > cell_area->height)
    {
      border.top = cell_area->height / 2;
      border.bottom = (cell_area->height + 1) / 2;
    }
  inner_area->y += border.top;
  inner_area->height -= border.top + border.bottom;
}

/**
 * gtk_cell_area_request_renderer:
 * @area: a `GtkCellArea`
 * @renderer: the `GtkCellRenderer` to request size for
 * @orientation: the `GtkOrientation` in which to request size
 * @widget: the `GtkWidget` that @area is rendering onto
 * @for_size: the allocation contextual size to request for, or -1 if
 * the base request for the orientation is to be returned.
 * @minimum_size: (out) (optional): location to store the minimum size
 * @natural_size: (out) (optional): location to store the natural size
 *
 * This is a convenience function for `GtkCellArea` implementations
 * to request size for cell renderers. It’s important to use this
 * function to request size and then use gtk_cell_area_inner_cell_area()
 * at render and event time since this function will add padding
 * around the cell for focus painting.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_area_request_renderer (GtkCellArea        *area,
                                GtkCellRenderer    *renderer,
                                GtkOrientation      orientation,
                                GtkWidget          *widget,
                                int                 for_size,
                                int                *minimum_size,
                                int                *natural_size)
{
  GtkBorder border;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_size != NULL);
  g_return_if_fail (natural_size != NULL);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (context, &border);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size < 0)
          gtk_cell_renderer_get_preferred_width (renderer, widget, minimum_size, natural_size);
      else
        {
          for_size = MAX (0, for_size - border.left - border.right);

          gtk_cell_renderer_get_preferred_width_for_height (renderer, widget, for_size,
                                                            minimum_size, natural_size);
        }

      *minimum_size += border.left + border.right;
      *natural_size += border.left + border.right;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      if (for_size < 0)
        gtk_cell_renderer_get_preferred_height (renderer, widget, minimum_size, natural_size);
      else
        {
          for_size = MAX (0, for_size - border.top - border.bottom);

          gtk_cell_renderer_get_preferred_height_for_width (renderer, widget, for_size,
                                                            minimum_size, natural_size);
        }

      *minimum_size += border.top + border.bottom;
      *natural_size += border.top + border.bottom;
    }
}

void
_gtk_cell_area_set_cell_data_func_with_proxy (GtkCellArea           *area,
					      GtkCellRenderer       *cell,
					      GFunc                  func,
					      gpointer               func_data,
					      GDestroyNotify         destroy,
					      gpointer               proxy)
{
  GtkCellAreaPrivate *priv = gtk_cell_area_get_instance_private (area);
  CellInfo           *info;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = g_hash_table_lookup (priv->cell_info, cell);

  /* Note we do not take a reference to the proxy, the proxy is a GtkCellLayout
   * that is forwarding its implementation to a delegate GtkCellArea therefore
   * its life-cycle is longer than the area's life cycle.
   */
  if (info)
    {
      if (info->destroy && info->data)
	info->destroy (info->data);

      if (func)
	{
	  info->func    = (GtkCellLayoutDataFunc)func;
	  info->data    = func_data;
	  info->destroy = destroy;
	  info->proxy   = proxy;
	}
      else
	{
	  info->func    = NULL;
	  info->data    = NULL;
	  info->destroy = NULL;
	  info->proxy   = NULL;
	}
    }
  else
    {
      info = cell_info_new ((GtkCellLayoutDataFunc)func, func_data, destroy);
      info->proxy = proxy;

      g_hash_table_insert (priv->cell_info, cell, info);
    }
}
