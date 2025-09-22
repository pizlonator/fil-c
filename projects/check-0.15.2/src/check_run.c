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

#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>

#if defined(HAVE_FORK) && HAVE_FORK==1
# include <sys/wait.h>
#endif

#include "check.h"
#include "check_error.h"
#include "check_list.h"
#include "check_impl.h"
#include "check_msg.h"
#include "check_log.h"

enum rinfo
{
    CK_R_SIG,
    CK_R_PASS,
    CK_R_EXIT,
    CK_R_FAIL_TEST,
    CK_R_FAIL_FIXTURE
};

enum tf_type
{
    CK_FORK_TEST,
    CK_NOFORK_TEST,
    CK_NOFORK_FIXTURE
};


/* all functions are defined in the same order they are declared.
   functions that depend on forking are gathered all together.
   non-static functions are at the end of the file. */
static void srunner_run_init(SRunner * sr, enum print_output print_mode);
static void srunner_run_end(SRunner * sr, enum print_output print_mode);
static void srunner_iterate_suites(SRunner * sr,
                                   const char *sname, const char *tcname,
                                   const char *include_tags,
                                   const char *exclude_tags,
                                   enum print_output print_mode);
static void srunner_iterate_tcase_tfuns(SRunner * sr, TCase * tc);
static void srunner_add_failure(SRunner * sr, TestResult * tf);
static TestResult * srunner_run_setup(List * func_list,
    enum fork_status fork_usage, const char * test_name,
    const char * setup_name);
static int srunner_run_unchecked_setup(SRunner * sr, TCase * tc);
static TestResult *tcase_run_checked_setup(SRunner * sr, TCase * tc);
static void srunner_run_teardown(List * fixture_list, enum fork_status fork_usage);
static void srunner_run_unchecked_teardown(SRunner * sr, TCase * tc);
static void tcase_run_checked_teardown(TCase * tc);
static void srunner_run_tcase(SRunner * sr, TCase * tc);
static TestResult *tcase_run_tfun_nofork(SRunner * sr, TCase * tc, TF * tf,
                                         int i);
static TestResult *receive_result_info_nofork(const char *tcname,
                                              const char *tname, int iter,
                                              int duration);
static void set_nofork_info(TestResult * tr);
static char *pass_msg(void);

#if defined(HAVE_FORK) && HAVE_FORK==1
static TestResult *tcase_run_tfun_fork(SRunner * sr, TCase * tc, TF * tf,
                                       int i);
static TestResult *receive_result_info_fork(const char *tcname,
                                            const char *tname, int iter,
                                            int status, int expected_signal,
                                            signed char allowed_exit_value);
static void set_fork_info(TestResult * tr, int status, int expected_signal,
                          signed char allowed_exit_value);
static char *signal_msg(int sig);
static char *signal_error_msg(int signal_received, int signal_expected);
static char *exit_msg(int exitstatus);
static int waserror(int status, int expected_signal);

static int alarm_received;
static pid_t group_pid;
static struct sigaction sigint_old_action;
static struct sigaction sigterm_old_action;

static void CK_ATTRIBUTE_UNUSED sig_handler(int sig_nr)
{
    switch (sig_nr)
    {
        case SIGALRM:
            alarm_received = 1;
            killpg(group_pid, SIGKILL);
            break;
        case SIGTERM:
        case SIGINT:
        {
            pid_t own_group_pid;
            int child_sig = SIGTERM;

            if (sig_nr == SIGINT)
            {
                child_sig = SIGKILL;
                sigaction(SIGINT, &sigint_old_action, NULL);
            }
            else
            {
                sigaction(SIGTERM, &sigterm_old_action, NULL);
            }

            killpg(group_pid, child_sig);

            /* POSIX says that calling killpg(0)
             * does not necessarily mean to call it on the callers
             * group pid! */
            own_group_pid = getpgrp();
            killpg(own_group_pid, sig_nr);
            break;
        }
        default:
            eprintf("Unhandled signal: %d", __FILE__, __LINE__, sig_nr);
            break;
    }
}
#endif /* HAVE_FORK */

#define MSG_LEN 100

