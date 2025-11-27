#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<time.h>
#include<assert.h>
#include<limits.h>
#include<errno.h>

#include "cmph_structs.h"
#include "chd_structs.h"
#include "chd.h"
#include "bitbool.h"
//#define DEBUG
#include "debug.h"

chd_config_data_t *chd_config_new(cmph_config_t *mph)
{
	cmph_io_adapter_t *key_source = mph->key_source;
	chd_config_data_t *chd;
	chd = (chd_config_data_t *)malloc(sizeof(chd_config_data_t));
	assert(chd);
	memset(chd, 0, sizeof(chd_config_data_t));

	chd->chd_ph = cmph_config_new(key_source);
	cmph_config_set_algo(chd->chd_ph, CMPH_CHD_PH);

	return chd;
}

void chd_config_destroy(cmph_config_t *mph)
{
	chd_config_data_t *data = (chd_config_data_t *) mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	if(data->chd_ph)
	{
		cmph_config_destroy(data->chd_ph);
		data->chd_ph = NULL;
	}
	free(data);
}


void chd_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	chd_config_data_t *data = (chd_config_data_t *) mph->data;
	cmph_config_set_hashfuncs(data->chd_ph, hashfuncs);
}


void chd_config_set_b(cmph_config_t *mph, cmph_uint32 keys_per_bucket)
{
	chd_config_data_t *data = (chd_config_data_t *) mph->data;
	cmph_config_set_b(data->chd_ph, keys_per_bucket);
}


void chd_config_set_keys_per_bin(cmph_config_t *mph, cmph_uint32 keys_per_bin)
{
	chd_config_data_t *data = (chd_config_data_t *) mph->data;
	cmph_config_set_keys_per_bin(data->chd_ph, keys_per_bin);
}


cmph_t *chd_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	chd_data_t *chdf = NULL;
	chd_config_data_t *chd = (chd_config_data_t *)mph->data;
	chd_ph_config_data_t * chd_ph = (chd_ph_config_data_t *)chd->chd_ph->data;
	compressed_rank_t cr;
	
	register cmph_t * chd_phf = NULL;
	register cmph_uint32 packed_chd_phf_size = 0; 
	cmph_uint8 * packed_chd_phf = NULL;
	
	register cmph_uint32 packed_cr_size = 0; 
	cmph_uint8 * packed_cr = NULL;

	register cmph_uint32 i, idx, nkeys, nvals, nbins;
	cmph_uint32 * vals_table = NULL;
	register cmph_uint32 * occup_table = NULL;
	#ifdef CMPH_TIMING
	double construction_time_begin = 0.0;
	double construction_time = 0.0;
	ELAPSED_TIME_IN_SECONDS(&construction_time_begin);
	#endif

	cmph_config_set_verbosity(chd->chd_ph, mph->verbosity);	
	cmph_config_set_graphsize(chd->chd_ph, c);
	
	if (mph->verbosity)
	{
		fprintf(stderr, "Generating a CHD_PH perfect hash function with a load factor equal to %.3f\n", c);
	}
	
	chd_phf = cmph_new(chd->chd_ph);
	
	if(chd_phf == NULL) 
	{
		return NULL;
	}
	
	packed_chd_phf_size = cmph_packed_size(chd_phf); 
	DEBUGP("packed_chd_phf_size = %u\n", packed_chd_phf_size);
	
	/* Make sure that we have enough space to pack the mphf. */
	packed_chd_phf = calloc((size_t)packed_chd_phf_size,(size_t)1);

	/* Pack the mphf. */
	cmph_pack(chd_phf, packed_chd_phf);

	cmph_destroy(chd_phf);
	
	
	if (mph->verbosity)
	{
		fprintf(stderr, "Compressing the range of the resulting CHD_PH perfect hash function\n");
	}

	compressed_rank_init(&cr);
	nbins = chd_ph->n;
	nkeys = chd_ph->m;
	nvals =  nbins - nkeys; 
	
	vals_table = (cmph_uint32 *)calloc(nvals, sizeof(cmph_uint32));
	occup_table = (cmph_uint32 *)chd_ph->occup_table;
	
	for(i = 0, idx = 0; i < nbins; i++)
	{
		if(!GETBIT32(occup_table, i))
		{
			vals_table[idx++] = i;
		}
	}
	
	compressed_rank_generate(&cr, vals_table, nvals);
	free(vals_table);
	
	packed_cr_size = compressed_rank_packed_size(&cr);
	packed_cr = (cmph_uint8 *) calloc(packed_cr_size, sizeof(cmph_uint8));
	compressed_rank_pack(&cr, packed_cr);
	compressed_rank_destroy(&cr);

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	chdf = (chd_data_t *)malloc(sizeof(chd_data_t));
	
	chdf->packed_cr = packed_cr;
	packed_cr = NULL; //transfer memory ownership

	chdf->packed_chd_phf = packed_chd_phf;
	packed_chd_phf = NULL; //transfer memory ownership
	
	chdf->packed_chd_phf_size = packed_chd_phf_size;
	chdf->packed_cr_size = packed_cr_size;
	
	mphf->data = chdf;
	mphf->size = nkeys;

	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	#ifdef CMPH_TIMING	
	ELAPSED_TIME_IN_SECONDS(&construction_time);
	register cmph_uint32 space_usage =  chd_packed_size(mphf)*8;
	construction_time = construction_time - construction_time_begin;
	fprintf(stdout, "%u\t%.2f\t%u\t%.4f\t%.4f\n", nkeys, c, chd_ph->keys_per_bucket, construction_time, space_usage/(double)nkeys);
	#endif	

	return mphf;
}

