Title: Coordinate systems in GTK
Slug: gtk-coordinates

All coordinate systems in GTK have the origin at the top left, with the X axis
pointing right, and the Y axis pointing down. This matches the convention used
in X11, Wayland and cairo, but differs from OpenGL and PostScript, where the origin
is in the lower left, and the Y axis is pointing up.

Every widget in a window has its own coordinate system that it uses to place its
child widgets and to interpret events. Most of the time, this fact can be safely
ignored. The section will explain the details for the few cases when it is important.

## The box model

When it comes to rendering, GTK follows the CSS box model as far as practical.

<picture>
  <source srcset="box-model-dark.png" media="(prefers-color-scheme: dark)">
  <img alt="Box Model" src="box-model-light.png">
</picture>

The CSS stylesheet that is in use determines the sizes (and appearance) of the
margin, border and padding areas for each widget. The size of the content area
is determined by GTKs layout algorithm using each widget’s [vfunc@Gtk.Widget.measure]
and [vfunc@Gtk.Widget.size_allocate] vfuncs.

You can learn more about the CSS box model by reading the
[CSS specification](https://www.w3.org/TR/css-box-3/#box-model) or the
Mozilla [documentation](https://developer.mozilla.org/en-US/docs/Learn/CSS/Building_blocks/The_box_model).

To learn more about where GTK CSS differs from CSS on the web, see the
[CSS overview](css-overview.html).

## Widgets

The content area in the CSS box model is the region that the widget considers its own.

The origin of the widget’s coordinate system is the top left corner of the content area,
and its size is the widget’s size. The size can be queried with [method@Gtk.Widget.get_width]
and [method@Gtk.Widget.get_height]. GTK allows general 3D transformations to position
widgets (although most of the time, the transformation will be a simple 2D translation).
The transform to go from one widget’s coordinate system to another one can be obtained
with [method@Gtk.Widget.compute_transform].

In addition to a size, widgets can optionally have a **_baseline_** to position text on.
Containers such as [class@Gtk.Box] may position their children to match up their baselines.
[method@Gtk.Widget.get_baseline] returns the y position of the baseline in widget coordinates.

When widget APIs expect positions or areas, they need to be expressed in this coordinate
system, typically called **_widget coordinates_**. GTK provides a number of APIs to translate
between different widgets' coordinate systems, such as [method@Gtk.Widget.compute_point]
or [method@Gtk.Widget.compute_bounds]. These methods can fail (either because the widgets
don't share a common ancestor, or because of a singular transformation), and callers need
to handle this eventuality.

Another area that is occasionally relevant are the widget’s **_bounds_**, which is the area
that a widget’s rendering is typically confined to (technically, widgets can draw outside
of this area, unless clipping is enforced via the [property@Gtk.Widget:overflow] property).
In CSS terms, the bounds of a widget correspond to the border area.

During GTK's layout algorithm, a parent widget needs to measure each visible child and
allocate them at least as much size as measured. These functions take care of respecting
the CSS box model and widget properties such as align and margin. This happens in the
parent's coordinate system.

Note that the **_text direction_** of a widget does not influence its coordinate
system, but simply determines whether text flows in the direction of increasing
or decreasing X coordinates.

## Events

Event controllers and gestures report positions in the coordinate system of the widget
they are attached to.

If you are dealing with raw events in the form of [class@Gdk.Event] that have positions
associated with them (e.g. the pointer position), such positions are expressed in
**_surface coordinates_**, which have their origin at the top left corner of the
[class@Gdk.Surface].

To translate from surface to widget coordinates, you have to apply the offset from the
top left corner of the surface to the top left corner of the topmost widget, which can
be obtained with [method@Gtk.Native.get_surface_transform].
