/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_printf.h"
#include "radv_device.h"
#include "radv_physical_device.h"

#include "util/hash_table.h"
#include "util/strndup.h"
#include "util/u_printf.h"

#include "nir.h"
#include "nir_builder.h"

static struct hash_table *device_ht = NULL;

VkResult
radv_printf_data_init(struct radv_device *device)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);

   util_dynarray_init(&device->printf.formats, NULL);

   device->printf.buffer_size = debug_get_num_option("RADV_PRINTF_BUFFER_SIZE", 0);
   if (device->printf.buffer_size < sizeof(struct radv_printf_buffer_header))
      return VK_SUCCESS;

   VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext =
         &(VkBufferUsageFlags2CreateInfoKHR){
            .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR,
            .usage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR,
         },
      .size = device->printf.buffer_size,
   };

   VkDevice _device = radv_device_to_handle(device);
   VkResult result = device->vk.dispatch_table.CreateBuffer(_device, &buffer_create_info, NULL, &device->printf.buffer);
   if (result != VK_SUCCESS)
      return result;

   VkMemoryRequirements requirements;
   device->vk.dispatch_table.GetBufferMemoryRequirements(_device, device->printf.buffer, &requirements);

   VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = requirements.size,
      .memoryTypeIndex =
         radv_find_memory_index(pdev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
   };

   result = device->vk.dispatch_table.AllocateMemory(_device, &alloc_info, NULL, &device->printf.memory);
   if (result != VK_SUCCESS)
      return result;

   result = device->vk.dispatch_table.MapMemory(_device, device->printf.memory, 0, VK_WHOLE_SIZE, 0,
                                                (void **)&device->printf.data);
   if (result != VK_SUCCESS)
      return result;

   result = device->vk.dispatch_table.BindBufferMemory(_device, device->printf.buffer, device->printf.memory, 0);
   if (result != VK_SUCCESS)
      return result;

   struct radv_printf_buffer_header *header = device->printf.data;
   header->offset = sizeof(struct radv_printf_buffer_header);
   header->size = device->printf.buffer_size;

   VkBufferDeviceAddressInfo addr_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = device->printf.buffer,
   };
   device->printf.buffer_addr = device->vk.dispatch_table.GetBufferDeviceAddress(_device, &addr_info);

   return VK_SUCCESS;
}

void
radv_printf_data_finish(struct radv_device *device)
{
   VkDevice _device = radv_device_to_handle(device);

   device->vk.dispatch_table.DestroyBuffer(_device, device->printf.buffer, NULL);
   if (device->printf.memory)
      device->vk.dispatch_table.UnmapMemory(_device, device->printf.memory);
   device->vk.dispatch_table.FreeMemory(_device, device->printf.memory, NULL);

   util_dynarray_foreach (&device->printf.formats, struct radv_printf_format, format)
      free(format->string);

   util_dynarray_fini(&device->printf.formats);
}

void
radv_build_printf(nir_builder *b, nir_def *cond, const char *format_string, ...)
{
   if (!device_ht)
      return;

   struct radv_device *device = _mesa_hash_table_search(device_ht, b->shader)->data;
   if (!device->printf.buffer_addr)
      return;

   struct radv_printf_format format = {0};
   format.string = strdup(format_string);
   if (!format.string)
      return;

   uint32_t format_index = util_dynarray_num_elements(&device->printf.formats, struct radv_printf_format);

   if (cond)
      nir_push_if(b, cond);

   nir_def *size = nir_imm_int(b, 4);

   va_list arg_list;
   va_start(arg_list, format_string);

   uint32_t num_args = 0;
   for (uint32_t i = 0; i < strlen(format_string); i++)
      if (format_string[i] == '%')
         num_args++;

   nir_def **args = malloc(num_args * sizeof(nir_def *));
   nir_def **strides = malloc(num_args * sizeof(nir_def *));

   nir_def *ballot = nir_ballot(b, 1, 64, nir_imm_true(b));
   nir_def *active_invocation_count = nir_bit_count(b, ballot);

   for (uint32_t i = 0; i < num_args; i++) {
      nir_def *arg = va_arg(arg_list, nir_def *);

      if (arg->bit_size == 1)
         arg = nir_b2i32(b, arg);

      args[i] = arg;

      uint32_t arg_size = arg->bit_size == 1 ? 32 : arg->bit_size / 8;
      format.element_sizes[i] = arg_size;

      nir_update_instr_divergence(b->shader, arg->parent_instr);

      if (arg->divergent) {
         strides[i] = nir_imul_imm(b, active_invocation_count, arg_size);
         format.divergence_mask |= BITFIELD_BIT(i);
      } else {
         strides[i] = nir_imm_int(b, arg_size);
      }

      size = nir_iadd(b, size, strides[i]);
   }

   va_end(arg_list);

   nir_def *offset;
   nir_def *undef;

   nir_push_if(b, nir_elect(b, 1));
   {
      offset = nir_global_atomic(
         b, 32, nir_imm_int64(b, device->printf.buffer_addr + offsetof(struct radv_printf_buffer_header, offset)), size,
         .atomic_op = nir_atomic_op_iadd);
   }
   nir_push_else(b, NULL);
   {
      undef = nir_undef(b, 1, 32);
   }
   nir_pop_if(b, NULL);

   offset = nir_read_first_invocation(b, nir_if_phi(b, offset, undef));

   nir_def *buffer_size = nir_load_global(
      b, nir_imm_int64(b, device->printf.buffer_addr + offsetof(struct radv_printf_buffer_header, size)), 4, 1, 32);

   nir_push_if(b, nir_ige(b, buffer_size, nir_iadd(b, offset, size)));
   {
      nir_def *addr = nir_iadd_imm(b, nir_u2u64(b, offset), device->printf.buffer_addr);

      /* header */
      nir_store_global(b, addr, 4, nir_ior_imm(b, active_invocation_count, format_index << 16), 1);
      addr = nir_iadd_imm(b, addr, 4);

      for (uint32_t i = 0; i < num_args; i++) {
         nir_def *arg = args[i];

         if (arg->divergent) {
            nir_def *invocation_index = nir_mbcnt_amd(b, ballot, nir_imm_int(b, 0));
            nir_store_global(
               b, nir_iadd(b, addr, nir_u2u64(b, nir_imul_imm(b, invocation_index, format.element_sizes[i]))), 4, arg,
               1);
         } else {
            nir_store_global(b, addr, 4, arg, 1);
         }

         addr = nir_iadd(b, addr, nir_u2u64(b, strides[i]));
      }
   }
   nir_pop_if(b, NULL);

   if (cond)
      nir_pop_if(b, NULL);

   free(args);
   free(strides);

   util_dynarray_append(&device->printf.formats, struct radv_printf_format, format);
}

