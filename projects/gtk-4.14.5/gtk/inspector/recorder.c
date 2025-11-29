/*
 * Copyright (c) 2016 Red Hat, Inc.
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

#include "config.h"

#include "recorder.h"

#include <gtk/gtkbinlayout.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkdragsource.h>
#include <gtk/gtkeventcontroller.h>
#include <gtk/gtkfiledialog.h>
#include <gtk/gtkinscription.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>
#include <gtk/gtklistitem.h>
#include <gtk/gtklistview.h>
#include <gtk/gtkalertdialog.h>
#include <gtk/gtkpicture.h>
#include <gtk/gtkpopover.h>
#include <gtk/gtksignallistitemfactory.h>
#include <gtk/gtksingleselection.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreeexpander.h>
#include <gtk/gtktreelistmodel.h>
#include <gtk/gtkstack.h>
#include <gtk/gtknoselection.h>
#include <gtk/gtkcolumnview.h>
#include <gtk/gtkcolumnviewcolumn.h>
#include <gsk/gskrendererprivate.h>
#include <gsk/gskrendernodeprivate.h>
#include <gsk/gskroundedrectprivate.h>
#include <gsk/gsktransformprivate.h>

#include <glib/gi18n-lib.h>
#include <gdk/gdktextureprivate.h>
#include <gdk/gdksurfaceprivate.h>
#include <gdk/gdksubsurfaceprivate.h>
#include "gtk/gtkdebug.h"
#include "gtk/gtkbuiltiniconprivate.h"
#include "gtk/gtkrendernodepaintableprivate.h"

#include "recording.h"
#include "renderrecording.h"
#include "startrecording.h"
#include "eventrecording.h"
#include "recorderrow.h"

/* {{{ ObjectProperty object */

typedef struct _ObjectProperty ObjectProperty;

G_DECLARE_FINAL_TYPE (ObjectProperty, object_property, OBJECT, PROPERTY, GObject);

struct _ObjectProperty
{
  GObject parent;

  char *name;
  char *value;
  GdkTexture *texture;
};

enum {
  OBJECT_PROPERTY_PROP_NAME = 1,
  OBJECT_PROPERTY_PROP_VALUE,
  OBJECT_PROPERTY_PROP_TEXTURE,
  OBJECT_PROPERTY_NUM_PROPERTIES
};

G_DEFINE_TYPE (ObjectProperty, object_property, G_TYPE_OBJECT);

static void
object_property_init (ObjectProperty *self)
{
}

static void
object_property_finalize (GObject *object)
{
  ObjectProperty *self = OBJECT_PROPERTY (object);

  g_free (self->name);
  g_free (self->value);
  g_object_unref (self->texture);

  G_OBJECT_CLASS (object_property_parent_class)->finalize (object);
}

static void
object_property_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ObjectProperty *self = OBJECT_PROPERTY (object);

  switch (property_id)
    {
    case OBJECT_PROPERTY_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case OBJECT_PROPERTY_PROP_VALUE:
      g_value_set_string (value, self->value);
      break;

    case OBJECT_PROPERTY_PROP_TEXTURE:
      g_value_set_object (value, self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
object_property_class_init (ObjectPropertyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = object_property_finalize;
  object_class->get_property = object_property_get_property;

  pspec = g_param_spec_string ("name", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, OBJECT_PROPERTY_PROP_NAME, pspec);

  pspec = g_param_spec_string ("value", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, OBJECT_PROPERTY_PROP_VALUE, pspec);

  pspec = g_param_spec_object ("texture", NULL, NULL,
                               GDK_TYPE_TEXTURE,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, OBJECT_PROPERTY_PROP_TEXTURE, pspec);
}

static ObjectProperty *
object_property_new (const char *name,
                     const char *value,
                     GdkTexture *texture)
{
  ObjectProperty *self;

  self = g_object_new (object_property_get_type (), NULL);

  self->name = g_strdup (name);
  self->value = g_strdup (value);
  g_set_object (&self->texture, texture);

  return self;
}

/* }}} */

struct _GtkInspectorRecorder
{
  GtkWidget parent;

  GListModel *recordings;
  GtkTreeListModel *render_node_model;
  GListStore *render_node_root_model;
  GtkSingleSelection *render_node_selection;

  GtkWidget *box;
  GtkWidget *recordings_list;
  GtkWidget *render_node_view;
  GtkWidget *render_node_list;
  GtkWidget *render_node_save_button;
  GtkWidget *render_node_clip_button;
  GtkWidget *node_property_tree;
  GtkWidget *recording_data_stack;
  GListStore *render_node_properties;
  GListStore *event_properties;
  GtkWidget *event_property_tree;
  GtkWidget *event_view;

  GtkInspectorRecording *recording; /* start recording if recording or NULL if not */
  gint64 start_time;

  gboolean debug_nodes;
  gboolean highlight_sequences;

  GdkEventSequence *selected_sequence;
};

typedef struct _GtkInspectorRecorderClass
{
  GtkWidgetClass parent;
} GtkInspectorRecorderClass;


enum
{
  PROP_0,
  PROP_RECORDING,
  PROP_DEBUG_NODES,
  PROP_HIGHLIGHT_SEQUENCES,
  PROP_SELECTED_SEQUENCE,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkInspectorRecorder, gtk_inspector_recorder, GTK_TYPE_WIDGET)

static GListModel *
create_render_node_list_model (GskRenderNode **nodes,
                               guint           n_nodes)
{
  GListStore *store;
  guint i;

  /* can't put render nodes into list models - they're not GObjects */
  store = g_list_store_new (GDK_TYPE_PAINTABLE);

  for (i = 0; i < n_nodes; i++)
    {
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (nodes[i], &bounds);
      GdkPaintable *paintable = gtk_render_node_paintable_new (nodes[i], &bounds);
      g_list_store_append (store, paintable);
      g_object_unref (paintable);
    }

  return G_LIST_MODEL (store);
}

static GListModel *
create_list_model_for_render_node (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    default:
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return NULL;

    case GSK_CAIRO_NODE:
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      /* no children */
      return NULL;

    case GSK_TRANSFORM_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_transform_node_get_child (node) }, 1);

    case GSK_OPACITY_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_opacity_node_get_child (node) }, 1);

    case GSK_COLOR_MATRIX_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_color_matrix_node_get_child (node) }, 1);

    case GSK_BLUR_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_blur_node_get_child (node) }, 1);

    case GSK_REPEAT_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_repeat_node_get_child (node) }, 1);

    case GSK_CLIP_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_clip_node_get_child (node) }, 1);

    case GSK_ROUNDED_CLIP_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_rounded_clip_node_get_child (node) }, 1);

    case GSK_FILL_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_fill_node_get_child (node) }, 1);

    case GSK_STROKE_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_stroke_node_get_child (node) }, 1);

    case GSK_SHADOW_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_shadow_node_get_child (node) }, 1);

    case GSK_BLEND_NODE:
      return create_render_node_list_model ((GskRenderNode *[2]) { gsk_blend_node_get_bottom_child (node),
                                                                   gsk_blend_node_get_top_child (node) }, 2);

    case GSK_MASK_NODE:
      return create_render_node_list_model ((GskRenderNode *[2]) { gsk_mask_node_get_source (node),
                                                                   gsk_mask_node_get_mask (node) }, 2);

    case GSK_CROSS_FADE_NODE:
      return create_render_node_list_model ((GskRenderNode *[2]) { gsk_cross_fade_node_get_start_child (node),
                                                                   gsk_cross_fade_node_get_end_child (node) }, 2);

    case GSK_GL_SHADER_NODE:
      {
        GListStore *store = g_list_store_new (GDK_TYPE_PAINTABLE);

        for (guint i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
          {
            GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);
            graphene_rect_t bounds;
            GdkPaintable *paintable;

            gsk_render_node_get_bounds (child, &bounds);
            paintable = gtk_render_node_paintable_new (child, &bounds);
            g_list_store_append (store, paintable);
            g_object_unref (paintable);
          }

        return G_LIST_MODEL (store);
      }

    case GSK_CONTAINER_NODE:
      {
        GListStore *store;
        guint i;

        /* can't put render nodes into list models - they're not GObjects */
        store = g_list_store_new (GDK_TYPE_PAINTABLE);

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);
            graphene_rect_t bounds;
            GdkPaintable *paintable;

            gsk_render_node_get_bounds (child, &bounds);
            paintable = gtk_render_node_paintable_new (child, &bounds);
            g_list_store_append (store, paintable);
            g_object_unref (paintable);
          }

        return G_LIST_MODEL (store);
      }

    case GSK_DEBUG_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_debug_node_get_child (node) }, 1);

    case GSK_SUBSURFACE_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_subsurface_node_get_child (node) }, 1);
    }
}

