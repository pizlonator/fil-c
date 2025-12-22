/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                        Copyright (c) 1999                             */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Alan W Black (awb@cstr.ed.ac.uk)                */
/*             Date   :  March 1999                                      */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Client end of Festival server API in C designed specifically for      */
/* Galaxy Communicator use though might be of use for other things       */
/*                                                                       */
/* This is a modified version of the standalone client as provided in    */
/* festival example code: festival_client.c                              */
/*                                                                       */
/*=======================================================================*/

/**
 * SECTION:element-festival
 * @title: festival
 *
 * This element connects to a
 * [festival](http://www.festvox.org/festival/index.html) server process and
 * uses it to synthesize speech. Festival need to run already in server mode,
 * started as `festival --server`
 *
 * ## Example pipeline
 * |[
 * echo 'Hello G-Streamer!' | gst-launch-1.0 fdsrc fd=0 ! festival ! wavparse ! audioconvert ! alsasink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>               /* Needed for G_OS_XXXX macros */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
/* dup, read, close */
#include <io.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "gstfestival.h"
#include <gst/audio/audio.h>

GST_DEBUG_CATEGORY_STATIC (festival_debug);
#define GST_CAT_DEFAULT festival_debug

static void gst_festival_finalize (GObject * object);

static GstFlowReturn gst_festival_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_festival_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstStateChangeReturn gst_festival_change_state (GstElement * element,
    GstStateChange transition);

static FT_Info *festival_default_info (void);
static char *socket_receive_file_to_buff (int fd, int *size);
static char *client_accept_s_expr (int fd);

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format=(string)utf8")
    );

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-wav")
    );

/* Festival signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0
      /* FILL ME */
};

/*static guint gst_festival_signals[LAST_SIGNAL] = { 0 }; */

G_DEFINE_TYPE (GstFestival, gst_festival, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (festival, "festival", GST_RANK_NONE,
    GST_TYPE_FESTIVAL, GST_DEBUG_CATEGORY_INIT (festival_debug, "festival",
        0, "Festival text-to-speech synthesizer");
    );;

static void
gst_festival_class_init (GstFestivalClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_festival_finalize);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_festival_change_state);

  /* register pads */
  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "Festival Text-to-Speech synthesizer", "Filter/Effect/Audio",
      "Synthesizes plain text into audio",
      "Wim Taymans <wim.taymans@gmail.com>");
}

static void
gst_festival_init (GstFestival * festival)
{
  festival->sinkpad =
      gst_pad_new_from_static_template (&sink_template_factory, "sink");
  gst_pad_set_chain_function (festival->sinkpad, gst_festival_chain);
  gst_element_add_pad (GST_ELEMENT (festival), festival->sinkpad);

  festival->srcpad =
      gst_pad_new_from_static_template (&src_template_factory, "src");
  gst_pad_set_query_function (festival->srcpad, gst_festival_src_query);
  gst_element_add_pad (GST_ELEMENT (festival), festival->srcpad);

  festival->info = festival_default_info ();
}

static void
gst_festival_finalize (GObject * object)
{
  GstFestival *festival = GST_FESTIVAL (object);

  g_free (festival->info);

  G_OBJECT_CLASS (gst_festival_parent_class)->finalize (object);
}

static gboolean
read_response (GstFestival * festival)
{
  char ack[4];
  char *data;
  int filesize;
  int fd;
  int n;
  gboolean ret = TRUE;

  fd = festival->info->server_fd;
  do {
    for (n = 0; n < 3;)
      n += read (fd, ack + n, 3 - n);
    ack[3] = '\0';
    GST_DEBUG_OBJECT (festival, "got response %s", ack);
    if (strcmp (ack, "WV\n") == 0) {
      GstBuffer *buffer;

      /* receive a waveform */
      data = socket_receive_file_to_buff (fd, &filesize);
      GST_DEBUG_OBJECT (festival, "received %d bytes of waveform data",
          filesize);

      /* push contents as a buffer */
      buffer = gst_buffer_new_wrapped (data, filesize);
      GST_BUFFER_TIMESTAMP (buffer) = GST_CLOCK_TIME_NONE;
      gst_pad_push (festival->srcpad, buffer);

    } else if (strcmp (ack, "LP\n") == 0) {
      /* receive an s-expr */
      data = client_accept_s_expr (fd);
      GST_DEBUG_OBJECT (festival, "received s-expression: %s", data);
      g_free (data);
    } else if (strcmp (ack, "ER\n") == 0) {
      /* server got an error */
      GST_ELEMENT_ERROR (festival,
          LIBRARY,
          FAILED,
          ("Festival speech server returned an error"),
          ("Make sure you have voices/languages installed"));
      ret = FALSE;
      break;
    }

  } while (strcmp (ack, "OK\n") != 0);

  return ret;
}

