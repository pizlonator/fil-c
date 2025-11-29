/* gtkcelllayout.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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
 * GtkCellLayout:
 *
 * An interface for packing cells
 *
 * `GtkCellLayout` is an interface to be implemented by all objects which
 * want to provide a `GtkTreeViewColumn` like API for packing cells,
 * setting attributes and data funcs.
 *
 * One of the notable features provided by implementations of
 * `GtkCellLayout` are attributes. Attributes let you set the properties
 * in flexible ways. They can just be set to constant values like regular
 * properties. But they can also be mapped to a column of the underlying
 * tree model with gtk_cell_layout_set_attributes(), which means that the value
 * of the attribute can change from cell to cell as they are rendered by
 * the cell renderer. Finally, it is possible to specify a function with
 * gtk_cell_layout_set_cell_data_func() that is called to determine the
 * value of the attribute for each cell that is rendered.
 *
 * ## GtkCellLayouts as GtkBuildable
 *
 * Implementations of GtkCellLayout which also implement the GtkBuildable
 * interface (`GtkCellView`, `GtkIconView`, `GtkComboBox`,
 * `GtkEntryCompletion`, `GtkTreeViewColumn`) accept `GtkCellRenderer` objects
 * as `<child>` elements in UI definitions. They support a custom `<attributes>`
 * element for their children, which can contain multiple `<attribute>`
 * elements. Each `<attribute>` element has a name attribute which specifies
 * a property of the cell renderer; the content of the element is the
 * attribute value.
 *
 * This is an example of a UI definition fragment specifying attributes:
 *
 * ```xml
 * <object class="GtkCellView">
 *   <child>
 *     <object class="GtkCellRendererText"/>
 *     <attributes>
 *       <attribute name="text">0</attribute>
 *     </attributes>
 *   </child>
 * </object>
 * ```
 *
 * Furthermore for implementations of `GtkCellLayout` that use a `GtkCellArea`
 * to lay out cells (all `GtkCellLayout`s in GTK use a `GtkCellArea`)
 * [cell properties](class.CellArea.html#cell-properties) can also be defined
 * in the format by specifying the custom `<cell-packing>` attribute which can
 * contain multiple `<property>` elements.
 *
 * Here is a UI definition fragment specifying cell properties:
 *
 * ```xml
 * <object class="GtkTreeViewColumn">
 *   <child>
 *     <object class="GtkCellRendererText"/>
 *     <cell-packing>
 *       <property name="align">True</property>
 *       <property name="expand">False</property>
 *     </cell-packing>
 *   </child>
 * </object>
 * ```
 *
 * ## Subclassing GtkCellLayout implementations
 *
 * When subclassing a widget that implements `GtkCellLayout` like
 * `GtkIconView` or `GtkComboBox`, there are some considerations related
 * to the fact that these widgets internally use a `GtkCellArea`.
 * The cell area is exposed as a construct-only property by these
 * widgets. This means that it is possible to e.g. do
 *
 * ```c
 * GtkWIdget *combo =
 *   g_object_new (GTK_TYPE_COMBO_BOX, "cell-area", my_cell_area, NULL);
 * ```
 *
 * to use a custom cell area with a combo box. But construct properties
 * are only initialized after instance `init()`
 * functions have run, which means that using functions which rely on
 * the existence of the cell area in your subclass `init()` function will
 * cause the default cell area to be instantiated. In this case, a provided
 * construct property value will be ignored (with a warning, to alert
 * you to the problem).
 *
 * ```c
 * static void
 * my_combo_box_init (MyComboBox *b)
 * {
 *   GtkCellRenderer *cell;
 *
 *   cell = gtk_cell_renderer_pixbuf_new ();
 *
 *   // The following call causes the default cell area for combo boxes,
 *   // a GtkCellAreaBox, to be instantiated
 *   gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (b), cell, FALSE);
 *   ...
 * }
 *
 * GtkWidget *
 * my_combo_box_new (GtkCellArea *area)
 * {
 *   // This call is going to cause a warning about area being ignored
 *   return g_object_new (MY_TYPE_COMBO_BOX, "cell-area", area, NULL);
 * }
 * ```
 *
 * If supporting alternative cell areas with your derived widget is
 * not important, then this does not have to concern you. If you want
 * to support alternative cell areas, you can do so by moving the
 * problematic calls out of `init()` and into a `constructor()`
 * for your class.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   See [class@Gtk.LayoutManager] for layout manager delegate objects
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "gtkcelllayout.h"
#include "gtkbuilderprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define warn_no_cell_area(func)					\
  g_critical ("%s: Called but no GtkCellArea is available yet", func)

typedef GtkCellLayoutIface GtkCellLayoutInterface;
G_DEFINE_INTERFACE (GtkCellLayout, gtk_cell_layout, G_TYPE_OBJECT);

static void   gtk_cell_layout_default_pack_start         (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell,
							  gboolean               expand);
static void   gtk_cell_layout_default_pack_end           (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell,
							  gboolean               expand);
static void   gtk_cell_layout_default_clear              (GtkCellLayout         *cell_layout);
static void   gtk_cell_layout_default_add_attribute      (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell,
							  const char            *attribute,
							  int                    column);
static void   gtk_cell_layout_default_set_cell_data_func (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell,
							  GtkCellLayoutDataFunc  func,
							  gpointer               func_data,
							  GDestroyNotify         destroy);
static void   gtk_cell_layout_default_clear_attributes   (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell);
static void   gtk_cell_layout_default_reorder            (GtkCellLayout         *cell_layout,
							  GtkCellRenderer       *cell,
							  int                    position);
static GList *gtk_cell_layout_default_get_cells          (GtkCellLayout         *cell_layout);


static void
gtk_cell_layout_default_init (GtkCellLayoutIface *iface)
{
  iface->pack_start         = gtk_cell_layout_default_pack_start;
  iface->pack_end           = gtk_cell_layout_default_pack_end;
  iface->clear              = gtk_cell_layout_default_clear;
  iface->add_attribute      = gtk_cell_layout_default_add_attribute;
  iface->set_cell_data_func = gtk_cell_layout_default_set_cell_data_func;
  iface->clear_attributes   = gtk_cell_layout_default_clear_attributes;
  iface->reorder            = gtk_cell_layout_default_reorder;
  iface->get_cells          = gtk_cell_layout_default_get_cells;
}

/* Default implementation is to fall back on an underlying cell area */
static void
gtk_cell_layout_default_pack_start (GtkCellLayout         *cell_layout,
				    GtkCellRenderer       *cell,
				    gboolean               expand)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (area), cell, expand);
      else
	warn_no_cell_area ("GtkCellLayoutIface->pack_start()");
    }
}

