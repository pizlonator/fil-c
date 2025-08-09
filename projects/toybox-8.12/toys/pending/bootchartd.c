/* bootchartd.c - bootchartd is commonly used to profile the boot process.
 *
 * Copyright 2014 Bilal Qureshi <bilal.jmi@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_BOOTCHARTD(NEWTOY(bootchartd, 0, TOYFLAG_STAYROOT|TOYFLAG_USR|TOYFLAG_BIN))

config BOOTCHARTD
  bool "bootchartd"
  default n
  depends on TOYBOX_FORK
  help
    usage: bootchartd {start [PROG ARGS]}|stop|init

    Record boot chart data into /var/log/bootlog.tgz

    start: start background logging; with PROG, run PROG,
           then kill logging with SIGUSR1
    stop:  send SIGUSR1 to all bootchartd processes
    init:  start background logging; stop when getty/xdm is seen
          (for init scripts)

    Under PID 1: as init, then exec $bootchart_init, /init, /sbin/init
*/

#define FOR_bootchartd
#include "toys.h"

GLOBALS(
  char timestamp[32];
  long msec;
  int proc_accounting;

  pid_t pid;
)

static void dump_data_in_file(char *fname, int wfd)
{
  int rfd = open(fname, O_RDONLY);

  if (rfd != -1) {
    xwrite(wfd, TT.timestamp, strlen(TT.timestamp));
    xsendfile(rfd, wfd);
    close(rfd);
    xwrite(wfd, "\n", 1);
  }
}

static int dump_proc_data(FILE *fp)
{
  struct dirent *pid_dir;
  int login_flag = 0;
  pid_t pid;
  DIR *proc_dir = opendir("/proc");

  fputs(TT.timestamp, fp);
  while ((pid_dir = readdir(proc_dir))) {
    char filename[64];
    int fd;

    if (!isdigit(pid_dir->d_name[0])) continue;
    sscanf(pid_dir->d_name, "%d", &pid);
    sprintf(filename, "/proc/%d/stat", pid);
    if ((fd = open(filename, O_RDONLY)) != -1 ) {
      char *ptr;
      ssize_t len;

      if ((len = readall(fd, toybuf, sizeof(toybuf)-1)) < 0) {
        xclose(fd);
        continue;
      }
      toybuf[len] = 0;
      close(fd);
      fputs(toybuf, fp);
      if (TT.pid != 1) continue;
      if ((ptr = strchr(toybuf, '('))) {
        char *tmp = strchr(++ptr, ')');

        if (tmp) *tmp = 0;
      }
      // Checks for gdm, kdm or getty
      if ((strchr("gkx", *ptr) && ptr[1] == 'd' && ptr[2] == 'm')
        || strstr(ptr, "getty")) login_flag = 1;
    }
  }
  closedir(proc_dir);
  fputc('\n', fp);
  return login_flag;
}

static int parse_config_file(char *fname)
{
  size_t len = 0;
  char  *line = 0;
  FILE *fp = fopen(fname, "r");

  if (!fp) return 0;
  for (;getline(&line, &len, fp) != -1; line = 0) {
    char *ptr = line;

    while (*ptr == ' ' || *ptr == '\t') ptr++;
    if (!*ptr || *ptr == '#' || *ptr == '\n') continue;
    if (strstart(&ptr, "SAMPLE_PERIOD=")) {
      TT.msec = xparsemillitime(ptr);
      if (TT.msec<1) TT.msec = 1;
    } else if (strstart(&ptr, "PROCESS_ACCOUNTING="))
      if (strstart(&ptr, "\"on\"") || strstart(&ptr, "\"yes\""))
        TT.proc_accounting = 1;
    free(line);
  }
  fclose(fp);

  return 1;
}

static char *create_tmp_dir()
{
  char *dir_list[] = {"/tmp", "/mnt", "/boot", "/proc"}, **target = dir_list;
  char *dir, dir_path[] = "/tmp/bootchart.XXXXXX";

  if ((dir = mkdtemp(dir_path))) {
    xchdir((dir = xstrdup(dir)));

    return dir;
  }
  while (mount("none", *target, "tmpfs", (1<<15), "size=16m")) //MS_SILENT
    if (!++target) perror_exit("can't mount tmpfs");
  xchdir(*target);
  if (umount2(*target, MNT_DETACH)) perror_exit("Can't unmount tmpfs");

  return *target;
}

static void start_logging()
{
  struct timespec ts;
  int sfd = xcreate("proc_stat.log",  O_WRONLY|O_CREAT|O_TRUNC, 0644),
      dfd = xcreate("proc_diskstats.log",  O_WRONLY|O_CREAT|O_TRUNC, 0644);
  FILE *proc_ps_fp = xfopen("proc_ps.log", "w");
  long tcnt = 60 * 1000 / TT.msec;

  if (tcnt <= 0) tcnt = 1;
  if (TT.proc_accounting) {
    xclose(xcreate("kernel_procs_acct", O_WRONLY|O_CREAT|O_TRUNC, 0666));
    acct("kernel_procs_acct");
  }
  while (--tcnt && !toys.signal) {
    clock_gettime(CLOCK_BOOTTIME, &ts);
    sprintf(TT.timestamp, "%ld.%02d\n", (long) ts.tv_sec,
        (int) (ts.tv_nsec/10000000));
    dump_data_in_file("/proc/stat", sfd);
    dump_data_in_file("/proc/diskstats", dfd);
    // stop proc dumping in 2 secs if getty or gdm, kdm, xdm found
    if (dump_proc_data(proc_ps_fp))
      if (tcnt > 2 * 1000 / TT.msec) tcnt = 2 * 1000 / TT.msec;
    fflush(0);
    msleep(TT.msec);
  }
  xclose(sfd);
  xclose(dfd);
  fclose(proc_ps_fp);
}

