#include "fch.h"
#include "cmph_structs.h"
#include "fch_structs.h"
#include "hash.h"
#include "bitbool.h"
#include "fch_buckets.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define INDEX 0 /* alignment index within a bucket */
//#define DEBUG
#include "debug.h"

static fch_buckets_t * mapping(cmph_config_t *mph);
static cmph_uint32 * ordering(fch_buckets_t * buckets);
static cmph_uint8 check_for_collisions_h2(fch_config_data_t *fch, fch_buckets_t * buckets, cmph_uint32 *sorted_indexes);
static void permut(cmph_uint32 * vector, cmph_uint32 n);
static cmph_uint8 searching(fch_config_data_t *fch, fch_buckets_t *buckets, cmph_uint32 *sorted_indexes);

fch_config_data_t *fch_config_new(void)
{
	fch_config_data_t *fch;
	fch = (fch_config_data_t *)malloc(sizeof(fch_config_data_t));
	assert(fch);
	memset(fch, 0, sizeof(fch_config_data_t));
	fch->hashfuncs[0] = CMPH_HASH_JENKINS;
	fch->hashfuncs[1] = CMPH_HASH_JENKINS;
	fch->m = fch->b = 0;
	fch->c = fch->p1 = fch->p2 = 0.0;
	fch->g = NULL;
	fch->h1 = NULL;
	fch->h2 = NULL;
	return fch;
}

void fch_config_destroy(cmph_config_t *mph)
{
	fch_config_data_t *data = (fch_config_data_t *)mph->data;
	//DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void fch_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; //fch only uses two hash functions
		fch->hashfuncs[i] = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_uint32 mixh10h11h12(cmph_uint32 b, double p1, double p2, cmph_uint32 initial_index)
{
	register cmph_uint32 int_p2 = (cmph_uint32)p2;
	if (initial_index < p1)  initial_index %= int_p2;  /* h11 o h10 */
	else { /* h12 o h10 */
		initial_index %= b;
		if(initial_index < p2) initial_index += int_p2;
	}
	return initial_index;
}


cmph_uint32 fch_calc_b(double c, cmph_uint32 m)
{
	return (cmph_uint32)ceil((c*m)/(log((double)m)/log(2.0) + 1));
}

double fch_calc_p1(cmph_uint32 m)
{
	return ceil(0.55*m);
}

double fch_calc_p2(cmph_uint32 b)
{
	return ceil(0.3*b);
}

static fch_buckets_t * mapping(cmph_config_t *mph)
{
	cmph_uint32 i = 0;
	fch_buckets_t *buckets = NULL;
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	if (fch->h1) hash_state_destroy(fch->h1);
	fch->h1 = hash_state_new(fch->hashfuncs[0], fch->m);  
	fch->b = fch_calc_b(fch->c, fch->m);
	fch->p1 = fch_calc_p1(fch->m);
	fch->p2 = fch_calc_p2(fch->b);
	//DEBUGP("b:%u   p1:%f   p2:%f\n", fch->b, fch->p1, fch->p2);
	buckets = fch_buckets_new(fch->b);

	mph->key_source->rewind(mph->key_source->data);  
	for(i = 0; i < fch->m; i++)
	{
		cmph_uint32 h1, keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);	
		h1 = hash(fch->h1, key, keylen) % fch->m;
		h1 = mixh10h11h12 (fch->b, fch->p1, fch->p2, h1);
		fch_buckets_insert(buckets, h1, key, keylen);
		key = NULL; // transger memory ownership
		
	}
	return buckets;  
}


// returns the buckets indexes sorted by their sizes. 
static cmph_uint32 * ordering(fch_buckets_t * buckets)
{
  return fch_buckets_get_indexes_sorted_by_size(buckets);
}

/* Check whether function h2 causes collisions among the keys of each bucket */ 
static cmph_uint8 check_for_collisions_h2(fch_config_data_t *fch, fch_buckets_t * buckets, cmph_uint32 *sorted_indexes)
{
	//cmph_uint32 max_size = fch_buckets_get_max_size(buckets);
	cmph_uint8 * hashtable = (cmph_uint8 *)calloc((size_t)fch->m, sizeof(cmph_uint8));
	cmph_uint32 nbuckets = fch_buckets_get_nbuckets(buckets);
	cmph_uint32 i = 0, index = 0, j =0;
	for (i = 0; i < nbuckets; i++)
	{
		cmph_uint32 nkeys = fch_buckets_get_size(buckets, sorted_indexes[i]);
		memset(hashtable, 0, (size_t)fch->m);
		//DEBUGP("bucket %u -- nkeys: %u\n", i, nkeys);
		for (j = 0; j < nkeys; j++)
		{
			char * key = fch_buckets_get_key(buckets, sorted_indexes[i], j);
			cmph_uint32 keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], j);
			index = hash(fch->h2, key, keylen) % fch->m;
			if(hashtable[index]) { // collision detected
				free(hashtable);
				return 1;
			}
			hashtable[index] = 1;
		}
	}
	free(hashtable);
	return 0;
}

