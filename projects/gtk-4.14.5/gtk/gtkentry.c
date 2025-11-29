/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2004-2006 Christian Hammond
 * Copyright (C) 2008 Cody Russell
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "gtkentryprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkadjustment.h"
#include "deprecated/gtkcelleditable.h"
#include "gtkdebug.h"
#include "gtkeditable.h"
#include "gtkemojichooser.h"
#include "gtkemojicompletion.h"
#include "gtkentrybuffer.h"
#include "gtkgesturedrag.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkpangoprivate.h"
#include "gtkprivate.h"
#include "gtkprogressbar.h"
#include "gtksnapshot.h"
#include "gtktextprivate.h"
#include "gtktextutilprivate.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkgestureclick.h"
#include "gtkdragsourceprivate.h"
#include "gtkdragicon.h"
#include "gtkwidgetpaintable.h"

#include <cairo-gobject.h>
#include <string.h>

/**
 * GtkEntry:
 *
 * `GtkEntry` is a single line text entry widget.
 *
 * ![An example GtkEntry](entry.png)
 *
 * A fairly large set of key bindings are supported by default. If the
 * entered text is longer than the allocation of the widget, the widget
 * will scroll so that the cursor position is visible.
 *
 * When using an entry for passwords and other sensitive information, it
 * can be put into “password mode” using [method@Gtk.Entry.set_visibility].
 * In this mode, entered text is displayed using a “invisible” character.
 * By default, GTK picks the best invisible character that is available
 * in the current font, but it can be changed with
 * [method@Gtk.Entry.set_invisible_char].
 *
 * `GtkEntry` has the ability to display progress or activity
 * information behind the text. To make an entry display such information,
 * use [method@Gtk.Entry.set_progress_fraction] or
 * [method@Gtk.Entry.set_progress_pulse_step].
 *
 * Additionally, `GtkEntry` can show icons at either side of the entry.
 * These icons can be activatable by clicking, can be set up as drag source
 * and can have tooltips. To add an icon, use
 * [method@Gtk.Entry.set_icon_from_gicon] or one of the various other functions
 * that set an icon from an icon name or a paintable. To trigger an action when
 * the user clicks an icon, connect to the [signal@Gtk.Entry::icon-press] signal.
 * To allow DND operations from an icon, use
 * [method@Gtk.Entry.set_icon_drag_source]. To set a tooltip on an icon, use
 * [method@Gtk.Entry.set_icon_tooltip_text] or the corresponding function
 * for markup.
 *
 * Note that functionality or information that is only available by clicking
 * on an icon in an entry may not be accessible at all to users which are not
 * able to use a mouse or other pointing device. It is therefore recommended
 * that any such functionality should also be available by other means, e.g.
 * via the context menu of the entry.
 *
 * # CSS nodes
 *
 * ```
 * entry[.flat][.warning][.error]
 * ├── text[.readonly]
 * ├── image.left
 * ├── image.right
 * ╰── [progress[.pulse]]
 * ```
 *
 * `GtkEntry` has a main node with the name entry. Depending on the properties
 * of the entry, the style classes .read-only and .flat may appear. The style
 * classes .warning and .error may also be used with entries.
 *
 * When the entry shows icons, it adds subnodes with the name image and the
 * style class .left or .right, depending on where the icon appears.
 *
 * When the entry shows progress, it adds a subnode with the name progress.
 * The node has the style class .pulse when the shown progress is pulsing.
 *
 * For all the subnodes added to the text node in various situations,
 * see [class@Gtk.Text].
 *
 * # GtkEntry as GtkBuildable
 *
 * The `GtkEntry` implementation of the `GtkBuildable` interface supports a
 * custom `<attributes>` element, which supports any number of `<attribute>`
 * elements. The `<attribute>` element has attributes named “name“, “value“,
 * “start“ and “end“ and allows you to specify `PangoAttribute` values for
 * this label.
 *
 * An example of a UI definition fragment specifying Pango attributes:
 * ```xml
 * <object class="GtkEntry">
 *   <attributes>
 *     <attribute name="weight" value="PANGO_WEIGHT_BOLD"/>
 *     <attribute name="background" value="red" start="5" end="10"/>
 *   </attributes>
 * </object>
 * ```
 *
 * The start and end attributes specify the range of characters to which the
 * Pango attribute applies. If start and end are not specified, the attribute
 * is applied to the whole text. Note that specifying ranges does not make much
 * sense with translatable attributes. Use markup embedded in the translatable
 * content instead.
 *
 * # Accessibility
 *
 * `GtkEntry` uses the %GTK_ACCESSIBLE_ROLE_TEXT_BOX role.
 */

#define MAX_ICONS 2

#define IS_VALID_ICON_POSITION(pos)               \
  ((pos) == GTK_ENTRY_ICON_PRIMARY ||                   \
   (pos) == GTK_ENTRY_ICON_SECONDARY)

static GQuark          quark_entry_completion = 0;

typedef struct _EntryIconInfo EntryIconInfo;

typedef struct _GtkEntryPrivate       GtkEntryPrivate;
struct _GtkEntryPrivate
{
  EntryIconInfo *icons[MAX_ICONS];

  GtkWidget     *text;
  GtkWidget     *progress_widget;

  guint         show_emoji_icon         : 1;
  guint         editing_canceled        : 1; /* Only used by GtkCellRendererText */
};

struct _EntryIconInfo
{
  GtkWidget *widget;
  char *tooltip;
  guint nonactivatable : 1;
  guint in_drag        : 1;

  GdkDragAction actions;
  GdkContentProvider *content;
};

enum {
  ACTIVATE,
  ICON_PRESS,
  ICON_RELEASE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_HAS_FRAME,
  PROP_INVISIBLE_CHAR,
  PROP_ACTIVATES_DEFAULT,
  PROP_SCROLL_OFFSET,
  PROP_TRUNCATE_MULTILINE,
  PROP_OVERWRITE_MODE,
  PROP_TEXT_LENGTH,
  PROP_INVISIBLE_CHAR_SET,
  PROP_PROGRESS_FRACTION,
  PROP_PROGRESS_PULSE_STEP,
  PROP_PAINTABLE_PRIMARY,
  PROP_PAINTABLE_SECONDARY,
  PROP_ICON_NAME_PRIMARY,
  PROP_ICON_NAME_SECONDARY,
  PROP_GICON_PRIMARY,
  PROP_GICON_SECONDARY,
  PROP_STORAGE_TYPE_PRIMARY,
  PROP_STORAGE_TYPE_SECONDARY,
  PROP_ACTIVATABLE_PRIMARY,
  PROP_ACTIVATABLE_SECONDARY,
  PROP_SENSITIVE_PRIMARY,
  PROP_SENSITIVE_SECONDARY,
  PROP_TOOLTIP_TEXT_PRIMARY,
  PROP_TOOLTIP_TEXT_SECONDARY,
  PROP_TOOLTIP_MARKUP_PRIMARY,
  PROP_TOOLTIP_MARKUP_SECONDARY,
  PROP_IM_MODULE,
  PROP_PLACEHOLDER_TEXT,
  PROP_COMPLETION,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_ATTRIBUTES,
  PROP_TABS,
  PROP_EXTRA_MENU,
  PROP_SHOW_EMOJI_ICON,
  PROP_ENABLE_EMOJI_COMPLETION,
  PROP_EDITING_CANCELED,
  NUM_PROPERTIES = PROP_EDITING_CANCELED,
};

static GParamSpec *entry_props[NUM_PROPERTIES] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

typedef enum
{
  DISPLAY_NORMAL,       /* The entry text is being shown */
  DISPLAY_INVISIBLE,    /* In invisible mode, text replaced by (eg) bullets */
  DISPLAY_BLANK         /* In invisible mode, nothing shown at all */
} DisplayMode;

/* GObject methods
 */
static void   gtk_entry_editable_init        (GtkEditableInterface *iface);
static void   gtk_entry_cell_editable_init   (GtkCellEditableIface *iface);
static void   gtk_entry_set_property         (GObject          *object,
                                              guint             prop_id,
                                              const GValue     *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_get_property         (GObject          *object,
                                              guint             prop_id,
                                              GValue           *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_finalize             (GObject          *object);
static void   gtk_entry_dispose              (GObject          *object);

/* GtkWidget methods
 */
static void   gtk_entry_size_allocate        (GtkWidget        *widget,
                                              int               width,
                                              int               height,
                                                int               baseline);
static void   gtk_entry_snapshot             (GtkWidget        *widget,
                                              GtkSnapshot      *snapshot);
static gboolean gtk_entry_query_tooltip      (GtkWidget        *widget,
                                              int               x,
                                              int               y,
                                              gboolean          keyboard_tip,
                                              GtkTooltip       *tooltip);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);


/* GtkCellEditable method implementations
 */
static void gtk_entry_start_editing (GtkCellEditable *cell_editable,
				     GdkEvent        *event);

/* Default signal handlers
 */
static GtkEntryBuffer *get_buffer                      (GtkEntry       *entry);
static void         set_show_emoji_icon                (GtkEntry       *entry,
                                                        gboolean        value);

static void     gtk_entry_measure (GtkWidget           *widget,
                                   GtkOrientation       orientation,
                                   int                  for_size,
                                   int                 *minimum,
                                   int                 *natural,
                                   int                 *minimum_baseline,
                                   int                 *natural_baseline);

static GtkBuildableIface *buildable_parent_iface = NULL;

static void     gtk_entry_buildable_interface_init (GtkBuildableIface *iface);
static void     gtk_entry_accessible_interface_init (GtkAccessibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkEntry, gtk_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                gtk_entry_accessible_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_entry_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_entry_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_entry_cell_editable_init))

/* Implement the GtkAccessible interface, in order to obtain focus
 * state from the GtkText widget that we are wrapping. The GtkText
 * widget is ignored for accessibility purposes (it has role NONE),
 * and any a11y text functionality is implemented for GtkEntry and
 * similar wrappers (GtkPasswordEntry, GtkSpinButton, etc).
 */
static gboolean
gtk_entry_accessible_get_platform_state (GtkAccessible              *self,
                                         GtkAccessiblePlatformState  state)
{
  return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (self), state);
}

