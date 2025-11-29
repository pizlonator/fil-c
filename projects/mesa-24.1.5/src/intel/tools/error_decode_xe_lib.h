/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum xe_topic {
   XE_TOPIC_DEVICE = 0,
   XE_TOPIC_GUC_CT,
   XE_TOPIC_JOB,
   XE_TOPIC_HW_ENGINES,
   XE_TOPIC_VM,
   XE_TOPIC_INVALID,
};

enum xe_vm_topic_type {
   XE_VM_TOPIC_TYPE_UNKNOWN = 0,
   XE_VM_TOPIC_TYPE_LENGTH,
   XE_VM_TOPIC_TYPE_DATA,
   XE_VM_TOPIC_TYPE_ERROR,
};

struct xe_vm_entry {
   uint64_t address;
   uint32_t length;
   const uint32_t *data;
};

struct xe_vm {
   /* TODO: entries could be appended sorted or a hash could be used to
    * optimize performance
    */
   struct xe_vm_entry *entries;
   uint32_t entries_len;

   struct xe_vm_entry hw_context;
};

bool error_decode_xe_read_u64_hexacimal_parameter(const char *line, const char *parameter, uint64_t *value);
bool error_decode_xe_read_hexacimal_parameter(const char *line, const char *parameter, uint32_t *value);
bool error_decode_xe_read_engine_name(const char *line, char *ring_name);

bool error_decode_xe_decode_topic(const char *line, enum xe_topic *new_topic);

enum xe_vm_topic_type error_decode_xe_read_vm_line(const char *line, uint64_t *address, const char **value_ptr);
enum xe_vm_topic_type error_decode_xe_read_hw_sp_or_ctx_line(const char *line, const char **value_ptr, bool *is_hw_ctx);

void error_decode_xe_vm_init(struct xe_vm *xe_vm);
void error_decode_xe_vm_fini(struct xe_vm *xe_vm);
void error_decode_xe_vm_hw_ctx_set(struct xe_vm *xe_vm, const uint32_t length, const uint32_t *data);
bool error_decode_xe_vm_append(struct xe_vm *xe_vm, const uint64_t address, const uint32_t length, const uint32_t *data);
const struct xe_vm_entry *error_decode_xe_vm_entry_get(struct xe_vm *xe_vm, const uint64_t address);
uint32_t *error_decode_xe_vm_entry_address_get_data(const struct xe_vm_entry *entry, const uint64_t address);
uint32_t error_decode_xe_vm_entry_address_get_len(const struct xe_vm_entry *entry, const uint64_t address);

bool error_decode_xe_ascii85_decode_allocated(const char *in, uint32_t *out, uint32_t vm_entry_bytes_len);
