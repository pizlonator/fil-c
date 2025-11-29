#include "demolayout.h"

struct _DemoLayout
{
  GtkLayoutManager parent_instance;

  float position;
  int pos[16];
};

struct _DemoLayoutClass
{
  GtkLayoutManagerClass parent_class;
};

G_DEFINE_TYPE (DemoLayout, demo_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
demo_layout_measure (GtkLayoutManager *layout_manager,
                     GtkWidget        *widget,
                     GtkOrientation    orientation,
                     int               for_size,
                     int              *minimum,
                     int              *natural,
                     int              *minimum_baseline,
                     int              *natural_baseline)
{
  GtkWidget *child;
  int minimum_size = 0;
  int natural_size = 0;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min = 0, child_nat = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation, -1,
                          &child_min, &child_nat,
                          NULL, NULL);
      minimum_size = MAX (minimum_size, child_min);
      natural_size = MAX (natural_size, child_nat);
    }

  /* A back-of-a-napkin calculation to reserve enough
   * space for arranging 16 children in a circle.
   */
  *minimum = 16 * minimum_size / G_PI + minimum_size;
  *natural = 16 * natural_size / G_PI + natural_size;
}

static void
demo_layout_allocate (GtkLayoutManager *layout_manager,
                      GtkWidget        *widget,
                      int               width,
                      int               height,
                      int               baseline)
{
  DemoLayout *self = DEMO_LAYOUT (layout_manager);
  GtkWidget *child;
  int i;
  int child_width = 0;
  int child_height = 0;
  int x0, y0;
  float r;
  float t;

  t = self->position;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkRequisition child_req;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_get_preferred_size (child, &child_req, NULL);

      child_width = MAX (child_width, child_req.width);
      child_height = MAX (child_height, child_req.height);
    }

  /* the center of our layout */
  x0 = (width / 2);
  y0 = (height / 2);

  /* the radius for our circle of children */
  r = 8 * child_width / G_PI;

  for (child = gtk_widget_get_first_child (widget), i = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), i++)
    {
      GtkRequisition child_req;
      float a = self->pos[i] * G_PI / 8;
      int gx, gy;
      int cx, cy;
      int x, y;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_get_preferred_size (child, &child_req, NULL);

      /* The grid position of child. */
      gx = x0 + (i % 4 - 2) * child_width;
      gy = y0 + (i / 4 - 2) * child_height;

      /* The circle position of child. Note that we
       * are adjusting the position by half the child size
       * to place the center of child on a centered circle.
       * This assumes that the children don't use align flags
       * or uneven margins that would shift the center.
       */
      cx = x0 + sin (a) * r - child_req.width / 2;
      cy = y0 + cos (a) * r - child_req.height / 2;

      /* we interpolate between the two layouts according to
       * the position value that has been set on the layout.
       */
      x = t * cx + (1 - t) * gx;
      y = t * cy + (1 - t) * gy;

      gtk_widget_size_allocate (child,
                                &(const GtkAllocation){ x, y, child_width, child_height},
                                -1);
    }
}

static GtkSizeRequestMode
demo_layout_get_request_mode (GtkLayoutManager *layout_manager,
                              GtkWidget        *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
demo_layout_class_init (DemoLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_request_mode = demo_layout_get_request_mode;
  layout_class->measure = demo_layout_measure;
  layout_class->allocate = demo_layout_allocate;
}

static void
demo_layout_init (DemoLayout *self)
{
  int i;

  for (i = 0; i < 16; i++)
    self->pos[i] = i;
}

GtkLayoutManager *
demo_layout_new (void)
{
  return g_object_new (DEMO_TYPE_LAYOUT, NULL);
}

void
demo_layout_set_position (DemoLayout *layout,
                          float       position)
{
  layout->position = position;
}

/* Shuffle the circle positions of the children.
 * Should be called when we are in the grid layout.
 */
void
demo_layout_shuffle (DemoLayout *layout)
{
  int i, j, tmp;

  for (i = 0; i < 16; i++)
    {
      j = g_random_int_range (0, i + 1);
      tmp = layout->pos[i];
      layout->pos[i] = layout->pos[j];
      layout->pos[j] = tmp;
    }
}