static void
gtk_entry_accessible_interface_init (GtkAccessibleInterface *iface)
{
  GtkAccessibleInterface *parent_iface = g_type_interface_peek_parent (iface);
  iface->get_at_context = parent_iface->get_at_context;
  iface->get_platform_state = gtk_entry_accessible_get_platform_state;
}

static const GtkBuildableParser pango_parser =
{
  gtk_pango_attribute_start_element,
};

static gboolean
gtk_entry_buildable_custom_tag_start (GtkBuildable       *buildable,
                                      GtkBuilder         *builder,
                                      GObject            *child,
                                      const char         *tagname,
                                      GtkBuildableParser *parser,
                                      gpointer           *data)
{
  if (buildable_parent_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  if (strcmp (tagname, "attributes") == 0)
    {
      GtkPangoAttributeParserData *parser_data;

      parser_data = g_new0 (GtkPangoAttributeParserData, 1);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = (GObject *) g_object_ref (buildable);
      *parser = pango_parser;
      *data = parser_data;
      return TRUE;
    }
  return FALSE;
}

static void
gtk_entry_buildable_custom_finished (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const char   *tagname,
                                     gpointer      user_data)
{
  GtkPangoAttributeParserData *data = user_data;

  buildable_parent_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);

  if (strcmp (tagname, "attributes") == 0)
    {
      if (data->attrs)
        {
          gtk_entry_set_attributes (GTK_ENTRY (buildable), data->attrs);
          pango_attr_list_unref (data->attrs);
        }

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_free (data);
    }
}

static void
gtk_entry_buildable_interface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_entry_buildable_custom_tag_start;
  iface->custom_finished = gtk_entry_buildable_custom_finished;
}

static gboolean
gtk_entry_grab_focus (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return gtk_widget_grab_focus (priv->text);
}

static gboolean
gtk_entry_mnemonic_activate (GtkWidget *widget,
                             gboolean   group_cycling)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->text);

  return TRUE;
}

