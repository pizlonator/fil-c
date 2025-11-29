/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/utsname.h>
#endif
#include <sys/stat.h>

#include "util/mesa-sha1.h"
#include "util/os_time.h"
#include "ac_debug.h"
#include "radv_buffer.h"
#include "radv_debug.h"
#include "radv_descriptor_set.h"
#include "radv_entrypoints.h"
#include "radv_pipeline_graphics.h"
#include "radv_pipeline_rt.h"
#include "radv_shader.h"
#include "sid.h"

#define TRACE_BO_SIZE 4096
#define TMA_BO_SIZE   4096

#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[31m"
#define COLOR_GREEN  "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_CYAN   "\033[1;36m"

#define RADV_DUMP_DIR "radv_dumps"

/* Trace BO layout (offsets are 4 bytes):
 *
 * [0]: primary trace ID
 * [1]: secondary trace ID
 * [2-3]: 64-bit GFX ring pipeline pointer
 * [4-5]: 64-bit COMPUTE ring pipeline pointer
 * [6-7]: Vertex descriptors pointer
 * [8-9]: 64-bit Vertex prolog pointer
 * [10-11]: 64-bit descriptor set #0 pointer
 * ...
 * [72-73]: 64-bit descriptor set #31 pointer
 */

bool
radv_init_trace(struct radv_device *device)
{
   struct radeon_winsys *ws = device->ws;
   VkResult result;

   result = radv_bo_create(
      device, NULL, TRACE_BO_SIZE, 8, RADEON_DOMAIN_VRAM,
      RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_NO_INTERPROCESS_SHARING | RADEON_FLAG_ZERO_VRAM | RADEON_FLAG_VA_UNCACHED,
      RADV_BO_PRIORITY_UPLOAD_BUFFER, 0, true, &device->trace_bo);
   if (result != VK_SUCCESS)
      return false;

   result = ws->buffer_make_resident(ws, device->trace_bo, true);
   if (result != VK_SUCCESS)
      return false;

   device->trace_id_ptr = radv_buffer_map(ws, device->trace_bo);
   if (!device->trace_id_ptr)
      return false;

   return true;
}

void
radv_finish_trace(struct radv_device *device)
{
   struct radeon_winsys *ws = device->ws;

   if (unlikely(device->trace_bo)) {
      ws->buffer_make_resident(ws, device->trace_bo, false);
      radv_bo_destroy(device, NULL, device->trace_bo);
   }
}

static void
radv_dump_trace(const struct radv_device *device, struct radeon_cmdbuf *cs, FILE *f)
{
   fprintf(f, "Trace ID: %x\n", *device->trace_id_ptr);
   device->ws->cs_dump(cs, f, (const int *)device->trace_id_ptr, 2, RADV_CS_DUMP_TYPE_IBS);
}

static void
radv_dump_mmapped_reg(const struct radv_device *device, FILE *f, unsigned offset)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_winsys *ws = device->ws;
   uint32_t value;

   if (ws->read_registers(ws, offset, 1, &value))
      ac_dump_reg(f, pdev->info.gfx_level, pdev->info.family, offset, value, ~0);
}

static void
radv_dump_debug_registers(const struct radv_device *device, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radeon_info *gpu_info = &pdev->info;

   fprintf(f, "Memory-mapped registers:\n");
   radv_dump_mmapped_reg(device, f, R_008010_GRBM_STATUS);

   radv_dump_mmapped_reg(device, f, R_008008_GRBM_STATUS2);
   radv_dump_mmapped_reg(device, f, R_008014_GRBM_STATUS_SE0);
   radv_dump_mmapped_reg(device, f, R_008018_GRBM_STATUS_SE1);
   radv_dump_mmapped_reg(device, f, R_008038_GRBM_STATUS_SE2);
   radv_dump_mmapped_reg(device, f, R_00803C_GRBM_STATUS_SE3);
   radv_dump_mmapped_reg(device, f, R_00D034_SDMA0_STATUS_REG);
   radv_dump_mmapped_reg(device, f, R_00D834_SDMA1_STATUS_REG);
   if (gpu_info->gfx_level <= GFX8) {
      radv_dump_mmapped_reg(device, f, R_000E50_SRBM_STATUS);
      radv_dump_mmapped_reg(device, f, R_000E4C_SRBM_STATUS2);
      radv_dump_mmapped_reg(device, f, R_000E54_SRBM_STATUS3);
   }
   radv_dump_mmapped_reg(device, f, R_008680_CP_STAT);
   radv_dump_mmapped_reg(device, f, R_008674_CP_STALLED_STAT1);
   radv_dump_mmapped_reg(device, f, R_008678_CP_STALLED_STAT2);
   radv_dump_mmapped_reg(device, f, R_008670_CP_STALLED_STAT3);
   radv_dump_mmapped_reg(device, f, R_008210_CP_CPC_STATUS);
   radv_dump_mmapped_reg(device, f, R_008214_CP_CPC_BUSY_STAT);
   radv_dump_mmapped_reg(device, f, R_008218_CP_CPC_STALLED_STAT1);
   radv_dump_mmapped_reg(device, f, R_00821C_CP_CPF_STATUS);
   radv_dump_mmapped_reg(device, f, R_008220_CP_CPF_BUSY_STAT);
   radv_dump_mmapped_reg(device, f, R_008224_CP_CPF_STALLED_STAT1);
   fprintf(f, "\n");
}

