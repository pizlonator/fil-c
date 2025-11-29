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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtklabelprivate.h"

#include "gtkaccessibletextprivate.h"
#include "gtkbuildable.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkdragsourceprivate.h"
#include "gtkdragicon.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkfilelauncher.h"
#include "gtkgesturedrag.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkjoinedmenuprivate.h"
#include "gtkmarshalers.h"
#include "gtknative.h"
#include "gtknotebook.h"
#include "gtkpangoprivate.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrenderlayoutprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcuttrigger.h"
#include "gtksnapshot.h"
#include "gtktextutilprivate.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtkurilauncher.h"
#include "gtkwidgetprivate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>

/**
 * GtkLabel:
 *
 * The `GtkLabel` widget displays a small amount of text.
 *
 * As the name implies, most labels are used to label another widget
 * such as a [class@Button].
 *
 * ![An example GtkLabel](label.png)
 *
 * ## CSS nodes
 *
 * ```
 * label
 * ├── [selection]
 * ├── [link]
 * ┊
 * ╰── [link]
 * ```
 *
 * `GtkLabel` has a single CSS node with the name label. A wide variety
 * of style classes may be applied to labels, such as .title, .subtitle,
 * .dim-label, etc. In the `GtkShortcutsWindow`, labels are used with the
 * .keycap style class.
 *
 * If the label has a selection, it gets a subnode with name selection.
 *
 * If the label has links, there is one subnode per link. These subnodes
 * carry the link or visited state depending on whether they have been
 * visited. In this case, label node also gets a .link style class.
 *
 * ## GtkLabel as GtkBuildable
 *
 * The GtkLabel implementation of the GtkBuildable interface supports a
 * custom `<attributes>` element, which supports any number of `<attribute>`
 * elements. The `<attribute>` element has attributes named “name“, “value“,
 * “start“ and “end“ and allows you to specify [struct@Pango.Attribute]
 * values for this label.
 *
 * An example of a UI definition fragment specifying Pango attributes:
 *
 * ```xml
 * <object class="GtkLabel">
 *   <attributes>
 *     <attribute name="weight" value="PANGO_WEIGHT_BOLD"/>
 *     <attribute name="background" value="red" start="5" end="10"/>
 *   </attributes>
 * </object>
 * ```
 *
 * The start and end attributes specify the range of characters to which the
 * Pango attribute applies. If start and end are not specified, the attribute is
 * applied to the whole text. Note that specifying ranges does not make much
 * sense with translatable attributes. Use markup embedded in the translatable
 * content instead.
 *
 * ## Accessibility
 *
 * `GtkLabel` uses the %GTK_ACCESSIBLE_ROLE_LABEL role.
 *
 * ## Mnemonics
 *
 * Labels may contain “mnemonics”. Mnemonics are underlined characters in the
 * label, used for keyboard navigation. Mnemonics are created by providing a
 * string with an underscore before the mnemonic character, such as `"_File"`,
 * to the functions [ctor@Gtk.Label.new_with_mnemonic] or
 * [method@Gtk.Label.set_text_with_mnemonic].
 *
 * Mnemonics automatically activate any activatable widget the label is
 * inside, such as a [class@Gtk.Button]; if the label is not inside the
 * mnemonic’s target widget, you have to tell the label about the target
 * using [method@Gtk.Label.set_mnemonic_widget].
 *
 * Here’s a simple example where the label is inside a button:
 *
 * ```c
 * // Pressing Alt+H will activate this button
 * GtkWidget *button = gtk_button_new ();
 * GtkWidget *label = gtk_label_new_with_mnemonic ("_Hello");
 * gtk_button_set_child (GTK_BUTTON (button), label);
 * ```
 *
 * There’s a convenience function to create buttons with a mnemonic label
 * already inside:
 *
 * ```c
 * // Pressing Alt+H will activate this button
 * GtkWidget *button = gtk_button_new_with_mnemonic ("_Hello");
 * ```
 *
 * To create a mnemonic for a widget alongside the label, such as a
 * [class@Gtk.Entry], you have to point the label at the entry with
 * [method@Gtk.Label.set_mnemonic_widget]:
 *
 * ```c
 * // Pressing Alt+H will focus the entry
 * GtkWidget *entry = gtk_entry_new ();
 * GtkWidget *label = gtk_label_new_with_mnemonic ("_Hello");
 * gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
 * ```
 *
 * ## Markup (styled text)
 *
 * To make it easy to format text in a label (changing colors,
 * fonts, etc.), label text can be provided in a simple
 * markup format:
 *
 * Here’s how to create a label with a small font:
 * ```c
 * GtkWidget *label = gtk_label_new (NULL);
 * gtk_label_set_markup (GTK_LABEL (label), "<small>Small text</small>");
 * ```
 *
 * (See the Pango manual for complete documentation] of available
 * tags, [func@Pango.parse_markup])
 *
 * The markup passed to [method@Gtk.Label.set_markup] must be valid; for example,
 * literal `<`, `>` and `&` characters must be escaped as `&lt;`, `&gt;`, and `&amp;`.
 * If you pass text obtained from the user, file, or a network to
 * [method@Gtk.Label.set_markup], you’ll want to escape it with
 * [func@GLib.markup_escape_text] or [func@GLib.markup_printf_escaped].
 *
 * Markup strings are just a convenient way to set the [struct@Pango.AttrList]
 * on a label; [method@Gtk.Label.set_attributes] may be a simpler way to set
 * attributes in some cases. Be careful though; [struct@Pango.AttrList] tends
 * to cause internationalization problems, unless you’re applying attributes
 * to the entire string (i.e. unless you set the range of each attribute
 * to [0, %G_MAXINT)). The reason is that specifying the start_index and
 * end_index for a [struct@Pango.Attribute] requires knowledge of the exact
 * string being displayed, so translations will cause problems.
 *
 * ## Selectable labels
 *
 * Labels can be made selectable with [method@Gtk.Label.set_selectable].
 * Selectable labels allow the user to copy the label contents to
 * the clipboard. Only labels that contain useful-to-copy information—such
 * as error messages—should be made selectable.
 *
 * ## Text layout
 *
 * A label can contain any number of paragraphs, but will have
 * performance problems if it contains more than a small number.
 * Paragraphs are separated by newlines or other paragraph separators
 * understood by Pango.
 *
 * Labels can automatically wrap text if you call [method@Gtk.Label.set_wrap].
 *
 * [method@Gtk.Label.set_justify] sets how the lines in a label align
 * with one another. If you want to set how the label as a whole aligns
 * in its available space, see the [property@Gtk.Widget:halign] and
 * [property@Gtk.Widget:valign] properties.
 *
 * The [property@Gtk.Label:width-chars] and [property@Gtk.Label:max-width-chars]
 * properties can be used to control the size allocation of ellipsized or
 * wrapped labels. For ellipsizing labels, if either is specified (and less
 * than the actual text size), it is used as the minimum width, and the actual
 * text size is used as the natural width of the label. For wrapping labels,
 * width-chars is used as the minimum width, if specified, and max-width-chars
 * is used as the natural width. Even if max-width-chars specified, wrapping
 * labels will be rewrapped to use all of the available width.
 *
 * ## Links
 *
 * GTK supports markup for clickable hyperlinks in addition to regular Pango
 * markup. The markup for links is borrowed from HTML, using the `<a>` with
 * “href“, “title“ and “class“ attributes. GTK renders links similar to the
 * way they appear in web browsers, with colored, underlined text. The “title“
 * attribute is displayed as a tooltip on the link. The “class“ attribute is
 * used as style class on the CSS node for the link.
 *
 * An example of inline links looks like this:
 *
 * ```c
 * const char *text =
 * "Go to the "
 * "<a href=\"https://www.gtk.org\" title=\"&lt;i&gt;Our&lt;/i&gt; website\">"
 * "GTK website</a> for more...";
 * GtkWidget *label = gtk_label_new (NULL);
 * gtk_label_set_markup (GTK_LABEL (label), text);
 * ```
 *
 * It is possible to implement custom handling for links and their tooltips
 * with the [signal@Gtk.Label::activate-link] signal and the
 * [method@Gtk.Label.get_current_uri] function.
 */

typedef struct _GtkLabelClass         GtkLabelClass;
typedef struct _GtkLabelSelectionInfo GtkLabelSelectionInfo;

struct _GtkLabel
{
  GtkWidget parent_instance;

  GtkLabelSelectionInfo *select_info;
  GtkWidget *mnemonic_widget;
  GtkEventController *mnemonic_controller;

  PangoAttrList *attrs;
  PangoAttrList *markup_attrs;
  PangoLayout   *layout;
  PangoTabArray *tabs;

  GtkWidget *popup_menu;
  GMenuModel *extra_menu;

  char    *label;
  char    *text;

  float    xalign;
  float    yalign;

  guint    mnemonics_visible  : 1;
  guint    jtype              : 2;
  guint    wrap               : 1;
  guint    use_underline      : 1;
  guint    ellipsize          : 3;
  guint    use_markup         : 1;
  guint    wrap_mode          : 3;
  guint    natural_wrap_mode  : 3;
  guint    single_line_mode   : 1;
  guint    in_click           : 1;
  guint    track_links        : 1;

  guint    mnemonic_keyval;

  int      width_chars;
  int      max_width_chars;
  int      lines;
};

struct _GtkLabelClass
{
  GtkWidgetClass parent_class;

  void (* move_cursor)     (GtkLabel       *self,
                            GtkMovementStep step,
                            int             count,
                            gboolean        extend_selection);
  void (* copy_clipboard)  (GtkLabel       *self);

  gboolean (*activate_link) (GtkLabel       *self,
                             const char     *uri);
};

/* Notes about the handling of links:
 *
 * Links share the GtkLabelSelectionInfo struct with selectable labels.
 * There are some new fields for links. The links field contains the list
 * of GtkLabelLink structs that describe the links which are embedded in
 * the label. The active_link field points to the link under the mouse
 * pointer. For keyboard navigation, the “focus” link is determined by
 * finding the link which contains the selection_anchor position.
 * The link_clicked field is used with button press and release events
 * to ensure that pressing inside a link and releasing outside of it
 * does not activate the link.
 *
 * Links are rendered with the %GTK_STATE_FLAG_LINK/%GTK_STATE_FLAG_VISITED
 * state flags. When the mouse pointer is over a link, the pointer is changed
 * to indicate the link.
 *
 * Labels with links accept keyboard focus, and it is possible to move
 * the focus between the embedded links using Tab/Shift-Tab. The focus
 * is indicated by a focus rectangle that is drawn around the link text.
 * Pressing Enter activates the focused link, and there is a suitable
 * context menu for links that can be opened with the Menu key. Pressing
 * Control-C copies the link URI to the clipboard.
 *
 * In selectable labels with links, link functionality is only available
 * when the selection is empty.
 */
typedef struct
{
  char *uri;
  char *title;     /* the title attribute, used as tooltip */

  GtkCssNode *cssnode;

  gboolean visited; /* get set when the link is activated; this flag
                     * gets preserved over later set_markup() calls
                     */
  int start;       /* position of the link in the PangoLayout */
  int end;
} GtkLabelLink;

struct _GtkLabelSelectionInfo
{
  int selection_anchor;
  int selection_end;
  GtkCssNode *selection_node;
  GdkContentProvider *provider;

  GtkLabelLink *links;
  guint n_links;
  GtkLabelLink *active_link;
  GtkLabelLink *context_link;

  GtkGesture *drag_gesture;
  GtkGesture *click_gesture;
  GtkEventController *motion_controller;
  GtkEventController *focus_controller;

  int drag_start_x;
  int drag_start_y;

  guint in_drag      : 1;
  guint select_words : 1;
  guint selectable   : 1;
  guint link_clicked : 1;
};

enum {
  MOVE_CURSOR,
  COPY_CLIPBOARD,
  ACTIVATE_LINK,
  ACTIVATE_CURRENT_LINK,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  PROP_JUSTIFY,
  PROP_WRAP,
  PROP_WRAP_MODE,
  PROP_NATURAL_WRAP_MODE,
  PROP_SELECTABLE,
  PROP_MNEMONIC_KEYVAL,
  PROP_MNEMONIC_WIDGET,
  PROP_ELLIPSIZE,
  PROP_WIDTH_CHARS,
  PROP_SINGLE_LINE_MODE,
  PROP_MAX_WIDTH_CHARS,
  PROP_LINES,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_EXTRA_MENU,
  PROP_TABS,
  NUM_PROPERTIES
};

static GParamSpec *label_props[NUM_PROPERTIES] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

static GQuark quark_mnemonics_visible_connected;

static void gtk_label_set_markup_internal        (GtkLabel      *self,
                                                  const char    *str,
                                                  gboolean       with_uline);
static void gtk_label_recalculate                (GtkLabel      *self);
static void gtk_label_do_popup                   (GtkLabel      *self,
                                                  double         x,
                                                  double         y);
static void gtk_label_ensure_select_info  (GtkLabel *self);
static void gtk_label_clear_select_info   (GtkLabel *self);
static void gtk_label_clear_layout        (GtkLabel *self);
static void gtk_label_ensure_layout       (GtkLabel *self);
static void gtk_label_select_region_index (GtkLabel *self,
                                           int       anchor_index,
                                           int       end_index);
static void gtk_label_update_active_link  (GtkWidget *widget,
                                           double     x,
                                           double     y);
static void     gtk_label_setup_mnemonic    (GtkLabel          *self);

/* For selectable labels: */
static void gtk_label_move_cursor        (GtkLabel        *self,
                                          GtkMovementStep  step,
                                          int              count,
                                          gboolean         extend_selection);

static void     gtk_label_buildable_interface_init   (GtkBuildableIface  *iface);
static GtkBuildableIface *buildable_parent_iface = NULL;

static void     gtk_label_accessible_text_init (GtkAccessibleTextInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLabel, gtk_label, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_label_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT,
                                                gtk_label_accessible_text_init))

static void
add_move_binding (GtkWidgetClass *widget_class,
                  guint           keyval,
                  guint           modmask,
                  GtkMovementStep step,
                  int             count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, modmask,
                                       "move-cursor",
                                       "(iib)", step, count, FALSE);

  /* Selection-extending version */
  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, modmask | GDK_SHIFT_MASK,
                                       "move-cursor",
                                       "(iib)", step, count, TRUE);
}

static void
gtk_label_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkLabel *self = GTK_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (self, g_value_get_string (value));
      break;
    case PROP_ATTRIBUTES:
      gtk_label_set_attributes (self, g_value_get_boxed (value));
      break;
    case PROP_USE_MARKUP:
      gtk_label_set_use_markup (self, g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (self, g_value_get_boolean (value));
      break;
    case PROP_JUSTIFY:
      gtk_label_set_justify (self, g_value_get_enum (value));
      break;
    case PROP_WRAP:
      gtk_label_set_wrap (self, g_value_get_boolean (value));
      break;
    case PROP_WRAP_MODE:
      gtk_label_set_wrap_mode (self, g_value_get_enum (value));
      break;
    case PROP_NATURAL_WRAP_MODE:
      gtk_label_set_natural_wrap_mode (self, g_value_get_enum (value));
      break;
    case PROP_SELECTABLE:
      gtk_label_set_selectable (self, g_value_get_boolean (value));
      break;
    case PROP_MNEMONIC_WIDGET:
      gtk_label_set_mnemonic_widget (self, (GtkWidget*) g_value_get_object (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_label_set_ellipsize (self, g_value_get_enum (value));
      break;
    case PROP_WIDTH_CHARS:
      gtk_label_set_width_chars (self, g_value_get_int (value));
      break;
    case PROP_SINGLE_LINE_MODE:
      gtk_label_set_single_line_mode (self, g_value_get_boolean (value));
      break;
    case PROP_MAX_WIDTH_CHARS:
      gtk_label_set_max_width_chars (self, g_value_get_int (value));
      break;
    case PROP_LINES:
      gtk_label_set_lines (self, g_value_get_int (value));
      break;
    case PROP_XALIGN:
      gtk_label_set_xalign (self, g_value_get_float (value));
      break;
    case PROP_YALIGN:
      gtk_label_set_yalign (self, g_value_get_float (value));
      break;
    case PROP_EXTRA_MENU:
      gtk_label_set_extra_menu (self, g_value_get_object (value));
      break;
    case PROP_TABS:
      gtk_label_set_tabs (self, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_get_property (GObject     *object,
                        guint        prop_id,
                        GValue      *value,
                        GParamSpec  *pspec)
{
  GtkLabel *self = GTK_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, self->label);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, self->attrs);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, self->use_markup);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, self->use_underline);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, self->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, self->wrap);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, self->wrap_mode);
      break;
    case PROP_NATURAL_WRAP_MODE:
      g_value_set_enum (value, self->natural_wrap_mode);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_label_get_selectable (self));
      break;
    case PROP_MNEMONIC_KEYVAL:
      g_value_set_uint (value, self->mnemonic_keyval);
      break;
    case PROP_MNEMONIC_WIDGET:
      g_value_set_object (value, (GObject*) self->mnemonic_widget);
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, self->ellipsize);
      break;
    case PROP_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_width_chars (self));
      break;
    case PROP_SINGLE_LINE_MODE:
      g_value_set_boolean (value, gtk_label_get_single_line_mode (self));
      break;
    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_max_width_chars (self));
      break;
    case PROP_LINES:
      g_value_set_int (value, gtk_label_get_lines (self));
      break;
    case PROP_XALIGN:
      g_value_set_float (value, gtk_label_get_xalign (self));
      break;
    case PROP_YALIGN:
      g_value_set_float (value, gtk_label_get_yalign (self));
      break;
    case PROP_EXTRA_MENU:
      g_value_set_object (value, gtk_label_get_extra_menu (self));
      break;
    case PROP_TABS:
      g_value_set_boxed (value, self->tabs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_init (GtkLabel *self)
{
  self->width_chars = -1;
  self->max_width_chars = -1;
  self->label = g_strdup ("");
  self->lines = -1;

  self->xalign = 0.5;
  self->yalign = 0.5;

  self->jtype = GTK_JUSTIFY_LEFT;
  self->wrap = FALSE;
  self->wrap_mode = PANGO_WRAP_WORD;
  self->natural_wrap_mode = GTK_NATURAL_WRAP_INHERIT;
  self->ellipsize = PANGO_ELLIPSIZE_NONE;

  self->use_underline = FALSE;
  self->use_markup = FALSE;

  self->mnemonic_keyval = GDK_KEY_VoidSymbol;
  self->layout = NULL;
  self->text = g_strdup ("");
  self->attrs = NULL;
  self->tabs = NULL;

  self->mnemonic_widget = NULL;

  self->mnemonics_visible = FALSE;
}

static const GtkBuildableParser pango_parser =
{
  gtk_pango_attribute_start_element,
};

static gboolean
gtk_label_buildable_custom_tag_start (GtkBuildable       *buildable,
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
gtk_label_buildable_custom_finished (GtkBuildable *buildable,
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
          gtk_label_set_attributes (GTK_LABEL (buildable), data->attrs);
          pango_attr_list_unref (data->attrs);
        }

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_free (data);
    }
}

