#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rfbdecoder.h"
#include "d3des.h"
#include <gst/gst.h>

#include <stdlib.h>
#include <string.h>

#define RFB_GET_UINT32(ptr) GST_READ_UINT32_BE(ptr)
#define RFB_GET_UINT16(ptr) GST_READ_UINT16_BE(ptr)
#define RFB_GET_UINT8(ptr) GST_READ_UINT8(ptr)

#define RFB_SET_UINT32(ptr, val) GST_WRITE_UINT32_BE((ptr),(val))
#define RFB_SET_UINT16(ptr, val) GST_WRITE_UINT16_BE((ptr),(val))
#define RFB_SET_UINT8(ptr, val) GST_WRITE_UINT8((ptr),(val))

GST_DEBUG_CATEGORY_EXTERN (rfbdecoder_debug);
#define GST_CAT_DEFAULT rfbdecoder_debug


static gboolean rfb_decoder_state_wait_for_protocol_version (RfbDecoder *
    decoder);
static gboolean rfb_decoder_state_wait_for_security (RfbDecoder * decoder);
static gboolean rfb_decoder_state_send_client_initialisation (RfbDecoder *
    decoder);
static gboolean rfb_decoder_state_wait_for_server_initialisation (RfbDecoder *
    decoder);
static gboolean rfb_decoder_state_security_result (RfbDecoder * decoder);
static gboolean rfb_decoder_state_normal (RfbDecoder * decoder);
static gboolean rfb_decoder_state_framebuffer_update (RfbDecoder * decoder);
static gboolean rfb_decoder_state_framebuffer_update_rectangle (RfbDecoder *
    decoder);
static gboolean rfb_decoder_state_set_colour_map_entries (RfbDecoder * decoder);
static gboolean rfb_decoder_state_server_cut_text (RfbDecoder * decoder);
static gboolean rfb_decoder_raw_encoding (RfbDecoder * decoder, gint start_x,
    gint start_y, gint rect_w, gint rect_h);
static gboolean rfb_decoder_copyrect_encoding (RfbDecoder * decoder,
    gint start_x, gint start_y, gint rect_w, gint rect_h);
static gboolean rfb_decoder_rre_encoding (RfbDecoder * decoder, gint start_x,
    gint start_y, gint rect_w, gint rect_h);
static gboolean rfb_decoder_corre_encoding (RfbDecoder * decoder, gint start_x,
    gint start_y, gint rect_w, gint rect_h);
static gboolean rfb_decoder_hextile_encoding (RfbDecoder * decoder,
    gint start_x, gint start_y, gint rect_w, gint rect_h);

RfbDecoder *
rfb_decoder_new (void)
{
  RfbDecoder *decoder = g_new0 (RfbDecoder, 1);

  decoder->socket_client = g_socket_client_new ();
  decoder->connection = NULL;
  decoder->cancellable = g_cancellable_new ();

  decoder->password = NULL;

  decoder->use_copyrect = FALSE;

  decoder->offset_x = 0;
  decoder->offset_y = 0;
  decoder->rect_width = 0;
  decoder->rect_height = 0;
  decoder->shared_flag = TRUE;
  decoder->data = NULL;
  decoder->data_len = 0;
  decoder->error = NULL;

  g_mutex_init (&decoder->write_lock);

  return decoder;
}

void
rfb_decoder_free (RfbDecoder * decoder)
{
  g_return_if_fail (decoder != NULL);

  rfb_decoder_disconnect (decoder);

  g_clear_object (&decoder->socket_client);
  g_clear_object (&decoder->cancellable);
  g_mutex_clear (&decoder->write_lock);
  g_free (decoder);
}

gboolean
rfb_decoder_connect_tcp (RfbDecoder * decoder, gchar * host, guint port)
{
  GError *err = NULL;
  GSocketConnection *connection;

  GST_DEBUG ("connecting to the rfb server");

  g_return_val_if_fail (decoder != NULL, FALSE);
  g_return_val_if_fail (decoder->connection == NULL, FALSE);
  g_return_val_if_fail (host != NULL, FALSE);

  g_cancellable_reset (decoder->cancellable);

  connection =
      g_socket_client_connect_to_host (decoder->socket_client, host, port,
      decoder->cancellable, &err);

  if (!connection)
    goto connect_failed;

  decoder->connection = connection;

  return TRUE;

connect_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG ("Cancelled connecting");
    } else {
      GST_WARNING ("Failed to connect to host '%s:%d': %s", host, port,
          err->message);
      if (decoder->error == NULL) {
        decoder->error = err;
        err = NULL;
      }
    }
    g_clear_error (&err);
    return FALSE;
  }
}

