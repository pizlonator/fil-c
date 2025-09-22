/*
 * display.c - display ps output
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2012-2014 Jaromir Capik <jcapik@redhat.com
 * Copyright © 1998-2003 Albert Cahalan
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

#include <grp.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/sysmacros.h>
#include <sys/types.h>

#include "c.h"
#include "fileutils.h"
#include "signals.h"
#include "xalloc.h"

#include "common.h"

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#define SIG_IS_TERM_OR_HUP(signo) (((signo) == SIGTERM) || (signo) == SIGHUP)

char *myname;
long Hertz;

/* just reports a crash */
static void signal_handler(int signo){
  sigset_t ss;

  sigfillset(&ss);
  sigprocmask(SIG_BLOCK, &ss, NULL);
  if(signo==SIGPIPE) _exit(0);  /* "ps | head" will cause this */
  /* fprintf() is not reentrant, but we _exit() anyway */
  if (!SIG_IS_TERM_OR_HUP(signo)) {
    fprintf(stderr,
      _("Signal %d (%s) caught by %s (%s).\n"),
      signo,
      signal_number_to_name(signo),
      myname,
      PACKAGE_VERSION
    );
  }
  switch (signo) {
    case SIGHUP:
    case SIGUSR1:
    case SIGUSR2:
      exit(EXIT_FAILURE);
    default:
      if (!SIG_IS_TERM_OR_HUP(signo))
        error_at_line(0, 0, __FILE__, __LINE__, "%s", _("please report this bug"));
      signal(signo, SIG_DFL);  /* allow core file creation */
      sigemptyset(&ss);
      sigaddset(&ss, signo);
      sigprocmask(SIG_UNBLOCK, &ss, NULL);
      kill(getpid(), signo);
      _exit(EXIT_FAILURE);
  }
}

/////////////////////////////////////////////////////////////////////////////////////
#undef DEBUG
#ifdef DEBUG
void init_stack_trace(char *prog_name);

#include <ctype.h>

void hex_dump(void *vp){
  char *charlist;
  int i = 0;
  int line = 45;
  char *cp = (char *)vp;

  while(line--){
      printf("%8lx  ", (unsigned long)cp);
      charlist = cp;
      cp += 16;
      for(i=0; i<16; i++){
        if((charlist[i]>31) && (charlist[i]<127)){
          printf("%c", charlist[i]);
        }else{
          printf(".");
        }
      }
      printf(" ");
      for(i=0; i<16; i++) printf(" %2x",(unsigned int)((unsigned char)(charlist[i])));
      printf("\n");
      i=0;
  }
}

static void show_tgid(char *s, int n, sel_union *data){
  printf("%s  ", s);
  while(--n){
    printf("%d,", data[n].tgid);
  }
  printf("%d\n", data[0].tgid);
}

static void show_uid(char *s, int n, sel_union *data){
  struct passwd *pw_data;
  printf("%s  ", s);
  while(--n){
    pw_data = getpwuid(data[n].uid);
    if(pw_data) printf("%s,", pw_data->pw_name);
    else        printf("%d,", data[n].uid);
  }
  pw_data = getpwuid(data[n].uid);
  if(pw_data) printf("%s\n", pw_data->pw_name);
  else        printf("%d\n", data[n].uid);
}

static void show_gid(char *s, int n, sel_union *data){
  struct group *gr_data;
  printf("%s  ", s);
  while(--n){
    gr_data = getgrgid(data[n].gid);
    if(gr_data) printf("%s,", gr_data->gr_name);
    else        printf("%d,", data[n].gid);
  }
  gr_data = getgrgid(data[n].gid);
  if(gr_data) printf("%s\n", gr_data->gr_name);
  else        printf("%d\n", data[n].gid);
}

static void show_tty(char *s, int n, sel_union *data){
  printf("%s  ", s);
  while(--n){
    printf("%d:%d,", (int)major(data[n].tty), (int)minor(data[n].tty));
  }
  printf("%d:%d\n", (int)major(data[n].tty), (int)minor(data[n].tty));
}

static void show_cmd(char *s, int n, sel_union *data){
  printf("%s  ", s);
  while(--n){
    printf("%.8s,", data[n].cmd);
  }
  printf("%.8s\n", data[0].cmd);
}

