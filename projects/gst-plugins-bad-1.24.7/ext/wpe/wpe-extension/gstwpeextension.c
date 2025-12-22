/* Copyright (C) <2021> Thibault Saunier <tsaunier@igalia.com>
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
#include <config.h>
#endif

#include "gstwpeextension.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <gst/gst.h>
#include <gmodule.h>
#include <gio/gunixfdlist.h>

GST_DEBUG_CATEGORY_STATIC (wpe_extension_debug);
#define GST_CAT_DEFAULT wpe_extension_debug

#if USE_WPE2
#define WebKitWebExtension WebKitWebProcessExtension
#define extension_initialize webkit_web_process_extension_initialize
#define extension_send_message_to_context webkit_web_process_extension_send_message_to_context
#else
#define extension_initialize webkit_web_extension_initialize
#define extension_send_message_to_context webkit_web_extension_send_message_to_context
#endif

G_MODULE_EXPORT void extension_initialize (WebKitWebExtension * extension);

static WebKitWebExtension *global_extension = NULL;

#if !USE_WPE2
static void
console_message_cb (WebKitWebPage * page,
    WebKitConsoleMessage * console_message, gpointer data)
{
  char *message = g_strdup (webkit_console_message_get_text (console_message));
  gst_wpe_extension_send_message (webkit_user_message_new
      ("gstwpe.console_message", g_variant_new ("(s)", message)), NULL, NULL,
      NULL);
  g_free (message);
}
#endif

static void
web_page_created_callback (WebKitWebExtension * extension,
    WebKitWebPage * web_page, gpointer data)
{
  // WebKitConsoleMessage is deprecated in wpe1 and has no replacement in wpe2.
#if !USE_WPE2
  g_signal_connect (web_page, "console-message-sent",
      G_CALLBACK (console_message_cb), NULL);
#endif
}

void
extension_initialize (WebKitWebExtension * extension)
{
  g_return_if_fail (!global_extension);

  gst_init (NULL, NULL);

  GST_DEBUG_CATEGORY_INIT (wpe_extension_debug, "wpewebextension", 0,
      "GstWPE WebExtension");

  /* Register our own audio sink to */
  gst_element_register (NULL, "gstwpeaudiosink", GST_RANK_PRIMARY + 500,
      gst_wpe_audio_sink_get_type ());
  gst_object_unref (g_object_new (gst_wpe_bus_msg_forwarder_get_type (), NULL));

  global_extension = extension;
  GST_INFO ("Setting as global extension.");

  g_signal_connect (global_extension, "page-created",
      G_CALLBACK (web_page_created_callback), NULL);
}

void
gst_wpe_extension_send_message (WebKitUserMessage * msg,
    GCancellable * cancellable, GAsyncReadyCallback cb, gpointer udata)
{
  extension_send_message_to_context (global_extension, msg, cancellable, cb,
      udata);
}
