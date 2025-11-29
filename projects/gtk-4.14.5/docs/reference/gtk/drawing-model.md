Title: Overview of the drawing model
Slug: drawing-overview

This chapter describes the GTK drawing model in detail. If you
are interested in the procedure which GTK follows to draw its
widgets and windows, you should read this chapter; this will be
useful to know if you decide to implement your own widgets. This
chapter will also clarify the reasons behind the ways certain
things are done in GTK.

## Windows and events

Applications that use a windowing system generally create
rectangular regions in the screen called _surfaces_ (GTK is
following the Wayland terminology, other windowing systems
such as X11 may call these _windows_). Traditional windowing
systems do not automatically save the graphical content of
surfaces, and instead ask applications to provide new content
whenever it is needed. For example, if a window that is stacked
below other windows gets raised to the top, then the application
has to repaint it, so the previously obscured area can be shown.
When the windowing system asks an application to redraw a window,
it sends a _frame event_ (_expose event_ in X11 terminology)
for that window.

Each GTK toplevel window or dialog is associated with a
windowing system surface. Child widgets such as buttons or
entries don't have their own surface; they use the surface
of their toplevel.

Generally, the drawing cycle begins when GTK receives a frame event
from the underlying windowing system: if the user drags a window
over another one, the windowing system will tell the underlying
surface that it needs to repaint itself. The drawing cycle can
also be initiated when a widget itself decides that it needs to
update its display. For example, when the user types a character
in an entry widget, the entry asks GTK to queue a redraw operation
for itself.

The windowing system generates frame events for surfaces. The GDK
interface to the windowing system translates such events into
emissions of the ::render signal on the affected surfaces. The GTK
toplevel window connects to that signal, and reacts appropriately.

The following sections describe how GTK decides which widgets
need to be repainted in response to such events, and how widgets
work internally in terms of the resources they use from the
windowing system.

## The frame clock

All GTK applications are mainloop-driven, which means that most
of the time the app is idle inside a loop that just waits for
something to happen and then calls out to the right place when
it does. On top of this GTK has a frame clock that gives a
“pulse” to the application. This clock beats at a steady rate,
which is tied to the framerate of the output (this is synced to
the monitor via the window manager/compositor). A typical
refresh rate is 60 frames per second, so a new “pulse” happens
roughly every 16 milliseconds.

The clock has several phases:

- Events
- Update
- Layout
- Paint

 The phases happens in this order and we will always run each
 phase through before going back to the start.

The Events phase is a stretch of time between each redraw where
GTK processes input events from the user and other events
(like e.g. network I/O). Some events, like mouse motion are
compressed so that only a single mouse motion event per clock
cycle needs to be handled.

Once the Events phase is over, external events are paused and
the redraw loop is run. First is the Update phase, where all
animations are run to calculate the new state based on the
estimated time the next frame will be visible (available via
the frame clock). This often involves geometry changes which
drive the next phase, Layout. If there are any changes in
widget size requirements the new layout is calculated for the
widget hierarchy (i.e. sizes and positions for all widgets are
determined). Then comes the Paint phase, where we redraw the
regions of the window that need redrawing.

If nothing requires the Update/Layout/Paint phases we will
stay in the Events phase forever, as we don’t want to redraw
if nothing changes. Each phase can request further processing
in the following phases (e.g. the Update phase will cause there
to be layout work, and layout changes cause repaints).

There are multiple ways to drive the clock, at the lowest level you
can request a particular phase with gdk_frame_clock_request_phase()
which will schedule a clock beat as needed so that it eventually
reaches the requested phase. However, in practice most things
happen at higher levels:

- If you are doing an animation, you can use
  gtk_widget_add_tick_callback() which will cause a regular
  beating of the clock with a callback in the Update phase
  until you stop the tick.
- If some state changes that causes the size of your widget to
  change you call gtk_widget_queue_resize() which will request
  a Layout phase and mark your widget as needing relayout.
- If some state changes so you need to redraw some area of
  your widget you use the normal gtk_widget_queue_draw()
  set of functions. These will request a Paint phase and
  mark the region as needing redraw.

There are also a lot of implicit triggers of these from the
CSS layer (which does animations, resizes and repaints as needed).

## The scene graph

The first step in “drawing” a window is that GTK creates
_render nodes_ for all the widgets in the window. The render
nodes are combined into a tree that you can think of as a
_scene graph_ describing your window contents.

Render nodes belong to the GSK layer, and there are various kinds
of them, for the various kinds of drawing primitives you are likely
to need when translating widget content and CSS styling. Typical
examples are text nodes, gradient nodes, texture nodes or clip nodes.

In the past, all drawing in GTK happened via cairo. It is still possible
to use cairo for drawing your custom widget contents, by using a cairo
render node.

A GSK _renderer_ takes these render nodes, transforms them into
rendering commands for the drawing API it targets, and arranges
for the resulting drawing to be associated with the right surface.
GSK has renderers for OpenGL, Vulkan and cairo.

## Hierarchical drawing

During the Paint phase GTK receives a single ::render signal on the
toplevel surface. The signal handler will create a snapshot object
(which is a helper for creating a scene graph) and call the
GtkWidget snapshot() vfunc, which will propagate down the widget
hierarchy. This lets each widget snapshot its content at the right
place and time, correctly handling things like partial transparencies
and overlapping widgets.

During the snapshotting of each widget, GTK automatically handles
the CSS rendering according to the CSS box model. It snapshots first
the background, then the border, then the widget content itself, and
finally the outline.

To avoid excessive work when generating scene graphs, GTK caches render
nodes. Each widget keeps a reference to its render node (which in turn,
will refer to the render nodes of children, and grandchildren, and so
on), and will reuse that node during the Paint phase. Invalidating a
widget (by calling gtk_widget_queue_draw()) discards the cached render
node, forcing the widget to regenerate it the next time it needs to
produce a snapshot.
