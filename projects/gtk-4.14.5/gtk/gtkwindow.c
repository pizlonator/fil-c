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

#include "gtkwindowprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkapplicationprivate.h"
#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkcheckbutton.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkdroptargetasync.h"
#include "gtkeventcontrollerlegacy.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtkheaderbar.h"
#include "gtkicontheme.h"
#include <glib/gi18n-lib.h>
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "deprecated/gtkmessagedialog.h"
#include "gtkpointerfocusprivate.h"
#include "gtkprivate.h"
#include "gtkroot.h"
#include "gtknativeprivate.h"
#include "gtksettings.h"
#include "gtkshortcut.h"
#include "gtkshortcutcontrollerprivate.h"
#include "gtkshortcutmanager.h"
#include "gtkshortcuttrigger.h"
#include "gtksizerequest.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowgroup.h"
#include "gtkpopovermenubarprivate.h"
#include "gtkcssboxesimplprivate.h"
#include "gtktooltipprivate.h"
#include "gtkmenubutton.h"

#include "inspector/window.h"

#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktoplevelprivate.h"

#include <cairo-gobject.h>
#include <errno.h>
#include <graphene.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include "wayland/gdkdisplay-wayland.h"
#include "wayland/gdksurface-wayland.h"
#include "wayland/gdktoplevel-wayland-private.h"
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#define GDK_ARRAY_ELEMENT_TYPE GtkWidget *
#define GDK_ARRAY_TYPE_NAME GtkWidgetStack
#define GDK_ARRAY_NAME gtk_widget_stack
#define GDK_ARRAY_FREE_FUNC g_object_unref
#define GDK_ARRAY_PREALLOC 16
#include "gdk/gdkarrayimpl.c"

/**
 * GtkWindow:
 *
 * A `GtkWindow` is a toplevel window which can contain other widgets.
 *
 * ![An example GtkWindow](window.png)
 *
 * Windows normally have decorations that are under the control
 * of the windowing system and allow the user to manipulate the window
 * (resize it, move it, close it,...).
 *
 * # GtkWindow as GtkBuildable
 *
 * The `GtkWindow` implementation of the [iface@Gtk.Buildable] interface supports
 * setting a child as the titlebar by specifying “titlebar” as the “type”
 * attribute of a `<child>` element.
 *
 * # CSS nodes
 *
 * ```
 * window.background [.csd / .solid-csd / .ssd] [.maximized / .fullscreen / .tiled]
 * ├── <child>
 * ╰── <titlebar child>.titlebar [.default-decoration]
 * ```
 *
 * `GtkWindow` has a main CSS node with name window and style class .background.
 *
 * Style classes that are typically used with the main CSS node are .csd (when
 * client-side decorations are in use), .solid-csd (for client-side decorations
 * without invisible borders), .ssd (used by mutter when rendering server-side
 * decorations). GtkWindow also represents window states with the following
 * style classes on the main node: .maximized, .fullscreen, .tiled (when supported,
 * also .tiled-top, .tiled-left, .tiled-right, .tiled-bottom).
 *
 * `GtkWindow` subclasses often add their own discriminating style classes,
 * such as .dialog, .popup or .tooltip.
 *
 * Generally, some CSS properties don't make sense on the toplevel window node,
 * such as margins or padding. When client-side decorations without invisible
 * borders are in use (i.e. the .solid-csd style class is added to the
 * main window node), the CSS border of the toplevel window is used for
 * resize drags. In the .csd case, the shadow area outside of the window
 * can be used to resize it.
 *
 * `GtkWindow` adds the .titlebar and .default-decoration style classes to the
 * widget that is added as a titlebar child.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkWindow` used the `GTK_ACCESSIBLE_ROLE_WINDOW` role.
 *
 * Since GTK 4.12, `GtkWindow` uses the `GTK_ACCESSIBLE_ROLE_APPLICATION` role.
 *
 * # Actions
 *
 * `GtkWindow` defines a set of built-in actions:
 * - `default.activate`: Activate the default widget.
 * - `window.minimize`: Minimize the window.
 * - `window.toggle-maximized`: Maximize or restore the window.
 * - `window.close`: Close the window.
 */

#define MENU_BAR_ACCEL GDK_KEY_F10
#define RESIZE_HANDLE_SIZE 12 /* Width of resize borders */
#define RESIZE_HANDLE_CORNER_SIZE 24 /* How resize corners extend */
#define MNEMONICS_DELAY 300 /* ms */
#define NO_CONTENT_CHILD_NAT 200 /* ms */
#define VISIBLE_FOCUS_DURATION 3 /* s */


/* In case the content (excluding header bar and shadows) of the window
 * would be empty, either because there is no visible child widget or only an
 * empty container widget, we use NO_CONTENT_CHILD_NAT as natural width/height
 * instead.
 */

typedef struct _GtkWindowGeometryInfo GtkWindowGeometryInfo;

typedef struct
{
  GtkWidget             *child;

  GtkWidget             *default_widget;
  GtkWidget             *focus_widget;
  GtkWidget             *move_focus_widget;
  GtkWindow             *transient_parent;
  GtkWindowGeometryInfo *geometry_info;
  GtkWindowGroup        *group;
  GdkDisplay            *display;
  GtkApplication        *application;

  int default_width;
  int default_height;

  char    *startup_id;
  char    *title;

  guint    keys_changed_handler;

  guint32  initial_timestamp;

  guint    mnemonics_display_timeout_id;

  guint    focus_visible_timeout;

  int      scale;

  int title_height;
  GtkWidget *title_box;
  GtkWidget *titlebar;
  GtkWidget *key_press_focus;

  GdkMonitor *initial_fullscreen_monitor;
  guint      edge_constraints;

  GdkToplevelState state;

  /* The following flags are initially TRUE (before a window is mapped).
   * They cause us to compute a configure request that involves
   * default-only parameters. Once mapped, we set them to FALSE.
   * Then we set them to TRUE again on unmap (for position)
   * and on unrealize (for size).
   */
  guint    need_default_size         : 1;

  guint    decorated                 : 1;
  guint    deletable                 : 1;
  guint    destroy_with_parent       : 1;
  guint    minimize_initially        : 1;
  guint    is_active                 : 1;
  guint    mnemonics_visible         : 1;
  guint    focus_visible             : 1;
  guint    modal                     : 1;
  guint    resizable                 : 1;
  guint    transient_parent_group    : 1;
  guint    csd_requested             : 1;
  guint    client_decorated          : 1; /* Decorations drawn client-side */
  guint    use_client_shadow         : 1; /* Decorations use client-side shadows */
  guint    maximized                 : 1;
  guint    suspended                 : 1;
  guint    fullscreen                : 1;
  guint    tiled                     : 1;

  guint    hide_on_close             : 1;
  guint    in_emit_close_request     : 1;
  guint    move_focus                : 1;
  guint    unset_default             : 1;
  guint    in_present                : 1;

  GtkGesture *click_gesture;
  GtkEventController *application_shortcut_controller;

  GdkSurface  *surface;
  GskRenderer *renderer;

  GList *foci;

  GtkConstraintSolver *constraint_solver;

  int surface_width;
  int surface_height;

  GdkCursor *resize_cursor;

  GtkEventController *menubar_controller;
} GtkWindowPrivate;

enum {
  SET_FOCUS,
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  KEYS_CHANGED,
  ENABLE_DEBUGGING,
  CLOSE_REQUEST,
  LAST_SIGNAL
};

enum {
  PROP_0,

  /* Normal Props */
  PROP_TITLE,
  PROP_RESIZABLE,
  PROP_MODAL,
  PROP_DEFAULT_WIDTH,
  PROP_DEFAULT_HEIGHT,
  PROP_DESTROY_WITH_PARENT,
  PROP_HIDE_ON_CLOSE,
  PROP_ICON_NAME,
  PROP_DISPLAY,
  PROP_DECORATED,
  PROP_DELETABLE,
  PROP_TRANSIENT_FOR,
  PROP_APPLICATION,
  PROP_DEFAULT_WIDGET,
  PROP_FOCUS_WIDGET,
  PROP_CHILD,
  PROP_TITLEBAR,
  PROP_HANDLE_MENUBAR_ACCEL,

  /* Readonly properties */
  PROP_IS_ACTIVE,
  PROP_SUSPENDED,

  /* Writeonly properties */
  PROP_STARTUP_ID,

  PROP_MNEMONICS_VISIBLE,
  PROP_FOCUS_VISIBLE,

  PROP_MAXIMIZED,
  PROP_FULLSCREENED,

  LAST_ARG
};

static GParamSpec *window_props[LAST_ARG] = { NULL, };

/* Must be kept in sync with GdkSurfaceEdge ! */
typedef enum
{
  GTK_WINDOW_REGION_EDGE_NW,
  GTK_WINDOW_REGION_EDGE_N,
  GTK_WINDOW_REGION_EDGE_NE,
  GTK_WINDOW_REGION_EDGE_W,
  GTK_WINDOW_REGION_EDGE_E,
  GTK_WINDOW_REGION_EDGE_SW,
  GTK_WINDOW_REGION_EDGE_S,
  GTK_WINDOW_REGION_EDGE_SE,
  GTK_WINDOW_REGION_CONTENT,
} GtkWindowRegion;

typedef struct
{
  char      *icon_name;
  guint      realized : 1;
  guint      using_default_icon : 1;
  guint      using_themed_icon : 1;
} GtkWindowIconInfo;

typedef struct {
  GdkGeometry    geometry; /* Last set of geometry hints we set */
  GdkSurfaceHints flags;
  GdkRectangle   configure_request;
} GtkWindowLastGeometryInfo;

struct _GtkWindowGeometryInfo
{
  GtkWindowLastGeometryInfo last;
};

static void gtk_window_constructed        (GObject           *object);
static void gtk_window_dispose            (GObject           *object);
static void gtk_window_finalize           (GObject           *object);
static void gtk_window_show               (GtkWidget         *widget);
static void gtk_window_hide               (GtkWidget         *widget);
static void gtk_window_map                (GtkWidget         *widget);
static void gtk_window_unmap              (GtkWidget         *widget);
static void gtk_window_realize            (GtkWidget         *widget);
static void gtk_window_unrealize          (GtkWidget         *widget);
static void gtk_window_size_allocate      (GtkWidget         *widget,
                                           int                width,
                                           int                height,
                                           int                  baseline);
static gboolean gtk_window_close_request  (GtkWindow         *window);
static gboolean gtk_window_handle_focus   (GtkWidget         *widget,
                                           GdkEvent          *event,
                                           double             x,
                                           double             y);
static gboolean gtk_window_key_pressed    (GtkWidget         *widget,
                                           guint              keyval,
                                           guint              keycode,
                                           GdkModifierType    state,
                                           gpointer           data);
static gboolean gtk_window_key_released   (GtkWidget         *widget,
                                           guint              keyval,
                                           guint              keycode,
                                           GdkModifierType    state,
                                           gpointer           data);

static void     surface_state_changed     (GtkWidget          *widget);
static void     surface_size_changed      (GtkWidget          *widget,
                                           int                 width,
                                           int                 height);
static gboolean surface_render            (GdkSurface         *surface,
                                           cairo_region_t     *region,
                                           GtkWidget          *widget);
static gboolean surface_event             (GdkSurface         *surface,
                                           GdkEvent           *event,
                                           GtkWidget          *widget);
static void     after_paint               (GdkFrameClock      *clock,
                                           GtkWindow          *window);

static int gtk_window_focus              (GtkWidget        *widget,
				           GtkDirectionType  direction);
static void gtk_window_move_focus         (GtkWidget         *widget,
                                           GtkDirectionType   dir);

static void gtk_window_real_activate_default (GtkWindow         *window);
static void gtk_window_real_activate_focus   (GtkWindow         *window);
static void gtk_window_keys_changed          (GtkWindow         *window);
static gboolean gtk_window_enable_debugging  (GtkWindow         *window,
                                              gboolean           toggle);
static void gtk_window_unset_transient_for         (GtkWindow  *window);
static void gtk_window_transient_parent_realized   (GtkWidget  *parent,
						    GtkWidget  *window);
static void gtk_window_transient_parent_unrealized (GtkWidget  *parent,
						    GtkWidget  *window);

static GtkWindowGeometryInfo* gtk_window_get_geometry_info         (GtkWindow    *window,
                                                                    gboolean      create);

static void     gtk_window_set_default_size_internal (GtkWindow    *window,
                                                      gboolean      change_width,
                                                      int           width,
                                                      gboolean      change_height,
                                                      int           height);

static void     update_themed_icon                    (GtkWindow    *window);
static GList   *icon_list_from_theme                  (GtkWindow    *window,
						       const char   *name);
static void     gtk_window_realize_icon               (GtkWindow    *window);
static void     gtk_window_unrealize_icon             (GtkWindow    *window);
static void     update_window_actions                 (GtkWindow    *window);
static void     get_shadow_width                      (GtkWindow    *window,
                                                       GtkBorder    *shadow_width);

static gboolean    gtk_window_activate_menubar        (GtkWidget    *widget,
                                                       GVariant     *args,
                                                       gpointer      unused);
#ifdef GDK_WINDOWING_X11
static void        gtk_window_on_theme_variant_changed (GtkSettings *settings,
                                                        GParamSpec  *pspec,
                                                        GtkWindow   *window);
#endif
static void        gtk_window_set_theme_variant         (GtkWindow  *window);

static void gtk_window_activate_default_activate (GtkWidget *widget,
                                                  const char *action_name,
                                                  GVariant *parameter);
static void gtk_window_activate_minimize (GtkWidget  *widget,
                                          const char *action_name,
                                          GVariant   *parameter);
static void gtk_window_activate_toggle_maximized (GtkWidget  *widget,
                                                  const char *name,
                                                  GVariant   *parameter);
static void gtk_window_activate_close (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *parameter);

static void        gtk_window_css_changed               (GtkWidget      *widget,
                                                         GtkCssStyleChange *change);
static void _gtk_window_set_is_active (GtkWindow *window,
			               gboolean   is_active);
static void gtk_window_present_toplevel (GtkWindow *window);
static void gtk_window_update_toplevel (GtkWindow         *window,
                                        GdkToplevelLayout *layout);

static void gtk_window_release_application (GtkWindow *window);

static GListStore  *toplevel_list = NULL;
static guint        window_signals[LAST_SIGNAL] = { 0 };
static char        *default_icon_name = NULL;
static gboolean     disable_startup_notification = FALSE;

static GQuark       quark_gtk_window_icon_info = 0;

static GtkBuildableIface *parent_buildable_iface;

static void gtk_window_set_property (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_window_get_property (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);

/* GtkBuildable */
static void     gtk_window_buildable_interface_init         (GtkBuildableIface  *iface);
static void     gtk_window_buildable_add_child              (GtkBuildable       *buildable,
                                                             GtkBuilder         *builder,
                                                             GObject            *child,
                                                             const char         *type);

static void             gtk_window_shortcut_manager_interface_init      (GtkShortcutManagerInterface *iface);
/* GtkRoot */
static void             gtk_window_root_interface_init (GtkRootInterface *iface);
static void             gtk_window_native_interface_init  (GtkNativeInterface  *iface);

static void             gtk_window_accessible_interface_init (GtkAccessibleInterface *iface);


static void ensure_state_flag_backdrop (GtkWidget *widget);
static void unset_titlebar (GtkWindow *window);

#define INCLUDE_CSD_SIZE 1
#define EXCLUDE_CSD_SIZE -1

static void
gtk_window_update_csd_size (GtkWindow *window,
                            int       *width,
                            int       *height,
                            int        apply);

G_DEFINE_TYPE_WITH_CODE (GtkWindow, gtk_window, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkWindow)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
						gtk_window_accessible_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_window_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
						gtk_window_native_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SHORTCUT_MANAGER,
						gtk_window_shortcut_manager_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ROOT,
						gtk_window_root_interface_init))

static GtkAccessibleInterface *parent_accessible_iface;

static gboolean
gtk_window_accessible_get_platform_state (GtkAccessible              *self,
                                          GtkAccessiblePlatformState  state)
{
  switch (state)
    {
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE:
    case GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED:
      return parent_accessible_iface->get_platform_state (self, state);
    case GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE:
      return gtk_window_is_active (GTK_WINDOW (self));
    default:
      g_assert_not_reached ();
    }
}

static void
gtk_window_accessible_interface_init (GtkAccessibleInterface *iface)
{
  parent_accessible_iface = g_type_interface_peek_parent (iface);
  iface->get_at_context = parent_accessible_iface->get_at_context;
  iface->get_platform_state = gtk_window_accessible_get_platform_state;
}

static void
add_tab_bindings (GtkWidgetClass   *widget_class,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  GtkShortcut *shortcut;

  shortcut = gtk_shortcut_new_with_arguments (
                 gtk_alternative_trigger_new (gtk_keyval_trigger_new (GDK_KEY_Tab, modifiers),
                                              gtk_keyval_trigger_new (GDK_KEY_KP_Tab, modifiers)),
                 gtk_signal_action_new ("move-focus"),
                 "(i)", direction);

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

static void
add_arrow_bindings (GtkWidgetClass   *widget_class,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_widget_class_add_binding_signal (widget_class, keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
}

static guint32
extract_time_from_startup_id (const char * startup_id)
{
  char *timestr = g_strrstr (startup_id, "_TIME");
  guint32 retval = GDK_CURRENT_TIME;

  if (timestr)
    {
      char *end;
      guint32 timestamp;

      /* Skip past the "_TIME" part */
      timestr += 5;

      end = NULL;
      errno = 0;
      timestamp = g_ascii_strtoull (timestr, &end, 0);
      if (errno == 0 && end != timestr)
        retval = timestamp;
    }

  return retval;
}

static gboolean
startup_id_is_fake (const char * startup_id)
{
  return strncmp (startup_id, "_TIME", 5) == 0;
}

static void
gtk_window_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child = priv->child;
  gboolean has_size_request = gtk_widget_has_size_request (widget);
  int title_for_size = for_size;
  int title_min_size = 0;
  int title_nat_size = 0;
  int child_for_size = for_size;
  int child_min_size = 0;
  int child_nat_size = 0;

  if (priv->decorated && !priv->fullscreen)
    {
      if (priv->title_box != NULL &&
          gtk_widget_get_visible (priv->title_box) &&
          gtk_widget_get_child_visible (priv->title_box))
        {
          if (orientation == GTK_ORIENTATION_HORIZONTAL && for_size >= 0 &&
              child != NULL && gtk_widget_get_visible (child))
            {
              GtkRequestedSize sizes[2];

              gtk_widget_measure (priv->title_box,
                                  GTK_ORIENTATION_VERTICAL,
                                  -1,
                                  &sizes[0].minimum_size, &sizes[0].natural_size,
                                  NULL, NULL);
              gtk_widget_measure (child,
                                  GTK_ORIENTATION_VERTICAL,
                                  -1,
                                  &sizes[1].minimum_size, &sizes[1].natural_size,
                                  NULL, NULL);
              for_size -= sizes[0].minimum_size + sizes[1].minimum_size;
              for_size = gtk_distribute_natural_allocation (for_size, 2, sizes);
              title_for_size = sizes[0].minimum_size;
              child_for_size = sizes[1].minimum_size + for_size;
            }

          gtk_widget_measure (priv->title_box,
                              orientation,
                              title_for_size,
                              &title_min_size, &title_nat_size,
                              NULL, NULL);
        }
    }

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gtk_widget_measure (child,
                          orientation,
                          child_for_size,
                          &child_min_size, &child_nat_size,
                          NULL, NULL);

      if (child_nat_size == 0 && !has_size_request)
        child_nat_size = NO_CONTENT_CHILD_NAT;
    }
  else if (!has_size_request)
    {
      child_nat_size = NO_CONTENT_CHILD_NAT;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = MAX (title_min_size, child_min_size);
      *natural = MAX (title_nat_size, child_nat_size);
    }
  else
    {
      *minimum = title_min_size + child_min_size;
      *natural = title_nat_size + child_nat_size;
    }
}

