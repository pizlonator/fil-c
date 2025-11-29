/*
 * Copyright (c) 2013 Red Hat, Inc.
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
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkstack.h"
#include "gtkenums.h"
#include "gtkaccessibleprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"
#include "gtksingleselection.h"
#include "gtklistlistmodelprivate.h"
#include <math.h>
#include <string.h>

/**
 * GtkStack:
 *
 * `GtkStack` is a container which only shows one of its children
 * at a time.
 *
 * In contrast to `GtkNotebook`, `GtkStack` does not provide a means
 * for users to change the visible child. Instead, a separate widget
 * such as [class@Gtk.StackSwitcher] or [class@Gtk.StackSidebar] can
 * be used with `GtkStack` to provide this functionality.
 *
 * Transitions between pages can be animated as slides or fades. This
 * can be controlled with [method@Gtk.Stack.set_transition_type].
 * These animations respect the [property@Gtk.Settings:gtk-enable-animations]
 * setting.
 *
 * `GtkStack` maintains a [class@Gtk.StackPage] object for each added
 * child, which holds additional per-child properties. You
 * obtain the `GtkStackPage` for a child with [method@Gtk.Stack.get_page]
 * and you can obtain a `GtkSelectionModel` containing all the pages
 * with [method@Gtk.Stack.get_pages].
 *
 * # GtkStack as GtkBuildable
 *
 * To set child-specific properties in a .ui file, create `GtkStackPage`
 * objects explicitly, and set the child widget as a property on it:
 *
 * ```xml
 *   <object class="GtkStack" id="stack">
 *     <child>
 *       <object class="GtkStackPage">
 *         <property name="name">page1</property>
 *         <property name="title">In the beginning…</property>
 *         <property name="child">
 *           <object class="GtkLabel">
 *             <property name="label">It was dark</property>
 *           </object>
 *         </property>
 *       </object>
 *     </child>
 * ```
 *
 * # CSS nodes
 *
 * `GtkStack` has a single CSS node named stack.
 *
 * # Accessibility
 *
 * `GtkStack` uses the %GTK_ACCESSIBLE_ROLE_TAB_PANEL for the stack
 * pages, which are the accessible parent objects of the child widgets.
 */

/**
 * GtkStackTransitionType:
 * @GTK_STACK_TRANSITION_TYPE_NONE: No transition
 * @GTK_STACK_TRANSITION_TYPE_CROSSFADE: A cross-fade
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT: Slide from left to right
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT: Slide from right to left
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP: Slide from bottom up
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN: Slide from top down
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT: Slide from left or right according to the children order
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN: Slide from top down or bottom up according to the order
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP: Cover the old page by sliding up
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN: Cover the old page by sliding down
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT: Cover the old page by sliding to the left
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT: Cover the old page by sliding to the right
 * @GTK_STACK_TRANSITION_TYPE_UNDER_UP: Uncover the new page by sliding up
 * @GTK_STACK_TRANSITION_TYPE_UNDER_DOWN: Uncover the new page by sliding down
 * @GTK_STACK_TRANSITION_TYPE_UNDER_LEFT: Uncover the new page by sliding to the left
 * @GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT: Uncover the new page by sliding to the right
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN: Cover the old page sliding up or uncover the new page sliding down, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP: Cover the old page sliding down or uncover the new page sliding up, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT: Cover the old page sliding left or uncover the new page sliding right, according to order
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT: Cover the old page sliding right or uncover the new page sliding left, according to order
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT: Pretend the pages are sides of a cube and rotate that cube to the left
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT: Pretend the pages are sides of a cube and rotate that cube to the right
 * @GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT: Pretend the pages are sides of a cube and rotate that cube to the left or right according to the children order
 *
 * Possible transitions between pages in a `GtkStack` widget.
 *
 * New values may be added to this enumeration over time.
 */

/**
 * GtkStackPage:
 *
 * `GtkStackPage` is an auxiliary class used by `GtkStack`.
 */

/* TODO:
 *  filter events out events to the last_child widget during transitions
 */

struct _GtkStack {
  GtkWidget parent_instance;
};

typedef struct _GtkStackClass GtkStackClass;
struct _GtkStackClass {
  GtkWidgetClass parent_class;
};

typedef struct {
  GPtrArray *children;

  GtkStackPage *visible_child;

  gboolean homogeneous[2];

  GtkStackTransitionType transition_type;
  guint transition_duration;

  GtkStackPage *last_visible_child;
  guint tick_id;
  GtkProgressTracker tracker;
  gboolean first_frame_skipped;

  int last_visible_widget_width;
  int last_visible_widget_height;

  gboolean interpolate_size;

  GtkStackTransitionType active_transition_type;

  GtkSelectionModel *pages;

} GtkStackPrivate;

static void gtk_stack_buildable_interface_init (GtkBuildableIface *iface);
static void gtk_stack_accessible_init (GtkAccessibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkStack, gtk_stack, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkStack)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                gtk_stack_accessible_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_stack_buildable_interface_init))
enum  {
  PROP_0,
  PROP_HHOMOGENEOUS,
  PROP_VHOMOGENEOUS,
  PROP_VISIBLE_CHILD,
  PROP_VISIBLE_CHILD_NAME,
  PROP_TRANSITION_DURATION,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_RUNNING,
  PROP_INTERPOLATE_SIZE,
  PROP_PAGES,
  LAST_PROP
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CHILD,
  CHILD_PROP_NAME,
  CHILD_PROP_TITLE,
  CHILD_PROP_ICON_NAME,
  CHILD_PROP_NEEDS_ATTENTION,
  CHILD_PROP_VISIBLE,
  CHILD_PROP_USE_UNDERLINE,
  LAST_CHILD_PROP,

  PROP_ACCESSIBLE_ROLE
};

struct _GtkStackPage
{
  GObject parent_instance;

  GtkWidget *widget;
  char *name;
  char *title;
  char *icon_name;
  GtkWidget *last_focus;

  GtkStackPage *next_page;

  GtkATContext *at_context;

  guint needs_attention : 1;
  guint visible         : 1;
  guint use_underline   : 1;
  guint in_destruction  : 1;
};

typedef struct _GtkStackPageClass GtkStackPageClass;
struct _GtkStackPageClass
{
  GObjectClass parent_class;
};

static GParamSpec *stack_props[LAST_PROP] = { NULL, };
static GParamSpec *stack_page_props[LAST_CHILD_PROP] = { NULL, };

static GtkATContext *
gtk_stack_page_accessible_get_at_context (GtkAccessible *accessible)
{
  GtkStackPage *page = GTK_STACK_PAGE (accessible);

  if (page->in_destruction)
    {
      GTK_DEBUG (A11Y, "ATContext for “%s” [%p] accessed during destruction",
                       G_OBJECT_TYPE_NAME (accessible),
                       accessible);
      return NULL;
    }

  if (page->at_context == NULL)
    {
      GtkAccessibleRole role = GTK_ACCESSIBLE_ROLE_TAB_PANEL;
      GdkDisplay *display;

      if (page->widget != NULL)
        display = gtk_widget_get_display (page->widget);
      else
        display = gdk_display_get_default ();

      page->at_context = gtk_at_context_create (role, accessible, display);
      if (page->at_context == NULL)
        return NULL;
    }

  return g_object_ref (page->at_context);
}

static gboolean
gtk_stack_page_accessible_get_platform_state (GtkAccessible              *self,
                                              GtkAccessiblePlatformState  state)
{
  return FALSE;
}

static GtkAccessible *
gtk_stack_page_accessible_get_accessible_parent (GtkAccessible *accessible)
{
  GtkStackPage *page = GTK_STACK_PAGE (accessible);
  GtkWidget *parent;

  if (page->widget == NULL)
    return NULL;

  parent = _gtk_widget_get_parent (page->widget);

  return GTK_ACCESSIBLE (g_object_ref (parent));
}

static GtkAccessible *
gtk_stack_page_accessible_get_first_accessible_child (GtkAccessible *accessible)
{
  GtkStackPage *page = GTK_STACK_PAGE (accessible);

  if (page->widget == NULL)
    return NULL;

  return GTK_ACCESSIBLE (g_object_ref (page->widget));
}

static GtkAccessible *
gtk_stack_page_accessible_get_next_accessible_sibling (GtkAccessible *accessible)
{
  GtkStackPage *page = GTK_STACK_PAGE (accessible);

  if (page->next_page == NULL)
    return NULL;

  return GTK_ACCESSIBLE (g_object_ref (page->next_page));
}

static gboolean
gtk_stack_page_accessible_get_bounds (GtkAccessible *accessible,
                                      int           *x,
                                      int           *y,
                                      int           *width,
                                      int           *height)
{
  GtkStackPage *page = GTK_STACK_PAGE (accessible);
  if (page->widget != NULL)
    return gtk_accessible_get_bounds (GTK_ACCESSIBLE (page->widget), x, y, width, height);
  else
    return FALSE;
}

