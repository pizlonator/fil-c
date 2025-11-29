#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <xf86drm.h>

#include "drm-uapi/nouveau_drm.h"
#include "nouveau.h"
#include "nvif/class.h"
#include "nvif/cl0080.h"
#include "nvif/ioctl.h"

#include "util/bitscan.h"
#include "util/list.h"
#include "util/os_mman.h"
#include "util/simple_mtx.h"
#include "util/u_atomic.h"
#include "util/u_memory.h"

static FILE *nouveau_out = NULL;
static uint32_t nouveau_debug = 0;

#define dbg_on(lvl) (nouveau_debug & (1 << lvl))
#define dbg(lvl, fmt, args...) do {                                       \
   if (dbg_on((lvl)))                                                     \
      fprintf(nouveau_out, "nouveau: "fmt, ##args);                       \
} while(0)
#define err(fmt, args...) fprintf(nouveau_out, "nouveau: "fmt, ##args)

static void
debug_init(void)
{
   static bool once = false;
   char *debug, *out;

   if (once)
      return;
   once = true;

   debug = getenv("NOUVEAU_LIBDRM_DEBUG");
   if (debug) {
      int n = strtol(debug, NULL, 0);
      if (n >= 0)
         nouveau_debug = n;
   }

   nouveau_out = stderr;
   out = getenv("NOUVEAU_LIBDRM_OUT");
   if (out) {
      FILE *fout = fopen(out, "w");
      if (fout)
         nouveau_out = fout;
   }
}

int
nouveau_drm_new(int fd, struct nouveau_drm **pdrm)
{
   debug_init();

   struct nouveau_drm *drm = calloc(1, sizeof(*drm));
   if (!drm)
      return -ENOMEM;
   drm->fd = fd;
   *pdrm = drm;

   drmVersionPtr ver = drmGetVersion(fd);
   if (!ver)
      goto out_err;

   drm->version = (ver->version_major << 24) |
                  (ver->version_minor << 8) |
                   ver->version_patchlevel;
   if (drm->version < 0x01000301)
      goto out_err;

   drmFreeVersion(ver);
   return 0;

out_err:
   nouveau_drm_del(pdrm);
   return -EINVAL;
}

void
nouveau_drm_del(struct nouveau_drm **pdrm)
{
   free(*pdrm);
   *pdrm = NULL;
}

static int
nouveau_object_channel_new(struct nouveau_object *parent, uint64_t handle, uint32_t oclass,
                           struct nouveau_object *obj)
{
   // TODO nv04?
   struct nouveau_drm *drm = nouveau_drm(parent);
   struct nouveau_device *dev = (void*)obj->parent;
   struct nouveau_fifo *fifo = obj->data;
   struct drm_nouveau_channel_alloc req = { };

   /* nvc0 doesn't need any special handling here */
   if (dev->chipset < 0xc0) {
      struct nv04_fifo *nv04 = obj->data;
      req.fb_ctxdma_handle = nv04->vram;
      req.tt_ctxdma_handle = nv04->gart;
   } else if (dev->chipset >= 0xe0) {
      struct nve0_fifo *nve0 = obj->data;
      req.fb_ctxdma_handle = 0xffffffff;
      req.tt_ctxdma_handle = nve0->engine;
   }

   int ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_CHANNEL_ALLOC, &req, sizeof(req));
   if (ret)
      return ret;

   fifo->pushbuf = req.pushbuf_domains;
   fifo->notify = req.notifier_handle;
   obj->handle = req.channel;
   return 0;
}

static int
nouveau_object_notifier_new(struct nouveau_object *parent, uint64_t handle,
                            struct nouveau_object *obj)
{
   struct nouveau_drm *drm = nouveau_drm(obj);
   struct nv04_notify *ntfy = obj->data;
   struct drm_nouveau_notifierobj_alloc req = {
      .channel = parent->handle,
      .handle = handle,
      .size = ntfy->length,
   };

   int ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_NOTIFIEROBJ_ALLOC, &req, sizeof(req));
   if (ret)
      return ret;

   ntfy->offset = req.offset;
   return 0;
}

static int
nouveau_object_subchan_new(struct nouveau_object *parent, uint64_t handle, uint32_t oclass,
                           struct nouveau_object *obj)
{
   struct nouveau_drm *drm = nouveau_drm(parent);
   struct {
      struct nvif_ioctl_v0 ioctl;
      struct nvif_ioctl_new_v0 new;
   } args = {
      .ioctl = {
         .route = 0xff,
         .token = parent->handle,
         .type = NVIF_IOCTL_V0_NEW,
         .version = 0,
      },
      .new = {
         .handle = handle,
         .object = (uintptr_t)obj,
         .oclass = oclass,
         .route = NVIF_IOCTL_V0_ROUTE_NVIF,
         .token = (uintptr_t)obj,
         .version = 0,
      },
   };

   return drmCommandWrite(drm->fd, DRM_NOUVEAU_NVIF, &args, sizeof(args));
}

/* TODO: split this interfaces up so we can verify the right parent object type gets passed in */
int
nouveau_object_new(struct nouveau_object *parent, uint64_t handle, uint32_t oclass, void *data,
                   uint32_t length, struct nouveau_object **pobj)
{
   struct nouveau_object *obj = calloc(1, sizeof(*obj));
   if (!obj)
      return -ENOMEM;

   obj->parent = parent;
   obj->handle = handle;
   obj->oclass = oclass;
   if (length) {
      obj->data = malloc(length);
      memcpy(obj->data, data, length);
   }

   int ret;
   switch (oclass) {
   case NOUVEAU_FIFO_CHANNEL_CLASS:
      ret = nouveau_object_channel_new(parent, handle, oclass, obj);
      break;
   case NOUVEAU_NOTIFIER_CLASS:
      ret = nouveau_object_notifier_new(parent, handle, obj);
      break;
   default:
      ret = nouveau_object_subchan_new(parent, handle, oclass, obj);
      break;
   }

   if (ret) {
      free(obj->data);
      free(obj);
      return ret;
   }

   *pobj = obj;
   return 0;
}

static void
nouveau_object_channel_del(struct nouveau_object *obj)
{
   struct nouveau_drm *drm = nouveau_drm(obj->parent);
   struct drm_nouveau_channel_free req = {
      .channel = obj->handle,
   };

   int ret = drmCommandWrite(drm->fd, DRM_NOUVEAU_CHANNEL_FREE, &req, sizeof(req));
   assert(!ret);
}

static void
nouveau_object_subchan_del(struct nouveau_object *obj)
{
   struct {
      struct nvif_ioctl_v0 ioctl;
      struct nvif_ioctl_del del;
   } args = {
      .ioctl = {
         .object = (uintptr_t)obj,
         .owner = NVIF_IOCTL_V0_OWNER_ANY,
         .route = 0x00,
         .type = NVIF_IOCTL_V0_DEL,
         .version = 0,
      },
   };

   drmCommandWrite(obj->parent->handle, DRM_NOUVEAU_NVIF, &args, sizeof(args));
}

