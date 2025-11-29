/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkpaintable.h"

#include "gdksnapshotprivate.h"
#include "gdkprivate.h"

#include <graphene.h>

/* HACK: So we don't need to include any (not-yet-created) GSK or GTK headers */
GdkSnapshot *   gtk_snapshot_new                        (void);
void            gtk_snapshot_push_debug                 (GdkSnapshot            *snapshot,
                                                         const char             *message,
                                                         ...) G_GNUC_PRINTF (2, 3);
void            gtk_snapshot_pop                        (GdkSnapshot            *snapshot);
GdkPaintable *  gtk_snapshot_free_to_paintable          (GdkSnapshot            *snapshot,
                                                         const graphene_size_t  *size);

/**
 * GdkPaintable:
 *
 * `GdkPaintable` is a simple interface used by GTK to represent content that
 * can be painted.
 *
 * The content of a `GdkPaintable` can be painted anywhere at any size
 * without requiring any sort of layout. The interface is inspired by
 * similar concepts elsewhere, such as
 * [ClutterContent](https://developer.gnome.org/clutter/stable/ClutterContent.html),
 * [HTML/CSS Paint Sources](https://www.w3.org/TR/css-images-4/#paint-source),
 * or [SVG Paint Servers](https://www.w3.org/TR/SVG2/pservers.html).
 *
 * A `GdkPaintable` can be snapshot at any time and size using
 * [method@Gdk.Paintable.snapshot]. How the paintable interprets that size and
 * if it scales or centers itself into the given rectangle is implementation
 * defined, though if you are implementing a `GdkPaintable` and don't know what
 * to do, it is suggested that you scale your paintable ignoring any potential
 * aspect ratio.
 *
 * The contents that a `GdkPaintable` produces may depend on the [class@Gdk.Snapshot]
 * passed to it. For example, paintables may decide to use more detailed images
 * on higher resolution screens or when OpenGL is available. A `GdkPaintable`
 * will however always produce the same output for the same snapshot.
 *
 * A `GdkPaintable` may change its contents, meaning that it will now produce
 * a different output with the same snapshot. Once that happens, it will call
 * [method@Gdk.Paintable.invalidate_contents] which will emit the
 * [signal@Gdk.Paintable::invalidate-contents] signal. If a paintable is known
 * to never change its contents, it will set the %GDK_PAINTABLE_STATIC_CONTENTS
 * flag. If a consumer cannot deal with changing contents, it may call
 * [method@Gdk.Paintable.get_current_image] which will return a static
 * paintable and use that.
 *
 * A paintable can report an intrinsic (or preferred) size or aspect ratio it
 * wishes to be rendered at, though it doesn't have to. Consumers of the interface
 * can use this information to layout thepaintable appropriately. Just like the
 * contents, the size of a paintable can change. A paintable will indicate this
 * by calling [method@Gdk.Paintable.invalidate_size] which will emit the
 * [signal@Gdk.Paintable::invalidate-size] signal. And just like for contents,
 * if a paintable is known to never change its size, it will set the
 * %GDK_PAINTABLE_STATIC_SIZE flag.
 *
 * Besides API for applications, there are some functions that are only
 * useful for implementing subclasses and should not be used by applications:
 * [method@Gdk.Paintable.invalidate_contents],
 * [method@Gdk.Paintable.invalidate_size],
 * [func@Gdk.Paintable.new_empty].
 */

G_DEFINE_INTERFACE (GdkPaintable, gdk_paintable, G_TYPE_OBJECT)

enum {
  INVALIDATE_CONTENTS,
  INVALIDATE_SIZE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gdk_paintable_default_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  g_critical ("Paintable of type '%s' does not implement GdkPaintable::snapshot", G_OBJECT_TYPE_NAME (paintable));
}

static GdkPaintable *
gdk_paintable_default_get_current_image (GdkPaintable *paintable)
{
  int width, height;
  GdkSnapshot *snapshot;

  /* No need to check whether the paintable is static, as
   * gdk_paintable_get_current_image () takes care of that already.  */

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  if (width <= 0 || height <= 0)
    return gdk_paintable_new_empty (width, height);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  return gtk_snapshot_free_to_paintable (snapshot, NULL);
}