static void srunner_run_init(SRunner * sr, enum print_output print_mode)
{
    set_fork_status(srunner_fork_status(sr));
    setup_messaging();
    srunner_init_logging(sr, print_mode);
    log_srunner_start(sr);
}

static void srunner_run_end(SRunner * sr,
                            enum print_output CK_ATTRIBUTE_UNUSED print_mode)
{
    log_srunner_end(sr);
    srunner_end_logging(sr);
    teardown_messaging();
    set_fork_status(CK_FORK);
}

static void srunner_iterate_suites(SRunner * sr,
                                   const char *sname, const char *tcname,
                                   const char *include_tags,
                                   const char *exclude_tags,
                                   enum print_output CK_ATTRIBUTE_UNUSED
                                   print_mode)
{
    List *include_tag_lst;
    List *exclude_tag_lst;
    List *slst;
    List *tcl;
    TCase *tc;

    slst = sr->slst;

    include_tag_lst = tag_string_to_list(include_tags);
    exclude_tag_lst = tag_string_to_list(exclude_tags);

    for(check_list_front(slst); !check_list_at_end(slst);
        check_list_advance(slst))
    {
        Suite *s = (Suite *)check_list_val(slst);

        if(((sname != NULL) && (strcmp(sname, s->name) != 0))
           || ((tcname != NULL) && (!suite_tcase(s, tcname))))
            continue;

        log_suite_start(sr, s);

        tcl = s->tclst;

        for(check_list_front(tcl); !check_list_at_end(tcl);
            check_list_advance(tcl))
        {
            tc = (TCase *)check_list_val(tcl);

            if((tcname != NULL) && (strcmp(tcname, tc->name) != 0))
            {
                continue;
            }
            if (include_tags != NULL)
            {
                if (!tcase_matching_tag(tc, include_tag_lst))
                {
                    continue;
                }
            }
            if (exclude_tags != NULL)
            {
                if (tcase_matching_tag(tc, exclude_tag_lst))
                {
                    continue;
                }
            }

            srunner_run_tcase(sr, tc);
        }

        log_suite_end(sr, s);
    }

    check_list_apply(include_tag_lst, free);
    check_list_apply(exclude_tag_lst, free);
    check_list_free(include_tag_lst);
    check_list_free(exclude_tag_lst);
}

static void srunner_iterate_tcase_tfuns(SRunner * sr, TCase * tc)
{
    List *tfl;
    TF *tfun;
    TestResult *tr = NULL;

    tfl = tc->tflst;

    for(check_list_front(tfl); !check_list_at_end(tfl);
        check_list_advance(tfl))
    {
        int i;

        tfun = (TF *)check_list_val(tfl);

        for(i = tfun->loop_start; i < tfun->loop_end; i++)
        {
            log_test_start(sr, tc, tfun);
            switch (srunner_fork_status(sr))
            {
                case CK_FORK:
#if defined(HAVE_FORK) && HAVE_FORK==1
                    tr = tcase_run_tfun_fork(sr, tc, tfun, i);
#else /* HAVE_FORK */
                    eprintf("This version does not support fork", __FILE__,
                            __LINE__);
#endif /* HAVE_FORK */
                    break;
                case CK_NOFORK:
                    tr = tcase_run_tfun_nofork(sr, tc, tfun, i);
                    break;
                case CK_FORK_GETENV:
                default:
                    eprintf("Bad fork status in SRunner", __FILE__, __LINE__);
            }

            if(NULL != tr)
            {
                srunner_add_failure(sr, tr);
                log_test_end(sr, tr);
            }
        }
    }
}

static void srunner_add_failure(SRunner * sr, TestResult * tr)
{
    check_list_add_end(sr->resultlst, tr);
    sr->stats->n_checked++;     /* count checks during setup, test, and teardown */
    if(tr->rtype == CK_FAILURE)
        sr->stats->n_failed++;
    else if(tr->rtype == CK_ERROR)
        sr->stats->n_errors++;

}

