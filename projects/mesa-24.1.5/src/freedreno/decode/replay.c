/*
 * Copyright © 2022 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#if FD_REPLAY_KGSL
#include "../vulkan/msm_kgsl.h"
#elif FD_REPLAY_MSM
#include <xf86drm.h>
#include "drm-uapi/msm_drm.h"
#elif FD_REPLAY_WSL
#define __KERNEL__
#include "drm-uapi/d3dkmthk.h"
#endif

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "util/os_time.h"
#include "util/rb_tree.h"
#include "util/u_vector.h"
#include "util/vma.h"
#include "buffers.h"
#include "cffdec.h"
#include "io.h"
#include "redump.h"
#include "rdutil.h"

/**
 * Replay command stream obtained from:
 * - /sys/kernel/debug/dri/0/rd
 * - /sys/kernel/debug/dri/0/hangrd
 * !!! Command stream capture should be done with ALL buffers:
 * - echo 1 > /sys/module/msm/parameters/rd_full
 *
 * Requires kernel with MSM_INFO_SET_IOVA support.
 * In case userspace IOVAs are not supported, like on KGSL, we have to
 * pre-allocate a single buffer and hope it always allocated starting
 * from the same address.
 *
 * TODO: Misrendering, would require marking framebuffer images
 *       at each renderpass in order to fetch and decode them.
 *
 * Code from Freedreno/Turnip is not re-used here since the relevant
 * pieces may introduce additional allocations which cannot be allowed
 * during the replay.
 *
 * For how-to see freedreno.rst
 */

static const char *exename = NULL;

static const uint64_t FAKE_ADDRESS_SPACE_SIZE = 1024 * 1024 * 1024;

static int handle_file(const char *filename, uint32_t first_submit,
                       uint32_t last_submit, uint32_t submit_to_override,
                       uint64_t base_addr, const char *cmdstreamgen);

static void
print_usage(const char *name, const char *default_csgen)
{
   /* clang-format off */
   fprintf(stderr, "Usage:\n\n"
           "\t%s [OPTIONS]... FILE...\n\n"
           "Options:\n"
           "\t-e, --exe=NAME         - only use cmdstream from named process\n"
           "\t-o  --override=submit  - № of the submit to override\n"
           "\t-g  --generator=path   - executable which generate cmdstream for override (default: %s)\n"
           "\t-f  --first=submit     - first submit № to replay\n"
           "\t-l  --last=submit      - last submit № to replay\n"
           "\t-a  --address=address  - base iova address on WSL\n"
           "\t-h, --help             - show this message\n"
           , name, default_csgen);
   /* clang-format on */
   exit(2);
}

/* clang-format off */
static const struct option opts[] = {
      { "exe",       required_argument, 0, 'e' },
      { "override",  required_argument, 0, 'o' },
      { "generator", required_argument, 0, 'g' },
      { "first",     required_argument, 0, 'f' },
      { "last",      required_argument, 0, 'l' },
      { "address",   required_argument, 0, 'a' },
      { "help",      no_argument,       0, 'h' },
};
/* clang-format on */

int
main(int argc, char **argv)
{
   int ret = -1;
   int c;

   uint32_t submit_to_override = -1;
   uint32_t first_submit = 0;
   uint32_t last_submit = -1;
   uint64_t base_addr = 0;

   char *default_csgen = malloc(PATH_MAX);
   snprintf(default_csgen, PATH_MAX, "%s/generate_rd", dirname(argv[0]));

   const char *csgen = default_csgen;

   while ((c = getopt_long(argc, argv, "e:o:g:f:l:a:h", opts, NULL)) != -1) {
      switch (c) {
      case 0:
         /* option that set a flag, nothing to do */
         break;
      case 'e':
         exename = optarg;
         break;
      case 'o':
         submit_to_override = strtoul(optarg, NULL, 0);
         break;
      case 'g':
         csgen = optarg;
         break;
      case 'f':
         first_submit = strtoul(optarg, NULL, 0);
         break;
      case 'l':
         last_submit = strtoul(optarg, NULL, 0);
         break;
      case 'a':
         base_addr = strtoull(optarg, NULL, 0);
         break;
      case 'h':
      default:
         print_usage(argv[0], default_csgen);
      }
   }

   while (optind < argc) {
      ret = handle_file(argv[optind], first_submit, last_submit,
                        submit_to_override, base_addr, csgen);
      if (ret) {
         fprintf(stderr, "error reading: %s\n", argv[optind]);
         fprintf(stderr, "continuing..\n");
      }
      optind++;
   }

   if (ret)
      print_usage(argv[0], default_csgen);

   return ret;
}

struct buffer {
   struct rb_node node;

   uint32_t gem_handle;
   uint64_t size;
   uint64_t iova;
   void *map;

   bool used;
   uint32_t flags;
};

struct cmdstream {
   uint64_t iova;
   uint64_t size;
};

struct wrbuf {
   uint64_t iova;
   uint64_t size;
   char* name;
};

struct device {
   int fd;

   struct rb_tree buffers;
   struct util_vma_heap vma;

   struct u_vector cmdstreams;

   uint64_t shader_log_iova;
   uint64_t cp_log_iova;

   bool has_set_iova;

   uint32_t va_id;
   void *va_map;
   uint64_t va_iova;

   struct u_vector wrbufs;

#ifdef FD_REPLAY_MSM
   uint32_t queue_id;
#endif

#ifdef FD_REPLAY_KGSL
   uint32_t context_id;
#endif

#ifdef FD_REPLAY_WSL
   struct d3dkmthandle device;
   struct d3dkmthandle context;

   /* We don't know at the moment a good way to wait for submission to complete
    * on WSL, so we could use our own fences.
    */
   uint64_t fence_iova;
   uint64_t fence_ib_iova;
   volatile uint32_t *fence;
   uint32_t *fence_ib;
#endif
};