static GstFlowReturn
gst_festival_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstFestival *festival;
  GstMapInfo info;
  guint8 *p, *ep;
  gint f;
  FILE *fd;

  festival = GST_FESTIVAL (parent);

  GST_LOG_OBJECT (festival, "Got text buffer, %" G_GSIZE_FORMAT " bytes",
      gst_buffer_get_size (buf));

  f = dup (festival->info->server_fd);
  if (f < 0)
    goto fail_open;
  fd = fdopen (f, "wb");
  if (fd == NULL) {
    close (f);
    goto fail_open;
  }

  /* Copy text over to server, escaping any quotes */
  fprintf (fd, "(Parameter.set 'Audio_Required_Rate 16000)\n");
  fflush (fd);
  GST_DEBUG_OBJECT (festival, "issued Parameter.set command");
  if (read_response (festival) == FALSE) {
    fclose (fd);
    goto fail_read;
  }

  fprintf (fd, "(tts_textall \"");
  gst_buffer_map (buf, &info, GST_MAP_READ);
  p = info.data;
  ep = p + info.size;
  for (; p < ep && (*p != '\0'); p++) {
    if ((*p == '"') || (*p == '\\')) {
      putc ('\\', fd);
    }

    putc (*p, fd);
  }
  fprintf (fd, "\" \"%s\")\n", festival->info->text_mode);
  fclose (fd);
  gst_buffer_unmap (buf, &info);

  GST_DEBUG_OBJECT (festival, "issued tts_textall command");

  /* Read back info from server */
  if (read_response (festival) == FALSE)
    goto fail_read;

out:
  gst_buffer_unref (buf);
  return ret;

  /* ERRORS */
fail_open:
  {
    GST_ELEMENT_ERROR (festival, RESOURCE, OPEN_WRITE, (NULL), (NULL));
    ret = GST_FLOW_ERROR;
    goto out;
  }
fail_read:
  {
    GST_ELEMENT_ERROR (festival, RESOURCE, READ, (NULL), (NULL));
    ret = GST_FLOW_ERROR;
    goto out;
  }
}

static FT_Info *
festival_default_info (void)
{
  FT_Info *info;

  info = (FT_Info *) malloc (1 * sizeof (FT_Info));

  info->server_host = FESTIVAL_DEFAULT_SERVER_HOST;
  info->server_port = FESTIVAL_DEFAULT_SERVER_PORT;
  info->text_mode = FESTIVAL_DEFAULT_TEXT_MODE;

  info->server_fd = -1;

  return info;
}

