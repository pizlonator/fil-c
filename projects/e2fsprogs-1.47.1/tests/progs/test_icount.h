/*
 * test_icount.h
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

void do_create_icount(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_free_icount(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_fetch(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_increment(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_decrement(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_store(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_get_size(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_dump(int argc, ss_argv_t argv, int sci_idx, void *infop);
void do_validate(int argc, ss_argv_t argv, int sci_idx, void *infop);

