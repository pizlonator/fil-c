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

#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#if ENABLE_SUBUNIT
#include <subunit/child.h>
#endif

#include "check_error.h"
#include "check_list.h"
#include "check_impl.h"
#include "check_log.h"
#include "check_print.h"
#include "check_str.h"

/*
 * If a log file is specified to be "-", then instead of
 * opening a file the log output is printed to stdout.
 */
#define STDOUT_OVERRIDE_LOG_FILE_NAME "-"

static void srunner_send_evt(SRunner * sr, void *obj, enum cl_event evt);

void srunner_set_log(SRunner * sr, const char *fname)
{
    if(sr->log_fname)
        return;
    sr->log_fname = fname;
}

int srunner_has_log(SRunner * sr)
{
    return srunner_log_fname(sr) != NULL;
}

const char *srunner_log_fname(SRunner * sr)
{
    /* check if log filename have been set explicitly */
    if(sr->log_fname != NULL)
        return sr->log_fname;

    return getenv("CK_LOG_FILE_NAME");
}


void srunner_set_xml(SRunner * sr, const char *fname)
{
    if(sr->xml_fname)
        return;
    sr->xml_fname = fname;
}

int srunner_has_xml(SRunner * sr)
{
    return srunner_xml_fname(sr) != NULL;
}

const char *srunner_xml_fname(SRunner * sr)
{
    /* check if XML log filename have been set explicitly */
    if(sr->xml_fname != NULL)
    {
        return sr->xml_fname;
    }

    return getenv("CK_XML_LOG_FILE_NAME");
}

void srunner_set_tap(SRunner * sr, const char *fname)
{
    if(sr->tap_fname)
        return;
    sr->tap_fname = fname;
}

int srunner_has_tap(SRunner * sr)
{
    return srunner_tap_fname(sr) != NULL;
}

const char *srunner_tap_fname(SRunner * sr)
{
    /* check if tap log filename have been set explicitly */
    if(sr->tap_fname != NULL)
    {
        return sr->tap_fname;
    }

    return getenv("CK_TAP_LOG_FILE_NAME");
}

void srunner_register_lfun(SRunner * sr, FILE * lfile, int close,
                           LFun lfun, enum print_output printmode)
{
    Log *l = (Log *)emalloc(sizeof(Log));

    if(printmode == CK_ENV)
    {
        printmode = get_env_printmode();
    }

    l->lfile = lfile;
    l->lfun = lfun;
    l->close = close;
    l->mode = printmode;
    check_list_add_end(sr->loglst, l);
    return;
}

void log_srunner_start(SRunner * sr)
{
    srunner_send_evt(sr, NULL, CLSTART_SR);
}

void log_srunner_end(SRunner * sr)
{
    srunner_send_evt(sr, NULL, CLEND_SR);
}

void log_suite_start(SRunner * sr, Suite * s)
{
    srunner_send_evt(sr, s, CLSTART_S);
}

void log_suite_end(SRunner * sr, Suite * s)
{
    srunner_send_evt(sr, s, CLEND_S);
}

void log_test_start(SRunner * sr, TCase * tc, TF * tfun)
{
    char buffer[100];

    snprintf(buffer, 99, "%s:%s", tc->name, tfun->ttest->name);
    srunner_send_evt(sr, buffer, CLSTART_T);
}

void log_test_end(SRunner * sr, TestResult * tr)
{
    srunner_send_evt(sr, tr, CLEND_T);
}

static void srunner_send_evt(SRunner * sr, void *obj, enum cl_event evt)
{
    List *l;

    l = sr->loglst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        Log *lg = (Log *)check_list_val(l);
        fflush(lg->lfile);
        lg->lfun(sr, lg->lfile, lg->mode, obj, evt);
        fflush(lg->lfile);
    }
}

void stdout_lfun(SRunner * sr, FILE * file, enum print_output printmode,
                 void *obj, enum cl_event evt)
{
    Suite *s;

    switch (evt)
    {
        case CLINITLOG_SR:
            break;
        case CLENDLOG_SR:
            break;
        case CLSTART_SR:
            if(printmode > CK_SILENT)
            {
                fprintf(file, "Running suite(s):");
            }
            break;
        case CLSTART_S:
            s = (Suite *)obj;
            if(printmode > CK_SILENT)
            {
                fprintf(file, " %s\n", s->name);
            }
            break;
        case CLEND_SR:
            if(printmode > CK_SILENT)
            {
                /* we don't want a newline before printing here, newlines should
                   come after printing a string, not before.  it's better to add
                   the newline above in CLSTART_S.
                 */
                srunner_fprint(file, sr, printmode);
            }
            break;
        case CLEND_S:
            break;
        case CLSTART_T:
            break;
        case CLEND_T:
            break;
        default:
            eprintf("Bad event type received in stdout_lfun", __FILE__,
                    __LINE__);
    }


}