static GListModel *
create_list_model_for_render_node_paintable (gpointer paintable,
                                             gpointer unused)
{
  GskRenderNode *node = gtk_render_node_paintable_get_render_node (paintable);

  return create_list_model_for_render_node (node);
}

static void
recordings_clear_all (GtkButton            *button,
                      GtkInspectorRecorder *recorder)
{
  g_list_store_remove_all (G_LIST_STORE (recorder->recordings));
}

static const char *
node_type_name (GskRenderNodeType type)
{
  switch (type)
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return "Unknown";
    case GSK_CONTAINER_NODE:
      return "Container";
    case GSK_DEBUG_NODE:
      return "Debug";
    case GSK_CAIRO_NODE:
      return "Cairo";
    case GSK_COLOR_NODE:
      return "Color";
    case GSK_LINEAR_GRADIENT_NODE:
      return "Linear Gradient";
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      return "Repeating Linear Gradient";
    case GSK_RADIAL_GRADIENT_NODE:
      return "Radial Gradient";
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
      return "Repeating Radial Gradient";
    case GSK_CONIC_GRADIENT_NODE:
      return "Conic Gradient";
    case GSK_BORDER_NODE:
      return "Border";
    case GSK_TEXTURE_NODE:
      return "Texture";
    case GSK_TEXTURE_SCALE_NODE:
      return "Scaled Texture";
    case GSK_INSET_SHADOW_NODE:
      return "Inset Shadow";
    case GSK_OUTSET_SHADOW_NODE:
      return "Outset Shadow";
    case GSK_TRANSFORM_NODE:
      return "Transform";
    case GSK_OPACITY_NODE:
      return "Opacity";
    case GSK_COLOR_MATRIX_NODE:
      return "Color Matrix";
    case GSK_REPEAT_NODE:
      return "Repeat";
    case GSK_CLIP_NODE:
      return "Clip";
    case GSK_ROUNDED_CLIP_NODE:
      return "Rounded Clip";
    case GSK_FILL_NODE:
      return "Fill";
    case GSK_STROKE_NODE:
      return "Stroke";
    case GSK_SHADOW_NODE:
      return "Shadow";
    case GSK_BLEND_NODE:
      return "Blend";
    case GSK_MASK_NODE:
      return "Mask";
    case GSK_CROSS_FADE_NODE:
      return "CrossFade";
    case GSK_TEXT_NODE:
      return "Text";
    case GSK_BLUR_NODE:
      return "Blur";
    case GSK_GL_SHADER_NODE:
      return "GL Shader";
    case GSK_SUBSURFACE_NODE:
      return "Subsurface";
    }
}

static char *
node_name (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_MASK_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_SUBSURFACE_NODE:
      return g_strdup (node_type_name (gsk_render_node_get_node_type (node)));

    case GSK_DEBUG_NODE:
      return g_strdup (gsk_debug_node_get_message (node));

    case GSK_COLOR_NODE:
      return gdk_rgba_to_string (gsk_color_node_get_color (node));

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        return g_strdup_printf ("%dx%d Texture", gdk_texture_get_width (texture), gdk_texture_get_height (texture));
      }
    case GSK_TEXTURE_SCALE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        return g_strdup_printf ("%dx%d Texture, Filter %d", gdk_texture_get_width (texture), gdk_texture_get_height (texture), gsk_texture_scale_node_get_filter (node));
      }
    }
}

static GdkContentProvider *
prepare_render_node_drag (GtkDragSource  *source,
                          double          x,
                          double          y,
                          GtkListItem    *list_item)
{
  GtkTreeListRow *row_item;
  GdkPaintable *paintable;
  GskRenderNode *node;

  row_item = gtk_list_item_get_item (list_item);
  if (row_item == NULL)
    return NULL;

  paintable = gtk_tree_list_row_get_item (row_item);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));

  return gdk_content_provider_new_typed (GSK_TYPE_RENDER_NODE, node);
}

static void
setup_widget_for_render_node (GtkSignalListItemFactory *factory,
                              GtkListItem              *list_item)
{
  GtkWidget *expander, *box, *child;
  GtkDragSource *source;

  /* expander */
  expander = gtk_tree_expander_new ();
  gtk_list_item_set_child (list_item, expander);
  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (prepare_render_node_drag), list_item);
  gtk_widget_add_controller (expander, GTK_EVENT_CONTROLLER (source));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), box);

  /* icon */
  child = gtk_image_new ();
  gtk_box_append (GTK_BOX (box), child);

  /* name */
  child = gtk_inscription_new (NULL);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_box_append (GTK_BOX (box), child);
}

static void
bind_widget_for_render_node (GtkSignalListItemFactory *factory,
                             GtkListItem              *list_item)
{
  GdkPaintable *paintable;
  GskRenderNode *node;
  GtkTreeListRow *row_item;
  GtkWidget *expander, *box, *child;
  char *name;

  row_item = gtk_list_item_get_item (list_item);
  paintable = gtk_tree_list_row_get_item (row_item);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));

  /* expander */
  expander = gtk_list_item_get_child (list_item);
  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), row_item);
  box = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));

  /* icon */
  child = gtk_widget_get_first_child (box);
  gtk_image_set_from_paintable (GTK_IMAGE (child), paintable);

  /* name */
  name = node_name (node);
  child = gtk_widget_get_last_child (box);
  gtk_inscription_set_text (GTK_INSCRIPTION (child), name);
  g_free (name);

  g_object_unref (paintable);
}

static void
show_render_node (GtkInspectorRecorder *recorder,
                  GskRenderNode        *node)
{
  graphene_rect_t bounds;
  GdkPaintable *paintable;

  gsk_render_node_get_bounds (node, &bounds);
  paintable = gtk_render_node_paintable_new (node, &bounds);

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (recorder->recording_data_stack)), "frame_data") == 0)
    {
      gtk_picture_set_paintable (GTK_PICTURE (recorder->render_node_view), paintable);

      g_list_store_splice (recorder->render_node_root_model,
                           0, g_list_model_get_n_items (G_LIST_MODEL (recorder->render_node_root_model)),
                           (gpointer[1]) { paintable },
                           1);
    }
  else
    {
      gtk_picture_set_paintable (GTK_PICTURE (recorder->event_view), paintable);
    }

  g_object_unref (paintable);
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_property_name (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *label;
  ObjectProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->name);
}

static void
setup_value_widgets (GtkSignalListItemFactory *factory,
                     GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkWidget *picture;
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_box_append (GTK_BOX (box), label);
  picture = gtk_picture_new ();
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
  gtk_box_append (GTK_BOX (box), picture);
  gtk_list_item_set_child (list_item, box);
}

static void
bind_value_widgets (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *box, *label, *picture;
  ObjectProperty *property;

  property = gtk_list_item_get_item (list_item);
  box = gtk_list_item_get_child (list_item);
  label = gtk_widget_get_first_child (box);
  picture = gtk_widget_get_next_sibling (label);
  gtk_label_set_text (GTK_LABEL (label), property->value);
  gtk_widget_set_visible (picture, property->texture != NULL);
  if (property->texture)
    gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (property->texture));
}

static GskRenderNode *
make_dot (double x, double y)
{
  GskRenderNode *fill, *dot;
  GdkRGBA red = (GdkRGBA){ 1, 0, 0, 1 };
  graphene_rect_t rect = GRAPHENE_RECT_INIT (x - 3, y - 3, 6, 6);
  graphene_size_t corner = GRAPHENE_SIZE_INIT (3, 3);
  GskRoundedRect clip;

  fill = gsk_color_node_new (&red, &rect);
  dot = gsk_rounded_clip_node_new (fill, gsk_rounded_rect_init (&clip, &rect,
                                                               &corner, &corner, &corner, &corner));
  gsk_render_node_unref (fill);

  return dot;
}

static void
show_event (GtkInspectorRecorder *recorder,
            GskRenderNode        *node,
            GdkEvent             *event)
{
  GskRenderNode *temp;
  double x, y;

  if (gdk_event_get_position (event, &x, &y))
    {
      GskRenderNode *dot = make_dot (x, y);
      temp = gsk_container_node_new ((GskRenderNode *[]) { node, dot }, 2);
      gsk_render_node_unref (dot);
    }
  else
    temp = gsk_render_node_ref (node);

  show_render_node (recorder, temp);

  gsk_render_node_unref (temp);
}

static void populate_event_properties (GListStore *store,
                                       GdkEvent   *event);

