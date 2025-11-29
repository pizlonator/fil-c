/*
 * Copyright © 2024 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <perfetto.h>

#include "freedreno_drm_perfetto.h"
#include "freedreno_drmif.h"

#include "util/log.h"
#include "util/perf/u_perfetto.h"
#include "util/simple_mtx.h"

class FdMemoryDataSource : public perfetto::DataSource<FdMemoryDataSource> {
 public:
   void OnSetup(const SetupArgs &) override
   {
   }

   void OnStart(const StartArgs &) override
   {
      PERFETTO_LOG("Memory tracing started");
   }

   void OnStop(const StopArgs &) override
   {
      PERFETTO_LOG("Memory tracing stopped");
   }
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(FdMemoryDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(FdMemoryDataSource);

extern "C" void
fd_drm_perfetto_init(void)
{
   util_perfetto_init();

   perfetto::DataSourceDescriptor dsd;
   dsd.set_name("gpu.memory.msm");
   FdMemoryDataSource::Register(dsd);
}

extern "C" void
fd_alloc_log(struct fd_bo *bo, enum fd_alloc_category from, enum fd_alloc_category to)
{
   /* Special case for BOs that back heap chunks, they don't immediately
    * transition to active, despite what the caller thinks:
    */
   if (bo->alloc_flags & _FD_BO_HINT_HEAP) {
      if (to == FD_ALLOC_ACTIVE) {
         to = FD_ALLOC_HEAP;
      } else if (from == FD_ALLOC_ACTIVE) {
         from = FD_ALLOC_HEAP;
      }
   }

#define MEMORY_DEBUGGING 0
   if (MEMORY_DEBUGGING) {
      static simple_mtx_t lock = SIMPLE_MTX_INITIALIZER;

      assert(bo->size);

      simple_mtx_lock(&lock);

      static uint32_t sizes[4];
      static uint32_t size_buffer, size_image, size_command, size_internal, *size_cat;

      if (from != FD_ALLOC_NONE) {
         assert(sizes[from] >= bo->size);
         sizes[from] -= bo->size;
      }

      if (to != FD_ALLOC_NONE) {
         sizes[to] += bo->size;
      }

      if (bo->alloc_flags & FD_BO_HINT_BUFFER) {
         size_cat = &size_buffer;
      } else if (bo->alloc_flags & FD_BO_HINT_IMAGE) {
         size_cat = &size_image;
      } else if (bo->alloc_flags & FD_BO_HINT_COMMAND) {
         size_cat = &size_command;
      } else {
         size_cat = &size_internal;
      }
      if (to == FD_ALLOC_ACTIVE) {
         *size_cat += bo->size;
      } else if (from == FD_ALLOC_ACTIVE) {
         assert(*size_cat >= bo->size);
         *size_cat -= bo->size;
      }

      static time_t last_time;
      struct timespec time;

      clock_gettime(CLOCK_MONOTONIC, &time);

      if (last_time != time.tv_sec) {
         mesa_logi("active=%'u, heap=%'u, cache=%'u, buffer=%'u, image=%'u, command=%'u, internal=%'u",
                   sizes[FD_ALLOC_ACTIVE], sizes[FD_ALLOC_HEAP], sizes[FD_ALLOC_CACHE],
                   size_buffer, size_image, size_command, size_internal);
         last_time = time.tv_sec;
      }

      simple_mtx_unlock(&lock);
   }

   if ((to != FD_ALLOC_ACTIVE) && (from != FD_ALLOC_ACTIVE))
      return;

   FdMemoryDataSource::Trace([=](FdMemoryDataSource::TraceContext tctx) {
      auto packet = tctx.NewTracePacket();

      packet->set_timestamp(perfetto::base::GetBootTimeNs().count());

      auto event = packet->set_vulkan_memory_event();

      event->set_timestamp(perfetto::base::GetBootTimeNs().count());
      event->set_memory_size(bo->size);
      event->set_memory_address(bo->iova);
      event->set_allocation_scope(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::SCOPE_COMMAND);
      event->set_pid(getpid());

      if (bo->alloc_flags & FD_BO_HINT_BUFFER) {
         event->set_source(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::SOURCE_BUFFER);
         event->set_memory_type(1);
      } else if (bo->alloc_flags & FD_BO_HINT_IMAGE) {
         event->set_source(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::SOURCE_IMAGE);
         event->set_memory_type(2);
      } else {
         event->set_source(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::SOURCE_DRIVER);
         event->set_memory_type(3);
      }

      if (bo->alloc_flags & (FD_BO_HINT_BUFFER | FD_BO_HINT_IMAGE)) {
         /* For IMAGE/BUFFER, the trace processor is looking for BIND/DESTROY_BOUND: */
         if (to == FD_ALLOC_ACTIVE) {
            event->set_operation(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::OP_BIND);
         } else {
            event->set_operation(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::OP_DESTROY_BOUND);
         }
      } else {
         /* For SOURCE_DRIVER, the relevant ops are CREATE/DESTROY */
         if (to == FD_ALLOC_ACTIVE) {
            event->set_operation(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::OP_CREATE);
         } else {
            event->set_operation(perfetto::protos::pbzero::perfetto_pbzero_enum_VulkanMemoryEvent::OP_DESTROY);
         }
      }
   });
}
