/* GStreamer Navigation
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2007-2009 Jan Schmidt <thaytan@noraisin.net>
 *
 * navigation.c: navigation event virtual class function wrappers
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

/**
 * SECTION:gstnavigation
 * @title: GstNavigation
 * @short_description: Interface for creating, sending and parsing navigation
 * events.
 *
 * The Navigation interface is used for creating and injecting navigation
 * related events such as mouse button presses, cursor motion and key presses.
 * The associated library also provides methods for parsing received events, and
 * for sending and receiving navigation related bus events. One main usecase is
 * DVD menu navigation.
 *
 * The main parts of the API are:
 *
 * * The GstNavigation interface, implemented by elements which provide an
 *   application with the ability to create and inject navigation events into
 *   the pipeline.
 * * GstNavigation event handling API. GstNavigation events are created in
 *   response to calls on a GstNavigation interface implementation, and sent in
 *   the pipeline. Upstream elements can use the navigation event API functions
 *   to parse the contents of received messages.
 *
 * * GstNavigation message handling API. GstNavigation messages may be sent on
 *   the message bus to inform applications of navigation related changes in the
 *   pipeline, such as the mouse moving over a clickable region, or the set of
 *   available angles changing.
 *
 * The GstNavigation message functions provide functions for creating and
 * parsing custom bus messages for signaling GstNavigation changes.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/navigation.h>
#include <gst/video/video-enumtypes.h>

#define GST_NAVIGATION_MESSAGE_NAME "GstNavigationMessage"
#define GST_NAVIGATION_QUERY_NAME "GstNavigationQuery"
#define GST_NAVIGATION_EVENT_NAME "application/x-gst-navigation"

#define WARN_IF_FAIL(exp,msg) if(G_UNLIKELY(!(exp))){g_warning("%s",(msg));}

G_DEFINE_INTERFACE (GstNavigation, gst_navigation, 0);

static void
gst_navigation_default_send_event_simple (GstNavigation * navigation,
    GstEvent * event)
{
  GstNavigationInterface *iface = GST_NAVIGATION_GET_INTERFACE (navigation);

  if (iface->send_event) {
    iface->send_event (navigation,
        gst_structure_copy (gst_event_get_structure (event)));
  } else {
    gst_event_unref (event);
  }
}

static void
gst_navigation_default_init (GstNavigationInterface * iface)
{
  /* default virtual functions */
  iface->send_event = NULL;
  iface->send_event_simple = gst_navigation_default_send_event_simple;
}

/* The interface implementer should make sure that the object can handle
 * the event. */
void
gst_navigation_send_event (GstNavigation * navigation, GstStructure * structure)
{
  GstNavigationInterface *iface = GST_NAVIGATION_GET_INTERFACE (navigation);

  if (iface->send_event) {
    iface->send_event (navigation, structure);
  } else if (iface->send_event_simple) {
    iface->send_event_simple (navigation, gst_event_new_navigation (structure));
  } else {
    gst_structure_free (structure);
  }
}

/**
 * gst_navigation_send_key_event:
 * @navigation: The navigation interface instance
 * @event: The type of the key event. Recognised values are "key-press" and
 * "key-release"
 * @key: Character representation of the key. This is typically as produced
 * by XKeysymToString.
 */
void
gst_navigation_send_key_event (GstNavigation * navigation, const char *event,
    const char *key)
{
  g_return_if_fail (g_strcmp0 (event, "key-press") == 0 ||
      g_strcmp0 (event, "key-release") == 0);

  gst_navigation_send_event (navigation,
      gst_structure_new (GST_NAVIGATION_EVENT_NAME, "event", G_TYPE_STRING,
          event, "key", G_TYPE_STRING, key, NULL));
}

/**
 * gst_navigation_send_mouse_event:
 * @navigation: The navigation interface instance
 * @event: The type of mouse event, as a text string. Recognised values are
 * "mouse-button-press", "mouse-button-release" and "mouse-move".
 * @button: The button number of the button being pressed or released. Pass 0
 * for mouse-move events.
 * @x: The x coordinate of the mouse event.
 * @y: The y coordinate of the mouse event.
 *
 * Sends a mouse event to the navigation interface. Mouse event coordinates
 * are sent relative to the display space of the related output area. This is
 * usually the size in pixels of the window associated with the element
 * implementing the #GstNavigation interface.
 *
 */
