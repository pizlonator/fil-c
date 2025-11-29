Title: Cairo interaction

## Functions to support using cairo

[Cairo](http://cairographics.org) is a graphics library that supports vector
graphics and image compositing that can be used with GTK.

GDK does not wrap the Cairo API and it is not possible to use cairo directly
to draw on a [class@Gdk.Surface]. You can either use a
[GtkDrawingArea](../gtk4/class.DrawingArea.html) widget or
[gtk_snapshot_append_cairo](../gtk4/func.Snapshot.append_cairo.html)
for drawing with cairo in a GTK4 application.

Additional functions allow use [struct@Gdk.Rectangle]s with Cairo
and to use [struct@Gdk.RGBA], `GdkPixbuf`, and [class@Gdk.Surface]
instances as sources for drawing operations.

For more information on Cairo, please see the
[Cairo API reference](https://www.cairographics.org/manual/).