static GdkPaintableFlags
gdk_paintable_default_get_flags (GdkPaintable *paintable)
{
  return 0;
}

static int
gdk_paintable_default_get_intrinsic_width (GdkPaintable *paintable)
{
  return 0;
}

static int
gdk_paintable_default_get_intrinsic_height (GdkPaintable *paintable)
{
  return 0;
}

static double gdk_paintable_default_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  int width, height;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  if (width <= 0 || height <= 0)
    return 0.0;

  return (double) width / height;
};

static void
g_value_object_transform_value (const GValue *src_value,
				GValue       *dest_value)
{
  if (src_value->data[0].v_pointer && g_type_is_a (G_OBJECT_TYPE (src_value->data[0].v_pointer), G_VALUE_TYPE (dest_value)))
    dest_value->data[0].v_pointer = g_object_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static void
gdk_paintable_default_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gdk_paintable_default_snapshot;
  iface->get_current_image = gdk_paintable_default_get_current_image;
  iface->get_flags = gdk_paintable_default_get_flags;
  iface->get_intrinsic_width = gdk_paintable_default_get_intrinsic_width;
  iface->get_intrinsic_height = gdk_paintable_default_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gdk_paintable_default_get_intrinsic_aspect_ratio;

  g_value_register_transform_func (G_TYPE_OBJECT, GDK_TYPE_PAINTABLE, g_value_object_transform_value);
  g_value_register_transform_func (GDK_TYPE_PAINTABLE, G_TYPE_OBJECT, g_value_object_transform_value);

  /**
   * GdkPaintable::invalidate-contents
   * @paintable: a `GdkPaintable`
   *
   * Emitted when the contents of the @paintable change.
   *
   * Examples for such an event would be videos changing to the next frame or
   * the icon theme for an icon changing.
   */
  signals[INVALIDATE_CONTENTS] =
    g_signal_new (I_("invalidate-contents"),
                  GDK_TYPE_PAINTABLE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkPaintable::invalidate-size
   * @paintable: a `GdkPaintable`
   *
   * Emitted when the intrinsic size of the @paintable changes.
   *
   * This means the values reported by at least one of
   * [method@Gdk.Paintable.get_intrinsic_width],
   * [method@Gdk.Paintable.get_intrinsic_height] or
   * [method@Gdk.Paintable.get_intrinsic_aspect_ratio]
   * has changed.
   *
   * Examples for such an event would be a paintable displaying
   * the contents of a toplevel surface being resized.
   */
  signals[INVALIDATE_SIZE] =
    g_signal_new (I_("invalidate-size"),
                  GDK_TYPE_PAINTABLE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

/**
 * gdk_paintable_snapshot:
 * @paintable: a `GdkPaintable`
 * @snapshot: a `GdkSnapshot` to snapshot to
 * @width: width to snapshot in
 * @height: height to snapshot in
 *
 * Snapshots the given paintable with the given @width and @height.
 *
 * The paintable is drawn at the current (0,0) offset of the @snapshot.
 * If @width and @height are not larger than zero, this function will
 * do nothing.
 */
void
gdk_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  GdkPaintableInterface *iface;

  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (snapshot != NULL);

  if (width <= 0.0 || height <= 0.0)
    return;

  gtk_snapshot_push_debug (snapshot, "%s %p @ %gx%g", G_OBJECT_TYPE_NAME (paintable), paintable, width, height);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  iface->snapshot (paintable, snapshot, width, height);

  gtk_snapshot_pop (snapshot);
}

#define GDK_PAINTABLE_IMMUTABLE (GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS)
static inline gboolean
gdk_paintable_is_immutable (GdkPaintable *paintable)
{
  return (gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_IMMUTABLE) == GDK_PAINTABLE_IMMUTABLE;
}

/**
 * gdk_paintable_get_current_image:
 * @paintable: a `GdkPaintable`
 *
 * Gets an immutable paintable for the current contents displayed by @paintable.
 *
 * This is useful when you want to retain the current state of an animation,
 * for example to take a screenshot of a running animation.
 *
 * If the @paintable is already immutable, it will return itself.
 *
 * Returns: (transfer full): An immutable paintable for the current
 *   contents of @paintable
 */
GdkPaintable *
gdk_paintable_get_current_image (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);

  if (gdk_paintable_is_immutable (paintable))
    return g_object_ref (paintable);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_current_image (paintable);
}

/**
 * gdk_paintable_get_flags:
 * @paintable: a `GdkPaintable`
 *
 * Get flags for the paintable.
 *
 * This is oftentimes useful for optimizations.
 *
 * See [flags@Gdk.PaintableFlags] for the flags and what they mean.
 *
 * Returns: The `GdkPaintableFlags` for this paintable
 */
GdkPaintableFlags
gdk_paintable_get_flags (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_flags (paintable);
}

/**
 * gdk_paintable_get_intrinsic_width:
 * @paintable: a `GdkPaintable`
 *
 * Gets the preferred width the @paintable would like to be displayed at.
 *
 * Consumers of this interface can use this to reserve enough space to draw
 * the paintable.
 *
 * This is a purely informational value and does not in any way limit the
 * values that may be passed to [method@Gdk.Paintable.snapshot].
 *
 * If the @paintable does not have a preferred width, it returns 0.
 * Negative values are never returned.
 *
 * Returns: the intrinsic width of @paintable or 0 if none.
 */
int
gdk_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_width (paintable);
}