void
rfb_decoder_disconnect (RfbDecoder * decoder)
{
  GST_DEBUG ("Disconnecting from the rfb server");

  g_return_if_fail (decoder);
  g_return_if_fail (decoder->cancellable);

  g_cancellable_cancel (decoder->cancellable);

  /* Make sure threaded write a done first, this avoids race condition,
   * specially when the decoder is freed */
  g_mutex_lock (&decoder->write_lock);

  g_clear_object (&decoder->connection);
  g_clear_error (&decoder->error);
  g_clear_pointer (&decoder->data, g_free);

  g_mutex_unlock (&decoder->write_lock);
}

/**
 * rfb_decoder_iterate:
 * @decoder: The rfb context
 *
 * Initializes the connection with the rfb server
 *
 * Returns: TRUE if initialization was successful, FALSE on fail.
 */
gboolean
rfb_decoder_iterate (RfbDecoder * decoder)
{
  gboolean ret;

  g_return_val_if_fail (decoder != NULL, FALSE);
  g_return_val_if_fail (decoder->connection != NULL, FALSE);

  if (decoder->state == NULL) {
    GST_DEBUG ("First iteration: set state to -> wait for protocol version");
    decoder->state = rfb_decoder_state_wait_for_protocol_version;
  }

  GST_DEBUG ("Executing next state in initialization");
  ret = decoder->state (decoder);

  if (ret == FALSE) {
    if (decoder->error == NULL)
      GST_WARNING ("Failure, but no error stored");
    else
      GST_WARNING ("Failure: %s", decoder->error->message);
  }

  return ret;
}

static guint8 *
rfb_decoder_read (RfbDecoder * decoder, guint32 len)
{
  GInputStream *in;
  GError *err = NULL;
  gsize count = 0;

  if (!decoder->connection)
    return FALSE;

  g_return_val_if_fail (len > 0, NULL);

  in = g_io_stream_get_input_stream (G_IO_STREAM (decoder->connection));

  g_return_val_if_fail (in != NULL, NULL);

  if (G_UNLIKELY (len > decoder->data_len)) {
    g_free (decoder->data);
    decoder->data = g_malloc (len);
    decoder->data_len = len;
  }

  if (!g_input_stream_read_all (in, decoder->data, len, &count,
          decoder->cancellable, &err))
    goto recv_error;

  if (count == 0) {
    g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
        "Connection was closed.");
    goto recv_error;
  }

  return decoder->data;

recv_error:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG ("Read on socket cancelled");
    } else {
      GST_ERROR ("Read error on socket: %s", err->message);
      if (decoder->error == NULL) {
        decoder->error = err;
        err = NULL;
      }
    }
    g_clear_error (&err);
    return NULL;
  }
}

static gboolean
rfb_decoder_send (RfbDecoder * decoder, guint8 * buffer, guint len)
{
  GOutputStream *out;
  GError *err = NULL;

  if (!decoder->connection)
    return FALSE;

  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (len > 0, 0);

  g_mutex_lock (&decoder->write_lock);

  out = g_io_stream_get_output_stream (G_IO_STREAM (decoder->connection));

  if (!g_output_stream_write_all (out, buffer, len, NULL, decoder->cancellable,
          &err))
    goto send_error;

  g_mutex_unlock (&decoder->write_lock);

  return TRUE;

send_error:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG ("Send on socket cancelled");
    } else {
      GST_ERROR ("Send error on socket: %s", err->message);
      if (decoder->error == NULL) {
        decoder->error = err;
        err = NULL;
      }
    }
    g_clear_error (&err);
    g_mutex_unlock (&decoder->write_lock);
    return FALSE;
  }
}

void
rfb_decoder_send_update_request (RfbDecoder * decoder,
    gboolean incremental, gint x, gint y, gint width, gint height)
{
  guint8 data[10];

  g_return_if_fail (decoder != NULL);
  g_return_if_fail (decoder->connection != NULL);

  data[0] = 3;
  data[1] = incremental;
  RFB_SET_UINT16 (data + 2, x);
  RFB_SET_UINT16 (data + 4, y);
  RFB_SET_UINT16 (data + 6, width);
  RFB_SET_UINT16 (data + 8, height);

  rfb_decoder_send (decoder, data, 10);

  /* create a backup of the prev frame for copyrect encoding */
  if (decoder->use_copyrect) {
    memcpy (decoder->prev_frame, decoder->frame,
        decoder->rect_width * decoder->rect_height * decoder->bpp / 8);
  }

  decoder->state = rfb_decoder_state_normal;
}