static TestResult * srunner_run_setup(List * fixture_list, enum fork_status fork_usage,
    const char * test_name, const char * setup_name)
{
    TestResult *tr = NULL;

    if(fork_usage == CK_FORK)
    {
        send_ctx_info(CK_CTX_SETUP);
    }

    for(check_list_front(fixture_list); !check_list_at_end(fixture_list);
        check_list_advance(fixture_list))
    {
        Fixture *setup_fixture = (Fixture *)check_list_val(fixture_list);

        if(fork_usage == CK_NOFORK)
        {
            send_ctx_info(CK_CTX_SETUP);

            if(0 == setjmp(error_jmp_buffer))
            {
                setup_fixture->fun();
            }

            /* Stop the setup and return the failure in nofork mode. */
            tr = receive_result_info_nofork(test_name, setup_name, 0, -1);
            if(tr->rtype != CK_PASS)
            {
                break;
            }

            free(tr->file);
            free(tr->msg);
            free(tr);
            tr = NULL;
        }
        else
        {
            setup_fixture->fun();
        }
    }

    return tr;
}

static int srunner_run_unchecked_setup(SRunner * sr, TCase * tc)
{
    TestResult *tr = NULL;
    int rval = 1;

    set_fork_status(CK_NOFORK);
    tr = srunner_run_setup(tc->unch_sflst, CK_NOFORK, tc->name, "unchecked_setup");
    set_fork_status(srunner_fork_status(sr));

    if(tr != NULL && tr->rtype != CK_PASS)
    {
        srunner_add_failure(sr, tr);
        rval = 0;
    }

    return rval;
}

static TestResult *tcase_run_checked_setup(SRunner * sr, TCase * tc)
{
    TestResult *tr = srunner_run_setup(tc->ch_sflst, srunner_fork_status(sr),
        tc->name, "checked_setup");

    return tr;
}

static void srunner_run_teardown(List * fixture_list, enum fork_status fork_usage)
{
    for(check_list_front(fixture_list); !check_list_at_end(fixture_list);
        check_list_advance(fixture_list))
    {
        Fixture *fixture = (Fixture *)check_list_val(fixture_list);
        send_ctx_info(CK_CTX_TEARDOWN);

        if(fork_usage == CK_NOFORK)
        {
            if(0 == setjmp(error_jmp_buffer))
            {
                fixture->fun();
            }
            else
            {
                /* Abort the remaining teardowns */
                break;
            }
        }
        else
        {
            fixture->fun();
        }
    }
}

static void srunner_run_unchecked_teardown(SRunner * sr, TCase * tc)
{
    srunner_run_teardown(tc->unch_tflst, srunner_fork_status(sr));
}

static void tcase_run_checked_teardown(TCase * tc)
{
    srunner_run_teardown(tc->ch_tflst, CK_NOFORK);
}

static void srunner_run_tcase(SRunner * sr, TCase * tc)
{
    if(srunner_run_unchecked_setup(sr, tc))
    {
        srunner_iterate_tcase_tfuns(sr, tc);
        srunner_run_unchecked_teardown(sr, tc);
    }
}

static TestResult *tcase_run_tfun_nofork(SRunner * sr, TCase * tc, TF * tfun,
                                         int i)
{
    TestResult *tr;
    struct timespec ts_start = {0, 0}, ts_end = {0, 0};

    tr = tcase_run_checked_setup(sr, tc);
    if(tr == NULL)
    {
        clock_gettime(check_get_clockid(), &ts_start);
        if(0 == setjmp(error_jmp_buffer))
        {
            tcase_fn_start(tfun->ttest->name, tfun->ttest->file, tfun->ttest->line);
            tfun->ttest->fn(i);
        }
        clock_gettime(check_get_clockid(), &ts_end);
        tcase_run_checked_teardown(tc);
        return receive_result_info_nofork(tc->name, tfun->ttest->name, i,
                                          DIFF_IN_USEC(ts_start, ts_end));
    }

    return tr;
}

static TestResult *receive_result_info_nofork(const char *tcname,
                                              const char *tname,
                                              int iter, int duration)
{
    TestResult *tr;

    tr = receive_test_result(0);
    if(tr == NULL)
    {
        eprintf("Failed to receive test result", __FILE__, __LINE__);
    }
    else
    {
        tr->tcname = tcname;
        tr->tname = tname;
        tr->iter = iter;
        tr->duration = duration;
        set_nofork_info(tr);
    }

    return tr;
}