void
gst_navigation_send_mouse_event (GstNavigation * navigation, const char *event,
    int button, double x, double y)
{
  g_return_if_fail (g_strcmp0 (event, "mouse-button-press") == 0 ||
      g_strcmp0 (event, "mouse-button-release") == 0 ||
      g_strcmp0 (event, "mouse-move") == 0);

  gst_navigation_send_event (navigation,
      gst_structure_new (GST_NAVIGATION_EVENT_NAME, "event", G_TYPE_STRING,
          event, "button", G_TYPE_INT, button, "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y, NULL));
}

/**
 * gst_navigation_send_mouse_scroll_event:
 * @navigation: The navigation interface instance
 * @x: The x coordinate of the mouse event.
 * @y: The y coordinate of the mouse event.
 * @delta_x: The delta_x coordinate of the mouse event.
 * @delta_y: The delta_y coordinate of the mouse event.
 *
 * Sends a mouse scroll event to the navigation interface. Mouse event coordinates
 * are sent relative to the display space of the related output area. This is
 * usually the size in pixels of the window associated with the element
 * implementing the #GstNavigation interface.
 *
 * Since: 1.18
 */
void
gst_navigation_send_mouse_scroll_event (GstNavigation * navigation,
    double x, double y, double delta_x, double delta_y)
{
  gst_navigation_send_event (navigation,
      gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "mouse-scroll",
          "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "delta_pointer_x", G_TYPE_DOUBLE, delta_x,
          "delta_pointer_y", G_TYPE_DOUBLE, delta_y, NULL));
}

/**
 * gst_navigation_send_command:
 * @navigation: The navigation interface instance
 * @command: The command to issue
 *
 * Sends the indicated command to the navigation interface.
 */
void
gst_navigation_send_command (GstNavigation * navigation,
    GstNavigationCommand command)
{
  gst_navigation_send_event (navigation,
      gst_structure_new (GST_NAVIGATION_EVENT_NAME, "event", G_TYPE_STRING,
          "command", "command-code", G_TYPE_UINT, (guint) command, NULL));
}

/**
 * gst_navigation_send_event_simple:
 * @navigation: The navigation interface instance
 * @event: (transfer full): The event to send
 *
 * Sends an event to the navigation interface.
 * Since: 1.22
 */
void
gst_navigation_send_event_simple (GstNavigation * navigation, GstEvent * event)
{
  GstNavigationInterface *iface = GST_NAVIGATION_GET_INTERFACE (navigation);

  g_return_if_fail (GST_EVENT_TYPE (event) == GST_EVENT_NAVIGATION);

  if (iface->send_event_simple) {
    iface->send_event_simple (navigation, event);
  } else if (iface->send_event) {
    iface->send_event (navigation,
        gst_structure_copy (gst_event_get_structure (event)));
    gst_event_unref (event);
  } else {
    gst_event_unref (event);
  }
}

/* Navigation Queries */

#define GST_NAVIGATION_QUERY_HAS_TYPE(query,query_type) \
(gst_navigation_query_get_type (query) == GST_NAVIGATION_QUERY_ ## query_type)

/**
 * gst_navigation_query_get_type:
 * @query: The query to inspect
 *
 * Inspect a #GstQuery and return the #GstNavigationQueryType associated with
 * it if it is a #GstNavigation query.
 *
 * Returns: The #GstNavigationQueryType of the query, or
 * #GST_NAVIGATION_QUERY_INVALID
 */
GstNavigationQueryType
gst_navigation_query_get_type (GstQuery * query)
{
  const GstStructure *s;
  const gchar *q_type;

  if (query == NULL || GST_QUERY_TYPE (query) != GST_QUERY_CUSTOM)
    return GST_NAVIGATION_QUERY_INVALID;

  s = gst_query_get_structure (query);
  if (s == NULL || !gst_structure_has_name (s, GST_NAVIGATION_QUERY_NAME))
    return GST_NAVIGATION_QUERY_INVALID;

  q_type = gst_structure_get_string (s, "type");
  if (q_type == NULL)
    return GST_NAVIGATION_QUERY_INVALID;

  if (g_str_equal (q_type, "commands"))
    return GST_NAVIGATION_QUERY_COMMANDS;
  else if (g_str_equal (q_type, "angles"))
    return GST_NAVIGATION_QUERY_ANGLES;

  return GST_NAVIGATION_QUERY_INVALID;
}

/**
 * gst_navigation_query_new_commands:
 *
 * Create a new #GstNavigation commands query. When executed, it will
 * query the pipeline for the set of currently available commands.
 *
 * Returns: The new query.
 */
GstQuery *
gst_navigation_query_new_commands (void)
{
  GstQuery *query;
  GstStructure *structure;

  structure = gst_structure_new (GST_NAVIGATION_QUERY_NAME,
      "type", G_TYPE_STRING, "commands", NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, structure);

  return query;
}

static void
gst_query_list_add_command (GValue * list, GstNavigationCommand val)
{
  GValue item = { 0, };

  g_value_init (&item, GST_TYPE_NAVIGATION_COMMAND);
  g_value_set_enum (&item, val);
  gst_value_list_append_value (list, &item);
  g_value_unset (&item);
}

/**
 * gst_navigation_query_set_commands:
 * @query: a #GstQuery
 * @n_cmds: the number of commands to set.
 * @...: A list of @GstNavigationCommand values, @n_cmds entries long.
 *
 * Set the #GstNavigation command query result fields in @query. The number
 * of commands passed must be equal to @n_commands.
 */
void
gst_navigation_query_set_commands (GstQuery * query, gint n_cmds, ...)
{
  va_list ap;
  GValue list = { 0, };
  GstStructure *structure;
  gint i;

  g_return_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, COMMANDS));

  g_value_init (&list, GST_TYPE_LIST);

  va_start (ap, n_cmds);
  for (i = 0; i < n_cmds; i++) {
    GstNavigationCommand val = va_arg (ap, GstNavigationCommand);
    gst_query_list_add_command (&list, val);
  }
  va_end (ap);

  structure = gst_query_writable_structure (query);
  gst_structure_take_value (structure, "commands", &list);
}

/**
 * gst_navigation_query_set_commandsv:
 * @query: a #GstQuery
 * @n_cmds: the number of commands to set.
 * @cmds: (array length=n_cmds): An array containing @n_cmds
 *     @GstNavigationCommand values.
 *
 * Set the #GstNavigation command query result fields in @query. The number
 * of commands passed must be equal to @n_commands.
 */
