#include "gtkshaderbin.h"

typedef struct {
  GskGLShader *shader;
  GtkStateFlags state;
  GtkStateFlags state_mask;
  float extra_border;
  gboolean compiled;
  gboolean compiled_ok;
} ShaderInfo;

struct _GtkShaderBin
{
  GtkWidget parent_instance;
  GtkWidget *child;
  ShaderInfo *active_shader;
  GPtrArray *shaders;
  guint tick_id;
  float time;
  float mouse_x, mouse_y;
  gint64 first_frame_time;
};

struct _GtkShaderBinClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkShaderBin, gtk_shader_bin, GTK_TYPE_WIDGET)

static void
shader_info_free (ShaderInfo *info)
{
  g_object_unref (info->shader);
  g_free (info);
}

static void
gtk_shader_bin_finalize (GObject *object)
{
  GtkShaderBin *self = GTK_SHADER_BIN (object);

  g_ptr_array_free (self->shaders, TRUE);

  G_OBJECT_CLASS (gtk_shader_bin_parent_class)->finalize (object);
}

static void
gtk_shader_bin_dispose (GObject *object)
{
  GtkShaderBin *self = GTK_SHADER_BIN (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_shader_bin_parent_class)->dispose (object);
}

static gboolean
gtk_shader_bin_tick (GtkWidget     *widget,
                     GdkFrameClock *frame_clock,
                     gpointer       unused)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);
  gint64 frame_time;

  frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  if (self->first_frame_time == 0)
    self->first_frame_time = frame_time;
  self->time = (frame_time - self->first_frame_time) / (float)G_USEC_PER_SEC;

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
motion_cb (GtkEventControllerMotion *controller,
           double                    x,
           double                    y,
           GtkShaderBin             *self)
{
  self->mouse_x = x;
  self->mouse_y = y;
}

static void
gtk_shader_bin_init (GtkShaderBin *self)
{
  GtkEventController *controller;
  self->shaders = g_ptr_array_new_with_free_func ((GDestroyNotify)shader_info_free);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

void
gtk_shader_bin_update_active_shader (GtkShaderBin *self)
{
  GtkStateFlags new_state = gtk_widget_get_state_flags (GTK_WIDGET (self));
  ShaderInfo *new_shader = NULL;

  for (int i = 0; i < self->shaders->len; i++)
    {
      ShaderInfo *info = g_ptr_array_index (self->shaders, i);

      if ((info->state_mask & new_state) == info->state)
        {
          new_shader = info;
          break;
        }
    }

  if (self->active_shader == new_shader)
    return;

  self->active_shader = new_shader;
  self->first_frame_time = 0;

  if (self->active_shader)
    {
      if (self->tick_id == 0)
        self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                      gtk_shader_bin_tick,
                                                      NULL, NULL);
    }
  else
    {
      if (self->tick_id != 0)
        {
          gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
          self->tick_id = 0;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_shader_bin_state_flags_changed (GtkWidget        *widget,
                                    GtkStateFlags     previous_state_flags)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);

  gtk_shader_bin_update_active_shader (self);
}

void
gtk_shader_bin_add_shader (GtkShaderBin *self,
                           GskGLShader  *shader,
                           GtkStateFlags state,
                           GtkStateFlags state_mask,
                           float         extra_border)
{
  ShaderInfo *info = g_new0 (ShaderInfo, 1);
  info->shader = g_object_ref (shader);
  info->state = state;
  info->state_mask = state_mask;
  info->extra_border = extra_border;

  g_ptr_array_add (self->shaders, info);

  gtk_shader_bin_update_active_shader (self);
}

void
gtk_shader_bin_set_child (GtkShaderBin *self,
                          GtkWidget    *child)
{

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }
}

GtkWidget *
gtk_shader_bin_get_child   (GtkShaderBin *self)
{
  return self->child;
}

static void
gtk_shader_bin_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkShaderBin *self = GTK_SHADER_BIN (widget);
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (self->active_shader)
    {
      if (!self->active_shader->compiled)
        {
          GtkNative *native = gtk_widget_get_native (widget);
          GskRenderer *renderer = gtk_native_get_renderer (native);
          GError *error = NULL;

          self->active_shader->compiled = TRUE;
          self->active_shader->compiled_ok =
            gsk_gl_shader_compile (self->active_shader->shader,
                                   renderer, &error);
          if (!self->active_shader->compiled_ok)
            {
              g_warning ("GtkShaderBin failed to compile shader: %s", error->message);
              g_error_free (error);
            }
        }

      if (self->active_shader->compiled_ok)
        {
          float border = self->active_shader->extra_border;
          graphene_vec2_t mouse;
          graphene_vec2_init (&mouse, self->mouse_x + border, self->mouse_y + border);
          gtk_snapshot_push_gl_shader (snapshot, self->active_shader->shader,
                                       &GRAPHENE_RECT_INIT(-border, -border, width+2*border, height+2*border),
                                       gsk_gl_shader_format_args (self->active_shader->shader,
                                                                  "u_time", self->time,
                                                                  "u_mouse", &mouse,
                                                                  NULL));
          gtk_widget_snapshot_child (widget, self->child, snapshot);
          gtk_snapshot_gl_shader_pop_texture (snapshot);
          gtk_snapshot_pop (snapshot);

          return;
        }
    }

  /* Non-shader fallback */
  gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
gtk_shader_bin_class_init (GtkShaderBinClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_shader_bin_finalize;
  object_class->dispose = gtk_shader_bin_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  widget_class->snapshot = gtk_shader_bin_snapshot;
  widget_class->state_flags_changed = gtk_shader_bin_state_flags_changed;
}

GtkWidget *
gtk_shader_bin_new (void)
{
  GtkShaderBin *self;

  self = g_object_new (GTK_TYPE_SHADER_BIN, NULL);

  return GTK_WIDGET (self);
}