static void arg_show(void){
  selection_node *walk = selection_list;
  while(walk){
    switch(walk->typecode){
    case SEL_RUID: show_uid("RUID", walk->n, walk->u); break;
    case SEL_EUID: show_uid("EUID", walk->n, walk->u); break;
    case SEL_SUID: show_uid("SUID", walk->n, walk->u); break;
    case SEL_FUID: show_uid("FUID", walk->n, walk->u); break;
    case SEL_RGID: show_gid("RGID", walk->n, walk->u); break;
    case SEL_EGID: show_gid("EGID", walk->n, walk->u); break;
    case SEL_SGID: show_gid("SGID", walk->n, walk->u); break;
    case SEL_FGID: show_gid("FGID", walk->n, walk->u); break;
    case SEL_PGRP: show_pid("PGRP", walk->n, walk->u); break;
    case SEL_PID : show_pid("PID ", walk->n, walk->u); break;
    case SEL_PID_QUICK : show_pid("PID_QUICK ", walk->n, walk->u); break;
    case SEL_PPID: show_pid("PPID", walk->n, walk->u); break;
    case SEL_TTY : show_tty("TTY ", walk->n, walk->u); break;
    case SEL_SESS: show_pid("SESS", walk->n, walk->u); break;
    case SEL_COMM: show_cmd("COMM", walk->n, walk->u); break;
    default: printf("Garbage typecode value!\n");
    }
    walk = walk->next;
  }
}

#endif
//////////////////////////////////////////////////////////////////////////


/***** check the header */
/* Unix98: must not print empty header */
static void check_headers(void){
  format_node *walk = format_list;
  int head_normal = 0;
  if(header_type==HEAD_MULTI){
    header_gap = screen_rows-1;  /* true BSD */
    return;
  }
  if(header_type==HEAD_NONE){
    lines_to_next_header = -1;  /* old Linux */
    return;
  }
  while(walk){
    if(!*(walk->name)){
      walk = walk->next;
      continue;
    }
    if(walk->pr){
      head_normal++;
      walk = walk->next;
      continue;
    }
    walk = walk->next;
  }
  if(!head_normal) lines_to_next_header = -1; /* how UNIX does --noheader */
}

static format_node *proc_format_list;
static format_node *task_format_list;


/***** munge lists and determine final needs */
static void lists_and_needs(void){
  check_headers();

  // only care about the difference when showing both
  if(thread_flags & TF_show_both){
    format_node pfn, tfn; // junk, to handle special case at begin of list
    format_node *walk = format_list;
    format_node *p_end = &pfn;
    format_node *t_end = &tfn;
    while(walk){
      format_node *new = xmalloc(sizeof(format_node));
      memcpy(new,walk,sizeof(format_node));
      p_end->next = walk;
      t_end->next = new;
      p_end       = walk;
      t_end       = new;
      switch(walk->flags & CF_PRINT_MASK){
      case CF_PRINT_THREAD_ONLY:
        p_end->pr   = pr_nop;
        break;
      case CF_PRINT_PROCESS_ONLY:
        t_end->pr   = pr_nop;
        break;
      default:
        catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));
        // FALL THROUGH
      case CF_PRINT_AS_NEEDED:
      case CF_PRINT_EVERY_TIME:
        break;
      }
      walk = walk->next;
    }
    t_end->next = NULL;
    p_end->next = NULL;
    proc_format_list = pfn.next;
    task_format_list = tfn.next;
  }else{
    proc_format_list = format_list;
    task_format_list = format_list;
  }
}

//////////////////////////////////////////////////////////////////////////

/***** fill in %CPU; not in libproc because of include_dead_children */
/* Note: for sorting, not display, so 0..0x7fffffff would be OK */
static void value_this_proc_pcpu(proc_t *buf){
  unsigned long long used_jiffies;
  unsigned long pcpu = 0;
  unsigned long long seconds;

  if(want_this_proc(buf)) {

    if(include_dead_children) used_jiffies = rSv(TICS_ALL_C, ull_int, buf);
    else used_jiffies = rSv(TICS_ALL, ull_int, buf);

    seconds = rSv(TIME_ELAPSED, real, buf);
    if(seconds) pcpu = (used_jiffies * 1000ULL / Hertz) / seconds;

    // if xtra-procps-debug.h active, can't use PIDS_VAL as base due to assignment
    buf->head[rel_extra].result.ul_int = pcpu;
  }
}

