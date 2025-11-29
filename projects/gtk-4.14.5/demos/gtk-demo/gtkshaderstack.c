#include "gtkshaderstack.h"

struct _GtkShaderStack
{
  GtkWidget parent_instance;

  GskGLShader *shader;
  GPtrArray *children;
  int current;
  int next;
  gboolean backwards;

  guint tick_id;
  float time;
  float duration;
  gint64 start_time;
};

struct _GtkShaderStackClass
{
  GtkWidgetClass parent_class;
};


enum {
  PROP_DURATION = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

G_DEFINE_TYPE (GtkShaderStack, gtk_shader_stack, GTK_TYPE_WIDGET)

static void
gtk_shader_stack_finalize (GObject *object)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  g_object_unref (self->shader);

  G_OBJECT_CLASS (gtk_shader_stack_parent_class)->finalize (object);
}

static void
update_child_visible (GtkShaderStack *self)
{
  int i;

  for (i = 0; i < self->children->len; i++)
    {
      GtkWidget *child = g_ptr_array_index (self->children, i);

      gtk_widget_set_child_visible (child,
                                    i == self->current || i == self->next);
    }
}

static gboolean
transition_cb (GtkWidget     *widget,
               GdkFrameClock *clock,
               gpointer       unused)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  gint64 frame_time;

  frame_time = gdk_frame_clock_get_frame_time (clock);

  if (self->start_time == 0)
    self->start_time = frame_time;

  self->time = (frame_time - self->start_time) / (float)G_USEC_PER_SEC;

  gtk_widget_queue_draw (widget);

  if (self->time >= self->duration)
    {
      self->current = self->next;
      self->next = -1;

      update_child_visible (self);

      return G_SOURCE_REMOVE;
    }
  else
    return G_SOURCE_CONTINUE;
}

static void
start_transition (GtkShaderStack *self)
{
  self->start_time = 0;
  self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                transition_cb,
                                                NULL, NULL);
}

static void
stop_transition (GtkShaderStack *self)
{
  if (self->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
      self->tick_id = 0;
    }

  if (self->next != -1)
    self->current = self->next;
  self->next = -1;

  update_child_visible (self);
}

static void
gtk_shader_stack_dispose (GObject *object)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  stop_transition (self);

  g_clear_pointer (&self->children, g_ptr_array_unref);

  G_OBJECT_CLASS (gtk_shader_stack_parent_class)->dispose (object);
}

void
gtk_shader_stack_transition (GtkShaderStack *self,
                             gboolean forward)
{
  stop_transition (self);

  self->backwards = !forward;
  if (self->backwards)
    self->next = (self->current + self->children->len - 1) % self->children->len;
  else
    self->next = (self->current + 1) % self->children->len;

  update_child_visible (self);

  start_transition (self);
}

static void
gtk_shader_stack_init (GtkShaderStack *self)
{
  self->children = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_widget_unparent);
  self->current = -1;
  self->next = -1;
  self->backwards = FALSE;
  self->duration = 1.0;
}

static void
gtk_shader_stack_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  int i;

  *minimum = 0;
  *natural = 0;

  for (i = 0; i < self->children->len; i++)
    {
      GtkWidget *child = g_ptr_array_index (self->children, i);
      int child_min, child_nat;

      if (gtk_widget_get_visible (child))
        {
          gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }
    }
}

static void
gtk_shader_stack_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  GtkAllocation child_allocation;
  GtkWidget *child;
  int i;

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = width;
  child_allocation.height = height;

  for (i = 0; i < self->children->len; i++)
    {
      child = g_ptr_array_index (self->children, i);
      if (gtk_widget_get_visible (child))
        gtk_widget_size_allocate (child, &child_allocation, -1);
    }
}

static void
gtk_shader_stack_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkShaderStack *self = GTK_SHADER_STACK (widget);
  int width, height;
  GtkWidget *current, *next;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  current = g_ptr_array_index (self->children, self->current);

  if (self->next == -1)
    {
      gtk_widget_snapshot_child (widget, current, snapshot);
    }
  else
    {
      GtkNative *native = gtk_widget_get_native (widget);
      GskRenderer *renderer = gtk_native_get_renderer (native);
      float progress;

      next = g_ptr_array_index (self->children, self->next);

      progress = self->time / self->duration;

      if (self->backwards)
        {
          GtkWidget *tmp = next;
          next = current;
          current = tmp;
          progress = 1. - progress;
        }

      if (gsk_gl_shader_compile (self->shader, renderer, NULL))
        {
          gtk_snapshot_push_gl_shader (snapshot,
                                       self->shader,
                                       &GRAPHENE_RECT_INIT(0, 0, width, height),
                                       gsk_gl_shader_format_args (self->shader,
                                                                  "progress", progress,
                                                                  NULL));

          gtk_widget_snapshot_child (widget, current, snapshot);
          gtk_snapshot_gl_shader_pop_texture (snapshot); /* current child */
          gtk_widget_snapshot_child (widget, next, snapshot);
          gtk_snapshot_gl_shader_pop_texture (snapshot); /* next child */
          gtk_snapshot_pop (snapshot);
        }
      else
        {
          /* Non-shader fallback */
          gtk_widget_snapshot_child (widget, current, snapshot);
        }
    }
}

static void
gtk_shader_stack_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_float (value, self->duration);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_shader_stack_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkShaderStack *self = GTK_SHADER_STACK (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      self->duration = g_value_get_float (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_shader_stack_class_init (GtkShaderStackClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_shader_stack_finalize;
  object_class->dispose = gtk_shader_stack_dispose;
  object_class->get_property = gtk_shader_stack_get_property;
  object_class->set_property = gtk_shader_stack_set_property;

  widget_class->snapshot = gtk_shader_stack_snapshot;
  widget_class->measure = gtk_shader_stack_measure;
  widget_class->size_allocate = gtk_shader_stack_size_allocate;

  properties[PROP_DURATION] =
      g_param_spec_float ("duration", "Duration", "Duration",
                          0.1, 3.0, 1.0,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

GtkWidget *
gtk_shader_stack_new (void)
{
  return g_object_new (GTK_TYPE_SHADER_STACK, NULL);
}

void
gtk_shader_stack_set_shader (GtkShaderStack *self,
                             GskGLShader    *shader)
{
  g_set_object (&self->shader, shader);
}

void
gtk_shader_stack_add_child (GtkShaderStack *self,
                            GtkWidget      *child)
{
  g_ptr_array_add (self->children, child);
  gtk_widget_set_parent (child, GTK_WIDGET (self));
  gtk_widget_queue_resize (GTK_WIDGET (self));

  if (self->current == -1)
    self->current = 0;
  else
    gtk_widget_set_child_visible (child, FALSE);
}

void
gtk_shader_stack_set_active (GtkShaderStack *self,
                             int             index)
{
  stop_transition (self);
  self->current = MIN (index, self->children->len);
  update_child_visible (self);
}
