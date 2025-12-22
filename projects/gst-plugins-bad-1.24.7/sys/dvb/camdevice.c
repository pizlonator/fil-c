/*
 * camdevice.c - GStreamer hardware CAM support
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

#include <glib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/dvb/ca.h>

#include "camdevice.h"

#define GST_CAT_DEFAULT cam_debug_cat

CamDevice *
cam_device_new (void)
{
  CamDevice *device = g_new0 (CamDevice, 1);

  device->state = CAM_DEVICE_STATE_CLOSED;

  return device;
}

static void
reset_state (CamDevice * device)
{
  if (device->filename) {
    g_free (device->filename);
    device->filename = NULL;
  }

  if (device->fd) {
    close (device->fd);
    device->fd = -1;
  }

  if (device->cas) {
    cam_conditional_access_destroy (device->cas);
    device->cas = NULL;
  }

  if (device->mgr) {
    cam_resource_manager_destroy (device->mgr);
    device->mgr = NULL;
  }

  if (device->info) {
    cam_application_info_destroy (device->info);
    device->info = NULL;
  }

  if (device->al) {
    cam_al_destroy (device->al);
    device->al = NULL;
  }

  if (device->sl) {
    cam_sl_destroy (device->sl);
    device->sl = NULL;
  }

  if (device->tl) {
    cam_tl_destroy (device->tl);
    device->tl = NULL;
  }

  device->state = CAM_DEVICE_STATE_CLOSED;
}

void
cam_device_free (CamDevice * device)
{
  if (device->state != CAM_DEVICE_STATE_CLOSED)
    GST_WARNING ("device not in CLOSED state when free'd");

  reset_state (device);
  g_free (device);
}

gboolean
cam_device_open (CamDevice * device, const char *filename)
{
  ca_caps_t ca_caps;
  int ret;
  int i;
  int count = 10;

  g_return_val_if_fail (device != NULL, FALSE);
  g_return_val_if_fail (device->state == CAM_DEVICE_STATE_CLOSED, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  GST_INFO ("opening CA device %s", filename);

  ret = open (filename, O_RDWR);
  if (ret == -1) {
    GST_ERROR ("can't open CA device: %s", g_strerror (errno));
    return FALSE;
  }

  GST_DEBUG ("Successfully opened device %s", filename);

  device->fd = ret;

  ret = ioctl (device->fd, CA_RESET);

  g_usleep (G_USEC_PER_SEC / 10);

  while (TRUE) {
    /* get the capabilities of the CA */
    ret = ioctl (device->fd, CA_GET_CAP, &ca_caps);
    if (ret == -1) {
      GST_ERROR ("CA_GET_CAP ioctl failed: %s", g_strerror (errno));
      reset_state (device);
      return FALSE;
    }
    if (ca_caps.slot_num > 0)
      break;
    if (!count) {
      GST_ERROR ("CA_GET_CAP succeeded but not slots");
      reset_state (device);
      return FALSE;
    }
    count--;
    g_usleep (G_USEC_PER_SEC / 5);
  }

  device->tl = cam_tl_new (device->fd);
  device->sl = cam_sl_new (device->tl);
  device->al = cam_al_new (device->sl);

  device->mgr = cam_resource_manager_new ();
  cam_al_install (device->al, CAM_AL_APPLICATION (device->mgr));

  device->info = cam_application_info_new ();
  cam_al_install (device->al, CAM_AL_APPLICATION (device->info));

  device->cas = cam_conditional_access_new ();
  cam_al_install (device->al, CAM_AL_APPLICATION (device->cas));

  /* open a connection to each slot */
  for (i = 0; i < ca_caps.slot_num; ++i) {
    CamTLConnection *connection;

    ret = cam_tl_create_connection (device->tl, i, &connection);
    if (CAM_FAILED (ret)) {
      /* just ignore the slot, error out later only if no connection has been
       * established */
      GST_WARNING ("connection to slot %d failed, error: %d", i, ret);
      continue;
    }
  }

  if (g_hash_table_size (device->tl->connections) == 0) {
    GST_ERROR ("couldn't connect to any slot");

    reset_state (device);
    return FALSE;
  }

  device->state = CAM_DEVICE_STATE_OPEN;
  device->filename = g_strdup (filename);

  /* poll each connection to initiate the protocol */
  cam_tl_read_all (device->tl, TRUE);

  return TRUE;
}

void
cam_device_close (CamDevice * device)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (device->state == CAM_DEVICE_STATE_OPEN);

  GST_INFO ("closing CA device %s", device->filename);
  reset_state (device);
}

void
cam_device_poll (CamDevice * device)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (device->state == CAM_DEVICE_STATE_OPEN);

  cam_tl_read_all (device->tl, TRUE);
}

gboolean
cam_device_ready (CamDevice * device)
{
  g_return_val_if_fail (device != NULL, FALSE);
  g_return_val_if_fail (device->state == CAM_DEVICE_STATE_OPEN, FALSE);

  return device->cas->ready;
}

void
cam_device_set_pmt (CamDevice * device,
    GstMpegtsPMT * pmt, CamConditionalAccessPmtFlag flag)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (device->state == CAM_DEVICE_STATE_OPEN);
  g_return_if_fail (pmt != NULL);

  cam_conditional_access_set_pmt (device->cas, pmt, flag);
  cam_tl_read_all (device->tl, FALSE);
}
