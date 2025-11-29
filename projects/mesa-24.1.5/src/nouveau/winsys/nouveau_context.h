#ifndef NOUVEAU_CONTEXT
#define NOUVEAU_CONTEXT 1

#include "nouveau_private.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nouveau_ws_device;

struct nouveau_ws_object {
   uint16_t cls;
};

enum nouveau_ws_engines {
   NOUVEAU_WS_ENGINE_COPY     = (1 << 0),
   NOUVEAU_WS_ENGINE_2D       = (1 << 1),
   NOUVEAU_WS_ENGINE_3D       = (1 << 2),
   NOUVEAU_WS_ENGINE_M2MF     = (1 << 3),
   NOUVEAU_WS_ENGINE_COMPUTE  = (1 << 4),
};

struct nouveau_ws_context {
   struct nouveau_ws_device *dev;

   int channel;

   struct nouveau_ws_object copy;
   struct nouveau_ws_object eng2d;
   struct nouveau_ws_object eng3d;
   struct nouveau_ws_object m2mf;
   struct nouveau_ws_object compute;
};

int nouveau_ws_context_create(struct nouveau_ws_device *,
                              enum nouveau_ws_engines engines,
                              struct nouveau_ws_context **out);
bool nouveau_ws_context_killed(struct nouveau_ws_context *);
void nouveau_ws_context_destroy(struct nouveau_ws_context *);

#ifdef __cplusplus
}
#endif

#endif
