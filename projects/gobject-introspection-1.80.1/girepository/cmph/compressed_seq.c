#include "compressed_seq.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "bitbool.h"

// #define DEBUG
#include "debug.h"

static inline cmph_uint32 compressed_seq_i_log2(cmph_uint32 x)
{
	register cmph_uint32 res = 0;
	
	while(x > 1)
	{
		x >>= 1;
		res++;
	}
	return res;
};

void compressed_seq_init(compressed_seq_t * cs)
{
	select_init(&cs->sel);
	cs->n = 0;
	cs->rem_r = 0;
	cs->length_rems = 0;
	cs->total_length = 0;
	cs->store_table = 0;
}

void compressed_seq_destroy(compressed_seq_t * cs)
{
	free(cs->store_table);
	cs->store_table = 0;
	free(cs->length_rems);
	cs->length_rems = 0;
	select_destroy(&cs->sel);
};


void compressed_seq_generate(compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n)
{
	register cmph_uint32 i;
	// lengths: represents lengths of encoded values	
	register cmph_uint32 * lengths = (cmph_uint32 *)calloc(n, sizeof(cmph_uint32));
	register cmph_uint32 rems_mask;
	register cmph_uint32 stored_value;
	
	cs->n = n;
	cs->total_length = 0;
	
	for(i = 0; i < cs->n; i++)
	{
		if(vals_table[i] == 0)
		{
			lengths[i] = 0;
		}
		else
		{
			lengths[i] = compressed_seq_i_log2(vals_table[i] + 1);
			cs->total_length += lengths[i];
		};
	};
	
	if(cs->store_table)
	{
		free(cs->store_table);
	}
	cs->store_table = (cmph_uint32 *) calloc(((cs->total_length + 31) >> 5), sizeof(cmph_uint32));
	cs->total_length = 0;
	
	for(i = 0; i < cs->n; i++)
	{
		if(vals_table[i] == 0)
			continue;
		stored_value = vals_table[i] - ((1U << lengths[i]) - 1U);
		set_bits_at_pos(cs->store_table, cs->total_length, stored_value, lengths[i]);
		cs->total_length += lengths[i];
	};
	
	cs->rem_r = compressed_seq_i_log2(cs->total_length/cs->n);
	
	if(cs->rem_r == 0)
	{
		cs->rem_r = 1;
	}
	
	if(cs->length_rems)
	{
		free(cs->length_rems);
	}
	
	cs->length_rems = (cmph_uint32 *) calloc(BITS_TABLE_SIZE(cs->n, cs->rem_r), sizeof(cmph_uint32));
	
	rems_mask = (1U << cs->rem_r) - 1U;
	cs->total_length = 0;
	
	for(i = 0; i < cs->n; i++)
	{
		cs->total_length += lengths[i];
		set_bits_value(cs->length_rems, i, cs->total_length & rems_mask, cs->rem_r, rems_mask);
		lengths[i] = cs->total_length >> cs->rem_r;
	};
	
	select_init(&cs->sel);
	 
	// FABIANO: before it was (cs->total_length >> cs->rem_r) + 1. But I wiped out the + 1 because
	// I changed the select structure to work up to m, instead of up to m - 1.
	select_generate(&cs->sel, lengths, cs->n, (cs->total_length >> cs->rem_r));
	
	free(lengths);
};

cmph_uint32 compressed_seq_get_space_usage(compressed_seq_t * cs)
{
	register cmph_uint32 space_usage = select_get_space_usage(&cs->sel);
	space_usage += ((cs->total_length + 31) >> 5) * (cmph_uint32)sizeof(cmph_uint32) * 8;
	space_usage += BITS_TABLE_SIZE(cs->n, cs->rem_r) * (cmph_uint32)sizeof(cmph_uint32) * 8;
	return  4 * (cmph_uint32)sizeof(cmph_uint32) * 8 + space_usage;
}