/**
 * gdk_paintable_get_intrinsic_height:
 * @paintable: a `GdkPaintable`
 *
 * Gets the preferred height the @paintable would like to be displayed at.
 *
 * Consumers of this interface can use this to reserve enough space to draw
 * the paintable.
 *
 * This is a purely informational value and does not in any way limit the
 * values that may be passed to [method@Gdk.Paintable.snapshot].
 *
 * If the @paintable does not have a preferred height, it returns 0.
 * Negative values are never returned.
 *
 * Returns: the intrinsic height of @paintable or 0 if none.
 */
int
gdk_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_height (paintable);
}

/**
 * gdk_paintable_get_intrinsic_aspect_ratio:
 * @paintable: a `GdkPaintable`
 *
 * Gets the preferred aspect ratio the @paintable would like to be displayed at.
 *
 * The aspect ratio is the width divided by the height, so a value of 0.5
 * means that the @paintable prefers to be displayed twice as high as it
 * is wide. Consumers of this interface can use this to preserve aspect
 * ratio when displaying the paintable.
 *
 * This is a purely informational value and does not in any way limit the
 * values that may be passed to [method@Gdk.Paintable.snapshot].
 *
 * Usually when a @paintable returns nonzero values from
 * [method@Gdk.Paintable.get_intrinsic_width] and
 * [method@Gdk.Paintable.get_intrinsic_height] the aspect ratio
 * should conform to those values, though that is not required.
 *
 * If the @paintable does not have a preferred aspect ratio,
 * it returns 0. Negative values are never returned.
 *
 * Returns: the intrinsic aspect ratio of @paintable or 0 if none.
 */
double
gdk_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_aspect_ratio (paintable);
}

/**
 * gdk_paintable_invalidate_contents:
 * @paintable: a `GdkPaintable`
 *
 * Called by implementations of `GdkPaintable` to invalidate their contents.
 *
 * Unless the contents are invalidated, implementations must guarantee that
 * multiple calls of [method@Gdk.Paintable.snapshot] produce the same output.
 *
 * This function will emit the [signal@Gdk.Paintable::invalidate-contents]
 * signal.
 *
 * If a @paintable reports the %GDK_PAINTABLE_STATIC_CONTENTS flag,
 * it must not call this function.
 */
void
gdk_paintable_invalidate_contents (GdkPaintable *paintable)
{
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (!(gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_STATIC_CONTENTS));

  g_signal_emit (paintable, signals[INVALIDATE_CONTENTS], 0);
}

