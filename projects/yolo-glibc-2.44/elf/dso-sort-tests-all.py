#!/usr/bin/env python3
# Generate all DAGs for dependency ordering of a given number of objects.
# Copyright (C) 2024-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

import argparse
import sys


def print_dag(state, dag, postorder, postorder_new):
    """Print a DAG in the form used by dso-ordering-test.py."""
    out = []
    for i in range(len(dag)):
        if dag[i]:
            if i == len(dag) - 1:
                name = '{}'
            else:
                name = chr(ord('a') + len(dag) - 2 - i)
            this_deps = [chr(ord('a') + len(dag) - 2 - j) for j in dag[i]]
            this_out = ('[%s]' % (''.join(this_deps))
                        if len(this_deps) > 1
                        else this_deps[0])
            out.append('%s->%s' % (name, this_out))
    output_old = (
        '%s>{}<%s' %
        ('>'.join(chr(ord('a') + i) for i in range(len(dag) - 2, -1, -1)),
         '<'.join(chr(ord('a') + i) for i in range(0, len(dag) - 1))))
    if postorder == postorder_new:
        print('tst-dso-ordering-all%d-%d: %s\n'
              'output: %s\n'
              % (len(dag), state['num_dags'], ';'.join(out), output_old))
    else:
        names_new = [chr(ord('a') + len(dag) - 2 - x)
                     for x in postorder_new[:-1]]
        output_new = '%s>{}<%s' % ('>'.join(names_new),
                                   '<'.join(reversed(names_new)))
        print('tst-dso-ordering-all%d-%d: %s\n'
              'output(glibc.rtld.dynamic_sort=1): %s\n'
              'output(glibc.rtld.dynamic_sort=2): %s\n'
              % (len(dag), state['num_dags'], ';'.join(out), output_old,
                 output_new))
    state['num_dags'] += 1


def gen_postorder_old(dag, postorder):
    """Generate a postorder traversal of the vertices of the given
    DAG, in the particular choice of ordering that corresponds to how
    the dynamic linker sorts constructor executions (old algorithm)."""
    # First list all the vertices, breadth-first.
    postorder.append(len(dag) - 1)
    for i in range(len(dag)):
        for v in dag[postorder[i]]:
            if v not in postorder:
                postorder.append(v)
    # Now move any vertex with an edge from a later one to just after
    # the last vertex with an edge to it (emulating the older dynamic
    # linker algorithm).
    changed = True
    while changed:
        changed = False
        i = 0
        while i < len(dag):
            move_past = None
            for k in range(len(dag) - 1, i, -1):
                if postorder[i] in dag[postorder[k]]:
                    move_past = k
                    break
            if move_past is None:
                i += 1
            else:
                changed = True
                postorder[i:k+1] = postorder[i+1:k+1] + [postorder[i]]
    # Finally, reverse the list.
    postorder.reverse()


def gen_postorder_dfs(dag, postorder, v):
    """Traverse the dependencies of a vertex as part of generating a
    postorder traversal of the given DAG (new algorithm)."""
    if v in postorder:
        return
    for d in dag[v]:
        gen_postorder_dfs(dag, postorder, d)
    postorder.append(v)


def gen_postorder_new(dag, postorder):
    """Generate a postorder traversal of the vertices of the given
    DAG, in the particular choice of ordering that corresponds to how
    the dynamic linker sorts constructor executions (new algorithm)."""
    # First list all the vertices, breadth-first.
    tmp = []
    tmp.append(len(dag) - 1)
    for i in range(len(dag)):
        for v in dag[tmp[i]]:
            if v not in tmp:
                tmp.append(v)
    # Starting at the end of the breadth-first list, do depth-first
    # traversal of dependencies to add to the final ordering.
    for v in reversed(tmp):
        gen_postorder_dfs(dag, postorder, v)


