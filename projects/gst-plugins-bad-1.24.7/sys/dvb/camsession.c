/*
 * camsession.c - GStreamer CAM (EN50221) Session Layer
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
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

#include "camsession.h"

#define GST_CAT_DEFAULT cam_debug_cat
#define I_TAG 0
#define I_LENGTH_FB 1

#define TAG_SESSION_NUMBER 0x90
#define TAG_OPEN_SESSION_REQUEST 0x91
#define TAG_OPEN_SESSION_RESPONSE 0x92
#define TAG_CREATE_SESSION 0x93
#define TAG_CREATE_SESSION_RESPONSE 0x94
#define TAG_CLOSE_SESSION_REQUEST 0x95
#define TAG_CLOSE_SESSION_RESPONSE 0x96

static CamReturn connection_data_cb (CamTL * tl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length);

static CamSLSession *
cam_sl_session_new (CamSL * sl, CamTLConnection * connection,
    guint16 session_nb, guint resource_id)
{
  CamSLSession *session = g_new0 (CamSLSession, 1);

  session->state = CAM_SL_SESSION_STATE_IDLE;
  session->sl = sl;
  session->connection = connection;
  session->session_nb = session_nb;
  session->resource_id = resource_id;

  return session;
}

static void
cam_sl_session_destroy (CamSLSession * session)
{
  g_free (session);
}

CamSL *
cam_sl_new (CamTL * tl)
{
  CamSL *sl = g_new0 (CamSL, 1);

  sl->sessions = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) cam_sl_session_destroy);

  tl->user_data = sl;
  tl->connection_data = connection_data_cb;

  return sl;
}

void
cam_sl_destroy (CamSL * sl)
{
  g_hash_table_destroy (sl->sessions);

  g_free (sl);
}

CamReturn
cam_sl_create_session (CamSL * sl,
    CamTLConnection * connection, guint resource_id,
    CamSLSession ** out_session)
{
  CamReturn ret;
  CamSLSession *session = NULL;
  guint size;
  guint offset;
  guint8 *tpdu = NULL;
  guint8 *spdu;
  guint16 session_nb;

  /* FIXME: implement session number allocations properly */
  if (sl->session_ids == G_MAXUINT16)
    return CAM_RETURN_SESSION_TOO_MANY_SESSIONS;

  session_nb = ++sl->session_ids;
  session = cam_sl_session_new (sl, connection, session_nb, resource_id);

  /* SPDU layout (8 bytes):
   * TAG_CREATE_SESSION 1 byte
   * length_field () 1 byte
   * resource_id 4 bytes
   * session_nb 2 bytes
   */

  /* get TPDU size */
  cam_tl_calc_buffer_size (sl->tl, 8, &size, &offset);

  tpdu = (guint8 *) g_malloc (size);
  spdu = tpdu + offset;

  /* SPDU header */
  /* tag */
  spdu[0] = TAG_CREATE_SESSION;
  /* fixed length_field */
  spdu[1] = 6;

  /* SPDU body */
  /* resource id */
  GST_WRITE_UINT32_BE (&spdu[2], resource_id);
  /* session_nb */
  GST_WRITE_UINT16_BE (&spdu[6], session_nb);

  /* write the TPDU */
  ret = cam_tl_connection_write (session->connection, tpdu, size, 8);
  if (CAM_FAILED (ret))
    goto error;

  *out_session = session;

  g_free (tpdu);
  return CAM_RETURN_OK;

error:
  if (session)
    cam_sl_session_destroy (session);

  g_free (tpdu);

  return ret;
}

/* send a TAG_CLOSE_SESSION SPDU */
CamReturn
cam_sl_session_close (CamSLSession * session)
{
  CamReturn ret;
  guint size;
  guint offset;
  guint8 *tpdu = NULL;
  guint8 *spdu;
  CamSL *sl = session->sl;

  /* SPDU layout (4 bytes):
   * TAG_CLOSE_SESSION 1 byte
   * length_field () 1 byte
   * session_nb 2 bytes
   */

  /* get the size of the TPDU */
  cam_tl_calc_buffer_size (sl->tl, 4, &size, &offset);

  tpdu = (guint8 *) g_malloc (size);
  /* the spdu header starts after the TPDU headers */
  spdu = tpdu + offset;

  /* SPDU header */
  /* tag */
  spdu[0] = TAG_CLOSE_SESSION_REQUEST;
  /* fixed length_field */
  spdu[1] = 2;
  /* SPDU body */
  /* session_nb */
  GST_WRITE_UINT16_BE (&spdu[2], session->session_nb);

  /* write the TPDU */
  ret = cam_tl_connection_write (session->connection, tpdu, size, 4);
  if (CAM_FAILED (ret))
    goto error;

  session->state = CAM_SL_SESSION_STATE_CLOSING;

  g_free (tpdu);

  return CAM_RETURN_OK;

error:
  g_free (tpdu);

  return ret;
}