static void
gtk_label_buildable_interface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_label_buildable_custom_tag_start;
  iface->custom_finished = gtk_label_buildable_custom_finished;
}

static void
update_link_state (GtkLabel *self)
{
  GtkStateFlags state;
  guint i;

  if (!self->select_info)
    return;

  for (i = 0; i < self->select_info->n_links; i++)
    {
      const GtkLabelLink *link = &self->select_info->links[i];

      state = gtk_widget_get_state_flags (GTK_WIDGET (self));
      if (link->visited)
        state |= GTK_STATE_FLAG_VISITED;
      else
        state |= GTK_STATE_FLAG_LINK;
      if (link == self->select_info->active_link)
        {
          if (self->select_info->link_clicked)
            state |= GTK_STATE_FLAG_ACTIVE;
          else
            state |= GTK_STATE_FLAG_PRELIGHT;
        }
      gtk_css_node_set_state (link->cssnode, state);
    }
}

static void
gtk_label_update_cursor (GtkLabel *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  if (!self->select_info)
    return;

  if (gtk_widget_is_sensitive (widget))
    {
      if (self->select_info->active_link)
        gtk_widget_set_cursor_from_name (widget, "pointer");
      else if (self->select_info->selectable)
        gtk_widget_set_cursor_from_name (widget, "text");
      else
        gtk_widget_set_cursor (widget, NULL);
    }
  else
    gtk_widget_set_cursor (widget, NULL);
}

static void
gtk_label_state_flags_changed (GtkWidget     *widget,
                               GtkStateFlags  prev_state)
{
  GtkLabel *self = GTK_LABEL (widget);

  if (self->select_info)
    {
      GtkStateFlags state;

      if (!gtk_widget_is_sensitive (widget))
        gtk_label_select_region (self, 0, 0);

      gtk_label_update_cursor (self);
      update_link_state (self);

      state = gtk_widget_get_state_flags (widget) & ~GTK_STATE_FLAG_DROP_ACTIVE;

      if (self->select_info->selection_node)
        {
          gtk_css_node_set_state (self->select_info->selection_node, state);

          gtk_widget_queue_draw (widget);
        }
    }

  if (GTK_WIDGET_CLASS (gtk_label_parent_class)->state_flags_changed)
    GTK_WIDGET_CLASS (gtk_label_parent_class)->state_flags_changed (widget, prev_state);
}

static void
gtk_label_update_layout_attributes (GtkLabel      *self,
                                    PangoAttrList *style_attrs)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkCssStyle *style;
  PangoAttrList *attrs;

  if (self->layout == NULL)
    {
      pango_attr_list_unref (style_attrs);
      return;
    }

  if (self->select_info && self->select_info->links)
    {
      guint i;

      attrs = pango_attr_list_new ();

      for (i = 0; i < self->select_info->n_links; i++)
        {
          const GtkLabelLink *link = &self->select_info->links[i];
          const GdkRGBA *link_color;
          PangoAttrList *link_attrs;
          PangoAttribute *attr;

          style = gtk_css_node_get_style (link->cssnode);
          link_attrs = gtk_css_style_get_pango_attributes (style);
          if (link_attrs)
            {
              GSList *attributes = pango_attr_list_get_attributes (link_attrs);
              GSList *l;
              for (l = attributes; l; l = l->next)
                {
                  attr = l->data;

                  attr->start_index = link->start;
                  attr->end_index = link->end;
                  pango_attr_list_insert (attrs, attr);
                }
              g_slist_free (attributes);
            }

          link_color = gtk_css_color_value_get_rgba (style->core->color);
          attr = pango_attr_foreground_new (link_color->red * 65535,
                                            link_color->green * 65535,
                                            link_color->blue * 65535);
          attr->start_index = link->start;
          attr->end_index = link->end;
          pango_attr_list_insert (attrs, attr);

          pango_attr_list_unref (link_attrs);
        }
    }
  else
    attrs = NULL;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  if (!style_attrs)
    style_attrs = gtk_css_style_get_pango_attributes (style);

  if (style_attrs)
    {
      attrs = _gtk_pango_attr_list_merge (attrs, style_attrs);
      pango_attr_list_unref (style_attrs);
    }

  attrs = _gtk_pango_attr_list_merge (attrs, self->markup_attrs);
  attrs = _gtk_pango_attr_list_merge (attrs, self->attrs);

  pango_layout_set_attributes (self->layout, attrs);

  pango_attr_list_unref (attrs);
}

static void
gtk_label_css_changed (GtkWidget         *widget,
                       GtkCssStyleChange *change)
{
  GtkLabel *self = GTK_LABEL (widget);
  gboolean attrs_affected;
  PangoAttrList *new_attrs = NULL;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->css_changed (widget, change);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_ATTRS))
    {
      new_attrs = gtk_css_style_get_pango_attributes (gtk_css_style_change_get_new_style (change));
      attrs_affected = (self->layout && pango_layout_get_attributes (self->layout)) ||
                       new_attrs;
    }
  else
    attrs_affected = FALSE;

  if (change == NULL || attrs_affected  || (self->select_info && self->select_info->links))
    {
      gtk_label_update_layout_attributes (self, new_attrs);

      if (attrs_affected)
        gtk_widget_queue_draw (widget);
    }
}

static PangoDirection
get_cursor_direction (GtkLabel *self)
{
  GSList *l;

  g_assert (self->select_info);

  gtk_label_ensure_layout (self);

  for (l = pango_layout_get_lines_readonly (self->layout); l; l = l->next)
    {
      PangoLayoutLine *line = l->data;

      /* If self->select_info->selection_end is at the very end of
       * the line, we don't know if the cursor is on this line or
       * the next without looking ahead at the next line. (End
       * of paragraph is different from line break.) But it's
       * definitely in this paragraph, which is good enough
       * to figure out the resolved direction.
       */
       if (pango_layout_line_get_start_index (line) + pango_layout_line_get_length (line) >= self->select_info->selection_end)
        return pango_layout_line_get_resolved_direction (line);
    }

  return PANGO_DIRECTION_LTR;
}

static int
_gtk_label_get_link_at (GtkLabel *self,
                        int       pos)
{
  if (self->select_info)
    {
      guint i;

      for (i = 0; i < self->select_info->n_links; i++)
        {
          const GtkLabelLink *link = &self->select_info->links[i];

          if (link->start <= pos && pos < link->end)
            return i;
        }
    }

  return -1;
}

static GtkLabelLink *
gtk_label_get_focus_link (GtkLabel *self,
                          int      *out_index)
{
  GtkLabelSelectionInfo *info = self->select_info;
  int link_index;

  if (!info ||
      info->selection_anchor != info->selection_end)
    goto nope;

  link_index = _gtk_label_get_link_at (self, info->selection_anchor);

  if (link_index != -1)
    {
      if (out_index)
        *out_index = link_index;

      return &info->links[link_index];
    }

nope:
  if (out_index)
    *out_index = -1;
  return NULL;
}

/**
 * gtk_label_get_measuring_layout:
 * @self: the label
 * @existing_layout: %NULL or an existing layout already in use.
 * @width: the width to measure with in pango units, or -1 for infinite
 *
 * Gets a layout that can be used for measuring sizes.
 *
 * The returned layout will be identical to the label’s layout except for
 * the layout’s width, which will be set to @width. Do not modify the
 * returned layout.
 *
 * Returns: a new reference to a pango layout
 */
static PangoLayout *
gtk_label_get_measuring_layout (GtkLabel    *self,
                                PangoLayout *existing_layout,
                                int          width)
{
  PangoLayout *copy;

  if (existing_layout != NULL)
    {
      if (existing_layout != self->layout)
        {
          pango_layout_set_width (existing_layout, width);
          return existing_layout;
        }

      g_object_unref (existing_layout);
    }

  gtk_label_ensure_layout (self);

  if (pango_layout_get_width (self->layout) == width)
    {
      g_object_ref (self->layout);
      return self->layout;
    }

  /* We can use the label's own layout if we're not allocated a size yet,
   * because we don't need it to be properly setup at that point.
   * This way we can make use of caching upon the label's creation.
   */
  if (gtk_widget_get_width (GTK_WIDGET (self)) <= 1)
    {
      g_object_ref (self->layout);
      pango_layout_set_width (self->layout, width);
      return self->layout;
    }

  /* oftentimes we want to measure a width that is far wider than the current width,
   * even though the layout would not change if we made it wider. In that case, we
   * can just return the current layout, because for measuring purposes, it will be
   * identical.
   */
  if (!pango_layout_is_wrapped (self->layout) &&
      !pango_layout_is_ellipsized (self->layout))
    {
      PangoRectangle rect;

      if (width == -1)
        return g_object_ref (self->layout);

      pango_layout_get_extents (self->layout, NULL, &rect);
      if (rect.width <= width)
        return g_object_ref (self->layout);
    }

  copy = pango_layout_copy (self->layout);
  pango_layout_set_width (copy, width);
  return copy;
}

static int
get_char_pixels (PangoLayout *layout)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int char_width, digit_width;

  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context, NULL, NULL);
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  return MAX (char_width, digit_width);
}

static void
get_default_widths (GtkLabel    *self,
                    int         *minimum,
                    int         *natural)
{
  int char_pixels;

  if (self->width_chars < 0 && self->max_width_chars < 0)
    {
      if (minimum)
        *minimum = -1;
      if (natural)
        *natural = -1;
      return;
    }

  gtk_label_ensure_layout (self);
  char_pixels = get_char_pixels (self->layout);

  if (minimum)
    {
      if (self->width_chars < 0)
        *minimum = -1;
      else
        *minimum = char_pixels * self->width_chars;
    }

  if (natural)
    {
      if (self->max_width_chars < 0)
        *natural = -1;
      else
        *natural = char_pixels * MAX (self->width_chars, self->max_width_chars);
    }
}

static void
get_static_size (GtkLabel       *self,
                 GtkOrientation  orientation,
                 int            *minimum,
                 int            *natural,
                 int            *minimum_baseline,
                 int            *natural_baseline)
{
  int minimum_default, natural_default;
  PangoLayout *layout;

  get_default_widths (self, &minimum_default, &natural_default);

  layout = gtk_label_get_measuring_layout (self, NULL, self->ellipsize ? natural_default : -1);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      pango_layout_get_size (layout, natural, NULL);
      if (self->ellipsize)
        {
          layout = gtk_label_get_measuring_layout (self, layout, 0);
          pango_layout_get_size (layout, minimum, NULL);
          /* yes, Pango ellipsizes even when that needs more space */
          *minimum = MIN (*minimum, *natural);
        }
      else
        *minimum = *natural;

      if (minimum_default > *minimum)
        *minimum = minimum_default;
      *natural = MAX (*minimum, *natural);
    }
  else
    {
      pango_layout_get_size (layout, NULL, minimum);
      *minimum_baseline = pango_layout_get_baseline (layout);

      *natural = *minimum;
      *natural_baseline = *minimum_baseline;
    }

  g_object_unref (layout);
}

static void
get_height_for_width (GtkLabel *self,
                      int       width,
                      int      *minimum_height,
                      int      *natural_height,
                      int      *minimum_baseline,
                      int      *natural_baseline)
{
  PangoLayout *layout;
  int natural_width, text_height, baseline;

  if (width < 0)
    {
      /* Minimum height is assuming infinite width */
      layout = gtk_label_get_measuring_layout (self, NULL, -1);
      pango_layout_get_size (layout, NULL, minimum_height);
      baseline = pango_layout_get_baseline (layout);
      *minimum_baseline = baseline;

      /* Natural height is assuming natural width */
      get_default_widths (self, NULL, &natural_width);

      layout = gtk_label_get_measuring_layout (self, layout, natural_width);
      pango_layout_get_size (layout, NULL, natural_height);
      baseline = pango_layout_get_baseline (layout);
      *natural_baseline = baseline;
    }
  else
    {
      /* minimum = natural for any given width */
      layout = gtk_label_get_measuring_layout (self, NULL, width);

      pango_layout_get_size (layout, NULL, &text_height);

      *minimum_height = text_height;
      *natural_height = text_height;

      baseline = pango_layout_get_baseline (layout);
      *minimum_baseline = baseline;
      *natural_baseline = baseline;
    }

  g_object_unref (layout);
}

static int
my_pango_layout_get_width_for_height (PangoLayout *layout,
                                      int          for_height,
                                      int          min,
                                      int          max)
{
  int mid, text_width, text_height;

  min = PANGO_PIXELS_CEIL (min);
  max = PANGO_PIXELS_CEIL (max);

  while (min < max)
    {
      mid = (min + max) / 2;
      pango_layout_set_width (layout, mid * PANGO_SCALE);
      pango_layout_get_size (layout, &text_width, &text_height);
      text_width = PANGO_PIXELS_CEIL (text_width);
      if (text_width > mid)
        min = text_width;
      else if (text_height > for_height)
        min = mid + 1;
      else
        max = text_width;
    }

  return min * PANGO_SCALE;
}

static void
get_width_for_height (GtkLabel *self,
                      int       height,
                      int      *minimum_width,
                      int      *natural_width)
{
  PangoLayout *layout;
  int minimum_default, natural_default;

  get_default_widths (self, &minimum_default, &natural_default);

  if (height < 0)
    {
      /* Minimum width is as many line breaks as possible */
      layout = gtk_label_get_measuring_layout (self, NULL, MAX (minimum_default, 0));
      pango_layout_get_size (layout, minimum_width, NULL);
      *minimum_width = MAX (*minimum_width, minimum_default);

      /* Natural width is natural width - or as wide as possible */
      layout = gtk_label_get_measuring_layout (self, layout, natural_default);
      pango_layout_get_size (layout, natural_width, NULL);
      *natural_width = MAX (*natural_width, *minimum_width);
    }
  else
    {
      int min, max;

      /* Can't use a measuring layout here, because we need to force
       * ellipsizing mode */
      gtk_label_ensure_layout (self);
      layout = pango_layout_copy (self->layout);
      pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_NONE);

      /* binary search for the smallest width where the height doesn't
       * eclipse the given height */
      min = MAX (minimum_default, 0);

      pango_layout_set_width (layout, -1);
      pango_layout_get_size (layout, &max, NULL);

      /* first, do natural width */
      if (self->natural_wrap_mode == GTK_NATURAL_WRAP_NONE)
        {
          *natural_width = max;
        }
      else
        {
          if (self->natural_wrap_mode == GTK_NATURAL_WRAP_WORD)
            pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
          *natural_width = my_pango_layout_get_width_for_height (layout, height, min, max);
        }

      /* then, do minimum width */
      if (self->ellipsize != PANGO_ELLIPSIZE_NONE)
        {
          g_object_unref (layout);
          layout = gtk_label_get_measuring_layout (self, NULL, MAX (minimum_default, 0));
          pango_layout_get_size (layout, minimum_width, NULL);
          *minimum_width = MAX (*minimum_width, minimum_default);
        }
      else if (self->natural_wrap_mode == GTK_NATURAL_WRAP_INHERIT)
        {
          *minimum_width = *natural_width;
        }
      else
        {
          pango_layout_set_wrap (layout, self->wrap_mode);
          *minimum_width = my_pango_layout_get_width_for_height (layout, height, min, *natural_width);
        }
    }

  g_object_unref (layout);
}

static void
gtk_label_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkLabel *self = GTK_LABEL (widget);

  if (for_size > 0)
    for_size *= PANGO_SCALE;

  if (!self->wrap)
    get_static_size (self, orientation, minimum, natural, minimum_baseline, natural_baseline);
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    get_height_for_width (self, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    get_width_for_height (self, for_size, minimum, natural);

  *minimum = PANGO_PIXELS_CEIL (*minimum);
  *natural = PANGO_PIXELS_CEIL (*natural);
  if (*minimum_baseline > 0)
    *minimum_baseline = PANGO_PIXELS_CEIL (*minimum_baseline);
  if (*natural_baseline > 0)
    *natural_baseline = PANGO_PIXELS_CEIL (*natural_baseline);
}

static void
get_layout_location (GtkLabel  *self,
                     float     *xp,
                     float     *yp)
{
  GtkWidget *widget = GTK_WIDGET (self);
  const int widget_width = gtk_widget_get_width (widget);
  const int widget_height = gtk_widget_get_height (widget);
  PangoRectangle logical;
  float xalign;
  int baseline;
  float x, y;

  g_assert (xp);
  g_assert (yp);

  xalign = self->xalign;

  if (_gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    xalign = 1.0 - xalign;

  pango_layout_get_pixel_extents (self->layout, NULL, &logical);
  x = floor ((xalign * (widget_width - logical.width)) - logical.x);
  if (x < 0)
    x = 0.f;

  baseline = gtk_widget_get_baseline (widget);
  if (baseline != -1)
    {
      int layout_baseline = pango_layout_get_baseline (self->layout) / PANGO_SCALE;
      /* yalign is 0 because we can't support yalign while baseline aligning */
      y = baseline - layout_baseline;
    }
  else
    {
      y = floor ((widget_height - logical.height) * self->yalign);
    }

  *xp = x;
  *yp = y;
}

static void
gtk_label_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkLabel *self = GTK_LABEL (widget);

  if (self->layout)
    {
      if (self->ellipsize || self->wrap)
        pango_layout_set_width (self->layout, width * PANGO_SCALE);
      else
        pango_layout_set_width (self->layout, -1);
    }

  if (self->popup_menu)
    gtk_popover_present (GTK_POPOVER (self->popup_menu));
}



#define GRAPHENE_RECT_FROM_RECT(_r) (GRAPHENE_RECT_INIT ((_r)->x, (_r)->y, (_r)->width, (_r)->height))