void
gst_navigation_query_set_commandsv (GstQuery * query, gint n_cmds,
    GstNavigationCommand * cmds)
{
  GValue list = { 0, };
  GstStructure *structure;
  gint i;

  g_return_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, COMMANDS));

  g_value_init (&list, GST_TYPE_LIST);
  for (i = 0; i < n_cmds; i++) {
    gst_query_list_add_command (&list, cmds[i]);
  }
  structure = gst_query_writable_structure (query);
  gst_structure_take_value (structure, "commands", &list);
}

/**
 * gst_navigation_query_parse_commands_length:
 * @query: a #GstQuery
 * @n_cmds: (out) (optional): the number of commands in this query.
 *
 * Parse the number of commands in the #GstNavigation commands @query.
 *
 * Returns: %TRUE if the query could be successfully parsed. %FALSE if not.
 */
gboolean
gst_navigation_query_parse_commands_length (GstQuery * query, guint * n_cmds)
{
  const GstStructure *structure;
  const GValue *list;

  g_return_val_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, COMMANDS), FALSE);

  if (n_cmds == NULL)
    return TRUE;

  structure = gst_query_get_structure (query);
  list = gst_structure_get_value (structure, "commands");
  if (list == NULL)
    *n_cmds = 0;
  else
    *n_cmds = gst_value_list_get_size (list);

  return TRUE;
}

/**
 * gst_navigation_query_parse_commands_nth:
 * @query: a #GstQuery
 * @nth: the nth command to retrieve.
 * @cmd: (out) (optional): a pointer to store the nth command into.
 *
 * Parse the #GstNavigation command query and retrieve the @nth command from
 * it into @cmd. If the list contains less elements than @nth, @cmd will be
 * set to #GST_NAVIGATION_COMMAND_INVALID.
 *
 * Returns: %TRUE if the query could be successfully parsed. %FALSE if not.
 */
gboolean
gst_navigation_query_parse_commands_nth (GstQuery * query, guint nth,
    GstNavigationCommand * cmd)
{
  const GstStructure *structure;
  const GValue *list;

  g_return_val_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, COMMANDS), FALSE);

  if (cmd == NULL)
    return TRUE;

  structure = gst_query_get_structure (query);
  list = gst_structure_get_value (structure, "commands");
  if (list == NULL) {
    *cmd = GST_NAVIGATION_COMMAND_INVALID;
  } else {
    if (nth < gst_value_list_get_size (list)) {
      *cmd = (GstNavigationCommand)
          g_value_get_enum (gst_value_list_get_value (list, nth));
    } else
      *cmd = GST_NAVIGATION_COMMAND_INVALID;
  }

  return TRUE;
}

/**
 * gst_navigation_query_new_angles:
 *
 * Create a new #GstNavigation angles query. When executed, it will
 * query the pipeline for the set of currently available angles, which may be
 * greater than one in a multiangle video.
 *
 * Returns: The new query.
 */
GstQuery *
gst_navigation_query_new_angles (void)
{
  GstQuery *query;
  GstStructure *structure;

  structure = gst_structure_new (GST_NAVIGATION_QUERY_NAME,
      "type", G_TYPE_STRING, "angles", NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, structure);

  return query;
}

/**
 * gst_navigation_query_set_angles:
 * @query: a #GstQuery
 * @cur_angle: the current viewing angle to set.
 * @n_angles: the number of viewing angles to set.
 *
 * Set the #GstNavigation angles query result field in @query.
 */
void
gst_navigation_query_set_angles (GstQuery * query, guint cur_angle,
    guint n_angles)
{
  GstStructure *structure;

  g_return_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, ANGLES));

  structure = gst_query_writable_structure (query);
  gst_structure_set (structure,
      "angle", G_TYPE_UINT, cur_angle, "angles", G_TYPE_UINT, n_angles, NULL);
}

/**
 * gst_navigation_query_parse_angles:
 * @query: a #GstQuery
 * @cur_angle: (out) (optional): Pointer to a #guint into which to store the
 *     currently selected angle value from the query, or NULL
 * @n_angles: (out) (optional): Pointer to a #guint into which to store the
 *     number of angles value from the query, or NULL
 *
 * Parse the current angle number in the #GstNavigation angles @query into the
 * #guint pointed to by the @cur_angle variable, and the number of available
 * angles into the #guint pointed to by the @n_angles variable.
 *
 * Returns: %TRUE if the query could be successfully parsed. %FALSE if not.
 */
gboolean
gst_navigation_query_parse_angles (GstQuery * query, guint * cur_angle,
    guint * n_angles)
{
  const GstStructure *structure;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_QUERY_HAS_TYPE (query, ANGLES), FALSE);

  structure = gst_query_get_structure (query);

  if (cur_angle)
    ret &= gst_structure_get_uint (structure, "angle", cur_angle);

  if (n_angles)
    ret &= gst_structure_get_uint (structure, "angles", n_angles);

  WARN_IF_FAIL (ret, "Couldn't extract details from angles query");

  return ret;
}

/* Navigation Messages */

