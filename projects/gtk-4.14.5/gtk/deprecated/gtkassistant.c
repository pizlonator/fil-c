/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003  Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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

/**
 * GtkAssistant:
 *
 * `GtkAssistant` is used to represent a complex as a series of steps.
 *
 * ![An example GtkAssistant](assistant.png)
 *
 * Each step consists of one or more pages. `GtkAssistant` guides the user
 * through the pages, and controls the page flow to collect the data needed
 * for the operation.
 *
 * `GtkAssistant` handles which buttons to show and to make sensitive based
 * on page sequence knowledge and the [enum@Gtk.AssistantPageType] of each
 * page in addition to state information like the *completed* and *committed*
 * page statuses.
 *
 * If you have a case that doesn’t quite fit in `GtkAssistant`s way of
 * handling buttons, you can use the %GTK_ASSISTANT_PAGE_CUSTOM page
 * type and handle buttons yourself.
 *
 * `GtkAssistant` maintains a `GtkAssistantPage` object for each added
 * child, which holds additional per-child properties. You
 * obtain the `GtkAssistantPage` for a child with [method@Gtk.Assistant.get_page].
 *
 * # GtkAssistant as GtkBuildable
 *
 * The `GtkAssistant` implementation of the `GtkBuildable` interface
 * exposes the @action_area as internal children with the name
 * “action_area”.
 *
 * To add pages to an assistant in `GtkBuilder`, simply add it as a
 * child to the `GtkAssistant` object. If you need to set per-object
 * properties, create a `GtkAssistantPage` object explicitly, and
 * set the child widget as a property on it.
 *
 * # CSS nodes
 *
 * `GtkAssistant` has a single CSS node with the name window and style
 * class .assistant.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */

/**
 * GtkAssistantPage:
 *
 * `GtkAssistantPage` is an auxiliary object used by `GtkAssistant.
 *
 * Deprecated: 4.10: This object will be removed in GTK 5
 */

#include "config.h"

#include "gtkassistant.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkframe.h"
#include "gtkheaderbar.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtklistlistmodelprivate.h"
#include "gtkmaplistmodel.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtksizegroup.h"
#include "gtksizerequest.h"
#include "gtkstack.h"
#include "gtktypebuiltins.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct _GtkAssistantPageClass   GtkAssistantPageClass;

struct _GtkAssistantPage
{
  GObject instance;
  GtkAssistantPageType type;
  guint      complete     : 1;
  guint      complete_set : 1;

  char *title;

  GtkWidget *page;
  GtkWidget *regular_title;
  GtkWidget *current_title;
};

struct _GtkAssistantPageClass
{
  GObjectClass parent_class;
};

typedef struct _GtkAssistantClass   GtkAssistantClass;

struct _GtkAssistant
{
  GtkWindow  parent;

  GtkWidget *cancel;
  GtkWidget *forward;
  GtkWidget *back;
  GtkWidget *apply;
  GtkWidget *close;
  GtkWidget *last;

  GtkWidget *sidebar;
  GtkWidget *content;
  GtkWidget *action_area;
  GtkWidget *headerbar;
  int use_header_bar;
  gboolean constructed;

  GList     *pages;
  GSList    *visited_pages;
  GtkAssistantPage *current_page;

  GtkSizeGroup *button_size_group;
  GtkSizeGroup *title_size_group;

  GtkAssistantPageFunc forward_function;
  gpointer forward_function_data;
  GDestroyNotify forward_data_destroy;

  GListModel *model;

  int extra_buttons;

  guint committed : 1;
};

struct _GtkAssistantClass
{
  GtkWindowClass parent_class;

  void (* prepare) (GtkAssistant *assistant, GtkWidget *page);
  void (* apply)   (GtkAssistant *assistant);
  void (* close)   (GtkAssistant *assistant);
  void (* cancel)  (GtkAssistant *assistant);
};

#define GTK_TYPE_ASSISTANT_PAGES (gtk_assistant_pages_get_type ())
G_DECLARE_FINAL_TYPE (GtkAssistantPages, gtk_assistant_pages, GTK, ASSISTANT_PAGES, GObject)

struct _GtkAssistantPages
{
  GObject parent_instance;
  GtkAssistant *assistant;
};

struct _GtkAssistantPagesClass
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

static void     gtk_assistant_dispose            (GObject           *object);
static void     gtk_assistant_map                (GtkWidget         *widget);
static void     gtk_assistant_unmap              (GtkWidget         *widget);
static gboolean gtk_assistant_close_request      (GtkWindow         *window);

static void     gtk_assistant_page_set_property  (GObject           *object,
                                                  guint              property_id,
                                                  const GValue      *value,
                                                  GParamSpec        *pspec);
static void     gtk_assistant_page_get_property  (GObject           *object,
                                                  guint              property_id,
                                                  GValue            *value,
                                                  GParamSpec        *pspec);

static void     gtk_assistant_buildable_interface_init   (GtkBuildableIface  *iface);
static void     gtk_assistant_buildable_add_child        (GtkBuildable       *buildable,
                                                          GtkBuilder         *builder,
                                                          GObject            *child,
                                                          const char         *type);
static gboolean gtk_assistant_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                          GtkBuilder         *builder,
                                                          GObject            *child,
                                                          const char         *tagname,
                                                          GtkBuildableParser *parser,
                                                          gpointer           *data);
static void     gtk_assistant_buildable_custom_finished  (GtkBuildable       *buildable,
                                                          GtkBuilder         *builder,
                                                          GObject            *child,
                                                          const char         *tagname,
                                                          gpointer            user_data);

static GList*   find_page                                (GtkAssistant       *assistant,
                                                          GtkWidget          *page);
static void     on_assistant_close                       (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);
static void     on_assistant_apply                       (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);
static void     on_assistant_forward                     (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);
static void     on_assistant_back                        (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);
static void     on_assistant_cancel                      (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);
static void     on_assistant_last                        (GtkWidget          *widget,
                                                          GtkAssistant       *assistant);

static int        gtk_assistant_add_page                 (GtkAssistant     *assistant,
                                                          GtkAssistantPage *page_info,
                                                          int               position);

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CHILD,
  CHILD_PROP_PAGE_TYPE,
  CHILD_PROP_PAGE_TITLE,
  CHILD_PROP_PAGE_COMPLETE,
  CHILD_PROP_HAS_PADDING
};

G_DEFINE_TYPE (GtkAssistantPage, gtk_assistant_page, G_TYPE_OBJECT)

static void
gtk_assistant_page_init (GtkAssistantPage *page)
{
  page->type = GTK_ASSISTANT_PAGE_CONTENT;
}

static void
gtk_assistant_page_finalize (GObject *object)
{
  GtkAssistantPage *page = GTK_ASSISTANT_PAGE (object);

  g_clear_object (&page->page);
  g_free (page->title);

  G_OBJECT_CLASS (gtk_assistant_page_parent_class)->finalize (object);
}