static void
gtk_label_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkLabel *self = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info;
  GtkCssStyle *style;
  float lx, ly;
  int width, height;
  GtkCssBoxes boxes;

  if (!self->text || (*self->text == '\0'))
    return;

  gtk_label_ensure_layout (self);

  get_layout_location (self, &lx, &ly);

  gtk_css_boxes_init (&boxes, widget);
  gtk_css_style_snapshot_layout (&boxes, snapshot, lx, ly, self->layout);

  info = self->select_info;
  if (!info)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (info->selection_anchor != info->selection_end)
    {
      int range[2];
      cairo_region_t *range_clip;
      cairo_rectangle_int_t clip_rect;
      int i;

      range[0] = MIN (info->selection_anchor, info->selection_end);
      range[1] = MAX (info->selection_anchor, info->selection_end);

      style = gtk_css_node_get_style (info->selection_node);
      gtk_css_boxes_init_border_box (&boxes, style, 0, 0, width, height);

      range_clip = gdk_pango_layout_get_clip_region (self->layout, lx, ly, range, 1);
      for (i = 0; i < cairo_region_num_rectangles (range_clip); i++)
        {
          cairo_region_get_rectangle (range_clip, i, &clip_rect);

          gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_FROM_RECT (&clip_rect));
          gtk_css_style_snapshot_background (&boxes, snapshot);
          gtk_css_style_snapshot_layout (&boxes, snapshot, lx, ly, self->layout);
          gtk_snapshot_pop (snapshot);
        }

      cairo_region_destroy (range_clip);
    }
  else
    {
      GtkLabelLink *focus_link;
      GtkLabelLink *active_link;
      int range[2];
      cairo_region_t *range_clip;
      cairo_rectangle_int_t clip_rect;
      int i;
      GdkRectangle rect;

      if (info->selectable &&
          gtk_widget_has_focus (widget) &&
          gtk_widget_is_drawable (widget))
        {
          PangoDirection cursor_direction;

          cursor_direction = get_cursor_direction (self);
          gtk_css_style_snapshot_caret (&boxes, gtk_widget_get_display (widget),
                                        snapshot,
                                        lx, ly,
                                        self->layout,
                                        self->select_info->selection_end,
                                        cursor_direction);
        }

      focus_link = gtk_label_get_focus_link (self, NULL);
      active_link = info->active_link;

      if (active_link)
        {
          range[0] = active_link->start;
          range[1] = active_link->end;

          style = gtk_css_node_get_style (active_link->cssnode);
          gtk_css_boxes_init_border_box (&boxes, style, 0, 0, width, height);

          range_clip = gdk_pango_layout_get_clip_region (self->layout, lx, ly, range, 1);
          for (i = 0; i < cairo_region_num_rectangles (range_clip); i++)
            {
              cairo_region_get_rectangle (range_clip, i, &clip_rect);

              gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_FROM_RECT (&clip_rect));
              gtk_css_style_snapshot_background (&boxes, snapshot);
              gtk_css_style_snapshot_layout (&boxes, snapshot, lx, ly, self->layout);
              gtk_snapshot_pop (snapshot);
            }

          cairo_region_destroy (range_clip);
        }

      if (focus_link && gtk_widget_has_visible_focus (widget))
        {
          range[0] = focus_link->start;
          range[1] = focus_link->end;

          style = gtk_css_node_get_style (focus_link->cssnode);

          range_clip = gdk_pango_layout_get_clip_region (self->layout, lx, ly, range, 1);
          cairo_region_get_extents (range_clip, &rect);

          gtk_css_boxes_init_border_box (&boxes, style, rect.x, rect.y, rect.width, rect.height);
          gtk_css_style_snapshot_outline (&boxes, snapshot);

          cairo_region_destroy (range_clip);
        }
    }
}

static GtkSizeRequestMode
gtk_label_get_request_mode (GtkWidget *widget)
{
  GtkLabel *self = GTK_LABEL (widget);

  if (self->wrap)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_label_dispose (GObject *object)
{
  GtkLabel *self = GTK_LABEL (object);

  gtk_label_set_mnemonic_widget (self, NULL);
  gtk_label_clear_select_info (self);

  G_OBJECT_CLASS (gtk_label_parent_class)->dispose (object);
}

static void
gtk_label_clear_links (GtkLabel *self)
{
  guint i;

  if (!self->select_info)
    return;

  for (i = 0; i < self->select_info->n_links; i++)
    {
      const GtkLabelLink *link = &self->select_info->links[i];
      gtk_css_node_set_parent (link->cssnode, NULL);
      g_free (link->uri);
      g_free (link->title);
    }
  g_free (self->select_info->links);
  self->select_info->links = NULL;
  self->select_info->n_links = 0;
  self->select_info->active_link = NULL;
  gtk_widget_remove_css_class (GTK_WIDGET (self), "link");
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *self = GTK_LABEL (object);

  g_free (self->label);
  g_free (self->text);

  g_clear_object (&self->layout);
  g_clear_pointer (&self->attrs, pango_attr_list_unref);
  g_clear_pointer (&self->markup_attrs, pango_attr_list_unref);

  if (self->select_info)
    g_object_unref (self->select_info->provider);

  gtk_label_clear_links (self);
  g_free (self->select_info);

  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);
  g_clear_object (&self->extra_menu);

  g_clear_pointer (&self->tabs, pango_tab_array_free);

  G_OBJECT_CLASS (gtk_label_parent_class)->finalize (object);
}

static void
gtk_label_unrealize (GtkWidget *widget)
{
  GtkLabel *self = GTK_LABEL (widget);

  if (self->select_info &&
      self->select_info->provider)
    {
      GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (widget);

      if (gdk_clipboard_get_content (clipboard) == self->select_info->provider)
        gdk_clipboard_set_content (clipboard, NULL);
    }

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unrealize (widget);
}

static gboolean
range_is_in_ellipsis_full (GtkLabel *self,
                           int       range_start,
                           int       range_end,
                           int      *ellipsis_start,
                           int      *ellipsis_end)
{
  PangoLayoutIter *iter;
  gboolean in_ellipsis;

  if (!self->ellipsize)
    return FALSE;

  gtk_label_ensure_layout (self);

  if (!pango_layout_is_ellipsized (self->layout))
    return FALSE;

  iter = pango_layout_get_iter (self->layout);

  in_ellipsis = FALSE;

  do {
    PangoLayoutRun *run;

    run = pango_layout_iter_get_run_readonly (iter);
    if (run)
      {
        PangoItem *item;

        item = ((PangoGlyphItem*)run)->item;

        if (item->offset <= range_start && range_end <= item->offset + item->length)
          {
            if (item->analysis.flags & PANGO_ANALYSIS_FLAG_IS_ELLIPSIS)
              {
                if (ellipsis_start)
                  *ellipsis_start = item->offset;
                if (ellipsis_end)
                  *ellipsis_end = item->offset + item->length;
                in_ellipsis = TRUE;
              }
            break;
          }
        else if (item->offset + item->length >= range_end)
          break;
      }
  } while (pango_layout_iter_next_run (iter));

  pango_layout_iter_free (iter);

  return in_ellipsis;
}

static gboolean
range_is_in_ellipsis (GtkLabel *self,
                      int       range_start,
                      int       range_end)
{
  return range_is_in_ellipsis_full (self, range_start, range_end, NULL, NULL);
}

static gboolean
gtk_label_grab_focus (GtkWidget *widget)
{
  GtkLabel *self = GTK_LABEL (widget);
  gboolean select_on_focus;
  GtkWidget *prev_focus;

  if (self->select_info == NULL)
    return FALSE;

  prev_focus = gtk_root_get_focus (gtk_widget_get_root (widget));

  if (!GTK_WIDGET_CLASS (gtk_label_parent_class)->grab_focus (widget))
    return FALSE;

  if (self->select_info->selectable)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-label-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus && !self->in_click &&
          !(prev_focus && gtk_widget_is_ancestor (prev_focus, widget)))
        gtk_label_select_region (self, 0, -1);
    }
  else
    {
      if (self->select_info->links && !self->in_click &&
          !(prev_focus && gtk_widget_is_ancestor (prev_focus, widget)))
        {
          guint i;

          for (i = 0; i < self->select_info->n_links; i++)
            {
              const GtkLabelLink *link = &self->select_info->links[i];

              if (!range_is_in_ellipsis (self, link->start, link->end))
                {
                  self->select_info->selection_anchor = link->start;
                  self->select_info->selection_end = link->start;
                  break;
                }
            }
        }
    }

  return TRUE;
}

static gboolean
get_layout_index (GtkLabel *self,
                  int       x,
                  int       y,
                  int      *index)
{
  int trailing = 0;
  const char *cluster;
  const char *cluster_end;
  gboolean inside;
  float lx, ly;

  *index = 0;

  gtk_label_ensure_layout (self);
  get_layout_location (self, &lx, &ly);

  /* Translate x/y to layout position */
  x -= lx;
  y -= ly;

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;

  inside = pango_layout_xy_to_index (self->layout,
                                     x, y,
                                     index, &trailing);

  cluster = self->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);

  return inside;
}

static gboolean
gtk_label_query_tooltip (GtkWidget  *widget,
                         int         x,
                         int         y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkLabel *self = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = self->select_info;
  int index = -1;

  if (info && info->links)
    {
      if (keyboard_tip)
        {
          if (info->selection_anchor == info->selection_end)
            index = info->selection_anchor;
        }
      else
        {
          if (!get_layout_index (self, x, y, &index))
            index = -1;
        }

      if (index != -1)
        {
          const int link_index = _gtk_label_get_link_at (self, index);

          if (link_index != -1)
            {
              const GtkLabelLink *link = &info->links[link_index];

              if (link->title)
                {
                  gtk_tooltip_set_markup (tooltip, link->title);
                  return TRUE;
                }
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_label_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}

static gboolean
gtk_label_focus (GtkWidget        *widget,
                 GtkDirectionType  direction)
{
  GtkLabel *self = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = self->select_info;
  GtkLabelLink *focus_link;

  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      if (info)
        {
          focus_link = gtk_label_get_focus_link (self, NULL);
          if (focus_link && direction == GTK_DIR_TAB_BACKWARD)
            {
              int i;
              for (i = info->n_links - 1; i >= 0; i--)
                {
                  focus_link = &info->links[i];
                  if (!range_is_in_ellipsis (self, focus_link->start, focus_link->end))
                    {
                      info->selection_anchor = focus_link->start;
                      info->selection_end = focus_link->start;
                      break;
                    }
                }
            }

          return TRUE;
        }

      return FALSE;
    }

  if (!info)
    return FALSE;

  if (info->selectable)
    {
      int index;

      if (info->selection_anchor != info->selection_end)
        goto out;

      index = info->selection_anchor;

      if (direction == GTK_DIR_TAB_FORWARD)
        {
          guint i;
          for (i = 0; i < info->n_links; i++)
            {
              const GtkLabelLink *link = &info->links[i];

              if (link->start > index)
                {
                  if (!range_is_in_ellipsis (self, link->start, link->end))
                    {
                      gtk_label_select_region_index (self, link->start, link->start);
                      return TRUE;
                    }
                }
            }
        }
      else if (direction == GTK_DIR_TAB_BACKWARD)
        {
          int i;
          for (i = info->n_links - 1; i >= 0; i--)
            {
              GtkLabelLink *link = &info->links[i];

              if (link->end < index)
                {
                  if (!range_is_in_ellipsis (self, link->start, link->end))
                    {
                      gtk_label_select_region_index (self, link->start, link->start);
                      return TRUE;
                    }
                }
            }
        }

      goto out;
    }
  else
    {
      int focus_link_index;
      int new_index = -1;

      if (info->n_links == 0)
        goto out;

      focus_link = gtk_label_get_focus_link (self, &focus_link_index);

      if (!focus_link)
        goto out;

      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          if (focus_link)
            new_index = focus_link_index + 1;
          else
            new_index = 0;

          if (new_index >= info->n_links)
            goto out;

          while (new_index < info->n_links)
            {
              const GtkLabelLink *link = &info->links[new_index];
              if (!range_is_in_ellipsis (self, link->start, link->end))
                break;

              new_index++;
            }
          break;

        case GTK_DIR_TAB_BACKWARD:
          if (focus_link)
            new_index = focus_link_index - 1;
          else
            new_index = info->n_links - 1;

          if (new_index < 0)
            goto out;

          while (new_index >= 0)
            {
              const GtkLabelLink *link = &info->links[new_index];
              if (!range_is_in_ellipsis (self, link->start, link->end))
                break;

              new_index--;
            }
          break;

        default:
        case GTK_DIR_UP:
        case GTK_DIR_DOWN:
        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
          goto out;
        }

      if (new_index != -1 && new_index < info->n_links)
        {
          focus_link = &info->links[new_index];
          info->selection_anchor = focus_link->start;
          info->selection_end = focus_link->start;
          gtk_widget_queue_draw (widget);

          return TRUE;
        }
    }

out:

  return FALSE;
}

static void
emit_activate_link (GtkLabel     *self,
                    GtkLabelLink *link)
{
  gboolean handled;

  g_signal_emit (self, signals[ACTIVATE_LINK], 0, link->uri, &handled);

  /* signal handler might have invalidated the layout */
  if (!self->layout)
    return;

  if (handled && !link->visited &&
      self->select_info && self->select_info->links)
    {
      link->visited = TRUE;
      update_link_state (self);
    }
}

static void
gtk_label_activate_link_open (GtkWidget  *widget,
                              const char *name,
                              GVariant   *parameter)
{
  GtkLabel *self = GTK_LABEL (widget);
  if (self->select_info)
    {
      GtkLabelLink *link = self->select_info->context_link;

      if (link)
        emit_activate_link (self, link);
    }
}

static void
gtk_label_activate_link_copy (GtkWidget  *widget,
                              const char *name,
                              GVariant   *parameter)
{
  GtkLabel *self = GTK_LABEL (widget);
  if (self->select_info)
    {
      GtkLabelLink *link = self->select_info->context_link;

      if (link)
        {
          GdkClipboard *clipboard;

          clipboard = gtk_widget_get_clipboard (widget);
          gdk_clipboard_set_text (clipboard, link->uri);
        }
    }
}

static void
gtk_label_activate_clipboard_copy (GtkWidget  *widget,
                                   const char *name,
                                   GVariant   *parameter)
{
  g_signal_emit_by_name (widget, "copy-clipboard");
}

static void
gtk_label_select_all (GtkLabel *self)
{
  gtk_label_select_region_index (self, 0, strlen (self->text));
}

static void
gtk_label_activate_selection_select_all (GtkWidget  *widget,
                                         const char *name,
                                         GVariant   *parameter)
{
  gtk_label_select_all (GTK_LABEL (widget));
}

static void
gtk_label_nop (GtkWidget  *widget,
               const char *name,
               GVariant   *parameter)
{
}

static gboolean
gtk_label_mnemonic_activate (GtkWidget *widget,
                             gboolean   group_cycling)
{
  GtkLabel *self = GTK_LABEL (widget);
  GtkWidget *parent;

  if (self->mnemonic_widget)
    return gtk_widget_mnemonic_activate (self->mnemonic_widget, group_cycling);

  /* Not a label for something else, but is selectable, so set focus into
   * the label itself.
  */
  if (gtk_label_get_selectable (self) && gtk_widget_get_focusable (widget))
    return gtk_label_grab_focus (widget);

  /* Try to find the widget to activate by traversing the
   * widget's ancestry.
   */
  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_NOTEBOOK (parent))
    return FALSE;

  while (parent)
    {
      if (gtk_widget_get_focusable (parent) ||
          (!group_cycling && gtk_widget_can_activate (parent)) ||
          GTK_IS_NOTEBOOK (gtk_widget_get_parent (parent)))
        return gtk_widget_mnemonic_activate (parent, group_cycling);
      parent = gtk_widget_get_parent (parent);
    }

  /* barf if there was nothing to activate */
  g_warning ("Couldn't find a target for a mnemonic activation.");
  gtk_widget_error_bell (widget);

  return FALSE;
}

static void
gtk_label_popup_menu (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameters)
{
  GtkLabel *self = GTK_LABEL (widget);

  gtk_label_do_popup (self, -1, -1);
}

static void
gtk_label_root (GtkWidget *widget)
{
  GtkLabel *self = GTK_LABEL (widget);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->root (widget);

  gtk_label_setup_mnemonic (self);

  /* The PangoContext is replaced when the display changes, so clear the layouts */
  gtk_label_clear_layout (GTK_LABEL (widget));
}

static void
gtk_label_unroot (GtkWidget *widget)
{
  GtkLabel *self = GTK_LABEL (widget);

  gtk_label_setup_mnemonic (self);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unroot (widget);
}

static void
launch_done (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GError *error = NULL;
  gboolean success;

  if (GTK_IS_FILE_LAUNCHER (source))
    success = gtk_file_launcher_launch_finish (GTK_FILE_LAUNCHER (source), result, &error);
  else if (GTK_IS_URI_LAUNCHER (source))
    success = gtk_uri_launcher_launch_finish (GTK_URI_LAUNCHER (source), result, &error);
  else
    g_assert_not_reached ();

  if (!success)
    {
      g_warning ("Failed to launch handler: %s", error->message);
      g_error_free (error);
    }
}

static gboolean
gtk_label_activate_link (GtkLabel    *self,
                         const char *uri)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  const char *uri_scheme;

  if (!GTK_IS_WINDOW (toplevel))
    return FALSE;

  uri_scheme = g_uri_peek_scheme (uri);
  if (g_strcmp0 (uri_scheme, "file") == 0)
    {
      GFile *file;
      GtkFileLauncher *launcher;

      file = g_file_new_for_uri (uri);
      launcher = gtk_file_launcher_new (file);
      gtk_file_launcher_launch (launcher, GTK_WINDOW (toplevel), NULL, launch_done, NULL);
      g_object_unref (launcher);
      g_object_unref (file);
    }
  else
    {
      GtkUriLauncher *launcher;

      launcher = gtk_uri_launcher_new (uri);
      gtk_uri_launcher_launch (launcher, GTK_WINDOW (toplevel), NULL, launch_done, NULL);
      g_object_unref (launcher);
    }

  return TRUE;
}

static void
gtk_label_activate_current_link (GtkLabel *self)
{
  GtkLabelLink *link;
  GtkWidget *widget = GTK_WIDGET (self);

  link = gtk_label_get_focus_link (self, NULL);

  if (link)
    emit_activate_link (self, link);
  else
    gtk_widget_activate_default (widget);
}

