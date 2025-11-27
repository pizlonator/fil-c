#ifndef __CMPH_BUFFER_ENTRY_H__
#define __CMPH_BUFFER_ENTRY_H__

#include "cmph_types.h"
#include <stdio.h>
typedef struct __buffer_entry_t buffer_entry_t;

buffer_entry_t * buffer_entry_new(cmph_uint32 capacity);
void buffer_entry_set_capacity(buffer_entry_t * buffer_entry, cmph_uint32 capacity);
cmph_uint32 buffer_entry_get_capacity(buffer_entry_t * buffer_entry);
void buffer_entry_open(buffer_entry_t * buffer_entry, char * filename);
cmph_uint8 * buffer_entry_read_key(buffer_entry_t * buffer_entry, cmph_uint32 * keylen);
void buffer_entry_destroy(buffer_entry_t * buffer_entry);
#endif