static void
gtk_window_compute_expand (GtkWidget *widget,
                           gboolean  *hexpand,
                           gboolean  *vexpand)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->child)
    {
      *hexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_window_get_request_mode (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->child)
    return gtk_widget_get_request_mode (priv->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_window_class_init (GtkWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  quark_gtk_window_icon_info = g_quark_from_static_string ("gtk-window-icon-info");

  if (toplevel_list == NULL)
    toplevel_list = g_list_store_new (GTK_TYPE_WIDGET);

  gobject_class->constructed = gtk_window_constructed;
  gobject_class->dispose = gtk_window_dispose;
  gobject_class->finalize = gtk_window_finalize;

  gobject_class->set_property = gtk_window_set_property;
  gobject_class->get_property = gtk_window_get_property;

  widget_class->show = gtk_window_show;
  widget_class->hide = gtk_window_hide;
  widget_class->map = gtk_window_map;
  widget_class->unmap = gtk_window_unmap;
  widget_class->realize = gtk_window_realize;
  widget_class->unrealize = gtk_window_unrealize;
  widget_class->size_allocate = gtk_window_size_allocate;
  widget_class->compute_expand = gtk_window_compute_expand;
  widget_class->get_request_mode = gtk_window_get_request_mode;
  widget_class->focus = gtk_window_focus;
  widget_class->move_focus = gtk_window_move_focus;
  widget_class->measure = gtk_window_measure;
  widget_class->css_changed = gtk_window_css_changed;

  klass->activate_default = gtk_window_real_activate_default;
  klass->activate_focus = gtk_window_real_activate_focus;
  klass->keys_changed = gtk_window_keys_changed;
  klass->enable_debugging = gtk_window_enable_debugging;
  klass->close_request = gtk_window_close_request;

  /**
   * GtkWindow:title: (attributes org.gtk.Property.get=gtk_window_get_title org.gtk.Property.set=gtk_window_set_title)
   *
   * The title of the window.
   */
  window_props[PROP_TITLE] =
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkWindow:startup-id: (attributes org.gtk.Property.set=gtk_window_set_startup_id)
   *
   * A write-only property for setting window's startup notification identifier.
   */
  window_props[PROP_STARTUP_ID] =
      g_param_spec_string ("startup-id", NULL, NULL,
                           NULL,
                           GTK_PARAM_WRITABLE);

  /**
   * GtkWindow:resizable: (attributes org.gtk.Property.get=gtk_window_get_resizable org.gtk.Property.set=gtk_window_set_resizable)
   *
   * If %TRUE, users can resize the window.
   */
  window_props[PROP_RESIZABLE] =
      g_param_spec_boolean ("resizable", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:modal: (attributes org.gtk.Property.get=gtk_window_get_modal org.gtk.Property.set=gtk_window_set_modal)
   *
   * If %TRUE, the window is modal.
   */
  window_props[PROP_MODAL] =
      g_param_spec_boolean ("modal", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:default-width:
   *
   * The default width of the window.
   */
  window_props[PROP_DEFAULT_WIDTH] =
      g_param_spec_int ("default-width", NULL, NULL,
                        -1, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:default-height:
   *
   * The default height of the window.
   */
  window_props[PROP_DEFAULT_HEIGHT] =
      g_param_spec_int ("default-height", NULL, NULL,
                        -1, G_MAXINT,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:destroy-with-parent: (attributes org.gtk.Property.get=gtk_window_get_destroy_with_parent org.gtk.Property.set=gtk_window_set_destroy_with_parent)
   *
   * If this window should be destroyed when the parent is destroyed.
   */
  window_props[PROP_DESTROY_WITH_PARENT] =
      g_param_spec_boolean ("destroy-with-parent", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:hide-on-close: (attributes org.gtk.Property.get=gtk_window_get_hide_on_close org.gtk.Property.set=gtk_window_set_hide_on_close)
   *
   * If this window should be hidden when the users clicks the close button.
   */
  window_props[PROP_HIDE_ON_CLOSE] =
      g_param_spec_boolean ("hide-on-close", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:mnemonics-visible: (attributes org.gtk.Property.get=gtk_window_get_mnemonics_visible org.gtk.Property.set=gtk_window_set_mnemonics_visible)
   *
   * Whether mnemonics are currently visible in this window.
   *
   * This property is maintained by GTK based on user input,
   * and should not be set by applications.
   */
  window_props[PROP_MNEMONICS_VISIBLE] =
      g_param_spec_boolean ("mnemonics-visible", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:focus-visible: (attributes org.gtk.Property.get=gtk_window_get_focus_visible org.gtk.Property.set=gtk_window_set_focus_visible)
   *
   * Whether 'focus rectangles' are currently visible in this window.
   *
   * This property is maintained by GTK based on user input
   * and should not be set by applications.
   */
  window_props[PROP_FOCUS_VISIBLE] =
      g_param_spec_boolean ("focus-visible", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:icon-name: (attributes org.gtk.Property.get=gtk_window_get_icon_name org.gtk.Property.set=gtk_window_set_icon_name)
   *
   * Specifies the name of the themed icon to use as the window icon.
   *
   * See [class@Gtk.IconTheme] for more details.
   */
  window_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:display: (attributes org.gtk.Property.set=gtk_window_set_display)
   *
   * The display that will display this window.
   */
  window_props[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:is-active: (attributes org.gtk.Property.get=gtk_window_is_active)
   *
   * Whether the toplevel is the currently active window.
   */
  window_props[PROP_IS_ACTIVE] =
      g_param_spec_boolean ("is-active", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE);

  /**
   * GtkWindow:decorated: (attributes org.gtk.Property.get=gtk_window_get_decorated org.gtk.Property.set=gtk_window_set_decorated)
   *
   * Whether the window should have a frame (also known as *decorations*).
   */
  window_props[PROP_DECORATED] =
      g_param_spec_boolean ("decorated", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:deletable: (attributes org.gtk.Property.get=gtk_window_get_deletable org.gtk.Property.set=gtk_window_set_deletable)
   *
   * Whether the window frame should have a close button.
   */
  window_props[PROP_DELETABLE] =
      g_param_spec_boolean ("deletable", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:transient-for: (attributes org.gtk.Property.get=gtk_window_get_transient_for org.gtk.Property.set=gtk_window_set_transient_for)
   *
   * The transient parent of the window.
   */
  window_props[PROP_TRANSIENT_FOR] =
      g_param_spec_object ("transient-for", NULL, NULL,
                           GTK_TYPE_WINDOW,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:maximized: (attributes org.gtk.Property.get=gtk_window_is_maximized)
   *
   * Whether the window is maximized.
   *
   * Setting this property is the equivalent of calling
   * [method@Gtk.Window.maximize] or [method@Gtk.Window.unmaximize];
   * either operation is asynchronous, which means you will need to
   * connect to the ::notify signal in order to know whether the
   * operation was successful.
   */
  window_props[PROP_MAXIMIZED] =
      g_param_spec_boolean ("maximized", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:fullscreened: (attributes org.gtk.Property.get=gtk_window_is_fullscreen)
   *
   * Whether the window is fullscreen.
   *
   * Setting this property is the equivalent of calling
   * [method@Gtk.Window.fullscreen] or [method@Gtk.Window.unfullscreen];
   * either operation is asynchronous, which means you will need to
   * connect to the ::notify signal in order to know whether the
   * operation was successful.
   */
  window_props[PROP_FULLSCREENED] =
      g_param_spec_boolean ("fullscreened", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:suspended: (attributes org.gtk.Property.get=gtk_window_is_suspended)
   *
   * Whether the window is suspended.
   *
   * See [method@Gtk.Window.is_suspended] for details about what suspended means.
   *
   * Since: 4.12
   */
  window_props[PROP_SUSPENDED] =
      g_param_spec_boolean ("suspended", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:application: (attributes org.gtk.Property.get=gtk_window_get_application org.gtk.Property.set=gtk_window_set_application)
   *
   * The `GtkApplication` associated with the window.
   *
   * The application will be kept alive for at least as long as it
   * has any windows associated with it (see g_application_hold()
   * for a way to keep it alive without windows).
   *
   * Normally, the connection between the application and the window
   * will remain until the window is destroyed, but you can explicitly
   * remove it by setting the :application property to %NULL.
   */
  window_props[PROP_APPLICATION] =
      g_param_spec_object ("application", NULL, NULL,
                           GTK_TYPE_APPLICATION,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:default-widget: (attributes org.gtk.Property.get=gtk_window_get_default_widget org.gtk.Property.set=gtk_window_set_default_widget)
   *
   * The default widget.
   */
  window_props[PROP_DEFAULT_WIDGET] =
      g_param_spec_object ("default-widget", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:focus-widget: (attributes org.gtk.Property.get=gtk_window_get_focus org.gtk.Property.set=gtk_window_set_focus)
   *
   * The focus widget.
   */
  window_props[PROP_FOCUS_WIDGET] =
      g_param_spec_object ("focus-widget", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:child: (attributes org.gtk.Property.get=gtk_window_get_child org.gtk.Property.set=gtk_window_set_child)
   *
   * The child widget.
   */
  window_props[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:titlebar: (attributes org.gtk.Property.get=gtk_window_get_titlebar org.gtk.Property.set=gtk_window_set_titlebar)
   *
   * The titlebar widget.
   *
   * Since: 4.6
   */
  window_props[PROP_TITLEBAR] =
      g_param_spec_object ("titlebar", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindow:handle-menubar-accel: (attributes org.gtk.Property.get=gtk_window_get_handle_menubar_accel org.gtk.Property.set=gtk_window_set_handle_menubar_accel)
   *
   * Whether the window frame should handle F10 for activating
   * menubars.
   *
   * Since: 4.2
   */
  window_props[PROP_HANDLE_MENUBAR_ACCEL] =
      g_param_spec_boolean ("handle-menubar-accel", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_ARG, window_props);

  /**
   * GtkWindow::activate-focus:
   * @window: the window which received the signal
   *
   * Emitted when the user activates the currently focused
   * widget of @window.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   */
  window_signals[ACTIVATE_FOCUS] =
    g_signal_new (I_("activate-focus"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, activate_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::activate-default:
   * @window: the window which received the signal
   *
   * Emitted when the user activates the default widget
   * of @window.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   */
  window_signals[ACTIVATE_DEFAULT] =
    g_signal_new (I_("activate-default"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, activate_default),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::keys-changed:
   * @window: the window which received the signal
   *
   * emitted when the set of accelerators or mnemonics that
   * are associated with @window changes.
   *
   * Deprecated: 4.10: Use [class@Gtk.Shortcut] and [class@Gtk.EventController]
   * to implement keyboard shortcuts
   */
  window_signals[KEYS_CHANGED] =
    g_signal_new (I_("keys-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DEPRECATED,
                  G_STRUCT_OFFSET (GtkWindowClass, keys_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkWindow::enable-debugging:
   * @window: the window on which the signal is emitted
   * @toggle: toggle the debugger
   *
   * Emitted when the user enables or disables interactive debugging.
   *
   * When @toggle is %TRUE, interactive debugging is toggled on or off,
   * when it is %FALSE, the debugger will be pointed at the widget
   * under the pointer.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are Ctrl-Shift-I
   * and Ctrl-Shift-D.
   *
   * Return: %TRUE if the key binding was handled
   */
  window_signals[ENABLE_DEBUGGING] =
    g_signal_new (I_("enable-debugging"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWindowClass, enable_debugging),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN,
                  1, G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (window_signals[ENABLE_DEBUGGING],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  /**
   * GtkWindow::close-request:
   * @window: the window on which the signal is emitted
   *
   * Emitted when the user clicks on the close button of the window.
   *
   * Return: %TRUE to stop other handlers from being invoked for the signal
   */
  window_signals[CLOSE_REQUEST] =
    g_signal_new (I_("close-request"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWindowClass, close_request),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN,
                  0);
  g_signal_set_va_marshaller (window_signals[CLOSE_REQUEST],
                              GTK_TYPE_WINDOW,
                              _gtk_marshal_BOOLEAN__VOIDv);


  /*
   * Key bindings
   */

  /**
   * GtkWindow|default.activate:
   *
   * Activate the default widget.
   */
  gtk_widget_class_install_action (widget_class, "default.activate", NULL,
                                   gtk_window_activate_default_activate);

  /**
   * GtkWindow|window.minimize:
   *
   * Minimize the window.
   */
  gtk_widget_class_install_action (widget_class, "window.minimize", NULL,
                                   gtk_window_activate_minimize);

  /**
   * GtkWindow|window.toggle-maximized:
   *
   * Maximize or restore the window.
   */
  gtk_widget_class_install_action (widget_class, "window.toggle-maximized", NULL,
                                   gtk_window_activate_toggle_maximized);

  /**
   * GtkWindow|window.close:
   *
   * Close the window.
   */
  gtk_widget_class_install_action (widget_class, "window.close", NULL,
                                   gtk_window_activate_close);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0,
                                       "activate-focus", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Space, 0,
                                       "activate-focus", NULL);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
                                       "activate-default", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
                                       "activate-default", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
                                       "activate-default", NULL);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_I, GDK_CONTROL_MASK|GDK_SHIFT_MASK,
                                       "enable-debugging", "(b)", FALSE);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_D, GDK_CONTROL_MASK|GDK_SHIFT_MASK,
                                       "enable-debugging", "(b)", TRUE);

  add_arrow_bindings (widget_class, GDK_KEY_Up, GTK_DIR_UP);
  add_arrow_bindings (widget_class, GDK_KEY_Down, GTK_DIR_DOWN);
  add_arrow_bindings (widget_class, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (widget_class, GDK_KEY_Right, GTK_DIR_RIGHT);

  add_tab_bindings (widget_class, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (widget_class, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_css_name (widget_class, I_("window"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_APPLICATION);
}

/**
 * gtk_window_is_maximized: (attributes org.gtk.Method.get_property=maximized)
 * @window: a `GtkWindow`
 *
 * Retrieves the current maximized state of @window.
 *
 * Note that since maximization is ultimately handled by the window
 * manager and happens asynchronously to an application request, you
 * shouldn’t assume the return value of this function changing
 * immediately (or at all), as an effect of calling
 * [method@Gtk.Window.maximize] or [method@Gtk.Window.unmaximize].
 *
 * If the window isn't yet mapped, the value returned will whether the
 * initial requested state is maximized.
 *
 * Returns: whether the window has a maximized state.
 */
gboolean
gtk_window_is_maximized (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->maximized;
}

/**
 * gtk_window_is_fullscreen: (attributes org.gtk.Property.get=fullscreened)
 * @window: a `GtkWindow`
 *
 * Retrieves the current fullscreen state of @window.
 *
 * Note that since fullscreening is ultimately handled by the window
 * manager and happens asynchronously to an application request, you
 * shouldn’t assume the return value of this function changing
 * immediately (or at all), as an effect of calling
 * [method@Gtk.Window.fullscreen] or [method@Gtk.Window.unfullscreen].
 *
 * If the window isn't yet mapped, the value returned will whether the
 * initial requested state is fullscreen.
 *
 * Returns: whether the window has a fullscreen state.
 */
gboolean
gtk_window_is_fullscreen (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->fullscreen;
}

/**
 * gtk_window_is_suspended: (attributes org.gtk.Property.get=suspended)
 * @window: a `GtkWindow`
 *
 * Retrieves the current suspended state of @window.
 *
 * A window being suspended means it's currently not visible to the user, for
 * example by being on a inactive workspace, minimized, obstructed.
 *
 * Returns: whether the window is suspended.
 *
 * Since: 4.12
 */
gboolean
gtk_window_is_suspended (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->suspended;
}

void
_gtk_window_toggle_maximized (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->maximized)
    gtk_window_unmaximize (window);
  else
    gtk_window_maximize (window);
}

/**
 * gtk_window_close:
 * @window: a `GtkWindow`
 *
 * Requests that the window is closed.
 *
 * This is similar to what happens when a window manager
 * close button is clicked.
 *
 * This function can be used with close buttons in custom
 * titlebars.
 */
void
gtk_window_close (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (!_gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (priv->in_emit_close_request)
    return;

  g_object_ref (window);

  if (!gtk_window_emit_close_request (window))
    gtk_window_destroy (window);

  g_object_unref (window);
}

static guint
constraints_for_edge (GdkSurfaceEdge edge)
{
  switch (edge)
    {
    case GDK_SURFACE_EDGE_NORTH_WEST:
      return GDK_TOPLEVEL_STATE_LEFT_RESIZABLE | GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
    case GDK_SURFACE_EDGE_NORTH:
      return GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
    case GDK_SURFACE_EDGE_NORTH_EAST:
      return GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE | GDK_TOPLEVEL_STATE_TOP_RESIZABLE;
    case GDK_SURFACE_EDGE_WEST:
      return GDK_TOPLEVEL_STATE_LEFT_RESIZABLE;
    case GDK_SURFACE_EDGE_EAST:
      return GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE;
    case GDK_SURFACE_EDGE_SOUTH_WEST:
      return GDK_TOPLEVEL_STATE_LEFT_RESIZABLE | GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
    case GDK_SURFACE_EDGE_SOUTH:
      return GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
    case GDK_SURFACE_EDGE_SOUTH_EAST:
      return GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE | GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE;
    default:
      g_warn_if_reached ();
      return 0;
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
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style->border->border_top_width) + get_number (style->size->padding_top);
  border->left = get_number (style->border->border_left_width) + get_number (style->size->padding_left);
  border->bottom = get_number (style->border->border_bottom_width) + get_number (style->size->padding_bottom);
  border->right = get_number (style->border->border_right_width) + get_number (style->size->padding_right);
}

static int
get_edge_for_coordinates (GtkWindow *window,
                          double     x,
                          double     y)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean supports_edge_constraints;
  GtkBorder handle_size;
  GtkCssBoxes css_boxes;
  const graphene_rect_t *border_rect;
  float left, top;

#define edge_or_minus_one(edge) ((supports_edge_constraints && (priv->edge_constraints & constraints_for_edge (edge)) != constraints_for_edge (edge)) ? -1 : edge)

  if (!priv->client_decorated ||
      !priv->resizable ||
      priv->fullscreen ||
      priv->maximized)
    return -1;

  supports_edge_constraints = gdk_toplevel_supports_edge_constraints (GDK_TOPLEVEL (priv->surface));

  if (!supports_edge_constraints && priv->tiled)
    return -1;

  gtk_css_boxes_init (&css_boxes, GTK_WIDGET (window));
  border_rect = gtk_css_boxes_get_content_rect (&css_boxes);

  get_box_border (gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (window))),
                  &handle_size);

  if (priv->use_client_shadow)
    {
      /* We use a maximum of RESIZE_HANDLE_SIZE pixels for the handle size */
      GtkBorder shadow;

      get_shadow_width (window, &shadow);
      /* This logic is duplicated in update_realized_window_properties() */
      handle_size.left += shadow.left;
      handle_size.top += shadow.top;
      handle_size.right += shadow.right;
      handle_size.bottom += shadow.bottom;
    }

  left = border_rect->origin.x;
  top = border_rect->origin.y;

  if (x < left && x >= left - handle_size.left)
    {
      if (y < top + RESIZE_HANDLE_CORNER_SIZE && y >= top - handle_size.top)
        return edge_or_minus_one (GDK_SURFACE_EDGE_NORTH_WEST);

      if (y > top + border_rect->size.height - RESIZE_HANDLE_CORNER_SIZE &&
          y <= top + border_rect->size.height + handle_size.bottom)
        return edge_or_minus_one (GDK_SURFACE_EDGE_SOUTH_WEST);

      return edge_or_minus_one (GDK_SURFACE_EDGE_WEST);
    }
  else if (x > left + border_rect->size.width &&
           x <= left + border_rect->size.width + handle_size.right)
    {
      if (y < top + RESIZE_HANDLE_CORNER_SIZE && y >= top - handle_size.top)
        return edge_or_minus_one (GDK_SURFACE_EDGE_NORTH_EAST);

      if (y > top + border_rect->size.height - RESIZE_HANDLE_CORNER_SIZE &&
          y <= top + border_rect->size.height + handle_size.bottom)
        return edge_or_minus_one (GDK_SURFACE_EDGE_SOUTH_EAST);

      return edge_or_minus_one (GDK_SURFACE_EDGE_EAST);
    }
  else if (y < top && y >= top - handle_size.top)
    {
      if (x < left + RESIZE_HANDLE_CORNER_SIZE && x >= left - handle_size.left)
        return edge_or_minus_one (GDK_SURFACE_EDGE_NORTH_WEST);

      if (x > left + border_rect->size.width - RESIZE_HANDLE_CORNER_SIZE &&
          x <= left + border_rect->size.width + handle_size.right)
        return edge_or_minus_one (GDK_SURFACE_EDGE_NORTH_EAST);

      return edge_or_minus_one (GDK_SURFACE_EDGE_NORTH);
    }
  else if (y > top + border_rect->size.height &&
           y <= top + border_rect->size.height + handle_size.bottom)
    {
      if (x < left + RESIZE_HANDLE_CORNER_SIZE && x >= left - handle_size.left)
        return edge_or_minus_one (GDK_SURFACE_EDGE_SOUTH_WEST);

      if (x > left + border_rect->size.width - RESIZE_HANDLE_CORNER_SIZE &&
          x <= left + border_rect->size.width + handle_size.right)
        return edge_or_minus_one (GDK_SURFACE_EDGE_SOUTH_EAST);

      return edge_or_minus_one (GDK_SURFACE_EDGE_SOUTH);
    }

  return -1;
}

static void
click_gesture_pressed_cb (GtkGestureClick *gesture,
                          int              n_press,
                          double           x,
                          double           y,
                          GtkWindow       *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkEventSequence *sequence;
  GtkWindowRegion region;
  GdkEvent *event;
  GdkDevice *device;
  guint button;
  double tx, ty;
  int edge;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  device = gtk_gesture_get_device (GTK_GESTURE (gesture));

  if (!event)
    return;

  if (button != GDK_BUTTON_PRIMARY)
    return;

  if (priv->maximized)
    return;

  if (gdk_display_device_is_grabbed (gtk_widget_get_display (GTK_WIDGET (window)), device))
    return;

  if (!priv->client_decorated)
    return;

  edge = get_edge_for_coordinates (window, x, y);

  if (edge == -1)
    return;

  region = (GtkWindowRegion)edge;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  gdk_event_get_position (event, &tx, &ty);
  gdk_toplevel_begin_resize (GDK_TOPLEVEL (priv->surface),
                             (GdkSurfaceEdge) region,
                             device,
                             GDK_BUTTON_PRIMARY,
                             tx, ty,
                             gdk_event_get_time (event));

  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
device_removed_cb (GdkSeat   *seat,
                   GdkDevice *device,
                   gpointer   user_data)
{
  GtkWindow *window = user_data;
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l = priv->foci;

  while (l)
    {
      GList *next;
      GtkPointerFocus *focus = l->data;

      next = l->next;

      if (focus->device == device)
        {
          priv->foci = g_list_delete_link (priv->foci, l);
          gtk_pointer_focus_unref (focus);
        }

      l = next;
    }
}

static void
gtk_window_capture_motion (GtkWidget *widget,
                           double     x,
                           double     y)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  const char *cursor_names[8] = {
    "nw-resize", "n-resize", "ne-resize",
    "w-resize",               "e-resize",
    "sw-resize", "s-resize", "se-resize"
  };
  int edge;

  edge = get_edge_for_coordinates (window, x, y);
  if (edge != -1 &&
      priv->resize_cursor &&
      strcmp (gdk_cursor_get_name (priv->resize_cursor), cursor_names[edge]) == 0)
    return;

  g_clear_object (&priv->resize_cursor);

  if (edge != -1)
    priv->resize_cursor = gdk_cursor_new_from_name (cursor_names[edge], NULL);

  gtk_window_maybe_update_cursor (window, widget, NULL);
}

static void
gtk_window_capture_leave (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_clear_object (&priv->resize_cursor);
}

static void
gtk_window_activate_default_activate (GtkWidget  *widget,
                                      const char *name,
                                      GVariant   *parameter)
{
  gtk_window_real_activate_default (GTK_WINDOW (widget));
}

static void
gtk_window_activate_minimize (GtkWidget  *widget,
                              const char *name,
                              GVariant   *parameter)
{
  gtk_window_minimize (GTK_WINDOW (widget));
}

static void
gtk_window_activate_toggle_maximized (GtkWidget  *widget,
                                      const char *name,
                                      GVariant   *parameter)
{
  _gtk_window_toggle_maximized (GTK_WINDOW (widget));
}

static void
gtk_window_activate_close (GtkWidget  *widget,
                           const char *name,
                           GVariant   *parameter)
{
  gtk_window_close (GTK_WINDOW (widget));
}

static gboolean
gtk_window_accept_rootwindow_drop (GtkDropTargetAsync *self,
                                   GdkDrop            *drop,
                                   double              x,
                                   double              y,
                                   gpointer            unused)
{
  gdk_drop_finish (drop, GDK_ACTION_MOVE);

  return TRUE;
}

static void
gtk_window_init (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget;
  GdkSeat *seat;
  GtkEventController *controller;
  GtkDropTargetAsync *target;
  GtkShortcut *shortcut;

  widget = GTK_WIDGET (window);

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  priv->title = NULL;
  priv->geometry_info = NULL;
  priv->focus_widget = NULL;
  priv->default_widget = NULL;
  priv->resizable = TRUE;
  priv->need_default_size = TRUE;
  priv->modal = FALSE;
  priv->decorated = TRUE;
  priv->display = gdk_display_get_default ();

  priv->state = 0;

  priv->deletable = TRUE;
  priv->startup_id = NULL;
  priv->initial_timestamp = GDK_CURRENT_TIME;
  priv->mnemonics_visible = FALSE;
  priv->focus_visible = TRUE;
  priv->initial_fullscreen_monitor = NULL;

  g_object_ref_sink (window);

#ifdef GDK_WINDOWING_X11
  g_signal_connect (gtk_settings_get_for_display (priv->display),
                    "notify::gtk-application-prefer-dark-theme",
                    G_CALLBACK (gtk_window_on_theme_variant_changed), window);
#endif

  gtk_widget_add_css_class (widget, "background");

  priv->scale = gtk_widget_get_scale_factor (widget);

  target = gtk_drop_target_async_new (gdk_content_formats_new ((const char*[1]) { "application/x-rootwindow-drop" }, 1),
                                      GDK_ACTION_MOVE);
  g_signal_connect (target, "drop", G_CALLBACK (gtk_window_accept_rootwindow_drop), NULL);
  gtk_widget_add_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (target));

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  if (seat)
    g_signal_connect (seat, "device-removed",
                      G_CALLBACK (device_removed_cb), window);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_phase (controller,
                                              GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (controller, "motion",
                            G_CALLBACK (gtk_window_capture_motion), window);
  g_signal_connect_swapped (controller, "leave",
                            G_CALLBACK (gtk_window_capture_leave), window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (controller, "key-pressed",
                            G_CALLBACK (gtk_window_key_pressed), window);
  g_signal_connect_swapped (controller, "key-released",
                            G_CALLBACK (gtk_window_key_released), window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_event_controller_legacy_new ();
  gtk_event_controller_set_static_name (controller, "gtk-window-toplevel-focus");
  g_signal_connect_swapped (controller, "event",
                            G_CALLBACK (gtk_window_handle_focus), window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_shortcut_controller_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (MENU_BAR_ACCEL, 0),
                               gtk_callback_action_new (gtk_window_activate_menubar, NULL, NULL));
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_event_controller_set_static_name (controller, "gtk-window-menubar-accel");
  gtk_widget_add_controller (widget, controller);

  priv->menubar_controller = controller;
}

static void
gtk_window_constructed (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  G_OBJECT_CLASS (gtk_window_parent_class)->constructed (object);

  priv->click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->click_gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->click_gesture),
                                              GTK_PHASE_BUBBLE);
  g_signal_connect (priv->click_gesture, "pressed",
                    G_CALLBACK (click_gesture_pressed_cb), object);
  gtk_widget_add_controller (GTK_WIDGET (object), GTK_EVENT_CONTROLLER (priv->click_gesture));

  g_list_store_append (toplevel_list, window);

  gtk_accessible_update_state (GTK_ACCESSIBLE (window),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                               -1);

  g_object_unref (window);
}

static void
gtk_window_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window = GTK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_window_set_title (window, g_value_get_string (value));
      break;
    case PROP_STARTUP_ID:
      gtk_window_set_startup_id (window, g_value_get_string (value));
      break;
    case PROP_RESIZABLE:
      gtk_window_set_resizable (window, g_value_get_boolean (value));
      break;
    case PROP_MODAL:
      gtk_window_set_modal (window, g_value_get_boolean (value));
      break;
    case PROP_DEFAULT_WIDTH:
      gtk_window_set_default_size_internal (window,
                                            TRUE, g_value_get_int (value),
                                            FALSE, -1);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_DEFAULT_HEIGHT:
      gtk_window_set_default_size_internal (window,
                                            FALSE, -1,
                                            TRUE, g_value_get_int (value));
      gtk_widget_queue_resize (GTK_WIDGET (window));
      break;
    case PROP_DESTROY_WITH_PARENT:
      gtk_window_set_destroy_with_parent (window, g_value_get_boolean (value));
      break;
    case PROP_HIDE_ON_CLOSE:
      gtk_window_set_hide_on_close (window, g_value_get_boolean (value));
      break;
    case PROP_ICON_NAME:
      gtk_window_set_icon_name (window, g_value_get_string (value));
      break;
    case PROP_DISPLAY:
      gtk_window_set_display (window, g_value_get_object (value));
      break;
    case PROP_DECORATED:
      gtk_window_set_decorated (window, g_value_get_boolean (value));
      break;
    case PROP_DELETABLE:
      gtk_window_set_deletable (window, g_value_get_boolean (value));
      break;
    case PROP_TRANSIENT_FOR:
      gtk_window_set_transient_for (window, g_value_get_object (value));
      break;
    case PROP_APPLICATION:
      gtk_window_set_application (window, g_value_get_object (value));
      break;
    case PROP_DEFAULT_WIDGET:
      gtk_window_set_default_widget (window, g_value_get_object (value));
      break;
    case PROP_MNEMONICS_VISIBLE:
      gtk_window_set_mnemonics_visible (window, g_value_get_boolean (value));
      break;
    case PROP_FOCUS_VISIBLE:
      gtk_window_set_focus_visible (window, g_value_get_boolean (value));
      break;
    case PROP_MAXIMIZED:
      if (g_value_get_boolean (value))
        gtk_window_maximize (window);
      else
        gtk_window_unmaximize (window);
      break;
    case PROP_FULLSCREENED:
      if (g_value_get_boolean (value))
        gtk_window_fullscreen (window);
      else
        gtk_window_unfullscreen (window);
      break;
    case PROP_FOCUS_WIDGET:
      gtk_window_set_focus (window, g_value_get_object (value));
      break;
    case PROP_CHILD:
      gtk_window_set_child (window, g_value_get_object (value));
      break;
    case PROP_TITLEBAR:
      gtk_window_set_titlebar (window, g_value_get_object (value));
      break;
    case PROP_HANDLE_MENUBAR_ACCEL:
      gtk_window_set_handle_menubar_accel (window, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_get_property (GObject      *object,
			 guint         prop_id,
			 GValue       *value,
			 GParamSpec   *pspec)
{
  GtkWindow  *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_RESIZABLE:
      g_value_set_boolean (value, priv->resizable);
      break;
    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
      break;
    case PROP_DEFAULT_WIDTH:
      g_value_set_int (value, priv->default_width);
      break;
    case PROP_DEFAULT_HEIGHT:
      g_value_set_int (value, priv->default_height);
      break;
    case PROP_DESTROY_WITH_PARENT:
      g_value_set_boolean (value, priv->destroy_with_parent);
      break;
    case PROP_HIDE_ON_CLOSE:
      g_value_set_boolean (value, priv->hide_on_close);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_window_get_icon_name (window));
      break;
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    case PROP_IS_ACTIVE:
      g_value_set_boolean (value, priv->is_active);
      break;
    case PROP_DECORATED:
      g_value_set_boolean (value, gtk_window_get_decorated (window));
      break;
    case PROP_DELETABLE:
      g_value_set_boolean (value, gtk_window_get_deletable (window));
      break;
    case PROP_TRANSIENT_FOR:
      g_value_set_object (value, gtk_window_get_transient_for (window));
      break;
    case PROP_APPLICATION:
      g_value_set_object (value, gtk_window_get_application (window));
      break;
    case PROP_DEFAULT_WIDGET:
      g_value_set_object (value, gtk_window_get_default_widget (window));
      break;
    case PROP_MNEMONICS_VISIBLE:
      g_value_set_boolean (value, priv->mnemonics_visible);
      break;
    case PROP_FOCUS_VISIBLE:
      g_value_set_boolean (value, priv->focus_visible);
      break;
    case PROP_MAXIMIZED:
      g_value_set_boolean (value, gtk_window_is_maximized (window));
      break;
    case PROP_FULLSCREENED:
      g_value_set_boolean (value, gtk_window_is_fullscreen (window));
      break;
    case PROP_SUSPENDED:
      g_value_set_boolean (value, gtk_window_is_suspended (window));
      break;
    case PROP_FOCUS_WIDGET:
      g_value_set_object (value, gtk_window_get_focus (window));
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_window_get_child (window));
      break;
    case PROP_TITLEBAR:
      g_value_set_object (value, gtk_window_get_titlebar (window));
      break;
    case PROP_HANDLE_MENUBAR_ACCEL:
      g_value_set_boolean (value, gtk_window_get_handle_menubar_accel (window));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_window_buildable_add_child;
}

static void
gtk_window_buildable_add_child (GtkBuildable *buildable,
                                GtkBuilder   *builder,
                                GObject      *child,
                                const char   *type)
{
  if (type && strcmp (type, "titlebar") == 0)
    gtk_window_set_titlebar (GTK_WINDOW (buildable), GTK_WIDGET (child));
  else if (GTK_IS_WIDGET (child))
    gtk_window_set_child (GTK_WINDOW (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_window_shortcut_manager_interface_init (GtkShortcutManagerInterface *iface)
{
}

static GdkDisplay *
gtk_window_root_get_display (GtkRoot *root)
{
  GtkWindow *window = GTK_WINDOW (root);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  return priv->display;
}

static GdkSurface *
gtk_window_native_get_surface (GtkNative *native)
{
  GtkWindow *self = GTK_WINDOW (native);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (self);

  return priv->surface;
}

static GskRenderer *
gtk_window_native_get_renderer (GtkNative *native)
{
  GtkWindow *self = GTK_WINDOW (native);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (self);

  return priv->renderer;
}

static GtkConstraintSolver *
gtk_window_root_get_constraint_solver (GtkRoot *root)
{
  GtkWindow *self = GTK_WINDOW (root);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (self);

  if (!priv->constraint_solver)
    {
      /* Shared constraint solver */
      priv->constraint_solver = gtk_constraint_solver_new ();
    }

  return priv->constraint_solver;
}

static GtkWidget *
gtk_window_root_get_focus (GtkRoot *root)
{
  GtkWindow *self = GTK_WINDOW (root);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (self);

  return priv->focus_widget;
}

static void synthesize_focus_change_events (GtkWindow       *window,
                                            GtkWidget       *old_focus,
                                            GtkWidget       *new_focus,
                                            GtkCrossingType  type);

static void
gtk_window_root_set_focus (GtkRoot   *root,
                           GtkWidget *focus)
{
  GtkWindow *self = GTK_WINDOW (root);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (self);
  GtkWidget *old_focus = NULL;

  if (focus && !gtk_widget_is_sensitive (focus))
    return;

  if (focus == priv->focus_widget)
    {
      if (priv->move_focus &&
          focus && gtk_widget_is_visible (focus))
        {
          priv->move_focus = FALSE;
          g_clear_object (&priv->move_focus_widget);
        }
      return;
    }

  if (priv->focus_widget)
    old_focus = g_object_ref (priv->focus_widget);
  g_set_object (&priv->focus_widget, NULL);

  if (old_focus)
    gtk_widget_set_has_focus (old_focus, FALSE);

  synthesize_focus_change_events (self, old_focus, focus, GTK_CROSSING_FOCUS);

  if (focus)
    gtk_widget_set_has_focus (focus, priv->is_active);

  g_set_object (&priv->focus_widget, focus);

  g_clear_object (&old_focus);

  if (priv->move_focus &&
      focus && gtk_widget_is_visible (focus))
    {
      priv->move_focus = FALSE;
      g_clear_object (&priv->move_focus_widget);
    }

  g_object_notify (G_OBJECT (self), "focus-widget");
}

static void
gtk_window_native_get_surface_transform (GtkNative *native,
                                         double    *x,
                                         double    *y)
{
  GtkBorder shadow;
  GtkCssBoxes css_boxes;
  const graphene_rect_t *margin_rect;

  get_shadow_width (GTK_WINDOW (native), &shadow);
  gtk_css_boxes_init (&css_boxes, GTK_WIDGET (native));
  margin_rect = gtk_css_boxes_get_margin_rect (&css_boxes);

  *x = shadow.left - margin_rect->origin.x;
  *y = shadow.top  - margin_rect->origin.y;
}

static void
gtk_window_native_layout (GtkNative *native,
                          int        width,
                          int        height)
{
  GtkWindow *window = GTK_WINDOW (native);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget = GTK_WIDGET (native);

  if (priv->surface_width != width || priv->surface_height != height)
    {
      surface_size_changed (widget, width, height);
      priv->surface_width = width;
      priv->surface_height = height;
    }

  /* This fake motion event is needed for getting up to date pointer focus
   * and coordinates when tho pointer didn't move but the layout changed
   * within the window.
   */
  if (gtk_widget_needs_allocate (widget))
    {
      GdkSeat *seat;

      seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
      if (seat)
        {
          GdkDevice *device;
          GtkWidget *focus;

          device = gdk_seat_get_pointer (seat);
          focus = gtk_window_lookup_pointer_focus_widget (GTK_WINDOW (widget),
                                                          device, NULL);
          if (focus)
            {
              GdkSurface *focus_surface =
                gtk_native_get_surface (gtk_widget_get_native (focus));

              gdk_surface_request_motion (focus_surface);
            }
        }
    }

  if (gtk_widget_needs_allocate (widget))
    {
      gtk_window_update_csd_size (window,
                                  &width, &height,
                                  EXCLUDE_CSD_SIZE);
      gtk_widget_allocate (widget, width, height, -1, NULL);
    }
  else
    {
      gtk_widget_ensure_allocate (widget);
    }
}

static void
gtk_window_root_interface_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_window_root_get_display;
  iface->get_constraint_solver = gtk_window_root_get_constraint_solver;
  iface->get_focus = gtk_window_root_get_focus;
  iface->set_focus = gtk_window_root_set_focus;
}

static void
gtk_window_native_interface_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_window_native_get_surface;
  iface->get_renderer = gtk_window_native_get_renderer;
  iface->get_surface_transform = gtk_window_native_get_surface_transform;
  iface->layout = gtk_window_native_layout;
}

/**
 * gtk_window_new:
 *
 * Creates a new `GtkWindow`.
 *
 * To get an undecorated window (no window borders), use
 * [method@Gtk.Window.set_decorated].
 *
 * All top-level windows created by gtk_window_new() are stored
 * in an internal top-level window list. This list can be obtained
 * from [func@Gtk.Window.list_toplevels]. Due to GTK keeping a
 * reference to the window internally, gtk_window_new() does not
 * return a reference to the caller.
 *
 * To delete a `GtkWindow`, call [method@Gtk.Window.destroy].
 *
 * Returns: a new `GtkWindow`.
 */
GtkWidget*
gtk_window_new (void)
{
  return g_object_new (GTK_TYPE_WINDOW, NULL);
}

/**
 * gtk_window_set_title: (attributes org.gtk.Method.set_property=title)
 * @window: a `GtkWindow`
 * @title: (nullable): title of the window
 *
 * Sets the title of the `GtkWindow`.
 *
 * The title of a window will be displayed in its title bar; on the
 * X Window System, the title bar is rendered by the window manager
 * so exactly how the title appears to users may vary according to a
 * user’s exact configuration. The title should help a user distinguish
 * this window from other windows they may have open. A good title might
 * include the application name and current document filename, for example.
 *
 * Passing %NULL does the same as setting the title to an empty string.
 */
void
gtk_window_set_title (GtkWindow  *window,
                      const char *title)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  char *new_title;

  g_return_if_fail (GTK_IS_WINDOW (window));

  new_title = g_strdup (title);
  g_free (priv->title);
  priv->title = new_title;

  if (_gtk_widget_get_realized (GTK_WIDGET (window)))
    gdk_toplevel_set_title (GDK_TOPLEVEL (priv->surface), new_title != NULL ? new_title : "");

  gtk_accessible_update_property (GTK_ACCESSIBLE (window),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, priv->title,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_TITLE]);
}

/**
 * gtk_window_get_title: (attributes org.gtk.Method.get_property=title)
 * @window: a `GtkWindow`
 *
 * Retrieves the title of the window.
 *
 * Returns: (nullable): the title of the window
 */
const char *
gtk_window_get_title (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->title;
}

/**
 * gtk_window_set_default_widget: (attributes org.gtk.Property.set=default-widget)
 * @window: a `GtkWindow`
 * @default_widget: (nullable): widget to be the default
 *   to unset the default widget for the toplevel
 *
 * Sets the default widget.
 *
 * The default widget is the widget that is activated when the user
 * presses Enter in a dialog (for example).
 */
void
gtk_window_set_default_widget (GtkWindow *window,
                               GtkWidget *default_widget)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->default_widget != default_widget)
    {
      GtkWidget *old_default_widget = NULL;

      if (default_widget)
	g_object_ref (default_widget);

      if (priv->default_widget)
	{
          old_default_widget = priv->default_widget;

          if (priv->focus_widget != priv->default_widget ||
              !gtk_widget_get_receives_default (priv->default_widget))
            _gtk_widget_set_has_default (priv->default_widget, FALSE);

          gtk_widget_queue_draw (priv->default_widget);
	}

      priv->default_widget = default_widget;

      priv->unset_default = FALSE;

      if (priv->default_widget)
	{
          if (priv->focus_widget == NULL ||
              !gtk_widget_get_receives_default (priv->focus_widget))
            _gtk_widget_set_has_default (priv->default_widget, TRUE);

          gtk_widget_queue_draw (priv->default_widget);
	}

      if (old_default_widget)
	g_object_notify (G_OBJECT (old_default_widget), "has-default");

      if (default_widget)
	{
	  g_object_notify (G_OBJECT (default_widget), "has-default");
	  g_object_unref (default_widget);
	}

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DEFAULT_WIDGET]);
    }
}

/**
 * gtk_window_get_default_widget: (attributes org.gtk.Property.get=default-widget)
 * @window: a `GtkWindow`
 *
 * Returns the default widget for @window.
 *
 * Returns: (nullable) (transfer none): the default widget
 */
GtkWidget *
gtk_window_get_default_widget (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->default_widget;
}

static gboolean
handle_keys_changed (gpointer data)
{
  GtkWindow *window = GTK_WINDOW (data);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->keys_changed_handler)
    {
      g_source_remove (priv->keys_changed_handler);
      priv->keys_changed_handler = 0;
    }

  if (priv->application_shortcut_controller)
    gtk_shortcut_controller_update_accels (GTK_SHORTCUT_CONTROLLER (priv->application_shortcut_controller));
  g_signal_emit (window, window_signals[KEYS_CHANGED], 0);

  return FALSE;
}

void
_gtk_window_notify_keys_changed (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (!priv->keys_changed_handler)
    {
      priv->keys_changed_handler = g_idle_add (handle_keys_changed, window);
      gdk_source_set_static_name_by_id (priv->keys_changed_handler, "[gtk] handle_keys_changed");
    }
}

/**
 * gtk_window_get_focus: (attributes org.gtk.Property.get=focus-widget)
 * @window: a `GtkWindow`
 *
 * Retrieves the current focused widget within the window.
 *
 * Note that this is the widget that would have the focus
 * if the toplevel window focused; if the toplevel window
 * is not focused then `gtk_widget_has_focus (widget)` will
 * not be %TRUE for the widget.
 *
 * Returns: (nullable) (transfer none): the currently focused widget
 */
GtkWidget *
gtk_window_get_focus (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->focus_widget;
}

static void
gtk_window_real_activate_default (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->default_widget && gtk_widget_is_sensitive (priv->default_widget) &&
      (!priv->focus_widget || !gtk_widget_get_receives_default (priv->focus_widget)))
    gtk_widget_activate (priv->default_widget);
  else if (priv->focus_widget && gtk_widget_is_sensitive (priv->focus_widget))
    gtk_widget_activate (priv->focus_widget);
}

/**
 * gtk_window_set_modal: (attributes org.gtk.Method.set_property=modal)
 * @window: a `GtkWindow`
 * @modal: whether the window is modal
 *
 * Sets a window modal or non-modal.
 *
 * Modal windows prevent interaction with other windows in the same
 * application. To keep modal dialogs on top of main application windows,
 * use [method@Gtk.Window.set_transient_for] to make the dialog transient
 * for the parent; most window managers will then disallow lowering the
 * dialog below the parent.
 */
void
gtk_window_set_modal (GtkWindow *window,
                      gboolean   modal)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  modal = modal != FALSE;
  if (priv->modal == modal)
    return;

  priv->modal = modal;
  widget = GTK_WIDGET (window);

  if (_gtk_widget_get_realized (widget))
    gdk_toplevel_set_modal (GDK_TOPLEVEL (priv->surface), modal);

  if (gtk_widget_get_visible (widget))
    {
      if (priv->modal)
        gtk_grab_add (widget);
      else
        gtk_grab_remove (widget);
    }

  update_window_actions (window);

  gtk_accessible_update_property (GTK_ACCESSIBLE (window),
                                  GTK_ACCESSIBLE_PROPERTY_MODAL, modal,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MODAL]);
}

/**
 * gtk_window_get_modal: (attributes org.gtk.Method.get_property=modal)
 * @window: a `GtkWindow`
 *
 * Returns whether the window is modal.
 *
 * Returns: %TRUE if the window is set to be modal and
 *   establishes a grab when shown
 */
gboolean
gtk_window_get_modal (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->modal;
}

/**
 * gtk_window_get_toplevels:
 *
 * Returns a list of all existing toplevel windows.
 *
 * If you want to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets or add new ones, be aware that
 * the list of toplevels will change and emit the "items-changed" signal.
 *
 * Returns: (transfer none) (attributes element-type=GtkWindow): the list
 *   of toplevel widgets
 */
GListModel *
gtk_window_get_toplevels (void)
{
  if (toplevel_list == NULL)
    toplevel_list = g_list_store_new (GTK_TYPE_WIDGET);

  return G_LIST_MODEL (toplevel_list);
}

/**
 * gtk_window_list_toplevels:
 *
 * Returns a list of all existing toplevel windows.
 *
 * The widgets in the list are not individually referenced.
 * If you want to iterate through the list and perform actions
 * involving callbacks that might destroy the widgets, you must
 * call `g_list_foreach (result, (GFunc)g_object_ref, NULL)` first,
 * and then unref all the widgets afterwards.
 *
 * Returns: (element-type GtkWidget) (transfer container): list of
 *   toplevel widgets
 */
GList*
gtk_window_list_toplevels (void)
{
  GListModel *toplevels;
  GList *list = NULL;
  guint i;

  toplevels = gtk_window_get_toplevels ();

  for (i = 0; i < g_list_model_get_n_items (toplevels); i++)
    {
      gpointer item = g_list_model_get_item (toplevels, i);
      list = g_list_prepend (list, item);
      g_object_unref (item);
    }

  return list;
}

static void
gtk_window_dispose (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  gtk_window_release_application (window);

  if (priv->transient_parent)
    gtk_window_set_transient_for (window, NULL);

  if (priv->group)
    gtk_window_group_remove_window (priv->group, window);

  g_list_free_full (priv->foci, (GDestroyNotify) gtk_pointer_focus_unref);
  priv->foci = NULL;

  g_clear_object (&priv->move_focus_widget);
  gtk_window_set_focus (window, NULL);
  gtk_window_set_default_widget (window, NULL);

  g_clear_pointer (&priv->child, gtk_widget_unparent);
  unset_titlebar (window);

  G_OBJECT_CLASS (gtk_window_parent_class)->dispose (object);
}

static void
gtk_window_transient_parent_destroyed (GtkWindow *parent,
                                       GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (GTK_WINDOW (window));

  if (priv->destroy_with_parent)
    gtk_window_destroy (window);
  else
    priv->transient_parent = NULL;
}

static void
gtk_window_transient_parent_realized (GtkWidget *parent,
                                      GtkWidget *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (GTK_WINDOW (window));
  GtkWindowPrivate *parent_priv = gtk_window_get_instance_private (GTK_WINDOW (parent));
  if (_gtk_widget_get_realized (window))
    gdk_toplevel_set_transient_for (GDK_TOPLEVEL (priv->surface), parent_priv->surface);
}

static void
gtk_window_transient_parent_unrealized (GtkWidget *parent,
                                        GtkWidget *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (GTK_WINDOW (window));
  if (_gtk_widget_get_realized (window))
    gdk_toplevel_set_transient_for (GDK_TOPLEVEL (priv->surface), NULL);
}

static void
gtk_window_transient_parent_display_changed (GtkWindow  *parent,
                                             GParamSpec *pspec,
                                             GtkWindow  *window)
{
  GtkWindowPrivate *parent_priv = gtk_window_get_instance_private (parent);

  gtk_window_set_display (window, parent_priv->display);
}

static void
gtk_window_unset_transient_for (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->transient_parent)
    {
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
                                            gtk_window_transient_parent_realized,
                                            window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
                                            gtk_window_transient_parent_unrealized,
                                            window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
                                            gtk_window_transient_parent_display_changed,
                                            window);
      g_signal_handlers_disconnect_by_func (priv->transient_parent,
                                            gtk_window_transient_parent_destroyed,
                                            window);

      priv->transient_parent = NULL;

      if (priv->transient_parent_group)
        {
          priv->transient_parent_group = FALSE;
          gtk_window_group_remove_window (priv->group, window);
        }
    }
}

/**
 * gtk_window_set_transient_for: (attributes org.gtk.Method.set_property=transient-for)
 * @window: a `GtkWindow`
 * @parent: (nullable): parent window
 *
 * Dialog windows should be set transient for the main application
 * window they were spawned from. This allows window managers to e.g.
 * keep the dialog on top of the main window, or center the dialog
 * over the main window. [ctor@Gtk.Dialog.new_with_buttons] and other
 * convenience functions in GTK will sometimes call
 * gtk_window_set_transient_for() on your behalf.
 *
 * Passing %NULL for @parent unsets the current transient window.
 *
 * On Windows, this function puts the child window on top of the parent,
 * much as the window manager would have done on X.
 */
void
gtk_window_set_transient_for (GtkWindow *window,
                              GtkWindow *parent)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  g_return_if_fail (window != parent);

  if (priv->transient_parent)
    {
      if (_gtk_widget_get_realized (GTK_WIDGET (window)) &&
          _gtk_widget_get_realized (GTK_WIDGET (priv->transient_parent)) &&
          (!parent || !_gtk_widget_get_realized (GTK_WIDGET (parent))))
        gtk_window_transient_parent_unrealized (GTK_WIDGET (priv->transient_parent),
                                                GTK_WIDGET (window));
      gtk_window_unset_transient_for (window);
    }

  priv->transient_parent = parent;

  if (parent)
    {
      GtkWindowPrivate *parent_priv = gtk_window_get_instance_private (parent);
      g_signal_connect (parent, "realize",
                        G_CALLBACK (gtk_window_transient_parent_realized), window);
      g_signal_connect (parent, "unrealize",
                        G_CALLBACK (gtk_window_transient_parent_unrealized), window);
      g_signal_connect (parent, "notify::display",
                        G_CALLBACK (gtk_window_transient_parent_display_changed), window);
      g_signal_connect (parent, "destroy",
                        G_CALLBACK (gtk_window_transient_parent_destroyed), window);

      gtk_window_set_display (window, parent_priv->display);


      if (_gtk_widget_get_realized (GTK_WIDGET (window)) &&
          _gtk_widget_get_realized (GTK_WIDGET (parent)))
        gtk_window_transient_parent_realized (GTK_WIDGET (parent), GTK_WIDGET (window));

      if (parent_priv->group)
        {
          gtk_window_group_add_window (parent_priv->group, window);
          priv->transient_parent_group = TRUE;
        }
    }

  update_window_actions (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_TRANSIENT_FOR]);
}

/**
 * gtk_window_get_transient_for: (attributes org.gtk.Method.get_property=transient-for)
 * @window: a `GtkWindow`
 *
 * Fetches the transient parent for this window.
 *
 * Returns: (nullable) (transfer none): the transient parent for this window
 */
GtkWindow *
gtk_window_get_transient_for (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->transient_parent;
}

/**
 * gtk_window_get_application: (attributes org.gtk.Method.get_property=application)
 * @window: a `GtkWindow`
 *
 * Gets the `GtkApplication` associated with the window.
 *
 * Returns: (nullable) (transfer none): a `GtkApplication`
 */
GtkApplication *
gtk_window_get_application (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->application;
}

static void
gtk_window_release_application (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->application)
    {
      GtkApplication *application;

      /* steal reference into temp variable */
      application = priv->application;
      priv->application = NULL;
      gtk_widget_remove_controller (GTK_WIDGET (window),
                                    priv->application_shortcut_controller);
      priv->application_shortcut_controller = NULL;

      gtk_application_remove_window (application, window);
      g_object_unref (application);
    }
}

/**
 * gtk_window_set_application: (attributes org.gtk.Method.set_property=application)
 * @window: a `GtkWindow`
 * @application: (nullable): a `GtkApplication`, or %NULL to unset
 *
 * Sets or unsets the `GtkApplication` associated with the window.
 *
 * The application will be kept alive for at least as long as it has
 * any windows associated with it (see g_application_hold() for a way
 * to keep it alive without windows).
 *
 * Normally, the connection between the application and the window will
 * remain until the window is destroyed, but you can explicitly remove
 * it by setting the @application to %NULL.
 *
 * This is equivalent to calling [method@Gtk.Application.remove_window]
 * and/or [method@Gtk.Application.add_window] on the old/new applications
 * as relevant.
 */
void
gtk_window_set_application (GtkWindow      *window,
                            GtkApplication *application)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->application != application)
    {
      gtk_window_release_application (window);

      priv->application = application;

      if (priv->application != NULL)
        {
          GtkApplicationAccels *app_accels;

          g_object_ref (priv->application);

          gtk_application_add_window (priv->application, window);

          app_accels = gtk_application_get_application_accels (priv->application);
          priv->application_shortcut_controller = gtk_shortcut_controller_new_for_model (gtk_application_accels_get_shortcuts (app_accels));
          gtk_event_controller_set_static_name (priv->application_shortcut_controller, "gtk-application-shortcuts");
          gtk_event_controller_set_propagation_phase (priv->application_shortcut_controller, GTK_PHASE_CAPTURE);
          gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (priv->application_shortcut_controller), GTK_SHORTCUT_SCOPE_GLOBAL);
          gtk_widget_add_controller (GTK_WIDGET (window), priv->application_shortcut_controller);
        }

      _gtk_widget_update_parent_muxer (GTK_WIDGET (window));

      _gtk_window_notify_keys_changed (window);

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_APPLICATION]);
    }
}

/**
 * gtk_window_set_destroy_with_parent: (attributes org.gtk.Method.set_property=destroy-with-parent)
 * @window: a `GtkWindow`
 * @setting: whether to destroy @window with its transient parent
 *
 * If @setting is %TRUE, then destroying the transient parent of @window
 * will also destroy @window itself.
 *
 * This is useful for dialogs that shouldn’t persist beyond the lifetime
 * of the main window they are associated with, for example.
 */
void
gtk_window_set_destroy_with_parent (GtkWindow *window,
                                    gboolean   setting)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->destroy_with_parent == (setting != FALSE))
    return;

  priv->destroy_with_parent = setting;

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DESTROY_WITH_PARENT]);
}

/**
 * gtk_window_get_destroy_with_parent: (attributes org.gtk.Method.get_property=destroy-with-parent)
 * @window: a `GtkWindow`
 *
 * Returns whether the window will be destroyed with its transient parent.
 *
 * Returns: %TRUE if the window will be destroyed with its transient parent.
 */
gboolean
gtk_window_get_destroy_with_parent (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->destroy_with_parent;
}

/**
 * gtk_window_set_hide_on_close: (attributes org.gtk.Method.set_property=hide-on-close)
 * @window: a `GtkWindow`
 * @setting: whether to hide the window when it is closed
 *
 * If @setting is %TRUE, then clicking the close button on the window
 * will not destroy it, but only hide it.
 */
void
gtk_window_set_hide_on_close (GtkWindow *window,
                              gboolean   setting)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->hide_on_close == setting)
    return;

  priv->hide_on_close = setting;

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_HIDE_ON_CLOSE]);
}