static void set_nofork_info(TestResult * tr)
{
    if(tr->msg == NULL)
    {
        tr->rtype = CK_PASS;
        tr->msg = pass_msg();
    }
    else
    {
        tr->rtype = CK_FAILURE;
    }
}

static char *pass_msg(void)
{
    return strdup("Passed");
}

#if defined(HAVE_FORK) && HAVE_FORK==1
static TestResult *tcase_run_tfun_fork(SRunner * sr, TCase * tc, TF * tfun,
                                       int i)
{
    pid_t pid_w;
    pid_t pid;
    int status = 0;
    struct timespec ts_start = { 0, 0 }, ts_end ={ 0, 0 };

    timer_t timerid;
    struct itimerspec timer_spec;


    pid = fork();
    if(pid == -1)
        eprintf("Error in call to fork:", __FILE__, __LINE__ - 2);
    if(pid == 0)
    {
        TestResult *tr;
        setpgid(0, 0);
        group_pid = getpgrp();
        tr = tcase_run_checked_setup(sr, tc);
        free(tr);
        clock_gettime(check_get_clockid(), &ts_start);
        tcase_fn_start(tfun->ttest->name, tfun->ttest->file, tfun->ttest->line);
        tfun->ttest->fn(i);
        clock_gettime(check_get_clockid(), &ts_end);
        tcase_run_checked_teardown(tc);
        send_duration_info(DIFF_IN_USEC(ts_start, ts_end));
        exit(EXIT_SUCCESS);
    }
    else
    {
        group_pid = pid;
    }

    alarm_received = 0;

    if(timer_create(check_get_clockid(),
                    NULL /* fire SIGALRM if timer expires */,
                    &timerid) == 0)
    {
        /* Set the timer to fire once */
        timer_spec.it_value = tc->timeout;
        timer_spec.it_interval.tv_sec = 0;
        timer_spec.it_interval.tv_nsec = 0;
        if(timer_settime(timerid, 0, &timer_spec, NULL) == 0)
        {
            do
            {
                pid_w = waitpid(pid, &status, 0);
            }
            while(pid_w == -1);
        }
        else
        {
            eprintf("Error in call to timer_settime:", __FILE__, __LINE__);
        }

        /* If the timer has not fired, disable it */
        timer_delete(timerid);
    }
    else
    {
        eprintf("Error in call to timer_create:", __FILE__, __LINE__);
    }

    killpg(pid, SIGKILL);       /* Kill remaining processes. */

    return receive_result_info_fork(tc->name, tfun->ttest->name, i, status,
                                    tfun->signal, tfun->allowed_exit_value);
}

static TestResult *receive_result_info_fork(const char *tcname,
                                            const char *tname,
                                            int iter,
                                            int status, int expected_signal,
                                            signed char allowed_exit_value)
{
    TestResult *tr;

    tr = receive_test_result(waserror(status, expected_signal));
    if(tr == NULL)
    {
        eprintf("Failed to receive test result", __FILE__, __LINE__);
    }
    else
    {
        tr->tcname = tcname;
        tr->tname = tname;
        tr->iter = iter;
        set_fork_info(tr, status, expected_signal, allowed_exit_value);
    }

    return tr;
}

