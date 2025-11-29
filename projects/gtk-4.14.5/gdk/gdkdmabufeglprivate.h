#pragma once

#if defined(HAVE_DMABUF) && defined (HAVE_EGL)

#include "gdkdmabufprivate.h"
#include "gdkdmabufdownloaderprivate.h"

#include <epoxy/egl.h>

GdkDmabufDownloader *       gdk_dmabuf_get_egl_downloader       (GdkDisplay                     *display,
                                                                 GdkDmabufFormatsBuilder        *builder);
EGLImage                    gdk_dmabuf_egl_create_image         (GdkDisplay                     *display,
                                                                 int                             width,
                                                                 int                             height,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 int                             target);

#endif  /* HAVE_DMABUF && HAVE_EGL */