static void
gtk_stack_page_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_at_context = gtk_stack_page_accessible_get_at_context;
  iface->get_platform_state = gtk_stack_page_accessible_get_platform_state;
  iface->get_accessible_parent = gtk_stack_page_accessible_get_accessible_parent;
  iface->get_first_accessible_child = gtk_stack_page_accessible_get_first_accessible_child;
  iface->get_next_accessible_sibling = gtk_stack_page_accessible_get_next_accessible_sibling;
  iface->get_bounds = gtk_stack_page_accessible_get_bounds;
}

G_DEFINE_TYPE_WITH_CODE (GtkStackPage, gtk_stack_page, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                gtk_stack_page_accessible_init))

static void
gtk_stack_page_init (GtkStackPage *page)
{
  page->visible = TRUE;
}

static void
gtk_stack_page_finalize (GObject *object)
{
  GtkStackPage *page = GTK_STACK_PAGE (object);

  g_clear_object (&page->widget);
  g_free (page->name);
  g_free (page->title);
  g_free (page->icon_name);

  if (page->last_focus)
    g_object_remove_weak_pointer (G_OBJECT (page->last_focus),
                                  (gpointer *)&page->last_focus);

  G_OBJECT_CLASS (gtk_stack_page_parent_class)->finalize (object);
}

static void
gtk_stack_page_dispose (GObject *object)
{
  GtkStackPage *page = GTK_STACK_PAGE (object);

  page->in_destruction = TRUE;

  g_clear_object (&page->at_context);

  G_OBJECT_CLASS (gtk_stack_page_parent_class)->dispose (object);
}

static void
gtk_stack_page_get_property (GObject      *object,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkStackPage *info = GTK_STACK_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_value_set_object (value, info->widget);
      break;

    case CHILD_PROP_NAME:
      g_value_set_string (value, gtk_stack_page_get_name (info));
      break;

    case CHILD_PROP_TITLE:
      g_value_set_string (value, gtk_stack_page_get_title (info));
      break;

    case CHILD_PROP_ICON_NAME:
      g_value_set_string (value, gtk_stack_page_get_icon_name (info));
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      g_value_set_boolean (value, gtk_stack_page_get_needs_attention (info));
      break;

    case CHILD_PROP_VISIBLE:
      g_value_set_boolean (value, gtk_stack_page_get_visible (info));
      break;

    case CHILD_PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_stack_page_get_use_underline (info));
      break;

    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, GTK_ACCESSIBLE_ROLE_TAB_PANEL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_page_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkStackPage *info = GTK_STACK_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_set_object (&info->widget, g_value_get_object (value));
      gtk_accessible_set_accessible_parent (GTK_ACCESSIBLE (info->widget),
                                            GTK_ACCESSIBLE (info),
                                            NULL);
      break;

    case CHILD_PROP_NAME:
      gtk_stack_page_set_name (info, g_value_get_string (value));
      break;

    case CHILD_PROP_TITLE:
      gtk_stack_page_set_title (info, g_value_get_string (value));
      break;

    case CHILD_PROP_ICON_NAME:
      gtk_stack_page_set_icon_name (info, g_value_get_string (value));
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      gtk_stack_page_set_needs_attention (info, g_value_get_boolean (value));
      break;

    case CHILD_PROP_VISIBLE:
      gtk_stack_page_set_visible (info, g_value_get_boolean (value));
      break;

    case CHILD_PROP_USE_UNDERLINE:
      gtk_stack_page_set_use_underline (info, g_value_get_boolean (value));
      break;

    case PROP_ACCESSIBLE_ROLE:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_page_constructed (GObject *gobject)
{
  GtkStackPage *self = GTK_STACK_PAGE (gobject);

  if (G_UNLIKELY (self->widget == NULL))
    g_error ("GtkStackPage '%s' [%p] is missing a child widget",
             self->name != NULL ? self->name : "<unnamed>",
             self);

  G_OBJECT_CLASS (gtk_stack_page_parent_class)->constructed (gobject);
}

static void
gtk_stack_page_class_init (GtkStackPageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_stack_page_finalize;
  object_class->dispose = gtk_stack_page_dispose;
  object_class->get_property = gtk_stack_page_get_property;
  object_class->set_property = gtk_stack_page_set_property;
  object_class->constructed = gtk_stack_page_constructed;

  /**
   * GtkStackPage:child: (attributes org.gtk.Property.get=gtk_stack_page_get_child)
   *
   * The child that this page is for.
   */
  stack_page_props[CHILD_PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkStackPage:name: (attributes org.gtk.Property.get=gtk_stack_page_get_name org.gtk.Property.set=gtk_stack_page_set_name)
   *
   * The name of the child page.
   */
  stack_page_props[CHILD_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkStackPage:title: (attributes org.gtk.Property.get=gtk_stack_page_get_title org.gtk.Property.set=gtk_stack_page_set_title)
   *
   * The title of the child page.
   */
  stack_page_props[CHILD_PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkStackPage:icon-name: (attributes org.gtk.Property.get=gtk_stack_page_get_icon_name org.gtk.Property.set=gtk_stack_page_set_icon_name)
   *
   * The icon name of the child page.
   */
  stack_page_props[CHILD_PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkStackPage:needs-attention: (attributes org.gtk.Property.get=gtk_stack_page_get_needs_attention org.gtk.Property.set=gtk_stack_page_set_needs_attention)
   *
   * Whether the page requires the user attention.
   *
   * This is used by the [class@Gtk.StackSwitcher] to change the
   * appearance of the corresponding button when a page needs
   * attention and it is not the current one.
   */
  stack_page_props[CHILD_PROP_NEEDS_ATTENTION] =
    g_param_spec_boolean ("needs-attention", NULL, NULL,
                         FALSE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStackPage:visible: (attributes org.gtk.Property.get=gtk_stack_page_get_visible org.gtk.Property.set=gtk_stack_page_set_visible)
   *
   * Whether this page is visible.
   */
  stack_page_props[CHILD_PROP_VISIBLE] =
    g_param_spec_boolean ("visible", NULL, NULL,
                         TRUE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStackPage:use-underline: (attributes org.gtk.Property.get=gtk_stack_page_get_use_underline org.gtk.Property.set=gtk_stack_page_set_use_underline)
   *
   * If set, an underline in the title indicates a mnemonic.
   */
  stack_page_props[CHILD_PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline", NULL, NULL,
                         FALSE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_CHILD_PROP, stack_page_props);

  g_object_class_override_property (object_class, PROP_ACCESSIBLE_ROLE, "accessible-role");
}

#define GTK_TYPE_STACK_PAGES (gtk_stack_pages_get_type ())
G_DECLARE_FINAL_TYPE (GtkStackPages, gtk_stack_pages, GTK, STACK_PAGES, GObject)

struct _GtkStackPages
{
  GObject parent_instance;
  GtkStack *stack;
};

struct _GtkStackPagesClass
{
  GObjectClass parent_class;
};

enum {
  PAGES_PROP_0,
  PAGES_PROP_ITEM_TYPE,
  PAGES_PROP_N_ITEMS,

  PAGES_N_PROPS
};

static GParamSpec *pages_properties[PAGES_N_PROPS] = { NULL, };

static GType
gtk_stack_pages_get_item_type (GListModel *model)
{
  return GTK_TYPE_STACK_PAGE;
}

static guint
gtk_stack_pages_get_n_items (GListModel *model)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);

  return priv->children->len;
}

static gpointer
gtk_stack_pages_get_item (GListModel *model,
                          guint       position)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  if (position >= priv->children->len)
    return NULL;

  page = g_ptr_array_index (priv->children, position);

  return g_object_ref (page);
}

static void
gtk_stack_pages_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_stack_pages_get_item_type;
  iface->get_n_items = gtk_stack_pages_get_n_items;
  iface->get_item = gtk_stack_pages_get_item;
}

static gboolean
gtk_stack_pages_is_selected (GtkSelectionModel *model,
                             guint              position)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  if (position >= priv->children->len)
    return FALSE;

  page = g_ptr_array_index (priv->children, position);

  return page == priv->visible_child;
}

static void set_visible_child (GtkStack               *stack,
                               GtkStackPage           *child_info,
                               GtkStackTransitionType  transition_type,
                               guint                   transition_duration);

static gboolean
gtk_stack_pages_select_item (GtkSelectionModel *model,
                             guint              position,
                             gboolean           exclusive)
{
  GtkStackPages *pages = GTK_STACK_PAGES (model);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (pages->stack);
  GtkStackPage *page;

  if (position >= priv->children->len)
    return FALSE;

  page = g_ptr_array_index (priv->children, position);

  set_visible_child (pages->stack, page, priv->transition_type, priv->transition_duration);

  return TRUE;
}

static void
gtk_stack_pages_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_stack_pages_is_selected;
  iface->select_item = gtk_stack_pages_select_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkStackPages, gtk_stack_pages, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_stack_pages_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL, gtk_stack_pages_selection_model_init))

