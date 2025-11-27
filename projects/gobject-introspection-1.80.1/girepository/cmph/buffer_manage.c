#include "buffer_manage.h"
#include "buffer_entry.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
struct __buffer_manage_t
{
	cmph_uint32 memory_avail;         // memory available
	buffer_entry_t ** buffer_entries; // buffer entries to be managed
	cmph_uint32 nentries;             // number of entries to be managed
	cmph_uint32 *memory_avail_list;   // memory available list
	int pos_avail_list;               // current position in memory available list
};

buffer_manage_t * buffer_manage_new(cmph_uint32 memory_avail, cmph_uint32 nentries)
{
	cmph_uint32 memory_avail_entry, i;
	buffer_manage_t *buff_manage = (buffer_manage_t *)malloc(sizeof(buffer_manage_t));
	assert(buff_manage);
	buff_manage->memory_avail = memory_avail;
	buff_manage->buffer_entries = (buffer_entry_t **)calloc((size_t)nentries, sizeof(buffer_entry_t *));
	buff_manage->memory_avail_list = (cmph_uint32 *)calloc((size_t)nentries, sizeof(cmph_uint32));
	buff_manage->pos_avail_list = -1;
	buff_manage->nentries = nentries;
	memory_avail_entry = buff_manage->memory_avail/buff_manage->nentries + 1;
	for(i = 0; i < buff_manage->nentries; i++)
	{
		buff_manage->buffer_entries[i] = buffer_entry_new(memory_avail_entry);
	}	
	return buff_manage;
}

void buffer_manage_open(buffer_manage_t * buffer_manage, cmph_uint32 index, char * filename)
{
	buffer_entry_open(buffer_manage->buffer_entries[index], filename);
}

cmph_uint8 * buffer_manage_read_key(buffer_manage_t * buffer_manage, cmph_uint32 index)
{
	cmph_uint8 * key = NULL;
	if (buffer_manage->pos_avail_list >= 0 ) // recovering memory
	{
		cmph_uint32 new_capacity = buffer_entry_get_capacity(buffer_manage->buffer_entries[index]) + buffer_manage->memory_avail_list[(buffer_manage->pos_avail_list)--];
		buffer_entry_set_capacity(buffer_manage->buffer_entries[index], new_capacity);
		//fprintf(stderr, "recovering memory\n");
	}
	key = buffer_entry_read_key(buffer_manage->buffer_entries[index]);
	if (key == NULL) // storing memory to be recovered
	{
		buffer_manage->memory_avail_list[++(buffer_manage->pos_avail_list)] = buffer_entry_get_capacity(buffer_manage->buffer_entries[index]);
		//fprintf(stderr, "storing memory to be recovered\n");
	}
	return key;
}

void buffer_manage_destroy(buffer_manage_t * buffer_manage)
{ 
	cmph_uint32 i;
	for(i = 0; i < buffer_manage->nentries; i++)
	{
		buffer_entry_destroy(buffer_manage->buffer_entries[i]);
	}
	free(buffer_manage->memory_avail_list);
	free(buffer_manage->buffer_entries);
	free(buffer_manage);
}
