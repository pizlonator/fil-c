Title: Paths
Slug: paths

GSK provides a path API that can be used to render more complex
shapes than lines or rounded rectangles. It is comparable to cairos
[path API](https://www.cairographics.org/manual/cairo-Paths.html),
with some notable differences.

In general, a path consists of one or more connected **_contours_**,
each of which may have multiple **_operations_**, and may or may not be closed.
Operations can be straight lines or curves of various kinds. At the points
where the operations connect, the path can have sharp turns.

<figure>
  <picture>
    <source srcset="path-dark.png" media="(prefers-color-scheme: dark)">
    <img alt="A path with multiple contours" src="path-light.png">
  </picture>
  <figcaption>A path with one closed, and one open contour</figcaption>
</figure>

The central object of the GSK path API is the immutable [struct@Gsk.Path]
struct, which contains path data in compact form suitable for rendering.

## Creating Paths

Since `GskPath` is immutable, the auxiliary [struct@Gsk.PathBuilder] struct
can be used to construct a path piece by piece. The pieces are specified with
points, some of which will be on the path, while others are just **_control points_**
that are used to influence the shape of the resulting path.

<figure>
  <picture>
    <source srcset="cubic-dark.png" media="(prefers-color-scheme: dark)">
    <img alt="An cubic Bézier" src="cubic-light.png">
  </picture>
  <figcaption>A cubic Bézier operation, with 2 control points</figcaption>
</figure>

The `GskPathBuilder` API has three distinct groups of functions:

- Functions for building contours from individual operations, like [method@Gsk.PathBuilder.move_to],
  [method@Gsk.PathBuilder.line_to], [method@Gsk.PathBuilder.cubic_to], [method@Gsk.PathBuilder.close]. `GskPathBuilder` maintains a **_current point_**, so these methods all
  take one less points than necessary for the operation (e.g. `gsk_path_builder_line_to`
  only takes a single point and draws a line from the current point to the new point).

- Functions for adding complete contours, such as [method@Gsk.PathBuilder.add_rect],
  [method@Gsk.PathBuilder.add_rounded_rect], [method@Gsk.PathBuilder.add_circle].

- Adding parts of a preexisting path. Functions in this group include
  [method@Gsk.PathBuilder.add_path] and [method@Gsk.PathBuilder.add_segment].

When you are done with building a path, you can convert the accumulated path
data into a `GskPath` struct with [method@Gsk.PathBuilder.free_to_path].

A sometimes convenient alternative is to create a path from a serialized form,
with [func@Gsk.Path.parse]. This function interprets strings in SVG path syntax,
such as:

    M 100 100 C 100 200 200 200 200 100 Z

## Rendering with Paths

There are two main ways to render with paths. The first is to **_fill_** the
interior of a path with a color or more complex content, such as a gradient.
GSK supports different ways of determining what part of the plane are interior
to the path, which can be selected with a [enum@Gsk.FillRule] value.

<figure>
  <picture>
    <img alt="A filled path" src="fill-winding.png">
  </picture>
  <figcaption>A path filled with GSK_FILL_RULE_WINDING</figcaption>
</figure>

<figure>
  <picture>
    <img alt="A filled path" src="fill-even-odd.png">
  </picture>
  <figcaption>The same path, filled with GSK_FILL_RULE_EVEN_ODD</figcaption>
</figure>

To fill a path, use [gtk_snapshot_append_fill()](../gtk4/method.Snapshot.append_fill.html)
or the more general [gtk_snapshot_push_fill()](../gtk4/method.Snapshot.push_fill.html).

Alternatively, a path can be **_stroked_**, which means to emulate drawing
with an idealized pen along the path. The result of stroking a path is another
path (the **_stroke path_**), which is then filled.

The stroke operation can be influenced with the [struct@Gsk.Stroke] struct
that collects various stroke parameters, such as the line width, the style
of line joins and line caps, and a dash pattern.

<figure>
  <picture>
    <img alt="A stroked path" src="stroke-miter.png">
  </picture>
  <figcaption>The same path, stroked with GSK_LINE_JOIN_MITER</figcaption>
</figure>

<figure>
  <picture>
    <img alt="A stroked path" src="stroke-round.png">
  </picture>
  <figcaption>The same path, stroked with GSK_LINE_JOIN_ROUND</figcaption>
</figure>

To stroke a path, use
[gtk_snapshot_append_stroke()](../gtk4/method.Snapshot.append_stroke.html)
or [gtk_snapshot_push_stroke()](../gtk4/method.Snapshot.push_stroke.html).

## Hit testing

When paths are rendered as part of an interactive interface, it is sometimes
necessary to determine whether the mouse points is over the path. GSK provides
[method@Gsk.Path.in_fill] for this purpose.

## Path length

An important property of paths is their **_length_**. Computing it efficiently
requires caching, therefore GSK provides a separate [struct@Gsk.PathMeasure] object
to deal with path lengths. After constructing a `GskPathMeasure` object for a path,
it can be used to determine the length of the path with [method@Gsk.PathMeasure.get_length]
and locate points at a given distance into the path with [method@Gsk.PathMeasure.get_point].

## Other Path APIs

Paths have uses beyond rendering, for example as trajectories in animations.
In such uses, it is often important to access properties of paths, such as
their tangents at certain points. GSK provides an abstract representation
for points on a path in the form of the [struct@Gsk.PathPoint] struct.
You can query properties of a path at certain point once you have a
`GskPathPoint` representing that point.

`GskPathPoint` structs can be compared for equality with [method@Gsk.PathPoint.equal]
and ordered wrt. to which one comes first, using [method@Gsk.PathPoint.compare].

To obtain a `GskPathPoint`, use [method@Gsk.Path.get_closest_point],
[method@Gsk.Path.get_start_point], [method@Gsk.Path.get_end_point] or
[method@Gsk.PathMeasure.get_point].

To query properties of the path at a point, use [method@Gsk.PathPoint.get_position],
[method@Gsk.PathPoint.get_tangent], [method@Gsk.PathPoint.get_rotation],
[method@Gsk.PathPoint.get_curvature] and [method@Gsk.PathPoint.get_distance].

Some of the properties can have different values for the path going into
the point and the path leaving the point, typically at points where the
path takes sharp turns. Examples for this are tangents (which can have up
to 4 different values) and curvatures (which can have two different values).

<figure>
  <picture>
    <source srcset="directions-dark.png" media="(prefers-color-scheme: dark)">
    <img alt="Path Tangents" src="directions-light.png">
  </picture>
  <figcaption>Path Tangents</figcaption>
</figure>

## Going beyond GskPath

Lots of powerful functionality can be implemented for paths:

- Finding intersections
- Offsetting curves
- Turning stroke outlines into paths
- Molding curves (making them pass through a given point)

GSK does not provide API for all of these, but it does offer a way to get at
the underlying Bézier curves, so you can implement such functionality yourself.
You can use [method@Gsk.Path.foreach] to iterate over the operations of the
path, and get the points needed to reconstruct or modify the path piece by piece.

See e.g. the [Primer on Bézier curves](https://pomax.github.io/bezierinfo/)
for inspiration of useful things to explore.