static void
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_entry_dispose;
  gobject_class->finalize = gtk_entry_finalize;
  gobject_class->set_property = gtk_entry_set_property;
  gobject_class->get_property = gtk_entry_get_property;

  widget_class->measure = gtk_entry_measure;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->snapshot = gtk_entry_snapshot;
  widget_class->query_tooltip = gtk_entry_query_tooltip;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->grab_focus = gtk_entry_grab_focus;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->mnemonic_activate = gtk_entry_mnemonic_activate;

  quark_entry_completion = g_quark_from_static_string ("gtk-entry-completion-key");

  /**
   * GtkEntry:buffer: (attributes org.gtk.Property.get=gtk_entry_get_buffer org.gtk.Property.set=gtk_entry_set_buffer)
   *
   * The buffer object which actually stores the text.
   */
  entry_props[PROP_BUFFER] =
      g_param_spec_object ("buffer", NULL, NULL,
                           GTK_TYPE_ENTRY_BUFFER,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:max-length: (attributes org.gtk.Property.get=gtk_entry_get_max_length org.gtk.Property.set=gtk_entry_set_max_length)
   *
   * Maximum number of characters for this entry.
   */
  entry_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length", NULL, NULL,
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:visibility: (attributes org.gtk.Property.get=gtk_entry_get_visibility org.gtk.Property.set=gtk_entry_set_visibility)
   *
   * Whether the entry should show the “invisible char” instead of the
   * actual text (“password mode”).
   */
  entry_props[PROP_VISIBILITY] =
      g_param_spec_boolean ("visibility", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:has-frame: (attributes org.gtk.Property.get=gtk_entry_get_has_frame org.gtk.Property.set=gtk_entry_set_has_frame)
   *
   * Whether the entry should draw a frame.
   */
  entry_props[PROP_HAS_FRAME] =
      g_param_spec_boolean ("has-frame", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:invisible-char: (attributes org.gtk.Property.get=gtk_entry_get_invisible_char org.gtk.Property.set=gtk_entry_set_invisible_char)
   *
   * The character to use when masking entry contents (“password mode”).
   */
  entry_props[PROP_INVISIBLE_CHAR] =
      g_param_spec_unichar ("invisible-char", NULL, NULL,
                            '*',
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:activates-default: (attributes org.gtk.Property.get=gtk_entry_get_activates_default org.gtk.Property.set=gtk_entry_set_activates_default)
   *
   * Whether to activate the default widget when Enter is pressed.
   */
  entry_props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:scroll-offset:
   *
   * Number of pixels of the entry scrolled off the screen to the left.
   */
  entry_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset", NULL, NULL,
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:truncate-multiline:
   *
   * When %TRUE, pasted multi-line text is truncated to the first line.
   */
  entry_props[PROP_TRUNCATE_MULTILINE] =
      g_param_spec_boolean ("truncate-multiline", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:overwrite-mode: (attributes org.gtk.Property.get=gtk_entry_get_overwrite_mode org.gtk.Property.set=gtk_entry_set_overwrite_mode)
   *
   * If text is overwritten when typing in the `GtkEntry`.
   */
  entry_props[PROP_OVERWRITE_MODE] =
      g_param_spec_boolean ("overwrite-mode", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:text-length: (attributes org.gtk.Property.get=gtk_entry_get_text_length)
   *
   * The length of the text in the `GtkEntry`.
   */
  entry_props[PROP_TEXT_LENGTH] =
      g_param_spec_uint ("text-length", NULL, NULL,
                         0, G_MAXUINT16,
                         0,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:invisible-char-set:
   *
   * Whether the invisible char has been set for the `GtkEntry`.
   */
  entry_props[PROP_INVISIBLE_CHAR_SET] =
      g_param_spec_boolean ("invisible-char-set", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
   * GtkEntry:progress-fraction: (attributes org.gtk.Property.get=gtk_entry_get_progress_fraction org.gtk.Property.set=gtk_entry_set_progress_fraction)
   *
   * The current fraction of the task that's been completed.
   */
  entry_props[PROP_PROGRESS_FRACTION] =
      g_param_spec_double ("progress-fraction", NULL, NULL,
                           0.0, 1.0,
                           0.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:progress-pulse-step: (attributes org.gtk.Property.get=gtk_entry_get_progress_pulse_step org.gtk.Property.set=gtk_entry_set_progress_pulse_step)
   *
   * The fraction of total entry width to move the progress
   * bouncing block for each pulse.
   *
   * See [method@Gtk.Entry.progress_pulse].
   */
  entry_props[PROP_PROGRESS_PULSE_STEP] =
      g_param_spec_double ("progress-pulse-step", NULL, NULL,
                           0.0, 1.0,
                           0.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
  * GtkEntry:placeholder-text: (attributes org.gtk.Property.get=gtk_entry_get_placeholder_text org.gtk.Property.set=gtk_entry_set_placeholder_text)
  *
  * The text that will be displayed in the `GtkEntry` when it is empty
  * and unfocused.
  */
  entry_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

   /**
   * GtkEntry:primary-icon-paintable:
   *
   * A `GdkPaintable` to use as the primary icon for the entry.
   */
  entry_props[PROP_PAINTABLE_PRIMARY] =
      g_param_spec_object ("primary-icon-paintable", NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-paintable:
   *
   * A `GdkPaintable` to use as the secondary icon for the entry.
   */
  entry_props[PROP_PAINTABLE_SECONDARY] =
      g_param_spec_object ("secondary-icon-paintable", NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-name:
   *
   * The icon name to use for the primary icon for the entry.
   */
  entry_props[PROP_ICON_NAME_PRIMARY] =
      g_param_spec_string ("primary-icon-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-name:
   *
   * The icon name to use for the secondary icon for the entry.
   */
  entry_props[PROP_ICON_NAME_SECONDARY] =
      g_param_spec_string ("secondary-icon-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-gicon:
   *
   * The `GIcon` to use for the primary icon for the entry.
   */
  entry_props[PROP_GICON_PRIMARY] =
      g_param_spec_object ("primary-icon-gicon", NULL, NULL,
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-gicon:
   *
   * The `GIcon` to use for the secondary icon for the entry.
   */
  entry_props[PROP_GICON_SECONDARY] =
      g_param_spec_object ("secondary-icon-gicon", NULL, NULL,
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-storage-type:
   *
   * The representation which is used for the primary icon of the entry.
   */
  entry_props[PROP_STORAGE_TYPE_PRIMARY] =
      g_param_spec_enum ("primary-icon-storage-type", NULL, NULL,
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:secondary-icon-storage-type:
   *
   * The representation which is used for the secondary icon of the entry.
   */
  entry_props[PROP_STORAGE_TYPE_SECONDARY] =
      g_param_spec_enum ("secondary-icon-storage-type", NULL, NULL,
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkEntry:primary-icon-activatable:
   *
   * Whether the primary icon is activatable.
   *
   * GTK emits the [signal@Gtk.Entry::icon-press] and
   * [signal@Gtk.Entry::icon-release] signals only on sensitive,
   * activatable icons.
   *
   * Sensitive, but non-activatable icons can be used for purely
   * informational purposes.
   */
  entry_props[PROP_ACTIVATABLE_PRIMARY] =
      g_param_spec_boolean ("primary-icon-activatable", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-activatable:
   *
   * Whether the secondary icon is activatable.
   *
   * GTK emits the [signal@Gtk.Entry::icon-press] and
   * [signal@Gtk.Entry::icon-release] signals only on sensitive,
   * activatable icons.
   *
   * Sensitive, but non-activatable icons can be used for purely
   * informational purposes.
   */
  entry_props[PROP_ACTIVATABLE_SECONDARY] =
      g_param_spec_boolean ("secondary-icon-activatable", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-sensitive:
   *
   * Whether the primary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK does not emit the
   * [signal@Gtk.Entry::icon-press] and [signal@Gtk.Entry::icon-release]
   * signals and does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   */
  entry_props[PROP_SENSITIVE_PRIMARY] =
      g_param_spec_boolean ("primary-icon-sensitive", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-sensitive:
   *
   * Whether the secondary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK does not emit the
   * [signal@Gtk.Entry::icon-press[ and [signal@Gtk.Entry::icon-release]
   * signals and does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   */
  entry_props[PROP_SENSITIVE_SECONDARY] =
      g_param_spec_boolean ("secondary-icon-sensitive", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-tooltip-text:
   *
   * The contents of the tooltip on the primary icon.
   *
   * Also see [method@Gtk.Entry.set_icon_tooltip_text].
   */
  entry_props[PROP_TOOLTIP_TEXT_PRIMARY] =
      g_param_spec_string ("primary-icon-tooltip-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-tooltip-text:
   *
   * The contents of the tooltip on the secondary icon.
   *
   * Also see [method@Gtk.Entry.set_icon_tooltip_text].
   */
  entry_props[PROP_TOOLTIP_TEXT_SECONDARY] =
      g_param_spec_string ("secondary-icon-tooltip-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:primary-icon-tooltip-markup:
   *
   * The contents of the tooltip on the primary icon, with markup.
   *
   * Also see [method@Gtk.Entry.set_icon_tooltip_markup].
   */
  entry_props[PROP_TOOLTIP_MARKUP_PRIMARY] =
      g_param_spec_string ("primary-icon-tooltip-markup", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:secondary-icon-tooltip-markup:
   *
   * The contents of the tooltip on the secondary icon, with markup.
   *
   * Also see [method@Gtk.Entry.set_icon_tooltip_markup].
   */
  entry_props[PROP_TOOLTIP_MARKUP_SECONDARY] =
      g_param_spec_string ("secondary-icon-tooltip-markup", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:im-module:
   *
   * Which IM (input method) module should be used for this entry.
   *
   * See [class@Gtk.IMContext].
   *
   * Setting this to a non-%NULL value overrides the system-wide IM
   * module setting. See the GtkSettings [property@Gtk.Settings:gtk-im-module]
   * property.
   */
  entry_props[PROP_IM_MODULE] =
      g_param_spec_string ("im-module", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:completion: (attributes org.gtk.Property.get=gtk_entry_get_completion org.gtk.Property.set=gtk_entry_set_completion)
   *
   * The auxiliary completion object to use with the entry.
   *
   * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
   */
  entry_props[PROP_COMPLETION] =
      g_param_spec_object ("completion", NULL, NULL,
                           GTK_TYPE_ENTRY_COMPLETION,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);

  /**
   * GtkEntry:input-purpose: (attributes org.gtk.Property.get=gtk_entry_get_input_purpose org.gtk.Property.set=gtk_entry_set_input_purpose)
   *
   * The purpose of this text field.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   *
   * Note that setting the purpose to %GTK_INPUT_PURPOSE_PASSWORD or
   * %GTK_INPUT_PURPOSE_PIN is independent from setting
   * [property@Gtk.Entry:visibility].
   */
  entry_props[PROP_INPUT_PURPOSE] =
      g_param_spec_enum ("input-purpose", NULL, NULL,
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:input-hints: (attributes org.gtk.Property.get=gtk_entry_get_input_hints org.gtk.Property.set=gtk_entry_set_input_hints)
   *
   * Additional hints that allow input methods to fine-tune their behavior.
   *
   * Also see [property@Gtk.Entry:input-purpose]
   */
  entry_props[PROP_INPUT_HINTS] =
      g_param_spec_flags ("input-hints", NULL, NULL,
                          GTK_TYPE_INPUT_HINTS,
                          GTK_INPUT_HINT_NONE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:attributes: (attributes org.gtk.Property.get=gtk_entry_get_attributes org.gtk.Property.set=gtk_entry_set_attributes)
   *
   * A list of Pango attributes to apply to the text of the entry.
   *
   * This is mainly useful to change the size or weight of the text.
   *
   * The `PangoAttribute`'s @start_index and @end_index must refer to the
   * [class@Gtk.EntryBuffer] text, i.e. without the preedit string.
   */
  entry_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes", NULL, NULL,
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry::tabs: (attributes org.gtk.Property.get=gtk_entry_get_tabs org.gtk.Property.set=gtk_entry_set_tabs)
   *
   * A list of tabstops to apply to the text of the entry.
   */
  entry_props[PROP_TABS] =
      g_param_spec_boxed ("tabs", NULL, NULL,
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry::show-emoji-icon: 
   *
   * Whether the entry will show an Emoji icon in the secondary icon position
   * to open the Emoji chooser.
   */
  entry_props[PROP_SHOW_EMOJI_ICON] =
      g_param_spec_boolean ("show-emoji-icon", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:extra-menu: (attributes org.gtk.Property.get=gtk_entry_get_extra_menu org.gtk.Property.set=gtk_entry_set_extra_menu)
   *
   * A menu model whose contents will be appended to the context menu.
   */
  entry_props[PROP_EXTRA_MENU] =
      g_param_spec_object ("extra-menu", NULL, NULL,
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:enable-emoji-completion:
   *
   * Whether to suggest Emoji replacements for :-delimited names
   * like `:heart:`.
   */
  entry_props[PROP_ENABLE_EMOJI_COMPLETION] =
      g_param_spec_boolean ("enable-emoji-completion", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, entry_props);
  g_object_class_override_property (gobject_class, PROP_EDITING_CANCELED, "editing-canceled");
  gtk_editable_install_properties (gobject_class, PROP_EDITING_CANCELED + 1);

  /**
   * GtkEntry::activate:
   * @self: The widget on which the signal is emitted
   *
   * Emitted when the entry is activated.
   *
   * The keybindings for this signal are all forms of the Enter key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkEntryClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEntry::icon-press:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   *
   * Emitted when an activatable icon is clicked.
   */
  signals[ICON_PRESS] =
    g_signal_new (I_("icon-press"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ENTRY_ICON_POSITION);

  /**
   * GtkEntry::icon-release:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   *
   * Emitted on the button release from a mouse click
   * over an activatable icon.
   */
  signals[ICON_RELEASE] =
    g_signal_new (I_("icon-release"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_ENTRY_ICON_POSITION);

  gtk_widget_class_set_css_name (widget_class, I_("entry"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}

static GtkEditable *
gtk_entry_get_delegate (GtkEditable *editable)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return GTK_EDITABLE (priv->text);
}

static void
gtk_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_entry_get_delegate;
}

static void
gtk_entry_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_entry_start_editing;
}

static void
gtk_entry_set_property (GObject         *object,
                        guint            prop_id,
                        const GValue    *value,
                        GParamSpec      *pspec)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    {
      if (prop_id == PROP_EDITING_CANCELED + 1 + GTK_EDITABLE_PROP_EDITABLE)
        {
          gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                          GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                          -1);
        }

      return;
    }

  switch (prop_id)
    {
    case PROP_BUFFER:
    case PROP_MAX_LENGTH:
    case PROP_VISIBILITY:
    case PROP_INVISIBLE_CHAR:
    case PROP_INVISIBLE_CHAR_SET:
    case PROP_ACTIVATES_DEFAULT:
    case PROP_TRUNCATE_MULTILINE:
    case PROP_OVERWRITE_MODE:
    case PROP_IM_MODULE:
    case PROP_INPUT_PURPOSE:
    case PROP_INPUT_HINTS:
    case PROP_ATTRIBUTES:
    case PROP_TABS:
    case PROP_ENABLE_EMOJI_COMPLETION:
      g_object_set_property (G_OBJECT (priv->text), pspec->name, value);
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_entry_set_placeholder_text (entry, g_value_get_string (value));
      break;

    case PROP_HAS_FRAME:
      gtk_entry_set_has_frame (entry, g_value_get_boolean (value));
      break;

    case PROP_PROGRESS_FRACTION:
      gtk_entry_set_progress_fraction (entry, g_value_get_double (value));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      gtk_entry_set_progress_pulse_step (entry, g_value_get_double (value));
      break;

    case PROP_PAINTABLE_PRIMARY:
      gtk_entry_set_icon_from_paintable (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_object (value));
      break;

    case PROP_PAINTABLE_SECONDARY:
      gtk_entry_set_icon_from_paintable (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_object (value));
      break;

    case PROP_ICON_NAME_PRIMARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_ICON_NAME_SECONDARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_GICON_PRIMARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_PRIMARY,
                                     g_value_get_object (value));
      break;

    case PROP_GICON_SECONDARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     g_value_get_object (value));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_PRIMARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_PRIMARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_PRIMARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_SECONDARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_SECONDARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_PRIMARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_EDITING_CANCELED:
      if (priv->editing_canceled != g_value_get_boolean (value))
        {
          priv->editing_canceled = g_value_get_boolean (value);
          g_object_notify (object, "editing-canceled");
        }
      break;

    case PROP_COMPLETION:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_entry_set_completion (entry, GTK_ENTRY_COMPLETION (g_value_get_object (value)));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_SHOW_EMOJI_ICON:
      set_show_emoji_icon (entry, g_value_get_boolean (value));
      break;

    case PROP_EXTRA_MENU:
      gtk_entry_set_extra_menu (entry, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_get_property (GObject         *object,
                        guint            prop_id,
                        GValue          *value,
                        GParamSpec      *pspec)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_BUFFER:
    case PROP_IM_MODULE:
    case PROP_MAX_LENGTH:
    case PROP_VISIBILITY:
    case PROP_INVISIBLE_CHAR:
    case PROP_INVISIBLE_CHAR_SET:
    case PROP_ACTIVATES_DEFAULT:
    case PROP_SCROLL_OFFSET:
    case PROP_TRUNCATE_MULTILINE:
    case PROP_OVERWRITE_MODE:
    case PROP_PLACEHOLDER_TEXT:
    case PROP_INPUT_PURPOSE:
    case PROP_INPUT_HINTS:
    case PROP_ATTRIBUTES:
    case PROP_TABS:
    case PROP_ENABLE_EMOJI_COMPLETION:
      g_object_get_property (G_OBJECT (priv->text), pspec->name, value);
      break;

    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_entry_get_has_frame (entry));
      break;

    case PROP_TEXT_LENGTH:
      g_value_set_uint (value, gtk_entry_get_text_length (entry));
      break;

    case PROP_PROGRESS_FRACTION:
      g_value_set_double (value, gtk_entry_get_progress_fraction (entry));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      g_value_set_double (value, gtk_entry_get_progress_pulse_step (entry));
      break;

    case PROP_PAINTABLE_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_paintable (entry,
                                                        GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_PAINTABLE_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_paintable (entry,
                                                        GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ICON_NAME_PRIMARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ICON_NAME_SECONDARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_GICON_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_GICON_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_STORAGE_TYPE_PRIMARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_STORAGE_TYPE_SECONDARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_SENSITIVE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_SENSITIVE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value,
                           priv->editing_canceled);
      break;

    case PROP_COMPLETION:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_object (value, G_OBJECT (gtk_entry_get_completion (entry)));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_SHOW_EMOJI_ICON:
      g_value_set_boolean (value, priv->show_emoji_icon);
      break;

    case PROP_EXTRA_MENU:
      g_value_set_object (value, gtk_entry_get_extra_menu (entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
activate_cb (GtkText *text, GtkEntry  *entry)
{
  g_signal_emit (entry, signals[ACTIVATE], 0);
}

static void
notify_cb (GObject    *object,
           GParamSpec *pspec,
           gpointer    data)
{
  gpointer iface;
  gpointer class;

  /* The editable interface properties are already forwarded by the editable delegate setup */
  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)), gtk_editable_get_type ());
  class = g_type_class_peek (GTK_TYPE_ENTRY);
  if (!g_object_interface_find_property (iface, pspec->name) &&
      g_object_class_find_property (class, pspec->name))
    g_object_notify (data, pspec->name);
}

static void
connect_text_signals (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_signal_connect (priv->text, "activate", G_CALLBACK (activate_cb), entry);
  g_signal_connect (priv->text, "notify", G_CALLBACK (notify_cb), entry);
}

static void
disconnect_text_signals (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_signal_handlers_disconnect_by_func (priv->text, activate_cb, entry);
  g_signal_handlers_disconnect_by_func (priv->text, notify_cb, entry);
}

static void
catchall_click_press (GtkGestureClick *gesture,
                      int              n_press,
                      double           x,
                      double           y,
                      gpointer         user_data)
{
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkGesture *catchall;

  priv->text = gtk_text_new ();
  gtk_widget_set_parent (priv->text, GTK_WIDGET (entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  connect_text_signals (entry);

  catchall = gtk_gesture_click_new ();
  g_signal_connect (catchall, "pressed",
                    G_CALLBACK (catchall_click_press), entry);
  gtk_widget_add_controller (GTK_WIDGET (entry),
                             GTK_EVENT_CONTROLLER (catchall));

  priv->editing_canceled = FALSE;

  gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);
}

static void
gtk_entry_dispose (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY, NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_entry_set_completion (entry, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (priv->text)
    {
      disconnect_text_signals (entry);
      gtk_editable_finish_delegate (GTK_EDITABLE (entry));
    }
  g_clear_pointer (&priv->text, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_entry_parent_class)->dispose (object);
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = NULL;
  int i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_info = priv->icons[i];
      if (icon_info == NULL)
        continue;

      g_clear_object (&icon_info->content);

      gtk_widget_unparent (icon_info->widget);

      g_free (icon_info);
    }

  g_clear_pointer (&priv->progress_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_entry_parent_class)->finalize (object);
}

static void
update_icon_style (GtkWidget            *widget,
                   GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  const char *sides[2] = { "left", "right" };

  if (icon_info == NULL)
    return;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    icon_pos = 1 - icon_pos;

  gtk_widget_add_css_class (icon_info->widget, sides[icon_pos]);
  gtk_widget_remove_css_class (icon_info->widget, sides[1 - icon_pos]);
}

static void
update_node_ordering (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  GtkEntryIconPosition first_icon_pos, second_icon_pos;

  if (priv->progress_widget)
    gtk_widget_insert_before (priv->progress_widget, GTK_WIDGET (entry), NULL);

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
    {
      first_icon_pos = GTK_ENTRY_ICON_SECONDARY;
      second_icon_pos = GTK_ENTRY_ICON_PRIMARY;
    }
  else
    {
      first_icon_pos = GTK_ENTRY_ICON_PRIMARY;
      second_icon_pos = GTK_ENTRY_ICON_SECONDARY;
    }

  icon_info = priv->icons[first_icon_pos];
  if (icon_info)
    gtk_widget_insert_after (icon_info->widget, GTK_WIDGET (entry), NULL);

  icon_info = priv->icons[second_icon_pos];
  if (icon_info)
    gtk_widget_insert_before (icon_info->widget, GTK_WIDGET (entry), NULL);
}

static GtkEntryIconPosition
get_icon_position_from_controller (GtkEntry           *entry,
                                   GtkEventController *controller)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);

  if (priv->icons[GTK_ENTRY_ICON_PRIMARY] &&
      priv->icons[GTK_ENTRY_ICON_PRIMARY]->widget == widget)
    return GTK_ENTRY_ICON_PRIMARY;
  else if (priv->icons[GTK_ENTRY_ICON_SECONDARY] &&
           priv->icons[GTK_ENTRY_ICON_SECONDARY]->widget == widget)
    return GTK_ENTRY_ICON_SECONDARY;

  g_assert_not_reached ();
  return -1;
}

static void
icon_pressed_cb (GtkGestureClick *gesture,
                 int                   n_press,
                 double                x,
                 double                y,
                 GtkEntry             *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (!icon_info->nonactivatable)
    g_signal_emit (entry, signals[ICON_PRESS], 0, pos);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
icon_released_cb (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  GtkEntry        *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (!icon_info->nonactivatable)
    g_signal_emit (entry, signals[ICON_RELEASE], 0, pos);
}

static void
icon_drag_update_cb (GtkGestureDrag *gesture,
                     double          offset_x,
                     double          offset_y,
                     GtkEntry       *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  GtkEntryIconPosition pos;
  EntryIconInfo *icon_info;

  pos = get_icon_position_from_controller (entry, GTK_EVENT_CONTROLLER (gesture));
  icon_info = priv->icons[pos];

  if (icon_info->content != NULL &&
      gtk_drag_check_threshold_double (icon_info->widget, 0, 0, offset_x, offset_y))
    {
      GdkPaintable *paintable;
      GdkSurface *surface;
      GdkDevice *device;
      GdkDrag *drag;
      double start_x, start_y;

      icon_info->in_drag = TRUE;

      surface = gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (entry)));
      device = gtk_gesture_get_device (GTK_GESTURE (gesture));

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

      drag = gdk_drag_begin (surface, device, icon_info->content, icon_info->actions, start_x, start_y);
      paintable = gtk_widget_paintable_new (icon_info->widget);
      gtk_drag_icon_set_from_paintable (drag, paintable, -2, -2);
      g_object_unref (paintable);

      g_object_unref (drag);
    }
}

static EntryIconInfo*
construct_icon_info (GtkWidget            *widget,
                     GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  GtkGesture *drag, *press;

  g_return_val_if_fail (priv->icons[icon_pos] == NULL, NULL);

  icon_info = g_new0 (EntryIconInfo, 1);
  priv->icons[icon_pos] = icon_info;

  icon_info->widget = gtk_image_new ();
  gtk_widget_set_cursor_from_name (icon_info->widget, "default");
  if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
    gtk_widget_insert_before (icon_info->widget, widget, priv->text);
  else
    gtk_widget_insert_after (icon_info->widget, widget, priv->text);

  update_icon_style (widget, icon_pos);
  update_node_ordering (entry);

  press = gtk_gesture_click_new ();
  g_signal_connect (press, "pressed", G_CALLBACK (icon_pressed_cb), entry);
  g_signal_connect (press, "released", G_CALLBACK (icon_released_cb), entry);
  gtk_widget_add_controller (icon_info->widget, GTK_EVENT_CONTROLLER (press));

  drag = gtk_gesture_drag_new ();
  g_signal_connect (drag, "drag-update",
                    G_CALLBACK (icon_drag_update_cb), entry);
  gtk_widget_add_controller (icon_info->widget, GTK_EVENT_CONTROLLER (drag));

  gtk_gesture_group (press, drag);

  return icon_info;
}

static void
gtk_entry_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int             *minimum,
                   int             *natural,
                   int             *minimum_baseline,
                   int             *natural_baseline)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  int text_min, text_nat;
  int i;

  gtk_widget_measure (priv->text,
                      orientation,
                      for_size,
                      &text_min, &text_nat,
                      minimum_baseline, natural_baseline);

  *minimum = text_min;
  *natural = text_nat;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      int icon_min, icon_nat;

      if (!icon_info)
        continue;

      gtk_widget_measure (icon_info->widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1, &icon_min, &icon_nat, NULL, NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum += icon_min;
          *natural += icon_nat;
        }
      else
        {
          *minimum = MAX (*minimum, icon_min);
          *natural = MAX (*natural, icon_nat);
        }
    }

  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    {
      int prog_min, prog_nat;

      gtk_widget_measure (priv->progress_widget,
                          orientation,
                          for_size,
                          &prog_min, &prog_nat,
                          NULL, NULL);

      *minimum = MAX (*minimum, prog_min);
      *natural = MAX (*natural, prog_nat);
    }

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (G_LIKELY (*minimum_baseline >= 0))
        *minimum_baseline += (*minimum - text_min) / 2;
      if (G_LIKELY (*natural_baseline >= 0))
        *natural_baseline += (*natural - text_nat) / 2;
    }
}

static void
gtk_entry_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  const gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  int i;
  GtkAllocation text_alloc;

  text_alloc.x = 0;
  text_alloc.y = 0;
  text_alloc.width = width;
  text_alloc.height = height;

  if (gtk_widget_get_valign (widget) != GTK_ALIGN_BASELINE_FILL &&
      gtk_widget_get_valign (widget) != GTK_ALIGN_BASELINE_CENTER)
    baseline = -1;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      GtkAllocation icon_alloc;
      int icon_width;

      if (!icon_info)
        continue;

      gtk_widget_measure (icon_info->widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &icon_width,
                          NULL, NULL);

      if ((is_rtl  && i == GTK_ENTRY_ICON_PRIMARY) ||
          (!is_rtl && i == GTK_ENTRY_ICON_SECONDARY))
        icon_alloc.x = width - icon_width;
      else
        icon_alloc.x = 0;
      icon_alloc.y = 0;
      icon_alloc.width = icon_width;
      icon_alloc.height = height;

      gtk_widget_size_allocate (icon_info->widget, &icon_alloc, baseline);

      text_alloc.width -= icon_width;

      if ((!is_rtl  && i == GTK_ENTRY_ICON_PRIMARY) ||
          (is_rtl && i == GTK_ENTRY_ICON_SECONDARY))
        text_alloc.x += icon_width;
    }

  gtk_widget_size_allocate (priv->text, &text_alloc, baseline);

  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    {
      GtkAllocation progress_alloc;
      int min, nat;

      gtk_widget_measure (priv->progress_widget,
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &min, &nat,
                          NULL, NULL);
      progress_alloc.x = 0;
      progress_alloc.y = height - nat;
      progress_alloc.width = width;
      progress_alloc.height = nat;

      gtk_widget_size_allocate (priv->progress_widget, &progress_alloc, -1);
    }

  /* Do this here instead of gtk_entry_size_allocate() so it works
   * inside spinbuttons, which don't chain up.
   */
  if (gtk_widget_get_realized (widget))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      GtkEntryCompletion *completion;

      completion = gtk_entry_get_completion (entry);
      if (completion)
        _gtk_entry_completion_resize_popup (completion);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
}

static void
gtk_entry_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  int i;

  /* Draw progress */
  if (priv->progress_widget && gtk_widget_get_visible (priv->progress_widget))
    gtk_widget_snapshot_child (widget, priv->progress_widget, snapshot);

  gtk_widget_snapshot_child (widget, priv->text, snapshot);

  /* Draw icons */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        gtk_widget_snapshot_child (widget, icon_info->widget, snapshot);
    }
}

/**
 * gtk_entry_grab_focus_without_selecting:
 * @entry: a `GtkEntry`
 *
 * Causes @entry to have keyboard focus.
 *
 * It behaves like [method@Gtk.Widget.grab_focus], except that it doesn't
 * select the contents of the entry. You only want to call this on some
 * special entries which the user usually doesn't want to replace all text
 * in, such as search-as-you-type entries.
 *
 * Returns: %TRUE if focus is now inside @self
 */
gboolean
gtk_entry_grab_focus_without_selecting (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_text_grab_focus_without_selecting (GTK_TEXT (priv->text));
}

static void
gtk_entry_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  update_icon_style (widget, GTK_ENTRY_ICON_PRIMARY);
  update_icon_style (widget, GTK_ENTRY_ICON_SECONDARY);

  update_node_ordering (entry);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->direction_changed (widget, previous_dir);
}

/* GtkCellEditable method implementations
 */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
gtk_cell_editable_entry_activated (GtkEntry *entry, gpointer data)
{
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
}

static gboolean
gtk_cell_editable_entry_key_pressed (GtkEventControllerKey *key,
                                     guint                  keyval,
                                     guint                  keycode,
                                     GdkModifierType        modifiers,
                                     GtkEntry              *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (keyval == GDK_KEY_Escape)
    {
      priv->editing_canceled = TRUE;
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return GDK_EVENT_STOP;
    }

  /* override focus */
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gtk_entry_start_editing (GtkCellEditable *cell_editable,
			 GdkEvent        *event)
{
  g_signal_connect (cell_editable, "activate",
		    G_CALLBACK (gtk_cell_editable_entry_activated), NULL);
  g_signal_connect (gtk_entry_get_key_controller (GTK_ENTRY (cell_editable)),
                    "key-pressed",
                    G_CALLBACK (gtk_cell_editable_entry_key_pressed),
                    cell_editable);
}

G_GNUC_END_IGNORE_DEPRECATIONS

/* Internal functions
 */

/**
 * gtk_entry_reset_im_context:
 * @entry: a `GtkEntry`
 *
 * Reset the input method context of the entry if needed.
 *
 * This can be necessary in the case where modifying the buffer
 * would confuse on-going input method behavior.
 */
void
gtk_entry_reset_im_context (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_reset_im_context (GTK_TEXT (priv->text));
}

static void
gtk_entry_clear_icon (GtkEntry             *entry,
                      GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GtkImageType storage_type;

  if (icon_info == NULL)
    return;

  storage_type = gtk_image_get_storage_type (GTK_IMAGE (icon_info->widget));

  if (storage_type == GTK_IMAGE_EMPTY)
    return;

  g_object_freeze_notify (G_OBJECT (entry));

  switch (storage_type)
    {
    case GTK_IMAGE_PAINTABLE:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_PAINTABLE_PRIMARY
                                            : PROP_PAINTABLE_SECONDARY]);
      break;

    case GTK_IMAGE_ICON_NAME:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_ICON_NAME_PRIMARY
                                            : PROP_ICON_NAME_SECONDARY]);
      break;

    case GTK_IMAGE_GICON:
      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_GICON_PRIMARY
                                            : PROP_GICON_SECONDARY]);
      break;

    case GTK_IMAGE_EMPTY:
    default:
      g_assert_not_reached ();
      break;
    }

  gtk_image_clear (GTK_IMAGE (icon_info->widget));

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                            ? PROP_STORAGE_TYPE_PRIMARY
                            : PROP_STORAGE_TYPE_SECONDARY]);

  g_object_thaw_notify (G_OBJECT (entry));
}

/* Public API
 */

/**
 * gtk_entry_new:
 *
 * Creates a new entry.
 *
 * Returns: a new `GtkEntry`.
 */
GtkWidget*
gtk_entry_new (void)
{
  return g_object_new (GTK_TYPE_ENTRY, NULL);
}

/**
 * gtk_entry_new_with_buffer:
 * @buffer: The buffer to use for the new `GtkEntry`.
 *
 * Creates a new entry with the specified text buffer.
 *
 * Returns: a new `GtkEntry`
 */
GtkWidget*
gtk_entry_new_with_buffer (GtkEntryBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_ENTRY_BUFFER (buffer), NULL);

  return g_object_new (GTK_TYPE_ENTRY, "buffer", buffer, NULL);
}

static GtkEntryBuffer*
get_buffer (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return gtk_text_get_buffer (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_get_buffer: (attributes org.gtk.Method.get_property=buffer)
 * @entry: a `GtkEntry`
 *
 * Get the `GtkEntryBuffer` object which holds the text for
 * this widget.
 *
 * Returns: (transfer none): A `GtkEntryBuffer` object.
 */
GtkEntryBuffer*
gtk_entry_get_buffer (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return get_buffer (entry);
}

/**
 * gtk_entry_set_buffer: (attributes org.gtk.Method.set_property=buffer)
 * @entry: a `GtkEntry`
 * @buffer: a `GtkEntryBuffer`
 *
 * Set the `GtkEntryBuffer` object which holds the text for
 * this widget.
 */
void
gtk_entry_set_buffer (GtkEntry       *entry,
                      GtkEntryBuffer *buffer)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_buffer (GTK_TEXT (priv->text), buffer);
}

/**
 * gtk_entry_set_visibility: (attributes org.gtk.Method.set_property=visibility)
 * @entry: a `GtkEntry`
 * @visible: %TRUE if the contents of the entry are displayed as plaintext
 *
 * Sets whether the contents of the entry are visible or not.
 *
 * When visibility is set to %FALSE, characters are displayed
 * as the invisible char, and will also appear that way when
 * the text in the entry widget is copied elsewhere.
 *
 * By default, GTK picks the best invisible character available
 * in the current font, but it can be changed with
 * [method@Gtk.Entry.set_invisible_char].
 *
 * Note that you probably want to set [property@Gtk.Entry:input-purpose]
 * to %GTK_INPUT_PURPOSE_PASSWORD or %GTK_INPUT_PURPOSE_PIN to
 * inform input methods about the purpose of this entry,
 * in addition to setting visibility to %FALSE.
 */
void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean  visible)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_visibility (GTK_TEXT (priv->text), visible);
}

/**
 * gtk_entry_get_visibility: (attributes org.gtk.Method.get_property=visibility)
 * @entry: a `GtkEntry`
 *
 * Retrieves whether the text in @entry is visible.
 *
 * See [method@Gtk.Entry.set_visibility].
 *
 * Returns: %TRUE if the text is currently visible
 */
gboolean
gtk_entry_get_visibility (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_text_get_visibility (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_invisible_char: (attributes org.gtk.Method.set_property=invisible-char)
 * @entry: a `GtkEntry`
 * @ch: a Unicode character
 *
 * Sets the character to use in place of the actual text
 * in “password mode”.
 *
 * See [method@Gtk.Entry.set_visibility] for how to enable
 * “password mode”.
 *
 * By default, GTK picks the best invisible char available in
 * the current font. If you set the invisible char to 0, then
 * the user will get no feedback at all; there will be no text
 * on the screen as they type.
 */
void
gtk_entry_set_invisible_char (GtkEntry *entry,
                              gunichar  ch)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_invisible_char (GTK_TEXT (priv->text), ch);
}

/**
 * gtk_entry_get_invisible_char: (attributes org.gtk.Method.get_property=invisible-char)
 * @entry: a `GtkEntry`
 *
 * Retrieves the character displayed in place of the actual text
 * in “password mode”.
 *
 * Returns: the current invisible char, or 0, if the entry does not
 *   show invisible text at all.
 */
gunichar
gtk_entry_get_invisible_char (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_text_get_invisible_char (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_unset_invisible_char:
 * @entry: a `GtkEntry`
 *
 * Unsets the invisible char, so that the default invisible char
 * is used again. See [method@Gtk.Entry.set_invisible_char].
 */
void
gtk_entry_unset_invisible_char (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_unset_invisible_char (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_overwrite_mode: (attributes org.gtk.Method.set_property=overwrite-mode)
 * @entry: a `GtkEntry`
 * @overwrite: new value
 *
 * Sets whether the text is overwritten when typing in the `GtkEntry`.
 */
void
gtk_entry_set_overwrite_mode (GtkEntry *entry,
                              gboolean  overwrite)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_overwrite_mode (GTK_TEXT (priv->text), overwrite);
}

/**
 * gtk_entry_get_overwrite_mode: (attributes org.gtk.Method.get_property=overwrite-mode)
 * @entry: a `GtkEntry`
 *
 * Gets whether the `GtkEntry` is in overwrite mode.
 *
 * Returns: whether the text is overwritten when typing.
 */
gboolean
gtk_entry_get_overwrite_mode (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_text_get_overwrite_mode (GTK_TEXT (priv->text));

}

/**
 * gtk_entry_set_max_length: (attributes org.gtk.Method.set_property=max-length)
 * @entry: a `GtkEntry`
 * @max: the maximum length of the entry, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 *
 * Sets the maximum allowed length of the contents of the widget.
 *
 * If the current contents are longer than the given length, then
 * they will be truncated to fit. The length is in characters.
 *
 * This is equivalent to getting @entry's `GtkEntryBuffer` and
 * calling [method@Gtk.EntryBuffer.set_max_length] on it.
 */
void
gtk_entry_set_max_length (GtkEntry     *entry,
                          int           max)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_max_length (GTK_TEXT (priv->text), max);
}

/**
 * gtk_entry_get_max_length: (attributes org.gtk.Method.get_property=max-length)
 * @entry: a `GtkEntry`
 *
 * Retrieves the maximum allowed length of the text in @entry.
 *
 * See [method@Gtk.Entry.set_max_length].
 *
 * Returns: the maximum allowed number of characters
 *   in `GtkEntry`, or 0 if there is no maximum.
 */
int
gtk_entry_get_max_length (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_text_get_max_length (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_get_text_length: (attributes org.gtk.Method.get_property=text-length)
 * @entry: a `GtkEntry`
 *
 * Retrieves the current length of the text in @entry.
 *
 * This is equivalent to getting @entry's `GtkEntryBuffer`
 * and calling [method@Gtk.EntryBuffer.get_length] on it.
 *
 * Returns: the current number of characters
 *   in `GtkEntry`, or 0 if there are none.
 */
guint16
gtk_entry_get_text_length (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return gtk_text_get_text_length (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_activates_default: (attributes org.gtk.Method.set_property=activates-default)
 * @entry: a `GtkEntry`
 * @setting: %TRUE to activate window’s default widget on Enter keypress
 *
 * Sets whether pressing Enter in the @entry will activate the default
 * widget for the window containing the entry.
 *
 * This usually means that the dialog containing the entry will be closed,
 * since the default widget is usually one of the dialog buttons.
 */
void
gtk_entry_set_activates_default (GtkEntry *entry,
                                 gboolean  setting)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_activates_default (GTK_TEXT (priv->text), setting);
}

/**
 * gtk_entry_get_activates_default: (attributes org.gtk.Method.get_property=activates-default)
 * @entry: a `GtkEntry`
 *
 * Retrieves the value set by gtk_entry_set_activates_default().
 *
 * Returns: %TRUE if the entry will activate the default widget
 */
gboolean
gtk_entry_get_activates_default (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_text_get_activates_default (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_has_frame: (attributes org.gtk.Method.set_property=has-frame)
 * @entry: a `GtkEntry`
 * @setting: new value
 *
 * Sets whether the entry has a beveled frame around it.
 */
void
gtk_entry_set_has_frame (GtkEntry *entry,
                         gboolean  setting)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  setting = (setting != FALSE);

  if (setting == gtk_entry_get_has_frame (entry))
    return;

  if (setting)
    gtk_widget_remove_css_class (GTK_WIDGET (entry), "flat");
  else
    gtk_widget_add_css_class (GTK_WIDGET (entry), "flat");

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_HAS_FRAME]);
}

/**
 * gtk_entry_get_has_frame: (attributes org.gtk.Method.get_property=has-frame)
 * @entry: a `GtkEntry`
 *
 * Gets the value set by gtk_entry_set_has_frame().
 *
 * Returns: whether the entry has a beveled frame
 */
gboolean
gtk_entry_get_has_frame (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return !gtk_widget_has_css_class (GTK_WIDGET (entry), "flat");
}

/**
 * gtk_entry_set_alignment:
 * @entry: a `GtkEntry`
 * @xalign: The horizontal alignment, from 0 (left) to 1 (right).
 *   Reversed for RTL layouts
 *
 * Sets the alignment for the contents of the entry.
 *
 * This controls the horizontal positioning of the contents when
 * the displayed text is shorter than the width of the entry.
 *
 * See also: [property@Gtk.Editable:xalign]
 */
void
gtk_entry_set_alignment (GtkEntry *entry,
                         float     xalign)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_alignment (GTK_EDITABLE (priv->text), xalign);
}

/**
 * gtk_entry_get_alignment:
 * @entry: a `GtkEntry`
 *
 * Gets the value set by gtk_entry_set_alignment().
 *
 * See also: [property@Gtk.Editable:xalign]
 *
 * Returns: the alignment
 */
float
gtk_entry_get_alignment (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  return gtk_editable_get_alignment (GTK_EDITABLE (priv->text));
}

/**
 * gtk_entry_set_icon_from_paintable:
 * @entry: a `GtkEntry`
 * @icon_pos: Icon position
 * @paintable: (nullable): A `GdkPaintable`
 *
 * Sets the icon shown in the specified position using a `GdkPaintable`.
 *
 * If @paintable is %NULL, no icon will be shown in the specified position.
 */
void
gtk_entry_set_icon_from_paintable (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 GdkPaintable           *paintable)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  g_object_freeze_notify (G_OBJECT (entry));

  if (paintable)
    {
      EntryIconInfo *icon_info;

      if ((icon_info = priv->icons[icon_pos]) == NULL)
        icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

      g_object_ref (paintable);

      gtk_image_set_from_paintable (GTK_IMAGE (icon_info->widget), paintable);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PAINTABLE_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PAINTABLE_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }

      g_object_unref (paintable);
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_icon_name:
 * @entry: A `GtkEntry`
 * @icon_pos: The position at which to set the icon
 * @icon_name: (nullable): An icon name
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.
 *
 * If @icon_name is %NULL, no icon will be shown in the
 * specified position.
 */
void
gtk_entry_set_icon_from_icon_name (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const char           *icon_name)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));


  if (icon_name != NULL)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (icon_info->widget), icon_name);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ICON_NAME_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_ICON_NAME_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_gicon:
 * @entry: A `GtkEntry`
 * @icon_pos: The position at which to set the icon
 * @icon: (nullable): The icon to set
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 *
 * If the icon isn’t known, a “broken image” icon will be
 * displayed instead.
 *
 * If @icon is %NULL, no icon will be shown in the
 * specified position.
 */
void
gtk_entry_set_icon_from_gicon (GtkEntry             *entry,
                               GtkEntryIconPosition  icon_pos,
                               GIcon                *icon)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  if (icon)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (icon_info->widget), icon);

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_GICON_PRIMARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_PRIMARY]);
        }
      else
        {
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_GICON_SECONDARY]);
          g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_STORAGE_TYPE_SECONDARY]);
        }
    }
  else
    gtk_entry_clear_icon (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_activatable:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 * @activatable: %TRUE if the icon should be activatable
 *
 * Sets whether the icon is activatable.
 */
void
gtk_entry_set_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                gboolean              activatable)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  activatable = activatable != FALSE;

  if (icon_info->nonactivatable != !activatable)
    {
      icon_info->nonactivatable = !activatable;

      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_ACTIVATABLE_PRIMARY
                                            : PROP_ACTIVATABLE_SECONDARY]);
    }
}

/**
 * gtk_entry_get_icon_activatable:
 * @entry: a `GtkEntry`
 * @icon_pos: Icon position
 *
 * Returns whether the icon is activatable.
 *
 * Returns: %TRUE if the icon is activatable.
 */
gboolean
gtk_entry_get_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), FALSE);

  icon_info = priv->icons[icon_pos];

  return (!icon_info || !icon_info->nonactivatable);
}