/**
 * gtk_window_get_hide_on_close: (attributes org.gtk.Method.get_property=hide-on-close)
 * @window: a `GtkWindow`
 *
 * Returns whether the window will be hidden when the close button is clicked.
 *
 * Returns: %TRUE if the window will be hidden
 */
gboolean
gtk_window_get_hide_on_close (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->hide_on_close;
}

static GtkWindowGeometryInfo*
gtk_window_get_geometry_info (GtkWindow *window,
			      gboolean   create)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWindowGeometryInfo *info;

  info = priv->geometry_info;
  if (!info && create)
    {
      info = g_new0 (GtkWindowGeometryInfo, 1);

      info->last.configure_request.x = 0;
      info->last.configure_request.y = 0;
      info->last.configure_request.width = -1;
      info->last.configure_request.height = -1;
      priv->geometry_info = info;
    }

  return info;
}

static void
unset_titlebar (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->title_box != NULL)
    {
      gtk_widget_unparent (priv->title_box);
      priv->title_box = NULL;
      priv->titlebar = NULL;
    }
}

static gboolean
gtk_window_is_composited (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkDisplay *display;

  display = priv->display;

  return gdk_display_is_rgba (display) && gdk_display_is_composited (display);
}

static gboolean
gtk_window_supports_client_shadow (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkDisplay *display;

  display = priv->display;

  return gdk_display_supports_shadow_width (display);
}