#define GST_NAVIGATION_MESSAGE_HAS_TYPE(msg,msg_type) \
(gst_navigation_message_get_type (msg) == GST_NAVIGATION_MESSAGE_ ## msg_type)

/**
 * gst_navigation_message_get_type:
 * @message: A #GstMessage to inspect.
 *
 * Check a bus message to see if it is a #GstNavigation event, and return
 * the #GstNavigationMessageType identifying the type of the message if so.
 *
 * Returns: The type of the #GstMessage, or
 * #GST_NAVIGATION_MESSAGE_INVALID if the message is not a #GstNavigation
 * notification.
 */
GstNavigationMessageType
gst_navigation_message_get_type (GstMessage * message)
{
  const GstStructure *s;
  const gchar *m_type;

  if (message == NULL || GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_NAVIGATION_MESSAGE_INVALID;

  s = gst_message_get_structure (message);
  if (s == NULL || !gst_structure_has_name (s, GST_NAVIGATION_MESSAGE_NAME))
    return GST_NAVIGATION_MESSAGE_INVALID;

  m_type = gst_structure_get_string (s, "type");
  if (m_type == NULL)
    return GST_NAVIGATION_MESSAGE_INVALID;

  if (g_str_equal (m_type, "mouse-over"))
    return GST_NAVIGATION_MESSAGE_MOUSE_OVER;
  else if (g_str_equal (m_type, "commands-changed"))
    return GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED;
  else if (g_str_equal (m_type, "angles-changed"))
    return GST_NAVIGATION_MESSAGE_ANGLES_CHANGED;
  else if (g_str_equal (m_type, "event"))
    return GST_NAVIGATION_MESSAGE_EVENT;

  return GST_NAVIGATION_MESSAGE_INVALID;
}

/**
 * gst_navigation_message_new_mouse_over:
 * @src: A #GstObject to set as source of the new message.
 * @active: %TRUE if the mouse has entered a clickable area of the display.
 * %FALSE if it over a non-clickable area.
 *
 * Creates a new #GstNavigation message with type
 * #GST_NAVIGATION_MESSAGE_MOUSE_OVER.
 *
 * Returns: The new #GstMessage.
 */
GstMessage *
gst_navigation_message_new_mouse_over (GstObject * src, gboolean active)
{
  GstStructure *s;
  GstMessage *m;

  s = gst_structure_new (GST_NAVIGATION_MESSAGE_NAME,
      "type", G_TYPE_STRING, "mouse-over", "active", G_TYPE_BOOLEAN, active,
      NULL);

  m = gst_message_new_custom (GST_MESSAGE_ELEMENT, src, s);

  return m;
}

/**
 * gst_navigation_message_parse_mouse_over:
 * @message: A #GstMessage to inspect.
 * @active: (out) (optional): A pointer to a gboolean to receive the
 *     active/inactive state, or NULL.
 *
 * Parse a #GstNavigation message of type #GST_NAVIGATION_MESSAGE_MOUSE_OVER
 * and extract the active/inactive flag. If the mouse over event is marked
 * active, it indicates that the mouse is over a clickable area.
 *
 * Returns: %TRUE if the message could be successfully parsed. %FALSE if not.
 */
gboolean
gst_navigation_message_parse_mouse_over (GstMessage * message,
    gboolean * active)
{
  if (!GST_NAVIGATION_MESSAGE_HAS_TYPE (message, MOUSE_OVER))
    return FALSE;

  if (active) {
    const GstStructure *s = gst_message_get_structure (message);
    if (!gst_structure_get_boolean (s, "active", active))
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_navigation_message_new_event:
 * @src: A #GstObject to set as source of the new message.
 * @event: (transfer none): A navigation #GstEvent
 *
 * Creates a new #GstNavigation message with type
 * #GST_NAVIGATION_MESSAGE_EVENT.
 *
 * Returns: The new #GstMessage.
 *
 * Since: 1.6
 */
GstMessage *
gst_navigation_message_new_event (GstObject * src, GstEvent * event)
{
  GstStructure *s;
  GstMessage *m;

  s = gst_structure_new (GST_NAVIGATION_MESSAGE_NAME,
      "type", G_TYPE_STRING, "event", "event", GST_TYPE_EVENT, event, NULL);

  m = gst_message_new_custom (GST_MESSAGE_ELEMENT, src, s);

  return m;
}

/**
 * gst_navigation_message_parse_event:
 * @message: A #GstMessage to inspect.
 * @event: (out) (optional) (transfer full): a pointer to a #GstEvent to receive
 *     the contained navigation event.
 *
 * Parse a #GstNavigation message of type #GST_NAVIGATION_MESSAGE_EVENT
 * and extract contained #GstEvent. The caller must unref the @event when done
 * with it.
 *
 * Returns: %TRUE if the message could be successfully parsed. %FALSE if not.
 *
 * Since: 1.6
 */
gboolean
gst_navigation_message_parse_event (GstMessage * message, GstEvent ** event)
{
  if (!GST_NAVIGATION_MESSAGE_HAS_TYPE (message, EVENT))
    return FALSE;

  if (event) {
    const GstStructure *s = gst_message_get_structure (message);
    if (!gst_structure_get (s, "event", GST_TYPE_EVENT, event, NULL))
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_navigation_message_new_commands_changed:
 * @src: A #GstObject to set as source of the new message.
 *
 * Creates a new #GstNavigation message with type
 * #GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED
 *
 * Returns: The new #GstMessage.
 */
GstMessage *
gst_navigation_message_new_commands_changed (GstObject * src)
{
  GstStructure *s;
  GstMessage *m;

  s = gst_structure_new (GST_NAVIGATION_MESSAGE_NAME,
      "type", G_TYPE_STRING, "commands-changed", NULL);

  m = gst_message_new_custom (GST_MESSAGE_ELEMENT, src, s);

  return m;
}

/**
 * gst_navigation_message_new_angles_changed:
 * @src: A #GstObject to set as source of the new message.
 * @cur_angle: The currently selected angle.
 * @n_angles: The number of viewing angles now available.
 *
 * Creates a new #GstNavigation message with type
 * #GST_NAVIGATION_MESSAGE_ANGLES_CHANGED for notifying an application
 * that the current angle, or current number of angles available in a
 * multiangle video has changed.
 *
 * Returns: The new #GstMessage.
 */
GstMessage *
gst_navigation_message_new_angles_changed (GstObject * src, guint cur_angle,
    guint n_angles)
{
  GstStructure *s;
  GstMessage *m;

  s = gst_structure_new (GST_NAVIGATION_MESSAGE_NAME,
      "type", G_TYPE_STRING, "angles-changed",
      "angle", G_TYPE_UINT, cur_angle, "angles", G_TYPE_UINT, n_angles, NULL);

  m = gst_message_new_custom (GST_MESSAGE_ELEMENT, src, s);

  return m;
}

/**
 * gst_navigation_message_parse_angles_changed:
 * @message: A #GstMessage to inspect.
 * @cur_angle: (out) (optional): A pointer to a #guint to receive the new
 *     current angle number, or NULL
 * @n_angles: (out) (optional): A pointer to a #guint to receive the new angle
 *     count, or NULL.
 *
 * Parse a #GstNavigation message of type GST_NAVIGATION_MESSAGE_ANGLES_CHANGED
 * and extract the @cur_angle and @n_angles parameters.
 *
 * Returns: %TRUE if the message could be successfully parsed. %FALSE if not.
 */
gboolean
gst_navigation_message_parse_angles_changed (GstMessage * message,
    guint * cur_angle, guint * n_angles)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_MESSAGE_HAS_TYPE (message,
          ANGLES_CHANGED), FALSE);

  s = gst_message_get_structure (message);
  if (cur_angle)
    ret &= gst_structure_get_uint (s, "angle", cur_angle);

  if (n_angles)
    ret &= gst_structure_get_uint (s, "angles", n_angles);

  WARN_IF_FAIL (ret, "Couldn't extract details from angles-changed event");

  return ret;
}

#define GST_NAVIGATION_EVENT_HAS_TYPE(event,event_type) \
(gst_navigation_event_get_type (event) == GST_NAVIGATION_EVENT_ ## event_type)

/**
 * gst_navigation_event_get_type:
 * @event: A #GstEvent to inspect.
 *
 * Inspect a #GstEvent and return the #GstNavigationEventType of the event, or
 * #GST_NAVIGATION_EVENT_INVALID if the event is not a #GstNavigation event.
 */
GstNavigationEventType
gst_navigation_event_get_type (GstEvent * event)
{
  const GstStructure *s;
  const gchar *e_type;

  if (event == NULL || GST_EVENT_TYPE (event) != GST_EVENT_NAVIGATION)
    return GST_NAVIGATION_EVENT_INVALID;

  s = gst_event_get_structure (event);
  if (s == NULL || !gst_structure_has_name (s, GST_NAVIGATION_EVENT_NAME))
    return GST_NAVIGATION_EVENT_INVALID;

  e_type = gst_structure_get_string (s, "event");
  if (e_type == NULL)
    return GST_NAVIGATION_EVENT_INVALID;

  if (g_str_equal (e_type, "mouse-button-press"))
    return GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS;
  else if (g_str_equal (e_type, "mouse-button-release"))
    return GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE;
  else if (g_str_equal (e_type, "mouse-move"))
    return GST_NAVIGATION_EVENT_MOUSE_MOVE;
  else if (g_str_equal (e_type, "mouse-scroll"))
    return GST_NAVIGATION_EVENT_MOUSE_SCROLL;
  else if (g_str_equal (e_type, "key-press"))
    return GST_NAVIGATION_EVENT_KEY_PRESS;
  else if (g_str_equal (e_type, "key-release"))
    return GST_NAVIGATION_EVENT_KEY_RELEASE;
  else if (g_str_equal (e_type, "command"))
    return GST_NAVIGATION_EVENT_COMMAND;
  else if (g_str_equal (e_type, "touch-down"))
    return GST_NAVIGATION_EVENT_TOUCH_DOWN;
  else if (g_str_equal (e_type, "touch-up"))
    return GST_NAVIGATION_EVENT_TOUCH_UP;
  else if (g_str_equal (e_type, "touch-cancel"))
    return GST_NAVIGATION_EVENT_TOUCH_CANCEL;
  else if (g_str_equal (e_type, "touch-motion"))
    return GST_NAVIGATION_EVENT_TOUCH_MOTION;
  else if (g_str_equal (e_type, "touch-frame"))
    return GST_NAVIGATION_EVENT_TOUCH_FRAME;

  return GST_NAVIGATION_EVENT_INVALID;
}

/**
 * gst_navigation_event_new_key_press:
 * @key: A string identifying the key press.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the given key press.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_key_press (const gchar * key,
    GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "key-press", "key", G_TYPE_STRING, key,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_key_release:
 * @key: A string identifying the released key.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the given key release.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_key_release (const gchar * key,
    GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "key-release", "key", G_TYPE_STRING, key,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_mouse_button_press:
 * @button: The number of the pressed mouse button.
 * @x: The x coordinate of the mouse cursor.
 * @y: The y coordinate of the mouse cursor.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the given key mouse button press.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_mouse_button_press (gint button, gdouble x, gdouble y,
    GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "mouse-button-press",
          "button", G_TYPE_INT, button, "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_mouse_button_release:
 * @button: The number of the released mouse button.
 * @x: The x coordinate of the mouse cursor.
 * @y: The y coordinate of the mouse cursor.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the given key mouse button release.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_mouse_button_release (gint button, gdouble x,
    gdouble y, GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "mouse-button-release",
          "button", G_TYPE_INT, button, "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_mouse_move:
 * @x: The x coordinate of the mouse cursor.
 * @y: The y coordinate of the mouse cursor.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the new mouse location.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_mouse_move (gdouble x, gdouble y,
    GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "mouse-move",
          "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_mouse_scroll:
 * @x: The x coordinate of the mouse cursor.
 * @y: The y coordinate of the mouse cursor.
 * @delta_x: The x component of the scroll movement.
 * @delta_y: The y component of the scroll movement.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for the mouse scroll.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_mouse_scroll (gdouble x, gdouble y, gdouble delta_x,
    gdouble delta_y, GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "mouse-scroll",
          "pointer_x", G_TYPE_DOUBLE, x, "pointer_y", G_TYPE_DOUBLE, y,
          "delta_pointer_x", G_TYPE_DOUBLE, delta_x,
          "delta_pointer_y", G_TYPE_DOUBLE, delta_y,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_command:
 * @command: The navigation command to use.
 *
 * Create a new navigation event given navigation command..
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_command (GstNavigationCommand command)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "command",
          "command-code", G_TYPE_UINT, (guint) command, NULL));
}

/**
 * gst_navigation_event_new_touch_down:
 * @identifier: A number uniquely identifying this touch point. It must stay
 *    unique to this touch point at least until an up event is sent for
 *    the same identifier, or all touch points are cancelled.
 * @x: The x coordinate of the new touch point.
 * @y: The y coordinate of the new touch point.
 * @pressure: Pressure data of the touch point, from 0.0 to 1.0, or NaN if no
 *    data is available.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for an added touch point.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_touch_down (guint identifier, gdouble x, gdouble y,
    gdouble pressure, GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "touch-down",
          "identifier", G_TYPE_UINT, identifier,
          "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "pressure", G_TYPE_DOUBLE, pressure,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_touch_motion:
 * @identifier: A number uniquely identifying this touch point. It must
 *    correlate to exactly one previous touch_start event.
 * @x: The x coordinate of the touch point.
 * @y: The y coordinate of the touch point.
 * @pressure: Pressure data of the touch point, from 0.0 to 1.0, or NaN if no
 *    data is available.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for a moved touch point.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_touch_motion (guint identifier, gdouble x, gdouble y,
    gdouble pressure, GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "touch-motion",
          "identifier", G_TYPE_UINT, identifier,
          "pointer_x", G_TYPE_DOUBLE, x,
          "pointer_y", G_TYPE_DOUBLE, y,
          "pressure", G_TYPE_DOUBLE, pressure,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_touch_up:
 * @identifier: A number uniquely identifying this touch point. It must
 *    correlate to exactly one previous down event, but can be reused
 *    after sending this event.
 * @x: The x coordinate of the touch point.
 * @y: The y coordinate of the touch point.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event for a removed touch point.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_touch_up (guint identifier, gdouble x, gdouble y,
    GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "touch-up",
          "identifier", G_TYPE_UINT, identifier,
          "pointer_x", G_TYPE_DOUBLE, x, "pointer_y", G_TYPE_DOUBLE, y,
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_new_touch_frame:
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event signalling the end of a touch frame. Touch
 * frames signal that all previous down, motion and up events not followed by
 * another touch frame event already should be considered simultaneous.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_touch_frame (GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "touch-frame",
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}


/**
 * gst_navigation_event_new_touch_cancel:
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Create a new navigation event signalling that all currently active touch
 * points are cancelled and should be discarded. For example, under Wayland
 * this event might be sent when a swipe passes the threshold to be recognized
 * as a gesture by the compositor.
 *
 * Returns: (transfer full): a new #GstEvent
 *
 * Since: 1.22
 */
GstEvent *
gst_navigation_event_new_touch_cancel (GstNavigationModifierType state)
{
  return gst_event_new_navigation (gst_structure_new (GST_NAVIGATION_EVENT_NAME,
          "event", G_TYPE_STRING, "touch-cancel",
          "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state, NULL));
}

/**
 * gst_navigation_event_parse_key_event:
 * @event: A #GstEvent to inspect.
 * @key: (out) (optional) (transfer none): A pointer to a location to receive
 *     the string identifying the key press. The returned string is owned by the
 *     event, and valid only until the event is unreffed.
 *
 * Note: Modifier keys (as defined in #GstNavigationModifierType)
 * [press](GST_NAVIGATION_EVENT_KEY_PRESS) and
 * [release](GST_NAVIGATION_KEY_PRESS) events are generated even if those states are
 * present on all other related events
 */
gboolean
gst_navigation_event_parse_key_event (GstEvent * event, const gchar ** key)
{
  GstNavigationEventType e_type;
  const GstStructure *s;

  e_type = gst_navigation_event_get_type (event);
  g_return_val_if_fail (e_type == GST_NAVIGATION_EVENT_KEY_PRESS ||
      e_type == GST_NAVIGATION_EVENT_KEY_RELEASE, FALSE);

  if (key) {
    s = gst_event_get_structure (event);
    *key = gst_structure_get_string (s, "key");
    if (*key == NULL)
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_navigation_event_parse_mouse_button_event:
 * @event: A #GstEvent to inspect.
 * @button: (out) (optional): Pointer to a gint that will receive the button
 *     number associated with the event.
 * @x: (out) (optional): Pointer to a gdouble to receive the x coordinate of the
 *     mouse button event.
 * @y: (out) (optional): Pointer to a gdouble to receive the y coordinate of the
 *     mouse button event.
 *
 * Retrieve the details of either a #GstNavigation mouse button press event or
 * a mouse button release event. Determine which type the event is using
 * gst_navigation_event_get_type() to retrieve the #GstNavigationEventType.
 *
 * Returns: TRUE if the button number and both coordinates could be extracted,
 *     otherwise FALSE.
 */
gboolean
gst_navigation_event_parse_mouse_button_event (GstEvent * event, gint * button,
    gdouble * x, gdouble * y)
{
  GstNavigationEventType e_type;
  const GstStructure *s;
  gboolean ret = TRUE;

  e_type = gst_navigation_event_get_type (event);
  g_return_val_if_fail (e_type == GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS ||
      e_type == GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE, FALSE);

  s = gst_event_get_structure (event);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);
  if (button)
    ret &= gst_structure_get_int (s, "button", button);

  WARN_IF_FAIL (ret, "Couldn't extract details from mouse button event");

  return ret;
}

/**
 * gst_navigation_event_parse_mouse_move_event:
 * @event: A #GstEvent to inspect.
 * @x: (out) (optional): Pointer to a gdouble to receive the x coordinate of the
 *     mouse movement.
 * @y: (out) (optional): Pointer to a gdouble to receive the y coordinate of the
 *     mouse movement.
 *
 * Inspect a #GstNavigation mouse movement event and extract the coordinates
 * of the event.
 *
 * Returns: TRUE if both coordinates could be extracted, otherwise FALSE.
 */
gboolean
gst_navigation_event_parse_mouse_move_event (GstEvent * event, gdouble * x,
    gdouble * y)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_EVENT_HAS_TYPE (event, MOUSE_MOVE),
      FALSE);

  s = gst_event_get_structure (event);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);

  WARN_IF_FAIL (ret, "Couldn't extract positions from mouse move event");

  return ret;
}

/**
 * gst_navigation_event_parse_mouse_scroll_event:
 * @event: A #GstEvent to inspect.
 * @x: (out) (optional): Pointer to a gdouble to receive the x coordinate of the
 *     mouse movement.
 * @y: (out) (optional): Pointer to a gdouble to receive the y coordinate of the
 *     mouse movement.
 * @delta_x: (out) (optional): Pointer to a gdouble to receive the delta_x coordinate of the
 *     mouse movement.
 * @delta_y: (out) (optional): Pointer to a gdouble to receive the delta_y coordinate of the
 *     mouse movement.
 *
 * Inspect a #GstNavigation mouse scroll event and extract the coordinates
 * of the event.
 *
 * Returns: TRUE if all coordinates could be extracted, otherwise FALSE.
 *
 * Since: 1.18
 */
gboolean
gst_navigation_event_parse_mouse_scroll_event (GstEvent * event,
    gdouble * x, gdouble * y, gdouble * delta_x, gdouble * delta_y)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_EVENT_HAS_TYPE (event, MOUSE_SCROLL),
      FALSE);

  s = gst_event_get_structure (event);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);
  if (delta_x)
    ret &= gst_structure_get_double (s, "delta_pointer_x", delta_x);
  if (delta_y)
    ret &= gst_structure_get_double (s, "delta_pointer_y", delta_y);

  WARN_IF_FAIL (ret, "Couldn't extract positions from mouse scroll event");

  return ret;
}

/**
 * gst_navigation_event_parse_command:
 * @event: A #GstEvent to inspect.
 * @command: (out) (optional): Pointer to GstNavigationCommand to receive the
 *     type of the navigation event.
 *
 * Inspect a #GstNavigation command event and retrieve the enum value of the
 * associated command.
 *
 * Returns: TRUE if the navigation command could be extracted, otherwise FALSE.
 */
gboolean
gst_navigation_event_parse_command (GstEvent * event,
    GstNavigationCommand * command)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_EVENT_HAS_TYPE (event, COMMAND), FALSE);

  if (command) {
    s = gst_event_get_structure (event);
    ret = gst_structure_get_uint (s, "command-code", (guint *) command);
    WARN_IF_FAIL (ret, "Couldn't extract command code from command event");
  }

  return ret;
}

