/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "error_decode_xe_lib.h"

#include <stdlib.h>
#include <string.h>

#include "error_decode_lib.h"
#include "util/macros.h"

static const char *
read_parameter_helper(const char *line, const char *parameter)
{
   if (!strstr(line, parameter))
      return NULL;

   while (*line != ':')
      line++;
   /* skip ':' and ' ' */
   line += 2;

   return line;
}

/* parse lines like 'batch_addr[0]: 0x0000effeffff5000 */
bool
error_decode_xe_read_u64_hexacimal_parameter(const char *line, const char *parameter, uint64_t *value)
{
   line = read_parameter_helper(line, parameter);
   if (!line)
      return false;

   *value = (uint64_t)strtoull(line, NULL, 0);
   return true;
}

/* parse lines like 'PCI ID: 0x9a49' */
bool
error_decode_xe_read_hexacimal_parameter(const char *line, const char *parameter, uint32_t *value)
{
   line = read_parameter_helper(line, parameter);
   if (!line)
      return false;

   *value = (int)strtoul(line, NULL, 0);
   return true;
}

/* parse lines like 'rcs0 (physical), logical instance=0' */
bool
error_decode_xe_read_engine_name(const char *line, char *ring_name)
{
   int i;

   if (!strstr(line, " (physical), logical instance="))
      return false;

   i = 0;
   for (i = 0; *line != ' '; i++, line++)
      ring_name[i] = *line;

   ring_name[i] = 0;
   return true;
}

/*
 * when a topic string is parsed it sets new_topic and returns true, otherwise
 * does nothing.
 */
bool
error_decode_xe_decode_topic(const char *line, enum xe_topic *new_topic)
{
   static const char *xe_topic_strings[] = {
      "**** Xe Device Coredump ****",
      "**** GuC CT ****",
      "**** Job ****",
      "**** HW Engines ****",
      "**** VM state ****",
   };
   bool topic_changed = false;

   for (int i = 0; i < ARRAY_SIZE(xe_topic_strings); i++) {
      if (strncmp(xe_topic_strings[i], line, strlen(xe_topic_strings[i])) == 0) {
         topic_changed = true;
         *new_topic = i;
         break;
      }
   }

   return topic_changed;
}

/* return type of VM topic lines like '[200000].data: x...' and points
 * value_ptr to first char of data of topic type
 */
enum xe_vm_topic_type
error_decode_xe_read_vm_line(const char *line, uint64_t *address, const char **value_ptr)
{
   enum xe_vm_topic_type type;
   char text_addr[64];
   int i;

   if (*line != '[')
      return XE_VM_TOPIC_TYPE_UNKNOWN;

   for (i = 0, line++; *line != ']'; i++, line++)
      text_addr[i] = *line;

   text_addr[i] = 0;
   *address = (uint64_t)strtoull(text_addr, NULL, 16);

   /* at this point line points to last address digit so +3 to point to type */
   line += 2;
   switch (*line) {
   case 'd':
      type = XE_VM_TOPIC_TYPE_DATA;
      break;
   case 'l':
      type = XE_VM_TOPIC_TYPE_LENGTH;
      break;
   case 'e':
      type = XE_VM_TOPIC_TYPE_ERROR;
      break;
   default:
      printf("type char: %c\n", *line);
      return XE_VM_TOPIC_TYPE_UNKNOWN;
   }

   for (; *line != ':'; line++);

   *value_ptr = line + 2;
   return type;
}

/*
 * similar to read_xe_vm_line() but it parses '[HWCTX].data: ...'
 */