void
rfb_decoder_send_key_event (RfbDecoder * decoder, guint key, gboolean down_flag)
{
  guint8 data[8];

  g_return_if_fail (decoder != NULL);
  g_return_if_fail (decoder->connection != NULL);

  data[0] = 4;
  data[1] = down_flag;
  RFB_SET_UINT16 (data + 2, 0);
  RFB_SET_UINT32 (data + 4, key);

  rfb_decoder_send (decoder, data, 8);
}

void
rfb_decoder_send_pointer_event (RfbDecoder * decoder,
    gint button_mask, gint x, gint y)
{
  guint8 data[6];

  g_return_if_fail (decoder != NULL);
  g_return_if_fail (decoder->connection != NULL);

  data[0] = 5;
  data[1] = button_mask;
  RFB_SET_UINT16 (data + 2, x);
  RFB_SET_UINT16 (data + 4, y);

  rfb_decoder_send (decoder, data, 6);
}

/**
 * rfb_decoder_state_wait_for_protocol_version:
 *
 * Negotiate the rfb version used
 */
static gboolean
rfb_decoder_state_wait_for_protocol_version (RfbDecoder * decoder)
{
  gchar version_str[] = "RFB 003.003\n";

  if (!rfb_decoder_read (decoder, 12))
    return FALSE;

  g_return_val_if_fail (memcmp (decoder->data, "RFB 003.", 8) == 0, FALSE);
  g_return_val_if_fail (*(decoder->data + 11) == 0x0a, FALSE);

  GST_DEBUG ("\"%.11s\"", decoder->data);
  *(decoder->data) = 0x00;
  *(decoder->data + 11) = 0x00;
  decoder->protocol_major = atoi ((char *) (decoder->data + 4));
  decoder->protocol_minor = atoi ((char *) (decoder->data + 8));
  GST_DEBUG ("Major version : %d", decoder->protocol_major);
  GST_DEBUG ("Minor version : %d", decoder->protocol_minor);

  if (decoder->protocol_major != 3) {
    GST_INFO
        ("A major protocol version of %d is not supported, falling back to 3",
        decoder->protocol_major);
    decoder->protocol_major = 3;
    decoder->protocol_minor = 3;
  }
  switch (decoder->protocol_minor) {
    case 3:
    case 7:
    case 8:
      break;
    default:
      GST_INFO ("Minor version %d is not supported, using 3",
          decoder->protocol_minor);
      decoder->protocol_minor = 3;
  }

  version_str[10] = '0' + decoder->protocol_minor;

  if (!rfb_decoder_send (decoder, (guint8 *) version_str, 12))
    return FALSE;

  decoder->state = rfb_decoder_state_wait_for_security;
  return TRUE;
}

/*
 * a string describing the reason (where a string is specified as a length
 * followed by that many ASCII characters)
 **/
static gboolean
rfb_decoder_state_reason (RfbDecoder * decoder)
{
  gint reason_length;

  if (!rfb_decoder_read (decoder, 4))
    return FALSE;

  reason_length = RFB_GET_UINT32 (decoder->data);

  if (!rfb_decoder_read (decoder, reason_length))
    return FALSE;

  GST_WARNING ("Reason by server: %s", decoder->data);

  if (decoder->error == NULL) {
    decoder->error = g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
        "VNC server error: %s", decoder->data);
  }

  return FALSE;
}