static void
gtk_label_copy_clipboard (GtkLabel *self)
{
  if (self->text && self->select_info)
    {
      int start, end;
      int len;
      GdkClipboard *clipboard;

      start = MIN (self->select_info->selection_anchor,
                   self->select_info->selection_end);
      end = MAX (self->select_info->selection_anchor,
                 self->select_info->selection_end);

      len = strlen (self->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));

      if (start != end)
        {
          char *str = g_strndup (self->text + start, end - start);
          gdk_clipboard_set_text (clipboard, str);
          g_free (str);
        }
      else
        {
          GtkLabelLink *link;

          link = gtk_label_get_focus_link (self, NULL);
          if (link)
            gdk_clipboard_set_text (clipboard, link->uri);
        }
    }
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_label_set_property;
  gobject_class->get_property = gtk_label_get_property;
  gobject_class->finalize = gtk_label_finalize;
  gobject_class->dispose = gtk_label_dispose;

  widget_class->size_allocate = gtk_label_size_allocate;
  widget_class->state_flags_changed = gtk_label_state_flags_changed;
  widget_class->css_changed = gtk_label_css_changed;
  widget_class->query_tooltip = gtk_label_query_tooltip;
  widget_class->snapshot = gtk_label_snapshot;
  widget_class->unrealize = gtk_label_unrealize;
  widget_class->root = gtk_label_root;
  widget_class->unroot = gtk_label_unroot;
  widget_class->mnemonic_activate = gtk_label_mnemonic_activate;
  widget_class->grab_focus = gtk_label_grab_focus;
  widget_class->focus = gtk_label_focus;
  widget_class->get_request_mode = gtk_label_get_request_mode;
  widget_class->measure = gtk_label_measure;

  class->move_cursor = gtk_label_move_cursor;
  class->copy_clipboard = gtk_label_copy_clipboard;
  class->activate_link = gtk_label_activate_link;

  /**
   * GtkLabel::move-cursor:
   * @entry: the object which received the signal
   * @step: the granularity of the move, as a `GtkMovementStep`
   * @count: the number of @step units to move
   * @extend_selection: %TRUE if the move should extend the selection
   *
   * Gets emitted when the user initiates a cursor movement.
   *
   * The ::move-cursor signal is a [keybinding signal](class.SignalAction.html).
   * If the cursor is not visible in @entry, this signal causes the viewport to
   * be moved instead.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal come in two variants,
   * the variant with the Shift modifier extends the selection,
   * the variant without the Shift modifier does not.
   * There are too many key combinations to list them all here.
   *
   * - <kbd>←</kbd>, <kbd>→</kbd>, <kbd>↑</kbd>, <kbd>↓</kbd>
   *   move by individual characters/lines
   * - <kbd>Ctrl</kbd>+<kbd>←</kbd>, etc. move by words/paragraphs
   * - <kbd>Home</kbd> and <kbd>End</kbd> move to the ends of the buffer
   */
  signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkLabelClass, move_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
                  G_TYPE_NONE, 3,
                  GTK_TYPE_MOVEMENT_STEP,
                  G_TYPE_INT,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[MOVE_CURSOR],
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              _gtk_marshal_VOID__ENUM_INT_BOOLEANv);

   /**
   * GtkLabel::copy-clipboard:
   * @self: the object which received the signal
   *
   * Gets emitted to copy the selection to the clipboard.
   *
   * The ::copy-clipboard signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>Ctrl</kbd>+<kbd>c</kbd>.
   */
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkLabelClass, copy_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkLabel::activate-current-link:
   * @self: The label on which the signal was emitted
   *
   * Gets emitted when the user activates a link in the label.
   *
   * The ::activate-current-link is a [keybinding signal](class.SignalAction.html).
   *
   * Applications may also emit the signal with g_signal_emit_by_name()
   * if they need to control activation of URIs programmatically.
   *
   * The default bindings for this signal are all forms of the <kbd>Enter</kbd> key.
   */
  signals[ACTIVATE_CURRENT_LINK] =
    g_signal_new_class_handler (I_("activate-current-link"),
                                G_TYPE_FROM_CLASS (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_label_activate_current_link),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  /**
   * GtkLabel::activate-link:
   * @self: The label on which the signal was emitted
   * @uri: the URI that is activated
   *
   * Gets emitted to activate a URI.
   *
   * Applications may connect to it to override the default behaviour,
   * which is to call [method@Gtk.FileLauncher.launch].
   *
   * Returns: %TRUE if the link has been activated
   */
  signals[ACTIVATE_LINK] =
    g_signal_new (I_("activate-link"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkLabelClass, activate_link),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals[ACTIVATE_LINK],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__STRINGv);

  /**
   * GtkLabel:label: (attributes org.gtk.Property.get=gtk_label_get_label org.gtk.Property.set=gtk_label_set_label)
   *
   * The contents of the label.
   *
   * If the string contains Pango markup (see [func@Pango.parse_markup]),
   * you will have to set the [property@Gtk.Label:use-markup] property to
   * %TRUE in order for the label to display the markup attributes. See also
   * [method@Gtk.Label.set_markup] for a convenience function that sets both
   * this property and the [property@Gtk.Label:use-markup] property at the
   * same time.
   *
   * If the string contains underlines acting as mnemonics, you will have to
   * set the [property@Gtk.Label:use-underline] property to %TRUE in order
   * for the label to display them.
   */
  label_props[PROP_LABEL] =
      g_param_spec_string ("label", NULL, NULL,
                           "",
                           GTK_PARAM_READWRITE);

  /**
   * GtkLabel:attributes: (attributes org.gtk.Property.get=gtk_label_get_attributes org.gtk.Property.set=gtk_label_set_attributes)
   *
   * A list of style attributes to apply to the text of the label.
   */
  label_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes", NULL, NULL,
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE);

  /**
   * GtkLabel:use-markup: (attributes org.gtk.Property.get=gtk_label_get_use_markup org.gtk.Property.set=gtk_label_set_use_markup)
   *
   * %TRUE if the text of the label includes Pango markup.
   *
   * See [func@Pango.parse_markup].
   */
  label_props[PROP_USE_MARKUP] =
      g_param_spec_boolean ("use-markup", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:use-underline: (attributes org.gtk.Property.get=gtk_label_get_use_underline org.gtk.Property.set=gtk_label_set_use_underline)
   *
   * %TRUE if the text of the label indicates a mnemonic with an _
   * before the mnemonic character.
   */
  label_props[PROP_USE_UNDERLINE] =
      g_param_spec_boolean ("use-underline", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:justify: (attributes org.gtk.Property.get=gtk_label_get_justify org.gtk.Property.set=gtk_label_set_justify)
   *
   * The alignment of the lines in the text of the label, relative to each other.
   *
   * This does *not* affect the alignment of the label within its allocation.
   * See [property@Gtk.Label:xalign] for that.
   */
  label_props[PROP_JUSTIFY] =
      g_param_spec_enum ("justify", NULL, NULL,
                         GTK_TYPE_JUSTIFICATION,
                         GTK_JUSTIFY_LEFT,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:xalign: (attributes org.gtk.Property.get=gtk_label_get_xalign org.gtk.Property.set=gtk_label_set_xalign)
   *
   * The horizontal alignment of the label text inside its size allocation.
   *
   * Compare this to [property@Gtk.Widget:halign], which determines how the
   * labels size allocation is positioned in the space available for the label.
   */
  label_props[PROP_XALIGN] =
      g_param_spec_float ("xalign", NULL, NULL,
                          0.0, 1.0,
                          0.5,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:yalign: (attributes org.gtk.Property.get=gtk_label_get_yalign org.gtk.Property.set=gtk_label_set_yalign)
   *
   * The vertical alignment of the label text inside its size allocation.
   *
   * Compare this to [property@Gtk.Widget:valign], which determines how the
   * labels size allocation is positioned in the space available for the label.
   */
  label_props[PROP_YALIGN] =
      g_param_spec_float ("yalign", NULL, NULL,
                          0.0, 1.0,
                          0.5,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:wrap: (attributes org.gtk.Property.get=gtk_label_get_wrap org.gtk.Property.set=gtk_label_set_wrap)
   *
   * %TRUE if the label text will wrap if it gets too wide.
   */
  label_props[PROP_WRAP] =
      g_param_spec_boolean ("wrap", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:wrap-mode: (attributes org.gtk.Property.get=gtk_label_get_wrap_mode org.gtk.Property.set=gtk_label_set_wrap_mode)
   *
   * Controls how the line wrapping is done.
   *
   * This only affects the formatting if line wrapping is on (see the
   * [property@Gtk.Label:wrap] property). The default is %PANGO_WRAP_WORD,
   * which means wrap on word boundaries.
   *
   * For sizing behavior, also consider the [property@Gtk.Label:natural-wrap-mode]
   * property.
   */
  label_props[PROP_WRAP_MODE] =
      g_param_spec_enum ("wrap-mode", NULL, NULL,
                         PANGO_TYPE_WRAP_MODE,
                         PANGO_WRAP_WORD,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:natural-wrap-mode: (attributes org.gtk.Property.get=gtk_label_get_natural_wrap_mode org.gtk.Property.set=gtk_label_set_natural_wrap_mode)
   *
   * Select the line wrapping for the natural size request.
   *
   * This only affects the natural size requested. For the actual wrapping used,
   * see the [property@Gtk.Label:wrap-mode] property.
   *
   * The default is %GTK_NATURAL_WRAP_INHERIT, which inherits the behavior of the
   * [property@Gtk.Label:wrap-mode] property.
   *
   * Since: 4.6
   */
  label_props[PROP_NATURAL_WRAP_MODE] =
      g_param_spec_enum ("natural-wrap-mode", NULL, NULL,
                         GTK_TYPE_NATURAL_WRAP_MODE,
                         GTK_NATURAL_WRAP_INHERIT,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:selectable: (attributes org.gtk.Property.get=gtk_label_get_selectable og.gtk.Property.set=gtk_label_set_selectable)
   *
   * Whether the label text can be selected with the mouse.
   */
  label_props[PROP_SELECTABLE] =
      g_param_spec_boolean ("selectable", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:mnemonic-keyval: (attributes org.gtk.Property.get=gtk_label_get_mnemonic_keyval)
   *
   * The mnemonic accelerator key for the label.
   */
  label_props[PROP_MNEMONIC_KEYVAL] =
      g_param_spec_uint ("mnemonic-keyval", NULL, NULL,
                         0, G_MAXUINT,
                         GDK_KEY_VoidSymbol,
                         GTK_PARAM_READABLE);

  /**
   * GtkLabel:mnemonic-widget: (attributes org.gtk.Property.get=gtk_label_get_mnemonic_widget org.gtk.Property.set=gtk_label_set_mnemonic_widget)
   *
   * The widget to be activated when the labels mnemonic key is pressed.
   */
  label_props[PROP_MNEMONIC_WIDGET] =
      g_param_spec_object ("mnemonic-widget", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:ellipsize: (attributes org.gtk.Property.get=gtk_label_get_ellipsize org.gtk.Property.set=gtk_label_set_ellipsize)
   *
   * The preferred place to ellipsize the string, if the label does
   * not have enough room to display the entire string.
   *
   * Note that setting this property to a value other than
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the label requests
   * only enough space to display the ellipsis "...". In particular, this
   * means that ellipsizing labels do not work well in notebook tabs, unless
   * the [property@Gtk.NotebookPage:tab-expand] child property is set to %TRUE.
   * Other ways to set a label's width are [method@Gtk.Widget.set_size_request]
   * and [method@Gtk.Label.set_width_chars].
   */
  label_props[PROP_ELLIPSIZE] =
      g_param_spec_enum ("ellipsize", NULL, NULL,
                         PANGO_TYPE_ELLIPSIZE_MODE,
                         PANGO_ELLIPSIZE_NONE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:width-chars: (attributes org.gtk.Property.get=gtk_label_get_width_chars org.gtk.Property.set=gtk_label_set_width_chars)
   *
   * The desired width of the label, in characters.
   *
   * If this property is set to -1, the width will be calculated automatically.
   *
   * See the section on [text layout](class.Label.html#text-layout) for details of how
   * [property@Gtk.Label:width-chars] and [property@Gtk.Label:max-width-chars]
   * determine the width of ellipsized and wrapped labels.
   */
  label_props[PROP_WIDTH_CHARS] =
      g_param_spec_int ("width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:single-line-mode: (attributes org.gtk.Property.get=gtk_label_get_single_line_mode org.gtk.Property.set=gtk_label_set_single_line_mode)
   *
   * Whether the label is in single line mode.
   *
   * In single line mode, the height of the label does not depend on the
   * actual text, it is always set to ascent + descent of the font. This
   * can be an advantage in situations where resizing the label because
   * of text changes would be distracting, e.g. in a statusbar.
   */
  label_props[PROP_SINGLE_LINE_MODE] =
      g_param_spec_boolean ("single-line-mode", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:max-width-chars: (attributes org.gtk.Property.get=gtk_label_set_max_width_chars org.gtk.Property.set=gtk_label_set_max_width_chars)
   *
   * The desired maximum width of the label, in characters.
   *
   * If this property is set to -1, the width will be calculated automatically.
   *
   * See the section on [text layout](class.Label.html#text-layout) for details of how
   * [property@Gtk.Label:width-chars] and [property@Gtk.Label:max-width-chars]
   * determine the width of ellipsized and wrapped labels.
   */
  label_props[PROP_MAX_WIDTH_CHARS] =
      g_param_spec_int ("max-width-chars", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:lines: (attributes org.gtk.Property.get=gtk_label_get_lines org.gtk.Property.set=gtk_label_set_lines)
   *
   * The number of lines to which an ellipsized, wrapping label
   * should be limited.
   *
   * This property has no effect if the label is not wrapping or ellipsized.
   * Set this property to -1 if you don't want to limit the number of lines.
   */
  label_props[PROP_LINES] =
      g_param_spec_int ("lines", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:extra-menu: (attributes org.gtk.Property.get=gtk_label_get_extra_menu org.gtk.Property.set=gtk_label_set_extra_menu)
   *
   * A menu model whose contents will be appended to the context menu.
   */
  label_props[PROP_EXTRA_MENU] =
      g_param_spec_object ("extra-menu", NULL, NULL,
                          G_TYPE_MENU_MODEL,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLabel:tabs: (attributes org.gtk.Property.get=gtk_label_get_tabs org.gtk.Property.set=gtk_label_set_tabs)
   *
   * Custom tabs for this label.
   *
   * Since: 4.8
   */
  label_props[PROP_TABS] =
      g_param_spec_boxed ("tabs", NULL, NULL,
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, label_props);

  /**
   * GtkLabel|menu.popup:
   *
   * Opens the context menu.
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, gtk_label_popup_menu);

  /*
   * Key bindings
   */

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);

  /* Moving the insertion point */
  add_move_binding (widget_class, GDK_KEY_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, 0,
                      GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_f, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_b, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK,
                      GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  /* select all */
  gtk_widget_class_add_binding (widget_class,
                                GDK_KEY_a, GDK_CONTROL_MASK,
                                (GtkShortcutFunc) gtk_label_select_all,
                                NULL);
  gtk_widget_class_add_binding (widget_class,
                                GDK_KEY_slash, GDK_CONTROL_MASK,
                                (GtkShortcutFunc) gtk_label_select_all,
                                NULL);

  /* unselect all */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "move-cursor",
                                       "(iib)", GTK_MOVEMENT_PARAGRAPH_ENDS, 0, FALSE);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_backslash, GDK_CONTROL_MASK,
                                       "move-cursor",
                                       "(iib)", GTK_MOVEMENT_PARAGRAPH_ENDS, 0, FALSE);

  add_move_binding (widget_class, GDK_KEY_f, GDK_ALT_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_b, GDK_ALT_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* copy */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_c, GDK_CONTROL_MASK,
                                       "copy-clipboard",
                                       NULL);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Return, 0,
                                       "activate-current-link",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_ISO_Enter, 0,
                                       "activate-current-link",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Enter, 0,
                                       "activate-current-link",
                                       NULL);

  gtk_widget_class_set_css_name (widget_class, I_("label"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LABEL);

  quark_mnemonics_visible_connected = g_quark_from_static_string ("gtk-label-mnemonics-visible-connected");

  /**
   * GtkLabel|clipboard.cut:
   *
   * Doesn't do anything, since text in labels can't be deleted.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.cut", NULL,
                                   gtk_label_nop);

  /**
   * GtkLabel|clipboard.copy:
   *
   * Copies the text to the clipboard.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL,
                                   gtk_label_activate_clipboard_copy);

  /**
   * GtkLabel|clipboard.paste:
   *
   * Doesn't do anything, since text in labels can't be edited.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL,
                                   gtk_label_nop);

  /**
   * GtkLabel|selection.delete:
   *
   * Doesn't do anything, since text in labels can't be deleted.
   */
  gtk_widget_class_install_action (widget_class, "selection.delete", NULL,
                                   gtk_label_nop);

  /**
   * GtkLabel|selection.select-all:
   *
   * Selects all of the text, if the label allows selection.
   */
  gtk_widget_class_install_action (widget_class, "selection.select-all", NULL,
                                   gtk_label_activate_selection_select_all);

  /**
   * GtkLabel|link.open:
   *
   * Opens the link, when activated on a link inside the label.
   */
  gtk_widget_class_install_action (widget_class, "link.open", NULL,
                                   gtk_label_activate_link_open);

  /**
   * GtkLabel|link.copy:
   *
   * Copies the link to the clipboard, when activated on a link
   * inside the label.
   */
  gtk_widget_class_install_action (widget_class, "link.copy", NULL,
                                   gtk_label_activate_link_copy);
}

/**
 * gtk_label_new:
 * @str: (nullable): The text of the label
 *
 * Creates a new label with the given text inside it.
 *
 * You can pass %NULL to get an empty label widget.
 *
 * Returns: the new `GtkLabel`
 **/
GtkWidget*
gtk_label_new (const char *str)
{
  GtkLabel *self;

  self = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text (self, str);

  return GTK_WIDGET (self);
}

/**
 * gtk_label_new_with_mnemonic:
 * @str: (nullable): The text of the label, with an underscore in front of the
 *   mnemonic character
 *
 * Creates a new `GtkLabel`, containing the text in @str.
 *
 * If characters in @str are preceded by an underscore, they are
 * underlined. If you need a literal underscore character in a label, use
 * '__' (two underscores). The first underlined character represents a
 * keyboard accelerator called a mnemonic. The mnemonic key can be used
 * to activate another widget, chosen automatically, or explicitly using
 * [method@Gtk.Label.set_mnemonic_widget].
 *
 * If [method@Gtk.Label.set_mnemonic_widget] is not called, then the first
 * activatable ancestor of the `GtkLabel` will be chosen as the mnemonic
 * widget. For instance, if the label is inside a button or menu item,
 * the button or menu item will automatically become the mnemonic widget
 * and be activated by the mnemonic.
 *
 * Returns: the new `GtkLabel`
 **/
GtkWidget*
gtk_label_new_with_mnemonic (const char *str)
{
  GtkLabel *self;

  self = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text_with_mnemonic (self, str);

  return GTK_WIDGET (self);
}

static void
_gtk_label_mnemonics_visible_apply_recursively (GtkWidget *widget,
                                                gboolean   visible)
{
  if (GTK_IS_LABEL (widget))
    {
      GtkLabel *self = GTK_LABEL (widget);

      if (self->mnemonics_visible != visible)
        {
          self->mnemonics_visible = visible;
          gtk_label_recalculate (self);
        }
    }
  else
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (GTK_IS_NATIVE (child))
            continue;

          _gtk_label_mnemonics_visible_apply_recursively (child, visible);
        }
    }
}

static void
label_mnemonics_visible_changed (GtkWidget  *widget,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  gboolean visible;

  g_object_get (widget, "mnemonics-visible", &visible, NULL);
  _gtk_label_mnemonics_visible_apply_recursively (widget, visible);
}

static void
gtk_label_setup_mnemonic (GtkLabel *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkShortcut *shortcut;
  GtkNative *native;
  gboolean connected;
  gboolean mnemonics_visible;

  if (self->mnemonic_keyval == GDK_KEY_VoidSymbol)
    {
      if (self->mnemonic_controller)
        {
          gtk_widget_remove_controller (widget, self->mnemonic_controller);
          self->mnemonic_controller = NULL;
        }
      return;
    }

  if (self->mnemonic_controller == NULL)
    {
      self->mnemonic_controller = gtk_shortcut_controller_new ();
      gtk_event_controller_set_propagation_phase (self->mnemonic_controller, GTK_PHASE_CAPTURE);
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (self->mnemonic_controller), GTK_SHORTCUT_SCOPE_MANAGED);
      shortcut = gtk_shortcut_new (gtk_mnemonic_trigger_new (self->mnemonic_keyval),
                                   g_object_ref (gtk_mnemonic_action_get ()));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (self->mnemonic_controller), shortcut);
      gtk_widget_add_controller (GTK_WIDGET (self), self->mnemonic_controller);
    }
  else
    {
      shortcut = g_list_model_get_item (G_LIST_MODEL (self->mnemonic_controller), 0);
      gtk_shortcut_set_trigger (shortcut, gtk_mnemonic_trigger_new (self->mnemonic_keyval));
      g_object_unref (shortcut);
    }

  /* Connect to notify::mnemonics-visible of the root */
  native = gtk_widget_get_native (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (native) && !GTK_IS_POPOVER (native))
    return;

  /* always set up this widgets initial value */
  g_object_get (native, "mnemonics-visible", &mnemonics_visible, NULL);
  self->mnemonics_visible = mnemonics_visible;

  connected = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (native),
                                                   quark_mnemonics_visible_connected));

  if (!connected)
    {
      g_signal_connect (native,
                        "notify::mnemonics-visible",
                        G_CALLBACK (label_mnemonics_visible_changed),
                        self);
      g_object_set_qdata (G_OBJECT (native),
                          quark_mnemonics_visible_connected,
                          GINT_TO_POINTER (1));
    }
}

static void
label_mnemonic_widget_weak_notify (gpointer      data,
                                   GObject      *where_the_object_was)
{
  GtkLabel *self = data;

  self->mnemonic_widget = NULL;
  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_MNEMONIC_WIDGET]);
}

/**
 * gtk_label_set_mnemonic_widget: (attributes org.gtk.Method.set_property=mnemonic-widget)
 * @self: a `GtkLabel`
 * @widget: (nullable): the target `GtkWidget`, or %NULL to unset
 *
 * Associate the label with its mnemonic target.
 *
 * If the label has been set so that it has a mnemonic key (using
 * i.e. [method@Gtk.Label.set_markup_with_mnemonic],
 * [method@Gtk.Label.set_text_with_mnemonic],
 * [ctor@Gtk.Label.new_with_mnemonic]
 * or the [property@Gtk.Label:use_underline] property) the label can be
 * associated with a widget that is the target of the mnemonic. When the
 * label is inside a widget (like a [class@Gtk.Button] or a
 * [class@Gtk.Notebook] tab) it is automatically associated with the correct
 * widget, but sometimes (i.e. when the target is a [class@Gtk.Entry] next to
 * the label) you need to set it explicitly using this function.
 *
 * The target widget will be accelerated by emitting the
 * [signal@Gtk.Widget::mnemonic-activate] signal on it. The default handler for
 * this signal will activate the widget if there are no mnemonic collisions
 * and toggle focus between the colliding widgets otherwise.
 */
void
gtk_label_set_mnemonic_widget (GtkLabel  *self,
                               GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_LABEL (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->mnemonic_widget == widget)
    return;

  if (self->mnemonic_widget)
    {
      gtk_widget_remove_mnemonic_label (self->mnemonic_widget, GTK_WIDGET (self));
      g_object_weak_unref (G_OBJECT (self->mnemonic_widget),
                           label_mnemonic_widget_weak_notify,
                           self);
    }
  self->mnemonic_widget = widget;
  if (self->mnemonic_widget)
    {
      g_object_weak_ref (G_OBJECT (self->mnemonic_widget),
                         label_mnemonic_widget_weak_notify,
                         self);
      gtk_widget_add_mnemonic_label (self->mnemonic_widget, GTK_WIDGET (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_MNEMONIC_WIDGET]);
}

/**
 * gtk_label_get_mnemonic_widget: (attributes org.gtk.Method.get_property=mnemonic-widget)
 * @self: a `GtkLabel`
 *
 * Retrieves the target of the mnemonic (keyboard shortcut) of this
 * label.
 *
 * See [method@Gtk.Label.set_mnemonic_widget].
 *
 * Returns: (nullable) (transfer none): the target of the label’s mnemonic,
 *   or %NULL if none has been set and the default algorithm will be used.
 **/
GtkWidget *
gtk_label_get_mnemonic_widget (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->mnemonic_widget;
}

/**
 * gtk_label_get_mnemonic_keyval: (attributes org.gtk.Method.get_property=mnemonic-keyval)
 * @self: a `GtkLabel`
 *
 * Return the mnemonic accelerator.
 *
 * If the label has been set so that it has a mnemonic key this function
 * returns the keyval used for the mnemonic accelerator. If there is no
 * mnemonic set up it returns `GDK_KEY_VoidSymbol`.
 *
 * Returns: GDK keyval usable for accelerators, or `GDK_KEY_VoidSymbol`
 **/
guint
gtk_label_get_mnemonic_keyval (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), GDK_KEY_VoidSymbol);

  return self->mnemonic_keyval;
}

static void
gtk_label_set_text_internal (GtkLabel *self,
                             char     *str)
{
  if (g_strcmp0 (self->text, str) == 0)
    {
      g_free (str);
      return;
    }

  g_free (self->text);
  self->text = str;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  self->text,
                                  -1);

  gtk_label_select_region_index (self, 0, 0);
}

static gboolean
gtk_label_set_label_internal (GtkLabel   *self,
                              const char *str)
{
  if (g_strcmp0 (str, self->label) == 0)
    return FALSE;

  g_free (self->label);
  self->label = g_strdup (str ? str : "");

  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_LABEL]);

  return TRUE;
}

static gboolean
gtk_label_set_use_markup_internal (GtkLabel *self,
                                   gboolean  val)
{
  if (self->use_markup != val)
    {
      self->use_markup = val;

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_USE_MARKUP]);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_label_set_use_underline_internal (GtkLabel *self,
                                      gboolean  val)
{
  if (self->use_underline != val)
    {
      self->use_underline = val;

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_USE_UNDERLINE]);

      return TRUE;
    }

  return FALSE;
}

/* Calculates text, attrs and mnemonic_keyval from
 * label, use_underline and use_markup
 */
static void
gtk_label_recalculate (GtkLabel *self)
{
  guint keyval = self->mnemonic_keyval;

  gtk_label_clear_links (self);
  gtk_label_clear_layout (self);
  gtk_label_clear_select_info (self);

  if (self->use_markup)
    {
      gtk_label_set_markup_internal (self, self->label, self->use_underline);
    }
  else if (self->use_underline)
    {
      char *text;

      text = g_markup_escape_text (self->label, -1);
      gtk_label_set_markup_internal (self, text, TRUE);
      g_free (text);
    }
  else
    {
      g_clear_pointer (&self->markup_attrs, pango_attr_list_unref);

      gtk_label_set_text_internal (self, g_strdup (self->label));
    }

  if (!self->use_underline)
    self->mnemonic_keyval = GDK_KEY_VoidSymbol;

  if (keyval != self->mnemonic_keyval)
    {
      gtk_label_setup_mnemonic (self);
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_MNEMONIC_KEYVAL]);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_label_set_text:
 * @self: a `GtkLabel`
 * @str: The text you want to set
 *
 * Sets the text within the `GtkLabel` widget.
 *
 * It overwrites any text that was there before.
 *
 * This function will clear any previously set mnemonic accelerators,
 * and set the [property@Gtk.Label:use-underline] property to %FALSE as
 * a side effect.
 *
 * This function will set the [property@Gtk.Label:use-markup] property
 * to %FALSE as a side effect.
 *
 * See also: [method@Gtk.Label.set_markup]
 */
void
gtk_label_set_text (GtkLabel    *self,
                    const char *str)
{
  gboolean changed;

  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  changed = gtk_label_set_label_internal (self, str);
  changed = gtk_label_set_use_markup_internal (self, FALSE) || changed;
  changed = gtk_label_set_use_underline_internal (self, FALSE) || changed;

  if (changed)
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @self: a `GtkLabel`
 * @attrs: (nullable): a [struct@Pango.AttrList]
 *
 * Apply attributes to the label text.
 *
 * The attributes set with this function will be applied and merged with
 * any other attributes previously effected by way of the
 * [property@Gtk.Label:use-underline] or [property@Gtk.Label:use-markup]
 * properties. While it is not recommended to mix markup strings with
 * manually set attributes, if you must; know that the attributes will
 * be applied to the label after the markup string is parsed.
 */
void
gtk_label_set_attributes (GtkLabel         *self,
                          PangoAttrList    *attrs)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (!attrs && !self->attrs)
    return;

  if (attrs)
    pango_attr_list_ref (attrs);

  if (self->attrs)
    pango_attr_list_unref (self->attrs);
  self->attrs = attrs;

  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_ATTRIBUTES]);

  gtk_label_clear_layout (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_label_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @self: a `GtkLabel`
 *
 * Gets the label's attribute list.
 *
 * This is the [struct@Pango.AttrList] that was set on the label using
 * [method@Gtk.Label.set_attributes], if any. This function does not
 * reflect attributes that come from the label's markup (see
 * [method@Gtk.Label.set_markup]). If you want to get the effective
 * attributes for the label, use
 * `pango_layout_get_attributes (gtk_label_get_layout (self))`.
 *
 * Returns: (nullable) (transfer none): the attribute list
 */
