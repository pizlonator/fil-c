/* vi: set sw=4 ts=4: */
/*
 * tsort implementation for busybox
 *
 * public domain -- David Leonard, 2022
 */
//config:config TSORT
//config:	bool "tsort (2.6 kb)"
//config:	default y
//config:	help
//config:	tsort performs a topological sort.

//applet:IF_TSORT(APPLET(tsort, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TSORT) += tsort.o

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tsort.html */

//usage:#define tsort_trivial_usage
//usage:       "[FILE]"
//usage:#define tsort_full_usage "\n\n"
//usage:       "Topological sort"
//usage:#define tsort_example_usage
//usage:       "$ echo -e \"a b\\nb c\" | tsort\n"
//usage:       "a\n"
//usage:       "b\n"
//usage:       "c\n"

#include "libbb.h"
#include "common_bufsiz.h"

struct node {
	unsigned in_count;
	unsigned out_count;
	struct node **out;
	char name[1];
};

struct globals {
	struct node **nodes;
	unsigned nodes_len;
};
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	setup_common_bufsiz(); \
	BUILD_BUG_ON(sizeof(G) > COMMON_BUFSIZE); \
	G.nodes = NULL; \
	G.nodes_len = 0; \
} while (0)

static struct node *
get_node(const char *name)
{
	struct node *n;
	unsigned a = 0;
	unsigned b = G.nodes_len;

	/* Binary search for name */
	while (a != b) {
		unsigned m = (a + b) / 2;
		int cmp = strcmp(name, G.nodes[m]->name);
		if (cmp == 0)
			return G.nodes[m]; /* found */
		if (cmp < 0) {
			b = m;
		} else {
			a = m + 1;
		}
	}

	/* Allocate new node */
	n = xzalloc(sizeof(*n) + strlen(name));
	//n->in_count = 0;
	//n->out_count = 0;
	//n->out = NULL;
	strcpy(n->name, name);

	/* Insert to maintain sort */
	G.nodes = xrealloc(G.nodes, (G.nodes_len + 1) * sizeof(*G.nodes));
	memmove(&G.nodes[a + 1], &G.nodes[a],
		(G.nodes_len - a) * sizeof(*G.nodes));
	G.nodes[a] = n;
	G.nodes_len++;
	return n;
}

static void
add_edge(struct node *a, struct node *b)
{
	a->out = xrealloc_vector(a->out, 6, a->out_count);
	a->out[a->out_count++] = b;
	b->in_count++;
}

int tsort_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tsort_main(int argc UNUSED_PARAM, char **argv)
{
	char *line;
	size_t linesz;
	ssize_t len;
	struct node *a;
	int cycles;
	unsigned i;
#if ENABLE_FEATURE_CLEAN_UP
	unsigned max_len;
#endif

	INIT_G();

	if (argv[1]) {
		if (argv[2])
			bb_show_usage();
		if (NOT_LONE_DASH(argv[1])) {
			close(STDIN_FILENO); /* == 0 */
			xopen(argv[1], O_RDONLY); /* fd will be 0 */
		}
	}

	/* Read in words separated by <blank>s */
	a = NULL;
	line = NULL;
	linesz = 0;
	while ((len = getline(&line, &linesz, stdin)) != -1) {
		char *s = line;
		while (*(s = skip_whitespace(s)) != '\0') {
			struct node *b;
			char *word;

			word = s;
			s = skip_non_whitespace(s);
			if (*s)
				*s++ = '\0';

			/* Create nodes and edges for each word pair */
			b = get_node(word);
			if (!a) {
				a = b;
			} else {
				if (a != b)
					add_edge(a, b);
				a = NULL;
			}
		}
	}
// Most other tools do not check for input read error (treat them as EOF)
//	die_if_ferror(in, input_filename);
	if (a)
		bb_simple_error_msg_and_die("odd input");
	free(line);

	/*
	 * Kahn's algorithm:
	 *   - find a node that has no incoming edges, print and remove it
	 *   - repeat until the graph is empty
	 *   - if any nodes are left, they form cycles.
	 */
	cycles = 0;
#if ENABLE_FEATURE_CLEAN_UP
	max_len = G.nodes_len;
#endif
	while (G.nodes_len) {
		struct node *n;

		/* Search for first node with no incoming edges */
		for (i = 0; i < G.nodes_len; i++) {
			if (!G.nodes[i]->in_count)
				break;
		}
		if (i == G.nodes_len) {
			/* Must be a cycle; arbitraily break it at node 0 */
			cycles++;
			i = 0;
#ifndef TINY
			bb_error_msg("cycle at %s", G.nodes[i]->name);
#endif
		}

		/* Remove the node (need no longer maintain sort) */
		n = G.nodes[i];
		G.nodes[i] = G.nodes[--G.nodes_len];
#if ENABLE_FEATURE_CLEAN_UP
		/* Keep reference to removed node so it can be freed */
		G.nodes[G.nodes_len] = n;
#endif

		/* And remove its outgoing edges */
		for (i = 0; i < n->out_count; i++)
			n->out[i]->in_count--;

		puts(n->name);
	}
#if ENABLE_FEATURE_CLEAN_UP
	for (i = 0; i < max_len; i++) {
		free(G.nodes[i]->out);
		free(G.nodes[i]);
	}
	free(G.nodes);
#endif

	fflush_stdout_and_exit(cycles ? 1 : 0);
}
