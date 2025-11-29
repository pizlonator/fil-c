/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright 2013, 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkrevealer.h"

#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkbuildable.h"

/**
 * GtkRevealer:
 *
 * A `GtkRevealer` animates the transition of its child from invisible to visible.
 *
 * The style of transition can be controlled with
 * [method@Gtk.Revealer.set_transition_type].
 *
 * These animations respect the [property@Gtk.Settings:gtk-enable-animations]
 * setting.
 *
 * # CSS nodes
 *
 * `GtkRevealer` has a single CSS node with name revealer.
 * When styling `GtkRevealer` using CSS, remember that it only hides its contents,
 * not itself. That means applied margin, padding and borders will be visible even
 * when the [property@Gtk.Revealer:reveal-child] property is set to %FALSE.
 *
 * # Accessibility
 *
 * `GtkRevealer` uses the %GTK_ACCESSIBLE_ROLE_GROUP role.
 *
 * The child of `GtkRevealer`, if set, is always available in the accessibility
 * tree, regardless of the state of the revealer widget.
 */

/**
 * GtkRevealerTransitionType:
 * @GTK_REVEALER_TRANSITION_TYPE_NONE: No transition
 * @GTK_REVEALER_TRANSITION_TYPE_CROSSFADE: Fade in
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT: Slide in from the left
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT: Slide in from the right
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP: Slide in from the bottom
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN: Slide in from the top
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT: Floop in from the left
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT: Floop in from the right
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_UP: Floop in from the bottom
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN: Floop in from the top
 *
 * These enumeration values describe the possible transitions
 * when the child of a `GtkRevealer` widget is shown or hidden.
 */

struct _GtkRevealer
{
  GtkWidget parent_instance;

  GtkWidget *child;

  GtkRevealerTransitionType transition_type;
  guint transition_duration;

  double current_pos;
  double source_pos;
  double target_pos;

  guint tick_id;
  GtkProgressTracker tracker;
};

typedef struct
{
  GtkWidgetClass parent_class;
} GtkRevealerClass;

enum  {
  PROP_0,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_DURATION,
  PROP_REVEAL_CHILD,
  PROP_CHILD_REVEALED,
  PROP_CHILD,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_revealer_size_allocate (GtkWidget     *widget,
                                        int            width,
                                        int            height,
                                        int            baseline);
static void gtk_revealer_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);

static void     gtk_revealer_set_position (GtkRevealer *revealer,
                                           double       pos);

static void gtk_revealer_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkRevealer, gtk_revealer, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_revealer_buildable_iface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_revealer_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_revealer_set_child (GTK_REVEALER (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_revealer_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_revealer_buildable_add_child;
}

static void
gtk_revealer_init (GtkRevealer *revealer)
{
  revealer->transition_type = GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
  revealer->transition_duration = 250;
  revealer->current_pos = 0.0;
  revealer->target_pos = 0.0;

  gtk_widget_set_overflow (GTK_WIDGET (revealer), GTK_OVERFLOW_HIDDEN);
}

static void
gtk_revealer_dispose (GObject *obj)
{
  GtkRevealer *revealer = GTK_REVEALER (obj);

  g_clear_pointer (&revealer->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_revealer_parent_class)->dispose (obj);
}

static void
gtk_revealer_finalize (GObject *obj)
{
  GtkRevealer *revealer = GTK_REVEALER (obj);

  if (revealer->tick_id != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), revealer->tick_id);
  revealer->tick_id = 0;

  G_OBJECT_CLASS (gtk_revealer_parent_class)->finalize (obj);
}

