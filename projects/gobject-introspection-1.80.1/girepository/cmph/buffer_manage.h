#ifndef __CMPH_BUFFER_MANAGE_H__
#define __CMPH_BUFFER_MANAGE_H__

#include "cmph_types.h"
#include <stdio.h>
typedef struct __buffer_manage_t buffer_manage_t;

buffer_manage_t * buffer_manage_new(cmph_uint32 memory_avail, cmph_uint32 nentries);
void buffer_manage_open(buffer_manage_t * buffer_manage, cmph_uint32 index, char * filename);
cmph_uint8 * buffer_manage_read_key(buffer_manage_t * buffer_manage, cmph_uint32 index);
void buffer_manage_destroy(buffer_manage_t * buffer_manage);
#endif