void lfile_lfun(SRunner * sr, FILE * file,
                enum print_output printmode CK_ATTRIBUTE_UNUSED, void *obj,
                enum cl_event evt)
{
    TestResult *tr;
    Suite *s;

    switch (evt)
    {
        case CLINITLOG_SR:
            break;
        case CLENDLOG_SR:
            break;
        case CLSTART_SR:
            break;
        case CLSTART_S:
            s = (Suite *)obj;
            fprintf(file, "Running suite %s\n", s->name);
            break;
        case CLEND_SR:
            fprintf(file, "Results for all suites run:\n");
            srunner_fprint(file, sr, CK_MINIMAL);
            break;
        case CLEND_S:
            break;
        case CLSTART_T:
            break;
        case CLEND_T:
            tr = (TestResult *)obj;
            tr_fprint(file, tr, CK_VERBOSE);
            break;
        default:
            eprintf("Bad event type received in lfile_lfun", __FILE__,
                    __LINE__);
    }


}

void xml_lfun(SRunner * sr CK_ATTRIBUTE_UNUSED, FILE * file,
              enum print_output printmode CK_ATTRIBUTE_UNUSED, void *obj,
              enum cl_event evt)
{
    TestResult *tr;
    Suite *s;
    static struct timespec ts_start = { 0, 0 };
    static char t[sizeof "yyyy-mm-dd hh:mm:ss"] = { 0 };

    if(t[0] == 0)
    {
        struct timeval inittv;
        struct tm now;

        gettimeofday(&inittv, NULL);
        clock_gettime(check_get_clockid(), &ts_start);
        if(localtime_r((const time_t *)&(inittv.tv_sec), &now) != NULL)
        {
            strftime(t, sizeof("yyyy-mm-dd hh:mm:ss"), "%Y-%m-%d %H:%M:%S",
                     &now);
        }
    }

    switch (evt)
    {
        case CLINITLOG_SR:
            fprintf(file,
            "<?xml version=\"1.0\"?>\n"
            "<?xml-stylesheet type=\"text/xsl\" "
            "href=\"http://check.sourceforge.net/xml/check_unittest.xslt\"?>\n"
            "<testsuites xmlns=\"http://check.sourceforge.net/ns\">\n"
            "  <datetime>%s</datetime>\n", t);
            break;
        case CLENDLOG_SR:
        {
            struct timespec ts_end = { 0, 0 };
            unsigned long duration;

            /* calculate time the test were running */
            clock_gettime(check_get_clockid(), &ts_end);
            duration = (unsigned long)DIFF_IN_USEC(ts_start, ts_end);
            fprintf(file,
            "  <duration>%lu.%06lu</duration>\n"
            "</testsuites>\n",
                    duration / US_PER_SEC, duration % US_PER_SEC);
        }
            break;
        case CLSTART_SR:
            break;
        case CLSTART_S:
            s = (Suite *)obj;
            fprintf(file,
            "  <suite>\n"
            "    <title>");
            fprint_xml_esc(file, s->name);
            fprintf(file, "</title>\n");
            break;
        case CLEND_SR:
            break;
        case CLEND_S:
            fprintf(file, "  </suite>\n");
            break;
        case CLSTART_T:
            break;
        case CLEND_T:
            tr = (TestResult *)obj;
            tr_xmlprint(file, tr, CK_VERBOSE);
            break;
        default:
            eprintf("Bad event type received in xml_lfun", __FILE__,
                    __LINE__);
    }

}

void tap_lfun(SRunner * sr CK_ATTRIBUTE_UNUSED, FILE * file,
              enum print_output printmode CK_ATTRIBUTE_UNUSED, void *obj,
              enum cl_event evt)
{
    TestResult *tr;

    static int num_tests_run = 0;

    switch (evt)
    {
        case CLINITLOG_SR:
            /* As this is a new log file, reset the number of tests executed */
            num_tests_run = 0;
            break;
        case CLENDLOG_SR:
            /* Output the test plan as the last line */
            fprintf(file, "1..%d\n", num_tests_run);
            fflush(file);
            break;
        case CLSTART_SR:
            break;
        case CLSTART_S:
            break;
        case CLEND_SR:
            break;
        case CLEND_S:
            break;
        case CLSTART_T:
            break;
        case CLEND_T:
            /* Print the test result to the tap file */
            num_tests_run += 1;
            tr = (TestResult *)obj;
            fprintf(file, "%s %d - %s:%s:%s: %s\n",
                    tr->rtype == CK_PASS ? "ok" : "not ok", num_tests_run,
                    tr->file, tr->tcname, tr->tname, tr->msg);
            fflush(file);
            break;
        default:
            eprintf("Bad event type received in tap_lfun", __FILE__,
                    __LINE__);
    }
}