static void permut(cmph_uint32 * vector, cmph_uint32 n)
{ 
  cmph_uint32 i, j, b;
  for (i = 0; i < n; i++) {
    j = (cmph_uint32) rand() % n;
    b = vector[i];
    vector[i] = vector[j];
    vector[j] = b;
  }
}

static cmph_uint8 searching(fch_config_data_t *fch, fch_buckets_t *buckets, cmph_uint32 *sorted_indexes)
{
	cmph_uint32 * random_table = (cmph_uint32 *) calloc((size_t)fch->m, sizeof(cmph_uint32));
	cmph_uint32 * map_table    = (cmph_uint32 *) calloc((size_t)fch->m, sizeof(cmph_uint32));
	cmph_uint32 iteration_to_generate_h2 = 0;
	cmph_uint32 searching_iterations     = 0;
	cmph_uint8 restart                   = 0;
	cmph_uint32 nbuckets                 = fch_buckets_get_nbuckets(buckets);
	cmph_uint32 i, j, z, counter = 0, filled_count = 0;
	if (fch->g) free (fch->g);
	fch->g = (cmph_uint32 *) calloc((size_t)fch->b, sizeof(cmph_uint32));

	//DEBUGP("max bucket size: %u\n", fch_buckets_get_max_size(buckets));

	for(i = 0; i < fch->m; i++)
	{
		random_table[i] = i;
	}
	permut(random_table, fch->m);
	for(i = 0; i < fch->m; i++)
	{
		map_table[random_table[i]] = i;
	}
	do {   
		if (fch->h2) hash_state_destroy(fch->h2);
		fch->h2 = hash_state_new(fch->hashfuncs[1], fch->m);  
		restart = check_for_collisions_h2(fch, buckets, sorted_indexes);
		filled_count = 0;
		if (!restart) 
		{
			searching_iterations++; iteration_to_generate_h2 = 0;
			//DEBUGP("searching_iterations: %u\n", searching_iterations);
		}
		else {
			iteration_to_generate_h2++;
			//DEBUGP("iteration_to_generate_h2: %u\n", iteration_to_generate_h2);
		}		
		for(i = 0; (i < nbuckets) && !restart; i++) {
			cmph_uint32 bucketsize = fch_buckets_get_size(buckets, sorted_indexes[i]);
			if (bucketsize == 0)
			{
				restart = 0; // false
				break;
			}
			else restart = 1; // true
			for(z = 0; (z < (fch->m - filled_count)) && restart; z++) {
				char * key = fch_buckets_get_key(buckets, sorted_indexes[i], INDEX);
				cmph_uint32 keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], INDEX);
				cmph_uint32 h2 = hash(fch->h2, key, keylen) % fch->m;				
				counter = 0; 
				restart = 0; // false
				fch->g[sorted_indexes[i]] = (fch->m + random_table[filled_count + z] - h2) % fch->m;
				//DEBUGP("g[%u]: %u\n", sorted_indexes[i], fch->g[sorted_indexes[i]]);
				j = INDEX;
				do {
					cmph_uint32 index = 0;
					key = fch_buckets_get_key(buckets, sorted_indexes[i], j);
					keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], j);
					h2 = hash(fch->h2, key, keylen) % fch->m;
					index = (h2 + fch->g[sorted_indexes[i]]) % fch->m;
					//DEBUGP("key:%s  keylen:%u  index: %u  h2:%u  bucketsize:%u\n", key, keylen, index, h2, bucketsize);
					if (map_table[index] >= filled_count) {  
						cmph_uint32 y  = map_table[index];
						cmph_uint32 ry = random_table[y];
						random_table[y] = random_table[filled_count];
						random_table[filled_count] = ry;
						map_table[random_table[y]] = y;
						map_table[random_table[filled_count]] = filled_count;
						filled_count++;
						counter ++; 
					}
					else { 
						restart = 1; // true
						filled_count = filled_count - counter;
						counter = 0; 
						break;
					}
					j = (j + 1) % bucketsize;
				} while(j % bucketsize != INDEX); 
			}
			//getchar();
		}              
	} while(restart  && (searching_iterations < 10) && (iteration_to_generate_h2 < 1000));
	free(map_table);
	free(random_table);
	return restart;
}



