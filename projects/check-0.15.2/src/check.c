/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "../lib/libcompat.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#if defined(HAVE_FORK) && HAVE_FORK==1
#include <unistd.h>
#endif /* HAVE_FORK */

#include "check.h"
#include "check_error.h"
#include "check_list.h"
#include "check_impl.h"
#include "check_msg.h"

#ifndef DEFAULT_TIMEOUT
#define DEFAULT_TIMEOUT 4
#endif

/*
 * When a process exits either normally, with exit(), or
 * by an uncaught signal, The lower 0x377 bits are passed
 * to the parent. Of those, only the lower 8 bits are
 * returned by the WEXITSTATUS() macro.
 */
#define WEXITSTATUS_MASK 0xFF

int check_major_version = CHECK_MAJOR_VERSION;
int check_minor_version = CHECK_MINOR_VERSION;
int check_micro_version = CHECK_MICRO_VERSION;

const char* current_test_name = NULL;

static int non_pass(enum test_result);
static Fixture *fixture_create(SFun fun, int ischecked);
static void tcase_add_fixture(TCase * tc, SFun setup, SFun teardown,
                              int ischecked);
static void tr_init(TestResult * tr);
static void suite_free(Suite * s);
static void tcase_free(TCase * tc);

Suite *suite_create(const char *name)
{
    Suite *s;

    s = (Suite *)emalloc(sizeof(Suite)); /* freed in suite_free */
    if(name == NULL)
        s->name = "";
    else
        s->name = name;
    s->tclst = check_list_create();
    return s;
}

int suite_tcase(Suite * s, const char *tcname)
{
    List *l;

    if(s == NULL)
        return 0;

    l = s->tclst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        TCase *tc = (TCase *)check_list_val(l);
        if(strcmp(tcname, tc->name) == 0)
            return 1;
    }

    return 0;
}

static void suite_free(Suite * s)
{
    List *l;

    if(s == NULL)
        return;
    l = s->tclst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        tcase_free((TCase *)check_list_val(l));
    }
    check_list_free(s->tclst);
    free(s);
}


TCase *tcase_create(const char *name)
{
    char *env;
    double timeout_sec = DEFAULT_TIMEOUT;

    TCase *tc = (TCase *)emalloc(sizeof(TCase)); /*freed in tcase_free */

    if(name == NULL)
        tc->name = "";
    else
        tc->name = name;

    env = getenv("CK_DEFAULT_TIMEOUT");
    if(env != NULL)
    {
        char *endptr = NULL;
        double tmp = strtod(env, &endptr);

        if(tmp >= 0 && endptr != env && (*endptr) == '\0')
        {
            timeout_sec = tmp;
        }
    }

    env = getenv("CK_TIMEOUT_MULTIPLIER");
    if(env != NULL)
    {
        char *endptr = NULL;
        double tmp = strtod(env, &endptr);

        if(tmp >= 0 && endptr != env && (*endptr) == '\0')
        {
            timeout_sec = timeout_sec * tmp;
        }
    }

    tc->timeout.tv_sec = (time_t) floor(timeout_sec);
    tc->timeout.tv_nsec =
        (long)((timeout_sec -
                floor(timeout_sec)) * (double)NANOS_PER_SECONDS);

    tc->tflst = check_list_create();
    tc->unch_sflst = check_list_create();
    tc->ch_sflst = check_list_create();
    tc->unch_tflst = check_list_create();
    tc->ch_tflst = check_list_create();
    tc->tags = check_list_create();

    return tc;
}

/*
 * Helper function to create a list of tags from
 * a space separated string.
 */
List *tag_string_to_list(const char *tags_string)
{
    List *list;
    char *tags;
    char *tag;

    list = check_list_create();

    if (NULL == tags_string)
    {
        return list;
    }

    tags = strdup(tags_string);
    tag = strtok(tags, " ");
    while (tag)
    {
        check_list_add_end(list, strdup(tag));
        tag = strtok(NULL, " ");
    }
    free(tags);
    return list;
}

void tcase_set_tags(TCase * tc, const char *tags_orig)
{
    /* replace any pre-existing list */
    if (tc->tags)
    {
        check_list_apply(tc->tags, free);
        check_list_free(tc->tags);
    }
    tc->tags = tag_string_to_list(tags_orig);
}

static void tcase_free(TCase * tc)
{
    check_list_apply(tc->tflst, free);
    check_list_apply(tc->unch_sflst, free);
    check_list_apply(tc->ch_sflst, free);
    check_list_apply(tc->unch_tflst, free);
    check_list_apply(tc->ch_tflst, free);
    check_list_apply(tc->tags, free);
    check_list_free(tc->tflst);
    check_list_free(tc->unch_sflst);
    check_list_free(tc->ch_sflst);
    check_list_free(tc->unch_tflst);
    check_list_free(tc->ch_tflst);
    check_list_free(tc->tags);
    free(tc);
}

