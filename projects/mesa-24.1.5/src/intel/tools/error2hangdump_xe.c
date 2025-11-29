/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "error2hangdump_xe.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error_decode_xe_lib.h"
#include "error2hangdump_lib.h"
#include "intel/dev/intel_device_info.h"
#include "util/macros.h"

void
read_xe_data_file(FILE *dump_file, FILE *hang_dump_file, bool verbose)
{
   enum  xe_topic xe_topic = XE_TOPIC_INVALID;
   uint32_t *vm_entry_data = NULL;
   uint32_t vm_entry_len = 0;
   struct xe_vm xe_vm;
   char *line = NULL;
   size_t line_size;
   struct {
      uint64_t *addrs;
      uint8_t len;
   } batch_buffers = { .addrs = NULL, .len = 0 };
   uint32_t i;

   error_decode_xe_vm_init(&xe_vm);

   while (getline(&line, &line_size, dump_file) > 0) {
      if (error_decode_xe_decode_topic(line, &xe_topic))
         continue;

      switch (xe_topic) {
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

         type = error_decode_xe_read_hw_sp_or_ctx_line(line, &value_ptr, &is_hw_ctx);
         if (type == XE_VM_TOPIC_TYPE_UNKNOWN || !is_hw_ctx) {
            break;
         }

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

            error_decode_xe_vm_hw_ctx_set(&xe_vm, vm_entry_len, vm_entry_data);
            break;
         }
         case XE_VM_TOPIC_TYPE_ERROR:
            printf("HWCTX not present in dump, content will be zeroed: %s\n", line);
            break;
         default:
            printf("Not expected line in HWCTX: %s", line);
         }

         break;
      }
      case XE_TOPIC_VM: {
         enum xe_vm_topic_type type;
         const char *value_ptr;
         uint64_t address;

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
   }

   if (verbose) {
      fprintf(stdout, "BOs found:\n");
      for (i = 0; i < xe_vm.entries_len; i++) {
         struct xe_vm_entry *entry = &xe_vm.entries[i];

         fprintf(stdout, "\taddr=0x%016" PRIx64 " size=%" PRIu32 "\n", entry->address, entry->length);
      }
   }

   fail_if(!batch_buffers.len, "Failed to find batch buffer.\n");
   fail_if(!xe_vm.hw_context.length, "Failed to find HW image buffer.\n");

   for (i = 0; i < xe_vm.entries_len; i++) {
      struct xe_vm_entry *entry = &xe_vm.entries[i];
      const char *name = "user";
      uint32_t j;

      for (j = 0; j < batch_buffers.len; j++) {
         if (batch_buffers.addrs[j] == entry->address)
            name = "batch";
      }

      write_buffer(hang_dump_file, entry->address, entry->data, entry->length, name);
   }

   fprintf(stderr, "writing image buffer size=0x%016" PRIx32 "\n", xe_vm.hw_context.length);
   write_hw_image_buffer(hang_dump_file, xe_vm.hw_context.data, xe_vm.hw_context.length);

   for (i = 0; i < batch_buffers.len; i++) {
      write_exec(hang_dump_file, batch_buffers.addrs[i]);
   }

   free(batch_buffers.addrs);
   free(line);
   error_decode_xe_vm_fini(&xe_vm);
}