/**
 * gtk_entry_get_icon_paintable:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 *
 * Retrieves the `GdkPaintable` used for the icon.
 *
 * If no `GdkPaintable` was used for the icon, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): A `GdkPaintable`
 *   if no icon is set for this position or the icon set is not
 *   a `GdkPaintable`.
 */
GdkPaintable *
gtk_entry_get_icon_paintable (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_paintable (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_gicon:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 *
 * Retrieves the `GIcon` used for the icon.
 *
 * %NULL will be returned if there is no icon or if the icon was
 * set by some other method (e.g., by `GdkPaintable` or icon name).
 *
 * Returns: (transfer none) (nullable): A `GIcon`
 */
GIcon *
gtk_entry_get_icon_gicon (GtkEntry             *entry,
                          GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_gicon (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_name:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 *
 * Retrieves the icon name used for the icon.
 *
 * %NULL is returned if there is no icon or if the icon was set
 * by some other method (e.g., by `GdkPaintable` or gicon).
 *
 * Returns: (nullable): An icon name
 */
const char *
gtk_entry_get_icon_name (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return gtk_image_get_icon_name (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_set_icon_sensitive:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 * @sensitive: Specifies whether the icon should appear
 *   sensitive or insensitive
 *
 * Sets the sensitivity for the specified icon.
 */
void
gtk_entry_set_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos,
                              gboolean              sensitive)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (gtk_widget_get_sensitive (icon_info->widget) != sensitive)
    {
      gtk_widget_set_sensitive (icon_info->widget, sensitive);

      g_object_notify_by_pspec (G_OBJECT (entry),
                                entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                            ? PROP_SENSITIVE_PRIMARY
                                            : PROP_SENSITIVE_SECONDARY]);
    }
}

/**
 * gtk_entry_get_icon_sensitive:
 * @entry: a `GtkEntry`
 * @icon_pos: Icon position
 *
 * Returns whether the icon appears sensitive or insensitive.
 *
 * Returns: %TRUE if the icon is sensitive.
 */
gboolean
gtk_entry_get_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), TRUE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), TRUE);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return TRUE; /* Default of the property */

  return gtk_widget_get_sensitive (icon_info->widget);
}