static void
radv_dump_buffer_descriptor(enum amd_gfx_level gfx_level, enum radeon_family family, const uint32_t *desc, FILE *f)
{
   fprintf(f, COLOR_CYAN "Buffer:" COLOR_RESET "\n");
   for (unsigned j = 0; j < 4; j++)
      ac_dump_reg(f, gfx_level, family, R_008F00_SQ_BUF_RSRC_WORD0 + j * 4, desc[j], 0xffffffff);
}

static void
radv_dump_image_descriptor(enum amd_gfx_level gfx_level, enum radeon_family family, const uint32_t *desc, FILE *f)
{
   unsigned sq_img_rsrc_word0 = gfx_level >= GFX10 ? R_00A000_SQ_IMG_RSRC_WORD0 : R_008F10_SQ_IMG_RSRC_WORD0;

   fprintf(f, COLOR_CYAN "Image:" COLOR_RESET "\n");
   for (unsigned j = 0; j < 8; j++)
      ac_dump_reg(f, gfx_level, family, sq_img_rsrc_word0 + j * 4, desc[j], 0xffffffff);

   fprintf(f, COLOR_CYAN "    FMASK:" COLOR_RESET "\n");
   for (unsigned j = 0; j < 8; j++)
      ac_dump_reg(f, gfx_level, family, sq_img_rsrc_word0 + j * 4, desc[8 + j], 0xffffffff);
}

static void
radv_dump_sampler_descriptor(enum amd_gfx_level gfx_level, enum radeon_family family, const uint32_t *desc, FILE *f)
{
   fprintf(f, COLOR_CYAN "Sampler state:" COLOR_RESET "\n");
   for (unsigned j = 0; j < 4; j++) {
      ac_dump_reg(f, gfx_level, family, R_008F30_SQ_IMG_SAMP_WORD0 + j * 4, desc[j], 0xffffffff);
   }
}

static void
radv_dump_combined_image_sampler_descriptor(enum amd_gfx_level gfx_level, enum radeon_family family,
                                            const uint32_t *desc, FILE *f)
{
   radv_dump_image_descriptor(gfx_level, family, desc, f);
   radv_dump_sampler_descriptor(gfx_level, family, desc + 16, f);
}

static void
radv_dump_descriptor_set(const struct radv_device *device, const struct radv_descriptor_set *set, unsigned id, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   enum amd_gfx_level gfx_level = pdev->info.gfx_level;
   enum radeon_family family = pdev->info.family;
   const struct radv_descriptor_set_layout *layout;
   int i;

   if (!set)
      return;
   layout = set->header.layout;

   for (i = 0; i < set->header.layout->binding_count; i++) {
      uint32_t *desc = set->header.mapped_ptr + layout->binding[i].offset / 4;

      fprintf(f, "(set=%u binding=%u offset=0x%x) ", id, i, layout->binding[i].offset);

      switch (layout->binding[i].type) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         radv_dump_buffer_descriptor(gfx_level, family, desc, f);
         break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         radv_dump_image_descriptor(gfx_level, family, desc, f);
         break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         radv_dump_combined_image_sampler_descriptor(gfx_level, family, desc, f);
         break;
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         radv_dump_sampler_descriptor(gfx_level, family, desc, f);
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
      case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
         /* todo */
         break;
      default:
         assert(!"unknown descriptor type");
         break;
      }
      fprintf(f, "\n");
   }
   fprintf(f, "\n\n");
}

static void
radv_dump_descriptors(struct radv_device *device, FILE *f)
{
   uint64_t *ptr = (uint64_t *)device->trace_id_ptr;
   int i;

   fprintf(f, "Descriptors:\n");
   for (i = 0; i < MAX_SETS; i++) {
      struct radv_descriptor_set *set = *(struct radv_descriptor_set **)(ptr + i + 5);

      radv_dump_descriptor_set(device, set, i, f);
   }
}

struct radv_shader_inst {
   char text[160];  /* one disasm line */
   unsigned offset; /* instruction offset */
   unsigned size;   /* instruction size = 4 or 8 */
};

/* Split a disassembly string into lines and add them to the array pointed
 * to by "instructions". */
static void
radv_add_split_disasm(const char *disasm, uint64_t start_addr, unsigned *num, struct radv_shader_inst *instructions)
{
   struct radv_shader_inst *last_inst = *num ? &instructions[*num - 1] : NULL;
   char *next;

   while ((next = strchr(disasm, '\n'))) {
      struct radv_shader_inst *inst = &instructions[*num];
      unsigned len = next - disasm;

      if (!memchr(disasm, ';', len)) {
         /* Ignore everything that is not an instruction. */
         disasm = next + 1;
         continue;
      }

      assert(len < ARRAY_SIZE(inst->text));
      memcpy(inst->text, disasm, len);
      inst->text[len] = 0;
      inst->offset = last_inst ? last_inst->offset + last_inst->size : 0;

      const char *semicolon = strchr(disasm, ';');
      assert(semicolon);
      /* More than 16 chars after ";" means the instruction is 8 bytes long. */
      inst->size = next - semicolon > 16 ? 8 : 4;

      snprintf(inst->text + len, ARRAY_SIZE(inst->text) - len, " [PC=0x%" PRIx64 ", off=%u, size=%u]",
               start_addr + inst->offset, inst->offset, inst->size);

      last_inst = inst;
      (*num)++;
      disasm = next + 1;
   }
}

