#include "buffer_entry.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct __buffer_entry_t
{
	FILE *fd;
	cmph_uint8 * buff;
	cmph_uint32 capacity, // buffer entry capacity
		    nbytes,   // buffer entry used bytes
		    pos;      // current read position in buffer entry
	cmph_uint8  eof;      // flag to indicate end of file
};

buffer_entry_t * buffer_entry_new(cmph_uint32 capacity)
{
	buffer_entry_t *buff_entry = (buffer_entry_t *)malloc(sizeof(buffer_entry_t));
	assert(buff_entry);
	buff_entry->fd = NULL;
	buff_entry->buff = NULL;
	buff_entry->capacity = capacity;
	buff_entry->nbytes = capacity;
	buff_entry->pos = capacity;
        buff_entry->eof = 0;
	return buff_entry;
}

void buffer_entry_open(buffer_entry_t * buffer_entry, char * filename)
{
	buffer_entry->fd = fopen(filename, "rb");
}

void buffer_entry_set_capacity(buffer_entry_t * buffer_entry, cmph_uint32 capacity)
{
	buffer_entry->capacity = capacity;
}


cmph_uint32 buffer_entry_get_capacity(buffer_entry_t * buffer_entry)
{
	return buffer_entry->capacity;
}

static void buffer_entry_load(buffer_entry_t * buffer_entry)
{
	free(buffer_entry->buff);
	buffer_entry->buff = (cmph_uint8 *)calloc((size_t)buffer_entry->capacity, sizeof(cmph_uint8));
	buffer_entry->nbytes = (cmph_uint32)fread(buffer_entry->buff, (size_t)1, (size_t)buffer_entry->capacity, buffer_entry->fd);
	if (buffer_entry->nbytes != buffer_entry->capacity) buffer_entry->eof = 1;
	buffer_entry->pos = 0;
}

cmph_uint8 * buffer_entry_read_key(buffer_entry_t * buffer_entry, cmph_uint32 * keylen)
{
	cmph_uint8 * buf = NULL;
	cmph_uint32 lacked_bytes = sizeof(*keylen);
	cmph_uint32 copied_bytes = 0;
	if(buffer_entry->eof && (buffer_entry->pos == buffer_entry->nbytes)) // end
	{
		free(buf);
		return NULL;
	}
	if((buffer_entry->pos + lacked_bytes) > buffer_entry->nbytes) 
	{
		copied_bytes = buffer_entry->nbytes - buffer_entry->pos;
		lacked_bytes = (buffer_entry->pos + lacked_bytes) - buffer_entry->nbytes;
		if (copied_bytes != 0) memcpy(keylen, buffer_entry->buff + buffer_entry->pos, (size_t)copied_bytes);
		buffer_entry_load(buffer_entry);
	}
	memcpy(keylen + copied_bytes, buffer_entry->buff + buffer_entry->pos, (size_t)lacked_bytes);
	buffer_entry->pos += lacked_bytes;
	
	lacked_bytes = *keylen;
	copied_bytes = 0;
	buf = (cmph_uint8 *)malloc(*keylen + sizeof(*keylen));
        memcpy(buf, keylen, sizeof(*keylen));
	if((buffer_entry->pos + lacked_bytes) > buffer_entry->nbytes) {
		copied_bytes = buffer_entry->nbytes - buffer_entry->pos;
		lacked_bytes = (buffer_entry->pos + lacked_bytes) - buffer_entry->nbytes;
		if (copied_bytes != 0) {
			memcpy(buf + sizeof(*keylen), buffer_entry->buff + buffer_entry->pos, (size_t)copied_bytes);
                }
		buffer_entry_load(buffer_entry);
	}        
	memcpy(buf+sizeof(*keylen)+copied_bytes, buffer_entry->buff + buffer_entry->pos, (size_t)lacked_bytes);
	buffer_entry->pos += lacked_bytes;
	return buf;
}

void buffer_entry_destroy(buffer_entry_t * buffer_entry)
{
  fclose(buffer_entry->fd);
  buffer_entry->fd = NULL;
  free(buffer_entry->buff);
  buffer_entry->buff = NULL;
  buffer_entry->capacity = 0;
  buffer_entry->nbytes = 0;
  buffer_entry->pos = 0;  
  buffer_entry->eof = 0;
  free(buffer_entry);
}