static void
nouveau_object_gpuobj_del(struct nouveau_object *obj)
{
   struct nouveau_drm *drm = nouveau_drm(obj->parent);
   struct drm_nouveau_gpuobj_free req = {
      .channel = obj->parent->handle,
      .handle = obj->handle,
   };

   drmCommandWrite(drm->fd, DRM_NOUVEAU_GPUOBJ_FREE, &req, sizeof(req));
}

void
nouveau_object_del(struct nouveau_object **pobj)
{
   if (!*pobj)
      return;

   struct nouveau_object *obj = *pobj;
   switch (obj->oclass) {
   case NOUVEAU_FIFO_CHANNEL_CLASS:
      nouveau_object_channel_del(obj);
      break;
   case NOUVEAU_NOTIFIER_CLASS:
      nouveau_object_gpuobj_del(obj);
      break;
   default:
      nouveau_object_subchan_del(obj);
      break;
   }
   free(obj->data);
   free(obj);
   *pobj = NULL;
}

#define NOUVEAU_OBJECT_MAX_CLASSES 16
int
nouveau_object_mclass(struct nouveau_object *obj, const struct nouveau_mclass *mclass)
{
   struct nouveau_drm *drm = nouveau_drm(obj->parent);
   struct {
      struct nvif_ioctl_v0 ioctl;
      struct nvif_ioctl_sclass_v0 sclass;
      struct nvif_ioctl_sclass_oclass_v0 list[NOUVEAU_OBJECT_MAX_CLASSES];
   } args = {
      .ioctl = {
         .route = 0xff,
         .token = obj->handle,
         .type = NVIF_IOCTL_V0_SCLASS,
         .version = 0,
      },
      .sclass = {
         .count = NOUVEAU_OBJECT_MAX_CLASSES,
         .version = 0,
      },
   };

   int ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_NVIF, &args, sizeof(args));
   if (ret)
      return ret;

   int i;
   for (i = 0; mclass[i].oclass; i++) {
      for (int j = 0; j < args.sclass.count; j++) {
         if (args.list[j].oclass == mclass[i].oclass)
            return i;
      }
   }

   return -ENODEV;
}

struct nouveau_device_priv {
   struct nouveau_device base;
   simple_mtx_t lock;
   struct list_head bo_list;
   int gart_limit_percent;
   int vram_limit_percent;
};

static inline struct nouveau_device_priv *
nouveau_device(struct nouveau_device *dev)
{
   return (struct nouveau_device_priv *)dev;
}

int
nouveau_device_info(struct nouveau_device *dev, struct nv_device_info_v0 *info)
{
   struct nouveau_drm *drm = nouveau_drm(dev->object.parent);
   struct {
      struct nvif_ioctl_v0 ioctl;
      struct nvif_ioctl_mthd_v0 mthd;
      struct nv_device_info_v0 info;
   } args = {
      .ioctl = {
         .object = (uintptr_t)dev,
         .owner = NVIF_IOCTL_V0_OWNER_ANY,
         .route = 0x00,
         .type = NVIF_IOCTL_V0_MTHD,
         .version = 0,
      },
      .mthd = {
         .method = NV_DEVICE_V0_INFO,
         .version = 0,
      },
      .info = {
         .version = 0,
      },
   };

   int ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_NVIF, &args, sizeof(args));
   if (ret)
      return ret;

   *info = args.info;
   return 0;
}

int
nouveau_device_new(struct nouveau_object *parent, struct nouveau_device **pdev)
{
   struct nouveau_drm *drm = nouveau_drm(parent);
   struct nouveau_device *dev;
   uint64_t v;
   char *tmp;

   struct nouveau_device_priv *nvdev = calloc(1, sizeof(*nvdev));
   if (!nvdev)
      return -ENOMEM;
   dev = *pdev = &nvdev->base;
   dev->object.parent = parent;

   struct {
      struct nvif_ioctl_v0 ioctl;
      struct nvif_ioctl_new_v0 new;
      struct nv_device_v0 dev;
   } args = {
      .ioctl = {
         .object = 0,
         .owner = NVIF_IOCTL_V0_OWNER_ANY,
         .route = 0x00,
         .type = NVIF_IOCTL_V0_NEW,
         .version = 0,
      },
      .new = {
         .handle = 0,
         .object = (uintptr_t)&nvdev->base.object,
         .oclass = NV_DEVICE,
         .route = NVIF_IOCTL_V0_ROUTE_NVIF,
         .token = (uintptr_t)&nvdev->base.object,
         .version = 0,
      },
      .dev = {
         .device = ~0ULL,
      },
   };

   int ret = drmCommandWrite(drm->fd, DRM_NOUVEAU_NVIF, &args, sizeof(args));
   if (ret)
      goto done;

   struct nv_device_info_v0 info;
   ret = nouveau_device_info(dev, &info);
   if (ret)
      goto done;

   nvdev->base.chipset = info.chipset;

   ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_FB_SIZE, &v);
   if (ret)
      goto done;
   nvdev->base.vram_size = v;

   ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_AGP_SIZE, &v);
   if (ret)
      goto done;
   nvdev->base.gart_size = v;

   tmp = getenv("NOUVEAU_LIBDRM_VRAM_LIMIT_PERCENT");
   if (tmp)
      nvdev->vram_limit_percent = atoi(tmp);
   else
      nvdev->vram_limit_percent = 80;
   nvdev->base.vram_limit = (nvdev->base.vram_size * nvdev->vram_limit_percent) / 100;

   tmp = getenv("NOUVEAU_LIBDRM_GART_LIMIT_PERCENT");
   if (tmp)
      nvdev->gart_limit_percent = atoi(tmp);
   else
      nvdev->gart_limit_percent = 80;
   nvdev->base.gart_limit = (nvdev->base.gart_size * nvdev->gart_limit_percent) / 100;

   simple_mtx_init(&nvdev->lock, mtx_plain);
   list_inithead(&nvdev->bo_list);
done:
   if (ret)
      nouveau_device_del(pdev);
   return ret;
}

void
nouveau_device_del(struct nouveau_device **pdev)
{
   struct nouveau_device_priv *nvdev = nouveau_device(*pdev);
   if (!nvdev)
      return;

   simple_mtx_destroy(&nvdev->lock);
   free(nvdev);
   *pdev = NULL;
}

int
nouveau_getparam(struct nouveau_device *dev, uint64_t param, uint64_t *value)
{
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct drm_nouveau_getparam r = { .param = param };
   int ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GETPARAM, &r, sizeof(r));
   *value = r.value;
   return ret;
}

struct nouveau_client_kref {
   struct drm_nouveau_gem_pushbuf_bo *kref;
   struct nouveau_pushbuf *push;
};

struct nouveau_client_priv {
   struct nouveau_client base;
   struct nouveau_client_kref *kref;
   unsigned kref_nr;
};