/**
 * gdk_paintable_invalidate_size:
 * @paintable: a `GdkPaintable`
 *
 * Called by implementations of `GdkPaintable` to invalidate their size.
 *
 * As long as the size is not invalidated, @paintable must return the same
 * values for its intrinsic width, height and aspect ratio.
 *
 * This function will emit the [signal@Gdk.Paintable::invalidate-size]
 * signal.
 *
 * If a @paintable reports the %GDK_PAINTABLE_STATIC_SIZE flag,
 * it must not call this function.
 */
void
gdk_paintable_invalidate_size (GdkPaintable *paintable)
{
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (!(gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_STATIC_SIZE));

  g_signal_emit (paintable, signals[INVALIDATE_SIZE], 0);
}

/**
 * gdk_paintable_compute_concrete_size:
 * @paintable: a `GdkPaintable`
 * @specified_width: the width @paintable could be drawn into or
 *   0.0 if unknown
 * @specified_height: the height @paintable could be drawn into or
 *   0.0 if unknown
 * @default_width: the width @paintable would be drawn into if
 *   no other constraints were given
 * @default_height: the height @paintable would be drawn into if
 *   no other constraints were given
 * @concrete_width: (out): will be set to the concrete width computed
 * @concrete_height: (out): will be set to the concrete height computed
 *
 * Compute a concrete size for the `GdkPaintable`.
 *
 * Applies the sizing algorithm outlined in the
 * [CSS Image spec](https://drafts.csswg.org/css-images-3/#default-sizing)
 * to the given @paintable. See that link for more details.
 *
 * It is not necessary to call this function when both @specified_width
 * and @specified_height are known, but it is useful to call this
 * function in GtkWidget:measure implementations to compute the
 * other dimension when only one dimension is given.
 */
void
gdk_paintable_compute_concrete_size (GdkPaintable *paintable,
                                     double        specified_width,
                                     double        specified_height,
                                     double        default_width,
                                     double        default_height,
                                     double       *concrete_width,
                                     double       *concrete_height)
{
  double image_width, image_height, image_aspect;

  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (specified_width >= 0);
  g_return_if_fail (specified_height >= 0);
  g_return_if_fail (default_width > 0);
  g_return_if_fail (default_height > 0);
  g_return_if_fail (concrete_width != NULL);
  g_return_if_fail (concrete_height != NULL);

  /* If the specified size is a definite width and height,
   * the concrete object size is given that width and height.
   */
  if (specified_width && specified_height)
    {
      *concrete_width = specified_width;
      *concrete_height = specified_height;
      return;
    }

  image_width  = gdk_paintable_get_intrinsic_width (paintable);
  image_height = gdk_paintable_get_intrinsic_height (paintable);
  image_aspect = gdk_paintable_get_intrinsic_aspect_ratio (paintable);

  /* If the specified size has neither a definite width nor height,
   * and has no additional constraints, the dimensions of the concrete
   * object size are calculated as follows:
   */
  if (specified_width == 0.0 && specified_height == 0.0)
    {
      /* If the object has only an intrinsic aspect ratio,
       * the concrete object size must have that aspect ratio,
       * and additionally be as large as possible without either
       * its height or width exceeding the height or width of the
       * default object size.
       */
      if (image_aspect > 0 && image_width == 0 && image_height == 0)
        {
          if (image_aspect * default_height > default_width)
            {
              *concrete_width = default_width;
              *concrete_height = default_width / image_aspect;
            }
          else
            {
              *concrete_width = default_height * image_aspect;
              *concrete_height = default_height;
            }
        }
      else
        {
          /* Otherwise, the width and height of the concrete object
           * size is the same as the object's intrinsic width and
           * intrinsic height, if they exist.
           * If the concrete object size is still missing a width or
           * height, and the object has an intrinsic aspect ratio,
           * the missing dimension is calculated from the present
           * dimension and the intrinsic aspect ratio.
           * Otherwise, the missing dimension is taken from the default
           * object size.
           */
          if (image_width)
            *concrete_width = image_width;
          else if (image_aspect)
            *concrete_width = image_height * image_aspect;
          else
            *concrete_width = default_width;

          if (image_height)
            *concrete_height = image_height;
          else if (image_aspect)
            *concrete_height = image_width / image_aspect;
          else
            *concrete_height = default_height;
        }

      return;
    }

  /* If the specified size has only a width or height, but not both,
   * then the concrete object size is given that specified width or height.
   * The other dimension is calculated as follows:
   * If the object has an intrinsic aspect ratio, the missing dimension of
   * the concrete object size is calculated using the intrinsic aspect-ratio
   * and the present dimension.
   * Otherwise, if the missing dimension is present in the object's intrinsic
   * dimensions, the missing dimension is taken from the object's intrinsic
   * dimensions.
   * Otherwise, the missing dimension of the concrete object size is taken
   * from the default object size.
   */
  if (specified_width)
    {
      *concrete_width = specified_width;
      if (image_aspect)
        *concrete_height = specified_width / image_aspect;
      else if (image_height)
        *concrete_height = image_height;
      else
        *concrete_height = default_height;
    }
  else
    {
      *concrete_height = specified_height;
      if (image_aspect)
        *concrete_width = specified_height * image_aspect;
      else if (image_width)
        *concrete_width = image_width;
      else
        *concrete_width = default_width;
    }
}