static void
gtk_assistant_page_class_init (GtkAssistantPageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_assistant_page_finalize;
  object_class->get_property = gtk_assistant_page_get_property;
  object_class->set_property = gtk_assistant_page_set_property;

  /**
   * GtkAssistantPage:page-type:
   *
   * The type of the assistant page.
   *
   * Deprecated: 4.10: This object will be removed in GTK 5
   */
  g_object_class_install_property (object_class,
                                   CHILD_PROP_PAGE_TYPE,
                                   g_param_spec_enum ("page-type", NULL, NULL,
                                                      GTK_TYPE_ASSISTANT_PAGE_TYPE,
                                                      GTK_ASSISTANT_PAGE_CONTENT,
                                                      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkAssistantPage:title:
   *
   * The title of the page.
   *
   * Deprecated: 4.10: This object will be removed in GTK 5
   */
  g_object_class_install_property (object_class,
                                   CHILD_PROP_PAGE_TITLE,
                                   g_param_spec_string ("title", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkAssistantPage:complete:
   *
   * Whether all required fields are filled in.
   *
   * GTK uses this information to control the sensitivity
   * of the navigation buttons.
   *
   * Deprecated: 4.10: This object will be removed in GTK 5
   */
  g_object_class_install_property (object_class,
                                   CHILD_PROP_PAGE_COMPLETE,
                                   g_param_spec_boolean ("complete", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkAssistantPage:child:
   *
   * The child widget.
   *
   * Deprecated: 4.10: This object will be removed in GTK 5
   */
  g_object_class_install_property (object_class,
                                   CHILD_PROP_CHILD,
                                   g_param_spec_object ("child", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

enum
{
  CANCEL,
  PREPARE,
  APPLY,
  CLOSE,
  ESCAPE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_USE_HEADER_BAR,
  PROP_PAGES
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkAssistant, gtk_assistant, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_assistant_buildable_interface_init))

static void
set_use_header_bar (GtkAssistant *assistant,
                    int           use_header_bar)
{
  if (use_header_bar == -1)
    return;

  assistant->use_header_bar = use_header_bar;
}

static void
gtk_assistant_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      set_use_header_bar (assistant, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_assistant_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      g_value_set_int (value, assistant->use_header_bar);
      break;

    case PROP_PAGES:
      g_value_set_object (value, gtk_assistant_get_pages (assistant));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
apply_use_header_bar (GtkAssistant *assistant)
{
  gtk_widget_set_visible (assistant->action_area, !assistant->use_header_bar);
  gtk_widget_set_visible (assistant->headerbar, assistant->use_header_bar);
  if (!assistant->use_header_bar)
    gtk_window_set_titlebar (GTK_WINDOW (assistant), NULL);
}

static void
add_to_header_bar (GtkAssistant *assistant,
                   GtkWidget    *child)
{
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);

  if (child == assistant->back || child == assistant->cancel)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (assistant->headerbar), child);
  else
    gtk_header_bar_pack_end (GTK_HEADER_BAR (assistant->headerbar), child);
}

static void
add_action_widgets (GtkAssistant *assistant)
{
  GList *children, *l;
  GtkWidget *child;

  if (assistant->use_header_bar)
    {
      children = NULL;
      for (child = gtk_widget_get_last_child (assistant->action_area);
           child != NULL;
           child = gtk_widget_get_prev_sibling (child))
        children = g_list_prepend (children, child);
      for (l = children; l != NULL; l = l->next)
        {
          gboolean has_default;

          child = l->data;
          has_default = gtk_widget_has_default (child);

          g_object_ref (child);
          gtk_box_remove (GTK_BOX (assistant->action_area), child);
          add_to_header_bar (assistant, child);
          g_object_unref (child);

          if (has_default)
            {
              gtk_window_set_default_widget (GTK_WINDOW (assistant), child);
              gtk_widget_add_css_class (child, "suggested-action");
            }
        }
      g_list_free (children);
    }
}

static void
gtk_assistant_constructed (GObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  G_OBJECT_CLASS (gtk_assistant_parent_class)->constructed (object);

  assistant->constructed = TRUE;
  if (assistant->use_header_bar == -1)
    assistant->use_header_bar = FALSE;

  add_action_widgets (assistant);
  apply_use_header_bar (assistant);
}

static void
escape_cb (GtkAssistant *assistant)
{
  /* Do not allow cancelling in the middle of a progress page */
  if (assistant->current_page &&
      (assistant->current_page->type != GTK_ASSISTANT_PAGE_PROGRESS ||
       assistant->current_page->complete))
    g_signal_emit (assistant, signals [CANCEL], 0, NULL);

  /* don't run any user handlers - this is not a public signal */
  g_signal_stop_emission (assistant, signals[ESCAPE], 0);
}

static void
gtk_assistant_finalize (GObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  if (assistant->model)
    g_object_remove_weak_pointer (G_OBJECT (assistant->model), (gpointer *)&assistant->model);

  G_OBJECT_CLASS (gtk_assistant_parent_class)->finalize (object);
}

static void
gtk_assistant_class_init (GtkAssistantClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;

  gobject_class   = (GObjectClass *) class;
  widget_class    = (GtkWidgetClass *) class;
  window_class    = (GtkWindowClass *) class;

  gobject_class->dispose = gtk_assistant_dispose;
  gobject_class->finalize = gtk_assistant_finalize;
  gobject_class->constructed  = gtk_assistant_constructed;
  gobject_class->set_property = gtk_assistant_set_property;
  gobject_class->get_property = gtk_assistant_get_property;

  widget_class->map = gtk_assistant_map;
  widget_class->unmap = gtk_assistant_unmap;

  window_class->close_request = gtk_assistant_close_request;

  /**
   * GtkAssistant::cancel:
   * @assistant: the `GtkAssistant`
   *
   * Emitted when then the cancel button is clicked.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  signals[CANCEL] =
    g_signal_new (I_("cancel"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, cancel),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::prepare:
   * @assistant: the `GtkAssistant`
   * @page: the current page
   *
   * Emitted when a new page is set as the assistant's current page,
   * before making the new page visible.
   *
   * A handler for this signal can do any preparations which are
   * necessary before showing @page.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  signals[PREPARE] =
    g_signal_new (I_("prepare"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, prepare),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GtkAssistant::apply:
   * @assistant: the `GtkAssistant`
   *
   * Emitted when the apply button is clicked.
   *
   * The default behavior of the `GtkAssistant` is to switch to the page
   * after the current page, unless the current page is the last one.
   *
   * A handler for the ::apply signal should carry out the actions for
   * which the wizard has collected data. If the action takes a long time
   * to complete, you might consider putting a page of type
   * %GTK_ASSISTANT_PAGE_PROGRESS after the confirmation page and handle
   * this operation within the [signal@Gtk.Assistant::prepare] signal of
   * the progress page.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  signals[APPLY] =
    g_signal_new (I_("apply"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, apply),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::close:
   * @assistant: the `GtkAssistant`
   *
   * Emitted either when the close button of a summary page is clicked,
   * or when the apply button in the last page in the flow (of type
   * %GTK_ASSISTANT_PAGE_CONFIRM) is clicked.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  signals[CLOSE] =
    g_signal_new (I_("close"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, close),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::escape
   * @assistant: the `GtkAssistant`
   *
   * The action signal for the Escape binding.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  signals[ESCAPE] =
    g_signal_new_class_handler (I_("escape"),
                                G_TYPE_FROM_CLASS (gobject_class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (escape_cb),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "escape",
                                       NULL);

  /**
   * GtkAssistant:use-header-bar:
   *
   * %TRUE if the assistant uses a `GtkHeaderBar` for action buttons
   * instead of the action-area.
   *
   * For technical reasons, this property is declared as an integer
   * property, but you should only set it to %TRUE or %FALSE.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_HEADER_BAR,
                                   g_param_spec_int ("use-header-bar", NULL, NULL,
                                                     -1, 1, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkAssistant:pages:
   *
   * `GListModel` containing the pages.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PAGES,
                                   g_param_spec_object ("pages", NULL, NULL,
                                                        G_TYPE_LIST_MODEL,
                                                        GTK_PARAM_READABLE));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkassistant.ui");

  gtk_widget_class_bind_template_child_internal (widget_class, GtkAssistant, action_area);
  gtk_widget_class_bind_template_child_internal (widget_class, GtkAssistant, headerbar);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, content);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, cancel);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, forward);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, back);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, apply);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, close);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, last);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, sidebar);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, button_size_group);
  gtk_widget_class_bind_template_child (widget_class, GtkAssistant, title_size_group);

  gtk_widget_class_bind_template_callback (widget_class, on_assistant_close);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_apply);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_forward);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_back);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_cancel);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_last);
}

static int
default_forward_function (int current_page, gpointer data)
{
  GtkAssistant *assistant = GTK_ASSISTANT (data);
  GtkAssistantPage *page_info;
  GList *page_node;

  page_node = g_list_nth (assistant->pages, ++current_page);

  if (!page_node)
    return -1;

  page_info = (GtkAssistantPage *) page_node->data;

  while (page_node && !gtk_widget_get_visible (page_info->page))
    {
      page_node = page_node->next;
      current_page++;

      if (page_node)
        page_info = (GtkAssistantPage *) page_node->data;
    }

  return current_page;
}

static gboolean
last_button_visible (GtkAssistant *assistant, GtkAssistantPage *page)
{
  GtkAssistantPage *page_info;
  int count, page_num, n_pages;

  if (page == NULL)
    return FALSE;

  if (page->type != GTK_ASSISTANT_PAGE_CONTENT)
    return FALSE;

  count = 0;
  page_num = g_list_index (assistant->pages, page);
  n_pages  = g_list_length (assistant->pages);
  page_info = page;

  while (page_num >= 0 && page_num < n_pages &&
         page_info->type == GTK_ASSISTANT_PAGE_CONTENT &&
         (count == 0 || page_info->complete) &&
         count < n_pages)
    {
      page_num = (assistant->forward_function) (page_num, assistant->forward_function_data);
      page_info = g_list_nth_data (assistant->pages, page_num);

      count++;
    }

  /* Make the last button visible if we can skip multiple
   * pages and end on a confirmation or summary page
   */
  if (count > 1 && page_info &&
      (page_info->type == GTK_ASSISTANT_PAGE_CONFIRM ||
       page_info->type == GTK_ASSISTANT_PAGE_SUMMARY))
    return TRUE;
  else
    return FALSE;
}

static void
update_actions_size (GtkAssistant *assistant)
{
  GList *l;
  GtkAssistantPage *page;
  int buttons, page_buttons;

  if (!assistant->current_page)
    return;

  /* Some heuristics to find out how many buttons we should
   * reserve space for. It is possible to trick this code
   * with page forward functions and invisible pages, etc.
   */
  buttons = 0;
  for (l = assistant->pages; l; l = l->next)
    {
      page = l->data;

      if (!gtk_widget_get_visible (page->page))
        continue;

      page_buttons = 2; /* cancel, forward/apply/close */
      if (l != assistant->pages)
        page_buttons += 1; /* back */
      if (last_button_visible (assistant, page))
        page_buttons += 1; /* last */

      buttons = MAX (buttons, page_buttons);
    }

  buttons += assistant->extra_buttons;

  gtk_widget_set_size_request (assistant->action_area,
                               buttons * gtk_widget_get_allocated_width (assistant->cancel) + (buttons - 1) * 6,
                               -1);
}

static void
compute_last_button_state (GtkAssistant *assistant)
{
  gtk_widget_set_sensitive (assistant->last, assistant->current_page->complete);
  gtk_widget_set_visible (assistant->last,
                          last_button_visible (assistant, assistant->current_page));
}

static void
compute_progress_state (GtkAssistant *assistant)
{
  int page_num, n_pages;

  n_pages = gtk_assistant_get_n_pages (assistant);
  page_num = gtk_assistant_get_current_page (assistant);

  page_num = (assistant->forward_function) (page_num, assistant->forward_function_data);
  gtk_widget_set_visible (assistant->forward, page_num >= 0 && page_num < n_pages);
}

static void
update_buttons_state (GtkAssistant *assistant)
{
  if (!assistant->current_page)
    return;

  switch (assistant->current_page->type)
    {
    case GTK_ASSISTANT_PAGE_INTRO:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->forward, assistant->current_page->complete);
      gtk_window_set_default_widget (GTK_WINDOW (assistant), assistant->forward);
      gtk_widget_set_visible (assistant->forward, TRUE);
      gtk_widget_set_visible (assistant->back, FALSE);
      gtk_widget_set_visible (assistant->apply, FALSE);
      gtk_widget_set_visible (assistant->close, FALSE);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CONFIRM:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->apply, assistant->current_page->complete);
      gtk_window_set_default_widget (GTK_WINDOW (assistant), assistant->apply);
      gtk_widget_set_visible (assistant->back, TRUE);
      gtk_widget_set_visible (assistant->apply, TRUE);
      gtk_widget_set_visible (assistant->forward, FALSE);
      gtk_widget_set_visible (assistant->close, FALSE);
      gtk_widget_set_visible (assistant->last, FALSE);
      break;
    case GTK_ASSISTANT_PAGE_CONTENT:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->forward, assistant->current_page->complete);
      gtk_window_set_default_widget (GTK_WINDOW (assistant), assistant->forward);
      gtk_widget_set_visible (assistant->back, TRUE);
      gtk_widget_set_visible (assistant->forward, TRUE);
      gtk_widget_set_visible (assistant->apply, FALSE);
      gtk_widget_set_visible (assistant->close, FALSE);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_SUMMARY:
      gtk_widget_set_sensitive (assistant->close, assistant->current_page->complete);
      gtk_window_set_default_widget (GTK_WINDOW (assistant), assistant->close);
      gtk_widget_set_visible (assistant->close, TRUE);
      gtk_widget_set_visible (assistant->back, FALSE);
      gtk_widget_set_visible (assistant->forward, FALSE);
      gtk_widget_set_visible (assistant->apply, FALSE);
      gtk_widget_set_visible (assistant->last, FALSE);
      break;
    case GTK_ASSISTANT_PAGE_PROGRESS:
      gtk_widget_set_sensitive (assistant->cancel, assistant->current_page->complete);
      gtk_widget_set_sensitive (assistant->back, assistant->current_page->complete);
      gtk_widget_set_sensitive (assistant->forward, assistant->current_page->complete);
      gtk_window_set_default_widget (GTK_WINDOW (assistant), assistant->forward);
      gtk_widget_set_visible (assistant->back, TRUE);
      gtk_widget_set_visible (assistant->apply, FALSE);
      gtk_widget_set_visible (assistant->close, FALSE);
      gtk_widget_set_visible (assistant->last, FALSE);
      compute_progress_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CUSTOM:
      gtk_widget_set_visible (assistant->cancel, FALSE);
      gtk_widget_set_visible (assistant->back, FALSE);
      gtk_widget_set_visible (assistant->forward, FALSE);
      gtk_widget_set_visible (assistant->apply, FALSE);
      gtk_widget_set_visible (assistant->last, FALSE);
      gtk_widget_set_visible (assistant->close, FALSE);
      break;
    default:
      g_assert_not_reached ();
    }

  if (assistant->committed)
    gtk_widget_set_visible (assistant->cancel, FALSE);
  else if (assistant->current_page->type == GTK_ASSISTANT_PAGE_SUMMARY ||
           assistant->current_page->type == GTK_ASSISTANT_PAGE_CUSTOM)
    gtk_widget_set_visible (assistant->cancel, FALSE);
  else
    gtk_widget_set_visible (assistant->cancel, TRUE);

  /* this is quite general, we don't want to
   * go back if it's the first page
   */
  if (!assistant->visited_pages)
    gtk_widget_set_visible (assistant->back, FALSE);
}

static gboolean
update_page_title_state (GtkAssistant *assistant, GList *list)
{
  GtkAssistantPage *page, *other;
  gboolean visible;
  GList *l;

  page = list->data;

  if (page->title == NULL || page->title[0] == 0)
    visible = FALSE;
  else
    visible = gtk_widget_get_visible (page->page);

  if (page == assistant->current_page)
    {
      gtk_widget_set_visible (page->regular_title, FALSE);
      gtk_widget_set_visible (page->current_title, visible);
    }
  else
    {
      /* If multiple consecutive pages have the same title,
       * we only show it once, since it would otherwise look
       * silly. We have to be a little careful, since we
       * _always_ show the title of the current page.
       */
      if (list->prev)
        {
          other = list->prev->data;
          if (g_strcmp0 (page->title, other->title) == 0)
            visible = FALSE;
        }
      for (l = list->next; l; l = l->next)
        {
          other = l->data;
          if (g_strcmp0 (page->title, other->title) != 0)
            break;

          if (other == assistant->current_page)
            {
              visible = FALSE;
              break;
            }
        }

      gtk_widget_set_visible (page->regular_title, visible);
      gtk_widget_set_visible (page->current_title, FALSE);
    }

  return visible;
}

static void
update_title_state (GtkAssistant *assistant)
{
  GList *l;
  gboolean show_titles;

  show_titles = FALSE;
  for (l = assistant->pages; l != NULL; l = l->next)
    {
      if (update_page_title_state (assistant, l))
        show_titles = TRUE;
    }

  gtk_widget_set_visible (assistant->sidebar, show_titles);
}

static void
set_current_page (GtkAssistant *assistant,
                  int           page_num)
{
  assistant->current_page = (GtkAssistantPage *)g_list_nth_data (assistant->pages, page_num);

  g_signal_emit (assistant, signals [PREPARE], 0, assistant->current_page->page);
  /* do not continue if the prepare signal handler has already changed the
   * current page */
  if (assistant->current_page != (GtkAssistantPage *)g_list_nth_data (assistant->pages, page_num))
    return;

  update_title_state (assistant);

  gtk_window_set_title (GTK_WINDOW (assistant), assistant->current_page->title);

  gtk_stack_set_visible_child (GTK_STACK (assistant->content), assistant->current_page->page);

  /* update buttons state, flow may have changed */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    update_buttons_state (assistant);

  if (!gtk_widget_child_focus (assistant->current_page->page, GTK_DIR_TAB_FORWARD))
    {
      GtkWidget *button[6];
      int i;

      /* find the best button to focus */
      button[0] = assistant->apply;
      button[1] = assistant->close;
      button[2] = assistant->forward;
      button[3] = assistant->back;
      button[4] = assistant->cancel;
      button[5] = assistant->last;
      for (i = 0; i < 6; i++)
        {
          if (gtk_widget_get_visible (button[i]) &&
              gtk_widget_get_sensitive (button[i]))
            {
              gtk_widget_grab_focus (button[i]);
              break;
            }
        }
    }
}

static int
compute_next_step (GtkAssistant *assistant)
{
  GtkAssistantPage *page_info;
  int current_page, n_pages, next_page;

  current_page = gtk_assistant_get_current_page (assistant);
  page_info = assistant->current_page;
  n_pages = gtk_assistant_get_n_pages (assistant);

  next_page = (assistant->forward_function) (current_page,
                                        assistant->forward_function_data);

  if (next_page >= 0 && next_page < n_pages)
    {
      assistant->visited_pages = g_slist_prepend (assistant->visited_pages, page_info);
      set_current_page (assistant, next_page);

      return TRUE;
    }

  return FALSE;
}

static void
on_assistant_close (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CLOSE], 0, NULL);
}

static void
on_assistant_apply (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  gboolean success;

  g_signal_emit (assistant, signals [APPLY], 0);

  success = compute_next_step (assistant);

  /* if the assistant hasn't switched to another page, just emit
   * the CLOSE signal, it't the last page in the assistant flow
   */
  if (!success)
    g_signal_emit (assistant, signals [CLOSE], 0);
}

static void
on_assistant_forward (GtkWidget    *widget,
                      GtkAssistant *assistant)
{
  gtk_assistant_next_page (assistant);
}

static void
on_assistant_back (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  gtk_assistant_previous_page (assistant);
}

static void
on_assistant_cancel (GtkWidget    *widget,
                     GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CANCEL], 0, NULL);
}

