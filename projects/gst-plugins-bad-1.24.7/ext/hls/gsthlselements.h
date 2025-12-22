#ifndef __GST_HLS_ELEMENT_H__
#define __GST_HLS_ELEMENT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

void hls_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (hlsdemux);
GST_ELEMENT_REGISTER_DECLARE (hlssink);
GST_ELEMENT_REGISTER_DECLARE (hlssink2);

GST_DEBUG_CATEGORY_EXTERN (hls_debug);

G_END_DECLS

#endif /* __GST_HLS_ELEMENT_H__ */
