/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "aubinator_error_decode_xe.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aubinator_error_decode_lib.h"
#include "error_decode_lib.h"
#include "error_decode_xe_lib.h"
#include "intel/compiler/brw_isa_info.h"
#include "intel/dev/intel_device_info.h"

static struct intel_batch_decode_bo
get_bo(void *user_data, bool ppgtt, uint64_t bo_addr)
{
   struct intel_batch_decode_bo ret = {};
   const struct xe_vm_entry *vm_entry;
   struct xe_vm *xe_vm = user_data;

   if (!ppgtt)
      return ret;

   vm_entry = error_decode_xe_vm_entry_get(xe_vm, bo_addr);
   if (!vm_entry)
      return ret;

   ret.addr = bo_addr;
   ret.map = error_decode_xe_vm_entry_address_get_data(vm_entry, bo_addr);
   ret.size = error_decode_xe_vm_entry_address_get_len(vm_entry, bo_addr);

   return ret;
}

static void
print_batch(struct intel_batch_decode_ctx *batch_ctx, const uint32_t *bb_data,
            const uint64_t bb_addr, uint32_t bb_len, const char *buffer_name,
            const char *engine_name, enum intel_engine_class engine_class,
            enum intel_batch_decode_flags batch_flags,
            bool option_print_all_bb, bool ring_wraps)
{
   bool is_ring_buffer;

   printf("--- %s (%s) at 0x%016"PRIx64"\n", buffer_name, engine_name, bb_addr);

   /* TODO: checks around buffer_name are copied from i915, if Xe KMD
    * starts to dump HW context or ring buffer this might become
    * useful.
    */
   is_ring_buffer = strcmp(buffer_name, "ring buffer") == 0;
   if (option_print_all_bb || is_ring_buffer ||
       strcmp(buffer_name, "batch buffer") == 0 ||
       strcmp(buffer_name, "HW Context") == 0) {
      if (is_ring_buffer && ring_wraps)
         batch_ctx->flags &= ~INTEL_BATCH_DECODE_OFFSETS;
      batch_ctx->engine = engine_class;
      intel_print_batch(batch_ctx, bb_data, bb_len, bb_addr, is_ring_buffer);
      batch_ctx->flags = batch_flags;
      printf("\n");
   }
}

static void
print_register(struct intel_spec *spec, enum decode_color option_color,
               const char *name, uint32_t reg)
{
   struct intel_group *reg_spec =
      name ? intel_spec_find_register_by_name(spec, name) : NULL;

   if (reg_spec) {
      const char *spacing_reg = "\t\t";
      const char *spacing_dword = "\t";

      intel_print_group_custom_spacing(stdout, reg_spec, 0, &reg, 0,
                                       option_color == DECODE_COLOR_ALWAYS,
                                       spacing_reg, spacing_dword);
   }
}

void
read_xe_data_file(FILE *file,
                  enum intel_batch_decode_flags batch_flags,
                  const char *spec_xml_path,
                  bool option_dump_kernels,
                  bool option_print_all_bb,
                  enum decode_color option_color)
{
   struct intel_batch_decode_ctx batch_ctx;
   struct intel_device_info devinfo;
   struct intel_spec *spec = NULL;
   struct brw_isa_info isa;
   struct {
      uint64_t *addrs;
      uint8_t len;
   } batch_buffers = { .addrs = NULL, .len = 0 };
   enum intel_engine_class engine_class = INTEL_ENGINE_CLASS_INVALID;
   uint32_t *vm_entry_data = NULL;
   uint32_t vm_entry_len = 0;
   bool ring_wraps = false;
   uint64_t acthd = 0;
   struct xe_vm xe_vm;
   char *line = NULL;
   size_t line_size;
   enum xe_topic xe_topic = XE_TOPIC_INVALID;

   error_decode_xe_vm_init(&xe_vm);