cmph_t *fch_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	fch_data_t *fchf = NULL;
	cmph_uint32 iterations = 100;
	cmph_uint8 restart_mapping = 0;
	fch_buckets_t * buckets = NULL;
	cmph_uint32 * sorted_indexes = NULL;
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	fch->m = mph->key_source->nkeys;
	//DEBUGP("m: %f\n", fch->m);
	if (c <= 2) c = 2.6; // validating restrictions over parameter c.
	fch->c = c;
	//DEBUGP("c: %f\n", fch->c);
	fch->h1 = NULL;
	fch->h2 = NULL;
	fch->g = NULL;
	do
	{	  
		if (mph->verbosity)
		{
			fprintf(stderr, "Entering mapping step for mph creation of %u keys\n", fch->m);
		}
		if (buckets) fch_buckets_destroy(buckets);
		buckets = mapping(mph);
		if (mph->verbosity)
		{
			fprintf(stderr, "Starting ordering step\n");
		}
		if (sorted_indexes) free (sorted_indexes);
		sorted_indexes = ordering(buckets);
		if (mph->verbosity)
		{
			fprintf(stderr, "Starting searching step.\n");
		}
		restart_mapping = searching(fch, buckets, sorted_indexes);
		iterations--;
		
        } while(restart_mapping && iterations > 0);
	if (buckets) fch_buckets_destroy(buckets);
	if (sorted_indexes) free (sorted_indexes);
	if (iterations == 0) return NULL;
	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	fchf = (fch_data_t *)malloc(sizeof(fch_data_t));
	fchf->g = fch->g;
	fch->g = NULL; //transfer memory ownership
	fchf->h1 = fch->h1;
	fch->h1 = NULL; //transfer memory ownership
	fchf->h2 = fch->h2;
	fch->h2 = NULL; //transfer memory ownership
	fchf->p2 = fch->p2;
	fchf->p1 = fch->p1;
	fchf->b = fch->b;
	fchf->c = fch->c;
	fchf->m = fch->m;
	mphf->data = fchf;
	mphf->size = fch->m;
	//DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

int fch_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	register size_t nbytes;
	
	fch_data_t *data = (fch_data_t *)mphf->data;

#ifdef DEBUG
	cmph_uint32 i;
#endif
	__cmph_dump(mphf, fd);

	hash_state_dump(data->h1, &buf, &buflen);
	//DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	hash_state_dump(data->h2, &buf, &buflen);
	//DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	nbytes = fwrite(&(data->m), sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(&(data->c), sizeof(double), (size_t)1, fd);
	nbytes = fwrite(&(data->b), sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(&(data->p1), sizeof(double), (size_t)1, fd);
	nbytes = fwrite(&(data->p2), sizeof(double), (size_t)1, fd);
	nbytes = fwrite(data->g, sizeof(cmph_uint32)*(data->b), (size_t)1, fd);
	if (nbytes == 0 && ferror(fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return 0;
        }
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < data->b; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void fch_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	register size_t nbytes;
	fch_data_t *fch = (fch_data_t *)malloc(sizeof(fch_data_t));
#ifdef DEBUG
	cmph_uint32 i;
#endif

	//DEBUGP("Loading fch mphf\n");
	mphf->data = fch;
	//DEBUGP("Reading h1\n");
	fch->h1 = NULL;
	nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	//DEBUGP("Hash state of h1 has %u bytes\n", buflen);
	buf = (char *)malloc((size_t)buflen);
	nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
	fch->h1 = hash_state_load(buf, buflen);
	free(buf);
	
	//DEBUGP("Loading fch mphf\n");
	mphf->data = fch;
	//DEBUGP("Reading h2\n");
	fch->h2 = NULL;
	nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	//DEBUGP("Hash state of h2 has %u bytes\n", buflen);
	buf = (char *)malloc((size_t)buflen);
	nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
	fch->h2 = hash_state_load(buf, buflen);
	free(buf);
	
	
	//DEBUGP("Reading m and n\n");
	nbytes = fread(&(fch->m), sizeof(cmph_uint32), (size_t)1, f);
	nbytes = fread(&(fch->c), sizeof(double), (size_t)1, f);
	nbytes = fread(&(fch->b), sizeof(cmph_uint32), (size_t)1, f);
	nbytes = fread(&(fch->p1), sizeof(double), (size_t)1, f);
	nbytes = fread(&(fch->p2), sizeof(double), (size_t)1, f);

	fch->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*fch->b);
	nbytes = fread(fch->g, fch->b*sizeof(cmph_uint32), (size_t)1, f);
	if (nbytes == 0 && ferror(f)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return;
        }

	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < fch->b; ++i) fprintf(stderr, "%u ", fch->g[i]);
	fprintf(stderr, "\n");
	#endif
	return;
}