static void
recording_selected (GtkSingleSelection   *selection,
                    GParamSpec           *pspec,
                    GtkInspectorRecorder *recorder)
{
  GtkInspectorRecording *recording;
  GdkEventSequence *selected_sequence = NULL;

  if (recorder->recordings == NULL)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (recorder->recording_data_stack), "no_data");
      return;
    }

  recording = gtk_single_selection_get_selected_item (selection);

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      GskRenderNode *node;

      gtk_stack_set_visible_child_name (GTK_STACK (recorder->recording_data_stack), "frame_data");

      node = gtk_inspector_render_recording_get_node (GTK_INSPECTOR_RENDER_RECORDING (recording));
      show_render_node (recorder, node);
    }
  else if (GTK_INSPECTOR_IS_EVENT_RECORDING (recording))
    {
      GdkEvent *event;

      gtk_stack_set_visible_child_name (GTK_STACK (recorder->recording_data_stack), "event_data");

      event = gtk_inspector_event_recording_get_event (GTK_INSPECTOR_EVENT_RECORDING (recording));

      for (guint pos = gtk_single_selection_get_selected (selection) - 1; pos > 0; pos--)
        {
          GtkInspectorRecording *item = g_list_model_get_item (G_LIST_MODEL (selection), pos);

          g_object_unref (item);
          if (GTK_INSPECTOR_IS_RENDER_RECORDING (item))
            {
              GskRenderNode *node;

              node = gtk_inspector_render_recording_get_node (GTK_INSPECTOR_RENDER_RECORDING (item));
              show_event (recorder, node, event);
              break;
            }
        }

      populate_event_properties (recorder->event_properties, event);

      if (recorder->highlight_sequences)
        selected_sequence = gdk_event_get_event_sequence (event);
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (recorder->recording_data_stack), "no_data");

      gtk_picture_set_paintable (GTK_PICTURE (recorder->render_node_view), NULL);
      g_list_store_remove_all (recorder->render_node_root_model);
    }

  gtk_inspector_recorder_set_selected_sequence (recorder, selected_sequence);
}

static GdkTexture *
get_color_texture (const GdkRGBA *color)
{
  GdkTexture *texture;
  guchar pixel[4];
  guchar *data;
  GBytes *bytes;
  int width = 30;
  int height = 30;
  int i;

  pixel[0] = round (color->red * 255);
  pixel[1] = round (color->green * 255);
  pixel[2] = round (color->blue * 255);
  pixel[3] = round (color->alpha * 255);

  data = g_malloc (4 * width * height);
  for (i = 0; i < width * height; i++)
    {
      memcpy (data + 4 * i, pixel, 4);
    }

  bytes = g_bytes_new_take (data, 4 * width * height);
  texture = gdk_memory_texture_new (width,
                                    height,
                                    GDK_MEMORY_R8G8B8A8,
                                    bytes,
                                    width * 4);
  g_bytes_unref (bytes);

  return texture;
}

static GdkTexture *
get_linear_gradient_texture (gsize n_stops, const GskColorStop *stops)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  GdkTexture *texture;
  int i;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 90, 30);
  cr = cairo_create (surface);

  pattern = cairo_pattern_create_linear (0, 0, 90, 0);
  for (i = 0; i < n_stops; i++)
    {
      cairo_pattern_add_color_stop_rgba (pattern,
                                         stops[i].offset,
                                         stops[i].color.red,
                                         stops[i].color.green,
                                         stops[i].color.blue,
                                         stops[i].color.alpha);
    }

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_rectangle (cr, 0, 0, 90, 30);
  cairo_fill (cr);
  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
add_text_row (GListStore *store,
              const char *name,
              const char *text)
{
  g_list_store_append (store, object_property_new (name, text, NULL));
}

static void
add_color_row (GListStore    *store,
               const char    *name,
               const GdkRGBA *color)
{
  char *text;
  GdkTexture *texture;

  text = gdk_rgba_to_string (color);
  texture = get_color_texture (color);
  g_list_store_append (store, object_property_new (name, text, texture));
  g_free (text);
  g_object_unref (texture);
}

static void
add_int_row (GListStore  *store,
             const char  *name,
             int          value)
{
  char *text = g_strdup_printf ("%d", value);
  add_text_row (store, name, text);
  g_free (text);
}

static void
add_uint_row (GListStore  *store,
              const char  *name,
              guint        value)
{
  char *text = g_strdup_printf ("%u", value);
  add_text_row (store, name, text);
  g_free (text);
}

static void
add_boolean_row (GListStore  *store,
                 const char  *name,
                 gboolean     value)
{
  add_text_row (store, name, value ? "TRUE" : "FALSE");
}

static void
add_float_row (GListStore  *store,
               const char  *name,
               float        value)
{
  char *text = g_strdup_printf ("%.2f", value);
  add_text_row (store, name, text);
  g_free (text);
}

static const char *
enum_to_nick (GType type,
              int   value)
{
  GEnumClass *class;
  GEnumValue *v;

  class = g_type_class_ref (type);
  v = g_enum_get_value (class, value);
  g_type_class_unref (class);

  return v->value_nick;
}