void
cam_sl_calc_buffer_size (CamSL * sl, guint body_length,
    guint * buffer_size, guint * offset)
{
  /* an APDU is sent in a SESSION_NUMBER SPDU, which has a fixed header size (4
   * bytes) */
  cam_tl_calc_buffer_size (sl->tl, 4 + body_length, buffer_size, offset);
  *offset += 4;
}

CamReturn
cam_sl_session_write (CamSLSession * session,
    guint8 * buffer, guint buffer_size, guint body_length)
{
  guint8 *spdu;

  /* SPDU layout (4 + body_length bytes):
   * TAG_SESSION_NUMBER (1 byte)
   * length_field (1 byte)
   * session number (2 bytes)
   * one or more APDUs (body_length bytes)
   */

  spdu = (buffer + buffer_size) - body_length - 4;
  spdu[0] = TAG_SESSION_NUMBER;
  spdu[1] = 2;
  GST_WRITE_UINT16_BE (&spdu[2], session->session_nb);

  /* add our header to the body length */
  return cam_tl_connection_write (session->connection,
      buffer, buffer_size, 4 + body_length);
}

static CamReturn
send_open_session_response (CamSL * sl, CamSLSession * session, guint8 status)
{
  CamReturn ret;
  guint8 *tpdu;
  guint size;
  guint offset;
  guint8 *spdu;

  /* SPDU layout (9 bytes):
   * TAG_OPEN_SESSION_RESPONSE 1 byte
   * length_field () 1 byte
   * session_status 1 byte
   * resource_id 4 bytes
   * session_nb 2 bytes
   */

  cam_tl_calc_buffer_size (session->sl->tl, 9, &size, &offset);

  tpdu = g_malloc0 (size);
  spdu = tpdu + offset;

  spdu[0] = TAG_OPEN_SESSION_RESPONSE;
  /* fixed length_field () */
  spdu[1] = 7;
  spdu[2] = status;
  GST_WRITE_UINT32_BE (&spdu[3], session->resource_id);
  GST_WRITE_UINT16_BE (&spdu[7], session->session_nb);

  ret = cam_tl_connection_write (session->connection, tpdu, size, 9);
  g_free (tpdu);
  if (CAM_FAILED (ret))
    return ret;

  return CAM_RETURN_OK;
}

static CamReturn
send_close_session_response (CamSL * sl, CamSLSession * session, guint8 status)
{
  CamReturn ret;
  guint8 *tpdu;
  guint size;
  guint offset;
  guint8 *spdu;

  /* SPDU layout (5 bytes):
   * TAG_CLOSE_SESSION_RESPONSE 1 byte
   * length_field () 1 byte
   * session_status 1 byte
   * session_nb 2 bytes
   */

  cam_tl_calc_buffer_size (session->sl->tl, 5, &size, &offset);

  tpdu = g_malloc0 (size);
  spdu = tpdu + offset;

  spdu[0] = TAG_OPEN_SESSION_RESPONSE;
  /* fixed length_field() */
  spdu[1] = 3;
  spdu[2] = status;
  GST_WRITE_UINT16_BE (&spdu[3], session->session_nb);

  ret = cam_tl_connection_write (session->connection, tpdu, size, 5);
  g_free (tpdu);
  if (CAM_FAILED (ret))
    return ret;

  return CAM_RETURN_OK;
}