PangoAttrList *
gtk_label_get_attributes (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->attrs;
}

/**
 * gtk_label_set_label: (attributes org.gtk.Method.set_property=label)
 * @self: a `GtkLabel`
 * @str: the new text to set for the label
 *
 * Sets the text of the label.
 *
 * The label is interpreted as including embedded underlines and/or Pango
 * markup depending on the values of the [property@Gtk.Label:use-underline]
 * and [property@Gtk.Label:use-markup] properties.
 */
void
gtk_label_set_label (GtkLabel    *self,
                     const char *str)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (gtk_label_set_label_internal (self, str))
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_get_label: (attributes org.gtk.Method.get_property=label)
 * @self: a `GtkLabel`
 *
 * Fetches the text from a label.
 *
 * The returned text includes any embedded underlines indicating
 * mnemonics and Pango markup. (See [method@Gtk.Label.get_text]).
 *
 * Returns: the text of the label widget. This string is
 *   owned by the widget and must not be modified or freed.
 */
const char *
gtk_label_get_label (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->label;
}

typedef struct
{
  GtkLabel *label;
  GArray *links;
  GString *new_str;
  gsize text_len;
  gboolean strip_ulines;
  GString *text_data;
  gunichar accel_key;
} UriParserData;

static char *
strip_ulines (const char *text,
              guint      *accel_key)
{
  char *new_text;
  const char *p;
  char *q;
  gboolean after_uline = FALSE;

  new_text = g_malloc (strlen (text) + 1);

  q = new_text;
  for (p = text; *p; p++)
    {
      if (*p == '_' && !after_uline)
        {
          after_uline = TRUE;
          continue;
        }

      *q = *p;
      if (after_uline && *p != '_' && *accel_key == 0)
        *accel_key = g_utf8_get_char (p);

      q++;
      after_uline = FALSE;
    }

  if (after_uline)
    {
      *q = '_';
      q++;
    }

  *q = '\0';

  return new_text;
}

static void
finish_text (UriParserData *pdata)
{
  if (pdata->text_data->len > 0)
    {
      char *text;
      gsize text_len;
      char *newtext;

      if (pdata->strip_ulines && strchr (pdata->text_data->str, '_'))
        {
          text = strip_ulines (pdata->text_data->str, &pdata->accel_key);
          text_len = strlen (text);
        }
      else
        {
          text = pdata->text_data->str;
          text_len = pdata->text_data->len;
        }

      newtext = g_markup_escape_text (text, text_len);
      g_string_append (pdata->new_str, newtext);
      pdata->text_len += text_len;
      g_free (newtext);

      if (text != pdata->text_data->str)
        g_free (text);

      g_string_set_size (pdata->text_data, 0);
    }
}

static void
start_element_handler (GMarkupParseContext  *context,
                       const char           *element_name,
                       const char          **attribute_names,
                       const char          **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  UriParserData *pdata = user_data;
  GtkLabel *self = pdata->label;

  finish_text (pdata);

  if (strcmp (element_name, "a") == 0)
    {
      GtkLabelLink link;
      const char *uri = NULL;
      const char *title = NULL;
      const char *class = NULL;
      gboolean visited = FALSE;
      int line_number;
      int char_number;
      int i;
      GtkCssNode *widget_node;
      GtkStateFlags state;

      g_markup_parse_context_get_position (context, &line_number, &char_number);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const char *attr = attribute_names[i];

          if (strcmp (attr, "href") == 0)
            uri = attribute_values[i];
          else if (strcmp (attr, "title") == 0)
            title = attribute_values[i];
          else if (strcmp (attr, "class") == 0)
            class = attribute_values[i];
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           "Attribute '%s' is not allowed on the <a> tag "
                           "on line %d char %d",
                            attr, line_number, char_number);
              return;
            }
        }

      if (uri == NULL)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Attribute 'href' was missing on the <a> tag "
                       "on line %d char %d",
                       line_number, char_number);
          return;
        }

      visited = FALSE;
      if (self->select_info)
        {
          for (i = 0; i < self->select_info->n_links; i++)
            {
              const GtkLabelLink *l = &self->select_info->links[i];

              if (strcmp (uri, l->uri) == 0)
                {
                  visited = l->visited;
                  break;
                }
            }
        }

      if (!pdata->links)
        pdata->links = g_array_new (FALSE, TRUE, sizeof (GtkLabelLink));

      link.uri = g_strdup (uri);
      link.title = g_strdup (title);

      widget_node = gtk_widget_get_css_node (GTK_WIDGET (pdata->label));
      link.cssnode = gtk_css_node_new ();
      gtk_css_node_set_name (link.cssnode, g_quark_from_static_string ("link"));
      gtk_css_node_set_parent (link.cssnode, widget_node);
      if (class)
        gtk_css_node_add_class (link.cssnode, g_quark_from_string (class));

      state = gtk_css_node_get_state (widget_node);
      if (visited)
        state |= GTK_STATE_FLAG_VISITED;
      else
        state |= GTK_STATE_FLAG_LINK;
      gtk_css_node_set_state (link.cssnode, state);
      g_object_unref (link.cssnode);

      link.visited = visited;
      link.start = pdata->text_len;
      g_array_append_val (pdata->links, link);
    }
  else
    {
      int i;

      g_string_append_c (pdata->new_str, '<');
      g_string_append (pdata->new_str, element_name);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const char *attr  = attribute_names[i];
          const char *value = attribute_values[i];
          char *newvalue;

          newvalue = g_markup_escape_text (value, -1);

          g_string_append_c (pdata->new_str, ' ');
          g_string_append (pdata->new_str, attr);
          g_string_append (pdata->new_str, "=\"");
          g_string_append (pdata->new_str, newvalue);
          g_string_append_c (pdata->new_str, '\"');

          g_free (newvalue);
        }
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
end_element_handler (GMarkupParseContext  *context,
                     const char           *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  UriParserData *pdata = user_data;

  finish_text (pdata);

  if (!strcmp (element_name, "a"))
    {
      GtkLabelLink *link = &g_array_index (pdata->links, GtkLabelLink, pdata->links->len - 1);
      link->end = pdata->text_len;
    }
  else
    {
      g_string_append (pdata->new_str, "</");
      g_string_append (pdata->new_str, element_name);
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
text_handler (GMarkupParseContext  *context,
              const char           *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
  UriParserData *pdata = user_data;

  g_string_append_len (pdata->text_data, text, text_len);
}

static const GMarkupParser markup_parser =
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static gboolean
xml_isspace (char c)
{
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static gboolean
parse_uri_markup (GtkLabel      *self,
                  const char    *str,
                  gboolean       strip_ulines,
                  gunichar      *accel_key,
                  char         **new_str,
                  GtkLabelLink **links,
                  guint         *out_n_links,
                  GError       **error)
{
  GMarkupParseContext *context;
  const char *p, *end;
  gsize length;
  UriParserData pdata;

  length = strlen (str);
  p = str;
  end = str + length;

  pdata.label = self;
  pdata.links = NULL;
  pdata.new_str = g_string_sized_new (length);
  pdata.text_len = 0;
  pdata.strip_ulines = strip_ulines;
  pdata.text_data = g_string_new ("");
  pdata.accel_key = 0;

  while (p != end && xml_isspace (*p))
    p++;

  context = g_markup_parse_context_new (&markup_parser, 0, &pdata, NULL);

  if (end - p >= 8 && strncmp (p, "<markup>", 8) == 0)
    {
      if (!g_markup_parse_context_parse (context, str, length, error))
        goto failed;
    }
  else
    {
      if (!g_markup_parse_context_parse (context, "<markup>", 8, error))
        goto failed;

      if (!g_markup_parse_context_parse (context, str, length, error))
        goto failed;

      if (!g_markup_parse_context_parse (context, "</markup>", 9, error))
        goto failed;
    }

  if (!g_markup_parse_context_end_parse (context, error))
    goto failed;

  g_markup_parse_context_free (context);

  g_string_free (pdata.text_data, TRUE);

  *new_str = g_string_free (pdata.new_str, FALSE);

  if (pdata.links)
    {
      *out_n_links = pdata.links->len;
      *links = (GtkLabelLink *)g_array_free (pdata.links, FALSE);
    }
  else
    {
      *links = NULL;
    }

  if (accel_key)
    *accel_key = pdata.accel_key;

  return TRUE;

failed:
  g_markup_parse_context_free (context);
  g_string_free (pdata.new_str, TRUE);

  if (pdata.links)
    g_array_free (pdata.links, TRUE);

  return FALSE;
}

static void
gtk_label_ensure_has_tooltip (GtkLabel *self)
{
  guint i;
  gboolean has_tooltip = gtk_widget_get_has_tooltip(GTK_WIDGET(self));

  if (has_tooltip) {
    return;
  }

  for (i = 0; i < self->select_info->n_links; i++)
    {
      const GtkLabelLink *link = &self->select_info->links[i];

      if (link->title)
        {
          has_tooltip = TRUE;
          break;
        }
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (self), has_tooltip);
}

static void
gtk_label_set_markup_internal (GtkLabel   *self,
                               const char *str,
                               gboolean    with_uline)
{
  char *text = NULL;
  GError *error = NULL;
  PangoAttrList *attrs = NULL;
  char *str_for_display = NULL;
  GtkLabelLink *links = NULL;
  guint n_links = 0;
  gunichar accel_keyval = 0;
  gboolean do_mnemonics;

  do_mnemonics = self->mnemonics_visible &&
                 gtk_widget_is_sensitive (GTK_WIDGET (self)) &&
                 (!self->mnemonic_widget || gtk_widget_is_sensitive (self->mnemonic_widget));

  if (!parse_uri_markup (self, str,
                         with_uline && !do_mnemonics,
                         &accel_keyval,
                         &str_for_display,
                         &links, &n_links,
                         &error))
    goto error_set;

  if (links)
    {
      gtk_label_ensure_select_info (self);
      self->select_info->links = g_steal_pointer (&links);
      self->select_info->n_links = n_links;
      gtk_label_ensure_has_tooltip (self);
      gtk_widget_add_css_class (GTK_WIDGET (self), "link");
    }

  if (!pango_parse_markup (str_for_display, -1,
                           with_uline && do_mnemonics ? '_' : 0,
                           &attrs, &text,
                           with_uline && do_mnemonics ? &accel_keyval : NULL,
                           &error))
    goto error_set;

  g_free (str_for_display);

  if (text)
    gtk_label_set_text_internal (self, text);

  g_clear_pointer (&self->markup_attrs, pango_attr_list_unref);
  self->markup_attrs = attrs;

  self->mnemonic_keyval = accel_keyval;

  return;

error_set:
  g_warning ("Failed to set text '%s' from markup due to error parsing markup: %s",
             str, error->message);
  g_error_free (error);

}

/**
 * gtk_label_set_markup:
 * @self: a `GtkLabel`
 * @str: a markup string
 *
 * Sets the labels text and attributes from markup.
 *
 * The string must be marked up with Pango markup
 * (see [func@Pango.parse_markup]).
 *
 * If the @str is external data, you may need to escape it
 * with g_markup_escape_text() or g_markup_printf_escaped():
 *
 * ```c
 * GtkWidget *self = gtk_label_new (NULL);
 * const char *str = "...";
 * const char *format = "<span style=\"italic\">\%s</span>";
 * char *markup;
 *
 * markup = g_markup_printf_escaped (format, str);
 * gtk_label_set_markup (GTK_LABEL (self), markup);
 * g_free (markup);
 * ```
 *
 * This function will set the [property@Gtk.Label:use-markup] property
 * to %TRUE as a side effect.
 *
 * If you set the label contents using the [property@Gtk.Label:label]
 * property you should also ensure that you set the
 * [property@Gtk.Label:use-markup] property accordingly.
 *
 * See also: [method@Gtk.Label.set_text]
 */
void
gtk_label_set_markup (GtkLabel    *self,
                      const char *str)
{
  gboolean changed;

  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  changed = gtk_label_set_label_internal (self, str);
  changed = gtk_label_set_use_markup_internal (self, TRUE) || changed;
  changed = gtk_label_set_use_underline_internal (self, FALSE) || changed;

  if (changed)
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_set_markup_with_mnemonic:
 * @self: a `GtkLabel`
 * @str: a markup string
 *
 * Sets the labels text, attributes and mnemonic from markup.
 *
 * Parses @str which is marked up with Pango markup (see [func@Pango.parse_markup]),
 * setting the label’s text and attribute list based on the parse results.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 *
 * The mnemonic key can be used to activate another widget, chosen
 * automatically, or explicitly using [method@Gtk.Label.set_mnemonic_widget].
 */
void
gtk_label_set_markup_with_mnemonic (GtkLabel    *self,
                                    const char *str)
{
  gboolean changed;

  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  changed = gtk_label_set_label_internal (self, str);
  changed = gtk_label_set_use_markup_internal (self, TRUE) || changed;
  changed = gtk_label_set_use_underline_internal (self, TRUE) || changed;

  if (changed)
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_get_text:
 * @self: a `GtkLabel`
 *
 * Fetches the text from a label.
 *
 * The returned text is as it appears on screen. This does not include
 * any embedded underlines indicating mnemonics or Pango markup. (See
 * [method@Gtk.Label.get_label])
 *
 * Returns: the text in the label widget. This is the internal
 *   string used by the label, and must not be modified.
 **/
const char *
gtk_label_get_text (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->text;
}

/**
 * gtk_label_set_justify: (attributes org.gtk.Method.set_property=justify)
 * @self: a `GtkLabel`
 * @jtype: a `GtkJustification`
 *
 * Sets the alignment of the lines in the text of the label relative to
 * each other.
 *
 * %GTK_JUSTIFY_LEFT is the default value when the widget is first created
 * with [ctor@Gtk.Label.new]. If you instead want to set the alignment of
 * the label as a whole, use [method@Gtk.Widget.set_halign] instead.
 * [method@Gtk.Label.set_justify] has no effect on labels containing
 * only a single line.
 */
void
gtk_label_set_justify (GtkLabel        *self,
                       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (self));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);

  if ((GtkJustification) self->jtype != jtype)
    {
      self->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (self);

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_JUSTIFY]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_justify: (attributes org.gtk.Method.get_property=justify)
 * @self: a `GtkLabel`
 *
 * Returns the justification of the label.
 *
 * See [method@Gtk.Label.set_justify].
 *
 * Returns: `GtkJustification`
 **/
GtkJustification
gtk_label_get_justify (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), 0);

  return self->jtype;
}

/**
 * gtk_label_set_ellipsize: (attributes org.gtk.Method.set_property=ellipsize)
 * @self: a `GtkLabel`
 * @mode: a `PangoEllipsizeMode`
 *
 * Sets the mode used to ellipsize the text.
 *
 * The text will be ellipsized if there is not enough space
 * to render the entire string.
 */
void
gtk_label_set_ellipsize (GtkLabel          *self,
                         PangoEllipsizeMode mode)
{
  g_return_if_fail (GTK_IS_LABEL (self));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE && mode <= PANGO_ELLIPSIZE_END);

  if ((PangoEllipsizeMode) self->ellipsize != mode)
    {
      self->ellipsize = mode;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (self);

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_ELLIPSIZE]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_ellipsize: (attributes org.gtk.Method.get_property=ellipsize)
 * @self: a `GtkLabel`
 *
 * Returns the ellipsizing position of the label.
 *
 * See [method@Gtk.Label.set_ellipsize].
 *
 * Returns: `PangoEllipsizeMode`
 **/
PangoEllipsizeMode
gtk_label_get_ellipsize (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), PANGO_ELLIPSIZE_NONE);

  return self->ellipsize;
}