cmph_uint32 compressed_seq_query(compressed_seq_t * cs, cmph_uint32 idx)
{
	register cmph_uint32 enc_idx, enc_length;
	register cmph_uint32 rems_mask;
	register cmph_uint32 stored_value;
	register cmph_uint32 sel_res;

	assert(idx < cs->n); // FABIANO ADDED

	rems_mask = (1U << cs->rem_r) - 1U;
	
	if(idx == 0)
	{
		enc_idx = 0;
		sel_res = select_query(&cs->sel, idx);
	}
	else
	{
		sel_res = select_query(&cs->sel, idx - 1);
		
		enc_idx = (sel_res - (idx - 1)) << cs->rem_r;
		enc_idx += get_bits_value(cs->length_rems, idx-1, cs->rem_r, rems_mask);
		
		sel_res = select_next_query(&cs->sel, sel_res);
	};

	enc_length = (sel_res - idx) << cs->rem_r;
	enc_length += get_bits_value(cs->length_rems, idx, cs->rem_r, rems_mask);
	enc_length -= enc_idx;
	if(enc_length == 0)
		return 0;
		
	stored_value = get_bits_at_pos(cs->store_table, enc_idx, enc_length);
	return stored_value + ((1U << enc_length) - 1U);
};

void compressed_seq_dump(compressed_seq_t * cs, char ** buf, cmph_uint32 * buflen)
{
	register cmph_uint32 sel_size = select_packed_size(&(cs->sel));
	register cmph_uint32 length_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r) * 4;
	register cmph_uint32 store_table_size = ((cs->total_length + 31) >> 5) * 4;
	register cmph_uint32 pos = 0;
	char * buf_sel = 0;
	cmph_uint32 buflen_sel = 0;
#ifdef DEBUG
	cmph_uint32 i;
