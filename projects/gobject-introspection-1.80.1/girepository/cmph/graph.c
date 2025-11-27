#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include "vstack.h"
#include "bitbool.h"

//#define DEBUG
#include "debug.h"

/* static const cmph_uint8 bitmask[8] = { 1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7 }; */
/* #define GETBIT(array, i) (array[(i) / 8] & bitmask[(i) % 8]) */
/* #define SETBIT(array, i) (array[(i) / 8] |= bitmask[(i) % 8]) */
/* #define UNSETBIT(array, i) (array[(i) / 8] &= (~(bitmask[(i) % 8]))) */

#define abs_edge(e, i) (e % g->nedges + i * g->nedges)

struct __graph_t
{
	cmph_uint32 nnodes;
	cmph_uint32 nedges;
	cmph_uint32 *edges;
	cmph_uint32 *first;
	cmph_uint32 *next;
        cmph_uint8  *critical_nodes;   /* included -- Fabiano*/
        cmph_uint32 ncritical_nodes;   /* included -- Fabiano*/
	cmph_uint32 cedges;
	int shrinking;
};

static cmph_uint32 EMPTY = UINT_MAX;

graph_t *graph_new(cmph_uint32 nnodes, cmph_uint32 nedges)
{
	graph_t *graph = (graph_t *)malloc(sizeof(graph_t));
	if (!graph) return NULL;

	graph->edges = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * 2 * nedges);
	graph->next =  (cmph_uint32 *)malloc(sizeof(cmph_uint32) * 2 * nedges);
	graph->first = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * nnodes);
        graph->critical_nodes = NULL; /* included -- Fabiano*/
	graph->ncritical_nodes = 0;   /* included -- Fabiano*/
	graph->nnodes = nnodes;
	graph->nedges = nedges;

	graph_clear_edges(graph);
	return graph;
}


void graph_destroy(graph_t *graph)
{
	DEBUGP("Destroying graph\n");
	free(graph->edges);
	free(graph->first);
	free(graph->next);
        free(graph->critical_nodes); /* included -- Fabiano*/
	free(graph);
	return;
}

void graph_print(graph_t *g)
{
	cmph_uint32 i, e;
	for (i = 0; i < g->nnodes; ++i)
	{
		DEBUGP("Printing edges connected to %u\n", i);
		e = g->first[i];
		if (e != EMPTY)
		{
			printf("%u -> %u\n", g->edges[abs_edge(e, 0)], g->edges[abs_edge(e, 1)]);
			while ((e = g->next[e]) != EMPTY)
			{
				printf("%u -> %u\n", g->edges[abs_edge(e, 0)], g->edges[abs_edge(e, 1)]);
			}
		}
			
	}
	return;
}

void graph_add_edge(graph_t *g, cmph_uint32 v1, cmph_uint32 v2)
{
	cmph_uint32 e = g->cedges;

	assert(v1 < g->nnodes);
	assert(v2 < g->nnodes);
	assert(e < g->nedges);
	assert(!g->shrinking);

	g->next[e] = g->first[v1];
	g->first[v1] = e;
	g->edges[e] = v2;

	g->next[e + g->nedges] = g->first[v2];
	g->first[v2] = e + g->nedges;
	g->edges[e + g->nedges] = v1;

	++(g->cedges);
}

static int check_edge(graph_t *g, cmph_uint32 e, cmph_uint32 v1, cmph_uint32 v2)
{
	DEBUGP("Checking edge %u %u looking for %u %u\n", g->edges[abs_edge(e, 0)], g->edges[abs_edge(e, 1)], v1, v2);
	if (g->edges[abs_edge(e, 0)] == v1 && g->edges[abs_edge(e, 1)] == v2) return 1;
	if (g->edges[abs_edge(e, 0)] == v2 && g->edges[abs_edge(e, 1)] == v1) return 1;
	return 0;
}