/**
 * gtk_label_set_width_chars: (attributes org.gtk.Method.set_property=width-chars)
 * @self: a `GtkLabel`
 * @n_chars: the new desired width, in characters.
 *
 * Sets the desired width in characters of @label to @n_chars.
 */
void
gtk_label_set_width_chars (GtkLabel *self,
                           int       n_chars)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->width_chars != n_chars)
    {
      self->width_chars = n_chars;
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_WIDTH_CHARS]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_width_chars: (attributes org.gtk.Method.get_property=width-chars)
 * @self: a `GtkLabel`
 *
 * Retrieves the desired width of @label, in characters.
 *
 * See [method@Gtk.Label.set_width_chars].
 *
 * Returns: the width of the label in characters.
 */
int
gtk_label_get_width_chars (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), -1);

  return self->width_chars;
}

/**
 * gtk_label_set_max_width_chars: (attributes org.gtk.Method.set_property=max-width-chars)
 * @self: a `GtkLabel`
 * @n_chars: the new desired maximum width, in characters.
 *
 * Sets the desired maximum width in characters of @label to @n_chars.
 */
void
gtk_label_set_max_width_chars (GtkLabel *self,
                               int       n_chars)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->max_width_chars != n_chars)
    {
      self->max_width_chars = n_chars;

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_MAX_WIDTH_CHARS]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_max_width_chars: (attributes org.gtk.Method.get_property=max-width-chars)
 * @self: a `GtkLabel`
 *
 * Retrieves the desired maximum width of @label, in characters.
 *
 * See [method@Gtk.Label.set_width_chars].
 *
 * Returns: the maximum width of the label in characters.
 **/
int
gtk_label_get_max_width_chars (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), -1);

  return self->max_width_chars;
}

/**
 * gtk_label_set_wrap: (attributes org.gtk.Method.set_property=wrap)
 * @self: a `GtkLabel`
 * @wrap: the setting
 *
 * Toggles line wrapping within the `GtkLabel` widget.
 *
 * %TRUE makes it break lines if text exceeds the widget’s size.
 * %FALSE lets the text get cut off by the edge of the widget if
 * it exceeds the widget size.
 *
 * Note that setting line wrapping to %TRUE does not make the label
 * wrap at its parent container’s width, because GTK widgets
 * conceptually can’t make their requisition depend on the parent
 * container’s size. For a label that wraps at a specific position,
 * set the label’s width using [method@Gtk.Widget.set_size_request].
 */
void
gtk_label_set_wrap (GtkLabel *self,
                    gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  wrap = wrap != FALSE;

  if (self->wrap != wrap)
    {
      self->wrap = wrap;

      gtk_label_clear_layout (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_WRAP]);
    }
}

/**
 * gtk_label_get_wrap: (attributes org.gtk.Method.get_property=wrap)
 * @self: a `GtkLabel`
 *
 * Returns whether lines in the label are automatically wrapped.
 *
 * See [method@Gtk.Label.set_wrap].
 *
 * Returns: %TRUE if the lines of the label are automatically wrapped.
 */
gboolean
gtk_label_get_wrap (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  return self->wrap;
}

/**
 * gtk_label_set_wrap_mode: (attributes org.gtk.Method.set_property=wrap-mode)
 * @self: a `GtkLabel`
 * @wrap_mode: the line wrapping mode
 *
 * Controls how line wrapping is done.
 *
 * This only affects the label if line wrapping is on. (See
 * [method@Gtk.Label.set_wrap]) The default is %PANGO_WRAP_WORD
 * which means wrap on word boundaries.
 *
 * For sizing behavior, also consider the [property@Gtk.Label:natural-wrap-mode]
 * property.
 */
void
gtk_label_set_wrap_mode (GtkLabel *self,
                         PangoWrapMode wrap_mode)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->wrap_mode != wrap_mode)
    {
      self->wrap_mode = wrap_mode;
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_WRAP_MODE]);

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_wrap_mode: (attributes org.gtk.Method.get_property=wrap-mode)
 * @self: a `GtkLabel`
 *
 * Returns line wrap mode used by the label.
 *
 * See [method@Gtk.Label.set_wrap_mode].
 *
 * Returns: the line wrap mode
 */
PangoWrapMode
gtk_label_get_wrap_mode (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), PANGO_WRAP_WORD);

  return self->wrap_mode;
}

/**
 * gtk_label_set_natural_wrap_mode: (attributes org.gtk.Method.set_property=natural-wrap-mode)
 * @self: a `GtkLabel`
 * @wrap_mode: the line wrapping mode
 *
 * Select the line wrapping for the natural size request.
 *
 * This only affects the natural size requested, for the actual wrapping used,
 * see the [property@Gtk.Label:wrap-mode] property.
 *
 * Since: 4.6
 */
void
gtk_label_set_natural_wrap_mode (GtkLabel           *self,
                                 GtkNaturalWrapMode  wrap_mode)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->natural_wrap_mode != wrap_mode)
    {
      self->natural_wrap_mode = wrap_mode;
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_NATURAL_WRAP_MODE]);

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_natural_wrap_mode: (attributes org.gtk.Method.get_property=natural-wrap-mode)
 * @self: a `GtkLabel`
 *
 * Returns line wrap mode used by the label.
 *
 * See [method@Gtk.Label.set_natural_wrap_mode].
 *
 * Returns: the natural line wrap mode
 *
 * Since: 4.6
 */
GtkNaturalWrapMode
gtk_label_get_natural_wrap_mode (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), GTK_NATURAL_WRAP_INHERIT);

  return self->natural_wrap_mode;
}

static void
gtk_label_clear_layout (GtkLabel *self)
{
  g_clear_object (&self->layout);
}

static void
gtk_label_ensure_layout (GtkLabel *self)
{
  PangoAlignment align;
  gboolean rtl;

  if (self->layout)
    return;

  rtl = _gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), self->text);

  gtk_label_update_layout_attributes (self, NULL);

  switch (self->jtype)
    {
    case GTK_JUSTIFY_LEFT:
      align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
      break;
    case GTK_JUSTIFY_RIGHT:
      align = rtl ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
      break;
    case GTK_JUSTIFY_CENTER:
      align = PANGO_ALIGN_CENTER;
      break;
    case GTK_JUSTIFY_FILL:
      align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
      pango_layout_set_justify (self->layout, TRUE);
      break;
    default:
      g_assert_not_reached();
    }

  pango_layout_set_alignment (self->layout, align);
  pango_layout_set_ellipsize (self->layout, self->ellipsize);
  pango_layout_set_wrap (self->layout, self->wrap_mode);
  pango_layout_set_single_paragraph_mode (self->layout, self->single_line_mode);
  if (self->lines > 0)
    pango_layout_set_height (self->layout, - self->lines);

  if (self->ellipsize || self->wrap)
    pango_layout_set_width (self->layout, gtk_widget_get_width (GTK_WIDGET (self)) * PANGO_SCALE);

  pango_layout_set_tabs (self->layout, self->tabs);
}

/**
 * gtk_label_set_text_with_mnemonic:
 * @self: a `GtkLabel`
 * @str: a string
 *
 * Sets the label’s text from the string @str.
 *
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 * The mnemonic key can be used to activate another widget, chosen
 * automatically, or explicitly using [method@Gtk.Label.set_mnemonic_widget].
 */