static CamReturn
handle_open_session_request (CamSL * sl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length)
{
  CamReturn ret;
  guint resource_id;
  guint status;
  guint16 session_nb;
  CamSLSession *session;

  /* SPDU layout (6 bytes):
   * TAG_OPEN_SESSION_REQUEST (1 byte)
   * length_field() (1 byte)
   * resource id (4 bytes)
   */
  if (spdu_length != 6) {
    GST_ERROR ("expected OPEN_SESSION_REQUEST to be 6 bytes, got %d",
        spdu_length);
    return CAM_RETURN_SESSION_ERROR;
  }

  /* skip tag and length_field () */
  resource_id = GST_READ_UINT32_BE (&spdu[2]);

  /* create a new session */
  if (sl->session_ids == G_MAXUINT16) {
    GST_ERROR ("too many sessions opened");
    return CAM_RETURN_SESSION_TOO_MANY_SESSIONS;
  }

  session_nb = ++sl->session_ids;
  session = cam_sl_session_new (sl, connection, session_nb, resource_id);

  GST_INFO ("session request: %d %x", session_nb, session->resource_id);

  if (sl->open_session_request) {
    /* forward the request to the upper layer */
    ret = sl->open_session_request (sl, session, &status);
    if (CAM_FAILED (ret))
      goto error;
  } else {
    status = 0xF0;
  }

  ret = send_open_session_response (sl, session, (guint8) status);
  if (CAM_FAILED (ret))
    goto error;

  GST_INFO ("session request response: %d %x", session_nb, status);

  if (status == CAM_SL_RESOURCE_STATUS_OPEN) {
    /* if the session has been accepted add it and signal */
    session->state = CAM_SL_SESSION_STATE_ACTIVE;
    g_hash_table_insert (sl->sessions,
        GINT_TO_POINTER ((guint) session_nb), session);

    if (sl->session_opened) {
      /* notify the upper layer */
      ret = sl->session_opened (sl, session);
      if (CAM_FAILED (ret))
        return ret;
    }
  } else {
    /* session request wasn't accepted */
    cam_sl_session_destroy (session);
  }

  return CAM_RETURN_OK;

error:
  cam_sl_session_destroy (session);

  return ret;
}

static CamReturn
handle_create_session_response (CamSL * sl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length)
{
  guint16 session_nb;
  CamSLSession *session;

  /* SPDU layout (9 bytes):
   * TAG_CREATE_SESSION_RESPONSE (1 byte)
   * length_field() (1 byte)
   * status (1 byte)
   * resource id (4 bytes)
   * session number (2 bytes)
   */
  if (spdu_length != 9) {
    GST_ERROR ("expected CREATE_SESSION_RESPONSE to be 9 bytes, got %d",
        spdu_length);
    return CAM_RETURN_SESSION_ERROR;
  }

  /* skip tag and length */
  /* status = spdu[2]; */
  /* resource_id = GST_READ_UINT32_BE (&spdu[3]); */
  session_nb = GST_READ_UINT16_BE (&spdu[7]);

  session = g_hash_table_lookup (sl->sessions,
      GINT_TO_POINTER ((guint) session_nb));
  if (session == NULL) {
    GST_DEBUG ("got CREATE_SESSION_RESPONSE for unknown session: %d",
        session_nb);
    return CAM_RETURN_SESSION_ERROR;
  }

  if (session->state == CAM_SL_SESSION_STATE_CLOSING) {
    GST_DEBUG ("ignoring CREATE_SESSION_RESPONSE for closing session: %d",
        session_nb);
    return CAM_RETURN_OK;
  }

  session->state = CAM_SL_SESSION_STATE_ACTIVE;

  GST_DEBUG ("session opened %d", session->session_nb);

  if (sl->session_opened)
    /* notify the upper layer */
    return sl->session_opened (sl, session);
  return CAM_RETURN_OK;
}

static CamReturn
handle_close_session_request (CamSL * sl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length)
{
  CamReturn ret;
  guint16 session_nb;
  CamSLSession *session;
  guint8 status = 0;

  /* SPDU layout (4 bytes):
   * TAG_CLOSE_SESSION_REQUEST (1 byte)
   * length_field () (1 byte)
   * session number (2 bytes)
   */
  if (spdu_length != 4) {
    GST_ERROR ("expected CLOSE_SESSION_REQUEST to be 4 bytes, got %d",
        spdu_length);
    return CAM_RETURN_SESSION_ERROR;
  }

  /* skip tag and length_field() */
  session_nb = GST_READ_UINT16_BE (&spdu[2]);

  GST_DEBUG ("close session request %d", session_nb);

  session = g_hash_table_lookup (sl->sessions,
      GINT_TO_POINTER ((guint) session_nb));

  if (session == NULL) {
    GST_WARNING ("got CLOSE_SESSION_REQUEST for unknown session: %d",
        session_nb);
    return CAM_RETURN_OK;
  }

  if (session->state == CAM_SL_SESSION_STATE_CLOSING) {
    GST_WARNING ("got CLOSE_SESSION_REQUEST for closing session: %d",
        session_nb);
    status = 0xF0;
  }

  GST_DEBUG ("close session response: %d %d", session->session_nb, status);

  ret = send_close_session_response (sl, session, status);
  if (CAM_FAILED (ret))
    return ret;

  if (session->state != CAM_SL_SESSION_STATE_CLOSING) {
    GST_DEBUG ("session closed %d", session->session_nb);

    if (sl->session_closed)
      ret = sl->session_closed (sl, session);

    g_hash_table_remove (sl->sessions,
        GINT_TO_POINTER ((guint) session->session_nb));

    if (CAM_FAILED (ret))
      return ret;
  }

  return CAM_RETURN_OK;
}

