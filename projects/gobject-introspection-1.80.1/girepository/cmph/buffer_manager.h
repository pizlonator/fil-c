#ifndef __CMPH_BUFFER_MANAGE_H__
#define __CMPH_BUFFER_MANAGE_H__

#include "cmph_types.h"
#include <stdio.h>
typedef struct __buffer_manager_t buffer_manager_t;

buffer_manager_t * buffer_manager_new(cmph_uint32 memory_avail, cmph_uint32 nentries);
void buffer_manager_open(buffer_manager_t * buffer_manager, cmph_uint32 index, char * filename);
cmph_uint8 * buffer_manager_read_key(buffer_manager_t * buffer_manager, cmph_uint32 index, cmph_uint32 * keylen);
void buffer_manager_destroy(buffer_manager_t * buffer_manager);
#endif