static inline struct nouveau_client_priv *
nouveau_client(struct nouveau_client *client)
{
   return (struct nouveau_client_priv *)client;
}

int
nouveau_client_new(struct nouveau_device *dev, struct nouveau_client **pclient)
{
   struct nouveau_client_priv *pcli = calloc(1, sizeof(*pcli));
   if (!pcli)
      return -ENOMEM;

   pcli->base.device = dev;
   *pclient = &pcli->base;

   return 0;
}

void
nouveau_client_del(struct nouveau_client **pclient)
{
   struct nouveau_client_priv *pcli = nouveau_client(*pclient);
   if (pcli) {
      free(pcli->kref);
      free(pcli);
   }
}

struct nouveau_bo_priv {
   struct nouveau_bo base;
   struct list_head head;
   uint32_t refcnt;
   uint64_t map_handle;
   uint32_t name;
   uint32_t access;
};

static inline struct nouveau_bo_priv *
nouveau_bo(struct nouveau_bo *bo)
{
   return (struct nouveau_bo_priv *)bo;
}

static void
bo_info(struct nouveau_bo *bo, struct drm_nouveau_gem_info *info)
{
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

   nvbo->map_handle = info->map_handle;
   bo->handle = info->handle;
   bo->size = info->size;
   bo->offset = info->offset;

   bo->flags = 0;
   if (info->domain & NOUVEAU_GEM_DOMAIN_VRAM)
      bo->flags |= NOUVEAU_BO_VRAM;
   if (info->domain & NOUVEAU_GEM_DOMAIN_GART)
      bo->flags |= NOUVEAU_BO_GART;
   if (!(info->tile_flags & NOUVEAU_GEM_TILE_NONCONTIG))
      bo->flags |= NOUVEAU_BO_CONTIG;
   if (nvbo->map_handle)
      bo->flags |= NOUVEAU_BO_MAP;

   if (bo->device->chipset >= 0xc0) {
      bo->config.nvc0.memtype   = (info->tile_flags & 0xff00) >> 8;
      bo->config.nvc0.tile_mode = info->tile_mode;
   } else
   if (bo->device->chipset >= 0x80 || bo->device->chipset == 0x50) {
      bo->config.nv50.memtype   = (info->tile_flags & 0x07f00) >> 8 |
                   (info->tile_flags & 0x30000) >> 9;
      bo->config.nv50.tile_mode = info->tile_mode << 4;
   } else {
      //bo->config.nv04.surf_flags = info->tile_flags & 7;
      //bo->config.nv04.surf_pitch = info->tile_mode;
   }
}

static void
nouveau_bo_make_global(struct nouveau_bo_priv *nvbo)
{
   if (!nvbo->head.next) {
      struct nouveau_device_priv *nvdev = nouveau_device(nvbo->base.device);
      simple_mtx_lock(&nvdev->lock);
      if (!nvbo->head.next)
         list_add(&nvbo->head, &nvdev->bo_list);
      simple_mtx_unlock(&nvdev->lock);
   }
}

static int
nouveau_bo_wrap_locked(struct nouveau_device *dev, uint32_t handle,
             struct nouveau_bo **pbo, int name)
{
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct nouveau_device_priv *nvdev = nouveau_device(dev);
   struct drm_nouveau_gem_info req = { .handle = handle };
   struct nouveau_bo_priv *nvbo;
   int ret;

   list_for_each_entry(struct nouveau_bo_priv, nvbo, &nvdev->bo_list, head) {
      if (nvbo->base.handle == handle) {
         if (p_atomic_inc_return(&nvbo->refcnt) == 1) {
            /*
             * Uh oh, this bo is dead and someone else
             * will free it, but because refcnt is
             * now non-zero fortunately they won't
             * call the ioctl to close the bo.
             *
             * Remove this bo from the list so other
             * calls to nouveau_bo_wrap_locked will
             * see our replacement nvbo.
             */
            list_del(&nvbo->head);
            if (!name)
               name = nvbo->name;
            break;
         }

         *pbo = &nvbo->base;
         return 0;
      }
   }

   ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GEM_INFO, &req, sizeof(req));
   if (ret)
      return ret;

   nvbo = calloc(1, sizeof(*nvbo));
   if (nvbo) {
      p_atomic_set(&nvbo->refcnt, 1);
      nvbo->base.device = dev;
      bo_info(&nvbo->base, &req);
      nvbo->name = name;
      list_add(&nvbo->head, &nvdev->bo_list);
      *pbo = &nvbo->base;
      return 0;
   }

   return -ENOMEM;
}

int
nouveau_bo_new(struct nouveau_device *dev, uint32_t flags, uint32_t align, uint64_t size,
               union nouveau_bo_config *config, struct nouveau_bo **pbo)
{
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct drm_nouveau_gem_new req = {};
   struct drm_nouveau_gem_info *info = &req.info;
   int ret;

   struct nouveau_bo_priv *nvbo = calloc(1, sizeof(*nvbo));
   if (!nvbo)
      return -ENOMEM;

   struct nouveau_bo *bo = &nvbo->base;
   p_atomic_set(&nvbo->refcnt, 1);
   bo->device = dev;
   bo->flags = flags;
   bo->size = size;

   if (bo->flags & NOUVEAU_BO_VRAM)
      info->domain |= NOUVEAU_GEM_DOMAIN_VRAM;
   if (bo->flags & NOUVEAU_BO_GART)
      info->domain |= NOUVEAU_GEM_DOMAIN_GART;
   if (!info->domain)
      info->domain |= NOUVEAU_GEM_DOMAIN_VRAM | NOUVEAU_GEM_DOMAIN_GART;

   if (bo->flags & NOUVEAU_BO_MAP)
      info->domain |= NOUVEAU_GEM_DOMAIN_MAPPABLE;

   if (bo->flags & NOUVEAU_BO_COHERENT)
      info->domain |= NOUVEAU_GEM_DOMAIN_COHERENT;

   if (!(bo->flags & NOUVEAU_BO_CONTIG))
      info->tile_flags = NOUVEAU_GEM_TILE_NONCONTIG;

   info->size = bo->size;
   req.align = align;

   if (config) {
      if (dev->chipset >= 0xc0) {
         info->tile_flags = (config->nvc0.memtype & 0xff) << 8;
         info->tile_mode  =  config->nvc0.tile_mode;
      } else
      if (dev->chipset >= 0x80 || dev->chipset == 0x50) {
         info->tile_flags = (config->nv50.memtype & 0x07f) << 8 |
                            (config->nv50.memtype & 0x180) << 9;
         info->tile_mode  = config->nv50.tile_mode >> 4;
      }
   }

   ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GEM_NEW, &req, sizeof(req));
   if (ret) {
      free(nvbo);
      return ret;
   }

   bo_info(bo, info);

   *pbo = bo;
   return 0;
}

