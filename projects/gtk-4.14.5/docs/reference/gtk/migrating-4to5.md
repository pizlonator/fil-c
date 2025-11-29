Title: Preparing for GTK 5
Slug: gtk-migrating-4-to-5

GTK 5 will be a major new version of GTK that breaks both API and
ABI compared to GTK 4.x. GTK 5 does not exist yet, so we cannot
be entirely sure what will be involved in a migration from GTK 4
to GTK 5. But we can already give some preliminary hints about
the likely changes, and how to prepare for them in code that is
using GTK 4.

### Do not use deprecated symbols

As always, functions and types that are known to go away in the
next major version of GTK are being marked as deprecated in GTK 4.

Removing the use of deprecated APIs is the most important step
to prepare your code for the next major version of GTK. Often,
deprecation notes will include hints about replacement APIs to
help you with this.

Sometimes, it is helpful to have some background information about
the motivation and goals of larger API changes.

## Cell renderers are going away

Cell renderers were introduced in GTK 2 to support rendering of
"big data" UIs, in particular treeviews. Over the years, more
"data-like" widgets have started to use them, and cell renderers
have grown into a shadowy, alternative rendering infrastructure
that duplicates much of what widgets do, while duplicating the
code and adding their own dose of bugs.

In GTK 4, replacement widgets for GtkTreeView, GtkIconView and
GtkComboBox have appeared: GtkListView, GtkColumnView, GtkGridView
and GtkDropDown. For GTK 5, we will take the next step and remove
all cell renderer-based widgets.

## Themed rendering APIs are going away

The old GTK 2 era rendering APIs for theme components like
gtk_render_frame() or gtk_render_check() have not been used by
GTK itself even in later GTK 3, but they have been kept around
for the benefit of "external drawing" users - applications that
want their controls to look like GTK without using widgets.

Supporting this is increasingly getting in the way of making
the GTK CSS machinery fast and correct. One notable problem is
that temporary style changes (using gtk_style_context_save())
is breaking animations. Therefore, these APIs will be going away
in GTK 5, together with their more modern GtkSnapshot variants
like gtk_snapshot_render_background() or gtk_snapshot_render_focus().

The best way to render parts of your widget using CSS styling
is to use subwidgets. For example, to show a piece of text with
fonts, effects and shadows according to the current CSS style,
use a GtkLabel.

If you have a need for custom drawing that fits into the current
(dark or light) theme, e.g. for rendering a graph, you can still
get the current style foreground color, using
[method@Gtk.Widget.get_style_color].

## Local stylesheets are going away

The cascading part of GTK's CSS implementation is complicated by
the existence of local stylesheets (i.e. those added with
gtk_style_context_add_provider()). And local stylesheets are
unintuitive in that they do not apply to the whole subtree of
widgets, but just to the one widget where the stylesheet was
added.

GTK 5 will no longer provide this functionality. The recommendations
is to use a global stylesheet (i.e. gtk_style_context_add_provider_for_display())
and rely on style classes to make your CSS apply only where desired.

## Chooser interfaces are going away

The GtkColorChooser, GtkFontChooser, GtkFileChooser and GtkAppChooser
interfaces and their implementations as dialogs, buttons and widgets
are phased out. The are being replaced by a new family of async APIs
that will be more convenient to use from language bindings, in particular
for languages that have concepts like promises. The new APIs are
[class@Gtk.ColorDialog], [class@Gtk.FontDialog] and [class@Gtk.FileDialog],
There are also equivalents for some of the 'button' widgets:
[class@Gtk.ColorDialogButton], [class@Gtk.FontDialogButton].

## GtkMessageDialog is going away

Like the Chooser interfaces, GtkMessageDialog has been replaced by
a new async API that will be more convenient, in particular for
language binding. The new API is [class@Gtk.AlertDialog].

## GtkDialog is going away

After gtk_dialog_run() was removed, the usefulness of GtkDialog
is much reduced, and it has awkward, archaice APIs. Therefore,
it is dropped. The recommended replacement is to just create
your own window and add buttons as required, either in the header
or elsewhere.

## GtkInfoBar is going away

GtkInfoBar had a dialog API, and with dialogs going away, it was time to
retire it. If you need such a widget, it is relatively trivial to create one
using a [class@Gtk.Revealer] with labels and buttons.

Other libraries, such as libadwaita, may provide replacements as well.

## gtk_show_uri is being replaced

Instead of gtk_show_uri(), you should use GtkUriLauncher or GtkFileLauncher.

## GtkStatusbar is going away

This is an oldfashioned widget that does not do all that much anymore, since
it no longer has a resize handle for the window.

## GtkLockButton and GtkVolumeButton are going away

These are very specialized widgets that should better live with the application
where they are used.

## Widget size api changes

The functions gtk_widget_get_allocated_width() and gtk_widget_get_allocated_height()
are going away. In most cases, [method@Gtk.Widget.get_width] and [method@Gtk.Widget.get_height]
are suitable replacements. Note that the semantics are slightly different though:
the old functions return the size of the CSS border area, while the new functions return
the size of the widgets content area. In places where this difference matters, you can
use `gtk_widget_compute_bounds (widget, widget, &bounds)` instead.

The function gtk_widget_get_allocation() is also going away. It does not have a direct
replacement, but the previously mentioned alternatives can be used for it too.

The function gtk_widget_get_allocated_baseline() has been renamed to [method@Gtk.Widget.get_baseline].

## Stop using GdkPixbuf

GTK is moving away from GdkPixbuf as the primary API for transporting image data, in favor
of GdkTexture. APIs that are accepting or returning GdkPixbufs are being replaced by equivalent
APIs using GdkTexture or GdkPaintable objects.
