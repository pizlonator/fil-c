/*
 * camconditionalaccess.c - CAM (EN50221) Conditional Access Resource
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <unistd.h>
#include <string.h>
#include "camutils.h"
#include "camconditionalaccess.h"

#define GST_CAT_DEFAULT cam_debug_cat
#define TAG_CONDITIONAL_ACCESS_INFO_ENQUIRY 0x9F8030
#define TAG_CONDITIONAL_ACCESS_INFO_REPLY 0x9F8031
#define TAG_CONDITIONAL_ACCESS_PMT 0x9F8032
#define TAG_CONDITIONAL_ACCESS_PMT_REPLY 0x9F8033

static CamReturn session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status);
static CamReturn open_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn close_impl (CamALApplication * application,
    CamSLSession * session);
static CamReturn data_impl (CamALApplication * application,
    CamSLSession * session, guint tag, guint8 * buffer, guint length);

CamConditionalAccess *
cam_conditional_access_new (void)
{
  CamConditionalAccess *cas;
  CamALApplication *application;

  cas = g_new0 (CamConditionalAccess, 1);

  application = CAM_AL_APPLICATION (cas);
  _cam_al_application_init (application);
  application->resource_id = CAM_AL_CONDITIONAL_ACCESS_ID;
  application->session_request = session_request_impl;
  application->open = open_impl;
  application->close = close_impl;
  application->data = data_impl;

  cas->ready = FALSE;

  return cas;
}

void
cam_conditional_access_destroy (CamConditionalAccess * cas)
{
  _cam_al_application_destroy (CAM_AL_APPLICATION (cas));
  g_free (cas);
}

static CamReturn
send_ca_pmt (CamConditionalAccess * cas, GstMpegtsPMT * pmt,
    guint8 list_management, guint8 cmd_id)
{
  CamReturn ret;
  guint8 *buffer;
  guint buffer_size;
  guint offset;
  guint8 *ca_pmt;
  guint ca_pmt_size;
  GList *walk;

  ca_pmt = cam_build_ca_pmt (pmt, list_management, cmd_id, &ca_pmt_size);
  cam_al_calc_buffer_size (CAM_AL_APPLICATION (cas)->al,
      ca_pmt_size, &buffer_size, &offset);

  buffer = g_malloc0 (buffer_size);
  memcpy (buffer + offset, ca_pmt, ca_pmt_size);

  for (walk = CAM_AL_APPLICATION (cas)->sessions; walk; walk = walk->next) {
    CamSLSession *session = CAM_SL_SESSION (walk->data);

    ret = cam_al_application_write (CAM_AL_APPLICATION (cas), session,
        TAG_CONDITIONAL_ACCESS_PMT, buffer, buffer_size, ca_pmt_size);
    if (CAM_FAILED (ret)) {
      GST_ERROR ("error sending ca_pmt to slot %d, error: %d",
          session->connection->slot, ret);
      continue;
    }
  }

  g_free (ca_pmt);
  g_free (buffer);

  return CAM_RETURN_OK;
}

CamReturn
cam_conditional_access_set_pmt (CamConditionalAccess * cas,
    GstMpegtsPMT * pmt, CamConditionalAccessPmtFlag flag)
{
  return send_ca_pmt (cas, pmt, flag, 0x01 /* ok_descrambling */ );
}

static CamReturn
send_simple (CamConditionalAccess * cas, CamSLSession * session, guint tag)
{
  guint8 *buffer;
  guint offset;
  guint buffer_size;
  CamReturn ret;

  cam_al_calc_buffer_size (CAM_AL_APPLICATION (cas)->al, 0, &buffer_size,
      &offset);
  buffer = g_malloc (buffer_size);

  ret = cam_al_application_write (CAM_AL_APPLICATION (cas), session,
      tag, buffer, buffer_size, 0);

  g_free (buffer);

  return ret;
}

static CamReturn
send_conditional_access_enquiry (CamConditionalAccess * cas,
    CamSLSession * session)
{
  GST_DEBUG ("sending application CAS enquiry");
  return send_simple (cas, session, TAG_CONDITIONAL_ACCESS_INFO_ENQUIRY);
}

static CamReturn
session_request_impl (CamALApplication * application,
    CamSLSession * session, CamSLResourceStatus * status)
{
  *status = CAM_SL_RESOURCE_STATUS_OPEN;

  return CAM_RETURN_OK;
}

static CamReturn
open_impl (CamALApplication * application, CamSLSession * session)
{
  CamConditionalAccess *cas = CAM_CONDITIONAL_ACCESS (application);

  GST_INFO ("opening conditional access session %d", session->session_nb);

  return send_conditional_access_enquiry (cas, session);
}

static CamReturn
close_impl (CamALApplication * application, CamSLSession * session)
{
  GST_INFO ("closing conditional access session %d", session->session_nb);

  return CAM_RETURN_OK;
}

static CamReturn
handle_conditional_access_info_reply (CamConditionalAccess * cas,
    CamSLSession * session, guint8 * buffer, guint length)
{
#ifndef GST_DISABLE_GST_DEBUG
  int i;
  guint16 cas_id;

  GST_INFO ("conditional access info enquiry reply");

  for (i = 0; i < length / 2; ++i) {
    cas_id = GST_READ_UINT16_BE (buffer);

    GST_INFO ("slot %d, cas_id 0x%x", session->connection->slot, cas_id);

    buffer += 2;
  }

  cas->ready = TRUE;
#endif

  return CAM_RETURN_OK;
}

static CamReturn
handle_conditional_access_pmt_reply (CamConditionalAccess * cas,
    CamSLSession * session, guint8 * buffer, guint length)
{
#ifndef GST_DISABLE_GST_DEBUG
  guint16 program_num;
  guint8 version_num, current_next_indicator;

  GST_INFO ("conditional access PMT reply");

  program_num = GST_READ_UINT16_BE (buffer);
  buffer += 2;

  GST_INFO ("program_number : %d", program_num);

  version_num = *buffer >> 1 & 0x1f;
  current_next_indicator = *buffer & 0x1;
  buffer++;

  GST_INFO ("version_num:%d, current_next_indicator:%d",
      version_num, current_next_indicator);

  GST_INFO ("CA_enable : %d (0x%x)", *buffer >> 7 ? *buffer & 0x7f : 0,
      *buffer);
  buffer++;

  length -= 4;

  while (length > 0) {
    guint16 PID = GST_READ_UINT16_BE (buffer);
    buffer += 2;
    GST_INFO ("PID 0x%x CA_enable : %d (0x%x)", PID,
        *buffer >> 7 ? *buffer & 0x7f : 0, *buffer);
    buffer++;

    length -= 3;
  }
#endif

  return CAM_RETURN_OK;
}

static CamReturn
data_impl (CamALApplication * application, CamSLSession * session,
    guint tag, guint8 * buffer, guint length)
{
  CamReturn ret;
  CamConditionalAccess *cas = CAM_CONDITIONAL_ACCESS (application);

  switch (tag) {
    case TAG_CONDITIONAL_ACCESS_INFO_REPLY:
      ret = handle_conditional_access_info_reply (cas, session, buffer, length);
      break;
    case TAG_CONDITIONAL_ACCESS_PMT_REPLY:
      ret = handle_conditional_access_pmt_reply (cas, session, buffer, length);
      break;
    default:
      GST_WARNING ("Got unknown callback, tag 0x%x", tag);
      g_return_val_if_reached (CAM_RETURN_ERROR);
  }

  return ret;
}
