/*
 * signals.c - signal name, and number, conversions
 * Copyright 1998-2003 by Albert Cahalan
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "signals.h"
#include "c.h"

/* Linux signals:
 *
 * SIGSYS is required by Unix98.
 * SIGEMT is part of SysV, BSD, and ancient UNIX tradition.
 *
 * They are provided by these Linux ports: alpha, mips, sparc, and sparc64.
 * You get SIGSTKFLT and SIGUNUSED instead on i386, m68k, ppc, and arm.
 * (this is a Linux & libc bug -- both must be fixed)
 *
 * Total garbage: SIGIO SIGINFO SIGIOT SIGCLD
 *                 (popular ones are handled as aliases)
 *                SIGLOST
 *                 (except on the Hurd; reused to mean a server died)
 * Nearly garbage: SIGSTKFLT SIGUNUSED (nothing else to fill slots)
 */

/* Linux 2.3.29 replaces SIGUNUSED with the standard SIGSYS signal */
#ifndef SIGSYS
#  warning Standards require that <signal.h> define SIGSYS
#  define SIGSYS SIGUNUSED
#endif

/* If we see both, it is likely SIGSTKFLT (junk) was replaced. */
#ifdef SIGEMT
#  undef SIGSTKFLT
#endif

#ifndef SIGRTMIN
#  warning Standards require that <signal.h> define SIGRTMIN; assuming 32
#  define SIGRTMIN 32
#endif

/* It seems the SPARC libc does not know the kernel supports SIGPWR. */
#if defined(__linux__) && !defined(SIGPWR)
#  warning Your header files lack SIGPWR. (assuming it is number 29)
#  define SIGPWR 29
#endif

typedef struct mapstruct {
  const char *name;
  int num;
} mapstruct;


static const mapstruct sigtable[] = {
  {"ABRT",   SIGABRT},  /* IOT */
  {"ALRM",   SIGALRM},
  {"BUS",    SIGBUS},
  {"CHLD",   SIGCHLD},  /* CLD */
  {"CONT",   SIGCONT},
#ifdef SIGEMT
  {"EMT",    SIGEMT},
#endif
  {"FPE",    SIGFPE},
  {"HUP",    SIGHUP},
  {"ILL",    SIGILL},
  {"INT",    SIGINT},
  {"KILL",   SIGKILL},
#if defined(__GNU__)
  {"LOST",   SIGLOST},  /* Hurd-specific */
#endif
  {"PIPE",   SIGPIPE},
  {"POLL",   SIGPOLL},  /* IO */
  {"PROF",   SIGPROF},
#ifdef SIGPWR
  {"PWR",    SIGPWR},
#endif
  {"QUIT",   SIGQUIT},
  {"SEGV",   SIGSEGV},
#ifdef SIGSTKFLT
  {"STKFLT", SIGSTKFLT},
#endif
  {"STOP",   SIGSTOP},
  {"SYS",    SIGSYS},   /* UNUSED */
  {"TERM",   SIGTERM},
  {"TRAP",   SIGTRAP},
  {"TSTP",   SIGTSTP},
  {"TTIN",   SIGTTIN},
  {"TTOU",   SIGTTOU},
  {"URG",    SIGURG},
  {"USR1",   SIGUSR1},
  {"USR2",   SIGUSR2},
  {"VTALRM", SIGVTALRM},
  {"WINCH",  SIGWINCH},
  {"XCPU",   SIGXCPU},
  {"XFSZ",   SIGXFSZ}
};


#define number_of_signals (sizeof(sigtable)/sizeof(mapstruct))

#define XJOIN(a, b) JOIN(a, b)
#define JOIN(a, b) a##b
#define STATIC_ASSERT(x) typedef int XJOIN(static_assert_on_line_,__LINE__)[(x) ? 1 : -1]

/* sanity check */
#if defined(__linux__)
STATIC_ASSERT(number_of_signals == 31);
#elif defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
STATIC_ASSERT(number_of_signals == 30);
#elif defined(__GNU__)
STATIC_ASSERT(number_of_signals == 31);
#elif defined(__CYGWIN__)
STATIC_ASSERT(number_of_signals == 31);
#else
#  warning Unknown operating system; assuming number_of_signals is correct
#endif

static int compare_signal_names(const void *a, const void *b){
  return strcasecmp( ((const mapstruct*)a)->name, ((const mapstruct*)b)->name );
}


const char *get_sigtable_name(int row)
{
    if (row < 0 || row >= number_of_signals)
        return NULL;
    return sigtable[row].name;
}

