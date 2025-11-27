#include<stdlib.h>
#include<stdio.h>
#include<limits.h>
#include<string.h>
#include"compressed_rank.h"
#include"bitbool.h"
// #define DEBUG
#include"debug.h"
static inline cmph_uint32 compressed_rank_i_log2(cmph_uint32 x)
{
	register cmph_uint32 res = 0;
	
	while(x > 1)
	{
		x >>= 1;
		res++;
	}
	return res;
};

void compressed_rank_init(compressed_rank_t * cr)
{
	cr->max_val = 0;
	cr->n = 0;
	cr->rem_r = 0;
	select_init(&cr->sel);
	cr->vals_rems = 0;
}

void compressed_rank_destroy(compressed_rank_t * cr)
{
	free(cr->vals_rems);
	cr->vals_rems = 0;
	select_destroy(&cr->sel);
}

void compressed_rank_generate(compressed_rank_t * cr, cmph_uint32 * vals_table, cmph_uint32 n)
{
	register cmph_uint32 i,j;
	register cmph_uint32 rems_mask;
	register cmph_uint32 * select_vec = 0;
	cr->n = n;
	cr->max_val = vals_table[cr->n - 1];
	cr->rem_r = compressed_rank_i_log2(cr->max_val/cr->n);
	if(cr->rem_r == 0)
	{
		cr->rem_r = 1;
	}
	select_vec = (cmph_uint32 *) calloc(cr->max_val >> cr->rem_r, sizeof(cmph_uint32));
	cr->vals_rems = (cmph_uint32 *) calloc(BITS_TABLE_SIZE(cr->n, cr->rem_r), sizeof(cmph_uint32));
	rems_mask = (1U << cr->rem_r) - 1U;
	
	for(i = 0; i < cr->n; i++)
	{
		set_bits_value(cr->vals_rems, i, vals_table[i] & rems_mask, cr->rem_r, rems_mask);
	}

	for(i = 1, j = 0; i <= cr->max_val >> cr->rem_r; i++)
	{
		while(i > (vals_table[j] >> cr->rem_r))
		{
			j++;
		}
		select_vec[i - 1] = j;
	};


	// FABIANO: before it was (cr->total_length >> cr->rem_r) + 1. But I wiped out the + 1 because
	// I changed the select structure to work up to m, instead of up to m - 1.
	select_generate(&cr->sel, select_vec, cr->max_val >> cr->rem_r, cr->n);

	free(select_vec);
}

cmph_uint32 compressed_rank_query(compressed_rank_t * cr, cmph_uint32 idx)
{
	register cmph_uint32 rems_mask;
	register cmph_uint32 val_quot, val_rem;
	register cmph_uint32 sel_res, rank;
	
	if(idx > cr->max_val)
	{
		return cr->n;
	}
	
	val_quot = idx >> cr->rem_r;
	rems_mask = (1U << cr->rem_r) - 1U;
	val_rem = idx & rems_mask;
	if(val_quot == 0)
	{
		rank = sel_res = 0;
	}
	else
	{
		sel_res = select_query(&cr->sel, val_quot - 1) + 1;
		rank = sel_res - val_quot;
	}
	
	do
	{
		if(GETBIT32(cr->sel.bits_vec, sel_res))
		{
			break;
		}
		if(get_bits_value(cr->vals_rems, rank, cr->rem_r, rems_mask) >= val_rem)
		{
			break;
		}
		sel_res++;
		rank++;
	} while(1);	
	
	return rank;
}

cmph_uint32 compressed_rank_get_space_usage(compressed_rank_t * cr)
{
	register cmph_uint32 space_usage = select_get_space_usage(&cr->sel);
	space_usage += BITS_TABLE_SIZE(cr->n, cr->rem_r)*(cmph_uint32)sizeof(cmph_uint32)*8;
	space_usage += 3*(cmph_uint32)sizeof(cmph_uint32)*8;
	return space_usage;
}

