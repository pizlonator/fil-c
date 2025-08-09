/* crontab.c - files used to schedule the execution of programs.
 *
 * Copyright 2014 Ranjan Kumar <ranjankumar.bth@gmail.com>
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/crontab.html

USE_CRONTAB(NEWTOY(crontab, "c:u:elr[!elr]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))

config CRONTAB
  bool "crontab"
  default n
  depends on TOYBOX_FORK
  help
    usage: crontab [-u user] FILE
                   [-u user] [-e | -l | -r]
                   [-c dir]

    Files used to schedule the execution of programs.

    -c crontab dir
    -e edit user's crontab
    -l list user's crontab
    -r delete user's crontab
    -u user
    FILE Replace crontab by FILE ('-': stdin)
*/
#define FOR_crontab
#include "toys.h"

GLOBALS(
  char *user;
  char *cdir;
)

static char *omitspace(char *line)
{
  while (*line == ' ' || *line == '\t') line++;
  return line;
}

/*
 * Names can also be used for the 'month' and 'day of week' fields
 * (First three letters of the particular day or month).
 */
static int getindex(char *src, int size)
{
  int i;
  char days[]={"sun""mon""tue""wed""thu""fri""sat"};
  char months[]={"jan""feb""mar""apr""may""jun""jul"
    "aug""sep""oct""nov""dec"};
  char *field = (size == 12) ? months : days;

  // strings are not allowed for min, hour and dom fields.
  if (!(size == 7 || size == 12)) return -1;

  for (i = 0; field[i]; i += 3) {
    if (!strncasecmp(src, &field[i], 3))
      return (i/3);
  }
  return -1;
}

static long getval(char *num, long low, long high)
{
  long val = strtol(num, &num, 10);

  if (*num || (val < low) || (val > high)) return -1;
  return val;
}

// Validate minute, hour, day of month, month and day of week fields.
static int validate_component(int min, int max, char *src)
{
  int skip = 0;
  char *ptr;

  if (!src) return 1;
  if ((ptr = strchr(src, '/'))) {
    *ptr++ = 0;
    if ((skip = getval(ptr, min, (min ? max: max-1))) < 0) return 1;
  }

  if (*src == '-' || *src == ',') return 1;
  if (*src == '*') {
    if (*(src+1)) return 1;
  }
  else {
    for (;;) {
      char *ctoken = strsep(&src, ","), *dtoken;

      if (!ctoken) break;
      if (!*ctoken) return 1;

      // validate start position.
      dtoken = strsep(&ctoken, "-");
      if (isdigit(*dtoken)) {
        if (getval(dtoken, min, (min ? max : max-1)) < 0) return 1;
      } else if (getindex(dtoken, max) < 0) return 1;

      // validate end position.
      if (!ctoken) {
        if (skip) return 1; // case 10/20 or 1,2,4/3
      }
      else if (*ctoken) {// e.g. N-M
        if (isdigit(*ctoken)) {
          if (getval(ctoken, min, (min ? max : max-1)) < 0) return 1;
        } else if (getindex(ctoken, max) < 0) return 1;
      } else return 1; // error condition 'N-'
    }
  }
  return 0;
}

