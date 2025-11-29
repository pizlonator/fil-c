#pragma once

#include "gdkdmabufformats.h"

typedef struct _GdkDmabufFormat GdkDmabufFormat;
struct _GdkDmabufFormat
{
  guint32 fourcc;
  guint64 modifier;
};

GdkDmabufFormats *      gdk_dmabuf_formats_new                  (GdkDmabufFormat  *formats,
                                                                 gsize             n_formats);

const GdkDmabufFormat * gdk_dmabuf_formats_peek_formats         (GdkDmabufFormats *self);
