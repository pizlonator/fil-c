Title: Drag-and-Drop in GTK

Drag-and-Drop (DND) is a user interaction pattern where users drag a UI element
from one place to another, either inside a single application or between
different application windows.

When the element is 'dropped', data is transferred from the source to the
destination, according to the drag action that is negotiated between both
sides. Most commonly, that is a _copy_, but it can also be a _move_ or a
_link_, depending on the kind of data and how the drag operation has been
set up.

This chapter gives an overview over how Drag-and-Drop is handled with event
controllers in GTK.

## Drag sources

To make data available via DND, you create a [class@Gtk.DragSource]. Drag sources
are event controllers, which initiate a Drag-and-Drop operation when the user clicks
and drags the widget.

A drag source can be set up ahead of time, with the desired drag action(s) and the data
to be transferred. But it is also possible to provide the data when a drag operation
is about to begin, by connecting to the [signal@Gtk.DragSource::prepare] signal.

The GtkDragSource emits the [signal@Gtk.DragSource::drag-begin] signal when the DND
operation starts, and the [signal@Gtk.DragSource::drag-end] signal when it is done.
But it is not normally necessary to handle these signals. One case in which a ::drag-end
handler is necessary is to implement `GDK_ACTION_MOVE`.

## Drop targets

To receive data via DND, you create a [class@Gtk.DropTarget], and tell it what kind of
data to accept. You need to connect to the [signal@Gtk.DropTarget::drop] signal to receive
the data when a DND operation occurs.

While a DND operation is ongoing, GTK provides updates when the pointer moves over
the widget to which the drop target is associated. The [signal@Gtk.DropTarget::enter],
[signal@Gtk.DropTarget::leave] and [signal@Gtk.DropTarget::motion] signals get emitted
for this purpose.

GtkDropTarget provides a simple API, and only provides the data when it has been completely
transferred. If you need to handle the data transfer yourself (for example to provide progress
information during the transfer), you can use the more complicated [class@Gtk.DropTargetAsync].

## Other considerations

It is sometimes necessary to update the UI of the destination while a DND operation is ongoing,
say to scroll or expand a view, or to switch pages. Typically, such UI changes are triggered
by hovering over the widget in question.

[class@Gtk.DropControllerMotion] is an event controller that can help with implementing such
behaviors. It is very similar to [class@Gtk.EventControllerMotion], but provides events during
a DND operation.
