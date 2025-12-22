#ifndef __GST_WEBRTCNICE_FWD_H__
#define __GST_WEBRTCNICE_FWD_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The GstWebRTCNice library from gst-plugins-bad is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#ifndef GST_WEBRTCNICE_API
# ifdef BUILDING_GST_WEBRTCNICE
#  define GST_WEBRTCNICE_API GST_API_EXPORT         /* from config.h */
# else
#  define GST_WEBRTCNICE_API GST_API_IMPORT
# endif
#endif

#endif /* __GST_WEBRTCNICE_FWD_H__ */