#if ENABLE_SUBUNIT
void subunit_lfun(SRunner * sr, FILE * file, enum print_output printmode,
                  void *obj, enum cl_event evt)
{
    TestResult *tr;
    char const *name;

    /* assert(printmode == CK_SUBUNIT); */

    switch (evt)
    {
        case CLINITLOG_SR:
            break;
        case CLENDLOG_SR:
            break;
        case CLSTART_SR:
            break;
        case CLSTART_S:
            break;
        case CLEND_SR:
            if(printmode > CK_SILENT)
            {
                fprintf(file, "\n");
                srunner_fprint(file, sr, printmode);
            }
            break;
        case CLEND_S:
            break;
        case CLSTART_T:
            name = (const char *)obj;
            subunit_test_start(name);
            break;
        case CLEND_T:
            tr = (TestResult *)obj;
            {
                char *name = ck_strdup_printf("%s:%s", tr->tcname, tr->tname);
                char *msg = tr_short_str(tr);

                switch (tr->rtype)
                {
                    case CK_PASS:
                        subunit_test_pass(name);
                        break;
                    case CK_FAILURE:
                        subunit_test_fail(name, msg);
                        break;
                    case CK_ERROR:
                        subunit_test_error(name, msg);
                        break;
                    case CK_TEST_RESULT_INVALID:
                    default:
                        eprintf("Bad result type in subunit_lfun", __FILE__,
                                __LINE__);
                        free(name);
                        free(msg);
                }
            }
            break;
        default:
            eprintf("Bad event type received in subunit_lfun", __FILE__,
                    __LINE__);
    }
}
#endif

static FILE *srunner_open_file(const char *filename)
{
    FILE *f = NULL;

    if(strcmp(filename, STDOUT_OVERRIDE_LOG_FILE_NAME) == 0)
    {
        f = stdout;
    }
    else
    {
        f = fopen(filename, "w");
        if(f == NULL)
        {
            eprintf("Error in call to fopen while opening file %s:", __FILE__,
                    __LINE__ - 2, filename);
        }
    }
    return f;
}

FILE *srunner_open_lfile(SRunner * sr)
{
    FILE *f = NULL;

    if(srunner_has_log(sr))
    {
        f = srunner_open_file(srunner_log_fname(sr));
    }
    return f;
}

FILE *srunner_open_xmlfile(SRunner * sr)
{
    FILE *f = NULL;

    if(srunner_has_xml(sr))
    {
        f = srunner_open_file(srunner_xml_fname(sr));
    }
    return f;
}

FILE *srunner_open_tapfile(SRunner * sr)
{
    FILE *f = NULL;

    if(srunner_has_tap(sr))
    {
        f = srunner_open_file(srunner_tap_fname(sr));
    }
    return f;
}

void srunner_init_logging(SRunner * sr, enum print_output print_mode)
{
    FILE *f;

    sr->loglst = check_list_create();
#if ENABLE_SUBUNIT
    if(print_mode != CK_SUBUNIT)
#endif
        srunner_register_lfun(sr, stdout, 0, stdout_lfun, print_mode);
#if ENABLE_SUBUNIT
    else
        srunner_register_lfun(sr, stdout, 0, subunit_lfun, print_mode);
#endif
    f = srunner_open_lfile(sr);
    if(f)
    {
        srunner_register_lfun(sr, f, f != stdout, lfile_lfun, print_mode);
    }
    f = srunner_open_xmlfile(sr);
    if(f)
    {
        srunner_register_lfun(sr, f, f != stdout, xml_lfun, print_mode);
    }
    f = srunner_open_tapfile(sr);
    if(f)
    {
        srunner_register_lfun(sr, f, f != stdout, tap_lfun, print_mode);
    }
    srunner_send_evt(sr, NULL, CLINITLOG_SR);
}

void srunner_end_logging(SRunner * sr)
{
    List *l;
    int rval;

    srunner_send_evt(sr, NULL, CLENDLOG_SR);

    l = sr->loglst;
    for(check_list_front(l); !check_list_at_end(l); check_list_advance(l))
    {
        Log *lg = (Log *)check_list_val(l);

        if(lg->close)
        {
            rval = fclose(lg->lfile);
            if(rval != 0)
                eprintf("Error in call to fclose while closing log file:",
                        __FILE__, __LINE__ - 2);
        }
        free(lg);
    }
    check_list_free(l);
    sr->loglst = NULL;
}