static void
nouveau_bo_del(struct nouveau_bo *bo)
{
   struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
   struct nouveau_device_priv *nvdev = nouveau_device(bo->device);
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

   if (nvbo->head.next) {
      simple_mtx_lock(&nvdev->lock);
      if (p_atomic_read(&nvbo->refcnt) == 0) {
         list_del(&nvbo->head);
         drmCloseBufferHandle(drm->fd, bo->handle);
      }
      simple_mtx_unlock(&nvdev->lock);
   } else {
      drmCloseBufferHandle(drm->fd, bo->handle);
   }
   if (bo->map)
      os_munmap(bo->map, bo->size);
   free(nvbo);
}

int
nouveau_bo_map(struct nouveau_bo *bo, uint32_t access, struct nouveau_client *client)
{
   struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
   if (bo->map == NULL) {
      bo->map = os_mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm->fd, nvbo->map_handle);
      if (bo->map == MAP_FAILED) {
         bo->map = NULL;
         return -errno;
      }
   }
   return nouveau_bo_wait(bo, access, client);
}

int
nouveau_bo_name_get(struct nouveau_bo *bo, uint32_t *name)
{
   struct drm_gem_flink req = { .handle = bo->handle };
   struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

   *name = nvbo->name;
   if (!*name) {
      int ret = drmIoctl(drm->fd, DRM_IOCTL_GEM_FLINK, &req);

      if (ret) {
         *name = 0;
         return ret;
      }
      nvbo->name = *name = req.name;

      nouveau_bo_make_global(nvbo);
   }
   return 0;
}

int
nouveau_bo_name_ref(struct nouveau_device *dev, uint32_t name, struct nouveau_bo **pbo)
{
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct nouveau_device_priv *nvdev = nouveau_device(dev);
   struct drm_gem_open req = { .name = name };
   int ret;

   simple_mtx_lock(&nvdev->lock);
   list_for_each_entry(struct nouveau_bo_priv, nvbo, &nvdev->bo_list, head) {
      if (nvbo->name == name) {
         ret = nouveau_bo_wrap_locked(dev, nvbo->base.handle, pbo, name);
         simple_mtx_unlock(&nvdev->lock);
         return ret;
      }
   }

   ret = drmIoctl(drm->fd, DRM_IOCTL_GEM_OPEN, &req);
   if (ret == 0) {
      ret = nouveau_bo_wrap_locked(dev, req.handle, pbo, name);
   }

   simple_mtx_unlock(&nvdev->lock);
   return ret;
}

int
nouveau_bo_prime_handle_ref(struct nouveau_device *dev, int prime_fd, struct nouveau_bo **bo)
{
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct nouveau_device_priv *nvdev = nouveau_device(dev);
   int ret;
   unsigned int handle;

   nouveau_bo_ref(NULL, bo);

   simple_mtx_lock(&nvdev->lock);
   ret = drmPrimeFDToHandle(drm->fd, prime_fd, &handle);
   if (ret == 0) {
      ret = nouveau_bo_wrap_locked(dev, handle, bo, 0);
   }
   simple_mtx_unlock(&nvdev->lock);
   return ret;
}

void
nouveau_bo_ref(struct nouveau_bo *bo, struct nouveau_bo **pref)
{
   struct nouveau_bo *ref = *pref;
   if (bo) {
      p_atomic_inc(&nouveau_bo(bo)->refcnt);
   }
   if (ref) {
      if (p_atomic_dec_zero(&nouveau_bo(ref)->refcnt))
         nouveau_bo_del(ref);
   }
   *pref = bo;
}

int
nouveau_bo_set_prime(struct nouveau_bo *bo, int *prime_fd)
{
   struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
   int ret;

   ret = drmPrimeHandleToFD(drm->fd, nvbo->base.handle, O_CLOEXEC, prime_fd);
   if (ret)
      return ret;

   nouveau_bo_make_global(nvbo);
   return 0;
}

static inline struct nouveau_pushbuf *
cli_push_get(struct nouveau_client *client, struct nouveau_bo *bo)
{
   struct nouveau_client_priv *pcli = nouveau_client(client);
   struct nouveau_pushbuf *push = NULL;
   if (pcli->kref_nr > bo->handle)
      push = pcli->kref[bo->handle].push;
   return push;
}

static inline struct drm_nouveau_gem_pushbuf_bo *
cli_kref_get(struct nouveau_client *client, struct nouveau_bo *bo)
{
   struct nouveau_client_priv *pcli = nouveau_client(client);
   struct drm_nouveau_gem_pushbuf_bo *kref = NULL;
   if (pcli->kref_nr > bo->handle)
      kref = pcli->kref[bo->handle].kref;
   return kref;
}

static inline void
cli_kref_set(struct nouveau_client *client, struct nouveau_bo *bo,
             struct drm_nouveau_gem_pushbuf_bo *kref, struct nouveau_pushbuf *push)
{
   struct nouveau_client_priv *pcli = nouveau_client(client);
   if (pcli->kref_nr <= bo->handle) {
      pcli->kref = realloc(pcli->kref, sizeof(*pcli->kref) * bo->handle * 2);
      while (pcli->kref_nr < bo->handle * 2) {
         pcli->kref[pcli->kref_nr].kref = NULL;
         pcli->kref[pcli->kref_nr].push = NULL;
         pcli->kref_nr++;
      }
   }
   pcli->kref[bo->handle].kref = kref;
   pcli->kref[bo->handle].push = push;
}

int
nouveau_bo_wait(struct nouveau_bo *bo, uint32_t access, struct nouveau_client *client)
{
   struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
   struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
   struct drm_nouveau_gem_cpu_prep req;
   struct nouveau_pushbuf *push;
   int ret = 0;

   if (!(access & NOUVEAU_BO_RDWR))
      return 0;

   push = cli_push_get(client, bo);
   if (push)
      nouveau_pushbuf_kick(push);

   if (!nvbo->head.next && !(nvbo->access & NOUVEAU_BO_WR) && !(access & NOUVEAU_BO_WR))
      return 0;

   req.handle = bo->handle;
   req.flags = 0;
   if (access & NOUVEAU_BO_WR)
      req.flags |= NOUVEAU_GEM_CPU_PREP_WRITE;
   if (access & NOUVEAU_BO_NOBLOCK)
      req.flags |= NOUVEAU_GEM_CPU_PREP_NOWAIT;

   ret = drmCommandWrite(drm->fd, DRM_NOUVEAU_GEM_CPU_PREP, &req, sizeof(req));
   if (ret == 0)
      nvbo->access = 0;
   return ret;
}

int
nouveau_bo_wrap(struct nouveau_device *dev, uint32_t handle, struct nouveau_bo **pbo)
{
   struct nouveau_device_priv *nvdev = nouveau_device(dev);
   int ret;
   simple_mtx_lock(&nvdev->lock);
   ret = nouveau_bo_wrap_locked(dev, handle, pbo, 0);
   simple_mtx_unlock(&nvdev->lock);
   return ret;
}

struct nouveau_bufref_priv {
   struct nouveau_bufref base;
   struct nouveau_bufref_priv *next;
   struct nouveau_bufctx *bufctx;
};