void
radv_dump_printf_data(struct radv_device *device, FILE *out)
{
   if (!device->printf.data)
      return;

   device->vk.dispatch_table.DeviceWaitIdle(radv_device_to_handle(device));

   struct radv_printf_buffer_header *header = device->printf.data;
   uint8_t *data = device->printf.data;

   for (uint32_t offset = sizeof(struct radv_printf_buffer_header); offset < header->offset;) {
      uint32_t printf_header = *(uint32_t *)&data[offset];
      offset += sizeof(uint32_t);

      uint32_t format_index = printf_header >> 16;
      struct radv_printf_format *printf_format =
         util_dynarray_element(&device->printf.formats, struct radv_printf_format, format_index);

      uint32_t invocation_count = printf_header & 0xFFFF;

      uint32_t num_args = 0;
      for (uint32_t i = 0; i < strlen(printf_format->string); i++)
         if (printf_format->string[i] == '%')
            num_args++;

      char *format = printf_format->string;

      for (uint32_t i = 0; i <= num_args; i++) {
         size_t spec_pos = util_printf_next_spec_pos(format, 0);

         if (spec_pos == -1) {
            fprintf(out, "%s", format);
            continue;
         }

         const char *token = util_printf_prev_tok(&format[spec_pos]);
         char *next_format = &format[spec_pos + 1];

         /* print the part before the format token */
         if (token != format)
            fwrite(format, token - format, 1, out);

         char *print_str = strndup(token, next_format - token);
         /* rebase spec_pos so we can use it with print_str */
         spec_pos += format - token;

         size_t element_size = printf_format->element_sizes[i];
         bool is_float = strpbrk(print_str, "fFeEgGaA") != NULL;

         uint32_t lane_count = (printf_format->divergence_mask & BITFIELD_BIT(i)) ? invocation_count : 1;
         for (uint32_t lane = 0; lane < lane_count; lane++) {
            switch (element_size) {
            case 1: {
               uint8_t v;
               memcpy(&v, &data[offset], element_size);
               fprintf(out, print_str, v);
               break;
            }
            case 2: {
               uint16_t v;
               memcpy(&v, &data[offset], element_size);
               fprintf(out, print_str, v);
               break;
            }
            case 4: {
               if (is_float) {
                  float v;
                  memcpy(&v, &data[offset], element_size);
                  fprintf(out, print_str, v);
               } else {
                  uint32_t v;
                  memcpy(&v, &data[offset], element_size);
                  fprintf(out, print_str, v);
               }
               break;
            }
            case 8: {
               if (is_float) {
                  double v;
                  memcpy(&v, &data[offset], element_size);
                  fprintf(out, print_str, v);
               } else {
                  uint64_t v;
                  memcpy(&v, &data[offset], element_size);
                  fprintf(out, print_str, v);
               }
               break;
            }
            default:
               unreachable("Unsupported data type");
            }

            if (lane != lane_count - 1)
               fprintf(out, " ");

            offset += element_size;
         }

         /* rebase format */
         format = next_format;
         free(print_str);
      }
   }

   fflush(out);

   header->offset = sizeof(struct radv_printf_buffer_header);
}

void
radv_device_associate_nir(struct radv_device *device, nir_shader *nir)
{
   if (!device->printf.buffer_addr)
      return;

   if (!device_ht)
      device_ht = _mesa_pointer_hash_table_create(NULL);

   _mesa_hash_table_insert(device_ht, nir, device);
}