void buffer_mem_free(struct device *dev, struct buffer *buf);

static int
rb_buffer_insert_cmp(const struct rb_node *n1, const struct rb_node *n2)
{
   const struct buffer *buf1 = (const struct buffer *)n1;
   const struct buffer *buf2 = (const struct buffer *)n2;
   /* Note that gpuaddr comparisions can overflow an int: */
   if (buf1->iova > buf2->iova)
      return 1;
   else if (buf1->iova < buf2->iova)
      return -1;
   return 0;
}

static int
rb_buffer_search_cmp(const struct rb_node *node, const void *addrptr)
{
   const struct buffer *buf = (const struct buffer *)node;
   uint64_t iova = *(uint64_t *)addrptr;
   if (buf->iova + buf->size <= iova)
      return -1;
   else if (buf->iova > iova)
      return 1;
   return 0;
}

static struct buffer *
device_get_buffer(struct device *dev, uint64_t iova)
{
   if (iova == 0)
      return NULL;
   return (struct buffer *)rb_tree_search(&dev->buffers, &iova,
                                          rb_buffer_search_cmp);
}

static void
device_mark_buffers(struct device *dev)
{
   rb_tree_foreach_safe (struct buffer, buf, &dev->buffers, node) {
      buf->used = false;
   }
}

static void
device_free_buffers(struct device *dev)
{
   rb_tree_foreach_safe (struct buffer, buf, &dev->buffers, node) {
      buffer_mem_free(dev, buf);
      rb_tree_remove(&dev->buffers, &buf->node);
      free(buf);
   }
}

static void
device_print_shader_log(struct device *dev)
{
   struct shader_log {
      uint64_t cur_iova;
      union {
         uint32_t entries_u32[0];
         float entries_float[0];
      };
   };

   if (dev->shader_log_iova != 0)
   {
      struct buffer *buf = device_get_buffer(dev, dev->shader_log_iova);
      if (buf) {
         struct shader_log *log = buf->map + (dev->shader_log_iova - buf->iova);
         uint32_t count = (log->cur_iova - dev->shader_log_iova -
                           offsetof(struct shader_log, entries_u32)) / 4;

         printf("Shader Log Entries: %u\n", count);

         for (uint32_t i = 0; i < count; i++) {
            printf("[%u] %08x %.4f\n", i, log->entries_u32[i],
                   log->entries_float[i]);
         }

         printf("========================================\n");
      }
   }
}

static void
device_print_cp_log(struct device *dev)
{
   struct cp_log {
      uint64_t cur_iova;
      uint64_t tmp;
      uint64_t first_entry_size;
   };

   struct cp_log_entry {
      uint64_t size;
      uint32_t data[0];
   };

   if (dev->cp_log_iova == 0)
      return;

   struct buffer *buf = device_get_buffer(dev, dev->cp_log_iova);
   if (!buf)
      return;

   struct cp_log *log = buf->map + (dev->cp_log_iova - buf->iova);
   if (log->first_entry_size == 0)
      return;

   struct cp_log_entry *log_entry =
      buf->map + offsetof(struct cp_log, first_entry_size);
   uint32_t idx = 0;
   while (log_entry->size != 0) {
      printf("\nCP Log [%u]:\n", idx++);
      uint32_t dwords = log_entry->size / 4;

      for (uint32_t i = 0; i < dwords; i++) {
         if (i % 8 == 0)
            printf("\t");
         printf("%08x ", log_entry->data[i]);
         if (i % 8 == 7)
            printf("\n");
      }
      printf("\n");

      log_entry = (void *)log_entry + log_entry->size +
                  offsetof(struct cp_log_entry, data);
   }
}

static void
device_dump_wrbuf(struct device *dev)
{
   if (!u_vector_length(&dev->wrbufs))
      return;

   char buffer_dir[PATH_MAX];
   getcwd(buffer_dir, sizeof(buffer_dir));
   strcat(buffer_dir, "/buffers");
   rmdir(buffer_dir);
   mkdir(buffer_dir, 0777);

   struct wrbuf *wrbuf;
   u_vector_foreach(wrbuf, &dev->wrbufs) {
      char buffer_path[PATH_MAX];
      snprintf(buffer_path, sizeof(buffer_path), "%s/%s", buffer_dir, wrbuf->name);
      FILE *f = fopen(buffer_path, "wb");
      if (!f) {
         fprintf(stderr, "Error opening %s\n", buffer_path);
         goto end_it;
      }

      struct buffer *buf = device_get_buffer(dev, wrbuf->iova);
      if (!buf) {
         fprintf(stderr, "Error getting buffer for %s\n", buffer_path);
         goto end_it;
      }

      uint64_t offset = wrbuf->iova - buf->iova;
      uint64_t size = MIN2(wrbuf->size, buf->size - offset);
      if (size != wrbuf->size) {
         fprintf(stderr, "Warning: Clamping buffer %s as it's smaller than expected (0x%" PRIx64 " < 0x%" PRIx64 ")\n", wrbuf->name, size, wrbuf->size);
      }

      printf("Dumping %s (0x%" PRIx64 " - 0x%" PRIx64 ")\n", wrbuf->name, wrbuf->iova, wrbuf->iova + size);

      fwrite(buf->map + offset, size, 1, f);

      end_it:
      fclose(f);
   }
}

#if FD_REPLAY_MSM
static inline void
get_abs_timeout(struct drm_msm_timespec *tv, uint64_t ns)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   tv->tv_sec = t.tv_sec + ns / 1000000000;
   tv->tv_nsec = t.tv_nsec + ns % 1000000000;
}

