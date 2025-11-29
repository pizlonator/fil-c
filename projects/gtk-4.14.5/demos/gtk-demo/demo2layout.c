#include "demo2layout.h"
#include "four_point_transform.h"

struct _Demo2Layout
{
  GtkLayoutManager parent_instance;

  float position;
  float offset;
};

struct _Demo2LayoutClass
{
  GtkLayoutManagerClass parent_class;
};

G_DEFINE_TYPE (Demo2Layout, demo2_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
demo2_layout_measure (GtkLayoutManager *layout_manager,
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

  *minimum = minimum_size;
  *natural = 3 * natural_size;
}


#define RADIANS(angle) ((angle)*M_PI/180.0);

/* Spherical coordinates */
#define SX(r,t,p) ((r) * sin (t) * cos (p))
#define SZ(r,t,p) ((r) * sin (t) * sin (p))
#define SY(r,t,p) ((r) * cos (t))

static double
map_offset (double x)
{
  x = fmod (x, 180.0);
  if (x < 0.0)
    x += 180.0;

  return x;
}

static void
demo2_layout_allocate (GtkLayoutManager *layout_manager,
                       GtkWidget        *widget,
                       int               width,
                       int               height,
                       int               baseline)
{
  GtkWidget *child;
  GtkRequisition child_req;
  int i, j, k;
  float x0, y0;
  float w, h;
  graphene_point3d_t p1, p2, p3, p4;
  graphene_point3d_t q1, q2, q3, q4;
  double t_1, t_2, p_1, p_2;
  double r;
  graphene_matrix_t m;
  GskTransform *transform;
  double position = DEMO2_LAYOUT (layout_manager)->position;
  double offset = DEMO2_LAYOUT (layout_manager)->offset;

  /* for simplicity, assume all children are the same size */
  gtk_widget_get_preferred_size (gtk_widget_get_first_child (widget), &child_req, NULL);
  w = child_req.width;
  h = child_req.height;

  r = 300;
  x0 = y0 = 300;

  for (child = gtk_widget_get_first_child (widget), i = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), i++)
    {
      j = i / 36;
      k = i % 36;

      gtk_widget_set_child_visible (child, FALSE);

      graphene_point3d_init (&p1, w, h, 1.);
      graphene_point3d_init (&p2, w, 0., 1.);
      graphene_point3d_init (&p3, 0., 0., 1.);
      graphene_point3d_init (&p4, 0., h, 1.);

      t_1 = RADIANS (map_offset (offset + 10 * j));
      t_2 = RADIANS (map_offset (offset + 10 * (j + 1)));
      p_1 = RADIANS (position + 10 * k);
      p_2 = RADIANS (position + 10 * (k + 1));

      if (t_2 < t_1)
        continue;

      if (SZ (r, t_1, p_1) > 0 ||
          SZ (r, t_2, p_1) > 0 ||
          SZ (r, t_1, p_2) > 0 ||
          SZ (r, t_2, p_2) > 0)
        continue;

      gtk_widget_set_child_visible (child, TRUE);

      graphene_point3d_init (&q1, x0 + SX (r, t_1, p_1), y0 + SY (r, t_1, p_1), SZ (r, t_1, p_1));
      graphene_point3d_init (&q2, x0 + SX (r, t_2, p_1), y0 + SY (r, t_2, p_1), SZ (r, t_2, p_1));
      graphene_point3d_init (&q3, x0 + SX (r, t_2, p_2), y0 + SY (r, t_2, p_2), SZ (r, t_2, p_2));
      graphene_point3d_init (&q4, x0 + SX (r, t_1, p_2), y0 + SY (r, t_1, p_2), SZ (r, t_1, p_2));

      /* Get a matrix that moves p1 -> q1, p2 -> q2, ... */
      perspective_3d (&p1, &p2, &p3, &p4,
                      &q1, &q2, &q3, &q4,
                      &m);

      transform = gsk_transform_matrix (NULL, &m);

      /* Since our matrix was built for transforming points with z = 1,
       * prepend a translation to the z = 1 plane.
       */
      transform = gsk_transform_translate_3d (transform,
                                              &GRAPHENE_POINT3D_INIT (0, 0, 1));

      gtk_widget_allocate (child, w, h, -1, transform);
    }
}

static GtkSizeRequestMode
demo2_layout_get_request_mode (GtkLayoutManager *layout_manager,
                               GtkWidget        *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
demo2_layout_class_init (Demo2LayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_request_mode = demo2_layout_get_request_mode;
  layout_class->measure = demo2_layout_measure;
  layout_class->allocate = demo2_layout_allocate;
}

static void
demo2_layout_init (Demo2Layout *self)
{
}

GtkLayoutManager *
demo2_layout_new (void)
{
  return g_object_new (DEMO2_TYPE_LAYOUT, NULL);
}

void
demo2_layout_set_position (Demo2Layout *layout,
                           float        position)
{
  layout->position = position;
}

float
demo2_layout_get_position (Demo2Layout *layout)
{
  return layout->position;
}

void
demo2_layout_set_offset (Demo2Layout *layout,
                         float        offset)
{
  layout->offset = offset;
}

float
demo2_layout_get_offset (Demo2Layout *layout)
{
  return layout->offset;
}