/**
 * gtk_entry_get_icon_storage_type:
 * @entry: a `GtkEntry`
 * @icon_pos: Icon position
 *
 * Gets the type of representation being used by the icon
 * to store image data.
 *
 * If the icon has no image data, the return value will
 * be %GTK_IMAGE_EMPTY.
 *
 * Returns: image representation being used
 */
GtkImageType
gtk_entry_get_icon_storage_type (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_IMAGE_EMPTY);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), GTK_IMAGE_EMPTY);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return GTK_IMAGE_EMPTY;

  return gtk_image_get_storage_type (GTK_IMAGE (icon_info->widget));
}

/**
 * gtk_entry_get_icon_at_pos:
 * @entry: a `GtkEntry`
 * @x: the x coordinate of the position to find, relative to @entry
 * @y: the y coordinate of the position to find, relative to @entry
 *
 * Finds the icon at the given position and return its index.
 *
 * The position’s coordinates are relative to the @entry’s
 * top left corner. If @x, @y doesn’t lie inside an icon,
 * -1 is returned. This function is intended for use in a
 *  [signal@Gtk.Widget::query-tooltip] signal handler.
 *
 * Returns: the index of the icon at the given position, or -1
 */
int
gtk_entry_get_icon_at_pos (GtkEntry *entry,
                           int       x,
                           int       y)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  guint i;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      graphene_point_t p;

      if (icon_info == NULL)
        continue;

      if (!gtk_widget_compute_point (GTK_WIDGET (entry), icon_info->widget,
                                     &GRAPHENE_POINT_INIT (x, y), &p))
        continue;

      if (gtk_widget_contains (icon_info->widget, p.x, p.y))
        return i;
    }

  return -1;
}

