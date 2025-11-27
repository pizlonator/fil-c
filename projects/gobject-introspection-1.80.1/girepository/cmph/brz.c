#include "graph.h"
#include "fch.h"
#include "fch_structs.h"
#include "bmz8.h"
#include "bmz8_structs.h"
#include "brz.h"
#include "cmph_structs.h"
#include "brz_structs.h"
#include "buffer_manager.h"
#include "cmph.h"
#include "hash.h"
#include "bitbool.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#define MAX_BUCKET_SIZE 255
//#define DEBUG
#include "debug.h"

#if defined (__ia64) || defined (__x86_64__) || defined (_WIN64)
# define __brz_use_64bit__
#endif

static int brz_gen_mphf(cmph_config_t *mph);
static cmph_uint32 brz_min_index(cmph_uint32 * vector, cmph_uint32 n);
static void brz_destroy_keys_vd(cmph_uint8 ** keys_vd, cmph_uint32 nkeys);
static char * brz_copy_partial_fch_mphf(brz_config_data_t *brz, fch_data_t * fchf, cmph_uint32 index,  cmph_uint32 *buflen);
static char * brz_copy_partial_bmz8_mphf(brz_config_data_t *brz, bmz8_data_t * bmzf, cmph_uint32 index,  cmph_uint32 *buflen);
brz_config_data_t *brz_config_new(void)
{
	brz_config_data_t *brz = NULL; 	
	brz = (brz_config_data_t *)malloc(sizeof(brz_config_data_t));
	brz->algo = CMPH_FCH;
	brz->b = 128;
	brz->hashfuncs[0] = CMPH_HASH_JENKINS;
	brz->hashfuncs[1] = CMPH_HASH_JENKINS;
	brz->hashfuncs[2] = CMPH_HASH_JENKINS;
	brz->size   = NULL;
	brz->offset = NULL;
	brz->g      = NULL;
	brz->h1 = NULL;
	brz->h2 = NULL;
	brz->h0 = NULL;
	brz->memory_availability = 1024*1024;
	brz->tmp_dir = (cmph_uint8 *)calloc((size_t)10, sizeof(cmph_uint8));
	brz->mphf_fd = NULL;
	strcpy((char *)(brz->tmp_dir), "/var/tmp/"); 
	assert(brz);
	return brz;
}

void brz_config_destroy(cmph_config_t *mph)
{
	brz_config_data_t *data = (brz_config_data_t *)mph->data;
	free(data->tmp_dir);
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void brz_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 3) break; //brz only uses three hash functions
		brz->hashfuncs[i] = *hashptr;	
		++i, ++hashptr;
	}
}

void brz_config_set_memory_availability(cmph_config_t *mph, cmph_uint32 memory_availability)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(memory_availability > 0) brz->memory_availability = memory_availability*1024*1024;
}

void brz_config_set_tmp_dir(cmph_config_t *mph, cmph_uint8 *tmp_dir)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(tmp_dir)
	{
		size_t len = strlen((char *)tmp_dir);
		free(brz->tmp_dir);
		if(tmp_dir[len-1] != '/')
		{
			brz->tmp_dir = (cmph_uint8 *)calloc((size_t)len+2, sizeof(cmph_uint8));
			sprintf((char *)(brz->tmp_dir), "%s/", (char *)tmp_dir); 
		}
		else
		{
			brz->tmp_dir = (cmph_uint8 *)calloc((size_t)len+1, sizeof(cmph_uint8));
			sprintf((char *)(brz->tmp_dir), "%s", (char *)tmp_dir); 
		}
		
	}
}

void brz_config_set_mphf_fd(cmph_config_t *mph, FILE *mphf_fd)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	brz->mphf_fd = mphf_fd;
	assert(brz->mphf_fd);
}

void brz_config_set_b(cmph_config_t *mph, cmph_uint32 b)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(b <= 64 || b >= 175) 
	{
		b =  128;
	}
	brz->b = (cmph_uint8)b;
}

void brz_config_set_algo(cmph_config_t *mph, CMPH_ALGO algo) 
{
	if (algo == CMPH_BMZ8 || algo == CMPH_FCH) // supported algorithms
	{
		brz_config_data_t *brz = (brz_config_data_t *)mph->data;
		brz->algo = algo;
	}
}