static void
gtk_cell_layout_default_pack_end (GtkCellLayout         *cell_layout,
				  GtkCellRenderer       *cell,
				  gboolean               expand)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (area), cell, expand);
      else
	warn_no_cell_area ("GtkCellLayoutIface->pack_end()");
    }
}

static void
gtk_cell_layout_default_clear (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (area));
      else
	warn_no_cell_area ("GtkCellLayoutIface->clear()");
    }
}

static void
gtk_cell_layout_default_add_attribute (GtkCellLayout         *cell_layout,
				       GtkCellRenderer       *cell,
				       const char            *attribute,
				       int                    column)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (area), cell, attribute, column);
      else
	warn_no_cell_area ("GtkCellLayoutIface->add_attribute()");
    }
}

static void
gtk_cell_layout_default_set_cell_data_func (GtkCellLayout         *cell_layout,
					    GtkCellRenderer       *cell,
					    GtkCellLayoutDataFunc  func,
					    gpointer               func_data,
					    GDestroyNotify         destroy)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	_gtk_cell_area_set_cell_data_func_with_proxy (area, cell,
						      (GFunc)func, func_data, destroy,
						      cell_layout);
      else
	warn_no_cell_area ("GtkCellLayoutIface->set_cell_data_func()");
    }
}

static void
gtk_cell_layout_default_clear_attributes (GtkCellLayout         *cell_layout,
					  GtkCellRenderer       *cell)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (area), cell);
      else
	warn_no_cell_area ("GtkCellLayoutIface->clear_attributes()");
    }
}