static gboolean
rfb_decoder_state_wait_for_security (RfbDecoder * decoder)
{
  /*
   * Version 3.3 The server decides the security type and sends a single word
   *
   * The security-type may only take the value 0, 1 or 2. A value of 0 means that the
   * connection has failed and is followed by a string giving the reason, as described
   * above.
   */
  if (IS_VERSION_3_3 (decoder)) {
    if (!rfb_decoder_read (decoder, 4))
      return FALSE;

    decoder->security_type = RFB_GET_UINT32 (decoder->data);
    GST_DEBUG ("security = %d", decoder->security_type);

    g_return_val_if_fail (decoder->security_type < 3, FALSE);

    if (decoder->security_type == SECURITY_FAIL) {
      decoder->state = rfb_decoder_state_reason;
      return TRUE;
    }
  } else {
    guint8 num_type;
    gint i;
    guint8 *type = NULL;

    if (!rfb_decoder_read (decoder, 1))
      return FALSE;

    num_type = RFB_GET_UINT8 (decoder->data);
    if (num_type == 0) {
      decoder->state = rfb_decoder_state_reason;
      return TRUE;
    }

    if (!rfb_decoder_read (decoder, num_type))
      return FALSE;

    decoder->security_type = SECURITY_FAIL;

    /* For now, simply pick the first support security method */
    for (i = 0; i < num_type; i++) {
      guint val = RFB_GET_UINT8 (decoder->data + i);

      GST_DEBUG ("Server supports security type %u", val);

      if (val == SECURITY_NONE || val == SECURITY_VNC) {
        decoder->security_type = val;
        type = decoder->data + i;
        break;
      }
    }

    if (!type) {
      GST_WARNING ("Security type negotiation failed.");
      decoder->error = g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
          "VNC server requires unsupported security method.");
      return FALSE;
    }

    GST_DEBUG ("security = %d", decoder->security_type);
    if (!rfb_decoder_send (decoder, type, 1))
      return FALSE;
  }

  switch (decoder->security_type) {
    case SECURITY_NONE:
      GST_DEBUG ("Security type is None");
      if (IS_VERSION_3_8 (decoder)) {
        decoder->state = rfb_decoder_state_security_result;
      } else {
        decoder->state = rfb_decoder_state_send_client_initialisation;
      }
      break;
    case SECURITY_VNC:{
      unsigned char key[8], *challenge;
      DESContext des_ctx;
      gsize password_len;

      /*
       * VNC authentication is to be used and protocol data is to be sent
       * unencrypted. The server sends a random 16-byte challenge
       */
      GST_DEBUG ("Security type is VNC Authentication");
      /* VNC Authentication can't be used if the password is not set */
      if (!decoder->password) {
        GST_WARNING
            ("VNC Authentication can't be used if the password is not set");
        decoder->error =
            g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
            "VNC servers needs authentication, but no password set");
        return FALSE;
      }

      /* key is 8 bytes and made up of password (padded with 0s if needed) */
      memset (key, 0, 8);
      password_len = strlen (decoder->password);
      memcpy (key, decoder->password, MIN (password_len, 8));

      /* read challenge */
      challenge = rfb_decoder_read (decoder, 16);
      if (challenge == NULL)
        return FALSE;

      /* encrypt 16 challenge bytes in place using key */
      memset (&des_ctx, 0, sizeof (DESContext));
      deskey (&des_ctx, key, EN0);
      des (&des_ctx, challenge, challenge);
      des (&des_ctx, challenge + 8, challenge + 8);

      /* .. and send back to server */
      if (!rfb_decoder_send (decoder, challenge, 16))
        return FALSE;

      GST_DEBUG ("Encrypted challenge sent to server");

      decoder->state = rfb_decoder_state_security_result;
      break;
    }
    default:
      GST_WARNING ("Security type is not known");
      return FALSE;
      break;
  }
  return TRUE;
}

/*
 * The server sends a word to inform the client whether the security handshaking was
 * successful.
 */
static gboolean
rfb_decoder_state_security_result (RfbDecoder * decoder)
{
  if (!rfb_decoder_read (decoder, 4))
    return FALSE;

  if (RFB_GET_UINT32 (decoder->data) != 0) {
    GST_WARNING ("Security handshaking failed");
    if (IS_VERSION_3_8 (decoder)) {
      decoder->state = rfb_decoder_state_reason;
      return TRUE;
    }
    if (decoder->error == NULL) {
      decoder->error = g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
          "authentication failed");
    }
    return FALSE;
  }

  GST_DEBUG ("Security handshake successful");
  decoder->state = rfb_decoder_state_send_client_initialisation;

  return TRUE;
}

static guint8 *
rfb_decoder_message_set_encodings (GSList * encodings_list)
{

  guint8 *message = g_malloc0 (4 + 4 * g_slist_length (encodings_list));
  guint32 *encoding_type;

  message[0] = 0x02;            /* message type */
  RFB_SET_UINT16 (message + 2, g_slist_length (encodings_list));        /* number of encodings */

  /* write all the encoding types */
  encoding_type = (guint32 *) (message + 4);

  while (encodings_list) {
    RFB_SET_UINT32 (encoding_type, GPOINTER_TO_UINT (encodings_list->data));
    encoding_type++;
    encodings_list = encodings_list->next;
  }

  return message;
}

/*
 * rfb_decoder_state_set_encodings:
 * @decoder: The rfb context
 *
 * Sends the encoding types that the client can decode to the server
 *
 * Returns: TRUE if initialization was successful, FALSE on fail.
 */