static struct device *
device_create(uint64_t base_addr)
{
   struct device *dev = calloc(sizeof(struct device), 1);

   dev->fd = drmOpenWithType("msm", NULL, DRM_NODE_RENDER);
   if (dev->fd < 0) {
      errx(1, "Cannot open MSM fd!");
   }

   uint64_t va_start, va_size;

   struct drm_msm_param req = {
      .pipe = MSM_PIPE_3D0,
      .param = MSM_PARAM_VA_START,
   };

   int ret = drmCommandWriteRead(dev->fd, DRM_MSM_GET_PARAM, &req, sizeof(req));
   va_start = req.value;

   if (!ret) {
      req.param = MSM_PARAM_VA_SIZE;
      ret = drmCommandWriteRead(dev->fd, DRM_MSM_GET_PARAM, &req, sizeof(req));
      va_size = req.value;

      dev->has_set_iova = true;
   }

   if (ret) {
      printf("MSM_INFO_SET_IOVA is not supported!\n");

      struct drm_msm_gem_new req_new = {.size = FAKE_ADDRESS_SPACE_SIZE, .flags = MSM_BO_CACHED_COHERENT};
      drmCommandWriteRead(dev->fd, DRM_MSM_GEM_NEW, &req_new, sizeof(req_new));
      dev->va_id = req_new.handle;

      struct drm_msm_gem_info req_info = {
         .handle = req_new.handle,
         .info = MSM_INFO_GET_IOVA,
      };

      drmCommandWriteRead(dev->fd,
                                 DRM_MSM_GEM_INFO, &req_info, sizeof(req_info));
      dev->va_iova = req_info.value;

      struct drm_msm_gem_info req_offset = {
         .handle = req_new.handle,
         .info = MSM_INFO_GET_OFFSET,
      };

      drmCommandWriteRead(dev->fd, DRM_MSM_GEM_INFO, &req_offset, sizeof(req_offset));

      dev->va_map = mmap(0, FAKE_ADDRESS_SPACE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                       dev->fd, req_offset.value);
      if (dev->va_map == MAP_FAILED) {
         err(1, "mmap failure");
      }

      va_start = dev->va_iova;
      va_size = FAKE_ADDRESS_SPACE_SIZE;

      printf("Allocated iova %" PRIx64 "\n", dev->va_iova);
   }

   struct drm_msm_submitqueue req_queue = {
      .flags = 0,
      .prio = 0,
   };

   ret = drmCommandWriteRead(dev->fd, DRM_MSM_SUBMITQUEUE_NEW, &req_queue,
                             sizeof(req_queue));
   if (ret) {
      err(1, "DRM_MSM_SUBMITQUEUE_NEW failure");
   }

   dev->queue_id = req_queue.id;

   rb_tree_init(&dev->buffers);
   util_vma_heap_init(&dev->vma, va_start, ROUND_DOWN_TO(va_size, 4096));
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   return dev;
}

static void
device_submit_cmdstreams(struct device *dev)
{
   if (!u_vector_length(&dev->cmdstreams)) {
      device_free_buffers(dev);
      return;
   }

   struct drm_msm_gem_submit_cmd cmds[u_vector_length(&dev->cmdstreams)];

   uint32_t idx = 0;
   struct cmdstream *cmd;
   u_vector_foreach(cmd, &dev->cmdstreams) {
      struct buffer *cmdstream_buf = device_get_buffer(dev, cmd->iova);

      uint32_t bo_idx = 0;
      rb_tree_foreach (struct buffer, buf, &dev->buffers, node) {
         if (buf == cmdstream_buf)
            break;

         bo_idx++;
      }

      if (cmdstream_buf)
         cmdstream_buf->flags = MSM_SUBMIT_BO_DUMP;

      struct drm_msm_gem_submit_cmd *submit_cmd = &cmds[idx];
      submit_cmd->type = MSM_SUBMIT_CMD_BUF;
      submit_cmd->submit_idx = bo_idx;
      if (dev->has_set_iova) {
         submit_cmd->submit_offset = cmd->iova - cmdstream_buf->iova;
      } else {
         submit_cmd->submit_offset = cmd->iova - dev->va_iova;
      }
      submit_cmd->size = cmd->size;
      submit_cmd->pad = 0;
      submit_cmd->nr_relocs = 0;
      submit_cmd->relocs = 0;

      idx++;
   }

   uint32_t bo_count = 0;
   rb_tree_foreach (struct buffer, buf, &dev->buffers, node) {
      if (buf)
         bo_count++;
   }

   if (!dev->has_set_iova) {
      bo_count = 1;
   }

   struct drm_msm_gem_submit_bo *bo_list =
      calloc(sizeof(struct drm_msm_gem_submit_bo), bo_count);

   if (dev->has_set_iova) {
      uint32_t bo_idx = 0;
      rb_tree_foreach (struct buffer, buf, &dev->buffers, node) {
         struct drm_msm_gem_submit_bo *submit_bo = &bo_list[bo_idx++];
         submit_bo->handle = buf->gem_handle;
         submit_bo->flags =
            buf->flags | MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE;
         submit_bo->presumed = buf->iova;

         buf->flags = 0;
      }
   } else {
      bo_list[0].handle = dev->va_id;
      bo_list[0].flags =
         MSM_SUBMIT_BO_DUMP | MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE;
      bo_list[0].presumed = dev->va_iova;
   }

   struct drm_msm_gem_submit submit_req = {
      .flags = MSM_PIPE_3D0,
      .queueid = dev->queue_id,
      .bos = (uint64_t)(uintptr_t)bo_list,
      .nr_bos = bo_count,
      .cmds = (uint64_t)(uintptr_t)cmds,
      .nr_cmds = u_vector_length(&dev->cmdstreams),
      .in_syncobjs = 0,
      .out_syncobjs = 0,
      .nr_in_syncobjs = 0,
      .nr_out_syncobjs = 0,
      .syncobj_stride = sizeof(struct drm_msm_gem_submit_syncobj),
   };

   int ret = drmCommandWriteRead(dev->fd, DRM_MSM_GEM_SUBMIT, &submit_req,
                                 sizeof(submit_req));

   if (ret) {
      err(1, "DRM_MSM_GEM_SUBMIT failure %d", ret);
   }

   /* Wait for submission to complete in order to be sure that
    * freeing buffers would free their VMAs in the kernel.
    * Makes sure that new allocations won't clash with old ones.
    */
   struct drm_msm_wait_fence wait_req = {
      .fence = submit_req.fence,
      .queueid = dev->queue_id,
   };
   get_abs_timeout(&wait_req.timeout, 1000000000);

   ret =
      drmCommandWrite(dev->fd, DRM_MSM_WAIT_FENCE, &wait_req, sizeof(wait_req));
   if (ret && (ret != -ETIMEDOUT)) {
      err(1, "DRM_MSM_WAIT_FENCE failure %d", ret);
   }

   u_vector_finish(&dev->cmdstreams);
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));

   device_print_shader_log(dev);
   device_print_cp_log(dev);

   device_dump_wrbuf(dev);
   u_vector_finish(&dev->wrbufs);
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   device_free_buffers(dev);
}