static void
on_assistant_last (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  while (assistant->current_page->type == GTK_ASSISTANT_PAGE_CONTENT &&
         assistant->current_page->complete)
    compute_next_step (assistant);
}

static gboolean
alternative_button_order (GtkAssistant *assistant)
{
  gboolean result;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (assistant)),
                "gtk-alternative-button-order", &result,
                NULL);
  return result;
}

static void
on_page_page_notify (GtkWidget  *widget,
                     GParamSpec *arg,
                     gpointer    data)
{
  GtkAssistant *assistant = GTK_ASSISTANT (data);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_title_state (assistant);
    }
}

static void
on_page_notify (GtkAssistantPage *page,
                GParamSpec *arg,
                gpointer    data)
{
  if (page->page)
    on_page_page_notify (page->page, arg, data);
}

static void
assistant_remove_page (GtkAssistant *assistant,
                       GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *page_node;
  GList *element;

  element = find_page (assistant, page);
  if (!element)
    return;

  page_info = element->data;

  /* If this is the current page, we need to switch away. */
  if (page_info == assistant->current_page)
    {
      if (!compute_next_step (assistant))
        {
          /* The best we can do at this point is probably to pick
           * the first visible page.
           */
          page_node = assistant->pages;

          while (page_node &&
                 !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
            page_node = page_node->next;

          if (page_node == element)
            page_node = page_node->next;

          if (page_node)
            assistant->current_page = page_node->data;
          else
            assistant->current_page = NULL;
        }
    }

  g_signal_handlers_disconnect_by_func (page_info->page, on_page_page_notify, assistant);
  g_signal_handlers_disconnect_by_func (page_info, on_page_notify, assistant);

  gtk_size_group_remove_widget (assistant->title_size_group, page_info->regular_title);
  gtk_size_group_remove_widget (assistant->title_size_group, page_info->current_title);

  gtk_box_remove (GTK_BOX (assistant->sidebar), page_info->regular_title);
  gtk_box_remove (GTK_BOX (assistant->sidebar), page_info->current_title);

  assistant->pages = g_list_remove_link (assistant->pages, element);
  assistant->visited_pages = g_slist_remove_all (assistant->visited_pages, page_info);

  g_object_unref (page_info);

  g_list_free_1 (element);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_actions_size (assistant);
    }
}