static void
gtk_stack_pages_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkStackPages *self = GTK_STACK_PAGES (object);

  switch (prop_id)
    {
    case PAGES_PROP_ITEM_TYPE:
      g_value_set_gtype (value, GTK_TYPE_STACK_PAGE);
      break;

    case PAGES_PROP_N_ITEMS:
      g_value_set_uint (value, gtk_stack_pages_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_pages_init (GtkStackPages *pages)
{
}

static void
gtk_stack_pages_class_init (GtkStackPagesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_stack_pages_get_property;

  pages_properties[PAGES_PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        GTK_TYPE_STACK_PAGE,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  pages_properties[PAGES_PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PAGES_N_PROPS, pages_properties);
}

static GtkStackPages *
gtk_stack_pages_new (GtkStack *stack)
{
  GtkStackPages *pages;

  pages = g_object_new (GTK_TYPE_STACK_PAGES, NULL);
  pages->stack = stack;

  return pages;
}

static GtkStackPage *gtk_stack_add_internal (GtkStack *stack,
                                             GtkWidget  *child,
                                             const char *name,
                                             const char *title);

static GtkSizeRequestMode gtk_stack_get_request_mode (GtkWidget *widget);
static void     gtk_stack_compute_expand                 (GtkWidget     *widget,
                                                          gboolean      *hexpand,
                                                          gboolean      *vexpand);
static void     gtk_stack_size_allocate                  (GtkWidget     *widget,
                                                          int            width,
                                                          int            height,
                                                          int            baseline);
static void     gtk_stack_snapshot                       (GtkWidget     *widget,
                                                          GtkSnapshot   *snapshot);
static void     gtk_stack_measure                        (GtkWidget      *widget,
                                                          GtkOrientation  orientation,
                                                          int             for_size,
                                                          int            *minimum,
                                                          int            *natural,
                                                          int            *minimum_baseline,
                                                          int            *natural_baseline);
static void     gtk_stack_dispose                        (GObject       *obj);
static void     gtk_stack_finalize                       (GObject       *obj);
static void     gtk_stack_get_property                   (GObject       *object,
                                                          guint          property_id,
                                                          GValue        *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_set_property                   (GObject       *object,
                                                          guint          property_id,
                                                          const GValue  *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_unschedule_ticks               (GtkStack      *stack);


static void     gtk_stack_add_page                       (GtkStack     *stack,
                                                          GtkStackPage *page);

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_stack_buildable_add_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               GObject      *child,
                               const char   *type)
{
  if (GTK_IS_STACK_PAGE (child))
    gtk_stack_add_page (GTK_STACK (buildable), GTK_STACK_PAGE (child));
  else if (GTK_IS_WIDGET (child))
    gtk_stack_add_internal (GTK_STACK (buildable), GTK_WIDGET (child), NULL, NULL);
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_stack_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_stack_buildable_add_child;
}

static GtkAccessible *
gtk_stack_accessible_get_first_accessible_child (GtkAccessible *accessible)
{
  GtkStack *stack = GTK_STACK (accessible);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAccessible *page_accessible = NULL;

  if (priv->children->len > 0)
    page_accessible = GTK_ACCESSIBLE (g_object_ref (g_ptr_array_index (priv->children, 0)));

  return page_accessible;
}

static void
gtk_stack_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_first_accessible_child = gtk_stack_accessible_get_first_accessible_child;
}

static void stack_remove (GtkStack  *stack,
                          GtkWidget *child,
                          gboolean   in_dispose);

static void
gtk_stack_dispose (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *child;
  guint n_pages = priv->children->len;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (stack))))
    stack_remove (stack, child, TRUE);

  if (priv->pages && n_pages > 0)
    {
      g_list_model_items_changed (G_LIST_MODEL (priv->pages), 0, n_pages, 0);
      g_object_notify_by_pspec (G_OBJECT (priv->pages), pages_properties[PAGES_PROP_N_ITEMS]);
    }

  G_OBJECT_CLASS (gtk_stack_parent_class)->dispose (obj);
}

static void
gtk_stack_finalize (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->pages)
    g_object_remove_weak_pointer (G_OBJECT (priv->pages), (gpointer *)&priv->pages);

  gtk_stack_unschedule_ticks (stack);

  g_ptr_array_free (priv->children, TRUE);

  G_OBJECT_CLASS (gtk_stack_parent_class)->finalize (obj);
}