static void
gtk_revealer_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
   {
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_revealer_get_transition_type (revealer));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_revealer_get_transition_duration (revealer));
      break;
    case PROP_REVEAL_CHILD:
      g_value_set_boolean (value, gtk_revealer_get_reveal_child (revealer));
      break;
    case PROP_CHILD_REVEALED:
      g_value_set_boolean (value, gtk_revealer_get_child_revealed (revealer));
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_revealer_get_child (revealer));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
    {
    case PROP_TRANSITION_TYPE:
      gtk_revealer_set_transition_type (revealer, g_value_get_enum (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_revealer_set_transition_duration (revealer, g_value_get_uint (value));
      break;
    case PROP_REVEAL_CHILD:
      gtk_revealer_set_reveal_child (revealer, g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_revealer_set_child (revealer, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_unmap (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->unmap (widget);

  /* Finish & stop the animation */
  if (revealer->current_pos != revealer->target_pos)
    gtk_revealer_set_position (revealer, revealer->target_pos);

  if (revealer->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), revealer->tick_id);
      revealer->tick_id = 0;
    }
}

static void
gtk_revealer_compute_expand (GtkWidget *widget,
                             gboolean  *hexpand,
                             gboolean  *vexpand)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);

  if (revealer->child)
    {
      *hexpand = gtk_widget_compute_expand (revealer->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (revealer->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_revealer_get_request_mode (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);

  if (revealer->child)
    return gtk_widget_get_request_mode (revealer->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_revealer_class_init (GtkRevealerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = gtk_revealer_dispose;
  object_class->finalize = gtk_revealer_finalize;
  object_class->get_property = gtk_revealer_get_property;
  object_class->set_property = gtk_revealer_set_property;

  widget_class->unmap = gtk_revealer_unmap;
  widget_class->size_allocate = gtk_revealer_size_allocate;
  widget_class->measure = gtk_revealer_measure;
  widget_class->compute_expand = gtk_revealer_compute_expand;
  widget_class->get_request_mode = gtk_revealer_get_request_mode;

  /**
   * GtkRevealer:transition-type: (attributes org.gtk.Property.get=gtk_revealer_get_transition_type org.gtk.Property.set=gtk_revealer_set_transition_type)
   *
   * The type of animation used to transition.
   */
  props[PROP_TRANSITION_TYPE] =
    g_param_spec_enum ("transition-type", NULL, NULL,
                       GTK_TYPE_REVEALER_TRANSITION_TYPE,
                       GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRevealer:transition-duration: (attributes org.gtk.Property.get=gtk_revealer_get_transition_duration org.gtk.Property.set=gtk_revealer_set_transition_duration)
   *
   * The animation duration, in milliseconds.
   */
  props[PROP_TRANSITION_DURATION] =
    g_param_spec_uint ("transition-duration", NULL, NULL,
                       0, G_MAXUINT, 250,
                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRevealer:reveal-child: (attributes org.gtk.Property.get=gtk_revealer_get_reveal_child org.gtk.Property.set=gtk_revealer_set_reveal_child)
   *
   * Whether the revealer should reveal the child.
   */
  props[PROP_REVEAL_CHILD] =
    g_param_spec_boolean ("reveal-child", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRevealer:child-revealed: (attributes org.gtk.Property.get=gtk_revealer_get_child_revealed)
   *
   * Whether the child is revealed and the animation target reached.
   */
  props[PROP_CHILD_REVEALED] =
    g_param_spec_boolean ("child-revealed", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READABLE);

  /**
   * GtkRevealer:child: (attributes org.gtk.Property.get=gtk_revealer_get_child org.gtk.Property.set=gtk_revealer_set_child)
   *
   * The child widget.
   */
  props[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);


  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, I_("revealer"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

/**
 * gtk_revealer_new:
 *
 * Creates a new `GtkRevealer`.
 *
 * Returns: a newly created `GtkRevealer`
 */
GtkWidget *
gtk_revealer_new (void)
{
  return g_object_new (GTK_TYPE_REVEALER, NULL);
}

static GtkRevealerTransitionType
effective_transition (GtkRevealer *revealer)
{
  if (gtk_widget_get_direction (GTK_WIDGET (revealer)) == GTK_TEXT_DIR_RTL)
    {
      if (revealer->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
      else if (revealer->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
      if (revealer->transition_type == GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT)
        return GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT;
      else if (revealer->transition_type == GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT)
        return GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT;
    }

  return revealer->transition_type;
}

static double
get_child_size_scale (GtkRevealer    *revealer,
                      GtkOrientation  orientation)
{
  switch (effective_transition (revealer))
    {
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT:
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        return revealer->current_pos;
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP:
      if (orientation == GTK_ORIENTATION_VERTICAL)
        return revealer->current_pos;
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT:
    case GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT:
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        return sin (G_PI * revealer->current_pos / 2);
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN:
    case GTK_REVEALER_TRANSITION_TYPE_SWING_UP:
      if (orientation == GTK_ORIENTATION_VERTICAL)
        return sin (G_PI * revealer->current_pos / 2);
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_NONE:
    case GTK_REVEALER_TRANSITION_TYPE_CROSSFADE:
    default:
      return 1.0;
    }
}

static void
gtk_revealer_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GskTransform *transform;
  double hscale, vscale;
  int child_width, child_height;

  if (revealer->child == NULL || !gtk_widget_get_visible (revealer->child))
    return;

  if (revealer->current_pos >= 1.0)
    {
      gtk_widget_allocate (revealer->child, width, height, baseline, NULL);
      return;
    }

  hscale = get_child_size_scale (revealer, GTK_ORIENTATION_HORIZONTAL);
  vscale = get_child_size_scale (revealer, GTK_ORIENTATION_VERTICAL);
  if (hscale <= 0 || vscale <= 0)
    {
      /* don't allocate anything, the child is invisible and the numbers
       * don't make sense. */
      return;
    }

  /* We request a different size than the child requested scaled by
   * this scale as it will render smaller from the transition.
   * However, we still want to allocate the child widget with its
   * unscaled size so it renders right instead of e.g. ellipsizing or
   * some other form of clipping. We do this by reverse-applying
   * the scale when size allocating the child.
   *
   * Unfortunately this causes precision issues.
   *
   * So we assume that the fully expanded revealer will likely get
   * an allocation that matches the child's minimum or natural allocation,
   * so we special-case these two values.
   * So when - due to the precision loss - multiple sizes would match
   * the current allocation, we don't pick one at random, we prefer the
   * min and nat size.
   *
   * On top, the scaled size request is always rounded up to an integer.
   * For instance if natural with is 100, and scale is 0.001, we would
   * request a natural size of ceil(0.1) == 1, but reversing this would
   * result in 1 / 0.001 == 1000 (rather than 100).
   * In the swing case we can get the scale arbitrarily near 0 causing
   * arbitrary large problems.
   * These also get avoided by the preference.
   */

  if (hscale < 1.0)
    {
      int min, nat;
      g_assert (vscale == 1.0);
      gtk_widget_measure (revealer->child, GTK_ORIENTATION_HORIZONTAL, height, &min, &nat, NULL, NULL);
      if (ceil (nat * hscale) == width)
        child_width = nat;
      else if (ceil (min * hscale) == width)
        child_width = min;
      else
        child_width = floor (width / hscale);
      child_height = height;
    }
  else if (vscale < 1.0)
    {
      int min, nat;
      child_width = width;
      gtk_widget_measure (revealer->child, GTK_ORIENTATION_VERTICAL, width, &min, &nat, NULL, NULL);
      if (ceil (nat * vscale) == height)
        child_height = nat;
      else if (ceil (min * vscale) == height)
        child_height = min;
      else
        child_height = floor (height / vscale);
    }
  else
    {
      child_width = width;
      child_height = height;
    }

  transform = NULL;
  switch (effective_transition (revealer))
    {
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width - child_width, 0));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, height - child_height));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width, height / 2));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, -90 * (1.0 - revealer->current_pos), graphene_vec3_y_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width, - child_height / 2));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, height / 2));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, 90 * (1.0 - revealer->current_pos), graphene_vec3_y_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, - child_height / 2));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width / 2, 0));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, -90 * (1.0 - revealer->current_pos), graphene_vec3_x_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width / 2, 0));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_UP:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width / 2, height));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, 90 * (1.0 - revealer->current_pos), graphene_vec3_x_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width / 2, - child_height));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_NONE:
    case GTK_REVEALER_TRANSITION_TYPE_CROSSFADE:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP:
    default:
      break;
    }

  gtk_widget_allocate (revealer->child, child_width, child_height, -1, transform);
}