static void
populate_render_node_properties (GListStore    *store,
                                 GskRenderNode *node)
{
  graphene_rect_t bounds;
  char *tmp;

  g_list_store_remove_all (store);

  gsk_render_node_get_bounds (node, &bounds);

  add_text_row (store, "Type", node_type_name (gsk_render_node_get_node_type (node)));

  tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                         bounds.size.width,
                         bounds.size.height,
                         bounds.origin.x,
                         bounds.origin.y);
  add_text_row (store, "Bounds", tmp);
  g_free (tmp);

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CAIRO_NODE:
      {
        GdkTexture *texture;
        cairo_surface_t *drawn_surface;
        cairo_t *cr;

        drawn_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                    ceilf (node->bounds.size.width),
                                                    ceilf (node->bounds.size.height));
        cr = cairo_create (drawn_surface);
        cairo_save (cr);
        cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
        gsk_render_node_draw (node, cr);
        cairo_restore (cr);

        cairo_destroy (cr);

        texture = gdk_texture_new_for_surface (drawn_surface);
        cairo_surface_destroy (drawn_surface);

        g_list_store_append (store, object_property_new ("Surface", "", texture));
      }
      break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = g_object_ref (gsk_texture_node_get_texture (node));
        g_list_store_append (store, object_property_new ("Texture", "", texture));
      }
      break;

    case GSK_TEXTURE_SCALE_NODE:
      {
        GdkTexture *texture = g_object_ref (gsk_texture_scale_node_get_texture (node));
        GskScalingFilter filter = gsk_texture_scale_node_get_filter (node);
        g_list_store_append (store, object_property_new ("Texture", "", texture));

        tmp = g_enum_to_string (GSK_TYPE_SCALING_FILTER, filter);
        add_text_row (store, "Filter", tmp);
        g_free (tmp);
      }
      break;

    case GSK_COLOR_NODE:
      add_color_row (store, "Color", gsk_color_node_get_color (node));
      break;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      {
        const graphene_point_t *start = gsk_linear_gradient_node_get_start (node);
        const graphene_point_t *end = gsk_linear_gradient_node_get_end (node);
        const gsize n_stops = gsk_linear_gradient_node_get_n_color_stops (node);
        const GskColorStop *stops = gsk_linear_gradient_node_get_color_stops (node, NULL);
        int i;
        GString *s;
        GdkTexture *texture;

        tmp = g_strdup_printf ("%.2f %.2f ⟶ %.2f %.2f", start->x, start->y, end->x, end->y);
        add_text_row (store, "Direction", tmp);
        g_free (tmp);

        s = g_string_new ("");
        for (i = 0; i < n_stops; i++)
          {
            tmp = gdk_rgba_to_string (&stops[i].color);
            g_string_append_printf (s, "%.2f, %s\n", stops[i].offset, tmp);
            g_free (tmp);
          }

        texture = get_linear_gradient_texture (n_stops, stops);
        g_list_store_append (store, object_property_new ("Color Stops", s->str, texture));
        g_object_unref (texture);

        g_string_free (s, TRUE);
      }
      break;

    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
      {
        const graphene_point_t *center = gsk_radial_gradient_node_get_center (node);
        const float start = gsk_radial_gradient_node_get_start (node);
        const float end = gsk_radial_gradient_node_get_end (node);
        const float hradius = gsk_radial_gradient_node_get_hradius (node);
        const float vradius = gsk_radial_gradient_node_get_vradius (node);
        const gsize n_stops = gsk_radial_gradient_node_get_n_color_stops (node);
        const GskColorStop *stops = gsk_radial_gradient_node_get_color_stops (node, NULL);
        int i;
        GString *s;
        GdkTexture *texture;

        tmp = g_strdup_printf ("%.2f, %.2f", center->x, center->y);
        add_text_row (store, "Center", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f ⟶  %.2f", start, end);
        add_text_row (store, "Direction", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f, %.2f", hradius, vradius);
        add_text_row (store, "Radius", tmp);
        g_free (tmp);

        s = g_string_new ("");
        for (i = 0; i < n_stops; i++)
          {
            tmp = gdk_rgba_to_string (&stops[i].color);
            g_string_append_printf (s, "%.2f, %s\n", stops[i].offset, tmp);
            g_free (tmp);
          }

        texture = get_linear_gradient_texture (n_stops, stops);
        g_list_store_append (store, object_property_new ("Color Stops", s->str, texture));
        g_object_unref (texture);

        g_string_free (s, TRUE);
      }
      break;

    case GSK_CONIC_GRADIENT_NODE:
      {
        const graphene_point_t *center = gsk_conic_gradient_node_get_center (node);
        const float rotation = gsk_conic_gradient_node_get_rotation (node);
        const gsize n_stops = gsk_conic_gradient_node_get_n_color_stops (node);
        const GskColorStop *stops = gsk_conic_gradient_node_get_color_stops (node, NULL);
        gsize i;
        GString *s;
        GdkTexture *texture;

        tmp = g_strdup_printf ("%.2f, %.2f", center->x, center->y);
        add_text_row (store, "Center", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f", rotation);
        add_text_row (store, "Rotation", tmp);
        g_free (tmp);

        s = g_string_new ("");
        for (i = 0; i < n_stops; i++)
          {
            tmp = gdk_rgba_to_string (&stops[i].color);
            g_string_append_printf (s, "%.2f, %s\n", stops[i].offset, tmp);
            g_free (tmp);
          }

        texture = get_linear_gradient_texture (n_stops, stops);
        g_list_store_append (store, object_property_new ("Color Stops", s->str, texture));
        g_object_unref (texture);

        g_string_free (s, TRUE);
      }
      break;

    case GSK_TEXT_NODE:
      {
        const PangoFont *font = gsk_text_node_get_font (node);
        const GdkRGBA *color = gsk_text_node_get_color (node);
        const graphene_point_t *offset = gsk_text_node_get_offset (node);
        PangoFontDescription *desc;
        GString *s;

        desc = pango_font_describe ((PangoFont *)font);
        tmp = pango_font_description_to_string (desc);
        add_text_row (store, "Font", tmp);
        g_free (tmp);
        pango_font_description_free (desc);

        s = g_string_sized_new (0);
        gsk_text_node_serialize_glyphs (node, s);
        add_text_row (store, "Glyphs", s->str);
        g_string_free (s, TRUE);

        tmp = g_strdup_printf ("%.2f %.2f", offset->x, offset->y);
        add_text_row (store, "Position", tmp);
        g_free (tmp);

        add_color_row (store, "Color", color);
      }
      break;

    case GSK_BORDER_NODE:
      {
        const char *name[4] = { "Top", "Right", "Bottom", "Left" };
        const float *widths = gsk_border_node_get_widths (node);
        const GdkRGBA *colors = gsk_border_node_get_colors (node);
        int i;

        for (i = 0; i < 4; i++)
          {
            GdkTexture *texture;
            char *text;

            text = gdk_rgba_to_string (&colors[i]);
            tmp = g_strdup_printf ("%.2f, %s", widths[i], text);
            texture = get_color_texture (&colors[i]);
            g_list_store_append (store, object_property_new (name[i], tmp, texture));
            g_object_unref (texture);

            g_free (text);
            g_free (tmp);
          }
      }
      break;

    case GSK_OPACITY_NODE:
      add_float_row (store, "Opacity", gsk_opacity_node_get_opacity (node));
      break;

    case GSK_CROSS_FADE_NODE:
      add_float_row (store, "Progress", gsk_cross_fade_node_get_progress (node));
      break;

    case GSK_BLEND_NODE:
      {
        GskBlendMode mode = gsk_blend_node_get_blend_mode (node);
        add_text_row (store, "Blendmode", enum_to_nick (GSK_TYPE_BLEND_MODE, mode));
      }
      break;

    case GSK_MASK_NODE:
      {
        GskMaskMode mode = gsk_mask_node_get_mask_mode (node);
        tmp = g_enum_to_string (GSK_TYPE_MASK_MODE, mode);
        add_text_row (store, "Mask mode", tmp);
        g_free (tmp);
      }
      break;

    case GSK_BLUR_NODE:
      add_float_row (store, "Radius", gsk_blur_node_get_radius (node));
      break;

    case GSK_GL_SHADER_NODE:
      {
        GskGLShader *shader = gsk_gl_shader_node_get_shader (node);
        GBytes *args = gsk_gl_shader_node_get_args (node);
        int i;

        add_int_row (store, "Required textures", gsk_gl_shader_get_n_textures (shader));
        for (i = 0; i < gsk_gl_shader_get_n_uniforms (shader); i++)
          {
            const char *name;
            char *title;

            name = gsk_gl_shader_get_uniform_name (shader, i);
            title = g_strdup_printf ("Uniform %s", name);

            switch (gsk_gl_shader_get_uniform_type (shader, i))
              {
              case GSK_GL_UNIFORM_TYPE_NONE:
              default:
                g_assert_not_reached ();
                break;

              case GSK_GL_UNIFORM_TYPE_FLOAT:
                add_float_row (store, title,
                               gsk_gl_shader_get_arg_float (shader, args, i));
                break;

              case GSK_GL_UNIFORM_TYPE_INT:
                add_int_row (store, title,
                             gsk_gl_shader_get_arg_int (shader, args, i));
                break;

              case GSK_GL_UNIFORM_TYPE_UINT:
                add_uint_row (store, title,
                              gsk_gl_shader_get_arg_uint (shader, args, i));
                break;

              case GSK_GL_UNIFORM_TYPE_BOOL:
                add_boolean_row (store, title,
                                 gsk_gl_shader_get_arg_bool (shader, args, i));
                break;

              case GSK_GL_UNIFORM_TYPE_VEC2:
                {
                  graphene_vec2_t v;
                  gsk_gl_shader_get_arg_vec2 (shader, args, i, &v);
                  float x = graphene_vec2_get_x (&v);
                  float y = graphene_vec2_get_x (&v);
                  char *s = g_strdup_printf ("%.2f %.2f", x, y);
                  add_text_row (store, title, s);
                  g_free (s);
                }
                break;

              case GSK_GL_UNIFORM_TYPE_VEC3:
                {
                  graphene_vec3_t v;
                  gsk_gl_shader_get_arg_vec3 (shader, args, i, &v);
                  float x = graphene_vec3_get_x (&v);
                  float y = graphene_vec3_get_y (&v);
                  float z = graphene_vec3_get_z (&v);
                  char *s = g_strdup_printf ("%.2f %.2f %.2f", x, y, z);
                  add_text_row (store, title, s);
                  g_free (s);
                }
                break;

              case GSK_GL_UNIFORM_TYPE_VEC4:
                {
                  graphene_vec4_t v;
                  gsk_gl_shader_get_arg_vec4 (shader, args, i, &v);
                  float x = graphene_vec4_get_x (&v);
                  float y = graphene_vec4_get_y (&v);
                  float z = graphene_vec4_get_z (&v);
                  float w = graphene_vec4_get_w (&v);
                  char *s = g_strdup_printf ("%.2f %.2f %.2f %.2f", x, y, z, w);
                  add_text_row (store, title, s);
                  g_free (s);
                }
                break;
              }
            g_free (title);
          }
      }
      break;

    case GSK_INSET_SHADOW_NODE:
      {
        const GdkRGBA *color = gsk_inset_shadow_node_get_color (node);
        float dx = gsk_inset_shadow_node_get_dx (node);
        float dy = gsk_inset_shadow_node_get_dy (node);
        float spread = gsk_inset_shadow_node_get_spread (node);
        float radius = gsk_inset_shadow_node_get_blur_radius (node);

        add_color_row (store, "Color", color);

        tmp = g_strdup_printf ("%.2f %.2f", dx, dy);
        add_text_row (store, "Offset", tmp);
        g_free (tmp);

        add_float_row (store, "Spread", spread);
        add_float_row (store, "Radius", radius);
      }
      break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
        const GdkRGBA *color = gsk_outset_shadow_node_get_color (node);
        float dx = gsk_outset_shadow_node_get_dx (node);
        float dy = gsk_outset_shadow_node_get_dy (node);
        float spread = gsk_outset_shadow_node_get_spread (node);
        float radius = gsk_outset_shadow_node_get_blur_radius (node);
        float rect[12];

        gsk_rounded_rect_to_float (outline, graphene_point_zero (), rect);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               rect[2], rect[3], rect[0], rect[1]);
        add_text_row (store, "Outline", tmp);
        g_free (tmp);

        add_color_row (store, "Color", color);

        tmp = g_strdup_printf ("%.2f %.2f", dx, dy);
        add_text_row (store, "Offset", tmp);
        g_free (tmp);

        add_float_row (store, "Spread", spread);
        add_float_row (store, "Radius", radius);
      }
      break;

    case GSK_REPEAT_NODE:
      {
        const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);

        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               child_bounds->size.width,
                               child_bounds->size.height,
                               child_bounds->origin.x,
                               child_bounds->origin.y);
        add_text_row (store, "Child Bounds", tmp);
        g_free (tmp);
      }
      break;

    case GSK_COLOR_MATRIX_NODE:
      {
        const graphene_matrix_t *matrix = gsk_color_matrix_node_get_color_matrix (node);
        const graphene_vec4_t *offset = gsk_color_matrix_node_get_color_offset (node);

        tmp = g_strdup_printf ("% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f",
                               graphene_matrix_get_value (matrix, 0, 0),
                               graphene_matrix_get_value (matrix, 0, 1),
                               graphene_matrix_get_value (matrix, 0, 2),
                               graphene_matrix_get_value (matrix, 0, 3),
                               graphene_matrix_get_value (matrix, 1, 0),
                               graphene_matrix_get_value (matrix, 1, 1),
                               graphene_matrix_get_value (matrix, 1, 2),
                               graphene_matrix_get_value (matrix, 1, 3),
                               graphene_matrix_get_value (matrix, 2, 0),
                               graphene_matrix_get_value (matrix, 2, 1),
                               graphene_matrix_get_value (matrix, 2, 2),
                               graphene_matrix_get_value (matrix, 2, 3),
                               graphene_matrix_get_value (matrix, 3, 0),
                               graphene_matrix_get_value (matrix, 3, 1),
                               graphene_matrix_get_value (matrix, 3, 2),
                               graphene_matrix_get_value (matrix, 3, 3));
        add_text_row (store, "Matrix", tmp);
        g_free (tmp);
        tmp = g_strdup_printf ("%.2f %.2f %.2f %.2f",
                               graphene_vec4_get_x (offset),
                               graphene_vec4_get_y (offset),
                               graphene_vec4_get_z (offset),
                               graphene_vec4_get_w (offset));
        add_text_row (store, "Offset", tmp);
        g_free (tmp);
      }
      break;

    case GSK_CLIP_NODE:
      {
        const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               clip->size.width,
                               clip->size.height,
                               clip->origin.x,
                               clip->origin.y);
        add_text_row (store, "Clip", tmp);
        g_free (tmp);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        const GskRoundedRect *clip = gsk_rounded_clip_node_get_clip (node);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               clip->bounds.size.width,
                               clip->bounds.size.height,
                               clip->bounds.origin.x,
                               clip->bounds.origin.y);
        add_text_row (store, "Clip", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[0].width, clip->corner[0].height);
        add_text_row (store, "Top Left Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[1].width, clip->corner[1].height);
        add_text_row (store, "Top Right Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[2].width, clip->corner[2].height);
        add_text_row (store, "Bottom Right Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[3].width, clip->corner[3].height);
        add_text_row (store, "Bottom Left Corner Size", tmp);
        g_free (tmp);
      }
      break;

    case GSK_FILL_NODE:
      {
        GskPath *path = gsk_fill_node_get_path (node);
        GskFillRule fill_rule = gsk_fill_node_get_fill_rule (node);

        tmp = gsk_path_to_string (path);
        add_text_row (store, "Path", tmp);
        g_free (tmp);

        add_text_row (store, "Fill rule", enum_to_nick (GSK_TYPE_FILL_RULE, fill_rule));
      }
      break;

    case GSK_STROKE_NODE:
      {
        GskPath *path = gsk_stroke_node_get_path (node);
        const GskStroke *stroke = gsk_stroke_node_get_stroke (node);
        GskLineCap line_cap = gsk_stroke_get_line_cap (stroke);
        GskLineJoin line_join = gsk_stroke_get_line_join (stroke);

        tmp = gsk_path_to_string (path);
        add_text_row (store, "Path", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f", gsk_stroke_get_line_width (stroke));
        add_text_row (store, "Line width", tmp);
        g_free (tmp);

        add_text_row (store, "Line cap", enum_to_nick (GSK_TYPE_LINE_CAP, line_cap));
        add_text_row (store, "Line join", enum_to_nick (GSK_TYPE_LINE_JOIN, line_join));
      }
      break;

    case GSK_CONTAINER_NODE:
      tmp = g_strdup_printf ("%d", gsk_container_node_get_n_children (node));
      add_text_row (store, "Children", tmp);
      g_free (tmp);
      break;

    case GSK_DEBUG_NODE:
      add_text_row (store, "Message", gsk_debug_node_get_message (node));
      break;

    case GSK_SHADOW_NODE:
      {
        int i;

        for (i = 0; i < gsk_shadow_node_get_n_shadows (node); i++)
          {
            char *label;
            char *value;
            const GskShadow *shadow = gsk_shadow_node_get_shadow (node, i);

            label = g_strdup_printf ("Color %d", i);
            add_color_row (store, label, &shadow->color);
            g_free (label);

            label = g_strdup_printf ("Offset %d", i);
            value = g_strdup_printf ("%.2f %.2f", shadow->dx, shadow->dy);
            add_text_row (store, label, value);
            g_free (value);
            g_free (label);

            label = g_strdup_printf ("Radius %d", i);
            add_float_row (store, label, shadow->radius);
            g_free (label);
          }
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        static const char * category_names[] = {
          [GSK_TRANSFORM_CATEGORY_UNKNOWN] = "unknown",
          [GSK_TRANSFORM_CATEGORY_ANY] = "any",
          [GSK_TRANSFORM_CATEGORY_3D] = "3D",
          [GSK_TRANSFORM_CATEGORY_2D] = "2D",
          [GSK_TRANSFORM_CATEGORY_2D_AFFINE] = "2D affine",
          [GSK_TRANSFORM_CATEGORY_2D_TRANSLATE] = "2D translate",
          [GSK_TRANSFORM_CATEGORY_IDENTITY] = "identity"
        };
        GskTransform *transform;
        char *s;

        transform = gsk_transform_node_get_transform (node);
        s = gsk_transform_to_string (transform);
        add_text_row (store, "Matrix", s);
        g_free (s);
        add_text_row (store, "Category", category_names[gsk_transform_get_category (transform)]);
      }
      break;

    case GSK_SUBSURFACE_NODE:
      {
        GdkSubsurface *subsurface = gsk_subsurface_node_get_subsurface (node);
        char s[40];

        g_snprintf (s, sizeof (s), "%p", subsurface);

        add_text_row (store, "Subsurface", s);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      break;
    }
}

static const char *
event_type_name (GdkEventType type)
{
  const char *event_name[] = {
    "Delete",
    "Motion",
    "Button Press",
    "Button Release",
    "Key Press",
    "Key Release",
    "Enter",
    "Leave",
    "Focus",
    "Proximity In",
    "Proximity Out",
    "Drag Enter",
    "Drag Leave",
    "Drag Motion",
    "Drop Start",
    "Scroll",
    "Grab Broken",
    "Touch Begin",
    "Touch Update",
    "Touch End",
    "Touch Cancel",
    "Touchpad Swipe",
    "Touchpad Pinch",
    "Pad Button Press",
    "Pad Button Release",
    "Pad Rind",
    "Pad Strip",
    "Pad Group Mode",
    "Touchpad Hold",
  };

  G_STATIC_ASSERT (G_N_ELEMENTS (event_name) == GDK_EVENT_LAST);

  return event_name[type];
}

static const char *
scroll_direction_name (GdkScrollDirection dir)
{
  const char *scroll_dir[] = {
    "Up", "Down", "Left", "Right", "Smooth"
  };
  return scroll_dir[dir];
}

static char *
modifier_names (GdkModifierType state)
{
  struct {
    const char *name;
    int mask;
  } mods[] = {
    { "Shift", GDK_SHIFT_MASK },
    { "Lock", GDK_LOCK_MASK },
    { "Control", GDK_CONTROL_MASK },
    { "Alt", GDK_ALT_MASK },
    { "Button1", GDK_BUTTON1_MASK },
    { "Button2", GDK_BUTTON2_MASK },
    { "Button3", GDK_BUTTON3_MASK },
    { "Button4", GDK_BUTTON4_MASK },
    { "Button5", GDK_BUTTON5_MASK },
    { "Super", GDK_SUPER_MASK },
    { "Hyper", GDK_HYPER_MASK },
    { "Meta", GDK_META_MASK },
  };
  GString *s;

  s = g_string_new ("");

  for (int i = 0; i < G_N_ELEMENTS (mods); i++)
    {
      if (state & mods[i].mask)
        {
          if (s->len > 0)
            g_string_append (s, " ");
          g_string_append (s, mods[i].name);
        }
    }

  return g_string_free (s, FALSE);
}

static char *
key_event_string (GdkEvent *event)
{
  guint keyval;
  gunichar c;
  char buf[5] = { 0, };

  keyval = gdk_key_event_get_keyval (event);
  c = gdk_keyval_to_unicode (keyval);
  if (c)
    {
      g_unichar_to_utf8 (c, buf);
      return g_strdup (buf);
    }

  return g_strdup (gdk_keyval_name (keyval));
}

static const char *
device_tool_name (GdkDeviceTool *tool)
{
  const char *name[] = {
    "Unknown",
    "Pen",
    "Eraser",
    "Brush",
    "Pencil",
    "Airbrush",
    "Mouse",
    "Lens"
  };

  return name[gdk_device_tool_get_tool_type (tool)];
}

static const char *
axis_name (GdkAxisUse axis)
{
  const char *name[] = {
    "",
    "X",
    "Y",
    "Delta X",
    "Delta Y",
    "Pressure",
    "X Tilt",
    "Y Tilt",
    "Wheel",
    "Distance",
    "Rotation",
    "Slider"
  };

  return name[axis];
}

static const char *
gesture_phase_name (GdkTouchpadGesturePhase phase)
{
  const char *name[] = {
    "Begin",
    "Update",
    "End",
    "Cancel"
  };

  return name[phase];
}

static const char *
scroll_unit_name (GdkScrollUnit unit)
{
  if (unit == GDK_SCROLL_UNIT_WHEEL)
    return "Wheel";
  else if (unit == GDK_SCROLL_UNIT_SURFACE)
    return "Surface";
  else
    return "Incorrect value";
}

static void
populate_event_properties (GListStore *store,
                           GdkEvent   *event)
{
  GdkEventType type;
  GdkDevice *device;
  GdkDeviceTool *tool;
  double x, y;
  double dx, dy;
  char *tmp;
  GdkModifierType state;
  GdkScrollUnit scroll_unit;

  g_list_store_remove_all (store);

  type = gdk_event_get_event_type (event);

  add_text_row (store, "Type", event_type_name (type));
  if (gdk_event_get_event_sequence (event) != NULL)
    {
      tmp = g_strdup_printf ("%p", gdk_event_get_event_sequence (event));
      add_text_row (store, "Sequence", tmp);
      g_free (tmp);
    }
  add_int_row (store, "Timestamp", gdk_event_get_time (event));

  device = gdk_event_get_device (event);
  if (device)
    add_text_row (store, "Device", gdk_device_get_name (device));

  tool = gdk_event_get_device_tool (event);
  if (tool)
    add_text_row (store, "Device Tool", device_tool_name (tool));

  if (gdk_event_get_position (event, &x, &y))
    {
      tmp = g_strdup_printf ("%.2f %.2f", x, y);
      add_text_row (store, "Position", tmp);
      g_free (tmp);
    }

  if (tool)
    {
      GdkAxisFlags axes = gdk_device_tool_get_axes (tool);

      /* We report position and scroll delta separately, so skip them here */
      axes &= ~(GDK_AXIS_FLAG_X|GDK_AXIS_FLAG_Y|GDK_AXIS_FLAG_DELTA_X|GDK_AXIS_FLAG_DELTA_Y);

      for (int i = 1; i < GDK_AXIS_LAST; i++)
        {
          if (axes & (1 << i))
            {
              double val;
              gdk_event_get_axis (event, i, &val);
              tmp = g_strdup_printf ("%.2f", val);
              add_text_row (store, axis_name (i), tmp);
              g_free (tmp);
            }
        }
    }

  state = gdk_event_get_modifier_state (event);
  if (state != 0)
    {
      tmp = modifier_names (state);
      add_text_row (store, "State", tmp);
      g_free (tmp);
    }

  switch ((int)type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      add_int_row (store, "Button", gdk_button_event_get_button (event));
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      add_int_row (store, "Keycode", gdk_key_event_get_keycode (event));
      add_int_row (store, "Keyval", gdk_key_event_get_keyval (event));
      tmp = key_event_string (event);
      add_text_row (store, "Key", tmp);
      g_free (tmp);
      add_int_row (store, "Layout", gdk_key_event_get_layout (event));
      add_int_row (store, "Level", gdk_key_event_get_level (event));
      add_boolean_row (store, "Is Modifier", gdk_key_event_is_modifier (event));
      break;

    case GDK_SCROLL:
      if (gdk_scroll_event_get_direction (event) == GDK_SCROLL_SMOOTH)
        {
          gdk_scroll_event_get_deltas (event, &x, &y);
          tmp = g_strdup_printf ("%.2f %.2f", x, y);
          add_text_row (store, "Delta", tmp);
          g_free (tmp);

          scroll_unit = gdk_scroll_event_get_unit (event);
          add_text_row (store, "Unit", scroll_unit_name (scroll_unit));
        }
      else
        {
          add_text_row (store, "Direction", scroll_direction_name (gdk_scroll_event_get_direction (event)));
        }
      add_boolean_row (store, "Is Stop", gdk_scroll_event_is_stop (event));
      break;

    case GDK_FOCUS_CHANGE:
      add_text_row (store, "Direction", gdk_focus_event_get_in (event) ? "In" : "Out");
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      add_int_row (store, "Mode", gdk_crossing_event_get_mode (event));
      add_int_row (store, "Detail", gdk_crossing_event_get_detail (event));
      add_boolean_row (store, "Is Focus", gdk_crossing_event_get_focus (event));
      break;

    case GDK_GRAB_BROKEN:
      add_boolean_row (store, "Implicit", gdk_grab_broken_event_get_implicit (event));
      break;

    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      add_text_row (store, "Phase", gesture_phase_name (gdk_touchpad_event_get_gesture_phase (event)));
      add_int_row (store, "Fingers", gdk_touchpad_event_get_n_fingers (event));
      gdk_touchpad_event_get_deltas (event, &dx, &dy);
      tmp = g_strdup_printf ("%.2f %.f2", dx, dy);
      add_text_row (store, "Delta", tmp);
      g_free (tmp);
      if (type == GDK_TOUCHPAD_PINCH)
        {
          tmp = g_strdup_printf ("%.2f", gdk_touchpad_event_get_pinch_angle_delta (event));
          add_text_row (store, "Angle Delta", tmp);
          g_free (tmp);
          tmp = g_strdup_printf ("%.2f", gdk_touchpad_event_get_pinch_scale (event));
          add_text_row (store, "Scale", tmp);
          g_free (tmp);
        }
      break;

    default:
      /* FIXME */
      ;
    }

  if (type == GDK_MOTION_NOTIFY || type == GDK_SCROLL)
    {
      GdkTimeCoord *history;
      guint n_coords;

      history = gdk_event_get_history (event, &n_coords);
      if (history)
        {
          GString *s = g_string_new ("");

          for (int i = 0; i < n_coords; i++)
            {
              if (i > 0)
                g_string_append (s, "\n");

              g_string_append_printf (s, "%d", history[i].time);

              if (history[i].flags & (GDK_AXIS_FLAG_X|GDK_AXIS_FLAG_Y))
                g_string_append_printf (s, " Position %.2f %.2f", history[i].axes[GDK_AXIS_X], history[i].axes[GDK_AXIS_Y]);

              if (history[i].flags & (GDK_AXIS_FLAG_DELTA_X|GDK_AXIS_FLAG_DELTA_Y))
                g_string_append_printf (s, " Delta %.2f %.2f", history[i].axes[GDK_AXIS_DELTA_X], history[i].axes[GDK_AXIS_DELTA_Y]);

              for (int j = GDK_AXIS_PRESSURE; j < GDK_AXIS_LAST; j++)
                {
                  if (history[i].flags & (1 << j))
                    g_string_append_printf (s, " %s %.2f", axis_name (j), history[i].axes[j]);
                }
            }

          add_text_row (store, "History", s->str);

          g_string_free (s, TRUE);
          g_free (history);
        }
    }
}

static GskRenderNode *
get_selected_node (GtkInspectorRecorder *recorder)
{
  GtkTreeListRow *row_item;
  GdkPaintable *paintable;
  GskRenderNode *node;

  row_item = gtk_single_selection_get_selected_item (recorder->render_node_selection);
  if (row_item == NULL)
    return NULL;

  paintable = gtk_tree_list_row_get_item (row_item);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));
  g_object_unref (paintable);

  return node;
}