static void
gtk_stack_get_property (GObject   *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_hhomogeneous (stack));
      break;
    case PROP_VHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_vhomogeneous (stack));
      break;
    case PROP_VISIBLE_CHILD:
      g_value_set_object (value, gtk_stack_get_visible_child (stack));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      g_value_set_string (value, gtk_stack_get_visible_child_name (stack));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_stack_get_transition_duration (stack));
      break;
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_stack_get_transition_type (stack));
      break;
    case PROP_TRANSITION_RUNNING:
      g_value_set_boolean (value, gtk_stack_get_transition_running (stack));
      break;
    case PROP_INTERPOLATE_SIZE:
      g_value_set_boolean (value, gtk_stack_get_interpolate_size (stack));
      break;
    case PROP_PAGES:
      g_value_take_object (value, gtk_stack_get_pages (stack));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_set_property (GObject     *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HHOMOGENEOUS:
      gtk_stack_set_hhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VHOMOGENEOUS:
      gtk_stack_set_vhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_CHILD:
      gtk_stack_set_visible_child (stack, g_value_get_object (value));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      gtk_stack_set_visible_child_name (stack, g_value_get_string (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_stack_set_transition_duration (stack, g_value_get_uint (value));
      break;
    case PROP_TRANSITION_TYPE:
      gtk_stack_set_transition_type (stack, g_value_get_enum (value));
      break;
    case PROP_INTERPOLATE_SIZE:
      gtk_stack_set_interpolate_size (stack, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_class_init (GtkStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtk_stack_get_property;
  object_class->set_property = gtk_stack_set_property;
  object_class->dispose = gtk_stack_dispose;
  object_class->finalize = gtk_stack_finalize;

  widget_class->size_allocate = gtk_stack_size_allocate;
  widget_class->snapshot = gtk_stack_snapshot;
  widget_class->measure = gtk_stack_measure;
  widget_class->compute_expand = gtk_stack_compute_expand;
  widget_class->get_request_mode = gtk_stack_get_request_mode;

  /**
   * GtkStack:hhomogeneous: (attributes org.gtk.Property.get=gtk_stack_get_hhomogeneous org.gtk.Property.set=gtk_stack_set_hhomogeneous)
   *
   * %TRUE if the stack allocates the same width for all children.
   */
  stack_props[PROP_HHOMOGENEOUS] =
      g_param_spec_boolean ("hhomogeneous", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:vhomogeneous: (attributes org.gtk.Property.get=gtk_stack_get_vhomogeneous org.gtk.Property.set=gtk_stack_set_vhomogeneous)
   *
   * %TRUE if the stack allocates the same height for all children.
   */
  stack_props[PROP_VHOMOGENEOUS] =
      g_param_spec_boolean ("vhomogeneous", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:visible-child: (attributes org.gtk.Property.get=gtk_stack_get_visible_child org.gtk.Property.set=gtk_stack_set_visible_child)
   *
   * The widget currently visible in the stack.
   */
  stack_props[PROP_VISIBLE_CHILD] =
      g_param_spec_object ("visible-child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:visible-child-name: (attributes org.gtk.Property.get=gtk_stack_get_visible_child_name org.gtk.Property.set=gtk_stack_set_visible_child_name)
   *
   * The name of the widget currently visible in the stack.
   */
  stack_props[PROP_VISIBLE_CHILD_NAME] =
      g_param_spec_string ("visible-child-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:transition-duration: (attributes org.gtk.Property.get=gtk_stack_get_transition_duration org.gtk.Property.set=gtk_stack_set_transition_duration)
   *
   * The animation duration, in milliseconds.
   */
  stack_props[PROP_TRANSITION_DURATION] =
      g_param_spec_uint ("transition-duration", NULL, NULL,
                         0, G_MAXUINT, 200,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:transition-type: (attributes org.gtk.Property.get=gtk_stack_get_transition_type org.gtk.Property.set=gtk_stack_set_transition_type)
   *
   * The type of animation used to transition.
   */
  stack_props[PROP_TRANSITION_TYPE] =
      g_param_spec_enum ("transition-type", NULL, NULL,
                         GTK_TYPE_STACK_TRANSITION_TYPE, GTK_STACK_TRANSITION_TYPE_NONE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:transition-running: (attributes org.gtk.Property.get=gtk_stack_get_transition_running)
   *
   * Whether or not the transition is currently running.
   */
  stack_props[PROP_TRANSITION_RUNNING] =
      g_param_spec_boolean ("transition-running", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READABLE);

  /**
   * GtkStack:interpolate-size: (attributes org.gtk.Property.get=gtk_stack_get_interpolate_size org.gtk.Property.set=gtk_stack_set_interpolate_size)
   *
   * Whether or not the size should smoothly change during the transition.
   */
  stack_props[PROP_INTERPOLATE_SIZE] =
      g_param_spec_boolean ("interpolate-size", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:pages: (attributes org.gtk.Property.get=gtk_stack_get_pages)
   *
   * A selection model with the stack pages.
   */
  stack_props[PROP_PAGES] =
      g_param_spec_object ("pages", NULL, NULL,
                           GTK_TYPE_SELECTION_MODEL,
                           GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, stack_props);

  gtk_widget_class_set_css_name (widget_class, I_("stack"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

/**
 * gtk_stack_new:
 *
 * Creates a new `GtkStack`.
 *
 * Returns: a new `GtkStack`
 */
GtkWidget *
gtk_stack_new (void)
{
  return g_object_new (GTK_TYPE_STACK, NULL);
}

static GtkStackPage *
find_child_info_for_widget (GtkStack  *stack,
                            GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  guint idx;

  for (idx = 0; idx < priv->children->len; idx++)
    {
      info = g_ptr_array_index (priv->children, idx);
      if (info->widget == child)
        return info;
    }

  return NULL;
}

static inline gboolean
is_left_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT);
}

static inline gboolean
is_right_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

static inline gboolean
is_up_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP);
}

static inline gboolean
is_down_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN);
}

/* Transitions that cause the bin window to move */
static inline gboolean
is_window_moving_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

/* Transitions that change direction depending on the relative order of the
old and new child */
static inline gboolean
is_direction_dependent_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT);
}

/* Returns simple transition type for a direction dependent transition, given
whether the new child (the one being switched to) is first in the stacking order
(added earlier). */
static inline GtkStackTransitionType
get_simple_transition_type (gboolean               new_child_first,
                            GtkStackTransitionType transition_type)
{
  switch (transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT : GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
    case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT : GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT;
    case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN : GTK_STACK_TRANSITION_TYPE_SLIDE_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_DOWN : GTK_STACK_TRANSITION_TYPE_OVER_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_UP : GTK_STACK_TRANSITION_TYPE_OVER_DOWN;
    case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT : GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
    case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_LEFT : GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
    case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
    case GTK_STACK_TRANSITION_TYPE_OVER_UP:
    case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
    case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
    case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
    case GTK_STACK_TRANSITION_TYPE_NONE:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
    case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
    case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
    case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
    case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
    case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
    case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
    default:
      return transition_type;
    }
}

static int
get_bin_window_x (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int width;
  int x = 0;

  width = gtk_widget_get_width (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
    {
      if (is_left_transition (priv->active_transition_type))
        x = width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      if (is_right_transition (priv->active_transition_type))
        x = -width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
    }

  return x;
}

static int
get_bin_window_y (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int height;
  int y = 0;

  height = gtk_widget_get_height (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
    {
      if (is_up_transition (priv->active_transition_type))
        y = height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      if (is_down_transition(priv->active_transition_type))
        y = -height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
    }

  return y;
}

static void
gtk_stack_progress_updated (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (!priv->homogeneous[GTK_ORIENTATION_VERTICAL] || !priv->homogeneous[GTK_ORIENTATION_HORIZONTAL])
    gtk_widget_queue_resize (GTK_WIDGET (stack));
  else if (is_window_moving_transition (priv->active_transition_type))
    gtk_widget_queue_allocate (GTK_WIDGET (stack));
  else
    gtk_widget_queue_draw (GTK_WIDGET (stack));

  if (gtk_progress_tracker_get_state (&priv->tracker) == GTK_PROGRESS_STATE_AFTER &&
      priv->last_visible_child != NULL)
    {
      gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
      priv->last_visible_child = NULL;
    }
}

static gboolean
gtk_stack_transition_cb (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->first_frame_skipped)
    gtk_progress_tracker_advance_frame (&priv->tracker,
                                        gdk_frame_clock_get_frame_time (frame_clock));
  else
    priv->first_frame_skipped = TRUE;

  /* Finish animation early if not mapped anymore */
  if (!gtk_widget_get_mapped (widget))
    gtk_progress_tracker_finish (&priv->tracker);

  gtk_stack_progress_updated (GTK_STACK (widget));

  if (gtk_progress_tracker_get_state (&priv->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);

      return FALSE;
    }

  return TRUE;
}

static void
gtk_stack_schedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id == 0)
    {
      priv->tick_id =
        gtk_widget_add_tick_callback (GTK_WIDGET (stack), gtk_stack_transition_cb, stack, NULL);
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static void
gtk_stack_unschedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (stack), priv->tick_id);
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static GtkStackTransitionType
effective_transition_type (GtkStack               *stack,
                           GtkStackTransitionType  transition_type)
{
  if (_gtk_widget_get_direction (GTK_WIDGET (stack)) == GTK_TEXT_DIR_RTL)
    {
      switch (transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
        case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
          return GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_LEFT;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
        case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
        case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
        case GTK_STACK_TRANSITION_TYPE_NONE:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
        case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
        case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
        default:
          return transition_type;
        }
    }
  else
    {
      return transition_type;
    }
}

static void
gtk_stack_start_transition (GtkStack               *stack,
                            GtkStackTransitionType  transition_type,
                            guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *widget = GTK_WIDGET (stack);

  if (gtk_widget_get_mapped (widget) &&
      gtk_settings_get_enable_animations (gtk_widget_get_settings (widget)) &&
      transition_type != GTK_STACK_TRANSITION_TYPE_NONE &&
      transition_duration != 0 &&
      priv->last_visible_child != NULL)
    {
      priv->active_transition_type = effective_transition_type (stack, transition_type);
      priv->first_frame_skipped = FALSE;
      gtk_stack_schedule_ticks (stack);
      gtk_progress_tracker_start (&priv->tracker,
                                  priv->transition_duration * 1000,
                                  0,
                                  1.0);
    }
  else
    {
      gtk_stack_unschedule_ticks (stack);
      priv->active_transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
      gtk_progress_tracker_finish (&priv->tracker);
    }

  gtk_stack_progress_updated (GTK_STACK (widget));
}

static void
set_visible_child (GtkStack               *stack,
                   GtkStackPage      *child_info,
                   GtkStackTransitionType  transition_type,
                   guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  GtkWidget *widget = GTK_WIDGET (stack);
  guint idx;
  GtkWidget *focus;
  gboolean contains_focus = FALSE;
  guint old_pos = GTK_INVALID_LIST_POSITION;
  guint new_pos = GTK_INVALID_LIST_POSITION;

  /* if we are being destroyed, do not bother with transitions
   * and notifications
   */
  if (gtk_widget_in_destruction (widget))
    return;

  /* If none, pick first visible */
  if (child_info == NULL)
    {
      for (idx = 0; idx < priv->children->len; idx++)
        {
          info = g_ptr_array_index (priv->children, idx);
          if (gtk_widget_get_visible (info->widget))
            {
              child_info = info;
              break;
            }
        }
    }

  if (child_info == priv->visible_child)
    return;

  if (priv->pages)
    {
      guint position;
      for (idx = 0, position = 0; idx < priv->children->len; idx++, position++)
        {
          info = g_ptr_array_index (priv->children, idx);
          if (info == priv->visible_child)
            old_pos = position;
          else if (info == child_info)
            new_pos = position;
        }
    }

  if (gtk_widget_get_root (widget))
    focus = gtk_root_get_focus (gtk_widget_get_root (widget));
  else
    focus = NULL;
  if (focus &&
      priv->visible_child &&
      priv->visible_child->widget &&
      gtk_widget_is_ancestor (focus, priv->visible_child->widget))
    {
      contains_focus = TRUE;

      if (priv->visible_child->last_focus)
        g_object_remove_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                      (gpointer *)&priv->visible_child->last_focus);
      priv->visible_child->last_focus = focus;
      g_object_add_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                 (gpointer *)&priv->visible_child->last_focus);
    }

  if (priv->last_visible_child)
    gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
  priv->last_visible_child = NULL;

  if (priv->visible_child && priv->visible_child->widget)
    {
      if (gtk_widget_is_visible (widget))
        {
          priv->last_visible_child = priv->visible_child;
          priv->last_visible_widget_width = gtk_widget_get_width (widget);
          priv->last_visible_widget_height = gtk_widget_get_height (widget);
        }
      else
        {
          gtk_widget_set_child_visible (priv->visible_child->widget, FALSE);
        }
    }

  priv->visible_child = child_info;

  if (child_info)
    {
      gtk_widget_set_child_visible (child_info->widget, TRUE);

      if (contains_focus)
        {
          if (child_info->last_focus)
            gtk_widget_grab_focus (child_info->last_focus);
          else
            gtk_widget_child_focus (child_info->widget, GTK_DIR_TAB_FORWARD);
        }
    }

  if ((child_info == NULL || priv->last_visible_child == NULL) &&
      is_direction_dependent_transition (transition_type))
    {
      transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
    }
  else if (is_direction_dependent_transition (transition_type))
    {
      gboolean i_first = FALSE;
      for (idx = 0; idx < priv->children->len; idx++)
        {
	  if (child_info == g_ptr_array_index (priv->children, idx))
	    {
	      i_first = TRUE;
	      break;
	    }
	  if (priv->last_visible_child == g_ptr_array_index (priv->children, idx))
	    break;
        }

      transition_type = get_simple_transition_type (i_first, transition_type);
    }

  if (priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] && priv->homogeneous[GTK_ORIENTATION_VERTICAL])
    gtk_widget_queue_allocate (widget);
  else
    gtk_widget_queue_resize (widget);

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VISIBLE_CHILD]);
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_VISIBLE_CHILD_NAME]);

  if (priv->pages)
    {
      if (old_pos == GTK_INVALID_LIST_POSITION && new_pos == GTK_INVALID_LIST_POSITION)
        ; /* nothing to do */
      else if (old_pos == GTK_INVALID_LIST_POSITION)
        gtk_selection_model_selection_changed (priv->pages, new_pos, 1);
      else if (new_pos == GTK_INVALID_LIST_POSITION)
        gtk_selection_model_selection_changed (priv->pages, old_pos, 1);
      else
        gtk_selection_model_selection_changed (priv->pages,
                                               MIN (old_pos, new_pos),
                                               MAX (old_pos, new_pos) - MIN (old_pos, new_pos) + 1);
    }

  gtk_stack_start_transition (stack, transition_type, transition_duration);
}