/***** just display */
static void simple_spew(void){
  struct pids_fetch *pidread;
  proc_t *buf;
  int i;

  // -q option (only single SEL_PID_QUICK typecode entry expected in the list, if present)
  if (selection_list && selection_list->typecode == SEL_PID_QUICK) {
    unsigned *pidlist = xcalloc(selection_list->n, sizeof(unsigned));
    enum pids_select_type which;
    for (i = 0; i < selection_list->n; i++)
      pidlist[i] = selection_list->u[selection_list->n-i-1].pid;
    which = (thread_flags & (TF_loose_tasks|TF_show_task))
      ? PIDS_SELECT_PID_THREADS : PIDS_SELECT_PID;
    pidread = procps_pids_select(Pids_info, pidlist, selection_list->n, which);
    free(pidlist);
  } else {
    enum pids_fetch_type which;
    which = (thread_flags & (TF_loose_tasks|TF_show_task))
      ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY;
    pidread = procps_pids_reap(Pids_info, which);
  }
  if (!pidread) {
    fprintf(stderr, _("fatal library error, reap\n"));
    exit(EXIT_FAILURE);
  }

  switch(thread_flags & (TF_show_proc|TF_loose_tasks|TF_show_task)){
    case TF_show_proc:                   // normal non-thread output
      for (i = 0; i < pidread->counts->total; i++) {
        buf = pidread->stacks[i];
        if (want_this_proc(buf))
          show_one_proc(buf, proc_format_list);
      }
      break;
    case TF_show_task:                   // -L and -T options
    case TF_show_proc|TF_loose_tasks:    // H option
      for (i = 0; i < pidread->counts->total; i++) {
        buf = pidread->stacks[i];
        if (want_this_proc(buf))
          show_one_proc(buf, task_format_list);
      }
      break;
    case TF_show_proc|TF_show_task:      // m and -m options
      procps_pids_sort(Pids_info, pidread->stacks
        , pidread->counts->total, PIDS_TICS_BEGAN, PIDS_SORT_ASCEND);
      procps_pids_sort(Pids_info, pidread->stacks
        , pidread->counts->total, PIDS_ID_TGID, PIDS_SORT_ASCEND);
      for (i = 0; i < pidread->counts->total; i++) {
        buf = pidread->stacks[i];
next_proc:
        if (want_this_proc(buf)) {
          int self = rSv(ID_PID, s_int, buf);
          show_one_proc(buf, proc_format_list);
          for (; i < pidread->counts->total; i++) {
            buf = pidread->stacks[i];
            if (rSv(ID_TGID, s_int, buf) != self) goto next_proc;
            show_one_proc(buf, task_format_list);
          }
        }
      }
      break;
  }
}


/***** forest output requires sorting by ppid; add start_time by default */
static void prep_forest_sort(void){
  sort_node *tmp_list = sort_list;
  const format_struct *incoming;

  if(!sort_list) {     /* assume start time order */
    incoming = search_format_array("ppid");
    if(!incoming) { fprintf(stderr, _("could not find ppid\n")); exit(1); }
    tmp_list = xmalloc(sizeof(sort_node));
    tmp_list->reverse = PIDS_SORT_ASCEND;
    tmp_list->typecode = '?'; /* what was this for? */
    tmp_list->sr = incoming->sr;
    tmp_list->next = sort_list;
    sort_list = tmp_list;
  }
  /* this is required for the forest option */
  incoming = search_format_array("start_time");
  if(!incoming) { fprintf(stderr, _("could not find start_time\n")); exit(1); }
  tmp_list = xmalloc(sizeof(sort_node));
  tmp_list->reverse = PIDS_SORT_ASCEND;
  tmp_list->typecode = '?'; /* what was this for? */
  tmp_list->sr = incoming->sr;
  tmp_list->next = sort_list;
  sort_list = tmp_list;
}

/* we rely on the POSIX requirement for zeroed memory */
static proc_t **processes;

/***** show pre-sorted array of process pointers */
static void show_proc_array(int n){
  proc_t **p = processes;
  while(n--){
    show_one_proc(*p, proc_format_list);
    p++;
  }
}

/***** show tree */
/* this needs some optimization work */
#define ADOPTED(x) 1

#define IS_LEVEL_SAFE(level) \
  ((level) >= 0 && (size_t)(level) < sizeof(forest_prefix))