void
gtk_label_set_text_with_mnemonic (GtkLabel    *self,
                                  const char *str)
{
  gboolean changed;

  g_return_if_fail (GTK_IS_LABEL (self));
  g_return_if_fail (str != NULL);

  g_object_freeze_notify (G_OBJECT (self));

  changed = gtk_label_set_label_internal (self, str);
  changed = gtk_label_set_use_markup_internal (self, FALSE) || changed;
  changed = gtk_label_set_use_underline_internal (self, TRUE) || changed;

  if (changed)
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

static int
gtk_label_move_forward_word (GtkLabel *self,
                             int       start)
{
  int new_pos = g_utf8_pointer_to_offset (self->text, self->text + start);
  int length;

  length = g_utf8_strlen (self->text, -1);
  if (new_pos < length)
    {
      const PangoLogAttr *log_attrs;
      int n_attrs;

      gtk_label_ensure_layout (self);

      log_attrs = pango_layout_get_log_attrs_readonly (self->layout, &n_attrs);

      /* Find the next word end */
      new_pos++;
      while (new_pos < n_attrs && !log_attrs[new_pos].is_word_end)
        new_pos++;
    }

  return g_utf8_offset_to_pointer (self->text, new_pos) - self->text;
}

static int
gtk_label_move_backward_word (GtkLabel *self,
                              int       start)
{
  int new_pos = g_utf8_pointer_to_offset (self->text, self->text + start);

  if (new_pos > 0)
    {
      const PangoLogAttr *log_attrs;
      int n_attrs;

      gtk_label_ensure_layout (self);

      log_attrs = pango_layout_get_log_attrs_readonly (self->layout, &n_attrs);

      new_pos -= 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
        new_pos--;
    }

  return g_utf8_offset_to_pointer (self->text, new_pos) - self->text;
}

static void
gtk_label_select_word (GtkLabel *self)
{
  int min, max;

  int start_index = gtk_label_move_backward_word (self, self->select_info->selection_end);
  int end_index = gtk_label_move_forward_word (self, self->select_info->selection_end);

  min = MIN (self->select_info->selection_anchor,
             self->select_info->selection_end);
  max = MAX (self->select_info->selection_anchor,
             self->select_info->selection_end);

  min = MIN (min, start_index);
  max = MAX (max, end_index);

  gtk_label_select_region_index (self, min, max);
}

static void
gtk_label_click_gesture_pressed (GtkGestureClick *gesture,
                                 int              n_press,
                                 double           widget_x,
                                 double           widget_y,
                                 GtkLabel        *self)
{
  GtkLabelSelectionInfo *info = self->select_info;
  GtkWidget *widget = GTK_WIDGET (self);
  GdkEventSequence *sequence;
  GdkEvent *event;
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  gtk_label_update_active_link (widget, widget_x, widget_y);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (info->active_link)
    {
      if (gdk_event_triggers_context_menu (event))
        {
          info->link_clicked = TRUE;
          update_link_state (self);
          gtk_label_do_popup (self, widget_x, widget_y);
          return;
        }
      else if (button == GDK_BUTTON_PRIMARY)
        {
          info->link_clicked = TRUE;
          update_link_state (self);
          gtk_widget_queue_draw (widget);
          if (!info->selectable)
            return;
        }
    }

  if (!info->selectable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  info->in_drag = FALSE;
  info->select_words = FALSE;

  if (gdk_event_triggers_context_menu (event))
    gtk_label_do_popup (self, widget_x, widget_y);
  else if (button == GDK_BUTTON_PRIMARY)
    {
      if (!gtk_widget_has_focus (widget))
        {
          self->in_click = TRUE;
          gtk_widget_grab_focus (widget);
          self->in_click = FALSE;
        }

      if (n_press == 3)
        gtk_label_select_region_index (self, 0, strlen (self->text));
      else if (n_press == 2)
        {
          info->select_words = TRUE;
          gtk_label_select_word (self);
        }
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_label_click_gesture_released (GtkGestureClick *gesture,
                                  int              n_press,
                                  double           x,
                                  double           y,
                                  GtkLabel        *self)
{
  GtkLabelSelectionInfo *info = self->select_info;
  GdkEventSequence *sequence;
  int index;

  if (info == NULL)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  if (n_press != 1)
    return;

  if (info->in_drag)
    {
      info->in_drag = FALSE;
      get_layout_index (self, x, y, &index);
      gtk_label_select_region_index (self, index, index);
    }
  else if (info->active_link &&
           info->selection_anchor == info->selection_end &&
           info->link_clicked)
    {
      emit_activate_link (self, info->active_link);
      info->link_clicked = FALSE;
    }
}

static GdkPaintable *
get_selection_paintable (GtkLabel *self)
{
  if ((self->select_info->selection_anchor !=
       self->select_info->selection_end) &&
      self->text)
    {
      int start, end;
      int len;

      start = MIN (self->select_info->selection_anchor,
                   self->select_info->selection_end);
      end = MAX (self->select_info->selection_anchor,
                 self->select_info->selection_end);

      len = strlen (self->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      return gtk_text_util_create_drag_icon (GTK_WIDGET (self), self->text + start, end - start);
    }

  return NULL;
}

static void
gtk_label_drag_gesture_begin (GtkGestureDrag *gesture,
                              double          start_x,
                              double          start_y,
                              GtkLabel       *self)
{
  GtkLabelSelectionInfo *info = self->select_info;
  GdkModifierType state_mask;
  GdkEventSequence *sequence;
  GdkEvent *event;
  int min, max, index;

  if (!info || !info->selectable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  get_layout_index (self, start_x, start_y, &index);
  min = MIN (info->selection_anchor, info->selection_end);
  max = MAX (info->selection_anchor, info->selection_end);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  state_mask = gdk_event_get_modifier_state (event);

  if ((info->selection_anchor != info->selection_end) &&
      ((state_mask & GDK_SHIFT_MASK) != 0))
    {
      if (index > min && index < max)
        {
          /* truncate selection, but keep it as big as possible */
          if (index - min > max - index)
            max = index;
          else
            min = index;
        }
      else
        {
          /* extend (same as motion) */
          min = MIN (min, index);
          max = MAX (max, index);
        }

      /* ensure the anchor is opposite index */
      if (index == min)
        {
          int tmp = min;
          min = max;
          max = tmp;
        }

      gtk_label_select_region_index (self, min, max);
    }
  else
    {
      if (min < max && min <= index && index <= max)
        {
          if (!info->select_words)
            info->in_drag = TRUE;
          info->drag_start_x = start_x;
          info->drag_start_y = start_y;
        }
      else
        /* start a replacement */
        gtk_label_select_region_index (self, index, index);
    }
}

static void
gtk_label_drag_gesture_update (GtkGestureDrag *gesture,
                               double          offset_x,
                               double          offset_y,
                               GtkLabel       *self)
{
  GtkLabelSelectionInfo *info = self->select_info;
  GtkWidget *widget = GTK_WIDGET (self);
  GdkEventSequence *sequence;
  double x, y;
  int index;

  if (info == NULL || !info->selectable)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &x, &y);

  if (info->in_drag)
    {
      if (gtk_drag_check_threshold_double (widget, info->drag_start_x, info->drag_start_y, x, y))
        {
          GdkDrag *drag;
          GdkSurface *surface;
          GdkDevice *device;

          surface = gtk_native_get_surface (gtk_widget_get_native (widget));
          device = gtk_gesture_get_device (GTK_GESTURE (gesture));

          drag = gdk_drag_begin (surface,
                                 device,
                                 info->provider,
                                 GDK_ACTION_COPY,
                                 info->drag_start_x,
                                 info->drag_start_y);

          gtk_drag_icon_set_from_paintable (drag, get_selection_paintable (self), 0, 0);

          g_object_unref (drag);
          info->in_drag = FALSE;
        }
    }
  else
    {
      get_layout_index (self, x, y, &index);

      if (index != info->selection_anchor)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      if (info->select_words)
        {
          int min, max;
          int old_min, old_max;
          int anchor, end;

          min = gtk_label_move_backward_word (self, index);
          max = gtk_label_move_forward_word (self, index);

          anchor = info->selection_anchor;
          end = info->selection_end;

          old_min = MIN (anchor, end);
          old_max = MAX (anchor, end);

          if (min < old_min)
            {
              anchor = min;
              end = old_max;
            }
          else if (old_max < max)
            {
              anchor = max;
              end = old_min;
            }
          else if (anchor == old_min)
            {
              if (anchor != min)
                anchor = max;
            }
          else
            {
              if (anchor != max)
                anchor = min;
            }

          gtk_label_select_region_index (self, anchor, end);
        }
      else
        gtk_label_select_region_index (self, info->selection_anchor, index);
    }
}

static void
gtk_label_update_actions (GtkLabel *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  gboolean has_selection;
  GtkLabelLink *link;

  if (self->select_info)
    {
      has_selection = self->select_info->selection_anchor != self->select_info->selection_end;
      link = self->select_info->active_link;
    }
  else
    {
      has_selection = FALSE;
      link = gtk_label_get_focus_link (self, NULL);
    }

  gtk_widget_action_set_enabled (widget, "clipboard.cut", FALSE);
  gtk_widget_action_set_enabled (widget, "clipboard.copy", has_selection);
  gtk_widget_action_set_enabled (widget, "clipboard.paste", FALSE);
  gtk_widget_action_set_enabled (widget, "selection.select-all",
                                 gtk_label_get_selectable (self));
  gtk_widget_action_set_enabled (widget, "selection.delete", FALSE);
  gtk_widget_action_set_enabled (widget, "link.open", !has_selection && link);
  gtk_widget_action_set_enabled (widget, "link.copy", !has_selection && link);
}

static void
gtk_label_update_active_link (GtkWidget *widget,
                              double     x,
                              double     y)
{
  GtkLabel *self = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = self->select_info;
  int index;

  if (info == NULL)
    return;

  if (info->links && !info->in_drag)
    {
      GtkLabelLink *link;
      gboolean found = FALSE;

      if (info->selection_anchor == info->selection_end)
        {
          if (get_layout_index (self, x, y, &index))
            {
              const int link_index = _gtk_label_get_link_at (self, index);

              if (link_index != -1)
                {
                  link = &info->links[link_index];

                  if (!range_is_in_ellipsis (self, link->start, link->end))
                    found = TRUE;
                }
            }
        }

      if (found)
        {
          if (info->active_link != link)
            {
              info->link_clicked = FALSE;
              info->active_link = link;
              update_link_state (self);
              gtk_label_update_cursor (self);
              gtk_widget_queue_draw (widget);
            }
        }
      else
        {
          if (info->active_link != NULL)
            {
              info->link_clicked = FALSE;
              info->active_link = NULL;
              update_link_state (self);
              gtk_label_update_cursor (self);
              gtk_widget_queue_draw (widget);
            }
        }

      gtk_label_update_actions (self);
    }
}

static void
gtk_label_motion (GtkEventControllerMotion *controller,
                  double                    x,
                  double                    y,
                  gpointer                  data)
{
  gtk_label_update_active_link (GTK_WIDGET (data), x, y);
}

static void
gtk_label_leave (GtkEventControllerMotion *controller,
                 gpointer                  data)
{
  GtkLabel *self = GTK_LABEL (data);

  if (self->select_info)
    {
      self->select_info->active_link = NULL;
      gtk_label_update_cursor (self);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

#define GTK_TYPE_LABEL_CONTENT            (gtk_label_content_get_type ())
#define GTK_LABEL_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LABEL_CONTENT, GtkLabelContent))
#define GTK_IS_LABEL_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LABEL_CONTENT))
#define GTK_LABEL_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LABEL_CONTENT, GtkLabelContentClass))
#define GTK_IS_LABEL_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LABEL_CONTENT))
#define GTK_LABEL_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LABEL_CONTENT, GtkLabelContentClass))

typedef struct _GtkLabelContent GtkLabelContent;
typedef struct _GtkLabelContentClass GtkLabelContentClass;

struct _GtkLabelContent
{
  GdkContentProvider parent;

  GtkLabel *label;
};

struct _GtkLabelContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_label_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkLabelContent, gtk_label_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_label_content_ref_formats (GdkContentProvider *provider)
{
  GtkLabelContent *content = GTK_LABEL_CONTENT (provider);

  if (content->label)
    return gdk_content_formats_new_for_gtype (G_TYPE_STRING);
  else
    return gdk_content_formats_new (NULL, 0);
}

static gboolean
gtk_label_content_get_value (GdkContentProvider  *provider,
                             GValue              *value,
                             GError             **error)
{
  GtkLabelContent *content = GTK_LABEL_CONTENT (provider);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING) &&
      content->label != NULL)
    {
      GtkLabel *self = content->label;

      if (self->select_info &&
          (self->select_info->selection_anchor !=
           self->select_info->selection_end) &&
          self->text)
        {
          int start, end;
          int len;
          char *str;

          start = MIN (self->select_info->selection_anchor,
                       self->select_info->selection_end);
          end = MAX (self->select_info->selection_anchor,
                     self->select_info->selection_end);

          len = strlen (self->text);

          if (end > len)
            end = len;

          if (start > len)
            start = len;

          str = g_strndup (self->text + start, end - start);
          g_value_take_string (value, str);
          return TRUE;
        }
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_label_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_label_content_detach (GdkContentProvider *provider,
                          GdkClipboard       *clipboard)
{
  GtkLabelContent *content = GTK_LABEL_CONTENT (provider);
  GtkLabel *self = content->label;

  if (self == NULL || self->select_info == NULL)
    return;

  self->select_info->selection_anchor = self->select_info->selection_end;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_label_content_class_init (GtkLabelContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_label_content_ref_formats;
  provider_class->get_value = gtk_label_content_get_value;
  provider_class->detach_clipboard = gtk_label_content_detach;
}

static void
gtk_label_content_init (GtkLabelContent *content)
{
}

static void
focus_change (GtkEventControllerFocus *controller,
              GtkLabel                *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_label_ensure_select_info (GtkLabel *self)
{
  if (self->select_info == NULL)
    {
      self->select_info = g_new0 (GtkLabelSelectionInfo, 1);

      gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

      self->select_info->drag_gesture = gtk_gesture_drag_new ();
      g_signal_connect (self->select_info->drag_gesture, "drag-begin",
                        G_CALLBACK (gtk_label_drag_gesture_begin), self);
      g_signal_connect (self->select_info->drag_gesture, "drag-update",
                        G_CALLBACK (gtk_label_drag_gesture_update), self);
      gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->select_info->drag_gesture), TRUE);
      gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->select_info->drag_gesture));

      self->select_info->click_gesture = gtk_gesture_click_new ();
      g_signal_connect (self->select_info->click_gesture, "pressed",
                        G_CALLBACK (gtk_label_click_gesture_pressed), self);
      g_signal_connect (self->select_info->click_gesture, "released",
                        G_CALLBACK (gtk_label_click_gesture_released), self);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->select_info->click_gesture), 0);
      gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->select_info->click_gesture), TRUE);
      gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->select_info->click_gesture));

      self->select_info->motion_controller = gtk_event_controller_motion_new ();
      g_signal_connect (self->select_info->motion_controller, "motion",
                        G_CALLBACK (gtk_label_motion), self);
      g_signal_connect (self->select_info->motion_controller, "leave",
                        G_CALLBACK (gtk_label_leave), self);
      gtk_widget_add_controller (GTK_WIDGET (self), self->select_info->motion_controller);

      self->select_info->focus_controller = gtk_event_controller_focus_new ();
      g_signal_connect (self->select_info->focus_controller, "enter",
                        G_CALLBACK (focus_change), self);
      g_signal_connect (self->select_info->focus_controller, "leave",
                        G_CALLBACK (focus_change), self);
      gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->select_info->focus_controller));

      self->select_info->provider = g_object_new (GTK_TYPE_LABEL_CONTENT, NULL);
      GTK_LABEL_CONTENT (self->select_info->provider)->label = self;

      gtk_label_update_cursor (self);
    }
}

static void
gtk_label_clear_select_info (GtkLabel *self)
{
  if (self->select_info == NULL)
    return;

  if (!self->select_info->selectable && !self->select_info->links)
    {
      gtk_widget_remove_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->select_info->drag_gesture));
      gtk_widget_remove_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->select_info->click_gesture));
      gtk_widget_remove_controller (GTK_WIDGET (self), self->select_info->motion_controller);
      gtk_widget_remove_controller (GTK_WIDGET (self), self->select_info->focus_controller);
      GTK_LABEL_CONTENT (self->select_info->provider)->label = NULL;
      g_object_unref (self->select_info->provider);

      g_free (self->select_info);
      self->select_info = NULL;

      gtk_widget_set_cursor (GTK_WIDGET (self), NULL);

      gtk_widget_set_focusable (GTK_WIDGET (self), FALSE);
    }
}

/**
 * gtk_label_set_selectable: (attributes org.gtk.Method.set_property=selectable)
 * @self: a `GtkLabel`
 * @setting: %TRUE to allow selecting text in the label
 *
 * Makes text in the label selectable.
 *
 * Selectable labels allow the user to select text from the label,
 * for copy-and-paste.
 */
void
gtk_label_set_selectable (GtkLabel *self,
                          gboolean  setting)
{
  gboolean old_setting;

  g_return_if_fail (GTK_IS_LABEL (self));

  setting = setting != FALSE;
  old_setting = self->select_info && self->select_info->selectable;

  if (setting)
    {
      gtk_label_ensure_select_info (self);
      self->select_info->selectable = TRUE;
      gtk_label_update_cursor (self);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                      -1);
    }
  else
    {
      if (old_setting)
        {
          /* unselect, to give up the selection */
          gtk_label_select_region (self, 0, 0);

          self->select_info->selectable = FALSE;
          gtk_label_clear_select_info (self);
        }

      gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_HAS_POPUP);
    }

  if (setting != old_setting)
    {
      g_object_freeze_notify (G_OBJECT (self));
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_SELECTABLE]);
      g_object_thaw_notify (G_OBJECT (self));
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_selectable: (attributes org.gtk.Method.get_property=selectable)
 * @self: a `GtkLabel`
 *
 * Returns whether the label is selectable.
 *
 * Returns: %TRUE if the user can copy text from the label
 */
gboolean
gtk_label_get_selectable (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  return self->select_info && self->select_info->selectable;
}

static void
gtk_label_select_region_index (GtkLabel *self,
                               int       anchor_index,
                               int       end_index)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->select_info && self->select_info->selectable)
    {
      GdkClipboard *clipboard;
      int s, e;

      /* Ensure that we treat an ellipsized region like a single
       * character with respect to selection.
       */
      if (anchor_index < end_index)
        {
          if (range_is_in_ellipsis_full (self, anchor_index, anchor_index + 1, &s, &e))
            {
              if (self->select_info->selection_anchor == s)
                anchor_index = e;
              else
                anchor_index = s;
            }
          if (range_is_in_ellipsis_full (self, end_index - 1, end_index, &s, &e))
            {
              if (self->select_info->selection_end == e)
                end_index = s;
              else
                end_index = e;
            }
        }
      else if (end_index < anchor_index)
        {
          if (range_is_in_ellipsis_full (self, end_index, end_index + 1, &s, &e))
            {
              if (self->select_info->selection_end == s)
                end_index = e;
              else
                end_index = s;
            }
          if (range_is_in_ellipsis_full (self, anchor_index - 1, anchor_index, &s, &e))
            {
              if (self->select_info->selection_anchor == e)
                anchor_index = s;
              else
                anchor_index = e;
            }
        }
      else
        {
          if (range_is_in_ellipsis_full (self, anchor_index, anchor_index, &s, &e))
            {
              if (self->select_info->selection_anchor == s)
                anchor_index = e;
              else if (self->select_info->selection_anchor == e)
                anchor_index = s;
              else if (anchor_index - s < e - anchor_index)
                anchor_index = s;
              else
                anchor_index = e;
              end_index = anchor_index;
            }
        }

      if (self->select_info->selection_anchor == anchor_index &&
          self->select_info->selection_end == end_index)
        return;

      g_object_freeze_notify (G_OBJECT (self));

      self->select_info->selection_anchor = anchor_index;
      self->select_info->selection_end = end_index;

      clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (self));

      if (anchor_index != end_index)
        {
          gdk_content_provider_content_changed (self->select_info->provider);
          gdk_clipboard_set_content (clipboard, self->select_info->provider);

          if (!self->select_info->selection_node)
            {
              GtkCssNode *widget_node;

              widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
              self->select_info->selection_node = gtk_css_node_new ();
              gtk_css_node_set_name (self->select_info->selection_node, g_quark_from_static_string ("selection"));
              gtk_css_node_set_parent (self->select_info->selection_node, widget_node);
              gtk_css_node_set_state (self->select_info->selection_node, gtk_css_node_get_state (widget_node));
              g_object_unref (self->select_info->selection_node);
            }
        }
      else
        {
          if (gdk_clipboard_get_content (clipboard) == self->select_info->provider)
            gdk_clipboard_set_content (clipboard, NULL);

          if (self->select_info->selection_node)
            {
              gtk_css_node_set_parent (self->select_info->selection_node, NULL);
              self->select_info->selection_node = NULL;
            }
        }


      gtk_label_update_actions (self);

      gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (self));
      gtk_accessible_text_update_selection_bound (GTK_ACCESSIBLE_TEXT (self));

      gtk_widget_queue_draw (GTK_WIDGET (self));

      g_object_thaw_notify (G_OBJECT (self));
    }
}

/**
 * gtk_label_select_region:
 * @self: a `GtkLabel`
 * @start_offset: start offset (in characters not bytes)
 * @end_offset: end offset (in characters not bytes)
 *
 * Selects a range of characters in the label, if the label is selectable.
 *
 * See [method@Gtk.Label.set_selectable]. If the label is not selectable,
 * this function has no effect. If @start_offset or
 * @end_offset are -1, then the end of the label will be substituted.
 */
void
gtk_label_select_region  (GtkLabel *self,
                          int       start_offset,
                          int       end_offset)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->text && self->select_info)
    {
      if (start_offset < 0)
        start_offset = g_utf8_strlen (self->text, -1);

      if (end_offset < 0)
        end_offset = g_utf8_strlen (self->text, -1);

      gtk_label_select_region_index (self,
                                     g_utf8_offset_to_pointer (self->text, start_offset) - self->text,
                                     g_utf8_offset_to_pointer (self->text, end_offset) - self->text);
    }
}

/**
 * gtk_label_get_selection_bounds:
 * @self: a `GtkLabel`
 * @start: (out) (optional): return location for start of selection, as a character offset
 * @end: (out) (optional): return location for end of selection, as a character offset
 *
 * Gets the selected range of characters in the label.
 *
 * Returns: %TRUE if selection is non-empty
 **/
gboolean
gtk_label_get_selection_bounds (GtkLabel  *self,
                                int       *start,
                                int       *end)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  if (self->select_info == NULL)
    {
      /* not a selectable label */
      if (start)
        *start = 0;
      if (end)
        *end = 0;

      return FALSE;
    }
  else
    {
      int start_index, end_index;
      int start_offset, end_offset;
      int len;

      start_index = MIN (self->select_info->selection_anchor,
                   self->select_info->selection_end);
      end_index = MAX (self->select_info->selection_anchor,
                 self->select_info->selection_end);

      len = strlen (self->text);

      if (end_index > len)
        end_index = len;

      if (start_index > len)
        start_index = len;

      start_offset = g_utf8_strlen (self->text, start_index);
      end_offset = g_utf8_strlen (self->text, end_index);

      if (start_offset > end_offset)
        {
          int tmp = start_offset;
          start_offset = end_offset;
          end_offset = tmp;
        }

      if (start)
        *start = start_offset;

      if (end)
        *end = end_offset;

      return start_offset != end_offset;
    }
}


/**
 * gtk_label_get_layout:
 * @self: a `GtkLabel`
 *
 * Gets the `PangoLayout` used to display the label.
 *
 * The layout is useful to e.g. convert text positions to pixel
 * positions, in combination with [method@Gtk.Label.get_layout_offsets].
 * The returned layout is owned by the @label so need not be
 * freed by the caller. The @label is free to recreate its layout
 * at any time, so it should be considered read-only.
 *
 * Returns: (transfer none): the [class@Pango.Layout] for this label
 */
PangoLayout*
gtk_label_get_layout (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  gtk_label_ensure_layout (self);

  return self->layout;
}

/**
 * gtk_label_get_layout_offsets:
 * @self: a `GtkLabel`
 * @x: (out) (optional): location to store X offset of layout
 * @y: (out) (optional): location to store Y offset of layout
 *
 * Obtains the coordinates where the label will draw its `PangoLayout`.
 *
 * The coordinates are useful to convert mouse events into coordinates
 * inside the [class@Pango.Layout], e.g. to take some action if some part
 * of the label is clicked. Remember when using the [class@Pango.Layout]
 * functions you need to convert to and from pixels using PANGO_PIXELS()
 * or [const@Pango.SCALE].
 */
void
gtk_label_get_layout_offsets (GtkLabel *self,
                              int      *x,
                              int      *y)
{
  float local_x, local_y;
  g_return_if_fail (GTK_IS_LABEL (self));

  gtk_label_ensure_layout (self);
  get_layout_location (self, &local_x, &local_y);

  if (x)
    *x = (int) local_x;

  if (y)
    *y = (int) local_y;
}

/**
 * gtk_label_set_use_markup: (attributes org.gtk.Method.set_property=use-markup)
 * @self: a `GtkLabel`
 * @setting: %TRUE if the label’s text should be parsed for markup.
 *
 * Sets whether the text of the label contains markup.
 *
 * See [method@Gtk.Label.set_markup].
 */