static CamReturn
handle_close_session_response (CamSL * sl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length)
{
  guint16 session_nb;
  CamSLSession *session;
  CamReturn ret = CAM_RETURN_OK;

  /* SPDU layout (5 bytes):
   * TAG_CLOSE_SESSION_RESPONSE (1 byte)
   * length_field () (1 byte)
   * status (1 byte)
   * session number (2 bytes)
   */

  if (spdu_length != 5) {
    GST_ERROR ("expected CLOSE_SESSION_RESPONSE to be 5 bytes, got %d",
        spdu_length);
    return CAM_RETURN_SESSION_ERROR;
  }

  /* skip tag, length_field() and session_status */
  session_nb = GST_READ_UINT16_BE (&spdu[3]);

  session = g_hash_table_lookup (sl->sessions,
      GINT_TO_POINTER ((guint) session_nb));
  if (session == NULL || session->state != CAM_SL_SESSION_STATE_ACTIVE) {
    GST_ERROR ("unexpected CLOSED_SESSION_RESPONSE");
    return CAM_RETURN_SESSION_ERROR;
  }

  GST_DEBUG ("session closed %d", session->session_nb);

  if (sl->session_closed)
    ret = sl->session_closed (sl, session);

  g_hash_table_remove (sl->sessions,
      GINT_TO_POINTER ((guint) session->session_nb));

  return ret;
}

static CamReturn
handle_session_data (CamSL * sl, CamTLConnection * connection,
    guint8 * spdu, guint length)
{
  guint16 session_nb;
  CamSLSession *session;

  /* SPDU layout (>= 4 bytes):
   * TAG_SESSION_NUMBER (1 byte)
   * length_field() (1 byte)
   * session number (2 bytes)
   * one or more APDUs 
   */

  if (length < 4) {
    GST_ERROR ("invalid SESSION_NUMBER SPDU length %d", length);
    return CAM_RETURN_SESSION_ERROR;
  }

  session_nb = GST_READ_UINT16_BE (&spdu[2]);

  session = g_hash_table_lookup (sl->sessions,
      GINT_TO_POINTER ((guint) session_nb));
  if (session == NULL) {
    GST_ERROR ("got SESSION_NUMBER on an unknown connection: %d", session_nb);
    return CAM_RETURN_SESSION_ERROR;
  }

  if (sl->session_data)
    /* pass the APDUs to the upper layer, removing our 4-bytes header */
    return sl->session_data (sl, session, spdu + 4, length - 4);

  return CAM_RETURN_OK;
}

static CamReturn
connection_data_cb (CamTL * tl, CamTLConnection * connection,
    guint8 * spdu, guint spdu_length)
{
  CamReturn ret;
  CamSL *sl = CAM_SL (tl->user_data);

  switch (spdu[I_TAG]) {
    case TAG_CREATE_SESSION_RESPONSE:
      ret = handle_create_session_response (sl, connection, spdu, spdu_length);
      break;
    case TAG_OPEN_SESSION_REQUEST:
      ret = handle_open_session_request (sl, connection, spdu, spdu_length);
      break;
    case TAG_CLOSE_SESSION_REQUEST:
      ret = handle_close_session_request (sl, connection, spdu, spdu_length);
      break;
    case TAG_CLOSE_SESSION_RESPONSE:
      ret = handle_close_session_response (sl, connection, spdu, spdu_length);
      break;
    case TAG_SESSION_NUMBER:
      ret = handle_session_data (sl, connection, spdu, spdu_length);
      break;
    default:
      g_return_val_if_reached (CAM_RETURN_SESSION_ERROR);
  }

  return ret;
}