struct nouveau_bufbin_priv {
   struct nouveau_bufref_priv *list;
   int relocs;
};

struct nouveau_bufctx_priv {
   struct nouveau_bufctx base;
   struct nouveau_bufref_priv *free;
   int nr_bins;
   struct nouveau_bufbin_priv bins[];
};

static inline struct nouveau_bufctx_priv *
nouveau_bufctx(struct nouveau_bufctx *bctx)
{
   return (struct nouveau_bufctx_priv *)bctx;
}

int
nouveau_bufctx_new(struct nouveau_client *client, int bins, struct nouveau_bufctx **pbctx)
{
   struct nouveau_bufctx_priv *priv;

   priv = CALLOC_VARIANT_LENGTH_STRUCT(nouveau_bufctx_priv, sizeof(priv->bins[0]) * bins);
   if (priv) {
      list_inithead(&priv->base.head);
      list_inithead(&priv->base.pending);
      list_inithead(&priv->base.current);
      priv->base.client = client;
      priv->nr_bins = bins;
      *pbctx = &priv->base;
      return 0;
   }

   return -ENOMEM;
}

void
nouveau_bufctx_del(struct nouveau_bufctx **pbctx)
{
   struct nouveau_bufctx_priv *pctx = nouveau_bufctx(*pbctx);
   struct nouveau_bufref_priv *pref;
   if (pctx) {
      while (pctx->nr_bins--)
         nouveau_bufctx_reset(&pctx->base, pctx->nr_bins);
      while ((pref = pctx->free)) {
         pctx->free = pref->next;
         free(pref);
      }
      free(pctx);
      *pbctx = NULL;
   }
}

struct nouveau_bufref *
nouveau_bufctx_mthd(struct nouveau_bufctx *bctx, int bin,  uint32_t packet, struct nouveau_bo *bo,
                    uint64_t data, uint32_t flags, uint32_t vor, uint32_t tor)
{
   struct nouveau_bufctx_priv *pctx = nouveau_bufctx(bctx);
   struct nouveau_bufbin_priv *pbin = &pctx->bins[bin];
   struct nouveau_bufref *bref = nouveau_bufctx_refn(bctx, bin, bo, flags);
   if (bref) {
      bref->packet = packet;
      bref->data = data;
      bref->vor = vor;
      bref->tor = tor;
      pbin->relocs++;
      bctx->relocs++;
   }
   return bref;
}

struct nouveau_bufref *
nouveau_bufctx_refn(struct nouveau_bufctx *bctx, int bin,
          struct nouveau_bo *bo, uint32_t flags)
{
   struct nouveau_bufctx_priv *pctx = nouveau_bufctx(bctx);
   struct nouveau_bufbin_priv *pbin = &pctx->bins[bin];
   struct nouveau_bufref_priv *pref = pctx->free;

   if (!pref)
      pref = malloc(sizeof(*pref));
   else
      pctx->free = pref->next;

   if (!pref)
      return NULL;

   pref->base.bo = bo;
   pref->base.flags = flags;
   pref->base.packet = 0;

   list_addtail(&pref->base.thead, &bctx->pending);
   pref->bufctx = bctx;
   pref->next = pbin->list;
   pbin->list = pref;

   return &pref->base;
}

struct nouveau_pushbuf_krec {
   struct nouveau_pushbuf_krec *next;
   struct drm_nouveau_gem_pushbuf_bo buffer[NOUVEAU_GEM_MAX_BUFFERS];
   struct drm_nouveau_gem_pushbuf_reloc reloc[NOUVEAU_GEM_MAX_RELOCS];
   struct drm_nouveau_gem_pushbuf_push push[NOUVEAU_GEM_MAX_PUSH];
   int nr_buffer;
   int nr_reloc;
   int nr_push;
   uint64_t vram_used;
   uint64_t gart_used;
};

struct nouveau_pushbuf_priv {
   struct nouveau_pushbuf base;
   struct nouveau_pushbuf_krec *list;
   struct nouveau_pushbuf_krec *krec;
   struct list_head bctx_list;
   struct nouveau_bo *bo;
   uint32_t type;
   uint32_t suffix0;
   uint32_t suffix1;
   uint32_t *ptr;
   uint32_t *bgn;
   int bo_next;
   int bo_nr;
   struct nouveau_bo *bos[];
};

static inline struct nouveau_pushbuf_priv *
nouveau_pushbuf(struct nouveau_pushbuf *push)
{
   return (struct nouveau_pushbuf_priv *)push;
}

static void
pushbuf_dump(struct nouveau_pushbuf_krec *krec, int krec_id, int chid)
{
   struct drm_nouveau_gem_pushbuf_reloc *krel;
   struct drm_nouveau_gem_pushbuf_push *kpsh;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   struct nouveau_bo *bo;
   uint32_t *bgn, *end;
   int i;

   err("ch%d: krec %d pushes %d bufs %d relocs %d\n",
       chid, krec_id, krec->nr_push, krec->nr_buffer, krec->nr_reloc);

   kref = krec->buffer;
   for (i = 0; i < krec->nr_buffer; i++, kref++) {
      bo = (void *)(uintptr_t)kref->user_priv;
      err("ch%d: buf %08x %08x %08x %08x %08x %p 0x%"PRIx64" 0x%"PRIx64"\n",
          chid, i, kref->handle, kref->valid_domains,
          kref->read_domains, kref->write_domains, bo->map, bo->offset, bo->size);
   }

   krel = krec->reloc;
   for (i = 0; i < krec->nr_reloc; i++, krel++) {
      err("ch%d: rel %08x %08x %08x %08x %08x %08x %08x\n",
          chid, krel->reloc_bo_index, krel->reloc_bo_offset,
          krel->bo_index, krel->flags, krel->data,
          krel->vor, krel->tor);
   }

   kpsh = krec->push;
   for (i = 0; i < krec->nr_push; i++, kpsh++) {
      kref = krec->buffer + kpsh->bo_index;
      bo = (void *)(unsigned long)kref->user_priv;
      bgn = (uint32_t *)((char *)bo->map + kpsh->offset);
      end = bgn + ((kpsh->length & 0x7fffff) /4);

      err("ch%d: psh %s%08x %010llx %010llx\n", chid,
          bo->map ? "" : "(unmapped) ", kpsh->bo_index,
          (unsigned long long)kpsh->offset,
          (unsigned long long)(kpsh->offset + kpsh->length));
      if (!bo->map)
         continue;
      while (bgn < end)
         err("\t0x%08x\n", *bgn++);
   }
}