static int
festival_socket_open (const char *host, int port)
{
  /* Return an FD to a remote server */
  struct sockaddr_in serv_addr;
  struct hostent *serverhost;
  int fd;

  fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (fd < 0) {
    fprintf (stderr, "festival_client: can't get socket\n");
    return -1;
  }
  memset (&serv_addr, 0, sizeof (serv_addr));
  if ((serv_addr.sin_addr.s_addr = inet_addr (host)) == -1) {
    /* its a name rather than an ipnum */
    serverhost = gethostbyname (host);
    if (serverhost == (struct hostent *) 0) {
      fprintf (stderr, "festival_client: gethostbyname failed\n");
      close (fd);
      return -1;
    }
    memmove (&serv_addr.sin_addr, serverhost->h_addr, serverhost->h_length);
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons (port);

  if (connect (fd, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) != 0) {
    fprintf (stderr, "festival_client: connect to server failed\n");
    close (fd);
    return -1;
  }

  return fd;
}

static char *
client_accept_s_expr (int fd)
{
  /* Read s-expression from server, as a char * */
  char *expr;
  int filesize;

  expr = socket_receive_file_to_buff (fd, &filesize);
  expr[filesize] = '\0';

  return expr;
}

static char *
socket_receive_file_to_buff (int fd, int *size)
{
  /* Receive file (probably a waveform file) from socket using   */
  /* Festival key stuff technique, but long winded I know, sorry */
  /* but will receive any file without closing the stream or    */
  /* using OOB data                                              */
  static const char file_stuff_key[] = "ft_StUfF_key";  /* must == Festival's key */
  char *buff;
  int bufflen;
  int n, k, i;
  char c;

  bufflen = 1024;
  buff = (char *) g_malloc (bufflen);
  *size = 0;

  for (k = 0; file_stuff_key[k] != '\0';) {
    n = read (fd, &c, 1);
    if (n == 0)
      break;                    /* hit stream eof before end of file */

    if ((*size) + k + 1 >= bufflen) {
      /* +1 so you can add a NULL if you want */
      bufflen += bufflen / 4;
      buff = (char *) g_realloc (buff, bufflen);
    }
    if (file_stuff_key[k] == c)
      k++;
    else if ((c == 'X') && (file_stuff_key[k + 1] == '\0')) {
      /* It looked like the key but wasn't */
      for (i = 0; i < k; i++, (*size)++)
        buff[*size] = file_stuff_key[i];
      k = 0;
      /* omit the stuffed 'X' */
    } else {
      for (i = 0; i < k; i++, (*size)++)
        buff[*size] = file_stuff_key[i];
      k = 0;
      buff[*size] = c;
      (*size)++;
    }
  }

  return buff;
}

/***********************************************************************/
/* Public Functions to this API                                        */
/***********************************************************************/

static gboolean
gst_festival_open (GstFestival * festival)
{
  /* Open socket to server */
  if (festival->info == NULL)
    festival->info = festival_default_info ();

  festival->info->server_fd =
      festival_socket_open (festival->info->server_host,
      festival->info->server_port);
  if (festival->info->server_fd == -1) {
    GST_ERROR
        ("Could not talk to festival server (no server running or wrong host/port?)");
    return FALSE;
  }
  GST_OBJECT_FLAG_SET (festival, GST_FESTIVAL_OPEN);
  return TRUE;
}

static void
gst_festival_close (GstFestival * festival)
{
  if (festival->info == NULL)
    return;

  if (festival->info->server_fd != -1)
    close (festival->info->server_fd);
  GST_OBJECT_FLAG_UNSET (festival, GST_FESTIVAL_OPEN);
  return;
}

static GstStateChangeReturn
gst_festival_change_state (GstElement * element, GstStateChange transition)
{
  g_return_val_if_fail (GST_IS_FESTIVAL (element), GST_STATE_CHANGE_FAILURE);

  if (GST_STATE_PENDING (element) == GST_STATE_NULL) {
    if (GST_OBJECT_FLAG_IS_SET (element, GST_FESTIVAL_OPEN)) {
      GST_DEBUG ("Closing connection ");
      gst_festival_close (GST_FESTIVAL (element));
    }
  } else {
    if (!GST_OBJECT_FLAG_IS_SET (element, GST_FESTIVAL_OPEN)) {
      GST_DEBUG ("Opening connection ");
      if (!gst_festival_open (GST_FESTIVAL (element)))
        return GST_STATE_CHANGE_FAILURE;
    }
  }

  if (GST_ELEMENT_CLASS (gst_festival_parent_class)->change_state)
    return GST_ELEMENT_CLASS (gst_festival_parent_class)->change_state (element,
        transition);

  return GST_STATE_CHANGE_SUCCESS;
}

static gboolean
gst_festival_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      /* Not supported */
      return FALSE;
    case GST_QUERY_DURATION:
      gst_query_set_duration (query, GST_FORMAT_BYTES, -1);
      return TRUE;
    case GST_QUERY_SEEKING:
      gst_query_set_seeking (query, GST_FORMAT_BYTES, FALSE, 0, -1);
      return TRUE;
    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 1, GST_FORMAT_BYTES);
      return TRUE;
    default:
      break;
  }

  return gst_pad_query_default (pad, parent, query);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (festival, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    festival,
    "Synthesizes plain text into audio",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