/**
 * gtk_entry_set_icon_drag_source:
 * @entry: a `GtkEntry`
 * @icon_pos: icon position
 * @provider: a `GdkContentProvider`
 * @actions: a bitmask of the allowed drag actions
 *
 * Sets up the icon at the given position as drag source.
 *
 * This makes it so that GTK will start a drag
 * operation when the user clicks and drags the icon.
 */
void
gtk_entry_set_icon_drag_source (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                GdkContentProvider   *provider,
                                GdkDragAction         actions)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_set_object (&icon_info->content, provider);
  icon_info->actions = actions;
}

/**
 * gtk_entry_get_current_icon_drag_source:
 * @entry: a `GtkEntry`
 *
 * Returns the index of the icon which is the source of the
 * current  DND operation, or -1.
 *
 * Returns: index of the icon which is the source of the
 *   current DND operation, or -1.
 */
int
gtk_entry_get_current_icon_drag_source (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info = NULL;
  int i;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]))
        {
          if (icon_info->in_drag)
            return i;
        }
    }

  return -1;
}

/**
 * gtk_entry_get_icon_area:
 * @entry: A `GtkEntry`
 * @icon_pos: Icon position
 * @icon_area: (out): Return location for the icon’s area
 *
 * Gets the area where entry’s icon at @icon_pos is drawn.
 *
 * This function is useful when drawing something to the
 * entry in a draw callback.
 *
 * If the entry is not realized or has no icon at the given
 * position, @icon_area is filled with zeros. Otherwise,
 * @icon_area will be filled with the icon's allocation,
 * relative to @entry's allocation.
 */
