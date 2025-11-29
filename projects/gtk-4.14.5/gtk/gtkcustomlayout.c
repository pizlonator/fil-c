/**
 * GtkCustomLayout:
 *
 * `GtkCustomLayout` uses closures for size negotiation.
 *
 * A `GtkCustomLayout `uses closures matching to the old `GtkWidget`
 * virtual functions for size negotiation, as a convenience API to
 * ease the porting towards the corresponding `GtkLayoutManager
 * virtual functions.
 */

#include "config.h"

#include "gtkcustomlayout.h"

struct _GtkCustomLayout
{
  GtkLayoutManager parent_instance;

  GtkCustomRequestModeFunc request_mode_func;
  GtkCustomMeasureFunc measure_func;
  GtkCustomAllocateFunc allocate_func;
};

G_DEFINE_TYPE (GtkCustomLayout, gtk_custom_layout, GTK_TYPE_LAYOUT_MANAGER)

static GtkSizeRequestMode
gtk_custom_layout_get_request_mode (GtkLayoutManager *manager,
                                    GtkWidget        *widget)
{
  GtkCustomLayout *self = GTK_CUSTOM_LAYOUT (manager);

  if (self->request_mode_func != NULL)
    return self->request_mode_func (widget);

  return GTK_LAYOUT_MANAGER_CLASS (gtk_custom_layout_parent_class)->get_request_mode (manager, widget);
}

static void
gtk_custom_layout_measure (GtkLayoutManager *manager,
                           GtkWidget        *widget,
                           GtkOrientation    orientation,
                           int               for_size,
                           int              *minimum,
                           int              *natural,
                           int              *minimum_baseline,
                           int              *natural_baseline)
{
  GtkCustomLayout *self = GTK_CUSTOM_LAYOUT (manager);
  int min = 0, nat = 0;
  int min_baseline = -1, nat_baseline = -1;

  self->measure_func (widget, orientation, for_size,
                      &min, &nat,
                      &min_baseline, &nat_baseline);

  if (minimum != NULL)
    *minimum = min;
  if (natural != NULL)
    *natural = nat;

  if (minimum_baseline != NULL)
    *minimum_baseline = min_baseline;
  if (natural_baseline != NULL)
    *natural_baseline = nat_baseline;
}

static void
gtk_custom_layout_allocate (GtkLayoutManager *manager,
                            GtkWidget        *widget,
                            int               width,
                            int               height,
                            int               baseline)
{
  GtkCustomLayout *self = GTK_CUSTOM_LAYOUT (manager);

  self->allocate_func (widget, width, height, baseline);
}

static void
gtk_custom_layout_class_init (GtkCustomLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_request_mode = gtk_custom_layout_get_request_mode;
  layout_class->measure = gtk_custom_layout_measure;
  layout_class->allocate = gtk_custom_layout_allocate;
}

static void
gtk_custom_layout_init (GtkCustomLayout *self)
{
}

/**
 * gtk_custom_layout_new:
 * @request_mode: (nullable) (scope call): a function to retrieve
 *   the `GtkSizeRequestMode` of the widget using the layout; the
 *   default request mode is %GTK_SIZE_REQUEST_CONSTANT_SIZE
 * @measure: (not nullable) (scope call): a function to measure the widget using the layout manager
 * @allocate: (not nullable) (scope call): a function to allocate the children of the widget using
 *   the layout manager
 *
 * Creates a new legacy layout manager.
 *
 * Legacy layout managers map to the old `GtkWidget` size negotiation
 * virtual functions, and are meant to be used during the transition
 * from layout containers to layout manager delegates.
 *
 * Returns: (transfer full): the newly created `GtkCustomLayout`
 */
GtkLayoutManager *
gtk_custom_layout_new (GtkCustomRequestModeFunc request_mode,
                       GtkCustomMeasureFunc measure,
                       GtkCustomAllocateFunc allocate)
{
  GtkCustomLayout *self = g_object_new (GTK_TYPE_CUSTOM_LAYOUT, NULL);

  g_return_val_if_fail (measure != NULL, NULL);
  g_return_val_if_fail (allocate != NULL, NULL);

  self->request_mode_func = request_mode;
  self->measure_func = measure;
  self->allocate_func = allocate;

  return GTK_LAYOUT_MANAGER (self);
}