static void
render_node_list_selection_changed (GtkListBox           *list,
                                    GtkListBoxRow        *row,
                                    GtkInspectorRecorder *recorder)
{
  GskRenderNode *node;
  GdkPaintable *paintable;
  GtkTreeListRow *row_item;

  row_item = gtk_single_selection_get_selected_item (recorder->render_node_selection);

  gtk_widget_set_sensitive (recorder->render_node_save_button, row_item != NULL);
  gtk_widget_set_sensitive (recorder->render_node_clip_button, row_item != NULL);

  if (row_item == NULL)
    return;

  paintable = gtk_tree_list_row_get_item (row_item);

  gtk_picture_set_paintable (GTK_PICTURE (recorder->render_node_view), paintable);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));
  populate_render_node_properties (recorder->render_node_properties, node);

  g_object_unref (paintable);
}

static void
render_node_save_response (GObject *source,
                           GAsyncResult *result,
                           gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GskRenderNode *node = data;
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  if (file)
    {
      GBytes *bytes = gsk_render_node_serialize (node);

      if (!g_file_replace_contents (file,
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes),
                                    NULL,
                                    FALSE,
                                    0,
                                    NULL,
                                    NULL,
                                    &error))
        {
          GtkAlertDialog *alert;

          alert = gtk_alert_dialog_new (_("Saving RenderNode failed"));
          gtk_alert_dialog_set_detail (alert, error->message);
          gtk_alert_dialog_show (alert, GTK_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog))));
          g_object_unref (alert);
          g_error_free (error);
        }

      g_bytes_unref (bytes);
      g_object_unref (file);
    }
  else
    {
      g_print ("Error saving nodes: %s\n", error->message);
      g_error_free (error);
    }
}