static void
update_child_visible (GtkStack     *stack,
                      GtkStackPage *child_info)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gboolean visible;

  visible = child_info->visible && gtk_widget_get_visible (child_info->widget);

  if (priv->visible_child == NULL && visible)
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);
  else if (priv->visible_child == child_info && !visible)
    set_visible_child (stack, NULL, priv->transition_type, priv->transition_duration);

  if (child_info == priv->last_visible_child)
    {
      gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
      priv->last_visible_child = NULL;
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (child_info),
                               GTK_ACCESSIBLE_STATE_HIDDEN, !visible,
                               -1);
}

static void
stack_child_visibility_notify_cb (GObject    *obj,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GtkStack *stack = GTK_STACK (user_data);
  GtkStackPage *child_info;

  child_info = find_child_info_for_widget (stack, GTK_WIDGET (obj));
  g_return_if_fail (child_info != NULL);

  update_child_visible (stack, child_info);
}

/**
 * gtk_stack_add_titled:
 * @stack: a `GtkStack`
 * @child: the widget to add
 * @name: (nullable): the name for @child
 * @title: a human-readable title for @child
 *
 * Adds a child to @stack.
 *
 * The child is identified by the @name. The @title
 * will be used by `GtkStackSwitcher` to represent
 * @child in a tab bar, so it should be short.
 *
 * Returns: (transfer none): the `GtkStackPage` for @child
 */
GtkStackPage *
gtk_stack_add_titled (GtkStack   *stack,
                      GtkWidget  *child,
                      const char *name,
                      const char *title)
{
  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  return gtk_stack_add_internal (stack, child, name, title);
}

/**
 * gtk_stack_add_child:
 * @stack: a `GtkStack`
 * @child: the widget to add
 *
 * Adds a child to @stack.
 *
 * Returns: (transfer none): the `GtkStackPage` for @child
 */
GtkStackPage *
gtk_stack_add_child (GtkStack   *stack,
                     GtkWidget  *child)
{
  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  return gtk_stack_add_internal (stack, child, NULL, NULL);
}

/**
 * gtk_stack_add_named:
 * @stack: a `GtkStack`
 * @child: the widget to add
 * @name: (nullable): the name for @child
 *
 * Adds a child to @stack.
 *
 * The child is identified by the @name.
 *
 * Returns: (transfer none): the `GtkStackPage` for @child
 */
GtkStackPage *
gtk_stack_add_named (GtkStack   *stack,
                     GtkWidget  *child,
                     const char *name)
{
  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  return gtk_stack_add_internal (stack, child, name, NULL);
}

static GtkStackPage *
gtk_stack_add_internal (GtkStack   *stack,
                        GtkWidget  *child,
                        const char *name,
                        const char *title)
{
  GtkStackPage *child_info;

  g_return_val_if_fail (child != NULL, NULL);

  child_info = g_object_new (GTK_TYPE_STACK_PAGE,
                             "child", child,
                             "name", name,
                             "title", title,
                             NULL);

  gtk_stack_add_page (stack, child_info);

  g_object_unref (child_info);

  return child_info;
}

static void
gtk_stack_add_page (GtkStack     *stack,
                    GtkStackPage *child_info)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  guint idx;

  g_return_if_fail (child_info->widget != NULL);

  if (child_info->name)
    {
      for (idx = 0; idx < priv->children->len; idx++)
        {
          GtkStackPage *info = g_ptr_array_index (priv->children, idx);
          if (info->name &&
              g_strcmp0 (info->name, child_info->name) == 0)
            {
              g_warning ("While adding page: duplicate child name in GtkStack: %s", child_info->name);
              break;
            }
        }
    }


  if (priv->children->len > 0)
    {
      GtkStackPage *prev_last = g_ptr_array_index (priv->children, priv->children->len - 1);

      prev_last->next_page = child_info;
    }
  else
    {
      child_info->next_page = NULL;
    }

  g_ptr_array_add (priv->children, g_object_ref (child_info));

  gtk_widget_set_child_visible (child_info->widget, FALSE);
  gtk_widget_set_parent (child_info->widget, GTK_WIDGET (stack));

  if (priv->pages)
    {
      g_list_model_items_changed (G_LIST_MODEL (priv->pages), priv->children->len - 1, 0, 1);
      g_object_notify_by_pspec (G_OBJECT (priv->pages), pages_properties[PAGES_PROP_N_ITEMS]);
    }

  g_signal_connect (child_info->widget, "notify::visible",
                    G_CALLBACK (stack_child_visibility_notify_cb), stack);

  if (priv->visible_child == NULL &&
      gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);

  if (priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] || priv->homogeneous[GTK_ORIENTATION_VERTICAL] || priv->visible_child == child_info)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

static void
stack_remove (GtkStack  *stack,
              GtkWidget *child,
              gboolean   in_dispose)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;
  gboolean was_visible;

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    return;

  g_signal_handlers_disconnect_by_func (child,
                                        stack_child_visibility_notify_cb,
                                        stack);

  was_visible = gtk_widget_get_visible (child);

  if (priv->visible_child == child_info)
    priv->visible_child = NULL;

  if (priv->last_visible_child == child_info)
    priv->last_visible_child = NULL;

  gtk_widget_unparent (child);

  g_clear_object (&child_info->widget);

    g_ptr_array_remove (priv->children, child_info);

  for (guint prev_idx = 0; prev_idx < priv->children->len; prev_idx++)
    {
      GtkStackPage *prev_page = g_ptr_array_index (priv->children, prev_idx);
      if (prev_page->next_page == child_info)
        {
          prev_page->next_page = child_info->next_page;
          break;
        }
    }

  g_object_unref (child_info);

  if (!in_dispose &&
      (priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] || priv->homogeneous[GTK_ORIENTATION_VERTICAL]) &&
      was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

/**
 * gtk_stack_remove:
 * @stack: a `GtkStack`
 * @child: the child to remove
 *
 * Removes a child widget from @stack.
 */
void
gtk_stack_remove (GtkStack  *stack,
                  GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  guint position;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (stack));

  for (position = 0; position < priv->children->len; position++)
    {
      GtkStackPage *page = g_ptr_array_index (priv->children, position);
      if (page->widget == child)
        break;
    }

  stack_remove (stack, child, FALSE);

  if (priv->pages)
    {
      g_list_model_items_changed (G_LIST_MODEL (priv->pages), position, 1, 0);
      g_object_notify_by_pspec (G_OBJECT (priv->pages), pages_properties[PAGES_PROP_N_ITEMS]);
    }
}

/**
 * gtk_stack_get_page:
 * @stack: a `GtkStack`
 * @child: a child of @stack
 *
 * Returns the `GtkStackPage` object for @child.
 *
 * Returns: (transfer none): the `GtkStackPage` for @child
 */
GtkStackPage *
gtk_stack_get_page (GtkStack  *stack,
                    GtkWidget *child)
{
  return find_child_info_for_widget (stack, child);
}

/**
 * gtk_stack_get_child_by_name:
 * @stack: a `GtkStack`
 * @name: the name of the child to find
 *
 * Finds the child with the name given as the argument.
 *
 * Returns %NULL if there is no child with this name.
 *
 * Returns: (transfer none) (nullable): the requested child
 *   of the `GtkStack`
 */
GtkWidget *
gtk_stack_get_child_by_name (GtkStack    *stack,
                             const char *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *info;
  guint idx;

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (idx = 0; idx < priv->children->len; idx++)
    {
      info = g_ptr_array_index (priv->children, idx);
      if (info->name && strcmp (info->name, name) == 0)
        return info->widget;
    }

  return NULL;
}

/**
 * gtk_stack_page_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkStackPage`
 *
 * Returns the stack child to which @self belongs.
 *
 * Returns: (transfer none): the child to which @self belongs
 */
GtkWidget *
gtk_stack_page_get_child (GtkStackPage *self)
{
  return self->widget;
}

