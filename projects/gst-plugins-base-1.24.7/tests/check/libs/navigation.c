/* GStreamer
 *
 * unit tests for the navigation interface library
 *
 * Copyright (C) 2009 Jan Schmidt <thaytan@noraisin.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/video/navigation.h>

#include <string.h>

#define TEST_ELEMENT_TYPE (test_element_get_type())

typedef struct TestElement TestElement;
typedef struct TestElementClass TestElementClass;

struct TestElement
{
  GstElement parent;

  GstNavigationEventType sent_type;
  const gchar *sent_key;
  gdouble sent_x, sent_y;
  gdouble sent_delta_x, sent_delta_y;
  gint sent_button;
  GstNavigationCommand sent_command;
};

struct TestElementClass
{
  GstElementClass parent_class;
};

GType test_element_get_type (void);

static void init_interface (GType type);
static void nav_send_event (GstNavigation * navigation, GstEvent * event);

G_DEFINE_TYPE_WITH_CODE (TestElement, test_element, GST_TYPE_ELEMENT,
    init_interface (g_define_type_id));

static void
test_element_navigation_interface_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = nav_send_event;
}

static void
init_interface (GType type)
{
  static const GInterfaceInfo navigation_iface_info = {
    (GInterfaceInitFunc) test_element_navigation_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type, GST_TYPE_NAVIGATION,
      &navigation_iface_info);
}

static void
test_element_class_init (TestElementClass * klass)
{
}

static void
test_element_init (TestElement * this)
{
}

static void
nav_send_event (GstNavigation * navigation, GstEvent * event)
{
  GstNavigationEventType etype = gst_navigation_event_get_type (event);
  TestElement *self = (TestElement *) (navigation);

  fail_if (etype == GST_NAVIGATION_EVENT_INVALID,
      "Received navigation event could not be parsed");
  fail_unless (etype == self->sent_type,
      "Received navigation event did not match sent");

  switch (etype) {
    case GST_NAVIGATION_EVENT_KEY_PRESS:
    case GST_NAVIGATION_EVENT_KEY_RELEASE:{
      const gchar *key;
      fail_unless (gst_navigation_event_parse_key_event (event, &key));
      fail_unless (strcmp (key, self->sent_key) == 0);
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:{
      gint button;
      gdouble x, y;
      fail_unless (gst_navigation_event_parse_mouse_button_event (event,
              &button, &x, &y));
      fail_unless (button == self->sent_button);
      fail_unless (x == self->sent_x);
      fail_unless (y == self->sent_y);
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_MOVE:{
      gdouble x, y;
      fail_unless (gst_navigation_event_parse_mouse_move_event (event, &x, &y));
      fail_unless (x == self->sent_x);
      fail_unless (y == self->sent_y);
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_SCROLL:{
      gdouble x, y, delta_x, delta_y;
      fail_unless (gst_navigation_event_parse_mouse_scroll_event (event, &x, &y,
              &delta_x, &delta_y));
      fail_unless (x == self->sent_x);
      fail_unless (y == self->sent_y);
      fail_unless (delta_x == self->sent_delta_x);
      fail_unless (delta_y == self->sent_delta_y);
      break;
    }
    case GST_NAVIGATION_EVENT_COMMAND:{
      GstNavigationCommand cmd;
      fail_unless (gst_navigation_event_parse_command (event, &cmd));
      fail_unless (cmd == self->sent_command);
    }
    default:
      break;
  }

  gst_event_unref (event);
}

GST_START_TEST (test_events)
{
  /* Create an empty GstElement that has a GstNavigation interface and then
   * send some navigation events and validate them */
  TestElement *test_element =
      (TestElement *) g_object_new (TEST_ELEMENT_TYPE, NULL);
  GstNavigationCommand cmds[] = {
    GST_NAVIGATION_COMMAND_MENU1, GST_NAVIGATION_COMMAND_MENU2,
    GST_NAVIGATION_COMMAND_MENU3, GST_NAVIGATION_COMMAND_MENU4,
    GST_NAVIGATION_COMMAND_MENU5, GST_NAVIGATION_COMMAND_MENU6,
    GST_NAVIGATION_COMMAND_MENU7, GST_NAVIGATION_COMMAND_LEFT,
    GST_NAVIGATION_COMMAND_RIGHT, GST_NAVIGATION_COMMAND_UP,
    GST_NAVIGATION_COMMAND_DOWN, GST_NAVIGATION_COMMAND_ACTIVATE,
    GST_NAVIGATION_COMMAND_PREV_ANGLE, GST_NAVIGATION_COMMAND_NEXT_ANGLE
  };
  gint i;

  test_element->sent_type = GST_NAVIGATION_EVENT_KEY_PRESS;
  test_element->sent_key = "1";
  gst_navigation_send_key_event (GST_NAVIGATION (test_element), "key-press",
      "1");
  gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
      gst_navigation_event_new_key_press ("1", GST_NAVIGATION_MODIFIER_NONE));

  test_element->sent_type = GST_NAVIGATION_EVENT_KEY_RELEASE;
  test_element->sent_key = "2";
  gst_navigation_send_key_event (GST_NAVIGATION (test_element), "key-release",
      "2");
  gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
      gst_navigation_event_new_key_release ("2", GST_NAVIGATION_MODIFIER_NONE));

  test_element->sent_type = GST_NAVIGATION_EVENT_MOUSE_MOVE;
  test_element->sent_x = 50;
  test_element->sent_y = 100;
  gst_navigation_send_mouse_event (GST_NAVIGATION (test_element), "mouse-move",
      0, 50, 100);
  gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
      gst_navigation_event_new_mouse_move (50, 100,
          GST_NAVIGATION_MODIFIER_NONE));

  test_element->sent_type = GST_NAVIGATION_EVENT_MOUSE_SCROLL;
  test_element->sent_x = 60;
  test_element->sent_y = 120;
  test_element->sent_delta_x = 2;
  test_element->sent_delta_y = 3;
  gst_navigation_send_mouse_scroll_event (GST_NAVIGATION (test_element),
      60, 120, 2, 3);
  gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
      gst_navigation_event_new_mouse_scroll (60, 120, 2, 3,
          GST_NAVIGATION_MODIFIER_NONE));

  test_element->sent_type = GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS;
  test_element->sent_x = 10;
  test_element->sent_y = 20;
  test_element->sent_button = 1;
  gst_navigation_send_mouse_event (GST_NAVIGATION (test_element),
      "mouse-button-press", 1, 10, 20);
  gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
      gst_navigation_event_new_mouse_button_press (1, 10, 20,
          GST_NAVIGATION_MODIFIER_NONE));

  for (i = 0; i < G_N_ELEMENTS (cmds); i++) {
    test_element->sent_type = GST_NAVIGATION_EVENT_COMMAND;
    test_element->sent_command = cmds[i];
    gst_navigation_send_command (GST_NAVIGATION (test_element), cmds[i]);
    gst_navigation_send_event_simple (GST_NAVIGATION (test_element),
        gst_navigation_event_new_command (cmds[i]));
  }

  gst_object_unref (test_element);
}

