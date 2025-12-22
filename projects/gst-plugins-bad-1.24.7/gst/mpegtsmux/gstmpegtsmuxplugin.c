/* SPDX-License-Identifier: LGPL-2.0-or-later */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstatscmux.h"
#include "gstmpegtsmux.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (mpegtsmux, plugin);
  ret |= GST_ELEMENT_REGISTER (atscmux, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    mpegtsmux, "MPEG-TS muxer",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