static void
radv_dump_annotated_shader(const struct radv_shader *shader, gl_shader_stage stage, struct ac_wave_info *waves,
                           unsigned num_waves, FILE *f)
{
   uint64_t start_addr, end_addr;
   unsigned i;

   if (!shader)
      return;

   start_addr = radv_shader_get_va(shader) & ((1ull << 48) - 1);
   end_addr = start_addr + shader->code_size;

   /* See if any wave executes the shader. */
   for (i = 0; i < num_waves; i++) {
      if (start_addr <= waves[i].pc && waves[i].pc <= end_addr)
         break;
   }

   if (i == num_waves)
      return; /* the shader is not being executed */

   /* Remember the first found wave. The waves are sorted according to PC. */
   waves = &waves[i];
   num_waves -= i;

   /* Get the list of instructions.
    * Buffer size / 4 is the upper bound of the instruction count.
    */
   unsigned num_inst = 0;
   struct radv_shader_inst *instructions = calloc(shader->code_size / 4, sizeof(struct radv_shader_inst));

   radv_add_split_disasm(shader->disasm_string, start_addr, &num_inst, instructions);

   fprintf(f, COLOR_YELLOW "%s - annotated disassembly:" COLOR_RESET "\n", radv_get_shader_name(&shader->info, stage));

   /* Print instructions with annotations. */
   for (i = 0; i < num_inst; i++) {
      struct radv_shader_inst *inst = &instructions[i];

      fprintf(f, "%s\n", inst->text);

      /* Print which waves execute the instruction right now. */
      while (num_waves && start_addr + inst->offset == waves->pc) {
         fprintf(f,
                 "          " COLOR_GREEN "^ SE%u SH%u CU%u "
                 "SIMD%u WAVE%u  EXEC=%016" PRIx64 "  ",
                 waves->se, waves->sh, waves->cu, waves->simd, waves->wave, waves->exec);

         if (inst->size == 4) {
            fprintf(f, "INST32=%08X" COLOR_RESET "\n", waves->inst_dw0);
         } else {
            fprintf(f, "INST64=%08X %08X" COLOR_RESET "\n", waves->inst_dw0, waves->inst_dw1);
         }

         waves->matched = true;
         waves = &waves[1];
         num_waves--;
      }
   }

   fprintf(f, "\n\n");
   free(instructions);
}

static void
radv_dump_spirv(const struct radv_shader *shader, const char *sha1, const char *dump_dir)
{
   char dump_path[512];
   FILE *f;

   snprintf(dump_path, sizeof(dump_path), "%s/%s.spv", dump_dir, sha1);

   f = fopen(dump_path, "w+");
   if (f) {
      fwrite(shader->spirv, shader->spirv_size, 1, f);
      fclose(f);
   }
}

static void
radv_dump_shader(struct radv_device *device, struct radv_pipeline *pipeline, struct radv_shader *shader,
                 gl_shader_stage stage, const char *dump_dir, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (!shader)
      return;

   fprintf(f, "%s:\n\n", radv_get_shader_name(&shader->info, stage));

   if (shader->spirv) {
      unsigned char sha1[21];
      char sha1buf[41];

      _mesa_sha1_compute(shader->spirv, shader->spirv_size, sha1);
      _mesa_sha1_format(sha1buf, sha1);

      if (device->vk.enabled_features.deviceFaultVendorBinary) {
         radv_print_spirv(shader->spirv, shader->spirv_size, f);
      } else {
         fprintf(f, "SPIRV (see %s.spv)\n\n", sha1buf);
         radv_dump_spirv(shader, sha1buf, dump_dir);
      }
   }

   if (shader->nir_string) {
      fprintf(f, "NIR:\n%s\n", shader->nir_string);
   }

   fprintf(f, "%s IR:\n%s\n", pdev->use_llvm ? "LLVM" : "ACO", shader->ir_string);
   fprintf(f, "DISASM:\n%s\n", shader->disasm_string);

   radv_dump_shader_stats(device, pipeline, shader, stage, f);
}

static void
radv_dump_vertex_descriptors(const struct radv_device *device, const struct radv_graphics_pipeline *pipeline, FILE *f)
{
   struct radv_shader *vs = radv_get_shader(pipeline->base.shaders, MESA_SHADER_VERTEX);
   uint64_t *ptr = (uint64_t *)device->trace_id_ptr;
   uint32_t count = util_bitcount(vs->info.vs.vb_desc_usage_mask);
   uint32_t *vb_ptr = *(uint32_t **)(ptr + 3);

   if (!count)
      return;

   fprintf(f, "Num vertex %s: %d\n", vs->info.vs.use_per_attribute_vb_descs ? "attributes" : "bindings", count);
   for (uint32_t i = 0; i < count; i++) {
      uint32_t *desc = &((uint32_t *)vb_ptr)[i * 4];
      uint64_t va = 0;

      va |= desc[0];
      va |= (uint64_t)G_008F04_BASE_ADDRESS_HI(desc[1]) << 32;

      fprintf(f, "VBO#%d:\n", i);
      fprintf(f, "\tVA: 0x%" PRIx64 "\n", va);
      fprintf(f, "\tStride: %d\n", G_008F04_STRIDE(desc[1]));
      fprintf(f, "\tNum records: %d (0x%x)\n", desc[2], desc[2]);
   }
}

static struct radv_shader_part *
radv_get_saved_vs_prolog(const struct radv_device *device)
{
   uint64_t *ptr = (uint64_t *)device->trace_id_ptr;
   return *(struct radv_shader_part **)(ptr + 4);
}