#endif
	
	*buflen = 4*(cmph_uint32)sizeof(cmph_uint32) + sel_size +  length_rems_size + store_table_size;
	
	DEBUGP("sel_size = %u\n", sel_size);
	DEBUGP("length_rems_size = %u\n", length_rems_size);
	DEBUGP("store_table_size = %u\n", store_table_size);
	*buf = (char *)calloc(*buflen, sizeof(char));
	
	if (!*buf) 
	{
		*buflen = UINT_MAX;
		return;
	}
	
	// dumping n, rem_r and total_length
	memcpy(*buf, &(cs->n), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cs->n);
	
	memcpy(*buf + pos, &(cs->rem_r), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cs->rem_r);

	memcpy(*buf + pos, &(cs->total_length), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("total_length = %u\n", cs->total_length);

	
	// dumping sel
	select_dump(&cs->sel, &buf_sel, &buflen_sel);
	memcpy(*buf + pos, &buflen_sel, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("buflen_sel = %u\n", buflen_sel);

	memcpy(*buf + pos, buf_sel, buflen_sel);
	#ifdef DEBUG
	i = 0;
	for(i = 0; i < buflen_sel; i++)
	{
	    DEBUGP("pos = %u  -- buf_sel[%u] = %u\n", pos, i, *(*buf + pos + i));
	}
	#endif
	pos += buflen_sel;
	
	free(buf_sel);
	
	// dumping length_rems
	memcpy(*buf + pos, cs->length_rems, length_rems_size);
	#ifdef DEBUG
	for(i = 0; i < length_rems_size; i++)
	{
	    DEBUGP("pos = %u -- length_rems_size = %u  -- length_rems[%u] = %u\n", pos, length_rems_size, i, *(*buf + pos + i));
	}
	#endif
	pos += length_rems_size;

	// dumping store_table
	memcpy(*buf + pos, cs->store_table, store_table_size);

	#ifdef DEBUG
	for(i = 0; i < store_table_size; i++)
	{
	    DEBUGP("pos = %u -- store_table_size = %u  -- store_table[%u] = %u\n", pos, store_table_size, i, *(*buf + pos + i));
	}
	#endif
	DEBUGP("Dumped compressed sequence structure with size %u bytes\n", *buflen);
}

void compressed_seq_load(compressed_seq_t * cs, const char * buf, cmph_uint32 buflen)
{
	register cmph_uint32 pos = 0;
	cmph_uint32 buflen_sel = 0;
	register cmph_uint32 length_rems_size = 0;
	register cmph_uint32 store_table_size = 0;
#ifdef DEBUG
	cmph_uint32 i;
#endif
	
	// loading n, rem_r and total_length
	memcpy(&(cs->n), buf, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cs->n);

	memcpy(&(cs->rem_r), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cs->rem_r);

	memcpy(&(cs->total_length), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("total_length = %u\n", cs->total_length);
	
	// loading sel
	memcpy(&buflen_sel, buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("buflen_sel = %u\n", buflen_sel);

	select_load(&cs->sel, buf + pos, buflen_sel);
	#ifdef DEBUG
	i = 0;
	for(i = 0; i < buflen_sel; i++)
	{
	    DEBUGP("pos = %u  -- buf_sel[%u] = %u\n", pos, i, *(buf + pos + i));
	}
	#endif
	pos += buflen_sel;
	
	// loading length_rems
	if(cs->length_rems)
	{
		free(cs->length_rems);
	}
	length_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r);
	cs->length_rems = (cmph_uint32 *) calloc(length_rems_size, sizeof(cmph_uint32));
	length_rems_size *= 4;
	memcpy(cs->length_rems, buf + pos, length_rems_size);
	
	#ifdef DEBUG
	for(i = 0; i < length_rems_size; i++)
	{
	    DEBUGP("pos = %u -- length_rems_size = %u  -- length_rems[%u] = %u\n", pos, length_rems_size, i, *(buf + pos + i));
	}
	#endif
	pos += length_rems_size;

	// loading store_table
	store_table_size = ((cs->total_length + 31) >> 5);
	if(cs->store_table)
	{
		free(cs->store_table);
	}
	cs->store_table = (cmph_uint32 *) calloc(store_table_size, sizeof(cmph_uint32));
        store_table_size *= 4;
	memcpy(cs->store_table, buf + pos, store_table_size);
	
	#ifdef DEBUG
	for(i = 0; i < store_table_size; i++)
	{
	    DEBUGP("pos = %u -- store_table_size = %u  -- store_table[%u] = %u\n", pos, store_table_size, i, *(buf + pos + i));
	}
	#endif

	DEBUGP("Loaded compressed sequence structure with size %u bytes\n", buflen);
}

void compressed_seq_pack(compressed_seq_t *cs, void *cs_packed)
{
	if (cs && cs_packed)
	{
		char *buf = NULL;
		cmph_uint32 buflen = 0;
		compressed_seq_dump(cs, &buf, &buflen);
		memcpy(cs_packed, buf, buflen);
		free(buf);
	}

}

cmph_uint32 compressed_seq_packed_size(compressed_seq_t *cs)
{
	register cmph_uint32 sel_size = select_packed_size(&cs->sel);
	register cmph_uint32 store_table_size = ((cs->total_length + 31) >> 5) * (cmph_uint32)sizeof(cmph_uint32);
	register cmph_uint32 length_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r) * (cmph_uint32)sizeof(cmph_uint32);
	return 4 * (cmph_uint32)sizeof(cmph_uint32) + sel_size + store_table_size + length_rems_size;
}


cmph_uint32 compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx)
{
	// unpacking cs_packed
	register cmph_uint32 *ptr = (cmph_uint32 *)cs_packed;
	register cmph_uint32 n = *ptr++;
	register cmph_uint32 rem_r = *ptr++;
	register cmph_uint32 buflen_sel, length_rems_size, enc_idx, enc_length;
	// compressed sequence query computation
	register cmph_uint32 rems_mask, stored_value, sel_res;
	register cmph_uint32 *sel_packed, *length_rems, *store_table;

	ptr++; // skipping total_length 
// 	register cmph_uint32 total_length = *ptr++;
	buflen_sel = *ptr++;
	sel_packed = ptr;
	length_rems = (ptr += (buflen_sel >> 2));
	length_rems_size = BITS_TABLE_SIZE(n, rem_r);
	store_table = (ptr += length_rems_size);


	rems_mask = (1U << rem_r) - 1U;
	
	if(idx == 0)
	{
		enc_idx = 0;
		sel_res = select_query_packed(sel_packed, idx);
	}
	else
	{
		sel_res = select_query_packed(sel_packed, idx - 1);
		
		enc_idx = (sel_res - (idx - 1)) << rem_r;
		enc_idx += get_bits_value(length_rems, idx-1, rem_r, rems_mask);
		
		sel_res = select_next_query_packed(sel_packed, sel_res);
	};

	enc_length = (sel_res - idx) << rem_r;
	enc_length += get_bits_value(length_rems, idx, rem_r, rems_mask);
	enc_length -= enc_idx;
	if(enc_length == 0)
		return 0;
		
	stored_value = get_bits_at_pos(store_table, enc_idx, enc_length);
	return stored_value + ((1U << enc_length) - 1U);
}