static void
buffer_mem_alloc(struct device *dev, struct buffer *buf)
{
   bool success = util_vma_heap_alloc_addr(&dev->vma, buf->iova, buf->size);
   if (!success)
      errx(1, "Failed to allocate buffer");

   if (!dev->has_set_iova) {
      uint64_t offset = buf->iova - dev->va_iova;
      assert(offset < FAKE_ADDRESS_SPACE_SIZE && (offset + buf->size) <= FAKE_ADDRESS_SPACE_SIZE);
      buf->map = ((uint8_t*)dev->va_map) + offset;
      return;
   }

   {
      struct drm_msm_gem_new req = {.size = buf->size, .flags = MSM_BO_WC};

      int ret =
         drmCommandWriteRead(dev->fd, DRM_MSM_GEM_NEW, &req, sizeof(req));
      if (ret) {
         err(1, "DRM_MSM_GEM_NEW failure %d", ret);
      }

      buf->gem_handle = req.handle;
   }

   {
      struct drm_msm_gem_info req = {
         .handle = buf->gem_handle,
         .info = MSM_INFO_SET_IOVA,
         .value = buf->iova,
      };

      int ret =
         drmCommandWriteRead(dev->fd, DRM_MSM_GEM_INFO, &req, sizeof(req));

      if (ret) {
         err(1, "MSM_INFO_SET_IOVA failure %d", ret);
      }
   }

   {
      struct drm_msm_gem_info req = {
         .handle = buf->gem_handle,
         .info = MSM_INFO_GET_OFFSET,
      };

      int ret =
         drmCommandWriteRead(dev->fd, DRM_MSM_GEM_INFO, &req, sizeof(req));
      if (ret) {
         err(1, "MSM_INFO_GET_OFFSET failure %d", ret);
      }

      void *map = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                       dev->fd, req.value);
      if (map == MAP_FAILED) {
         err(1, "mmap failure");
      }

      buf->map = map;
   }
}

void
buffer_mem_free(struct device *dev, struct buffer *buf)
{
   if (dev->has_set_iova) {
      munmap(buf->map, buf->size);

      struct drm_msm_gem_info req_iova = {
         .handle = buf->gem_handle,
         .info = MSM_INFO_SET_IOVA,
         .value = 0,
      };

      int ret = drmCommandWriteRead(dev->fd, DRM_MSM_GEM_INFO, &req_iova,
                                    sizeof(req_iova));
      if (ret < 0) {
         err(1, "MSM_INFO_SET_IOVA(0) failed! %d", ret);
         return;
      }

      struct drm_gem_close req = {
         .handle = buf->gem_handle,
      };
      drmIoctl(dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
   }

   util_vma_heap_free(&dev->vma, buf->iova, buf->size);
}

#elif FD_REPLAY_KGSL
static int
safe_ioctl(int fd, unsigned long request, void *arg)
{
   int ret;

   do {
      ret = ioctl(fd, request, arg);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   return ret;
}

static struct device *
device_create(uint64_t base_addr)
{
   struct device *dev = calloc(sizeof(struct device), 1);

   static const char path[] = "/dev/kgsl-3d0";

   dev->fd = open(path, O_RDWR | O_CLOEXEC);
   if (dev->fd < 0) {
      errx(1, "Cannot open KGSL fd!");
   }

   struct kgsl_gpumem_alloc_id req = {
      .size = FAKE_ADDRESS_SPACE_SIZE,
      .flags = KGSL_MEMFLAGS_IOCOHERENT,
   };

   int ret = safe_ioctl(dev->fd, IOCTL_KGSL_GPUMEM_ALLOC_ID, &req);
   if (ret) {
      err(1, "IOCTL_KGSL_GPUMEM_ALLOC_ID failure");
   }

   dev->va_id = req.id;
   dev->va_iova = req.gpuaddr;
   dev->va_map = mmap(0, FAKE_ADDRESS_SPACE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, dev->fd, req.id << 12);

   rb_tree_init(&dev->buffers);
   util_vma_heap_init(&dev->vma, req.gpuaddr, ROUND_DOWN_TO(FAKE_ADDRESS_SPACE_SIZE, 4096));
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   struct kgsl_drawctxt_create drawctxt_req = {
      .flags = KGSL_CONTEXT_SAVE_GMEM |
              KGSL_CONTEXT_NO_GMEM_ALLOC |
              KGSL_CONTEXT_PREAMBLE,
   };

   ret = safe_ioctl(dev->fd, IOCTL_KGSL_DRAWCTXT_CREATE, &drawctxt_req);
   if (ret) {
      err(1, "IOCTL_KGSL_DRAWCTXT_CREATE failure");
   }

   printf("Allocated iova %" PRIx64 "\n", dev->va_iova);

   dev->context_id = drawctxt_req.drawctxt_id;

   return dev;
}

static void
device_submit_cmdstreams(struct device *dev)
{
   if (!u_vector_length(&dev->cmdstreams)) {
      device_free_buffers(dev);
      return;
   }

   struct kgsl_command_object cmds[u_vector_length(&dev->cmdstreams)];

   uint32_t idx = 0;
   struct cmdstream *cmd;
   u_vector_foreach(cmd, &dev->cmdstreams) {
      struct kgsl_command_object *submit_cmd = &cmds[idx++];
      submit_cmd->gpuaddr = cmd->iova;
      submit_cmd->size = cmd->size;
      submit_cmd->flags = KGSL_CMDLIST_IB;
      submit_cmd->id = dev->va_id;
   }

   struct kgsl_gpu_command submit_req = {
      .flags = KGSL_CMDBATCH_SUBMIT_IB_LIST,
      .cmdlist = (uintptr_t) &cmds,
      .cmdsize = sizeof(struct kgsl_command_object),
      .numcmds = u_vector_length(&dev->cmdstreams),
      .numsyncs = 0,
      .context_id = dev->context_id,
   };

   int ret = safe_ioctl(dev->fd, IOCTL_KGSL_GPU_COMMAND, &submit_req);

   if (ret) {
      err(1, "IOCTL_KGSL_GPU_COMMAND failure %d", ret);
   }

   struct kgsl_device_waittimestamp_ctxtid wait = {
      .context_id = dev->context_id,
      .timestamp = submit_req.timestamp,
      .timeout = 3000,
   };

   ret = safe_ioctl(dev->fd, IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID, &wait);

   if (ret) {
      err(1, "IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID failure %d", ret);
   }

   u_vector_finish(&dev->cmdstreams);
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));

   device_print_shader_log(dev);
   device_print_cp_log(dev);

   device_dump_wrbuf(dev);
   u_vector_finish(&dev->wrbufs);
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   device_free_buffers(dev);
}

