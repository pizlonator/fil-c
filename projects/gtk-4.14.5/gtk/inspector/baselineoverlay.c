
#include "config.h"
#include "baselineoverlay.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssboxesprivate.h"

struct _GtkBaselineOverlay
{
  GtkInspectorOverlay parent_instance;
};

struct _GtkBaselineOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkBaselineOverlay, gtk_baseline_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
recurse_child_widgets (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  int baseline;
  GtkWidget *child;
  GtkCssBoxes boxes;

  if (!gtk_widget_get_mapped (widget))
    return;

  if (gtk_widget_get_overflow (widget) == GTK_OVERFLOW_HIDDEN)
    {
      gtk_css_boxes_init (&boxes, widget);
      gtk_snapshot_push_rounded_clip (snapshot, gtk_css_boxes_get_padding_box (&boxes));
    }

  baseline = gtk_widget_get_baseline (widget);

  if (baseline != -1)
    {
      GdkRGBA red = {1, 0, 0, 1};
      graphene_rect_t bounds;
      int width;

      width = gtk_widget_get_width (widget);

      /* Now do all the stuff */
      gtk_snapshot_push_debug (snapshot, "Widget baseline debugging");

      graphene_rect_init (&bounds,
                          0, baseline,
                          width, 1);
      gtk_snapshot_append_color (snapshot, &red, &bounds);

      gtk_snapshot_pop (snapshot);
    }

  /* Recurse into child widgets */
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      graphene_matrix_t matrix;

      if (gtk_widget_compute_transform (child, widget, &matrix))
        {
          gtk_snapshot_save (snapshot);
          gtk_snapshot_transform_matrix (snapshot, &matrix);
          recurse_child_widgets (child, snapshot);
          gtk_snapshot_restore (snapshot);
        }
    }

  if (gtk_widget_get_overflow (widget) == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_pop (snapshot);
}

static void
gtk_baseline_overlay_snapshot (GtkInspectorOverlay *overlay,
                               GtkSnapshot         *snapshot,
                               GskRenderNode       *node,
                               GtkWidget           *widget)
{
  recurse_child_widgets (widget, snapshot);
}

static void
gtk_baseline_overlay_init (GtkBaselineOverlay *self)
{
}

static void
gtk_baseline_overlay_class_init (GtkBaselineOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = (GtkInspectorOverlayClass *)klass;

  overlay_class->snapshot = gtk_baseline_overlay_snapshot;
}

GtkInspectorOverlay *
gtk_baseline_overlay_new (void)
{
  return g_object_new (GTK_TYPE_BASELINE_OVERLAY, NULL);
}