static void
gtk_window_enable_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget = GTK_WIDGET (window);

  /* We need a visual with alpha for rounded corners */
  if (gtk_window_is_composited (window))
    {
      gtk_widget_add_css_class (widget, "csd");
    }
  else
    {
      gtk_widget_add_css_class (widget, "solid-csd");
    }

  priv->client_decorated = TRUE;
}

/**
 * gtk_window_set_titlebar: (attributes org.gtk.Method.set_property=titlebar)
 * @window: a `GtkWindow`
 * @titlebar: (nullable): the widget to use as titlebar
 *
 * Sets a custom titlebar for @window.
 *
 * A typical widget used here is [class@Gtk.HeaderBar], as it
 * provides various features expected of a titlebar while allowing
 * the addition of child widgets to it.
 *
 * If you set a custom titlebar, GTK will do its best to convince
 * the window manager not to put its own titlebar on the window.
 * Depending on the system, this function may not work for a window
 * that is already visible, so you set the titlebar before calling
 * [method@Gtk.Widget.show].
 */
void
gtk_window_set_titlebar (GtkWindow *window,
                         GtkWidget *titlebar)
{
  GtkWidget *widget = GTK_WIDGET (window);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean was_mapped;

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->titlebar == titlebar)
    return;

  if ((!priv->title_box && titlebar) || (priv->title_box && !titlebar))
    {
      was_mapped = _gtk_widget_get_mapped (widget);
      if (_gtk_widget_get_realized (widget))
        {
          g_warning ("gtk_window_set_titlebar() called on a realized window");
          gtk_widget_unrealize (widget);
        }
    }
  else
    was_mapped = FALSE;

  unset_titlebar (window);

  if (titlebar == NULL)
    {
      /* these are updated in realize() */
      priv->client_decorated = FALSE;
      gtk_widget_remove_css_class (widget, "csd");
      gtk_widget_remove_css_class (widget, "solid-csd");
    }
  else
    {
      priv->use_client_shadow = gtk_window_supports_client_shadow (window);
      gtk_window_enable_csd (window);

      priv->titlebar = titlebar;
      priv->title_box = titlebar;
      gtk_widget_insert_before (priv->title_box, widget, NULL);

      gtk_widget_add_css_class (titlebar, "titlebar");
    }

  if (was_mapped)
    gtk_widget_map (widget);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_TITLEBAR]);
}

/**
 * gtk_window_get_titlebar: (attributes org.gtk.Method.get_property=titlebar)
 * @window: a `GtkWindow`
 *
 * Returns the custom titlebar that has been set with
 * gtk_window_set_titlebar().
 *
 * Returns: (nullable) (transfer none): the custom titlebar
 */
GtkWidget *
gtk_window_get_titlebar (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->titlebar;
}

/**
 * gtk_window_set_decorated: (attributes org.gtk.Method.set_property=decorated)
 * @window: a `GtkWindow`
 * @setting: %TRUE to decorate the window
 *
 * Sets whether the window should be decorated.
 *
 * By default, windows are decorated with a title bar, resize
 * controls, etc. Some window managers allow GTK to disable these
 * decorations, creating a borderless window. If you set the decorated
 * property to %FALSE using this function, GTK will do its best to
 * convince the window manager not to decorate the window. Depending on
 * the system, this function may not have any effect when called on a
 * window that is already visible, so you should call it before calling
 * [method@Gtk.Widget.show].
 *
 * On Windows, this function always works, since there’s no window manager
 * policy involved.
 */
void
gtk_window_set_decorated (GtkWindow *window,
                          gboolean   setting)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  if (setting == priv->decorated)
    return;

  priv->decorated = setting;

  if (priv->surface)
    gdk_toplevel_set_decorated (GDK_TOPLEVEL (priv->surface), priv->decorated && !priv->client_decorated);

  update_window_actions (window);
  gtk_widget_queue_resize (GTK_WIDGET (window));

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DECORATED]);
}

/**
 * gtk_window_get_decorated: (attributes org.gtk.Method.get_property=decorated)
 * @window: a `GtkWindow`
 *
 * Returns whether the window has been set to have decorations.
 *
 * Returns: %TRUE if the window has been set to have decorations
 */
gboolean
gtk_window_get_decorated (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  return priv->decorated;
}

/**
 * gtk_window_set_deletable: (attributes org.gtk.Method.set_property=deletable)
 * @window: a `GtkWindow`
 * @setting: %TRUE to decorate the window as deletable
 *
 * Sets whether the window should be deletable.
 *
 * By default, windows have a close button in the window frame.
 * Some  window managers allow GTK to disable this button. If you
 * set the deletable property to %FALSE using this function, GTK
 * will do its best to convince the window manager not to show a
 * close button. Depending on the system, this function may not
 * have any effect when called on a window that is already visible,
 * so you should call it before calling [method@Gtk.Widget.show].
 *
 * On Windows, this function always works, since there’s no window
 * manager policy involved.
 */
void
gtk_window_set_deletable (GtkWindow *window,
			  gboolean   setting)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  if (setting == priv->deletable)
    return;

  priv->deletable = setting;

  if (priv->surface)
    gdk_toplevel_set_deletable (GDK_TOPLEVEL (priv->surface), priv->deletable);

  update_window_actions (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DELETABLE]);
}

/**
 * gtk_window_get_deletable: (attributes org.gtk.Method.get_property=deletable)
 * @window: a `GtkWindow`
 *
 * Returns whether the window has been set to have a close button.
 *
 * Returns: %TRUE if the window has been set to have a close button
 */
gboolean
gtk_window_get_deletable (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  return priv->deletable;
}

static GtkWindowIconInfo*
get_icon_info (GtkWindow *window)
{
  return g_object_get_qdata (G_OBJECT (window), quark_gtk_window_icon_info);
}

static void
free_icon_info (GtkWindowIconInfo *info)
{
  g_free (info->icon_name);
  g_free (info);
}


static GtkWindowIconInfo*
ensure_icon_info (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  info = get_icon_info (window);

  if (info == NULL)
    {
      info = g_new0 (GtkWindowIconInfo, 1);
      g_object_set_qdata_full (G_OBJECT (window),
                              quark_gtk_window_icon_info,
                              info,
                              (GDestroyNotify)free_icon_info);
    }

  return info;
}

static int
icon_size_compare (GdkTexture *a,
                   GdkTexture *b)
{
  int area_a, area_b;

  area_a = gdk_texture_get_width (a) * gdk_texture_get_height (a);
  area_b = gdk_texture_get_width (b) * gdk_texture_get_height (b);

  return area_a - area_b;
}

static GdkTexture *
render_paintable_to_texture (GdkPaintable *paintable)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  gsk_render_node_unref (node);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static GList *
icon_list_from_theme (GtkWindow   *window,
		      const char *name)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *list;
  GtkIconTheme *icon_theme;
  GtkIconPaintable *info;
  GdkTexture *texture;
  int *sizes;
  int i;

  icon_theme = gtk_icon_theme_get_for_display (priv->display);

  sizes = gtk_icon_theme_get_icon_sizes (icon_theme, name);

  list = NULL;
  for (i = 0; sizes[i]; i++)
    {
      /* FIXME
       * We need an EWMH extension to handle scalable icons
       * by passing their name to the WM. For now just use a
       * fixed size of 48.
       */
      if (sizes[i] == -1)
        info = gtk_icon_theme_lookup_icon (icon_theme, name, NULL,
                                           48, priv->scale,
                                           gtk_widget_get_direction (GTK_WIDGET (window)),
                                           0);
      else
        info = gtk_icon_theme_lookup_icon (icon_theme, name, NULL,
                                           sizes[i], priv->scale,
                                           gtk_widget_get_direction (GTK_WIDGET (window)),
                                           0);

      texture = render_paintable_to_texture (GDK_PAINTABLE (info));
      list = g_list_insert_sorted (list, texture, (GCompareFunc) icon_size_compare);
      g_object_unref (info);
    }

  g_free (sizes);

  return list;
}

static void
gtk_window_realize_icon (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWindowIconInfo *info;
  GList *icon_list = NULL;

  g_return_if_fail (priv->surface != NULL);

  info = ensure_icon_info (window);

  if (info->realized)
    return;

  info->using_default_icon = FALSE;
  info->using_themed_icon = FALSE;

  /* Look up themed icon */
  if (icon_list == NULL && info->icon_name)
    {
      icon_list = icon_list_from_theme (window, info->icon_name);
      if (icon_list)
        info->using_themed_icon = TRUE;
    }

  /* Look up themed icon */
  if (icon_list == NULL && default_icon_name)
    {
      icon_list = icon_list_from_theme (window, default_icon_name);
      info->using_default_icon = TRUE;
      info->using_themed_icon = TRUE;
    }

  info->realized = TRUE;

  gdk_toplevel_set_icon_list (GDK_TOPLEVEL (priv->surface), icon_list);

  if (info->using_themed_icon)
    g_list_free_full (icon_list, g_object_unref);
}

GdkPaintable *
gtk_window_get_icon_for_size (GtkWindow *window,
                              int        size)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  const char *name;
  GtkIconPaintable *info;

  name = gtk_window_get_icon_name (window);

  if (!name)
    name = default_icon_name;
  if (!name)
    return NULL;

  info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (window))),
                                     name, NULL, size, priv->scale,
                                     gtk_widget_get_direction (GTK_WIDGET (window)),
                                     0);
  if (info == NULL)
    return NULL;

  return GDK_PAINTABLE (info);
}

static void
gtk_window_unrealize_icon (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  info = get_icon_info (window);

  if (info == NULL)
    return;

  /* We don't clear the properties on the window, just figure the
   * window is going away.
   */

  info->realized = FALSE;

}

static void
update_themed_icon (GtkWindow *window)
{
  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ICON_NAME]);

  gtk_window_unrealize_icon (window);

  if (_gtk_widget_get_realized (GTK_WIDGET (window)))
    gtk_window_realize_icon (window);
}

/**
 * gtk_window_set_icon_name: (attributes org.gtk.Method.set_property=icon-name)
 * @window: a `GtkWindow`
 * @name: (nullable): the name of the themed icon
 *
 * Sets the icon for the window from a named themed icon.
 *
 * See the docs for [class@Gtk.IconTheme] for more details.
 * On some platforms, the window icon is not used at all.
 *
 * Note that this has nothing to do with the WM_ICON_NAME
 * property which is mentioned in the ICCCM.
 */
void
gtk_window_set_icon_name (GtkWindow  *window,
			  const char *name)
{
  GtkWindowIconInfo *info;
  char *tmp;

  g_return_if_fail (GTK_IS_WINDOW (window));

  info = ensure_icon_info (window);

  if (g_strcmp0 (info->icon_name, name) == 0)
    return;

  tmp = info->icon_name;
  info->icon_name = g_strdup (name);
  g_free (tmp);

  update_themed_icon (window);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_ICON_NAME]);
}

/**
 * gtk_window_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @window: a `GtkWindow`
 *
 * Returns the name of the themed icon for the window.
 *
 * Returns: (nullable): the icon name
 */
const char *
gtk_window_get_icon_name (GtkWindow *window)
{
  GtkWindowIconInfo *info;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  info = ensure_icon_info (window);

  return info->icon_name;
}

/**
 * gtk_window_set_default_icon_name:
 * @name: the name of the themed icon
 *
 * Sets an icon to be used as fallback.
 *
 * The fallback icon is used for windows that
 * haven't had [method@Gtk.Window.set_icon_name]
 * called on them.
 */
void
gtk_window_set_default_icon_name (const char *name)
{
  GList *tmp_list;
  GList *toplevels;

  g_free (default_icon_name);
  default_icon_name = g_strdup (name);

  /* Update all toplevels */
  toplevels = gtk_window_list_toplevels ();
  tmp_list = toplevels;
  while (tmp_list != NULL)
    {
      GtkWindowIconInfo *info;
      GtkWindow *w = tmp_list->data;

      info = get_icon_info (w);
      if (info && info->using_default_icon && info->using_themed_icon)
        {
          gtk_window_unrealize_icon (w);
          if (_gtk_widget_get_realized (GTK_WIDGET (w)))
            gtk_window_realize_icon (w);
        }

      tmp_list = tmp_list->next;
    }
  g_list_free (toplevels);
}