/**
 * gst_navigation_event_parse_touch_event:
 * @event: A #GstEvent to inspect.
 * @identifier: (out) (optional): Pointer to a guint that will receive the
 *     identifier unique to this touch point.
 * @x: (out) (optional): Pointer to a gdouble that will receive the x
 *     coordinate of the touch event.
 * @y: (out) (optional): Pointer to a gdouble that will receive the y
 *     coordinate of the touch event.
 * @pressure: (out) (optional): Pointer to a gdouble that will receive the
 *     force of the touch event, in the range from 0.0 to 1.0. If pressure
 *     data is not available, NaN will be set instead.
 *
 * Retrieve the details of a #GstNavigation touch-down or touch-motion event.
 * Determine which type the event is using gst_navigation_event_get_type()
 * to retrieve the #GstNavigationEventType.
 *
 * Returns: TRUE if all details could be extracted, otherwise FALSE.
 *
 * Since: 1.22
 */
gboolean
gst_navigation_event_parse_touch_event (GstEvent * event, guint * identifier,
    gdouble * x, gdouble * y, gdouble * pressure)
{
  GstNavigationEventType e_type;
  const GstStructure *s;
  gboolean ret = TRUE;

  e_type = gst_navigation_event_get_type (event);
  g_return_val_if_fail (e_type == GST_NAVIGATION_EVENT_TOUCH_DOWN ||
      e_type == GST_NAVIGATION_EVENT_TOUCH_MOTION, FALSE);

  s = gst_event_get_structure (event);
  if (identifier)
    ret &= gst_structure_get_uint (s, "identifier", identifier);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);
  if (pressure)
    ret &= gst_structure_get_double (s, "pressure", pressure);

  WARN_IF_FAIL (ret, "Couldn't extract details from touch event");

  return ret;
}