void chd_load(FILE *fd, cmph_t *mphf)
{
	register size_t nbytes;
	chd_data_t *chd = (chd_data_t *)malloc(sizeof(chd_data_t));

	DEBUGP("Loading chd mphf\n");
	mphf->data = chd;

	nbytes = fread(&chd->packed_chd_phf_size, sizeof(cmph_uint32), (size_t)1, fd);
	DEBUGP("Loading CHD_PH perfect hash function with %u bytes to disk\n", chd->packed_chd_phf_size);
	chd->packed_chd_phf = (cmph_uint8 *) calloc((size_t)chd->packed_chd_phf_size,(size_t)1);
	nbytes = fread(chd->packed_chd_phf, chd->packed_chd_phf_size, (size_t)1, fd);

	nbytes = fread(&chd->packed_cr_size, sizeof(cmph_uint32), (size_t)1, fd);
	DEBUGP("Loading Compressed rank structure, which has %u bytes\n", chd->packed_cr_size);
	chd->packed_cr = (cmph_uint8 *) calloc((size_t)chd->packed_cr_size, (size_t)1);
	nbytes = fread(chd->packed_cr, chd->packed_cr_size, (size_t)1, fd);
	if (nbytes == 0 && ferror(fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
        }

}

int chd_dump(cmph_t *mphf, FILE *fd)
{
	register size_t nbytes;
	chd_data_t *data = (chd_data_t *)mphf->data;
	
	__cmph_dump(mphf, fd);
	// Dumping CHD_PH perfect hash function

	DEBUGP("Dumping CHD_PH perfect hash function with %u bytes to disk\n", data->packed_chd_phf_size);
	nbytes = fwrite(&data->packed_chd_phf_size, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(data->packed_chd_phf, data->packed_chd_phf_size, (size_t)1, fd);

	DEBUGP("Dumping compressed rank structure with %u bytes to disk\n", data->packed_cr_size);
	nbytes = fwrite(&data->packed_cr_size, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(data->packed_cr, data->packed_cr_size, (size_t)1, fd);
	if (nbytes == 0 && ferror(fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return 0;
        }

	return 1;
}

void chd_destroy(cmph_t *mphf)
{
	chd_data_t *data = (chd_data_t *)mphf->data;
	free(data->packed_chd_phf);
	free(data->packed_cr);
	free(data);
	free(mphf);
}

static inline cmph_uint32 _chd_search(void * packed_chd_phf, void * packed_cr, const char *key, cmph_uint32 keylen)
{
	register cmph_uint32 bin_idx = cmph_search_packed(packed_chd_phf, key, keylen);
	register cmph_uint32 rank = compressed_rank_query_packed(packed_cr, bin_idx);
	return bin_idx - rank;
}

cmph_uint32 chd_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	register chd_data_t * chd = mphf->data;
	return _chd_search(chd->packed_chd_phf, chd->packed_cr, key, keylen);
}

void chd_pack(cmph_t *mphf, void *packed_mphf)
{
	chd_data_t *data = (chd_data_t *)mphf->data;
	cmph_uint32 * ptr = packed_mphf;
	cmph_uint8 * ptr8;

	// packing packed_cr_size and packed_cr
	*ptr = data->packed_cr_size;
	ptr8 =  (cmph_uint8 *) (ptr + 1);
	
	memcpy(ptr8, data->packed_cr, data->packed_cr_size);
	ptr8 += data->packed_cr_size;
	
	ptr = (cmph_uint32 *) ptr8;
	*ptr = data->packed_chd_phf_size;

	ptr8 =  (cmph_uint8 *) (ptr + 1);
	memcpy(ptr8, data->packed_chd_phf, data->packed_chd_phf_size);
}

cmph_uint32 chd_packed_size(cmph_t *mphf)
{
	register chd_data_t *data = (chd_data_t *)mphf->data;
	return (cmph_uint32)(sizeof(CMPH_ALGO) + 2*sizeof(cmph_uint32) + data->packed_cr_size + data->packed_chd_phf_size);

}

cmph_uint32 chd_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{

	register cmph_uint32 * ptr = packed_mphf;
	register cmph_uint32 packed_cr_size = *ptr++;
	register cmph_uint8 * packed_chd_phf = ((cmph_uint8 *) ptr) + packed_cr_size + sizeof(cmph_uint32);
	return _chd_search(packed_chd_phf, ptr, key, keylen);
}