/**
 * gtk_stack_set_hhomogeneous: (attributes org.gtk.Method.set_property=hhomogeneous)
 * @stack: a `GtkStack`
 * @hhomogeneous: %TRUE to make @stack horizontally homogeneous
 *
 * Sets the `GtkStack` to be horizontally homogeneous or not.
 *
 * If it is homogeneous, the `GtkStack` will request the same
 * width for all its children. If it isn't, the stack
 * may change width when a different child becomes visible.
 */
void
gtk_stack_set_hhomogeneous (GtkStack *stack,
                            gboolean  hhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  hhomogeneous = !!hhomogeneous;

  if (priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] == hhomogeneous)
    return;

  priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] = hhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HHOMOGENEOUS]);
}

/**
 * gtk_stack_get_hhomogeneous: (attributes org.gtk.Method.get_property=hhomogeneous)
 * @stack: a `GtkStack`
 *
 * Gets whether @stack is horizontally homogeneous.
 *
 * Returns: whether @stack is horizontally homogeneous.
 */
gboolean
gtk_stack_get_hhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->homogeneous[GTK_ORIENTATION_HORIZONTAL];
}

/**
 * gtk_stack_set_vhomogeneous: (attributes org.gtk.Method.set_property=vhomogeneous)
 * @stack: a `GtkStack`
 * @vhomogeneous: %TRUE to make @stack vertically homogeneous
 *
 * Sets the `GtkStack` to be vertically homogeneous or not.
 *
 * If it is homogeneous, the `GtkStack` will request the same
 * height for all its children. If it isn't, the stack
 * may change height when a different child becomes visible.
 */
void
gtk_stack_set_vhomogeneous (GtkStack *stack,
                            gboolean  vhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  vhomogeneous = !!vhomogeneous;

  if (priv->homogeneous[GTK_ORIENTATION_VERTICAL] == vhomogeneous)
    return;

  priv->homogeneous[GTK_ORIENTATION_VERTICAL] = vhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VHOMOGENEOUS]);
}

/**
 * gtk_stack_get_vhomogeneous: (attributes org.gtk.Method.get_property=vhomogeneous)
 * @stack: a `GtkStack`
 *
 * Gets whether @stack is vertically homogeneous.
 *
 * Returns: whether @stack is vertically homogeneous.
 */
gboolean
gtk_stack_get_vhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->homogeneous[GTK_ORIENTATION_VERTICAL];
}

/**
 * gtk_stack_get_transition_duration: (attributes org.gtk.Method.get_property=transition-duration)
 * @stack: a `GtkStack`
 *
 * Returns the amount of time (in milliseconds) that
 * transitions between pages in @stack will take.
 *
 * Returns: the transition duration
 */
guint
gtk_stack_get_transition_duration (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), 0);

  return priv->transition_duration;
}

/**
 * gtk_stack_set_transition_duration: (attributes org.gtk.Method.set_property=transition-duration)
 * @stack: a `GtkStack`
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions between pages in @stack
 * will take.
 */
void
gtk_stack_set_transition_duration (GtkStack *stack,
                                   guint     duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_duration == duration)
    return;

  priv->transition_duration = duration;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_DURATION]);
}

/**
 * gtk_stack_get_transition_type: (attributes org.gtk.Method.get_property=transition-type)
 * @stack: a `GtkStack`
 *
 * Gets the type of animation that will be used
 * for transitions between pages in @stack.
 *
 * Returns: the current transition type of @stack
 */
GtkStackTransitionType
gtk_stack_get_transition_type (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), GTK_STACK_TRANSITION_TYPE_NONE);

  return priv->transition_type;
}

/**
 * gtk_stack_set_transition_type: (attributes org.gtk.Method.set_property=transition-type)
 * @stack: a `GtkStack`
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions between pages in @stack.
 *
 * Available types include various kinds of fades and slides.
 *
 * The transition type can be changed without problems
 * at runtime, so it is possible to change the animation
 * based on the page that is about to become current.
 */
void
gtk_stack_set_transition_type (GtkStack              *stack,
                              GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_type == transition)
    return;

  priv->transition_type = transition;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_TYPE]);
}

/**
 * gtk_stack_get_transition_running: (attributes org.gtk.Method.get_property=transition-running)
 * @stack: a `GtkStack`
 *
 * Returns whether the @stack is currently in a transition from one page to
 * another.
 *
 * Returns: %TRUE if the transition is currently running, %FALSE otherwise.
 */
gboolean
gtk_stack_get_transition_running (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return (priv->tick_id != 0);
}

/**
 * gtk_stack_set_interpolate_size: (attributes org.gtk.Method.set_property=interpolate-size)
 * @stack: A `GtkStack`
 * @interpolate_size: the new value
 *
 * Sets whether or not @stack will interpolate its size when
 * changing the visible child.
 *
 * If the [property@Gtk.Stack:interpolate-size] property is set
 * to %TRUE, @stack will interpolate its size between the current
 * one and the one it'll take after changing the visible child,
 * according to the set transition duration.
 */
void
gtk_stack_set_interpolate_size (GtkStack *stack,
                                gboolean  interpolate_size)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  g_return_if_fail (GTK_IS_STACK (stack));

  interpolate_size = !!interpolate_size;

  if (priv->interpolate_size == interpolate_size)
    return;

  priv->interpolate_size = interpolate_size;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_INTERPOLATE_SIZE]);
}

/**
 * gtk_stack_get_interpolate_size: (attributes org.gtk.Method.get_property=interpolate-size)
 * @stack: A `GtkStack`
 *
 * Returns whether the `GtkStack` is set up to interpolate between
 * the sizes of children on page switch.
 *
 * Returns: %TRUE if child sizes are interpolated
 */
gboolean
gtk_stack_get_interpolate_size (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->interpolate_size;
}



/**
 * gtk_stack_get_visible_child: (attributes org.gtk.Method.get_property=visible-child)
 * @stack: a `GtkStack`
 *
 * Gets the currently visible child of @stack.
 *
 * Returns %NULL if there are no visible children.
 *
 * Returns: (transfer none) (nullable): the visible child of the `GtkStack`
 */
GtkWidget *
gtk_stack_get_visible_child (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  return priv->visible_child ? priv->visible_child->widget : NULL;
}

/**
 * gtk_stack_get_visible_child_name: (attributes org.gtk.Method.get_property=visible-child-name)
 * @stack: a `GtkStack`
 *
 * Returns the name of the currently visible child of @stack.
 *
 * Returns %NULL if there is no visible child.
 *
 * Returns: (transfer none) (nullable): the name of the visible child
 *   of the `GtkStack`
 */
const char *
gtk_stack_get_visible_child_name (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  if (priv->visible_child)
    return priv->visible_child->name;

  return NULL;
}

/**
 * gtk_stack_set_visible_child: (attributes org.gtk.Method.set_property=visible-child)
 * @stack: a `GtkStack`
 * @child: a child of @stack
 *
 * Makes @child the visible child of @stack.
 *
 * If @child is different from the currently visible child,
 * the transition between the two will be animated with the
 * current transition type of @stack.
 *
 * Note that the @child widget has to be visible itself
 * (see [method@Gtk.Widget.show]) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child (GtkStack  *stack,
                             GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    {
      g_warning ("Given child of type '%s' not found in GtkStack",
                 G_OBJECT_TYPE_NAME (child));
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info,
                       priv->transition_type,
                       priv->transition_duration);
}

/**
 * gtk_stack_set_visible_child_name: (attributes org.gtk.Method.set_property=visible-child-name)
 * @stack: a `GtkStack`
 * @name: the name of the child to make visible
 *
 * Makes the child with the given name visible.
 *
 * If @child is different from the currently visible child,
 * the transition between the two will be animated with the
 * current transition type of @stack.
 *
 * Note that the child widget has to be visible itself
 * (see [method@Gtk.Widget.show]) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child_name (GtkStack   *stack,
                                 const char *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  gtk_stack_set_visible_child_full (stack, name, priv->transition_type);
}

/**
 * gtk_stack_set_visible_child_full:
 * @stack: a `GtkStack`
 * @name: the name of the child to make visible
 * @transition: the transition type to use
 *
 * Makes the child with the given name visible.
 *
 * Note that the child widget has to be visible itself
 * (see [method@Gtk.Widget.show]) in order to become the visible
 * child of @stack.
 */
void
gtk_stack_set_visible_child_full (GtkStack               *stack,
                                  const char             *name,
                                  GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info, *info;
  guint idx;

  g_return_if_fail (GTK_IS_STACK (stack));

  if (name == NULL)
    return;

  child_info = NULL;
  for (idx = 0; idx < priv->children->len; idx++)
    {
      info = g_ptr_array_index (priv->children, idx);
      if (info->name != NULL &&
          strcmp (info->name, name) == 0)
        {
          child_info = info;
          break;
        }
    }

  if (child_info == NULL)
    {
      g_warning ("Child name '%s' not found in GtkStack", name);
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info, transition, priv->transition_duration);
}