/**
 * gst_navigation_event_parse_touch_up_event:
 * @event: A #GstEvent to inspect.
 * @identifier: (out) (optional): Pointer to a guint that will receive the
 *     identifier unique to this touch point.
 * @x: (out) (optional): Pointer to a gdouble that will receive the x
 *     coordinate of the touch event.
 * @y: (out) (optional): Pointer to a gdouble that will receive the y
 *     coordinate of the touch event.
 *
 * Retrieve the details of a #GstNavigation touch-up event.
 *
 * Returns: TRUE if all details could be extracted, otherwise FALSE.
 *
 * Since: 1.22
 */
gboolean
gst_navigation_event_parse_touch_up_event (GstEvent * event,
    guint * identifier, gdouble * x, gdouble * y)
{
  const GstStructure *s;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_NAVIGATION_EVENT_HAS_TYPE (event, TOUCH_UP), FALSE);

  s = gst_event_get_structure (event);
  if (identifier)
    ret &= gst_structure_get_uint (s, "identifier", identifier);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);

  WARN_IF_FAIL (ret, "Couldn't extract details from touch-up event");

  return ret;
}

/**
 * gst_navigation_event_get_coordinates:
 * @event: The #GstEvent to inspect.
 * @x: (out) (optional): Pointer to a gdouble to receive the x coordinate of the
 *     navigation event.
 * @y: (out) (optional): Pointer to a gdouble to receive the y coordinate of the
 *     navigation event.
 *
 * Try to retrieve x and y coordinates of a #GstNavigation event.
 *
 * Returns: A boolean indicating success.
 *
 * Since: 1.22
 */