void
gtk_entry_get_icon_area (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos,
                         GdkRectangle         *icon_area)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  graphene_rect_t r;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (icon_area != NULL);

  icon_info = priv->icons[icon_pos];

  if (icon_info &&
      gtk_widget_compute_bounds (icon_info->widget, GTK_WIDGET (entry), &r))
    {
      *icon_area = (GdkRectangle){
        floorf (r.origin.x),
        floorf (r.origin.y),
        ceilf (r.size.width),
        ceilf (r.size.height),
      };
    }
  else
    {
      icon_area->x = 0;
      icon_area->y = 0;
      icon_area->width = 0;
      icon_area->height = 0;
    }
}

static void
ensure_has_tooltip (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  const char *text = gtk_widget_get_tooltip_text (GTK_WIDGET (entry));
  gboolean has_tooltip = text != NULL;

  if (!has_tooltip)
    {
      int i;

      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];

          if (icon_info != NULL && icon_info->tooltip != NULL)
            {
              has_tooltip = TRUE;
              break;
            }
        }
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (entry), has_tooltip);
}

/**
 * gtk_entry_get_icon_tooltip_text:
 * @entry: a `GtkEntry`
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified
 * position in @entry.
 *
 * Returns: (nullable) (transfer full): the tooltip text
 */
char *
gtk_entry_get_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  char *text = NULL;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  if (icon_info->tooltip && 
      !pango_parse_markup (icon_info->tooltip, -1, 0, NULL, &text, NULL, NULL))
    g_assert (NULL == text); /* text should still be NULL in case of markup errors */

  return text;
}

/**
 * gtk_entry_set_icon_tooltip_text:
 * @entry: a `GtkEntry`
 * @icon_pos: the icon position
 * @tooltip: (nullable): the contents of the tooltip for the icon
 *
 * Sets @tooltip as the contents of the tooltip for the icon
 * at the specified position.
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also [method@Gtk.Widget.set_tooltip_text] and
 * [method@Gtk.Entry.set_icon_tooltip_markup].
 *
 * If you unset the widget tooltip via
 * [method@Gtk.Widget.set_tooltip_text] or
 * [method@Gtk.Widget.set_tooltip_markup], this sets
 * [property@Gtk.Widget:has-tooltip] to %FALSE, which suppresses
 * icon tooltips too. You can resolve this by then calling
 * [method@Gtk.Widget.set_has_tooltip] to set
 * [property@Gtk.Widget:has-tooltip] back to %TRUE, or
 * setting at least one non-empty tooltip on any icon
 * achieves the same result.
 */
void
gtk_entry_set_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 const char           *tooltip)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = tooltip ? g_markup_escape_text (tooltip, -1) : NULL;

  ensure_has_tooltip (entry);

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                        ? PROP_TOOLTIP_TEXT_PRIMARY
                                        : PROP_TOOLTIP_TEXT_SECONDARY]);
}

/**
 * gtk_entry_get_icon_tooltip_markup:
 * @entry: a `GtkEntry`
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified
 * position in @entry.
 *
 * Returns: (nullable) (transfer full): the tooltip text
 */
char *
gtk_entry_get_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  return g_strdup (icon_info->tooltip);
}

/**
 * gtk_entry_set_icon_tooltip_markup:
 * @entry: a `GtkEntry`
 * @icon_pos: the icon position
 * @tooltip: (nullable): the contents of the tooltip for the icon
 *
 * Sets @tooltip as the contents of the tooltip for the icon at
 * the specified position.
 *
 * @tooltip is assumed to be marked up with Pango Markup.
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also [method@Gtk.Widget.set_tooltip_markup] and
 * [method@Gtk.Entry.set_icon_tooltip_text].
 */
void
gtk_entry_set_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const char           *tooltip)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = g_strdup (tooltip);

  ensure_has_tooltip (entry);

  g_object_notify_by_pspec (G_OBJECT (entry),
                            entry_props[icon_pos == GTK_ENTRY_ICON_PRIMARY
                                        ? PROP_TOOLTIP_MARKUP_PRIMARY
                                        : PROP_TOOLTIP_MARKUP_SECONDARY]);
}

static gboolean
gtk_entry_query_tooltip (GtkWidget  *widget,
                         int         x,
                         int         y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;
  int icon_pos;

  if (!keyboard_tip)
    {
      icon_pos = gtk_entry_get_icon_at_pos (entry, x, y);
      if (icon_pos != -1)
        {
          if ((icon_info = priv->icons[icon_pos]) != NULL)
            {
              if (icon_info->tooltip)
                {
                  gtk_tooltip_set_markup (tooltip, icon_info->tooltip);
                  return TRUE;
                }

              return FALSE;
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_entry_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * gtk_entry_set_completion: (attributes org.gtk.Method.set_property=completion)
 * @entry: A `GtkEntry`
 * @completion: (nullable): The `GtkEntryCompletion`
 *
 * Sets @completion to be the auxiliary completion object
 * to use with @entry.
 *
 * All further configuration of the completion mechanism is
 * done on @completion using the `GtkEntryCompletion` API.
 * Completion is disabled if @completion is set to %NULL.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_set_completion (GtkEntry           *entry,
                          GtkEntryCompletion *completion)
{
  GtkEntryCompletion *old;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (!completion || GTK_IS_ENTRY_COMPLETION (completion));

  old = gtk_entry_get_completion (entry);

  if (old == completion)
    return;
  
  if (old)
    {
      _gtk_entry_completion_disconnect (old);
      g_object_unref (old);
    }

  if (!completion)
    {
      g_object_set_qdata (G_OBJECT (entry), quark_entry_completion, NULL);
      return;
    }

  /* hook into the entry */
  g_object_ref (completion);

  _gtk_entry_completion_connect (completion, entry);

  g_object_set_qdata (G_OBJECT (entry), quark_entry_completion, completion);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_COMPLETION]);
}

/**
 * gtk_entry_get_completion: (attributes org.gtk.Method.get_property=completion)
 * @entry: A `GtkEntry`
 *
 * Returns the auxiliary completion object currently
 * in use by @entry.
 *
 * Returns: (nullable) (transfer none): The auxiliary
 *   completion object currently in use by @entry
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
GtkEntryCompletion *
gtk_entry_get_completion (GtkEntry *entry)
{
  GtkEntryCompletion *completion;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  completion = GTK_ENTRY_COMPLETION (g_object_get_qdata (G_OBJECT (entry), quark_entry_completion));

  return completion;
}

G_GNUC_END_IGNORE_DEPRECATIONS

static void
gtk_entry_ensure_progress_widget (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->progress_widget)
    return;

  priv->progress_widget = g_object_new (GTK_TYPE_PROGRESS_BAR,
                                        "css-name", "progress",
                                        NULL);
  gtk_widget_set_can_target (priv->progress_widget, FALSE);

  gtk_widget_set_parent (priv->progress_widget, GTK_WIDGET (entry));

  update_node_ordering (entry);
}

/**
 * gtk_entry_set_progress_fraction: (attributes org.gtk.Method.set_property=progress-fraction)
 * @entry: a `GtkEntry`
 * @fraction: fraction of the task that’s been completed
 *
 * Causes the entry’s progress indicator to “fill in” the given
 * fraction of the bar.
 *
 * The fraction should be between 0.0 and 1.0, inclusive.
 */
void
gtk_entry_set_progress_fraction (GtkEntry *entry,
                                 double    fraction)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  double           old_fraction;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_entry_ensure_progress_widget (entry);
  old_fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (priv->progress_widget));
  fraction = CLAMP (fraction, 0.0, 1.0);

  if (fraction != old_fraction)
    {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_widget), fraction);
      gtk_widget_set_visible (priv->progress_widget, fraction > 0);

      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PROGRESS_FRACTION]);
    }
}