   while (getline(&line, &line_size, file) > 0) {
      bool topic_changed = false;
      bool print_line = true;

      topic_changed = error_decode_xe_decode_topic(line, &xe_topic);
      if (topic_changed) {
         print_line = (xe_topic != XE_TOPIC_VM);
         if (print_line)
            fputs(line, stdout);
         continue;
      }

      switch (xe_topic) {
      case XE_TOPIC_DEVICE: {
         uint32_t value;

         if (error_decode_xe_read_hexacimal_parameter(line, "PCI ID", &value)) {
            if (intel_get_device_info_from_pci_id(value, &devinfo)) {
               printf("Detected GFX ver %i\n", devinfo.verx10);
               brw_init_isa_info(&isa, &devinfo);

               if (spec_xml_path == NULL)
                  spec = intel_spec_load(&devinfo);
               else
                  spec = intel_spec_load_from_path(&devinfo, spec_xml_path);
            } else {
               printf("Unable to identify devid: 0x%x\n", value);
            }
         }

         break;
      }
      case XE_TOPIC_HW_ENGINES: {
         char engine_name[64];
         uint64_t u64_reg;
         uint32_t reg;

         if (error_decode_xe_read_engine_name(line, engine_name)) {
            ring_name_to_class(engine_name, &engine_class);
            break;
         }

         if (error_decode_xe_read_u64_hexacimal_parameter(line, "ACTHD", &u64_reg)) {
            acthd = u64_reg;
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "RING_INSTDONE", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "INSTDONE_1", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "SC_INSTDONE", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "SC_INSTDONE", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "SC_INSTDONE_EXTRA", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "SC_INSTDONE_EXTRA", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "SC_INSTDONE_EXTRA2", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "SC_INSTDONE_EXTRA2", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "SAMPLER_INSTDONE", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "SAMPLER_INSTDONE", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "ROW_INSTDONE", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "ROW_INSTDONE", reg);
            break;
         }

         if (error_decode_xe_read_hexacimal_parameter(line, "INSTDONE_GEOM_SVGUNIT", &reg)) {
            print_line = false;
            fputs(line, stdout);
            print_register(spec, option_color, "INSTDONE_GEOM", reg);
            break;
         }

         /* TODO: parse other engine registers */
         break;
      }
      case XE_TOPIC_JOB: {
         uint64_t u64_value;

         if (error_decode_xe_read_u64_hexacimal_parameter(line, "batch_addr[", &u64_value)) {
            batch_buffers.addrs = realloc(batch_buffers.addrs, sizeof(uint64_t) * (batch_buffers.len + 1));
            batch_buffers.addrs[batch_buffers.len] = u64_value;
            batch_buffers.len++;
         }

         break;
      }
      case XE_TOPIC_GUC_CT: {
         enum xe_vm_topic_type type;
         const char *value_ptr;
         bool is_hw_ctx;

         /* TODO: what to do with HWSP? */
         type = error_decode_xe_read_hw_sp_or_ctx_line(line, &value_ptr, &is_hw_ctx);
         if (type != XE_VM_TOPIC_TYPE_UNKNOWN) {
            print_line = false;

            if (!is_hw_ctx)
               break;

            switch (type) {
            case XE_VM_TOPIC_TYPE_DATA:
               if (!error_decode_xe_ascii85_decode_allocated(value_ptr, vm_entry_data, vm_entry_len))
                  printf("Failed to parse HWCTX data\n");
               break;
            case XE_VM_TOPIC_TYPE_LENGTH: {
               vm_entry_len = strtoul(value_ptr, NULL, 0);
               vm_entry_data = calloc(1, vm_entry_len);
               if (!vm_entry_data) {
                  printf("Out of memory to allocate a buffer to store content of HWCTX\n");
                  break;
               }

               if (is_hw_ctx)
                  error_decode_xe_vm_hw_ctx_set(&xe_vm, vm_entry_len, vm_entry_data);
               break;
            }
            case XE_VM_TOPIC_TYPE_ERROR:
               printf("HWCTX not present in dump, content will be zeroed: %s\n", line);
               break;
            default:
               printf("Not expected line in HWCTX: %s", line);
            }
         }

         break;
      }
      case XE_TOPIC_VM: {
         enum xe_vm_topic_type type;
         const char *value_ptr;
         uint64_t address;

         print_line = false;
         type = error_decode_xe_read_vm_line(line, &address, &value_ptr);
         switch (type) {
         case XE_VM_TOPIC_TYPE_DATA: {
            if (!error_decode_xe_ascii85_decode_allocated(value_ptr, vm_entry_data, vm_entry_len))
               printf("Failed to parse VMA 0x%" PRIx64 " data\n", address);
            break;
         }
         case XE_VM_TOPIC_TYPE_LENGTH: {
            vm_entry_len = strtoul(value_ptr, NULL, 0);
            vm_entry_data = calloc(1, vm_entry_len);
            if (!vm_entry_data) {
               printf("Out of memory to allocate a buffer to store content of VMA 0x%" PRIx64 "\n", address);
               break;
            }
            if (!error_decode_xe_vm_append(&xe_vm, address, vm_entry_len, vm_entry_data)) {
               printf("xe_vm_append() failed for VMA 0x%" PRIx64 "\n", address);
               break;
            }
            break;
         }
         case XE_VM_TOPIC_TYPE_ERROR:
            printf("VMA 0x%" PRIx64 " not present in dump, content will be zeroed: %s\n", address, line);
            break;
         default:
            printf("Not expected line in VM state: %s", line);
         }
         break;
      }
      default:
            break;
      }

      if (print_line)
         fputs(line, stdout);
   }

   printf("**** Batch buffers ****\n");
   intel_batch_decode_ctx_init_brw(&batch_ctx, &isa, &devinfo, stdout,
                                   batch_flags, spec_xml_path, get_bo,
                                   NULL, &xe_vm);
   batch_ctx.acthd = acthd;

   if (option_dump_kernels)
      batch_ctx.shader_binary = dump_shader_binary;

   for (int i = 0; i < batch_buffers.len; i++) {
      const uint64_t bb_addr = batch_buffers.addrs[i];
      const struct xe_vm_entry *vm_entry = error_decode_xe_vm_entry_get(&xe_vm, bb_addr);
      const char *engine_name = intel_engines_class_to_string(engine_class);
      const char *buffer_name = "batch buffer";
      const uint32_t *bb_data;
      uint32_t bb_len;

      if (!vm_entry)
         continue;

      bb_data = error_decode_xe_vm_entry_address_get_data(vm_entry, bb_addr);
      bb_len = error_decode_xe_vm_entry_address_get_len(vm_entry, bb_addr);
      print_batch(&batch_ctx, bb_data, bb_addr, bb_len, buffer_name,
                  engine_name, engine_class, batch_flags, option_print_all_bb,
                  ring_wraps);
   }

   printf("**** HW context ****\n");
   if (xe_vm.hw_context.length) {
      const char *engine_name = intel_engines_class_to_string(engine_class);
      const char *buffer_name = "HW Context";
      const uint64_t bb_addr = 0;
      const uint32_t *bb_data;
      uint32_t bb_len;

      bb_data = xe_vm.hw_context.data;
      bb_len = xe_vm.hw_context.length;
      print_batch(&batch_ctx, bb_data, bb_addr, bb_len, buffer_name,
                  engine_name, engine_class, batch_flags, option_print_all_bb,
                  ring_wraps);
   }

   intel_batch_decode_ctx_finish(&batch_ctx);
   intel_spec_destroy(spec);
   free(batch_buffers.addrs);
   free(line);
   error_decode_xe_vm_fini(&xe_vm);
}