cmph_uint32 fch_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	fch_data_t *fch = mphf->data;
	cmph_uint32 h1 = hash(fch->h1, key, keylen) % fch->m;
	cmph_uint32 h2 = hash(fch->h2, key, keylen) % fch->m;
	h1 = mixh10h11h12 (fch->b, fch->p1, fch->p2, h1);
	//DEBUGP("key: %s h1: %u h2: %u  g[h1]: %u\n", key, h1, h2, fch->g[h1]);
	return (h2 + fch->g[h1]) % fch->m;
}
void fch_destroy(cmph_t *mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	free(data->g);
	hash_state_destroy(data->h1);
	hash_state_destroy(data->h2);
	free(data);
	free(mphf);
}

/** \fn void fch_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void fch_pack(cmph_t *mphf, void *packed_mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	cmph_uint8 * ptr = packed_mphf;

	// packing h1 type
	CMPH_HASH h1_type = hash_get_type(data->h1);
	CMPH_HASH h2_type;
	*((cmph_uint32 *) ptr) = h1_type;
	ptr += sizeof(cmph_uint32);

	// packing h1
	hash_state_pack(data->h1, ptr);
	ptr += hash_state_packed_size(h1_type);

	// packing h2 type
	h2_type = hash_get_type(data->h2);
	*((cmph_uint32 *) ptr) = h2_type;
	ptr += sizeof(cmph_uint32);

	// packing h2
	hash_state_pack(data->h2, ptr);
	ptr += hash_state_packed_size(h2_type);

	// packing m
	*((cmph_uint32 *) ptr) = data->m;
	ptr += sizeof(data->m);

	// packing b
	*((cmph_uint32 *) ptr) = data->b;
	ptr += sizeof(data->b);
	
	// packing p1
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->p1; 
	ptr += sizeof(data->p1);

	// packing p2
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->p2; 
	ptr += sizeof(data->p2);

	// packing g
	memcpy(ptr, data->g, sizeof(cmph_uint32)*(data->b));	
}

/** \fn cmph_uint32 fch_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 fch_packed_size(cmph_t *mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	CMPH_HASH h1_type = hash_get_type(data->h1); 
	CMPH_HASH h2_type = hash_get_type(data->h2); 

	return (cmph_uint32)(sizeof(CMPH_ALGO) + hash_state_packed_size(h1_type) + hash_state_packed_size(h2_type) + 
			4*sizeof(cmph_uint32) + 2*sizeof(double) + sizeof(cmph_uint32)*(data->b));
}


/** cmph_uint32 fch_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 fch_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	register cmph_uint8 *h1_ptr = packed_mphf;
	register CMPH_HASH h1_type  = *((cmph_uint32 *)h1_ptr);
	register cmph_uint8 *h2_ptr;
	register CMPH_HASH h2_type;
	register cmph_uint32 *g_ptr;
	register cmph_uint32 m, b, h1, h2;
	register double p1, p2;

	h1_ptr += 4;

	h2_ptr = h1_ptr + hash_state_packed_size(h1_type);
	h2_type  = *((cmph_uint32 *)h2_ptr);
	h2_ptr += 4;
	
	g_ptr = (cmph_uint32 *)(h2_ptr + hash_state_packed_size(h2_type));
	
	m = *g_ptr++;

	b = *g_ptr++;

	p1 = (double)(*((cmph_uint64 *)g_ptr));
	g_ptr += 2;

	p2 = (double)(*((cmph_uint64 *)g_ptr));
	g_ptr += 2;

	h1 = hash_packed(h1_ptr, h1_type, key, keylen) % m;
	h2 = hash_packed(h2_ptr, h2_type, key, keylen) % m;
	h1 = mixh10h11h12 (b, p1, p2, h1);
	return (h2 + g_ptr[h1]) % m;
}