static void
gtk_stack_compute_expand (GtkWidget *widget,
                          gboolean  *hexpand_p,
                          gboolean  *vexpand_p)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gboolean hexpand, vexpand;
  GtkStackPage *child_info;
  GtkWidget *child;
  guint idx;

  hexpand = FALSE;
  vexpand = FALSE;
  for (idx = 0; idx < priv->children->len; idx++)
    {
      child_info = g_ptr_array_index (priv->children, idx);
      child = child_info->widget;

      if (!hexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL))
        hexpand = TRUE;

      if (!vexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL))
        vexpand = TRUE;

      if (hexpand && vexpand)
        break;
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static GtkSizeRequestMode
gtk_stack_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_stack_snapshot_crossfade (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  double progress = gtk_progress_tracker_get_progress (&priv->tracker, FALSE);

  gtk_snapshot_push_cross_fade (snapshot, progress);

  if (priv->last_visible_child)
    {
      gtk_widget_snapshot_child (widget,
                                 priv->last_visible_child->widget,
                                 snapshot);
    }
  gtk_snapshot_pop (snapshot);

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
gtk_stack_snapshot_under (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int widget_width, widget_height;
  int x, y, width, height, pos_x, pos_y;


  x = y = 0;
  width = widget_width = gtk_widget_get_width (widget);
  height = widget_height = gtk_widget_get_height (widget);

  pos_x = pos_y = 0;

  switch ((guint) priv->active_transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
      y = 0;
      height = widget_height * (gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      pos_y = height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
      y = widget_height * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      height = widget_height - y;
      pos_y = y - widget_height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
      x = widget_width * (1 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      width = widget_width - x;
      pos_x = x - widget_width;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
      x = 0;
      width = widget_width * (gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE));
      pos_x = width;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT(x, y, width, height));

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);

  gtk_snapshot_pop (snapshot);

  if (priv->last_visible_child)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (pos_x, pos_y));
      gtk_widget_snapshot_child (widget, priv->last_visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gtk_stack_snapshot_cube (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  double progress = gtk_progress_tracker_get_progress (&priv->tracker, FALSE);

  g_assert (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT ||
            priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT);

  if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT)
    progress = 1 - progress;

  if (priv->last_visible_child && progress > 0.5)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 gtk_widget_get_width (widget) / 2.f,
                                 gtk_widget_get_height (widget) / 2.f,
                                 0));
      gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 0, 0,
                                 - gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_rotate_3d (snapshot, -90 * progress, graphene_vec3_y_axis());
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 - gtk_widget_get_width (widget) / 2.f,
                                 - gtk_widget_get_height (widget) / 2.f,
                                 gtk_widget_get_width (widget) / 2.f));
      if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
        gtk_widget_snapshot_child (widget, priv->last_visible_child->widget, snapshot);
      else
        gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             gtk_widget_get_width (widget) / 2.f,
                             gtk_widget_get_height (widget) / 2.f,
                             0));
  gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             0, 0,
                             - gtk_widget_get_width (widget) / 2.f));
  gtk_snapshot_rotate_3d (snapshot, 90 * (1.0 - progress), graphene_vec3_y_axis());
  gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                             - gtk_widget_get_width (widget) / 2.f,
                             - gtk_widget_get_height (widget) / 2.f,
                             gtk_widget_get_width (widget) / 2.f));

  if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
    gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
  else if (priv->last_visible_child)
    gtk_widget_snapshot_child (widget, priv->last_visible_child->widget, snapshot);
  gtk_snapshot_restore (snapshot);

  if (priv->last_visible_child && progress <= 0.5)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 gtk_widget_get_width (widget) / 2.f,
                                 gtk_widget_get_height (widget) / 2.f,
                                 0));
      gtk_snapshot_perspective (snapshot, 2 * gtk_widget_get_width (widget) / 1.f);
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 0, 0,
                                 - gtk_widget_get_width (widget) / 2.f));
      gtk_snapshot_rotate_3d (snapshot, -90 * progress, graphene_vec3_y_axis());
      gtk_snapshot_translate_3d (snapshot, &GRAPHENE_POINT3D_INIT (
                                 - gtk_widget_get_width (widget) / 2.f,
                                 - gtk_widget_get_height (widget) / 2.f,
                                 gtk_widget_get_width (widget) / 2.f));
      if (priv->active_transition_type == GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT)
        gtk_widget_snapshot_child (widget, priv->last_visible_child->widget, snapshot);
      else
        gtk_widget_snapshot_child (widget, priv->visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gtk_stack_snapshot_slide (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->last_visible_child)
    {
      int x, y;
      int width, height;

      width = gtk_widget_get_width (widget);
      height = gtk_widget_get_height (widget);

      x = get_bin_window_x (stack);
      y = get_bin_window_y (stack);

      switch ((guint) priv->active_transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          x -= width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          x += width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
          y -= height;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
          y += height;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
          y = 0;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          x = 0;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      if (priv->last_visible_child != NULL)
        {
          if (gtk_widget_get_valign (priv->last_visible_child->widget) == GTK_ALIGN_END &&
              priv->last_visible_widget_height > height)
            y -= priv->last_visible_widget_height - height;
          else if (gtk_widget_get_valign (priv->last_visible_child->widget) == GTK_ALIGN_CENTER)
            y -= (priv->last_visible_widget_height - height) / 2;
        }

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gtk_widget_snapshot_child (widget, priv->last_visible_child->widget, snapshot);
      gtk_snapshot_restore (snapshot);
     }

  gtk_widget_snapshot_child (widget,
                             priv->visible_child->widget,
                             snapshot);
}

static void
gtk_stack_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->visible_child)
    {
      if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
        {
          gtk_snapshot_push_clip (snapshot,
                                  &GRAPHENE_RECT_INIT(
                                      0, 0,
                                      gtk_widget_get_width (widget),
                                      gtk_widget_get_height (widget)
                                  ));

          switch (priv->active_transition_type)
            {
            case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
              gtk_stack_snapshot_crossfade (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_UP:
            case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
              gtk_stack_snapshot_slide (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
            case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
              gtk_stack_snapshot_under (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT:
            case GTK_STACK_TRANSITION_TYPE_ROTATE_RIGHT:
              gtk_stack_snapshot_cube (widget, snapshot);
              break;
            case GTK_STACK_TRANSITION_TYPE_NONE:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
            case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
            case GTK_STACK_TRANSITION_TYPE_ROTATE_LEFT_RIGHT:
            default:
              g_assert_not_reached ();
            }

          gtk_snapshot_pop (snapshot);
        }
      else
        gtk_widget_snapshot_child (widget,
                                   priv->visible_child->widget,
                                   snapshot);
    }
}

static void
gtk_stack_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAllocation child_allocation;

  if (priv->last_visible_child)
    {
      int child_width, child_height;
      int min, nat;

      gtk_widget_measure (priv->last_visible_child->widget, GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &min, &nat, NULL, NULL);
      child_width = MAX (min, width);
      gtk_widget_measure (priv->last_visible_child->widget, GTK_ORIENTATION_VERTICAL,
                          child_width,
                          &min, &nat, NULL, NULL);
      child_height = MAX (min, height);

      gtk_widget_size_allocate (priv->last_visible_child->widget,
                                &(GtkAllocation) { 0, 0, child_width, child_height }, -1);
    }

  child_allocation.x = get_bin_window_x (stack);
  child_allocation.y = get_bin_window_y (stack);
  child_allocation.width = width;
  child_allocation.height = height;

  if (priv->visible_child)
    {
      int min_width;
      int min_height;

      gtk_widget_measure (priv->visible_child->widget, GTK_ORIENTATION_HORIZONTAL,
                          height, &min_width, NULL, NULL, NULL);
      child_allocation.width = MAX (child_allocation.width, min_width);

      gtk_widget_measure (priv->visible_child->widget, GTK_ORIENTATION_VERTICAL,
                          child_allocation.width, &min_height, NULL, NULL, NULL);
      child_allocation.height = MAX (child_allocation.height, min_height);

      if (child_allocation.width > width)
        {
          GtkAlign halign = gtk_widget_get_halign (priv->visible_child->widget);

          if (halign == GTK_ALIGN_CENTER || halign == GTK_ALIGN_FILL)
            child_allocation.x = (width - child_allocation.width) / 2;
          else if (halign == GTK_ALIGN_END)
            child_allocation.x = (width - child_allocation.width);
        }

      if (child_allocation.height > height)
        {
          GtkAlign valign = gtk_widget_get_valign (priv->visible_child->widget);

          if (valign == GTK_ALIGN_CENTER || valign == GTK_ALIGN_FILL)
            child_allocation.y = (height - child_allocation.height) / 2;
          else if (valign == GTK_ALIGN_END)
            child_allocation.y = (height - child_allocation.height);
        }

      gtk_widget_size_allocate (priv->visible_child->widget, &child_allocation, -1);
    }
}

#define LERP(a, b, t) ((a) + (((b) - (a)) * (1.0 - (t))))
static void
gtk_stack_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackPage *child_info;
  GtkWidget *child;
  int child_min, child_nat;
  guint idx;

  *minimum = 0;
  *natural = 0;

  for (idx = 0; idx < priv->children->len; idx++)
    {
      child_info = g_ptr_array_index (priv->children, idx);
      child = child_info->widget;

      if (!priv->homogeneous[orientation] &&
          priv->visible_child != child_info)
        continue;

      if (gtk_widget_get_visible (child))
        {
          if (!priv->homogeneous[OPPOSITE_ORIENTATION(orientation)] && priv->visible_child != child_info)
            {
              int min_for_size;

              gtk_widget_measure (child, OPPOSITE_ORIENTATION (orientation), -1, &min_for_size, NULL, NULL, NULL);

              gtk_widget_measure (child, orientation, MAX (min_for_size, for_size), &child_min, &child_nat, NULL, NULL);
            }
          else
            gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }
    }

  if (priv->last_visible_child != NULL && !priv->homogeneous[orientation])
    {
      double t = priv->interpolate_size ? gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE) : 1.0;
      int last_size;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        last_size = priv->last_visible_widget_width;
      else
        last_size = priv->last_visible_widget_height;

      *minimum = LERP (*minimum, last_size, t);
      *natural = LERP (*natural, last_size, t);
    }
}