unsigned int tcase_matching_tag(TCase *tc, List *check_for)
{

    if (NULL == check_for)
    {
        return 0;
    }

    for(check_list_front(check_for); !check_list_at_end(check_for);
        check_list_advance(check_for))
    {
        for(check_list_front(tc->tags); !check_list_at_end(tc->tags);
            check_list_advance(tc->tags))
        {
            if (0 == strcmp((const char *)check_list_val(tc->tags),
                    (const char *)check_list_val(check_for)))
            {
            return 1;
            }
        }
    }
    return 0;
}

void suite_add_tcase(Suite * s, TCase * tc)
{
    if(s == NULL || tc == NULL || check_list_contains(s->tclst, tc))
    {
        return;
    }

    check_list_add_end(s->tclst, tc);
}

void _tcase_add_test(TCase * tc, const TTest * ttest,
                     int _signal, int allowed_exit_value,
                     int start, int end)
{
    TF *tf;

    if(tc == NULL || ttest == NULL)
        return;
    tf = (TF *)emalloc(sizeof(TF));   /* freed in tcase_free */
    tf->ttest = ttest;
    tf->loop_start = start;
    tf->loop_end = end;
    tf->signal = _signal;       /* 0 means no signal expected */
    tf->allowed_exit_value =
      (WEXITSTATUS_MASK & allowed_exit_value);   /* 0 is default successful exit */
    check_list_add_end(tc->tflst, tf);
}

static Fixture *fixture_create(SFun fun, int ischecked)
{
    Fixture *f;

    f = (Fixture *)emalloc(sizeof(Fixture));
    f->fun = fun;
    f->ischecked = ischecked;

    return f;
}

void tcase_add_unchecked_fixture(TCase * tc, SFun setup, SFun teardown)
{
    tcase_add_fixture(tc, setup, teardown, 0);
}

void tcase_add_checked_fixture(TCase * tc, SFun setup, SFun teardown)
{
    tcase_add_fixture(tc, setup, teardown, 1);
}

static void tcase_add_fixture(TCase * tc, SFun setup, SFun teardown,
                              int ischecked)
{
    if(setup)
    {
        if(ischecked)
            check_list_add_end(tc->ch_sflst,
                               fixture_create(setup, ischecked));
        else
            check_list_add_end(tc->unch_sflst,
                               fixture_create(setup, ischecked));
    }

    /* Add teardowns at front so they are run in reverse order. */
    if(teardown)
    {
        if(ischecked)
            check_list_add_front(tc->ch_tflst,
                                 fixture_create(teardown, ischecked));
        else
            check_list_add_front(tc->unch_tflst,
                                 fixture_create(teardown, ischecked));
    }
}

void tcase_set_timeout(TCase * tc, double timeout)
{
#if defined(HAVE_FORK)
    if(timeout >= 0)
    {
        char *env = getenv("CK_TIMEOUT_MULTIPLIER");

        if(env != NULL)
        {
            char *endptr = NULL;
            double tmp = strtod(env, &endptr);

            if(tmp >= 0 && endptr != env && (*endptr) == '\0')
            {
                timeout = timeout * tmp;
            }
        }

        tc->timeout.tv_sec = (time_t) floor(timeout);
        tc->timeout.tv_nsec =
            (long)((timeout - floor(timeout)) * (double)NANOS_PER_SECONDS);
    }
#else
    (void)tc;
    (void)timeout;
    /* Ignoring, as Check is not compiled with fork support. */
#endif /* HAVE_FORK */
}

void tcase_fn_start(const char *fname, const char *file,
                    int line)
{
    send_ctx_info(CK_CTX_TEST);
    send_loc_info(file, line);

    current_test_name = fname;
}

const char* tcase_name(void)
{
    return current_test_name;
}

void _mark_point(const char *file, int line)
{
    send_loc_info(file, line);
}

void _ck_assert_failed(const char *file, int line, const char *expr,
                       const char *msg, ...)
{
    char buf[BUFSIZ];
    const char *to_send;

    send_loc_info(file, line);

    /*
     * If a message was passed, format it with vsnprintf.
     * Otherwise, print the expression as is.
     */
    if(msg != NULL)
    {
        va_list ap;
        va_start(ap, msg);
        vsnprintf(buf, BUFSIZ, msg, ap);
        va_end(ap);
        to_send = buf;
    }
    else
    {
        to_send = expr;
    }

    send_failure_info(to_send);
    if(cur_fork_status() == CK_FORK)
    {
#if defined(HAVE_FORK) && HAVE_FORK==1
        _exit(1);
#endif /* HAVE_FORK */
    }
    else
    {
        longjmp(error_jmp_buffer, 1);
    }
}