static void
gtk_cell_layout_default_reorder (GtkCellLayout         *cell_layout,
				 GtkCellRenderer       *cell,
				 int                    position)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (area), cell, position);
      else
	warn_no_cell_area ("GtkCellLayoutIface->reorder()");
    }
}

static GList *
gtk_cell_layout_default_get_cells (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    {
      area = iface->get_area (cell_layout);

      if (area)
	return gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));
      else
	warn_no_cell_area ("GtkCellLayoutIface->get_cells()");
    }
  return NULL;
}


/**
 * gtk_cell_layout_pack_start:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer`
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout
 *
 * Packs the @cell into the beginning of @cell_layout. If @expand is %FALSE,
 * then the @cell is allocated no more space than it needs. Any unused space
 * is divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_pack_start (GtkCellLayout   *cell_layout,
                            GtkCellRenderer *cell,
                            gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_start (cell_layout, cell, expand);
}

/**
 * gtk_cell_layout_pack_end:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer`
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout
 *
 * Adds the @cell to the end of @cell_layout. If @expand is %FALSE, then the
 * @cell is allocated no more space than it needs. Any unused space is
 * divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_pack_end (GtkCellLayout   *cell_layout,
                          GtkCellRenderer *cell,
                          gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_end (cell_layout, cell, expand);
}

/**
 * gtk_cell_layout_clear:
 * @cell_layout: a `GtkCellLayout`
 *
 * Unsets all the mappings on all renderers on @cell_layout and
 * removes all renderers from @cell_layout.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_clear (GtkCellLayout *cell_layout)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear (cell_layout);
}

static void
gtk_cell_layout_set_attributesv (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 va_list          args)
{
  char *attribute;
  int column;

  attribute = va_arg (args, char *);

  gtk_cell_layout_clear_attributes (cell_layout, cell);

  while (attribute != NULL)
    {
      column = va_arg (args, int);

      gtk_cell_layout_add_attribute (cell_layout, cell, attribute, column);

      attribute = va_arg (args, char *);
    }
}

/**
 * gtk_cell_layout_set_attributes:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer`
 * @...: a %NULL-terminated list of attributes
 *
 * Sets the attributes in the parameter list as the attributes
 * of @cell_layout.
 *
 * See [method@Gtk.CellLayout.add_attribute] for more details.
 *
 * The attributes should be in attribute/column order, as in
 * gtk_cell_layout_add_attribute(). All existing attributes are
 * removed, and replaced with the new attributes.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_set_attributes (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  va_start (args, cell);
  gtk_cell_layout_set_attributesv (cell_layout, cell, args);
  va_end (args);
}

/**
 * gtk_cell_layout_add_attribute:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer`
 * @attribute: a property on the renderer
 * @column: the column position on the model to get the attribute from
 *
 * Adds an attribute mapping to the list in @cell_layout.
 *
 * The @column is the column of the model to get a value from, and the
 * @attribute is the property on @cell to be set from that value. So for
 * example if column 2 of the model contains strings, you could have the
 * “text” attribute of a `GtkCellRendererText` get its values from column 2.
 * In this context "attribute" and "property" are used interchangeably.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_add_attribute (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               const char      *attribute,
                               int              column)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (column >= 0);

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->add_attribute (cell_layout, cell, attribute, column);
}

/**
 * gtk_cell_layout_set_cell_data_func:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer`
 * @func: (nullable): the `GtkCellLayout`DataFunc to use
 * @func_data: (closure): user data for @func
 * @destroy: destroy notify for @func_data
 *
 * Sets the `GtkCellLayout`DataFunc to use for @cell_layout.
 *
 * This function is used instead of the standard attributes mapping
 * for setting the column value, and should set the value of @cell_layout’s
 * cell renderer(s) as appropriate.
 *
 * @func may be %NULL to remove a previously set function.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_set_cell_data_func (GtkCellLayout         *cell_layout,
                                    GtkCellRenderer       *cell,
                                    GtkCellLayoutDataFunc  func,
                                    gpointer               func_data,
                                    GDestroyNotify         destroy)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  GTK_CELL_LAYOUT_GET_IFACE
    (cell_layout)->set_cell_data_func (cell_layout, cell, func, func_data, destroy);
}

/**
 * gtk_cell_layout_clear_attributes:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer` to clear the attribute mapping on
 *
 * Clears all existing attributes previously set with
 * gtk_cell_layout_set_attributes().
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_clear_attributes (GtkCellLayout   *cell_layout,
                                  GtkCellRenderer *cell)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear_attributes (cell_layout, cell);
}

/**
 * gtk_cell_layout_reorder:
 * @cell_layout: a `GtkCellLayout`
 * @cell: a `GtkCellRenderer` to reorder
 * @position: new position to insert @cell at
 *
 * Re-inserts @cell at @position.
 *
 * Note that @cell has already to be packed into @cell_layout
 * for this to function properly.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_layout_reorder (GtkCellLayout   *cell_layout,
                         GtkCellRenderer *cell,
                         int              position)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->reorder (cell_layout, cell, position);
}

/**
 * gtk_cell_layout_get_cells:
 * @cell_layout: a `GtkCellLayout`
 *
 * Returns the cell renderers which have been added to @cell_layout.
 *
 * Returns: (element-type GtkCellRenderer) (transfer container):
 *   a list of cell renderers. The list, but not the renderers has
 *   been newly allocated and should be freed with g_list_free()
 *   when no longer needed.
 *
 * Deprecated: 4.10
 */
