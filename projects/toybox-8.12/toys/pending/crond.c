/* crond.c - daemon to execute scheduled commands.
 *
 * Copyright 2014 Ranjan Kumar <ranjankumar.bth@gmail.com>
 *
 * No Standard

USE_CROND(NEWTOY(crond, "fbSl#<0=8d#<0L:c:[-bf][-LS][-ld]", TOYFLAG_USR|TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config CROND
  bool "crond"
  default n
  help
    usage: crond [-fbS] [-l N] [-d N] [-L LOGFILE] [-c DIR]

    A daemon to execute scheduled commands.

    -b Background (default)
    -c crontab dir
    -d Set log level, log to stderr
    -f Foreground
    -l Set log level. 0 is the most verbose, default 8
    -S Log to syslog (default)
    -L Log to file
*/

#define FOR_crond
#include "toys.h"

GLOBALS(
  char *c, *l;
  int loglevel_d;
  int loglevel;

  time_t crontabs_dir_mtime;
)

typedef struct _var {
  struct _var *next, *prev;
  char *name, *val;
} VAR;

typedef struct _job {
  struct _job *next, *prev;
  char min[60], hour[24], dom[31], mon[12], dow[7], *cmd;
  int isrunning, needstart, mailsize;
  pid_t pid;
} JOB;

typedef struct _cronfile {
  struct _cronfile *next, *prev;
  struct double_list *job, *var;
  char *username, *mailto;
  int invalid;
} CRONFILE;

static char days[]={"sun""mon""tue""wed""thu""fri""sat"};
static char months[]={"jan""feb""mar""apr""may""jun""jul"
  "aug""sep""oct""nov""dec"};
CRONFILE *gclist;

static void loginfo(int loglevel, char *msg, ...)
{
  va_list s, d;
  int used;
  char *smsg;

  if (loglevel < TT.loglevel) return;

  va_start(s, msg);
  va_copy(d, s);

  if (!FLAG(d) && TT.l) {
    int fd = open(TT.l, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd==-1) perror_msg("%s", TT.l);
    else {
      dup2(fd, 2);
      close(fd);
    }
  }
  used = vsnprintf(NULL, 0, msg, d);
  smsg = xzalloc(++used);
  vsnprintf(smsg, used, msg, s);
  if (FLAG(d) || TT.l) {
    fflush(NULL);
    smsg[used-1] = '\n';
    writeall((loglevel > 8) ? 2 : 1, smsg, used);
  } else syslog((loglevel > 8) ? LOG_ERR : LOG_INFO, "%s", smsg);
  free(smsg);

  va_end(d);
  va_end(s);
}

/*
 * Names can also be used for the 'month' and 'day of week' fields
 * (First three letters of the particular day or month).
 */
static int getindex(char *src, int size)
{
  int i;
  char *field = (size == 12) ? months : days;

  // strings are not allowed for min, hour and dom fields.
  if (!(size == 7 || size == 12)) return -1;

  for (i = 0; field[i]; i += 3) {
    if (!strncasecmp(src, &field[i], 3))
      return (i/3);
  }
  return -1;
}

// set elements of minute, hour, day of month, month and day of week arrays.
static void fillarray(char *dst, int start, int end, int skip)
{
  int sk = 1;

  if (end < 0) {
    dst[start] = 1;
    return;
  }
  if (!skip) skip = 1;
  do {
    if (!--sk) {
      dst[start] = 1;
      sk = skip;
    }
  } while (start++ != end);
}

static long getval(char *num, long low, long high)
{
  long val = strtol(num, &num, 10);

  if (*num || (val < low) || (val > high)) return -1;
  return val;
}