static void show_tree(const int self, const int n, const int level, const int have_sibling){
  int i = 0;

  if(!IS_LEVEL_SAFE(level))
    catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));

  if(level){
    /* add prefix of "+" or "L" */
    if(have_sibling) forest_prefix[level-1] = '+';
    else             forest_prefix[level-1] = 'L';
  }
  forest_prefix[level] = '\0';
  show_one_proc(processes[self],format_list);  /* first show self */
  for(;;){  /* look for children */
    if(i >= n) return; /* no children */
    if(rSv(ID_PPID, s_int, processes[i]) == rSv(ID_PID, s_int, processes[self])) break;
    i++;
  }
  if(level){
    /* change our prefix to "|" or " " for the children */
    if(have_sibling) forest_prefix[level-1] = '|';
    else             forest_prefix[level-1] = ' ';
  }
  forest_prefix[level] = '\0';
  for(;;){
    int self_pid;
    int more_children = 1;
    if(i >= n) break; /* over the edge */
    self_pid=rSv(ID_PID, s_int, processes[self]);
    if(i+1 >= n)
      more_children = 0;
    else
      if(rSv(ID_PPID, s_int, processes[i+1]) != self_pid) more_children = 0;
    if(self_pid==1 && ADOPTED(processes[i]) && forest_type!='u')
      show_tree(i++, n, level, more_children);
    else
      show_tree(i++, n, IS_LEVEL_SAFE(level+1) ? level+1 : level, more_children);
    if(!more_children) break;
  }
  /* chop prefix that children added -- do we need this? */
  /* chop prefix that children added */
  forest_prefix[level] = '\0';
}

#undef IS_LEVEL_SAFE

/***** show forest */
static void show_forest(const int n){
  int i = n;
  int j;
  while(i--){   /* cover whole array looking for trees */
    j = n;
    while(j--){   /* search for parent: if none, i is a tree! */
      if(rSv(ID_PID, s_int, processes[j]) == rSv(ID_PPID, s_int, processes[i])) goto not_root;
    }
    show_tree(i,n,0,0);
not_root:
    ;
  }
  /* don't free the array because it takes time and ps will exit anyway */
}

#if 0
static int want_this_proc_nop(proc_t *dummy){
  (void)dummy;
  return 1;
}
#endif

/***** sorted or forest */
static void fancy_spew(void){
  struct pids_fetch *pidread;
  enum pids_fetch_type which;
  proc_t *buf;
  int i, n = 0;

  which = (thread_flags & TF_loose_tasks)
    ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY;

  pidread = procps_pids_reap(Pids_info, which);
  if (!pidread || !pidread->counts->total) {
    fprintf(stderr, _("fatal library error, reap\n"));
    exit(EXIT_FAILURE);
  }
  processes = xcalloc(pidread->counts->total, sizeof(void*));
  for (i = 0; i < pidread->counts->total; i++) {
    buf = pidread->stacks[i];
    value_this_proc_pcpu(buf);
    if (want_this_proc(buf))
      processes[n++] = buf;
  }
  if (n) {
    if(forest_type) prep_forest_sort();
    while(sort_list) {
      procps_pids_sort(Pids_info, processes, n, sort_list->sr, sort_list->reverse);
      sort_list = sort_list->next;
    }
    if(forest_type) show_forest(n);
    else show_proc_array(n);
  }
  free(processes);
}

static void arg_check_conflicts(void)
{
  int selection_list_len;
  int has_quick_pid;
  selection_node *walk = selection_list;
  has_quick_pid = 0;
  selection_list_len = 0;

  while (walk) {
    if (walk->typecode == SEL_PID_QUICK) has_quick_pid++;
    walk = walk->next;
    selection_list_len++;
  }

  /* -q doesn't allow multiple occurrences */
  if (has_quick_pid > 1) {
    fprintf(stderr, "q/-q/--quick-pid can only be used once.\n");
    exit(1);
  }

  /* -q doesn't allow combinations with other selection switches */
  if (has_quick_pid && selection_list_len > has_quick_pid) {
    fprintf(stderr, "q/-q/--quick-pid cannot be combined with other selection options.\n");
    exit(1);
  }

  /* -q cannot be used with forest type listings */
  if (has_quick_pid && forest_type) {
    fprintf(stderr, "q/-q/--quick-pid cannot be used together with forest type listings.\n");
    exit(1);
  }

  /* -q cannot be used with sort */
  if (has_quick_pid && sort_list) {
    fprintf(stderr, "q/-q,--quick-pid cannot be used together with sort options.\n");
    exit(1);
  }

  /* -q cannot be used with -N */
  if (has_quick_pid && negate_selection) {
    fprintf(stderr, "q/-q/--quick-pid cannot be used together with negation switches.\n");
    exit(1);
  }

}