/**
 * gtk_window_get_default_icon_name:
 *
 * Returns the fallback icon name for windows.
 *
 * The returned string is owned by GTK and should not
 * be modified. It is only valid until the next call to
 * [func@Gtk.Window.set_default_icon_name].
 *
 * Returns: (nullable): the fallback icon name for windows
 */
const char *
gtk_window_get_default_icon_name (void)
{
  return default_icon_name;
}

static void
gtk_window_update_csd_size (GtkWindow *window,
                            int       *width,
                            int       *height,
                            int        apply)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkBorder window_border = { 0 };
  int w, h;

  if (!priv->decorated ||
      priv->fullscreen)
    return;

  get_shadow_width (window, &window_border);
  w = *width + apply * (window_border.left + window_border.right);
  h = *height + apply * (window_border.top + window_border.bottom);

  /* Make sure the size remains acceptable */
  if (w < 1)
    w = 1;
  if (h < 1)
    h = 1;

  /* Only update given size if not negative */
  if (*width > -1)
    *width = w;
  if (*height > -1)
    *height = h;
}

static void
gtk_window_set_default_size_internal (GtkWindow    *window,
                                      gboolean      change_width,
                                      int           width,
                                      gboolean      change_height,
                                      int           height)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (change_width == FALSE || width >= -1);
  g_return_if_fail (change_height == FALSE || height >= -1);

  g_object_freeze_notify (G_OBJECT (window));

  if (change_width)
    {
      if (priv->default_width != width)
        {
          priv->default_width = width;
          g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DEFAULT_WIDTH]);
        }
    }

  if (change_height)
    {
      if (priv->default_height != height)
        {
          priv->default_height = height;
          g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DEFAULT_HEIGHT]);
        }
    }

  g_object_thaw_notify (G_OBJECT (window));
}

/**
 * gtk_window_set_default_size:
 * @window: a `GtkWindow`
 * @width: width in pixels, or -1 to unset the default width
 * @height: height in pixels, or -1 to unset the default height
 *
 * Sets the default size of a window.
 *
 * The default size of a window is the size that will be used if no other constraints apply.
 * 
 * The default size will be updated whenever the window is resized
 * to reflect the new size, unless the window is forced to a size,
 * like when it is maximized or fullscreened.
 *
 * If the window’s minimum size request is larger than
 * the default, the default will be ignored.
 *
 * Setting the default size to a value <= 0 will cause it to be
 * ignored and the natural size request will be used instead. It
 * is possible to do this while the window is showing to "reset"
 * it to its initial size.
 *
 * Unlike [method@Gtk.Widget.set_size_request], which sets a size
 * request for a widget and thus would keep users from shrinking
 * the window, this function only sets the initial size, just as
 * if the user had resized the window themselves. Users can still
 * shrink the window again as they normally would. Setting a default
 * size of -1 means to use the “natural” default size (the size request
 * of the window).
 *
 * If you use this function to reestablish a previously saved window size,
 * note that the appropriate size to save is the one returned by
 * [method@Gtk.Window.get_default_size]. Using the window allocation
 * directly will not work in all circumstances and can lead to growing
 * or shrinking windows.
 */
void
gtk_window_set_default_size (GtkWindow   *window,
			     int          width,
			     int          height)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  gtk_window_set_default_size_internal (window, TRUE, width, TRUE, height);
  gtk_widget_queue_resize (GTK_WIDGET (window));
}

/**
 * gtk_window_get_default_size:
 * @window: a `GtkWindow`
 * @width: (out) (optional): location to store the default width
 * @height: (out) (optional): location to store the default height
 *
 * Gets the default size of the window.
 *
 * A value of 0 for the width or height indicates that a default
 * size has not been explicitly set for that dimension, so the
 * “natural” size of the window will be used.
 *
 * This function is the recommended way for [saving window state
 * across restarts of applications](https://developer.gnome.org/documentation/tutorials/save-state.html).
 */
void
gtk_window_get_default_size (GtkWindow *window,
			     int       *width,
			     int       *height)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (width != NULL)
    *width = priv->default_width;

  if (height != NULL)
    *height = priv->default_height;
}

static gboolean
gtk_window_close_request (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->hide_on_close)
    {
      gtk_widget_set_visible (GTK_WIDGET (window), FALSE);
      return TRUE;
    }

  return FALSE;
}

gboolean
gtk_window_emit_close_request (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean handled;

  /* Avoid re-entrancy issues when calling gtk_window_close from a
   * close-request handler */
  if (priv->in_emit_close_request)
    return TRUE;

  priv->in_emit_close_request = TRUE;
  g_signal_emit (window, window_signals[CLOSE_REQUEST], 0, &handled);
  priv->in_emit_close_request = FALSE;

  return handled;
}

static void
gtk_window_finalize (GObject *object)
{
  GtkWindow *window = GTK_WINDOW (object);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkSeat *seat;

  g_free (priv->title);
  gtk_window_release_application (window);

  if (priv->geometry_info)
    {
      g_free (priv->geometry_info);
    }

  if (priv->keys_changed_handler)
    {
      g_source_remove (priv->keys_changed_handler);
      priv->keys_changed_handler = 0;
    }

  seat = gdk_display_get_default_seat (priv->display);
  if (seat)
    g_signal_handlers_disconnect_by_func (seat, device_removed_cb, window);

#ifdef GDK_WINDOWING_X11
  g_signal_handlers_disconnect_by_func (gtk_settings_get_for_display (priv->display),
                                        gtk_window_on_theme_variant_changed,
                                        window);
#endif

  g_free (priv->startup_id);

  if (priv->mnemonics_display_timeout_id)
    {
      g_source_remove (priv->mnemonics_display_timeout_id);
      priv->mnemonics_display_timeout_id = 0;
    }

  if (priv->focus_visible_timeout)
    {
      g_source_remove (priv->focus_visible_timeout);
      priv->focus_visible_timeout = 0;
    }

  g_clear_object (&priv->constraint_solver);
  g_clear_object (&priv->renderer);
  g_clear_object (&priv->resize_cursor);

  G_OBJECT_CLASS (gtk_window_parent_class)->finalize (object);
}

static gboolean
update_csd_visibility (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean visible;

  if (priv->title_box == NULL)
    return FALSE;

  visible = !priv->fullscreen &&
            priv->decorated;

  gtk_widget_set_child_visible (priv->title_box, visible);

  return visible;
}

static void
update_window_actions (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean is_sovereign_window = !priv->modal && !priv->transient_parent;

  gtk_widget_action_set_enabled (GTK_WIDGET (window), "window.minimize",
                                 is_sovereign_window);
  gtk_widget_action_set_enabled (GTK_WIDGET (window), "window.toggle-maximized",
                                 priv->resizable && is_sovereign_window);
  gtk_widget_action_set_enabled (GTK_WIDGET (window), "window.close",
                                 priv->deletable);

  update_csd_visibility (window);
}

void
_gtk_window_request_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  priv->csd_requested = TRUE;
}

static gboolean
gtk_window_should_use_csd (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  const char *csd_env;

  if (priv->csd_requested)
    return TRUE;

  if (!priv->decorated)
    return FALSE;

  csd_env = g_getenv ("GTK_CSD");

#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkDisplay *gdk_display = gtk_widget_get_display (GTK_WIDGET (window));
      return !gdk_wayland_display_prefers_ssd (gdk_display);
    }
#endif

#ifdef GDK_WINDOWING_WIN32
  if (g_strcmp0 (csd_env, "0") != 0 &&
      GDK_IS_WIN32_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    return TRUE;
#endif

  return (g_strcmp0 (csd_env, "1") == 0);
}

static void
gtk_window_show (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (!g_list_store_find (toplevel_list, window, NULL))
    g_warning ("A window is shown after it has been destroyed. This will leave the window in an inconsistent state.");

  _gtk_widget_set_visible_flag (widget, TRUE);

  gtk_css_node_validate (gtk_widget_get_css_node (widget));

  gtk_widget_realize (widget);

  gtk_window_present_toplevel (window);

  gtk_widget_map (widget);

  if (!priv->focus_widget)
    gtk_window_move_focus (widget, GTK_DIR_TAB_FORWARD);

  if (priv->modal)
    gtk_grab_add (widget);
}

static void
gtk_window_hide (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);

  if (priv->modal)
    gtk_grab_remove (widget);
}

static GdkToplevelLayout *
gtk_window_compute_base_layout (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkToplevelLayout *layout;

  layout = gdk_toplevel_layout_new ();

  gdk_toplevel_layout_set_resizable (layout, priv->resizable);

  return layout;
}

static void
gtk_window_present_toplevel (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkToplevelLayout *layout;

  layout = gtk_window_compute_base_layout (window);
  gdk_toplevel_layout_set_maximized (layout, priv->maximized);
  gdk_toplevel_layout_set_fullscreen (layout, priv->fullscreen,
                                      priv->initial_fullscreen_monitor);
  gdk_toplevel_present (GDK_TOPLEVEL (priv->surface), layout);
  gdk_toplevel_layout_unref (layout);
}

static void
gtk_window_update_toplevel (GtkWindow         *window,
                            GdkToplevelLayout *layout)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    gdk_toplevel_present (GDK_TOPLEVEL (priv->surface), layout);
  gdk_toplevel_layout_unref (layout);
}

static void
gtk_window_notify_startup (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (!disable_startup_notification)
    {
      /* Do we have a custom startup-notification id? */
      if (priv->startup_id != NULL)
        {
          /* Make sure we have a "real" id */
          if (!startup_id_is_fake (priv->startup_id))
            gdk_toplevel_set_startup_id (GDK_TOPLEVEL (priv->surface), priv->startup_id);

          g_free (priv->startup_id);
          priv->startup_id = NULL;
        }
      else
        gdk_toplevel_set_startup_id (GDK_TOPLEVEL (priv->surface), NULL);
    }
}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child = priv->child;

  GTK_WIDGET_CLASS (gtk_window_parent_class)->map (widget);

  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box))
    gtk_widget_map (priv->title_box);

  gtk_window_present_toplevel (window);

  if (priv->minimize_initially)
    gdk_toplevel_minimize (GDK_TOPLEVEL (priv->surface));

  gtk_window_set_theme_variant (window);

  if (!priv->in_present)
    gtk_window_notify_startup (window);

  /* inherit from transient parent, so that a dialog that is
   * opened via keynav shows focus initially
   */
  if (priv->transient_parent)
    gtk_window_set_focus_visible (window, gtk_window_get_focus_visible (priv->transient_parent));
  else
    gtk_window_set_focus_visible (window, FALSE);

  if (priv->application)
    gtk_application_handle_window_map (priv->application, window);

  gtk_widget_realize_at_context (widget);
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child = priv->child;

  GTK_WIDGET_CLASS (gtk_window_parent_class)->unmap (widget);
  gdk_surface_hide (priv->surface);

  gtk_widget_unrealize_at_context (widget);

  if (priv->title_box != NULL)
    gtk_widget_unmap (priv->title_box);

  if (child != NULL)
    gtk_widget_unmap (child);
}

static void
check_scale_changed (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget = GTK_WIDGET (window);
  int old_scale;

  old_scale = priv->scale;
  priv->scale = gtk_widget_get_scale_factor (widget);
  if (old_scale != priv->scale)
    _gtk_widget_scale_changed (widget);
}

static void
get_shadow_width (GtkWindow *window,
                  GtkBorder *shadow_width)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkCssStyle *style;

  if (!priv->decorated)
    goto out;

  if (!priv->client_decorated ||
      !priv->use_client_shadow)
    goto out;

  if (priv->maximized ||
      priv->fullscreen)
    goto out;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (window)));

  /* Calculate the size of the drop shadows ... */
  gtk_css_shadow_value_get_extents (style->background->box_shadow, shadow_width);

  shadow_width->left = MAX (shadow_width->left, RESIZE_HANDLE_SIZE);
  shadow_width->top = MAX (shadow_width->top, RESIZE_HANDLE_SIZE);
  shadow_width->bottom = MAX (shadow_width->bottom, RESIZE_HANDLE_SIZE);
  shadow_width->right = MAX (shadow_width->right, RESIZE_HANDLE_SIZE);

  return;

out:
  *shadow_width = (GtkBorder) {0, 0, 0, 0};
}

static void
update_opaque_region (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean subtract_decoration_corners;
  gboolean subtract_shadow;

  subtract_decoration_corners = (priv->client_decorated &&
                                 priv->decorated &&
                                 !priv->fullscreen &&
                                 !priv->maximized);
  subtract_shadow = (priv->client_decorated &&
                     priv->decorated &&
                     priv->use_client_shadow &&
                     !priv->maximized &&
                     !priv->fullscreen);

  gtk_native_update_opaque_region (GTK_NATIVE (window),
                                   NULL,
                                   subtract_decoration_corners,
                                   subtract_shadow,
                                   RESIZE_HANDLE_SIZE);
}

static void
update_realized_window_properties (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkRectangle rect;
  GtkCssBoxes css_boxes;
  const graphene_rect_t *border_rect;
  double native_x, native_y;

  update_opaque_region (window);

  if (!priv->client_decorated || !priv->use_client_shadow)
    return;

  gtk_native_get_surface_transform (GTK_NATIVE (window), &native_x, &native_y);

  /* update the input shape, which makes it so that clicks
   * outside the border windows go through. */
  gtk_css_boxes_init (&css_boxes, GTK_WIDGET (window));
  border_rect = gtk_css_boxes_get_border_rect (&css_boxes);

  /* This logic is duplicated in get_edge_for_coordinates() */
  rect.x = native_x + border_rect->origin.x - RESIZE_HANDLE_SIZE;
  rect.y = native_y + border_rect->origin.y - RESIZE_HANDLE_SIZE;
  rect.width = border_rect->size.width + 2 * RESIZE_HANDLE_SIZE;
  rect.height = border_rect->size.height + 2 * RESIZE_HANDLE_SIZE;

  if (rect.width > 0 && rect.height > 0)
    {
      cairo_region_t *region = cairo_region_create_rectangle (&rect);

      gdk_surface_set_input_region (priv->surface, region);
      cairo_region_destroy (region);
    }
}

/* NB: When orientation is VERTICAL, width/height are flipped.
 * The code uses the terms nonetheless to make it more intuitive
 * to understand.
 */
static void
gtk_window_compute_min_size (GtkWidget      *window,
                             GtkOrientation  orientation,
                             double          ideal_ratio,
                             int            *min_width,
                             int            *min_height)
{
  int start, end, mid, other;
  double ratio;

  /* start = min width, end = min width for min height (ie max width) */
  gtk_widget_measure (window, orientation, -1, &start, NULL, NULL, NULL);
  gtk_widget_measure (window, OPPOSITE_ORIENTATION (orientation), start, &other, NULL, NULL, NULL);
  if ((double) start / other >= ideal_ratio)
    {
      *min_width = start;
      *min_height = other;
      return;
    }
  gtk_widget_measure (window, OPPOSITE_ORIENTATION (orientation), -1, &other, NULL, NULL, NULL);
  gtk_widget_measure (window, orientation, other, &end, NULL, NULL, NULL);
  if ((double) end / other <= ideal_ratio)
    {
      *min_width = end;
      *min_height = other;
      return;
    }

  while (start < end)
    {
      mid = (start + end) / 2;

      gtk_widget_measure (window, OPPOSITE_ORIENTATION (orientation), mid, &other, NULL, NULL, NULL);
      ratio = (double) mid / other;
      if(ratio == ideal_ratio)
        {
          *min_width = mid;
          *min_height = other;
          return;
        }
      else if (ratio < ideal_ratio)
        start = mid + 1;
      else
        end = mid - 1;
    }

  gtk_widget_measure (window, orientation, other, &start, NULL, NULL, NULL);
  *min_width = start;
  *min_height = other;
}

static void
gtk_window_compute_default_size (GtkWindow *window,
                                 int        cur_width,
                                 int        cur_height,
                                 int        max_width,
                                 int        max_height,
                                 int       *min_width,
                                 int       *min_height,
                                 int       *width,
                                 int       *height)
{
  GtkWidget *widget = GTK_WIDGET (window);
  GtkSizeRequestMode request_mode = gtk_widget_get_request_mode (widget);

  if (request_mode == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    {
      int minimum, natural;

      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1,
                          &minimum, &natural,
                          NULL, NULL);
      *min_height = minimum;
      if (cur_height <= 0)
        cur_height = natural;
      *height = MAX (minimum, MIN (max_height, cur_height));

      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                          *height,
                          &minimum, &natural,
                          NULL, NULL);
      *min_width = minimum;
      if (cur_width <= 0)
        cur_width = natural;
      *width = MAX (minimum, MIN (max_width, cur_width));

      gtk_window_compute_min_size (widget, GTK_ORIENTATION_VERTICAL, (double) *height / *width, min_height, min_width);
    }
  else /* GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH or CONSTANT_SIZE */
    {
      int minimum, natural;

      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          &minimum, &natural,
                          NULL, NULL);
      *min_width = minimum;
      if (cur_width <= 0)
        cur_width = natural;
      *width = MAX (minimum, MIN (max_width, cur_width));

      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          *width,
                          &minimum, &natural,
                          NULL, NULL);
      *min_height = minimum;
      if (cur_height <= 0)
        cur_height = natural;

      *height = MAX (minimum, MIN (max_height, cur_height));

      if (request_mode != GTK_SIZE_REQUEST_CONSTANT_SIZE)
        gtk_window_compute_min_size (widget, GTK_ORIENTATION_HORIZONTAL, (double) *width / *height, min_width, min_height);
    }
}

static gboolean
should_remember_size (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (!priv->resizable)
    return FALSE;

  return !(priv->state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                          GDK_TOPLEVEL_STATE_MAXIMIZED |
                          GDK_TOPLEVEL_STATE_TILED |
                          GDK_TOPLEVEL_STATE_TOP_TILED |
                          GDK_TOPLEVEL_STATE_RIGHT_TILED |
                          GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                          GDK_TOPLEVEL_STATE_LEFT_TILED |
                          GDK_TOPLEVEL_STATE_MINIMIZED));
}

static void
toplevel_compute_size (GdkToplevel     *toplevel,
                       GdkToplevelSize *size,
                       GtkWidget       *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  int width, height;
  GtkBorder shadow;
  int bounds_width, bounds_height;
  int min_width, min_height;

  gdk_toplevel_size_get_bounds (size, &bounds_width, &bounds_height);

  gtk_window_compute_default_size (window,
                                   priv->default_width, priv->default_height,
                                   bounds_width, bounds_height,
                                   &min_width, &min_height,
                                   &width, &height);

  if (width < min_width)
    width = min_width;
  if (height < min_height)
    height = min_height;

  if (should_remember_size (window))
    gtk_window_set_default_size_internal (window, TRUE, width, TRUE, height);

  gtk_window_update_csd_size (window,
                              &width, &height,
                              INCLUDE_CSD_SIZE);
  gtk_window_update_csd_size (window,
                              &min_width, &min_height,
                              INCLUDE_CSD_SIZE);

  gdk_toplevel_size_set_min_size (size, min_width, min_height);

  gdk_toplevel_size_set_size (size, width, height);

  if (priv->use_client_shadow)
    {
      get_shadow_width (window, &shadow);
      gdk_toplevel_size_set_shadow_width (size,
                                          shadow.left, shadow.right,
                                          shadow.top, shadow.bottom);
    }

  gtk_widget_ensure_resize (widget);
}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkSurface *surface;
  GdkFrameClock *frame_clock;

  /* Create default title bar */
  if (!priv->client_decorated && gtk_window_should_use_csd (window))
    {
      if (gtk_window_is_composited (window))
        {
          priv->use_client_shadow = gtk_window_supports_client_shadow (window);
          gtk_window_enable_csd (window);

          if (priv->title_box == NULL)
            {
              priv->title_box = gtk_header_bar_new ();
              gtk_widget_add_css_class (priv->title_box, "titlebar");
              gtk_widget_add_css_class (priv->title_box, "default-decoration");

              gtk_widget_insert_before (priv->title_box, widget, NULL);
            }

          update_window_actions (window);
        }
      else
        priv->use_client_shadow = FALSE;
    }

  surface = gdk_surface_new_toplevel (gtk_widget_get_display (widget));
  priv->surface = surface;
  gdk_surface_set_widget (surface, widget);

  if (priv->renderer == NULL)
    priv->renderer = gsk_renderer_new_for_surface (surface);

  g_signal_connect_swapped (surface, "notify::state", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect_swapped (surface, "notify::mapped", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect (surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (surface, "event", G_CALLBACK (surface_event), widget);
  g_signal_connect (surface, "compute-size", G_CALLBACK (toplevel_compute_size), widget);

  frame_clock = gdk_surface_get_frame_clock (surface);
  g_signal_connect (frame_clock, "after-paint", G_CALLBACK (after_paint), widget);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->realize (widget);

  gtk_root_start_layout (GTK_ROOT (window));

  if (priv->transient_parent &&
      _gtk_widget_get_realized (GTK_WIDGET (priv->transient_parent)))
    {
      GtkWindowPrivate *parent_priv = gtk_window_get_instance_private (priv->transient_parent);
      gdk_toplevel_set_transient_for (GDK_TOPLEVEL (surface), parent_priv->surface);
    }

  if (priv->title)
    gdk_toplevel_set_title (GDK_TOPLEVEL (surface), priv->title);

  gdk_toplevel_set_decorated (GDK_TOPLEVEL (surface), priv->decorated && !priv->client_decorated);
  gdk_toplevel_set_deletable (GDK_TOPLEVEL (surface), priv->deletable);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_SURFACE (surface))
    {
      if (priv->client_decorated)
        gdk_wayland_toplevel_announce_csd (GDK_TOPLEVEL (surface));
      else
        gdk_wayland_toplevel_announce_ssd (GDK_TOPLEVEL (surface));
    }
#endif

  gdk_toplevel_set_modal (GDK_TOPLEVEL (surface), priv->modal);

  if (priv->startup_id)
    {
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_SURFACE (surface))
        {
          guint32 timestamp = extract_time_from_startup_id (priv->startup_id);
          if (timestamp != GDK_CURRENT_TIME)
            gdk_x11_surface_set_user_time (surface, timestamp);
        }
#endif
    }

#ifdef GDK_WINDOWING_X11
  if (priv->initial_timestamp != GDK_CURRENT_TIME)
    {
      if (GDK_IS_X11_SURFACE (surface))
        gdk_x11_surface_set_user_time (surface, priv->initial_timestamp);
    }
#endif

  update_realized_window_properties (window);

  if (priv->application)
    gtk_application_handle_window_realize (priv->application, window);

  /* Icons */
  gtk_window_realize_icon (window);

  check_scale_changed (window);

  gtk_native_realize (GTK_NATIVE (window));
}