cmph_t *brz_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	brz_data_t *brzf = NULL;
	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	brz_config_data_t *brz;

	DEBUGP("c: %f\n", c);
	brz = (brz_config_data_t *)mph->data;
	switch(brz->algo) // validating restrictions over parameter c.
	{
		case CMPH_BMZ8:
			if (c == 0 || c >= 2.0) c = 1;
			break;
		case CMPH_FCH:
			if (c <= 2.0) c = 2.6;
			break;
		default:
			assert(0);
	}
	brz->c = c;
	brz->m = mph->key_source->nkeys;
	DEBUGP("m: %u\n", brz->m);
        brz->k = (cmph_uint32)ceil(brz->m/((double)brz->b));
	DEBUGP("k: %u\n", brz->k);
	brz->size   = (cmph_uint8 *) calloc((size_t)brz->k, sizeof(cmph_uint8));
	
	// Clustering the keys by graph id.
	if (mph->verbosity)
	{
		fprintf(stderr, "Partitioning the set of keys.\n");
	}
		
	while(1)
	{
		int ok;
		DEBUGP("hash function 3\n");
		brz->h0 = hash_state_new(brz->hashfuncs[2], brz->k);
		DEBUGP("Generating graphs\n");
		ok = brz_gen_mphf(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(brz->h0);
			brz->h0 = NULL;
			DEBUGP("%u iterations remaining to create the graphs in a external file\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "Failure: A graph with more than 255 keys was created - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;	
	}
	if (iterations == 0) 
	{
		DEBUGP("Graphs with more than 255 keys were created in all 20 iterations\n");
		free(brz->size);
		return NULL;
	}
	DEBUGP("Graphs generated\n");
	
	brz->offset = (cmph_uint32 *)calloc((size_t)brz->k, sizeof(cmph_uint32));
	for (i = 1; i < brz->k; ++i)
	{
		brz->offset[i] = brz->size[i-1] + brz->offset[i-1];
	}
	// Generating a mphf
	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	brzf = (brz_data_t *)malloc(sizeof(brz_data_t));
	brzf->g = brz->g;
	brz->g = NULL; //transfer memory ownership
	brzf->h1 = brz->h1;
	brz->h1 = NULL; //transfer memory ownership
	brzf->h2 = brz->h2;
	brz->h2 = NULL; //transfer memory ownership
	brzf->h0 = brz->h0;
	brz->h0 = NULL; //transfer memory ownership
	brzf->size = brz->size;
	brz->size = NULL; //transfer memory ownership
	brzf->offset = brz->offset;
	brz->offset = NULL; //transfer memory ownership
	brzf->k = brz->k;
	brzf->c = brz->c;
	brzf->m = brz->m;
	brzf->algo = brz->algo;
	mphf->data = brzf;
	mphf->size = brz->m;	
	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

static int brz_gen_mphf(cmph_config_t *mph)
{
	cmph_uint32 i, e, error;
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	cmph_uint32 memory_usage = 0;
	cmph_uint32 nkeys_in_buffer = 0;
	cmph_uint8 *buffer = (cmph_uint8 *)malloc((size_t)brz->memory_availability);
	cmph_uint32 *buckets_size = (cmph_uint32 *)calloc((size_t)brz->k, sizeof(cmph_uint32));
	cmph_uint32 *keys_index = NULL;
	cmph_uint8 **buffer_merge = NULL;
	cmph_uint32 *buffer_h0 = NULL;
	cmph_uint32 nflushes = 0;
	cmph_uint32 h0;
	register size_t nbytes;
	FILE *  tmp_fd = NULL;
	buffer_manager_t * buff_manager = NULL;
	char *filename = NULL;
	char *key = NULL;
	cmph_uint32 keylen;
	cmph_uint32 cur_bucket = 0;
	cmph_uint8 nkeys_vd = 0;
	cmph_uint8 ** keys_vd = NULL;
	
	mph->key_source->rewind(mph->key_source->data);
	DEBUGP("Generating graphs from %u keys\n", brz->m);
	// Partitioning
	for (e = 0; e < brz->m; ++e)
	{
		mph->key_source->read(mph->key_source->data, &key, &keylen);

		/* Buffers management */
		if (memory_usage + keylen + sizeof(keylen) > brz->memory_availability) // flush buffers 
		{
			cmph_uint32 value, sum, keylen1;
			if(mph->verbosity)
			{
				fprintf(stderr, "Flushing  %u\n", nkeys_in_buffer);
			}
			value = buckets_size[0];
			sum = 0;
			keylen1 = 0;
			buckets_size[0]   = 0;
			for(i = 1; i < brz->k; i++)
			{
				if(buckets_size[i] == 0) continue;
				sum += value;
				value = buckets_size[i];
				buckets_size[i] = sum;
				
			}	
			memory_usage = 0;
			keys_index = (cmph_uint32 *)calloc((size_t)nkeys_in_buffer, sizeof(cmph_uint32));
			for(i = 0; i < nkeys_in_buffer; i++)
			{
				memcpy(&keylen1, buffer + memory_usage, sizeof(keylen1));
				h0 = hash(brz->h0, (char *)(buffer + memory_usage + sizeof(keylen1)), keylen1) % brz->k;
				keys_index[buckets_size[h0]] = memory_usage;
				buckets_size[h0]++;
				memory_usage +=  keylen1 + (cmph_uint32)sizeof(keylen1);
			}
			filename = (char *)calloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
			sprintf(filename, "%s%u.cmph",brz->tmp_dir, nflushes);
			tmp_fd = fopen(filename, "wb");
			free(filename);
			filename = NULL;
			for(i = 0; i < nkeys_in_buffer; i++)
			{
				memcpy(&keylen1, buffer + keys_index[i], sizeof(keylen1));
				nbytes = fwrite(buffer + keys_index[i], (size_t)1, keylen1 + sizeof(keylen1), tmp_fd);
			}
			nkeys_in_buffer = 0;
			memory_usage = 0;
			memset((void *)buckets_size, 0, brz->k*sizeof(cmph_uint32));
			nflushes++;
			free(keys_index);
			fclose(tmp_fd);
		}
		memcpy(buffer + memory_usage, &keylen, sizeof(keylen));
		memcpy(buffer + memory_usage + sizeof(keylen), key, (size_t)keylen);
		memory_usage += keylen + (cmph_uint32)sizeof(keylen);
		h0 = hash(brz->h0, key, keylen) % brz->k;
		
		if ((brz->size[h0] == MAX_BUCKET_SIZE) || (brz->algo == CMPH_BMZ8 && ((brz->c >= 1.0) && (cmph_uint8)(brz->c * brz->size[h0]) < brz->size[h0]))) 
		{
			free(buffer);
			free(buckets_size);
			return 0;
		}
		brz->size[h0] = (cmph_uint8)(brz->size[h0] + 1U);
		buckets_size[h0] ++;
		nkeys_in_buffer++;
		mph->key_source->dispose(mph->key_source->data, key, keylen);
	}
	if (memory_usage != 0) // flush buffers 
	{
		cmph_uint32 value;
		cmph_uint32 sum, keylen1;
		if(mph->verbosity)
		{
			fprintf(stderr, "Flushing  %u\n", nkeys_in_buffer);
		}
		value = buckets_size[0];
		sum = 0;
		keylen1 = 0;
		buckets_size[0]   = 0;
		for(i = 1; i < brz->k; i++)
		{
			if(buckets_size[i] == 0) continue;
			sum += value;
			value = buckets_size[i];
			buckets_size[i] = sum;
		}
		memory_usage = 0;
		keys_index = (cmph_uint32 *)calloc((size_t)nkeys_in_buffer, sizeof(cmph_uint32));
		for(i = 0; i < nkeys_in_buffer; i++)
		{
			memcpy(&keylen1, buffer + memory_usage, sizeof(keylen1));
			h0 = hash(brz->h0, (char *)(buffer + memory_usage + sizeof(keylen1)), keylen1) % brz->k;
			keys_index[buckets_size[h0]] = memory_usage;
			buckets_size[h0]++;
			memory_usage +=  keylen1 + (cmph_uint32)sizeof(keylen1);
		}
		filename = (char *)calloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
		sprintf(filename, "%s%u.cmph",brz->tmp_dir, nflushes);
		tmp_fd = fopen(filename, "wb");
		free(filename);
		filename = NULL;
		for(i = 0; i < nkeys_in_buffer; i++)
		{
			memcpy(&keylen1, buffer + keys_index[i], sizeof(keylen1));
			nbytes = fwrite(buffer + keys_index[i], (size_t)1, keylen1 + sizeof(keylen1), tmp_fd);
		}
		nkeys_in_buffer = 0;
		memory_usage = 0;
		memset((void *)buckets_size, 0, brz->k*sizeof(cmph_uint32));
		nflushes++;
		free(keys_index);
		fclose(tmp_fd);
	}

	free(buffer);
	free(buckets_size);
	if(nflushes > 1024) return 0; // Too many files generated.
	// mphf generation
	if(mph->verbosity)
	{
		fprintf(stderr, "\nMPHF generation \n");
	}
	/* Starting to dump to disk the resultant MPHF: __cmph_dump function */
	nbytes = fwrite(cmph_names[CMPH_BRZ], (size_t)(strlen(cmph_names[CMPH_BRZ]) + 1), (size_t)1, brz->mphf_fd);
	nbytes = fwrite(&(brz->m), sizeof(brz->m), (size_t)1, brz->mphf_fd);
	nbytes = fwrite(&(brz->c), sizeof(double), (size_t)1, brz->mphf_fd);
	nbytes = fwrite(&(brz->algo), sizeof(brz->algo), (size_t)1, brz->mphf_fd);
	nbytes = fwrite(&(brz->k), sizeof(cmph_uint32), (size_t)1, brz->mphf_fd); // number of MPHFs
	nbytes = fwrite(brz->size, sizeof(cmph_uint8)*(brz->k), (size_t)1, brz->mphf_fd);
  if (nbytes == 0 && ferror(brz->mphf_fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return 0;
        }

	//tmp_fds = (FILE **)calloc(nflushes, sizeof(FILE *));
	buff_manager = buffer_manager_new(brz->memory_availability, nflushes);
	buffer_merge = (cmph_uint8 **)calloc((size_t)nflushes, sizeof(cmph_uint8 *));
	buffer_h0    = (cmph_uint32 *)calloc((size_t)nflushes, sizeof(cmph_uint32));
	
	memory_usage = 0;
	for(i = 0; i < nflushes; i++)
	{
		filename = (char *)calloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
		sprintf(filename, "%s%u.cmph",brz->tmp_dir, i);
		buffer_manager_open(buff_manager, i, filename);
		free(filename);
		filename = NULL;
		key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
		h0 = hash(brz->h0, key+sizeof(keylen), keylen) % brz->k;
		buffer_h0[i] = h0;
                buffer_merge[i] = (cmph_uint8 *)key;
                key = NULL; //transfer memory ownership                 
	}
	e = 0;
	keys_vd = (cmph_uint8 **)calloc((size_t)MAX_BUCKET_SIZE, sizeof(cmph_uint8 *));
	nkeys_vd = 0;
	error = 0;
	while(e < brz->m)
	{
		i = brz_min_index(buffer_h0, nflushes);
		cur_bucket = buffer_h0[i];
		key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
		if(key)
		{
			while(key)
			{
				//keylen = strlen(key);
				h0 = hash(brz->h0, key+sizeof(keylen), keylen) % brz->k;
				if (h0 != buffer_h0[i]) break;
				keys_vd[nkeys_vd++] = (cmph_uint8 *)key;
				key = NULL; //transfer memory ownership
				e++;
				key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
			}
			if (key)
			{
				assert(nkeys_vd < brz->size[cur_bucket]);
				keys_vd[nkeys_vd++] = buffer_merge[i];
				buffer_merge[i] = NULL; //transfer memory ownership
				e++;
				buffer_h0[i] = h0;
				buffer_merge[i] = (cmph_uint8 *)key;
			}
		}
		if(!key)
		{
			assert(nkeys_vd < brz->size[cur_bucket]);
			keys_vd[nkeys_vd++] = buffer_merge[i];
			buffer_merge[i] = NULL; //transfer memory ownership
			e++;
			buffer_h0[i] = UINT_MAX;
		}
		
		if(nkeys_vd == brz->size[cur_bucket]) // Generating mphf for each bucket.
		{
			cmph_io_adapter_t *source = NULL;
			cmph_config_t *config = NULL;
			cmph_t *mphf_tmp = NULL;
			char *bufmphf = NULL;
			cmph_uint32 buflenmphf = 0;
			// Source of keys
			source = cmph_io_byte_vector_adapter(keys_vd, (cmph_uint32)nkeys_vd);
			config = cmph_config_new(source);
			cmph_config_set_algo(config, brz->algo);
			//cmph_config_set_algo(config, CMPH_BMZ8);
			cmph_config_set_graphsize(config, brz->c);
			mphf_tmp = cmph_new(config);
			if (mphf_tmp == NULL) 
			{
				if(mph->verbosity) fprintf(stderr, "ERROR: Can't generate MPHF for bucket %u out of %u\n", cur_bucket + 1, brz->k);
				error = 1;
				cmph_config_destroy(config);
 				brz_destroy_keys_vd(keys_vd, nkeys_vd);
				cmph_io_byte_vector_adapter_destroy(source);
				break;
			}
			if(mph->verbosity) 
			{
			  if (cur_bucket % 1000 == 0) 
  			  {
			  	fprintf(stderr, "MPHF for bucket %u out of %u was generated.\n", cur_bucket + 1, brz->k);
			  }
			}
			switch(brz->algo)
			{
				case CMPH_FCH:
				{
					fch_data_t * fchf = NULL;
					fchf = (fch_data_t *)mphf_tmp->data;			
					bufmphf = brz_copy_partial_fch_mphf(brz, fchf, cur_bucket, &buflenmphf);
				}
					break;
				case CMPH_BMZ8:
				{
					bmz8_data_t * bmzf = NULL;
					bmzf = (bmz8_data_t *)mphf_tmp->data;
					bufmphf = brz_copy_partial_bmz8_mphf(brz, bmzf, cur_bucket,  &buflenmphf);
				}
					break;
				default: assert(0);
			}
		        nbytes = fwrite(bufmphf, (size_t)buflenmphf, (size_t)1, brz->mphf_fd);
			free(bufmphf);
			bufmphf = NULL;
			cmph_config_destroy(config);
 			brz_destroy_keys_vd(keys_vd, nkeys_vd);
			cmph_destroy(mphf_tmp);
			cmph_io_byte_vector_adapter_destroy(source);
			nkeys_vd = 0;
		}
	}
	buffer_manager_destroy(buff_manager);
	free(keys_vd);
	free(buffer_merge);
	free(buffer_h0);
	if (error) return 0;
	return 1;
}

static cmph_uint32 brz_min_index(cmph_uint32 * vector, cmph_uint32 n)
{
	cmph_uint32 i, min_index = 0;
	for(i = 1; i < n; i++)
	{
		if(vector[i] < vector[min_index]) min_index = i;
	}
	return min_index;
}

static void brz_destroy_keys_vd(cmph_uint8 ** keys_vd, cmph_uint32 nkeys)
{
	cmph_uint8 i;
	for(i = 0; i < nkeys; i++) { free(keys_vd[i]); keys_vd[i] = NULL;}
}

static char * brz_copy_partial_fch_mphf(brz_config_data_t *brz, fch_data_t * fchf, cmph_uint32 index,  cmph_uint32 *buflen)
{
	cmph_uint32 i = 0;
	cmph_uint32 buflenh1 = 0;
	cmph_uint32 buflenh2 = 0; 
	char * bufh1 = NULL;
	char * bufh2 = NULL;
	char * buf   = NULL;
	cmph_uint32 n  = fchf->b;//brz->size[index];
	hash_state_dump(fchf->h1, &bufh1, &buflenh1);
	hash_state_dump(fchf->h2, &bufh2, &buflenh2);
	*buflen = buflenh1 + buflenh2 + n + 2U * (cmph_uint32)sizeof(cmph_uint32);
	buf = (char *)malloc((size_t)(*buflen));
	memcpy(buf, &buflenh1, sizeof(cmph_uint32));
	memcpy(buf+sizeof(cmph_uint32), bufh1, (size_t)buflenh1);
	memcpy(buf+sizeof(cmph_uint32)+buflenh1, &buflenh2, sizeof(cmph_uint32));
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1, bufh2, (size_t)buflenh2);	
	for (i = 0; i < n; i++) memcpy(buf+2*sizeof(cmph_uint32)+buflenh1+buflenh2+i,(fchf->g + i), (size_t)1);
	free(bufh1);
	free(bufh2);
	return buf;
}
static char * brz_copy_partial_bmz8_mphf(brz_config_data_t *brz, bmz8_data_t * bmzf, cmph_uint32 index,  cmph_uint32 *buflen)
{
	cmph_uint32 buflenh1 = 0;
	cmph_uint32 buflenh2 = 0; 
	char * bufh1 = NULL;
	char * bufh2 = NULL;
	char * buf   = NULL;
	cmph_uint32 n = (cmph_uint32)ceil(brz->c * brz->size[index]);
	hash_state_dump(bmzf->hashes[0], &bufh1, &buflenh1);
	hash_state_dump(bmzf->hashes[1], &bufh2, &buflenh2);
	*buflen = buflenh1 + buflenh2 + n + 2U * (cmph_uint32)sizeof(cmph_uint32);
	buf = (char *)malloc((size_t)(*buflen));
	memcpy(buf, &buflenh1, sizeof(cmph_uint32));
	memcpy(buf+sizeof(cmph_uint32), bufh1, (size_t)buflenh1);
	memcpy(buf+sizeof(cmph_uint32)+buflenh1, &buflenh2, sizeof(cmph_uint32));
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1, bufh2, (size_t)buflenh2);
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1+buflenh2,bmzf->g, (size_t)n);
	free(bufh1);
	free(bufh2);
	return buf;
}