//static int parse_and_fillarray(char *dst, int size, char *src)
static int parse_and_fillarray(char *dst, int min, int max, char *src)
{
  int start, end, skip = 0;
  char *ptr = strchr(src, '/');

  if (ptr) {
    *ptr++ = 0;
    if ((skip = getval(ptr, min, (min ? max: max-1))) < 0) goto ERROR;
  }

  if (*src == '-' || *src == ',') goto ERROR;
  if (*src == '*') {
    if (*(src+1)) goto ERROR;
    fillarray(dst, 0, max-1, skip);
  } else {
    for (;;) {
      char *ctoken = strsep(&src, ","), *dtoken;

      if (!ctoken) break;
      if (!*ctoken) goto ERROR;

      // Get start position.
      dtoken = strsep(&ctoken, "-");
      if (isdigit(*dtoken)) {
        if ((start = getval(dtoken, min, (min ? max : max-1))) < 0) goto ERROR;
        start = min ? (start-1) : start;
      } else if ((start = getindex(dtoken, max)) < 0) goto ERROR;

      // Get end position.
      if (!ctoken) end = -1; // e.g. N1,N2,N3
      else if (*ctoken) {// e.g. N-M
        if (isdigit(*ctoken)) {
          if ((end = getval(ctoken, min, (min ? max : max-1))) < 0) goto ERROR;
          end = min ? (end-1) : end;
        } else if ((end = getindex(ctoken, max)) < 0) goto ERROR;
        if (end == start) end = -1;
      } else goto ERROR; // error condition 'N-'
      fillarray(dst, start, end, skip);
    }
  }

  if (FLAG(d) && (TT.loglevel <= 5)) {
    for (start = 0; start < max; start++)
      fprintf(stderr, "%d", (unsigned char)dst[start]);
    fputc('\n', stderr);
  }
  return 0;
ERROR:
  loginfo(9, "parse error at %s", src);
  return -1;
}

static char *omitspace(char *line)
{
  while (*line == ' ' || *line == '\t') line++;
  return line;
}

static void parse_line(char *line, CRONFILE *cfile)
{
  int count = 0;
  char *name, *val, *tokens[5] = {0,};
  VAR *v;
  JOB *j;

  line = omitspace(line);
  if (!*line || *line == '#') return;

  /*
   * TODO: Enhancement to support 8 special strings
   * @reboot -> Run once at startup.
   * @yearly -> Run once a year (0 0 1 1 *).
   * @annually -> Same as above.
   * @monthly -> Run once a month (0 0 1 * *).
   * @weekly -> Run once a week (0 0 * * 0).
   * @daily -> Run once a day (0 0 * * *).
   * @midnight -> same as above.
   * @hourly -> Run once an hour (0 * * * *).
   */
  if (*line == '@') return;
  if (FLAG(d)) loginfo(5, "user:%s entry:%s", cfile->username, line);
  while (count<5) {
    int len = strcspn(line, " \t");

    if (line[len]) line[len++] = '\0';
    tokens[count++] = line;
    line += len;
    line = omitspace(line);
    if (!*line) break;
  }

  switch (count) {
    case 1: // form SHELL=/bin/sh
      name = tokens[0];
      if ((val = strchr(name, '='))) *val++ = 0;
      if (!val || !*val) return;
      break;
    case 2: // form SHELL =/bin/sh or SHELL= /bin/sh
      name = tokens[0];
      if ((val = strchr(name, '='))) {
        *val = 0;
        val = tokens[1];
      } else {
        if (*(tokens[1]) != '=') return;
        val = tokens[1] + 1;
      }
      if (!*val) return;
      break;
    case 3: // NAME = VAL
      name = tokens[0];
      val = tokens[2];
      if (*(tokens[1]) != '=') return;
      break;
    case 5:
      // don't have any cmd to execute.
      if (!*line) return;
      j = xzalloc(sizeof(JOB));

      if (parse_and_fillarray(j->min, 0, sizeof(j->min), tokens[0]))
        goto STOP_PARSING;
      if (parse_and_fillarray(j->hour, 0, sizeof(j->hour), tokens[1]))
        goto STOP_PARSING;
      if (parse_and_fillarray(j->dom, 1, sizeof(j->dom), tokens[2]))
        goto STOP_PARSING;
      if (parse_and_fillarray(j->mon, 1, sizeof(j->mon), tokens[3]))
        goto STOP_PARSING;
      if (parse_and_fillarray(j->dow, 0, sizeof(j->dow), tokens[4]))
        goto STOP_PARSING;
      j->cmd = xstrdup(line);

      if (FLAG(d)) loginfo(5, " command:%s", j->cmd);
      dlist_add_nomalloc((struct double_list **)&cfile->job, (struct double_list *)j);
      return;
STOP_PARSING:
      free(j);
      return;
    default: return;
  }
  // strip the newline from val, if any
  strtok(val, "\n");
  if (!strcmp(name, "MAILTO")) cfile->mailto = xstrdup(val);
  else {
    v = xzalloc(sizeof(VAR));
    v->name = xstrdup(name);
    v->val = xstrdup(val);
    dlist_add_nomalloc((struct double_list **)&cfile->var, (struct double_list *)v);
  }
}