static int parse_crontab(char *fname)
{
  FILE *fp = xfopen(fname, "r");
  long len = 0;
  char *line = NULL;
  size_t allocated_length;
  int lno;

  for (lno = 1; (len = getline(&line, &allocated_length, fp)) > 0; lno++) {
    char *name, *val, *tokens[5] = {0,}, *ptr = line;
    int count = 0;

    if (line[len - 1] == '\n') line[--len] = '\0';
    else {
      snprintf(toybuf, sizeof(toybuf), "'%d': premature EOF\n", lno);
      goto OUT;
    }

    ptr = omitspace(ptr);
    if (!*ptr || *ptr == '#' || *ptr == '@') continue;
    while (count<5) {
      int len = strcspn(ptr, " \t");

      if (ptr[len]) ptr[len++] = '\0';
      tokens[count++] = ptr;
      ptr += len;
      ptr = omitspace(ptr);
      if (!*ptr) break;
    }
    switch (count) {
      case 1: // form SHELL=/bin/sh
        name = tokens[0];
        if ((val = strchr(name, '='))) *val++ = 0;
        if (!val || !*val) {
          snprintf(toybuf, sizeof(toybuf), "'%d': %s\n", lno, line);
          goto OUT;
        }
        break;
      case 2: // form SHELL =/bin/sh or SHELL= /bin/sh
        name = tokens[0];
        if ((val = strchr(name, '='))) {
          *val = 0;
          val = tokens[1];
        } else {
          if (*(tokens[1]) != '=') {
            snprintf(toybuf, sizeof(toybuf), "'%d': %s\n", lno, line);
            goto OUT;
          }
          val = tokens[1] + 1;
        }
        if (!*val) {
          snprintf(toybuf, sizeof(toybuf), "'%d': %s\n", lno, line);
          goto OUT;
        }
        break;
      case 3: // NAME = VAL
        name = tokens[0];
        val = tokens[2];
        if (*(tokens[1]) != '=') {
          snprintf(toybuf, sizeof(toybuf), "'%d': %s\n", lno, line);
          goto OUT;
        }
        break;
      default:
        if (validate_component(0, 60, tokens[0])) {
          snprintf(toybuf, sizeof(toybuf), "'%d': bad minute\n", lno);
          goto OUT;
        }
        if (validate_component(0, 24, tokens[1])) {
          snprintf(toybuf, sizeof(toybuf), "'%d': bad hour\n", lno);
          goto OUT;
        }
        if (validate_component(1, 31, tokens[2])) {
          snprintf(toybuf, sizeof(toybuf), "'%d': bad day-of-month\n", lno);
          goto OUT;
        }
        if (validate_component(1, 12, tokens[3])) {
          snprintf(toybuf, sizeof(toybuf), "'%d': bad month\n", lno);
          goto OUT;
        }
        if (validate_component(0, 7, tokens[4])) {
          snprintf(toybuf, sizeof(toybuf), "'%d': bad day-of-week\n", lno);
          goto OUT;
        }
        if (!*ptr) { // don't have any cmd to execute.
          snprintf(toybuf, sizeof(toybuf), "'%d': bad command\n", lno);
          goto OUT;
        }
        break;
    }
  }
  free(line);
  fclose(fp);
  return 0;
OUT:
  free(line);
  printf("Error at line no %s", toybuf);
  fclose(fp);
  return 1;
}

static void do_list(char *name)
{
  int fdin;

  snprintf(toybuf, sizeof(toybuf), "%s%s", TT.cdir, name);
  fdin = xopenro(toybuf);
  xsendfile(fdin, 1);
  xclose(fdin);
}

static void do_remove(char *name)
{
  snprintf(toybuf, sizeof(toybuf), "%s%s", TT.cdir, name);
  if (unlink(toybuf))
    error_exit("No crontab for '%s'", name);
}

static void update_crontab(char *src, char *dest)
{
  int fdin, fdout;

  snprintf(toybuf, sizeof(toybuf), "%s%s", TT.cdir, dest);
  fdout = xcreate(toybuf, O_WRONLY|O_CREAT|O_TRUNC, 0600);
  fdin = xopenro(src);
  xsendfile(fdin, fdout);
  xclose(fdin);

  fchown(fdout, getuid(), geteuid());
  xclose(fdout);
}

static void do_replace(char *name)
{
  char *fname = *toys.optargs ? *toys.optargs : "-";
  char tname[] = "/tmp/crontab.XXXXXX";

  if ((*fname == '-') && !*(fname+1)) {
    int tfd = mkstemp(tname);

    if (tfd < 0) perror_exit("mkstemp");
    xsendfile(0, tfd);
    xclose(tfd);
    fname = tname;
  }

  if (parse_crontab(fname))
    error_exit("errors in crontab file '%s', can't install.", fname);
  update_crontab(fname, name);
  unlink(tname);
}