static void
buffer_mem_alloc(struct device *dev, struct buffer *buf)
{
   bool success = util_vma_heap_alloc_addr(&dev->vma, buf->iova, buf->size);
   if (!success)
      errx(1, "Failed to allocate buffer");

   buf->map = ((uint8_t*)dev->va_map) + (buf->iova - dev->va_iova);
}

void
buffer_mem_free(struct device *dev, struct buffer *buf)
{
   util_vma_heap_free(&dev->vma, buf->iova, buf->size);
}
#else

static int
safe_ioctl(int fd, unsigned long request, void *arg)
{
   int ret;

   do {
      ret = ioctl(fd, request, arg);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   return ret;
}

struct alloc_priv_info {
   __u32 struct_size;
   char _pad0[4];
   __u32 unk0; // 1
   char _pad1[4];
   __u64 size;
   __u32 alignment;
   char _pad2[20];
   __u64 allocated_size;
   __u32 unk1;   // 1
   char _pad4[8]; /* offset: 60*/
   __u32 unk2;   // 61
   char _pad5[76];
   __u32 unk3; /* offset: 148 */ // 1
   char _pad6[8];
   __u32 unk4; /* offset: 160 */ // 1
   char _pad7[44];
   __u32 unk5; /* offset: 208 */ // 3
   char _pad8[16];
   __u32 size_2; /* offset: 228 */
   __u32 unk6;   // 1
   __u32 size_3;
   __u32 size_4;
   __u32 unk7; /* offset: 244 */ // 1
   char _pad9[56];
};
static_assert(sizeof(struct alloc_priv_info) == 304);
static_assert(offsetof(struct alloc_priv_info, unk1) == 56);
static_assert(offsetof(struct alloc_priv_info, unk3) == 148);
static_assert(offsetof(struct alloc_priv_info, unk5) == 208);

struct submit_priv_ib_info {
   char _pad5[4];
   __u32 size_dwords;
   __u64 iova;
   char _pad6[8];
} __attribute__((packed));

struct submit_priv_data {
   __u32 magic0;
   char _pad0[4];
   __u32 struct_size;
   char _pad1[4];
   /* It seems that priv data can have several sub-datas
    * cmdbuf is one of them, after it there is another 8 byte struct
    * without anything useful in it. That second data doesn't seem
    * important for replaying.
    */
   __u32 datas_count;
   char _pad2[32];
   struct {
      __u32 magic1;
      __u32 data_size;

      struct {
         __u32 unk1;
         __u32 cmdbuf_size;
         char _pad3[32];
         __u32 ib_count;
         char _pad4[36];

         struct submit_priv_ib_info ibs[];
      } cmdbuf;
   } data0;

   //    unsigned char magic2[8];
} __attribute__((packed));
static_assert(offsetof(struct submit_priv_data, data0) == 0x34);
static_assert(offsetof(struct submit_priv_data, data0.cmdbuf.ibs) == 0x8c);

static struct device *
device_create(uint64_t base_addr)
{
   struct device *dev = calloc(sizeof(struct device), 1);

   static const char path[] = "/dev/dxg";

   dev->fd = open(path, O_RDWR | O_CLOEXEC);
   if (dev->fd < 0) {
      errx(1, "Cannot open /dev/dxg fd");
   }

   struct d3dkmt_adapterinfo adapters[1];
   struct d3dkmt_enumadapters3 enum_adapters = {
      .adapter_count = 1,
      .adapters = adapters,
   };
   int ret = safe_ioctl(dev->fd, LX_DXENUMADAPTERS3, &enum_adapters);
   if (ret) {
      errx(1, "LX_DXENUMADAPTERS3 failure");
   }

   if (enum_adapters.adapter_count == 0) {
      errx(1, "No adapters found");
   }

   struct winluid adapter_luid = enum_adapters.adapters[0].adapter_luid;

   struct d3dkmt_openadapterfromluid open_adapter = {
      .adapter_luid = adapter_luid,
   };
   ret = safe_ioctl(dev->fd, LX_DXOPENADAPTERFROMLUID, &open_adapter);
   if (ret) {
      errx(1, "LX_DXOPENADAPTERFROMLUID failure");
   }

   struct d3dkmthandle adapter = open_adapter.adapter_handle;

   struct d3dkmt_createdevice create_device = {
      .adapter = adapter,
   };
   ret = safe_ioctl(dev->fd, LX_DXCREATEDEVICE, &create_device);
   if (ret) {
      errx(1, "LX_DXCREATEDEVICE failure");
   }

   struct d3dkmthandle device = create_device.device;
   dev->device = device;

   unsigned char create_context_priv_data[] = {
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x0c, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   };

   struct d3dkmt_createcontextvirtual create_context = {
      .device = device,
      .node_ordinal = 0,
      .engine_affinity = 1,
      .priv_drv_data = create_context_priv_data,
      .priv_drv_data_size = sizeof(create_context_priv_data),
      .client_hint = 16,
   };
   ret = safe_ioctl(dev->fd, LX_DXCREATECONTEXTVIRTUAL, &create_context);
   if (ret) {
      errx(1, "LX_DXCREATECONTEXTVIRTUAL failure");
   }

   dev->context = create_context.context;

   struct d3dkmt_createpagingqueue create_paging_queue = {
      .device = device,
      .priority = _D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL,
      .physical_adapter_index = 0,
   };
   ret = safe_ioctl(dev->fd, LX_DXCREATEPAGINGQUEUE, &create_paging_queue);
   if (ret) {
      errx(1, "LX_DXCREATEPAGINGQUEUE failure");
   }
   struct d3dkmthandle paging_queue = create_paging_queue.paging_queue;


   uint32_t alloc_size = FAKE_ADDRESS_SPACE_SIZE;
   struct alloc_priv_info priv_alloc_info = {
      .struct_size = sizeof(struct alloc_priv_info),
      .unk0 = 1,
      .size = alloc_size,
      .alignment = 4096,
      .unk1 = 1,
      .unk2 = 61,
      .unk3 = 1,
      .unk4 = 1,
      .unk5 = 3,
      .size_2 = alloc_size,
      .unk6 = 1,
      .size_3 = alloc_size,
      .size_4 = alloc_size,
      .unk7 = 1,
   };

   struct d3dddi_allocationinfo2 alloc_info = {
      .priv_drv_data = &priv_alloc_info,
      .priv_drv_data_size = sizeof(struct alloc_priv_info),
   };

   struct d3dkmt_createallocation create_allocation = {
      .device = device,
      .alloc_count = 1,
      .allocation_info = &alloc_info,
   };
   ret = safe_ioctl(dev->fd, LX_DXCREATEALLOCATION, &create_allocation);
   if (ret) {
      errx(1, "LX_DXCREATEALLOCATION failure");
   }

   assert(priv_alloc_info.allocated_size == alloc_size);

   struct d3dddi_mapgpuvirtualaddress map_virtual_address = {
      .paging_queue = paging_queue,
      .base_address = base_addr,
      .maximum_address = 18446744073709551615ull,
      .allocation = create_allocation.allocation_info[0].allocation,
      .size_in_pages = MAX2(alloc_size / 4096, 1),
      .protection = {
         .write = 1,
         .execute = 1,
      },
   };
   ret = safe_ioctl(dev->fd, LX_DXMAPGPUVIRTUALADDRESS, &map_virtual_address);
   if (ret != 259) {
      errx(1, "LX_DXMAPGPUVIRTUALADDRESS failure");
   }

   __u32 priority = 0;
   struct d3dddi_makeresident make_resident = {
      .paging_queue = paging_queue,
      .alloc_count = 1,
      .allocation_list = &create_allocation.allocation_info[0].allocation,
      .priority_list = &priority,
   };
   ret = safe_ioctl(dev->fd, LX_DXMAKERESIDENT, &make_resident);
   if (ret != 259) {
      errx(1, "LX_DXMAKERESIDENT failure");
   }

   struct d3dkmt_lock2 lock = {
      .device = device,
      .allocation = create_allocation.allocation_info[0].allocation,
   };
   ret = safe_ioctl(dev->fd, LX_DXLOCK2, &lock);
   if (ret) {
      errx(1, "LX_DXLOCK2 failure");
   }

   dev->va_iova = map_virtual_address.virtual_address;
   dev->va_map = lock.data;

   rb_tree_init(&dev->buffers);
   util_vma_heap_init(&dev->vma, dev->va_iova, ROUND_DOWN_TO(alloc_size, 4096));
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   printf("Allocated iova at 0x%" PRIx64 "\n", dev->va_iova);

   uint64_t hole_size = 4096;
   dev->vma.alloc_high = true;
   dev->fence_iova = util_vma_heap_alloc(&dev->vma, hole_size, 4096);
   dev->fence_ib_iova = dev->fence_iova + 8;
   dev->fence = (uint32_t *) ((uint8_t*)dev->va_map + (dev->fence_iova - dev->va_iova));
   dev->fence_ib = (uint32_t *) ((uint8_t*)dev->va_map + (dev->fence_ib_iova - dev->va_iova));
   dev->vma.alloc_high = false;

   return dev;
}

static void
device_submit_cmdstreams(struct device *dev)
{
   if (!u_vector_length(&dev->cmdstreams)) {
      device_free_buffers(dev);
      return;
   }

   uint32_t cmdstream_count = u_vector_length(&dev->cmdstreams) + 1;

   uint32_t priv_data_size =
      sizeof(struct submit_priv_data) +
      cmdstream_count * sizeof(struct submit_priv_ib_info);

   struct submit_priv_data *priv_data = calloc(1, priv_data_size);
   priv_data->magic0 = 0xccaabbee;
   priv_data->struct_size = priv_data_size;
   priv_data->datas_count = 1;

   priv_data->data0.magic1 = 0xfadcab02;
   priv_data->data0.data_size =
      sizeof(priv_data->data0) +
      cmdstream_count * sizeof(struct submit_priv_ib_info);
   priv_data->data0.cmdbuf.unk1 = 0xcccc0001;
   priv_data->data0.cmdbuf.cmdbuf_size = sizeof(priv_data->data0.cmdbuf) +
      cmdstream_count * sizeof(struct submit_priv_ib_info);
   priv_data->data0.cmdbuf.ib_count = cmdstream_count;

   struct cmdstream *cmd;
   uint32_t idx = 0;
   u_vector_foreach(cmd, &dev->cmdstreams) {
      priv_data->data0.cmdbuf.ibs[idx].size_dwords = cmd->size / 4;
      priv_data->data0.cmdbuf.ibs[idx].iova = cmd->iova;
      idx++;
   }

   priv_data->data0.cmdbuf.ibs[idx].size_dwords = 4;
   priv_data->data0.cmdbuf.ibs[idx].iova = dev->fence_ib_iova;

   *dev->fence = 0x00000000;
   dev->fence_ib[0] = pm4_pkt7_hdr(0x3d, 3); // CP_MEM_WRITE
   dev->fence_ib[1] = dev->fence_iova;
   dev->fence_ib[2] = dev->fence_iova >> 32;
   dev->fence_ib[3] = 0xababfcfc;

   // Fill second (empty) data block
   // uint32_t *magic_end = (uint32_t *)(((char *) priv_data) + priv_data_size - 8);
   // magic_end[0] = 0xfadcab00;
   // magic_end[1] = 0x00000008;

   struct d3dkmt_submitcommand submission = {
      .command_buffer = priv_data->data0.cmdbuf.ibs[0].iova,
      .command_length = priv_data->data0.cmdbuf.ibs[0].size_dwords * sizeof(uint32_t),
      .broadcast_context_count = 1,
      .broadcast_context[0] = dev->context,
      .priv_drv_data_size = priv_data_size,
      .priv_drv_data = priv_data,
   };

   int ret = safe_ioctl(dev->fd, LX_DXSUBMITCOMMAND, &submission);
   if (ret) {
      errx(1, "LX_DXSUBMITCOMMAND failure");
   }

   free(priv_data);

   u_vector_finish(&dev->cmdstreams);
   u_vector_init(&dev->cmdstreams, 8, sizeof(struct cmdstream));

   // TODO: better way to wait
   for (unsigned i = 0; i < 1000; i++) {
      usleep(1000);
      if (*dev->fence != 0)
         break;
   }
   if (*dev->fence == 0) {
      errx(1, "Waiting for submission failed! GPU faulted or kernel did not execute this submission.");
   }

   device_print_shader_log(dev);
   device_print_cp_log(dev);

   device_dump_wrbuf(dev);
   u_vector_finish(&dev->wrbufs);
   u_vector_init(&dev->wrbufs, 8, sizeof(struct wrbuf));

   device_free_buffers(dev);
}

static void
buffer_mem_alloc(struct device *dev, struct buffer *buf)
{
   bool success = util_vma_heap_alloc_addr(&dev->vma, buf->iova, buf->size);
   if (!success)
      errx(1, "Failed to allocate buffer");

   buf->map = ((uint8_t*)dev->va_map) + (buf->iova - dev->va_iova);
}

void
buffer_mem_free(struct device *dev, struct buffer *buf)
{
   util_vma_heap_free(&dev->vma, buf->iova, buf->size);
}

#endif

static void
upload_buffer(struct device *dev, uint64_t iova, unsigned int size,
              void *hostptr)
{
   struct buffer *buf = device_get_buffer(dev, iova);

   if (!buf) {
      buf = calloc(sizeof(struct buffer), 1);
      buf->iova = iova;
      buf->size = size;

      rb_tree_insert(&dev->buffers, &buf->node, rb_buffer_insert_cmp);

      buffer_mem_alloc(dev, buf);
   } else if (buf->size != size) {
      buffer_mem_free(dev, buf);
      buf->size = size;
      buffer_mem_alloc(dev, buf);
   }

   memcpy(buf->map, hostptr, size);

   buf->used = true;
}

static int
override_cmdstream(struct device *dev, struct cmdstream *cs,
                   const char *cmdstreamgen)
{
#if FD_REPLAY_KGSL
   static const char *tmpfilename = "/sdcard/Download/cmdstream_override.rd";
#elif FD_REPLAY_MSM || FD_REPLAY_WSL
   static const char *tmpfilename = "/tmp/cmdstream_override.rd";
#endif


   /* Find a free space for the new cmdstreams and resources we will use
    * when overriding existing cmdstream.
    */
   uint64_t hole_size = util_vma_heap_get_max_free_continuous_size(&dev->vma);
   uint64_t hole_iova = util_vma_heap_alloc(&dev->vma, hole_size, 1);
   util_vma_heap_free(&dev->vma, hole_iova, hole_size);

   char cmd[2048];
   snprintf(cmd, sizeof(cmd),
            "%s --vastart=%" PRIu64 " --vasize=%" PRIu64 " %s", cmdstreamgen,
            hole_iova, hole_size, tmpfilename);

   printf("generating cmdstream '%s'\n", cmd);

   int ret = system(cmd);
   if (ret) {
      fprintf(stderr, "Error executing %s\n", cmd);
      return -1;
   }

   struct io *io;
   struct rd_parsed_section ps = {0};

   io = io_open(tmpfilename);
   if (!io) {
      fprintf(stderr, "could not open: %s\n", tmpfilename);
      return -1;
   }

   struct {
      unsigned int len;
      uint64_t gpuaddr;
   } gpuaddr = {0};

   while (parse_rd_section(io, &ps)) {
      switch (ps.type) {
      case RD_GPUADDR:
         parse_addr(ps.buf, ps.sz, &gpuaddr.len, &gpuaddr.gpuaddr);
         /* no-op */
         break;
      case RD_BUFFER_CONTENTS:
         upload_buffer(dev, gpuaddr.gpuaddr, gpuaddr.len, ps.buf);
         ps.buf = NULL;
         break;
      case RD_CMDSTREAM_ADDR: {
         unsigned int sizedwords;
         uint64_t gpuaddr;
         parse_addr(ps.buf, ps.sz, &sizedwords, &gpuaddr);
         printf("override cmdstream: %d dwords\n", sizedwords);

         cs->iova = gpuaddr;
         cs->size = sizedwords * sizeof(uint32_t);
         break;
      }
      case RD_SHADER_LOG_BUFFER: {
         unsigned int sizedwords;
         parse_addr(ps.buf, ps.sz, &sizedwords, &dev->shader_log_iova);
         break;
      }
      case RD_CP_LOG_BUFFER: {
         unsigned int sizedwords;
         parse_addr(ps.buf, ps.sz, &sizedwords, &dev->cp_log_iova);
         break;
      }
      case RD_WRBUFFER: {
         struct wrbuf *wrbuf = u_vector_add(&dev->wrbufs);
         uint64_t *p = (uint64_t *)ps.buf;
         wrbuf->iova = p[0];
         wrbuf->size = p[1];
         bool clear = p[2];
         int name_len = ps.sz - (3 * sizeof(uint64_t));
         wrbuf->name = calloc(sizeof(char), name_len);
         memcpy(wrbuf->name, (char*)(p + 3), name_len); // includes null terminator

         if (clear) {
            struct buffer *buf = device_get_buffer(dev, wrbuf->iova);
            assert(buf);

            uint64_t offset = wrbuf->iova - buf->iova;
            uint64_t end = MIN2(offset + wrbuf->size, buf->size);
            while (offset < end) {
               static const uint64_t clear_value = 0xdeadbeefdeadbeef;
               memcpy(buf->map + offset, &clear_value,
                      MIN2(sizeof(clear_value), end - offset));
               offset += sizeof(clear_value);
            }
         }

         break;
      }
      default:
         break;
      }
   }

   io_close(io);
   if (ps.ret < 0) {
      fprintf(stderr, "corrupt file %s\n", tmpfilename);
   }

   return ps.ret;
}

static int
handle_file(const char *filename, uint32_t first_submit, uint32_t last_submit,
            uint32_t submit_to_override, uint64_t base_addr, const char *cmdstreamgen)
{
   struct io *io;
   int submit = 0;
   bool skip = false;
   bool need_submit = false;
   struct rd_parsed_section ps = {0};

   printf("Reading %s...\n", filename);

   if (!strcmp(filename, "-"))
      io = io_openfd(0);
   else
      io = io_open(filename);

   if (!io) {
      fprintf(stderr, "could not open: %s\n", filename);
      return -1;
   }

   struct device *dev = device_create(base_addr);

   struct {
      unsigned int len;
      uint64_t gpuaddr;
   } gpuaddr = {0};

   while (parse_rd_section(io, &ps)) {
      switch (ps.type) {
      case RD_TEST:
      case RD_VERT_SHADER:
      case RD_FRAG_SHADER:
         /* no-op */
         break;
      case RD_CMD:
         skip = false;
         if (exename) {
            skip |= (strstr(ps.buf, exename) != ps.buf);
         } else {
            skip |= (strstr(ps.buf, "fdperf") == ps.buf);
            skip |= (strstr(ps.buf, "chrome") == ps.buf);
            skip |= (strstr(ps.buf, "surfaceflinger") == ps.buf);
            skip |= ((char *)ps.buf)[0] == 'X';
         }
         break;

      case RD_GPUADDR:
         if (need_submit) {
            need_submit = false;
            device_submit_cmdstreams(dev);
         }

         parse_addr(ps.buf, ps.sz, &gpuaddr.len, &gpuaddr.gpuaddr);
         /* no-op */
         break;
      case RD_BUFFER_CONTENTS:
         /* TODO: skip buffer uploading and even reading if this buffer
          * is used for submit outside of [first_submit, last_submit]
          * range. A set of buffers is shared between several cmdstreams,
          * so we'd have to find starting from which RD_CMD to upload
          * the buffers.
          */
         upload_buffer(dev, gpuaddr.gpuaddr, gpuaddr.len, ps.buf);
         break;
      case RD_CMDSTREAM_ADDR: {
         unsigned int sizedwords;
         uint64_t gpuaddr;
         parse_addr(ps.buf, ps.sz, &sizedwords, &gpuaddr);

         bool add_submit = !skip && (submit >= first_submit) && (submit <= last_submit);
         printf("%scmdstream %d: %d dwords\n", add_submit ? "" : "skipped ",
                submit, sizedwords);

         if (add_submit) {
            struct cmdstream *cs = u_vector_add(&dev->cmdstreams);

            if (submit == submit_to_override) {
               if (override_cmdstream(dev, cs, cmdstreamgen) < 0)
                  break;
            } else {
               cs->iova = gpuaddr;
               cs->size = sizedwords * sizeof(uint32_t);
            }
         }

         need_submit = true;

         submit++;
         break;
      }
      case RD_GPU_ID: {
         uint32_t gpu_id = parse_gpu_id(ps.buf);
         if (gpu_id)
            printf("gpuid: %d\n", gpu_id);
         break;
      }
      case RD_CHIP_ID: {
         uint64_t chip_id = parse_chip_id(ps.buf);
         printf("chip_id: 0x%" PRIx64 "\n", chip_id);
         break;
      }
      default:
         break;
      }
   }

   if (need_submit)
      device_submit_cmdstreams(dev);

   close(dev->fd);

   io_close(io);
   fflush(stdout);

   if (ps.ret < 0) {
      printf("corrupt file\n");
   }
   return 0;
}