static int
pushbuf_submit(struct nouveau_pushbuf *push, struct nouveau_object *chan)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->list;
   struct nouveau_device *dev = push->client->device;
   struct nouveau_drm *drm = nouveau_drm(&dev->object);
   struct drm_nouveau_gem_pushbuf_bo_presumed *info;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   struct drm_nouveau_gem_pushbuf req;
   int channel = chan->handle;
   struct nouveau_bo *bo;
   int krec_id = 0;
   int ret = 0, i;

   if (chan->oclass != NOUVEAU_FIFO_CHANNEL_CLASS)
      return -EINVAL;

   if (push->kick_notify)
      push->kick_notify(push);

   nouveau_pushbuf_data(push, NULL, 0, 0);

   while (krec && krec->nr_push) {
      req.channel = channel;
      req.nr_buffers = krec->nr_buffer;
      req.buffers = (uint64_t)(unsigned long)krec->buffer;
      req.nr_relocs = krec->nr_reloc;
      req.nr_push = krec->nr_push;
      req.relocs = (uint64_t)(unsigned long)krec->reloc;
      req.push = (uint64_t)(unsigned long)krec->push;
      req.suffix0 = nvpb->suffix0;
      req.suffix1 = nvpb->suffix1;
      req.vram_available = 0;
      if (dbg_on(1))
         req.vram_available |= NOUVEAU_GEM_PUSHBUF_SYNC;
      req.gart_available = 0;

      if (dbg_on(0))
         pushbuf_dump(krec, krec_id++, channel);

      ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GEM_PUSHBUF, &req, sizeof(req));
      nvpb->suffix0 = req.suffix0;
      nvpb->suffix1 = req.suffix1;
      dev->vram_limit = (req.vram_available * nouveau_device(dev)->vram_limit_percent) / 100;
      dev->gart_limit = (req.gart_available * nouveau_device(dev)->gart_limit_percent) / 100;

      if (ret) {
         err("kernel rejected pushbuf: %s\n", strerror(-ret));
         pushbuf_dump(krec, krec_id++, channel);
         break;
      }

      kref = krec->buffer;
      for (i = 0; i < krec->nr_buffer; i++, kref++) {
         bo = (void *)(unsigned long)kref->user_priv;

         info = &kref->presumed;
         if (!info->valid) {
            bo->flags &= ~NOUVEAU_BO_APER;
            if (info->domain == NOUVEAU_GEM_DOMAIN_VRAM)
               bo->flags |= NOUVEAU_BO_VRAM;
            else
               bo->flags |= NOUVEAU_BO_GART;
            bo->offset = info->offset;
         }

         if (kref->write_domains)
            nouveau_bo(bo)->access |= NOUVEAU_BO_WR;
         if (kref->read_domains)
            nouveau_bo(bo)->access |= NOUVEAU_BO_RD;
      }

      krec = krec->next;
   }

   return ret;
}

static int
pushbuf_flush(struct nouveau_pushbuf *push)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   int i;

   int ret = pushbuf_submit(push, push->channel);

   kref = krec->buffer;
   for (i = 0; i < krec->nr_buffer; i++, kref++) {
      struct nouveau_bo *bo = (void *)(unsigned long)kref->user_priv;
      cli_kref_set(push->client, bo, NULL, NULL);
      nouveau_bo_ref(NULL, &bo);
   }

   krec = nvpb->krec;
   krec->vram_used = 0;
   krec->gart_used = 0;
   krec->nr_buffer = 0;
   krec->nr_reloc = 0;
   krec->nr_push = 0;

   list_for_each_entry_safe(struct nouveau_bufctx, bctx, &nvpb->bctx_list, head) {
      list_splice(&bctx->current, &bctx->pending);
      list_inithead(&bctx->current);
      list_delinit(&bctx->head);
   }

   return ret;
}

static bool
pushbuf_kref_fits(struct nouveau_pushbuf *push, struct nouveau_bo *bo, uint32_t *domains)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct nouveau_device *dev = push->client->device;
   struct nouveau_bo *kbo;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   int i;

   /* VRAM is the only valid domain.  GART and VRAM|GART buffers
    * are all accounted to GART, so if this doesn't fit in VRAM
    * straight up, a flush is needed.
    */
   if (*domains == NOUVEAU_GEM_DOMAIN_VRAM) {
      if (krec->vram_used + bo->size > dev->vram_limit)
         return false;
      krec->vram_used += bo->size;
      return true;
   }

   /* GART or VRAM|GART buffer.  Account both of these buffer types
    * to GART only for the moment, which simplifies things.  If the
    * buffer can fit already, we're done here.
    */
   if (krec->gart_used + bo->size <= dev->gart_limit) {
      krec->gart_used += bo->size;
      return true;
   }

   /* Ran out of GART space, if it's a VRAM|GART buffer and it'll
    * fit into available VRAM, turn it into a VRAM buffer
    */
   if ((*domains & NOUVEAU_GEM_DOMAIN_VRAM) &&
       krec->vram_used + bo->size <= dev->vram_limit) {
      *domains &= NOUVEAU_GEM_DOMAIN_VRAM;
      krec->vram_used += bo->size;
      return true;
   }

   /* Still couldn't fit the buffer in anywhere, so as a last resort;
    * scan the buffer list for VRAM|GART buffers and turn them into
    * VRAM buffers until we have enough space in GART for this one
    */
   kref = krec->buffer;
   for (i = 0; i < krec->nr_buffer; i++, kref++) {
      if (!(kref->valid_domains & NOUVEAU_GEM_DOMAIN_GART))
         continue;

      kbo = (void *)(unsigned long)kref->user_priv;
      if (!(kref->valid_domains & NOUVEAU_GEM_DOMAIN_VRAM) ||
          krec->vram_used + kbo->size > dev->vram_limit)
         continue;

      kref->valid_domains &= NOUVEAU_GEM_DOMAIN_VRAM;
      krec->gart_used -= kbo->size;
      krec->vram_used += kbo->size;
      if (krec->gart_used + bo->size <= dev->gart_limit) {
         krec->gart_used += bo->size;
         return true;
      }
   }

   /* Couldn't resolve a placement, need to force a flush */
   return false;
}