static void
gtk_window_unrealize (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWindowGeometryInfo *info;
  GdkSurface *surface;
  GdkFrameClock *frame_clock;

  gtk_native_unrealize (GTK_NATIVE (window));

  /* On unrealize, we reset the size of the window such
   * that we will re-apply the default sizing stuff
   * next time we show the window.
   *
   * Default positioning is reset on unmap, instead of unrealize.
   */
  priv->need_default_size = TRUE;
  info = gtk_window_get_geometry_info (window, FALSE);
  if (info)
    {
      info->last.configure_request.x = 0;
      info->last.configure_request.y = 0;
      info->last.configure_request.width = -1;
      info->last.configure_request.height = -1;
      /* be sure we reset geom hints on re-realize */
      info->last.flags = 0;
    }

  gsk_renderer_unrealize (priv->renderer);

  /* Icons */
  gtk_window_unrealize_icon (window);

  if (priv->title_box)
    gtk_widget_unrealize (priv->title_box);

  if (priv->child)
    gtk_widget_unrealize (priv->child);

  g_clear_object (&priv->renderer);

  surface = priv->surface;

  g_signal_handlers_disconnect_by_func (surface, surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (surface, surface_event, widget);
  g_signal_handlers_disconnect_by_func (surface, toplevel_compute_size, widget);

  frame_clock = gdk_surface_get_frame_clock (surface);

  g_signal_handlers_disconnect_by_func (frame_clock, after_paint, widget);

  gtk_root_stop_layout (GTK_ROOT (window));

  GTK_WIDGET_CLASS (gtk_window_parent_class)->unrealize (widget);

  gdk_surface_set_widget (surface, NULL);
  g_clear_pointer (&priv->surface, gdk_surface_destroy);

  priv->use_client_shadow = FALSE;
}

static void
update_window_style_classes (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget = GTK_WIDGET (window);
  guint edge_constraints;

  edge_constraints = priv->edge_constraints;

  if (!priv->edge_constraints)
    {
      gtk_widget_remove_css_class (widget, "tiled-top");
      gtk_widget_remove_css_class (widget, "tiled-right");
      gtk_widget_remove_css_class (widget, "tiled-bottom");
      gtk_widget_remove_css_class (widget, "tiled-left");

      if (priv->tiled)
        gtk_widget_add_css_class (widget, "tiled");
      else
        gtk_widget_remove_css_class (widget, "tiled");
    }
  else
    {
      gtk_widget_remove_css_class (widget, "tiled");

      if (edge_constraints & GDK_TOPLEVEL_STATE_TOP_TILED)
        gtk_widget_add_css_class (widget, "tiled-top");
      else
        gtk_widget_remove_css_class (widget, "tiled-top");

      if (edge_constraints & GDK_TOPLEVEL_STATE_RIGHT_TILED)
        gtk_widget_add_css_class (widget, "tiled-right");
      else
        gtk_widget_remove_css_class (widget, "tiled-right");

      if (edge_constraints & GDK_TOPLEVEL_STATE_BOTTOM_TILED)
        gtk_widget_add_css_class (widget, "tiled-bottom");
      else
        gtk_widget_remove_css_class (widget, "tiled-bottom");

      if (edge_constraints & GDK_TOPLEVEL_STATE_LEFT_TILED)
        gtk_widget_add_css_class (widget, "tiled-left");
      else
        gtk_widget_remove_css_class (widget, "tiled-left");
    }

  if (priv->maximized)
    gtk_widget_add_css_class (widget, "maximized");
  else
    gtk_widget_remove_css_class (widget, "maximized");

  if (priv->fullscreen)
    gtk_widget_add_css_class (widget, "fullscreen");
  else
    gtk_widget_remove_css_class (widget, "fullscreen");
}

/* _gtk_window_set_allocation:
 * @window: a `GtkWindow`
 * @allocation: the original allocation for the window
 * @allocation_out: @allocation taking decorations into
 * consideration
 *
 * This function is like gtk_widget_set_allocation()
 * but does the necessary extra work to update
 * the resize grip positioning, etc.
 *
 * Call this instead of gtk_widget_set_allocation()
 * when overriding ::size_allocate in a GtkWindow
 * subclass without chaining up.
 *
 * The @allocation parameter will be adjusted to
 * reflect any internal decorations that the window
 * may have. That revised allocation will then be
 * returned in the @allocation_out parameter.
 */
void
_gtk_window_set_allocation (GtkWindow           *window,
                            int                  width,
                            int                  height,
                            GtkAllocation       *allocation_out)
{
  GtkWidget *widget = (GtkWidget *)window;
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkAllocation child_allocation;

  g_assert (allocation_out != NULL);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = width;
  child_allocation.height = height;

  if (_gtk_widget_get_realized (widget))
    {
      update_realized_window_properties (window);
    }

  priv->title_height = 0;

  if (priv->title_box != NULL &&
      gtk_widget_get_visible (priv->title_box) &&
      gtk_widget_get_child_visible (priv->title_box) &&
      priv->decorated &&
      !priv->fullscreen)
    {
      GtkAllocation title_allocation;

      title_allocation.x = 0;
      title_allocation.y = 0;
      title_allocation.width = width;

      gtk_widget_measure (priv->title_box, GTK_ORIENTATION_VERTICAL,
                          title_allocation.width,
                          NULL, &priv->title_height,
                          NULL, NULL);

      title_allocation.height = priv->title_height;

      gtk_widget_size_allocate (priv->title_box, &title_allocation, -1);
    }

  if (priv->decorated &&
      !priv->fullscreen)
    {
      child_allocation.y += priv->title_height;
      child_allocation.height -= priv->title_height;
    }

  *allocation_out = child_allocation;
}

static void
gtk_window_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child = priv->child;
  GtkAllocation child_allocation;

  _gtk_window_set_allocation (window, width, height, &child_allocation);

  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation, -1);

  gtk_tooltip_maybe_allocate (GTK_NATIVE (widget));
}

static void
update_edge_constraints (GtkWindow      *window,
                         GdkToplevelState  state)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  priv->edge_constraints = (state & GDK_TOPLEVEL_STATE_TOP_TILED) |
                           (state & GDK_TOPLEVEL_STATE_TOP_RESIZABLE) |
                           (state & GDK_TOPLEVEL_STATE_RIGHT_TILED) |
                           (state & GDK_TOPLEVEL_STATE_RIGHT_RESIZABLE) |
                           (state & GDK_TOPLEVEL_STATE_BOTTOM_TILED) |
                           (state & GDK_TOPLEVEL_STATE_BOTTOM_RESIZABLE) |
                           (state & GDK_TOPLEVEL_STATE_LEFT_TILED) |
                           (state & GDK_TOPLEVEL_STATE_LEFT_RESIZABLE);

  priv->tiled = (state & GDK_TOPLEVEL_STATE_TILED) ? 1 : 0;
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GdkToplevelState new_surface_state;
  GdkToplevelState changed_mask;

  new_surface_state = gdk_toplevel_get_state (GDK_TOPLEVEL (priv->surface));
  changed_mask = new_surface_state ^ priv->state;
  priv->state = new_surface_state;

  if (changed_mask & GDK_TOPLEVEL_STATE_FOCUSED)
    {
      gboolean focused = new_surface_state & GDK_TOPLEVEL_STATE_FOCUSED;

      ensure_state_flag_backdrop (widget);

      if (!focused)
        gtk_window_set_mnemonics_visible (window, FALSE);
    }

  if (changed_mask & GDK_TOPLEVEL_STATE_FULLSCREEN)
    {
      priv->fullscreen = (new_surface_state & GDK_TOPLEVEL_STATE_FULLSCREEN) ? TRUE : FALSE;

      g_object_notify_by_pspec (G_OBJECT (widget), window_props[PROP_FULLSCREENED]);
    }

  if (changed_mask & GDK_TOPLEVEL_STATE_MAXIMIZED)
    {
      priv->maximized = (new_surface_state & GDK_TOPLEVEL_STATE_MAXIMIZED) ? TRUE : FALSE;

      g_object_notify_by_pspec (G_OBJECT (widget), window_props[PROP_MAXIMIZED]);
    }

  if (changed_mask & GDK_TOPLEVEL_STATE_SUSPENDED)
    {
      priv->suspended = (new_surface_state & GDK_TOPLEVEL_STATE_SUSPENDED) ? TRUE : FALSE;

      g_object_notify_by_pspec (G_OBJECT (widget), window_props[PROP_SUSPENDED]);
    }

  update_edge_constraints (window, new_surface_state);

  if (changed_mask & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                      GDK_TOPLEVEL_STATE_MAXIMIZED |
                      GDK_TOPLEVEL_STATE_TILED |
                      GDK_TOPLEVEL_STATE_TOP_TILED |
                      GDK_TOPLEVEL_STATE_RIGHT_TILED |
                      GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                      GDK_TOPLEVEL_STATE_LEFT_TILED |
                      GDK_TOPLEVEL_STATE_MINIMIZED))
    {
      update_window_style_classes (window);
      update_window_actions (window);
      gtk_widget_queue_resize (widget);
    }
}

static void
surface_size_changed (GtkWidget *widget,
                      int        width,
                      int        height)
{
  GtkWindow *window = GTK_WINDOW (widget);

  check_scale_changed (GTK_WINDOW (widget));

  if (should_remember_size (window))
    {
      int width_to_remember;
      int height_to_remember;

      width_to_remember = width;
      height_to_remember = height;
      gtk_window_update_csd_size (window,
                                  &width_to_remember, &height_to_remember,
                                  EXCLUDE_CSD_SIZE);
      gtk_window_set_default_size_internal (window,
                                            TRUE, width_to_remember,
                                            TRUE, height_to_remember);
    }

  gtk_widget_queue_allocate (widget);
}

static void
maybe_unset_focus_and_default (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->move_focus)
    {
      GtkWidget *parent;

      parent = _gtk_widget_get_parent (priv->move_focus_widget);

      while (parent)
        {
          if (_gtk_widget_get_visible (parent))
            {
              if (gtk_widget_grab_focus (parent))
                break;
            }

          parent = _gtk_widget_get_parent (parent);
        }

      if (!parent)
        gtk_widget_child_focus (GTK_WIDGET (window), GTK_DIR_TAB_FORWARD);

      priv->move_focus = FALSE;
      g_clear_object (&priv->move_focus_widget);
    }

  if (priv->unset_default)
    gtk_window_set_default_widget (window, NULL);
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkWidget      *widget)
{
  gtk_widget_render (widget, surface, region);

  return TRUE;
}

static void
after_paint (GdkFrameClock *clock,
             GtkWindow     *window)
{
  maybe_unset_focus_and_default (window);
}

static gboolean
surface_event (GdkSurface *surface,
               GdkEvent   *event,
               GtkWidget  *widget)
{
  return gtk_main_do_event (event);
}

static void
gtk_window_real_activate_focus (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->focus_widget && gtk_widget_is_sensitive (priv->focus_widget))
    gtk_widget_activate (priv->focus_widget);
}

static gboolean
gtk_window_has_mnemonic_modifier_pressed (GtkWindow *window)
{
  GList *seats, *s;
  gboolean retval = FALSE;

  seats = gdk_display_list_seats (gtk_widget_get_display (GTK_WIDGET (window)));

  for (s = seats; s; s = s->next)
    {
      GdkDevice *dev = gdk_seat_get_keyboard (s->data);
      GdkModifierType mask;

      mask = gdk_device_get_modifier_state (dev);
      if ((mask & gtk_accelerator_get_default_mod_mask ()) == GDK_ALT_MASK)
        {
          retval = TRUE;
          break;
        }
    }

  g_list_free (seats);

  return retval;
}

static gboolean
gtk_window_handle_focus (GtkWidget *widget,
                         GdkEvent  *event,
                         double     x,
                         double     y)
{
  GtkWindow *window = GTK_WINDOW (widget);

  if (gdk_event_get_event_type (event) != GDK_FOCUS_CHANGE)
    return FALSE;

  if (gdk_focus_event_get_in (event))
    {
      _gtk_window_set_is_active (window, TRUE);

      if (gtk_window_has_mnemonic_modifier_pressed (window))
        _gtk_window_schedule_mnemonics_visible (window);
    }
  else
    {
      _gtk_window_set_is_active (window, FALSE);

      gtk_window_set_mnemonics_visible (window, FALSE);
    }

  return TRUE;
}

static void
update_mnemonics_visible (GtkWindow       *window,
                          guint            keyval,
                          GdkModifierType  state,
                          gboolean         visible)
{
  if ((keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) &&
      ((state & (gtk_accelerator_get_default_mod_mask ()) & ~(GDK_ALT_MASK)) == 0))
    {
      if (visible)
        _gtk_window_schedule_mnemonics_visible (window);
      else
        gtk_window_set_mnemonics_visible (window, FALSE);
    }
}

void
_gtk_window_update_focus_visible (GtkWindow       *window,
                                  guint            keyval,
                                  GdkModifierType  state,
                                  gboolean         visible)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (visible)
    {
      if (priv->focus_visible)
        priv->key_press_focus = NULL;
      else
        priv->key_press_focus = priv->focus_widget;

      if ((keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) &&
               ((state & (gtk_accelerator_get_default_mod_mask ()) & ~(GDK_ALT_MASK)) == 0))
        gtk_window_set_focus_visible (window, TRUE);
    }
  else
    {
      if (priv->key_press_focus == priv->focus_widget)
        gtk_window_set_focus_visible (window, FALSE);
      else
        gtk_window_set_focus_visible (window, TRUE);

      priv->key_press_focus = NULL;
    }
}

static gboolean
gtk_window_key_pressed (GtkWidget       *widget,
                        guint            keyval,
                        guint            keycode,
                        GdkModifierType  state,
                        gpointer         data)
{
  GtkWindow *window = GTK_WINDOW (widget);

  _gtk_window_update_focus_visible (window, keyval, state, TRUE);
  update_mnemonics_visible (window, keyval, state, TRUE);

  return FALSE;
}

static gboolean
gtk_window_key_released (GtkWidget       *widget,
                         guint            keyval,
                         guint            keycode,
                         GdkModifierType  state,
                         gpointer         data)
{
  GtkWindow *window = GTK_WINDOW (widget);

  _gtk_window_update_focus_visible (window, keyval, state, FALSE);
  update_mnemonics_visible (window, keyval, state, FALSE);

  return FALSE;
}

static gboolean
gtk_window_focus (GtkWidget        *widget,
                  GtkDirectionType  direction)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child;
  GtkWidget *old_focus_child;
  GtkWidget *parent;

  old_focus_child = gtk_widget_get_focus_child (widget);

  /* We need a special implementation here to deal properly with wrapping
   * around in the tab chain without the danger of going into an
   * infinite loop.
   */
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
        return TRUE;
    }

  if (priv->focus_widget)
    {
      if (direction == GTK_DIR_LEFT ||
          direction == GTK_DIR_RIGHT ||
          direction == GTK_DIR_UP ||
          direction == GTK_DIR_DOWN)
        {
          return FALSE;
        }

      /* Wrapped off the end, clear the focus setting for the toplpevel */
      parent = _gtk_widget_get_parent (priv->focus_widget);
      while (parent)
        {
          gtk_widget_set_focus_child (parent, NULL);
          parent = _gtk_widget_get_parent (parent);
        }

      gtk_window_set_focus (window, NULL);
    }

  /* Now try to focus the first widget in the window,
   * taking care to hook titlebar widgets into the
   * focus chain.
  */
  if (priv->title_box != NULL &&
      old_focus_child != NULL &&
      priv->title_box != old_focus_child)
    child = priv->title_box;
  else
    child = priv->child;

  if (child)
    {
      if (gtk_widget_child_focus (child, direction))
        return TRUE;
      else if (priv->title_box != NULL &&
               priv->title_box != child &&
               gtk_widget_child_focus (priv->title_box, direction))
        return TRUE;
      else if (priv->title_box == child &&
               gtk_widget_child_focus (priv->child, direction))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_window_move_focus (GtkWidget        *widget,
                       GtkDirectionType  dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!gtk_widget_get_focus_child (widget))
    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
}

void
check_crossing_invariants (GtkWidget       *widget,
                           GtkCrossingData *crossing)
{
#ifdef G_ENABLE_DEBUG
  if (crossing->old_target == NULL)
    {
      g_assert (crossing->old_descendent == NULL);
    }
  else if (crossing->old_descendent == NULL)
    {
      g_assert (crossing->old_target == widget || !gtk_widget_is_ancestor (crossing->old_target, widget));
    }
  else
    {
      g_assert (gtk_widget_get_parent (crossing->old_descendent) == widget);
      g_assert (crossing->old_target == crossing->old_descendent || gtk_widget_is_ancestor (crossing->old_target, crossing->old_descendent));
    }
  if (crossing->new_target == NULL)
    {
      g_assert (crossing->new_descendent == NULL);
    }
  else if (crossing->new_descendent == NULL)
    {
      g_assert (crossing->new_target == widget || !gtk_widget_is_ancestor (crossing->new_target, widget));
    }
  else
    {
      g_assert (gtk_widget_get_parent (crossing->new_descendent) == widget);
      g_assert (crossing->new_target == crossing->new_descendent || gtk_widget_is_ancestor (crossing->new_target, crossing->new_descendent));
    }
#endif
}

static void
synthesize_focus_change_events (GtkWindow       *window,
                                GtkWidget       *old_focus,
                                GtkWidget       *new_focus,
                                GtkCrossingType  type)
{
  GtkCrossingData crossing;
  GtkWidget *ancestor;
  GtkWidget *widget, *focus_child;
  GtkStateFlags flags;
  GtkWidget *prev;
  gboolean seen_ancestor;
  GtkWidgetStack focus_array;
  int i;

  if (old_focus == new_focus)
    return;

  if (old_focus && new_focus)
    ancestor = gtk_widget_common_ancestor (old_focus, new_focus);
  else
    ancestor = NULL;

  flags = GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_FOCUS_WITHIN;
  if (gtk_window_get_focus_visible (GTK_WINDOW (window)))
    flags |= GTK_STATE_FLAG_FOCUS_VISIBLE;

  crossing.type = type;
  crossing.mode = GDK_CROSSING_NORMAL;
  crossing.old_target = old_focus ? g_object_ref (old_focus) : NULL;
  crossing.old_descendent = NULL;
  crossing.new_target = new_focus ? g_object_ref (new_focus) : NULL;
  crossing.new_descendent = NULL;

  crossing.direction = GTK_CROSSING_OUT;

  prev = NULL;
  seen_ancestor = FALSE;
  widget = old_focus;
  while (widget)
    {
      crossing.old_descendent = prev;
      if (seen_ancestor)
        {
          crossing.new_descendent = new_focus ? prev : NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.new_descendent = NULL;
          for (w = new_focus; w != ancestor; w = gtk_widget_get_parent (w))
            crossing.new_descendent = w;

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.new_descendent = NULL;
        }

      check_crossing_invariants (widget, &crossing);
      gtk_widget_handle_crossing (widget, &crossing, 0, 0);
      gtk_widget_unset_state_flags (widget, flags);
      gtk_widget_set_focus_child (widget, NULL);
      prev = widget;
      widget = gtk_widget_get_parent (widget);

      flags = flags & ~GTK_STATE_FLAG_FOCUSED;
    }

  flags = GTK_STATE_FLAG_FOCUS_WITHIN;
  if (gtk_window_get_focus_visible (GTK_WINDOW (window)))
    flags |= GTK_STATE_FLAG_FOCUS_VISIBLE;

  gtk_widget_stack_init (&focus_array);
  for (widget = new_focus; widget; widget = _gtk_widget_get_parent (widget))
    gtk_widget_stack_append (&focus_array, g_object_ref (widget));

  crossing.direction = GTK_CROSSING_IN;

  seen_ancestor = FALSE;
  for (i = gtk_widget_stack_get_size (&focus_array) - 1; i >= 0; i--)
    {
      widget = gtk_widget_stack_get (&focus_array, i);

      if (i > 0)
        focus_child = gtk_widget_stack_get (&focus_array, i - 1);
      else
        focus_child = NULL;

      crossing.new_descendent = focus_child;
      if (seen_ancestor)
        {
          crossing.old_descendent = NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.old_descendent = NULL;
          for (w = old_focus; w != ancestor; w = gtk_widget_get_parent (w))
            {
              crossing.old_descendent = w;
            }

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.old_descendent = (old_focus && ancestor) ? focus_child : NULL;
        }

      check_crossing_invariants (widget, &crossing);
      gtk_widget_handle_crossing (widget, &crossing, 0, 0);

      if (i == 0)
        flags = flags | GTK_STATE_FLAG_FOCUSED;

      gtk_widget_set_state_flags (widget, flags, FALSE);
      gtk_widget_set_focus_child (widget, focus_child);
    }

  g_clear_object (&crossing.old_target);
  g_clear_object (&crossing.new_target);

  gtk_widget_stack_clear (&focus_array);
}

/**
 * gtk_window_set_focus: (attributes org.gtk.Method.set_property=focus-widget)
 * @window: a `GtkWindow`
 * @focus: (nullable): widget to be the new focus widget, or %NULL to unset
 *   any focus widget for the toplevel window.
 *
 * Sets the focus widget.
 *
 * If @focus is not the current focus widget, and is focusable,
 * sets it as the focus widget for the window. If @focus is %NULL,
 * unsets the focus widget for this window. To set the focus to a
 * particular widget in the toplevel, it is usually more convenient
 * to use [method@Gtk.Widget.grab_focus] instead of this function.
 */
void
gtk_window_set_focus (GtkWindow *window,
                      GtkWidget *focus)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (focus)
    gtk_widget_grab_focus (focus);
  else
    gtk_window_root_set_focus (GTK_ROOT (window), NULL);
}

static void
gtk_window_css_changed (GtkWidget         *widget,
                        GtkCssStyleChange *change)
{
  GtkWindow *window = GTK_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_window_parent_class)->css_changed (widget, change);

  if (!_gtk_widget_get_alloc_needed (widget) &&
      (change == NULL || gtk_css_style_change_changes_property (change, GTK_CSS_PROPERTY_BACKGROUND_COLOR)))
    update_opaque_region (window);
}

/*
 * _gtk_window_unset_focus_and_default:
 * @window: a `GtkWindow`
 * @widget: a widget inside of @window
 *
 * Checks whether the focus and default widgets of @window are
 * @widget or a descendent of @widget, and if so, unset them.
 */
void
_gtk_window_unset_focus_and_default (GtkWindow *window,
                                     GtkWidget *widget)

