#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtpsink.h"
#include "gstrtpsrc.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (rtpsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (rtpsink, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rtpmanagerbad,
    "GStreamer RTP Plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