static void finalize_stacks (void)
{
  format_node *f_node;
  sort_node *s_node;

#if (PIDSITEMS < 60)
 # error PIDSITEMS (common.h) should be at least 60!
#endif

  /* first, ensure minimum result structures for items
     which may or may not actually be displayable ... */
  Pids_index = 0;

  // needed by for selections
  chkREL(CMD)
  chkREL(ID_EGID)
  chkREL(ID_EUID)
  chkREL(ID_FGID)
  chkREL(ID_FUID)
  chkREL(ID_PID)
  chkREL(ID_PPID)
  chkREL(ID_RGID)
  chkREL(ID_RUID)
  chkREL(ID_SESSION)
  chkREL(ID_SGID)
  chkREL(ID_SUID)
  chkREL(ID_TGID)
  chkREL(STATE)
  chkREL(TTY)
  // needed to creata an enhanced 'stat/state'
  chkREL(ID_PGRP)
  chkREL(ID_TPGID)
  chkREL(NICE)
  chkREL(NLWP)
  chkREL(RSS)
  chkREL(VM_RSS_LOCKED)
  // needed with 's' switch, previously assured
  chkREL(SIGBLOCKED)
  chkREL(SIGCATCH)
  chkREL(SIGIGNORE)
  chkREL(SIGNALS)
  chkREL(SIGPENDING)
  // needed with loss of defunct 'cook_time' macros
  chkREL(TICS_ALL)
  chkREL(TICS_ALL_C)
  chkREL(TIME_ALL)
  chkREL(TIME_ELAPSED)
  chkREL(TICS_BEGAN)
  // special items with 'extra' used as former pcpu
  chkREL(extra)
  chkREL(noop)

  // now accommodate any results not yet satisfied
  f_node = format_list;
  while (f_node) {
    if (*f_node->pr) (*f_node->pr)(NULL, NULL);
    f_node = f_node->next;
  }
  s_node = sort_list;
  while (s_node) {
    if (s_node->xe) (*s_node->xe)(NULL, NULL);
    s_node = s_node->next;
  }

  procps_pids_reset(Pids_info, Pids_items, Pids_index);
}

/***** no comment */
int main(int argc, char *argv[]){
  atexit(close_stdout);
  myname = strrchr(*argv, '/');
  if (myname) ++myname; else myname = *argv;
  Hertz = procps_hertz_get();

  setlocale (LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  setenv("TZ", ":/etc/localtime", 0);

#ifdef DEBUG
  init_stack_trace(argv[0]);
#else
  do {
    struct sigaction sa;
    int i = 32;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigfillset(&sa.sa_mask);
    while(i--) switch(i){
    default:
      sigaction(i,&sa,NULL);
    case 0:
    case SIGCONT:
    case SIGINT:   /* ^C */
    case SIGTSTP:  /* ^Z */
    case SIGTTOU:  /* see stty(1) man page */
    case SIGQUIT:  /* ^\ */
    case SIGPROF:  /* profiling */
    case SIGKILL:  /* can not catch */
    case SIGSTOP:  /* can not catch */
    case SIGWINCH: /* don't care if window size changes */
    case SIGURG:   /* Urgent condition on socket (4.2BSD) */
      ;
    }
  } while (0);
#endif

  reset_global();  /* must be before parser */
  arg_parse(argc,argv);

  /* check for invalid combination of arguments */
  arg_check_conflicts();

/*  arg_show(); */
  trace("screen is %ux%u\n",screen_cols,screen_rows);
/*  printf("sizeof(proc_t) is %d.\n", sizeof(proc_t)); */
  trace("======= ps output follows =======\n");

  init_output(); /* must be between parser and output */

  finalize_stacks();
  lists_and_needs();

  if(forest_type || sort_list) fancy_spew(); /* sort or forest */
  else simple_spew(); /* no sort, no forest */
  show_one_proc((proc_t *)-1,format_list); /* no output yet? */

  procps_pids_unref(&Pids_info);
  return 0;
}