static void set_fork_info(TestResult * tr, int status, int signal_expected,
                          signed char allowed_exit_value)
{
    int was_sig = WIFSIGNALED(status);
    int was_exit = WIFEXITED(status);
    signed char exit_status = WEXITSTATUS(status);
    int signal_received = WTERMSIG(status);

    if(was_sig)
    {
        if(signal_expected == signal_received)
        {
            if(alarm_received)
            {
                /* Got alarm instead of signal */
                tr->rtype = CK_ERROR;
                if(tr->msg != NULL)
                {
                    free(tr->msg);
                }
                tr->msg = signal_error_msg(signal_received, signal_expected);
            }
            else
            {
                tr->rtype = CK_PASS;
                if(tr->msg != NULL)
                {
                    free(tr->msg);
                }
                tr->msg = pass_msg();
            }
        }
        else if(signal_expected != 0)
        {
            /* signal received, but not the expected one */
            tr->rtype = CK_ERROR;
            if(tr->msg != NULL)
            {
                free(tr->msg);
            }
            tr->msg = signal_error_msg(signal_received, signal_expected);
        }
        else
        {
            /* signal received and none expected */
            tr->rtype = CK_ERROR;
            if(tr->msg != NULL)
            {
                free(tr->msg);
            }
            tr->msg = signal_msg(signal_received);
        }
    }
    else if(signal_expected == 0)
    {
        if(was_exit && exit_status == allowed_exit_value)
        {
            tr->rtype = CK_PASS;
            if(tr->msg != NULL)
            {
                free(tr->msg);
            }
            tr->msg = pass_msg();
        }
        else if(was_exit && exit_status != allowed_exit_value)
        {
            if(tr->msg == NULL)
            {                   /* early exit */
                tr->rtype = CK_ERROR;
                tr->msg = exit_msg(exit_status);
            }
            else
            {
                tr->rtype = CK_FAILURE;
            }
        }
    }
    else
    {                           /* a signal was expected and none raised */
        if(was_exit)
        {
            if(tr->msg != NULL)
            {
                free(tr->msg);
            }
            tr->msg = exit_msg(exit_status);
            if(exit_status == allowed_exit_value)
            {
                tr->rtype = CK_FAILURE; /* normal exit status */
            }
            else
            {
                tr->rtype = CK_FAILURE; /* early exit */
            }
        }
    }
}

static char *signal_msg(int signal)
{
    char *msg = (char *)emalloc(MSG_LEN);       /* free'd by caller */

    if(alarm_received)
    {
        snprintf(msg, MSG_LEN, "Test timeout expired");
    }
    else
    {
        snprintf(msg, MSG_LEN, "Received signal %d (%s)",
                 signal, strsignal(signal));
    }
    return msg;
}

static char *signal_error_msg(int signal_received, int signal_expected)
{
    char *sig_r_str;
    char *sig_e_str;
    char *msg = (char *)emalloc(MSG_LEN);       /* free'd by caller */

    sig_r_str = strdup(strsignal(signal_received));
    sig_e_str = strdup(strsignal(signal_expected));
    if(alarm_received)
    {
        snprintf(msg, MSG_LEN,
                 "Test timeout expired, expected signal %d (%s)",
                 signal_expected, sig_e_str);
    }
    else
    {
        snprintf(msg, MSG_LEN, "Received signal %d (%s), expected %d (%s)",
                 signal_received, sig_r_str, signal_expected, sig_e_str);
    }
    free(sig_r_str);
    free(sig_e_str);
    return msg;
}

static char *exit_msg(int exitval)
{
    char *msg = (char *)emalloc(MSG_LEN);       /* free'd by caller */

    snprintf(msg, MSG_LEN, "Early exit with return value %d", exitval);
    return msg;
}

static int waserror(int status, int signal_expected)
{
    int was_sig = WIFSIGNALED(status);
    int was_exit = WIFEXITED(status);
    int exit_status = WEXITSTATUS(status);
    int signal_received = WTERMSIG(status);

    return ((was_sig && (signal_received != signal_expected)) ||
            (was_exit && exit_status != 0));
}
#endif /* HAVE_FORK */

enum fork_status srunner_fork_status(SRunner * sr)
{
    if(sr->fstat == CK_FORK_GETENV)
    {
        char *env = getenv("CK_FORK");

        if(env == NULL)
#if defined(HAVE_FORK) && HAVE_FORK==1
            return CK_FORK;
#else
            return CK_NOFORK;
#endif
        if(strcmp(env, "no") == 0)
            return CK_NOFORK;
#if defined(HAVE_FORK) && HAVE_FORK==1
        return CK_FORK;
#else /* HAVE_FORK */
        /* Ignoring, as Check is not compiled with fork support. */
        return CK_NOFORK;
#endif /* HAVE_FORK */
    }
    return sr->fstat;
}

void srunner_set_fork_status(SRunner * sr, enum fork_status fstat)
{
#if !defined(HAVE_FORK) || HAVE_FORK==0
    /* If fork() is unavailable, do not allow a fork mode to be set */
    if(fstat != CK_NOFORK)
    {
        /* Overriding, as Check is not compiled with fork support. */
        fstat = CK_NOFORK;
    }
#endif /* ! HAVE_FORK */
    sr->fstat = fstat;
}