static void
render_node_save (GtkButton            *button,
                  GtkInspectorRecorder *recorder)
{
  GskRenderNode *node;
  GtkFileDialog *dialog;
  char *filename, *nodename;

  node = get_selected_node (recorder);
  if (node == NULL)
    return;

  nodename = node_name (node);
  filename = g_strdup_printf ("%s.node", nodename);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_initial_name (dialog, filename);
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (recorder))),
                        NULL,
                        render_node_save_response, node);
  g_object_unref (dialog);
  g_free (filename);
  g_free (nodename);
}

static void
render_node_clip (GtkButton            *button,
                  GtkInspectorRecorder *recorder)
{
  GskRenderNode *node;
  GdkClipboard *clipboard;
  GBytes *bytes;
  GdkContentProvider *content;

  node = get_selected_node (recorder);
  if (node == NULL)
    return;

  bytes = gsk_render_node_serialize (node);
  content = gdk_content_provider_new_for_bytes ("text/plain;charset=utf-8", bytes);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (recorder));

  gdk_clipboard_set_content (clipboard, content);

  g_object_unref (content);
  g_bytes_unref (bytes);
}

static void
toggle_dark_mode (GtkToggleButton *button,
                  GParamSpec      *pspec,
                  gpointer         data)
{
  GtkWidget *picture = data;

  if (gtk_toggle_button_get_active (button))
    {
      gtk_widget_add_css_class (picture, "dark");
      gtk_widget_remove_css_class (picture, "light");
    }
  else
    {
      gtk_widget_remove_css_class (picture, "dark");
      gtk_widget_add_css_class (picture, "light");
    }
}