static gboolean
rfb_decoder_state_set_encodings (RfbDecoder * decoder)
{
  GSList *encoder_list = NULL;
  guint8 *message;

  GST_DEBUG ("entered set encodings");

  encoder_list =
      g_slist_append (encoder_list, GUINT_TO_POINTER (ENCODING_TYPE_HEXTILE));
  encoder_list =
      g_slist_append (encoder_list, GUINT_TO_POINTER (ENCODING_TYPE_CORRE));
  encoder_list =
      g_slist_append (encoder_list, GUINT_TO_POINTER (ENCODING_TYPE_RRE));
  if (decoder->use_copyrect) {
    encoder_list =
        g_slist_append (encoder_list,
        GUINT_TO_POINTER (ENCODING_TYPE_COPYRECT));
  }
  encoder_list =
      g_slist_append (encoder_list, GUINT_TO_POINTER (ENCODING_TYPE_RAW));

  message = rfb_decoder_message_set_encodings (encoder_list);

  if (!rfb_decoder_send (decoder, message,
          4 + 4 * g_slist_length (encoder_list))) {
    g_free (message);
    return FALSE;
  }

  g_free (message);

  decoder->state = rfb_decoder_state_normal;
  decoder->inited = TRUE;

  return TRUE;
}

static gboolean
rfb_decoder_state_send_client_initialisation (RfbDecoder * decoder)
{
  guint8 shared_flag;

  shared_flag = decoder->shared_flag;

  if (!rfb_decoder_send (decoder, &shared_flag, 1))
    return FALSE;

  GST_DEBUG ("shared_flag is %d", shared_flag);

  decoder->state = rfb_decoder_state_wait_for_server_initialisation;
  return TRUE;
}

static gboolean
rfb_decoder_state_wait_for_server_initialisation (RfbDecoder * decoder)
{
  guint32 name_length;

  if (!rfb_decoder_read (decoder, 24))
    return FALSE;

  decoder->width = RFB_GET_UINT16 (decoder->data + 0);
  decoder->height = RFB_GET_UINT16 (decoder->data + 2);
  decoder->bpp = RFB_GET_UINT8 (decoder->data + 4);
  decoder->depth = RFB_GET_UINT8 (decoder->data + 5);
  decoder->big_endian = RFB_GET_UINT8 (decoder->data + 6);
  decoder->true_colour = RFB_GET_UINT8 (decoder->data + 7);
  decoder->red_max = RFB_GET_UINT16 (decoder->data + 8);
  decoder->green_max = RFB_GET_UINT16 (decoder->data + 10);
  decoder->blue_max = RFB_GET_UINT16 (decoder->data + 12);
  decoder->red_shift = RFB_GET_UINT8 (decoder->data + 14);
  decoder->green_shift = RFB_GET_UINT8 (decoder->data + 15);
  decoder->blue_shift = RFB_GET_UINT8 (decoder->data + 16);

  GST_DEBUG ("Server Initialization");
  GST_DEBUG ("width      = %d", decoder->width);
  GST_DEBUG ("height     = %d", decoder->height);
  GST_DEBUG ("bpp        = %d", decoder->bpp);
  GST_DEBUG ("depth      = %d", decoder->depth);
  GST_DEBUG ("big_endian = %d", decoder->big_endian);
  GST_DEBUG ("true_colour= %d", decoder->true_colour);
  GST_DEBUG ("red_max    = %d", decoder->red_max);
  GST_DEBUG ("green_max  = %d", decoder->green_max);
  GST_DEBUG ("blue_max   = %d", decoder->blue_max);
  GST_DEBUG ("red_shift  = %d", decoder->red_shift);
  GST_DEBUG ("green_shift= %d", decoder->green_shift);
  GST_DEBUG ("blue_shift = %d", decoder->blue_shift);

  name_length = RFB_GET_UINT32 (decoder->data + 20);

  if (!rfb_decoder_read (decoder, name_length))
    return FALSE;

  decoder->name = g_strndup ((gchar *) (decoder->data), name_length);
  GST_DEBUG ("name       = %s", decoder->name);

  /* check if we need cropping */

  if (decoder->offset_x > 0) {
    if (decoder->offset_x > decoder->width) {
      GST_WARNING
          ("Trying to crop more than the width of the server.  Setting offset-x to 0.");
      decoder->offset_x = 0;
    } else {
      decoder->width -= decoder->offset_x;
    }
  }
  if (decoder->offset_y > 0) {
    if (decoder->offset_y > decoder->height) {
      GST_WARNING
          ("Trying to crop more than the height of the server. Setting offset-y to 0.");
      decoder->offset_y = 0;
    } else {
      decoder->height -= decoder->offset_y;
    }
  }
  if (decoder->rect_width > 0) {
    if (decoder->rect_width > decoder->width) {
      GST_WARNING
          ("Trying to crop more than the width of the server. Setting width to %u.",
          decoder->width);
      decoder->rect_width = decoder->width;
    } else {
      decoder->width = decoder->rect_width;
    }
  }
  if (decoder->rect_height > 0) {
    if (decoder->rect_height > decoder->height) {
      GST_WARNING
          ("Trying to crop more than the height of the server. Setting height to %u.",
          decoder->height);
      decoder->rect_height = decoder->height;
    } else {
      decoder->height = decoder->rect_height;
    }
  }

  decoder->state = rfb_decoder_state_set_encodings;
  return TRUE;
}