def gen_orderings_rec_sub(state, dag, num_done, num_swaps_done):
    """Generate possible orderings for the edges out from each vertex
    of a DAG and test whether a postorder traversal yields the
    vertices in order, where orderings have already been generated for
    some number of vertices and some number of initial edges have been
    chosen in the ordering for the next vertex."""
    if num_swaps_done >= len(dag[num_done]) - 1:
        gen_orderings_rec(state, dag, num_done + 1)
    else:
        for i in range(num_swaps_done, len(dag[num_done])):
            ndag = dag
            if i != num_swaps_done:
                ndag = ndag.copy()
                ndag[num_done] = ndag[num_done].copy()
                first = ndag[num_done][num_swaps_done]
                second = ndag[num_done][i]
                ndag[num_done][i] = first
                ndag[num_done][num_swaps_done] = second
            gen_orderings_rec_sub(state, ndag, num_done, num_swaps_done + 1)

def gen_orderings_rec(state, dag, num_done):
    """Generate possible orderings for the edges out from each vertex
    of a DAG and test whether a postorder traversal yields the
    vertices in order, where orderings have already been generated for
    some number of vertices."""
    if num_done == len(dag):
        postorder = []
        gen_postorder_old(dag, postorder)
        if postorder == sorted(postorder):
            postorder_new = []
            gen_postorder_new(dag, postorder_new)
            print_dag(state, dag, postorder, postorder_new)
    else:
        gen_orderings_rec_sub(state, dag, num_done, 0)


def gen_orderings(state, dag):
    """Generate possible orderings for the edges out from each vertex
    of a DAG and test whether a postorder traversal yields the
    vertices in order."""
    gen_orderings_rec(state, dag, 0)


def gen_dags_rec_sub(state, partial_dag, num_vertices, num_done_last):
    """Generate DAGs, where a partial DAG for an initial subsequence
    of the vertices, and partial information about edges from the last
    vertex, are passed in."""
    if num_done_last == len(partial_dag) - 1:
        gen_dags_rec(state, partial_dag, num_vertices)
    else:
        # Recurse with an edge to vertex num_done_last.
        new_dag = partial_dag.copy()
        new_dag[-1] = new_dag[-1].copy()
        new_dag[-1].append(num_done_last)
        gen_dags_rec_sub(state, new_dag, num_vertices, num_done_last + 1)
        # Recurse without an edge to vertex num_done_last, unless this is
        # the last vertex and num_done_last is not otherwise reachable.
        can_recurse_without = len(partial_dag) < num_vertices
        if not can_recurse_without:
            for i in range(num_done_last + 1, len(partial_dag) - 1):
                if num_done_last in partial_dag[i]:
                    can_recurse_without = True
                    break
        if can_recurse_without:
            gen_dags_rec_sub(state, partial_dag, num_vertices,
                             num_done_last + 1)


def gen_dags_rec(state, partial_dag, num_vertices):
    """Generate DAGs, where a partial DAG for an initial subsequence
    of the vertices is passed in."""
    if len(partial_dag) == num_vertices:
        gen_orderings(state, partial_dag)
    else:
        partial_dag = partial_dag.copy()
        partial_dag.append([])
        gen_dags_rec_sub(state, partial_dag, num_vertices, 0)


def gen_dags(state, num_vertices):
    """Generate DAGs with the given number of vertices, last vertex a
    distinguished root vertex from which all the others can be
    reached, order of edges from each vertex considered significant,
    such that a postorder traversal (corresponding to the order in
    which DSO dependency constructors are executed) yields the
    vertices in order."""
    gen_dags_rec(state, [[]], num_vertices)


def main(argv):
    """The main entry point."""
    parser = argparse.ArgumentParser(
        description='Generate DAGs to test DSO dependency ordering.')
    parser.add_argument('num_objects', help='number of objects in DAG')
    print('tunable_option: glibc.rtld.dynamic_sort=1\n'
          'tunable_option: glibc.rtld.dynamic_sort=2\n')
    gen_dags({'num_dags': 0}, int(parser.parse_args(argv).num_objects))


if __name__ == '__main__':
    main(sys.argv[1:])