static void free_jobs(JOB **jlist)
{
  JOB *j = dlist_pop(jlist);
  free(j->cmd);
  free(j);
}

static void free_cronfile(CRONFILE **list)
{
  CRONFILE *l = dlist_pop(list);
  VAR *v, *vnode = (VAR *)l->var;

  if (l->username != l->mailto) free(l->mailto);
  free(l->username);
  while (vnode && (v = dlist_pop(&vnode))) {
    free(v->name);
    free(v->val);
    free(v);
  }
  free(l);
}

/*
 * Iterate all cronfiles to identify the completed jobs and freed them.
 * If all jobs got completed for a cronfile, freed cronfile too.
 */
static void remove_completed_jobs()
{
  CRONFILE *lstart, *list = gclist;

  lstart = list;
  while (list) {
    int delete = 1;
    JOB *jstart, *jlist = (JOB *)list->job;

    list->invalid = 1;
    jstart = jlist;
    while (jlist) {
      jlist->isrunning = 0;
      if (jlist->pid > 0) {
        jlist->isrunning = 1;
        delete = 0;
        jlist = jlist->next;
      } else {
        if (jlist == jstart) { // if 1st node has to delete.
          jstart = jstart->next;
          free_jobs(&jlist);
          continue;
        } else free_jobs(&jlist);
      }
      if (jlist == jstart) break;
    }
    list->job = (struct double_list *)jlist;

    if (delete) {
      if (lstart == list) {
        lstart = lstart->next;
        free_cronfile(&list);
        continue;
      } else free_cronfile(&list);
    }
    list = list->next;
    if (lstart == list) break;
  }
  gclist = list;
}

// Scan cronfiles and prepare the list of cronfiles with their jobs.
static void scan_cronfiles()
{
  DIR *dp;
  struct dirent *entry;

  remove_completed_jobs();
  if (!(dp = opendir(TT.c))) {
    loginfo(10, "chdir(%s)", TT.c);
    toys.exitval = 20;
    xexit();
  }

  while ((entry = readdir(dp))) {
    CRONFILE *cfile;
    FILE *fp;
    char *line = 0;
    size_t allocated_length = 0;

    if (isdotdot(entry->d_name)) continue;

    if (!getpwnam(entry->d_name)) {
      loginfo(7, "ignoring file '%s' (no such user)", entry->d_name);
      continue;
    }

    if (!(fp = fopen(entry->d_name, "r"))) continue;

    // one node for each user
    cfile = xzalloc(sizeof(CRONFILE));
    cfile->username = xstrdup(entry->d_name);

    while (getline(&line, &allocated_length, fp) > 0) {
      parse_line(line, cfile);
    }
    free(line);
    fclose(fp);

    // If there is no job for a cron, remove the VAR list.
    if (!cfile->job) {
      VAR *v, *vnode = (VAR *)cfile->var;

      free(cfile->username);
      if (cfile->mailto) free(cfile->mailto);

      while (vnode && (v = dlist_pop(&vnode))) {
        free(v->name);
        free(v->val);
        free(v);
      }
      free(cfile);
    } else {
      if (!cfile->mailto) cfile->mailto = cfile->username;
      dlist_add_nomalloc((struct double_list **)&gclist,
          (struct double_list *)cfile);
    }
  }
  closedir(dp);
}

/*
 * Set env variables, if any in the cronfile. Execute given job with the given
 * SHELL or Default SHELL and send an e-mail with respect to every successfully
 * completed job (as per the given param 'prog').
 */