{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *child;

  child = priv->focus_widget;
  if (child && (child == widget || gtk_widget_is_ancestor (child, widget)))
    {
      priv->move_focus_widget = g_object_ref (widget);
      priv->move_focus = TRUE;
    }

  child = priv->default_widget;
  if (child && (child == widget || gtk_widget_is_ancestor (child, widget)))
    priv->unset_default = TRUE;

  if ((priv->move_focus || priv->unset_default) &&
      priv->surface != NULL)
    {
      GdkFrameClock *frame_clock;

      frame_clock = gdk_surface_get_frame_clock (priv->surface);
      gdk_frame_clock_request_phase (frame_clock,
                                     GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
    }
}

#undef INCLUDE_CSD_SIZE
#undef EXCLUDE_CSD_SIZE

static void
_gtk_window_present (GtkWindow *window,
                     guint32    timestamp)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget = GTK_WIDGET (window);

  if (gtk_widget_get_visible (widget))
    {
      /* Translate a timestamp of GDK_CURRENT_TIME appropriately */
      if (timestamp == GDK_CURRENT_TIME)
        {
#ifdef GDK_WINDOWING_X11
          if (GDK_IS_X11_SURFACE (priv->surface))
            {
              GdkDisplay *display = gtk_widget_get_display (widget);
              timestamp = gdk_x11_display_get_user_time (display);
            }
          else
#endif
            timestamp = gtk_get_current_event_time ();
        }
    }
  else
    {
      priv->initial_timestamp = timestamp;
      priv->in_present = TRUE;
      gtk_widget_set_visible (widget, TRUE);
      priv->in_present = FALSE;
    }

  gdk_toplevel_focus (GDK_TOPLEVEL (priv->surface), timestamp);
  gtk_window_notify_startup (window);
}

/**
 * gtk_window_set_startup_id: (attributes org.gtk.Method.set_property=startup-id)
 * @window: a `GtkWindow`
 * @startup_id: a string with startup-notification identifier
 *
 * Sets the startup notification ID.
 *
 * Startup notification identifiers are used by desktop environment
 * to track application startup, to provide user feedback and other
 * features. This function changes the corresponding property on the
 * underlying `GdkSurface`.
 *
 * Normally, startup identifier is managed automatically and you should
 * only use this function in special cases like transferring focus from
 * other processes. You should use this function before calling
 * [method@Gtk.Window.present] or any equivalent function generating
 * a window map event.
 *
 * This function is only useful on X11, not with other GTK targets.
 */
void
gtk_window_set_startup_id (GtkWindow   *window,
                           const char *startup_id)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);

  g_free (priv->startup_id);
  priv->startup_id = g_strdup (startup_id);

  if (_gtk_widget_get_realized (widget))
    {
      guint32 timestamp = extract_time_from_startup_id (priv->startup_id);

#ifdef GDK_WINDOWING_X11
      if (timestamp != GDK_CURRENT_TIME && GDK_IS_X11_SURFACE (priv->surface))
        gdk_x11_surface_set_user_time (priv->surface, timestamp);
#endif

      /* Here we differentiate real and "fake" startup notification IDs,
       * constructed on purpose just to pass interaction timestamp
       */
      if (startup_id_is_fake (priv->startup_id))
        _gtk_window_present (window, timestamp);
      else
        {
          /* If window is mapped, terminate the startup-notification */
          if (_gtk_widget_get_mapped (widget) && !disable_startup_notification)
            gdk_toplevel_set_startup_id (GDK_TOPLEVEL (priv->surface), priv->startup_id);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_STARTUP_ID]);
}

/**
 * gtk_window_present:
 * @window: a `GtkWindow`
 *
 * Presents a window to the user.
 *
 * This may mean raising the window in the stacking order,
 * unminimizing it, moving it to the current desktop and/or
 * giving it the keyboard focus (possibly dependent on the user’s
 * platform, window manager and preferences).
 *
 * If @window is hidden, this function also makes it visible.
 */
void
gtk_window_present (GtkWindow *window)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  _gtk_window_present (window, GDK_CURRENT_TIME);
}

/**
 * gtk_window_present_with_time:
 * @window: a `GtkWindow`
 * @timestamp: the timestamp of the user interaction (typically a
 *   button or key press event) which triggered this call
 *
 * Presents a window to the user in response to an user interaction.
 *
 * See [method@Gtk.Window.present] for more details.
 *
 * The timestamp should be gathered when the window was requested
 * to be shown (when clicking a link for example), rather than once
 * the window is ready to be shown.
 *
 * Deprecated: 4.14: Use gtk_window_present()
 */
void
gtk_window_present_with_time (GtkWindow *window,
                              guint32    timestamp)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  _gtk_window_present (window, timestamp);
}

/**
 * gtk_window_minimize:
 * @window: a `GtkWindow`
 *
 * Asks to minimize the specified @window.
 *
 * Note that you shouldn’t assume the window is definitely minimized
 * afterward, because the windowing system might not support this
 * functionality; other entities (e.g. the user or the window manager)
 * could unminimize it again, or there may not be a window manager in
 * which case minimization isn’t possible, etc.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be minimized before it ever appears
 * onscreen.
 *
 * You can track result of this operation via the
 * [property@Gdk.Toplevel:state] property.
 */
void
gtk_window_minimize (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv->minimize_initially = TRUE;

  if (priv->surface)
    gdk_toplevel_minimize (GDK_TOPLEVEL (priv->surface));
}

/**
 * gtk_window_unminimize:
 * @window: a `GtkWindow`
 *
 * Asks to unminimize the specified @window.
 *
 * Note that you shouldn’t assume the window is definitely unminimized
 * afterward, because the windowing system might not support this
 * functionality; other entities (e.g. the user or the window manager)
 * could minimize it again, or there may not be a window manager in
 * which case minimization isn’t possible, etc.
 *
 * You can track result of this operation via the
 * [property@Gdk.Toplevel:state] property.
 */
void
gtk_window_unminimize (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  priv->minimize_initially = FALSE;

  gtk_window_update_toplevel (window,
                              gtk_window_compute_base_layout (window));
}

/**
 * gtk_window_maximize:
 * @window: a `GtkWindow`
 *
 * Asks to maximize @window, so that it fills the screen.
 *
 * Note that you shouldn’t assume the window is definitely maximized
 * afterward, because other entities (e.g. the user or window manager)
 * could unmaximize it again, and not all window managers support
 * maximization.
 *
 * It’s permitted to call this function before showing a window,
 * in which case the window will be maximized when it appears onscreen
 * initially.
 *
 * You can track the result of this operation via the
 * [property@Gdk.Toplevel:state] property, or by listening to
 * notifications on the [property@Gtk.Window:maximized]
 * property.
 */
void
gtk_window_maximize (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      GdkToplevelLayout *layout;

      layout = gtk_window_compute_base_layout (window);
      gdk_toplevel_layout_set_maximized (layout, TRUE);
      gtk_window_update_toplevel (window, layout);
    }
  else if (!priv->maximized)
    {
      priv->maximized = TRUE;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MAXIMIZED]);
    }
}

/**
 * gtk_window_unmaximize:
 * @window: a `GtkWindow`
 *
 * Asks to unmaximize @window.
 *
 * Note that you shouldn’t assume the window is definitely unmaximized
 * afterward, because other entities (e.g. the user or window manager)
 * maximize it again, and not all window managers honor requests to
 * unmaximize.
 *
 * You can track the result of this operation via the
 * [property@Gdk.Toplevel:state] property, or by listening to
 * notifications on the [property@Gtk.Window:maximized] property.
 */
void
gtk_window_unmaximize (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      GdkToplevelLayout *layout;

      layout = gtk_window_compute_base_layout (window);
      gdk_toplevel_layout_set_maximized (layout, FALSE);
      gtk_window_update_toplevel (window, layout);
    }
  else if (priv->maximized)
    {
      priv->maximized = FALSE;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MAXIMIZED]);
    }
}

static void
unset_fullscreen_monitor (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->initial_fullscreen_monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->initial_fullscreen_monitor, unset_fullscreen_monitor, window);
      g_object_unref (priv->initial_fullscreen_monitor);
      priv->initial_fullscreen_monitor = NULL;
    }
}

/**
 * gtk_window_fullscreen:
 * @window: a `GtkWindow`
 *
 * Asks to place @window in the fullscreen state.
 *
 * Note that you shouldn’t assume the window is definitely fullscreen
 * afterward, because other entities (e.g. the user or window manager)
 * unfullscreen it again, and not all window managers honor requests
 * to fullscreen windows.
 *
 * You can track the result of this operation via the
 * [property@Gdk.Toplevel:state] property, or by listening to
 * notifications of the [property@Gtk.Window:fullscreened] property.
 */
void
gtk_window_fullscreen (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  unset_fullscreen_monitor (window);

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      GdkToplevelLayout *layout;

      layout = gtk_window_compute_base_layout (window);
      gdk_toplevel_layout_set_fullscreen (layout, TRUE, NULL);
      gtk_window_update_toplevel (window, layout);
    }
  else if (!priv->fullscreen)
    {
      priv->fullscreen = TRUE;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FULLSCREENED]);
    }
}

/**
 * gtk_window_fullscreen_on_monitor:
 * @window: a `GtkWindow`
 * @monitor: which monitor to go fullscreen on
 *
 * Asks to place @window in the fullscreen state on the given @monitor.
 *
 * Note that you shouldn't assume the window is definitely fullscreen
 * afterward, or that the windowing system allows fullscreen windows on
 * any given monitor.
 *
 * You can track the result of this operation via the
 * [property@Gdk.Toplevel:state] property, or by listening to
 * notifications of the [property@Gtk.Window:fullscreened] property.
 */
void
gtk_window_fullscreen_on_monitor (GtkWindow  *window,
                                  GdkMonitor *monitor)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_MONITOR (monitor));
  g_return_if_fail (gdk_monitor_is_valid (monitor));

  gtk_window_set_display (window, gdk_monitor_get_display (monitor));

  unset_fullscreen_monitor (window);
  priv->initial_fullscreen_monitor = monitor;
  g_signal_connect_swapped (priv->initial_fullscreen_monitor, "invalidate",
                            G_CALLBACK (unset_fullscreen_monitor), window);
  g_object_ref (priv->initial_fullscreen_monitor);

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      GdkToplevelLayout *layout;

      layout = gtk_window_compute_base_layout (window);
      gdk_toplevel_layout_set_fullscreen (layout, TRUE, monitor);
      gtk_window_update_toplevel (window, layout);
    }
  else if (!priv->fullscreen)
    {
      priv->fullscreen = TRUE;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FULLSCREENED]);
    }
}

/**
 * gtk_window_unfullscreen:
 * @window: a `GtkWindow`
 *
 * Asks to remove the fullscreen state for @window, and return to
 * its previous state.
 *
 * Note that you shouldn’t assume the window is definitely not
 * fullscreen afterward, because other entities (e.g. the user or
 * window manager) could fullscreen it again, and not all window
 * managers honor requests to unfullscreen windows; normally the
 * window will end up restored to its normal state. Just don’t
 * write code that crashes if not.
 *
 * You can track the result of this operation via the
 * [property@Gdk.Toplevel:state] property, or by listening to
 * notifications of the [property@Gtk.Window:fullscreened] property.
 */
void
gtk_window_unfullscreen (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  unset_fullscreen_monitor (window);

  if (_gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      GdkToplevelLayout *layout;

      layout = gtk_window_compute_base_layout (window);
      gdk_toplevel_layout_set_fullscreen (layout, FALSE, NULL);
      gtk_window_update_toplevel (window, layout);
    }
  else if (priv->fullscreen)
    {
      priv->fullscreen = FALSE;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FULLSCREENED]);
    }
}

/**
 * gtk_window_set_resizable: (attributes org.gtk.Method.set_property=resizable)
 * @window: a `GtkWindow`
 * @resizable: %TRUE if the user can resize this window
 *
 * Sets whether the user can resize a window.
 *
 * Windows are user resizable by default.
 **/
void
gtk_window_set_resizable (GtkWindow *window,
                          gboolean   resizable)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  resizable = (resizable != FALSE);

  if (priv->resizable != resizable)
    {
      priv->resizable = resizable;

      update_window_actions (window);

      gtk_window_update_toplevel (window,
                                  gtk_window_compute_base_layout (window));

      gtk_widget_queue_resize (GTK_WIDGET (window));

      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_RESIZABLE]);
    }
}

/**
 * gtk_window_get_resizable: (attributes org.gtk.Method.get_property=resizable)
 * @window: a `GtkWindow`
 *
 * Gets the value set by gtk_window_set_resizable().
 *
 * Returns: %TRUE if the user can resize the window
 **/
gboolean
gtk_window_get_resizable (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->resizable;
}

/**
 * gtk_window_set_display: (attributes org.gtk.Method.set_property=display)
 * @window: a `GtkWindow`
 * @display: a `GdkDisplay`
 *
 * Sets the `GdkDisplay` where the @window is displayed.
 *
 * If the window is already mapped, it will be unmapped,
 * and then remapped on the new display.
 */
void
gtk_window_set_display (GtkWindow  *window,
		        GdkDisplay *display)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkWidget *widget;
  gboolean was_mapped;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (display == priv->display)
    return;

  /* reset initial_fullscreen_monitor since they are relative to the screen */
  unset_fullscreen_monitor (window);

  widget = GTK_WIDGET (window);

  was_mapped = _gtk_widget_get_mapped (widget);

  if (was_mapped)
    gtk_widget_unmap (widget);
  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  if (priv->transient_parent && gtk_widget_get_display (GTK_WIDGET (priv->transient_parent)) != display)
    gtk_window_set_transient_for (window, NULL);

#ifdef GDK_WINDOWING_X11
  g_signal_handlers_disconnect_by_func (gtk_settings_get_for_display (priv->display),
                                        gtk_window_on_theme_variant_changed, window);
  g_signal_connect (gtk_settings_get_for_display (display),
                    "notify::gtk-application-prefer-dark-theme",
                    G_CALLBACK (gtk_window_on_theme_variant_changed), window);
#endif

  gtk_widget_unroot (widget);
  priv->display = display;

  gtk_widget_root (widget);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_DISPLAY]);

  if (was_mapped)
    gtk_widget_map (widget);

  check_scale_changed (window);

  gtk_widget_system_setting_changed (GTK_WIDGET (window), GTK_SYSTEM_SETTING_DISPLAY);
}

static void
gtk_window_set_theme_variant (GtkWindow *window)
{
#ifdef GDK_WINDOWING_X11
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean   dark_theme_requested;

  g_object_get (gtk_settings_get_for_display (priv->display),
                "gtk-application-prefer-dark-theme", &dark_theme_requested,
                NULL);

  if (GDK_IS_X11_SURFACE (priv->surface))
    gdk_x11_surface_set_theme_variant (priv->surface,
                                       dark_theme_requested ? "dark" : NULL);
#endif
}

#ifdef GDK_WINDOWING_X11
static void
gtk_window_on_theme_variant_changed (GtkSettings *settings,
                                     GParamSpec  *pspec,
                                     GtkWindow   *window)
{
  gtk_window_set_theme_variant (window);
}
#endif

/**
 * gtk_window_is_active: (attributes org.gtk.Method.get_property=is-active)
 * @window: a `GtkWindow`
 *
 * Returns whether the window is part of the current active toplevel.
 *
 * The active toplevel is the window receiving keystrokes.
 *
 * The return value is %TRUE if the window is active toplevel itself.
 * You might use this function if you wanted to draw a widget
 * differently in an active window from a widget in an inactive window.
 *
 * Returns: %TRUE if the window part of the current active window.
 */
gboolean
gtk_window_is_active (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->is_active;
}

/**
 * gtk_window_get_group:
 * @window: (nullable): a `GtkWindow`
 *
 * Returns the group for @window.
 *
 * If the window has no group, then the default group is returned.
 *
 * Returns: (transfer none): the `GtkWindowGroup` for a window
 *   or the default group
 */
GtkWindowGroup *
gtk_window_get_group (GtkWindow *window)
{
  static GtkWindowGroup *default_group = NULL;

  if (window)
    {
      GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

      if (priv->group)
        return priv->group;
    }

  if (!default_group)
    default_group = gtk_window_group_new ();

  return default_group;
}

/**
 * gtk_window_has_group:
 * @window: a `GtkWindow`
 *
 * Returns whether @window has an explicit window group.
 *
 * Returns: %TRUE if @window has an explicit window group.
 */
gboolean
gtk_window_has_group (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->group != NULL;
}

GtkWindowGroup *
_gtk_window_get_window_group (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  return priv->group;
}

void
_gtk_window_set_window_group (GtkWindow      *window,
                              GtkWindowGroup *group)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  priv->group = group;
}

static gboolean
gtk_window_activate_menubar (GtkWidget *widget,
                             GVariant  *args,
                             gpointer   unused)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *tmp_menubars, *l;
  GPtrArray *menubars;
  GtkWidget *focus;
  GtkWidget *first;

  tmp_menubars = gtk_popover_menu_bar_get_viewable_menu_bars (window);
  if (tmp_menubars == NULL)
    {
      focus = gtk_window_get_focus (window);
      return priv->title_box != NULL &&
             (focus == NULL || !gtk_widget_is_ancestor (focus, priv->title_box)) &&
             gtk_widget_child_focus (priv->title_box, GTK_DIR_TAB_FORWARD);
    }

  menubars = g_ptr_array_sized_new (g_list_length (tmp_menubars));;
  for (l = tmp_menubars; l; l = l->next)
    g_ptr_array_add (menubars, l->data);

  g_list_free (tmp_menubars);

  gtk_widget_focus_sort (GTK_WIDGET (window), GTK_DIR_TAB_FORWARD, menubars);

  first = g_ptr_array_index (menubars, 0);
  if (GTK_IS_POPOVER_MENU_BAR (first))
    gtk_popover_menu_bar_select_first (GTK_POPOVER_MENU_BAR (first));
  else if (GTK_IS_MENU_BUTTON (first))
    gtk_menu_button_popup (GTK_MENU_BUTTON (first));

  g_ptr_array_free (menubars, TRUE);

  return TRUE;
}

static void
gtk_window_keys_changed (GtkWindow *window)
{
}

/*
 * _gtk_window_set_is_active:
 * @window: a `GtkWindow`
 * @is_active: %TRUE if the window is in the currently active toplevel
 *
 * Internal function that sets whether the `GtkWindow` is part
 * of the currently active toplevel window (taking into account
 * inter-process embedding.)
 */
static void
_gtk_window_set_is_active (GtkWindow *window,
                           gboolean   is_active)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  if (priv->is_active == is_active)
    return;

  priv->is_active = is_active;

  if (priv->focus_widget)
    {
      GtkWidget *focus;

      focus = g_object_ref (priv->focus_widget);

      if (is_active)
        {
          synthesize_focus_change_events (window, NULL, focus, GTK_CROSSING_ACTIVE);
          gtk_widget_set_has_focus (focus, TRUE);
        }
      else
        {
          synthesize_focus_change_events (window, focus, NULL, GTK_CROSSING_ACTIVE);
          gtk_widget_set_has_focus (focus, FALSE);
        }

      g_object_unref (focus);
    }

  gtk_accessible_platform_changed (GTK_ACCESSIBLE (window), GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_IS_ACTIVE]);
}

/**
 * gtk_window_set_auto_startup_notification:
 * @setting: %TRUE to automatically do startup notification
 *
 * Sets whether the window should request startup notification.
 *
 * By default, after showing the first `GtkWindow`, GTK calls
 * [method@Gdk.Toplevel.set_startup_id]. Call this function
 * to disable the automatic startup notification. You might do this
 * if your first window is a splash screen, and you want to delay
 * notification until after your real main window has been shown,
 * for example.
 *
 * In that example, you would disable startup notification
 * temporarily, show your splash screen, then re-enable it so that
 * showing the main window would automatically result in notification.
 */
void
gtk_window_set_auto_startup_notification (gboolean setting)
{
  disable_startup_notification = !setting;
}

/**
 * gtk_window_get_mnemonics_visible: (attributes org.gtk.Method.get_property=mnemonics-visible)
 * @window: a `GtkWindow`
 *
 * Gets whether mnemonics are supposed to be visible.
 *
 * Returns: %TRUE if mnemonics are supposed to be visible
 *   in this window.
 */
gboolean
gtk_window_get_mnemonics_visible (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->mnemonics_visible;
}

/**
 * gtk_window_set_mnemonics_visible: (attributes org.gtk.Method.set_property=mnemonics-visible)
 * @window: a `GtkWindow`
 * @setting: the new value
 *
 * Sets whether mnemonics are supposed to be visible.
 *
 * This property is maintained by GTK based on user input,
 * and should not be set by applications.
 */
void
gtk_window_set_mnemonics_visible (GtkWindow *window,
                                  gboolean   setting)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  setting = setting != FALSE;

  if (priv->mnemonics_visible != setting)
    {
      priv->mnemonics_visible = setting;
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_MNEMONICS_VISIBLE]);
    }

  if (priv->mnemonics_display_timeout_id)
    {
      g_source_remove (priv->mnemonics_display_timeout_id);
      priv->mnemonics_display_timeout_id = 0;
    }
}

static gboolean
schedule_mnemonics_visible_cb (gpointer data)
{
  GtkWindow *window = data;
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  priv->mnemonics_display_timeout_id = 0;

  gtk_window_set_mnemonics_visible (window, TRUE);

  return FALSE;
}

void
_gtk_window_schedule_mnemonics_visible (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  if (priv->mnemonics_display_timeout_id)
    return;

  priv->mnemonics_display_timeout_id =
    g_timeout_add (MNEMONICS_DELAY, schedule_mnemonics_visible_cb, window);
  gdk_source_set_static_name_by_id (priv->mnemonics_display_timeout_id, "[gtk] schedule_mnemonics_visible_cb");
}

/**
 * gtk_window_get_focus_visible: (attributes org.gtk.Method.get_property=focus-visible)
 * @window: a `GtkWindow`
 *
 * Gets whether “focus rectangles” are supposed to be visible.
 *
 * Returns: %TRUE if “focus rectangles” are supposed to be visible
 *   in this window.
 */
gboolean
gtk_window_get_focus_visible (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return priv->focus_visible;
}

static gboolean
unset_focus_visible (gpointer data)
{
  GtkWindow *window = data;
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  priv->focus_visible_timeout = 0;

  gtk_window_set_focus_visible (window, FALSE);

  return G_SOURCE_REMOVE;
}

/**
 * gtk_window_set_focus_visible: (attributes org.gtk.Method.set_property=focus-visible)
 * @window: a `GtkWindow`
 * @setting: the new value
 *
 * Sets whether “focus rectangles” are supposed to be visible.
 *
 * This property is maintained by GTK based on user input,
 * and should not be set by applications.
 */