void srunner_run_all(SRunner * sr, enum print_output print_mode)
{
    srunner_run(sr, NULL,       /* All test suites.  */
                NULL,           /* All test cases.   */
                print_mode);
}

void srunner_run_tagged(SRunner * sr, const char *sname, const char *tcname,
                        const char *include_tags, const char *exclude_tags,
                        enum print_output print_mode)
{
#if defined(HAVE_SIGACTION) && defined(HAVE_FORK)
    static struct sigaction sigalarm_old_action;
    static struct sigaction sigalarm_new_action;
    static struct sigaction sigint_new_action;
    static struct sigaction sigterm_new_action;
#endif /* HAVE_SIGACTION && HAVE_FORK */

    /*  Get the selected test suite and test case from the
       environment.  */
    if(!tcname)
        tcname = getenv("CK_RUN_CASE");
    if(!sname)
        sname = getenv("CK_RUN_SUITE");
    if(!include_tags)
        include_tags = getenv("CK_INCLUDE_TAGS");
    if(!exclude_tags)
        exclude_tags = getenv("CK_EXCLUDE_TAGS");

    if(sr == NULL)
        return;
    if(print_mode >= CK_LAST)
    {
        eprintf("Bad print_mode argument to srunner_run_all: %d",
                __FILE__, __LINE__, print_mode);
    }
#if defined(HAVE_SIGACTION) && defined(HAVE_FORK)
    memset(&sigalarm_new_action, 0, sizeof(sigalarm_new_action));
    sigalarm_new_action.sa_handler = sig_handler;
    sigaction(SIGALRM, &sigalarm_new_action, &sigalarm_old_action);

    memset(&sigint_new_action, 0, sizeof(sigint_new_action));
    sigint_new_action.sa_handler = sig_handler;
    sigaction(SIGINT, &sigint_new_action, &sigint_old_action);

    memset(&sigterm_new_action, 0, sizeof(sigterm_new_action));
    sigterm_new_action.sa_handler = sig_handler;
    sigaction(SIGTERM, &sigterm_new_action, &sigterm_old_action);
#endif /* HAVE_SIGACTION && HAVE_FORK */
    srunner_run_init(sr, print_mode);
    srunner_iterate_suites(sr, sname, tcname, include_tags, exclude_tags,
                           print_mode);
    srunner_run_end(sr, print_mode);
#if defined(HAVE_SIGACTION) && defined(HAVE_FORK)
    sigaction(SIGALRM, &sigalarm_old_action, NULL);
    sigaction(SIGINT, &sigint_old_action, NULL);
    sigaction(SIGTERM, &sigterm_old_action, NULL);
#endif /* HAVE_SIGACTION && HAVE_FORK */
}

void srunner_run(SRunner * sr, const char *sname, const char *tcname,
                 enum print_output print_mode)
{
    srunner_run_tagged(sr, sname, tcname, NULL, NULL, print_mode);
}

pid_t check_fork(void)
{
#if defined(HAVE_FORK) && HAVE_FORK==1
    pid_t pid = fork();

    /* Set the process to a process group to be able to kill it easily. */
    if(pid >= 0)
    {
        setpgid(pid, group_pid);
    }
    return pid;
#else /* HAVE_FORK */
    /* Ignoring, as Check is not compiled with fork support. */
    return -1;
#endif /* HAVE_FORK */
}

void check_waitpid_and_exit(pid_t pid CK_ATTRIBUTE_UNUSED)
{
#if defined(HAVE_FORK) && HAVE_FORK==1
    pid_t pid_w;
    int status;

    if(pid > 0)
    {
        do
        {
            pid_w = waitpid(pid, &status, 0);
        }
        while(pid_w == -1);
        if(waserror(status, 0))
        {
            exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_SUCCESS);
#else /* HAVE_FORK */
    /* Ignoring, as Check is not compiled with fork support. */
    exit(EXIT_FAILURE);
#endif /* HAVE_FORK */
}