static struct drm_nouveau_gem_pushbuf_bo *
pushbuf_kref(struct nouveau_pushbuf *push, struct nouveau_bo *bo, uint32_t flags)
{
   struct nouveau_device *dev = push->client->device;
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct nouveau_pushbuf *fpush;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   uint32_t domains, domains_wr, domains_rd;

   domains = 0;
   if (flags & NOUVEAU_BO_VRAM)
      domains |= NOUVEAU_GEM_DOMAIN_VRAM;
   if (flags & NOUVEAU_BO_GART)
      domains |= NOUVEAU_GEM_DOMAIN_GART;
   domains_wr = domains * !!(flags & NOUVEAU_BO_WR);
   domains_rd = domains * !!(flags & NOUVEAU_BO_RD);

   /* if buffer is referenced on another pushbuf that is owned by the
    * same client, we need to flush the other pushbuf first to ensure
    * the correct ordering of commands
    */
   fpush = cli_push_get(push->client, bo);
   if (fpush && fpush != push)
      pushbuf_flush(fpush);

   kref = cli_kref_get(push->client, bo);
   if (kref) {
      /* possible conflict in memory types - flush and retry */
      if (!(kref->valid_domains & domains))
         return NULL;

      /* VRAM|GART buffer turning into a VRAM buffer.  Make sure
       * it'll fit in VRAM and force a flush if not.
       */
      if ((kref->valid_domains  & NOUVEAU_GEM_DOMAIN_GART) &&
          (            domains == NOUVEAU_GEM_DOMAIN_VRAM)) {
         if (krec->vram_used + bo->size > dev->vram_limit)
            return NULL;
         krec->vram_used += bo->size;
         krec->gart_used -= bo->size;
      }

      kref->valid_domains &= domains;
      kref->write_domains |= domains_wr;
      kref->read_domains  |= domains_rd;
   } else {
      if (krec->nr_buffer == NOUVEAU_GEM_MAX_BUFFERS ||
          !pushbuf_kref_fits(push, bo, &domains))
         return NULL;

      kref = &krec->buffer[krec->nr_buffer++];
      kref->user_priv = (unsigned long)bo;
      kref->handle = bo->handle;
      kref->valid_domains = domains;
      kref->write_domains = domains_wr;
      kref->read_domains = domains_rd;
      kref->presumed.valid = 1;
      kref->presumed.offset = bo->offset;
      if (bo->flags & NOUVEAU_BO_VRAM)
         kref->presumed.domain = NOUVEAU_GEM_DOMAIN_VRAM;
      else
         kref->presumed.domain = NOUVEAU_GEM_DOMAIN_GART;

      cli_kref_set(push->client, bo, kref, push);
      p_atomic_inc(&nouveau_bo(bo)->refcnt);
   }

   return kref;
}

static uint32_t
pushbuf_krel(struct nouveau_pushbuf *push, struct nouveau_bo *bo,
        uint32_t data, uint32_t flags, uint32_t vor, uint32_t tor)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_reloc *krel;
   struct drm_nouveau_gem_pushbuf_bo *pkref;
   struct drm_nouveau_gem_pushbuf_bo *bkref;
   uint32_t reloc = data;

   pkref = cli_kref_get(push->client, nvpb->bo);
   bkref = cli_kref_get(push->client, bo);
   krel  = &krec->reloc[krec->nr_reloc++];

   assert(pkref);
   assert(bkref);
   krel->reloc_bo_index = pkref - krec->buffer;
   krel->reloc_bo_offset = (push->cur - nvpb->ptr) * 4;
   krel->bo_index = bkref - krec->buffer;
   krel->flags = 0;
   krel->data = data;
   krel->vor = vor;
   krel->tor = tor;

   if (flags & NOUVEAU_BO_LOW) {
      reloc = (bkref->presumed.offset + data);
      krel->flags |= NOUVEAU_GEM_RELOC_LOW;
   }
   if (flags & NOUVEAU_BO_OR) {
      if (bkref->presumed.domain & NOUVEAU_GEM_DOMAIN_VRAM)
         reloc |= vor;
      else
         reloc |= tor;
      krel->flags |= NOUVEAU_GEM_RELOC_OR;
   }

   return reloc;
}

static void
pushbuf_refn_fail(struct nouveau_pushbuf *push, int sref, int srel)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_bo *kref;

   kref = krec->buffer + sref;
   while (krec->nr_buffer-- > sref) {
      struct nouveau_bo *bo = (void *)(unsigned long)kref->user_priv;
      cli_kref_set(push->client, bo, NULL, NULL);
      nouveau_bo_ref(NULL, &bo);
      kref++;
   }
   krec->nr_buffer = sref;
   krec->nr_reloc = srel;
}

static int
pushbuf_refn(struct nouveau_pushbuf *push, bool retry,
        struct nouveau_pushbuf_refn *refs, int nr)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   int sref = krec->nr_buffer;
   int ret = 0, i;

   for (i = 0; i < nr; i++) {
      kref = pushbuf_kref(push, refs[i].bo, refs[i].flags);
      if (!kref) {
         ret = -ENOSPC;
         break;
      }
   }

   if (ret) {
      pushbuf_refn_fail(push, sref, krec->nr_reloc);
      if (retry) {
         pushbuf_flush(push);
         nouveau_pushbuf_space(push, 0, 0, 0);
         return pushbuf_refn(push, false, refs, nr);
      }
   }

   return ret;
}

static int
pushbuf_validate(struct nouveau_pushbuf *push, bool retry)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_bo *kref;
   struct nouveau_bufctx *bctx = push->bufctx;
   int relocs = bctx ? bctx->relocs * 2: 0;
   int sref, srel, ret;

   ret = nouveau_pushbuf_space(push, relocs, relocs, 0);
   if (ret || bctx == NULL)
      return ret;

   sref = krec->nr_buffer;
   srel = krec->nr_reloc;

   list_del(&bctx->head);
   list_add(&bctx->head, &nvpb->bctx_list);

   list_for_each_entry(struct nouveau_bufref, bref, &bctx->pending, thead) {
      kref = pushbuf_kref(push, bref->bo, bref->flags);
      if (!kref) {
         ret = -ENOSPC;
         break;
      }

      if (bref->packet) {
         pushbuf_krel(push, bref->bo, bref->packet, 0, 0, 0);
         *push->cur++ = 0;
         pushbuf_krel(push, bref->bo, bref->data, bref->flags,
                  bref->vor, bref->tor);
         *push->cur++ = 0;
      }
   }

   list_splice(&bctx->pending, &bctx->current);
   list_inithead(&bctx->pending);

   if (ret) {
      pushbuf_refn_fail(push, sref, srel);
      if (retry) {
         pushbuf_flush(push);
         return pushbuf_validate(push, false);
      }
   }

   return ret;
}

int
nouveau_pushbuf_new(struct nouveau_client *client, struct nouveau_object *chan, int nr,
                    uint32_t size, struct nouveau_pushbuf **ppush)
{
   struct nouveau_drm *drm = nouveau_drm(&client->device->object);
   struct nouveau_fifo *fifo = chan->data;
   struct nouveau_pushbuf_priv *nvpb;
   struct nouveau_pushbuf *push;
   struct drm_nouveau_gem_pushbuf req = {};
   int ret;

   if (chan->oclass != NOUVEAU_FIFO_CHANNEL_CLASS)
      return -EINVAL;

   /* nop pushbuf call, to get the current "return to main" sequence
    * we need to append to the pushbuf on early chipsets
    */
   req.channel = chan->handle;
   req.nr_push = 0;
   ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GEM_PUSHBUF, &req, sizeof(req));
   if (ret)
      return ret;

   nvpb = calloc(1, sizeof(*nvpb) + nr * sizeof(*nvpb->bos));
   if (!nvpb)
      return -ENOMEM;

   nvpb->suffix0 = req.suffix0;
   nvpb->suffix1 = req.suffix1;
   nvpb->krec = calloc(1, sizeof(*nvpb->krec));
   nvpb->list = nvpb->krec;
   if (!nvpb->krec) {
      free(nvpb);
      return -ENOMEM;
   }

   push = &nvpb->base;
   push->client = client;
   push->channel = chan;
   push->flags = NOUVEAU_BO_RD;
   if (fifo->pushbuf & NOUVEAU_GEM_DOMAIN_GART) {
      push->flags |= NOUVEAU_BO_GART;
      nvpb->type   = NOUVEAU_BO_GART;
   } else
   if (fifo->pushbuf & NOUVEAU_GEM_DOMAIN_VRAM) {
      push->flags |= NOUVEAU_BO_VRAM;
      nvpb->type   = NOUVEAU_BO_VRAM;
   }
   nvpb->type |= NOUVEAU_BO_MAP;

   for (nvpb->bo_nr = 0; nvpb->bo_nr < nr; nvpb->bo_nr++) {
      ret = nouveau_bo_new(client->device, nvpb->type, 0, size, NULL, &nvpb->bos[nvpb->bo_nr]);
      if (ret) {
         nouveau_pushbuf_del(&push);
         return ret;
      }
   }

   list_inithead(&nvpb->bctx_list);
   *ppush = push;
   return 0;
}