int brz_dump(cmph_t *mphf, FILE *fd)
{
	brz_data_t *data = (brz_data_t *)mphf->data;
	char *buf = NULL;
	cmph_uint32 buflen;
	register size_t nbytes;
	DEBUGP("Dumping brzf\n");
	// The initial part of the MPHF have already been dumped to disk during construction
	// Dumping h0
        hash_state_dump(data->h0, &buf, &buflen);
        DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
        nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
        nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
        free(buf);
	// Dumping m and the vector offset.
	nbytes = fwrite(&(data->m), sizeof(cmph_uint32), (size_t)1, fd);	
	nbytes = fwrite(data->offset, sizeof(cmph_uint32)*(data->k), (size_t)1, fd);
	if (nbytes == 0 && ferror(fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return 0;
        }
	return 1;
}

void brz_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	register size_t nbytes;
	cmph_uint32 i, n;
	brz_data_t *brz = (brz_data_t *)malloc(sizeof(brz_data_t));

	DEBUGP("Loading brz mphf\n");
	mphf->data = brz;
	nbytes = fread(&(brz->c), sizeof(double), (size_t)1, f);
	nbytes = fread(&(brz->algo), sizeof(brz->algo), (size_t)1, f); // Reading algo.
	nbytes = fread(&(brz->k), sizeof(cmph_uint32), (size_t)1, f);
	brz->size   = (cmph_uint8 *) malloc(sizeof(cmph_uint8)*brz->k);
	nbytes = fread(brz->size, sizeof(cmph_uint8)*(brz->k), (size_t)1, f);	
	brz->h1 = (hash_state_t **)malloc(sizeof(hash_state_t *)*brz->k);
	brz->h2 = (hash_state_t **)malloc(sizeof(hash_state_t *)*brz->k);
	brz->g  = (cmph_uint8 **)  calloc((size_t)brz->k, sizeof(cmph_uint8 *));
	DEBUGP("Reading c = %f   k = %u   algo = %u \n", brz->c, brz->k, brz->algo);
	//loading h_i1, h_i2 and g_i.
	for(i = 0; i < brz->k; i++)
	{
		// h1
		nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
		DEBUGP("Hash state 1 has %u bytes\n", buflen);
		buf = (char *)malloc((size_t)buflen);
		nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
		brz->h1[i] = hash_state_load(buf, buflen);
		free(buf);
		//h2
		nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
		DEBUGP("Hash state 2 has %u bytes\n", buflen);
		buf = (char *)malloc((size_t)buflen);
		nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
		brz->h2[i] = hash_state_load(buf, buflen);
		free(buf);
		switch(brz->algo)
		{
			case CMPH_FCH:
				n = fch_calc_b(brz->c, brz->size[i]);
				break;
			case CMPH_BMZ8:
				n = (cmph_uint32)ceil(brz->c * brz->size[i]);
				break;
			default: assert(0);
		}
		DEBUGP("g_i has %u bytes\n", n);
		brz->g[i] = (cmph_uint8 *)calloc((size_t)n, sizeof(cmph_uint8));
		nbytes = fread(brz->g[i], sizeof(cmph_uint8)*n, (size_t)1, f);
	}
	//loading h0
	nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	DEBUGP("Hash state has %u bytes\n", buflen);
	buf = (char *)malloc((size_t)buflen);
	nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
	brz->h0 = hash_state_load(buf, buflen);
	free(buf);

	//loading c, m, and the vector offset.	
	nbytes = fread(&(brz->m), sizeof(cmph_uint32), (size_t)1, f);
	brz->offset = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*brz->k);
	nbytes = fread(brz->offset, sizeof(cmph_uint32)*(brz->k), (size_t)1, f);
	if (nbytes == 0 && ferror(f)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
        }

	return;
}