cmph_uint32 graph_edge_id(graph_t *g, cmph_uint32 v1, cmph_uint32 v2)
{
	cmph_uint32 e;
	e = g->first[v1];
	assert(e != EMPTY);
	if (check_edge(g, e, v1, v2)) return abs_edge(e, 0);
	do
	{
		e = g->next[e];
		assert(e != EMPTY);
	}
	while (!check_edge(g, e, v1, v2));
	return abs_edge(e, 0);
}
static void del_edge_point(graph_t *g, cmph_uint32 v1, cmph_uint32 v2)
{
	cmph_uint32 e, prev;

	DEBUGP("Deleting edge point %u %u\n", v1, v2);
	e = g->first[v1];
	if (check_edge(g, e, v1, v2)) 
	{
		g->first[v1] = g->next[e];
		//g->edges[e] = EMPTY;
		DEBUGP("Deleted\n");
		return;
	}
	DEBUGP("Checking linked list\n");
	do
	{
		prev = e;
		e = g->next[e];
		assert(e != EMPTY);
	}
	while (!check_edge(g, e, v1, v2));

	g->next[prev] = g->next[e];
	//g->edges[e] = EMPTY;
	DEBUGP("Deleted\n");
}

	
void graph_del_edge(graph_t *g, cmph_uint32 v1, cmph_uint32 v2)
{
	g->shrinking = 1;
	del_edge_point(g, v1, v2);
	del_edge_point(g, v2, v1);
}

void graph_clear_edges(graph_t *g)
{
	cmph_uint32 i;
	for (i = 0; i < g->nnodes; ++i) g->first[i] = EMPTY;
	for (i = 0; i < g->nedges*2; ++i) 
	{
		g->edges[i] = EMPTY;
		g->next[i] = EMPTY;
	}
	g->cedges = 0;
	g->shrinking = 0;
}

static cmph_uint8 find_degree1_edge(graph_t *g, cmph_uint32 v, cmph_uint8 *deleted, cmph_uint32 *e)
{
	cmph_uint32 edge = g->first[v];
	cmph_uint8 found = 0;
	DEBUGP("Checking degree of vertex %u\n", v);
	if (edge == EMPTY) return 0;
	else if (!(GETBIT(deleted, abs_edge(edge, 0)))) 
	{
		found = 1;
		*e = edge;
	}
	while(1)
	{
		edge = g->next[edge];
		if (edge == EMPTY) break;
		if (GETBIT(deleted, abs_edge(edge, 0))) continue;
		if (found) return 0;
		DEBUGP("Found first edge\n");
		*e = edge;
		found = 1;
	}
	return found;
}

static void cyclic_del_edge(graph_t *g, cmph_uint32 v, cmph_uint8 *deleted)
{

	cmph_uint32 e = 0;
	cmph_uint8 degree1;
	cmph_uint32 v1 = v;
	cmph_uint32 v2 = 0;

	degree1 = find_degree1_edge(g, v1, deleted, &e);
	if (!degree1) return;
	while(1) 
	{
		DEBUGP("Deleting edge %u (%u->%u)\n", e, g->edges[abs_edge(e, 0)], g->edges[abs_edge(e, 1)]);
		SETBIT(deleted, abs_edge(e, 0));
		
		v2 = g->edges[abs_edge(e, 0)];
		if (v2 == v1) v2 = g->edges[abs_edge(e, 1)];

		DEBUGP("Checking if second endpoint %u has degree 1\n", v2); 
		degree1 = find_degree1_edge(g, v2, deleted, &e);
		if (degree1) 
		{
			DEBUGP("Inspecting vertex %u\n", v2);
			v1 = v2;
		}
		else break;
	}
}