static void
setup_widget_for_recording (GtkListItemFactory *factory,
                            GtkListItem        *item,
                            gpointer            data)
{
  GtkWidget *row, *box, *label;

  row = g_object_new (GTK_TYPE_INSPECTOR_RECORDER_ROW, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new ("");
  gtk_box_append (GTK_BOX (box), label);

  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);

  gtk_widget_set_parent (box, row);

  gtk_list_item_set_child (item, row);
}

static char *
get_event_summary (GdkEvent *event)
{
  double x, y;
  GdkEventType type;
  const char *name;

  gdk_event_get_position (event, &x, &y);
  type = gdk_event_get_event_type (event);
  name = event_type_name (type);

  switch (type)
    {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_MOTION_NOTIFY:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_HOLD:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return g_strdup_printf ("%s (%.2f %.2f)", name, x, y);

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      {
        char *tmp, *ret;
        tmp = key_event_string (event);
        ret = g_strdup_printf ("%s %s\n", name, tmp);
        g_free (tmp);
        return ret;
      }

    case GDK_FOCUS_CHANGE:
      return g_strdup_printf ("%s %s", name, gdk_focus_event_get_in (event) ? "In" : "Out");

    case GDK_GRAB_BROKEN:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
    case GDK_PAD_GROUP_MODE:
    case GDK_DELETE:
      return g_strdup_printf ("%s", name);

    case GDK_SCROLL:
      if (gdk_scroll_event_get_direction (event) == GDK_SCROLL_SMOOTH)
        {
          gdk_scroll_event_get_deltas (event, &x, &y);
          return g_strdup_printf ("%s %.2f %.2f", name, x, y);
        }
      else
        {
          return g_strdup_printf ("%s %s", name, scroll_direction_name (gdk_scroll_event_get_direction (event)));
        }
      break;

    case GDK_EVENT_LAST:
    default:
      g_assert_not_reached ();
    }
}

static void
bind_widget_for_recording (GtkListItemFactory *factory,
                           GtkListItem        *item,
                           gpointer            data)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (data);
  GtkInspectorRecording *recording = gtk_list_item_get_item (item);
  GtkWidget *row, *box, *label, *label2;
  char *text;

  row = gtk_list_item_get_child (item);
  box = gtk_widget_get_first_child (row);
  label = gtk_widget_get_first_child (box);
  label2 = gtk_widget_get_next_sibling (label);

  g_object_set (row, "sequence", NULL, NULL);
  g_object_bind_property (recorder, "selected-sequence", row, "match-sequence", G_BINDING_SYNC_CREATE);

  gtk_label_set_use_markup (GTK_LABEL (label), FALSE);

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      gtk_label_set_label (GTK_LABEL (label), "Frame");
      gtk_label_set_use_markup (GTK_LABEL (label), FALSE);

      text = g_strdup_printf ("%.3f", gtk_inspector_recording_get_timestamp (recording) / 1000.0);
      gtk_label_set_label (GTK_LABEL (label2), text);
      g_free (text);
    }
  else if (GTK_INSPECTOR_IS_EVENT_RECORDING (recording))
    {
      GdkEvent *event = gtk_inspector_event_recording_get_event (GTK_INSPECTOR_EVENT_RECORDING (recording));

      g_object_set (row, "sequence", gdk_event_get_event_sequence (event), NULL);

      text = get_event_summary (event);
      gtk_label_set_label (GTK_LABEL (label), text);
      g_free (text);

      text = g_strdup_printf ("%.3f", gtk_inspector_recording_get_timestamp (recording) / 1000.0);
      gtk_label_set_label (GTK_LABEL (label2), text);
      g_free (text);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (label), "<b>Start of Recording</b>");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_label_set_label (GTK_LABEL (label2), "");
    }
}

static void
gtk_inspector_recorder_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);

  switch (param_id)
    {
    case PROP_RECORDING:
      g_value_set_boolean (value, recorder->recording != NULL);
      break;

    case PROP_DEBUG_NODES:
      g_value_set_boolean (value, recorder->debug_nodes);
      break;

    case PROP_HIGHLIGHT_SEQUENCES:
      g_value_set_boolean (value, recorder->highlight_sequences);
      break;

    case PROP_SELECTED_SEQUENCE:
      g_value_set_pointer (value, recorder->selected_sequence);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);

  switch (param_id)
    {
    case PROP_RECORDING:
      gtk_inspector_recorder_set_recording (recorder, g_value_get_boolean (value));
      break;

    case PROP_DEBUG_NODES:
      gtk_inspector_recorder_set_debug_nodes (recorder, g_value_get_boolean (value));
      break;

    case PROP_HIGHLIGHT_SEQUENCES:
      gtk_inspector_recorder_set_highlight_sequences (recorder, g_value_get_boolean (value));
      break;

    case PROP_SELECTED_SEQUENCE:
      recorder->selected_sequence = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_dispose (GObject *object)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);

  g_clear_object (&recorder->render_node_model);
  g_clear_object (&recorder->render_node_root_model);
  g_clear_object (&recorder->render_node_selection);

  gtk_widget_dispose_template (GTK_WIDGET (recorder), GTK_TYPE_INSPECTOR_RECORDER);

  G_OBJECT_CLASS (gtk_inspector_recorder_parent_class)->dispose (object);
}