static void do_fork(CRONFILE *cfile, JOB *job, int fd, char *prog)
{
  pid_t pid = vfork();

  if (pid == 0) {
    VAR *v, *vstart = (VAR *)cfile->var;
    struct passwd *pwd = getpwnam(cfile->username);

    if (!pwd) loginfo(9, "can't get uid for %s", cfile->username);
    else {
      char *file = "/bin/sh";

      if (setenv("USER", pwd->pw_name, 1)) _exit(1);
      if (setenv("LOGNAME", pwd->pw_name, 1)) _exit(1);
      if (setenv("HOME", pwd->pw_dir, 1)) _exit(1);
      for (v = vstart; v;) {
        if (!strcmp("SHELL", v->name)) file = v->val;
        if (setenv(v->name, v->val, 1)) _exit(1);
        if ((v=v->next) == vstart) break;
      }
      xsetuser(pwd);
      if (chdir(pwd->pw_dir)) loginfo(9, "chdir(%s)", pwd->pw_dir);
      if (prog) file = prog;
      if (FLAG(d)) loginfo(5, "child running %s", file);

      if (fd >= 0) {
        int newfd = prog ? 0 : 1;
        if (fd != newfd) {
          dup2(fd, newfd);
          close(fd);
        }
        dup2(1, 2);
      }
      setpgrp();
      execlp(file, file, (prog ? "-ti" : "-c"), (prog ? NULL : job->cmd), (char *) NULL);
      loginfo(10, "can't execute '%s' for user %s", file, cfile->username);

      if (!prog) dprintf(1, "Exec failed: %s -c %s\n", file, job->cmd);
      _exit(EXIT_SUCCESS);
    }
  }
  if (pid < 0) {
    loginfo(10, "can't vfork");
    pid = 0;
  }
  if (fd >=0) close(fd);
  job->pid = pid;
}

// Send an e-mail for each successfully completed jobs.
static void sendmail(CRONFILE *cfile, JOB *job)
{
  pid_t pid = job->pid;
  int mailfd;
  struct stat sb;

  job->pid = 0;
  if (pid <=0 || job->mailsize <=0) {
    job->isrunning = 0;
    job->needstart = 1;
    return;
  }
  snprintf(toybuf, sizeof(toybuf), "/var/spool/cron/cron.%s.%d",
      cfile->username, (int)pid);

  mailfd = open(toybuf, O_RDONLY);
  unlink(toybuf);
  if (mailfd < 0) return;

  if (fstat(mailfd, &sb) == -1 || sb.st_uid != 0 || sb.st_nlink != 0
      || sb.st_size == job->mailsize || !S_ISREG(sb.st_mode)) {
    xclose(mailfd);
    return;
  }
  job->mailsize = 0;
  do_fork(cfile, job, mailfd, "sendmail");
}

// Count the number of jobs, which are not completed.
static int count_running_jobs()
{
  CRONFILE *cfile = gclist;
  JOB *job, *jstart;
  int count = 0;

  while (cfile) {
    job = jstart = (JOB *)cfile->job;
    while (job) {
      int ret;

      if (!job->isrunning || job->pid<=0) goto NEXT_JOB;
      job->isrunning = 0;
      ret = waitpid(job->pid, NULL, WNOHANG);
      if (ret < 0 || ret == job->pid) {
        sendmail(cfile, job);
        if (job->pid) count += (job->isrunning=1);
        else {
          job->isrunning = 0;
          job->needstart = 1;
        }
      }
      else count += (job->isrunning=1);

NEXT_JOB:
      if ((job = job->next) == jstart) break;
    }
    if ((cfile = cfile->next) == gclist) break;
  }
  return count;
}