enum xe_vm_topic_type
error_decode_xe_read_hw_sp_or_ctx_line(const char *line, const char **value_ptr, bool *is_hw_ctx)
{
   enum xe_vm_topic_type type;
   char text_addr[64];
   bool is_hw_sp;
   int i;

   if (*line != '\t')
      return XE_VM_TOPIC_TYPE_UNKNOWN;

   line++;
   if (*line != '[')
      return XE_VM_TOPIC_TYPE_UNKNOWN;

   for (i = 0, line++; *line != ']'; i++, line++)
      text_addr[i] = *line;

   text_addr[i] = 0;
   *is_hw_ctx = strncmp(text_addr, "HWCTX", strlen("HWCTX")) == 0;
   is_hw_sp =  strncmp(text_addr, "HWSP", strlen("HWSP")) == 0;
   if (*is_hw_ctx == false && is_hw_sp == false)
         return XE_VM_TOPIC_TYPE_UNKNOWN;

   /* at this point line points to last address digit so +3 to point to type */
   line += 2;
   switch (*line) {
   case 'd':
      type = XE_VM_TOPIC_TYPE_DATA;
      break;
   case 'l':
      type = XE_VM_TOPIC_TYPE_LENGTH;
      break;
   case 'e':
      type = XE_VM_TOPIC_TYPE_ERROR;
      break;
   default:
      printf("type char: %c\n", *line);
      return XE_VM_TOPIC_TYPE_UNKNOWN;
   }

   for (; *line != ':'; line++);

   *value_ptr = line + 2;
   return type;
}

void error_decode_xe_vm_init(struct xe_vm *xe_vm)
{
   xe_vm->entries = NULL;
   xe_vm->entries_len = 0;
   memset(&xe_vm->hw_context, 0, sizeof(xe_vm->hw_context));
}

void error_decode_xe_vm_fini(struct xe_vm *xe_vm)
{
   uint32_t i;

   for (i = 0; i < xe_vm->entries_len; i++)
      free((uint32_t *)xe_vm->entries[i].data);

   free((uint32_t *)xe_vm->hw_context.data);
   free(xe_vm->entries);
}

static void
xe_vm_entry_set(struct xe_vm_entry *entry, const uint64_t address,
                const uint32_t length, const uint32_t *data)
{
   entry->address = address;
   entry->length = length;
   entry->data = data;
}

void
error_decode_xe_vm_hw_ctx_set(struct xe_vm *xe_vm, const uint32_t length,
                              const uint32_t *data)
{
   xe_vm_entry_set(&xe_vm->hw_context, 0, length, data);
}

/*
 * error_decode_xe_vm_fini() will take care to free data
 */
bool
error_decode_xe_vm_append(struct xe_vm *xe_vm, const uint64_t address,
                          const uint32_t length, const uint32_t *data)
{
   size_t len = sizeof(*xe_vm->entries) * (xe_vm->entries_len + 1);

   xe_vm->entries = realloc(xe_vm->entries, len);
   if (!xe_vm->entries)
      return false;

   xe_vm_entry_set(&xe_vm->entries[xe_vm->entries_len], address, length, data);
   xe_vm->entries_len++;
   return true;
}

const struct xe_vm_entry *
error_decode_xe_vm_entry_get(struct xe_vm *xe_vm, const uint64_t address)
{
   uint32_t i;

   for (i = 0; i < xe_vm->entries_len; i++) {
      struct xe_vm_entry *entry = &xe_vm->entries[i];

      if (entry->address == address)
         return entry;

      if (address > entry->address &&
          address < (entry->address + entry->length))
         return entry;
   }

   return NULL;
}

uint32_t *
error_decode_xe_vm_entry_address_get_data(const struct xe_vm_entry *entry,
                                          const uint64_t address)
{
   uint32_t offset = (address - entry->address) / sizeof(uint32_t);
   return (uint32_t *)&entry->data[offset];
}

uint32_t
error_decode_xe_vm_entry_address_get_len(const struct xe_vm_entry *entry,
                                         const uint64_t address)
{
   return entry->length - (address - entry->address);
}

bool
error_decode_xe_ascii85_decode_allocated(const char *in, uint32_t *out, uint32_t vm_entry_bytes_len)
{
   const uint32_t dword_len = vm_entry_bytes_len / sizeof(uint32_t);
   uint32_t i;

   for (i = 0; (*in >= '!') && (*in <= 'z') && (i < dword_len); i++)
      in = ascii85_decode_char(in, &out[i]);

   if (dword_len != i)
      printf("mismatch dword_len=%u i=%u\n", dword_len, i);

   return dword_len == i && (*in < '!' || *in > 'z');
}
