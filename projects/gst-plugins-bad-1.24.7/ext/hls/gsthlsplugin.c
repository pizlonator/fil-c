
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gsthlselements.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (hlsdemux, plugin);
  ret |= GST_ELEMENT_REGISTER (hlssink, plugin);
  ret |= GST_ELEMENT_REGISTER (hlssink2, plugin);
  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    hls,
    "HTTP Live Streaming (HLS)",
    plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