static void
gtk_stack_init (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  priv->homogeneous[GTK_ORIENTATION_VERTICAL] = TRUE;
  priv->homogeneous[GTK_ORIENTATION_HORIZONTAL] = TRUE;
  priv->transition_duration = 200;
  priv->transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
  priv->children = g_ptr_array_new();
}

/**
 * gtk_stack_get_pages: (attributes org.gtk.Method.get_property=pages)
 * @stack: a `GtkStack`
 *
 * Returns a `GListModel` that contains the pages of the stack.
 *
 * This can be used to keep an up-to-date view. The model also
 * implements [iface@Gtk.SelectionModel] and can be used to track
 * and modify the visible page.
 *
 * Returns: (transfer full): a `GtkSelectionModel` for the stack's children
 */
GtkSelectionModel *
gtk_stack_get_pages (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  if (priv->pages)
    return g_object_ref (priv->pages);

  priv->pages = GTK_SELECTION_MODEL (gtk_stack_pages_new (stack));
  g_object_add_weak_pointer (G_OBJECT (priv->pages), (gpointer *)&priv->pages);

  return priv->pages;
}

/**
 * gtk_stack_page_get_visible: (attributes org.gtk.Method.get_property=visible)
 * @self: a `GtkStackPage`
 *
 * Returns whether @page is visible in its `GtkStack`.
 *
 * This is independent from the [property@Gtk.Widget:visible]
 * property of its widget.
 *
 * Returns: %TRUE if @page is visible
 */
gboolean
gtk_stack_page_get_visible (GtkStackPage *self)
{
  g_return_val_if_fail (GTK_IS_STACK_PAGE (self), FALSE);

  return self->visible;
}

/**
 * gtk_stack_page_set_visible: (attributes org.gtk.Method.set_property=visible)
 * @self: a `GtkStackPage`
 * @visible: The new property value
 *
 * Sets whether @page is visible in its `GtkStack`.
 */
void
gtk_stack_page_set_visible (GtkStackPage *self,
                            gboolean      visible)
{
  g_return_if_fail (GTK_IS_STACK_PAGE (self));

  visible = !!visible;

  if (visible == self->visible)
    return;

  self->visible = visible;

  if (self->widget && gtk_widget_get_parent (self->widget))
    update_child_visible (GTK_STACK (gtk_widget_get_parent (self->widget)), self);

  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_VISIBLE]);
}

/**
 * gtk_stack_page_get_needs_attention: (attributes org.gtk.Method.get_property=needs-attention)
 * @self: a `GtkStackPage`
 *
 * Returns whether the page is marked as “needs attention”.
 *
 * Returns: The value of the [property@Gtk.StackPage:needs-attention]
 *   property.
 */
gboolean
gtk_stack_page_get_needs_attention (GtkStackPage *self)
{
  return self->needs_attention;
}

/**
 * gtk_stack_page_set_needs_attention: (attributes org.gtk.Method.set_property=needs-attention)
 * @self: a `GtkStackPage`
 * @setting: the new value to set
 *
 * Sets whether the page is marked as “needs attention”.
 */
void
gtk_stack_page_set_needs_attention (GtkStackPage *self,
                                    gboolean      setting)
{
  setting = !!setting;

  if (setting == self->needs_attention)
    return;

  self->needs_attention = setting;
  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_NEEDS_ATTENTION]);
}

/**
 * gtk_stack_page_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @self: a `GtkStackPage`
 *
 * Gets whether underlines in the page title indicate mnemonics.
 *
 * Returns: The value of the [property@Gtk.StackPage:use-underline] property
 */
gboolean
gtk_stack_page_get_use_underline (GtkStackPage *self)
{
  return self->use_underline;
}

/**
 * gtk_stack_page_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @self: a `GtkStackPage`
 * @setting: the new value to set
 *
 * Sets whether underlines in the page title indicate mnemonics.
 */
void
gtk_stack_page_set_use_underline (GtkStackPage *self,
                                  gboolean      setting)
{
  setting = !!setting;

  if (setting == self->use_underline)
    return;

  self->use_underline = setting;
  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_USE_UNDERLINE]);
}


/**
 * gtk_stack_page_get_name: (attributes org.gtk.Method.get_property=name)
 * @self: a `GtkStackPage`
 *
 * Returns the name of the page.
 *
 * Returns: (nullable): The value of the [property@Gtk.StackPage:name] property
 */
const char *
gtk_stack_page_get_name (GtkStackPage *self)
{
  g_return_val_if_fail (GTK_IS_STACK_PAGE (self), NULL);

  return self->name;
}

/**
 * gtk_stack_page_set_name: (attributes org.gtk.Method.set_property=name)
 * @self: a `GtkStackPage`
 * @setting: (transfer none): the new value to set
 *
 * Sets the name of the page.
 */
void
gtk_stack_page_set_name (GtkStackPage *self,
                         const char   *setting)
{
  GtkStack *stack = NULL;
  GtkStackPrivate *priv = NULL;

  g_return_if_fail (GTK_IS_STACK_PAGE (self));

  if (self->widget &&
      gtk_widget_get_parent (self->widget) &&
      GTK_IS_STACK (gtk_widget_get_parent (self->widget)))
    {
      guint idx;

      stack = GTK_STACK (gtk_widget_get_parent (self->widget));
      priv = gtk_stack_get_instance_private (stack);

      for (idx = 0; idx < priv->children->len; idx++)
        {
          GtkStackPage *info2 = g_ptr_array_index (priv->children, idx);
          if (self == info2)
            continue;

          if (g_strcmp0 (info2->name, setting) == 0)
            {
              g_warning ("Duplicate child name in GtkStack: %s", setting);
              break;
            }
        }
    }

  if (setting == self->name)
    return;

  g_free (self->name);
  self->name = g_strdup (setting);
  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_NAME]);

  if (priv && priv->visible_child == self)
    g_object_notify_by_pspec (G_OBJECT (stack),
                              stack_props[PROP_VISIBLE_CHILD_NAME]);
}

/**
 * gtk_stack_page_get_title: (attributes org.gtk.Method.get_property=title)
 * @self: a `GtkStackPage`
 *
 * Gets the page title.
 *
 * Returns: (nullable): The value of the [property@Gtk.StackPage:title] property
 */
const char *
gtk_stack_page_get_title (GtkStackPage *self)
{
  g_return_val_if_fail (GTK_IS_STACK_PAGE (self), NULL);

  return self->title;
}

/**
 * gtk_stack_page_set_title: (attributes org.gtk.Method.set_property=title)
 * @self: a `GtkStackPage`
 * @setting: (transfer none): the new value to set
 *
 * Sets the page title.
 */
void
gtk_stack_page_set_title (GtkStackPage *self,
                          const char   *setting)
{
  g_return_if_fail (GTK_IS_STACK_PAGE (self));

  if (setting == self->title)
    return;

  g_free (self->title);
  self->title = g_strdup (setting);
  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_TITLE]);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, self->title,
                                  -1);
}

/**
 * gtk_stack_page_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @self: a `GtkStackPage`
 *
 * Returns the icon name of the page.
 *
 * Returns: (nullable): The value of the [property@Gtk.StackPage:icon-name] property
 */
const char *
gtk_stack_page_get_icon_name (GtkStackPage *self)
{
  g_return_val_if_fail (GTK_IS_STACK_PAGE (self), NULL);

  return self->icon_name;
}

/**
 * gtk_stack_page_set_icon_name: (attributes org.gtk.Method.set_property=icon-name)
 * @self: a `GtkStackPage`
 * @setting: (transfer none): the new value to set
 *
 * Sets the icon name of the page.
 */
void
gtk_stack_page_set_icon_name (GtkStackPage *self,
                              const char   *setting)
{
  g_return_if_fail (GTK_IS_STACK_PAGE (self));

  if (setting == self->icon_name)
    return;

  g_free (self->icon_name);
  self->icon_name = g_strdup (setting);
  g_object_notify_by_pspec (G_OBJECT (self), stack_page_props[CHILD_PROP_ICON_NAME]);
}
