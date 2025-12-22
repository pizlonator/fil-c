#ifndef __GST_CUDA_GLSTUB_H__
#define __GST_CUDA_GLSTUB_H__

#include <glib.h>

G_BEGIN_DECLS
typedef enum
{
  CU_GL_DEVICE_LIST_ALL = 0x01,
} CUGLDeviceList;

#define cuGLGetDevices cuGLGetDevices_v2

G_END_DECLS

#endif /* __GST_CUDA_GLSTUB_H__ */