static cmph_uint32 brz_bmz8_search(brz_data_t *brz, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	register cmph_uint32 h0;
	register cmph_uint32 m, n, h1, h2;
	register cmph_uint8 mphf_bucket;

	hash_vector(brz->h0, key, keylen, fingerprint);
	h0 = fingerprint[2] % brz->k;

	m = brz->size[h0];
	n = (cmph_uint32)ceil(brz->c * m);
	h1 = hash(brz->h1[h0], key, keylen) % n;
	h2 = hash(brz->h2[h0], key, keylen) % n;
	
	if (h1 == h2 && ++h2 >= n) h2 = 0;
	mphf_bucket = (cmph_uint8)(brz->g[h0][h1] + brz->g[h0][h2]); 
	DEBUGP("key: %s h1: %u h2: %u h0: %u\n", key, h1, h2, h0);
	DEBUGP("key: %s g[h1]: %u g[h2]: %u offset[h0]: %u edges: %u\n", key, brz->g[h0][h1], brz->g[h0][h2], brz->offset[h0], brz->m);
	DEBUGP("Address: %u\n", mphf_bucket + brz->offset[h0]);
	return (mphf_bucket + brz->offset[h0]);
}

static cmph_uint32 brz_fch_search(brz_data_t *brz, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	register cmph_uint32 h0;
	register cmph_uint32 m, b, h1, h2;
	register double p1, p2;
	register cmph_uint8 mphf_bucket;

	hash_vector(brz->h0, key, keylen, fingerprint);
	h0 = fingerprint[2] % brz->k;

	m = brz->size[h0];
	b = fch_calc_b(brz->c, m);
	p1 = fch_calc_p1(m);
	p2 = fch_calc_p2(b);
	h1 = hash(brz->h1[h0], key, keylen) % m;
	h2 = hash(brz->h2[h0], key, keylen) % m;
	mphf_bucket = 0;
	h1 = mixh10h11h12(b, p1, p2, h1);
	mphf_bucket = (cmph_uint8)((h2 + brz->g[h0][h1]) % m);
	return (mphf_bucket + brz->offset[h0]);
}