// Execute jobs one by one and prepare for the e-mail sending.
static void execute_jobs(void)
{
  CRONFILE *cfile = gclist;
  JOB *job, *jstart;

  while (cfile) {
    job = jstart = (JOB *)cfile->job;
    while (job) {
      if (job->needstart) {
        job->needstart = 0;
        if (job->pid < 0) {
          int mailfd = -1;

          job->mailsize = job->pid = 0;
          snprintf(toybuf, sizeof(toybuf), "/var/spool/cron/cron.%s.%d",
              cfile->username, getpid());
          if ((mailfd = open(toybuf, O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_APPEND,
                  0600)) < 0) {
            loginfo(10, "can't create mail file %s for user %s, "
                "discarding output", toybuf, cfile->username);
          } else {
            dprintf(mailfd, "To: %s\nSubject: cron: %s\n\n", cfile->mailto, job->cmd);
            job->mailsize = lseek(mailfd, 0, SEEK_CUR);
          }
          do_fork(cfile, job, mailfd, NULL);
          if (mailfd >= 0) {
            if (job->pid <= 0) unlink(toybuf);
            else {
              char *mailfile = xmprintf("/var/spool/cron/cron.%s.%d",
                  cfile->username, (int)job->pid);
              rename(toybuf, mailfile);
              free(mailfile);
            }
          }
          loginfo(8, "USER %s pid %3d cmd %s",
              cfile->username, job->pid, job->cmd);
          if (job->pid < 0) job->needstart = 1;
          else job->isrunning = 1;
        }
      }
      if ((job = job->next) == jstart) break;
    }
    if ((cfile = cfile->next) == gclist) break;
  }
}

// Identify jobs, which needs to be started at the given time interval.
static void schedule_jobs(time_t ctime, time_t ptime)
{
  time_t tm = ptime-ptime%60;

  for (; tm <= ctime; tm += 60) {
    struct tm *lt;
    CRONFILE *cfile = gclist;
    JOB *job, *jstart;

    if (tm <= ptime) continue;
    lt = localtime(&tm);

    while (cfile) {
      if (FLAG(d)) loginfo(5, "file %s:", cfile->username);
      if (cfile->invalid) goto NEXT_CRONFILE;
      job = jstart = (JOB *)cfile->job;

      while (job) {
        if (FLAG(d)) loginfo(5, " line %s", job->cmd);

        if (job->min[lt->tm_min] && job->hour[lt->tm_hour]
            && (job->dom[lt->tm_mday-1] && job->dow[lt->tm_wday])
            && job->mon[lt->tm_mon]) {
          if (FLAG(d))
            loginfo(5, " job: %d %s\n", (int)job->pid, job->cmd);
          if (job->pid > 0) {
            loginfo(8, "user %s: process already running: %s",
                cfile->username, job->cmd);
          } else if (!job->pid) {
            job->pid = -1;
            job->needstart = 1;
            job->isrunning = 0;
          }
        }
        if ((job = job->next) == jstart) break;
      }
NEXT_CRONFILE:
      if ((cfile = cfile->next) == gclist) break;
    }
  }
}

void crond_main(void)
{
  long long ctime, ptime, tdiff;
  int sleepfor = 60;
  struct stat sb;

  // We do this twice on nommu (because xvdaemon restart) but here for error msg
  if (TT.c) {
    if (!strend(TT.c, "/")) TT.c = xmprintf("%s/", TT.c);
  } else TT.c = "/var/spool/cron/crontabs/";
  xchdir(TT.c);

  if (!FLAG(f)) xvdaemon();

  // Setting default params.
  if (FLAG(d)) TT.loglevel = TT.loglevel_d;

  if (!FLAG(d) && !TT.l)
    openlog(toys.which->name, LOG_CONS | LOG_PID, LOG_CRON);

  // Set default shell once.
  setenv("SHELL", "/bin/sh", 1);
  loginfo(8, "crond started, log level %d", TT.loglevel);

  if (stat(TT.c, &sb)) sb.st_mtime = 0;
  TT.crontabs_dir_mtime = sb.st_mtime;
  scan_cronfiles();
  ctime = time(0);

  for (;;) {
    ptime = ctime;
    sleep(sleepfor - (ptime%sleepfor) +1);
    tdiff = (ctime = time(0)) - ptime;

    if (stat(TT.c, &sb)) sb.st_mtime = 0;
    if (TT.crontabs_dir_mtime != sb.st_mtime) {
      TT.crontabs_dir_mtime = sb.st_mtime;
      scan_cronfiles();
    }

    if (FLAG(d)) loginfo(5, "wakeup diff=%ld\n", tdiff);
    if (tdiff < -60 * 60 || tdiff > 60 * 60)
      loginfo(9, "time disparity of %ld minutes detected", tdiff / 60);
    else if (tdiff > 0) {
      schedule_jobs(ctime, ptime);
      execute_jobs();
      if (count_running_jobs()) sleepfor = 10;
      else sleepfor = 60;
    }
  }
}