static void
radv_dump_vs_prolog(const struct radv_device *device, const struct radv_graphics_pipeline *pipeline, FILE *f)
{
   struct radv_shader_part *vs_prolog = radv_get_saved_vs_prolog(device);
   struct radv_shader *vs_shader = radv_get_shader(pipeline->base.shaders, MESA_SHADER_VERTEX);

   if (!vs_prolog || !vs_shader || !vs_shader->info.vs.has_prolog)
      return;

   fprintf(f, "Vertex prolog:\n\n");
   fprintf(f, "DISASM:\n%s\n", vs_prolog->disasm_string);
}

static struct radv_pipeline *
radv_get_saved_pipeline(struct radv_device *device, enum amd_ip_type ring)
{
   uint64_t *ptr = (uint64_t *)device->trace_id_ptr;
   int offset = ring == AMD_IP_GFX ? 1 : 2;

   return *(struct radv_pipeline **)(ptr + offset);
}

static void
radv_dump_queue_state(struct radv_queue *queue, const char *dump_dir, FILE *f)
{
   struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   enum amd_ip_type ring = radv_queue_ring(queue);
   struct radv_pipeline *pipeline;

   fprintf(f, "AMD_IP_%s:\n", ac_get_ip_type_string(&pdev->info, ring));

   pipeline = radv_get_saved_pipeline(device, ring);
   if (pipeline) {
      fprintf(f, "Pipeline hash: %" PRIx64 "\n", pipeline->pipeline_hash);

      if (pipeline->type == RADV_PIPELINE_GRAPHICS) {
         struct radv_graphics_pipeline *graphics_pipeline = radv_pipeline_to_graphics(pipeline);

         radv_dump_vs_prolog(device, graphics_pipeline, f);

         /* Dump active graphics shaders. */
         unsigned stages = graphics_pipeline->active_stages;
         while (stages) {
            int stage = u_bit_scan(&stages);

            radv_dump_shader(device, &graphics_pipeline->base, graphics_pipeline->base.shaders[stage], stage, dump_dir,
                             f);
         }
      } else if (pipeline->type == RADV_PIPELINE_RAY_TRACING) {
         struct radv_ray_tracing_pipeline *rt_pipeline = radv_pipeline_to_ray_tracing(pipeline);
         for (unsigned i = 0; i < rt_pipeline->stage_count; i++) {
            struct radv_shader *shader = rt_pipeline->stages[i].shader;
            if (shader)
               radv_dump_shader(device, pipeline, shader, shader->info.stage, dump_dir, f);
         }
         radv_dump_shader(device, pipeline, pipeline->shaders[MESA_SHADER_INTERSECTION], MESA_SHADER_INTERSECTION,
                          dump_dir, f);
      } else {
         struct radv_compute_pipeline *compute_pipeline = radv_pipeline_to_compute(pipeline);

         radv_dump_shader(device, &compute_pipeline->base, compute_pipeline->base.shaders[MESA_SHADER_COMPUTE],
                          MESA_SHADER_COMPUTE, dump_dir, f);
      }

      if (!(instance->debug_flags & RADV_DEBUG_NO_UMR)) {
         struct ac_wave_info waves[AC_MAX_WAVES_PER_CHIP];
         enum amd_gfx_level gfx_level = pdev->info.gfx_level;
         unsigned num_waves = ac_get_wave_info(gfx_level, &pdev->info, waves);

         fprintf(f, COLOR_CYAN "The number of active waves = %u" COLOR_RESET "\n\n", num_waves);

         if (pipeline->type == RADV_PIPELINE_GRAPHICS) {
            struct radv_graphics_pipeline *graphics_pipeline = radv_pipeline_to_graphics(pipeline);

            /* Dump annotated active graphics shaders. */
            unsigned stages = graphics_pipeline->active_stages;
            while (stages) {
               int stage = u_bit_scan(&stages);

               radv_dump_annotated_shader(graphics_pipeline->base.shaders[stage], stage, waves, num_waves, f);
            }
         } else if (pipeline->type == RADV_PIPELINE_RAY_TRACING) {
            struct radv_ray_tracing_pipeline *rt_pipeline = radv_pipeline_to_ray_tracing(pipeline);
            for (unsigned i = 0; i < rt_pipeline->stage_count; i++) {
               struct radv_shader *shader = rt_pipeline->stages[i].shader;
               if (shader)
                  radv_dump_annotated_shader(shader, shader->info.stage, waves, num_waves, f);
            }
            radv_dump_annotated_shader(pipeline->shaders[MESA_SHADER_INTERSECTION], MESA_SHADER_INTERSECTION, waves,
                                       num_waves, f);
         } else {
            struct radv_compute_pipeline *compute_pipeline = radv_pipeline_to_compute(pipeline);

            radv_dump_annotated_shader(compute_pipeline->base.shaders[MESA_SHADER_COMPUTE], MESA_SHADER_COMPUTE, waves,
                                       num_waves, f);
         }

         /* Print waves executing shaders that are not currently bound. */
         unsigned i;
         bool found = false;
         for (i = 0; i < num_waves; i++) {
            if (waves[i].matched)
               continue;

            if (!found) {
               fprintf(f, COLOR_CYAN "Waves not executing currently-bound shaders:" COLOR_RESET "\n");
               found = true;
            }

            struct radv_shader *shader = radv_find_shader(device, waves[0].pc);
            if (shader) {
               radv_dump_annotated_shader(shader, shader->info.stage, waves, num_waves, f);
               if (waves[i].matched)
                  continue;
            }

            fprintf(f, "    SE%u SH%u CU%u SIMD%u WAVE%u  EXEC=%016" PRIx64 "  INST=%08X %08X  PC=%" PRIx64 "\n",
                    waves[i].se, waves[i].sh, waves[i].cu, waves[i].simd, waves[i].wave, waves[i].exec,
                    waves[i].inst_dw0, waves[i].inst_dw1, waves[i].pc);
         }
         if (found)
            fprintf(f, "\n\n");
      }

      if (pipeline->type == RADV_PIPELINE_GRAPHICS) {
         struct radv_graphics_pipeline *graphics_pipeline = radv_pipeline_to_graphics(pipeline);
         radv_dump_vertex_descriptors(device, graphics_pipeline, f);
      }
      radv_dump_descriptors(device, f);
   }
}