static gboolean
rfb_decoder_state_normal (RfbDecoder * decoder)
{
  gint message_type;

  GST_DEBUG ("decoder_state_normal");

  if (!rfb_decoder_read (decoder, 1))
    return FALSE;

  message_type = RFB_GET_UINT8 (decoder->data);

  switch (message_type) {
    case MESSAGE_TYPE_FRAMEBUFFER_UPDATE:
      GST_DEBUG ("Receiving framebuffer update");
      decoder->state = rfb_decoder_state_framebuffer_update;
      break;
    case 1:
      decoder->state = rfb_decoder_state_set_colour_map_entries;
      break;
    case 2:
      /* bell, ignored */
      decoder->state = rfb_decoder_state_normal;
      break;
    case 3:
      decoder->state = rfb_decoder_state_server_cut_text;
      break;
    default:
      g_critical ("unknown message type %d", message_type);
  }

  return TRUE;
}

static gboolean
rfb_decoder_state_framebuffer_update (RfbDecoder * decoder)
{

  if (!rfb_decoder_read (decoder, 3))
    return FALSE;

  decoder->n_rects = RFB_GET_UINT16 (decoder->data + 1);
  GST_DEBUG ("Number of rectangles : %d", decoder->n_rects);

  decoder->state = rfb_decoder_state_framebuffer_update_rectangle;

  return TRUE;
}

static gboolean
rfb_decoder_state_framebuffer_update_rectangle (RfbDecoder * decoder)
{
  gint x, y, w, h;
  gint encoding;
  gboolean ret = FALSE;

  if (!rfb_decoder_read (decoder, 12))
    return FALSE;

  x = RFB_GET_UINT16 (decoder->data + 0) - decoder->offset_x;
  y = RFB_GET_UINT16 (decoder->data + 2) - decoder->offset_y;
  w = RFB_GET_UINT16 (decoder->data + 4);
  h = RFB_GET_UINT16 (decoder->data + 6);
  encoding = RFB_GET_UINT32 (decoder->data + 8);

  GST_DEBUG ("update received");
  GST_DEBUG ("x:%d y:%d", x, y);
  GST_DEBUG ("w:%d h:%d", w, h);
  GST_DEBUG ("encoding: %d", encoding);

  if (((w * h) + (x * y)) > (decoder->width * decoder->height)) {
    GST_ERROR ("Desktop resize is unsupported.");
    decoder->state = NULL;
    return TRUE;
  }

  switch (encoding) {
    case ENCODING_TYPE_RAW:
      ret = rfb_decoder_raw_encoding (decoder, x, y, w, h);
      break;
    case ENCODING_TYPE_COPYRECT:
      ret = rfb_decoder_copyrect_encoding (decoder, x, y, w, h);
      break;
    case ENCODING_TYPE_RRE:
      ret = rfb_decoder_rre_encoding (decoder, x, y, w, h);
      break;
    case ENCODING_TYPE_CORRE:
      ret = rfb_decoder_corre_encoding (decoder, x, y, w, h);
      break;
    case ENCODING_TYPE_HEXTILE:
      ret = rfb_decoder_hextile_encoding (decoder, x, y, w, h);
      break;
    default:
      g_critical ("unimplemented encoding\n");
      break;
  }

  if (!ret)
    return FALSE;

  decoder->n_rects--;
  if (decoder->n_rects == 0) {
    decoder->state = NULL;
  } else {
    decoder->state = rfb_decoder_state_framebuffer_update_rectangle;
  }

  return TRUE;
}