static void stop_logging(char *tmp_dir, char *prog)
{
  char host_name[32];
  int kcmd_line_fd;
  time_t t;
  struct tm st;
  struct utsname uts;
  FILE *hdr_fp = xfopen("header", "w");

  if (TT.proc_accounting) acct(NULL);
  if (prog) fprintf(hdr_fp, "profile.process = %s\n", prog);
  gethostname(host_name, sizeof(host_name));
  time(&t);
  localtime_r(&t, &st);
  memset(toybuf, 0, sizeof(toybuf));
  strftime(toybuf, sizeof(toybuf), "%a %b %e %H:%M:%S %Z %Y", &st);
  fprintf(hdr_fp, "version = TBX_BCHARTD_VER 1.0.0\n");
  fprintf(hdr_fp, "title = Boot chart for %s (%s)\n", host_name, toybuf);
  if (uname(&uts) < 0) perror_exit("uname");
  fprintf(hdr_fp, "system.uname = %s %s %s %s\n", uts.sysname, uts.release,
      uts.version, uts.machine);
  memset(toybuf, 0, sizeof(toybuf));
  if ((kcmd_line_fd = open("/proc/cmdline", O_RDONLY)) != -1) {
    ssize_t len;

    if ((len = readall(kcmd_line_fd, toybuf, sizeof(toybuf)-1)) > 0) {
      toybuf[len] = 0;
      while (--len >= 0 && !toybuf[len]) continue;
      for (; len > 0; len--) if (toybuf[len] < ' ') toybuf[len] = ' ';
    } else *toybuf = 0;
  }
  fprintf(hdr_fp, "system.kernel.options = %s", toybuf);
  close(kcmd_line_fd);
  fclose(hdr_fp);
  memset(toybuf, 0, sizeof(toybuf));
  snprintf(toybuf, sizeof(toybuf), "tar -zcf /var/log/bootlog.tgz header %s *.log",
      TT.proc_accounting ? "kernel_procs_acct" : "");
  system(toybuf);

  // We created a tmpdir then lazy unmounted it, why are we deleting files...?
  if (tmp_dir) {
    unlink("header");
    unlink("proc_stat.log");
    unlink("proc_diskstats.log");
    unlink("proc_ps.log");
    if (TT.proc_accounting) unlink("kernel_procs_acct");
    rmdir(tmp_dir);
  }
}

static int signal_pid(pid_t pid, char *name)
{
  if (pid != TT.pid) kill(pid, SIGUSR1);
  return 0;
}

void bootchartd_main()
{
  pid_t lgr_pid;
  int bchartd_opt = 0; // 0=PID1, 1=start, 2=stop, 3=init

  TT.pid = getpid();
  TT.msec = 200;

  if (*toys.optargs) {
    if (!strcmp("start", *toys.optargs)) bchartd_opt = 1;
    else if (!strcmp("stop", *toys.optargs)) bchartd_opt = 2;
    else if (!strcmp("init", *toys.optargs)) bchartd_opt = 3;
    else error_exit("Unknown option '%s'", *toys.optargs);

    if (bchartd_opt == 2) {
      char *process_name[] = {"bootchartd", NULL};

      names_to_pid(process_name, signal_pid, 0);
      return;
    }
  } else if (TT.pid != 1) error_exit("not PID 1");

  // Execute the code below for start or init or PID1
  if (!parse_config_file("bootchartd.conf"))
    parse_config_file("/etc/bootchartd.conf");

  memset(toybuf, 0, sizeof(toybuf));
  if (!(lgr_pid = xfork())) {
    char *tmp_dir = create_tmp_dir();

    sigatexit(generic_signal);
    raise(SIGSTOP);
    if (!bchartd_opt && !getenv("PATH"))
      putenv("PATH=/sbin:/usr/sbin:/bin:/usr/bin");
    start_logging();
    stop_logging(tmp_dir, bchartd_opt == 1 ? toys.optargs[1] : NULL);
    return;
  }
  waitpid(lgr_pid, NULL, WUNTRACED);
  kill(lgr_pid, SIGCONT);

  if (!bchartd_opt) {
    char *pbchart_init = getenv("bootchart_init");

    if (pbchart_init) execl(pbchart_init, pbchart_init, NULL);
    execl("/init", "init", (void *)0);
    execl("/sbin/init", "init", (void *)0);
  }
  if (bchartd_opt == 1 && toys.optargs[1]) {
    pid_t prog_pid;

    if (!(prog_pid = xfork())) xexec(toys.optargs+1);
    waitpid(prog_pid, NULL, 0);
    kill(lgr_pid, SIGUSR1);
  }
}