cmph_uint32 brz_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	brz_data_t *brz = mphf->data;
	cmph_uint32 fingerprint[3];
	switch(brz->algo)
	{
		case CMPH_FCH:
			return brz_fch_search(brz, key, keylen, fingerprint);
		case CMPH_BMZ8:
			return brz_bmz8_search(brz, key, keylen, fingerprint);
		default: assert(0);
	}
	return 0;
}
void brz_destroy(cmph_t *mphf)
{
	cmph_uint32 i;
	brz_data_t *data = (brz_data_t *)mphf->data;
	if(data->g)
	{
		for(i = 0; i < data->k; i++)
		{
			free(data->g[i]);
			hash_state_destroy(data->h1[i]);
			hash_state_destroy(data->h2[i]);
		}
		free(data->g);
		free(data->h1);
		free(data->h2);
	}
	hash_state_destroy(data->h0);
	free(data->size);
	free(data->offset);
	free(data);
	free(mphf);
}

/** \fn void brz_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void brz_pack(cmph_t *mphf, void *packed_mphf)
{
	brz_data_t *data = (brz_data_t *)mphf->data;
	cmph_uint8 * ptr = packed_mphf;
	cmph_uint32 i,n;
	CMPH_HASH h0_type, h1_type, h2_type;
#ifdef __brz_use_64bit__
	cmph_uint64 * g_is_ptr;
#else
	cmph_uint32 * g_is_ptr;
#endif
	cmph_uint8 * g_i;
	
	// packing internal algo type
	memcpy(ptr, &(data->algo), sizeof(data->algo));
	ptr += sizeof(data->algo);

	// packing h0 type
	h0_type = hash_get_type(data->h0);
	memcpy(ptr, &h0_type, sizeof(h0_type));
	ptr += sizeof(h0_type);

	// packing h0
	hash_state_pack(data->h0, ptr);
	ptr += hash_state_packed_size(h0_type);
	
	// packing k
	memcpy(ptr, &(data->k), sizeof(data->k));
	ptr += sizeof(data->k);

	// packing c
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->c; 
	ptr += sizeof(data->c);

	// packing h1 type
	h1_type = hash_get_type(data->h1[0]);
	memcpy(ptr, &h1_type, sizeof(h1_type));
	ptr += sizeof(h1_type);

	// packing h2 type
	h2_type = hash_get_type(data->h2[0]);
	memcpy(ptr, &h2_type, sizeof(h2_type));
	ptr += sizeof(h2_type);

	// packing size
	memcpy(ptr, data->size, sizeof(cmph_uint8)*data->k);	
	ptr += data->k;
	
	// packing offset
	memcpy(ptr, data->offset, sizeof(cmph_uint32)*data->k);	
	ptr += sizeof(cmph_uint32)*data->k;
	
	#ifdef __brz_use_64bit__
		g_is_ptr = (cmph_uint64 *)ptr;
	#else
		g_is_ptr = (cmph_uint32 *)ptr;
	#endif
	
	g_i = (cmph_uint8 *) (g_is_ptr + data->k);
	
	for(i = 0; i < data->k; i++)
	{
		#ifdef __brz_use_64bit__
			*g_is_ptr++ = (cmph_uint64)g_i;
		#else
			*g_is_ptr++ = (cmph_uint32)g_i;
		#endif
		// packing h1[i]
		hash_state_pack(data->h1[i], g_i);
		g_i += hash_state_packed_size(h1_type);
		
		// packing h2[i]
		hash_state_pack(data->h2[i], g_i);
		g_i += hash_state_packed_size(h2_type);

		// packing g_i
		switch(data->algo)
		{
			case CMPH_FCH:
				n = fch_calc_b(data->c, data->size[i]);
				break;
			case CMPH_BMZ8:
				n = (cmph_uint32)ceil(data->c * data->size[i]);
				break;
			default: assert(0);
		}
		memcpy(g_i, data->g[i], sizeof(cmph_uint8)*n);	
		g_i += n;
		
	}

}

/** \fn cmph_uint32 brz_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 brz_packed_size(cmph_t *mphf)
{
	cmph_uint32 i;
	cmph_uint32 size = 0;
	brz_data_t *data = (brz_data_t *)mphf->data;
	CMPH_HASH h0_type = hash_get_type(data->h0); 
	CMPH_HASH h1_type = hash_get_type(data->h1[0]); 
	CMPH_HASH h2_type = hash_get_type(data->h2[0]);
	cmph_uint32 n;
	size = (cmph_uint32)(2*sizeof(CMPH_ALGO) + 3*sizeof(CMPH_HASH) + hash_state_packed_size(h0_type) + sizeof(cmph_uint32) + 
			sizeof(double) + sizeof(cmph_uint8)*data->k + sizeof(cmph_uint32)*data->k);
	// pointers to g_is
	#ifdef __brz_use_64bit__
		size +=  (cmph_uint32) sizeof(cmph_uint64)*data->k;
	#else
		size +=  (cmph_uint32) sizeof(cmph_uint32)*data->k;
	#endif
	
	size += hash_state_packed_size(h1_type) * data->k;
	size += hash_state_packed_size(h2_type) * data->k;
	
	n = 0;
	for(i = 0; i < data->k; i++)
	{
   		switch(data->algo)
   		{
   			case CMPH_FCH:
   				n = fch_calc_b(data->c, data->size[i]);
   				break;
   			case CMPH_BMZ8:
   				n = (cmph_uint32)ceil(data->c * data->size[i]);
   				break;
   			default: assert(0);
   		}
		size += n;	
	}
	return size;
}



static cmph_uint32 brz_bmz8_search_packed(cmph_uint32 *packed_mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	register CMPH_HASH h0_type = *packed_mphf++;
	register cmph_uint32 *h0_ptr = packed_mphf;
	register cmph_uint32 k, h0, m, n, h1, h2;
	register cmph_uint32 *offset;
	register double c;
	register CMPH_HASH h1_type, h2_type;
	register cmph_uint8 * size;
#ifdef __brz_use_64bit__
	register cmph_uint64 * g_is_ptr;
#else
	register cmph_uint32 * g_is_ptr;
#endif
	register cmph_uint8 *h1_ptr, *h2_ptr, *g;
	register cmph_uint8 mphf_bucket;

	packed_mphf = (cmph_uint32 *)(((cmph_uint8 *)packed_mphf) + hash_state_packed_size(h0_type)); 
	
	k = *packed_mphf++;

	c = (double)(*((cmph_uint64*)packed_mphf));
	packed_mphf += 2;

	h1_type = *packed_mphf++;
	
	h2_type = *packed_mphf++;

	size = (cmph_uint8 *) packed_mphf;
	packed_mphf = (cmph_uint32 *)(size + k);  
	
	offset = packed_mphf;
	packed_mphf += k;

	
	hash_vector_packed(h0_ptr, h0_type, key, keylen, fingerprint);
	h0 = fingerprint[2] % k;
	
	m = size[h0];
	n = (cmph_uint32)ceil(c * m);

	#ifdef __brz_use_64bit__
		g_is_ptr = (cmph_uint64 *)packed_mphf;
	#else
		g_is_ptr = packed_mphf;
	#endif
	
	h1_ptr = (cmph_uint8 *) g_is_ptr[h0];
	
	h2_ptr = h1_ptr + hash_state_packed_size(h1_type);

	g = h2_ptr + hash_state_packed_size(h2_type);
	
	h1 = hash_packed(h1_ptr, h1_type, key, keylen) % n;
	h2 = hash_packed(h2_ptr, h2_type, key, keylen) % n;
		
	if (h1 == h2 && ++h2 >= n) h2 = 0;
	mphf_bucket = (cmph_uint8)(g[h1] + g[h2]); 
	DEBUGP("key: %s h1: %u h2: %u h0: %u\n", key, h1, h2, h0);
	DEBUGP("Address: %u\n", mphf_bucket + offset[h0]);
	return (mphf_bucket + offset[h0]);	
}

static cmph_uint32 brz_fch_search_packed(cmph_uint32 *packed_mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	register CMPH_HASH h0_type = *packed_mphf++;
	
	register cmph_uint32 *h0_ptr = packed_mphf;
	register cmph_uint32 k, h0, m, b, h1, h2;
	register double c, p1, p2;
	register CMPH_HASH h1_type, h2_type;
	register cmph_uint8 *size, *h1_ptr, *h2_ptr, *g;
	register cmph_uint32 *offset;
#ifdef __brz_use_64bit__
	register cmph_uint64 * g_is_ptr;
#else
	register cmph_uint32 * g_is_ptr;
#endif
	register cmph_uint8 mphf_bucket;

	packed_mphf = (cmph_uint32 *)(((cmph_uint8 *)packed_mphf) + hash_state_packed_size(h0_type)); 
	
	k = *packed_mphf++;

	c = (double)(*((cmph_uint64*)packed_mphf));
	packed_mphf += 2;

	h1_type = *packed_mphf++;

	h2_type = *packed_mphf++;

	size = (cmph_uint8 *) packed_mphf;
	packed_mphf = (cmph_uint32 *)(size + k);  
	
	offset = packed_mphf;
	packed_mphf += k;
	
	hash_vector_packed(h0_ptr, h0_type, key, keylen, fingerprint);
	h0 = fingerprint[2] % k;
	
	m = size[h0];
	b = fch_calc_b(c, m);
	p1 = fch_calc_p1(m);
	p2 = fch_calc_p2(b);
	
	#ifdef __brz_use_64bit__
		g_is_ptr = (cmph_uint64 *)packed_mphf;
	#else
		g_is_ptr = packed_mphf;
	#endif
	
	h1_ptr = (cmph_uint8 *) g_is_ptr[h0];
	
	h2_ptr = h1_ptr + hash_state_packed_size(h1_type);

	g = h2_ptr + hash_state_packed_size(h2_type);
	
	h1 = hash_packed(h1_ptr, h1_type, key, keylen) % m;
	h2 = hash_packed(h2_ptr, h2_type, key, keylen) % m;

	mphf_bucket = 0;
	h1 = mixh10h11h12(b, p1, p2, h1);
	mphf_bucket = (cmph_uint8)((h2 + g[h1]) % m);
	return (mphf_bucket + offset[h0]);
}

/** cmph_uint32 brz_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 brz_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	register cmph_uint32 *ptr = (cmph_uint32 *)packed_mphf;	
	register CMPH_ALGO algo = *ptr++;
	cmph_uint32 fingerprint[3];
	switch(algo)
	{
		case CMPH_FCH:
			return brz_fch_search_packed(ptr, key, keylen, fingerprint);
		case CMPH_BMZ8:
			return brz_bmz8_search_packed(ptr, key, keylen, fingerprint);
		default:
			assert(0);
			return 0; /* To avoid warnings that value must be returned */
	}
}