static gboolean
rfb_decoder_raw_encoding (RfbDecoder * decoder, gint start_x, gint start_y,
    gint rect_w, gint rect_h)
{
  gint size;
  guint8 *frame, *p;
  guint32 raw_line_size;

  raw_line_size = rect_w * decoder->bytespp;
  size = rect_h * raw_line_size;

  GST_DEBUG ("Reading %d bytes (%dx%d)", size, rect_w, rect_h);

  if (!rfb_decoder_read (decoder, size))
    return FALSE;

  frame =
      decoder->frame + (((start_y * decoder->rect_width) +
          start_x) * decoder->bytespp);
  p = decoder->data;

  while (rect_h--) {
    memcpy (frame, p, raw_line_size);
    p += raw_line_size;
    frame += decoder->line_size;
  }

  return TRUE;
}

static gboolean
rfb_decoder_copyrect_encoding (RfbDecoder * decoder, gint start_x, gint start_y,
    gint rect_w, gint rect_h)
{
  guint16 src_x, src_y;
  gint line_width, copyrect_width;
  guint8 *src, *dst;

  if (!rfb_decoder_read (decoder, 4))
    return FALSE;

  /* don't forget the offset */
  src_x = RFB_GET_UINT16 (decoder->data) - decoder->offset_x;
  src_y = RFB_GET_UINT16 (decoder->data + 2) - decoder->offset_y;
  GST_DEBUG ("Copyrect from %d %d", src_x, src_y);

  copyrect_width = rect_w * decoder->bytespp;
  line_width = decoder->line_size;
  src =
      decoder->prev_frame + ((src_y * decoder->rect_width) +
      src_x) * decoder->bytespp;
  dst =
      decoder->frame + ((start_y * decoder->rect_width) +
      start_x) * decoder->bytespp;

  while (rect_h--) {
    memcpy (dst, src, copyrect_width);
    src += line_width;
    dst += line_width;
  }

  return TRUE;
}

static void
rfb_decoder_fill_rectangle (RfbDecoder * decoder, gint x, gint y, gint w,
    gint h, guint32 color)
{
  /* fill the whole region with the same color */

  guint32 *offset;
  gint i, j;

  for (i = 0; i < h; i++) {
    offset =
        (guint32 *) (decoder->frame + ((x + (y +
                    i) * decoder->rect_width)) * decoder->bytespp);
    for (j = 0; j < w; j++) {
      *(offset++) = color;
    }
  }
}

static gboolean
rfb_decoder_rre_encoding (RfbDecoder * decoder, gint start_x, gint start_y,
    gint rect_w, gint rect_h)
{
  guint32 number_of_rectangles, color;
  guint16 x, y, w, h;

  if (!rfb_decoder_read (decoder, 4 + decoder->bytespp))
    return FALSE;

  number_of_rectangles = RFB_GET_UINT32 (decoder->data);
  color = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data + 4)));

  GST_DEBUG ("number of rectangles :%d", number_of_rectangles);

  /* color the background of this rectangle */
  rfb_decoder_fill_rectangle (decoder, start_x, start_y, rect_w, rect_h, color);

  while (number_of_rectangles--) {

    if (!rfb_decoder_read (decoder, decoder->bytespp + 8))
      return FALSE;

    color = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data)));
    x = RFB_GET_UINT16 (decoder->data + decoder->bytespp);
    y = RFB_GET_UINT16 (decoder->data + decoder->bytespp + 2);
    w = RFB_GET_UINT16 (decoder->data + decoder->bytespp + 4);
    h = RFB_GET_UINT16 (decoder->data + decoder->bytespp + 6);

    /* draw the rectangle in the foreground */
    rfb_decoder_fill_rectangle (decoder, start_x + x, start_y + y, w, h, color);
  }

  return TRUE;
}

static gboolean
rfb_decoder_corre_encoding (RfbDecoder * decoder, gint start_x, gint start_y,
    gint rect_w, gint rect_h)
{
  guint32 number_of_rectangles, color;
  guint8 x, y, w, h;

  if (!rfb_decoder_read (decoder, 4 + decoder->bytespp))
    return FALSE;

  number_of_rectangles = RFB_GET_UINT32 (decoder->data);
  color = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data + 4)));

  GST_DEBUG ("number of rectangles :%d", number_of_rectangles);

  /* color the background of this rectangle */
  rfb_decoder_fill_rectangle (decoder, start_x, start_y, rect_w, rect_h, color);

  while (number_of_rectangles--) {

    if (!rfb_decoder_read (decoder, decoder->bytespp + 4))
      return FALSE;

    color = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data)));
    x = RFB_GET_UINT8 (decoder->data + decoder->bytespp);
    y = RFB_GET_UINT8 (decoder->data + decoder->bytespp + 1);
    w = RFB_GET_UINT8 (decoder->data + decoder->bytespp + 2);
    h = RFB_GET_UINT8 (decoder->data + decoder->bytespp + 3);

    /* draw the rectangle in the foreground */
    rfb_decoder_fill_rectangle (decoder, start_x + x, start_y + y, w, h, color);
  }

  return TRUE;
}