static void
gtk_revealer_set_position (GtkRevealer *revealer,
                           double       pos)
{
  gboolean new_visible;
  GtkRevealerTransitionType transition;

  revealer->current_pos = pos;

  new_visible = revealer->current_pos != 0.0;

  if (revealer->child != NULL &&
      new_visible != gtk_widget_get_child_visible (revealer->child))
    {
      gtk_widget_set_child_visible (revealer->child, new_visible);
      gtk_widget_queue_resize (GTK_WIDGET (revealer));
    }

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_NONE)
    {
      gtk_widget_queue_draw (GTK_WIDGET (revealer));
    }
  else if (transition == GTK_REVEALER_TRANSITION_TYPE_CROSSFADE)
    {
      gtk_widget_set_opacity (GTK_WIDGET (revealer), revealer->current_pos);
      gtk_widget_queue_draw (GTK_WIDGET (revealer));
    }
  else
    {
      gtk_widget_queue_resize (GTK_WIDGET (revealer));
    }

  if (revealer->current_pos == revealer->target_pos)
    g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_CHILD_REVEALED]);
}

static gboolean
gtk_revealer_animate_cb (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  double ease;

  gtk_progress_tracker_advance_frame (&revealer->tracker,
                                      gdk_frame_clock_get_frame_time (frame_clock));
  ease = gtk_progress_tracker_get_ease_out_cubic (&revealer->tracker, FALSE);
  gtk_revealer_set_position (revealer,
                             revealer->source_pos + (ease * (revealer->target_pos - revealer->source_pos)));

  if (gtk_progress_tracker_get_state (&revealer->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      revealer->tick_id = 0;
      return FALSE;
    }

  return TRUE;
}

static void
gtk_revealer_start_animation (GtkRevealer *revealer,
                              double       target)
{
  GtkWidget *widget = GTK_WIDGET (revealer);
  GtkRevealerTransitionType transition;

  if (revealer->target_pos == target)
    return;

  revealer->target_pos = target;
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_REVEAL_CHILD]);

  transition = effective_transition (revealer);
  if (gtk_widget_get_mapped (widget) &&
      revealer->transition_duration != 0 &&
      transition != GTK_REVEALER_TRANSITION_TYPE_NONE &&
      gtk_settings_get_enable_animations (gtk_widget_get_settings (widget)))
    {
      revealer->source_pos = revealer->current_pos;
      if (revealer->tick_id == 0)
        revealer->tick_id =
          gtk_widget_add_tick_callback (widget, gtk_revealer_animate_cb, revealer, NULL);
      gtk_progress_tracker_start (&revealer->tracker,
                                  revealer->transition_duration * 1000,
                                  0,
                                  1.0);
    }
  else
    {
      gtk_revealer_set_position (revealer, target);
    }
}

