#include "graph.h"
#include "hashtree.h"
#include "cmph_structs.h"
#include "hastree_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

hashtree_config_data_t *hashtree_config_new()
{
	hashtree_config_data_t *hashtree;
	hashtree = (hashtree_config_data_t *)malloc(sizeof(hashtree_config_data_t));
	if (!hashtree) return NULL;
	memset(hashtree, 0, sizeof(hashtree_config_data_t));
	hashtree->hashfuncs[0] = CMPH_HASH_JENKINS;
	hashtree->hashfuncs[1] = CMPH_HASH_JENKINS;
	hashtree->hashfuncs[2] = CMPH_HASH_JENKINS;
	hashtree->memory = 32 * 1024 * 1024;
	return hashtree;
}
void hashtree_config_destroy(cmph_config_t *mph)
{
	hashtree_config_data_t *data = (hashtree_config_data_t *)mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void hashtree_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	hashtree_config_data_t *hashtree = (hashtree_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 3) break; //hashtree only uses three hash functions
		hashtree->hashfuncs[i] = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_t *hashtree_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	hashtree_data_t *hashtreef = NULL;

	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	hashtree_config_data_t *hashtree = (hashtree_config_data_t *)mph->data;
	hashtree->m = mph->key_source->nkeys;	
	hashtree->n = ceil(c * mph->key_source->nkeys);	
	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", hashtree->m, hashtree->n, c);
	hashtree->graph = graph_new(hashtree->n, hashtree->m);
	DEBUGP("Created graph\n");

	hashtree->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*3);
	for(i = 0; i < 3; ++i) hashtree->hashes[i] = NULL;
	//Mapping step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", hashtree->m, hashtree->n);
	}
	while(1)
	{
		int ok;
		hashtree->hashes[0] = hash_state_new(hashtree->hashfuncs[0], hashtree->n);
		hashtree->hashes[1] = hash_state_new(hashtree->hashfuncs[1], hashtree->n);
		ok = hashtree_gen_edges(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(hashtree->hashes[0]);
			hashtree->hashes[0] = NULL;
			hash_state_destroy(hashtree->hashes[1]);
			hashtree->hashes[1] = NULL;
			DEBUGP("%u iterations remaining\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "Acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;	
	}
	if (iterations == 0)
	{
		graph_destroy(hashtree->graph);	
		return NULL;
	}

	//Assignment step
	if (mph->verbosity)
	{
		fprintf(stderr, "Starting assignment step\n");
	}
	DEBUGP("Assignment step\n");
 	visited = (char *)malloc(hashtree->n/8 + 1);
	memset(visited, 0, hashtree->n/8 + 1);
	free(hashtree->g);
	hashtree->g = (cmph_uint32 *)malloc(hashtree->n * sizeof(cmph_uint32));
	assert(hashtree->g);
	for (i = 0; i < hashtree->n; ++i)
	{
	        if (!GETBIT(visited,i))
		{
			hashtree->g[i] = 0;
			hashtree_traverse(hashtree, visited, i);
		}
	}
	graph_destroy(hashtree->graph);	
	free(visited);
	hashtree->graph = NULL;

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	hashtreef = (hashtree_data_t *)malloc(sizeof(hashtree_data_t));
	hashtreef->g = hashtree->g;
	hashtree->g = NULL; //transfer memory ownership
	hashtreef->hashes = hashtree->hashes;
	hashtree->hashes = NULL; //transfer memory ownership
	hashtreef->n = hashtree->n;
	hashtreef->m = hashtree->m;
	mphf->data = hashtreef;
	mphf->size = hashtree->m;
	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

static void hashtree_traverse(hashtree_config_data_t *hashtree, cmph_uint8 *visited, cmph_uint32 v)
{

	graph_iterator_t it = graph_neighbors_it(hashtree->graph, v);
	cmph_uint32 neighbor = 0;
	SETBIT(visited,v);
	
	DEBUGP("Visiting vertex %u\n", v);
	while((neighbor = graph_next_neighbor(hashtree->graph, &it)) != GRAPH_NO_NEIGHBOR)
	{
		DEBUGP("Visiting neighbor %u\n", neighbor);
		if(GETBIT(visited,neighbor)) continue;
		DEBUGP("Visiting neighbor %u\n", neighbor);
		DEBUGP("Visiting edge %u->%u with id %u\n", v, neighbor, graph_edge_id(hashtree->graph, v, neighbor));
		hashtree->g[neighbor] = graph_edge_id(hashtree->graph, v, neighbor) - hashtree->g[v];
		DEBUGP("g is %u (%u - %u mod %u)\n", hashtree->g[neighbor], graph_edge_id(hashtree->graph, v, neighbor), hashtree->g[v], hashtree->m);
		hashtree_traverse(hashtree, visited, neighbor);
	}
}
		
static int hashtree_gen_edges(cmph_config_t *mph)
{
	cmph_uint32 e;
	hashtree_config_data_t *hashtree = (hashtree_config_data_t *)mph->data;
	int cycles = 0;

	DEBUGP("Generating edges for %u vertices with hash functions %s and %s\n", hashtree->n, cmph_hash_names[hashtree->hashfuncs[0]], cmph_hash_names[hashtree->hashfuncs[1]]);
	graph_clear_edges(hashtree->graph);	
	mph->key_source->rewind(mph->key_source->data);
	for (e = 0; e < mph->key_source->nkeys; ++e)
	{
		cmph_uint32 h1, h2;
		cmph_uint32 keylen;
		char *key;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h1 = hash(hashtree->hashes[0], key, keylen) % hashtree->n;
		h2 = hash(hashtree->hashes[1], key, keylen) % hashtree->n;
		if (h1 == h2) if (++h2 >= hashtree->n) h2 = 0;
		if (h1 == h2) 
		{
			if (mph->verbosity) fprintf(stderr, "Self loop for key %u\n", e);
			mph->key_source->dispose(mph->key_source->data, key, keylen);
			return 0;
		}
		DEBUGP("Adding edge: %u -> %u for key %s\n", h1, h2, key);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		graph_add_edge(hashtree->graph, h1, h2);
	}
	cycles = graph_is_cyclic(hashtree->graph);
	if (mph->verbosity && cycles) fprintf(stderr, "Cyclic graph generated\n");
	DEBUGP("Looking for cycles: %u\n", cycles);

	return ! cycles;
}

int hashtree_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 two = 2; //number of hash functions
	hashtree_data_t *data = (hashtree_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	fwrite(&two, sizeof(cmph_uint32), 1, fd);
	hash_state_dump(data->hashes[0], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	hash_state_dump(data->hashes[1], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	
	fwrite(data->g, sizeof(cmph_uint32)*data->n, 1, fd);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void hashtree_load(FILE *f, cmph_t *mphf)
{
	cmph_uint32 nhashes;
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	hashtree_data_t *hashtree = (hashtree_data_t *)malloc(sizeof(hashtree_data_t));

	DEBUGP("Loading hashtree mphf\n");
	mphf->data = hashtree;
	fread(&nhashes, sizeof(cmph_uint32), 1, f);
	hashtree->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*(nhashes + 1));
	hashtree->hashes[nhashes] = NULL;
	DEBUGP("Reading %u hashes\n", nhashes);
	for (i = 0; i < nhashes; ++i)
	{
		hash_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = hash_state_load(buf, buflen);
		hashtree->hashes[i] = state;
		free(buf);
	}

	DEBUGP("Reading m and n\n");
	fread(&(hashtree->n), sizeof(cmph_uint32), 1, f);	
	fread(&(hashtree->m), sizeof(cmph_uint32), 1, f);	

	hashtree->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*hashtree->n);
	fread(hashtree->g, hashtree->n*sizeof(cmph_uint32), 1, f);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < hashtree->n; ++i) fprintf(stderr, "%u ", hashtree->g[i]);
	fprintf(stderr, "\n");
	#endif
	return;
}
		

cmph_uint32 hashtree_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	hashtree_data_t *hashtree = mphf->data;
	cmph_uint32 h1 = hash(hashtree->hashes[0], key, keylen) % hashtree->n;
	cmph_uint32 h2 = hash(hashtree->hashes[1], key, keylen) % hashtree->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= hashtree->n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, hashtree->g[h1], hashtree->g[h2], hashtree->m);
	return (hashtree->g[h1] + hashtree->g[h2]) % hashtree->m;
}
void hashtree_destroy(cmph_t *mphf)
{
	hashtree_data_t *data = (hashtree_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hashes[0]);
	hash_state_destroy(data->hashes[1]);
	free(data->hashes);
	free(data);
	free(mphf);
}
