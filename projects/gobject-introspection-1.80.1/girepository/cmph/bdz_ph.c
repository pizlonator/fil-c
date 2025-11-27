#include "bdz_ph.h"
#include "cmph_structs.h"
#include "bdz_structs_ph.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
//#define DEBUG
#include "debug.h"
#define UNASSIGNED 3
#define NULL_EDGE 0xffffffff


static cmph_uint8 pow3_table[5] = {1,3,9,27,81};
static cmph_uint8 lookup_table[5][256] = {
 {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0},
 {0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 1},
 {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct 
{
	cmph_uint32 vertices[3];
	cmph_uint32 next_edges[3];
}bdz_ph_edge_t;

typedef cmph_uint32 * bdz_ph_queue_t;

static void bdz_ph_alloc_queue(bdz_ph_queue_t * queuep, cmph_uint32 nedges)
{
	(*queuep)=malloc(nedges*sizeof(cmph_uint32));
};
static void bdz_ph_free_queue(bdz_ph_queue_t * queue)
{
	free(*queue);
};

typedef struct 
{
	cmph_uint32 nedges;
	bdz_ph_edge_t * edges;
	cmph_uint32 * first_edge;
	cmph_uint8 * vert_degree;	
}bdz_ph_graph3_t;


static void bdz_ph_alloc_graph3(bdz_ph_graph3_t * graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	graph3->edges=malloc(nedges*sizeof(bdz_ph_edge_t));
	graph3->first_edge=malloc(nvertices*sizeof(cmph_uint32));
	graph3->vert_degree=malloc((size_t)nvertices);	
};
static void bdz_ph_init_graph3(bdz_ph_graph3_t * graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	memset(graph3->first_edge,0xff,nvertices*sizeof(cmph_uint32));
	memset(graph3->vert_degree,0,(size_t)nvertices);
	graph3->nedges=0;
};
static void bdz_ph_free_graph3(bdz_ph_graph3_t *graph3)
{
	free(graph3->edges);
	free(graph3->first_edge);
	free(graph3->vert_degree);
};

static void bdz_ph_partial_free_graph3(bdz_ph_graph3_t *graph3)
{
	free(graph3->first_edge);
	free(graph3->vert_degree);
	graph3->first_edge = NULL;
	graph3->vert_degree = NULL;
};

static void bdz_ph_add_edge(bdz_ph_graph3_t * graph3, cmph_uint32 v0, cmph_uint32 v1, cmph_uint32 v2)
{
	graph3->edges[graph3->nedges].vertices[0]=v0;
	graph3->edges[graph3->nedges].vertices[1]=v1;
	graph3->edges[graph3->nedges].vertices[2]=v2;
	graph3->edges[graph3->nedges].next_edges[0]=graph3->first_edge[v0];
	graph3->edges[graph3->nedges].next_edges[1]=graph3->first_edge[v1];
	graph3->edges[graph3->nedges].next_edges[2]=graph3->first_edge[v2];
	graph3->first_edge[v0]=graph3->first_edge[v1]=graph3->first_edge[v2]=graph3->nedges;
	graph3->vert_degree[v0]++;
	graph3->vert_degree[v1]++;
	graph3->vert_degree[v2]++;
	graph3->nedges++;
};

static void bdz_ph_dump_graph(bdz_ph_graph3_t* graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	cmph_uint32 i;
	for(i=0;i<nedges;i++){
		printf("\nedge %d %d %d %d ",i,graph3->edges[i].vertices[0],
			graph3->edges[i].vertices[1],graph3->edges[i].vertices[2]);
		printf(" nexts %d %d %d",graph3->edges[i].next_edges[0],
				graph3->edges[i].next_edges[1],graph3->edges[i].next_edges[2]);
	};
	
	for(i=0;i<nvertices;i++){
		printf("\nfirst for vertice %d %d ",i,graph3->first_edge[i]);
	
	};
};

static void bdz_ph_remove_edge(bdz_ph_graph3_t * graph3, cmph_uint32 curr_edge)
{
	cmph_uint32 i,j=0,vert,edge1,edge2;
	for(i=0;i<3;i++){
		vert=graph3->edges[curr_edge].vertices[i];
		edge1=graph3->first_edge[vert];
		edge2=NULL_EDGE;
		while(edge1!=curr_edge&&edge1!=NULL_EDGE){
			edge2=edge1;
			if(graph3->edges[edge1].vertices[0]==vert){
				j=0;
			} else if(graph3->edges[edge1].vertices[1]==vert){
				j=1;
			} else 
				j=2;
			edge1=graph3->edges[edge1].next_edges[j];
		};
		if(edge1==NULL_EDGE){
			printf("\nerror remove edge %d dump graph",curr_edge);
			bdz_ph_dump_graph(graph3,graph3->nedges,graph3->nedges+graph3->nedges/4);
			exit(-1);
		};
		
		if(edge2!=NULL_EDGE){
			graph3->edges[edge2].next_edges[j] = 
				graph3->edges[edge1].next_edges[i];
		} else 
			graph3->first_edge[vert]=
				graph3->edges[edge1].next_edges[i];
		graph3->vert_degree[vert]--;
	};
	
};

static int bdz_ph_generate_queue(cmph_uint32 nedges, cmph_uint32 nvertices, bdz_ph_queue_t queue, bdz_ph_graph3_t* graph3)
{
	cmph_uint32 i,v0,v1,v2;
	cmph_uint32 queue_head=0,queue_tail=0;
	cmph_uint32 curr_edge;
	cmph_uint32 tmp_edge;
	cmph_uint8 * marked_edge =malloc((size_t)(nedges >> 3) + 1);
	memset(marked_edge, 0, (size_t)(nedges >> 3) + 1);

	for(i=0;i<nedges;i++){
		v0=graph3->edges[i].vertices[0];
		v1=graph3->edges[i].vertices[1];
		v2=graph3->edges[i].vertices[2];
		if(graph3->vert_degree[v0]==1 || 
				graph3->vert_degree[v1]==1 ||
				graph3->vert_degree[v2]==1){
			if(!GETBIT(marked_edge,i)) {
				queue[queue_head++]=i;
				SETBIT(marked_edge,i);
			}
		};
	};
	while(queue_tail!=queue_head){
		curr_edge=queue[queue_tail++];
		bdz_ph_remove_edge(graph3,curr_edge);
		v0=graph3->edges[curr_edge].vertices[0];
		v1=graph3->edges[curr_edge].vertices[1];
		v2=graph3->edges[curr_edge].vertices[2];
		if(graph3->vert_degree[v0]==1 ) {
			tmp_edge=graph3->first_edge[v0];
			if(!GETBIT(marked_edge,tmp_edge)) {
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
			
		};
		if(graph3->vert_degree[v1]==1) {
			tmp_edge=graph3->first_edge[v1];
			if(!GETBIT(marked_edge,tmp_edge)){
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
			
		};
		if(graph3->vert_degree[v2]==1){
			tmp_edge=graph3->first_edge[v2];
			if(!GETBIT(marked_edge,tmp_edge)){
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
		};
	};
	free(marked_edge);
	return (int)queue_head - (int)nedges;/* returns 0 if successful otherwies return negative number*/
};

static int bdz_ph_mapping(cmph_config_t *mph, bdz_ph_graph3_t* graph3, bdz_ph_queue_t queue);
static void assigning(bdz_ph_config_data_t *bdz_ph, bdz_ph_graph3_t* graph3, bdz_ph_queue_t queue);
static void bdz_ph_optimization(bdz_ph_config_data_t *bdz_ph);

bdz_ph_config_data_t *bdz_ph_config_new(void)
{
	bdz_ph_config_data_t *bdz_ph;
	bdz_ph = (bdz_ph_config_data_t *)malloc(sizeof(bdz_ph_config_data_t));
	assert(bdz_ph);
	memset(bdz_ph, 0, sizeof(bdz_ph_config_data_t));
	bdz_ph->hashfunc = CMPH_HASH_JENKINS;
	bdz_ph->g = NULL;
	bdz_ph->hl = NULL;
	return bdz_ph;
}

void bdz_ph_config_destroy(cmph_config_t *mph)
{
	bdz_ph_config_data_t *data = (bdz_ph_config_data_t *)mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void bdz_ph_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	bdz_ph_config_data_t *bdz_ph = (bdz_ph_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 1) break; //bdz_ph only uses one linear hash function
		bdz_ph->hashfunc = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_t *bdz_ph_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	bdz_ph_data_t *bdz_phf = NULL;
	cmph_uint32 iterations;
	bdz_ph_queue_t edges;
	bdz_ph_graph3_t graph3;
	bdz_ph_config_data_t *bdz_ph = (bdz_ph_config_data_t *)mph->data;
	#ifdef CMPH_TIMING
	double construction_time_begin = 0.0;
	double construction_time = 0.0;
	ELAPSED_TIME_IN_SECONDS(&construction_time_begin);
	#endif


	if (c == 0) c = 1.23; // validating restrictions over parameter c.
	DEBUGP("c: %f\n", c);
	bdz_ph->m = mph->key_source->nkeys;	
	bdz_ph->r = (cmph_uint32)ceil((c * mph->key_source->nkeys)/3);	
	if ((bdz_ph->r % 2) == 0) bdz_ph->r += 1;
	bdz_ph->n = 3*bdz_ph->r;

	
	bdz_ph_alloc_graph3(&graph3, bdz_ph->m, bdz_ph->n);
	bdz_ph_alloc_queue(&edges,bdz_ph->m);
	DEBUGP("Created hypergraph\n");
	
	DEBUGP("m (edges): %u n (vertices): %u  r: %u c: %f \n", bdz_ph->m, bdz_ph->n, bdz_ph->r, c);

	// Mapping step
	iterations = 100;
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", bdz_ph->m, bdz_ph->n);
	}
	while(1)
	{
		int ok;
		DEBUGP("linear hash function \n");
		bdz_ph->hl = hash_state_new(bdz_ph->hashfunc, 15);

		ok = bdz_ph_mapping(mph, &graph3, edges);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(bdz_ph->hl);
			bdz_ph->hl = NULL;
			DEBUGP("%u iterations remaining\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;
	}
	
	if (iterations == 0)
	{
//		free(bdz_ph->g);
		bdz_ph_free_queue(&edges);
		bdz_ph_free_graph3(&graph3);
		return NULL;
	}
	bdz_ph_partial_free_graph3(&graph3);
	// Assigning step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering assigning step for mph creation of %u keys with graph sized %u\n", bdz_ph->m, bdz_ph->n);
	}
	assigning(bdz_ph, &graph3, edges);

	bdz_ph_free_queue(&edges);
	bdz_ph_free_graph3(&graph3);
	
	if (mph->verbosity)
	{
		fprintf(stderr, "Starting optimization step\n");
	}

	bdz_ph_optimization(bdz_ph);

	#ifdef CMPH_TIMING
	ELAPSED_TIME_IN_SECONDS(&construction_time);
	#endif
	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	bdz_phf = (bdz_ph_data_t *)malloc(sizeof(bdz_ph_data_t));
	bdz_phf->g = bdz_ph->g;
	bdz_ph->g = NULL; //transfer memory ownership
	bdz_phf->hl = bdz_ph->hl;
	bdz_ph->hl = NULL; //transfer memory ownership
	bdz_phf->n = bdz_ph->n;
	bdz_phf->m = bdz_ph->m;
	bdz_phf->r = bdz_ph->r;
	mphf->data = bdz_phf;
	mphf->size = bdz_ph->n;

	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}

	#ifdef CMPH_TIMING	
	register cmph_uint32 space_usage = bdz_ph_packed_size(mphf)*8;
	register cmph_uint32 keys_per_bucket = 1;
	construction_time = construction_time - construction_time_begin;
	fprintf(stdout, "%u\t%.2f\t%u\t%.4f\t%.4f\n", bdz_ph->m, bdz_ph->m/(double)bdz_ph->n, keys_per_bucket, construction_time, space_usage/(double)bdz_ph->m);
	#endif	

	return mphf;
}

		
static int bdz_ph_mapping(cmph_config_t *mph, bdz_ph_graph3_t* graph3, bdz_ph_queue_t queue)
{
	cmph_uint32 e;
	int cycles = 0;
	cmph_uint32 hl[3];
	
	bdz_ph_config_data_t *bdz_ph = (bdz_ph_config_data_t *)mph->data;
	bdz_ph_init_graph3(graph3, bdz_ph->m, bdz_ph->n);
	mph->key_source->rewind(mph->key_source->data);
	for (e = 0; e < mph->key_source->nkeys; ++e)
	{
		cmph_uint32 h0, h1, h2;
		cmph_uint32 keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);		
		hash_vector(bdz_ph->hl, key, keylen, hl);
		h0 = hl[0] % bdz_ph->r;
		h1 = hl[1] % bdz_ph->r + bdz_ph->r;
		h2 = hl[2] % bdz_ph->r + (bdz_ph->r << 1);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		bdz_ph_add_edge(graph3,h0,h1,h2);
	}
	cycles = bdz_ph_generate_queue(bdz_ph->m, bdz_ph->n, queue, graph3);	
	return (cycles == 0);
}

static void assigning(bdz_ph_config_data_t *bdz_ph, bdz_ph_graph3_t* graph3, bdz_ph_queue_t queue)
{
	cmph_uint32 i;
	cmph_uint32 nedges=graph3->nedges;
	cmph_uint32 curr_edge;
	cmph_uint32 v0,v1,v2;
	cmph_uint8 * marked_vertices =malloc((size_t)(bdz_ph->n >> 3) + 1);
	cmph_uint32 sizeg = (cmph_uint32)ceil(bdz_ph->n/4.0);
	bdz_ph->g = (cmph_uint8 *)calloc((size_t)sizeg, sizeof(cmph_uint8));	
	memset(marked_vertices, 0, (size_t)(bdz_ph->n >> 3) + 1);
	//memset(bdz_ph->g, 0xff, sizeg);

	for(i=nedges-1;i+1>=1;i--){
		curr_edge=queue[i];
		v0=graph3->edges[curr_edge].vertices[0];
		v1=graph3->edges[curr_edge].vertices[1];
		v2=graph3->edges[curr_edge].vertices[2];
		DEBUGP("B:%u %u %u -- %u %u %u\n", v0, v1, v2, GETVALUE(bdz_ph->g, v0), GETVALUE(bdz_ph->g, v1), GETVALUE(bdz_ph->g, v2));
		if(!GETBIT(marked_vertices, v0)){
			if(!GETBIT(marked_vertices,v1))
			{
				//SETVALUE(bdz_ph->g, v1, UNASSIGNED); 
				SETBIT(marked_vertices, v1);
			}
			if(!GETBIT(marked_vertices,v2))
			{
				//SETVALUE(bdz_ph->g, v2, UNASSIGNED);		
				SETBIT(marked_vertices, v2);
			}			
			SETVALUE0(bdz_ph->g, v0, (6-(GETVALUE(bdz_ph->g, v1) + GETVALUE(bdz_ph->g,v2)))%3);
			SETBIT(marked_vertices, v0);
		} else if(!GETBIT(marked_vertices, v1)) {
			if(!GETBIT(marked_vertices, v2))
			{
				//SETVALUE(bdz_ph->g, v2, UNASSIGNED);
				SETBIT(marked_vertices, v2);
			}
			SETVALUE0(bdz_ph->g, v1, (7 - (GETVALUE(bdz_ph->g, v0)+GETVALUE(bdz_ph->g, v2)))%3);
			SETBIT(marked_vertices, v1);
		}else {
			SETVALUE0(bdz_ph->g, v2, (8-(GETVALUE(bdz_ph->g,v0)+GETVALUE(bdz_ph->g, v1)))%3);
			SETBIT(marked_vertices, v2);
		}		
		DEBUGP("A:%u %u %u -- %u %u %u\n", v0, v1, v2, GETVALUE(bdz_ph->g, v0), GETVALUE(bdz_ph->g, v1), GETVALUE(bdz_ph->g, v2));
	};
	free(marked_vertices);
}

static void bdz_ph_optimization(bdz_ph_config_data_t *bdz_ph)
{
	cmph_uint32 i;
	cmph_uint8 byte = 0;
	cmph_uint32 sizeg = (cmph_uint32)ceil(bdz_ph->n/5.0);
	cmph_uint8 * new_g = (cmph_uint8 *)calloc((size_t)sizeg, sizeof(cmph_uint8));	
	cmph_uint8 value;
	cmph_uint32 idx;
	for(i = 0; i < bdz_ph->n; i++) 
	{	
            idx = i/5;
            byte = new_g[idx];
            value = GETVALUE(bdz_ph->g, i);
            byte = (cmph_uint8) (byte + value*pow3_table[i%5U]);
            new_g[idx] = byte;
        }
	free(bdz_ph->g);
	bdz_ph->g = new_g;
}


int bdz_ph_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 sizeg = 0;
	register size_t nbytes;
	bdz_ph_data_t *data = (bdz_ph_data_t *)mphf->data;
#ifdef DEBUG
	cmph_uint32 i;
#endif

	__cmph_dump(mphf, fd);

	hash_state_dump(data->hl, &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	nbytes = fwrite(&(data->n), sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(&(data->m), sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(&(data->r), sizeof(cmph_uint32), (size_t)1, fd);
	sizeg = (cmph_uint32)ceil(data->n/5.0);	
	nbytes = fwrite(data->g, sizeof(cmph_uint8)*sizeg, (size_t)1, fd);

	if (nbytes == 0 && ferror(fd)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
          return 0;
        }
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", GETVALUE(data->g, i));
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void bdz_ph_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 sizeg = 0;
	register size_t nbytes;
	bdz_ph_data_t *bdz_ph = (bdz_ph_data_t *)malloc(sizeof(bdz_ph_data_t));

	DEBUGP("Loading bdz_ph mphf\n");
	mphf->data = bdz_ph;

	nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	DEBUGP("Hash state has %u bytes\n", buflen);
	buf = (char *)malloc((size_t)buflen);
	nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
	bdz_ph->hl = hash_state_load(buf, buflen);
	free(buf);
	

	DEBUGP("Reading m and n\n");
	nbytes = fread(&(bdz_ph->n), sizeof(cmph_uint32), (size_t)1, f);	
	nbytes = fread(&(bdz_ph->m), sizeof(cmph_uint32), (size_t)1, f);	
	nbytes = fread(&(bdz_ph->r), sizeof(cmph_uint32), (size_t)1, f);	
	sizeg = (cmph_uint32)ceil(bdz_ph->n/5.0);
	bdz_ph->g = (cmph_uint8 *)calloc((size_t)sizeg, sizeof(cmph_uint8));
	nbytes = fread(bdz_ph->g, sizeg*sizeof(cmph_uint8), (size_t)1, f);

	if (nbytes == 0 && ferror(f)) {
          fprintf(stderr, "ERROR: %s\n", strerror(errno));
        }
	return;
}
		

cmph_uint32 bdz_ph_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	register bdz_ph_data_t *bdz_ph = mphf->data;
	cmph_uint32 hl[3];
	register cmph_uint8 byte0, byte1, byte2;
	register cmph_uint32 vertex;

	hash_vector(bdz_ph->hl, key, keylen,hl);
	hl[0] = hl[0] % bdz_ph->r;
	hl[1] = hl[1] % bdz_ph->r + bdz_ph->r;
	hl[2] = hl[2] % bdz_ph->r + (bdz_ph->r << 1);

	byte0 = bdz_ph->g[hl[0]/5];
	byte1 = bdz_ph->g[hl[1]/5];
	byte2 = bdz_ph->g[hl[2]/5];
	
	byte0 = lookup_table[hl[0]%5U][byte0];
	byte1 = lookup_table[hl[1]%5U][byte1];
	byte2 = lookup_table[hl[2]%5U][byte2];
	vertex = hl[(byte0 + byte1 + byte2)%3];
		
	return vertex;
}


void bdz_ph_destroy(cmph_t *mphf)
{
	bdz_ph_data_t *data = (bdz_ph_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hl);
	free(data);
	free(mphf);
}

/** \fn void bdz_ph_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void bdz_ph_pack(cmph_t *mphf, void *packed_mphf)
{
	bdz_ph_data_t *data = (bdz_ph_data_t *)mphf->data;
	cmph_uint8 * ptr = packed_mphf;
	cmph_uint32 sizeg;

	// packing hl type
	CMPH_HASH hl_type = hash_get_type(data->hl);
	*((cmph_uint32 *) ptr) = hl_type;
	ptr += sizeof(cmph_uint32);

	// packing hl
	hash_state_pack(data->hl, ptr);
	ptr += hash_state_packed_size(hl_type);

	// packing r
	*((cmph_uint32 *) ptr) = data->r;
	ptr += sizeof(data->r);

	// packing g
	sizeg = (cmph_uint32)ceil(data->n/5.0);
	memcpy(ptr, data->g,  sizeof(cmph_uint8)*sizeg);
}

/** \fn cmph_uint32 bdz_ph_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 bdz_ph_packed_size(cmph_t *mphf)
{
	bdz_ph_data_t *data = (bdz_ph_data_t *)mphf->data;
	CMPH_HASH hl_type = hash_get_type(data->hl); 
	cmph_uint32 sizeg = (cmph_uint32)ceil(data->n/5.0);
	return (cmph_uint32) (sizeof(CMPH_ALGO) + hash_state_packed_size(hl_type) + 2*sizeof(cmph_uint32) + sizeof(cmph_uint8)*sizeg);
}

/** cmph_uint32 bdz_ph_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 bdz_ph_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	
	register CMPH_HASH hl_type  = *(cmph_uint32 *)packed_mphf;
	register cmph_uint8 *hl_ptr = (cmph_uint8 *)(packed_mphf) + 4;
	
	register cmph_uint8 * ptr = hl_ptr + hash_state_packed_size(hl_type);

	register cmph_uint32 r = *((cmph_uint32*) ptr);
	register cmph_uint8 * g = ptr + 4;
	
	cmph_uint32 hl[3];
	register cmph_uint8 byte0, byte1, byte2;
	register cmph_uint32 vertex;

	hash_vector_packed(hl_ptr, hl_type, key, keylen, hl);
	
	hl[0] = hl[0] % r;
	hl[1] = hl[1] % r + r;
	hl[2] = hl[2] % r + (r << 1);

	byte0 = g[hl[0]/5];
	byte1 = g[hl[1]/5];
	byte2 = g[hl[2]/5];
	
	byte0 = lookup_table[hl[0]%5][byte0];
	byte1 = lookup_table[hl[1]%5][byte1];
	byte2 = lookup_table[hl[2]%5][byte2];
	vertex = hl[(byte0 + byte1 + byte2)%3];
		
	return vertex;
}