GList *
gtk_cell_layout_get_cells (GtkCellLayout *cell_layout)
{
  g_return_val_if_fail (GTK_IS_CELL_LAYOUT (cell_layout), NULL);

  return GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->get_cells (cell_layout);
}

/**
 * gtk_cell_layout_get_area:
 * @cell_layout: a `GtkCellLayout`
 *
 * Returns the underlying `GtkCellArea` which might be @cell_layout
 * if called on a `GtkCellArea` or might be %NULL if no `GtkCellArea`
 * is used by @cell_layout.
 *
 * Returns: (transfer none) (nullable): the cell area used by @cell_layout
 *
 * Deprecated: 4.10
 */
GtkCellArea *
gtk_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_LAYOUT (cell_layout), NULL);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);
  if (iface->get_area)
    return iface->get_area (cell_layout);

  return NULL;
}

/* Attribute parsing */
typedef struct {
  GtkCellLayout   *cell_layout;
  GtkCellRenderer *renderer;
  GtkBuilder      *builder;
  char            *attr_name;
  GString         *string;
} AttributesSubParserData;

static void
attributes_start_element (GtkBuildableParseContext *context,
                          const char               *element_name,
                          const char              **names,
                          const char              **values,
                          gpointer                  user_data,
                          GError                  **error)
{
  AttributesSubParserData *data = (AttributesSubParserData*)user_data;

  if (strcmp (element_name, "attribute") == 0)
    {
      const char *name;

      if (!_gtk_builder_check_parent (data->builder, context, "attributes", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->attr_name = g_strdup (name);
    }
  else if (strcmp (element_name, "attributes") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "child", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkCellLayout", element_name,
                                        error);
    }
}

static void
attributes_text_element (GtkBuildableParseContext  *context,
                         const char                *text,
                         gsize                      text_len,
                         gpointer                   user_data,
                         GError                   **error)
{
  AttributesSubParserData *data = (AttributesSubParserData*)user_data;

  if (data->attr_name)
    g_string_append_len (data->string, text, text_len);
}

static void
attributes_end_element (GtkBuildableParseContext  *context,
                        const char                *element_name,
                        gpointer                   user_data,
                        GError                   **error)
{
  AttributesSubParserData *data = (AttributesSubParserData*)user_data;
  GValue val = G_VALUE_INIT;

  if (!data->attr_name)
    return;

  if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_INT, data->string->str, &val, error))
    {
      _gtk_builder_prefix_error (data->builder, context, error);
       return;
    }

  gtk_cell_layout_add_attribute (data->cell_layout,
				 data->renderer,
				 data->attr_name,
                                 g_value_get_int (&val));

  g_free (data->attr_name);
  data->attr_name = NULL;

  g_string_set_size (data->string, 0);
}