static void
gtk_assistant_init (GtkAssistant *assistant)
{
  gtk_widget_add_css_class (GTK_WIDGET (assistant), "assistant");

  assistant->pages = NULL;
  assistant->current_page = NULL;
  assistant->visited_pages = NULL;

  assistant->forward_function = default_forward_function;
  assistant->forward_function_data = assistant;
  assistant->forward_data_destroy = NULL;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (assistant)),
                "gtk-dialogs-use-header", &assistant->use_header_bar,
                NULL);

  gtk_widget_init_template (GTK_WIDGET (assistant));

  if (alternative_button_order (assistant))
    {
      GList *buttons, *l;
      GtkWidget *child;

      buttons = NULL;
      for (child = gtk_widget_get_last_child (assistant->action_area);
           child != NULL;
           child = gtk_widget_get_prev_sibling (child))
        buttons = g_list_prepend (buttons, child);

      for (l = buttons; l; l = l->next)
        gtk_box_reorder_child_after (GTK_BOX (assistant->action_area), GTK_WIDGET (l->data), NULL);

      g_list_free (buttons);
    }
}

static void
gtk_assistant_page_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAssistantPage *page = GTK_ASSISTANT_PAGE (object);
  GtkWidget *assistant = NULL;

  if (page->page)
    assistant = gtk_widget_get_ancestor (page->page, GTK_TYPE_ASSISTANT);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_set_object (&page->page, g_value_get_object (value));
      break;

    case CHILD_PROP_PAGE_TYPE:
      if (page->type != g_value_get_enum (value))
        {
          page->type = g_value_get_enum (value);

          /* backwards compatibility to the era before fixing bug 604289 */
          if (page->type == GTK_ASSISTANT_PAGE_SUMMARY && !page->complete_set)
            {
              page->complete = TRUE;
              page->complete_set = FALSE;
            }

          /* Always set buttons state, a change in a future page
           * might change current page buttons
           */
          if (assistant)
            update_buttons_state (GTK_ASSISTANT (assistant));
          g_object_notify (G_OBJECT (page), "page-type");
        }
      break;

    case CHILD_PROP_PAGE_TITLE:
      g_free (page->title);
      page->title = g_value_dup_string (value);

      if (assistant)
        {
          gtk_label_set_text ((GtkLabel*) page->regular_title, page->title);
          gtk_label_set_text ((GtkLabel*) page->current_title, page->title);
          update_title_state (GTK_ASSISTANT (assistant));
        }

      g_object_notify (G_OBJECT (page), "title");

      break;

    case CHILD_PROP_PAGE_COMPLETE:
      if (page->complete != g_value_get_boolean (value))
        {
          page->complete = g_value_get_boolean (value);
          page->complete_set = TRUE;

          /* Always set buttons state, a change in a future page
           * might change current page buttons
           */
          if (assistant)
            update_buttons_state (GTK_ASSISTANT (assistant));
          g_object_notify (G_OBJECT (page), "complete");
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_page_get_property (GObject      *object,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GtkAssistantPage *page = GTK_ASSISTANT_PAGE (object);

  switch (property_id)
    {
    case CHILD_PROP_CHILD:
      g_value_set_object (value, page->page);
      break;

    case CHILD_PROP_PAGE_TYPE:
      g_value_set_enum (value, page->type);
      break;

    case CHILD_PROP_PAGE_TITLE:
      g_value_set_string (value, page->title);
      break;

    case CHILD_PROP_PAGE_COMPLETE:
      g_value_set_boolean (value, page->complete);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_dispose (GObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  if (assistant->model && g_list_length (assistant->pages))
    {
      g_list_model_items_changed (G_LIST_MODEL (assistant->model), 0, g_list_length (assistant->pages), 0);
      g_object_notify_by_pspec (G_OBJECT (assistant->model), pages_properties[PAGES_PROP_N_ITEMS]);
    }  

  /* We set current to NULL so that the remove code doesn't try
   * to do anything funny
   */
  assistant->current_page = NULL;

  if (assistant->content)
    {
      while (assistant->pages)
        gtk_assistant_remove_page (assistant, 0);

      assistant->content = NULL;
    }

  assistant->sidebar = NULL;
  assistant->action_area = NULL;

  if (assistant->forward_function)
    {
      if (assistant->forward_function_data &&
          assistant->forward_data_destroy)
        assistant->forward_data_destroy (assistant->forward_function_data);

      assistant->forward_function = NULL;
      assistant->forward_function_data = NULL;
      assistant->forward_data_destroy = NULL;
    }

  if (assistant->visited_pages)
    {
      g_slist_free (assistant->visited_pages);
      assistant->visited_pages = NULL;
    }

  G_OBJECT_CLASS (gtk_assistant_parent_class)->dispose (object);
}

static GList*
find_page (GtkAssistant  *assistant,
           GtkWidget     *page)
{
  GList *child = assistant->pages;

  while (child)
    {
      GtkAssistantPage *page_info = child->data;
      if (page_info->page == page)
        return child;

      child = child->next;
    }

  return NULL;
}

static void
gtk_assistant_map (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GList *page_node;
  GtkAssistantPage *page;
  int page_num;

  /* if there's no default page, pick the first one */
  page = NULL;
  page_num = 0;
  if (!assistant->current_page)
    {
      page_node = assistant->pages;

      while (page_node && !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
        {
          page_node = page_node->next;
          page_num++;
        }

      if (page_node)
        page = page_node->data;
    }

  if (page && gtk_widget_get_visible (page->page))
    set_current_page (assistant, page_num);

  update_buttons_state (assistant);
  update_actions_size (assistant);
  update_title_state (assistant);

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->map (widget);
}

static void
gtk_assistant_unmap (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);

  g_slist_free (assistant->visited_pages);
  assistant->visited_pages = NULL;
  assistant->current_page  = NULL;

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->unmap (widget);
}

static gboolean
gtk_assistant_close_request (GtkWindow *window)
{
  GtkAssistant *assistant = GTK_ASSISTANT (window);

  /* Do not allow cancelling in the middle of a progress page */
  if (assistant->current_page &&
      (assistant->current_page->type != GTK_ASSISTANT_PAGE_PROGRESS ||
       assistant->current_page->complete))
    g_signal_emit (assistant, signals [CANCEL], 0, NULL);

  return TRUE;
}

/**
 * gtk_assistant_new:
 *
 * Creates a new `GtkAssistant`.
 *
 * Returns: a newly created `GtkAssistant`
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget*
gtk_assistant_new (void)
{
  GtkWidget *assistant;

  assistant = g_object_new (GTK_TYPE_ASSISTANT, NULL);

  return assistant;
}

/**
 * gtk_assistant_get_current_page:
 * @assistant: a `GtkAssistant`
 *
 * Returns the page number of the current page.
 *
 * Returns: The index (starting from 0) of the current
 *   page in the @assistant, or -1 if the @assistant has no pages,
 *   or no current page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
int
gtk_assistant_get_current_page (GtkAssistant *assistant)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), -1);

  if (!assistant->pages || !assistant->current_page)
    return -1;

  return g_list_index (assistant->pages, assistant->current_page);
}

/**
 * gtk_assistant_set_current_page:
 * @assistant: a `GtkAssistant`
 * @page_num: index of the page to switch to, starting from 0.
 *   If negative, the last page will be used. If greater
 *   than the number of pages in the @assistant, nothing
 *   will be done.
 *
 * Switches the page to @page_num.
 *
 * Note that this will only be necessary in custom buttons,
 * as the @assistant flow can be set with
 * gtk_assistant_set_forward_page_func().
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_set_current_page (GtkAssistant *assistant,
                                int           page_num)
{
  GtkAssistantPage *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (assistant->pages != NULL);

  if (page_num >= 0)
    page = (GtkAssistantPage *) g_list_nth_data (assistant->pages, page_num);
  else
    {
      page = (GtkAssistantPage *) g_list_last (assistant->pages)->data;
      page_num = g_list_length (assistant->pages);
    }

  g_return_if_fail (page != NULL);

  if (assistant->current_page == page)
    return;

  /* only add the page to the visited list if the assistant is mapped,
   * if not, just use it as an initial page setting, for the cases where
   * the initial page is != to 0
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    assistant->visited_pages = g_slist_prepend (assistant->visited_pages,
                                           assistant->current_page);

  set_current_page (assistant, page_num);
}

/**
 * gtk_assistant_next_page:
 * @assistant: a `GtkAssistant`
 *
 * Navigate to the next page.
 *
 * It is a programming error to call this function when
 * there is no next page.
 *
 * This function is for use when creating pages of the
 * %GTK_ASSISTANT_PAGE_CUSTOM type.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_next_page (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  if (!compute_next_step (assistant))
    g_critical ("Page flow is broken.\n"
                "You may want to end it with a page of type\n"
                "GTK_ASSISTANT_PAGE_CONFIRM or GTK_ASSISTANT_PAGE_SUMMARY");
}

/**
 * gtk_assistant_previous_page:
 * @assistant: a `GtkAssistant`
 *
 * Navigate to the previous visited page.
 *
 * It is a programming error to call this function when
 * no previous page is available.
 *
 * This function is for use when creating pages of the
 * %GTK_ASSISTANT_PAGE_CUSTOM type.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_previous_page (GtkAssistant *assistant)
{
  GtkAssistantPage *page_info;
  GSList *page_node;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  /* skip the progress pages when going back */
  do
    {
      page_node = assistant->visited_pages;

      g_return_if_fail (page_node != NULL);

      assistant->visited_pages = assistant->visited_pages->next;
      page_info = (GtkAssistantPage *) page_node->data;
      g_slist_free_1 (page_node);
    }
  while (page_info->type == GTK_ASSISTANT_PAGE_PROGRESS ||
         !gtk_widget_get_visible (page_info->page));

  set_current_page (assistant, g_list_index (assistant->pages, page_info));
}

/**
 * gtk_assistant_get_n_pages:
 * @assistant: a `GtkAssistant`
 *
 * Returns the number of pages in the @assistant
 *
 * Returns: the number of pages in the @assistant
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
int
gtk_assistant_get_n_pages (GtkAssistant *assistant)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);

  return g_list_length (assistant->pages);
}

/**
 * gtk_assistant_get_nth_page:
 * @assistant: a `GtkAssistant`
 * @page_num: the index of a page in the @assistant,
 *   or -1 to get the last page
 *
 * Returns the child widget contained in page number @page_num.
 *
 * Returns: (nullable) (transfer none): the child widget, or %NULL
 *   if @page_num is out of bounds
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget*
gtk_assistant_get_nth_page (GtkAssistant *assistant,
                            int           page_num)
{
  GtkAssistantPage *page;
  GList *elem;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (page_num >= -1, NULL);

  if (page_num == -1)
    elem = g_list_last (assistant->pages);
  else
    elem = g_list_nth (assistant->pages, page_num);

  if (!elem)
    return NULL;

  page = (GtkAssistantPage *) elem->data;

  return page->page;
}

/**
 * gtk_assistant_prepend_page:
 * @assistant: a `GtkAssistant`
 * @page: a `GtkWidget`
 *
 * Prepends a page to the @assistant.
 *
 * Returns: the index (starting at 0) of the inserted page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
int
gtk_assistant_prepend_page (GtkAssistant *assistant,
                            GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, 0);
}

/**
 * gtk_assistant_append_page:
 * @assistant: a `GtkAssistant`
 * @page: a `GtkWidget`
 *
 * Appends a page to the @assistant.
 *
 * Returns: the index (starting at 0) of the inserted page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
int
gtk_assistant_append_page (GtkAssistant *assistant,
                           GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, -1);
}

/**
 * gtk_assistant_insert_page:
 * @assistant: a `GtkAssistant`
 * @page: a `GtkWidget`
 * @position: the index (starting at 0) at which to insert the page,
 *   or -1 to append the page to the @assistant
 *
 * Inserts a page in the @assistant at a given position.
 *
 * Returns: the index (starting from 0) of the inserted page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
int
gtk_assistant_insert_page (GtkAssistant *assistant,
                           GtkWidget    *page,
                           int           position)
{
  GtkAssistantPage *page_info;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);
  g_return_val_if_fail (gtk_widget_get_parent (page) == NULL, 0);

  page_info = g_object_new (GTK_TYPE_ASSISTANT_PAGE, NULL);
  page_info->page = g_object_ref (page);

  return gtk_assistant_add_page (assistant, page_info, position);

  g_object_unref (page_info);
}

static int
gtk_assistant_add_page (GtkAssistant *assistant,
                        GtkAssistantPage *page_info,
                        int position)
{
  int n_pages;
  GtkWidget *sibling;
  char *name;

  page_info->regular_title = gtk_label_new (page_info->title);
  page_info->current_title = gtk_label_new (page_info->title);

  gtk_label_set_xalign (GTK_LABEL (page_info->regular_title), 0.0);
  gtk_label_set_xalign (GTK_LABEL (page_info->current_title), 0.0);

  gtk_widget_set_visible (page_info->regular_title, TRUE);
  gtk_widget_set_visible (page_info->current_title, FALSE);

  gtk_widget_add_css_class (page_info->current_title, "highlight");

  gtk_size_group_add_widget (assistant->title_size_group, page_info->regular_title);
  gtk_size_group_add_widget (assistant->title_size_group, page_info->current_title);

  g_signal_connect (G_OBJECT (page_info->page), "notify::visible",
                    G_CALLBACK (on_page_page_notify), assistant);

  g_signal_connect (G_OBJECT (page_info), "notify::page-title",
                    G_CALLBACK (on_page_notify), assistant);

  g_signal_connect (G_OBJECT (page_info), "notify::page-type",
                    G_CALLBACK (on_page_notify), assistant);

  n_pages = g_list_length (assistant->pages);

  if (position < 0 || position > n_pages)
    position = n_pages;

  assistant->pages = g_list_insert (assistant->pages, g_object_ref (page_info), position);

  if (position == 0)
    sibling = NULL;
  else
    {
      int i;
      sibling = gtk_widget_get_first_child (assistant->sidebar);
      for (i = 1; i < 2 * position; i++)
        sibling = gtk_widget_get_next_sibling (sibling);
    }

  gtk_box_insert_child_after (GTK_BOX (assistant->sidebar), page_info->current_title, sibling);
  gtk_box_insert_child_after (GTK_BOX (assistant->sidebar), page_info->regular_title, sibling);

  name = g_strdup_printf ("%p", page_info->page);
  gtk_stack_add_named (GTK_STACK (assistant->content), page_info->page, name);
  g_free (name);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_actions_size (assistant);
    }

  if (assistant->model)
    {
      g_list_model_items_changed (assistant->model, position, 0, 1);
      g_object_notify_by_pspec (G_OBJECT (assistant->model), pages_properties[PAGES_PROP_N_ITEMS]);
    }

  return position;
}

/**
 * gtk_assistant_remove_page:
 * @assistant: a `GtkAssistant`
 * @page_num: the index of a page in the @assistant,
 *   or -1 to remove the last page
 *
 * Removes the @page_num’s page from @assistant.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_remove_page (GtkAssistant *assistant,
                           int           page_num)
{
  GtkWidget *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  page = gtk_assistant_get_nth_page (assistant, page_num);
  if (page)
    assistant_remove_page (assistant, page);

  if (assistant->model)
    {
      g_list_model_items_changed (assistant->model, page_num, 1, 0);
      g_object_notify_by_pspec (G_OBJECT (assistant->model), pages_properties[PAGES_PROP_N_ITEMS]);
    }
}

/**
 * gtk_assistant_set_forward_page_func:
 * @assistant: a `GtkAssistant`
 * @page_func: (nullable): the `GtkAssistantPageFunc`, or %NULL
 *   to use the default one
 * @data: user data for @page_func
 * @destroy: destroy notifier for @data
 *
 * Sets the page forwarding function to be @page_func.
 *
 * This function will be used to determine what will be
 * the next page when the user presses the forward button.
 * Setting @page_func to %NULL will make the assistant to
 * use the default forward function, which just goes to the
 * next visible page.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_set_forward_page_func (GtkAssistant         *assistant,
                                     GtkAssistantPageFunc  page_func,
                                     gpointer              data,
                                     GDestroyNotify        destroy)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  if (assistant->forward_data_destroy &&
      assistant->forward_function_data)
    (*assistant->forward_data_destroy) (assistant->forward_function_data);

  if (page_func)
    {
      assistant->forward_function = page_func;
      assistant->forward_function_data = data;
      assistant->forward_data_destroy = destroy;
    }
  else
    {
      assistant->forward_function = default_forward_function;
      assistant->forward_function_data = assistant;
      assistant->forward_data_destroy = NULL;
    }

  /* Page flow has possibly changed, so the
   * buttons state might need to change too
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    update_buttons_state (assistant);
}

static void
add_to_action_area (GtkAssistant *assistant,
                    GtkWidget    *child)
{
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE);

  gtk_box_append (GTK_BOX (assistant->action_area), child);
}

/**
 * gtk_assistant_add_action_widget:
 * @assistant: a `GtkAssistant`
 * @child: a `GtkWidget`
 *
 * Adds a widget to the action area of a `GtkAssistant`.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_add_action_widget (GtkAssistant *assistant,
                                 GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (GTK_IS_BUTTON (child))
    {
      gtk_size_group_add_widget (assistant->button_size_group, child);
      assistant->extra_buttons += 1;
      if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
        update_actions_size (assistant);
    }

  if (assistant->constructed && assistant->use_header_bar)
    add_to_header_bar (assistant, child);
  else
    add_to_action_area (assistant, child);
}

/**
 * gtk_assistant_remove_action_widget:
 * @assistant: a `GtkAssistant`
 * @child: a `GtkWidget`
 *
 * Removes a widget from the action area of a `GtkAssistant`.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_remove_action_widget (GtkAssistant *assistant,
                                    GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (GTK_IS_BUTTON (child))
    {
      gtk_size_group_remove_widget (assistant->button_size_group, child);
      assistant->extra_buttons -= 1;
      if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
        update_actions_size (assistant);
    }

  gtk_box_remove (GTK_BOX (assistant->action_area), child);
}

/**
 * gtk_assistant_set_page_title:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 * @title: the new title for @page
 *
 * Sets a title for @page.
 *
 * The title is displayed in the header area of the assistant
 * when @page is the current page.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_set_page_title (GtkAssistant *assistant,
                              GtkWidget    *page,
                              const char   *title)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  g_object_set (page_info, "title", title, NULL);
}

/**
 * gtk_assistant_get_page_title:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 *
 * Gets the title for @page.
 *
 * Returns: the title for @page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
const char *
gtk_assistant_get_page_title (GtkAssistant *assistant,
                              GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->title;
}

/**
 * gtk_assistant_set_page_type:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 * @type: the new type for @page
 *
 * Sets the page type for @page.
 *
 * The page type determines the page behavior in the @assistant.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_set_page_type (GtkAssistant         *assistant,
                             GtkWidget            *page,
                             GtkAssistantPageType  type)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  g_object_set (page_info, "page-type", type, NULL);
}

/**
 * gtk_assistant_get_page_type:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 *
 * Gets the page type of @page.
 *
 * Returns: the page type of @page
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkAssistantPageType
gtk_assistant_get_page_type (GtkAssistant *assistant,
                             GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), GTK_ASSISTANT_PAGE_CONTENT);
  g_return_val_if_fail (GTK_IS_WIDGET (page), GTK_ASSISTANT_PAGE_CONTENT);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, GTK_ASSISTANT_PAGE_CONTENT);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->type;
}

/**
 * gtk_assistant_set_page_complete:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 * @complete: the completeness status of the page
 *
 * Sets whether @page contents are complete.
 *
 * This will make @assistant update the buttons state
 * to be able to continue the task.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_set_page_complete (GtkAssistant *assistant,
                                 GtkWidget    *page,
                                 gboolean      complete)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  g_object_set (page_info, "complete", complete, NULL);
}

/**
 * gtk_assistant_get_page_complete:
 * @assistant: a `GtkAssistant`
 * @page: a page of @assistant
 *
 * Gets whether @page is complete.
 *
 * Returns: %TRUE if @page is complete.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_assistant_get_page_complete (GtkAssistant *assistant,
                                 GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (page), FALSE);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, FALSE);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->complete;
}

/**
 * gtk_assistant_update_buttons_state:
 * @assistant: a `GtkAssistant`
 *
 * Forces @assistant to recompute the buttons state.
 *
 * GTK automatically takes care of this in most situations,
 * e.g. when the user goes to a different page, or when the
 * visibility or completeness of a page changes.
 *
 * One situation where it can be necessary to call this
 * function is when changing a value on the current page
 * affects the future page flow of the assistant.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_update_buttons_state (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  update_buttons_state (assistant);
}

/**
 * gtk_assistant_commit:
 * @assistant: a `GtkAssistant`
 *
 * Erases the visited page history.
 *
 * GTK will then hide the back button on the current page,
 * and removes the cancel button from subsequent pages.
 *
 * Use this when the information provided up to the current
 * page is hereafter deemed permanent and cannot be modified
 * or undone. For example, showing a progress page to track
 * a long-running, unreversible operation after the user has
 * clicked apply on a confirmation page.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_assistant_commit (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  g_slist_free (assistant->visited_pages);
  assistant->visited_pages = NULL;

  assistant->committed = TRUE;

  update_buttons_state (assistant);
}

/* buildable implementation */

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_assistant_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_assistant_buildable_custom_tag_start;
  iface->custom_finished = gtk_assistant_buildable_custom_finished;
  iface->add_child = gtk_assistant_buildable_add_child;
}

static void
gtk_assistant_buildable_add_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   GObject      *child,
                                   const char   *type)
{
  if (GTK_IS_ASSISTANT_PAGE (child))
    gtk_assistant_add_page (GTK_ASSISTANT (buildable), GTK_ASSISTANT_PAGE (child), -1);
  else if (type && g_str_equal (type, "titlebar"))
    {
      GtkAssistant *assistant = GTK_ASSISTANT (buildable);
      assistant->headerbar = GTK_WIDGET (child);
      gtk_window_set_titlebar (GTK_WINDOW (buildable), assistant->headerbar);
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

gboolean
gtk_assistant_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const char    *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer      *data)
{
  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, data);
}