static void
radv_dump_cmd(const char *cmd, FILE *f)
{
#ifndef _WIN32
   char line[2048];
   FILE *p;

   p = popen(cmd, "r");
   if (p) {
      while (fgets(line, sizeof(line), p))
         fputs(line, f);
      fprintf(f, "\n");
      pclose(p);
   }
#endif
}

static void
radv_dump_dmesg(FILE *f)
{
   fprintf(f, "\nLast 60 lines of dmesg:\n\n");
   radv_dump_cmd("dmesg | tail -n60", f);
}

void
radv_dump_enabled_options(const struct radv_device *device, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   uint64_t mask;

   if (instance->debug_flags) {
      fprintf(f, "Enabled debug options: ");

      mask = instance->debug_flags;
      while (mask) {
         int i = u_bit_scan64(&mask);
         fprintf(f, "%s, ", radv_get_debug_option_name(i));
      }
      fprintf(f, "\n");
   }

   if (instance->perftest_flags) {
      fprintf(f, "Enabled perftest options: ");

      mask = instance->perftest_flags;
      while (mask) {
         int i = u_bit_scan64(&mask);
         fprintf(f, "%s, ", radv_get_perftest_option_name(i));
      }
      fprintf(f, "\n");
   }
}

static void
radv_dump_app_info(const struct radv_device *device, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   fprintf(f, "Application name: %s\n", instance->vk.app_info.app_name);
   fprintf(f, "Application version: %d\n", instance->vk.app_info.app_version);
   fprintf(f, "Engine name: %s\n", instance->vk.app_info.engine_name);
   fprintf(f, "Engine version: %d\n", instance->vk.app_info.engine_version);
   fprintf(f, "API version: %d.%d.%d\n", VK_VERSION_MAJOR(instance->vk.app_info.api_version),
           VK_VERSION_MINOR(instance->vk.app_info.api_version), VK_VERSION_PATCH(instance->vk.app_info.api_version));

   radv_dump_enabled_options(device, f);
}

static void
radv_dump_device_name(const struct radv_device *device, FILE *f)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radeon_info *gpu_info = &pdev->info;
#ifndef _WIN32
   char kernel_version[128] = {0};
   struct utsname uname_data;
#endif

#ifdef _WIN32
   fprintf(f, "Device name: %s (DRM %i.%i.%i)\n\n", pdev->marketing_name, gpu_info->drm_major, gpu_info->drm_minor,
           gpu_info->drm_patchlevel);
#else
   if (uname(&uname_data) == 0)
      snprintf(kernel_version, sizeof(kernel_version), " / %s", uname_data.release);

   fprintf(f, "Device name: %s (DRM %i.%i.%i%s)\n\n", pdev->marketing_name, gpu_info->drm_major, gpu_info->drm_minor,
           gpu_info->drm_patchlevel, kernel_version);
#endif
}

static void
radv_dump_umr_ring(const struct radv_queue *queue, FILE *f)
{
#ifndef _WIN32
   const struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const enum amd_ip_type ring = radv_queue_ring(queue);
   char cmd[256];

   /* TODO: Dump compute ring. */
   if (ring != AMD_IP_GFX)
      return;

   sprintf(cmd, "umr --by-pci %04x:%02x:%02x.%01x -RS %s 2>&1", pdev->bus_info.domain, pdev->bus_info.bus,
           pdev->bus_info.dev, pdev->bus_info.func, pdev->info.gfx_level >= GFX10 ? "gfx_0.0.0" : "gfx");
   fprintf(f, "\nUMR GFX ring:\n\n");
   radv_dump_cmd(cmd, f);
#endif
}

static void
radv_dump_umr_waves(struct radv_queue *queue, FILE *f)
{
#ifndef _WIN32
   const struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   enum amd_ip_type ring = radv_queue_ring(queue);
   char cmd[256];

   /* TODO: Dump compute ring. */
   if (ring != AMD_IP_GFX)
      return;

   sprintf(cmd, "umr --by-pci %04x:%02x:%02x.%01x -O bits,halt_waves -go 0 -wa %s -go 1 2>&1", pdev->bus_info.domain,
           pdev->bus_info.bus, pdev->bus_info.dev, pdev->bus_info.func,
           pdev->info.gfx_level >= GFX10 ? "gfx_0.0.0" : "gfx");
   fprintf(f, "\nUMR GFX waves:\n\n");
   radv_dump_cmd(cmd, f);
#endif
}

static bool
radv_gpu_hang_occurred(struct radv_queue *queue, enum amd_ip_type ring)
{
   const struct radv_device *device = radv_queue_device(queue);
   struct radeon_winsys *ws = device->ws;

   if (!ws->ctx_wait_idle(queue->hw_ctx, ring, queue->vk.index_in_family))
      return true;

   return false;
}

bool
radv_vm_fault_occurred(struct radv_device *device, struct radv_winsys_gpuvm_fault_info *fault_info)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (!pdev->info.has_gpuvm_fault_query)
      return false;

   return device->ws->query_gpuvm_fault(device->ws, fault_info);
}