int graph_is_cyclic(graph_t *g)
{
	cmph_uint32 i;
	cmph_uint32 v;
	cmph_uint8 *deleted = (cmph_uint8 *)malloc((g->nedges*sizeof(cmph_uint8))/8 + 1);
	size_t deleted_len = g->nedges/8 + 1;
	memset(deleted, 0, deleted_len);

	DEBUGP("Looking for cycles in graph with %u vertices and %u edges\n", g->nnodes, g->nedges);
	for (v = 0; v < g->nnodes; ++v)
	{
		cyclic_del_edge(g, v, deleted);
	}
	for (i = 0; i < g->nedges; ++i)
	{
		if (!(GETBIT(deleted, i))) 
		{
			DEBUGP("Edge %u %u->%u was not deleted\n", i, g->edges[i], g->edges[i + g->nedges]);
			free(deleted);
			return 1;
		}
	}
	free(deleted);
	return 0;
}

cmph_uint8 graph_node_is_critical(graph_t * g, cmph_uint32 v) /* included -- Fabiano */
{
        return (cmph_uint8)GETBIT(g->critical_nodes,v);
}

void graph_obtain_critical_nodes(graph_t *g) /* included -- Fabiano*/
{
        cmph_uint32 i;
	cmph_uint32 v;
	cmph_uint8 *deleted = (cmph_uint8 *)malloc((g->nedges*sizeof(cmph_uint8))/8+1);
	size_t deleted_len = g->nedges/8 + 1;
	memset(deleted, 0, deleted_len);
	free(g->critical_nodes);
	g->critical_nodes = (cmph_uint8 *)malloc((g->nnodes*sizeof(cmph_uint8))/8 + 1);
	g->ncritical_nodes = 0;
	memset(g->critical_nodes, 0, (g->nnodes*sizeof(cmph_uint8))/8 + 1);
	DEBUGP("Looking for the 2-core in graph with %u vertices and %u edges\n", g->nnodes, g->nedges);
	for (v = 0; v < g->nnodes; ++v)
	{
		cyclic_del_edge(g, v, deleted);
	}

	for (i = 0; i < g->nedges; ++i)
	{
		if (!(GETBIT(deleted,i))) 
		{
			DEBUGP("Edge %u %u->%u belongs to the 2-core\n", i, g->edges[i], g->edges[i + g->nedges]);
			if(!(GETBIT(g->critical_nodes,g->edges[i]))) 
			{
			  g->ncritical_nodes ++;
			  SETBIT(g->critical_nodes,g->edges[i]);
			}
			if(!(GETBIT(g->critical_nodes,g->edges[i + g->nedges]))) 
			{
			  g->ncritical_nodes ++;
			  SETBIT(g->critical_nodes,g->edges[i + g->nedges]);
			}
		}
	}
	free(deleted);
}

cmph_uint8 graph_contains_edge(graph_t *g, cmph_uint32 v1, cmph_uint32 v2) /* included -- Fabiano*/
{
	cmph_uint32 e;
	e = g->first[v1];
	if(e == EMPTY) return 0;
	if (check_edge(g, e, v1, v2)) return 1;
	do
	{
		e = g->next[e];
		if(e == EMPTY) return 0;
	}
	while (!check_edge(g, e, v1, v2));
	return 1;
}

cmph_uint32 graph_vertex_id(graph_t *g, cmph_uint32 e, cmph_uint32 id) /* included -- Fabiano*/
{
  return (g->edges[e + id*g->nedges]);
}

cmph_uint32 graph_ncritical_nodes(graph_t *g) /* included -- Fabiano*/
{
  return g->ncritical_nodes;
}

graph_iterator_t graph_neighbors_it(graph_t *g, cmph_uint32 v)
{
	graph_iterator_t it;
	it.vertex = v;
	it.edge = g->first[v];
	return it;
}
cmph_uint32 graph_next_neighbor(graph_t *g, graph_iterator_t* it)
{
	cmph_uint32 ret;
	if(it->edge == EMPTY) return GRAPH_NO_NEIGHBOR; 
	if (g->edges[it->edge] == it->vertex) ret = g->edges[it->edge + g->nedges];
	else ret = g->edges[it->edge];
	it->edge = g->next[it->edge];
	return ret;
}
	