gboolean
gst_navigation_event_get_coordinates (GstEvent * event,
    gdouble * x, gdouble * y)
{
  GstNavigationEventType e_type;
  const GstStructure *s;
  gboolean ret = TRUE;

  e_type = gst_navigation_event_get_type (event);
  if (e_type != GST_NAVIGATION_EVENT_MOUSE_MOVE
      && e_type != GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS
      && e_type != GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE
      && e_type != GST_NAVIGATION_EVENT_TOUCH_DOWN
      && e_type != GST_NAVIGATION_EVENT_TOUCH_MOTION
      && e_type != GST_NAVIGATION_EVENT_TOUCH_UP) {
    return FALSE;
  }

  s = gst_event_get_structure (event);
  if (x)
    ret &= gst_structure_get_double (s, "pointer_x", x);
  if (y)
    ret &= gst_structure_get_double (s, "pointer_y", y);

  WARN_IF_FAIL (ret, "Couldn't extract coordinates from the event");

  return ret;
}

/**
 * gst_navigation_event_set_coordinates:
 * @event: The #GstEvent to modify.
 * @x: The x coordinate to set.
 * @y: The y coordinate to set.
 *
 * Try to set x and y coordinates on a #GstNavigation event. The event must
 * be writable.
 *
 * Returns: A boolean indicating success.
 *
 * Since: 1.22
 */