#define GDK_TYPE_EMPTY_PAINTABLE (gdk_empty_paintable_get_type())
static
G_DECLARE_FINAL_TYPE(GdkEmptyPaintable, gdk_empty_paintable, GDK, EMPTY_PAINTABLE, GObject)

struct _GdkEmptyPaintable
{
  GObject parent_instance;

  int width;
  int height;
};

struct _GdkEmptyPaintableClass
{
  GObjectClass parent_class;
};

static void
gdk_empty_paintable_snapshot (GdkPaintable *paintable,
                              GdkSnapshot  *snapshot,
                              double        width,
                              double        height)
{
}

static GdkPaintableFlags
gdk_empty_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE
       | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
gdk_empty_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GdkEmptyPaintable *self = GDK_EMPTY_PAINTABLE (paintable);

  return self->width;
}

static int
gdk_empty_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GdkEmptyPaintable *self = GDK_EMPTY_PAINTABLE (paintable);

  return self->height;
}

static void
gdk_empty_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gdk_empty_paintable_snapshot;
  iface->get_flags = gdk_empty_paintable_get_flags;
  iface->get_intrinsic_width = gdk_empty_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gdk_empty_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (GdkEmptyPaintable, gdk_empty_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gdk_empty_paintable_paintable_init))

static void
gdk_empty_paintable_class_init (GdkEmptyPaintableClass *klass)
{
}

static void
gdk_empty_paintable_init (GdkEmptyPaintable *self)
{
}

/**
 * gdk_paintable_new_empty:
 * @intrinsic_width: The intrinsic width to report. Can be 0 for no width.
 * @intrinsic_height: The intrinsic height to report. Can be 0 for no height.
 *
 * Returns a paintable that has the given intrinsic size and draws nothing.
 *
 * This is often useful for implementing the
 * [vfunc@Gdk.Paintable.get_current_image] virtual function
 * when the paintable is in an incomplete state (like a
 * [GtkMediaStream](../gtk4/class.MediaStream.html) before receiving
 * the first frame).
 *
 * Returns: (transfer full): a `GdkPaintable`
 */
GdkPaintable *
gdk_paintable_new_empty (int intrinsic_width,
                         int intrinsic_height)
{
  GdkEmptyPaintable *result;

  g_return_val_if_fail (intrinsic_width >= 0, NULL);
  g_return_val_if_fail (intrinsic_height >= 0, NULL);

  result = g_object_new (GDK_TYPE_EMPTY_PAINTABLE, NULL);

  result->width = intrinsic_width;
  result->height = intrinsic_height;

  return GDK_PAINTABLE (result);
}