SRunner *srunner_create(Suite * s)
{
    SRunner *sr = (SRunner *)emalloc(sizeof(SRunner));     /* freed in srunner_free */

    sr->slst = check_list_create();
    if(s != NULL)
        check_list_add_end(sr->slst, s);
    sr->stats = (TestStats *)emalloc(sizeof(TestStats));     /* freed in srunner_free */
    sr->stats->n_checked = sr->stats->n_failed = sr->stats->n_errors = 0;
    sr->resultlst = check_list_create();
    sr->log_fname = NULL;
    sr->xml_fname = NULL;
    sr->tap_fname = NULL;
    sr->loglst = NULL;

#if defined(HAVE_FORK)
    sr->fstat = CK_FORK_GETENV;
#else
    /*
     * Overriding the default of running tests in fork mode,
     * as this system does not have fork()
     */
    sr->fstat = CK_NOFORK;
#endif /* HAVE_FORK */

    return sr;
}

void srunner_add_suite(SRunner * sr, Suite * s)
{
    if(s == NULL)
        return;

    check_list_add_end(sr->slst, s);
}

void srunner_free(SRunner * sr)
{
    List *l;

    if(sr == NULL)
        return;

    free(sr->stats);
    l = sr->slst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        suite_free((Suite *)check_list_val(l));
    }
    check_list_free(sr->slst);

    l = sr->resultlst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        TestResult *tr = (TestResult *)check_list_val(l);
        tr_free(tr);
    }
    check_list_free(sr->resultlst);

    free(sr);
}

int srunner_ntests_failed(SRunner * sr)
{
    return sr->stats->n_failed + sr->stats->n_errors;
}

int srunner_ntests_run(SRunner * sr)
{
    return sr->stats->n_checked;
}

TestResult **srunner_failures(SRunner * sr)
{
    int i = 0;
    TestResult **trarray;
    List *rlst;

    trarray = (TestResult **)emalloc(sizeof(trarray[0]) * srunner_ntests_failed(sr));

    rlst = sr->resultlst;
    for(check_list_front(rlst); !check_list_at_end(rlst);
        check_list_advance(rlst))
    {
        TestResult *tr = (TestResult *)check_list_val(rlst);

        if(non_pass(tr->rtype))
            trarray[i++] = tr;

    }
    return trarray;
}

TestResult **srunner_results(SRunner * sr)
{
    int i = 0;
    TestResult **trarray;
    List *rlst;

    trarray =(TestResult **) emalloc(sizeof(trarray[0]) * srunner_ntests_run(sr));

    rlst = sr->resultlst;
    for(check_list_front(rlst); !check_list_at_end(rlst);
        check_list_advance(rlst))
    {
        trarray[i++] = (TestResult *)check_list_val(rlst);
    }
    return trarray;
}

static int non_pass(enum test_result val)
{
    return val != CK_PASS;
}

TestResult *tr_create(void)
{
    TestResult *tr;

    tr = (TestResult *)emalloc(sizeof(TestResult));
    tr_init(tr);
    return tr;
}

static void tr_init(TestResult * tr)
{
    tr->ctx = CK_CTX_INVALID;
    tr->line = -1;
    tr->rtype = CK_TEST_RESULT_INVALID;
    tr->msg = NULL;
    tr->file = NULL;
    tr->tcname = NULL;
    tr->tname = NULL;
    tr->duration = -1;
}

void tr_free(TestResult * tr)
{
    free(tr->file);
    free(tr->msg);
    free(tr);
}


const char *tr_msg(TestResult * tr)
{
    return tr->msg;
}

int tr_lno(TestResult * tr)
{
    return tr->line;
}

const char *tr_lfile(TestResult * tr)
{
    return tr->file;
}

int tr_rtype(TestResult * tr)
{
    return tr->rtype;
}

enum ck_result_ctx tr_ctx(TestResult * tr)
{
    return tr->ctx;
}

const char *tr_tcname(TestResult * tr)
{
    return tr->tcname;
}

static enum fork_status _fstat = CK_FORK;

void set_fork_status(enum fork_status fstat)
{
    if(fstat == CK_FORK || fstat == CK_NOFORK || fstat == CK_FORK_GETENV)
        _fstat = fstat;
    else
        eprintf("Bad status in set_fork_status", __FILE__, __LINE__);
}

enum fork_status cur_fork_status(void)
{
    return _fstat;
}

/**
 * Not all systems support the same clockid_t's. This call checks
 * if the CLOCK_MONOTONIC clockid_t is valid. If so, that is returned,
 * otherwise, CLOCK_REALTIME is returned.
 *
 * The clockid_t that was found to work on the first call is
 * cached for subsequent calls.
 */
clockid_t check_get_clockid()
{
    static clockid_t clockid = -1;

/*
 * Only check if we have librt available. Otherwise, the clockid
 * will be ignored anyway, as the clock_gettime() and
 * timer_create() functions will be re-implemented in libcompat.
 * Worse, if librt and alarm() are unavailable, this check
 * will result in an assert(0).
 */
#ifdef HAVE_LIBRT
    timer_t timerid;

    if(timer_create(CLOCK_MONOTONIC, NULL, &timerid) == 0)
    {
        timer_delete(timerid);
        clockid = CLOCK_MONOTONIC;
    }
    else
    {
        clockid = CLOCK_REALTIME;
    }
#else
    clockid = CLOCK_MONOTONIC;
#endif

    return clockid;
}