void compressed_rank_dump(compressed_rank_t * cr, char **buf, cmph_uint32 *buflen)
{
	register cmph_uint32 sel_size = select_packed_size(&(cr->sel));
	register cmph_uint32 vals_rems_size = BITS_TABLE_SIZE(cr->n, cr->rem_r) * (cmph_uint32)sizeof(cmph_uint32);
	register cmph_uint32 pos = 0;
	char * buf_sel = 0;
	cmph_uint32 buflen_sel = 0;
#ifdef DEBUG
	cmph_uint32 i;
#endif
	
	*buflen = 4*(cmph_uint32)sizeof(cmph_uint32) + sel_size +  vals_rems_size;
	
	DEBUGP("sel_size = %u\n", sel_size);
	DEBUGP("vals_rems_size = %u\n", vals_rems_size);
	
	*buf = (char *)calloc(*buflen, sizeof(char));
	
	if (!*buf) 
	{
		*buflen = UINT_MAX;
		return;
	}
	
	// dumping max_val, n and rem_r
	memcpy(*buf, &(cr->max_val), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("max_val = %u\n", cr->max_val);

	memcpy(*buf + pos, &(cr->n), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cr->n);
	
	memcpy(*buf + pos, &(cr->rem_r), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cr->rem_r);

	// dumping sel
	select_dump(&cr->sel, &buf_sel, &buflen_sel);
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
	
	// dumping vals_rems
	memcpy(*buf + pos, cr->vals_rems, vals_rems_size);
	#ifdef DEBUG	
	for(i = 0; i < vals_rems_size; i++)
	{
	    DEBUGP("pos = %u -- vals_rems_size = %u  -- vals_rems[%u] = %u\n", pos, vals_rems_size, i, *(*buf + pos + i));
	}
	#endif
	pos += vals_rems_size;

	DEBUGP("Dumped compressed rank structure with size %u bytes\n", *buflen);
}

void compressed_rank_load(compressed_rank_t * cr, const char *buf, cmph_uint32 buflen)
{
	register cmph_uint32 pos = 0;
	cmph_uint32 buflen_sel = 0;
	register cmph_uint32 vals_rems_size = 0;
#ifdef DEBUG
	cmph_uint32 i;
#endif
	
	// loading max_val, n, and rem_r
	memcpy(&(cr->max_val), buf, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("max_val = %u\n", cr->max_val);

	memcpy(&(cr->n), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cr->n);

	memcpy(&(cr->rem_r), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cr->rem_r);
	
	// loading sel
	memcpy(&buflen_sel, buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("buflen_sel = %u\n", buflen_sel);

	select_load(&cr->sel, buf + pos, buflen_sel);
	#ifdef DEBUG	
	i = 0;
	for(i = 0; i < buflen_sel; i++)
	{
	    DEBUGP("pos = %u  -- buf_sel[%u] = %u\n", pos, i, *(buf + pos + i));
	}
	#endif
	pos += buflen_sel;
	
	// loading vals_rems
	if(cr->vals_rems)
	{
		free(cr->vals_rems);
	}
	vals_rems_size = BITS_TABLE_SIZE(cr->n, cr->rem_r);
	cr->vals_rems = (cmph_uint32 *) calloc(vals_rems_size, sizeof(cmph_uint32));
	vals_rems_size *= 4;
	memcpy(cr->vals_rems, buf + pos, vals_rems_size);
	
	#ifdef DEBUG	
	for(i = 0; i < vals_rems_size; i++)
	{
	    DEBUGP("pos = %u -- vals_rems_size = %u  -- vals_rems[%u] = %u\n", pos, vals_rems_size, i, *(buf + pos + i));
	}
	#endif
	pos += vals_rems_size;
	
	DEBUGP("Loaded compressed rank structure with size %u bytes\n", buflen);
}



void compressed_rank_pack(compressed_rank_t *cr, void *cr_packed)
{
	if (cr && cr_packed)
	{
		char *buf = NULL;
		cmph_uint32 buflen = 0;
		compressed_rank_dump(cr, &buf, &buflen);
		memcpy(cr_packed, buf, buflen);
		free(buf);
	}
}

cmph_uint32 compressed_rank_packed_size(compressed_rank_t *cr)
{
	register cmph_uint32 sel_size = select_packed_size(&cr->sel);
	register cmph_uint32 vals_rems_size = BITS_TABLE_SIZE(cr->n, cr->rem_r) * (cmph_uint32)sizeof(cmph_uint32);	
	return 4 * (cmph_uint32)sizeof(cmph_uint32)  + sel_size +  vals_rems_size;
}

cmph_uint32 compressed_rank_query_packed(void * cr_packed, cmph_uint32 idx)
{
	// unpacking cr_packed
	register cmph_uint32 *ptr = (cmph_uint32 *)cr_packed;
	register cmph_uint32 max_val = *ptr++;
	register cmph_uint32 n = *ptr++;
	register cmph_uint32 rem_r = *ptr++;
	register cmph_uint32 buflen_sel = *ptr++;
	register cmph_uint32 * sel_packed = ptr;
	
	register cmph_uint32 * bits_vec = sel_packed + 2; // skipping n and m

	register cmph_uint32 * vals_rems = (ptr += (buflen_sel >> 2)); 

	// compressed sequence query computation
	register cmph_uint32 rems_mask;
	register cmph_uint32 val_quot, val_rem;
	register cmph_uint32 sel_res, rank;
	
	if(idx > max_val)
	{
		return n;
	}
	
	val_quot = idx >> rem_r; 	
	rems_mask = (1U << rem_r) - 1U; 
	val_rem = idx & rems_mask; 
	if(val_quot == 0)
	{
		rank = sel_res = 0;
	}
	else
	{
		sel_res = select_query_packed(sel_packed, val_quot - 1) + 1;
		rank = sel_res - val_quot;
	}
	
	do
	{
		if(GETBIT32(bits_vec, sel_res))
		{
			break;
		}
		if(get_bits_value(vals_rems, rank, rem_r, rems_mask) >= val_rem)
		{
			break;
		}
		sel_res++;
		rank++;
	} while(1);	
	
	return rank;
}



