#include "buffer_manager.h"
#include "buffer_entry.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
struct __buffer_manager_t
{
	cmph_uint32 memory_avail;         // memory available
	buffer_entry_t ** buffer_entries; // buffer entries to be managed
	cmph_uint32 nentries;             // number of entries to be managed
	cmph_uint32 *memory_avail_list;   // memory available list
	int pos_avail_list;               // current position in memory available list
};

buffer_manager_t * buffer_manager_new(cmph_uint32 memory_avail, cmph_uint32 nentries)
{
	cmph_uint32 memory_avail_entry, i;
	buffer_manager_t *buff_manager = (buffer_manager_t *)malloc(sizeof(buffer_manager_t));
	assert(buff_manager);
	buff_manager->memory_avail = memory_avail;
	buff_manager->buffer_entries = (buffer_entry_t **)calloc((size_t)nentries, sizeof(buffer_entry_t *));
	buff_manager->memory_avail_list = (cmph_uint32 *)calloc((size_t)nentries, sizeof(cmph_uint32));
	buff_manager->pos_avail_list = -1;
	buff_manager->nentries = nentries;
	memory_avail_entry = buff_manager->memory_avail/buff_manager->nentries + 1;
	for(i = 0; i < buff_manager->nentries; i++)
	{
		buff_manager->buffer_entries[i] = buffer_entry_new(memory_avail_entry);
	}	
	return buff_manager;
}

void buffer_manager_open(buffer_manager_t * buffer_manager, cmph_uint32 index, char * filename)
{
	buffer_entry_open(buffer_manager->buffer_entries[index], filename);
}

cmph_uint8 * buffer_manager_read_key(buffer_manager_t * buffer_manager, cmph_uint32 index, cmph_uint32 * keylen)
{
	cmph_uint8 * key = NULL;
	if (buffer_manager->pos_avail_list >= 0 ) // recovering memory
	{
		cmph_uint32 new_capacity = buffer_entry_get_capacity(buffer_manager->buffer_entries[index]) + buffer_manager->memory_avail_list[(buffer_manager->pos_avail_list)--];
		buffer_entry_set_capacity(buffer_manager->buffer_entries[index], new_capacity);
	}
	key = buffer_entry_read_key(buffer_manager->buffer_entries[index], keylen);
	if (key == NULL) // storing memory to be recovered
	{
		buffer_manager->memory_avail_list[++(buffer_manager->pos_avail_list)] = buffer_entry_get_capacity(buffer_manager->buffer_entries[index]);
	}
	return key;
}

void buffer_manager_destroy(buffer_manager_t * buffer_manager)
{ 
	cmph_uint32 i;
	for(i = 0; i < buffer_manager->nentries; i++)
	{
		buffer_entry_destroy(buffer_manager->buffer_entries[i]);
	}
	free(buffer_manager->memory_avail_list);
	free(buffer_manager->buffer_entries);
	free(buffer_manager);
}