static void do_edit(struct passwd *pwd)
{
  struct stat sb;
  time_t mtime = 0;
  int srcfd, destfd, status;
  pid_t pid, cpid;
  char tname[] = "/tmp/crontab.XXXXXX";

  if ((destfd = mkstemp(tname)) < 0)
    perror_exit("Can't open tmp file");

  fchmod(destfd, 0666);
  snprintf(toybuf, sizeof(toybuf), "%s%s", TT.cdir, pwd->pw_name);

  if (!stat(toybuf, &sb)) { // file exists and have some content.
    if (sb.st_size) {
      srcfd = xopenro(toybuf);
      xsendfile(srcfd, destfd);
      xclose(srcfd);
    }
  } else printf("No crontab for '%s'- using an empty one\n", pwd->pw_name);
  xclose(destfd);

  if (!stat(tname, &sb)) mtime = sb.st_mtime;

RETRY:
  if (!(pid = xfork())) {
    char *prog = pwd->pw_shell;

    xsetuser(pwd);
    if (pwd->pw_uid) {
      if (setenv("USER", pwd->pw_name, 1)) _exit(1);
      if (setenv("LOGNAME", pwd->pw_name, 1)) _exit(1);
    }
    if (setenv("HOME", pwd->pw_dir, 1)) _exit(1);
    if (setenv("SHELL",((!prog || !*prog) ? "/bin/sh" : prog), 1)) _exit(1);

    if (!(prog = getenv("VISUAL"))) {
      if (!(prog = getenv("EDITOR")))
        prog = "vi";
    }
    execlp(prog, prog, tname, (char *) NULL);
    perror_exit("can't execute '%s'", prog);
  }

  // Parent Process.
  do {
    cpid = waitpid(pid, &status, 0);
  } while ((cpid == -1) && (errno == EINTR));

  if (!stat(tname, &sb) && (mtime == sb.st_mtime)) {
    printf("%s: no changes made to crontab\n", toys.which->name);
    unlink(tname);
    return;
  }
  printf("%s: installing new crontab\n", toys.which->name);
  if (parse_crontab(tname)) {
    fprintf(stderr, "errors in crontab file, can't install.\n"
        "Do you want to retry the same edit? ");
    if (!yesno(0)) {
      error_msg("edits left in '%s'", tname);
      return;
    }
    goto RETRY;
  }
  // parsing of crontab success; update the crontab.
  update_crontab(tname, pwd->pw_name);
  unlink(tname);
}

void crontab_main(void)
{
  struct passwd *pwd = NULL;
  long FLAG_elr = toys.optflags & (FLAG_e|FLAG_l|FLAG_r);

  if (TT.cdir && (TT.cdir[strlen(TT.cdir)-1] != '/'))
    TT.cdir = xmprintf("%s/", TT.cdir);
  if (!TT.cdir) TT.cdir = xstrdup("/var/spool/cron/crontabs/");

  if (toys.optflags & FLAG_u) {
    if (getuid()) error_exit("must be privileged to use -u");
    pwd = xgetpwnam(TT.user);
  } else pwd = xgetpwuid(getuid());

  if (!toys.optc) {
    if (!FLAG_elr) {
      if (toys.optflags & FLAG_u)
        help_exit("file name must be specified for replace");
      do_replace(pwd->pw_name);
    }
    else if (toys.optflags & FLAG_e) do_edit(pwd);
    else if (toys.optflags & FLAG_l) do_list(pwd->pw_name);
    else if (toys.optflags & FLAG_r) do_remove(pwd->pw_name);
  } else {
    if (FLAG_elr) help_exit("no arguments permitted after this option");
    do_replace(pwd->pw_name);
  }
  if (!(toys.optflags & FLAG_c)) free(TT.cdir);
}