void
gtk_label_set_use_markup (GtkLabel *self,
                          gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (gtk_label_set_use_markup_internal (self, !!setting))
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_get_use_markup: (attributes org.gtk.Method.get_property=use-markup)
 * @self: a `GtkLabel`
 *
 * Returns whether the label’s text is interpreted as Pango markup.
 *
 * See [method@Gtk.Label.set_use_markup].
 *
 * Returns: %TRUE if the label’s text will be parsed for markup.
 */
gboolean
gtk_label_get_use_markup (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  return self->use_markup;
}

/**
 * gtk_label_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @self: a `GtkLabel`
 * @setting: %TRUE if underlines in the text indicate mnemonics
 *
 * Sets whether underlines in the text indicate mnemonics.
 */
void
gtk_label_set_use_underline (GtkLabel *self,
                             gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (gtk_label_set_use_underline_internal (self, !!setting))
    gtk_label_recalculate (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_label_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @self: a `GtkLabel`
 *
 * Returns whether an embedded underlines in the label indicate mnemonics.
 *
 * See [method@Gtk.Label.set_use_underline].
 *
 * Returns: %TRUE whether an embedded underline in the label indicates
 *   the mnemonic accelerator keys.
 */
gboolean
gtk_label_get_use_underline (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  return self->use_underline;
}

/**
 * gtk_label_set_single_line_mode: (attributes org.gtk.Method.set_property=single-line-mode)
 * @self: a `GtkLabel`
 * @single_line_mode: %TRUE if the label should be in single line mode
 *
 * Sets whether the label is in single line mode.
 */
void
gtk_label_set_single_line_mode (GtkLabel *self,
                                gboolean single_line_mode)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  single_line_mode = single_line_mode != FALSE;

  if (self->single_line_mode != single_line_mode)
    {
      self->single_line_mode = single_line_mode;

      gtk_label_clear_layout (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));

      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_SINGLE_LINE_MODE]);
    }
}

/**
 * gtk_label_get_single_line_mode: (attributes org.gtk.Method.get_property=single-line-mode)
 * @self: a `GtkLabel`
 *
 * Returns whether the label is in single line mode.
 *
 * Returns: %TRUE when the label is in single line mode.
 **/
gboolean
gtk_label_get_single_line_mode  (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), FALSE);

  return self->single_line_mode;
}

/* Compute the X position for an offset that corresponds to the more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static void
get_better_cursor (GtkLabel *self,
                   int      index,
                   int      *x,
                   int      *y)
{
  GdkSeat *seat;
  GdkDevice *keyboard;
  PangoDirection keymap_direction;
  PangoDirection cursor_direction;
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  else
    keyboard = NULL;
  if (keyboard)
    keymap_direction = gdk_device_get_direction (keyboard);
  else
    keymap_direction = PANGO_DIRECTION_LTR;

  cursor_direction = get_cursor_direction (self);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-split-cursor", &split_cursor,
                NULL);

  gtk_label_ensure_layout (self);

  pango_layout_get_cursor_pos (self->layout, index,
                               &strong_pos, &weak_pos);

  if (split_cursor)
    {
      *x = strong_pos.x / PANGO_SCALE;
      *y = strong_pos.y / PANGO_SCALE;
    }
  else
    {
      if (keymap_direction == cursor_direction)
        {
          *x = strong_pos.x / PANGO_SCALE;
          *y = strong_pos.y / PANGO_SCALE;
        }
      else
        {
          *x = weak_pos.x / PANGO_SCALE;
          *y = weak_pos.y / PANGO_SCALE;
        }
    }
}


static int
gtk_label_move_logically (GtkLabel *self,
                          int       start,
                          int       count)
{
  int offset = g_utf8_pointer_to_offset (self->text, self->text + start);

  if (self->text)
    {
      const PangoLogAttr *log_attrs;
      int n_attrs;
      int length;

      gtk_label_ensure_layout (self);

      length = g_utf8_strlen (self->text, -1);

      log_attrs = pango_layout_get_log_attrs_readonly (self->layout, &n_attrs);

      while (count > 0 && offset < length)
        {
          do
            offset++;
          while (offset < length && !log_attrs[offset].is_cursor_position);

          count--;
        }
      while (count < 0 && offset > 0)
        {
          do
            offset--;
          while (offset > 0 && !log_attrs[offset].is_cursor_position);

          count++;
        }
    }

  return g_utf8_offset_to_pointer (self->text, offset) - self->text;
}

static int
gtk_label_move_visually (GtkLabel *self,
                         int       start,
                         int       count)
{
  int index;

  index = start;

  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      gtk_label_ensure_layout (self);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-split-cursor", &split_cursor,
                    NULL);

      if (split_cursor)
        strong = TRUE;
      else
        {
          GdkSeat *seat;
          GdkDevice *keyboard;
          PangoDirection keymap_direction;

          seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));
          if (seat)
            keyboard = gdk_seat_get_keyboard (seat);
          else
            keyboard = NULL;
          if (keyboard)
            keymap_direction = gdk_device_get_direction (keyboard);
          else
            keymap_direction = PANGO_DIRECTION_LTR;

          strong = keymap_direction == get_cursor_direction (self);
        }

      if (count > 0)
        {
          pango_layout_move_cursor_visually (self->layout, strong, index, 0, 1, &new_index, &new_trailing);
          count--;
        }
      else
        {
          pango_layout_move_cursor_visually (self->layout, strong, index, 0, -1, &new_index, &new_trailing);
          count++;
        }

      if (new_index < 0 || new_index == G_MAXINT)
        break;

      index = new_index;

      while (new_trailing--)
        index = g_utf8_next_char (self->text + new_index) - self->text;
    }

  return index;
}

static void
gtk_label_move_cursor (GtkLabel       *self,
                       GtkMovementStep step,
                       int             count,
                       gboolean        extend_selection)
{
  int old_pos;
  int new_pos;

  if (self->select_info == NULL)
    return;

  old_pos = new_pos = self->select_info->selection_end;

  if (self->select_info->selection_end != self->select_info->selection_anchor &&
      !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
        {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          {
            int end_x, end_y;
            int anchor_x, anchor_y;
            gboolean end_is_left;

            get_better_cursor (self, self->select_info->selection_end, &end_x, &end_y);
            get_better_cursor (self, self->select_info->selection_anchor, &anchor_x, &anchor_y);

            end_is_left = (end_y < anchor_y) || (end_y == anchor_y && end_x < anchor_x);

            if (count < 0)
              new_pos = end_is_left ? self->select_info->selection_end : self->select_info->selection_anchor;
            else
              new_pos = !end_is_left ? self->select_info->selection_end : self->select_info->selection_anchor;
            break;
          }
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
        case GTK_MOVEMENT_WORDS:
          if (count < 0)
            new_pos = MIN (self->select_info->selection_end, self->select_info->selection_anchor);
          else
            new_pos = MAX (self->select_info->selection_end, self->select_info->selection_anchor);
          break;
        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          /* FIXME: Can do better here */
          new_pos = count < 0 ? 0 : strlen (self->text);
          break;
        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
          break;
        }
    }
  else
    {
      switch (step)
        {
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
          new_pos = gtk_label_move_logically (self, new_pos, count);
          break;
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          new_pos = gtk_label_move_visually (self, new_pos, count);
          if (new_pos == old_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (self),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

                      if (root)
                        gtk_widget_child_focus (GTK_WIDGET (root), count > 0 ? GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (self));
                }
            }
          break;
        case GTK_MOVEMENT_WORDS:
          while (count > 0)
            {
              new_pos = gtk_label_move_forward_word (self, new_pos);
              count--;
            }
          while (count < 0)
            {
              new_pos = gtk_label_move_backward_word (self, new_pos);
              count++;
            }
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));
          break;
        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          /* FIXME: Can do better here */
          new_pos = count < 0 ? 0 : strlen (self->text);
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));
          break;
        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
          break;
        }
    }

  if (extend_selection)
    gtk_label_select_region_index (self,
                                   self->select_info->selection_anchor,
                                   new_pos);
  else
    gtk_label_select_region_index (self, new_pos, new_pos);
}

static GMenuModel *
gtk_label_get_menu_model (GtkLabel *self)
{
  GtkJoinedMenu *joined;
  GMenu *menu, *section;
  GMenuItem *item;

  joined = gtk_joined_menu_new ();
  menu = g_menu_new ();

  section = g_menu_new ();
  g_menu_append (section, _("Cu_t"), "clipboard.cut");
  g_menu_append (section, _("_Copy"), "clipboard.copy");
  g_menu_append (section, _("_Paste"), "clipboard.paste");
  g_menu_append (section, _("_Delete"), "selection.delete");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();
  g_menu_append (section, _("Select _All"), "selection.select-all");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();
  item = g_menu_item_new (_("_Open Link"), "link.open");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);
  item = g_menu_item_new (_("Copy _Link Address"), "link.copy");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  gtk_joined_menu_append_menu (joined, G_MENU_MODEL (menu));
  g_object_unref (menu);

  if (self->extra_menu)
    gtk_joined_menu_append_menu (joined, self->extra_menu);

  return G_MENU_MODEL (joined);
}

static void
gtk_label_do_popup (GtkLabel *self,
                    double    x,
                    double    y)
{
  if (!self->select_info)
    return;

  if (self->select_info->link_clicked)
    self->select_info->context_link = self->select_info->active_link;
  else
    self->select_info->context_link = gtk_label_get_focus_link (self, NULL);

  gtk_label_update_actions (self);

  if (!self->popup_menu)
    {
      GMenuModel *model;

      model = gtk_label_get_menu_model (self);
      self->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_widget_set_parent (self->popup_menu, GTK_WIDGET (self));
      gtk_popover_set_position (GTK_POPOVER (self->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (self->popup_menu), FALSE);
      gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_START);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self->popup_menu),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, _("Context menu"),
                                      -1);

      g_object_unref (model);
    }

  if (x != -1 && y != -1)
    {
      GdkRectangle rect = { x, y, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), &rect);
    }
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), NULL);

  gtk_popover_popup (GTK_POPOVER (self->popup_menu));
}

/**
 * gtk_label_get_current_uri:
 * @self: a `GtkLabel`
 *
 * Returns the URI for the currently active link in the label.
 *
 * The active link is the one under the mouse pointer or, in a
 * selectable label, the link in which the text cursor is currently
 * positioned.
 *
 * This function is intended for use in a [signal@Gtk.Label::activate-link]
 * handler or for use in a [signal@Gtk.Widget::query-tooltip] handler.
 *
 * Returns: (nullable): the currently active URI
 */
const char *
gtk_label_get_current_uri (GtkLabel *self)
{
  const GtkLabelLink *link;

  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  if (!self->select_info)
    return NULL;

  if (!self->select_info->link_clicked && self->select_info->selectable)
    link = gtk_label_get_focus_link (self, NULL);
  else
    link = self->select_info->active_link;

  if (link)
    return link->uri;

  return NULL;
}

int
_gtk_label_get_cursor_position (GtkLabel *self)
{
  if (self->select_info && self->select_info->selectable)
    return g_utf8_pointer_to_offset (self->text,
                                     self->text + self->select_info->selection_end);

  return 0;
}

int
_gtk_label_get_selection_bound (GtkLabel *self)
{
  if (self->select_info && self->select_info->selectable)
    return g_utf8_pointer_to_offset (self->text,
                                     self->text + self->select_info->selection_anchor);

  return 0;
}

/**
 * gtk_label_set_lines: (attributes org.gtk.Method.set_property=lines)
 * @self: a `GtkLabel`
 * @lines: the desired number of lines, or -1
 *
 * Sets the number of lines to which an ellipsized, wrapping label
 * should be limited.
 *
 * This has no effect if the label is not wrapping or ellipsized.
 * Set this to -1 if you don’t want to limit the number of lines.
 */
void
gtk_label_set_lines (GtkLabel *self,
                     int       lines)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->lines != lines)
    {
      self->lines = lines;
      gtk_label_clear_layout (self);
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_LINES]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_label_get_lines: (attributes org.gtk.Method.get_property=lines)
 * @self: a `GtkLabel`
 *
 * Gets the number of lines to which an ellipsized, wrapping
 * label should be limited.
 *
 * See [method@Gtk.Label.set_lines].
 *
 * Returns: The number of lines
 */
int
gtk_label_get_lines (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), -1);

  return self->lines;
}

/**
 * gtk_label_set_xalign: (attributes org.gtk.Method.set_property=xalign)
 * @self: a `GtkLabel`
 * @xalign: the new xalign value, between 0 and 1
 *
 * Sets the `xalign` of the label.
 *
 * See the [property@Gtk.Label:xalign] property.
 */
void
gtk_label_set_xalign (GtkLabel *self,
                      float     xalign)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (self->xalign == xalign)
    return;

  self->xalign = xalign;

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_XALIGN]);
}

/**
 * gtk_label_get_xalign: (attributes org.gtk.Method.get_property=xalign)
 * @self: a `GtkLabel`
 *
 * Gets the `xalign` of the label.
 *
 * See the [property@Gtk.Label:xalign] property.
 *
 * Returns: the xalign property
 */
float
gtk_label_get_xalign (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), 0.5);

  return self->xalign;
}

/**
 * gtk_label_set_yalign: (attributes org.gtk.Method.set_property=yalign)
 * @self: a `GtkLabel`
 * @yalign: the new yalign value, between 0 and 1
 *
 * Sets the `yalign` of the label.
 *
 * See the [property@Gtk.Label:yalign] property.
 */
void
gtk_label_set_yalign (GtkLabel *self,
                      float     yalign)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  yalign = CLAMP (yalign, 0.0, 1.0);

  if (self->yalign == yalign)
    return;

  self->yalign = yalign;

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_YALIGN]);
}

/**
 * gtk_label_get_yalign: (attributes org.gtk.Method.get_property=yalign)
 * @self: a `GtkLabel`
 *
 * Gets the `yalign` of the label.
 *
 * See the [property@Gtk.Label:yalign] property.
 *
 * Returns: the yalign property
 */
float
gtk_label_get_yalign (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), 0.5);

  return self->yalign;
}

/**
 * gtk_label_set_extra_menu: (attributes org.gtk.Method.set_property=extra-menu)
 * @self: a `GtkLabel`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model to add when constructing
 * the context menu for @label.
 */
void
gtk_label_set_extra_menu (GtkLabel   *self,
                          GMenuModel *model)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (g_set_object (&self->extra_menu, model))
    {
      g_clear_pointer (&self->popup_menu, gtk_widget_unparent);
      g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_EXTRA_MENU]);
    }
}

/**
 * gtk_label_get_extra_menu: (attributes org.gtk.Method.get_property=extra-menu)
 * @self: a `GtkLabel`
 *
 * Gets the extra menu model of @label.
 *
 * See [method@Gtk.Label.set_extra_menu].
 *
 * Returns: (transfer none) (nullable): the menu model
 */
GMenuModel *
gtk_label_get_extra_menu (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->extra_menu;
}

/**
 * gtk_label_set_tabs: (attributes org.gtk.Method.set_property=tabs)
 * @self: a `GtkLabel`
 * @tabs: (nullable): tabs as a `PangoTabArray`
 *
 * Sets the default tab stops for paragraphs in @self.
 *
 * Since: 4.8
 */
void
gtk_label_set_tabs (GtkLabel      *self,
                    PangoTabArray *tabs)
{
  g_return_if_fail (GTK_IS_LABEL (self));

  if (self->tabs == tabs)
    return;

  if (self->tabs)
    pango_tab_array_free (self->tabs);
  self->tabs = pango_tab_array_copy (tabs);

  gtk_label_clear_layout (self);
  g_object_notify_by_pspec (G_OBJECT (self), label_props[PROP_TABS]);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_label_get_tabs: (attributes org.gtk.Method.get_property=tabs)
 * @self: a `GtkLabel`
 *
 * Gets the tabs for @self.
 *
 * The returned array will be %NULL if “standard” (8-space) tabs are used.
 * Free the return value with [method@Pango.TabArray.free].
 *
 * Returns: (nullable) (transfer full): copy of default tab array,
 *   or %NULL if standard tabs are used; must be freed with
 *   [method@Pango.TabArray.free].
 *
 * Since: 4.8
 */
PangoTabArray *
gtk_label_get_tabs (GtkLabel *self)
{
  g_return_val_if_fail (GTK_IS_LABEL (self), NULL);

  return self->tabs ? pango_tab_array_copy (self->tabs) : NULL;
}

/* {{{ GtkAccessibleText implementation */

static GBytes *
gtk_label_accessible_text_get_contents (GtkAccessibleText *self,
                                        unsigned int       start,
                                        unsigned int       end)
{
  const char *text;
  int len;
  char *string;
  gsize size;

  text = gtk_label_get_text (GTK_LABEL (self));
  len = g_utf8_strlen (text, -1);

  start = CLAMP (start, 0, len);
  end = CLAMP (end, 0, len);

  if (end <= start)
    {
      string = g_strdup ("");
      size = 1;
    }
  else
    {
      const char *p, *q;
      p = g_utf8_offset_to_pointer (text, start);
      q = g_utf8_offset_to_pointer (text, end);
      size = q - p + 1;
      string = g_strndup (p, q - p);
    }

  return g_bytes_new_take (string, size);
}

static GBytes *
gtk_label_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                           unsigned int                  offset,
                                           GtkAccessibleTextGranularity  granularity,
                                           unsigned int                 *start,
                                           unsigned int                 *end)
{
  PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (self));
  char *string = gtk_pango_get_string_at (layout, offset, granularity, start, end);

  return g_bytes_new_take (string, strlen (string));
}

static unsigned
gtk_label_accessible_text_get_caret_position (GtkAccessibleText *self)
{
  return _gtk_label_get_cursor_position (GTK_LABEL (self));
}

static gboolean
gtk_label_accessible_text_get_selection (GtkAccessibleText       *self,
                                         gsize                   *n_ranges,
                                         GtkAccessibleTextRange **ranges)
{
  int start, end;

  if (!gtk_label_get_selection_bounds (GTK_LABEL (self), &start, &end))
    return FALSE;

  *n_ranges = 1;
  *ranges = g_new (GtkAccessibleTextRange, 1);
  (*ranges)[0].start = start;
  (*ranges)[0].length = end - start;

  return TRUE;
}

static void
gtk_label_accessible_text_get_default_attributes (GtkAccessibleText   *self,
                                                  char              ***attribute_names,
                                                  char              ***attribute_values)
{
  PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (self));
  char **names, **values;

  gtk_pango_get_default_attributes (layout, &names, &values);

  *attribute_names = names;
  *attribute_values = values;
}

static gboolean
gtk_label_accessible_text_get_attributes (GtkAccessibleText        *self,
                                          unsigned int              offset,
                                          gsize                    *n_ranges,
                                          GtkAccessibleTextRange  **ranges,
                                          char                   ***attribute_names,
                                          char                   ***attribute_values)
{
  PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (self));
  unsigned int start, end;
  char **names, **values;

  gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

  *n_ranges = g_strv_length (names);
  *ranges = g_new (GtkAccessibleTextRange, *n_ranges);

  for (unsigned i = 0; i < *n_ranges; i++)
    {
      GtkAccessibleTextRange *range = &(*ranges)[i];

      range->start = start;
      range->length = end - start;
    }

  *attribute_names = names;
  *attribute_values = values;

  return TRUE;
}

static void
gtk_label_accessible_text_init (GtkAccessibleTextInterface *iface)
{
  iface->get_contents = gtk_label_accessible_text_get_contents;
  iface->get_contents_at = gtk_label_accessible_text_get_contents_at;
  iface->get_caret_position = gtk_label_accessible_text_get_caret_position;
  iface->get_selection = gtk_label_accessible_text_get_selection;
  iface->get_attributes = gtk_label_accessible_text_get_attributes;
  iface->get_default_attributes = gtk_label_accessible_text_get_default_attributes;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