/**
 * gtk_revealer_set_reveal_child: (attributes org.gtk.Method.set_property=reveal-child)
 * @revealer: a `GtkRevealer`
 * @reveal_child: %TRUE to reveal the child
 *
 * Tells the `GtkRevealer` to reveal or conceal its child.
 *
 * The transition will be animated with the current
 * transition type of @revealer.
 */
void
gtk_revealer_set_reveal_child (GtkRevealer *revealer,
                               gboolean     reveal_child)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (reveal_child)
    gtk_revealer_start_animation (revealer, 1.0);
  else
    gtk_revealer_start_animation (revealer, 0.0);
}

/**
 * gtk_revealer_get_reveal_child: (attributes org.gtk.Method.get_property=reveal-child)
 * @revealer: a `GtkRevealer`
 *
 * Returns whether the child is currently revealed.
 *
 * This function returns %TRUE as soon as the transition
 * is to the revealed state is started. To learn whether
 * the child is fully revealed (ie the transition is completed),
 * use [method@Gtk.Revealer.get_child_revealed].
 *
 * Returns: %TRUE if the child is revealed.
 */
gboolean
gtk_revealer_get_reveal_child (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), FALSE);

  return revealer->target_pos != 0.0;
}

/**
 * gtk_revealer_get_child_revealed: (attributes org.gtk.Method.get_property=child-revealed)
 * @revealer: a `GtkRevealer`
 *
 * Returns whether the child is fully revealed.
 *
 * In other words, this returns whether the transition
 * to the revealed state is completed.
 *
 * Returns: %TRUE if the child is fully revealed
 */