static gboolean
rfb_decoder_hextile_encoding (RfbDecoder * decoder, gint start_x, gint start_y,
    gint rect_w, gint rect_h)
{
  gint32 x, x_count G_GNUC_UNUSED, x_end, x_max, x_max_16;
  gint32 y, y_count G_GNUC_UNUSED, y_end, y_max, y_max_16;
  guint8 subencoding, nr_subrect, xy, wh;
  guint32 background, foreground;

  foreground = background = 0;
  x_end = rect_w % 16;
  x_count = rect_w / 16 + (x_end > 0 ? 1 : 0);
  y_end = rect_h % 16;
  y_count = rect_h / 16 + (y_end > 0 ? 1 : 0);
  x_max = start_x + rect_w;
  y_max = start_y + rect_h;
  x_max_16 = x_max - 16;
  y_max_16 = y_max - 16;

  for (y = start_y; y < y_max; y += 16) {
    for (x = start_x; x < x_max; x += 16) {

      if (!rfb_decoder_read (decoder, 1))
        return FALSE;

      subencoding = RFB_GET_UINT8 (decoder->data);

      if (subencoding & SUBENCODING_RAW) {
        rfb_decoder_raw_encoding (decoder, x, y,
            (x <= x_max_16 ? 16 : x_end), (y <= y_max_16 ? 16 : y_end));
        continue;
      }

      if (subencoding & SUBENCODING_BACKGROUND) {
        if (!rfb_decoder_read (decoder, decoder->bytespp))
          return FALSE;

        background = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data)));
      }
      rfb_decoder_fill_rectangle (decoder, x, y,
          (x <= x_max_16 ? 16 : x_end), (y <= y_max_16 ? 16 : y_end),
          background);

      if (subencoding & SUBENCODING_FOREGROUND) {
        if (!rfb_decoder_read (decoder, decoder->bytespp))
          return FALSE;

        foreground = GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data)));
      }

      if (subencoding & SUBENCODING_ANYSUBRECTS) {
        if (!rfb_decoder_read (decoder, 1))
          return FALSE;

        nr_subrect = RFB_GET_UINT8 (decoder->data);
      } else {
        continue;
      }

      if (subencoding & SUBENCODING_SUBRECTSCOLORED) {
        guint offset = 0;

        if (!rfb_decoder_read (decoder, nr_subrect * (2 + decoder->bytespp)))
          return FALSE;

        while (nr_subrect--) {
          foreground =
              GUINT32_SWAP_LE_BE ((RFB_GET_UINT32 (decoder->data + offset)));
          offset += decoder->bytespp;
          xy = RFB_GET_UINT8 (decoder->data + offset++);
          wh = RFB_GET_UINT8 (decoder->data + offset++);
          rfb_decoder_fill_rectangle (decoder, x + (xy >> 4), y + (xy & 0xF),
              1 + (wh >> 4), 1 + (wh & 0xF), foreground);
        }
      } else {
        guint offset = 0;

        if (!rfb_decoder_read (decoder, 2 * nr_subrect))
          return FALSE;

        while (nr_subrect--) {
          xy = RFB_GET_UINT8 (decoder->data + offset++);
          wh = RFB_GET_UINT8 (decoder->data + offset++);
          rfb_decoder_fill_rectangle (decoder, x + (xy >> 4), y + (xy & 0xF),
              1 + (wh >> 4), 1 + (wh & 0xF), foreground);
        }
      }
    }
  }

  return TRUE;
}

static gboolean
rfb_decoder_state_set_colour_map_entries (RfbDecoder * decoder)
{
  g_critical ("not implemented");

  return FALSE;
}

static gboolean
rfb_decoder_state_server_cut_text (RfbDecoder * decoder)
{
  gint cut_text_length;

  /* 3 bytes padding, 4 bytes cut_text_length */
  if (!rfb_decoder_read (decoder, 7))
    return FALSE;

  cut_text_length = RFB_GET_UINT32 (decoder->data + 3);

  if (!rfb_decoder_read (decoder, cut_text_length))
    return FALSE;

  GST_DEBUG ("rfb_decoder_state_server_cut_text: throw away '%s'",
      decoder->data);

  decoder->state = rfb_decoder_state_normal;

  return TRUE;
}