/**
 * gtk_entry_get_progress_fraction: (attributes org.gtk.Method.get_property=progress-fraction)
 * @entry: a `GtkEntry`
 *
 * Returns the current fraction of the task that’s been completed.
 *
 * See [method@Gtk.Entry.set_progress_fraction].
 *
 * Returns: a fraction from 0.0 to 1.0
 */
double
gtk_entry_get_progress_fraction (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  if (priv->progress_widget)
    return gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (priv->progress_widget));

  return 0.0;
}

/**
 * gtk_entry_set_progress_pulse_step: (attributes org.gtk.Method.set_property=progress-pulse-step)
 * @entry: a `GtkEntry`
 * @fraction: fraction between 0.0 and 1.0
 *
 * Sets the fraction of total entry width to move the progress
 * bouncing block for each pulse.
 *
 * Use [method@Gtk.Entry.progress_pulse] to pulse
 * the progress.
 */
void
gtk_entry_set_progress_pulse_step (GtkEntry *entry,
                                   double    fraction)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  fraction = CLAMP (fraction, 0.0, 1.0);
  gtk_entry_ensure_progress_widget (entry);


  if (fraction != gtk_progress_bar_get_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget)))
    {
      gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget), fraction);
      g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_PROGRESS_PULSE_STEP]);
    }
}

/**
 * gtk_entry_get_progress_pulse_step: (attributes org.gtk.Method.get_property=progress-pulse-step)
 * @entry: a `GtkEntry`
 *
 * Retrieves the pulse step set with
 * gtk_entry_set_progress_pulse_step().
 *
 * Returns: a fraction from 0.0 to 1.0
 */
double
gtk_entry_get_progress_pulse_step (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  if (priv->progress_widget)
    return gtk_progress_bar_get_pulse_step (GTK_PROGRESS_BAR (priv->progress_widget));

  return 0.0;
}

/**
 * gtk_entry_progress_pulse:
 * @entry: a `GtkEntry`
 *
 * Indicates that some progress is made, but you don’t
 * know how much.
 *
 * Causes the entry’s progress indicator to enter “activity
 * mode”, where a block bounces back and forth. Each call to
 * gtk_entry_progress_pulse() causes the block to move by a
 * little bit (the amount of movement per pulse is determined
 * by [method@Gtk.Entry.set_progress_pulse_step]).
 */
void
gtk_entry_progress_pulse (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (priv->progress_widget)
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress_widget));
}

/**
 * gtk_entry_set_placeholder_text: (attributes org.gtk.Method.set_property=placeholder-text)
 * @entry: a `GtkEntry`
 * @text: (nullable): a string to be displayed when @entry is empty and unfocused
 *
 * Sets text to be displayed in @entry when it is empty.
 *
 * This can be used to give a visual hint of the expected
 * contents of the `GtkEntry`.
 */
void
gtk_entry_set_placeholder_text (GtkEntry    *entry,
                                const char *text)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_placeholder_text (GTK_TEXT (priv->text), text);

  gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                  GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, text,
                                  -1);
}

/**
 * gtk_entry_get_placeholder_text: (attributes org.gtk.Method.get_property=placeholder-text)
 * @entry: a `GtkEntry`
 *
 * Retrieves the text that will be displayed when @entry
 * is empty and unfocused
 *
 * Returns: (nullable) (transfer none):a pointer to the
 *   placeholder text as a string. This string points to
 *   internally allocated storage in the widget and must
 *   not be freed, modified or stored. If no placeholder
 *   text has been set, %NULL will be returned.
 */
const char *
gtk_entry_get_placeholder_text (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_placeholder_text (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_input_purpose: (attributes org.gtk.Method.set_property=input-purpose)
 * @entry: a `GtkEntry`
 * @purpose: the purpose
 *
 * Sets the input purpose which can be used by input methods
 * to adjust their behavior.
 */
void
gtk_entry_set_input_purpose (GtkEntry        *entry,
                             GtkInputPurpose  purpose)

{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_input_purpose (GTK_TEXT (priv->text), purpose);
}

/**
 * gtk_entry_get_input_purpose: (attributes org.gtk.Method.get_property=input-purpose)
 * @entry: a `GtkEntry`
 *
 * Gets the input purpose of the `GtkEntry`.
 *
 * Returns: the input purpose
 */
GtkInputPurpose
gtk_entry_get_input_purpose (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_PURPOSE_FREE_FORM);

  return gtk_text_get_input_purpose (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_input_hints: (attributes org.gtk.Method.set_property=input-hints)
 * @entry: a `GtkEntry`
 * @hints: the hints
 *
 * Set additional hints which allow input methods to
 * fine-tune their behavior.
 */
void
gtk_entry_set_input_hints (GtkEntry      *entry,
                           GtkInputHints  hints)

{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_input_hints (GTK_TEXT (priv->text), hints);
}

/**
 * gtk_entry_get_input_hints: (attributes org.gtk.Method.get_property=input-hints)
 * @entry: a `GtkEntry`
 *
 * Gets the input hints of this `GtkEntry`.
 *
 * Returns: the input hints
 */
GtkInputHints
gtk_entry_get_input_hints (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_INPUT_HINT_NONE);

  return gtk_text_get_input_hints (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @entry: a `GtkEntry`
 * @attrs: a `PangoAttrList`
 *
 * Sets a `PangoAttrList`.
 *
 * The attributes in the list are applied to the entry text.
 *
 * Since the attributes will be applied to text that changes
 * as the user types, it makes most sense to use attributes
 * with unlimited extent.
 */
void
gtk_entry_set_attributes (GtkEntry      *entry,
                          PangoAttrList *attrs)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_attributes (GTK_TEXT (priv->text), attrs);
}

/**
 * gtk_entry_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @entry: a `GtkEntry`
 *
 * Gets the attribute list of the `GtkEntry`.
 *
 * See [method@Gtk.Entry.set_attributes].
 *
 * Returns: (transfer none) (nullable): the attribute list
 */
PangoAttrList *
gtk_entry_get_attributes (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_attributes (GTK_TEXT (priv->text));
}

/**
 * gtk_entry_set_tabs: (attributes org.gtk.Method.set_property=tabs)
 * @entry: a `GtkEntry`
 * @tabs: (nullable): a `PangoTabArray`
 *
 * Sets a `PangoTabArray`.
 *
 * The tabstops in the array are applied to the entry text.
 */

void
gtk_entry_set_tabs (GtkEntry      *entry,
                    PangoTabArray *tabs)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_tabs (GTK_TEXT (priv->text), tabs);
}

/**
 * gtk_entry_get_tabs: (attributes org.gtk.Method.get_property=tabs)
 * @entry: a `GtkEntry`
 *
 * Gets the tabstops of the `GtkEntry`.
 *
 * See [method@Gtk.Entry.set_tabs].
 *
 * Returns: (nullable) (transfer none): the tabstops
 */

PangoTabArray *
gtk_entry_get_tabs (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_tabs (GTK_TEXT (priv->text));
}

static void
pick_emoji (GtkEntry *entry,
            int       icon,
            GdkEvent *event,
            gpointer  data)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  if (icon == GTK_ENTRY_ICON_SECONDARY)
    g_signal_emit_by_name (priv->text, "insert-emoji");
}

static void
set_show_emoji_icon (GtkEntry *entry,
                     gboolean  value)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  if (priv->show_emoji_icon == value)
    return;

  priv->show_emoji_icon = value;

  if (priv->show_emoji_icon)
    {
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         "face-smile-symbolic");

      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_SECONDARY,
                                    TRUE);

      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      TRUE);

      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       _("Insert Emoji"));

      g_signal_connect (entry, "icon-press", G_CALLBACK (pick_emoji), NULL);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (entry, pick_emoji, NULL);

      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         NULL);

      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_SHOW_EMOJI_ICON]);
  gtk_widget_queue_resize (GTK_WIDGET (entry));
}

GtkEventController *
gtk_entry_get_key_controller (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return gtk_text_get_key_controller (GTK_TEXT (priv->text));
}

GtkText *
gtk_entry_get_text_widget (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  return GTK_TEXT (priv->text);
}

/**
 * gtk_entry_set_extra_menu: (attributes org.gtk.Method.set_property=extra-menu)
 * @entry: a `GtkEntry`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model to add when constructing
 * the context menu for @entry.
 */
void
gtk_entry_set_extra_menu (GtkEntry   *entry,
                          GMenuModel *model)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_text_set_extra_menu (GTK_TEXT (priv->text), model);

  g_object_notify_by_pspec (G_OBJECT (entry), entry_props[PROP_EXTRA_MENU]);
}

/**
 * gtk_entry_get_extra_menu: (attributes org.gtk.Method.get_property=extra-menu)
 * @entry: a `GtkEntry`
 *
 * Gets the menu model set with gtk_entry_set_extra_menu().
 *
 * Returns: (transfer none) (nullable): the menu model
 */
GMenuModel *
gtk_entry_get_extra_menu (GtkEntry *entry)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return gtk_text_get_extra_menu (GTK_TEXT (priv->text));
}

/*< private >
 * gtk_entry_activate_icon:
 * @entry: a `GtkEntry`
 * @pos: the icon position
 *
 * Activates an icon.
 *
 * This causes the [signal@Gtk.Entry::icon-press] and
 * [signal@Gtk.Entry::icon-release] signals to be emitted
 * on the @entry icon at the given @pos, if the icon is
 * set and activatable.
 *
 * Returns: %TRUE if the signal was emitted
 */
gboolean
gtk_entry_activate_icon (GtkEntry             *entry,
                         GtkEntryIconPosition  pos)
{
  GtkEntryPrivate *priv = gtk_entry_get_instance_private (entry);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  icon_info = priv->icons[pos];

  if (icon_info != NULL &&
      gtk_image_get_storage_type (GTK_IMAGE (icon_info->widget)) != GTK_IMAGE_EMPTY &&
      !icon_info->nonactivatable)
    {
      g_signal_emit (entry, signals[ICON_PRESS], 0, pos);
      g_signal_emit (entry, signals[ICON_RELEASE], 0, pos);
      return TRUE;
    }

  return FALSE;
}
