/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkwidgetprivate.h"

#include "gtkaccelgroupprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkactionobserverprivate.h"
#include "gtkapplicationprivate.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkconstraint.h"
#include "gtkcssboxesprivate.h"
#include "gtkcssfiltervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssfontvariationsvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtkdebug.h"
#include "gtkgestureprivate.h"
#include "gtklayoutmanagerprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtknative.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrootprivate.h"
#include "gtknativeprivate.h"
#include "gtkscrollable.h"
#include "gtksettingsprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcutcontrollerprivate.h"
#include "gtkshortcutmanager.h"
#include "gtkshortcutmanagerprivate.h"
#include "gtkshortcuttrigger.h"
#include "gtksizegroup-private.h"
#include "gtksnapshotprivate.h"
#include "deprecated/gtkstylecontextprivate.h"
#include "gtktooltipprivate.h"
#include "gsktransformprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpaintableprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"
#include "gtktestatcontextprivate.h"

#include "inspector/window.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gsk/gskdebugprivate.h"
#include "gsk/gskrendererprivate.h"

#include <cairo-gobject.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

/**
 * GtkWidget:
 *
 * The base class for all widgets.
 *
 * `GtkWidget` is the base class all widgets in GTK derive from. It manages the
 * widget lifecycle, layout, states and style.
 *
 * ### Height-for-width Geometry Management
 *
 * GTK uses a height-for-width (and width-for-height) geometry management
 * system. Height-for-width means that a widget can change how much
 * vertical space it needs, depending on the amount of horizontal space
 * that it is given (and similar for width-for-height). The most common
 * example is a label that reflows to fill up the available width, wraps
 * to fewer lines, and therefore needs less height.
 *
 * Height-for-width geometry management is implemented in GTK by way
 * of two virtual methods:
 *
 * - [vfunc@Gtk.Widget.get_request_mode]
 * - [vfunc@Gtk.Widget.measure]
 *
 * There are some important things to keep in mind when implementing
 * height-for-width and when using it in widget implementations.
 *
 * If you implement a direct `GtkWidget` subclass that supports
 * height-for-width or width-for-height geometry management for itself
 * or its child widgets, the [vfunc@Gtk.Widget.get_request_mode] virtual
 * function must be implemented as well and return the widget's preferred
 * request mode. The default implementation of this virtual function
 * returns %GTK_SIZE_REQUEST_CONSTANT_SIZE, which means that the widget will
 * only ever get -1 passed as the for_size value to its
 * [vfunc@Gtk.Widget.measure] implementation.
 *
 * The geometry management system will query a widget hierarchy in
 * only one orientation at a time. When widgets are initially queried
 * for their minimum sizes it is generally done in two initial passes
 * in the [enum@Gtk.SizeRequestMode] chosen by the toplevel.
 *
 * For example, when queried in the normal %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH mode:
 *
 * First, the default minimum and natural width for each widget
 * in the interface will be computed using [method@Gtk.Widget.measure] with an
 * orientation of %GTK_ORIENTATION_HORIZONTAL and a for_size of -1.
 * Because the preferred widths for each widget depend on the preferred
 * widths of their children, this information propagates up the hierarchy,
 * and finally a minimum and natural width is determined for the entire
 * toplevel. Next, the toplevel will use the minimum width to query for the
 * minimum height contextual to that width using [method@Gtk.Widget.measure] with an
 * orientation of %GTK_ORIENTATION_VERTICAL and a for_size of the just computed
 * width. This will also be a highly recursive operation. The minimum height
 * for the minimum width is normally used to set the minimum size constraint
 * on the toplevel.
 *
 * After the toplevel window has initially requested its size in both
 * dimensions it can go on to allocate itself a reasonable size (or a size
 * previously specified with [method@Gtk.Window.set_default_size]). During the
 * recursive allocation process it’s important to note that request cycles
 * will be recursively executed while widgets allocate their children.
 * Each widget, once allocated a size, will go on to first share the
 * space in one orientation among its children and then request each child's
 * height for its target allocated width or its width for allocated height,
 * depending. In this way a `GtkWidget` will typically be requested its size
 * a number of times before actually being allocated a size. The size a
 * widget is finally allocated can of course differ from the size it has
 * requested. For this reason, `GtkWidget` caches a  small number of results
 * to avoid re-querying for the same sizes in one allocation cycle.
 *
 * If a widget does move content around to intelligently use up the
 * allocated size then it must support the request in both
 * `GtkSizeRequestMode`s even if the widget in question only
 * trades sizes in a single orientation.
 *
 * For instance, a [class@Gtk.Label] that does height-for-width word wrapping
 * will not expect to have [vfunc@Gtk.Widget.measure] with an orientation of
 * %GTK_ORIENTATION_VERTICAL called because that call is specific to a
 * width-for-height request. In this case the label must return the height
 * required for its own minimum possible width. By following this rule any
 * widget that handles height-for-width or width-for-height requests will
 * always be allocated at least enough space to fit its own content.
 *
 * Here are some examples of how a %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH widget
 * generally deals with width-for-height requests:
 *
 * ```c
 * static void
 * foo_widget_measure (GtkWidget      *widget,
 *                     GtkOrientation  orientation,
 *                     int             for_size,
 *                     int            *minimum_size,
 *                     int            *natural_size,
 *                     int            *minimum_baseline,
 *                     int            *natural_baseline)
 * {
 *   if (orientation == GTK_ORIENTATION_HORIZONTAL)
 *     {
 *       // Calculate minimum and natural width
 *     }
 *   else // VERTICAL
 *     {
 *       if (i_am_in_height_for_width_mode)
 *         {
 *           int min_width, dummy;
 *
 *           // First, get the minimum width of our widget
 *           GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
 *                                                   &min_width, &dummy, &dummy, &dummy);
 *
 *           // Now use the minimum width to retrieve the minimum and natural height to display
 *           // that width.
 *           GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_VERTICAL, min_width,
 *                                                   minimum_size, natural_size, &dummy, &dummy);
 *         }
 *       else
 *         {
 *           // ... some widgets do both.
 *         }
 *     }
 * }
 * ```
 *
 * Often a widget needs to get its own request during size request or
 * allocation. For example, when computing height it may need to also
 * compute width. Or when deciding how to use an allocation, the widget
 * may need to know its natural size. In these cases, the widget should
 * be careful to call its virtual methods directly, like in the code
 * example above.
 *
 * It will not work to use the wrapper function [method@Gtk.Widget.measure]
 * inside your own [vfunc@Gtk.Widget.size_allocate] implementation.
 * These return a request adjusted by [class@Gtk.SizeGroup], the widget's
 * align and expand flags, as well as its CSS style.
 *
 * If a widget used the wrappers inside its virtual method implementations,
 * then the adjustments (such as widget margins) would be applied
 * twice. GTK therefore does not allow this and will warn if you try
 * to do it.
 *
 * Of course if you are getting the size request for another widget, such
 * as a child widget, you must use [method@Gtk.Widget.measure]; otherwise, you
 * would not properly consider widget margins, [class@Gtk.SizeGroup], and
 * so forth.
 *
 * GTK also supports baseline vertical alignment of widgets. This
 * means that widgets are positioned such that the typographical baseline of
 * widgets in the same row are aligned. This happens if a widget supports
 * baselines, has a vertical alignment using baselines, and is inside
 * a widget that supports baselines and has a natural “row” that it aligns to
 * the baseline, or a baseline assigned to it by the grandparent.
 *
 * Baseline alignment support for a widget is also done by the
 * [vfunc@Gtk.Widget.measure] virtual function. It allows you to report
 * both a minimum and natural size.
 *
 * If a widget ends up baseline aligned it will be allocated all the space in
 * the parent as if it was %GTK_ALIGN_FILL, but the selected baseline can be
 * found via [method@Gtk.Widget.get_baseline]. If the baseline has a
 * value other than -1 you need to align the widget such that the baseline
 * appears at the position.
 *
 * ### GtkWidget as GtkBuildable
 *
 * The `GtkWidget` implementation of the `GtkBuildable` interface
 * supports various custom elements to specify additional aspects of widgets
 * that are not directly expressed as properties.
 *
 * If the widget uses a [class@Gtk.LayoutManager], `GtkWidget` supports
 * a custom `<layout>` element, used to define layout properties:
 *
 * ```xml
 * <object class="GtkGrid" id="my_grid">
 *   <child>
 *     <object class="GtkLabel" id="label1">
 *       <property name="label">Description</property>
 *       <layout>
 *         <property name="column">0</property>
 *         <property name="row">0</property>
 *         <property name="row-span">1</property>
 *         <property name="column-span">1</property>
 *       </layout>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="description_entry">
 *       <layout>
 *         <property name="column">1</property>
 *         <property name="row">0</property>
 *         <property name="row-span">1</property>
 *         <property name="column-span">1</property>
 *       </layout>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * `GtkWidget` allows style information such as style classes to
 * be associated with widgets, using the custom `<style>` element:
 *
 * ```xml
 * <object class="GtkButton" id="button1">
 *   <style>
 *     <class name="my-special-button-class"/>
 *     <class name="dark-button"/>
 *   </style>
 * </object>
 * ```
 *
 * `GtkWidget` allows defining accessibility information, such as properties,
 * relations, and states, using the custom `<accessibility>` element:
 *
 * ```xml
 * <object class="GtkButton" id="button1">
 *   <accessibility>
 *     <property name="label">Download</property>
 *     <relation name="labelled-by">label1</relation>
 *   </accessibility>
 * </object>
 * ```
 *
 * ### Building composite widgets from template XML
 *
 * `GtkWidget `exposes some facilities to automate the procedure
 * of creating composite widgets using "templates".
 *
 * To create composite widgets with `GtkBuilder` XML, one must associate
 * the interface description with the widget class at class initialization
 * time using [method@Gtk.WidgetClass.set_template].
 *
 * The interface description semantics expected in composite template descriptions
 * is slightly different from regular [class@Gtk.Builder] XML.
 *
 * Unlike regular interface descriptions, [method@Gtk.WidgetClass.set_template]
 * will expect a `<template>` tag as a direct child of the toplevel
 * `<interface>` tag. The `<template>` tag must specify the “class” attribute
 * which must be the type name of the widget. Optionally, the “parent”
 * attribute may be specified to specify the direct parent type of the widget
 * type; this is ignored by `GtkBuilder` but can be used by UI design tools to
 * introspect what kind of properties and internal children exist for a given
 * type when the actual type does not exist.
 *
 * The XML which is contained inside the `<template>` tag behaves as if it were
 * added to the `<object>` tag defining the widget itself. You may set properties
 * on a widget by inserting `<property>` tags into the `<template>` tag, and also
 * add `<child>` tags to add children and extend a widget in the normal way you
 * would with `<object>` tags.
 *
 * Additionally, `<object>` tags can also be added before and after the initial
 * `<template>` tag in the normal way, allowing one to define auxiliary objects
 * which might be referenced by other widgets declared as children of the
 * `<template>` tag.
 *
 * Since, unlike the `<object>` tag, the `<template>` tag does not contain an
 * “id” attribute, if you need to refer to the instance of the object itself that
 * the template will create, simply refer to the template class name in an
 * applicable element content.
 *
 * Here is an example of a template definition, which includes an example of
 * this in the `<signal>` tag:
 *
 * ```xml
 * <interface>
 *   <template class="FooWidget" parent="GtkBox">
 *     <property name="orientation">horizontal</property>
 *     <property name="spacing">4</property>
 *     <child>
 *       <object class="GtkButton" id="hello_button">
 *         <property name="label">Hello World</property>
 *         <signal name="clicked" handler="hello_button_clicked" object="FooWidget" swapped="yes"/>
 *       </object>
 *     </child>
 *     <child>
 *       <object class="GtkButton" id="goodbye_button">
 *         <property name="label">Goodbye World</property>
 *       </object>
 *     </child>
 *   </template>
 * </interface>
 * ```
 *
 * Typically, you'll place the template fragment into a file that is
 * bundled with your project, using `GResource`. In order to load the
 * template, you need to call [method@Gtk.WidgetClass.set_template_from_resource]
 * from the class initialization of your `GtkWidget` type:
 *
 * ```c
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 * }
 * ```
 *
 * You will also need to call [method@Gtk.Widget.init_template] from the
 * instance initialization function:
 *
 * ```c
 * static void
 * foo_widget_init (FooWidget *self)
 * {
 *   gtk_widget_init_template (GTK_WIDGET (self));
 *
 *   // Initialize the rest of the widget...
 * }
 * ```
 *
 * as well as calling [method@Gtk.Widget.dispose_template] from the dispose
 * function:
 *
 * ```c
 * static void
 * foo_widget_dispose (GObject *gobject)
 * {
 *   FooWidget *self = FOO_WIDGET (gobject);
 *
 *   // Dispose objects for which you have a reference...
 *
 *   // Clear the template children for this widget type
 *   gtk_widget_dispose_template (GTK_WIDGET (self), FOO_TYPE_WIDGET);
 *
 *   G_OBJECT_CLASS (foo_widget_parent_class)->dispose (gobject);
 * }
 * ```
 *
 * You can access widgets defined in the template using the
 * [method@Gtk.Widget.get_template_child] function, but you will typically declare
 * a pointer in the instance private data structure of your type using the same
 * name as the widget in the template definition, and call
 * [method@Gtk.WidgetClass.bind_template_child_full] (or one of its wrapper macros
 * [func@Gtk.widget_class_bind_template_child] and [func@Gtk.widget_class_bind_template_child_private])
 * with that name, e.g.
 *
 * ```c
 * typedef struct {
 *   GtkWidget *hello_button;
 *   GtkWidget *goodbye_button;
 * } FooWidgetPrivate;
 *
 * G_DEFINE_TYPE_WITH_PRIVATE (FooWidget, foo_widget, GTK_TYPE_BOX)
 *
 * static void
 * foo_widget_dispose (GObject *gobject)
 * {
 *   gtk_widget_dispose_template (GTK_WIDGET (gobject), FOO_TYPE_WIDGET);
 *
 *   G_OBJECT_CLASS (foo_widget_parent_class)->dispose (gobject);
 * }
 *
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *   G_OBJECT_CLASS (klass)->dispose = foo_widget_dispose;
 *
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 *   gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
 *                                                 FooWidget, hello_button);
 *   gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
 *                                                 FooWidget, goodbye_button);
 * }
 *
 * static void
 * foo_widget_init (FooWidget *widget)
 * {
 *   gtk_widget_init_template (GTK_WIDGET (widget));
 * }
 * ```
 *
 * You can also use [method@Gtk.WidgetClass.bind_template_callback_full] (or
 * is wrapper macro [func@Gtk.widget_class_bind_template_callback]) to connect
 * a signal callback defined in the template with a function visible in the
 * scope of the class, e.g.
 *
 * ```c
 * // the signal handler has the instance and user data swapped
 * // because of the swapped="yes" attribute in the template XML
 * static void
 * hello_button_clicked (FooWidget *self,
 *                       GtkButton *button)
 * {
 *   g_print ("Hello, world!\n");
 * }
 *
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 *   gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), hello_button_clicked);
 * }
 * ```
 */

#define GTK_STATE_FLAGS_DO_SET_PROPAGATE   (GTK_STATE_FLAG_INSENSITIVE | \
                                            GTK_STATE_FLAG_BACKDROP)
#define GTK_STATE_FLAGS_DO_UNSET_PROPAGATE (GTK_STATE_FLAG_INSENSITIVE | \
                                            GTK_STATE_FLAG_BACKDROP | \
                                            GTK_STATE_FLAG_PRELIGHT | \
                                            GTK_STATE_FLAG_ACTIVE)

typedef struct {
  char *name;           /* Name of the template automatic child */
  gboolean internal_child; /* Whether the automatic widget should be exported as an <internal-child> */
  gssize offset;         /* Instance private data offset where to set the automatic child (or 0) */
} AutomaticChildClass;

enum {
  DESTROY,
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  STATE_FLAGS_CHANGED,
  DIRECTION_CHANGED,
  MNEMONIC_ACTIVATE,
  MOVE_FOCUS,
  KEYNAV_FAILED,
  QUERY_TOOLTIP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT,
  PROP_ROOT,
  PROP_WIDTH_REQUEST,
  PROP_HEIGHT_REQUEST,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_CAN_FOCUS,
  PROP_HAS_FOCUS,
  PROP_CAN_TARGET,
  PROP_FOCUS_ON_CLICK,
  PROP_FOCUSABLE,
  PROP_HAS_DEFAULT,
  PROP_RECEIVES_DEFAULT,
  PROP_CURSOR,
  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_MARKUP,
  PROP_TOOLTIP_TEXT,
  PROP_OPACITY,
  PROP_OVERFLOW,
  PROP_HALIGN,
  PROP_VALIGN,
  PROP_MARGIN_START,
  PROP_MARGIN_END,
  PROP_MARGIN_TOP,
  PROP_MARGIN_BOTTOM,
  PROP_HEXPAND,
  PROP_VEXPAND,
  PROP_HEXPAND_SET,
  PROP_VEXPAND_SET,
  PROP_SCALE_FACTOR,
  PROP_CSS_NAME,
  PROP_CSS_CLASSES,
  PROP_LAYOUT_MANAGER,
  NUM_PROPERTIES,

  /* GtkAccessible */
  PROP_ACCESSIBLE_ROLE
};


typedef struct
{
  guint         flags_to_set;
  guint         flags_to_unset;

  int           old_scale_factor;
} GtkStateData;


static void     gtk_widget_base_class_init                      (gpointer            g_class);
static void     gtk_widget_class_init                           (GtkWidgetClass     *klass);
static void     gtk_widget_base_class_finalize                  (GtkWidgetClass     *klass);
static void     gtk_widget_init                                 (GTypeInstance      *instance,
                                                                 gpointer            g_class);
static void     gtk_widget_dispose                              (GObject            *object);
static void     gtk_widget_finalize                             (GObject            *object);
static void     gtk_widget_real_destroy                         (GtkWidget          *object);
static gboolean gtk_widget_real_focus                           (GtkWidget          *widget,
                                                                 GtkDirectionType    direction);
static void     gtk_widget_real_show                            (GtkWidget          *widget);
static void     gtk_widget_real_hide                            (GtkWidget          *widget);
static void     gtk_widget_real_map                             (GtkWidget          *widget);
static void     gtk_widget_real_unmap                           (GtkWidget          *widget);
static void     gtk_widget_real_realize                         (GtkWidget          *widget);
static void     gtk_widget_real_unrealize                       (GtkWidget          *widget);
static void     gtk_widget_real_direction_changed               (GtkWidget          *widget,
                                                                 GtkTextDirection    previous_direction);
static void     gtk_widget_real_css_changed                     (GtkWidget          *widget,
                                                                 GtkCssStyleChange  *change);
static void     gtk_widget_real_system_setting_changed          (GtkWidget          *widget,
                                                                 GtkSystemSetting    setting);
static void     gtk_widget_real_set_focus_child                 (GtkWidget          *widget,
                                                                 GtkWidget          *child);
static void     gtk_widget_real_move_focus                      (GtkWidget          *widget,
                                                                GtkDirectionType     direction);
static gboolean gtk_widget_real_keynav_failed                   (GtkWidget          *widget,
                                                                 GtkDirectionType    direction);
#ifdef G_ENABLE_CONSISTENCY_CHECKS
static void             gtk_widget_verify_invariants            (GtkWidget          *widget);
static void             gtk_widget_push_verify_invariants       (GtkWidget          *widget);
static void             gtk_widget_pop_verify_invariants        (GtkWidget          *widget);
#else
#define                 gtk_widget_verify_invariants(widget)
#define                 gtk_widget_push_verify_invariants(widget)
#define                 gtk_widget_pop_verify_invariants(widget)
#endif
static PangoContext*    gtk_widget_peek_pango_context           (GtkWidget          *widget);
static void             gtk_widget_update_default_pango_context (GtkWidget          *widget);
static void             gtk_widget_propagate_state              (GtkWidget          *widget,
                                                                 const GtkStateData *data);
static gboolean         gtk_widget_real_mnemonic_activate       (GtkWidget          *widget,
                                                                 gboolean            group_cycling);
static void             gtk_widget_accessible_interface_init    (GtkAccessibleInterface *iface);
static void             gtk_widget_buildable_interface_init     (GtkBuildableIface  *iface);
static void             gtk_widget_buildable_set_id             (GtkBuildable       *buildable,
                                                                 const char         *id);
static const char *     gtk_widget_buildable_get_id             (GtkBuildable       *buildable);
static GObject *        gtk_widget_buildable_get_internal_child (GtkBuildable       *buildable,
                                                                 GtkBuilder         *builder,
                                                                 const char         *childname);
static gboolean         gtk_widget_buildable_custom_tag_start   (GtkBuildable       *buildable,
                                                                 GtkBuilder         *builder,
                                                                 GObject            *child,
                                                                 const char         *tagname,
                                                                 GtkBuildableParser *parser,
                                                                 gpointer           *data);
static void             gtk_widget_buildable_custom_tag_end     (GtkBuildable       *buildable,
                                                                 GtkBuilder         *builder,
                                                                 GObject            *child,
                                                                 const char         *tagname,
                                                                 gpointer            data);
static void             gtk_widget_buildable_custom_finished    (GtkBuildable       *buildable,
                                                                 GtkBuilder         *builder,
                                                                 GObject            *child,
                                                                 const char         *tagname,
                                                                 gpointer            data);
static void             gtk_widget_set_usize_internal           (GtkWidget          *widget,
                                                                 int                 width,
                                                                 int                 height);

static void     remove_parent_surface_transform_changed_listener (GtkWidget *widget);
static void     add_parent_surface_transform_changed_listener    (GtkWidget *widget);
static void     gtk_widget_queue_compute_expand                  (GtkWidget *widget);

static GtkATContext *create_at_context (GtkWidget *self);


static int              GtkWidget_private_offset = 0;
static gpointer         gtk_widget_parent_class = NULL;
static guint            widget_signals[LAST_SIGNAL] = { 0 };
static GParamSpec      *widget_props[NUM_PROPERTIES] = { NULL, };
GtkTextDirection        gtk_default_direction = GTK_TEXT_DIR_LTR;

static GQuark           quark_pango_context = 0;
static GQuark           quark_mnemonic_labels = 0;
static GQuark           quark_size_groups = 0;
static GQuark           quark_auto_children = 0;
static GQuark           quark_font_options = 0;
static GQuark           quark_font_map = 0;
static GQuark           quark_builder_set_id = 0;

GType
gtk_widget_get_type (void)
{
  static GType widget_type = 0;

  if (G_UNLIKELY (widget_type == 0))
    {
      const GTypeInfo widget_info =
      {
        sizeof (GtkWidgetClass),
        gtk_widget_base_class_init,
        (GBaseFinalizeFunc) gtk_widget_base_class_finalize,
        (GClassInitFunc) gtk_widget_class_init,
        NULL, /* class_finalize */
        NULL, /* class_init */
        sizeof (GtkWidget),
        0, /* n_preallocs */
        gtk_widget_init,
        NULL, /* value_table */
      };

      const GInterfaceInfo accessible_info =
      {
        (GInterfaceInitFunc) gtk_widget_accessible_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL,
      };

      const GInterfaceInfo buildable_info =
      {
        (GInterfaceInitFunc) gtk_widget_buildable_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL /* interface data */
      };

      const GInterfaceInfo constraint_target_info =
      {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL /* interface data */
      };

      widget_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, g_intern_static_string ("GtkWidget"),
                                            &widget_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_class_private (widget_type, sizeof (GtkWidgetClassPrivate));

      GtkWidget_private_offset = g_type_add_instance_private (widget_type, sizeof (GtkWidgetPrivate));

      g_type_add_interface_static (widget_type, GTK_TYPE_ACCESSIBLE,
                                   &accessible_info);
      g_type_add_interface_static (widget_type, GTK_TYPE_BUILDABLE,
                                   &buildable_info);
      g_type_add_interface_static (widget_type, GTK_TYPE_CONSTRAINT_TARGET,
                                   &constraint_target_info);
    }

  return widget_type;
}

static inline gpointer
gtk_widget_get_instance_private (GtkWidget *self)
{
  return (G_STRUCT_MEMBER_P (self, GtkWidget_private_offset));
}

static void
gtk_widget_base_class_init (gpointer g_class)
{
  GtkWidgetClass *klass = g_class;
  GtkWidgetClassPrivate *priv;

  priv = klass->priv = G_TYPE_CLASS_GET_PRIVATE (g_class, GTK_TYPE_WIDGET, GtkWidgetClassPrivate);

  priv->template = NULL;

  if (priv->shortcuts == NULL)
    {
      priv->shortcuts = g_list_store_new (GTK_TYPE_SHORTCUT);
    }
  else
    {
      GListModel *parent_shortcuts = G_LIST_MODEL (priv->shortcuts);
      guint i, p;

      priv->shortcuts = g_list_store_new (GTK_TYPE_SHORTCUT);
      for (i = 0, p = g_list_model_get_n_items (parent_shortcuts); i < p; i++)
        {
          GtkShortcut *shortcut = g_list_model_get_item (parent_shortcuts, i);
          g_list_store_append (priv->shortcuts, shortcut);
          g_object_unref (shortcut);
        }
    }
}

static void
gtk_widget_real_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    gtk_widget_snapshot_child (widget, child, snapshot);
}

static gboolean
gtk_widget_real_contains (GtkWidget *widget,
                          double     x,
                          double     y)
{
  GtkCssBoxes boxes;

  gtk_css_boxes_init (&boxes, widget);

  return gsk_rounded_rect_contains_point (gtk_css_boxes_get_border_box (&boxes),
                                          &GRAPHENE_POINT_INIT (x, y));
}

static void
gtk_widget_real_root (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  gtk_widget_forall (widget, (GtkCallback) gtk_widget_root, NULL);

  for (l = priv->event_controllers; l; l = l->next)
    {
      if (GTK_IS_SHORTCUT_CONTROLLER (l->data))
        gtk_shortcut_controller_root (GTK_SHORTCUT_CONTROLLER (l->data));
    }
}

static void
gtk_widget_real_unroot (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  for (l = priv->event_controllers; l; l = l->next)
    {
      if (GTK_IS_SHORTCUT_CONTROLLER (l->data))
        gtk_shortcut_controller_unroot (GTK_SHORTCUT_CONTROLLER (l->data));
    }

  gtk_widget_forall (widget, (GtkCallback) gtk_widget_unroot, NULL);
}

static void
gtk_widget_constructed (GObject *object)
{
  G_OBJECT_CLASS (gtk_widget_parent_class)->constructed (object);

  if (GTK_WIDGET_GET_CLASS (object)->priv->actions)
    {
      GtkActionMuxer *muxer;

      muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (object), TRUE);
      gtk_action_muxer_connect_class_actions (muxer);
    }
}

static void
gtk_widget_real_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  *minimum = 0;
  *natural = 0;
}

static GtkSizeRequestMode
gtk_widget_real_get_request_mode (GtkWidget *widget)
{
  /* By default widgets don't trade size at all. */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_widget_real_state_flags_changed (GtkWidget     *widget,
                                     GtkStateFlags  old_state)
{
}

static gboolean
gtk_widget_real_query_tooltip (GtkWidget  *widget,
                               int         x,
                               int         y,
                               gboolean    keyboard_tip,
                               GtkTooltip *tooltip)
{
  const char *tooltip_markup;
  gboolean has_tooltip;

  has_tooltip = gtk_widget_get_has_tooltip (widget);
  tooltip_markup = gtk_widget_get_tooltip_markup (widget);
  if (tooltip_markup == NULL)
    tooltip_markup = gtk_widget_get_tooltip_text (widget);

  if (has_tooltip && tooltip_markup != NULL)
    {
      gtk_tooltip_set_markup (tooltip, tooltip_markup);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_widget_real_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
}

static void
gtk_widget_set_accessible_role (GtkWidget         *self,
                                GtkAccessibleRole  role)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);

  g_return_if_fail (!gtk_accessible_role_is_abstract (role));

  if (priv->at_context == NULL || !gtk_at_context_is_realized (priv->at_context))
    {
      priv->accessible_role = role;

      if (priv->at_context != NULL)
        gtk_at_context_set_accessible_role (priv->at_context, role);

      g_object_notify (G_OBJECT (self), "accessible-role");
    }
  else
    {
      char *role_str = g_enum_to_string (GTK_TYPE_ACCESSIBLE_ROLE, priv->accessible_role);

      g_critical ("Widget of type “%s” already has an accessible role of type “%s”",
                  G_OBJECT_TYPE_NAME (self),
                  role_str);
      g_free (role_str);
    }
}

static GtkAccessibleRole
gtk_widget_get_accessible_role (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);
  GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (self));

  if (context != NULL)
    {
      GtkAccessibleRole role = GTK_ACCESSIBLE_ROLE_WIDGET;

      if (gtk_at_context_is_realized (context))
        role = gtk_at_context_get_accessible_role (context);

      g_object_unref (context);

      if (role != GTK_ACCESSIBLE_ROLE_WIDGET)
        return role;
    }

  if (priv->accessible_role != GTK_ACCESSIBLE_ROLE_WIDGET)
    return priv->accessible_role;

  if (GTK_WIDGET_GET_CLASS (self)->priv->accessible_role != GTK_ACCESSIBLE_ROLE_WIDGET)
    return GTK_WIDGET_GET_CLASS (self)->priv->accessible_role;

  return GTK_ACCESSIBLE_ROLE_GENERIC;
}

static void
gtk_widget_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_widget_set_name (widget, g_value_get_string (value));
      break;
    case PROP_WIDTH_REQUEST:
      gtk_widget_set_usize_internal (widget, g_value_get_int (value), -2);
      break;
    case PROP_HEIGHT_REQUEST:
      gtk_widget_set_usize_internal (widget, -2, g_value_get_int (value));
      break;
    case PROP_VISIBLE:
      gtk_widget_set_visible (widget, g_value_get_boolean (value));
      break;
    case PROP_SENSITIVE:
      gtk_widget_set_sensitive (widget, g_value_get_boolean (value));
      break;
    case PROP_CAN_FOCUS:
      gtk_widget_set_can_focus (widget, g_value_get_boolean (value));
      break;
    case PROP_FOCUSABLE:
      gtk_widget_set_focusable (widget, g_value_get_boolean (value));
      break;
    case PROP_CAN_TARGET:
      gtk_widget_set_can_target (widget, g_value_get_boolean (value));
      break;
    case PROP_FOCUS_ON_CLICK:
      gtk_widget_set_focus_on_click (widget, g_value_get_boolean (value));
      break;
    case PROP_RECEIVES_DEFAULT:
      gtk_widget_set_receives_default (widget, g_value_get_boolean (value));
      break;
    case PROP_CURSOR:
      gtk_widget_set_cursor (widget, g_value_get_object (value));
      break;
    case PROP_HAS_TOOLTIP:
      gtk_widget_set_has_tooltip (widget, g_value_get_boolean (value));
      break;
    case PROP_TOOLTIP_MARKUP:
      gtk_widget_set_tooltip_markup (widget, g_value_get_string (value));
      break;
    case PROP_TOOLTIP_TEXT:
      gtk_widget_set_tooltip_text (widget, g_value_get_string (value));
      break;
    case PROP_HALIGN:
      gtk_widget_set_halign (widget, g_value_get_enum (value));
      break;
    case PROP_VALIGN:
      gtk_widget_set_valign (widget, g_value_get_enum (value));
      break;
    case PROP_MARGIN_START:
      gtk_widget_set_margin_start (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_END:
      gtk_widget_set_margin_end (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_TOP:
      gtk_widget_set_margin_top (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_BOTTOM:
      gtk_widget_set_margin_bottom (widget, g_value_get_int (value));
      break;
    case PROP_HEXPAND:
      gtk_widget_set_hexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_HEXPAND_SET:
      gtk_widget_set_hexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND:
      gtk_widget_set_vexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND_SET:
      gtk_widget_set_vexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_OPACITY:
      gtk_widget_set_opacity (widget, g_value_get_double (value));
      break;
    case PROP_OVERFLOW:
      gtk_widget_set_overflow (widget, g_value_get_enum (value));
      break;
    case PROP_CSS_NAME:
      if (g_value_get_string (value) != NULL)
        gtk_css_node_set_name (priv->cssnode, g_quark_from_string (g_value_get_string (value)));
      break;
    case PROP_CSS_CLASSES:
      gtk_widget_set_css_classes (widget, g_value_get_boxed (value));
      break;
    case PROP_LAYOUT_MANAGER:
      gtk_widget_set_layout_manager (widget, g_value_dup_object (value));
      break;
    case PROP_ACCESSIBLE_ROLE:
      gtk_widget_set_accessible_role (widget, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  switch (prop_id)
    {
    case PROP_NAME:
      if (priv->name)
        g_value_set_string (value, priv->name);
      else
        g_value_set_static_string (value, "");
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_ROOT:
      g_value_set_object (value, priv->root);
      break;
    case PROP_WIDTH_REQUEST:
      {
        int w;
        gtk_widget_get_size_request (widget, &w, NULL);
        g_value_set_int (value, w);
      }
      break;
    case PROP_HEIGHT_REQUEST:
      {
        int h;
        gtk_widget_get_size_request (widget, NULL, &h);
        g_value_set_int (value, h);
      }
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, _gtk_widget_get_visible (widget));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, gtk_widget_get_sensitive (widget));
      break;
    case PROP_CAN_FOCUS:
      g_value_set_boolean (value, gtk_widget_get_can_focus (widget));
      break;
    case PROP_FOCUSABLE:
      g_value_set_boolean (value, gtk_widget_get_focusable (widget));
      break;
    case PROP_HAS_FOCUS:
      g_value_set_boolean (value, gtk_widget_has_focus (widget));
      break;
    case PROP_CAN_TARGET:
      g_value_set_boolean (value, gtk_widget_get_can_target (widget));
      break;
    case PROP_FOCUS_ON_CLICK:
      g_value_set_boolean (value, gtk_widget_get_focus_on_click (widget));
      break;
    case PROP_HAS_DEFAULT:
      g_value_set_boolean (value, gtk_widget_has_default (widget));
      break;
    case PROP_RECEIVES_DEFAULT:
      g_value_set_boolean (value, gtk_widget_get_receives_default (widget));
      break;
    case PROP_CURSOR:
      g_value_set_object (value, gtk_widget_get_cursor (widget));
      break;
    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, gtk_widget_get_has_tooltip (widget));
      break;
    case PROP_TOOLTIP_TEXT:
      g_value_set_string (value, gtk_widget_get_tooltip_text (widget));
      break;
    case PROP_TOOLTIP_MARKUP:
      g_value_set_string (value, gtk_widget_get_tooltip_markup (widget));
      break;
    case PROP_HALIGN:
      g_value_set_enum (value, gtk_widget_get_halign (widget));
      break;
    case PROP_VALIGN:
      g_value_set_enum (value, gtk_widget_get_valign (widget));
      break;
    case PROP_MARGIN_START:
      g_value_set_int (value, gtk_widget_get_margin_start (widget));
      break;
    case PROP_MARGIN_END:
      g_value_set_int (value, gtk_widget_get_margin_end (widget));
      break;
    case PROP_MARGIN_TOP:
      g_value_set_int (value, gtk_widget_get_margin_top (widget));
      break;
    case PROP_MARGIN_BOTTOM:
      g_value_set_int (value, gtk_widget_get_margin_bottom (widget));
      break;
    case PROP_HEXPAND:
      g_value_set_boolean (value, gtk_widget_get_hexpand (widget));
      break;
    case PROP_HEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_hexpand_set (widget));
      break;
    case PROP_VEXPAND:
      g_value_set_boolean (value, gtk_widget_get_vexpand (widget));
      break;
    case PROP_VEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_vexpand_set (widget));
      break;
    case PROP_OPACITY:
      g_value_set_double (value, gtk_widget_get_opacity (widget));
      break;
    case PROP_OVERFLOW:
      g_value_set_enum (value, gtk_widget_get_overflow (widget));
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_int (value, gtk_widget_get_scale_factor (widget));
      break;
    case PROP_CSS_NAME:
      g_value_set_string (value, gtk_widget_get_css_name (widget));
      break;
    case PROP_CSS_CLASSES:
      g_value_take_boxed (value, gtk_widget_get_css_classes (widget));
      break;
    case PROP_LAYOUT_MANAGER:
      g_value_set_object (value, gtk_widget_get_layout_manager (widget));
      break;
    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, gtk_widget_get_accessible_role (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_adjust_private_offset (klass, &GtkWidget_private_offset);
  gtk_widget_parent_class = g_type_class_peek_parent (klass);

  quark_pango_context = g_quark_from_static_string ("gtk-pango-context");
  quark_mnemonic_labels = g_quark_from_static_string ("gtk-mnemonic-labels");
  quark_size_groups = g_quark_from_static_string ("gtk-widget-size-groups");
  quark_auto_children = g_quark_from_static_string ("gtk-widget-auto-children");
  quark_font_options = g_quark_from_static_string ("gtk-widget-font-options");
  quark_font_map = g_quark_from_static_string ("gtk-widget-font-map");

  gobject_class->constructed = gtk_widget_constructed;
  gobject_class->dispose = gtk_widget_dispose;
  gobject_class->finalize = gtk_widget_finalize;
  gobject_class->set_property = gtk_widget_set_property;
  gobject_class->get_property = gtk_widget_get_property;

  klass->show = gtk_widget_real_show;
  klass->hide = gtk_widget_real_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->root = gtk_widget_real_root;
  klass->unroot = gtk_widget_real_unroot;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->get_request_mode = gtk_widget_real_get_request_mode;
  klass->measure = gtk_widget_real_measure;
  klass->state_flags_changed = gtk_widget_real_state_flags_changed;
  klass->direction_changed = gtk_widget_real_direction_changed;
  klass->snapshot = gtk_widget_real_snapshot;
  klass->mnemonic_activate = gtk_widget_real_mnemonic_activate;
  klass->grab_focus = gtk_widget_grab_focus_self;
  klass->focus = gtk_widget_real_focus;
  klass->set_focus_child = gtk_widget_real_set_focus_child;
  klass->move_focus = gtk_widget_real_move_focus;
  klass->keynav_failed = gtk_widget_real_keynav_failed;
  klass->query_tooltip = gtk_widget_real_query_tooltip;
  klass->css_changed = gtk_widget_real_css_changed;
  klass->system_setting_changed = gtk_widget_real_system_setting_changed;
  klass->contains = gtk_widget_real_contains;

  /**
   * GtkWidget:name: (attributes org.gtk.Property.get=gtk_widget_get_name org.gtk.Property.set=gtk_widget_set_name)
   *
   * The name of the widget.
   */
  widget_props[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkWidget:parent: (attributes org.gtk.Property.get=gtk_widget_get_parent)
   *
   * The parent widget of this widget.
   */
  widget_props[PROP_PARENT] =
      g_param_spec_object ("parent", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:root: (attributes org.gtk.Property.get=gtk_widget_get_root)
   *
   * The `GtkRoot` widget of the widget tree containing this widget.
   *
   * This will be %NULL if the widget is not contained in a root widget.
   */
  widget_props[PROP_ROOT] =
      g_param_spec_object ("root", NULL, NULL,
                           GTK_TYPE_ROOT,
                           GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:width-request:
   *
   * Override for width request of the widget.
   *
   * If this is -1, the natural request will be used.
   */
  widget_props[PROP_WIDTH_REQUEST] =
      g_param_spec_int ("width-request", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:height-request:
   *
   * Override for height request of the widget.
   *
   * If this is -1, the natural request will be used.
   */
  widget_props[PROP_HEIGHT_REQUEST] =
      g_param_spec_int ("height-request", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:visible: (attributes org.gtk.Property.get=gtk_widget_get_visible org.gtk.Property.set=gtk_widget_set_visible)
   *
   * Whether the widget is visible.
   */
  widget_props[PROP_VISIBLE] =
      g_param_spec_boolean ("visible", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:sensitive: (attributes org.gtk.Property.get=gtk_widget_get_sensitive org.gtk.Property.set=gtk_widget_set_sensitive)
   *
   * Whether the widget responds to input.
   */
  widget_props[PROP_SENSITIVE] =
      g_param_spec_boolean ("sensitive", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:can-focus: (attributes org.gtk.Property.get=gtk_widget_get_can_focus org.gtk.Property.set=gtk_widget_set_can_focus)
   *
   * Whether the widget or any of its descendents can accept
   * the input focus.
   *
   * This property is meant to be set by widget implementations,
   * typically in their instance init function.
   */
  widget_props[PROP_CAN_FOCUS] =
      g_param_spec_boolean ("can-focus", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:focusable: (attributes org.gtk.Property.get=gtk_widget_get_focusable org.gtk.Property.set=gtk_widget_set_focusable)
   *
   * Whether this widget itself will accept the input focus.
   */
  widget_props[PROP_FOCUSABLE] =
      g_param_spec_boolean ("focusable", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:has-focus: (attributes org.gtk.Property.get=gtk_widget_has_focus)
   *
   * Whether the widget has the input focus.
   */
  widget_props[PROP_HAS_FOCUS] =
      g_param_spec_boolean ("has-focus", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:can-target: (attributes org.gtk.Property.get=gtk_widget_get_can_target org.gtk.Property.set=gtk_widget_set_can_target)
   *
   * Whether the widget can receive pointer events.
   */
  widget_props[PROP_CAN_TARGET] =
      g_param_spec_boolean ("can-target", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:focus-on-click: (attributes org.gtk.Property.get=gtk_widget_get_focus_on_click org.gtk.Property.set=gtk_widget_set_focus_on_click)
   *
   * Whether the widget should grab focus when it is clicked with the mouse.
   *
   * This property is only relevant for widgets that can take focus.
   */
  widget_props[PROP_FOCUS_ON_CLICK] =
      g_param_spec_boolean ("focus-on-click", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:has-default: (attributes org.gtk.Property.get=gtk_widget_has_default)
   *
   * Whether the widget is the default widget.
   */
  widget_props[PROP_HAS_DEFAULT] =
      g_param_spec_boolean ("has-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:receives-default: (attributes org.gtk.Property.get=gtk_widget_get_receives_default org.gtk.Property.set=gtk_widget_set_receives_default)
   *
   * Whether the widget will receive the default action when it is focused.
   */
  widget_props[PROP_RECEIVES_DEFAULT] =
      g_param_spec_boolean ("receives-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

/**
 * GtkWidget:cursor: (attributes org.gtk.Property.get=gtk_widget_get_cursor org.gtk.Property.set=gtk_widget_set_cursor)
 *
 * The cursor used by @widget.
 */
  widget_props[PROP_CURSOR] =
      g_param_spec_object("cursor", NULL, NULL,
                          GDK_TYPE_CURSOR,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

/**
 * GtkWidget:has-tooltip: (attributes org.gtk.Property.get=gtk_widget_get_has_tooltip org.gtk.Property.set=gtk_widget_set_has_tooltip)
 *
 * Enables or disables the emission of the ::query-tooltip signal on @widget.
 *
 * A value of %TRUE indicates that @widget can have a tooltip, in this case
 * the widget will be queried using [signal@Gtk.Widget::query-tooltip] to
 * determine whether it will provide a tooltip or not.
 */
  widget_props[PROP_HAS_TOOLTIP] =
      g_param_spec_boolean ("has-tooltip", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:tooltip-text: (attributes org.gtk.Property.get=gtk_widget_get_tooltip_text org.gtk.Property.set=gtk_widget_set_tooltip_text)
   *
   * Sets the text of tooltip to be the given string.
   *
   * Also see [method@Gtk.Tooltip.set_text].
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL:
   * [property@Gtk.Widget:has-tooltip] will automatically be set to %TRUE
   * and there will be taken care of [signal@Gtk.Widget::query-tooltip] in
   * the default signal handler.
   *
   * Note that if both [property@Gtk.Widget:tooltip-text] and
   * [property@Gtk.Widget:tooltip-markup] are set, the last one wins.
   */
  widget_props[PROP_TOOLTIP_TEXT] =
      g_param_spec_string ("tooltip-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:tooltip-markup: (attributes org.gtk.Property.get=gtk_widget_get_tooltip_markup org.gtk.Property.set=gtk_widget_set_tooltip_markup)
   *
   * Sets the text of tooltip to be the given string, which is marked up
   * with Pango markup.
   *
   * Also see [method@Gtk.Tooltip.set_markup].
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL:
   * [property@Gtk.Widget:has-tooltip] will automatically be set to %TRUE
   * and there will be taken care of [signal@Gtk.Widget::query-tooltip] in
   * the default signal handler.
   *
   * Note that if both [property@Gtk.Widget:tooltip-text] and
   * [property@Gtk.Widget:tooltip-markup] are set, the last one wins.
   */
  widget_props[PROP_TOOLTIP_MARKUP] =
      g_param_spec_string ("tooltip-markup", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:halign: (attributes org.gtk.Property.get=gtk_widget_get_halign org.gtk.Property.set=gtk_widget_set_halign)
   *
   * How to distribute horizontal space if widget gets extra space.
   */
  widget_props[PROP_HALIGN] =
      g_param_spec_enum ("halign", NULL, NULL,
                         GTK_TYPE_ALIGN,
                         GTK_ALIGN_FILL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:valign: (attributes org.gtk.Property.get=gtk_widget_get_valign org.gtk.Property.set=gtk_widget_set_valign)
   *
   * How to distribute vertical space if widget gets extra space.
   */
  widget_props[PROP_VALIGN] =
      g_param_spec_enum ("valign", NULL, NULL,
                         GTK_TYPE_ALIGN,
                         GTK_ALIGN_FILL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-start: (attributes org.gtk.Property.get=gtk_widget_get_margin_start org.gtk.Property.set=gtk_widget_set_margin_start)
   *
   * Margin on start of widget, horizontally.
   *
   * This property supports left-to-right and right-to-left text
   * directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * [method@Gtk.Widget.set_size_request] for example.
   */
  widget_props[PROP_MARGIN_START] =
      g_param_spec_int ("margin-start", NULL, NULL,
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-end: (attributes org.gtk.Property.get=gtk_widget_get_margin_end org.gtk.Property.set=gtk_widget_set_margin_end)
   *
   * Margin on end of widget, horizontally.
   *
   * This property supports left-to-right and right-to-left text
   * directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * [method@Gtk.Widget.set_size_request] for example.
   */
  widget_props[PROP_MARGIN_END] =
      g_param_spec_int ("margin-end", NULL, NULL,
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-top: (attributes org.gtk.Property.get=gtk_widget_get_margin_top org.gtk.Property.set=gtk_widget_set_margin_top)
   *
   * Margin on top side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * [method@Gtk.Widget.set_size_request] for example.
   */
  widget_props[PROP_MARGIN_TOP] =
      g_param_spec_int ("margin-top", NULL, NULL,
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-bottom: (attributes org.gtk.Property.get=gtk_widget_get_margin_bottom org.gtk.Property.set=gtk_widget_set_margin_bottom)
   *
   * Margin on bottom side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * [method@Gtk.Widget.set_size_request] for example.
   */
  widget_props[PROP_MARGIN_BOTTOM] =
      g_param_spec_int ("margin-bottom", NULL, NULL,
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:hexpand: (attributes org.gtk.Property.get=gtk_widget_get_hexpand org.gtk.Property.set=gtk_widget_set_hexpand)
   *
   * Whether to expand horizontally.
   */
  widget_props[PROP_HEXPAND] =
      g_param_spec_boolean ("hexpand", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:hexpand-set: (attributes org.gtk.Property.get=gtk_widget_get_hexpand_set org.gtk.Property.set=gtk_widget_set_hexpand_set)
   *
   * Whether to use the `hexpand` property.
   */
  widget_props[PROP_HEXPAND_SET] =
      g_param_spec_boolean ("hexpand-set", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:vexpand: (attributes org.gtk.Property.get=gtk_widget_get_vexpand org.gtk.Property.set=gtk_widget_set_vexpand)
   *
   * Whether to expand vertically.
   */
  widget_props[PROP_VEXPAND] =
      g_param_spec_boolean ("vexpand", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:vexpand-set: (attributes org.gtk.Property.get=gtk_widget_get_vexpand_set org.gtk.Property.set=gtk_widget_set_vexpand_set)
   *
   * Whether to use the `vexpand` property.
   */
  widget_props[PROP_VEXPAND_SET] =
      g_param_spec_boolean ("vexpand-set", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:opacity: (attributes org.gtk.Property.get=gtk_widget_get_opacity org.gtk.Widget.set=gtk_widget_set_opacity)
   *
   * The requested opacity of the widget.
   */
  widget_props[PROP_OPACITY] =
      g_param_spec_double ("opacity", NULL, NULL,
                           0.0, 1.0,
                           1.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:overflow: (attributes org.gtk.Property.get=gtk_widget_get_overflow org.gtk.Property.set=gtk_widget_set_overflow)
   *
   * How content outside the widget's content area is treated.
   *
   * This property is meant to be set by widget implementations,
   * typically in their instance init function.
   */
  widget_props[PROP_OVERFLOW] =
      g_param_spec_enum ("overflow", NULL, NULL,
                         GTK_TYPE_OVERFLOW,
                         GTK_OVERFLOW_VISIBLE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:scale-factor: (attributes org.gtk.Property.get=gtk_widget_get_scale_factor)
   *
   * The scale factor of the widget.
   */
  widget_props[PROP_SCALE_FACTOR] =
      g_param_spec_int ("scale-factor", NULL, NULL,
                        1, G_MAXINT,
                        1,
                        GTK_PARAM_READABLE);

  /**
   * GtkWidget:css-name: (attributes org.gtk.Property.get=gtk_widget_get_css_name)
   *
   * The name of this widget in the CSS tree.
   *
   * This property is meant to be set by widget implementations,
   * typically in their instance init function.
   */
  widget_props[PROP_CSS_NAME] =
      g_param_spec_string ("css-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkWidget:css-classes: (attributes org.gtk.Property.get=gtk_widget_get_css_classes org.gtk.Property.set=gtk_widget_set_css_classes)
   *
   * A list of css classes applied to this widget.
   */
  widget_props[PROP_CSS_CLASSES] =
      g_param_spec_boxed ("css-classes", NULL, NULL,
                          G_TYPE_STRV,
                          GTK_PARAM_READWRITE);

  /**
   * GtkWidget:layout-manager: (attributes org.gtk.Property.get=gtk_widget_get_layout_manager org.gtk.Property.set=gtk_widget_set_layout_manager)
   *
   * The `GtkLayoutManager` instance to use to compute the preferred size
   * of the widget, and allocate its children.
   *
   * This property is meant to be set by widget implementations,
   * typically in their instance init function.
   */
  widget_props[PROP_LAYOUT_MANAGER] =
    g_param_spec_object ("layout-manager", NULL, NULL,
                         GTK_TYPE_LAYOUT_MANAGER,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, widget_props);

  g_object_class_override_property (gobject_class, PROP_ACCESSIBLE_ROLE, "accessible-role");

  /**
   * GtkWidget::destroy:
   * @object: the object which received the signal
   *
   * Signals that all holders of a reference to the widget should release
   * the reference that they hold.
   *
   * May result in finalization of the widget if all references are released.
   *
   * This signal is not suitable for saving widget state.
   */
  widget_signals[DESTROY] =
    g_signal_new (I_("destroy"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::show:
   * @widget: the object which received the signal.
   *
   * Emitted when @widget is shown.
   */
  widget_signals[SHOW] =
    g_signal_new (I_("show"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, show),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::hide:
   * @widget: the object which received the signal.
   *
   * Emitted when @widget is hidden.
   */
  widget_signals[HIDE] =
    g_signal_new (I_("hide"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, hide),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::map:
   * @widget: the object which received the signal.
   *
   * Emitted when @widget is going to be mapped.
   *
   * A widget is mapped when the widget is visible (which is controlled with
   * [property@Gtk.Widget:visible]) and all its parents up to the toplevel widget
   * are also visible.
   *
   * The ::map signal can be used to determine whether a widget will be drawn,
   * for instance it can resume an animation that was stopped during the
   * emission of [signal@Gtk.Widget::unmap].
   */
  widget_signals[MAP] =
    g_signal_new (I_("map"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, map),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unmap:
   * @widget: the object which received the signal.
   *
   * Emitted when @widget is going to be unmapped.
   *
   * A widget is unmapped when either it or any of its parents up to the
   * toplevel widget have been set as hidden.
   *
   * As ::unmap indicates that a widget will not be shown any longer,
   * it can be used to, for example, stop an animation on the widget.
   */
  widget_signals[UNMAP] =
    g_signal_new (I_("unmap"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, unmap),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::realize:
   * @widget: the object which received the signal.
   *
   * Emitted when @widget is associated with a `GdkSurface`.
   *
   * This means that [method@Gtk.Widget.realize] has been called
   * or the widget has been mapped (that is, it is going to be drawn).
   */
  widget_signals[REALIZE] =
    g_signal_new (I_("realize"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, realize),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unrealize:
   * @widget: the object which received the signal.
   *
   * Emitted when the `GdkSurface` associated with @widget is destroyed.
   *
   * This means that [method@Gtk.Widget.unrealize] has been called
   * or the widget has been unmapped (that is, it is going to be hidden).
   */
  widget_signals[UNREALIZE] =
    g_signal_new (I_("unrealize"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, unrealize),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::state-flags-changed:
   * @widget: the object which received the signal.
   * @flags: The previous state flags.
   *
   * Emitted when the widget state changes.
   *
   * See [method@Gtk.Widget.get_state_flags].
   */
  widget_signals[STATE_FLAGS_CHANGED] =
    g_signal_new (I_("state-flags-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, state_flags_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_STATE_FLAGS);

  /**
   * GtkWidget::direction-changed:
   * @widget: the object on which the signal is emitted
   * @previous_direction: the previous text direction of @widget
   *
   * Emitted when the text direction of a widget changes.
   */
  widget_signals[DIRECTION_CHANGED] =
    g_signal_new (I_("direction-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, direction_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_TEXT_DIRECTION);

  /**
   * GtkWidget::mnemonic-activate:
   * @widget: the object which received the signal.
   * @group_cycling: %TRUE if there are other widgets with the same mnemonic
   *
   * Emitted when a widget is activated via a mnemonic.
   *
   * The default handler for this signal activates @widget if @group_cycling
   * is %FALSE, or just makes @widget grab focus if @group_cycling is %TRUE.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   * %FALSE to propagate the event further.
   */
  widget_signals[MNEMONIC_ACTIVATE] =
    g_signal_new (I_("mnemonic-activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, mnemonic_activate),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (widget_signals[MNEMONIC_ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  /**
   * GtkWidget::move-focus:
   * @widget: the object which received the signal.
   * @direction: the direction of the focus move
   *
   * Emitted when the focus is moved.
   *
   * The ::move-focus signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are <kbd>Tab</kbd> to move forward,
   * and <kbd>Shift</kbd>+<kbd>Tab</kbd> to move backward.
   */
  widget_signals[MOVE_FOCUS] =
    g_signal_new (I_("move-focus"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWidgetClass, move_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::keynav-failed:
   * @widget: the object which received the signal
   * @direction: the direction of movement
   *
   * Emitted if keyboard navigation fails.
   *
   * See [method@Gtk.Widget.keynav_failed] for details.
   *
   * Returns: %TRUE if stopping keyboard navigation is fine, %FALSE
   *   if the emitting widget should try to handle the keyboard
   *   navigation attempt in its parent widget(s).
   */
  widget_signals[KEYNAV_FAILED] =
    g_signal_new (I_("keynav-failed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, keynav_failed),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  g_signal_set_va_marshaller (widget_signals[KEYNAV_FAILED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__ENUMv);

  /**
   * GtkWidget::query-tooltip:
   * @widget: the object which received the signal
   * @x: the x coordinate of the cursor position where the request has
   *   been emitted, relative to @widget's left side
   * @y: the y coordinate of the cursor position where the request has
   *   been emitted, relative to @widget's top
   * @keyboard_mode: %TRUE if the tooltip was triggered using the keyboard
   * @tooltip: a `GtkTooltip`
   *
   * Emitted when the widget’s tooltip is about to be shown.
   *
   * This happens when the [property@Gtk.Widget:has-tooltip] property
   * is %TRUE and the hover timeout has expired with the cursor hovering
   * "above" @widget; or emitted when @widget got focus in keyboard mode.
   *
   * Using the given coordinates, the signal handler should determine
   * whether a tooltip should be shown for @widget. If this is the case
   * %TRUE should be returned, %FALSE otherwise.  Note that if
   * @keyboard_mode is %TRUE, the values of @x and @y are undefined and
   * should not be used.
   *
   * The signal handler is free to manipulate @tooltip with the therefore
   * destined function calls.
   *
   * Returns: %TRUE if @tooltip should be shown right now, %FALSE otherwise.
   */
  widget_signals[QUERY_TOOLTIP] =
    g_signal_new (I_("query-tooltip"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, query_tooltip),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECT,
                  G_TYPE_BOOLEAN, 4,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_BOOLEAN,
                  GTK_TYPE_TOOLTIP);
  g_signal_set_va_marshaller (widget_signals[QUERY_TOOLTIP],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECTv);

  gtk_widget_class_set_css_name (klass, I_("widget"));
  klass->priv->accessible_role = GTK_ACCESSIBLE_ROLE_WIDGET;
}

static void
template_child_class_free (AutomaticChildClass *child_class)
{
  if (child_class)
    {
      g_free (child_class->name);
      g_free (child_class);
    }
}

static void
gtk_widget_base_class_finalize (GtkWidgetClass *klass)
{
  GtkWidgetTemplate *template_data = klass->priv->template;

  if (template_data)
    {
      g_bytes_unref (template_data->data);
      g_slist_free_full (template_data->children, (GDestroyNotify)template_child_class_free);

      g_object_unref (template_data->scope);

      g_free (template_data);
    }

  g_object_unref (klass->priv->shortcuts);
}

static void
_gtk_widget_emulate_press (GtkWidget      *widget,
                           GdkEvent       *event,
                           GtkWidget      *event_widget)
{
  GtkWidget *next_child, *parent;
  GdkEvent *press;
  double x, y;
  graphene_point_t p;

  if (event_widget == widget)
    return;

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
      gdk_event_get_position (event, &x, &y);
      if (!gtk_widget_compute_point (event_widget,
                                     GTK_WIDGET (gtk_widget_get_root (event_widget)),
                                     &GRAPHENE_POINT_INIT (x, y),
                                     &p))
        return;
      break;
    default:
      return;
    }

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
      press = gdk_touch_event_new (GDK_TOUCH_BEGIN,
                                   gdk_event_get_event_sequence (event),
                                   gdk_event_get_surface (event),
                                   gdk_event_get_device (event),
                                   gdk_event_get_time (event),
                                   gdk_event_get_modifier_state (event),
                                   p.x, p.y,
                                   NULL,
                                   gdk_touch_event_get_emulating_pointer (event));
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      press = gdk_button_event_new (GDK_BUTTON_PRESS,
                                    gdk_event_get_surface (event),
                                    gdk_event_get_device (event),
                                    gdk_event_get_device_tool (event),
                                    gdk_event_get_time (event),
                                    gdk_event_get_modifier_state (event),
                                    gdk_button_event_get_button (event),
                                    p.x, p.y,
                                    NULL);
      break;
    case GDK_MOTION_NOTIFY:
      {
        GdkModifierType state = gdk_event_get_modifier_state (event);
        int button;
        if (state & GDK_BUTTON3_MASK)
          button = 3;
        else if (state & GDK_BUTTON2_MASK)
          button = 2;
        else
          {
            if ((state & GDK_BUTTON1_MASK) == 0)
              g_critical ("Guessing button number 1 on generated button press event");
            button = 1;
          }

        press = gdk_button_event_new (GDK_BUTTON_PRESS,
                                      gdk_event_get_surface (event),
                                      gdk_event_get_device (event),
                                      gdk_event_get_device_tool (event),
                                      gdk_event_get_time (event),
                                      gdk_event_get_modifier_state (event),
                                      button,
                                      p.x, p.y,
                                      NULL);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  next_child = event_widget;
  parent = _gtk_widget_get_parent (next_child);

  while (parent && parent != widget)
    {
      next_child = parent;
      parent = _gtk_widget_get_parent (parent);
    }

  /* Perform propagation state starting from the next child in the chain */
  gtk_propagate_event_internal (event_widget, press, next_child);
  gdk_event_unref (press);
}

static GdkEvent *
_gtk_widget_get_last_event (GtkWidget         *widget,
                            GdkEventSequence  *sequence,
                            GtkWidget        **target)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkEventController *controller;
  GdkEvent *event;
  GList *l;

  for (l = priv->event_controllers; l; l = l->next)
    {
      controller = l->data;

      if (!GTK_IS_GESTURE (controller))
        continue;

      event = gtk_gesture_get_last_event (GTK_GESTURE (controller), sequence);
      if (event)
        {
          *target = gtk_gesture_get_last_target (GTK_GESTURE (controller), sequence);
          return event;
        }
    }

  *target = NULL;
  return NULL;
}

static gboolean
_gtk_widget_get_emulating_sequence (GtkWidget         *widget,
                                    GdkEventSequence  *sequence,
                                    GdkEventSequence **sequence_out)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  *sequence_out = sequence;

  if (sequence)
    {
      GdkEvent *last_event;
      GtkWidget *target;

      last_event = _gtk_widget_get_last_event (widget, sequence, &target);

      if (last_event &&
          (gdk_event_get_event_type (last_event) == GDK_TOUCH_BEGIN ||
           gdk_event_get_event_type (last_event) == GDK_TOUCH_UPDATE ||
           gdk_event_get_event_type (last_event) == GDK_TOUCH_END) &&
           gdk_touch_event_get_emulating_pointer (last_event))
        return TRUE;
    }
  else
    {
      GList *l;

      /* For a NULL(pointer) sequence, find the pointer emulating one */
      for (l = priv->event_controllers; l; l = l->next)
        {
          GtkEventController *controller = l->data;

          if (!GTK_IS_GESTURE (controller))
            continue;

          if (_gtk_gesture_get_pointer_emulating_sequence (GTK_GESTURE (controller),
                                                           sequence_out))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_widget_needs_press_emulation (GtkWidget        *widget,
                                  GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean sequence_press_handled = FALSE;
  GList *l;

  /* Check whether there is any remaining gesture in
   * the capture phase that handled the press event
   */
  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;
      GtkPropagationPhase phase;
      GtkGesture *gesture;

      phase = gtk_event_controller_get_propagation_phase (controller);

      if (phase != GTK_PHASE_CAPTURE)
        continue;
      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);
      sequence_press_handled |=
        (gtk_gesture_handles_sequence (gesture, sequence) &&
         _gtk_gesture_handled_sequence_press (gesture, sequence));
    }

  return !sequence_press_handled;
}

static int
_gtk_widget_set_sequence_state_internal (GtkWidget             *widget,
                                         GdkEventSequence      *sequence,
                                         GtkEventSequenceState  state,
                                         GtkGesture            *emitter)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean emulates_pointer, sequence_handled = FALSE;
  GdkEvent *mimic_event;
  GtkWidget *target;
  GList *group = NULL, *l;
  GdkEventSequence *seq;
  int n_handled = 0;

  if (!priv->event_controllers && state != GTK_EVENT_SEQUENCE_CLAIMED)
    return TRUE;

  if (emitter)
    group = gtk_gesture_get_group (emitter);

  emulates_pointer = _gtk_widget_get_emulating_sequence (widget, sequence, &seq);
  mimic_event = _gtk_widget_get_last_event (widget, seq, &target);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller;
      GtkEventSequenceState gesture_state;
      GtkGesture *gesture;
      gboolean retval;

      seq = sequence;
      controller = l->data;
      gesture_state = state;

      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);

      if (gesture == emitter)
        {
          sequence_handled |=
            _gtk_gesture_handled_sequence_press (gesture, sequence);
          n_handled++;
          continue;
        }

      if (seq && emulates_pointer &&
          !gtk_gesture_handles_sequence (gesture, seq))
        seq = NULL;

      if (group && !g_list_find (group, controller))
        {
          /* If a group is provided, ensure only gestures pertaining to the group
           * get a "claimed" state, all other claiming gestures must deny the sequence.
           */
          if (state == GTK_EVENT_SEQUENCE_CLAIMED)
            gesture_state = GTK_EVENT_SEQUENCE_DENIED;
          else
            continue;
        }
      else if (!group &&
               gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
        continue;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      retval = gtk_gesture_set_sequence_state (gesture, seq, gesture_state);
G_GNUC_END_IGNORE_DEPRECATIONS

      if (retval || gesture == emitter)
        {
          sequence_handled |=
            _gtk_gesture_handled_sequence_press (gesture, seq);
          n_handled++;
        }
    }

  /* If the sequence goes denied, check whether this is a controller attached
   * to the capture phase, that additionally handled the button/touch press (i.e.
   * it was consumed), the corresponding press will be emulated for widgets
   * beneath, so the widgets beneath get a coherent stream of events from now on.
   */
  if (n_handled > 0 && sequence_handled &&
      state == GTK_EVENT_SEQUENCE_DENIED &&
      gtk_widget_needs_press_emulation (widget, sequence))
    _gtk_widget_emulate_press (widget, mimic_event, target);

  g_list_free (group);

  return n_handled;
}

static gboolean
_gtk_widget_cancel_sequence (GtkWidget        *widget,
                             GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean handled = FALSE;
  GList *l;

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller;
      GtkGesture *gesture;

      controller = l->data;

      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);

      handled |= _gtk_gesture_cancel_sequence (gesture, sequence);
    }

  return handled;
}

static gboolean
gtk_widget_class_get_visible_by_default (GtkWidgetClass *widget_class)
{
  return !g_type_is_a (G_TYPE_FROM_CLASS (widget_class), GTK_TYPE_NATIVE);
}

static void
gtk_widget_init (GTypeInstance *instance, gpointer g_class)
{
  GtkWidget *widget = GTK_WIDGET (instance);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GType layout_manager_type;
  GtkEventController *controller;

  widget->priv = priv;

  priv->visible = gtk_widget_class_get_visible_by_default (g_class);
  priv->child_visible = TRUE;
  priv->name = NULL;
  priv->user_alpha = 255;
  priv->parent = NULL;
  priv->first_child = NULL;
  priv->last_child = NULL;
  priv->prev_sibling = NULL;
  priv->next_sibling = NULL;
  priv->baseline = -1;
  priv->allocated_baseline = -1;

  priv->sensitive = TRUE;
  priv->alloc_needed = TRUE;
  priv->alloc_needed_on_child = TRUE;
  priv->draw_needed = TRUE;
  priv->focus_on_click = TRUE;
  priv->can_focus = TRUE;
  priv->focusable = FALSE;
  priv->can_target = TRUE;

  switch (_gtk_widget_get_direction (widget))
    {
    case GTK_TEXT_DIR_LTR:
      priv->state_flags = GTK_STATE_FLAG_DIR_LTR;
      break;

    case GTK_TEXT_DIR_RTL:
      priv->state_flags = GTK_STATE_FLAG_DIR_RTL;
      break;

    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  /* this will be set to TRUE if the widget gets a child or if the
   * expand flag is set on the widget, but until one of those happen
   * we know the expand is already properly FALSE.
   *
   * We really want to default FALSE here to avoid computing expand
   * all over the place while initially building a widget tree.
   */
  priv->need_compute_expand = FALSE;

  priv->halign = GTK_ALIGN_FILL;
  priv->valign = GTK_ALIGN_FILL;

  /* Note that we intentionally set this to an abstract role here.
   * See gtk_widget_get_accessible_role() for where it gets overridden
   * with GTK_ACCESSIBLE_ROLE_GENERIC.
   */
  priv->accessible_role = GTK_ACCESSIBLE_ROLE_WIDGET;

  priv->width_request = -1;
  priv->height_request = -1;

  _gtk_size_request_cache_init (&priv->requests);

  priv->cssnode = gtk_css_widget_node_new (widget);
  gtk_css_node_set_state (priv->cssnode, priv->state_flags);
  gtk_css_node_set_visible (priv->cssnode, priv->visible);
  /* need to set correct name here, and only class has the correct type here */
  gtk_css_node_set_name (priv->cssnode, GTK_WIDGET_CLASS (g_class)->priv->css_name);

  if (g_type_is_a (G_TYPE_FROM_CLASS (g_class), GTK_TYPE_ROOT))
    priv->root = (GtkRoot *) widget;

  if (g_type_is_a (G_TYPE_FROM_CLASS (g_class), GTK_TYPE_SHORTCUT_MANAGER))
    gtk_shortcut_manager_create_controllers (widget);

  layout_manager_type = gtk_widget_class_get_layout_manager_type (g_class);
  if (layout_manager_type != G_TYPE_INVALID)
    gtk_widget_set_layout_manager (widget, g_object_new (layout_manager_type, NULL));

  if (g_list_model_get_n_items (G_LIST_MODEL (GTK_WIDGET_CLASS (g_class)->priv->shortcuts)) > 0)
    {
      controller = gtk_shortcut_controller_new_for_model (G_LIST_MODEL (GTK_WIDGET_CLASS (g_class)->priv->shortcuts));
      gtk_event_controller_set_static_name (controller, "gtk-widget-class-shortcuts");
      gtk_widget_add_controller (widget, controller);
    }

  priv->at_context = create_at_context (widget);

  gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                               -1);
}

static void
gtk_widget_root_at_context (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);
  GtkAccessibleRole role = priv->accessible_role;

  if (priv->at_context == NULL)
    return;

  /* Reset the accessible role to its current value */
  if (role == GTK_ACCESSIBLE_ROLE_WIDGET)
    role = GTK_WIDGET_GET_CLASS (self)->priv->accessible_role;
  if (role == GTK_ACCESSIBLE_ROLE_WIDGET)
    role = GTK_ACCESSIBLE_ROLE_GENERIC;

  gtk_at_context_set_accessible_role (priv->at_context, role);
  if (priv->root)
    gtk_at_context_set_display (priv->at_context, gtk_root_get_display (priv->root));
}

static void
gtk_widget_unroot_at_context (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);

  if (priv->at_context != NULL)
    {
      gtk_at_context_set_display (priv->at_context, gdk_display_get_default ());
      gtk_at_context_unrealize (priv->at_context);
    }
}

void
gtk_widget_realize_at_context (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);

  if (priv->at_context == NULL || gtk_at_context_is_realized (priv->at_context))
     return;

  gtk_widget_root_at_context (self);
  gtk_at_context_realize (priv->at_context);
}

void
gtk_widget_unrealize_at_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->at_context != NULL)
    {
      gtk_at_context_set_display (priv->at_context, gdk_display_get_default ());
      gtk_at_context_unrealize (priv->at_context);
    }
}

void
gtk_widget_root (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (!priv->realized);

  if (GTK_IS_ROOT (widget))
    {
      g_assert (priv->root == GTK_ROOT (widget));
    }
  else
    {
      g_assert (priv->root == NULL);
      priv->root = priv->parent->priv->root;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (priv->context)
    gtk_style_context_set_display (priv->context, gtk_root_get_display (priv->root));
G_GNUC_END_IGNORE_DEPRECATIONS

  if (priv->surface_transform_data)
    add_parent_surface_transform_changed_listener (widget);

  _gtk_widget_update_parent_muxer (widget);

  if (priv->layout_manager)
    gtk_layout_manager_set_root (priv->layout_manager, priv->root);

  gtk_widget_root_at_context (widget);

  GTK_WIDGET_GET_CLASS (widget)->root (widget);

  if (!GTK_IS_ROOT (widget))
    g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_ROOT]);
}

void
gtk_widget_unroot (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data;

  g_assert (priv->root);
  g_assert (!priv->realized);

  surface_transform_data = priv->surface_transform_data;
  if (surface_transform_data &&
      surface_transform_data->tracked_parent)
    remove_parent_surface_transform_changed_listener (widget);

  _gtk_widget_update_parent_muxer (widget);

  GTK_WIDGET_GET_CLASS (widget)->unroot (widget);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (priv->context)
    gtk_style_context_set_display (priv->context, gdk_display_get_default ());
G_GNUC_END_IGNORE_DEPRECATIONS

  if (priv->layout_manager)
    gtk_layout_manager_set_root (priv->layout_manager, NULL);

  if (g_object_get_qdata (G_OBJECT (widget), quark_pango_context))
    g_object_set_qdata (G_OBJECT (widget), quark_pango_context, NULL);

  _gtk_tooltip_hide (widget);

  if (!GTK_IS_ROOT (widget))
    {
      /* Roots unrealize the ATContext on unmap */
      gtk_widget_unroot_at_context (widget);

      priv->root = NULL;
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_ROOT]);
    }
}

/**
 * gtk_widget_unparent:
 * @widget: a `GtkWidget`
 *
 * Dissociate @widget from its parent.
 *
 * This function is only for use in widget implementations,
 * typically in dispose.
 */
void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *old_parent;
  GtkWidget *old_prev_sibling;
  GtkRoot *root;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->parent == NULL)
    return;

  gtk_widget_push_verify_invariants (widget);

  g_object_freeze_notify (G_OBJECT (widget));

  gtk_accessible_update_children (GTK_ACCESSIBLE (priv->parent),
                                  GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_CHILD_STATE_REMOVED);

  root = _gtk_widget_get_root (widget);
  if (GTK_IS_WINDOW (root))
    _gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);

  if (gtk_widget_get_focus_child (priv->parent) == widget)
    gtk_widget_set_focus_child (priv->parent, NULL);

  if (_gtk_widget_get_mapped (priv->parent))
    gtk_widget_queue_draw (priv->parent);

  if (priv->visible && _gtk_widget_get_visible (priv->parent))
    gtk_widget_queue_resize (priv->parent);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent.
   */
  priv->width = 0;
  priv->height = 0;

  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  if (priv->root)
    gtk_widget_unroot (widget);

  root = NULL;

  /* Removing a widget from a container restores the child visible
   * flag to the default state, so it doesn't affect the child
   * in the next parent.
   */
  priv->child_visible = TRUE;

  old_parent = priv->parent;
  if (old_parent)
    {
      if (old_parent->priv->first_child == widget)
        old_parent->priv->first_child = priv->next_sibling;

      if (old_parent->priv->last_child == widget)
        old_parent->priv->last_child = priv->prev_sibling;

      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;
      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;
    }
  old_prev_sibling = priv->prev_sibling;
  priv->parent = NULL;
  priv->prev_sibling = NULL;
  priv->next_sibling = NULL;

  /* parent may no longer expand if the removed
   * child was expand=TRUE and could therefore
   * be forcing it to.
   */
  if (_gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (old_parent);
    }

  /* Unset BACKDROP since we are no longer inside a toplevel window */
  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
  gtk_css_node_set_parent (priv->cssnode, NULL);

  _gtk_widget_update_parent_muxer (widget);

  if (old_parent->priv->children_observer)
    gtk_list_list_model_item_removed (old_parent->priv->children_observer, old_prev_sibling);

  if (old_parent->priv->layout_manager)
    gtk_layout_manager_remove_layout_child (old_parent->priv->layout_manager, widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_PARENT]);
  g_object_thaw_notify (G_OBJECT (widget));

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

static void
gtk_widget_update_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_update_image (l->data);
}

static void
gtk_widget_push_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_push_snapshot_count (l->data);
}

static void
gtk_widget_pop_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_pop_snapshot_count (l->data);
}

/**
 * gtk_widget_show:
 * @widget: a `GtkWidget`
 *
 * Flags a widget to be displayed.
 *
 * Any widget that isn’t shown will not appear on the screen.
 *
 * Remember that you have to show the containers containing a widget,
 * in addition to the widget itself, before it will appear onscreen.
 *
 * When a toplevel container is shown, it is immediately realized and
 * mapped; other shown widgets are realized and mapped when their
 * toplevel container is realized and mapped.
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.set_visible] instead
 */
void
gtk_widget_show (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent;

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      parent = _gtk_widget_get_parent (widget);
      if (parent)
        {
          gtk_widget_queue_resize (parent);

          /* see comment in set_parent() for why this should and can be
           * conditional
           */
          if (priv->need_compute_expand ||
              priv->computed_hexpand ||
              priv->computed_vexpand)
            gtk_widget_queue_compute_expand (parent);
        }

      gtk_css_node_set_visible (priv->cssnode, TRUE);

      g_signal_emit (widget, widget_signals[SHOW], 0);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VISIBLE]);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (!_gtk_widget_get_visible (widget));

  priv->visible = TRUE;

  if (priv->parent &&
      _gtk_widget_get_mapped (priv->parent) &&
      _gtk_widget_get_child_visible (widget) &&
      !_gtk_widget_get_mapped (widget))
    gtk_widget_map (widget);
}

/**
 * gtk_widget_hide:
 * @widget: a `GtkWidget`
 *
 * Reverses the effects of gtk_widget_show().
 *
 * This is causing the widget to be hidden (invisible to the user).
 *
 * Deprecated: 4.10: Use [method@Gtk.Widget.set_visible] instead
 */
void
gtk_widget_hide (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent;
      GtkRoot *root;

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      root = _gtk_widget_get_root (widget);
      if (GTK_WIDGET (root) != widget && GTK_IS_WINDOW (root))
        _gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);

      /* a parent may now be expand=FALSE since we're hidden. */
      if (priv->need_compute_expand ||
          priv->computed_hexpand ||
          priv->computed_vexpand)
        {
          gtk_widget_queue_compute_expand (widget);
        }

      gtk_css_node_set_visible (priv->cssnode, FALSE);

      g_signal_emit (widget, widget_signals[HIDE], 0);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VISIBLE]);

      parent = gtk_widget_get_parent (widget);
      if (parent)
        gtk_widget_queue_resize (parent);

      gtk_widget_queue_allocate (widget);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (_gtk_widget_get_visible (widget));

  priv->visible = FALSE;

  if (_gtk_widget_get_mapped (widget))
    gtk_widget_unmap (widget);

  g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);
  priv->allocated_width = 0;
  priv->allocated_height = 0;
  priv->allocated_baseline = 0;
  g_clear_pointer (&priv->transform, gsk_transform_unref);
  priv->width = 0;
  priv->height = 0;
  priv->baseline = 0;
  gtk_widget_update_paintables (widget);
}

static void
update_cursor_on_state_change (GtkWidget *widget)
{
  GtkRoot *root;

  root = _gtk_widget_get_root (widget);
  if (GTK_IS_WINDOW (root))
    gtk_window_update_pointer_focus_on_state_change (GTK_WINDOW (root), widget);
}

/**
 * gtk_widget_map:
 * @widget: a `GtkWidget`
 *
 * Causes a widget to be mapped if it isn’t already.
 *
 * This function is only for use in widget implementations.
 */
void
gtk_widget_map (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (_gtk_widget_get_visible (widget));
  g_return_if_fail (_gtk_widget_get_child_visible (widget));

  if (!_gtk_widget_get_mapped (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      if (!_gtk_widget_get_realized (widget))
        gtk_widget_realize (widget);

      g_signal_emit (widget, widget_signals[MAP], 0);

      update_cursor_on_state_change (widget);

      gtk_widget_queue_draw (widget);

      gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                                   GTK_ACCESSIBLE_STATE_HIDDEN, FALSE,
                                   -1);

      gtk_widget_pop_verify_invariants (widget);
    }
}

/**
 * gtk_widget_unmap:
 * @widget: a `GtkWidget`
 *
 * Causes a widget to be unmapped if it’s currently mapped.
 *
 * This function is only for use in widget implementations.
 */
void
gtk_widget_unmap (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_mapped (widget))
    {
      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      gtk_widget_queue_draw (widget);
      _gtk_tooltip_hide (widget);

      g_signal_emit (widget, widget_signals[UNMAP], 0);

      update_cursor_on_state_change (widget);

      gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                                   GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                                   -1);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

typedef struct _GtkTickCallbackInfo GtkTickCallbackInfo;

struct _GtkTickCallbackInfo
{
  guint refcount;

  guint id;
  GtkTickCallback callback;
  gpointer user_data;
  GDestroyNotify notify;

  guint destroyed : 1;
};

static void
ref_tick_callback_info (GtkTickCallbackInfo *info)
{
  info->refcount++;
}

static void
unref_tick_callback_info (GtkWidget           *widget,
                          GtkTickCallbackInfo *info,
                          GList               *link)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  info->refcount--;
  if (info->refcount == 0)
    {
      priv->tick_callbacks = g_list_delete_link (priv->tick_callbacks, link);
      if (info->notify)
        info->notify (info->user_data);
      g_free (info);
    }

  if (priv->tick_callbacks == NULL && priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);
      g_signal_handler_disconnect (frame_clock, priv->clock_tick_id);
      priv->clock_tick_id = 0;
      gdk_frame_clock_end_updating (frame_clock);
    }
}

static void
destroy_tick_callback_info (GtkWidget           *widget,
                            GtkTickCallbackInfo *info,
                            GList               *link)
{
  if (!info->destroyed)
    {
      info->destroyed = TRUE;
      unref_tick_callback_info (widget, info, link);
    }
}

static void
destroy_tick_callbacks (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  for (l = priv->tick_callbacks; l;)
    {
      GList *next = l->next;
      destroy_tick_callback_info (widget, l->data, l);
      l = next;
    }
}

static void
gtk_widget_on_frame_clock_update (GdkFrameClock *frame_clock,
                                  GtkWidget     *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_object_ref (widget);

  for (l = priv->tick_callbacks; l;)
    {
      GtkTickCallbackInfo *info = l->data;
      GList *next;

      ref_tick_callback_info (info);
      if (!info->destroyed)
        {
          if (info->callback (widget,
                              frame_clock,
                              info->user_data) == G_SOURCE_REMOVE)
            {
              destroy_tick_callback_info (widget, info, l);
            }
        }

      next = l->next;
      unref_tick_callback_info (widget, info, l);
      l = next;
    }

  g_object_unref (widget);
}

static guint tick_callback_id;

/**
 * gtk_widget_add_tick_callback:
 * @widget: a `GtkWidget`
 * @callback: function to call for updating animations
 * @user_data: (closure): data to pass to @callback
 * @notify: function to call to free @user_data when the callback is removed.
 *
 * Queues an animation frame update and adds a callback to be called
 * before each frame.
 *
 * Until the tick callback is removed, it will be called frequently
 * (usually at the frame rate of the output device or as quickly as
 * the application can be repainted, whichever is slower). For this
 * reason, is most suitable for handling graphics that change every
 * frame or every few frames. The tick callback does not automatically
 * imply a relayout or repaint. If you want a repaint or relayout, and
 * aren’t changing widget properties that would trigger that (for example,
 * changing the text of a `GtkLabel`), then you will have to call
 * [method@Gtk.Widget.queue_resize] or [method@Gtk.Widget.queue_draw]
 * yourself.
 *
 * [method@Gdk.FrameClock.get_frame_time] should generally be used
 * for timing continuous animations and
 * [method@Gdk.FrameTimings.get_predicted_presentation_time] if you are
 * trying to display isolated frames at particular times.
 *
 * This is a more convenient alternative to connecting directly to the
 * [signal@Gdk.FrameClock::update] signal of `GdkFrameClock`, since you
 * don't have to worry about when a `GdkFrameClock` is assigned to a widget.
 *
 * Returns: an id for the connection of this callback. Remove the callback
 *   by passing the id returned from this function to
 *   [method@Gtk.Widget.remove_tick_callback]
 */
guint
gtk_widget_add_tick_callback (GtkWidget       *widget,
                              GtkTickCallback  callback,
                              gpointer         user_data,
                              GDestroyNotify   notify)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkTickCallbackInfo *info;
  GdkFrameClock *frame_clock;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  if (priv->realized && !priv->clock_tick_id)
    {
      frame_clock = gtk_widget_get_frame_clock (widget);

      if (frame_clock)
        {
          priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                                  G_CALLBACK (gtk_widget_on_frame_clock_update),
                                                  widget);
          gdk_frame_clock_begin_updating (frame_clock);
        }
    }

  info = g_new0 (GtkTickCallbackInfo, 1);

  info->refcount = 1;
  info->id = ++tick_callback_id;
  info->callback = callback;
  info->user_data = user_data;
  info->notify = notify;

  priv->tick_callbacks = g_list_prepend (priv->tick_callbacks,
                                         info);

  return info->id;
}

/**
 * gtk_widget_remove_tick_callback:
 * @widget: a `GtkWidget`
 * @id: an id returned by [method@Gtk.Widget.add_tick_callback]
 *
 * Removes a tick callback previously registered with
 * gtk_widget_add_tick_callback().
 */
void
gtk_widget_remove_tick_callback (GtkWidget *widget,
                                 guint      id)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  for (l = priv->tick_callbacks; l; l = l->next)
    {
      GtkTickCallbackInfo *info = l->data;
      if (info->id == id)
        {
          destroy_tick_callback_info (widget, info, l);
          return;
        }
    }
}

gboolean
gtk_widget_has_tick_callback (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->tick_callbacks != NULL;
}

typedef struct _GtkSurfaceTransformChangedCallbackInfo GtkSurfaceTransformChangedCallbackInfo;

struct _GtkSurfaceTransformChangedCallbackInfo
{
  guint id;
  GtkSurfaceTransformChangedCallback callback;
  gpointer user_data;
  GDestroyNotify notify;
};

static void
surface_transform_changed_callback_info_destroy (GtkSurfaceTransformChangedCallbackInfo *info)
{
  if (info->notify)
    info->notify (info->user_data);

  g_free (info);
}

static void
notify_surface_transform_changed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  graphene_matrix_t *surface_transform;
  GList *l;

  if (surface_transform_data->cached_surface_transform_valid)
    surface_transform = &surface_transform_data->cached_surface_transform;
  else
    surface_transform = NULL;

  for (l = surface_transform_data->callbacks; l;)
    {
      GtkSurfaceTransformChangedCallbackInfo *info = l->data;
      GList *l_next = l->next;

      if (info->callback (widget,
                          surface_transform,
                          info->user_data) == G_SOURCE_REMOVE)
        {
          surface_transform_data->callbacks =
            g_list_delete_link (surface_transform_data->callbacks, l);
          surface_transform_changed_callback_info_destroy (info);
        }

      l = l_next;
    }
}

static void
destroy_surface_transform_data (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data;

  surface_transform_data = priv->surface_transform_data;
  if (!surface_transform_data)
    return;

  g_list_free_full (surface_transform_data->callbacks,
                    (GDestroyNotify) surface_transform_changed_callback_info_destroy);
  g_free (surface_transform_data);
  priv->surface_transform_data = NULL;
}

static void
sync_widget_surface_transform (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  gboolean was_valid;
  graphene_matrix_t prev_transform;

  was_valid = surface_transform_data->cached_surface_transform_valid;
  prev_transform = surface_transform_data->cached_surface_transform;

  if (GTK_IS_NATIVE (widget))
    {
      gsk_transform_to_matrix (priv->transform,
                               &surface_transform_data->cached_surface_transform);
      surface_transform_data->cached_surface_transform_valid = TRUE;
    }
  else if (!priv->root)
    {
      surface_transform_data->cached_surface_transform_valid = FALSE;
    }
  else
    {
      GtkWidget *native = GTK_WIDGET (gtk_widget_get_native (widget));

      if (gtk_widget_compute_transform (widget, native,
                                        &surface_transform_data->cached_surface_transform))
        {
          surface_transform_data->cached_surface_transform_valid = TRUE;
        }
      else
        {
          g_warning ("Could not compute surface transform");
          surface_transform_data->cached_surface_transform_valid = FALSE;
        }
    }

  if (was_valid != surface_transform_data->cached_surface_transform_valid ||
      (was_valid && surface_transform_data->cached_surface_transform_valid &&
       !graphene_matrix_equal (&surface_transform_data->cached_surface_transform,
                               &prev_transform)))
    notify_surface_transform_changed (widget);
}

static guint surface_transform_changed_callback_id;

static gboolean
parent_surface_transform_changed_cb (GtkWidget               *parent,
                                     const graphene_matrix_t *transform,
                                     gpointer                 user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);

  sync_widget_surface_transform (widget);

  return G_SOURCE_CONTINUE;
}

static void
remove_parent_surface_transform_changed_listener (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;

  g_assert (surface_transform_data->tracked_parent);

  gtk_widget_remove_surface_transform_changed_callback (
    surface_transform_data->tracked_parent,
    surface_transform_data->parent_surface_transform_changed_id);
  surface_transform_data->parent_surface_transform_changed_id = 0;
  g_clear_object (&surface_transform_data->tracked_parent);
}

static void
add_parent_surface_transform_changed_listener (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  GtkWidget *parent;


  g_assert (!surface_transform_data->tracked_parent);

  parent = priv->parent;
  surface_transform_data->parent_surface_transform_changed_id =
    gtk_widget_add_surface_transform_changed_callback (
      parent,
      parent_surface_transform_changed_cb,
      widget,
      NULL);
  surface_transform_data->tracked_parent = g_object_ref (parent);
}

static GtkWidgetSurfaceTransformData *
ensure_surface_transform_data (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->surface_transform_data)
    priv->surface_transform_data = g_new0 (GtkWidgetSurfaceTransformData, 1);

  return priv->surface_transform_data;
}

/**
 * gtk_widget_add_surface_transform_changed_callback:
 * @widget: a `GtkWidget`
 * @callback: a function to call when the surface transform changes
 * @user_data: (closure): data to pass to @callback
 * @notify: function to call to free @user_data when the callback is removed
 *
 * Invokes the callback whenever the surface relative transform of
 * the widget changes.
 *
 * Returns: an id for the connection of this callback. Remove the
 *   callback by passing the id returned from this function to
 *   [method@Gtk.Widget.remove_surface_transform_changed_callback]
 */
guint
gtk_widget_add_surface_transform_changed_callback (GtkWidget                          *widget,
                                                   GtkSurfaceTransformChangedCallback  callback,
                                                   gpointer                            user_data,
                                                   GDestroyNotify                      notify)
{
  GtkWidgetPrivate *priv;
  GtkWidgetSurfaceTransformData *surface_transform_data;
  GtkSurfaceTransformChangedCallbackInfo *info;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (callback, 0);

  priv = gtk_widget_get_instance_private (widget);
  surface_transform_data = ensure_surface_transform_data (widget);

  if (priv->parent &&
      !surface_transform_data->parent_surface_transform_changed_id)
    add_parent_surface_transform_changed_listener (widget);

  if (!surface_transform_data->callbacks)
    sync_widget_surface_transform (widget);

  info = g_new0 (GtkSurfaceTransformChangedCallbackInfo, 1);

  info->id = ++surface_transform_changed_callback_id;
  info->callback = callback;
  info->user_data = user_data;
  info->notify = notify;

  surface_transform_data->callbacks =
    g_list_prepend (surface_transform_data->callbacks, info);

  return info->id;
}

/**
 * gtk_widget_remove_surface_transform_changed_callback:
 * @widget: a `GtkWidget`
 * @id: an id returned by [method@Gtk.Widget.add_surface_transform_changed_callback]
 *
 * Removes a surface transform changed callback previously registered with
 * gtk_widget_add_surface_transform_changed_callback().
 */
void
gtk_widget_remove_surface_transform_changed_callback (GtkWidget *widget,
                                                      guint      id)
{
  GtkWidgetPrivate *priv;
  GtkWidgetSurfaceTransformData *surface_transform_data;
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (id);

  priv = gtk_widget_get_instance_private (widget);
  surface_transform_data = priv->surface_transform_data;

  g_return_if_fail (surface_transform_data);

  for (l = surface_transform_data->callbacks; l; l = l->next)
    {
      GtkSurfaceTransformChangedCallbackInfo *info = l->data;

      if (info->id == id)
        {
          surface_transform_data->callbacks =
            g_list_delete_link (surface_transform_data->callbacks, l);

          surface_transform_changed_callback_info_destroy (info);
          break;
        }
    }

  if (!surface_transform_data->callbacks)
    {
      if (surface_transform_data->tracked_parent)
        remove_parent_surface_transform_changed_listener (widget);
      g_free (surface_transform_data);
      priv->surface_transform_data = NULL;
    }
}

GdkSurface *
gtk_widget_get_surface (GtkWidget *widget)
{
  GtkNative *native = gtk_widget_get_native (widget);

  if (native)
    return gtk_native_get_surface (native);

  return NULL;
}

/**
 * gtk_widget_realize:
 * @widget: a `GtkWidget`
 *
 * Creates the GDK resources associated with a widget.
 *
 * Normally realization happens implicitly; if you show a widget
 * and all its parent containers, then the widget will be realized
 * and mapped automatically.
 *
 * Realizing a widget requires all the widget’s parent widgets to be
 * realized; calling this function realizes the widget’s parents
 * in addition to @widget itself. If a widget is not yet inside a
 * toplevel window when you realize it, bad things will happen.
 *
 * This function is primarily used in widget implementations, and
 * isn’t very useful otherwise. Many times when you think you might
 * need it, a better approach is to connect to a signal that will be
 * called after the widget is realized automatically, such as
 * [signal@Gtk.Widget::realize].
 */
void
gtk_widget_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->realized)
    return;

  gtk_widget_push_verify_invariants (widget);

  /*
    if (GTK_IS_CONTAINER (widget) && _gtk_widget_get_has_surface (widget))
    g_message ("gtk_widget_realize(%s)", G_OBJECT_TYPE_NAME (widget));
  */

  if (priv->parent == NULL && !GTK_IS_ROOT (widget))
    g_warning ("Calling gtk_widget_realize() on a widget that isn't "
               "inside a toplevel window is not going to work very well. "
               "Widgets must be inside a toplevel container before realizing them.");

  if (priv->parent && !_gtk_widget_get_realized (priv->parent))
    gtk_widget_realize (priv->parent);

  g_signal_emit (widget, widget_signals[REALIZE], 0);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (priv->context)
    gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));
  else
    gtk_widget_get_style_context (widget);
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_unrealize:
 * @widget: a `GtkWidget`
 *
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget).
 *
 * This function is only useful in widget implementations.
 */
void
gtk_widget_unrealize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);
  gtk_widget_push_verify_invariants (widget);

  if (_gtk_widget_get_realized (widget))
    {
      if (priv->mapped)
        gtk_widget_unmap (widget);

      g_signal_emit (widget, widget_signals[UNREALIZE], 0);
      g_assert (!priv->mapped);
      g_assert (!priv->realized);
    }

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_queue_draw:
 * @widget: a `GtkWidget`
 *
 * Schedules this widget to be redrawn in the paint phase
 * of the current or the next frame.
 *
 * This means @widget's [vfunc@Gtk.Widget.snapshot]
 * implementation will be called.
 */
void
gtk_widget_queue_draw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Just return if the widget isn't mapped */
  if (!_gtk_widget_get_mapped (widget))
    return;

  for (; widget; widget = _gtk_widget_get_parent (widget))
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (priv->draw_needed)
        break;

      priv->draw_needed = TRUE;
      g_clear_pointer (&priv->render_node, gsk_render_node_unref);
      if (GTK_IS_NATIVE (widget) && _gtk_widget_get_realized (widget))
        gdk_surface_queue_render (gtk_native_get_surface (GTK_NATIVE (widget)));
    }
}

static void
gtk_widget_set_alloc_needed (GtkWidget *widget);

/**
 * gtk_widget_queue_allocate:
 * @widget: a `GtkWidget`
 *
 * Flags the widget for a rerun of the [vfunc@Gtk.Widget.size_allocate]
 * function.
 *
 * Use this function instead of [method@Gtk.Widget.queue_resize]
 * when the @widget's size request didn't change but it wants to
 * reposition its contents.
 *
 * An example user of this function is [method@Gtk.Widget.set_halign].
 *
 * This function is only for use in widget implementations.
 */
void
gtk_widget_queue_allocate (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_realized (widget))
    gtk_widget_queue_draw (widget);

  gtk_widget_set_alloc_needed (widget);
}

static inline gboolean
gtk_widget_get_resize_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->resize_needed;
}

/*
 * gtk_widget_queue_resize_internal:
 * @widget: a `GtkWidget`
 *
 * Queue a resize on a widget, and on all other widgets
 * grouped with this widget.
 */
static void
gtk_widget_queue_resize_internal (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups, *l, *widgets;

  if (gtk_widget_get_resize_needed (widget))
    return;

  priv->resize_needed = TRUE;
  _gtk_size_request_cache_clear (&priv->requests);
  gtk_widget_set_alloc_needed (widget);

  if (priv->resize_func)
    priv->resize_func (widget);

  groups = _gtk_widget_get_sizegroups (widget);

  for (l = groups; l; l = l->next)
    {
      for (widgets = gtk_size_group_get_widgets (l->data); widgets; widgets = widgets->next)
        gtk_widget_queue_resize_internal (widgets->data);
    }

  if (_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent = _gtk_widget_get_parent (widget);
      if (parent)
        {
          if (GTK_IS_NATIVE (widget))
            gtk_widget_queue_allocate (parent);
          else
            gtk_widget_queue_resize_internal (parent);
        }
    }
}

/**
 * gtk_widget_queue_resize:
 * @widget: a `GtkWidget`
 *
 * Flags a widget to have its size renegotiated.
 *
 * This should be called when a widget for some reason has a new
 * size request. For example, when you change the text in a
 * [class@Gtk.Label], the label queues a resize to ensure there’s
 * enough space for the new text.
 *
 * Note that you cannot call gtk_widget_queue_resize() on a widget
 * from inside its implementation of the [vfunc@Gtk.Widget.size_allocate]
 * virtual method. Calls to gtk_widget_queue_resize() from inside
 * [vfunc@Gtk.Widget.size_allocate] will be silently ignored.
 *
 * This function is only for use in widget implementations.
 */
void
gtk_widget_queue_resize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_realized (widget))
    gtk_widget_queue_draw (widget);

  gtk_widget_queue_resize_internal (widget);
}

/**
 * gtk_widget_get_frame_clock:
 * @widget: a `GtkWidget`
 *
 * Obtains the frame clock for a widget.
 *
 * The frame clock is a global “ticker” that can be used to drive
 * animations and repaints. The most common reason to get the frame
 * clock is to call [method@Gdk.FrameClock.get_frame_time], in order
 * to get a time to use for animating. For example you might record
 * the start of the animation with an initial value from
 * [method@Gdk.FrameClock.get_frame_time], and then update the animation
 * by calling [method@Gdk.FrameClock.get_frame_time] again during each repaint.
 *
 * [method@Gdk.FrameClock.request_phase] will result in a new frame on the
 * clock, but won’t necessarily repaint any widgets. To repaint a
 * widget, you have to use [method@Gtk.Widget.queue_draw] which invalidates
 * the widget (thus scheduling it to receive a draw on the next
 * frame). gtk_widget_queue_draw() will also end up requesting a frame
 * on the appropriate frame clock.
 *
 * A widget’s frame clock will not change while the widget is
 * mapped. Reparenting a widget (which implies a temporary unmap) can
 * change the widget’s frame clock.
 *
 * Unrealized widgets do not have a frame clock.
 *
 * Returns: (nullable) (transfer none): a `GdkFrameClock`
 */
GdkFrameClock*
gtk_widget_get_frame_clock (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->realized)
    {
      GdkSurface *surface = gtk_widget_get_surface (widget);

      return gdk_surface_get_frame_clock (surface);
    }
  else
    {
      return NULL;
    }
}

static int
get_number (GtkCssValue *value)
{
  double d = _gtk_css_number_value_get (value, 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style->size->margin_top);
  margin->left = get_number (style->size->margin_left);
  margin->bottom = get_number (style->size->margin_bottom);
  margin->right = get_number (style->size->margin_right);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style->border->border_top_width);
  border->left = get_number (style->border->border_left_width);
  border->bottom = get_number (style->border->border_bottom_width);
  border->right = get_number (style->border->border_right_width);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style->size->padding_top);
  border->left = get_number (style->size->padding_left);
  border->bottom = get_number (style->size->padding_bottom);
  border->right = get_number (style->size->padding_right);
}

/**
 * gtk_widget_size_allocate:
 * @widget: a `GtkWidget`
 * @allocation: position and size to be allocated to @widget
 * @baseline: The baseline of the child, or -1
 *
 * Allocates widget with a transformation that translates
 * the origin to the position in @allocation.
 *
 * This is a simple form of [method@Gtk.Widget.allocate].
 */
void
gtk_widget_size_allocate (GtkWidget           *widget,
                          const GtkAllocation *allocation,
                          int                  baseline)
{
  GskTransform *transform;

  if (allocation->x || allocation->y)
    transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (allocation->x, allocation->y));
  else
    transform = NULL;

  gtk_widget_allocate (widget,
                       allocation->width,
                       allocation->height,
                       baseline,
                       transform);
}

/* translate initial/final into start/end */
static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    case GTK_ALIGN_FILL:
    case GTK_ALIGN_CENTER:
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_BASELINE_CENTER:
    default:
      return align;
    }
}

static void
adjust_for_align (GtkAlign  align,
                  int       natural_size,
                  int      *allocated_pos,
                  int      *allocated_size,
                  int       nat_baseline,
                  int      *allocated_baseline)
{
  switch (align)
    {
    case GTK_ALIGN_BASELINE_CENTER:
      if (*allocated_size > natural_size &&
          nat_baseline > -1 &&
          *allocated_baseline > -1)
        {
          *allocated_pos = *allocated_baseline - nat_baseline;
          *allocated_size = MIN (*allocated_size, natural_size);
          *allocated_baseline = nat_baseline;
          break;
        }
      G_GNUC_FALLTHROUGH;

    case GTK_ALIGN_CENTER:
      if (*allocated_size > natural_size)
        {
          *allocated_pos += (*allocated_size - natural_size) / 2;
          *allocated_size = MIN (*allocated_size, natural_size);
        }
      break;
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_FILL:
    default:
      /* change nothing */
      break;
    case GTK_ALIGN_START:
      /* keep *allocated_pos where it is */
      *allocated_size = MIN (*allocated_size, natural_size);
      break;
    case GTK_ALIGN_END:
      if (*allocated_size > natural_size)
        {
          *allocated_pos += (*allocated_size - natural_size);
          *allocated_size = natural_size;
        }
      break;
    }
}

static inline void
gtk_widget_adjust_size_allocation (GtkWidget     *widget,
                                   GtkAllocation *allocation,
                                   int           *baseline)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  int natural_width, natural_height, nat_baseline;
  int min_width, min_height;

  if (priv->halign == GTK_ALIGN_FILL && priv->valign == GTK_ALIGN_FILL)
    goto out;

  /* Note that adjust_for_align removes any margins from the
   * allocated sizes and possibly limits them to the natural sizes
   */

  if (priv->halign == GTK_ALIGN_FILL ||
      (priv->valign != GTK_ALIGN_FILL &&
       gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH))
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                          allocation->height + priv->margin.top + priv->margin.bottom,
                          &min_width, NULL, NULL, NULL);
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &natural_width, NULL, NULL);
      natural_width = MAX (min_width, natural_width);
      adjust_for_align (effective_align (priv->halign, _gtk_widget_get_direction (widget)),
                        natural_width - priv->margin.left - priv->margin.right,
                        &allocation->x,
                        &allocation->width,
                        -1, baseline);
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          allocation->width + priv->margin.left + priv->margin.right,
                          NULL, &natural_height, NULL, &nat_baseline);
      adjust_for_align (priv->valign,
                        natural_height - priv->margin.top - priv->margin.bottom,
                        &allocation->y,
                        &allocation->height,
                        nat_baseline > -1 ? nat_baseline - priv->margin.top : -1,
                        baseline);
    }
  else
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          allocation->width + priv->margin.left + priv->margin.right,
                          &min_height, NULL, NULL, NULL);
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          -1,
                          NULL, &natural_height, NULL, &nat_baseline);
      natural_height = MAX (min_height, natural_height);
      adjust_for_align (priv->valign,
                        natural_height - priv->margin.top - priv->margin.bottom,
                        &allocation->y,
                        &allocation->height,
                        nat_baseline > -1 ? nat_baseline - priv->margin.top : -1,
                        baseline);
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                          allocation->height + priv->margin.top + priv->margin.bottom,
                          &min_width, &natural_width, NULL, NULL);
      adjust_for_align (effective_align (priv->halign, _gtk_widget_get_direction (widget)),
                        natural_width - priv->margin.left - priv->margin.right,
                        &allocation->x,
                        &allocation->width,
                        -1, NULL);
    }

out:
  if (priv->valign != GTK_ALIGN_BASELINE_FILL &&
      priv->valign != GTK_ALIGN_BASELINE_CENTER)
    *baseline = -1;
}

static void
gtk_widget_ensure_allocate_on_children (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  g_assert (!priv->resize_needed);
  g_assert (!priv->alloc_needed);

  if (!priv->alloc_needed_on_child)
    return;

  priv->alloc_needed_on_child = FALSE;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_should_layout (child))
        gtk_widget_ensure_allocate (child);
    }
}

/**
 * gtk_widget_allocate:
 * @widget: A `GtkWidget`
 * @width: New width of @widget
 * @height: New height of @widget
 * @baseline: New baseline of @widget, or -1
 * @transform: (transfer full) (nullable): Transformation to be applied to @widget
 *
 * This function is only used by `GtkWidget` subclasses, to
 * assign a size, position and (optionally) baseline to their
 * child widgets.
 *
 * In this function, the allocation and baseline may be adjusted.
 * The given allocation will be forced to be bigger than the
 * widget's minimum size, as well as at least 0×0 in size.
 *
 * For a version that does not take a transform, see
 * [method@Gtk.Widget.size_allocate].
 */
void
gtk_widget_allocate (GtkWidget    *widget,
                     int           width,
                     int           height,
                     int           baseline,
                     GskTransform *transform)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GdkRectangle adjusted;
  gboolean alloc_needed;
  gboolean size_changed;
  gboolean baseline_changed;
  gboolean transform_changed;
  GtkCssStyle *style;
  GtkBorder margin, border, padding;
  GskTransform *css_transform;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (baseline >= -1);

  gtk_widget_push_verify_invariants (widget);

  if (!priv->visible && !GTK_IS_ROOT (widget))
    {
      gsk_transform_unref (transform);
      goto out;
    }

#ifdef G_ENABLE_DEBUG
  if (gtk_widget_get_resize_needed (widget))
    {
      g_warning ("Allocating size to %s %p without calling gtk_widget_measure(). "
                 "How does the code know the size to allocate?",
                 gtk_widget_get_name (widget), widget);
    }
  if (!GTK_IS_SCROLLABLE (widget))
    {
      int min;
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, width, &min, NULL, NULL, NULL);
      if (min > height)
        {
          g_critical ("Allocation height too small. Tried to allocate %dx%d, but %s %p needs "
                      "at least %dx%d.",
                      width, height,
                      gtk_widget_get_name (widget), widget,
                      width, min);
        }
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, height, &min, NULL, NULL, NULL);
      if (min > width)
        {
          g_critical ("Allocation width too small. Tried to allocate %dx%d, but %s %p needs "
                      "at least %dx%d.",
                      width, height,
                      gtk_widget_get_name (widget), widget,
                      min, height);
        }
    }
#endif /* G_ENABLE_DEBUG */

  alloc_needed = priv->alloc_needed;
  /* Preserve request/allocate ordering */
  priv->alloc_needed = FALSE;

  baseline_changed = priv->allocated_baseline != baseline;
  transform_changed = !gsk_transform_equal (priv->allocated_transform, transform);

  gsk_transform_unref (priv->allocated_transform);
  priv->allocated_transform = gsk_transform_ref (transform);
  priv->allocated_width = width;
  priv->allocated_height = height;
  priv->allocated_baseline = baseline;

  if (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    adjusted.x = priv->margin.left;
  else
    adjusted.x = priv->margin.right;
  adjusted.y = priv->margin.top;
  adjusted.width = width - priv->margin.left - priv->margin.right;
  adjusted.height = height - priv->margin.top - priv->margin.bottom;
  if (baseline >= 0)
    baseline -= priv->margin.top;

  gtk_widget_adjust_size_allocation (widget, &adjusted, &baseline);

  if (adjusted.width < 0 || adjusted.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate %s %s %p with width %d and height %d",
                 G_OBJECT_TYPE_NAME (widget), g_quark_to_string (gtk_css_node_get_name (priv->cssnode)), widget,
                 adjusted.width,
                 adjusted.height);

      adjusted.width = 0;
      adjusted.height = 0;
    }

  style = gtk_css_node_get_style (priv->cssnode);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  /* Apply CSS transformation */
  adjusted.x += margin.left;
  adjusted.y += margin.top;
  adjusted.width -= margin.left + margin.right;
  adjusted.height -= margin.top + margin.bottom;
  css_transform = gtk_css_transform_value_get_transform (style->other->transform);

  if (css_transform)
    {
      double origin_x, origin_y;

      origin_x = _gtk_css_position_value_get_x (style->other->transform_origin, adjusted.width);
      origin_y = _gtk_css_position_value_get_y (style->other->transform_origin, adjusted.height);

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (adjusted.x, adjusted.y));
      adjusted.x = adjusted.y = 0;

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (origin_x, origin_y));
      transform = gsk_transform_transform (transform, css_transform);
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- origin_x, - origin_y));

      gsk_transform_unref (css_transform);
    }

  adjusted.x += border.left + padding.left;
  adjusted.y += border.top + padding.top;

  if (baseline >= 0)
    baseline -= margin.top + border.top + padding.top;
  if (adjusted.x || adjusted.y)
    transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (adjusted.x, adjusted.y));

  gsk_transform_unref (priv->transform);
  priv->transform = transform;

  if (priv->surface_transform_data)
    sync_widget_surface_transform (widget);

  /* Since gtk_widget_measure does it for us, we can be sure here that
   * the given alloaction is large enough for the css margin/bordder/padding */
  adjusted.width -= border.left + padding.left +
                    border.right + padding.right;
  adjusted.height -= border.top + padding.top +
                     border.bottom + padding.bottom;
  size_changed = (priv->width != adjusted.width) || (priv->height != adjusted.height);

  if (!alloc_needed && !size_changed && !baseline_changed)
    {
      gtk_widget_ensure_allocate_on_children (widget);
    }
  else
    {
      priv->width = adjusted.width;
      priv->height = adjusted.height;
      priv->baseline = baseline;

      priv->alloc_needed_on_child = FALSE;

      if (priv->layout_manager != NULL)
        {
          gtk_layout_manager_allocate (priv->layout_manager, widget,
                                       priv->width,
                                       priv->height,
                                       baseline);
        }
      else
        {
          GTK_WIDGET_GET_CLASS (widget)->size_allocate (widget,
                                                        priv->width,
                                                        priv->height,
                                                        baseline);
        }

      /* Size allocation is god... after consulting god, no further requests or allocations are needed */
      if (GTK_DISPLAY_DEBUG_CHECK (_gtk_widget_get_display (widget), GEOMETRY) &&
          gtk_widget_get_resize_needed (widget))
        {
          g_warning ("%s %p or a child called gtk_widget_queue_resize() during size_allocate().",
                     gtk_widget_get_name (widget), widget);
        }

      gtk_widget_ensure_resize (widget);
      priv->alloc_needed = FALSE;

      gtk_widget_update_paintables (widget);

      if (size_changed)
        gtk_accessible_bounds_changed (GTK_ACCESSIBLE (widget));

      if (size_changed || baseline_changed)
        gtk_widget_queue_draw (widget);
    }

  if (transform_changed && priv->parent)
    gtk_widget_queue_draw (priv->parent);

out:
  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_common_ancestor:
 * @widget_a: a `GtkWidget`
 * @widget_b: a `GtkWidget`
 *
 * Find the common ancestor of @widget_a and @widget_b that
 * is closest to the two widgets.
 *
 * Returns: (nullable): the closest common ancestor of @widget_a and
 *   @widget_b or %NULL if @widget_a and @widget_b do not
 *   share a common ancestor.
 */
GtkWidget *
gtk_widget_common_ancestor (GtkWidget *widget_a,
                            GtkWidget *widget_b)
{
  GtkWidget *parent_a;
  GtkWidget *parent_b;
  int depth_a = 0;
  int depth_b = 0;

  parent_a = widget_a;
  while (parent_a->priv->parent)
    {
      parent_a = parent_a->priv->parent;
      depth_a++;
    }

  parent_b = widget_b;
  while (parent_b->priv->parent)
    {
      parent_b = parent_b->priv->parent;
      depth_b++;
    }

  if (parent_a != parent_b)
    return NULL;

  while (depth_a > depth_b)
    {
      widget_a = widget_a->priv->parent;
      depth_a--;
    }

  while (depth_b > depth_a)
    {
      widget_b = widget_b->priv->parent;
      depth_b--;
    }

  while (widget_a != widget_b)
    {
      widget_a = widget_a->priv->parent;
      widget_b = widget_b->priv->parent;
    }

  return widget_a;
}

/**
 * gtk_widget_translate_coordinates:
 * @src_widget:  a `GtkWidget`
 * @dest_widget: a `GtkWidget`
 * @src_x: X position relative to @src_widget
 * @src_y: Y position relative to @src_widget
 * @dest_x: (out) (optional): location to store X position relative to @dest_widget
 * @dest_y: (out) (optional): location to store Y position relative to @dest_widget
 *
 * Translate coordinates relative to @src_widget’s allocation
 * to coordinates relative to @dest_widget’s allocations.
 *
 * In order to perform this operation, both widget must share
 * a common ancestor.
 *
 * Returns: %FALSE if @src_widget and @dest_widget have no common
 *   ancestor. In this case, 0 is stored in *@dest_x and *@dest_y.
 *   Otherwise %TRUE.
 *
 * Deprecated: 4.12: Use gtk_widget_compute_point() instead
 */
gboolean
gtk_widget_translate_coordinates (GtkWidget  *src_widget,
                                  GtkWidget  *dest_widget,
                                  double      src_x,
                                  double      src_y,
                                  double     *dest_x,
                                  double     *dest_y)
{
  graphene_point_t p;

  if (!gtk_widget_compute_point (src_widget, dest_widget,
                                 &GRAPHENE_POINT_INIT (src_x, src_y),
                                 &p))
    return FALSE;

  if (dest_x)
    *dest_x = p.x;

  if (dest_y)
    *dest_y = p.y;

  return TRUE;
}

/**
 * gtk_widget_compute_point:
 * @widget: the `GtkWidget` to query
 * @target: the `GtkWidget` to transform into
 * @point: a point in @widget's coordinate system
 * @out_point: (out caller-allocates): Set to the corresponding coordinates in
 *   @target's coordinate system
 *
 * Translates the given @point in @widget's coordinates to coordinates
 * relative to @target’s coordinate system.
 *
 * In order to perform this operation, both widgets must share a
 * common ancestor.
 *
 * Returns: %TRUE if the point could be determined, %FALSE on failure.
 *   In this case, 0 is stored in @out_point.
 */
gboolean
gtk_widget_compute_point (GtkWidget              *widget,
                          GtkWidget              *target,
                          const graphene_point_t *point,
                          graphene_point_t       *out_point)
{
  graphene_matrix_t transform;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);

  if (!gtk_widget_compute_transform (widget, target, &transform))
    {
      graphene_point_init (out_point, 0, 0);
      return FALSE;
    }

  gsk_matrix_transform_point (&transform, point, out_point);

  return TRUE;
}

/**
 * gtk_widget_class_add_binding: (skip)
 * @widget_class: the class to add the binding to
 * @keyval: key value of binding to install
 * @mods: key modifier of binding to install
 * @callback: the callback to call upon activation
 * @format_string: (nullable): GVariant format string for arguments
 *   or %NULL for no arguments
 * @...: arguments, as given by format string
 *
 * Creates a new shortcut for @widget_class that calls the given @callback
 * with arguments read according to @format_string.
 *
 * The arguments and format string must be provided in the same way as
 * with g_variant_new().
 *
 * This function is a convenience wrapper around
 * [method@Gtk.WidgetClass.add_shortcut] and must be called during class
 * initialization. It does not provide for user_data, if you need that,
 * you will have to use [method@Gtk.WidgetClass.add_shortcut] with a custom
 * shortcut.
 */
void
gtk_widget_class_add_binding (GtkWidgetClass  *widget_class,
                              guint            keyval,
                              GdkModifierType  mods,
                              GtkShortcutFunc  callback,
                              const char      *format_string,
                              ...)
{
  GtkShortcut *shortcut;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (keyval, mods),
                               gtk_callback_action_new (callback, NULL, NULL));
  if (format_string)
    {
      va_list args;
      va_start (args, format_string);
      gtk_shortcut_set_arguments (shortcut,
                                  g_variant_new_va (format_string, NULL, &args));
      va_end (args);
    }

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

/**
 * gtk_widget_class_add_binding_signal: (skip)
 * @widget_class: the class to add the binding to
 * @keyval: key value of binding to install
 * @mods: key modifier of binding to install
 * @signal: the signal to execute
 * @format_string: (nullable): GVariant format string for arguments
 *   or %NULL for no arguments
 * @...: arguments, as given by format string
 *
 * Creates a new shortcut for @widget_class that emits the given action
 * @signal with arguments read according to @format_string.
 *
 * The arguments and format string must be provided in the same way as
 * with g_variant_new().
 *
 * This function is a convenience wrapper around
 * [method@Gtk.WidgetClass.add_shortcut] and must be called during class
 * initialization.
 */
void
gtk_widget_class_add_binding_signal (GtkWidgetClass  *widget_class,
                                     guint            keyval,
                                     GdkModifierType  mods,
                                     const char      *signal,
                                     const char      *format_string,
                                     ...)
{
  GtkShortcut *shortcut;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_signal_lookup (signal, G_TYPE_FROM_CLASS (widget_class)));
  /* XXX: validate variant format for signal */

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (keyval, mods),
                               gtk_signal_action_new (signal));
  if (format_string)
    {
      va_list args;
      va_start (args, format_string);
      gtk_shortcut_set_arguments (shortcut,
                                  g_variant_new_va (format_string, NULL, &args));
      va_end (args);
    }

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

/**
 * gtk_widget_class_add_binding_action: (skip)
 * @widget_class: the class to add the binding to
 * @keyval: key value of binding to install
 * @mods: key modifier of binding to install
 * @action_name: the action to activate
 * @format_string: (nullable): GVariant format string for arguments
 *   or %NULL for no arguments
 * @...: arguments, as given by format string
 *
 * Creates a new shortcut for @widget_class that activates the given
 * @action_name with arguments read according to @format_string.
 *
 * The arguments and format string must be provided in the same way as
 * with g_variant_new().
 *
 * This function is a convenience wrapper around
 * [method@Gtk.WidgetClass.add_shortcut] and must be called during class
 * initialization.
 */
void
gtk_widget_class_add_binding_action (GtkWidgetClass  *widget_class,
                                     guint            keyval,
                                     GdkModifierType  mods,
                                     const char      *action_name,
                                     const char      *format_string,
                                     ...)
{
  GtkShortcut *shortcut;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  /* XXX: validate variant format for action */

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (keyval, mods),
                               gtk_named_action_new (action_name));
  if (format_string)
    {
      va_list args;
      va_start (args, format_string);
      gtk_shortcut_set_arguments (shortcut,
                                  g_variant_new_va (format_string, NULL, &args));
      va_end (args);
    }

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

/**
 * gtk_widget_class_add_shortcut:
 * @widget_class: the class to add the shortcut to
 * @shortcut: (transfer none): the `GtkShortcut` to add
 *
 * Installs a shortcut in @widget_class.
 *
 * Every instance created for @widget_class or its subclasses will
 * inherit this shortcut and trigger it.
 *
 * Shortcuts added this way will be triggered in the %GTK_PHASE_BUBBLE
 * phase, which means they may also trigger if child widgets have focus.
 *
 * This function must only be used in class initialization functions
 * otherwise it is not guaranteed that the shortcut will be installed.
 */
void
gtk_widget_class_add_shortcut (GtkWidgetClass *widget_class,
                               GtkShortcut    *shortcut)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  priv = widget_class->priv;

  g_list_store_append (priv->shortcuts, shortcut);
}

/**
 * gtk_widget_mnemonic_activate:
 * @widget: a `GtkWidget`
 * @group_cycling: %TRUE if there are other widgets with the same mnemonic
 *
 * Emits the ::mnemonic-activate signal.
 *
 * See [signal@Gtk.Widget::mnemonic-activate].
 *
 * Returns: %TRUE if the signal has been handled
 */
gboolean
gtk_widget_mnemonic_activate (GtkWidget *widget,
                              gboolean   group_cycling)
{
  gboolean handled;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  group_cycling = group_cycling != FALSE;
  if (!gtk_widget_is_sensitive (widget))
    handled = TRUE;
  else
    g_signal_emit (widget,
                   widget_signals[MNEMONIC_ACTIVATE],
                   0,
                   group_cycling,
                   &handled);
  return handled;
}

/*< private >
 * gtk_widget_can_activate:
 * @self: a `GtkWidget`
 *
 * Checks whether a `GtkWidget` can be activated.
 *
 * To activate a widget, use [method@Gtk.Widget.activate].
 */
gboolean
gtk_widget_can_activate (GtkWidget *self)
{
  g_return_val_if_fail (GTK_IS_WIDGET (self), FALSE);

  GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS (self);

  if (widget_class->priv->activate_signal != 0)
    return TRUE;

  return FALSE;
}

static gboolean
get_effective_can_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->can_focus)
    return FALSE;

  if (priv->parent)
    return get_effective_can_focus (priv->parent);

  return TRUE;
}

static gboolean
gtk_widget_real_mnemonic_activate (GtkWidget *widget,
                                   gboolean   group_cycling)
{
  if (!group_cycling && gtk_widget_can_activate (widget))
    gtk_widget_activate (widget);
  else if (get_effective_can_focus (widget))
    return gtk_widget_grab_focus (widget);
  else
    {
      g_warning ("widget '%s' isn't suitable for mnemonic activation",
                 G_OBJECT_TYPE_NAME (widget));
      gtk_widget_error_bell (widget);
    }
  return TRUE;
}

#define WIDGET_REALIZED_FOR_EVENT(widget, event) \
     (gdk_event_get_event_type (event) == GDK_FOCUS_CHANGE || _gtk_widget_get_realized (widget))

gboolean
gtk_widget_run_controllers (GtkWidget           *widget,
                            GdkEvent            *event,
                            GtkWidget           *target,
                            double               x,
                            double               y,
                            GtkPropagationPhase  phase)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkEventController *controller;
  gboolean handled = FALSE;
  GList *l;

  g_object_ref (widget);

  l = priv->event_controllers;
  while (l != NULL)
    {
      GList *next = l->next;

      if (!WIDGET_REALIZED_FOR_EVENT (widget, event))
        break;

      controller = l->data;

      if (controller == NULL)
        {
          priv->event_controllers = g_list_delete_link (priv->event_controllers, l);
        }
      else
        {
          GtkPropagationPhase controller_phase;

          controller_phase = gtk_event_controller_get_propagation_phase (controller);

          if (controller_phase == phase)
            {
              gboolean this_handled;
              gboolean is_gesture;

              is_gesture = GTK_IS_GESTURE (controller);
              this_handled = gtk_event_controller_handle_event (controller, event, target, x, y);

              if (GTK_DEBUG_CHECK (KEYBINDINGS))
                {
                  GdkEventType type = gdk_event_get_event_type (event);
                  if (this_handled &&
                      (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE))
                    {
                      g_message ("key %s (keyval %d) handled at widget %s by controller %s",
                                 type == GDK_KEY_PRESS ? "press" : "release",
                                 gdk_key_event_get_keyval (event),
                                 G_OBJECT_TYPE_NAME (widget),
                                 gtk_event_controller_get_name (controller));
                    }
                }

              handled |= this_handled;

              /* Non-gesture controllers are basically unique entities not meant
               * to collaborate with anything else. Break early if any such event
               * controller handled the event.
               */
              if (this_handled && !is_gesture)
                break;
            }
        }

      l = next;
    }

  g_object_unref (widget);

  return handled;
}

void
gtk_widget_handle_crossing (GtkWidget             *widget,
                            const GtkCrossingData *crossing,
                            double                 x,
                            double                 y)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_object_ref (widget);

  if (crossing->old_target)
    g_object_ref (crossing->old_target);
  if (crossing->new_target)
    g_object_ref (crossing->new_target);
  if (crossing->old_descendent)
    g_object_ref (crossing->old_descendent);
  if (crossing->new_descendent)
    g_object_ref (crossing->new_descendent);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;
      gtk_event_controller_handle_crossing (controller, crossing, x, y);
    }

  if (crossing->old_target)
    g_object_unref (crossing->old_target);
  if (crossing->new_target)
    g_object_unref (crossing->new_target);
  if (crossing->old_descendent)
    g_object_unref (crossing->old_descendent);
  if (crossing->new_descendent)
    g_object_unref (crossing->new_descendent);

  g_object_unref (widget);
}

static gboolean
event_surface_is_still_viewable (GdkEvent *event)
{
  /* Check that we think the event's window is viewable before
   * delivering the event, to prevent surprises. We do this here
   * at the last moment, since the event may have been queued
   * up behind other events, held over a recursive main loop, etc.
   */
  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_ENTER_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_SCROLL:
      return gdk_surface_get_mapped (gdk_event_get_surface (event));

#if 0
    /* The following events are the second half of paired events;
     * we always deliver them to deal with widgets that clean up
     * on the second half.
     */
    case GDK_BUTTON_RELEASE:
    case GDK_KEY_RELEASE:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROXIMITY_OUT:
#endif

    default:
      /* Remaining events would make sense on a not-viewable window,
       * or don't have an associated window.
       */
      return TRUE;
    }
}

static gboolean
translate_event_coordinates (GdkEvent  *event,
                             double    *x,
                             double    *y,
                             GtkWidget *widget)
{
  GtkWidget *event_widget;
  graphene_point_t p;
  double event_x, event_y;
  GtkNative *native;
  double nx, ny;

  *x = *y = 0;

  if (!gdk_event_get_position (event, &event_x, &event_y))
    return FALSE;

  event_widget = gtk_get_event_widget (event);
  native = gtk_widget_get_native (event_widget);
  gtk_native_get_surface_transform (native, &nx, &ny);
  event_x -= nx;
  event_y -= ny;

  if (!gtk_widget_compute_point (event_widget,
                                 widget,
                                 &GRAPHENE_POINT_INIT (event_x, event_y),
                                 &p))
    return FALSE;

  *x = p.x;
  *y = p.y;

  return TRUE;
}

gboolean
_gtk_widget_captured_event (GtkWidget *widget,
                            GdkEvent  *event,
                            GtkWidget *target)
{
  gboolean return_val = FALSE;
  double x, y;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (!event_surface_is_still_viewable (event))
    return TRUE;

  translate_event_coordinates (event, &x, &y, widget);

  return_val = gtk_widget_run_controllers (widget, event, target, x, y, GTK_PHASE_CAPTURE);
  return_val |= !WIDGET_REALIZED_FOR_EVENT (widget, event);

  return return_val;
}

gboolean
gtk_widget_event (GtkWidget *widget,
                  GdkEvent  *event,
                  GtkWidget *target)
{
  gboolean return_val = FALSE;
  double x, y;

  /* We check only once for is-still-visible; if someone
   * hides the window in one of the signals on the widget,
   * they are responsible for returning TRUE to terminate
   * handling.
   */
  if (!event_surface_is_still_viewable (event))
    return TRUE;

  if (!_gtk_widget_get_mapped (widget))
    return FALSE;

  translate_event_coordinates (event, &x, &y, widget);

  if (widget == target)
    return_val |= gtk_widget_run_controllers (widget, event, target, x, y, GTK_PHASE_TARGET);

  if (return_val == FALSE)
    return_val |= gtk_widget_run_controllers (widget, event, target, x, y, GTK_PHASE_BUBBLE);

  return return_val;
}

/**
 * gtk_widget_class_get_activate_signal:
 * @widget_class: a `GtkWidgetClass`
 *
 * Retrieves the signal id for the activation signal.
 *
 * the activation signal is set using
 * [method@Gtk.WidgetClass.set_activate_signal].
 *
 * Returns: a signal id, or 0 if the widget class does not
 *   specify an activation signal
 */
guint
gtk_widget_class_get_activate_signal (GtkWidgetClass *widget_class)
{
  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), 0);

  return widget_class->priv->activate_signal;
}

/**
 * gtk_widget_class_set_activate_signal:
 * @widget_class: a `GtkWidgetClass`
 * @signal_id: the id for the activate signal
 *
 * Sets the `GtkWidgetClass.activate_signal` field with the
 * given @signal_id.
 *
 * The signal will be emitted when calling [method@Gtk.Widget.activate].
 *
 * The @signal_id must have been registered with `g_signal_new()`
 * or g_signal_newv() before calling this function.
 */
void
gtk_widget_class_set_activate_signal (GtkWidgetClass *widget_class,
                                      guint           signal_id)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (signal_id != 0);

  widget_class->priv->activate_signal = signal_id;
}

/**
 * gtk_widget_class_set_activate_signal_from_name:
 * @widget_class: a `GtkWidgetClass`
 * @signal_name: the name of the activate signal of @widget_type
 *
 * Sets the `GtkWidgetClass.activate_signal` field with the signal id for
 * the given @signal_name.
 *
 * The signal will be emitted when calling [method@Gtk.Widget.activate].
 *
 * The @signal_name of @widget_type must have been registered with
 * g_signal_new() or g_signal_newv() before calling this function.
 */
void
gtk_widget_class_set_activate_signal_from_name (GtkWidgetClass *widget_class,
                                                const char     *signal_name)
{
  guint signal_id;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (signal_name != NULL);

  signal_id = g_signal_lookup (signal_name, G_TYPE_FROM_CLASS (widget_class));
  if (signal_id == 0)
    {
      g_critical ("Widget type “%s” does not have a “%s” signal",
                  G_OBJECT_CLASS_NAME (widget_class),
                  signal_name);
      return;
    }

  widget_class->priv->activate_signal = signal_id;
}

/**
 * gtk_widget_activate:
 * @widget: a `GtkWidget` that’s activatable
 *
 * For widgets that can be “activated” (buttons, menu items, etc.),
 * this function activates them.
 *
 * The activation will emit the signal set using
 * [method@Gtk.WidgetClass.set_activate_signal] during class initialization.
 *
 * Activation is what happens when you press <kbd>Enter</kbd>
 * on a widget during key navigation.
 *
 * If you wish to handle the activation keybinding yourself, it is
 * recommended to use [method@Gtk.WidgetClass.add_shortcut] with an action
 * created with [ctor@Gtk.SignalAction.new].
 *
 * If @widget isn't activatable, the function returns %FALSE.
 *
 * Returns: %TRUE if the widget was activatable
 */
gboolean
gtk_widget_activate (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (gtk_widget_can_activate (widget))
    {
      GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS (widget);
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget, widget_class->priv->activate_signal, 0);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_widget_grab_focus:
 * @widget: a `GtkWidget`
 *
 * Causes @widget to have the keyboard focus for the `GtkWindow` it's inside.
 *
 * If @widget is not focusable, or its [vfunc@Gtk.Widget.grab_focus]
 * implementation cannot transfer the focus to a descendant of @widget
 * that is focusable, it will not take focus and %FALSE will be returned.
 *
 * Calling [method@Gtk.Widget.grab_focus] on an already focused widget
 * is allowed, should not have an effect, and return %TRUE.
 *
 * Returns: %TRUE if focus is now inside @widget.
 */
gboolean
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!gtk_widget_is_sensitive (widget) ||
      !get_effective_can_focus (widget) ||
      widget->priv->root == NULL)
    return FALSE;

  return GTK_WIDGET_GET_CLASS (widget)->grab_focus (widget);
}

gboolean
gtk_widget_grab_focus_self (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->focusable)
    return FALSE;

  gtk_root_set_focus (priv->root, widget);

  return TRUE;
}

gboolean
gtk_widget_grab_focus_child (GtkWidget *widget)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_grab_focus (child))
        return TRUE;
    }

  return FALSE;
}

gboolean
gtk_widget_query_tooltip (GtkWidget  *widget,
                          int         x,
                          int         y,
                          gboolean    keyboard_mode,
                          GtkTooltip *tooltip)
{
  gboolean retval = FALSE;

  g_signal_emit (widget,
                 widget_signals[QUERY_TOOLTIP],
                 0,
                 x, y,
                 keyboard_mode,
                 tooltip,
                 &retval);

  return retval;
}

static void
gtk_widget_real_css_changed (GtkWidget         *widget,
                             GtkCssStyleChange *change)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (change)
    {
      const gboolean has_text = gtk_widget_peek_pango_context (widget) != NULL;

      if (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT))
        gtk_widget_update_default_pango_context (widget);

      if (priv->root)
        {
          if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE) ||
              (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_SIZE)))
            {
              gtk_widget_queue_resize (widget);
            }
          else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TRANSFORM) &&
                   priv->parent)
            {
              gtk_widget_queue_allocate (priv->parent);
            }

          if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_REDRAW) ||
              (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_CONTENT)))
            {
              gtk_widget_queue_draw (widget);
            }
        }
    }
  else
    {
      gtk_widget_update_default_pango_context (widget);

      if (priv->root)
        gtk_widget_queue_resize (widget);
    }
}

static void
gtk_widget_real_system_setting_changed (GtkWidget        *widget,
                                        GtkSystemSetting  setting)
{
  GtkWidget *child;

  if (setting == GTK_SYSTEM_SETTING_DPI ||
      setting == GTK_SYSTEM_SETTING_FONT_NAME ||
      setting == GTK_SYSTEM_SETTING_FONT_CONFIG)
    {
      gtk_widget_update_default_pango_context (widget);
      if (gtk_widget_peek_pango_context (widget))
        gtk_widget_queue_resize (widget);
    }

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      gtk_widget_system_setting_changed (child, setting);
    }
}

static gboolean
direction_is_forward (GtkDirectionType direction)
{
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_RIGHT:
    case GTK_DIR_DOWN:
      return TRUE;
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_UP:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gtk_widget_real_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  GtkWidget *focus;

  /* For focusable widgets, we want to focus the widget
   * before its children. We differentiate 3 cases:
   * 1) focus is currently on widget
   * 2) focus is on some child
   * 3) focus is outside
   */

  if (gtk_widget_is_focus (widget))
    {
      if (direction_is_forward (direction) &&
          gtk_widget_focus_move (widget, direction))
        return TRUE;

      return FALSE;
    }

  focus = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (widget)));

  if (focus && gtk_widget_is_ancestor (focus, widget))
    {
      if (gtk_widget_focus_move (widget, direction))
        return TRUE;

      if (direction_is_forward (direction))
        return FALSE;
      else
        return gtk_widget_grab_focus (widget);
    }

  if (!direction_is_forward (direction))
    {
      if (gtk_widget_focus_move (widget, direction))
        return TRUE;

      return gtk_widget_grab_focus (widget);
    }
  else
    {
      if (gtk_widget_grab_focus (widget))
        return TRUE;

      return gtk_widget_focus_move (widget, direction);
    }
}

gboolean
gtk_widget_focus_self (GtkWidget         *widget,
                       GtkDirectionType   direction)
{
  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  return FALSE;
}

gboolean
gtk_widget_focus_child (GtkWidget         *widget,
                        GtkDirectionType   direction)
{
  return gtk_widget_focus_move (widget, direction);
}

static void
gtk_widget_real_move_focus (GtkWidget         *widget,
                            GtkDirectionType   direction)
{
  GtkRoot *root;

  root = _gtk_widget_get_root (widget);
  if (widget != GTK_WIDGET (root))
    g_signal_emit (root, widget_signals[MOVE_FOCUS], 0, direction);
}

static gboolean
gtk_widget_real_keynav_failed (GtkWidget        *widget,
                               GtkDirectionType  direction)
{
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      return FALSE;

    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
    default:
      break;
    }

  gtk_widget_error_bell (widget);

  return TRUE;
}

/**
 * gtk_widget_set_can_focus: (attributes org.gtk.Method.set_property=can-focus)
 * @widget: a `GtkWidget`
 * @can_focus: whether or not the input focus can enter
 *   the widget or any of its children
 *
 * Specifies whether the input focus can enter the widget
 * or any of its children.
 *
 * Applications should set @can_focus to %FALSE to mark a
 * widget as for pointer/touch use only.
 *
 * Note that having @can_focus be %TRUE is only one of the
 * necessary conditions for being focusable. A widget must
 * also be sensitive and focusable and not have an ancestor
 * that is marked as not can-focus in order to receive input
 * focus.
 *
 * See [method@Gtk.Widget.grab_focus] for actually setting
 * the input focus on a widget.
 */
void
gtk_widget_set_can_focus (GtkWidget *widget,
                          gboolean   can_focus)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->can_focus != can_focus)
    {
      priv->can_focus = can_focus;

      gtk_widget_queue_resize (widget);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CAN_FOCUS]);
    }
}

/**
 * gtk_widget_get_can_focus: (attributes org.gtk.Method.get_property=can-focus)
 * @widget: a `GtkWidget`
 *
 * Determines whether the input focus can enter @widget or any
 * of its children.
 *
 * See [method@Gtk.Widget.set_focusable].
 *
 * Returns: %TRUE if the input focus can enter @widget, %FALSE otherwise
 */
gboolean
gtk_widget_get_can_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);

  return priv->can_focus;
}

/**
 * gtk_widget_set_focusable: (attributes org.gtk.Method.set_property=focusable)
 * @widget: a `GtkWidget`
 * @focusable: whether or not @widget can own the input focus
 *
 * Specifies whether @widget can own the input focus.
 *
 * Widget implementations should set @focusable to %TRUE in
 * their init() function if they want to receive keyboard input.
 *
 * Note that having @focusable be %TRUE is only one of the
 * necessary conditions for being focusable. A widget must
 * also be sensitive and can-focus and not have an ancestor
 * that is marked as not can-focus in order to receive input
 * focus.
 *
 * See [method@Gtk.Widget.grab_focus] for actually setting
 * the input focus on a widget.
 */
void
gtk_widget_set_focusable (GtkWidget *widget,
                          gboolean   focusable)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->focusable == focusable)
    return;

  priv->focusable = focusable;

  gtk_widget_queue_resize (widget);

  gtk_accessible_platform_changed (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_FOCUSABLE]);
}

/**
 * gtk_widget_get_focusable: (attributes org.gtk.Method.get_property=focusable)
 * @widget: a `GtkWidget`
 *
 * Determines whether @widget can own the input focus.
 *
 * See [method@Gtk.Widget.set_focusable].
 *
 * Returns: %TRUE if @widget can own the input focus, %FALSE otherwise
 */
gboolean
gtk_widget_get_focusable (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->focusable;
}

/**
 * gtk_widget_has_focus: (attributes org.gtk.Method.get_property=has-focus)
 * @widget: a `GtkWidget`
 *
 * Determines if the widget has the global input focus.
 *
 * See [method@Gtk.Widget.is_focus] for the difference between
 * having the global input focus, and only having the focus
 * within a toplevel.
 *
 * Returns: %TRUE if the widget has the global input focus.
 */
gboolean
gtk_widget_has_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_focus;
}

/**
 * gtk_widget_has_visible_focus:
 * @widget: a `GtkWidget`
 *
 * Determines if the widget should show a visible indication that
 * it has the global input focus.
 *
 * This is a convenience function that takes into account whether
 * focus indication should currently be shown in the toplevel window
 * of @widget. See [method@Gtk.Window.get_focus_visible] for more
 * information about focus indication.
 *
 * To find out if the widget has the global input focus, use
 * [method@Gtk.Widget.has_focus].
 *
 * Returns: %TRUE if the widget should display a “focus rectangle”
 */
gboolean
gtk_widget_has_visible_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean draw_focus;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (priv->has_focus)
    {
      GtkRoot *root = _gtk_widget_get_root (widget);

      if (GTK_IS_WINDOW (root))
        draw_focus = gtk_window_get_focus_visible (GTK_WINDOW (root));
      else
        draw_focus = TRUE;
    }
  else
    draw_focus = FALSE;

  return draw_focus;
}

/**
 * gtk_widget_is_focus:
 * @widget: a `GtkWidget`
 *
 * Determines if the widget is the focus widget within its
 * toplevel.
 *
 * This does not mean that the [property@Gtk.Widget:has-focus]
 * property is necessarily set; [property@Gtk.Widget:has-focus]
 * will only be set if the toplevel widget additionally has the
 * global input focus.
 *
 * Returns: %TRUE if the widget is the focus widget.
 */
gboolean
gtk_widget_is_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (priv->root)
    return widget == gtk_root_get_focus (priv->root);

  return FALSE;
}

/**
 * gtk_widget_set_focus_on_click: (attributes org.gtk.Method.set_property=focus-on-click)
 * @widget: a `GtkWidget`
 * @focus_on_click: whether the widget should grab focus when clicked
 *   with the mouse
 *
 * Sets whether the widget should grab focus when it is clicked
 * with the mouse.
 *
 * Making mouse clicks not grab focus is useful in places like
 * toolbars where you don’t want the keyboard focus removed from
 * the main area of the application.
 */
void
gtk_widget_set_focus_on_click (GtkWidget *widget,
                               gboolean   focus_on_click)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  focus_on_click = focus_on_click != FALSE;

  if (priv->focus_on_click != focus_on_click)
    {
      priv->focus_on_click = focus_on_click;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_FOCUS_ON_CLICK]);
    }
}

/**
 * gtk_widget_get_focus_on_click: (attributes org.gtk.Method.get_property=focus-on-click)
 * @widget: a `GtkWidget`
 *
 * Returns whether the widget should grab focus when it is clicked
 * with the mouse.
 *
 * See [method@Gtk.Widget.set_focus_on_click].
 *
 * Returns: %TRUE if the widget should grab focus when it is
 *   clicked with the mouse
 */
gboolean
gtk_widget_get_focus_on_click (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->focus_on_click;
}

/**
 * gtk_widget_has_default: (attributes org.gtk.Method.get_property=has-default)
 * @widget: a `GtkWidget`
 *
 * Determines whether @widget is the current default widget
 * within its toplevel.
 *
 * Returns: %TRUE if @widget is the current default widget
 *   within its toplevel, %FALSE otherwise
 */
gboolean
gtk_widget_has_default (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_default;
}

void
_gtk_widget_set_has_default (GtkWidget *widget,
                             gboolean   has_default)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->has_default = has_default;

  if (has_default)
    gtk_widget_add_css_class (widget, "default");
  else
    gtk_widget_remove_css_class (widget, "default");
}

/**
 * gtk_widget_set_receives_default: (attributes org.gtk.Method.set_property=receives-default)
 * @widget: a `GtkWidget`
 * @receives_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget will be treated as the default
 * widget within its toplevel when it has the focus, even if
 * another widget is the default.
 */
void
gtk_widget_set_receives_default (GtkWidget *widget,
                                 gboolean   receives_default)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->receives_default != receives_default)
    {
      priv->receives_default = receives_default;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_RECEIVES_DEFAULT]);
    }
}

/**
 * gtk_widget_get_receives_default: (attributes org.gtk.Method.get_property=receives-default)
 * @widget: a `GtkWidget`
 *
 * Determines whether @widget is always treated as the default widget
 * within its toplevel when it has the focus, even if another widget
 * is the default.
 *
 * See [method@Gtk.Widget.set_receives_default].
 *
 * Returns: %TRUE if @widget acts as the default widget when focused,
 *   %FALSE otherwise
 */
gboolean
gtk_widget_get_receives_default (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->receives_default;
}

/*< private >
 * gtk_widget_has_grab:
 * @widget: a `GtkWidget`
 *
 * Determines whether the widget is currently grabbing events, so it
 * is the only widget receiving input events (keyboard and mouse).
 *
 * See also gtk_grab_add().
 *
 * Returns: %TRUE if the widget is in the grab_widgets stack
 */
gboolean
gtk_widget_has_grab (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!GTK_IS_WIDGET (widget)) return FALSE;

  return priv->has_grab;
}

void
_gtk_widget_set_has_grab (GtkWidget *widget,
                          gboolean   has_grab)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->has_grab = has_grab;
}

/**
 * gtk_widget_set_name: (attributes org.gtk.Method.set_property=name)
 * @widget: a `GtkWidget`
 * @name: name for the widget
 *
 * Sets a widgets name.
 *
 * Setting a name allows you to refer to the widget from a
 * CSS file. You can apply a style to widgets with a particular name
 * in the CSS file. See the documentation for the CSS syntax (on the
 * same page as the docs for [class@Gtk.StyleContext].
 *
 * Note that the CSS syntax has certain special characters to delimit
 * and represent elements in a selector (period, #, >, *...), so using
 * these will make your widget impossible to match by name. Any combination
 * of alphanumeric symbols, dashes and underscores will suffice.
 */
void
gtk_widget_set_name (GtkWidget  *widget,
                     const char *name)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_free (priv->name);
  priv->name = g_strdup (name);

  gtk_css_node_set_id (priv->cssnode, g_quark_from_string (priv->name));

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_NAME]);
}

/**
 * gtk_widget_get_name: (attributes org.gtk.Method.get_property=name)
 * @widget: a `GtkWidget`
 *
 * Retrieves the name of a widget.
 *
 * See [method@Gtk.Widget.set_name] for the significance of widget names.
 *
 * Returns: name of the widget. This string is owned by GTK and
 *   should not be modified or freed
 */
const char *
gtk_widget_get_name (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->name)
    return priv->name;
  return G_OBJECT_TYPE_NAME (widget);
}

static void
gtk_widget_update_state_flags (GtkWidget     *widget,
                               GtkStateFlags  flags_to_set,
                               GtkStateFlags  flags_to_unset)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  /* Handle insensitive first, since it is propagated
   * differently throughout the widget hierarchy.
   */
  if ((priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags_to_unset & GTK_STATE_FLAG_INSENSITIVE))
    gtk_widget_set_sensitive (widget, TRUE);
  else if (!(priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags_to_set & GTK_STATE_FLAG_INSENSITIVE))
    gtk_widget_set_sensitive (widget, FALSE);

  flags_to_set &= ~(GTK_STATE_FLAG_INSENSITIVE);
  flags_to_unset &= ~(GTK_STATE_FLAG_INSENSITIVE);

  if (flags_to_set != 0 || flags_to_unset != 0)
    {
      GtkStateData data;

      data.old_scale_factor = gtk_widget_get_scale_factor (widget);
      data.flags_to_set = flags_to_set;
      data.flags_to_unset = flags_to_unset;

      gtk_widget_propagate_state (widget, &data);
    }
}

/**
 * gtk_widget_set_state_flags:
 * @widget: a `GtkWidget`
 * @flags: State flags to turn on
 * @clear: Whether to clear state before turning on @flags
 *
 * Turns on flag values in the current widget state.
 *
 * Typical widget states are insensitive, prelighted, etc.
 *
 * This function accepts the values %GTK_STATE_FLAG_DIR_LTR and
 * %GTK_STATE_FLAG_DIR_RTL but ignores them. If you want to set
 * the widget's direction, use [method@Gtk.Widget.set_direction].
 *
 * This function is for use in widget implementations.
 */
void
gtk_widget_set_state_flags (GtkWidget     *widget,
                            GtkStateFlags  flags,
                            gboolean       clear)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

#define ALLOWED_FLAGS (~(GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL))

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((!clear && (priv->state_flags & flags) == flags) ||
      (clear && priv->state_flags == flags))
    return;

  if (clear)
    gtk_widget_update_state_flags (widget, flags & ALLOWED_FLAGS, ~flags & ALLOWED_FLAGS);
  else
    gtk_widget_update_state_flags (widget, flags & ALLOWED_FLAGS, 0);

#undef ALLOWED_FLAGS
}

/**
 * gtk_widget_unset_state_flags:
 * @widget: a `GtkWidget`
 * @flags: State flags to turn off
 *
 * Turns off flag values for the current widget state.
 *
 * See [method@Gtk.Widget.set_state_flags].
 *
 * This function is for use in widget implementations.
 */
void
gtk_widget_unset_state_flags (GtkWidget     *widget,
                              GtkStateFlags  flags)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((priv->state_flags & flags) == 0)
    return;

  gtk_widget_update_state_flags (widget, 0, flags);
}

/**
 * gtk_widget_get_state_flags:
 * @widget: a `GtkWidget`
 *
 * Returns the widget state as a flag set.
 *
 * It is worth mentioning that the effective %GTK_STATE_FLAG_INSENSITIVE
 * state will be returned, that is, also based on parent insensitivity,
 * even if @widget itself is sensitive.
 *
 * Also note that if you are looking for a way to obtain the
 * [flags@Gtk.StateFlags] to pass to a [class@Gtk.StyleContext]
 * method, you should look at [method@Gtk.StyleContext.get_state].
 *
 * Returns: The state flags for widget
 */
GtkStateFlags
gtk_widget_get_state_flags (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->state_flags;
}

/**
 * gtk_widget_set_visible: (attributes org.gtk.Method.set_property=visible)
 * @widget: a `GtkWidget`
 * @visible: whether the widget should be shown or not
 *
 * Sets the visibility state of @widget.
 *
 * Note that setting this to %TRUE doesn’t mean the widget is
 * actually viewable, see [method@Gtk.Widget.get_visible].
 */
void
gtk_widget_set_visible (GtkWidget *widget,
                        gboolean   visible)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (visible)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
G_GNUC_END_IGNORE_DEPRECATIONS
}

void
_gtk_widget_set_visible_flag (GtkWidget *widget,
                              gboolean   visible)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->visible = visible;

  if (!visible)
    {
      g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);
      priv->allocated_width = 0;
      priv->allocated_height = 0;
      priv->allocated_baseline = 0;
      g_clear_pointer (&priv->transform, gsk_transform_unref);
      priv->width = 0;
      priv->height = 0;
      gtk_widget_update_paintables (widget);
    }
}

/**
 * gtk_widget_get_visible: (attributes org.gtk.Method.get_property=visible)
 * @widget: a `GtkWidget`
 *
 * Determines whether the widget is visible.
 *
 * If you want to take into account whether the widget’s
 * parent is also marked as visible, use
 * [method@Gtk.Widget.is_visible] instead.
 *
 * This function does not check if the widget is
 * obscured in any way.
 *
 * See [method@Gtk.Widget.set_visible].
 *
 * Returns: %TRUE if the widget is visible
 */
gboolean
gtk_widget_get_visible (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->visible;
}

/**
 * gtk_widget_is_visible:
 * @widget: a `GtkWidget`
 *
 * Determines whether the widget and all its parents are marked as
 * visible.
 *
 * This function does not check if the widget is obscured in any way.
 *
 * See also [method@Gtk.Widget.get_visible] and
 * [method@Gtk.Widget.set_visible].
 *
 * Returns: %TRUE if the widget and all its parents are visible
 */
gboolean
gtk_widget_is_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  while (widget)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (!priv->visible)
        return FALSE;

      widget = priv->parent;
    }

  return TRUE;
}

/**
 * gtk_widget_is_drawable:
 * @widget: a `GtkWidget`
 *
 * Determines whether @widget can be drawn to.
 *
 * A widget can be drawn if it is mapped and visible.
 *
 * Returns: %TRUE if @widget is drawable, %FALSE otherwise
 */
gboolean
gtk_widget_is_drawable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (_gtk_widget_get_visible (widget) &&
          _gtk_widget_get_mapped (widget));
}

/**
 * gtk_widget_get_realized:
 * @widget: a `GtkWidget`
 *
 * Determines whether @widget is realized.
 *
 * Returns: %TRUE if @widget is realized, %FALSE otherwise
 */
gboolean
gtk_widget_get_realized (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->realized;
}

/**
 * gtk_widget_get_mapped:
 * @widget: a `GtkWidget`
 *
 * Whether the widget is mapped.
 *
 * Returns: %TRUE if the widget is mapped, %FALSE otherwise.
 */
gboolean
gtk_widget_get_mapped (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->mapped;
}

/**
 * gtk_widget_set_sensitive: (attributes org.gtk.Method.set_property=sensitive)
 * @widget: a `GtkWidget`
 * @sensitive: %TRUE to make the widget sensitive
 *
 * Sets the sensitivity of a widget.
 *
 * A widget is sensitive if the user can interact with it.
 * Insensitive widgets are “grayed out” and the user can’t
 * interact with them. Insensitive widgets are known as
 * “inactive”, “disabled”, or “ghosted” in some other toolkits.
 */
void
gtk_widget_set_sensitive (GtkWidget *widget,
                          gboolean   sensitive)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  sensitive = (sensitive != FALSE);

  if (priv->sensitive == sensitive)
    return;

  priv->sensitive = sensitive;

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      gtk_event_controller_reset (controller);
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                               GTK_ACCESSIBLE_STATE_DISABLED, !sensitive,
                               -1);

  if (priv->parent == NULL
      || gtk_widget_is_sensitive (priv->parent))
    {
      GtkStateData data;

      data.old_scale_factor = gtk_widget_get_scale_factor (widget);

      if (sensitive)
        {
          data.flags_to_set = 0;
          data.flags_to_unset = GTK_STATE_FLAG_INSENSITIVE;
        }
      else
        {
          data.flags_to_set = GTK_STATE_FLAG_INSENSITIVE;
          data.flags_to_unset = GTK_STATE_FLAG_PRELIGHT |
                                GTK_STATE_FLAG_ACTIVE;
        }

      gtk_widget_propagate_state (widget, &data);
      update_cursor_on_state_change (widget);
    }

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_SENSITIVE]);
}

/**
 * gtk_widget_get_sensitive: (attributes org.gtk.Method.get_property=sensitive)
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s sensitivity.
 *
 * This function returns the value that has been set using
 * [method@Gtk.Widget.set_sensitive]).
 *
 * The effective sensitivity of a widget is however determined
 * by both its own and its parent widget’s sensitivity.
 * See [method@Gtk.Widget.is_sensitive].
 *
 * Returns: %TRUE if the widget is sensitive
 */
gboolean
gtk_widget_get_sensitive (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->sensitive;
}

/**
 * gtk_widget_is_sensitive:
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s effective sensitivity.
 *
 * This means it is sensitive itself and also its
 * parent widget is sensitive.
 *
 * Returns: %TRUE if the widget is effectively sensitive
 */
gboolean
gtk_widget_is_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return _gtk_widget_is_sensitive (widget);
}


/* Insert @widget into the children list of @parent,
 * after @previous_sibling */
static void
gtk_widget_reposition_after (GtkWidget *widget,
                             GtkWidget *parent,
                             GtkWidget *previous_sibling)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStateFlags parent_flags;
  GtkWidget *prev_parent, *prev_previous;
  GtkStateData data;

  prev_parent = priv->parent;
  prev_previous = priv->prev_sibling;

  if (priv->parent == parent && previous_sibling == prev_previous)
    return;

  if (priv->parent != NULL && priv->parent != parent)
    {
      g_warning ("Can't set new parent %s %p on widget %s %p, "
                 "which already has parent %s %p",
                 gtk_widget_get_name (parent), (void *)parent,
                 gtk_widget_get_name (widget), (void *)widget,
                 gtk_widget_get_name (priv->parent), (void *)priv->parent);
      return;
    }

  data.old_scale_factor = gtk_widget_get_scale_factor (widget);

  if (priv->parent == NULL)
    g_object_ref_sink (widget);

  gtk_widget_push_verify_invariants (widget);

  priv->parent = parent;

  if (previous_sibling)
    {
      if (previous_sibling->priv->next_sibling)
        previous_sibling->priv->next_sibling->priv->prev_sibling = widget;

      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;

      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;


      if (parent->priv->first_child == widget)
        parent->priv->first_child = priv->next_sibling;

      if (parent->priv->last_child == widget)
        parent->priv->last_child = priv->prev_sibling;

      priv->prev_sibling = previous_sibling;
      priv->next_sibling = previous_sibling->priv->next_sibling;
      previous_sibling->priv->next_sibling = widget;

      if (parent->priv->last_child == previous_sibling)
        parent->priv->last_child = widget;
      else if (parent->priv->last_child == widget)
        parent->priv->last_child = priv->next_sibling;
    }
  else
    {
      /* Beginning */
      if (parent->priv->last_child == widget)
        {
          parent->priv->last_child = priv->prev_sibling;
          if (priv->prev_sibling)
            priv->prev_sibling->priv->next_sibling = NULL;
        }
      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;

      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;

      priv->prev_sibling = NULL;
      priv->next_sibling = parent->priv->first_child;
      if (parent->priv->first_child)
        parent->priv->first_child->priv->prev_sibling = widget;

      parent->priv->first_child = widget;

      if (parent->priv->last_child == NULL)
        parent->priv->last_child = widget;
    }

  parent_flags = _gtk_widget_get_state_flags (parent);

  /* Merge both old state and current parent state,
   * making sure to only propagate the right states */
  data.flags_to_set = parent_flags & GTK_STATE_FLAGS_DO_SET_PROPAGATE;
  data.flags_to_unset = 0;
  gtk_widget_propagate_state (widget, &data);

  gtk_css_node_insert_after (parent->priv->cssnode,
                             priv->cssnode,
                             previous_sibling ? previous_sibling->priv->cssnode : NULL);

  _gtk_widget_update_parent_muxer (widget);

  if (parent->priv->root && priv->root == NULL)
    gtk_widget_root (widget);

  if (parent->priv->children_observer)
    {
      if (prev_previous)
        gtk_list_list_model_item_moved (parent->priv->children_observer, widget, prev_previous);
      else
        gtk_list_list_model_item_added (parent->priv->children_observer, widget);
    }

  if (prev_parent == NULL)
    g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_PARENT]);

  /* Enforce mapped invariants */
  if (_gtk_widget_get_visible (priv->parent) &&
      _gtk_widget_get_visible (widget))
    {
      if (_gtk_widget_get_child_visible (widget) &&
          _gtk_widget_get_mapped (priv->parent))
        gtk_widget_map (widget);

      gtk_widget_queue_resize (priv->parent);
    }

  /* child may cause parent's expand to change, if the child is
   * expanded. If child is not expanded, then it can't modify the
   * parent's expand. If the child becomes expanded later then it will
   * queue compute_expand then. This optimization plus defaulting
   * newly-constructed widgets to need_compute_expand=FALSE should
   * mean that initially building a widget tree doesn't have to keep
   * walking up setting need_compute_expand on parents over and over.
   *
   * We can't change a parent to need to expand unless we're visible.
   */
  if (_gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (parent);
    }

  if (prev_parent == NULL)
    gtk_accessible_update_children (GTK_ACCESSIBLE (parent),
                                    GTK_ACCESSIBLE (widget),
                                    GTK_ACCESSIBLE_CHILD_STATE_ADDED);

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_set_parent: (attributes org.gtk.Method.set_property=parent)
 * @widget: a `GtkWidget`
 * @parent: parent widget
 *
 * Sets @parent as the parent widget of @widget.
 *
 * This takes care of details such as updating the state and style
 * of the child to reflect its new location and resizing the parent.
 * The opposite function is [method@Gtk.Widget.unparent].
 *
 * This function is useful only when implementing subclasses of
 * `GtkWidget`.
 */
void
gtk_widget_set_parent (GtkWidget *widget,
                       GtkWidget *parent)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (_gtk_widget_get_parent (widget) == NULL);

  gtk_widget_reposition_after (widget,
                               parent,
                               _gtk_widget_get_last_child (parent));
}

/**
 * gtk_widget_get_parent: (attributes org.gtk.Method.get_property=parent)
 * @widget: a `GtkWidget`
 *
 * Returns the parent widget of @widget.
 *
 * Returns: (transfer none) (nullable): the parent widget of @widget
 */
GtkWidget *
gtk_widget_get_parent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->parent;
}

/**
 * gtk_widget_get_root: (attributes org.gtk.Method.get_property=root)
 * @widget: a `GtkWidget`
 *
 * Returns the `GtkRoot` widget of @widget.
 *
 * This function will return %NULL if the widget is not contained
 * inside a widget tree with a root widget.
 *
 * `GtkRoot` widgets will return themselves here.
 *
 * Returns: (transfer none) (nullable): the root widget of @widget
 */
GtkRoot *
gtk_widget_get_root (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return _gtk_widget_get_root (widget);
}

/**
 * gtk_widget_get_native:
 * @widget: a `GtkWidget`
 *
 * Returns the nearest `GtkNative` ancestor of @widget.
 *
 * This function will return %NULL if the widget is not
 * contained inside a widget tree with a native ancestor.
 *
 * `GtkNative` widgets will return themselves here.
 *
 * Returns: (transfer none) (nullable): the `GtkNative` ancestor of @widget
 */
GtkNative *
gtk_widget_get_native (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return (GtkNative *)gtk_widget_get_ancestor (widget, GTK_TYPE_NATIVE);
}

static void
gtk_widget_real_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  gtk_widget_queue_resize (widget);
}

#ifdef G_ENABLE_CONSISTENCY_CHECKS

/* Verify invariants, see docs/widget_system.txt for notes on much of
 * this.  Invariants may be temporarily broken while we’re in the
 * process of updating state, of course, so you can only
 * verify_invariants() after a given operation is complete.
 * Use push/pop_verify_invariants to help with that.
 */
static void
gtk_widget_verify_invariants (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *parent;

  if (priv->verifying_invariants_count > 0)
    return;

  parent = priv->parent;

  if (priv->mapped)
    {
      /* Mapped implies ... */

      if (!priv->realized)
        g_warning ("%s %p is mapped but not realized",
                   gtk_widget_get_name (widget), widget);

      if (!priv->visible)
        g_warning ("%s %p is mapped but not visible",
                   gtk_widget_get_name (widget), widget);

      if (!priv->child_visible && !GTK_IS_ROOT (widget))
        g_warning ("%s %p is mapped but not child_visible",
                   gtk_widget_get_name (widget), widget);
    }
  else
    {
      /* Not mapped implies... */

#if 0
  /* This check makes sense for normal toplevels, but for
   * something like a toplevel that is embedded within a clutter
   * state, mapping may depend on external factors.
   */
      if (widget->priv->toplevel)
        {
          if (widget->priv->visible)
            g_warning ("%s %p toplevel is visible but not mapped",
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
#endif
    }

  /* Parent related checks aren't possible if parent has
   * verifying_invariants_count > 0 because parent needs to recurse
   * children first before the invariants will hold.
   */
  if (parent == NULL || parent->priv->verifying_invariants_count == 0)
    {
      if (parent &&
          parent->priv->realized)
        {
          /* Parent realized implies... */

#if 0
          /* This is in widget_system.txt but appears to fail
           * because there's no gtk_container_realize() that
           * realizes all children... instead we just lazily
           * wait for map to fix things up.
           */
          if (!widget->priv->realized)
            g_warning ("%s %p is realized but child %s %p is not realized",
                       G_OBJECT_TYPE_NAME (parent), parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
#endif
        }
      else if (priv->realized && !GTK_IS_ROOT (widget))
        {
          /* No parent or parent not realized on non-toplevel implies... */
          g_warning ("%s %p is not realized but child %s %p is realized",
                     parent ? gtk_widget_get_name (parent) : "no parent", parent,
                     gtk_widget_get_name (widget), widget);
        }

      if (parent &&
          parent->priv->mapped &&
          priv->visible &&
          priv->child_visible)
        {
          /* Parent mapped and we are visible implies... */

          if (!priv->mapped)
            g_warning ("%s %p is mapped but visible child %s %p is not mapped",
                       gtk_widget_get_name (parent), parent,
                       gtk_widget_get_name (widget), widget);
        }
      else if (priv->mapped && !GTK_IS_ROOT (widget))
        {
          /* No parent or parent not mapped on non-toplevel implies... */
          g_warning ("%s %p is mapped but visible=%d child_visible=%d parent %s %p mapped=%d",
                     gtk_widget_get_name (widget), widget,
                     priv->visible,
                     priv->child_visible,
                     parent ? gtk_widget_get_name (parent) : "no parent", parent,
                     parent ? parent->priv->mapped : FALSE);
        }
    }

  if (!priv->realized)
    {
      /* Not realized implies... */

#if 0
      /* widget_system.txt says these hold, but they don't. */
      if (widget->priv->alloc_needed)
        g_warning ("%s %p alloc needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->width_request_needed)
        g_warning ("%s %p width request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->height_request_needed)
        g_warning ("%s %p height request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);
#endif
    }
}

/* The point of this push/pop is that invariants may not hold while
 * we’re busy making changes. So we only check at the outermost call
 * on the call stack, after we finish updating everything.
 */
static void
gtk_widget_push_verify_invariants (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->verifying_invariants_count += 1;
}

static void
gtk_widget_pop_verify_invariants (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (priv->verifying_invariants_count > 0);

  priv->verifying_invariants_count -= 1;

  if (priv->verifying_invariants_count == 0)
    {
      GtkWidget *child;
      gtk_widget_verify_invariants (widget);

      /* Check one level of children, because our
       * push_verify_invariants() will have prevented some of the
       * checks. This does not recurse because if recursion is
       * needed, it will happen naturally as each child has a
       * push/pop on that child. For example if we're recursively
       * mapping children, we'll push/pop on each child as we map
       * it.
       */
      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          gtk_widget_verify_invariants (child);
        }
    }
}
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

static PangoContext *
gtk_widget_peek_pango_context (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
}

/**
 * gtk_widget_get_pango_context:
 * @widget: a `GtkWidget`
 *
 * Gets a `PangoContext` with the appropriate font map, font description,
 * and base direction for this widget.
 *
 * Unlike the context returned by [method@Gtk.Widget.create_pango_context],
 * this context is owned by the widget (it can be used until the screen
 * for the widget changes or the widget is removed from its toplevel),
 * and will be updated to match any changes to the widget’s attributes.
 * This can be tracked by listening to changes of the
 * [property@Gtk.Widget:root] property on the widget.
 *
 * Returns: (transfer none): the `PangoContext` for the widget.
 */
PangoContext *
gtk_widget_get_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
  if (!context)
    {
      context = gtk_widget_create_pango_context (GTK_WIDGET (widget));
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_pango_context,
                               context,
                               g_object_unref);
    }

  return context;
}

static PangoFontMap *
gtk_widget_get_effective_font_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  PangoFontMap *font_map;

  font_map = PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
  if (font_map)
    return font_map;
  else if (priv->parent)
    return gtk_widget_get_effective_font_map (priv->parent);
  else
    return pango_cairo_font_map_get_default ();
}

gboolean
gtk_widget_update_pango_context (GtkWidget        *widget,
                                 PangoContext     *context,
                                 GtkTextDirection  direction)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssStyle *style = gtk_css_node_get_style (priv->cssnode);
  PangoFontDescription *font_desc;
  GtkSettings *settings;
  cairo_font_options_t *font_options;
  guint old_serial;
  gboolean hint_font_metrics = FALSE;
  int scale;

  old_serial = pango_context_get_serial (context);

  font_desc = gtk_css_style_get_pango_font (style);
  pango_context_set_font_description (context, font_desc);
  pango_font_description_free (font_desc);

  scale = gtk_widget_get_scale_factor (widget);
  settings = gtk_widget_get_settings (widget);

  if (settings != NULL &&
      cairo_version () >= CAIRO_VERSION_ENCODE (1, 17, 4))
    {
      g_object_get (settings, "gtk-hint-font-metrics", &hint_font_metrics, NULL);

      /* Override the user setting on non-HiDPI */
      if (scale == 1)
        hint_font_metrics = TRUE;

      pango_context_set_round_glyph_positions (context, hint_font_metrics);
    }

  if (direction != GTK_TEXT_DIR_NONE)
    pango_context_set_base_dir (context, direction == GTK_TEXT_DIR_LTR
                                         ? PANGO_DIRECTION_LTR
                                         : PANGO_DIRECTION_RTL);

  pango_cairo_context_set_resolution (context, _gtk_css_number_value_get (style->core->dpi, 100));

  font_options = (cairo_font_options_t*)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
  if (settings && font_options)
    {
      cairo_font_options_t *options;

      options = cairo_font_options_copy (gtk_settings_get_font_options (settings));
      cairo_font_options_merge (options, font_options);

      cairo_font_options_set_hint_metrics (options,
                                           hint_font_metrics == 1 ? CAIRO_HINT_METRICS_ON
                                                                  : CAIRO_HINT_METRICS_OFF);

      pango_cairo_context_set_font_options (context, options);
      cairo_font_options_destroy (options);
    }
  else if (settings)
    {
      cairo_font_options_t *options;

      options = cairo_font_options_copy (gtk_settings_get_font_options (settings));
      cairo_font_options_set_hint_metrics (options,
                                           hint_font_metrics == 1 ? CAIRO_HINT_METRICS_ON
                                                                  : CAIRO_HINT_METRICS_OFF);

      pango_cairo_context_set_font_options (context, options);
      cairo_font_options_destroy (options);
    }

  pango_context_set_font_map (context, gtk_widget_get_effective_font_map (widget));

  return old_serial != pango_context_get_serial (context);
}

static void
gtk_widget_update_default_pango_context (GtkWidget *widget)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);

  if (!context)
    return;

  if (gtk_widget_update_pango_context (widget, context, _gtk_widget_get_direction (widget)))
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_widget_set_font_options:
 * @widget: a `GtkWidget`
 * @options: (nullable): a `cairo_font_options_t`
 *   to unset any previously set default font options
 *
 * Sets the `cairo_font_options_t` used for Pango rendering
 * in this widget.
 *
 * When not set, the default font options for the `GdkDisplay`
 * will be used.
 */
void
gtk_widget_set_font_options (GtkWidget                  *widget,
                             const cairo_font_options_t *options)
{
  cairo_font_options_t *font_options;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  font_options = (cairo_font_options_t *)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
  if (font_options != options)
    {
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_font_options,
                               options ? cairo_font_options_copy (options) : NULL,
                               (GDestroyNotify)cairo_font_options_destroy);

      gtk_widget_update_default_pango_context (widget);
    }
}

/**
 * gtk_widget_get_font_options:
 * @widget: a `GtkWidget`
 *
 * Returns the `cairo_font_options_t` of widget.
 *
 * Seee [method@Gtk.Widget.set_font_options].
 *
 * Returns: (transfer none) (nullable): the `cairo_font_options_t`
 *   of widget
 */
const cairo_font_options_t *
gtk_widget_get_font_options (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return (cairo_font_options_t *)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
}

static void
gtk_widget_set_font_map_recurse (GtkWidget *widget, gpointer user_data)
{
  if (g_object_get_qdata (G_OBJECT (widget), quark_font_map))
    return;

  gtk_widget_update_default_pango_context (widget);

  gtk_widget_forall (widget, gtk_widget_set_font_map_recurse, user_data);
}

/**
 * gtk_widget_set_font_map:
 * @widget: a `GtkWidget`
 * @font_map: (nullable): a `PangoFontMap`, or %NULL to unset any
 *   previously set font map
 *
 * Sets the font map to use for Pango rendering.
 *
 * The font map is the object that is used to look up fonts.
 * Setting a custom font map can be useful in special situations,
 * e.g. when you need to add application-specific fonts to the set
 * of available fonts.
 *
 * When not set, the widget will inherit the font map from its parent.
 */
void
gtk_widget_set_font_map (GtkWidget    *widget,
                         PangoFontMap *font_map)
{
  PangoFontMap *map;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  map = PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
  if (map == font_map)
    return;

  g_object_set_qdata_full (G_OBJECT (widget),
                           quark_font_map,
                           g_object_ref (font_map),
                           g_object_unref);

  gtk_widget_update_default_pango_context (widget);

  gtk_widget_forall (widget, gtk_widget_set_font_map_recurse, NULL);
}

/**
 * gtk_widget_get_font_map:
 * @widget: a `GtkWidget`
 *
 * Gets the font map of @widget.
 *
 * See [method@Gtk.Widget.set_font_map].
 *
 * Returns: (transfer none) (nullable): A `PangoFontMap`
 */
PangoFontMap *
gtk_widget_get_font_map (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a `GtkWidget`
 *
 * Creates a new `PangoContext` with the appropriate font map,
 * font options, font description, and base direction for drawing
 * text for this widget.
 *
 * See also [method@Gtk.Widget.get_pango_context].
 *
 * Returns: (transfer full): the new `PangoContext`
 */
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
  gtk_widget_update_pango_context (widget, context, _gtk_widget_get_direction (widget));
  pango_context_set_language (context, gtk_get_default_language ());

  return context;
}

/**
 * gtk_widget_create_pango_layout:
 * @widget: a `GtkWidget`
 * @text: (nullable): text to set on the layout
 *
 * Creates a new `PangoLayout` with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget.
 *
 * If you keep a `PangoLayout` created in this way around,
 * you need to re-create it when the widget `PangoContext`
 * is replaced. This can be tracked by listening to changes
 * of the [property@Gtk.Widget:root] property on the widget.
 *
 * Returns: (transfer full): the new `PangoLayout`
 **/
PangoLayout *
gtk_widget_create_pango_layout (GtkWidget  *widget,
                                const char *text)
{
  PangoLayout *layout;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

/**
 * gtk_widget_set_child_visible:
 * @widget: a `GtkWidget`
 * @child_visible: if %TRUE, @widget should be mapped along
 *   with its parent.
 *
 * Sets whether @widget should be mapped along with its parent.
 *
 * The child visibility can be set for widget before it is added
 * to a container with [method@Gtk.Widget.set_parent], to avoid
 * mapping children unnecessary before immediately unmapping them.
 * However it will be reset to its default state of %TRUE when the
 * widget is removed from a container.
 *
 * Note that changing the child visibility of a widget does not
 * queue a resize on the widget. Most of the time, the size of
 * a widget is computed from all visible children, whether or
 * not they are mapped. If this is not the case, the container
 * can queue a resize itself.
 *
 * This function is only useful for container implementations
 * and should never be called by an application.
 */
void
gtk_widget_set_child_visible (GtkWidget *widget,
                              gboolean   child_visible)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_IS_ROOT (widget));

  child_visible = !!child_visible;

  if (priv->child_visible == child_visible)
    return;

  g_object_ref (widget);
  gtk_widget_verify_invariants (widget);

  if (child_visible)
    priv->child_visible = TRUE;
  else
    {
      GtkRoot *root;

      priv->child_visible = FALSE;

      root = _gtk_widget_get_root (widget);
      if (GTK_WIDGET (root) != widget && GTK_IS_WINDOW (root))
        _gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);
    }

  if (priv->parent && _gtk_widget_get_realized (priv->parent))
    {
      if (_gtk_widget_get_mapped (priv->parent) &&
          priv->child_visible &&
          _gtk_widget_get_visible (widget))
        gtk_widget_map (widget);
      else
        gtk_widget_unmap (widget);
    }

  gtk_widget_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_get_child_visible:
 * @widget: a `GtkWidget`
 *
 * Gets the value set with gtk_widget_set_child_visible().
 *
 * If you feel a need to use this function, your code probably
 * needs reorganization.
 *
 * This function is only useful for container implementations
 * and should never be called by an application.
 *
 * Returns: %TRUE if the widget is mapped with the parent.
 */
gboolean
gtk_widget_get_child_visible (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->child_visible;
}

void
_gtk_widget_scale_changed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (priv->context)
    gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_update_default_pango_context (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_SCALE_FACTOR]);

  gtk_widget_forall (widget, (GtkCallback)_gtk_widget_scale_changed, NULL);
}

/**
 * gtk_widget_get_scale_factor: (attributes org.gtk.Method.get_property=scale-factor)
 * @widget: a `GtkWidget`
 *
 * Retrieves the internal scale factor that maps from window
 * coordinates to the actual device pixels.
 *
 * On traditional systems this is 1, on high density outputs,
 * it can be a higher value (typically 2).
 *
 * See [method@Gdk.Surface.get_scale_factor].
 *
 * Returns: the scale factor for @widget
 */
int
gtk_widget_get_scale_factor (GtkWidget *widget)
{
  GtkWidget *root;
  GdkDisplay *display;
  GdkMonitor *monitor;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 1);

  if (_gtk_widget_get_realized (widget))
    {
      GdkSurface *surface = gtk_widget_get_surface (widget);

      if (surface)
        return gdk_surface_get_scale_factor (surface);
    }

  root = (GtkWidget *)_gtk_widget_get_root (widget);
  if (root && root != widget)
    return gtk_widget_get_scale_factor (root);

  /* else fall back to something that is more likely to be right than
   * just returning 1:
   */
  display = _gtk_widget_get_display (widget);
  if (display)
    {
      monitor = g_list_model_get_item (gdk_display_get_monitors (display), 0);
      if (monitor)
        {
          int result = gdk_monitor_get_scale_factor (monitor);
          g_object_unref (monitor);
          return result;
        }
    }

  return 1;
}

/**
 * gtk_widget_get_display:
 * @widget: a `GtkWidget`
 *
 * Get the `GdkDisplay` for the toplevel window associated with
 * this widget.
 *
 * This function can only be called after the widget has been
 * added to a widget hierarchy with a `GtkWindow` at the top.
 *
 * In general, you should only create display specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 *
 * Returns: (transfer none): the `GdkDisplay` for the toplevel
 *   for this widget.
 */
GdkDisplay *
gtk_widget_get_display (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return _gtk_widget_get_display (widget);
}

/**
 * gtk_widget_child_focus:
 * @widget: a `GtkWidget`
 * @direction: direction of focus movement
 *
 * Called by widgets as the user moves around the window using
 * keyboard shortcuts.
 *
 * The @direction argument indicates what kind of motion is taking place (up,
 * down, left, right, tab forward, tab backward).
 *
 * This function calls the [vfunc@Gtk.Widget.focus] virtual function; widgets
 * can override the virtual function in order to implement appropriate focus
 * behavior.
 *
 * The default `focus()` virtual function for a widget should return `TRUE` if
 * moving in @direction left the focus on a focusable location inside that
 * widget, and `FALSE` if moving in @direction moved the focus outside the
 * widget. When returning `TRUE`, widgets normally call [method@Gtk.Widget.grab_focus]
 * to place the focus accordingly; when returning `FALSE`, they don’t modify
 * the current focus location.
 *
 * This function is used by custom widget implementations; if you're
 * writing an app, you’d use [method@Gtk.Widget.grab_focus] to move
 * the focus to a particular widget.
 *
 * Returns: %TRUE if focus ended up inside @widget
 */
gboolean
gtk_widget_child_focus (GtkWidget       *widget,
                        GtkDirectionType direction)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!_gtk_widget_get_visible (widget) ||
      !gtk_widget_is_sensitive (widget) ||
      !gtk_widget_get_can_focus (widget))
    return FALSE;

  /* Emit ::focus in any case, even if focusable is FALSE,
   * since any widget might have child widgets that will take
   * focus
   */

  return GTK_WIDGET_GET_CLASS (widget)->focus (widget, direction);
}

/**
 * gtk_widget_keynav_failed:
 * @widget: a `GtkWidget`
 * @direction: direction of focus movement
 *
 * Emits the `::keynav-failed` signal on the widget.
 *
 * This function should be called whenever keyboard navigation
 * within a single widget hits a boundary.
 *
 * The return value of this function should be interpreted
 * in a way similar to the return value of
 * [method@Gtk.Widget.child_focus]. When %TRUE is returned,
 * stay in the widget, the failed keyboard  navigation is OK
 * and/or there is nowhere we can/should move the focus to.
 * When %FALSE is returned, the caller should continue with
 * keyboard navigation outside the widget, e.g. by calling
 * [method@Gtk.Widget.child_focus] on the widget’s toplevel.
 *
 * The default [signal@Gtk.Widget::keynav-failed] handler returns
 * %FALSE for %GTK_DIR_TAB_FORWARD and %GTK_DIR_TAB_BACKWARD.
 * For the other values of `GtkDirectionType` it returns %TRUE.
 *
 * Whenever the default handler returns %TRUE, it also calls
 * [method@Gtk.Widget.error_bell] to notify the user of the
 * failed keyboard navigation.
 *
 * A use case for providing an own implementation of ::keynav-failed
 * (either by connecting to it or by overriding it) would be a row of
 * [class@Gtk.Entry] widgets where the user should be able to navigate
 * the entire row with the cursor keys, as e.g. known from user
 * interfaces that require entering license keys.
 *
 * Returns: %TRUE if stopping keyboard navigation is fine, %FALSE
 *   if the emitting widget should try to handle the keyboard
 *   navigation attempt in its parent container(s).
 */
gboolean
gtk_widget_keynav_failed (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_signal_emit (widget, widget_signals[KEYNAV_FAILED], 0,
                 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_error_bell:
 * @widget: a `GtkWidget`
 *
 * Notifies the user about an input-related error on this widget.
 *
 * If the [property@Gtk.Settings:gtk-error-bell] setting is %TRUE,
 * it calls [method@Gdk.Surface.beep], otherwise it does nothing.
 *
 * Note that the effect of [method@Gdk.Surface.beep] can be configured
 * in many ways, depending on the windowing backend and the desktop
 * environment or window manager that is used.
 */
void
gtk_widget_error_bell (GtkWidget *widget)
{
  GtkSettings* settings;
  gboolean beep;
  GdkSurface *surface;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  settings = gtk_widget_get_settings (widget);
  if (!settings)
    return;

  surface = gtk_widget_get_surface (widget);

  g_object_get (settings,
                "gtk-error-bell", &beep,
                NULL);

  if (beep && surface)
    gdk_surface_beep (surface);
}

static void
gtk_widget_set_usize_internal (GtkWidget *widget,
                               int        width,
                               int        height)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (widget));

  if (width > -2 && priv->width_request != width)
    {
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_WIDTH_REQUEST]);
      priv->width_request = width;
      changed = TRUE;
    }
  if (height > -2 && priv->height_request != height)
    {
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HEIGHT_REQUEST]);
      priv->height_request = height;
      changed = TRUE;
    }

  if (_gtk_widget_get_visible (widget) && changed)
    {
      gtk_widget_queue_resize (widget);
    }

  g_object_thaw_notify (G_OBJECT (widget));
}

/**
 * gtk_widget_set_size_request:
 * @widget: a `GtkWidget`
 * @width: width @widget should request, or -1 to unset
 * @height: height @widget should request, or -1 to unset
 *
 * Sets the minimum size of a widget.
 *
 * That is, the widget’s size request will be at least @width
 * by @height. You can use this function to force a widget to
 * be larger than it normally would be.
 *
 * In most cases, [method@Gtk.Window.set_default_size] is a better
 * choice for toplevel windows than this function; setting the default
 * size will still allow users to shrink the window. Setting the size
 * request will force them to leave the window at least as large as
 * the size request.
 *
 * Note the inherent danger of setting any fixed size - themes,
 * translations into other languages, different fonts, and user action
 * can all change the appropriate size for a given widget. So, it's
 * basically impossible to hardcode a size that will always be
 * correct.
 *
 * The size request of a widget is the smallest size a widget can
 * accept while still functioning well and drawing itself correctly.
 * However in some strange cases a widget may be allocated less than
 * its requested size, and in many cases a widget may be allocated more
 * space than it requested.
 *
 * If the size request in a given direction is -1 (unset), then
 * the “natural” size request of the widget will be used instead.
 *
 * The size request set here does not include any margin from the
 * properties
 * [property@Gtk.Widget:margin-start],
 * [property@Gtk.Widget:margin-end],
 * [property@Gtk.Widget:margin-top], and
 * [property@Gtk.Widget:margin-bottom], but it does include pretty
 * much all other padding or border properties set by any subclass
 * of `GtkWidget`.
 */
void
gtk_widget_set_size_request (GtkWidget *widget,
                             int        width,
                             int        height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  gtk_widget_set_usize_internal (widget, width, height);
}


/**
 * gtk_widget_get_size_request:
 * @widget: a `GtkWidget`
 * @width: (out) (optional): return location for width
 * @height: (out) (optional): return location for height
 *
 * Gets the size request that was explicitly set for the widget using
 * gtk_widget_set_size_request().
 *
 * A value of -1 stored in @width or @height indicates that that
 * dimension has not been set explicitly and the natural requisition
 * of the widget will be used instead. See
 * [method@Gtk.Widget.set_size_request]. To get the size a widget will
 * actually request, call [method@Gtk.Widget.measure] instead of
 * this function.
 */
void
gtk_widget_get_size_request (GtkWidget *widget,
                             int       *width,
                             int       *height)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (width)
    *width = priv->width_request;

  if (height)
    *height = priv->height_request;
}

/*< private >
 * gtk_widget_has_size_request:
 * @widget: a `GtkWidget`
 *
 * Returns if the widget has a size request set (anything besides -1 for height
 * or width)
 */
gboolean
gtk_widget_has_size_request (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return !(priv->width_request == -1 && priv->height_request == -1);
}

/**
 * gtk_widget_get_ancestor:
 * @widget: a `GtkWidget`
 * @widget_type: ancestor type
 *
 * Gets the first ancestor of @widget with type @widget_type.
 *
 * For example, `gtk_widget_get_ancestor (widget, GTK_TYPE_BOX)`
 * gets the first `GtkBox` that’s an ancestor of @widget. No
 * reference will be added to the returned widget; it should
 * not be unreferenced.
 *
 * Note that unlike [method@Gtk.Widget.is_ancestor], this function
 * considers @widget to be an ancestor of itself.
 *
 * Returns: (transfer none) (nullable): the ancestor widget
 */
GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
                         GType      widget_type)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget && !g_type_is_a (G_OBJECT_TYPE (widget), widget_type))
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      widget = priv->parent;
    }

  return widget;
}

/**
 * gtk_widget_get_settings:
 * @widget: a `GtkWidget`
 *
 * Gets the settings object holding the settings used for this widget.
 *
 * Note that this function can only be called when the `GtkWidget`
 * is attached to a toplevel, since the settings object is specific
 * to a particular `GdkDisplay`. If you want to monitor the widget for
 * changes in its settings, connect to the `notify::display` signal.
 *
 * Returns: (transfer none): the relevant `GtkSettings` object
 */
GtkSettings*
gtk_widget_get_settings (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_settings_get_for_display (_gtk_widget_get_display (widget));
}

/**
 * gtk_widget_is_ancestor:
 * @widget: a `GtkWidget`
 * @ancestor: another `GtkWidget`
 *
 * Determines whether @widget is somewhere inside @ancestor,
 * possibly with intermediate containers.
 *
 * Returns: %TRUE if @ancestor contains @widget as a child,
 *   grandchild, great grandchild, etc.
 */
gboolean
gtk_widget_is_ancestor (GtkWidget *widget,
                        GtkWidget *ancestor)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  while (widget)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (priv->parent == ancestor)
        return TRUE;

      widget = priv->parent;
    }

  return FALSE;
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  old_dir)
{
  GtkTextDirection direction;
  GtkStateFlags state;

  gtk_widget_update_default_pango_context (widget);

  direction = _gtk_widget_get_direction (widget);

  switch (direction)
    {
    case GTK_TEXT_DIR_LTR:
      state = GTK_STATE_FLAG_DIR_LTR;
      break;

    case GTK_TEXT_DIR_RTL:
      state = GTK_STATE_FLAG_DIR_RTL;
      break;

    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  gtk_widget_update_state_flags (widget,
                                 state,
                                 state ^ (GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL));

  g_signal_emit (widget, widget_signals[DIRECTION_CHANGED], 0, old_dir);
}

/**
 * gtk_widget_set_direction:
 * @widget: a `GtkWidget`
 * @dir: the new direction
 *
 * Sets the reading direction on a particular widget.
 *
 * This direction controls the primary direction for widgets
 * containing text, and also the direction in which the children
 * of a container are packed. The ability to set the direction is
 * present in order so that correct localization into languages with
 * right-to-left reading directions can be done. Generally, applications
 * will let the default reading direction present, except for containers
 * where the containers are arranged in an order that is explicitly
 * visual rather than logical (such as buttons for text justification).
 *
 * If the direction is set to %GTK_TEXT_DIR_NONE, then the value
 * set by [func@Gtk.Widget.set_default_direction] will be used.
 */
void
gtk_widget_set_direction (GtkWidget        *widget,
                          GtkTextDirection  dir)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkTextDirection old_dir;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (dir >= GTK_TEXT_DIR_NONE && dir <= GTK_TEXT_DIR_RTL);

  old_dir = _gtk_widget_get_direction (widget);

  priv->direction = dir;

  if (old_dir != _gtk_widget_get_direction (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
}

/**
 * gtk_widget_get_direction:
 * @widget: a `GtkWidget`
 *
 * Gets the reading direction for a particular widget.
 *
 * See [method@Gtk.Widget.set_direction].
 *
 * Returns: the reading direction for the widget.
 */
GtkTextDirection
gtk_widget_get_direction (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_TEXT_DIR_LTR);

  if (priv->direction == GTK_TEXT_DIR_NONE)
    return gtk_default_direction;
  else
    return priv->direction;
}

static void
gtk_widget_set_default_direction_recurse (GtkWidget        *widget,
                                          GtkTextDirection  old_dir)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  g_object_ref (widget);

  if (priv->direction == GTK_TEXT_DIR_NONE)
    gtk_widget_emit_direction_changed (widget, old_dir);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      gtk_widget_set_default_direction_recurse (child, old_dir);
    }

  g_object_unref (widget);
}

/**
 * gtk_widget_set_default_direction:
 * @dir: the new default direction. This cannot be %GTK_TEXT_DIR_NONE.
 *
 * Sets the default reading direction for widgets.
 *
 * See [method@Gtk.Widget.set_direction].
 */
void
gtk_widget_set_default_direction (GtkTextDirection dir)
{
  g_return_if_fail (dir == GTK_TEXT_DIR_RTL || dir == GTK_TEXT_DIR_LTR);

  if (dir != gtk_default_direction)
    {
      GList *toplevels, *tmp_list;
      GtkTextDirection old_dir = gtk_default_direction;

      gtk_default_direction = dir;

      tmp_list = toplevels = gtk_window_list_toplevels ();
      g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);

      while (tmp_list)
      {
        gtk_widget_set_default_direction_recurse (tmp_list->data, old_dir);
        g_object_unref (tmp_list->data);
        tmp_list = tmp_list->next;
      }

      g_list_free (toplevels);
    }
}

/**
 * gtk_widget_get_default_direction:
 *
 * Obtains the current default reading direction.
 *
 * See [func@Gtk.Widget.set_default_direction].
 *
 * Returns: the current default direction.
 */
GtkTextDirection
gtk_widget_get_default_direction (void)
{
  return gtk_default_direction;
}

static void
gtk_widget_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *sizegroups;

  if (priv->muxer != NULL)
    g_object_run_dispose (G_OBJECT (priv->muxer));

  if (priv->children_observer)
    gtk_list_list_model_clear (priv->children_observer);
  if (priv->controller_observer)
    gtk_list_list_model_clear (priv->controller_observer);

  if (priv->parent)
    {
      g_critical ("%s %p has a parent %s %p during dispose. Parents hold a reference, so this should not happen.\n"
                  "Did you call g_object_unref() instead of gtk_widget_unparent()?",
                  G_OBJECT_TYPE_NAME (widget), widget,
                  G_OBJECT_TYPE_NAME (priv->parent), priv->parent);
      priv->parent = NULL;
    }

  while (priv->paintables)
    gtk_widget_paintable_set_widget (priv->paintables->data, NULL);

  if (priv->layout_manager != NULL)
    gtk_layout_manager_set_widget (priv->layout_manager, NULL);
  g_clear_object (&priv->layout_manager);

  priv->visible = FALSE;
  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  g_clear_object (&priv->cursor);

  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (object, widget_signals[DESTROY], 0);
      priv->in_destruction = FALSE;
      gtk_widget_real_destroy (widget);
    }

  sizegroups = _gtk_widget_get_sizegroups (widget);
  while (sizegroups)
    {
      GtkSizeGroup *size_group;

      size_group = sizegroups->data;
      sizegroups = sizegroups->next;
      gtk_size_group_remove_widget (size_group, widget);
    }

  if (priv->at_context != NULL)
    {
      gtk_at_context_unrealize (priv->at_context);
      g_clear_object (&priv->at_context);
    }

  g_clear_object (&priv->muxer);

  G_OBJECT_CLASS (gtk_widget_parent_class)->dispose (object);
}

#ifdef G_ENABLE_CONSISTENCY_CHECKS
typedef struct {
  AutomaticChildClass *child_class;
  GType                widget_type;
  GObject             *object;
  gboolean             did_finalize;
} FinalizeAssertion;

static void
finalize_assertion_weak_ref (gpointer  data,
                             GObject  *where_the_object_was)
{
  FinalizeAssertion *assertion = (FinalizeAssertion *)data;
  assertion->did_finalize = TRUE;
}
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

static void
gtk_widget_real_destroy (GtkWidget *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  if (g_object_get_qdata (G_OBJECT (widget), quark_auto_children))
    {
      GtkWidgetClass *class;
      GSList *l;
      GHashTable *auto_children;

#ifdef G_ENABLE_CONSISTENCY_CHECKS
      GSList *assertions = NULL;

      /* Note, GTK_WIDGET_ASSERT_COMPONENTS is very useful
       * to catch ref counting bugs, but can only be used in
       * test cases which simply create and destroy a composite
       * widget.
       *
       * This is because some API can expose components explicitly,
       * and so we cannot assert that a component is expected to finalize
       * in a full application ecosystem.
       */
      if (g_getenv ("GTK_WIDGET_ASSERT_COMPONENTS") != NULL)
        {
          GType class_type;

          for (class = GTK_WIDGET_GET_CLASS (widget);
               GTK_IS_WIDGET_CLASS (class);
               class = g_type_class_peek_parent (class))
            {
              if (!class->priv->template)
                continue;

              class_type = G_OBJECT_CLASS_TYPE (class);

              for (l = class->priv->template->children; l; l = l->next)
                {
                  AutomaticChildClass *child_class = l->data;
                  GObject *child_object = gtk_widget_get_template_child (widget,
                                                                         class_type,
                                                                         child_class->name);
                  if (G_IS_OBJECT (child_object))
                    {
                      FinalizeAssertion *assertion = g_new0 (FinalizeAssertion, 1);
                      assertion->child_class = child_class;
                      assertion->widget_type = class_type;
                      assertion->object = child_object;

                      g_object_weak_ref (child_object, finalize_assertion_weak_ref, assertion);

                      assertions = g_slist_prepend (assertions, assertion);
                    }
                }
            }
        }
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

      /* Prepare to release references to all automated children */
      auto_children = g_object_steal_qdata (G_OBJECT (widget), quark_auto_children);

      /* Set any automatic private data pointers to NULL and release child references */
      for (class = GTK_WIDGET_GET_CLASS (widget);
           GTK_IS_WIDGET_CLASS (class);
           class = g_type_class_peek_parent (class))
        {
          GHashTable *auto_child_hash = NULL;

          if (!class->priv->template)
            continue;

          if (auto_children)
            {
              GType type = G_TYPE_FROM_CLASS (class);

              g_hash_table_steal_extended (auto_children,
                                           GSIZE_TO_POINTER (type),
                                           NULL,
                                           (gpointer *) &auto_child_hash);
            }

          for (l = class->priv->template->children; l; l = l->next)
            {
              AutomaticChildClass *child_class = l->data;

              if (child_class->offset != 0)
                {
                  gpointer field_p;

                  /* Nullify instance private data for internal children */
                  field_p = G_STRUCT_MEMBER_P (widget, child_class->offset);
                  (* (gpointer *) field_p) = NULL;
                }

              /* Release the references in order after setting the pointer to NULL */
              if (auto_child_hash)
                g_hash_table_remove (auto_child_hash, child_class->name);
            }

          g_clear_pointer (&auto_child_hash, g_hash_table_unref);
        }

      /* Free the child reference hash table */
      g_clear_pointer (&auto_children, g_hash_table_unref);

#ifdef G_ENABLE_CONSISTENCY_CHECKS
      for (l = assertions; l; l = l->next)
        {
          FinalizeAssertion *assertion = l->data;

          if (!assertion->did_finalize)
            g_critical ("Automated component '%s' of class '%s' did not finalize in dispose()"
            "Current reference count is %d",
            assertion->child_class->name,
            g_type_name (assertion->widget_type),
            assertion->object->ref_count);

          g_free (assertion);
        }
      g_slist_free (assertions);
#endif /* G_ENABLE_CONSISTENCY_CHECKS */
    }

  /* Callers of add_mnemonic_label() should disconnect on ::destroy */
  g_object_set_qdata (G_OBJECT (widget), quark_mnemonic_labels, NULL);

  gtk_grab_remove (widget);

  destroy_tick_callbacks (widget);
  destroy_surface_transform_data (widget);
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  gtk_grab_remove (widget);

  g_free (priv->name);
  g_free (priv->tooltip_markup);
  g_free (priv->tooltip_text);

  g_clear_pointer (&priv->transform, gsk_transform_unref);
  g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);

  gtk_css_widget_node_widget_destroyed (GTK_CSS_WIDGET_NODE (priv->cssnode));
  g_object_unref (priv->cssnode);

  g_clear_object (&priv->context);
  g_clear_object (&priv->at_context);

  _gtk_size_request_cache_free (&priv->requests);

  l = priv->event_controllers;
  while (l)
    {
      GList *next = l->next;
      GtkEventController *controller = l->data;

      if (controller)
        gtk_widget_remove_controller (widget, controller);

      l = next;
    }
  g_assert (priv->event_controllers == NULL);

  if (_gtk_widget_get_first_child (widget) != NULL)
    {
      GtkWidget *child;
      GString *s;

      s = g_string_new (NULL);
      g_string_append_printf (s, "Finalizing %s %p, but it still has children left:",
                              gtk_widget_get_name (widget), widget);
      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          g_string_append_printf (s, "\n   - %s %p",
                                  gtk_widget_get_name (child), child);
        }
      g_warning ("%s", s->str);
      g_string_free (s, TRUE);
    }

  if (g_object_is_floating (object))
    g_warning ("A floating object was finalized. This means that someone\n"
               "called g_object_unref() on an object that had only a floating\n"
               "reference; the initial floating reference is not owned by anyone\n"
               "and must be removed with g_object_ref_sink().");

  G_OBJECT_CLASS (gtk_widget_parent_class)->finalize (object);
}

static void
gtk_widget_real_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (_gtk_widget_get_realized (widget));

  if (!_gtk_widget_get_mapped (widget))
    {
      GtkWidget *p;
      priv->mapped = TRUE;

      for (p = gtk_widget_get_first_child (widget);
           p != NULL;
           p = gtk_widget_get_next_sibling (p))
        {
          if (_gtk_widget_get_visible (p) &&
              _gtk_widget_get_child_visible (p) &&
              !_gtk_widget_get_mapped (p))
            gtk_widget_map (p);
        }
    }
}

static void
gtk_widget_real_unmap (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (_gtk_widget_get_mapped (widget))
    {
      GtkWidget *child;
      priv->mapped = FALSE;

      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          gtk_widget_unmap (child);
        }

      gtk_widget_update_paintables (widget);

      gtk_widget_unset_state_flags (widget,
                                    GTK_STATE_FLAG_PRELIGHT |
                                    GTK_STATE_FLAG_ACTIVE);
    }
}

static void
gtk_widget_real_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->realized = TRUE;

  /* Connect frame clock */
  if (priv->tick_callbacks != NULL && !priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);

      priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                              G_CALLBACK (gtk_widget_on_frame_clock_update),
                                              widget);
      gdk_frame_clock_begin_updating (frame_clock);
    }

  gtk_css_node_invalidate_frame_clock (priv->cssnode, FALSE);
}

static void
gtk_widget_real_unrealize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (!priv->mapped);

   /* We must do unrealize child widget BEFORE container widget.
    * gdk_surface_destroy() destroys specified xwindow and its sub-xwindows.
    * So, unrealizing container widget before its children causes the problem
    * (for example, gdk_ic_destroy () with destroyed window causes crash.)
    */

  gtk_widget_forall (widget, (GtkCallback)gtk_widget_unrealize, NULL);

  /* Disconnect frame clock */
  gtk_css_node_invalidate_frame_clock (priv->cssnode, FALSE);

  if (priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);

      g_signal_handler_disconnect (frame_clock, priv->clock_tick_id);
      priv->clock_tick_id = 0;
      gdk_frame_clock_end_updating (frame_clock);
    }

  priv->realized = FALSE;
}

void
gtk_widget_adjust_size_request (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                int            *minimum_size,
                                int            *natural_size)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && priv->width_request > 0)
    *minimum_size = MAX (*minimum_size, priv->width_request);
  else if (orientation == GTK_ORIENTATION_VERTICAL && priv->height_request > 0)
    *minimum_size = MAX (*minimum_size, priv->height_request);

  /* Fix it if set_size_request made natural size smaller than min size.
   * This would also silently fix broken widgets, but we warn about them
   * in gtksizerequest.c when calling their size request vfuncs.
   */
  *natural_size = MAX (*natural_size, *minimum_size);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum_size += priv->margin.left + priv->margin.right;
      *natural_size += priv->margin.left + priv->margin.right;
    }
  else
    {
      *minimum_size += priv->margin.top + priv->margin.bottom;
      *natural_size += priv->margin.top + priv->margin.bottom;
    }
}

void
gtk_widget_adjust_baseline_request (GtkWidget *widget,
                                    int       *minimum_baseline,
                                    int       *natural_baseline)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->height_request >= 0)
    {
      /* No baseline support for explicitly set height */
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else
    {
      *minimum_baseline += priv->margin.top;
      *natural_baseline += priv->margin.top;
    }
}

/*
 * _gtk_widget_list_devices:
 * @widget: a `GtkWidget`
 *
 * Returns the list of pointer `GdkDevice`s that are currently
 * on top of @widget. Free the list
 * with g_free(), the elements are owned by GTK and must
 * not be freed.
 */
static GdkDevice **
_gtk_widget_list_devices (GtkWidget *widget,
                          guint     *out_n_devices)
{
  GtkRoot *root;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_assert (out_n_devices);

  if (!_gtk_widget_get_mapped (widget))
    {
      *out_n_devices = 0;
      return NULL;
    }

  root = gtk_widget_get_root (widget);
  if (!GTK_IS_WINDOW (root))
    {
      *out_n_devices = 0;
      return NULL;
    }

  return gtk_window_get_foci_on_widget (GTK_WINDOW (root),
                                        widget, out_n_devices);
}

/*
 * _gtk_widget_synthesize_crossing:
 * @from: the `GtkWidget` the virtual pointer is leaving.
 * @to: the `GtkWidget` the virtual pointer is moving to.
 * @mode: the `GdkCrossingMode` to place on the synthesized events.
 *
 * Generate crossing event(s) on widget state (sensitivity) or GTK grab change.
 */
void
_gtk_widget_synthesize_crossing (GtkWidget       *from,
                                 GtkWidget       *to,
                                 GdkDevice       *device,
                                 GdkCrossingMode  mode)
{
  GdkSurface *from_surface = NULL, *to_surface = NULL;
  GtkCrossingData crossing;
  double x, y;

  g_return_if_fail (from != NULL || to != NULL);

  crossing.type = GTK_CROSSING_POINTER;
  crossing.mode = mode;
  crossing.old_target = from;
  crossing.old_descendent = NULL;
  crossing.new_target = to;
  crossing.new_descendent = NULL;

  if (from)
    {
      crossing.direction = GTK_CROSSING_OUT;

      from_surface = gtk_widget_get_surface (from);
      gdk_surface_get_device_position (from_surface, device, &x, &y, NULL);
      gtk_widget_handle_crossing (from, &crossing, x, y);
    }

  if (to)
    {
      to_surface = gtk_widget_get_surface (to);

      crossing.direction = GTK_CROSSING_IN;
      gdk_surface_get_device_position (to_surface, device, &x, &y, NULL);
      gtk_widget_handle_crossing (to, &crossing, x, y);
    }
}

static void
gtk_widget_propagate_state (GtkWidget          *widget,
                            const GtkStateData *data)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStateFlags new_flags, old_flags = priv->state_flags;
  GtkStateData child_data;
  GtkWidget *child;
  int new_scale_factor = gtk_widget_get_scale_factor (widget);

  priv->state_flags |= data->flags_to_set;
  priv->state_flags &= ~(data->flags_to_unset);

  /* make insensitivity unoverridable */
  if (!priv->sensitive)
    priv->state_flags |= GTK_STATE_FLAG_INSENSITIVE;

  if (gtk_widget_is_focus (widget) && !gtk_widget_is_sensitive (widget))
    gtk_root_set_focus (priv->root, NULL);

  new_flags = priv->state_flags;

  if (data->old_scale_factor != new_scale_factor)
    _gtk_widget_scale_changed (widget);

  if (old_flags != new_flags)
    {
      GtkRoot *root;
      GtkWidget *grab = NULL;
      gboolean shadowed;

      g_object_ref (widget);

      root = gtk_widget_get_root (widget);

      if (GTK_IS_WINDOW (root))
        {
          GtkWindowGroup *window_group;

          window_group = gtk_window_get_group (GTK_WINDOW (root));
          grab = gtk_window_group_get_current_grab (window_group);
        }

      shadowed = grab && grab != widget && !gtk_widget_is_ancestor (widget, grab);

      if (!gtk_widget_is_sensitive (widget) && gtk_widget_has_grab (widget))
        gtk_grab_remove (widget);

      gtk_css_node_set_state (priv->cssnode, new_flags);

      g_signal_emit (widget, widget_signals[STATE_FLAGS_CHANGED], 0, old_flags);

      if (!shadowed &&
          (new_flags & GTK_STATE_FLAG_INSENSITIVE) != (old_flags & GTK_STATE_FLAG_INSENSITIVE))
        {
          guint i, n_devices;
          GdkDevice **devices;

          devices = _gtk_widget_list_devices (widget, &n_devices);

          for (i = 0; i < n_devices; i++)
            {
              GdkDevice *device;

              device = devices[i];

              if (!gtk_widget_is_sensitive (widget))
                _gtk_widget_synthesize_crossing (widget, NULL, device,
                                                 GDK_CROSSING_STATE_CHANGED);
              else
                _gtk_widget_synthesize_crossing (NULL, widget, device,
                                                 GDK_CROSSING_STATE_CHANGED);
            }

          g_free (devices);
        }

      if (!gtk_widget_is_sensitive (widget))
        gtk_widget_reset_controllers (widget);

      /* Make sure to only propagate the right states further */
      child_data.old_scale_factor = new_scale_factor;
      child_data.flags_to_set = data->flags_to_set & GTK_STATE_FLAGS_DO_SET_PROPAGATE;
      child_data.flags_to_unset = data->flags_to_unset & GTK_STATE_FLAGS_DO_UNSET_PROPAGATE;

      if (child_data.flags_to_set != 0 ||
          child_data.flags_to_unset != 0)
        {
          for (child = _gtk_widget_get_first_child (widget);
               child != NULL;
               child = _gtk_widget_get_next_sibling (child))
            {
              gtk_widget_propagate_state (child, &child_data);
            }
        }

      g_object_unref (widget);
    }
}

/**
 * gtk_requisition_new:
 *
 * Allocates a new `GtkRequisition`.
 *
 * The struct is initialized to zero.
 *
 * Returns: a new empty `GtkRequisition`. The newly
 *   allocated `GtkRequisition` should be freed with
 *   [method@Gtk.Requisition.free]
 */
GtkRequisition *
gtk_requisition_new (void)
{
  return g_new0 (GtkRequisition, 1);
}

/**
 * gtk_requisition_copy:
 * @requisition: a `GtkRequisition`
 *
 * Copies a `GtkRequisition`.
 *
 * Returns: a copy of @requisition
 */
GtkRequisition *
gtk_requisition_copy (const GtkRequisition *requisition)
{
  GtkRequisition *copy = g_new (GtkRequisition, 1);
  memcpy (copy, requisition, sizeof (GtkRequisition));
  return copy;
}

/**
 * gtk_requisition_free:
 * @requisition: a `GtkRequisition`
 *
 * Frees a `GtkRequisition`.
 */
void
gtk_requisition_free (GtkRequisition *requisition)
{
  g_free (requisition);
}

G_DEFINE_BOXED_TYPE (GtkRequisition, gtk_requisition,
                     gtk_requisition_copy,
                     gtk_requisition_free)

/*
 * Expand flag management
 */

static void
gtk_widget_update_computed_expand (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->need_compute_expand)
    {
      gboolean h, v;

      if (priv->hexpand_set)
        h = priv->hexpand;
      else
        h = FALSE;

      if (priv->vexpand_set)
        v = priv->vexpand;
      else
        v = FALSE;

      /* we don't need to use compute_expand if both expands are
       * forced by the app
       */
      if (!(priv->hexpand_set && priv->vexpand_set))
        {
          if (GTK_WIDGET_GET_CLASS (widget)->compute_expand != NULL)
            {
              gboolean ignored;

              GTK_WIDGET_GET_CLASS (widget)->compute_expand (widget,
                                                             priv->hexpand_set ? &ignored : &h,
                                                             priv->vexpand_set ? &ignored : &v);
            }
        }

      priv->need_compute_expand = FALSE;
      priv->computed_hexpand = h != FALSE;
      priv->computed_vexpand = v != FALSE;
    }
}

/**
 * gtk_widget_queue_compute_expand:
 * @widget: a `GtkWidget`
 *
 * Mark @widget as needing to recompute its expand flags.
 *
 * Call this function when setting legacy expand child
 * properties on the child of a container.
 *
 * See [method@Gtk.Widget.compute_expand].
 */
static void
gtk_widget_queue_compute_expand (GtkWidget *widget)
{
  GtkWidget *parent;
  gboolean changed_anything;

  if (widget->priv->need_compute_expand)
    return;

  changed_anything = FALSE;
  parent = widget;
  while (parent != NULL)
    {
      if (!parent->priv->need_compute_expand)
        {
          parent->priv->need_compute_expand = TRUE;
          changed_anything = TRUE;
        }

      /* Note: if we had an invariant that "if a child needs to
       * compute expand, its parents also do" then we could stop going
       * up when we got to a parent that already needed to
       * compute. However, in general we compute expand lazily (as
       * soon as we see something in a subtree that is expand, we know
       * we're expanding) and so this invariant does not hold and we
       * have to always walk all the way up in case some ancestor
       * is not currently need_compute_expand.
       */

      parent = parent->priv->parent;
    }

  /* recomputing expand always requires
   * a relayout as well
   */
  if (changed_anything)
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_widget_compute_expand:
 * @widget: the widget
 * @orientation: expand direction
 *
 * Computes whether a container should give this widget
 * extra space when possible.
 *
 * Containers should check this, rather than looking at
 * [method@Gtk.Widget.get_hexpand] or [method@Gtk.Widget.get_vexpand].
 *
 * This function already checks whether the widget is visible, so
 * visibility does not need to be checked separately. Non-visible
 * widgets are not expanded.
 *
 * The computed expand value uses either the expand setting explicitly
 * set on the widget itself, or, if none has been explicitly set,
 * the widget may expand if some of its children do.
 *
 * Returns: whether widget tree rooted here should be expanded
 */
gboolean
gtk_widget_compute_expand (GtkWidget      *widget,
                           GtkOrientation  orientation)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  /* We never make a widget expand if not even showing. */
  if (!_gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_update_computed_expand (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return priv->computed_hexpand;
  else
    return priv->computed_vexpand;
}

static void
gtk_widget_set_expand (GtkWidget     *widget,
                       GtkOrientation orientation,
                       gboolean       expand)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  int expand_prop;
  int expand_set_prop;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  expand = expand != FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->hexpand_set &&
          priv->hexpand == expand)
        return;

      priv->hexpand_set = TRUE;
      priv->hexpand = expand;

      expand_prop = PROP_HEXPAND;
      expand_set_prop = PROP_HEXPAND_SET;
    }
  else
    {
      if (priv->vexpand_set &&
          priv->vexpand == expand)
        return;

      priv->vexpand_set = TRUE;
      priv->vexpand = expand;

      expand_prop = PROP_VEXPAND;
      expand_set_prop = PROP_VEXPAND_SET;
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[expand_prop]);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[expand_set_prop]);
  g_object_thaw_notify (G_OBJECT (widget));
}

static void
gtk_widget_set_expand_set (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gboolean        set)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  int prop;

  set = set != FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (set == priv->hexpand_set)
        return;

      priv->hexpand_set = set;
      prop = PROP_HEXPAND_SET;
    }
  else
    {
      if (set == priv->vexpand_set)
        return;

      priv->vexpand_set = set;
      prop = PROP_VEXPAND_SET;
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[prop]);
}

/**
 * gtk_widget_get_hexpand: (attributes org.gtk.Method.get_property=hexpand)
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra horizontal
 * space.
 *
 * When a user resizes a `GtkWindow`, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Containers should use [method@Gtk.Widget.compute_expand] rather
 * than this function, to see whether a widget, or any of its children,
 * has the expand flag set. If any child of a widget wants to
 * expand, the parent may ask to expand also.
 *
 * This function only looks at the widget’s own hexpand flag, rather
 * than computing whether the entire widget tree rooted at this widget
 * wants to expand.
 *
 * Returns: whether hexpand flag is set
 */
gboolean
gtk_widget_get_hexpand (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->hexpand;
}

/**
 * gtk_widget_set_hexpand: (attributes org.gtk.Method.set_property=hexpand)
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra horizontal
 * space.
 *
 * When a user resizes a `GtkWindow`, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Call this function to set the expand flag if you would like your
 * widget to become larger horizontally when the window has extra
 * room.
 *
 * By default, widgets automatically expand if any of their children
 * want to expand. (To see if a widget will automatically expand given
 * its current children and state, call [method@Gtk.Widget.compute_expand].
 * A container can decide how the expandability of children affects the
 * expansion of the container by overriding the compute_expand virtual
 * method on `GtkWidget`.).
 *
 * Setting hexpand explicitly with this function will override the
 * automatic expand behavior.
 *
 * This function forces the widget to expand or not to expand,
 * regardless of children.  The override occurs because
 * [method@Gtk.Widget.set_hexpand] sets the hexpand-set property (see
 * [method@Gtk.Widget.set_hexpand_set]) which causes the widget’s hexpand
 * value to be used, rather than looking at children and widget state.
 */
void
gtk_widget_set_hexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_HORIZONTAL, expand);
}

/**
 * gtk_widget_get_hexpand_set: (attributes org.gtk.Method.get_property=hexpand-set)
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_hexpand() has been used
 * to explicitly set the expand flag on this widget.
 *
 * If [property@Gtk.Widget:hexpand] property is set, then it
 * overrides any computed expand value based on child widgets.
 * If `hexpand` is not set, then the expand value depends on
 * whether any children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it’s here
 * for completeness and consistency.
 *
 * Returns: whether hexpand has been explicitly set
 */
gboolean
gtk_widget_get_hexpand_set (GtkWidget      *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->hexpand_set;
}

/**
 * gtk_widget_set_hexpand_set: (attributes org.gtk.Method.set_property=hexpand-set)
 * @widget: the widget
 * @set: value for hexpand-set property
 *
 * Sets whether the hexpand flag will be used.
 *
 * The [property@Gtk.Widget:hexpand-set] property will be set
 * automatically when you call [method@Gtk.Widget.set_hexpand]
 * to set hexpand, so the most likely reason to use this function
 * would be to unset an explicit expand flag.
 *
 * If hexpand is set, then it overrides any computed
 * expand value based on child widgets. If hexpand is not
 * set, then the expand value depends on whether any
 * children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it’s here
 * for completeness and consistency.
 */
void
gtk_widget_set_hexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_HORIZONTAL, set);
}


/**
 * gtk_widget_get_vexpand: (attributes org.gtk.Method.get_property=vexpand)
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra vertical
 * space.
 *
 * See [method@Gtk.Widget.get_hexpand] for more detail.
 *
 * Returns: whether vexpand flag is set
 */
gboolean
gtk_widget_get_vexpand (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->vexpand;
}

/**
 * gtk_widget_set_vexpand: (attributes org.gtk.Method.set_property=vexpand)
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra vertical
 * space.
 *
 * See [method@Gtk.Widget.set_hexpand] for more detail.
 */
void
gtk_widget_set_vexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_VERTICAL, expand);
}

/**
 * gtk_widget_get_vexpand_set: (attributes org.gtk.Method.get_property=vexpand-set)
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_vexpand() has been used to
 * explicitly set the expand flag on this widget.
 *
 * See [method@Gtk.Widget.get_hexpand_set] for more detail.
 *
 * Returns: whether vexpand has been explicitly set
 */
gboolean
gtk_widget_get_vexpand_set (GtkWidget      *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->vexpand_set;
}

/**
 * gtk_widget_set_vexpand_set: (attributes org.gtk.Method.set_property=vexpand-set)
 * @widget: the widget
 * @set: value for vexpand-set property
 *
 * Sets whether the vexpand flag will be used.
 *
 * See [method@Gtk.Widget.set_hexpand_set] for more detail.
 */
void
gtk_widget_set_vexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_VERTICAL, set);
}

/*
 * GtkAccessible implementation
 */

static GtkATContext *
create_at_context (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);
  GtkWidgetClassPrivate *class_priv = GTK_WIDGET_GET_CLASS (self)->priv;
  GtkAccessibleRole role;

  if (priv->in_destruction)
    {
      GTK_DEBUG (A11Y, "ATContext for widget “%s” [%p] accessed during destruction",
                       G_OBJECT_TYPE_NAME (self),
                       self);
      return NULL;
    }

  /* Widgets have two options to set the accessible role: either they
   * define it in their class_init() function, and the role applies to
   * all instances; or an instance is created with the :accessible-role
   * property (from GtkAccessible) set to anything other than the initial
   * GTK_ACCESSIBLE_ROLE_WIDGET value.
   *
   * In either case, the accessible role cannot be set post-construction.
   */

  if (priv->accessible_role != GTK_ACCESSIBLE_ROLE_WIDGET)
    role = priv->accessible_role;
  else
    role = class_priv->accessible_role;

  priv->accessible_role = role;

  return gtk_at_context_create (role, GTK_ACCESSIBLE (self), gdk_display_get_default ());
}

static GtkATContext *
gtk_widget_accessible_get_at_context (GtkAccessible *accessible)
{
  GtkWidget *self = GTK_WIDGET (accessible);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);

  if (priv->in_destruction)
    {
      GTK_DEBUG (A11Y, "ATContext for widget “%s” [%p] accessed during destruction",
                       G_OBJECT_TYPE_NAME (self),
                       self);
      return NULL;
    }

  if (priv->at_context != NULL)
    return g_object_ref (priv->at_context);

  priv->at_context = create_at_context (self);
  if (priv->at_context != NULL)
    return g_object_ref (priv->at_context);

  return NULL;
}

static gboolean
gtk_widget_accessible_get_platform_state (GtkAccessible              *self,
                                          GtkAccessiblePlatformState  state)
{
  switch (state)
    {
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE:
      return gtk_widget_get_focusable (GTK_WIDGET (self));
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED:
      return gtk_widget_has_focus (GTK_WIDGET (self));
    case GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static GtkAccessible *
gtk_widget_accessible_get_accessible_parent (GtkAccessible *self)
{
  GtkWidget *parent = _gtk_widget_get_parent (GTK_WIDGET (self));

  if (parent == NULL)
    return NULL;

  return GTK_ACCESSIBLE (g_object_ref (parent));
}

static GtkAccessible *
gtk_widget_accessible_get_next_accessible_sibling (GtkAccessible *self)
{
  GtkWidget *sibling = _gtk_widget_get_next_sibling (GTK_WIDGET (self));

  if (sibling == NULL)
    return NULL;

  return GTK_ACCESSIBLE (g_object_ref (sibling));
}

static GtkAccessible *
gtk_widget_accessible_get_first_accessible_child (GtkAccessible *self)
{
  GtkWidget *child = _gtk_widget_get_first_child (GTK_WIDGET (self));

  if (child == NULL)
    return NULL;

  return GTK_ACCESSIBLE (g_object_ref (child));
}

static gboolean
gtk_widget_accessible_get_bounds (GtkAccessible *self,
                                  int           *x,
                                  int           *y,
                                  int           *width,
                                  int           *height)
{
  GtkWidget *widget;
  GtkWidget *parent;
  GtkWidget *bounds_relative_to;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT_ZERO;

  widget = GTK_WIDGET (self);
  if (!gtk_widget_get_realized (widget))
    return FALSE;

  parent = gtk_widget_get_parent (widget);
  if (parent != NULL)
    {
      graphene_point_t p;
      if (!gtk_widget_compute_point (widget, parent, &GRAPHENE_POINT_INIT (0, 0), &p))
        graphene_point_init (&p, 0, 0);
      *x = floorf (p.x);
      *y = floorf (p.y);
      bounds_relative_to = parent;
    }
  else
    {
      *x = *y = 0;
      bounds_relative_to = widget;
    }

  if (!gtk_widget_compute_bounds (widget, bounds_relative_to, &bounds))
    {
      *width = 0;
      *height = 0;
    }
  else
    {
      *width = ceilf (graphene_rect_get_width (&bounds));
      *height = ceilf (graphene_rect_get_height (&bounds));
    }

  return TRUE;
}

static void
gtk_widget_accessible_interface_init (GtkAccessibleInterface *iface)
{
  iface->get_at_context = gtk_widget_accessible_get_at_context;
  iface->get_platform_state = gtk_widget_accessible_get_platform_state;
  iface->get_accessible_parent = gtk_widget_accessible_get_accessible_parent;
  iface->get_first_accessible_child = gtk_widget_accessible_get_first_accessible_child;
  iface->get_next_accessible_sibling = gtk_widget_accessible_get_next_accessible_sibling;
  iface->get_bounds = gtk_widget_accessible_get_bounds;
}

static void
gtk_widget_buildable_add_child (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const char    *type)
{
  if (type != NULL)
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  if (GTK_IS_WIDGET (child))
    {
      gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (buildable));
    }
  else if (GTK_IS_EVENT_CONTROLLER (child))
    {
      gtk_widget_add_controller (GTK_WIDGET (buildable), g_object_ref (GTK_EVENT_CONTROLLER (child)));
    }
  else
    {
      g_warning ("Cannot add an object of type %s to a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)), g_type_name (G_OBJECT_TYPE (buildable)));
    }
}

static void
gtk_widget_buildable_interface_init (GtkBuildableIface *iface)
{
  quark_builder_set_id = g_quark_from_static_string ("gtk-builder-set-id");

  iface->set_id = gtk_widget_buildable_set_id;
  iface->get_id = gtk_widget_buildable_get_id;
  iface->get_internal_child = gtk_widget_buildable_get_internal_child;
  iface->custom_tag_start = gtk_widget_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_widget_buildable_custom_tag_end;
  iface->custom_finished = gtk_widget_buildable_custom_finished;
  iface->add_child = gtk_widget_buildable_add_child;
}

static void
gtk_widget_buildable_set_id (GtkBuildable *buildable,
                             const char   *id)
{
  g_object_set_qdata_full (G_OBJECT (buildable), quark_builder_set_id,
                           g_strdup (id), g_free);
}

static const char *
gtk_widget_buildable_get_id (GtkBuildable *buildable)
{
  return g_object_get_qdata (G_OBJECT (buildable), quark_builder_set_id);
}

static GObject *
gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         const char   *childname)
{
  GtkWidgetClass *class;
  GSList *l;
  GType internal_child_type = 0;

  /* Find a widget type which has declared an automated child as internal by
   * the name 'childname', if any.
   */
  for (class = GTK_WIDGET_GET_CLASS (buildable);
       GTK_IS_WIDGET_CLASS (class);
       class = g_type_class_peek_parent (class))
    {
      GtkWidgetTemplate *template = class->priv->template;

      if (!template)
        continue;

      for (l = template->children; l && internal_child_type == 0; l = l->next)
        {
          AutomaticChildClass *child_class = l->data;

          if (child_class->internal_child && strcmp (childname, child_class->name) == 0)
            internal_child_type = G_OBJECT_CLASS_TYPE (class);
        }
    }

  /* Now return the 'internal-child' from the class which declared it, note
   * that gtk_widget_get_template_child() an API used to access objects
   * which are in the private scope of a given class.
   */
  if (internal_child_type != 0)
    return gtk_widget_get_template_child (GTK_WIDGET (buildable), internal_child_type, childname);

  return NULL;
}


typedef struct
{
  GtkBuilder *builder;
  GSList *classes;
} StyleParserData;

static void
style_start_element (GtkBuildableParseContext  *context,
                     const char                *element_name,
                     const char               **names,
                     const char               **values,
                     gpointer                   user_data,
                     GError                   **error)
{
  StyleParserData *data = (StyleParserData *)user_data;

  if (strcmp (element_name, "class") == 0)
    {
      const char *name;

      if (!_gtk_builder_check_parent (data->builder, context, "style", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->classes = g_slist_prepend (data->classes, g_strdup (name));
    }
  else if (strcmp (element_name, "style") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static const GtkBuildableParser style_parser =
  {
    style_start_element,
  };

typedef struct
{
  char *name;
  GString *value;
  char *context;
  gboolean translatable;
} LayoutPropertyInfo;

typedef struct
{
  GObject *object;
  GtkBuilder *builder;

  LayoutPropertyInfo *cur_property;

  /* SList<LayoutPropertyInfo> */
  GSList *properties;
} LayoutParserData;

static void
layout_property_info_free (gpointer data)
{
  LayoutPropertyInfo *pinfo = data;

  if (pinfo == NULL)
    return;

  g_free (pinfo->name);
  g_free (pinfo->context);
  g_string_free (pinfo->value, TRUE);
  g_free (pinfo);
}

static void
layout_start_element (GtkBuildableParseContext  *context,
                      const char                *element_name,
                      const char               **names,
                      const char               **values,
                      gpointer                   user_data,
                      GError                   **error)
{
  LayoutParserData *layout_data = user_data;

  if (strcmp (element_name, "property") == 0)
    {
      const char *name = NULL;
      const char *ctx = NULL;
      gboolean translatable = FALSE;
      LayoutPropertyInfo *pinfo;

      if (!_gtk_builder_check_parent (layout_data->builder, context, "layout", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "context", &ctx,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (layout_data->builder, context, error);
          return;
        }

      pinfo = g_new0 (LayoutPropertyInfo, 1);
      pinfo->name = g_strdup (name);
      pinfo->translatable = translatable;
      pinfo->context = g_strdup (ctx);
      pinfo->value = g_string_new (NULL);

      layout_data->cur_property = pinfo;
    }
  else if (strcmp (element_name, "layout") == 0)
    {
      if (!_gtk_builder_check_parent (layout_data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (layout_data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (layout_data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static void
layout_text (GtkBuildableParseContext  *context,
             const char                *text,
             gsize                      text_len,
             gpointer                   user_data,
             GError                   **error)
{
  LayoutParserData *layout_data = user_data;

  if (layout_data->cur_property != NULL)
    g_string_append_len (layout_data->cur_property->value, text, text_len);
}

static void
layout_end_element (GtkBuildableParseContext  *context,
                    const char                *element_name,
                    gpointer                   user_data,
                    GError                   **error)
{
  LayoutParserData *layout_data = user_data;

  if (layout_data->cur_property != NULL)
    {
      LayoutPropertyInfo *pinfo = g_steal_pointer (&layout_data->cur_property);

      /* Translate the string, if needed */
      if (pinfo->value->len != 0 && pinfo->translatable)
        {
          const char *translated;
          const char *domain;

          domain = gtk_builder_get_translation_domain (layout_data->builder);

          translated = _gtk_builder_parser_translate (domain, pinfo->context, pinfo->value->str);

          g_string_assign (pinfo->value, translated);
        }

      /* We assign all properties at the end of the `layout` section */
      layout_data->properties = g_slist_prepend (layout_data->properties, pinfo);
    }
}

static const GtkBuildableParser layout_parser =
  {
    layout_start_element,
    layout_end_element,
    layout_text,
  };

typedef struct
{
  char *name;
  GString *value;
  char *context;
  gboolean translatable;
} AccessibilityAttributeInfo;

typedef struct
{
  GObject *object;
  GtkBuilder *builder;

  AccessibilityAttributeInfo *cur_attribute;

  /* SList<AccessibilityAttributeInfo> */
  GSList *properties;
  GSList *states;
  GSList *relations;
} AccessibilityParserData;

static void
accessibility_attribute_info_free (gpointer data)
{
  AccessibilityAttributeInfo *pinfo = data;

  if (pinfo == NULL)
    return;

  g_free (pinfo->name);
  g_free (pinfo->context);
  g_string_free (pinfo->value, TRUE);
  g_free (pinfo);
}

static void
accessibility_start_element (GtkBuildableParseContext  *context,
                             const char                *element_name,
                             const char               **names,
                             const char               **values,
                             gpointer                   user_data,
                             GError                   **error)
{
  AccessibilityParserData *accessibility_data = user_data;

  if (strcmp (element_name, "property") == 0 ||
      strcmp (element_name, "relation") == 0 ||
      strcmp (element_name, "state") == 0)
    {
      const char *name = NULL;
      const char *ctx = NULL;
      gboolean translatable = FALSE;
      AccessibilityAttributeInfo *pinfo;

      if (!_gtk_builder_check_parent (accessibility_data->builder,
                                      context,
                                      "accessibility",
                                      error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "context", &ctx,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (accessibility_data->builder, context, error);
          return;
        }

      pinfo = g_new0 (AccessibilityAttributeInfo, 1);
      pinfo->name = g_strdup (name);
      pinfo->translatable = translatable;
      pinfo->context = g_strdup (ctx);
      pinfo->value = g_string_new (NULL);

      accessibility_data->cur_attribute = pinfo;
    }
  else if (strcmp (element_name, "accessibility") == 0)
    {
      if (!_gtk_builder_check_parent (accessibility_data->builder,
                                      context,
                                      "object",
                                      error))
        return;
    }
  else
    {
      _gtk_builder_error_unhandled_tag (accessibility_data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static void
accessibility_text (GtkBuildableParseContext  *context,
                    const char                *text,
                    gsize                      text_len,
                    gpointer                   user_data,
                    GError                   **error)
{
  AccessibilityParserData *accessibility_data = user_data;

  if (accessibility_data->cur_attribute != NULL)
    g_string_append_len (accessibility_data->cur_attribute->value, text, text_len);
}

static void
accessibility_end_element (GtkBuildableParseContext  *context,
                           const char                *element_name,
                           gpointer                   user_data,
                           GError                   **error)
{
  AccessibilityParserData *accessibility_data = user_data;

  if (accessibility_data->cur_attribute != NULL)
    {
      AccessibilityAttributeInfo *pinfo = g_steal_pointer (&accessibility_data->cur_attribute);

      /* Translate the string, if needed */
      if (pinfo->value->len != 0 && pinfo->translatable)
        {
          const char *translated;
          const char *domain;

          domain = gtk_builder_get_translation_domain (accessibility_data->builder);

          translated = _gtk_builder_parser_translate (domain, pinfo->context, pinfo->value->str);

          g_string_assign (pinfo->value, translated);
        }

      /* We assign all properties at the end of the `accessibility` section */
      if (strcmp (element_name, "property") == 0)
        accessibility_data->properties = g_slist_prepend (accessibility_data->properties, pinfo);
      else if (strcmp (element_name, "relation") == 0)
        accessibility_data->relations = g_slist_prepend (accessibility_data->relations, pinfo);
      else if (strcmp (element_name, "state") == 0)
        accessibility_data->states = g_slist_prepend (accessibility_data->states, pinfo);
      else
        {
          _gtk_builder_error_unhandled_tag (accessibility_data->builder, context,
                                            "GtkWidget", element_name,
                                            error);
          accessibility_attribute_info_free (pinfo);
        }
    }
}

static const GtkBuildableParser accessibility_parser = {
  accessibility_start_element,
  accessibility_end_element,
  accessibility_text,
};

static gboolean
gtk_widget_buildable_custom_tag_start (GtkBuildable       *buildable,
                                       GtkBuilder         *builder,
                                       GObject            *child,
                                       const char         *tagname,
                                       GtkBuildableParser *parser,
                                       gpointer           *parser_data)
{
  if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *data;

      data = g_new0 (StyleParserData, 1);
      data->builder = builder;

      *parser = style_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "layout") == 0)
    {
      LayoutParserData *data;

      data = g_new0 (LayoutParserData, 1);
      data->builder = builder;
      data->object = (GObject *) g_object_ref (buildable);

      *parser = layout_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilityParserData *data;

      data = g_new0 (AccessibilityParserData, 1);
      data->builder = builder;
      data->object = (GObject *) g_object_ref (buildable);

      *parser = accessibility_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_widget_buildable_custom_tag_end (GtkBuildable  *buildable,
                                     GtkBuilder    *builder,
                                     GObject       *child,
                                     const char    *tagname,
                                     gpointer       data)
{
}

static void
gtk_widget_buildable_finish_layout_properties (GtkWidget *widget,
                                               GtkWidget *parent,
                                               gpointer   data)
{
  LayoutParserData *layout_data = data;
  GtkLayoutManager *layout_manager;
  GtkLayoutChild *layout_child;
  GObject *gobject;
  GObjectClass *gobject_class;
  GSList *layout_properties, *l;

  layout_manager = gtk_widget_get_layout_manager (parent);
  if (layout_manager == NULL)
    return;

  layout_child = gtk_layout_manager_get_layout_child (layout_manager, widget);
  if (layout_child == NULL)
    return;

  gobject = G_OBJECT (layout_child);
  gobject_class = G_OBJECT_GET_CLASS (layout_child);

  layout_properties = g_slist_reverse (layout_data->properties);
  layout_data->properties = NULL;

  for (l = layout_properties; l != NULL; l = l->next)
    {
      LayoutPropertyInfo *pinfo = l->data;
      GParamSpec *pspec;
      GValue value = G_VALUE_INIT;
      GError *error = NULL;

      pspec = g_object_class_find_property (gobject_class, pinfo->name);
      if (pspec == NULL)
        {
          g_warning ("Unable to find layout property “%s” for children "
                     "of layout managers of type “%s”",
                     pinfo->name,
                     G_OBJECT_TYPE_NAME (layout_manager));
          continue;
        }

      gtk_builder_value_from_string (layout_data->builder,
                                     pspec,
                                     pinfo->value->str,
                                     &value,
                                     &error);
      if (error != NULL)
        {
          g_warning ("Failed to set property “%s.%s” to “%s”: %s",
                     G_OBJECT_TYPE_NAME (layout_child),
                     pinfo->name,
                     pinfo->value->str,
                     error->message);
          g_error_free (error);
          continue;
        }

      g_object_set_property (gobject, pinfo->name, &value);
      g_value_unset (&value);
    }

  g_slist_free_full (layout_properties, layout_property_info_free);
}

static void
gtk_widget_buildable_finish_accessibility_properties (GtkWidget *widget,
                                                      gpointer   data)
{
  AccessibilityParserData *accessibility_data = data;
  GSList *attributes, *l;
  GtkATContext *context;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget));
  if (context == NULL)
    return;

  attributes = g_slist_reverse (accessibility_data->properties);
  accessibility_data->properties = NULL;

  for (l = attributes; l != NULL; l = l->next)
    {
      AccessibilityAttributeInfo *pinfo = l->data;
      int property;
      GError *error = NULL;
      GtkAccessibleValue *value;

      _gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_PROPERTY,
                                     pinfo->name,
                                     &property,
                                     &error);
      if (error != NULL)
        {
          g_warning ("Failed to find accessible property “%s”: %s",
                     pinfo->name,
                     error->message);
          g_error_free (error);
          continue;
        }

      value = gtk_accessible_value_parse_for_property (property,
                                                       pinfo->value->str,
                                                       pinfo->value->len,
                                                       &error);
      if (error != NULL)
        {
          g_warning ("Failed to set accessible property “%s” to “%s”: %s",
                     pinfo->name,
                     pinfo->value->str,
                     error->message);
          g_error_free (error);
          continue;
        }

      gtk_at_context_set_accessible_property (context, property, value);
      gtk_accessible_value_unref (value);
    }

  g_slist_free_full (attributes, accessibility_attribute_info_free);

  attributes = g_slist_reverse (accessibility_data->relations);
  accessibility_data->relations = NULL;

  for (l = attributes; l != NULL; l = l->next)
    {
      AccessibilityAttributeInfo *pinfo = l->data;
      int relation;
      GError *error = NULL;
      GtkAccessibleValue *value;

      _gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_RELATION,
                                     pinfo->name,
                                     &relation,
                                     &error);
      if (error != NULL)
        {
          g_warning ("Failed to find accessible relation “%s”: %s",
                     pinfo->name,
                     error->message);
          g_error_free (error);
          continue;
        }

      value = gtk_accessible_value_parse_for_relation (relation,
                                                       pinfo->value->str,
                                                       pinfo->value->len,
                                                       &error);
      if (error != NULL)
        {
          g_warning ("Failed to set accessible relation “%s” to “%s”: %s",
                     pinfo->name,
                     pinfo->value->str,
                     error->message);
          g_error_free (error);
          continue;
        }

      if (value == NULL)
        {
          GObject *obj = gtk_builder_get_object (accessibility_data->builder,
                                                 pinfo->value->str);

          if (obj == NULL)
            {
              g_warning ("Failed to find accessible object “%s” for relation “%s”",
                         pinfo->value->str,
                         pinfo->name);
              continue;
            }

          /* FIXME: Need to distinguish between refs and refslist types */
          value = gtk_reference_list_accessible_value_new (g_list_append (NULL, obj));
        }

      gtk_at_context_set_accessible_relation (context, relation, value);
      gtk_accessible_value_unref (value);
    }

  g_slist_free_full (attributes, accessibility_attribute_info_free);

  attributes = g_slist_reverse (accessibility_data->states);
  accessibility_data->states = NULL;

  for (l = attributes; l != NULL; l = l->next)
    {
      AccessibilityAttributeInfo *pinfo = l->data;
      int state;
      GError *error = NULL;
      GtkAccessibleValue *value;

      _gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_STATE,
                                     pinfo->name,
                                     &state,
                                     &error);
      if (error != NULL)
        {
          g_warning ("Failed to find accessible state “%s”: %s",
                     pinfo->name,
                     error->message);
          g_error_free (error);
          continue;
        }

      value = gtk_accessible_value_parse_for_state (state,
                                                    pinfo->value->str,
                                                    pinfo->value->len,
                                                    &error);
      if (error != NULL)
        {
          g_warning ("Failed to set accessible state “%s” to “%s”: %s",
                     pinfo->name,
                     pinfo->value->str,
                     error->message);
          g_error_free (error);
          continue;
        }

      gtk_at_context_set_accessible_state (context, state, value);
      gtk_accessible_value_unref (value);
    }

  g_slist_free_full (attributes, accessibility_attribute_info_free);

  g_object_unref (context);
}

static void
gtk_widget_buildable_custom_finished (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      GObject      *child,
                                      const char   *tagname,
                                      gpointer      user_data)
{
  if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *style_data = (StyleParserData *)user_data;
      GSList *l;

      for (l = style_data->classes; l; l = l->next)
        gtk_widget_add_css_class (GTK_WIDGET (buildable), (const char *)l->data);

      g_slist_free_full (style_data->classes, g_free);
      g_free (style_data);
    }
  else if (strcmp (tagname, "layout") == 0)
    {
      LayoutParserData *layout_data = (LayoutParserData *) user_data;
      GtkWidget *parent = _gtk_widget_get_parent (GTK_WIDGET (buildable));

      if (parent != NULL)
        gtk_widget_buildable_finish_layout_properties (GTK_WIDGET (buildable),
                                                       parent,
                                                       layout_data);

      /* Free the unapplied properties, if any */
      g_slist_free_full (layout_data->properties, layout_property_info_free);
      g_object_unref (layout_data->object);
      g_free (layout_data);
    }
  else if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilityParserData *accessibility_data = user_data;

      gtk_widget_buildable_finish_accessibility_properties (GTK_WIDGET (buildable),
                                                            accessibility_data);

      g_slist_free_full (accessibility_data->properties,
                         accessibility_attribute_info_free);
      g_slist_free_full (accessibility_data->relations,
                         accessibility_attribute_info_free);
      g_slist_free_full (accessibility_data->states,
                         accessibility_attribute_info_free);
      g_object_unref (accessibility_data->object);
      g_free (accessibility_data);
    }
}

/**
 * gtk_widget_get_halign: (attributes org.gtk.Method.get_property=halign)
 * @widget: a `GtkWidget`
 *
 * Gets the horizontal alignment of @widget.
 *
 * For backwards compatibility reasons this method will never return
 * one of the baseline alignments, but instead it will convert it to
 * `GTK_ALIGN_FILL` or `GTK_ALIGN_CENTER`.
 *
 * Baselines are not supported for horizontal alignment.
 *
 * Returns: the horizontal alignment of @widget
 */
GtkAlign
gtk_widget_get_halign (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);

  if (priv->halign == GTK_ALIGN_BASELINE_FILL)
    return GTK_ALIGN_FILL;
  else if (priv->halign == GTK_ALIGN_BASELINE_CENTER)
    return GTK_ALIGN_CENTER;
  return priv->halign;
}

/**
 * gtk_widget_set_halign: (attributes org.gtk.Method.set_property=halign)
 * @widget: a `GtkWidget`
 * @align: the horizontal alignment
 *
 * Sets the horizontal alignment of @widget.
 */
void
gtk_widget_set_halign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->halign == align)
    return;

  priv->halign = align;
  gtk_widget_queue_allocate (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HALIGN]);
}

/**
 * gtk_widget_get_valign: (attributes org.gtk.Method.get_property=valign)
 * @widget: a `GtkWidget`
 *
 * Gets the vertical alignment of @widget.
 *
 * Returns: the vertical alignment of @widget
 */
GtkAlign
gtk_widget_get_valign (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);

  return priv->valign;
}

/**
 * gtk_widget_set_valign: (attributes org.gtk.Method.set_property=valign)
 * @widget: a `GtkWidget`
 * @align: the vertical alignment
 *
 * Sets the vertical alignment of @widget.
 */
void
gtk_widget_set_valign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->valign == align)
    return;

  priv->valign = align;
  gtk_widget_queue_allocate (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VALIGN]);
}

/**
 * gtk_widget_get_margin_start: (attributes org.gtk.Method.get_property=margin-start)
 * @widget: a `GtkWidget`
 *
 * Gets the start margin of @widget.
 *
 * Returns: The start margin of @widget
 */
int
gtk_widget_get_margin_start (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.left;
}

/**
 * gtk_widget_set_margin_start: (attributes org.gtk.Method.set_property=margin-start)
 * @widget: a `GtkWidget`
 * @margin: the start margin
 *
 * Sets the start margin of @widget.
 */
void
gtk_widget_set_margin_start (GtkWidget *widget,
                             int        margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  /* We always save margin-start as .left */

  if (priv->margin.left == margin)
    return;

  priv->margin.left = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_START]);
}

/**
 * gtk_widget_get_margin_end: (attributes org.gtk.Method.get_property=margin-end)
 * @widget: a `GtkWidget`
 *
 * Gets the end margin of @widget.
 *
 * Returns: The end margin of @widget
 */
int
gtk_widget_get_margin_end (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.right;
}

/**
 * gtk_widget_set_margin_end: (attributes org.gtk.Method.set_property=margin-end)
 * @widget: a `GtkWidget`
 * @margin: the end margin
 *
 * Sets the end margin of @widget.
 */
void
gtk_widget_set_margin_end (GtkWidget *widget,
                           int        margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  /* We always set margin-end as .right */

  if (priv->margin.right == margin)
    return;

  priv->margin.right = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_END]);
}

/**
 * gtk_widget_get_margin_top: (attributes org.gtk.Method.get_property=margin-top)
 * @widget: a `GtkWidget`
 *
 * Gets the top margin of @widget.
 *
 * Returns: The top margin of @widget
 */
int
gtk_widget_get_margin_top (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.top;
}

/**
 * gtk_widget_set_margin_top: (attributes org.gtk.Method.set_property=margin-top)
 * @widget: a `GtkWidget`
 * @margin: the top margin
 *
 * Sets the top margin of @widget.
 */
void
gtk_widget_set_margin_top (GtkWidget *widget,
                           int        margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  if (priv->margin.top == margin)
    return;

  priv->margin.top = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_TOP]);
}

/**
 * gtk_widget_get_margin_bottom: (attributes org.gtk.Method.get_property=margin-bottom)
 * @widget: a `GtkWidget`
 *
 * Gets the bottom margin of @widget.
 *
 * Returns: The bottom margin of @widget
 */
int
gtk_widget_get_margin_bottom (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.bottom;
}

/**
 * gtk_widget_set_margin_bottom: (attributes org.gtk.Method.set_property=margin-bottom)
 * @widget: a `GtkWidget`
 * @margin: the bottom margin
 *
 * Sets the bottom margin of @widget.
 */
void
gtk_widget_set_margin_bottom (GtkWidget *widget,
                              int        margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  if (priv->margin.bottom == margin)
    return;

  priv->margin.bottom = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_BOTTOM]);
}

/**
 * gtk_widget_get_clipboard:
 * @widget: a `GtkWidget`
 *
 * Gets the clipboard object for @widget.
 *
 * This is a utility function to get the clipboard object for the
 * `GdkDisplay` that @widget is using.
 *
 * Note that this function always works, even when @widget is not
 * realized yet.
 *
 * Returns: (transfer none): the appropriate clipboard object
 */
GdkClipboard *
gtk_widget_get_clipboard (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_display_get_clipboard (_gtk_widget_get_display (widget));
}

/**
 * gtk_widget_get_primary_clipboard:
 * @widget: a `GtkWidget`
 *
 * Gets the primary clipboard of @widget.
 *
 * This is a utility function to get the primary clipboard object
 * for the `GdkDisplay` that @widget is using.
 *
 * Note that this function always works, even when @widget is not
 * realized yet.
 *
 * Returns: (transfer none): the appropriate clipboard object
 **/
GdkClipboard *
gtk_widget_get_primary_clipboard (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_display_get_primary_clipboard (_gtk_widget_get_display (widget));
}

/**
 * gtk_widget_list_mnemonic_labels:
 * @widget: a `GtkWidget`
 *
 * Returns the widgets for which this widget is the target of a
 * mnemonic.
 *
 * Typically, these widgets will be labels. See, for example,
 * [method@Gtk.Label.set_mnemonic_widget].

 * The widgets in the list are not individually referenced.
 * If you want to iterate through the list and perform actions
 * involving callbacks that might destroy the widgets, you
 * must call `g_list_foreach (result, (GFunc)g_object_ref, NULL)`
 * first, and then unref all the widgets afterwards.

 * Returns: (element-type GtkWidget) (transfer container): the list
 *   of mnemonic labels; free this list with g_list_free() when you
 *   are done with it.
 */
GList *
gtk_widget_list_mnemonic_labels (GtkWidget *widget)
{
  GList *list = NULL;
  GSList *l;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (l = g_object_get_qdata (G_OBJECT (widget), quark_mnemonic_labels); l; l = l->next)
    list = g_list_prepend (list, l->data);

  return list;
}

/**
 * gtk_widget_add_mnemonic_label:
 * @widget: a `GtkWidget`
 * @label: a `GtkWidget` that acts as a mnemonic label for @widget
 *
 * Adds a widget to the list of mnemonic labels for this widget.
 *
 * See [method@Gtk.Widget.list_mnemonic_labels]. Note the
 * list of mnemonic labels for the widget is cleared when the
 * widget is destroyed, so the caller must make sure to update
 * its internal state at this point as well.
 */
void
gtk_widget_add_mnemonic_label (GtkWidget *widget,
                               GtkWidget *label)
{
  GSList *old_list, *new_list;
  GtkAccessibleRelation relation = GTK_ACCESSIBLE_RELATION_LABELLED_BY;
  GValue value = G_VALUE_INIT;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_prepend (old_list, label);

  g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
                           new_list, (GDestroyNotify) g_slist_free);

  /* The ATContext takes ownership of the GList returned by list_mnemonic_labels(),
   * so we don't need to free it
   */
  gtk_accessible_relation_init_value (relation, &value);
  g_value_set_pointer (&value, gtk_widget_list_mnemonic_labels (widget));
  gtk_accessible_update_relation_value (GTK_ACCESSIBLE (widget), 1, &relation, &value);
  g_value_unset (&value);
}

/**
 * gtk_widget_remove_mnemonic_label:
 * @widget: a `GtkWidget`
 * @label: a `GtkWidget` that was previously set as a mnemonic
 *   label for @widget with [method@Gtk.Widget.add_mnemonic_label]
 *
 * Removes a widget from the list of mnemonic labels for this widget.
 *
 * See [method@Gtk.Widget.list_mnemonic_labels]. The widget must
 * have previously been added to the list with
 * [method@Gtk.Widget.add_mnemonic_label].
 */
void
gtk_widget_remove_mnemonic_label (GtkWidget *widget,
                                  GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_remove (old_list, label);

  if (new_list)
    g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
                             new_list, (GDestroyNotify) g_slist_free);

  if (new_list != NULL && new_list->data != NULL)
    {
      GtkAccessibleRelation relation = GTK_ACCESSIBLE_RELATION_LABELLED_BY;
      GValue value = G_VALUE_INIT;

      /* The ATContext takes ownership of the GList returned by list_mnemonic_labels(),
       * so we don't need to free it
       */
      gtk_accessible_relation_init_value (relation, &value);
      g_value_set_pointer (&value, gtk_widget_list_mnemonic_labels (widget));
      gtk_accessible_update_relation_value (GTK_ACCESSIBLE (widget), 1, &relation, &value);
      g_value_unset (&value);
    }
  else
    {
      gtk_accessible_reset_relation (GTK_ACCESSIBLE (widget),
                                     GTK_ACCESSIBLE_RELATION_LABELLED_BY);
    }
}

/**
 * gtk_widget_trigger_tooltip_query:
 * @widget: a `GtkWidget`
 *
 * Triggers a tooltip query on the display where the toplevel
 * of @widget is located.
 */
void
gtk_widget_trigger_tooltip_query (GtkWidget *widget)
{
  gtk_tooltip_trigger_tooltip_query (widget);
}

/**
 * gtk_widget_set_tooltip_text: (attributes org.gtk.Method.set_property=tooltip-text)
 * @widget: a `GtkWidget`
 * @text: (nullable): the contents of the tooltip for @widget
 *
 * Sets @text as the contents of the tooltip.
 *
 * If @text contains any markup, it will be escaped.
 *
 * This function will take care of setting
 * [property@Gtk.Widget:has-tooltip] as a side effect,
 * and of the default handler for the
 * [signal@Gtk.Widget::query-tooltip] signal.
 *
 * See also [method@Gtk.Tooltip.set_text].
 */
void
gtk_widget_set_tooltip_text (GtkWidget  *widget,
                             const char *text)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GObject *object = G_OBJECT (widget);
  char *tooltip_text, *tooltip_markup;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_freeze_notify (object);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (text != NULL && *text == '\0')
    {
      tooltip_text = NULL;
      tooltip_markup = NULL;
    }
  else
    {
      tooltip_text = g_strdup (text);
      tooltip_markup = text != NULL ? g_markup_escape_text (text, -1) : NULL;
    }

  g_clear_pointer (&priv->tooltip_markup, g_free);
  g_clear_pointer (&priv->tooltip_text, g_free);

  priv->tooltip_text = tooltip_text;
  priv->tooltip_markup = tooltip_markup;

  gtk_widget_set_has_tooltip (widget, priv->tooltip_text != NULL);
  if (_gtk_widget_get_visible (widget))
    gtk_widget_trigger_tooltip_query (widget);

  g_object_notify_by_pspec (object, widget_props[PROP_TOOLTIP_TEXT]);
  g_object_notify_by_pspec (object, widget_props[PROP_TOOLTIP_MARKUP]);
  g_object_notify_by_pspec (object, widget_props[PROP_HAS_TOOLTIP]);

  g_object_thaw_notify (object);
}

/**
 * gtk_widget_get_tooltip_text: (attributes org.gtk.Method.get_property=tooltip-text)
 * @widget: a `GtkWidget`
 *
 * Gets the contents of the tooltip for @widget.
 *
 * If the @widget's tooltip was set using
 * [method@Gtk.Widget.set_tooltip_markup],
 * this function will return the escaped text.
 *
 * Returns: (nullable): the tooltip text
 */
const char *
gtk_widget_get_tooltip_text (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->tooltip_text;
}

/**
 * gtk_widget_set_tooltip_markup: (attributes org.gtk.Method.set_property=tooltip-markup)
 * @widget: a `GtkWidget`
 * @markup: (nullable): the contents of the tooltip for @widget
 *
 * Sets @markup as the contents of the tooltip, which is marked
 * up with Pango markup.
 *
 * This function will take care of setting the
 * [property@Gtk.Widget:has-tooltip] as a side effect, and of the
 * default handler for the [signal@Gtk.Widget::query-tooltip] signal.
 *
 * See also [method@Gtk.Tooltip.set_markup].
 */
void
gtk_widget_set_tooltip_markup (GtkWidget  *widget,
                               const char *markup)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GObject *object = G_OBJECT (widget);
  char *tooltip_markup;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_freeze_notify (object);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (markup != NULL && *markup == '\0')
    tooltip_markup = NULL;
  else
    tooltip_markup = g_strdup (markup);

  g_clear_pointer (&priv->tooltip_text, g_free);
  g_clear_pointer (&priv->tooltip_markup, g_free);

  priv->tooltip_markup = tooltip_markup;

  /* Store the tooltip without markup, as we might end up using
   * it for widget descriptions in the accessibility layer
   */
  if (priv->tooltip_markup != NULL)
    {
      pango_parse_markup (priv->tooltip_markup, -1, 0, NULL,
                          &priv->tooltip_text,
                          NULL,
                          NULL);
    }

  gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, priv->tooltip_text,
                                  -1);

  gtk_widget_set_has_tooltip (widget, tooltip_markup != NULL);
  if (_gtk_widget_get_visible (widget))
    gtk_widget_trigger_tooltip_query (widget);

  g_object_notify_by_pspec (object, widget_props[PROP_TOOLTIP_TEXT]);
  g_object_notify_by_pspec (object, widget_props[PROP_TOOLTIP_MARKUP]);
  g_object_notify_by_pspec (object, widget_props[PROP_HAS_TOOLTIP]);

  g_object_thaw_notify (object);
}

/**
 * gtk_widget_get_tooltip_markup: (attributes org.gtk.Method.get_property=tooltip-markup)
 * @widget: a `GtkWidget`
 *
 * Gets the contents of the tooltip for @widget.
 *
 * If the tooltip has not been set using
 * [method@Gtk.Widget.set_tooltip_markup], this
 * function returns %NULL.
 *
 * Returns: (nullable): the tooltip text
 */
const char *
gtk_widget_get_tooltip_markup (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->tooltip_markup;
}

/**
 * gtk_widget_set_has_tooltip: (attributes org.gtk.Method.set_property=has-tooltip)
 * @widget: a `GtkWidget`
 * @has_tooltip: whether or not @widget has a tooltip.
 *
 * Sets the `has-tooltip` property on @widget to @has_tooltip.
 */
void
gtk_widget_set_has_tooltip (GtkWidget *widget,
                            gboolean   has_tooltip)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  has_tooltip = !!has_tooltip;

  if (priv->has_tooltip != has_tooltip)
    {
      priv->has_tooltip = has_tooltip;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HAS_TOOLTIP]);
    }
}

/**
 * gtk_widget_get_has_tooltip: (attributes org.gtk.Method.get_property=has-tooltip)
 * @widget: a `GtkWidget`
 *
 * Returns the current value of the `has-tooltip` property.
 *
 * Returns: current value of `has-tooltip` on @widget.
 */
gboolean
gtk_widget_get_has_tooltip (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_tooltip;
}

/**
 * gtk_widget_get_allocation:
 * @widget: a `GtkWidget`
 * @allocation: (out): a pointer to a `GtkAllocation` to copy to
 *
 * Retrieves the widget’s allocation.
 *
 * Note, when implementing a layout container: a widget’s allocation
 * will be its “adjusted” allocation, that is, the widget’s parent
 * typically calls [method@Gtk.Widget.size_allocate] with an allocation,
 * and that allocation is then adjusted (to handle margin
 * and alignment for example) before assignment to the widget.
 * [method@Gtk.Widget.get_allocation] returns the adjusted allocation that
 * was actually assigned to the widget. The adjusted allocation is
 * guaranteed to be completely contained within the
 * [method@Gtk.Widget.size_allocate] allocation, however.
 *
 * So a layout container is guaranteed that its children stay inside
 * the assigned bounds, but not that they have exactly the bounds the
 * container assigned.
 *
 * Deprecated: 4.12: Use [method@Gtk.Widget.compute_bounds],
 * [method@Gtk.Widget.get_width] or [method@Gtk.Widget.get_height] instead.
 */
void
gtk_widget_get_allocation (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  const graphene_rect_t *margin_rect;
  float dx, dy;
  GtkCssBoxes boxes;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (allocation != NULL);

  gtk_css_boxes_init (&boxes, widget);
  margin_rect = gtk_css_boxes_get_margin_rect (&boxes);

  if (gsk_transform_get_category (priv->transform) >= GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
    gsk_transform_to_translate (priv->transform, &dx, &dy);
  else
    dx = dy = 0;

  allocation->x = dx + ceil (margin_rect->origin.x);
  allocation->y = dy + ceil (margin_rect->origin.y);
  allocation->width = ceil (margin_rect->size.width);
  allocation->height = ceil (margin_rect->size.height);
}

/**
 * gtk_widget_contains:
 * @widget: the widget to query
 * @x: X coordinate to test, relative to @widget's origin
 * @y: Y coordinate to test, relative to @widget's origin
 *
 * Tests if the point at (@x, @y) is contained in @widget.
 *
 * The coordinates for (@x, @y) must be in widget coordinates, so
 * (0, 0) is assumed to be the top left of @widget's content area.
 *
 * Returns: %TRUE if @widget contains (@x, @y).
 */
gboolean
gtk_widget_contains (GtkWidget  *widget,
                     double      x,
                     double      y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!_gtk_widget_get_mapped (widget))
    return FALSE;

  return GTK_WIDGET_GET_CLASS (widget)->contains (widget, x, y);
}

/* do the checks for gtk_widget_pick that do not depend on position */
static gboolean
gtk_widget_can_be_picked (GtkWidget    *widget,
                          GtkPickFlags  flags)
{
  if (!_gtk_widget_get_mapped (widget))
    return FALSE;

  if (!(flags & GTK_PICK_NON_TARGETABLE) &&
      !gtk_widget_get_can_target (widget))
    return FALSE;

  if (!(flags & GTK_PICK_INSENSITIVE) &&
      !_gtk_widget_is_sensitive (widget))
    return FALSE;

  return TRUE;
}

static GtkWidget *
gtk_widget_do_pick (GtkWidget    *widget,
                    double        x,
                    double        y,
                    GtkPickFlags  flags)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  if (priv->overflow == GTK_OVERFLOW_HIDDEN)
    {
      GtkCssBoxes boxes;

      gtk_css_boxes_init (&boxes, widget);

      if (!gsk_rounded_rect_contains_point (gtk_css_boxes_get_padding_box (&boxes),
                                            &GRAPHENE_POINT_INIT (x, y)))
        return NULL;
    }

  for (child = _gtk_widget_get_last_child (widget);
       child;
       child = _gtk_widget_get_prev_sibling (child))
    {
      GtkWidgetPrivate *child_priv = gtk_widget_get_instance_private (child);
      GtkWidget *picked;
      graphene_point3d_t res;

      if (!gtk_widget_can_be_picked (child, flags))
        continue;

      if (GTK_IS_NATIVE (child))
        continue;

      if (child_priv->transform)
        {
          if (gsk_transform_get_category (child_priv->transform) >= GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
            {
              graphene_point_t transformed_p;

              gsk_transform_transform_point (child_priv->transform,
                                             &(graphene_point_t) { 0, 0 },
                                             &transformed_p);

              graphene_point3d_init (&res, x - transformed_p.x, y - transformed_p.y, 0.);
            }
          else
            {
              GskTransform *transform;
              graphene_matrix_t inv;
              graphene_point3d_t p0, p1;

              transform = gsk_transform_invert (gsk_transform_ref (child_priv->transform));
              if (transform == NULL)
                continue;

              gsk_transform_to_matrix (transform, &inv);
              gsk_transform_unref (transform);
              graphene_point3d_init (&p0, x, y, 0);
              graphene_point3d_init (&p1, x, y, 1);
              graphene_matrix_transform_point3d (&inv, &p0, &p0);
              graphene_matrix_transform_point3d (&inv, &p1, &p1);
              if (fabs (p0.z - p1.z) < 1.f / 4096)
                continue;

              graphene_point3d_interpolate (&p0, &p1, p0.z / (p0.z - p1.z), &res);
            }
        }
      else
        {
          graphene_point3d_init (&res, x, y, 0);
        }

      picked = gtk_widget_do_pick (child, res.x, res.y, flags);
      if (picked)
        return picked;
    }

  if (!GTK_WIDGET_GET_CLASS (widget)->contains (widget, x, y))
    return NULL;

  return widget;
}

/**
 * gtk_widget_pick:
 * @widget: the widget to query
 * @x: X coordinate to test, relative to @widget's origin
 * @y: Y coordinate to test, relative to @widget's origin
 * @flags: Flags to influence what is picked
 *
 * Finds the descendant of @widget closest to the point (@x, @y).
 *
 * The point must be given in widget coordinates, so (0, 0) is assumed
 * to be the top left of @widget's content area.
 *
 * Usually widgets will return %NULL if the given coordinate is not
 * contained in @widget checked via [method@Gtk.Widget.contains].
 * Otherwise they will recursively try to find a child that does
 * not return %NULL. Widgets are however free to customize their
 * picking algorithm.
 *
 * This function is used on the toplevel to determine the widget
 * below the mouse cursor for purposes of hover highlighting and
 * delivering events.
 *
 * Returns: (nullable) (transfer none): The widget descendant at
 *   the given point
 */
GtkWidget *
gtk_widget_pick (GtkWidget    *widget,
                 double        x,
                 double        y,
                 GtkPickFlags  flags)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (!gtk_widget_can_be_picked (widget, flags))
    return NULL;

  return gtk_widget_do_pick (widget, x, y, flags);
}

/**
 * gtk_widget_compute_transform:
 * @widget: a `GtkWidget`
 * @target: the target widget that the matrix will transform to
 * @out_transform: (out caller-allocates): location to
 *   store the final transformation
 *
 * Computes a matrix suitable to describe a transformation from
 * @widget's coordinate system into @target's coordinate system.
 *
 * The transform can not be computed in certain cases, for example
 * when @widget and @target do not share a common ancestor. In that
 * case @out_transform gets set to the identity matrix.
 *
 * To learn more about widget coordinate systems, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: %TRUE if the transform could be computed, %FALSE otherwise
 */
gboolean
gtk_widget_compute_transform (GtkWidget         *widget,
                              GtkWidget         *target,
                              graphene_matrix_t *out_transform)
{
  GtkWidget *ancestor, *iter;
  graphene_matrix_t transform, inverse, tmp;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);
  g_return_val_if_fail (out_transform != NULL, FALSE);

  if (widget->priv->root != target->priv->root)
    return FALSE;

  /* optimization for common case: parent wants coordinates of a direct child */
  if (target == widget->priv->parent)
    {
      gsk_transform_to_matrix (widget->priv->transform, out_transform);
      return TRUE;
    }

  ancestor = gtk_widget_common_ancestor (widget, target);
  if (ancestor == NULL)
    {
      graphene_matrix_init_identity (out_transform);
      return FALSE;
    }

  graphene_matrix_init_identity (&transform);
  for (iter = widget; iter != ancestor; iter = iter->priv->parent)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (iter);

      if (GTK_IS_NATIVE (iter))
        {
          graphene_matrix_init_identity (out_transform);
          return FALSE;
        }

      gsk_transform_to_matrix (priv->transform, &tmp);
      graphene_matrix_multiply (&transform, &tmp, &transform);
    }

  /* optimization for common case: parent wants coordinates of a non-direct child */
  if (ancestor == target)
    {
      graphene_matrix_init_from_matrix (out_transform, &transform);
      return TRUE;
    }

  graphene_matrix_init_identity (&inverse);
  for (iter = target; iter != ancestor; iter = iter->priv->parent)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (iter);
      gsk_transform_to_matrix (priv->transform, &tmp);

      if (GTK_IS_NATIVE (iter))
        {
          graphene_matrix_init_identity (out_transform);
          return FALSE;
        }

      graphene_matrix_multiply (&inverse, &tmp, &inverse);
    }
  if (!graphene_matrix_inverse (&inverse, &inverse))
    {
      graphene_matrix_init_identity (out_transform);
      return FALSE;
    }

  graphene_matrix_multiply (&transform, &inverse, out_transform);

  return TRUE;
}

/**
 * gtk_widget_compute_bounds:
 * @widget: the `GtkWidget` to query
 * @target: the `GtkWidget`
 * @out_bounds: (out caller-allocates): the rectangle taking the bounds
 *
 * Computes the bounds for @widget in the coordinate space of @target.
 *
 * The bounds of widget are (the bounding box of) the region that it is
 * expected to draw in. See the [coordinate system](coordinates.html)
 * overview to learn more.
 *
 * If the operation is successful, %TRUE is returned. If @widget has no
 * bounds or the bounds cannot be expressed in @target's coordinate space
 * (for example if both widgets are in different windows), %FALSE is
 * returned and @bounds is set to the zero rectangle.
 *
 * It is valid for @widget and @target to be the same widget.
 *
 * Returns: %TRUE if the bounds could be computed
 */
gboolean
gtk_widget_compute_bounds (GtkWidget       *widget,
                           GtkWidget       *target,
                           graphene_rect_t *out_bounds)
{
  graphene_matrix_t transform;
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);
  g_return_val_if_fail (out_bounds != NULL, FALSE);

  if (!gtk_widget_compute_transform (widget, target, &transform))
    {
      graphene_rect_init_from_rect (out_bounds, graphene_rect_zero ());
      return FALSE;
    }

  gtk_css_boxes_init (&boxes, widget);
  gsk_matrix_transform_bounds (&transform,
                               gtk_css_boxes_get_border_rect (&boxes),
                               out_bounds);

  return TRUE;
}

/**
 * gtk_widget_get_allocated_width:
 * @widget: the widget to query
 *
 * Returns the width that has currently been allocated to @widget.
 *
 * To learn more about widget sizes, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: the width of the @widget
 *
 * Deprecated: 4.12: Use [method@Gtk.Widget.get_width] instead
 */
int
gtk_widget_get_allocated_width (GtkWidget *widget)
{
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  gtk_css_boxes_init (&boxes, widget);

  return gtk_css_boxes_get_margin_rect (&boxes)->size.width;
}

/**
 * gtk_widget_get_allocated_height:
 * @widget: the widget to query
 *
 * Returns the height that has currently been allocated to @widget.
 *
 * To learn more about widget sizes, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: the height of the @widget
 *
 * Deprecated: 4.12: Use [method@Gtk.Widget.get_height] instead
 */
int
gtk_widget_get_allocated_height (GtkWidget *widget)
{
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  gtk_css_boxes_init (&boxes, widget);

  return gtk_css_boxes_get_margin_rect (&boxes)->size.height;
}

/**
 * gtk_widget_get_allocated_baseline:
 * @widget: the widget to query
 *
 * Returns the baseline that has currently been allocated to @widget.
 *
 * This function is intended to be used when implementing handlers
 * for the `GtkWidget`Class.snapshot() function, and when allocating
 * child widgets in `GtkWidget`Class.size_allocate().
 *
 * Returns: the baseline of the @widget, or -1 if none
 *
 * Deprecated: 4.12: Use [method@Gtk.Widget.get_baseline] instead
 */
int
gtk_widget_get_allocated_baseline (GtkWidget *widget)
{
  return gtk_widget_get_baseline (widget);
}

/**
 * gtk_widget_get_baseline:
 * @widget: the widget to query
 *
 * Returns the baseline that has currently been allocated to @widget.
 *
 * This function is intended to be used when implementing handlers
 * for the `GtkWidget`Class.snapshot() function, and when allocating
 * child widgets in `GtkWidget`Class.size_allocate().
 *
 * Returns: the baseline of the @widget, or -1 if none
 *
 * Since: 4.12
 */
int
gtk_widget_get_baseline (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssStyle *style;
  GtkBorder margin, border, padding;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  if (priv->baseline == -1)
    return -1;

  style = gtk_css_node_get_style (priv->cssnode);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  return priv->baseline - margin.top - border.top - padding.top;
}

/**
 * gtk_widget_set_opacity: (attributes org.gtk.Method.set_property=opacity)
 * @widget: a `GtkWidget`
 * @opacity: desired opacity, between 0 and 1
 *
 * Request the @widget to be rendered partially transparent.
 *
 * An opacity of 0 is fully transparent and an opacity of 1
 * is fully opaque.
 *
 * Opacity works on both toplevel widgets and child widgets, although
 * there are some limitations: For toplevel widgets, applying opacity
 * depends on the capabilities of the windowing system. On X11, this
 * has any effect only on X displays with a compositing manager,
 * see gdk_display_is_composited(). On Windows and Wayland it should
 * always work, although setting a window’s opacity after the window
 * has been shown may cause some flicker.
 *
 * Note that the opacity is inherited through inclusion — if you set
 * a toplevel to be partially translucent, all of its content will
 * appear translucent, since it is ultimatively rendered on that
 * toplevel. The opacity value itself is not inherited by child
 * widgets (since that would make widgets deeper in the hierarchy
 * progressively more translucent). As a consequence, [class@Gtk.Popover]s
 * and other [iface@Gtk.Native] widgets with their own surface will use their
 * own opacity value, and thus by default appear non-translucent,
 * even if they are attached to a toplevel that is translucent.
 */
void
gtk_widget_set_opacity (GtkWidget *widget,
                        double     opacity)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  guint8 alpha;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  opacity = CLAMP (opacity, 0.0, 1.0);

  alpha = round (opacity * 255);

  if (alpha == priv->user_alpha)
    return;

  priv->user_alpha = alpha;

  gtk_widget_queue_draw (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_OPACITY]);
}

/**
 * gtk_widget_get_opacity: (attributes org.gtk.Method.get_property=opacity)
 * @widget: a `GtkWidget`
 *
 * #Fetches the requested opacity for this widget.
 *
 * See [method@Gtk.Widget.set_opacity].
 *
 * Returns: the requested opacity for this widget.
 */
double
gtk_widget_get_opacity (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0.0);

  return priv->user_alpha / 255.0;
}

/**
 * gtk_widget_set_overflow: (attributes org.gtk.Method.set_property=overflow)
 * @widget: a `GtkWidget`
 * @overflow: desired overflow
 *
 * Sets how @widget treats content that is drawn outside the
 * widget's content area.
 *
 * See the definition of [enum@Gtk.Overflow] for details.
 *
 * This setting is provided for widget implementations and
 * should not be used by application code.
 *
 * The default value is %GTK_OVERFLOW_VISIBLE.
 */
void
gtk_widget_set_overflow (GtkWidget   *widget,
                         GtkOverflow  overflow)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->overflow == overflow)
    return;

  priv->overflow = overflow;

  gtk_widget_queue_draw (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_OVERFLOW]);
}

/**
 * gtk_widget_get_overflow: (attributes org.gtk.Method.get_property=overflow)
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s overflow value.
 *
 * Returns: The widget's overflow.
 **/
GtkOverflow
gtk_widget_get_overflow (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_OVERFLOW_VISIBLE);

  return priv->overflow;
}

void
gtk_widget_set_has_focus (GtkWidget *widget,
                          gboolean   has_focus)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->has_focus == has_focus)
    return;

  priv->has_focus = has_focus;

  gtk_accessible_platform_changed (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HAS_FOCUS]);
}

/**
 * gtk_widget_in_destruction:
 * @widget: a `GtkWidget`
 *
 * Returns whether the widget is currently being destroyed.
 *
 * This information can sometimes be used to avoid doing
 * unnecessary work.
 *
 * Returns: %TRUE if @widget is being destroyed
 */
gboolean
gtk_widget_in_destruction (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  priv = gtk_widget_get_instance_private (widget);

  return priv->in_destruction;
}

gboolean
_gtk_widget_get_alloc_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->alloc_needed;
}

static void
gtk_widget_set_alloc_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->alloc_needed = TRUE;

  do
    {
      if (priv->alloc_needed_on_child)
        break;

      priv->alloc_needed_on_child = TRUE;

      if (!priv->visible)
        break;

      if (GTK_IS_NATIVE (widget))
        {
          gtk_native_queue_relayout (GTK_NATIVE (widget));
          return;
        }

      widget = priv->parent;
      if (widget == NULL)
        break;

      priv = widget->priv;
    }
  while (TRUE);
}

gboolean
gtk_widget_needs_allocate (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->visible || !priv->child_visible)
    return FALSE;

  if (priv->resize_needed || priv->alloc_needed || priv->alloc_needed_on_child)
    return TRUE;

  return FALSE;
}

void
gtk_widget_ensure_allocate (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!gtk_widget_needs_allocate (widget))
    return;

  gtk_widget_ensure_resize (widget);

  /*  This code assumes that we only reach here if the previous
   *  allocation is still valid (ie no resize was queued).
   *  If that wasn't true, the parent would have taken care of
   *  things.
   */
  if (priv->alloc_needed)
    {
      gtk_widget_allocate (widget,
                           priv->allocated_width,
                           priv->allocated_height,
                           priv->allocated_baseline,
                           gsk_transform_ref (priv->allocated_transform));
    }
  else
    {
      gtk_widget_ensure_allocate_on_children (widget);
    }
}

void
gtk_widget_ensure_resize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->resize_needed)
    return;

  priv->resize_needed = FALSE;
}

void
_gtk_widget_add_sizegroup (GtkWidget    *widget,
                           gpointer      group)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_prepend (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  priv->have_size_groups = TRUE;
}

void
_gtk_widget_remove_sizegroup (GtkWidget    *widget,
                              gpointer      group)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_remove (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  priv->have_size_groups = groups != NULL;
}

GSList *
_gtk_widget_get_sizegroups (GtkWidget    *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->have_size_groups)
    return g_object_get_qdata (G_OBJECT (widget), quark_size_groups);

  return NULL;
}

/**
 * gtk_widget_class_set_css_name:
 * @widget_class: class to set the name on
 * @name: name to use
 *
 * Sets the name to be used for CSS matching of widgets.
 *
 * If this function is not called for a given class, the name
 * set on the parent class is used. By default, `GtkWidget`
 * uses the name "widget".
 */
void
gtk_widget_class_set_css_name (GtkWidgetClass *widget_class,
                               const char     *name)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (name != NULL);

  priv = widget_class->priv;

  priv->css_name = g_quark_from_string (name);
}

/**
 * gtk_widget_class_get_css_name:
 * @widget_class: class to set the name on
 *
 * Gets the name used by this class for matching in CSS code.
 *
 * See [method@Gtk.WidgetClass.set_css_name] for details.
 *
 * Returns: the CSS name of the given class
 */
const char *
gtk_widget_class_get_css_name (GtkWidgetClass *widget_class)
{
  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), NULL);

  return g_quark_to_string (widget_class->priv->css_name);
}

void
gtk_widget_css_changed (GtkWidget         *widget,
                        GtkCssStyleChange *change)
{
  GTK_WIDGET_GET_CLASS (widget)->css_changed (widget, change);
}

void
gtk_widget_system_setting_changed (GtkWidget        *widget,
                                   GtkSystemSetting  setting)
{
  GTK_WIDGET_GET_CLASS (widget)->system_setting_changed (widget, setting);
}

void
gtk_system_setting_changed (GdkDisplay       *display,
                            GtkSystemSetting  setting)
{
  GList *list, *toplevels;

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) g_object_ref, NULL);

  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_display (list->data) == display)
        gtk_widget_system_setting_changed (list->data, setting);
      g_object_unref (list->data);
    }

  g_list_free (toplevels);
}

GtkCssNode *
gtk_widget_get_css_node (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->cssnode;
}

GtkStyleContext *
_gtk_widget_peek_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->context;
}


/**
 * gtk_widget_get_style_context:
 * @widget: a `GtkWidget`
 *
 * Returns the style context associated to @widget.
 *
 * The returned object is guaranteed to be the same
 * for the lifetime of @widget.
 *
 * Returns: (transfer none): the widget’s `GtkStyleContext`
 *
 * Deprecated: 4.10: Style contexts will be removed in GTK 5
 */
GtkStyleContext *
gtk_widget_get_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (G_UNLIKELY (priv->context == NULL))
    {
      GdkDisplay *display;

      priv->context = gtk_style_context_new_for_node (priv->cssnode);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

      display = _gtk_widget_get_display (widget);
      if (display)
        gtk_style_context_set_display (priv->context, display);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  return priv->context;
}

static GtkActionMuxer *
gtk_widget_get_parent_muxer (GtkWidget *widget,
                             gboolean   create)
{
  GtkWidget *parent;

  if (GTK_IS_WINDOW (widget))
    return gtk_application_get_parent_muxer_for_window ((GtkWindow *)widget);

  parent = _gtk_widget_get_parent (widget);

  if (parent)
    return _gtk_widget_get_action_muxer (parent, create);

  return NULL;
}

void
_gtk_widget_update_parent_muxer (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  if (priv->muxer == NULL)
    return;

  gtk_action_muxer_set_parent (priv->muxer,
                               gtk_widget_get_parent_muxer (widget, FALSE));
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    _gtk_widget_update_parent_muxer (child);
}

GtkActionMuxer *
_gtk_widget_get_action_muxer (GtkWidget *widget,
                              gboolean   create)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS (widget);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->muxer)
    return priv->muxer;

  if (create || widget_class->priv->actions)
    {
      priv->muxer = gtk_action_muxer_new (widget);
      _gtk_widget_update_parent_muxer (widget);

      return priv->muxer;
    }
  else
    return gtk_widget_get_parent_muxer (widget, FALSE);
}

/**
 * gtk_widget_insert_action_group:
 * @widget: a `GtkWidget`
 * @name: the prefix for actions in @group
 * @group: (nullable): a `GActionGroup`, or %NULL to remove
 *   the previously inserted group for @name
 *
 * Inserts @group into @widget.
 *
 * Children of @widget that implement [iface@Gtk.Actionable] can
 * then be associated with actions in @group by setting their
 * “action-name” to @prefix.`action-name`.
 *
 * Note that inheritance is defined for individual actions. I.e.
 * even if you insert a group with prefix @prefix, actions with
 * the same prefix will still be inherited from the parent, unless
 * the group contains an action with the same name.
 *
 * If @group is %NULL, a previously inserted group for @name is
 * removed from @widget.
 */
void
gtk_widget_insert_action_group (GtkWidget    *widget,
                                const char   *name,
                                GActionGroup *group)
{
  GtkActionMuxer *muxer;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (name != NULL);

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);

  if (group)
    gtk_action_muxer_insert (muxer, name, group);
  else
    gtk_action_muxer_remove (muxer, name);
}

/****************************************************************
 *                 GtkBuilder automated templates               *
 ****************************************************************/
static AutomaticChildClass *
template_child_class_new (const char *name,
                          gboolean     internal_child,
                          gssize       offset)
{
  AutomaticChildClass *child_class = g_new0 (AutomaticChildClass, 1);

  child_class->name = g_strdup (name);
  child_class->internal_child = internal_child;
  child_class->offset = offset;

  return child_class;
}

static GHashTable *
get_auto_child_hash (GtkWidget *widget,
                     GType      type,
                     gboolean   create)
{
  GHashTable *auto_children;
  GHashTable *auto_child_hash;

  auto_children = (GHashTable *)g_object_get_qdata (G_OBJECT (widget), quark_auto_children);
  if (auto_children == NULL)
    {
      if (!create)
        return NULL;

      auto_children = g_hash_table_new_full (g_direct_hash,
                                             NULL,
                                             NULL, (GDestroyNotify)g_hash_table_destroy);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_auto_children,
                               auto_children,
                               (GDestroyNotify)g_hash_table_destroy);
    }

  auto_child_hash =
    g_hash_table_lookup (auto_children, GSIZE_TO_POINTER (type));

  if (!auto_child_hash && create)
    {
      auto_child_hash = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               NULL,
                                               (GDestroyNotify)g_object_unref);

      g_hash_table_insert (auto_children,
                           GSIZE_TO_POINTER (type),
                           auto_child_hash);
    }

  return auto_child_hash;
}

/**
 * gtk_widget_init_template:
 * @widget: a `GtkWidget`
 *
 * Creates and initializes child widgets defined in templates.
 *
 * This function must be called in the instance initializer
 * for any class which assigned itself a template using
 * [method@Gtk.WidgetClass.set_template].
 *
 * It is important to call this function in the instance initializer
 * of a `GtkWidget` subclass and not in `GObject.constructed()` or
 * `GObject.constructor()` for two reasons:
 *
 *  - derived widgets will assume that the composite widgets
 *    defined by its parent classes have been created in their
 *    relative instance initializers
 *  - when calling `g_object_new()` on a widget with composite templates,
 *    it’s important to build the composite widgets before the construct
 *    properties are set. Properties passed to `g_object_new()` should
 *    take precedence over properties set in the private template XML
 *
 * A good rule of thumb is to call this function as the first thing in
 * an instance initialization function.
 */
void
gtk_widget_init_template (GtkWidget *widget)
{
  GtkWidgetTemplate *template;
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *object;
  GSList *l;
  GType class_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  object = G_OBJECT (widget);
  class_type = G_OBJECT_TYPE (widget);

  template = GTK_WIDGET_GET_CLASS (widget)->priv->template;
  g_return_if_fail (template != NULL);

  builder = gtk_builder_new ();

  if (template->scope)
    gtk_builder_set_scope (builder, template->scope);

  gtk_builder_set_current_object (builder, object);

  /* This will build the template XML as children to the widget instance, also it
   * will validate that the template is created for the correct GType and assert that
   * there is no infinite recursion.
   */
  if (!gtk_builder_extend_with_template (builder, object, class_type,
                                         (const char *)g_bytes_get_data (template->data, NULL),
                                         g_bytes_get_size (template->data),
                                         &error))
    {
      /* This should never happen, if the template XML cannot be built
       * then it is a critical programming error.
       */
      g_critical ("Error building template class '%s' for an instance of type '%s': %s",
                  g_type_name (class_type), G_OBJECT_TYPE_NAME (object), error->message);
      g_error_free (error);
      goto out;
    }

  /* Build the automatic child data */
  for (l = template->children; l; l = l->next)
    {
      AutomaticChildClass *child_class = l->data;
      GHashTable *auto_child_hash;
      GObject *child;

      /* This will setup the pointer of an automated child, and cause
       * it to be available in any GtkBuildable.get_internal_child()
       * invocations which may follow by reference in child classes. */
      child = gtk_builder_get_object (builder, child_class->name);
      if (!child)
        {
          g_critical ("Unable to retrieve child object '%s' from class "
                      "template for type '%s' while building a '%s'",
                      child_class->name, g_type_name (class_type), G_OBJECT_TYPE_NAME (widget));
          goto out;
        }

      /* Insert into the hash so that it can be fetched with
       * gtk_widget_get_template_child() and also in automated
       * implementations of GtkBuildable.get_internal_child() */
      auto_child_hash = get_auto_child_hash (widget, class_type, TRUE);
      g_hash_table_insert (auto_child_hash, child_class->name, g_object_ref (child));

      if (child_class->offset != 0)
        {
          gpointer field_p;

          /* Assign 'object' to the specified offset in the instance (or private) data */
          field_p = G_STRUCT_MEMBER_P (widget, child_class->offset);
          (* (gpointer *) field_p) = child;
        }
    }

out:
  g_object_unref (builder);
}

/**
 * gtk_widget_dispose_template:
 * @widget: the widget with a template
 * @widget_type: the type of the widget to finalize the template for
 *
 * Clears the template children for the given widget.
 *
 * This function is the opposite of [method@Gtk.Widget.init_template], and
 * it is used to clear all the template children from a widget instance.
 * If you bound a template child to a field in the instance structure, or
 * in the instance private data structure, the field will be set to `NULL`
 * after this function returns.
 *
 * You should call this function inside the `GObjectClass.dispose()`
 * implementation of any widget that called `gtk_widget_init_template()`.
 * Typically, you will want to call this function last, right before
 * chaining up to the parent type's dispose implementation, e.g.
 *
 * ```c
 * static void
 * some_widget_dispose (GObject *gobject)
 * {
 *   SomeWidget *self = SOME_WIDGET (gobject);
 *
 *   // Clear the template data for SomeWidget
 *   gtk_widget_dispose_template (GTK_WIDGET (self), SOME_TYPE_WIDGET);
 *
 *   G_OBJECT_CLASS (some_widget_parent_class)->dispose (gobject);
 * }
 * ```
 *
 * Since: 4.8
 */
void
gtk_widget_dispose_template (GtkWidget *widget,
                             GType      widget_type)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (g_type_name (widget_type) != NULL);

  GObjectClass *object_class = g_type_class_peek (widget_type);
  GtkWidgetTemplate *template = GTK_WIDGET_CLASS (object_class)->priv->template;
  g_return_if_fail (template != NULL);

  /* Tear down the automatic child data */
  GHashTable *auto_child_hash = get_auto_child_hash (widget, widget_type, FALSE);

  for (GSList *l = template->children; l != NULL; l = l->next)
    {
      AutomaticChildClass *child_class = l->data;

      /* This will drop the reference on the template children */
      if (auto_child_hash != NULL)
        {
          gpointer child = g_hash_table_lookup (auto_child_hash, child_class->name);

          g_assert (child != NULL);

          /* We have to explicitly unparent direct children of this widget */
          if (GTK_IS_WIDGET (child) && _gtk_widget_get_parent (child) == widget)
            gtk_widget_unparent (child);

          g_hash_table_remove (auto_child_hash, child_class->name);
        }

      /* Nullify the field last, to avoid re-entrancy issues */
      if (child_class->offset != 0)
        {
          gpointer field_p;

          field_p = G_STRUCT_MEMBER_P (widget, child_class->offset);
          (* (gpointer *) field_p) = NULL;
        }
    }
}

/**
 * gtk_widget_class_set_template:
 * @widget_class: A `GtkWidgetClass`
 * @template_bytes: A `GBytes` holding the `GtkBuilder` XML
 *
 * This should be called at class initialization time to specify
 * the `GtkBuilder` XML to be used to extend a widget.
 *
 * For convenience, [method@Gtk.WidgetClass.set_template_from_resource]
 * is also provided.
 *
 * Note that any class that installs templates must call
 * [method@Gtk.Widget.init_template] in the widget’s instance initializer.
 */
void
gtk_widget_class_set_template (GtkWidgetClass *widget_class,
                               GBytes         *template_bytes)
{
  GError *error = NULL;
  GBytes *data = NULL;
  gconstpointer bytes_data;
  gsize bytes_size;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template == NULL);
  g_return_if_fail (template_bytes != NULL);

  widget_class->priv->template = g_new0 (GtkWidgetTemplate, 1);
  bytes_data = g_bytes_get_data (template_bytes, &bytes_size);

  if (_gtk_buildable_parser_is_precompiled (bytes_data, bytes_size))
    {
      widget_class->priv->template->data = g_bytes_ref (template_bytes);
      return;
    }

  data = _gtk_buildable_parser_precompile (bytes_data, bytes_size, &error);
  if (data == NULL)
    {
      g_warning ("Failed to precompile template for class %s: %s", G_OBJECT_CLASS_NAME (widget_class), error->message);
      g_error_free (error);
      return;
    }

  widget_class->priv->template->data = data;
}

/**
 * gtk_widget_class_set_template_from_resource:
 * @widget_class: A `GtkWidgetClass`
 * @resource_name: The name of the resource to load the template from
 *
 * A convenience function that calls [method@Gtk.WidgetClass.set_template]
 * with the contents of a `GResource`.
 *
 * Note that any class that installs templates must call
 * [method@Gtk.Widget.init_template] in the widget’s instance
 * initializer.
 */
void
gtk_widget_class_set_template_from_resource (GtkWidgetClass    *widget_class,
                                             const char        *resource_name)
{
  GError *error = NULL;
  GBytes *bytes = NULL;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template == NULL);
  g_return_if_fail (resource_name && resource_name[0]);

  /* This is a hack, because class initializers now access resources
   * and GIR/gtk-doc initializes classes without initializing GTK,
   * we ensure that our base resources are registered here and
   * avoid warnings which building GIRs/documentation.
   */
  _gtk_ensure_resources ();

  bytes = g_resources_lookup_data (resource_name, 0, &error);
  if (!bytes)
    {
      g_critical ("Unable to load resource for composite template for type '%s': %s",
                  G_OBJECT_CLASS_NAME (widget_class), error->message);
      g_error_free (error);
      return;
    }

  gtk_widget_class_set_template (widget_class, bytes);
  g_bytes_unref (bytes);
}

/**
 * gtk_widget_class_bind_template_callback_full:
 * @widget_class: A `GtkWidgetClass`
 * @callback_name: The name of the callback as expected in the template XML
 * @callback_symbol: (scope async): The callback symbol
 *
 * Declares a @callback_symbol to handle @callback_name from
 * the template XML defined for @widget_type.
 *
 * This function is not supported after [method@Gtk.WidgetClass.set_template_scope]
 * has been used on @widget_class. See [method@Gtk.BuilderCScope.add_callback_symbol].
 *
 * Note that this must be called from a composite widget classes
 * class initializer after calling [method@Gtk.WidgetClass.set_template].
 */
void
gtk_widget_class_bind_template_callback_full (GtkWidgetClass *widget_class,
                                              const char     *callback_name,
                                              GCallback       callback_symbol)
{
  GtkWidgetTemplate *template;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);
  g_return_if_fail (callback_name && callback_name[0]);
  g_return_if_fail (callback_symbol != NULL);

  template = widget_class->priv->template;
  if (template->scope == NULL)
    template->scope = gtk_builder_cscope_new ();

  if (GTK_IS_BUILDER_CSCOPE (template->scope))
    {
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (template->scope),
                                              callback_name,
                                              callback_symbol);
    }
  else
    {
      g_critical ("Adding a callback to %s, but scope is not a GtkBuilderCScope.", G_OBJECT_CLASS_NAME (widget_class));
    }
}

/**
 * gtk_widget_class_set_template_scope:
 * @widget_class: A `GtkWidgetClass`
 * @scope: (transfer none): The `GtkBuilderScope` to use when loading
 *   the class template
 *
 * For use in language bindings, this will override the default
 * `GtkBuilderScope` to be used when parsing GtkBuilder XML from
 * this class’s template data.
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling [method@Gtk.WidgetClass.set_template].
 */
void
gtk_widget_class_set_template_scope (GtkWidgetClass  *widget_class,
                                     GtkBuilderScope *scope)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);
  g_return_if_fail (GTK_IS_BUILDER_SCOPE (scope));

  /* Defensive, destroy any previously set data */
  g_set_object (&widget_class->priv->template->scope, scope);
}

/**
 * gtk_widget_class_bind_template_child_full:
 * @widget_class: A `GtkWidgetClass`
 * @name: The “id” of the child defined in the template XML
 * @internal_child: Whether the child should be accessible as an “internal-child”
 *   when this class is used in GtkBuilder XML
 * @struct_offset: The structure offset into the composite widget’s instance
 *   public or private structure where the automated child pointer should be set,
 *   or 0 to not assign the pointer.
 *
 * Automatically assign an object declared in the class template XML to
 * be set to a location on a freshly built instance’s private data, or
 * alternatively accessible via [method@Gtk.Widget.get_template_child].
 *
 * The struct can point either into the public instance, then you should
 * use `G_STRUCT_OFFSET(WidgetType, member)` for @struct_offset, or in the
 * private struct, then you should use `G_PRIVATE_OFFSET(WidgetType, member)`.
 *
 * An explicit strong reference will be held automatically for the duration
 * of your instance’s life cycle, it will be released automatically when
 * `GObjectClass.dispose()` runs on your instance and if a @struct_offset
 * that is `!= 0` is specified, then the automatic location in your instance
 * public or private data will be set to %NULL. You can however access an
 * automated child pointer the first time your classes `GObjectClass.dispose()`
 * runs, or alternatively in [signal@Gtk.Widget::destroy].
 *
 * If @internal_child is specified, [vfunc@Gtk.Buildable.get_internal_child]
 * will be automatically implemented by the `GtkWidget` class so there is no
 * need to implement it manually.
 *
 * The wrapper macros [func@Gtk.widget_class_bind_template_child],
 * [func@Gtk.widget_class_bind_template_child_internal],
 * [func@Gtk.widget_class_bind_template_child_private] and
 * [func@Gtk.widget_class_bind_template_child_internal_private]
 * might be more convenient to use.
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling [method@Gtk.WidgetClass.set_template].
 */
void
gtk_widget_class_bind_template_child_full (GtkWidgetClass *widget_class,
                                           const char     *name,
                                           gboolean        internal_child,
                                           gssize          struct_offset)
{
  AutomaticChildClass *child_class;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);
  g_return_if_fail (name && name[0]);

  child_class = template_child_class_new (name,
                                          internal_child,
                                          struct_offset);
  widget_class->priv->template->children =
    g_slist_prepend (widget_class->priv->template->children, child_class);
}

/**
 * gtk_widget_get_template_child:
 * @widget: A `GtkWidget`
 * @widget_type: The `GType` to get a template child for
 * @name: The “id” of the child defined in the template XML
 *
 * Fetch an object build from the template XML for @widget_type in
 * this @widget instance.
 *
 * This will only report children which were previously declared
 * with [method@Gtk.WidgetClass.bind_template_child_full] or one of its
 * variants.
 *
 * This function is only meant to be called for code which is private
 * to the @widget_type which declared the child and is meant for language
 * bindings which cannot easily make use of the GObject structure offsets.
 *
 * Returns: (transfer none): The object built in the template XML with
 *   the id @name
 */
GObject *
gtk_widget_get_template_child (GtkWidget   *widget,
                               GType        widget_type,
                               const char *name)
{
  GHashTable *auto_child_hash;
  GObject *ret = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (g_type_name (widget_type) != NULL, NULL);
  g_return_val_if_fail (name && name[0], NULL);

  auto_child_hash = get_auto_child_hash (widget, widget_type, FALSE);

  if (auto_child_hash)
    ret = g_hash_table_lookup (auto_child_hash, name);

  return ret;
}

/**
 * gtk_widget_activate_action_variant: (rename-to gtk_widget_activate_action)
 * @widget: a `GtkWidget`
 * @name: the name of the action to activate
 * @args: (nullable): parameters to use
 *
 * Looks up the action in the action groups associated with
 * @widget and its ancestors, and activates it.
 *
 * If the action is in an action group added with
 * [method@Gtk.Widget.insert_action_group], the @name is expected
 * to be prefixed with the prefix that was used when the group was
 * inserted.
 *
 * The arguments must match the actions expected parameter type,
 * as returned by `g_action_get_parameter_type()`.
 *
 * Returns: %TRUE if the action was activated, %FALSE if the
 *   action does not exist.
 */
gboolean
gtk_widget_activate_action_variant (GtkWidget  *widget,
                                    const char *name,
                                    GVariant   *args)
{
  GtkActionMuxer *muxer;

  muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  if (muxer == NULL)
    return FALSE;

  if (!gtk_action_muxer_has_action (muxer, name))
    return FALSE;

  gtk_action_muxer_activate_action (muxer, name, args);

  return TRUE;
}

/**
 * gtk_widget_activate_action:
 * @widget: a `GtkWidget`
 * @name: the name of the action to activate
 * @format_string: (nullable): GVariant format string for arguments
 * @...: arguments, as given by format string
 *
 * Looks up the action in the action groups associated
 * with @widget and its ancestors, and activates it.
 *
 * This is a wrapper around [method@Gtk.Widget.activate_action_variant]
 * that constructs the @args variant according to @format_string.
 *
 * Returns: %TRUE if the action was activated, %FALSE if the action
 *   does not exist
 */
gboolean
gtk_widget_activate_action (GtkWidget  *widget,
                            const char *name,
                            const char *format_string,
                            ...)
{
  GVariant *parameters = NULL;
  gboolean result;

  if (format_string != NULL)
    {
      va_list args;

      va_start (args, format_string);
      parameters = g_variant_new_va (format_string, NULL, &args);
      va_end (args);

      g_variant_ref_sink (parameters);
    }

  result = gtk_widget_activate_action_variant (widget, name, parameters);

  g_clear_pointer (&parameters, g_variant_unref);

  return result;
}

/**
 * gtk_widget_activate_default:
 * @widget: a `GtkWidget`
 *
 * Activates the `default.activate` action from @widget.
 */
void
gtk_widget_activate_default (GtkWidget *widget)
{
  gtk_widget_activate_action (widget, "default.activate", NULL);
}

void
gtk_widget_cancel_event_sequence (GtkWidget             *widget,
                                  GtkGesture            *gesture,
                                  GdkEventSequence      *sequence,
                                  GtkEventSequenceState  state)
{
  gboolean handled = FALSE;
  GtkWidget *event_widget;
  gboolean cancel = TRUE;
  GdkEvent *event;

  handled = _gtk_widget_set_sequence_state_internal (widget, sequence,
                                                     state, gesture);

  if (!handled || state != GTK_EVENT_SEQUENCE_CLAIMED)
    return;

  event = _gtk_widget_get_last_event (widget, sequence, &event_widget);

  if (!event)
    return;

  while (event_widget)
    {
      if (event_widget == widget)
        cancel = FALSE;
      else if (cancel)
        _gtk_widget_cancel_sequence (event_widget, sequence);
      else
        _gtk_widget_set_sequence_state_internal (event_widget, sequence,
                                                 GTK_EVENT_SEQUENCE_DENIED,
                                                 NULL);

      event_widget = _gtk_widget_get_parent (event_widget);
    }

}

/**
 * gtk_widget_add_controller:
 * @widget: a `GtkWidget`
 * @controller: (transfer full): a `GtkEventController` that hasn't been
 *   added to a widget yet
 *
 * Adds @controller to @widget so that it will receive events.
 *
 * You will usually want to call this function right after
 * creating any kind of [class@Gtk.EventController].
 */
void
gtk_widget_add_controller (GtkWidget          *widget,
                           GtkEventController *controller)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (gtk_event_controller_get_widget (controller) == NULL);

  GTK_EVENT_CONTROLLER_GET_CLASS (controller)->set_widget (controller, widget);

  priv->event_controllers = g_list_prepend (priv->event_controllers, controller);

  if (priv->controller_observer)
    gtk_list_list_model_item_added_at (priv->controller_observer, 0);
}

/**
 * gtk_widget_remove_controller:
 * @widget: a `GtkWidget`
 * @controller: (transfer none): a `GtkEventController`
 *
 * Removes @controller from @widget, so that it doesn't process
 * events anymore.
 *
 * It should not be used again.
 *
 * Widgets will remove all event controllers automatically when they
 * are destroyed, there is normally no need to call this function.
 */
void
gtk_widget_remove_controller (GtkWidget          *widget,
                              GtkEventController *controller)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *before, *list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (gtk_event_controller_get_widget (controller) == widget);

  GTK_EVENT_CONTROLLER_GET_CLASS (controller)->unset_widget (controller);

  list = g_list_find (priv->event_controllers, controller);
  before = list->prev;
  priv->event_controllers = g_list_delete_link (priv->event_controllers, list);
  g_object_unref (controller);

  if (priv->controller_observer)
    gtk_list_list_model_item_removed (priv->controller_observer, before);
}

void
gtk_widget_reset_controllers (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  /* Reset all controllers */
  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      if (controller == NULL)
        continue;

      gtk_event_controller_reset (controller);
    }
}

GtkEventController **
gtk_widget_list_controllers (GtkWidget           *widget,
                             GtkPropagationPhase  phase,
                             guint               *out_n_controllers)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GPtrArray *controllers = g_ptr_array_new ();
  GList *l;

  g_assert (out_n_controllers);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      if (gtk_event_controller_get_propagation_phase (controller) == phase)
        g_ptr_array_add (controllers, controller);
    }

  *out_n_controllers = controllers->len;

  return (GtkEventController **)g_ptr_array_free (controllers, FALSE);
}

static GskRenderNode *
gtk_widget_create_render_node (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (widget);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssBoxes boxes;
  GtkCssValue *filter_value;
  double css_opacity, opacity;
  GtkCssStyle *style;

  style = gtk_css_node_get_style (priv->cssnode);

  css_opacity = _gtk_css_number_value_get (style->other->opacity, 100);
  opacity = CLAMP (css_opacity, 0.0, 1.0) * priv->user_alpha / 255.0;

  if (opacity <= 0.0)
    return NULL;

  gtk_css_boxes_init (&boxes, widget);

  gtk_snapshot_push_collect (snapshot);
  gtk_snapshot_push_debug (snapshot,
                           "RenderNode for %s %p",
                           G_OBJECT_TYPE_NAME (widget), widget);

  filter_value = style->other->filter;
  gtk_css_filter_value_push_snapshot (filter_value, snapshot);

  if (opacity < 1.0)
    gtk_snapshot_push_opacity (snapshot, opacity);

  gtk_css_style_snapshot_background (&boxes, snapshot);
  gtk_css_style_snapshot_border (&boxes, snapshot);

  if (priv->overflow == GTK_OVERFLOW_HIDDEN)
    {
      gtk_snapshot_push_rounded_clip (snapshot, gtk_css_boxes_get_padding_box (&boxes));
      klass->snapshot (widget, snapshot);
      gtk_snapshot_pop (snapshot);
    }
  else
    {
      klass->snapshot (widget, snapshot);
    }

  gtk_css_style_snapshot_outline (&boxes, snapshot);

  if (opacity < 1.0)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (filter_value, snapshot);

  gtk_snapshot_pop (snapshot);

  return gtk_snapshot_pop_collect (snapshot);
}

static void
gtk_widget_do_snapshot (GtkWidget *widget,
                        GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GskRenderNode *render_node;

  if (!priv->draw_needed)
    return;

  g_assert (priv->mapped);

  if (_gtk_widget_get_alloc_needed (widget))
    {
      g_warning ("Trying to snapshot %s %p without a current allocation", gtk_widget_get_name (widget), widget);
      return;
    }

  gtk_widget_push_paintables (widget);

  render_node = gtk_widget_create_render_node (widget, snapshot);
  /* This can happen when nested drawing happens and a widget contains itself
   * or when we replace a clipped area
   */
  g_clear_pointer (&priv->render_node, gsk_render_node_unref);
  priv->render_node = render_node;

  priv->draw_needed = FALSE;

  gtk_widget_pop_paintables (widget);
  gtk_widget_update_paintables (widget);
}

void
gtk_widget_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!_gtk_widget_get_mapped (widget))
    return;

  gtk_widget_do_snapshot (widget, snapshot);

  if (priv->render_node)
    gtk_snapshot_append_node (snapshot, priv->render_node);
}

void
gtk_widget_render (GtkWidget            *widget,
                   GdkSurface           *surface,
                   const cairo_region_t *region)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkSnapshot *snapshot;
  GskRenderer *renderer;
  GskRenderNode *root;
  double x, y;
  gint64 before_snapshot G_GNUC_UNUSED;
  gint64 before_render G_GNUC_UNUSED;

  before_snapshot = GDK_PROFILER_CURRENT_TIME;
  before_render = 0;

  if (!GTK_IS_NATIVE (widget))
    return;

  renderer = gtk_native_get_renderer (GTK_NATIVE (widget));
  if (renderer == NULL)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_native_get_surface_transform (GTK_NATIVE (widget), &x, &y);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gtk_widget_snapshot (widget, snapshot);
  root = gtk_snapshot_free_to_node (snapshot);

  if (GDK_PROFILER_IS_RUNNING)
    {
      before_render = GDK_PROFILER_CURRENT_TIME;
      gdk_profiler_add_mark (before_snapshot, (before_render - before_snapshot), "Widget snapshot", "");
    }

  if (root != NULL)
    {
      root = gtk_inspector_prepare_render (widget,
                                           renderer,
                                           surface,
                                           region,
                                           root,
                                           priv->render_node);

      gsk_renderer_render (renderer, root, region);

      gsk_render_node_unref (root);

      gdk_profiler_end_mark (before_render, "Widget render", "");
    }
}

static void
gtk_widget_child_observer_destroyed (gpointer widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->children_observer = NULL;
}

/**
 * gtk_widget_observe_children:
 * @widget: a `GtkWidget`
 *
 * Returns a `GListModel` to track the children of @widget.
 *
 * Calling this function will enable extra internal bookkeeping
 * to track children and emit signals on the returned listmodel.
 * It may slow down operations a lot.
 *
 * Applications should try hard to avoid calling this function
 * because of the slowdowns.
 *
 * Returns: (transfer full) (attributes element-type=GtkWidget):
 *   a `GListModel` tracking @widget's children
 */
GListModel *
gtk_widget_observe_children (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->children_observer)
    return g_object_ref (G_LIST_MODEL (priv->children_observer));

  priv->children_observer = gtk_list_list_model_new ((gpointer) gtk_widget_get_first_child,
                                                     (gpointer) gtk_widget_get_next_sibling,
                                                     (gpointer) gtk_widget_get_prev_sibling,
                                                     (gpointer) gtk_widget_get_last_child,
                                                     (gpointer) g_object_ref,
                                                     widget,
                                                     gtk_widget_child_observer_destroyed);

  return G_LIST_MODEL (priv->children_observer);
}

static void
gtk_widget_controller_observer_destroyed (gpointer widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->controller_observer = NULL;
}

static gpointer
gtk_widget_controller_list_get_first (gpointer widget)
{
  return GTK_WIDGET (widget)->priv->event_controllers;
}

static gpointer
gtk_widget_controller_list_get_next (gpointer item,
                                     gpointer widget)
{
  return g_list_next (item);
}

static gpointer
gtk_widget_controller_list_get_prev (gpointer item,
                                     gpointer widget)
{
  return g_list_previous (item);
}

static gpointer
gtk_widget_controller_list_get_item (gpointer item,
                                      gpointer widget)
{
  return g_object_ref (((GList *) item)->data);
}

/**
 * gtk_widget_observe_controllers:
 * @widget: a `GtkWidget`
 *
 * Returns a `GListModel` to track the [class@Gtk.EventController]s
 * of @widget.
 *
 * Calling this function will enable extra internal bookkeeping
 * to track controllers and emit signals on the returned listmodel.
 * It may slow down operations a lot.
 *
 * Applications should try hard to avoid calling this function
 * because of the slowdowns.
 *
 * Returns: (transfer full) (attributes element-type=GtkEventController):
 *   a `GListModel` tracking @widget's controllers
 */
GListModel *
gtk_widget_observe_controllers (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->controller_observer)
    return g_object_ref (G_LIST_MODEL (priv->controller_observer));

  priv->controller_observer = gtk_list_list_model_new (gtk_widget_controller_list_get_first,
                                                       gtk_widget_controller_list_get_next,
                                                       gtk_widget_controller_list_get_prev,
                                                       NULL,
                                                       gtk_widget_controller_list_get_item,
                                                       widget,
                                                       gtk_widget_controller_observer_destroyed);

  return G_LIST_MODEL (priv->controller_observer);
}

/**
 * gtk_widget_get_first_child:
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s first child.
 *
 * This API is primarily meant for widget implementations.
 *
 * Returns: (transfer none) (nullable): The widget's first child
 */
GtkWidget *
gtk_widget_get_first_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->first_child;
}

/**
 * gtk_widget_get_last_child:
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s last child.
 *
 * This API is primarily meant for widget implementations.
 *
 * Returns: (transfer none) (nullable): The widget's last child
 */
GtkWidget *
gtk_widget_get_last_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->last_child;
}

/**
 * gtk_widget_get_next_sibling:
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s next sibling.
 *
 * This API is primarily meant for widget implementations.
 *
 * Returns: (transfer none) (nullable): The widget's next sibling
 */
GtkWidget *
gtk_widget_get_next_sibling (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->next_sibling;
}

/**
 * gtk_widget_get_prev_sibling:
 * @widget: a `GtkWidget`
 *
 * Returns the widget’s previous sibling.
 *
 * This API is primarily meant for widget implementations.
 *
 * Returns: (transfer none) (nullable): The widget's previous sibling
 */
GtkWidget *
gtk_widget_get_prev_sibling (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->prev_sibling;
}

/**
 * gtk_widget_insert_after:
 * @widget: a `GtkWidget`
 * @parent: the parent `GtkWidget` to insert @widget into
 * @previous_sibling: (nullable): the new previous sibling of @widget
 *
 * Inserts @widget into the child widget list of @parent.
 *
 * It will be placed after @previous_sibling, or at the beginning if
 * @previous_sibling is %NULL.
 *
 * After calling this function, `gtk_widget_get_prev_sibling(widget)`
 * will return @previous_sibling.
 *
 * If @parent is already set as the parent widget of @widget, this
 * function can also be used to reorder @widget in the child widget
 * list of @parent.
 *
 * This API is primarily meant for widget implementations; if you are
 * just using a widget, you *must* use its own API for adding children.
 */
void
gtk_widget_insert_after (GtkWidget *widget,
                         GtkWidget *parent,
                         GtkWidget *previous_sibling)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (previous_sibling == NULL || GTK_IS_WIDGET (previous_sibling));
  g_return_if_fail (previous_sibling == NULL || _gtk_widget_get_parent (previous_sibling) == parent);

  if (widget == previous_sibling ||
      (previous_sibling && _gtk_widget_get_prev_sibling (widget) == previous_sibling))
    return;

  if (!previous_sibling && _gtk_widget_get_first_child (parent) == widget)
    return;

  gtk_widget_reposition_after (widget,
                               parent,
                               previous_sibling);
}

/**
 * gtk_widget_insert_before:
 * @widget: a `GtkWidget`
 * @parent: the parent `GtkWidget` to insert @widget into
 * @next_sibling: (nullable): the new next sibling of @widget
 *
 * Inserts @widget into the child widget list of @parent.
 *
 * It will be placed before @next_sibling, or at the end if
 * @next_sibling is %NULL.
 *
 * After calling this function, `gtk_widget_get_next_sibling(widget)`
 * will return @next_sibling.
 *
 * If @parent is already set as the parent widget of @widget, this function
 * can also be used to reorder @widget in the child widget list of @parent.
 *
 * This API is primarily meant for widget implementations; if you are
 * just using a widget, you *must* use its own API for adding children.
 */
void
gtk_widget_insert_before (GtkWidget *widget,
                          GtkWidget *parent,
                          GtkWidget *next_sibling)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (next_sibling == NULL || GTK_IS_WIDGET (next_sibling));
  g_return_if_fail (next_sibling == NULL || _gtk_widget_get_parent (next_sibling) == parent);

  if (widget == next_sibling ||
      (next_sibling && _gtk_widget_get_next_sibling (widget) == next_sibling))
    return;

  if (!next_sibling && _gtk_widget_get_last_child (parent) == widget)
    return;

  gtk_widget_reposition_after (widget, parent,
                               next_sibling ? _gtk_widget_get_prev_sibling (next_sibling) :
                                              _gtk_widget_get_last_child (parent));
}

void
gtk_widget_forall (GtkWidget   *widget,
                   GtkCallback  callback,
                   gpointer     user_data)
{
  GtkWidget *child;
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = _gtk_widget_get_first_child (widget);
  while (child)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (child);

      callback(child, user_data);

      child = next;
    }
}

/**
 * gtk_widget_snapshot_child:
 * @widget: a `GtkWidget`
 * @child: a child of @widget
 * @snapshot: `GtkSnapshot` as passed to the widget. In particular, no
 *   calls to gtk_snapshot_translate() or other transform calls should
 *   have been made.
 *
 * Snapshot the a child of @widget.
 *
 * When a widget receives a call to the snapshot function,
 * it must send synthetic [vfunc@Gtk.Widget.snapshot] calls
 * to all children. This function provides a convenient way
 * of doing this. A widget, when it receives a call to its
 * [vfunc@Gtk.Widget.snapshot] function, calls
 * gtk_widget_snapshot_child() once for each child, passing in
 * the @snapshot the widget received.
 *
 * gtk_widget_snapshot_child() takes care of translating the origin of
 * @snapshot, and deciding whether the child needs to be snapshot.
 *
 * This function does nothing for children that implement `GtkNative`.
 */
void
gtk_widget_snapshot_child (GtkWidget   *widget,
                           GtkWidget   *child,
                           GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (child);

  g_return_if_fail (_gtk_widget_get_parent (child) == widget);
  g_return_if_fail (snapshot != NULL);

  if (!_gtk_widget_get_mapped (child))
    return;

  if (GTK_IS_NATIVE (child))
    return;

  gtk_widget_do_snapshot (child, snapshot);

  if (!priv->render_node)
    return;

  if (priv->transform)
    {
      GskRenderNode *transform_node = gsk_transform_node_new (priv->render_node,
                                                              priv->transform);

      gtk_snapshot_append_node (snapshot, transform_node);
      gsk_render_node_unref (transform_node);
    }
  else
    {
      gtk_snapshot_append_node (snapshot, priv->render_node);
    }
}

/**
 * gtk_widget_set_focus_child:
 * @widget: a `GtkWidget`
 * @child: (nullable): a direct child widget of @widget or %NULL
 *   to unset the focus child of @widget
 *
 * Set @child as the current focus child of @widget.
 *
 * This function is only suitable for widget implementations.
 * If you want a certain widget to get the input focus, call
 * [method@Gtk.Widget.grab_focus] on it.
 */
void
gtk_widget_set_focus_child (GtkWidget *widget,
                            GtkWidget *child)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (child != NULL)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));
      g_return_if_fail (gtk_widget_get_parent (child) == widget);
    }

  GTK_WIDGET_GET_CLASS (widget)->set_focus_child (widget, child);
}

static void
gtk_widget_real_set_focus_child (GtkWidget *widget,
                                 GtkWidget *child)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_set_object (&priv->focus_child, child);
}

/**
 * gtk_widget_get_focus_child:
 * @widget: a `GtkWidget`
 *
 * Returns the current focus child of @widget.
 *
 * Returns: (nullable) (transfer none): The current focus
 *   child of @widget
 */
GtkWidget *
gtk_widget_get_focus_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->focus_child;
}

/**
 * gtk_widget_set_cursor: (attributes org.gtk.Method.set_property=cursor)
 * @widget: a `GtkWidget`
 * @cursor: (nullable): the new cursor
 *
 * Sets the cursor to be shown when pointer devices point
 * towards @widget.
 *
 * If the @cursor is NULL, @widget will use the cursor
 * inherited from the parent widget.
 */
void
gtk_widget_set_cursor (GtkWidget *widget,
                       GdkCursor *cursor)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkRoot *root;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cursor == NULL || GDK_IS_CURSOR (cursor));

  if (!g_set_object (&priv->cursor, cursor))
    return;

  root = _gtk_widget_get_root (widget);
  if (GTK_IS_WINDOW (root))
    gtk_window_maybe_update_cursor (GTK_WINDOW (root), widget, NULL);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CURSOR]);
}

/**
 * gtk_widget_set_cursor_from_name:
 * @widget: a `GtkWidget`
 * @name: (nullable): The name of the cursor
 *
 * Sets a named cursor to be shown when pointer devices point
 * towards @widget.
 *
 * This is a utility function that creates a cursor via
 * [ctor@Gdk.Cursor.new_from_name] and then sets it on @widget
 * with [method@Gtk.Widget.set_cursor]. See those functions for
 * details.
 *
 * On top of that, this function allows @name to be %NULL, which
 * will do the same as calling [method@Gtk.Widget.set_cursor]
 * with a %NULL cursor.
 */
void
gtk_widget_set_cursor_from_name (GtkWidget  *widget,
                                 const char *name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (name)
    {
      GdkCursor *cursor;

      cursor = gdk_cursor_new_from_name (name, NULL);
      gtk_widget_set_cursor (widget, cursor);
      g_object_unref (cursor);
    }
  else
    {
      gtk_widget_set_cursor (widget, NULL);
    }
}

/**
 * gtk_widget_get_cursor: (attributes org.gtk.Method.get_property=cursor)
 * @widget: a `GtkWidget`
 *
 * Queries the cursor set on @widget.
 *
 * See [method@Gtk.Widget.set_cursor] for details.
 *
 * Returns: (nullable) (transfer none): the cursor
 *   currently in use or %NULL if the cursor is inherited
 */
GdkCursor *
gtk_widget_get_cursor (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->cursor;
}

/**
 * gtk_widget_set_can_target: (attributes org.gtk.Method.set_property=can-target)
 * @widget: a `GtkWidget`
 * @can_target: whether this widget should be able to
 *   receive pointer events
 *
 * Sets whether @widget can be the target of pointer events.
 */
void
gtk_widget_set_can_target (GtkWidget *widget,
                           gboolean   can_target)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  can_target = !!can_target;

  if (priv->can_target == can_target)
    return;

  priv->can_target = can_target;

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CAN_TARGET]);
}

/**
 * gtk_widget_get_can_target: (attributes org.gtk.Method.get_property=can-target)
 * @widget: a `GtkWidget`
 *
 * Queries whether @widget can be the target of pointer events.
 *
 * Returns: %TRUE if @widget can receive pointer events
 */
gboolean
gtk_widget_get_can_target (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->can_target;
}

/**
 * gtk_widget_get_width:
 * @widget: a `GtkWidget`
 *
 * Returns the content width of the widget.
 *
 * This function returns the width passed to its
 * size-allocate implementation, which is the width you
 * should be using in [vfunc@Gtk.Widget.snapshot].
 *
 * For pointer events, see [method@Gtk.Widget.contains].
 *
 * To learn more about widget sizes, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: The width of @widget
 */
int
gtk_widget_get_width (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->width;
}

/**
 * gtk_widget_get_height:
 * @widget: a `GtkWidget`
 *
 * Returns the content height of the widget.
 *
 * This function returns the height passed to its
 * size-allocate implementation, which is the height you
 * should be using in [vfunc@Gtk.Widget.snapshot].
 *
 * For pointer events, see [method@Gtk.Widget.contains].
 *
 * To learn more about widget sizes, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: The height of @widget
 */
int
gtk_widget_get_height (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->height;
}

/**
 * gtk_widget_get_size:
 * @widget: a `GtkWidget`
 * @orientation: the orientation to query
 *
 * Returns the content width or height of the widget.
 *
 * Which dimension is returned depends on @orientation.
 *
 * This is equivalent to calling [method@Gtk.Widget.get_width]
 * for %GTK_ORIENTATION_HORIZONTAL or [method@Gtk.Widget.get_height]
 * for %GTK_ORIENTATION_VERTICAL, but can be used when
 * writing orientation-independent code, such as when
 * implementing [iface@Gtk.Orientable] widgets.
 *
 * To learn more about widget sizes, see the coordinate
 * system [overview](coordinates.html).
 *
 * Returns: The size of @widget in @orientation.
 */
int
gtk_widget_get_size (GtkWidget      *widget,
                     GtkOrientation  orientation)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return priv->width;
  else
    return priv->height;
}

/**
 * gtk_widget_class_set_layout_manager_type:
 * @widget_class: a `GtkWidgetClass`
 * @type: The object type that implements the `GtkLayoutManager`
 *   for @widget_class
 *
 * Sets the type to be used for creating layout managers for
 * widgets of @widget_class.
 *
 * The given @type must be a subtype of [class@Gtk.LayoutManager].
 *
 * This function should only be called from class init functions
 * of widgets.
 */
void
gtk_widget_class_set_layout_manager_type (GtkWidgetClass *widget_class,
                                          GType           type)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_type_is_a (type, GTK_TYPE_LAYOUT_MANAGER));

  priv = widget_class->priv;

  priv->layout_manager_type = type;
}

/**
 * gtk_widget_class_get_layout_manager_type:
 * @widget_class: a `GtkWidgetClass`
 *
 * Retrieves the type of the [class@Gtk.LayoutManager]
 * used by widgets of class @widget_class.
 *
 * See also: [method@Gtk.WidgetClass.set_layout_manager_type].
 *
 * Returns: type of a `GtkLayoutManager` subclass, or %G_TYPE_INVALID
 */
GType
gtk_widget_class_get_layout_manager_type (GtkWidgetClass *widget_class)
{
  GtkWidgetClassPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), G_TYPE_INVALID);

  priv = widget_class->priv;

  return priv->layout_manager_type;
}

/**
 * gtk_widget_set_layout_manager: (attributes org.gtk.Method.set_property=layout-manager)
 * @widget: a `GtkWidget`
 * @layout_manager: (nullable) (transfer full): a `GtkLayoutManager`
 *
 * Sets the layout manager delegate instance that provides an
 * implementation for measuring and allocating the children of @widget.
 */
void
gtk_widget_set_layout_manager (GtkWidget        *widget,
                               GtkLayoutManager *layout_manager)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (layout_manager == NULL || GTK_IS_LAYOUT_MANAGER (layout_manager));
  g_return_if_fail (layout_manager == NULL || gtk_layout_manager_get_widget (layout_manager) == NULL);

  if (priv->layout_manager == layout_manager)
    return;

  if (priv->layout_manager)
    {
      gtk_layout_manager_set_widget (priv->layout_manager, NULL);
      g_object_unref (priv->layout_manager);
    }

  priv->layout_manager = layout_manager;
  if (priv->layout_manager != NULL)
    gtk_layout_manager_set_widget (priv->layout_manager, widget);

  gtk_widget_queue_resize (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_LAYOUT_MANAGER]);
}

/**
 * gtk_widget_get_layout_manager: (attributes org.gtk.Method.get_property=layout-manager)
 * @widget: a `GtkWidget`
 *
 * Retrieves the layout manager used by @widget.
 *
 * See [method@Gtk.Widget.set_layout_manager].
 *
 * Returns: (transfer none) (nullable): a `GtkLayoutManager`
 */
GtkLayoutManager *
gtk_widget_get_layout_manager (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->layout_manager;
}

/**
 * gtk_widget_should_layout:
 * @widget: a widget
 *
 * Returns whether @widget should contribute to
 * the measuring and allocation of its parent.
 *
 * This is %FALSE for invisible children, but also
 * for children that have their own surface.
 *
 * Returns: %TRUE if child should be included in
 *   measuring and allocating
 */
gboolean
gtk_widget_should_layout (GtkWidget *widget)
{
  if (!_gtk_widget_get_visible (widget))
    return FALSE;

  if (GTK_IS_NATIVE (widget))
    return FALSE;

  return TRUE;
}

static void
gtk_widget_class_add_action (GtkWidgetClass  *widget_class,
                             GtkWidgetAction *action)
{
  GtkWidgetClassPrivate *priv = widget_class->priv;

  GTK_DEBUG (ACTIONS, "%sClass: Adding %s action",
                      g_type_name (G_TYPE_FROM_CLASS (widget_class)),
                      action->name);

  action->next = priv->actions;
  priv->actions = action;
}

/**
 * gtk_widget_class_install_action:
 * @widget_class: a `GtkWidgetClass`
 * @action_name: a prefixed action name, such as "clipboard.paste"
 * @parameter_type: (nullable): the parameter type
 * @activate: (scope notified): callback to use when the action is activated
 *
 * This should be called at class initialization time to specify
 * actions to be added for all instances of this class.
 *
 * Actions installed by this function are stateless. The only state
 * they have is whether they are enabled or not (which can be changed with
 * [method@Gtk.Widget.action_set_enabled]).
 */
void
gtk_widget_class_install_action (GtkWidgetClass              *widget_class,
                                 const char                  *action_name,
                                 const char                  *parameter_type,
                                 GtkWidgetActionActivateFunc  activate)
{
  GtkWidgetAction *action;

  action = g_new0 (GtkWidgetAction, 1);
  action->owner = G_TYPE_FROM_CLASS (widget_class);
  action->name = g_strdup (action_name);
  if (parameter_type)
    action->parameter_type = g_variant_type_new (parameter_type);
  else
    action->parameter_type = NULL;
  action->activate = activate;

  gtk_widget_class_add_action (widget_class, action);
}

static const GVariantType *
determine_type (GParamSpec *pspec)
{
  if (G_TYPE_IS_ENUM (pspec->value_type))
    return G_VARIANT_TYPE_STRING;

  switch (pspec->value_type)
    {
    case G_TYPE_BOOLEAN:
      return G_VARIANT_TYPE_BOOLEAN;

    case G_TYPE_INT:
      return G_VARIANT_TYPE_INT32;

    case G_TYPE_UINT:
      return G_VARIANT_TYPE_UINT32;

    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      return G_VARIANT_TYPE_DOUBLE;

    case G_TYPE_STRING:
      return G_VARIANT_TYPE_STRING;

    default:
      g_critical ("Unable to use gtk_widget_class_install_property_action with property '%s:%s' of type '%s'",
                  g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->value_type));
      return NULL;
    }
}

/**
 * gtk_widget_class_install_property_action:
 * @widget_class: a `GtkWidgetClass`
 * @action_name: name of the action
 * @property_name: name of the property in instances of @widget_class
 *   or any parent class.
 *
 * Installs an action called @action_name on @widget_class and
 * binds its state to the value of the @property_name property.
 *
 * This function will perform a few sanity checks on the property selected
 * via @property_name. Namely, the property must exist, must be readable,
 * writable and must not be construct-only. There are also restrictions
 * on the type of the given property, it must be boolean, int, unsigned int,
 * double or string. If any of these conditions are not met, a critical
 * warning will be printed and no action will be added.
 *
 * The state type of the action matches the property type.
 *
 * If the property is boolean, the action will have no parameter and
 * toggle the property value. Otherwise, the action will have a parameter
 * of the same type as the property.
 */
void
gtk_widget_class_install_property_action (GtkWidgetClass *widget_class,
                                          const char     *action_name,
                                          const char     *property_name)
{
  GParamSpec *pspec;
  GtkWidgetAction *action;
  const GVariantType *state_type;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  pspec = g_object_class_find_property (G_OBJECT_CLASS (widget_class), property_name);

  if (pspec == NULL)
    {
      g_critical ("Attempted to use non-existent property '%s:%s' for gtk_widget_class_install_property_action",
                  g_type_name (G_TYPE_FROM_CLASS (widget_class)), property_name);
      return;
    }

  if (~pspec->flags & G_PARAM_READABLE || ~pspec->flags & G_PARAM_WRITABLE || pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_critical ("Property '%s:%s' used with gtk_widget_class_install_property_action must be readable, writable, and not construct-only",
                  g_type_name (G_TYPE_FROM_CLASS (widget_class)), property_name);
      return;
    }

  state_type = determine_type (pspec);

  if (!state_type)
    return;

  action = g_new0 (GtkWidgetAction, 1);
  action->owner = G_TYPE_FROM_CLASS (widget_class);
  action->name = g_strdup (action_name);
  action->pspec = pspec;
  action->state_type = state_type;
  if (action->pspec->value_type == G_TYPE_BOOLEAN)
    action->parameter_type = NULL;
  else
    action->parameter_type = action->state_type;
  action->activate = NULL;

  gtk_widget_class_add_action (widget_class, action);
}

/**
 * gtk_widget_action_set_enabled:
 * @widget: a `GtkWidget`
 * @action_name: action name, such as "clipboard.paste"
 * @enabled: whether the action is now enabled
 *
 * Enable or disable an action installed with
 * gtk_widget_class_install_action().
 */
void
gtk_widget_action_set_enabled (GtkWidget  *widget,
                               const char *action_name,
                               gboolean    enabled)
{
  GtkActionMuxer *muxer;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);
  gtk_action_muxer_action_enabled_changed (muxer, action_name, enabled);
}

/**
 * gtk_widget_class_query_action:
 * @widget_class: a `GtkWidget` class
 * @index_: position of the action to query
 * @owner: (out) (transfer none): return location for the type where the action was defined
 * @action_name: (out) (transfer none): return location for the action name
 * @parameter_type: (out) (transfer none) (nullable): return location for the parameter type
 * @property_name: (out) (transfer none) (nullable): return location for the property name
 *
 * Returns details about the @index_-th action that has been
 * installed for @widget_class during class initialization.
 *
 * See [method@Gtk.WidgetClass.install_action] for details on
 * how to install actions.
 *
 * Note that this function will also return actions defined
 * by parent classes. You can identify those by looking
 * at @owner.
 *
 * Returns: %TRUE if the action was found, %FALSE if @index_
 *   is out of range
 */
gboolean
gtk_widget_class_query_action (GtkWidgetClass      *widget_class,
                               guint                index_,
                               GType               *owner,
                               const char         **action_name,
                               const GVariantType **parameter_type,
                               const char         **property_name)
{
  GtkWidgetClassPrivate *priv = widget_class->priv;
  GtkWidgetAction *action = priv->actions;

  for (; index_ > 0 && action != NULL; index_--)
    action = action->next;

  if (action != NULL && index_ == 0)
    {
      *owner = action->owner;
      *action_name = action->name;
      *parameter_type = action->parameter_type;
      if (action->pspec)
        *property_name = action->pspec->name;
      else
        *property_name = NULL;

      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_widget_get_css_name: (attributes org.gtk.Method.get_property=css-name)
 * @self: a `GtkWidget`
 *
 * Returns the CSS name that is used for @self.
 *
 * Returns: (transfer none): the CSS name
 */
const char *
gtk_widget_get_css_name (GtkWidget *self)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_WIDGET (self), NULL);

  return g_quark_to_string (gtk_css_node_get_name (priv->cssnode));
}

/**
 * gtk_widget_add_css_class:
 * @widget: a `GtkWidget`
 * @css_class: The style class to add to @widget, without
 *   the leading '.' used for notation of style classes
 *
 * Adds a style class to @widget.
 *
 * After calling this function, the widget’s style will match
 * for @css_class, according to CSS matching rules.
 *
 * Use [method@Gtk.Widget.remove_css_class] to remove the
 * style again.
 */
void
gtk_widget_add_css_class (GtkWidget  *widget,
                          const char *css_class)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (css_class != NULL);
  g_return_if_fail (css_class[0] != '\0');
  g_return_if_fail (css_class[0] != '.');

  gtk_css_node_add_class (priv->cssnode, g_quark_from_string (css_class));
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CSS_CLASSES]);
}

/**
 * gtk_widget_remove_css_class:
 * @widget: a `GtkWidget`
 * @css_class: The style class to remove from @widget, without
 *   the leading '.' used for notation of style classes
 *
 * Removes a style from @widget.
 *
 * After this, the style of @widget will stop matching for @css_class.
 */
void
gtk_widget_remove_css_class (GtkWidget  *widget,
                             const char *css_class)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GQuark class_quark;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (css_class != NULL);
  g_return_if_fail (css_class[0] != '\0');
  g_return_if_fail (css_class[0] != '.');

  class_quark = g_quark_try_string (css_class);
  if (!class_quark)
    return;

  gtk_css_node_remove_class (priv->cssnode, class_quark);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CSS_CLASSES]);
}

/**
 * gtk_widget_has_css_class:
 * @widget: a `GtkWidget`
 * @css_class: A style class, without the leading '.'
 *   used for notation of style classes
 *
 * Returns whether @css_class is currently applied to @widget.
 *
 * Returns: %TRUE if @css_class is currently applied to @widget,
 *   %FALSE otherwise.
 */
gboolean
gtk_widget_has_css_class (GtkWidget  *widget,
                          const char *css_class)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (css_class != NULL, FALSE);
  g_return_val_if_fail (css_class[0] != '\0', FALSE);
  g_return_val_if_fail (css_class[0] != '.', FALSE);

  class_quark = g_quark_try_string (css_class);
  if (!class_quark)
    return FALSE;

  return gtk_css_node_has_class (priv->cssnode, class_quark);
}

/**
 * gtk_widget_get_css_classes: (attributes org.gtk.Method.get_property=css-classes)
 * @widget: a `GtkWidget`
 *
 * Returns the list of style classes applied to @widget.
 *
 * Returns: (transfer full): a %NULL-terminated list of
 *   css classes currently applied to @widget. The returned
 *   list must freed using g_strfreev().
 */
char **
gtk_widget_get_css_classes (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  const GQuark *classes;
  guint n_classes;
  char **strv;
  guint i;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  classes = gtk_css_node_list_classes (priv->cssnode, &n_classes);
  strv = g_new (char *, n_classes + 1);

  for (i = 0; i < n_classes; i++)
    strv[i] = g_strdup (g_quark_to_string (classes[i]));

  strv[n_classes] = NULL;

  return strv;
}

/**
 * gtk_widget_set_css_classes: (attributes org.gtk.Method.set_property=css-classes)
 * @widget: a `GtkWidget`
 * @classes: (transfer none) (array zero-terminated=1):
 *   %NULL-terminated list of style classes to apply to @widget.
 *
 * Clear all style classes applied to @widget
 * and replace them with @classes.
 */
void
gtk_widget_set_css_classes (GtkWidget   *widget,
                            const char **classes)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_css_node_set_classes (priv->cssnode, classes);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CSS_CLASSES]);
}

/**
 * gtk_widget_get_color:
 * @widget: a `GtkWidget`
 * @color: (out): return location for the color
 *
 * Gets the current foreground color for the widget’s
 * CSS style.
 *
 * This function should only be used in snapshot
 * implementations that need to do custom
 * drawing with the foreground color.
 *
 * Since: 4.10
 */
void
gtk_widget_get_color (GtkWidget *widget,
                      GdkRGBA   *color)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = gtk_css_node_get_style (priv->cssnode);
  *color = *gtk_css_color_value_get_rgba (style->core->color);
}

/*< private >
 * gtk_widget_update_orientation:
 * @widget: a `GtkWidget` implementing `GtkOrientable`
 * @orientation: the orientation
 *
 * Update the internal state associated to the given
 * @orientation of a `GtkWidget`.
 */
void
gtk_widget_update_orientation (GtkWidget      *widget,
                               GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_widget_add_css_class (widget, "horizontal");
      gtk_widget_remove_css_class (widget, "vertical");
    }
  else
    {
      gtk_widget_add_css_class (widget, "vertical");
      gtk_widget_remove_css_class (widget, "horizontal");
    }

  gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_PROPERTY_ORIENTATION, orientation,
                                  -1);
}

/**
 * gtk_widget_class_set_accessible_role:
 * @widget_class: a `GtkWidgetClass`
 * @accessible_role: the `GtkAccessibleRole` used by the @widget_class
 *
 * Sets the accessible role used by the given `GtkWidget` class.
 *
 * Different accessible roles have different states, and are
 * rendered differently by assistive technologies.
 */
void
gtk_widget_class_set_accessible_role (GtkWidgetClass    *widget_class,
                                      GtkAccessibleRole  accessible_role)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (!gtk_accessible_role_is_abstract (accessible_role));

  priv = widget_class->priv;
  priv->accessible_role = accessible_role;
}

/**
 * gtk_widget_class_get_accessible_role:
 * @widget_class: a `GtkWidgetClass`
 *
 * Retrieves the accessible role used by the given `GtkWidget` class.
 *
 * Different accessible roles have different states, and are rendered
 * differently by assistive technologies.
 *
 * See also: [method@Gtk.Accessible.get_accessible_role].
 *
 * Returns: the accessible role for the widget class
 */
GtkAccessibleRole
gtk_widget_class_get_accessible_role (GtkWidgetClass *widget_class)
{
  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), GTK_ACCESSIBLE_ROLE_GENERIC);

  return widget_class->priv->accessible_role;
}

void
gtk_widget_set_active_state (GtkWidget *widget,
                             gboolean   active)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (active)
    {
      priv->n_active++;
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_ACTIVE, FALSE);
    }
  else
    {
      if (priv->n_active == 0)
        {
          g_warning ("Broken accounting of active state for widget %p(%s)",
                     widget, G_OBJECT_TYPE_NAME (widget));
        }
      else
        priv->n_active--;

      if (priv->n_active == 0)
        gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_ACTIVE);
    }
}