gboolean
gtk_revealer_get_child_revealed (GtkRevealer *revealer)
{
  gboolean animation_finished = (revealer->target_pos == revealer->current_pos);
  gboolean reveal_child = gtk_revealer_get_reveal_child (revealer);

  if (animation_finished)
    return reveal_child;
  else
    return !reveal_child;
}

static void
gtk_revealer_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  double scale;

  scale = get_child_size_scale (revealer, OPPOSITE_ORIENTATION (orientation));

  if (for_size >= 0)
    {
      if (scale == 0)
        return;
      else
        for_size = MIN (G_MAXINT, ceil (for_size / scale));
    }

  if (revealer->child != NULL && _gtk_widget_get_visible (revealer->child))
    {
      gtk_widget_measure (revealer->child,
                          orientation,
                          for_size,
                          minimum, natural,
                          NULL, NULL);
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }

  scale = get_child_size_scale (revealer, orientation);
  *minimum = ceil (*minimum * scale);
  *natural = ceil (*natural * scale);
}

/**
 * gtk_revealer_get_transition_duration: (attributes org.gtk.Method.get_property=transition-duration)
 * @revealer: a `GtkRevealer`
 *
 * Returns the amount of time (in milliseconds) that
 * transitions will take.
 *
 * Returns: the transition duration
 */
guint
gtk_revealer_get_transition_duration (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), 0);

  return revealer->transition_duration;
}

/**
 * gtk_revealer_set_transition_duration: (attributes org.gtk.Method.set_property=transition-duration)
 * @revealer: a `GtkRevealer`
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions will take.
 */
void
gtk_revealer_set_transition_duration (GtkRevealer *revealer,
                                      guint        value)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (revealer->transition_duration == value)
    return;

  revealer->transition_duration = value;
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_TRANSITION_DURATION]);
}

/**
 * gtk_revealer_get_transition_type: (attributes org.gtk.Method.get_property=transition-type)
 * @revealer: a `GtkRevealer`
 *
 * Gets the type of animation that will be used
 * for transitions in @revealer.
 *
 * Returns: the current transition type of @revealer
 */
GtkRevealerTransitionType
gtk_revealer_get_transition_type (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);

  return revealer->transition_type;
}

/**
 * gtk_revealer_set_transition_type: (attributes org.gtk.Method.set_property=transition-type)
 * @revealer: a `GtkRevealer`
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions in @revealer.
 *
 * Available types include various kinds of fades and slides.
 */
void
gtk_revealer_set_transition_type (GtkRevealer               *revealer,
                                  GtkRevealerTransitionType  transition)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (revealer->transition_type == transition)
    return;

  revealer->transition_type = transition;
  gtk_widget_queue_resize (GTK_WIDGET (revealer));
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_TRANSITION_TYPE]);
}

/**
 * gtk_revealer_set_child: (attributes org.gtk.Method.set_property=child)
 * @revealer: a `GtkRevealer`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @revealer.
 */
void
gtk_revealer_set_child (GtkRevealer *revealer,
                        GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));
  g_return_if_fail (child == NULL || revealer->child == child || gtk_widget_get_parent (child) == NULL);

  if (revealer->child == child)
    return;

  g_clear_pointer (&revealer->child, gtk_widget_unparent);

  if (child)
    {
      gtk_widget_set_parent (child, GTK_WIDGET (revealer));
      gtk_widget_set_child_visible (child, revealer->current_pos != 0.0);
      revealer->child = child;
   }

  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_CHILD]);
}

/**
 * gtk_revealer_get_child: (attributes org.gtk.Method.get_property=child)
 * @revealer: a `GtkRevealer`
 *
 * Gets the child widget of @revealer.
 *
 * Returns: (nullable) (transfer none): the child widget of @revealer
 */
GtkWidget *
gtk_revealer_get_child (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), NULL);

  return revealer->child;
}