GST_END_TEST;

GST_START_TEST (test_messages)
{
  GstMessage *m;
  /* GST_NAVIGATION_MESSAGE_MOUSE_OVER */
  {
    gboolean active;
    m = gst_navigation_message_new_mouse_over (NULL, TRUE);
    fail_if (m == NULL);
    fail_unless (gst_navigation_message_get_type (m) ==
        GST_NAVIGATION_MESSAGE_MOUSE_OVER);
    fail_unless (GST_MESSAGE_SRC (m) == NULL);
    fail_unless (gst_navigation_message_parse_mouse_over (m, &active));
    fail_unless (active == TRUE);
    gst_message_unref (m);

    m = gst_navigation_message_new_mouse_over (NULL, FALSE);
    fail_if (m == NULL);
    fail_unless (GST_MESSAGE_SRC (m) == NULL);
    fail_unless (gst_navigation_message_get_type (m) ==
        GST_NAVIGATION_MESSAGE_MOUSE_OVER);
    fail_unless (gst_navigation_message_parse_mouse_over (m, &active));
    fail_unless (active == FALSE);
    gst_message_unref (m);
  }

  /* GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED */
  {
    m = gst_navigation_message_new_commands_changed (NULL);
    fail_if (m == NULL);
    fail_unless (GST_MESSAGE_SRC (m) == NULL);
    fail_unless (gst_navigation_message_get_type (m) ==
        GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED);
    gst_message_unref (m);
  }

  /* GST_NAVIGATION_MESSAGE_ANGLES_CHANGED */
  {
    guint angle, angles;
    m = gst_navigation_message_new_angles_changed (NULL, 1, 5);
    fail_if (m == NULL);
    fail_unless (GST_MESSAGE_SRC (m) == NULL);
    fail_unless (gst_navigation_message_get_type (m) ==
        GST_NAVIGATION_MESSAGE_ANGLES_CHANGED);
    fail_unless (gst_navigation_message_parse_angles_changed (m, &angle,
            &angles));
    fail_unless (angle == 1);
    fail_unless (angles == 5);
    gst_message_unref (m);
  }
}

GST_END_TEST;

GST_START_TEST (test_queries)
{
  GstQuery *q;

  /* GST_NAVIGATION_QUERY_COMMANDS */
  {
    guint n;
    GstNavigationCommand cmd;

    q = gst_navigation_query_new_commands ();
    fail_unless (q != NULL);
    fail_unless (gst_navigation_query_get_type (q) ==
        GST_NAVIGATION_QUERY_COMMANDS);
    gst_navigation_query_set_commands (q, 3, GST_NAVIGATION_COMMAND_LEFT,
        GST_NAVIGATION_COMMAND_MENU1, GST_NAVIGATION_COMMAND_MENU5);
    fail_unless (gst_navigation_query_parse_commands_length (q, &n));
    fail_unless (n == 3);
    fail_unless (gst_navigation_query_parse_commands_nth (q, 1, &cmd));
    fail_unless (cmd == GST_NAVIGATION_COMMAND_MENU1);

    fail_unless (gst_navigation_query_parse_commands_length (q, NULL));
    fail_unless (gst_navigation_query_parse_commands_nth (q, 2, NULL));

    gst_query_unref (q);
  }

  /* GST_NAVIGATION_QUERY_ANGLES */
  {
    guint angle, angles;
    q = gst_navigation_query_new_angles ();
    fail_unless (q != NULL);
    fail_unless (gst_navigation_query_get_type (q) ==
        GST_NAVIGATION_QUERY_ANGLES);
    gst_navigation_query_set_angles (q, 4, 8);
    fail_unless (gst_navigation_query_parse_angles (q, &angle, &angles));
    fail_unless (angle == 4);
    fail_unless (angles == 8);

    fail_unless (gst_navigation_query_parse_angles (q, NULL, &angles));
    fail_unless (gst_navigation_query_parse_angles (q, &angle, NULL));
    fail_unless (gst_navigation_query_parse_angles (q, NULL, NULL));

    gst_query_unref (q);
  }

}

GST_END_TEST;

static Suite *
navigation_suite (void)
{
  Suite *s = suite_create ("navigation interface");
  TCase *tc_chain = tcase_create ("notifications");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_events);
  tcase_add_test (tc_chain, test_messages);
  tcase_add_test (tc_chain, test_queries);

  return s;
}

GST_CHECK_MAIN (navigation);