static void
gtk_inspector_recorder_class_init (GtkInspectorRecorderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_inspector_recorder_get_property;
  object_class->set_property = gtk_inspector_recorder_set_property;
  object_class->dispose = gtk_inspector_recorder_dispose;

  props[PROP_RECORDING] =
    g_param_spec_boolean ("recording", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE);
  props[PROP_DEBUG_NODES] =
    g_param_spec_boolean ("debug-nodes", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE);

  props[PROP_HIGHLIGHT_SEQUENCES] = g_param_spec_boolean ("highlight-sequences", NULL, NULL, FALSE, G_PARAM_READWRITE);
  props[PROP_SELECTED_SEQUENCE] = g_param_spec_pointer ("selected-sequence", NULL, NULL, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/recorder.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, recordings);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, recordings_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, render_node_view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, render_node_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, render_node_save_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, render_node_clip_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, node_property_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, recording_data_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, event_view);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorRecorder, event_property_tree);

  gtk_widget_class_bind_template_callback (widget_class, recordings_clear_all);
  gtk_widget_class_bind_template_callback (widget_class, recording_selected);
  gtk_widget_class_bind_template_callback (widget_class, render_node_save);
  gtk_widget_class_bind_template_callback (widget_class, render_node_clip);
  //gtk_widget_class_bind_template_callback (widget_class, node_property_activated);
  gtk_widget_class_bind_template_callback (widget_class, toggle_dark_mode);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_inspector_recorder_init (GtkInspectorRecorder *recorder)
{
  GtkListItemFactory *factory;
  GtkSelectionModel *model;
  GtkColumnViewColumn *column;

  gtk_widget_init_template (GTK_WIDGET (recorder));

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_widget_for_recording), recorder);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_widget_for_recording), recorder);
  gtk_list_view_set_factory (GTK_LIST_VIEW (recorder->recordings_list), factory);
  g_object_unref (factory);

  recorder->render_node_root_model = g_list_store_new (GDK_TYPE_PAINTABLE);
  recorder->render_node_model = gtk_tree_list_model_new (g_object_ref (G_LIST_MODEL (recorder->render_node_root_model)),
                                                     FALSE,
                                                     TRUE,
                                                     create_list_model_for_render_node_paintable,
                                                     NULL, NULL);
  recorder->render_node_selection = gtk_single_selection_new (g_object_ref (G_LIST_MODEL (recorder->render_node_model)));
  g_signal_connect (recorder->render_node_selection, "notify::selected-item", G_CALLBACK (render_node_list_selection_changed), recorder);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_widget_for_render_node), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_widget_for_render_node), NULL);

  gtk_list_view_set_factory (GTK_LIST_VIEW (recorder->render_node_list), factory);
  g_object_unref (factory);
  gtk_list_view_set_model (GTK_LIST_VIEW (recorder->render_node_list),
                           GTK_SELECTION_MODEL (recorder->render_node_selection));

  recorder->render_node_properties = g_list_store_new (object_property_get_type ());
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (recorder->render_node_properties)));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (recorder->node_property_tree), model);
  g_object_unref (model);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (recorder->node_property_tree)), 0);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_property_name), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (recorder->node_property_tree)), 1);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_value_widgets), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_value_widgets), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  recorder->event_properties = g_list_store_new (object_property_get_type ());
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (recorder->event_properties)));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (recorder->event_property_tree), model);
  g_object_unref (model);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (recorder->event_property_tree)), 0);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_property_name), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (recorder->event_property_tree)), 1);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_value_widgets), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_value_widgets), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);
}

static void
gtk_inspector_recorder_add_recording (GtkInspectorRecorder  *recorder,
                                      GtkInspectorRecording *recording)
{
  g_list_store_append (G_LIST_STORE (recorder->recordings), recording);
}

void
gtk_inspector_recorder_set_recording (GtkInspectorRecorder *recorder,
                                      gboolean              recording)
{
  if (gtk_inspector_recorder_is_recording (recorder) == recording)
    return;

  if (recording)
    {
      recorder->recording = gtk_inspector_start_recording_new ();
      recorder->start_time = 0;
      gtk_inspector_recorder_add_recording (recorder, recorder->recording);
    }
  else
    {
      g_clear_object (&recorder->recording);
    }

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_RECORDING]);
}

gboolean
gtk_inspector_recorder_is_recording (GtkInspectorRecorder *recorder)
{
  return recorder->recording != NULL;
}

void
gtk_inspector_recorder_record_render (GtkInspectorRecorder *recorder,
                                      GtkWidget            *widget,
                                      GskRenderer          *renderer,
                                      GdkSurface           *surface,
                                      const cairo_region_t *region,
                                      GskRenderNode        *node)
{
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;
  gint64 frame_time;

  if (!gtk_inspector_recorder_is_recording (recorder))
    return;

  frame_clock = gtk_widget_get_frame_clock (widget);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (recorder->start_time == 0)
    {
      recorder->start_time = frame_time;
      frame_time = 0;
    }
  else
    {
      frame_time = frame_time - recorder->start_time;
    }

  recording = gtk_inspector_render_recording_new (frame_time,
                                                  gsk_renderer_get_profiler (renderer),
                                                  &(GdkRectangle) { 0, 0,
                                                    gdk_surface_get_width (surface),
                                                    gdk_surface_get_height (surface) },
                                                  region,
                                                  node);
  gtk_inspector_recorder_add_recording (recorder, recording);
  g_object_unref (recording);
}

void
gtk_inspector_recorder_record_event (GtkInspectorRecorder *recorder,
                                     GtkWidget            *widget,
                                     GdkEvent             *event)
{
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;
  gint64 frame_time;

  if (!gtk_inspector_recorder_is_recording (recorder))
    return;

  frame_clock = gtk_widget_get_frame_clock (widget);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (recorder->start_time == 0)
    {
      recorder->start_time = frame_time;
      frame_time = 0;
    }
  else
    {
      frame_time = frame_time - recorder->start_time;
    }

  recording = gtk_inspector_event_recording_new (frame_time, event);
  gtk_inspector_recorder_add_recording (recorder, recording);
  g_object_unref (recording);
}

void
gtk_inspector_recorder_set_debug_nodes (GtkInspectorRecorder *recorder,
                                        gboolean              debug_nodes)
{
  guint flags;

  if (recorder->debug_nodes == debug_nodes)
    return;

  recorder->debug_nodes = debug_nodes;

  flags = gtk_get_debug_flags ();

  if (debug_nodes)
    flags |= GTK_DEBUG_SNAPSHOT;
  else
    flags &= ~GTK_DEBUG_SNAPSHOT;

  gtk_set_debug_flags (flags);

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_DEBUG_NODES]);
}

void
gtk_inspector_recorder_set_highlight_sequences (GtkInspectorRecorder *recorder,
                                                gboolean              highlight_sequences)
{
  GdkEventSequence *sequence = NULL;

  if (recorder->highlight_sequences == highlight_sequences)
    return;

  recorder->highlight_sequences = highlight_sequences;

  if (highlight_sequences)
    {
      GtkSingleSelection *selection;
      GtkInspectorRecording *recording;
      GdkEvent *event;

      selection = GTK_SINGLE_SELECTION (gtk_list_view_get_model (GTK_LIST_VIEW (recorder->recordings_list)));
      recording = gtk_single_selection_get_selected_item (selection);

      if (GTK_INSPECTOR_IS_EVENT_RECORDING (recording))
        {
          event = gtk_inspector_event_recording_get_event (GTK_INSPECTOR_EVENT_RECORDING (recording));
          sequence = gdk_event_get_event_sequence (event);
        }
    }

  gtk_inspector_recorder_set_selected_sequence (recorder, sequence);

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_HIGHLIGHT_SEQUENCES]);
}

void
gtk_inspector_recorder_set_selected_sequence (GtkInspectorRecorder *recorder,
                                              GdkEventSequence     *sequence)
{
  if (recorder->selected_sequence == sequence)
    return;

  recorder->selected_sequence = sequence;

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_SELECTED_SEQUENCE]);
}

/* vim:set foldmethod=marker expandtab: */

