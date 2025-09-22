/*
 * test_rel.h
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */



void do_brel_ma_create(int argc, ss_argv_t argv);
void do_brel_free(int argc, ss_argv_t argv);
void do_brel_put(int argc, ss_argv_t argv);
void do_brel_get(int argc, ss_argv_t argv);
void do_brel_start_iter(int argc, ss_argv_t argv);
void do_brel_next(int argc, ss_argv_t argv);
void do_brel_dump(int argc, ss_argv_t argv);
void do_brel_move(int argc, ss_argv_t argv);
void do_brel_delete(int argc, ss_argv_t argv);
void do_irel_ma_create(int argc, ss_argv_t argv);
void do_irel_free(int argc, ss_argv_t argv);
void do_irel_put(int argc, ss_argv_t argv);
void do_irel_get(int argc, ss_argv_t argv);
void do_irel_get_by_orig(int argc, ss_argv_t argv);
void do_irel_start_iter(int argc, ss_argv_t argv);
void do_irel_next(int argc, ss_argv_t argv);
void do_irel_dump(int argc, ss_argv_t argv);
void do_irel_add_ref(int argc, ss_argv_t argv);
void do_irel_start_iter_ref(int argc, ss_argv_t argv);
void do_irel_next_ref(int argc, ss_argv_t argv);
void do_irel_move(int argc, ss_argv_t argv);
void do_irel_delete(int argc, ss_argv_t argv);