gboolean
gst_navigation_event_set_coordinates (GstEvent * event, gdouble x, gdouble y)
{
  GstNavigationEventType e_type;
  GstStructure *s;

  g_return_val_if_fail (gst_event_is_writable (event), FALSE);

  e_type = gst_navigation_event_get_type (event);
  if (e_type != GST_NAVIGATION_EVENT_MOUSE_MOVE
      && e_type != GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS
      && e_type != GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE
      && e_type != GST_NAVIGATION_EVENT_TOUCH_DOWN
      && e_type != GST_NAVIGATION_EVENT_TOUCH_MOTION
      && e_type != GST_NAVIGATION_EVENT_TOUCH_UP) {
    return FALSE;
  }

  s = gst_event_writable_structure (event);
  gst_structure_set (s, "pointer_x", G_TYPE_DOUBLE, x,
      "pointer_y", G_TYPE_DOUBLE, y, NULL);

  return TRUE;
}


/**
 * gst_navigation_event_parse_modifier_state:
 * @event: The #GstEvent to modify.
 * @state: a bit-mask representing the state of the modifier keys (e.g. Control,
 * Shift and Alt).
 *
 * Returns: TRUE if the event is a #GstNavigation event with associated
 * modifiers state, otherwise FALSE.
 *
 * Since: 1.22
 */
gboolean
gst_navigation_event_parse_modifier_state (GstEvent * event,
    GstNavigationModifierType * state)
{
  GstNavigationEventType e_type;
  const GstStructure *s;

  g_return_val_if_fail (GST_IS_EVENT (event), FALSE);

  e_type = gst_navigation_event_get_type (event);
  if (e_type == GST_NAVIGATION_EVENT_COMMAND) {
    return FALSE;
  }

  s = gst_event_get_structure (event);
  if (!gst_structure_get (s, "state", GST_TYPE_NAVIGATION_MODIFIER_TYPE, state,
          NULL))
    *state = GST_NAVIGATION_MODIFIER_NONE;

  return TRUE;
}