void
nouveau_pushbuf_del(struct nouveau_pushbuf **ppush)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(*ppush);
   if (nvpb) {
      struct drm_nouveau_gem_pushbuf_bo *kref;
      struct nouveau_pushbuf_krec *krec;
      while ((krec = nvpb->list)) {
         kref = krec->buffer;
         while (krec->nr_buffer--) {
            unsigned long priv = kref++->user_priv;
            struct nouveau_bo *bo = (void *)priv;
            cli_kref_set(nvpb->base.client, bo, NULL, NULL);
            nouveau_bo_ref(NULL, &bo);
         }
         nvpb->list = krec->next;
         free(krec);
      }
      while (nvpb->bo_nr--)
         nouveau_bo_ref(NULL, &nvpb->bos[nvpb->bo_nr]);
      nouveau_bo_ref(NULL, &nvpb->bo);
      free(nvpb);
   }
   *ppush = NULL;
}

void
nouveau_bufctx_reset(struct nouveau_bufctx *bctx, int bin)
{
   struct nouveau_bufctx_priv *pctx = nouveau_bufctx(bctx);
   struct nouveau_bufbin_priv *pbin = &pctx->bins[bin];
   struct nouveau_bufref_priv *pref;

   while ((pref = pbin->list)) {
      list_delinit(&pref->base.thead);
      pbin->list = pref->next;
      pref->next = pctx->free;
      pctx->free = pref;
   }

   bctx->relocs -= pbin->relocs;
   pbin->relocs  = 0;
}

struct nouveau_bufctx *
nouveau_pushbuf_bufctx(struct nouveau_pushbuf *push, struct nouveau_bufctx *ctx)
{
   struct nouveau_bufctx *prev = push->bufctx;
   push->bufctx = ctx;
   return prev;
}

void
nouveau_pushbuf_data(struct nouveau_pushbuf *push, struct nouveau_bo *bo, uint64_t offset,
                     uint64_t length)
{
   struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct drm_nouveau_gem_pushbuf_push *kpsh;
   struct drm_nouveau_gem_pushbuf_bo *kref;

   if (bo != nvpb->bo && nvpb->bgn != push->cur) {
      if (nvpb->suffix0 || nvpb->suffix1) {
         *push->cur++ = nvpb->suffix0;
         *push->cur++ = nvpb->suffix1;
      }

      nouveau_pushbuf_data(push, nvpb->bo, (nvpb->bgn - nvpb->ptr) * 4, (push->cur - nvpb->bgn) * 4);
      nvpb->bgn = push->cur;
   }

   if (bo) {
      kref = cli_kref_get(push->client, bo);
      assert(kref);
      kpsh = &krec->push[krec->nr_push++];
      kpsh->bo_index = kref - krec->buffer;
      kpsh->offset   = offset;
      kpsh->length   = length;
   }
}

int
nouveau_pushbuf_kick(struct nouveau_pushbuf *push)
{
   pushbuf_flush(push);
   return pushbuf_validate(push, false);
}

int
nouveau_pushbuf_refn(struct nouveau_pushbuf *push, struct nouveau_pushbuf_refn *refs, int nr)
{
   return pushbuf_refn(push, true, refs, nr);
}

void
nouveau_pushbuf_reloc(struct nouveau_pushbuf *push, struct nouveau_bo *bo, uint32_t data,
                      uint32_t flags, uint32_t vor, uint32_t tor)
{
   *push->cur = pushbuf_krel(push, bo, data, flags, vor, tor);
   push->cur++;
}

int
nouveau_pushbuf_space(struct nouveau_pushbuf *push, uint32_t dwords, uint32_t relocs,
                      uint32_t pushes)
{
      struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(push);
   struct nouveau_pushbuf_krec *krec = nvpb->krec;
   struct nouveau_client *client = push->client;
   struct nouveau_bo *bo = NULL;
   bool flushed = false;
   int ret = 0;

   /* switch to next buffer if insufficient space in the current one */
   if (push->cur + dwords >= push->end) {
      if (nvpb->bo_next < nvpb->bo_nr) {
         nouveau_bo_ref(nvpb->bos[nvpb->bo_next++], &bo);
         if (nvpb->bo_next == nvpb->bo_nr)
            nvpb->bo_next = 0;
      } else {
         ret = nouveau_bo_new(client->device, nvpb->type, 0, nvpb->bos[0]->size, NULL, &bo);
         if (ret)
            return ret;
      }
   }

   /* make sure there's always enough space to queue up the pending
    * data in the pushbuf proper
    */
   pushes++;

   /* need to flush if we've run out of space on an immediate pushbuf,
    * if the new buffer won't fit, or if the kernel push/reloc limits
    * have been hit
    */
   if (bo ||
       krec->nr_reloc + relocs >= NOUVEAU_GEM_MAX_RELOCS ||
       krec->nr_push + pushes >= NOUVEAU_GEM_MAX_PUSH) {
      if (nvpb->bo && krec->nr_buffer)
         pushbuf_flush(push);
      flushed = true;
   }

   /* if necessary, switch to new buffer */
   if (bo) {
      ret = nouveau_bo_map(bo, NOUVEAU_BO_WR, push->client);
      if (ret)
         return ret;

      nouveau_pushbuf_data(push, NULL, 0, 0);
      nouveau_bo_ref(bo, &nvpb->bo);
      nouveau_bo_ref(NULL, &bo);

      nvpb->bgn = nvpb->bo->map;
      nvpb->ptr = nvpb->bgn;
      push->cur = nvpb->bgn;
      push->end = push->cur + (nvpb->bo->size / 4);
      push->end -= 2 + push->rsvd_kick; /* space for suffix */
   }

   pushbuf_kref(push, nvpb->bo, push->flags);
   return flushed ? pushbuf_validate(push, false) : 0;
}

int
nouveau_pushbuf_validate(struct nouveau_pushbuf *push)
{
   return pushbuf_validate(push, true);
}