const int get_sigtable_num(int row)
{
    if (row < 0 || row >= number_of_signals)
        return -1;
    return sigtable[row].num;
}

/* return -1 on failure */
int signal_name_to_number(const char *restrict name){
    long val;
    int offset;

    /* clean up name */
    if(!strncasecmp(name,"SIG",3))
        name += 3;

    if(!strcasecmp(name,"CLD"))
        return SIGCHLD;
    if(!strcasecmp(name,"IO"))
        return SIGPOLL;
    if(!strcasecmp(name,"IOT"))
        return SIGABRT;
    /* search the table */
    {
        const mapstruct ms = {name,0};
        const mapstruct *restrict const ptr = bsearch(
                                                      &ms,
                                                      sigtable,
                                                      number_of_signals,
                                                      sizeof(mapstruct),
                                                      compare_signal_names);
        if(ptr)
            return ptr->num;
    }

    if(!strcasecmp(name,"RTMIN"))
        return SIGRTMIN;
    if(!strcasecmp(name,"EXIT"))
        return 0;
    if(!strcasecmp(name,"NULL"))
        return 0;

    offset = 0;
    if(!strncasecmp(name,"RTMIN+",6)) {
        name += 6;
        offset = SIGRTMIN;
    }

    /* not found, so try as a number */
    {
        char *endp;
        val = strtol(name,&endp,10);
        if(*endp || endp==name)
            return -1; /* not valid */
    }
    if(val<0 || val+SIGRTMIN>127)
        return -1; /* not valid */
    return val+offset;
}

const char *signal_number_to_name(int signo)
{
    static char buf[32];
    int n = number_of_signals;
    signo &= 0x7f; /* need to process exit values too */
    while (n--) {
        if(sigtable[n].num==signo)
            return sigtable[n].name;
    }
    if (signo == SIGRTMIN)
        return "RTMIN";
    if (signo)
        sprintf(buf, "RTMIN+%d", signo-SIGRTMIN);
    else
        strcpy(buf,"0");  /* AIX has NULL; Solaris has EXIT */
    return buf;
}

int skill_sig_option(int *argc, char **argv)
{
    int i;
    int signo = -1;
    for (i = 1; i < *argc; i++) {
        if (argv[i][0] == '-') {
            signo = signal_name_to_number(argv[i] + 1);
            if (-1 < signo) {
                memmove(argv + i, argv + i + 1,
                        sizeof(char *) * (*argc - i));
                (*argc)--;
                return signo;
            }
        }
    }
    return signo;
}


/* strtosig is similar to print_given_signals() with exception, that
 * this function takes a string, and converts it to a signal name or
 * a number string depending on which way a round conversion is
 * queried.  Non-existing signals return NULL.  Notice that the
 * returned string should be freed after use.
 */
char *strtosig(const char *restrict s)
{
    char *converted = NULL, *copy, *p, *endp;
    int i, numsignal = 0;

    copy = strdup(s);
    if (!copy)
        xerrx(EXIT_FAILURE, "cannot duplicate string");
    for (p = copy; *p != '\0'; p++)
        *p = toupper(*p);
    p = copy;
    if (p[0] == 'S' && p[1] == 'I' && p[2] == 'G')
        p += 3;
    if (isdigit(*p)){
        numsignal = strtol(s,&endp,10);
        if(*endp || endp==s){
            free(p);
            return NULL; /* not valid */
        }
    }
    if (numsignal){
        for (i = 0; i < number_of_signals; i++){
            if (numsignal == get_sigtable_num(i)){
                converted = strdup(get_sigtable_name(i));
                break;
            }
        }
    } else {
        for (i = 0; i < number_of_signals; i++){
            if (strcmp(p, get_sigtable_name(i)) == 0){
                converted = malloc(12);
                if (converted)
                    snprintf(converted, 12, "%d", sigtable[i].num);
                break;
            }
        }
    }
    free(copy);
    return converted;
}

void unix_print_signals(void)
{
   int pos = 0;
    int i = 0;
    while(++i <= number_of_signals){
        if(i-1) printf("%c", (pos>73)?(pos=0,'\n'):(pos++,' ') );
        pos += printf("%s", signal_number_to_name(i));
    }
    printf("\n");
}

void pretty_print_signals(void)
{
    int i = 0;
    while(++i <= number_of_signals){
        int n;
        n = printf("%2d %s", i, signal_number_to_name(i));
        if(n>0 && i%7)
            printf("%s", "           \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" + n);
        else
            printf("\n");
    }
    if((i-1)%7) printf("\n");
}