void
gtk_window_set_focus_visible (GtkWindow *window,
                              gboolean   setting)
{
  gboolean changed;

  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));

  changed = priv->focus_visible != setting;

  priv->focus_visible = setting;

  if (priv->focus_visible_timeout)
    {
      g_source_remove (priv->focus_visible_timeout);
      priv->focus_visible_timeout = 0;
    }

  if (priv->focus_visible)
    {
      priv->focus_visible_timeout = g_timeout_add_seconds (VISIBLE_FOCUS_DURATION, unset_focus_visible, window);
      gdk_source_set_static_name_by_id (priv->focus_visible_timeout, "[gtk] unset_focus_visible");
    }

  if (changed)
    {
      if (priv->focus_widget)
        {
          GtkWidget *widget;

          for (widget = priv->focus_widget; widget; widget = gtk_widget_get_parent (widget))
            {
              if (priv->focus_visible)
                gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_FOCUS_VISIBLE, FALSE);
              else
                gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_FOCUS_VISIBLE);
            }
        }
      g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_FOCUS_VISIBLE]);
    }
}

static void
ensure_state_flag_backdrop (GtkWidget *widget)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (GTK_WINDOW (widget));
  gboolean surface_focused = TRUE;

  surface_focused = gdk_toplevel_get_state (GDK_TOPLEVEL (priv->surface)) & GDK_TOPLEVEL_STATE_FOCUSED;

  if (!surface_focused)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_BACKDROP, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
}

static void set_warn_again (gboolean warn);
static void gtk_window_set_debugging (GdkDisplay *display,
                                      gboolean    enable,
                                      gboolean    toggle,
                                      gboolean    select,
                                      gboolean    warn);

static void
warn_response (GtkDialog *dialog,
               int        response)
{
  GtkWidget *check;
  gboolean remember;
  GtkWidget *inspector_window;
  GdkDisplay *display;

  inspector_window = GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (dialog)));
  display = gtk_inspector_window_get_inspected_display (GTK_INSPECTOR_WINDOW (inspector_window));

  check = g_object_get_data (G_OBJECT (dialog), "check");
  remember = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

  gtk_window_destroy (GTK_WINDOW (dialog));
  g_object_set_data (G_OBJECT (inspector_window), "warning_dialog", NULL);

  if (response == GTK_RESPONSE_NO)
    gtk_window_set_debugging (display, FALSE, FALSE, FALSE, FALSE);
  else
    set_warn_again (!remember);
}

static void
gtk_window_set_debugging (GdkDisplay *display,
                          gboolean    enable,
                          gboolean    toggle,
                          gboolean    select,
                          gboolean    warn)
{
  GtkWidget *dialog = NULL;
  GtkWidget *area;
  GtkWidget *check;
  GtkWidget *inspector_window;
  gboolean was_debugging;

  was_debugging = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (display), "-gtk-debugging-enabled"));

  if (toggle)
    enable = !was_debugging;

  g_object_set_data (G_OBJECT (display), "-gtk-debugging-enabled", GINT_TO_POINTER (enable));

  if (enable)
    {
      inspector_window = gtk_inspector_window_get (display);

      gtk_window_present (GTK_WINDOW (inspector_window));

      if (warn)
        {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          dialog = gtk_message_dialog_new (GTK_WINDOW (inspector_window),
                                           GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           _("Do you want to use GTK Inspector?"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
              _("GTK Inspector is an interactive debugger that lets you explore and "
                "modify the internals of any GTK application. Using it may cause the "
                "application to break or crash."));

          area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
          check = gtk_check_button_new_with_label (_("Don’t show this message again"));
          gtk_widget_set_margin_start (check, 10);
          gtk_box_append (GTK_BOX (area), check);
          g_object_set_data (G_OBJECT (dialog), "check", check);
          gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_NO);
          gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_YES);
          g_signal_connect (dialog, "response", G_CALLBACK (warn_response), inspector_window);
          g_object_set_data (G_OBJECT (inspector_window), "warning_dialog", dialog);

          gtk_window_present (GTK_WINDOW (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
        }

      if (select)
        gtk_inspector_window_select_widget_under_pointer (GTK_INSPECTOR_WINDOW (inspector_window));
    }
  else if (was_debugging)
    {
      inspector_window = gtk_inspector_window_get (display);

      gtk_widget_set_visible (inspector_window, FALSE);
    }
}

/**
 * gtk_window_set_interactive_debugging:
 * @enable: %TRUE to enable interactive debugging
 *
 * Opens or closes the [interactive debugger](running.html#interactive-debugging).
 *
 * The debugger offers access to the widget hierarchy of the application
 * and to useful debugging tools.
 *
 * This function allows applications that already use
 * <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd>
 * (or <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd>)
 * for their own key shortcuts to add a different shortcut to open the Inspector.
 *
 * If you are not overriding the default key shortcuts for the Inspector,
 * you should not use this function.
 */
void
gtk_window_set_interactive_debugging (gboolean enable)
{
  GdkDisplay *display = gdk_display_get_default ();

  gtk_window_set_debugging (display, enable, FALSE, FALSE, FALSE);
}

static gboolean
inspector_keybinding_enabled (gboolean *warn)
{
  GSettingsSchema *schema;
  GSettings *settings;
  gboolean enabled;

  enabled = TRUE;
  *warn = TRUE;

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gtk.gtk4.Settings.Debug",
                                            TRUE);

  if (schema)
    {
      settings = g_settings_new_full (schema, NULL, NULL);
      enabled = g_settings_get_boolean (settings, "enable-inspector-keybinding");
      *warn = g_settings_get_boolean (settings, "inspector-warning");
      g_object_unref (settings);
      g_settings_schema_unref (schema);
    }

  return enabled;
}

static void
set_warn_again (gboolean warn)
{
  GSettingsSchema *schema;
  GSettings *settings;

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gtk.gtk4.Settings.Debug",
                                            TRUE);

  if (schema)
    {
      settings = g_settings_new_full (schema, NULL, NULL);
      g_settings_set_boolean (settings, "inspector-warning", warn);
      g_object_unref (settings);
      g_settings_schema_unref (schema);
    }
}

static gboolean
gtk_window_enable_debugging (GtkWindow *window,
                             gboolean   toggle)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  gboolean warn;

  if (!inspector_keybinding_enabled (&warn))
    return FALSE;

  gtk_window_set_debugging (priv->display, TRUE, toggle, !toggle, warn);

  return TRUE;
}

typedef struct {
  GtkWindow *window;
  GtkWindowHandleExported callback;
  gpointer user_data;
} ExportHandleData;

static char *
prefix_handle (GdkDisplay *display,
               char       *handle)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return g_strconcat ("wayland:", handle, NULL);
  else
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    return g_strconcat ("x11:", handle, NULL);
  else
#endif
    return NULL;
}

static const char *
unprefix_handle (const char *handle)
{
  if (g_str_has_prefix (handle, "wayland:"))
    return handle + strlen ("wayland:");
  else if (g_str_has_prefix (handle, "x11:"))
    return handle + strlen ("x1!:");
  else
    return handle;
}

static void
export_handle_done (GObject      *source,
                    GAsyncResult *result,
                    void         *user_data)
{
  ExportHandleData *data = (ExportHandleData *)user_data;
  GtkWindowPrivate *priv = gtk_window_get_instance_private (data->window);
  char *handle;

  handle = gdk_toplevel_export_handle_finish (GDK_TOPLEVEL (priv->surface), result, NULL);
  if (handle)
    {
      char *prefixed;

      prefixed = prefix_handle (priv->display, handle);
      data->callback (data->window, prefixed, data->user_data);
      g_free (prefixed);
      g_free (handle);
    }
  else
    data->callback (data->window, NULL, data->user_data);


  g_free (data);
}

gboolean
gtk_window_export_handle (GtkWindow               *window,
                          GtkWindowHandleExported  callback,
                          gpointer                 user_data)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  ExportHandleData *data;

  data = g_new (ExportHandleData, 1);
  data->window = window;
  data->callback = callback;
  data->user_data = user_data;

  gdk_toplevel_export_handle (GDK_TOPLEVEL (priv->surface), NULL, export_handle_done, data);

  return TRUE;
}

void
gtk_window_unexport_handle (GtkWindow  *window,
                            const char *handle)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  gdk_toplevel_unexport_handle (GDK_TOPLEVEL (priv->surface), unprefix_handle (handle));
}

static GtkPointerFocus *
gtk_window_lookup_pointer_focus (GtkWindow        *window,
                                 GdkDevice        *device,
                                 GdkEventSequence *sequence)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;

      if (focus->device == device && focus->sequence == sequence)
        return focus;
    }

  return NULL;
}

GtkWidget *
gtk_window_lookup_pointer_focus_widget (GtkWindow        *window,
                                        GdkDevice        *device,
                                        GdkEventSequence *sequence)
{
  GtkPointerFocus *focus;

  focus = gtk_window_lookup_pointer_focus (window, device, sequence);
  return focus ? gtk_pointer_focus_get_target (focus) : NULL;
}

GtkWidget *
gtk_window_lookup_effective_pointer_focus_widget (GtkWindow        *window,
                                                  GdkDevice        *device,
                                                  GdkEventSequence *sequence)
{
  GtkPointerFocus *focus;

  focus = gtk_window_lookup_pointer_focus (window, device, sequence);
  return focus ? gtk_pointer_focus_get_effective_target (focus) : NULL;
}

GtkWidget *
gtk_window_lookup_pointer_focus_implicit_grab (GtkWindow        *window,
                                               GdkDevice        *device,
                                               GdkEventSequence *sequence)
{
  GtkPointerFocus *focus;

  focus = gtk_window_lookup_pointer_focus (window, device, sequence);
  return focus ? gtk_pointer_focus_get_implicit_grab (focus) : NULL;
}

void
gtk_window_update_pointer_focus (GtkWindow        *window,
                                 GdkDevice        *device,
                                 GdkEventSequence *sequence,
                                 GtkWidget        *target,
                                 double            x,
                                 double            y)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkPointerFocus *focus;

  focus = gtk_window_lookup_pointer_focus (window, device, sequence);
  if (focus)
    {
      gtk_pointer_focus_ref (focus);

      if (target)
        {
          gtk_pointer_focus_set_target (focus, target);
          gtk_pointer_focus_set_coordinates (focus, x, y);
        }
      else
        {
          GList *pos;

          pos = g_list_find (priv->foci, focus);
          if (pos)
            {
              priv->foci = g_list_remove (priv->foci, focus);
              gtk_pointer_focus_unref (focus);
            }
        }

      gtk_pointer_focus_unref (focus);
    }
  else if (target)
    {
      focus = gtk_pointer_focus_new (window, target, device, sequence, x, y);
      priv->foci = g_list_prepend (priv->foci, focus);
    }
}

static void
clear_widget_active_state (GtkWidget *widget,
                           GtkWidget *topmost)
{
  GtkWidget *w = widget;

  while (w)
    {
      gtk_widget_set_active_state (w, FALSE);
      if (w == topmost)
        break;
      w = _gtk_widget_get_parent (w);
    }
}

void
gtk_window_update_pointer_focus_on_state_change (GtkWindow *window,
                                                 GtkWidget *widget)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l = priv->foci;

  while (l)
    {
      GList *next;

      GtkPointerFocus *focus = l->data;

      next = l->next;

      gtk_pointer_focus_ref (focus);

      if (focus->grab_widget &&
          (focus->grab_widget == widget ||
           gtk_widget_is_ancestor (focus->grab_widget, widget)))
        {
          clear_widget_active_state (focus->grab_widget, widget);
          gtk_pointer_focus_set_implicit_grab (focus,
                                               gtk_widget_get_parent (widget));
        }

      if (GTK_WIDGET (focus->toplevel) == widget)
        {
          /* Unmapping the toplevel, remove pointer focus */
          priv->foci = g_list_remove_link (priv->foci, l);
          gtk_pointer_focus_unref (focus);
          g_list_free (l);
        }
      else if (focus->target == widget ||
               gtk_widget_is_ancestor (focus->target, widget))
        {
          GtkWidget *old_target;

          old_target = g_object_ref (focus->target);
          gtk_pointer_focus_repick_target (focus);
          gtk_synthesize_crossing_events (GTK_ROOT (window),
                                          GTK_CROSSING_POINTER,
                                          old_target, focus->target,
                                          focus->x, focus->y,
                                          GDK_CROSSING_NORMAL,
                                          NULL);
          g_object_unref (old_target);
        }

      gtk_pointer_focus_unref (focus);

      l = next;
    }
}

void
gtk_window_maybe_revoke_implicit_grab (GtkWindow *window,
                                       GdkDevice *device,
                                       GtkWidget *grab_widget)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l = priv->foci;

  while (l)
    {
      GtkPointerFocus *focus = l->data;

      l = l->next;

      if (focus->toplevel != window)
        continue;

      if ((!device || focus->device == device) &&
          focus->target != grab_widget &&
          !gtk_widget_is_ancestor (focus->target, grab_widget))
        gtk_window_set_pointer_focus_grab (window,
                                           focus->device,
                                           focus->sequence,
                                           NULL);
    }
}

void
gtk_window_set_pointer_focus_grab (GtkWindow        *window,
                                   GdkDevice        *device,
                                   GdkEventSequence *sequence,
                                   GtkWidget        *grab_widget)
{
  GtkPointerFocus *focus;

  focus = gtk_window_lookup_pointer_focus (window, device, sequence);
  if (!focus && !grab_widget)
    return;
  g_assert (focus != NULL);
  gtk_pointer_focus_set_implicit_grab (focus, grab_widget);
}

static void
update_cursor (GtkWindow *toplevel,
               GdkDevice *device,
               GtkWidget *grab_widget,
               GtkWidget *target)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (toplevel);
  GdkCursor *cursor = NULL;
  GtkNative *native;
  GdkSurface *surface;

  native = gtk_widget_get_native (target);
  surface = gtk_native_get_surface (native);

  if (grab_widget && !gtk_widget_is_ancestor (target, grab_widget) && target != grab_widget)
    {
      /* Outside the grab widget, cursor stays to whatever the grab
       * widget says.
       */
      if (gtk_widget_get_native (grab_widget) == native)
        cursor = gtk_widget_get_cursor (grab_widget);
      else
        cursor = NULL;
    }
  else
    {
      /* Inside the grab widget or in absence of grabs, allow walking
       * up the hierarchy to find out the cursor.
       */
      while (target)
        {
          /* Don't inherit cursors across surfaces */
          if (native != gtk_widget_get_native (target))
            break;

          if (target == GTK_WIDGET (toplevel) && priv->resize_cursor != NULL)
            cursor = priv->resize_cursor;
          else
            cursor = gtk_widget_get_cursor (target);

          if (cursor)
            break;

          if (grab_widget && target == grab_widget)
            break;

          target = _gtk_widget_get_parent (target);
        }
    }

  gdk_surface_set_device_cursor (surface, device, cursor);
}

void
gtk_window_maybe_update_cursor (GtkWindow *window,
                                GtkWidget *widget,
                                GdkDevice *device)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;
      GtkWidget *grab_widget, *target;
      GtkWindowGroup *group;

      if (focus->sequence)
        continue;
      if (device && device != focus->device)
        continue;

      group = gtk_window_get_group (window);

      grab_widget = gtk_window_group_get_current_grab (group);
      if (!grab_widget)
        grab_widget = gtk_pointer_focus_get_implicit_grab (focus);

      target = gtk_pointer_focus_get_target (focus);

      if (widget)
        {
          /* Check whether the changed widget affects the current cursor
           * lookups.
           */
          if (grab_widget && grab_widget != widget &&
              !gtk_widget_is_ancestor (widget, grab_widget))
            continue;
          if (target != widget &&
              !gtk_widget_is_ancestor (target, widget))
            continue;
        }

      update_cursor (focus->toplevel, focus->device, grab_widget, target);

      if (device)
        break;
    }
}

/**
 * gtk_window_set_child: (attributes org.gtk.Method.set_property=child)
 * @window: a `GtkWindow`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @window.
 */
void
gtk_window_set_child (GtkWindow *window,
                      GtkWidget *child)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (child == NULL || priv->child == child || gtk_widget_get_parent (child) == NULL);

  if (priv->child == child)
    return;

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  if (child)
    {
      priv->child = child;
      gtk_widget_insert_before (child, GTK_WIDGET (window), priv->title_box);
    }

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_CHILD]);
}

/**
 * gtk_window_get_child: (attributes org.gtk.Method.get_property=child)
 * @window: a `GtkWindow`
 *
 * Gets the child widget of @window.
 *
 * Returns: (nullable) (transfer none): the child widget of @window
 */
GtkWidget *
gtk_window_get_child (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  return priv->child;
}

/**
 * gtk_window_destroy:
 * @window: The window to destroy
 *
 * Drop the internal reference GTK holds on toplevel windows.
 */
void
gtk_window_destroy (GtkWindow *window)
{
  guint i;

  g_return_if_fail (GTK_IS_WINDOW (window));

  /* If gtk_window_destroy() has been called before. Can happen
   * when destroying a dialog manually in a ::close handler for example. */
  if (!g_list_store_find (toplevel_list, window, &i))
    return;

  g_object_ref (window);

  gtk_tooltip_unset_surface (GTK_NATIVE (window));

  gtk_window_hide (GTK_WIDGET (window));
  gtk_accessible_update_state (GTK_ACCESSIBLE (window),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                               -1);

  g_list_store_remove (toplevel_list, i);

  gtk_window_release_application (window);

  gtk_widget_unrealize (GTK_WIDGET (window));

  g_object_unref (window);
}

GdkDevice**
gtk_window_get_foci_on_widget (GtkWindow *window,
                               GtkWidget *widget,
                               guint     *n_devices)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GPtrArray *array = g_ptr_array_new ();
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;
      GtkWidget *target;

      target = gtk_pointer_focus_get_effective_target (focus);

      if (target == widget || gtk_widget_is_ancestor (target, widget))
        g_ptr_array_add (array, focus->device);
    }

  if (n_devices)
    *n_devices = array->len;

  return (GdkDevice**) g_ptr_array_free (array, FALSE);
}

static void
gtk_synthesize_grab_crossing (GtkWidget *child,
                              GdkDevice *device,
                              GtkWidget *new_grab_widget,
                              GtkWidget *old_grab_widget,
                              gboolean   from_grab,
                              gboolean   was_shadowed,
                              gboolean   is_shadowed)
{
  g_object_ref (child);

  if (is_shadowed)
    {
      if (!was_shadowed &&
          gtk_widget_is_sensitive (child))
        _gtk_widget_synthesize_crossing (child,
                                         new_grab_widget,
                                         device,
                                         GDK_CROSSING_GTK_GRAB);
    }
  else
    {
      if (was_shadowed &&
          gtk_widget_is_sensitive (child))
        _gtk_widget_synthesize_crossing (old_grab_widget, child,
                                         device,
                                         from_grab ? GDK_CROSSING_GTK_GRAB :
                                         GDK_CROSSING_GTK_UNGRAB);
    }

  g_object_unref (child);
}

static void
gtk_window_propagate_grab_notify (GtkWindow *window,
                                  GtkWidget *target,
                                  GdkDevice *device,
                                  GtkWidget *old_grab_widget,
                                  GtkWidget *new_grab_widget,
                                  gboolean   from_grab)
{
  GList *l, *widgets = NULL;
  gboolean was_grabbed = FALSE, is_grabbed = FALSE;

  while (target)
    {
      if (target == old_grab_widget)
        was_grabbed = TRUE;
      if (target == new_grab_widget)
        is_grabbed = TRUE;
      widgets = g_list_prepend (widgets, g_object_ref (target));
      target = gtk_widget_get_parent (target);
    }

  widgets = g_list_reverse (widgets);

  for (l = widgets; l; l = l->next)
    {
      gboolean was_shadowed, is_shadowed;

      was_shadowed = old_grab_widget && !was_grabbed;
      is_shadowed = new_grab_widget && !is_grabbed;

      if (l->data == old_grab_widget)
        was_grabbed = FALSE;
      if (l->data == new_grab_widget)
        is_grabbed = FALSE;

      if (was_shadowed == is_shadowed)
        break;

      gtk_synthesize_grab_crossing (l->data,
                                    device,
                                    old_grab_widget,
                                    new_grab_widget,
                                    from_grab,
                                    was_shadowed,
                                    is_shadowed);

      gtk_widget_reset_controllers (l->data);
    }

  g_list_free_full (widgets, g_object_unref);
}

void
gtk_window_grab_notify (GtkWindow *window,
                        GtkWidget *old_grab_widget,
                        GtkWidget *new_grab_widget,
                        gboolean   from_grab)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;

      gtk_window_propagate_grab_notify (window,
                                        gtk_pointer_focus_get_effective_target (focus),
                                        focus->device,
                                        old_grab_widget,
                                        new_grab_widget,
                                        from_grab);
    }
}

/**
 * gtk_window_set_handle_menubar_accel: (attributes org.gtk.Method.set_property=handle-menubar-accel)
 * @window: a `GtkWindow`
 * @handle_menubar_accel: %TRUE to make @window handle F10
 *
 * Sets whether this window should react to F10 key presses
 * by activating a menubar it contains.
 *
 * Since: 4.2
 */
void
gtk_window_set_handle_menubar_accel (GtkWindow *window,
                                     gboolean   handle_menubar_accel)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkPropagationPhase phase;

  g_return_if_fail (GTK_IS_WINDOW (window));

  phase = handle_menubar_accel ? GTK_PHASE_CAPTURE : GTK_PHASE_NONE;

  if (gtk_event_controller_get_propagation_phase (priv->menubar_controller) == phase)
    return;

  gtk_event_controller_set_propagation_phase (priv->menubar_controller, phase);

  g_object_notify_by_pspec (G_OBJECT (window), window_props[PROP_HANDLE_MENUBAR_ACCEL]);
}

/**
 * gtk_window_get_handle_menubar_accel: (attributes org.gtk.Method.get_property=handle-menubar-accel)
 * @window: a `GtkWindow`
 *
 * Returns whether this window reacts to F10 key presses by
 * activating a menubar it contains.
 *
 * Returns: %TRUE if the window handles F10
 *
 * Since: 4.2
 */
gboolean
gtk_window_get_handle_menubar_accel (GtkWindow *window)
{
  GtkWindowPrivate *priv = gtk_window_get_instance_private (window);
  GtkPropagationPhase phase;

  g_return_val_if_fail (GTK_IS_WINDOW (window), TRUE);

  phase = gtk_event_controller_get_propagation_phase (priv->menubar_controller);

  return phase == GTK_PHASE_CAPTURE;
}