static const GtkBuildableParser attributes_parser =
  {
    attributes_start_element,
    attributes_end_element,
    attributes_text_element
  };


/* Cell packing parsing */
static void
gtk_cell_layout_buildable_set_cell_property (GtkCellArea     *area,
					     GtkBuilder      *builder,
					     GtkCellRenderer *cell,
					     char            *name,
					     const char      *value)
{
  GParamSpec *pspec;
  GValue gvalue = G_VALUE_INIT;
  GError *error = NULL;

  pspec = gtk_cell_area_class_find_cell_property (GTK_CELL_AREA_GET_CLASS (area), name);
  if (!pspec)
    {
      g_warning ("%s does not have a property called %s",
		 g_type_name (G_OBJECT_TYPE (area)), name);
      return;
    }

  if (!gtk_builder_value_from_string (builder, pspec, value, &gvalue, &error))
    {
      g_warning ("Could not read property %s:%s with value %s of type %s: %s",
		 g_type_name (G_OBJECT_TYPE (area)),
		 name,
		 value,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 error->message);
      g_error_free (error);
      return;
    }

  gtk_cell_area_cell_set_property (area, cell, name, &gvalue);
  g_value_unset (&gvalue);
}

typedef struct {
  GtkBuilder      *builder;
  GtkCellLayout   *cell_layout;
  GtkCellRenderer *renderer;
  GString         *string;
  char            *cell_prop_name;
  char            *context;
  gboolean         translatable;
} CellPackingSubParserData;

static void
cell_packing_start_element (GtkBuildableParseContext *context,
                            const char               *element_name,
                            const char              **names,
                            const char              **values,
                            gpointer                  user_data,
                            GError                  **error)
{
  CellPackingSubParserData *data = (CellPackingSubParserData*)user_data;

  if (strcmp (element_name, "property") == 0)
    {
      const char *name;
      gboolean translatable = FALSE;
      const char *ctx = NULL;

      if (!_gtk_builder_check_parent (data->builder, context, "cell-packing", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &ctx,
                                        G_MARKUP_COLLECT_INVALID))
       {
         _gtk_builder_prefix_error (data->builder, context, error);
         return;
       }

      data->cell_prop_name = g_strdup (name);
      data->translatable = translatable;
      data->context = g_strdup (ctx);
    }
  else if (strcmp (element_name, "cell-packing") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "child", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkCellLayout", element_name,
                                        error);
    }
}

static void
cell_packing_text_element (GtkBuildableParseContext *context,
                           const char               *text,
                           gsize                     text_len,
                           gpointer                  user_data,
                           GError                  **error)
{
  CellPackingSubParserData *data = (CellPackingSubParserData*)user_data;

  if (data->cell_prop_name)
    g_string_append_len (data->string, text, text_len);
}