static void
gtk_assistant_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const char   *tagname,
                                         gpointer      user_data)
{
  parent_buildable_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);
}

/**
 * gtk_assistant_get_page:
 * @assistant: a `GtkAssistant`
 * @child: a child of @assistant
 *
 * Returns the `GtkAssistantPage` object for @child.
 *
 * Returns: (transfer none): the `GtkAssistantPage` for @child
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkAssistantPage *
gtk_assistant_get_page (GtkAssistant *assistant,
                        GtkWidget    *child)
{
  GList *page_info = find_page (assistant, child);
  return (GtkAssistantPage *) (page_info ? page_info->data : NULL);
}

/**
 * gtk_assistant_page_get_child:
 * @page: a `GtkAssistantPage`
 *
 * Returns the child to which @page belongs.
 *
 * Returns: (transfer none): the child to which @page belongs
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget *
gtk_assistant_page_get_child (GtkAssistantPage *page)
{
  return page->page;
}

static GType
gtk_assistant_pages_get_item_type (GListModel *model)
{
  return GTK_TYPE_ASSISTANT_PAGE;
}

static guint
gtk_assistant_pages_get_n_items (GListModel *model)
{
  GtkAssistantPages *pages = GTK_ASSISTANT_PAGES (model);

  return g_list_length (pages->assistant->pages);
}

static gpointer
gtk_assistant_pages_get_item (GListModel *model,
                              guint       position)
{
  GtkAssistantPages *pages = GTK_ASSISTANT_PAGES (model);
  GtkAssistantPage *page;

  page = g_list_nth_data (pages->assistant->pages, position);

  return g_object_ref (page);
}

static void
gtk_assistant_pages_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_assistant_pages_get_item_type;
  iface->get_n_items = gtk_assistant_pages_get_n_items;
  iface->get_item = gtk_assistant_pages_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkAssistantPages, gtk_assistant_pages, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_assistant_pages_list_model_init))

static void
gtk_assistant_pages_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkAssistantPages *self = GTK_ASSISTANT_PAGES (object);

  switch (prop_id)
    {
    case PAGES_PROP_ITEM_TYPE:
      g_value_set_gtype (value, GTK_TYPE_ASSISTANT_PAGE);
      break;

    case PAGES_PROP_N_ITEMS:
      g_value_set_uint (value, gtk_assistant_pages_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_assistant_pages_init (GtkAssistantPages *pages)
{
}

static void
gtk_assistant_pages_class_init (GtkAssistantPagesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_assistant_pages_get_property;

  pages_properties[PAGES_PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        GTK_TYPE_ASSISTANT_PAGE,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  pages_properties[PAGES_PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PAGES_N_PROPS, pages_properties);
}

static GtkAssistantPages *
gtk_assistant_pages_new (GtkAssistant *assistant)
{
  GtkAssistantPages *pages;

  pages = g_object_new (GTK_TYPE_ASSISTANT_PAGES, NULL);
  pages->assistant = assistant;

  return pages;
}

/**
 * gtk_assistant_get_pages:
 * @assistant: a `GtkAssistant`
 * 
 * Gets a list model of the assistant pages.
 *
 * Returns: (transfer full) (attributes element-type=GtkAssistantPage): A list model of the pages.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GListModel *
gtk_assistant_get_pages (GtkAssistant *assistant)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);

  if (assistant->model)
    return g_object_ref (assistant->model);

  assistant->model = G_LIST_MODEL (gtk_assistant_pages_new (assistant));

  g_object_add_weak_pointer (G_OBJECT (assistant->model), (gpointer *)&assistant->model);

  return assistant->model;
}
