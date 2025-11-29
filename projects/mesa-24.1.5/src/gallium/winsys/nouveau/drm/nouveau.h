#ifndef NOUVEAU_H
#define NOUVEAU_H

#include <stdbool.h>
#include <stdint.h>

#include "util/list.h"

#include "drm-uapi/nouveau_drm.h"

#define NOUVEAU_FIFO_CHANNEL_CLASS 0x80000001
#define NOUVEAU_NOTIFIER_CLASS     0x80000002

#define NOUVEAU_BO_VRAM     0x00000001
#define NOUVEAU_BO_GART     0x00000002
#define NOUVEAU_BO_APER    (NOUVEAU_BO_VRAM | NOUVEAU_BO_GART)
#define NOUVEAU_BO_RD       0x00000100
#define NOUVEAU_BO_WR       0x00000200
#define NOUVEAU_BO_RDWR    (NOUVEAU_BO_RD | NOUVEAU_BO_WR)
#define NOUVEAU_BO_NOBLOCK  0x00000400
#define NOUVEAU_BO_LOW      0x00001000
#define NOUVEAU_BO_OR       0x00004000
#define NOUVEAU_BO_COHERENT 0x10000000
#define NOUVEAU_BO_NOSNOOP  0x20000000
#define NOUVEAU_BO_CONTIG   0x40000000
#define NOUVEAU_BO_MAP      0x80000000

struct nouveau_mclass {
   int32_t oclass;
   int version;
};

struct nouveau_object {
   struct nouveau_object *parent;
   uint64_t handle;
   uint32_t oclass;
   void *data;
};

struct nouveau_drm {
   struct nouveau_object client;
   int fd;
   uint32_t version;
};

struct nouveau_device {
   struct nouveau_object object;
   uint32_t chipset;
   uint64_t vram_size;
   uint64_t gart_size;
   uint64_t vram_limit;
   uint64_t gart_limit;
};

struct nouveau_client {
   struct nouveau_device *device;
};

union nouveau_bo_config {
   struct {
      uint32_t memtype;
      uint32_t tile_mode;
   } nv50;
   struct {
      uint32_t memtype;
      uint32_t tile_mode;
   } nvc0;
};

struct nouveau_bo {
   struct nouveau_device *device;
   uint32_t handle;
   uint64_t size;
   uint32_t flags;
   uint64_t offset;
   void *map;
   union nouveau_bo_config config;
};

struct nouveau_bufref {
   struct list_head thead;
   struct nouveau_bo *bo;
   uint32_t packet;
   uint32_t flags;
   uint32_t data;
   uint32_t vor;
   uint32_t tor;
   uint32_t priv_data;
   void *priv;
};

struct nouveau_bufctx {
   struct nouveau_client *client;
   struct list_head head;
   struct list_head pending;
   struct list_head current;
   int relocs;
};

struct nouveau_pushbuf {
   struct nouveau_client *client;
   struct nouveau_object *channel;
   struct nouveau_bufctx *bufctx;
   void (*kick_notify)(struct nouveau_pushbuf *);
   void *user_priv;
   uint32_t rsvd_kick;
   uint32_t flags;
   uint32_t *cur;
   uint32_t *end;
};

struct nouveau_pushbuf_refn {
   struct nouveau_bo *bo;
   uint32_t flags;
};

struct nouveau_fifo {
   uint32_t pushbuf;
   uint32_t notify;
};

struct nv04_fifo {
   struct nouveau_fifo base;
   uint32_t vram;
   uint32_t gart;
};

struct nv04_notify {
   uint32_t offset;
   uint32_t length;
};

struct nvc0_fifo {
   struct nouveau_fifo base;
};

struct nve0_fifo {
   struct nouveau_fifo base;
   uint32_t engine;
};

int nouveau_drm_new(int fd, struct nouveau_drm **);
void nouveau_drm_del(struct nouveau_drm **);

static inline struct nouveau_drm *
nouveau_drm(struct nouveau_object *obj)
{
   while (obj && obj->parent)
      obj = obj->parent;
   return (struct nouveau_drm *)obj;
}

struct nv_device_info_v0;

int nouveau_device_new(struct nouveau_object *parent, struct nouveau_device **);
void nouveau_device_del(struct nouveau_device **);
int nouveau_device_info(struct nouveau_device *, struct nv_device_info_v0 *);
int nouveau_getparam(struct nouveau_device *, uint64_t param, uint64_t *value);

int nouveau_client_new(struct nouveau_device *, struct nouveau_client **);
void nouveau_client_del(struct nouveau_client **);

int nouveau_object_new(struct nouveau_object *parent, uint64_t handle, uint32_t oclass, void *data,
                       uint32_t length, struct nouveau_object **);
void nouveau_object_del(struct nouveau_object **);
int nouveau_object_mclass(struct nouveau_object *, const struct nouveau_mclass *);

int nouveau_bo_new(struct nouveau_device *, uint32_t flags, uint32_t align, uint64_t size,
                   union nouveau_bo_config *, struct nouveau_bo **);
int nouveau_bo_map(struct nouveau_bo *, uint32_t access, struct nouveau_client *);
int nouveau_bo_name_get(struct nouveau_bo *, uint32_t *name);
int nouveau_bo_name_ref(struct nouveau_device *, uint32_t name, struct nouveau_bo **);
int nouveau_bo_prime_handle_ref(struct nouveau_device *, int prime_fd, struct nouveau_bo **);
void nouveau_bo_ref(struct nouveau_bo *, struct nouveau_bo **);
int nouveau_bo_set_prime(struct nouveau_bo *, int *prime_fd);
int nouveau_bo_wait(struct nouveau_bo *, uint32_t access, struct nouveau_client *);
int nouveau_bo_wrap(struct nouveau_device *, uint32_t handle, struct nouveau_bo **);

int nouveau_bufctx_new(struct nouveau_client *, int bins, struct nouveau_bufctx **);
void nouveau_bufctx_del(struct nouveau_bufctx **);
struct nouveau_bufref *
nouveau_bufctx_refn(struct nouveau_bufctx *, int bin, struct nouveau_bo *, uint32_t flags);
struct nouveau_bufref *
nouveau_bufctx_mthd(struct nouveau_bufctx *, int bin,  uint32_t packet, struct nouveau_bo *,
                    uint64_t data, uint32_t flags, uint32_t vor, uint32_t tor);
void nouveau_bufctx_reset(struct nouveau_bufctx *, int bin);

int nouveau_pushbuf_new(struct nouveau_client *, struct nouveau_object *chan, int nr, uint32_t size,
                        struct nouveau_pushbuf **);
void nouveau_pushbuf_del(struct nouveau_pushbuf **);
struct nouveau_bufctx *
nouveau_pushbuf_bufctx(struct nouveau_pushbuf *, struct nouveau_bufctx *);
void nouveau_pushbuf_data(struct nouveau_pushbuf *, struct nouveau_bo *, uint64_t offset,
                          uint64_t length);
int nouveau_pushbuf_kick(struct nouveau_pushbuf *);
int nouveau_pushbuf_refn(struct nouveau_pushbuf *, struct nouveau_pushbuf_refn *, int nr);
void nouveau_pushbuf_reloc(struct nouveau_pushbuf *, struct nouveau_bo *, uint32_t data,
                           uint32_t flags, uint32_t vor, uint32_t tor);
int nouveau_pushbuf_space(struct nouveau_pushbuf *, uint32_t dwords, uint32_t relocs,
                          uint32_t pushes);
int nouveau_pushbuf_validate(struct nouveau_pushbuf *);

#endif