enum radv_device_fault_chunk {
   RADV_DEVICE_FAULT_CHUNK_TRACE,
   RADV_DEVICE_FAULT_CHUNK_QUEUE_STATE,
   RADV_DEVICE_FAULT_CHUNK_UMR_WAVES,
   RADV_DEVICE_FAULT_CHUNK_UMR_RING,
   RADV_DEVICE_FAULT_CHUNK_REGISTERS,
   RADV_DEVICE_FAULT_CHUNK_BO_RANGES,
   RADV_DEVICE_FAULT_CHUNK_BO_HISTORY,
   RADV_DEVICE_FAULT_CHUNK_VM_FAULT,
   RADV_DEVICE_FAULT_CHUNK_APP_INFO,
   RADV_DEVICE_FAULT_CHUNK_GPU_INFO,
   RADV_DEVICE_FAULT_CHUNK_DMESG,
   RADV_DEVICE_FAULT_CHUNK_COUNT,
};

void
radv_check_gpu_hangs(struct radv_queue *queue, const struct radv_winsys_submit_info *submit_info)
{
   enum amd_ip_type ring;

   ring = radv_queue_ring(queue);

   bool hang_occurred = radv_gpu_hang_occurred(queue, ring);
   if (!hang_occurred)
      return;

   fprintf(stderr, "radv: GPU hang detected...\n");

#ifndef _WIN32
   struct radv_device *device = radv_queue_device(queue);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   const bool save_hang_report = !device->vk.enabled_features.deviceFaultVendorBinary;
   struct radv_winsys_gpuvm_fault_info fault_info = {0};

   /* Query if a VM fault happened for this GPU hang. */
   bool vm_fault_occurred = radv_vm_fault_occurred(device, &fault_info);

   /* Create a directory into $HOME/radv_dumps_<pid>_<time> to save
    * various debugging info about that GPU hang.
    */
   struct tm *timep, result;
   time_t raw_time;
   FILE *f;
   char dump_dir[256], dump_path[512], buf_time[128];

   if (save_hang_report) {
      time(&raw_time);
      timep = os_localtime(&raw_time, &result);
      strftime(buf_time, sizeof(buf_time), "%Y.%m.%d_%H.%M.%S", timep);

      snprintf(dump_dir, sizeof(dump_dir), "%s/" RADV_DUMP_DIR "_%d_%s", debug_get_option("HOME", "."), getpid(),
               buf_time);
      if (mkdir(dump_dir, 0774) && errno != EEXIST) {
         fprintf(stderr, "radv: can't create directory '%s' (%i).\n", dump_dir, errno);
         abort();
      }

      fprintf(stderr, "radv: GPU hang report will be saved to '%s'!\n", dump_dir);
   }

   struct {
      const char *name;
      char *ptr;
      size_t size;
   } chunks[RADV_DEVICE_FAULT_CHUNK_COUNT] = {
      {"trace"},      {"pipeline"}, {"umr_waves"}, {"umr_ring"}, {"registers"}, {"bo_ranges"},
      {"bo_history"}, {"vm_fault"}, {"app_info"},  {"gpu_info"}, {"dmesg"},
   };

   for (uint32_t i = 0; i < RADV_DEVICE_FAULT_CHUNK_COUNT; i++) {

      if (save_hang_report) {
         snprintf(dump_path, sizeof(dump_path), "%s/%s.log", dump_dir, chunks[i].name);

         f = fopen(dump_path, "w+");
      } else {
         f = open_memstream(&chunks[i].ptr, &chunks[i].size);
      }

      if (!f)
         continue;

      switch (i) {
      case RADV_DEVICE_FAULT_CHUNK_TRACE:
         radv_dump_trace(device, submit_info->cs_array[0], f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_QUEUE_STATE:
         radv_dump_queue_state(queue, dump_dir, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_UMR_WAVES:
         if (!(instance->debug_flags & RADV_DEBUG_NO_UMR))
            radv_dump_umr_waves(queue, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_UMR_RING:
         if (!(instance->debug_flags & RADV_DEBUG_NO_UMR))
            radv_dump_umr_ring(queue, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_REGISTERS:
         radv_dump_debug_registers(device, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_BO_RANGES:
         device->ws->dump_bo_ranges(device->ws, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_BO_HISTORY:
         device->ws->dump_bo_log(device->ws, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_VM_FAULT:
         if (vm_fault_occurred) {
            fprintf(f, "VM fault report.\n\n");
            fprintf(f, "Failing VM page: 0x%08" PRIx64 "\n", fault_info.addr);
            ac_print_gpuvm_fault_status(f, pdev->info.gfx_level, fault_info.status);
         }
         break;
      case RADV_DEVICE_FAULT_CHUNK_APP_INFO:
         radv_dump_app_info(device, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_GPU_INFO:
         radv_dump_device_name(device, f);
         ac_print_gpu_info(&pdev->info, f);
         break;
      case RADV_DEVICE_FAULT_CHUNK_DMESG:
         radv_dump_dmesg(f);
         break;
      default:
         break;
      }

      fclose(f);
   }

   if (save_hang_report) {
      fprintf(stderr, "radv: GPU hang report saved successfully!\n");
      abort();
   } else {
      char *report;

      report = ralloc_strdup(NULL, "========== RADV GPU hang report ==========\n");
      for (uint32_t i = 0; i < RADV_DEVICE_FAULT_CHUNK_COUNT; i++) {
         if (!chunks[i].size)
            continue;

         ralloc_asprintf_append(&report, "\n========== %s ==========\n", chunks[i].name);
         ralloc_asprintf_append(&report, "%s", chunks[i].ptr);

         free(chunks[i].ptr);
      }

      device->gpu_hang_report = report;
   }

#endif
}

void
radv_print_spirv(const char *data, uint32_t size, FILE *fp)
{
#ifndef _WIN32
   char path[] = "/tmp/fileXXXXXX";
   char command[128];
   int fd;

   /* Dump the binary into a temporary file. */
   fd = mkstemp(path);
   if (fd < 0)
      return;

   if (write(fd, data, size) == -1)
      goto fail;

   /* Disassemble using spirv-dis if installed. */
   sprintf(command, "spirv-dis %s", path);
   radv_dump_cmd(command, fp);

fail:
   close(fd);
   unlink(path);
#endif
}

bool
radv_trap_handler_init(struct radv_device *device)
{
   struct radeon_winsys *ws = device->ws;
   VkResult result;

   /* Create the trap handler shader and upload it like other shaders. */
   device->trap_handler_shader = radv_create_trap_handler_shader(device);
   if (!device->trap_handler_shader) {
      fprintf(stderr, "radv: failed to create the trap handler shader.\n");
      return false;
   }

   result = ws->buffer_make_resident(ws, device->trap_handler_shader->bo, true);
   if (result != VK_SUCCESS)
      return false;

   result = radv_bo_create(
      device, NULL, TMA_BO_SIZE, 256, RADEON_DOMAIN_VRAM,
      RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_NO_INTERPROCESS_SHARING | RADEON_FLAG_ZERO_VRAM | RADEON_FLAG_32BIT,
      RADV_BO_PRIORITY_SCRATCH, 0, true, &device->tma_bo);
   if (result != VK_SUCCESS)
      return false;

   result = ws->buffer_make_resident(ws, device->tma_bo, true);
   if (result != VK_SUCCESS)
      return false;

   device->tma_ptr = radv_buffer_map(ws, device->tma_bo);
   if (!device->tma_ptr)
      return false;

   /* Upload a buffer descriptor to store various info from the trap. */
   uint64_t tma_va = radv_buffer_get_va(device->tma_bo) + 16;
   uint32_t desc[4];

   desc[0] = tma_va;
   desc[1] = S_008F04_BASE_ADDRESS_HI(tma_va >> 32);
   desc[2] = TMA_BO_SIZE;
   desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) | S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
             S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) | S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
             S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);

   memcpy(device->tma_ptr, desc, sizeof(desc));

   return true;
}

void
radv_trap_handler_finish(struct radv_device *device)
{
   struct radeon_winsys *ws = device->ws;

   if (unlikely(device->trap_handler_shader)) {
      ws->buffer_make_resident(ws, device->trap_handler_shader->bo, false);
      radv_shader_unref(device, device->trap_handler_shader);
   }

   if (unlikely(device->tma_bo)) {
      ws->buffer_make_resident(ws, device->tma_bo, false);
      radv_bo_destroy(device, NULL, device->tma_bo);
   }
}

static void
radv_dump_faulty_shader(struct radv_device *device, uint64_t faulty_pc)
{
   struct radv_shader *shader;
   uint64_t start_addr, end_addr;
   uint32_t instr_offset;

   shader = radv_find_shader(device, faulty_pc);
   if (!shader)
      return;

   start_addr = radv_shader_get_va(shader);
   end_addr = start_addr + shader->code_size;
   instr_offset = faulty_pc - start_addr;

   fprintf(stderr,
           "Faulty shader found "
           "VA=[0x%" PRIx64 "-0x%" PRIx64 "], instr_offset=%d\n",
           start_addr, end_addr, instr_offset);

   /* Get the list of instructions.
    * Buffer size / 4 is the upper bound of the instruction count.
    */
   unsigned num_inst = 0;
   struct radv_shader_inst *instructions = calloc(shader->code_size / 4, sizeof(struct radv_shader_inst));

   /* Split the disassembly string into instructions. */
   radv_add_split_disasm(shader->disasm_string, start_addr, &num_inst, instructions);

   /* Print instructions with annotations. */
   for (unsigned i = 0; i < num_inst; i++) {
      struct radv_shader_inst *inst = &instructions[i];

      if (start_addr + inst->offset == faulty_pc) {
         fprintf(stderr, "\n!!! Faulty instruction below !!!\n");
         fprintf(stderr, "%s\n", inst->text);
         fprintf(stderr, "\n");
      } else {
         fprintf(stderr, "%s\n", inst->text);
      }
   }

   free(instructions);
}

struct radv_sq_hw_reg {
   uint32_t status;
   uint32_t trap_sts;
   uint32_t hw_id;
   uint32_t ib_sts;
};

static void
radv_dump_sq_hw_regs(struct radv_device *device)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   enum amd_gfx_level gfx_level = pdev->info.gfx_level;
   enum radeon_family family = pdev->info.family;
   struct radv_sq_hw_reg *regs = (struct radv_sq_hw_reg *)&device->tma_ptr[6];

   fprintf(stderr, "\nHardware registers:\n");
   if (pdev->info.gfx_level >= GFX10) {
      ac_dump_reg(stderr, gfx_level, family, R_000408_SQ_WAVE_STATUS, regs->status, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_00040C_SQ_WAVE_TRAPSTS, regs->trap_sts, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_00045C_SQ_WAVE_HW_ID1, regs->hw_id, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_00041C_SQ_WAVE_IB_STS, regs->ib_sts, ~0);
   } else {
      ac_dump_reg(stderr, gfx_level, family, R_000048_SQ_WAVE_STATUS, regs->status, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_00004C_SQ_WAVE_TRAPSTS, regs->trap_sts, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_000050_SQ_WAVE_HW_ID, regs->hw_id, ~0);
      ac_dump_reg(stderr, gfx_level, family, R_00005C_SQ_WAVE_IB_STS, regs->ib_sts, ~0);
   }
   fprintf(stderr, "\n\n");
}

void
radv_check_trap_handler(struct radv_queue *queue)
{
   enum amd_ip_type ring = radv_queue_ring(queue);
   struct radv_device *device = radv_queue_device(queue);
   struct radeon_winsys *ws = device->ws;

   /* Wait for the context to be idle in a finite time. */
   ws->ctx_wait_idle(queue->hw_ctx, ring, queue->vk.index_in_family);

   /* Try to detect if the trap handler has been reached by the hw by
    * looking at ttmp0 which should be non-zero if a shader exception
    * happened.
    */
   if (!device->tma_ptr[4])
      return;

#if 0
	fprintf(stderr, "tma_ptr:\n");
	for (unsigned i = 0; i < 10; i++)
		fprintf(stderr, "tma_ptr[%d]=0x%x\n", i, device->tma_ptr[i]);
#endif

   radv_dump_sq_hw_regs(device);

   uint32_t ttmp0 = device->tma_ptr[4];
   uint32_t ttmp1 = device->tma_ptr[5];

   /* According to the ISA docs, 3.10 Trap and Exception Registers:
    *
    * "{ttmp1, ttmp0} = {3'h0, pc_rewind[3:0], HT[0], trapID[7:0], PC[47:0]}"
    *
    * "When the trap handler is entered, the PC of the faulting
    *  instruction is: (PC - PC_rewind * 4)."
    * */
   uint8_t trap_id = (ttmp1 >> 16) & 0xff;
   uint8_t ht = (ttmp1 >> 24) & 0x1;
   uint8_t pc_rewind = (ttmp1 >> 25) & 0xf;
   uint64_t pc = (ttmp0 | ((ttmp1 & 0x0000ffffull) << 32)) - (pc_rewind * 4);

   fprintf(stderr, "PC=0x%" PRIx64 ", trapID=%d, HT=%d, PC_rewind=%d\n", pc, trap_id, ht, pc_rewind);

   radv_dump_faulty_shader(device, pc);

   abort();
}

/* VK_EXT_device_fault */
VKAPI_ATTR VkResult VKAPI_CALL
radv_GetDeviceFaultInfoEXT(VkDevice _device, VkDeviceFaultCountsEXT *pFaultCounts, VkDeviceFaultInfoEXT *pFaultInfo)
{
   VK_OUTARRAY_MAKE_TYPED(VkDeviceFaultAddressInfoEXT, out, pFaultInfo ? pFaultInfo->pAddressInfos : NULL,
                          &pFaultCounts->addressInfoCount);
   struct radv_winsys_gpuvm_fault_info fault_info = {0};
   VK_FROM_HANDLE(radv_device, device, _device);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   bool vm_fault_occurred = false;

   /* Query if a GPUVM fault happened. */
   vm_fault_occurred = radv_vm_fault_occurred(device, &fault_info);

   /* No vendor-specific crash dumps yet. */
   pFaultCounts->vendorInfoCount = 0;
   pFaultCounts->vendorBinarySize = 0;

   if (device->gpu_hang_report) {
      VkDeviceFaultVendorBinaryHeaderVersionOneEXT hdr;

      hdr.headerSize = sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT);
      hdr.headerVersion = VK_DEVICE_FAULT_VENDOR_BINARY_HEADER_VERSION_ONE_EXT;
      hdr.vendorID = pdev->vk.properties.vendorID;
      hdr.deviceID = pdev->vk.properties.deviceID;
      hdr.driverVersion = pdev->vk.properties.driverVersion;
      memcpy(hdr.pipelineCacheUUID, pdev->cache_uuid, VK_UUID_SIZE);
      hdr.applicationNameOffset = 0;
      hdr.applicationVersion = instance->vk.app_info.app_version;
      hdr.engineNameOffset = 0;
      hdr.engineVersion = instance->vk.app_info.engine_version;
      hdr.apiVersion = instance->vk.app_info.api_version;

      pFaultCounts->vendorBinarySize = sizeof(hdr) + strlen(device->gpu_hang_report);
      if (pFaultInfo) {
         memcpy(pFaultInfo->pVendorBinaryData, &hdr, sizeof(hdr));
         memcpy((char *)pFaultInfo->pVendorBinaryData + sizeof(hdr), device->gpu_hang_report,
                strlen(device->gpu_hang_report));
      }
   }

   if (vm_fault_occurred) {
      VkDeviceFaultAddressInfoEXT addr_fault_info = {
         .reportedAddress = ((int64_t)fault_info.addr << 16) >> 16,
         .addressPrecision = 4096, /* 4K page granularity */
      };

      if (pFaultInfo)
         strncpy(pFaultInfo->description, "A GPUVM fault has been detected", sizeof(pFaultInfo->description));

      if (pdev->info.gfx_level >= GFX10) {
         addr_fault_info.addressType = G_00A130_RW(fault_info.status) ? VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT
                                                                      : VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT;
      } else {
         /* Not sure how to get the access status on GFX6-9. */
         addr_fault_info.addressType = VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT;
      }
      vk_outarray_append_typed(VkDeviceFaultAddressInfoEXT, &out, elem) *elem = addr_fault_info;
   }

   return vk_outarray_status(&out);
}