static void
cell_packing_end_element (GtkBuildableParseContext *context,
                          const char               *element_name,
                          gpointer                  user_data,
                          GError                  **error)
{
  CellPackingSubParserData *data = (CellPackingSubParserData*)user_data;
  GtkCellArea *area;

  area = gtk_cell_layout_get_area (data->cell_layout);

  if (area)
    {
      /* translate the string */
      if (data->string->len && data->translatable)
	{
	  const char *translated;
	  const char * domain;

	  domain = gtk_builder_get_translation_domain (data->builder);

	  translated = _gtk_builder_parser_translate (domain,
						      data->context,
						      data->string->str);
	  g_string_assign (data->string, translated);
	}

      if (data->cell_prop_name)
	gtk_cell_layout_buildable_set_cell_property (area,
						     data->builder,
						     data->renderer,
						     data->cell_prop_name,
						     data->string->str);
    }
  else
    g_warning ("%s does not have an internal GtkCellArea class and cannot apply child cell properties",
	       g_type_name (G_OBJECT_TYPE (data->cell_layout)));

  g_string_set_size (data->string, 0);
  g_free (data->cell_prop_name);
  g_free (data->context);
  data->cell_prop_name = NULL;
  data->context = NULL;
  data->translatable = FALSE;
}


static const GtkBuildableParser cell_packing_parser =
  {
    cell_packing_start_element,
    cell_packing_end_element,
    cell_packing_text_element
  };

gboolean
_gtk_cell_layout_buildable_custom_tag_start (GtkBuildable       *buildable,
                                             GtkBuilder         *builder,
                                             GObject            *child,
                                             const char         *tagname,
                                             GtkBuildableParser *parser,
                                             gpointer           *data)
{
  AttributesSubParserData  *attr_data;
  CellPackingSubParserData *packing_data;

  if (!child)
    return FALSE;

  if (strcmp (tagname, "attributes") == 0)
    {
      attr_data = g_slice_new0 (AttributesSubParserData);
      attr_data->cell_layout = GTK_CELL_LAYOUT (buildable);
      attr_data->renderer = GTK_CELL_RENDERER (child);
      attr_data->builder = builder;
      attr_data->attr_name = NULL;
      attr_data->string = g_string_new ("");

      *parser = attributes_parser;
      *data = attr_data;

      return TRUE;
    }
  else if (strcmp (tagname, "cell-packing") == 0)
    {
      packing_data = g_slice_new0 (CellPackingSubParserData);
      packing_data->string = g_string_new ("");
      packing_data->builder = builder;
      packing_data->cell_layout = GTK_CELL_LAYOUT (buildable);
      packing_data->renderer = GTK_CELL_RENDERER (child);

      *parser = cell_packing_parser;
      *data = packing_data;

      return TRUE;
    }

  return FALSE;
}

gboolean
_gtk_cell_layout_buildable_custom_tag_end (GtkBuildable *buildable,
					   GtkBuilder   *builder,
					   GObject      *child,
					   const char   *tagname,
					   gpointer     *data)
{
  AttributesSubParserData *attr_data;
  CellPackingSubParserData *packing_data;

  if (strcmp (tagname, "attributes") == 0)
    {
      attr_data = (AttributesSubParserData*)data;
      g_assert (!attr_data->attr_name);
      g_string_free (attr_data->string, TRUE);
      g_slice_free (AttributesSubParserData, attr_data);

      return TRUE;
    }
  else if (strcmp (tagname, "cell-packing") == 0)
    {
      packing_data = (CellPackingSubParserData *)data;
      g_string_free (packing_data->string, TRUE);
      g_slice_free (CellPackingSubParserData, packing_data);

      return TRUE;
    }
  return FALSE;
}

void
_gtk_cell_layout_buildable_add_child (GtkBuildable      *buildable,
				      GtkBuilder        *builder,
				      GObject           *child,
				      const char        *type)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (buildable));
  g_return_if_fail (GTK_IS_CELL_RENDERER (child));

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (buildable), GTK_CELL_RENDERER (child), FALSE);
}
