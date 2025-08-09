/* fsck.c -  check and repair a Linux filesystem
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>

USE_FSCK(NEWTOY(fsck, "?t:ANPRTVsC#", TOYFLAG_USR|TOYFLAG_BIN))

config FSCK
  bool "fsck"
  default n
  help
    usage: fsck [-ANPRTV] [-C FD] [-t FSTYPE] [FS_OPTS] [BLOCKDEV]...

    Check and repair filesystems

    -A      Walk /etc/fstab and check all filesystems
    -N      Don't execute, just show what would be done
    -P      With -A, check filesystems in parallel
    -R      With -A, skip the root filesystem
    -T      Don't show title on startup
    -V      Verbose
    -C n    Write status information to specified file descriptor
    -t TYPE List of filesystem types to check

*/

#define FOR_fsck
#include "toys.h"
#include <mntent.h>

#define FLAG_WITHOUT_NO_PRFX 1
#define FLAG_WITH_NO_PRFX 2
#define FLAG_DONE 1

GLOBALS(
  int fd_num;
  char *t_list;

  struct double_list *devices;
  char *arr_flag;
  char **arr_type;
  int negate;
  int sum_status;
  int nr_run;
  int sig_num;
  long max_nr_run;
)

struct f_sys_info {
  char *device, *mountpt, *type, *opts;
  int passno, flag;
  struct f_sys_info *next;
};

struct child_list {
  struct child_list *next;
  pid_t pid;
  char *prog_name, *dev_name;
};

static struct f_sys_info *filesys_info = NULL; //fstab entry list
static struct child_list *c_list = NULL; //fsck.type child list.

static void kill_all(void)
{
  struct child_list *child;

  for (child = c_list; child; child = child->next)
    kill(child->pid, SIGTERM);
  _exit(0);
}

static long strtol_range(char *str, int min, int max)
{
  char *endptr = NULL;
  errno = 0;
  long ret_value = strtol(str, &endptr, 10);

  if(errno) perror_exit("Invalid num %s", str);
  else if(endptr && (*endptr != '\0' || endptr == str))
    perror_exit("Not a valid num %s", str);
  if(ret_value >= min && ret_value <= max) return ret_value;
  else perror_exit("Number %s is not in valid [%d-%d] Range", str, min, max);
}

//create fstab entries list.
static struct f_sys_info* create_db(struct mntent *f_info)
{
  struct f_sys_info *temp = filesys_info;
  if (temp) {
    while (temp->next) temp = temp->next;
    temp->next = xzalloc(sizeof(struct f_sys_info));
    temp = temp->next;
  } else filesys_info = temp = xzalloc(sizeof(struct f_sys_info));

  temp->device = xstrdup(f_info->mnt_fsname);
  temp->mountpt = xstrdup(f_info->mnt_dir);
  if (strchr(f_info->mnt_type, ',')) temp->type = xstrdup("auto");
  else  temp->type = xstrdup(f_info->mnt_type);
  temp->opts = xstrdup(f_info->mnt_opts);
  temp->passno = f_info->mnt_passno;
  return temp;
}

//is we have 'no' or ! before type.
static int is_no_prefix(char **p)
{
  int no = 0;

  if ((*p[0] == 'n' && *(*p + 1) == 'o')) no = 2;
  else if (*p[0] == '!') no = 1;
  *p += no;
  return ((no) ? 1 :0);
}

static void fix_tlist(void)
{
  char *p, *s = TT.t_list;
  int n = 1, no;

  while ((s = strchr(s, ','))) {
    s++;
    n++;
  }

  TT.arr_flag = xzalloc(n + 1);
  TT.arr_type = xzalloc((n + 1) * sizeof(char *));
  s = TT.t_list;
  n = 0;
  while ((p = strsep(&s, ","))) {
    no = is_no_prefix(&p);
    if (!strcmp(p, "loop")) {
      TT.arr_flag[n] = no ? FLAG_WITH_NO_PRFX :FLAG_WITHOUT_NO_PRFX;
      TT.negate = no;
    } else if (!strncmp(p, "opts=", 5)) {
      p+=5;
      TT.arr_flag[n] = is_no_prefix(&p) ?FLAG_WITH_NO_PRFX :FLAG_WITHOUT_NO_PRFX;
      TT.negate = no;
    } else {
      if (!n) TT.negate = no;
      if (n && TT.negate != no) error_exit("either all or none of the filesystem"
          " types passed to -t must be prefixed with 'no' or '!'");
    }
    TT.arr_type[n++] = p;
  }
}

//ignore these types...
static int ignore_type(char *type)
{
  int i = 0;
  char *str;
  char *ignored_types[] = {
    "ignore","iso9660", "nfs","proc",
    "sw","swap", "tmpfs","devpts",NULL
  };
  while ((str = ignored_types[i++])) {
    if (!strcmp(str, type)) return 1;
  }
  return 0;
}

// return true if has to ignore the filesystem.
static int to_be_ignored(struct f_sys_info *finfo)
{
  int i, ret = 0, type_present = 0;

  if (!finfo->passno) return 1; //Ignore with pass num = 0
  if (TT.arr_type) {
    for (i = 0; TT.arr_type[i]; i++) {
      if (!TT.arr_flag[i]) { //it is type of filesys.
        type_present = 2;
        if (!strcmp(TT.arr_type[i], finfo->type)) ret = 0;
        else ret = 1;
      } else if (TT.arr_flag[i] == FLAG_WITH_NO_PRFX) { //it is option of filesys
        if (hasmntopt((const struct mntent *)finfo, TT.arr_type[i])) return 1;
      } else { //FLAG_WITHOUT_NO_PRFX
        if (!hasmntopt((const struct mntent *)finfo, TT.arr_type[i])) return 1;
      }
    }
  }
  if (ignore_type(finfo->type)) return 1;
  if (TT.arr_type && type_present != 2) return 0;
  return ((TT.negate) ? !ret : ret);
}

// find type and execute corresponding fsck.type prog.
static void do_fsck(struct f_sys_info *finfo)
{
  struct child_list *child;
  char **args;
  char *type;
  pid_t pid;
  int i = 1, j = 0;

  if (strcmp(finfo->type, "auto")) type = finfo->type;
  else if (TT.t_list && (TT.t_list[0] != 'n' || TT.t_list[1] != 'o' || TT.t_list[0] != '!')
      && strncmp(TT.t_list, "opts=", 5) && strncmp(TT.t_list , "loop", 4)
      && !TT.arr_type[1]) type = TT.t_list; //one file sys at cmdline
  else type = "auto";

  args = xzalloc((toys.optc + 2 + 1 + 1) * sizeof(char*)); //+1, for NULL, +1 if -C
  args[0] = xmprintf("fsck.%s", type);

  if(toys.optflags & FLAG_C) args[i++] = xmprintf("%s %d","-C", TT.fd_num);
  while(toys.optargs[j]) {
    if(*toys.optargs[j]) args[i++] = xstrdup(toys.optargs[j]);
    j++;
  }
  args[i] = finfo->device;

  TT.nr_run++;
  if ((toys.optflags & FLAG_V) || (toys.optflags & FLAG_N)) {
    printf("[%s (%d) -- %s]", args[0], TT.nr_run,
        finfo->mountpt ? finfo->mountpt : finfo->device);
    for (i = 0; args[i]; i++) xprintf(" %s", args[i]);
    xputc('\n');
  }

  if (toys.optflags & FLAG_N) {
    for (j=0;j<i;j++) free(args[i]);
    free(args);
    return;
  } else {
    if ((pid = fork()) < 0) {
      perror_msg_raw(*args);
      for (j=0;j<i;j++) free(args[i]);
      free(args);
      return;
    }
    if (!pid) xexec(args); //child, executes fsck.type
  }

  child = xzalloc(sizeof(struct child_list)); //Parent, add to child list.
  child->dev_name = xstrdup(finfo->device);
  child->prog_name = args[0];
  child->pid = pid;

  if (c_list) {
    child->next = c_list;
    c_list = child;
  } else {
    c_list = child;
    child->next =NULL;
  }
}

// for_all = 1; wait for all child to exit
// for_all = 0; wait for any one to exit
static int wait_for(int for_all)
{
  pid_t pid;
  int status = 0, child_exited;
  struct child_list *prev, *temp;

  errno = 0;
  if (!c_list) return 0;
  while ((pid = wait(&status))) {
    temp = c_list;
    prev = temp;
    if (TT.sig_num) kill_all();
    child_exited = 0;
    if (pid < 0) {
      if (errno == EINTR) continue;
      else if (errno == ECHILD) break; //No child to wait, break and return status.
      else perror_exit("option arg Invalid\n"); //paranoid.
    }
    while (temp) {
      if (temp->pid == pid) {
        child_exited = 1;
        break;
      }
      prev = temp;
      temp = temp->next;
    }
    if (child_exited) {
      if (WIFEXITED(status)) TT.sum_status |= WEXITSTATUS(status);
      else if (WIFSIGNALED(status)) {
        TT.sum_status |= 4; //Uncorrected.
        if (WTERMSIG(status) != SIGINT)
          perror_msg("child Term. by sig: %d\n",(WTERMSIG(status)));
        TT.sum_status |= 8; //Operatinal error
      } else {
        TT.sum_status |= 4; //Uncorrected.
        perror_msg("%s %s: status is %x, should never happen\n",
            temp->prog_name, temp->dev_name, status);
      }
      TT.nr_run--;
      if (prev == temp) c_list = c_list->next; //first node
      else prev->next = temp->next;
      free(temp->prog_name);
      free(temp->dev_name);
      free(temp);
      if (!for_all) break;
    }
  }
  return TT.sum_status;
}

//scan all the fstab entries or -t matches with fstab.
static int scan_all(void)
{
  struct f_sys_info *finfo = filesys_info;
  int ret = 0, passno;

  if (toys.optflags & FLAG_V) xprintf("Checking all filesystem\n");
  while (finfo) {
    if (to_be_ignored(finfo)) finfo->flag |= FLAG_DONE;
    finfo = finfo->next;
  }
  finfo = filesys_info;

  if (!(toys.optflags & FLAG_P)) {
    while (finfo) {
      if (!strcmp(finfo->mountpt, "/")) { // man says: check / in parallel with others if -P is absent.
        if ((toys.optflags & FLAG_R) || to_be_ignored(finfo)) {
          finfo->flag |= FLAG_DONE;
          break;
        } else {
          do_fsck(finfo);
          finfo->flag |= FLAG_DONE;
          if (TT.sig_num) kill_all();
          if ((ret |= wait_for(1)) > 4) return ret; //destruction in filesys.
          break;
        }
      }
      finfo = finfo->next;
    }
  }
  if (toys.optflags & FLAG_R) { // with -PR we choose to skip root.
    for (finfo = filesys_info; finfo; finfo = finfo->next) {
      if(!strcmp(finfo->mountpt, "/")) finfo->flag |= FLAG_DONE;
    }
  }
  passno = 1;
  while (1) {
    for (finfo = filesys_info; finfo; finfo = finfo->next)
      if (!finfo->flag) break;
    if (!finfo) break;

    for (finfo = filesys_info; finfo; finfo = finfo->next) {
      if (finfo->flag) continue;
      if (finfo->passno == passno) {
        do_fsck(finfo);
        finfo->flag |= FLAG_DONE;
        if ((toys.optflags & FLAG_s) || (TT.nr_run
              && (TT.nr_run >= TT.max_nr_run))) ret |= wait_for(0);
      }
    }
    if (TT.sig_num) kill_all();
    ret |= wait_for(1);
    passno++;
  }
  return ret;
}

void record_sig_num(int sig)
{
  TT.sig_num = sig;
}

void fsck_main(void)
{
  struct mntent mt;
  struct double_list *dev;
  struct f_sys_info *finfo;
  FILE *fp;
  char *tmp, **arg = toys.optargs;

  sigatexit(record_sig_num);
  while (*arg) {
    if ((**arg == '/') || strchr(*arg, '=')) {
      dlist_add(&TT.devices, xstrdup(*arg));
      **arg = '\0';
    }
    arg++;
  }
  if (toys.optflags & FLAG_t) fix_tlist();
  if (!(tmp = getenv("FSTAB_FILE"))) tmp = "/etc/fstab";
  if (!(fp = setmntent(tmp, "r"))) perror_exit("setmntent failed:");
  while (getmntent_r(fp, &mt, toybuf, 4096)) create_db(&mt);
  endmntent(fp);

  if (!(toys.optflags & FLAG_T)) xprintf("fsck ----- (Toybox)\n");

  if ((tmp = getenv("FSCK_MAX_INST")))
    TT.max_nr_run = strtol_range(tmp, 0, INT_MAX);
  if (!TT.devices || (toys.optflags & FLAG_A)) {
    toys.exitval = scan_all();
    if (CFG_TOYBOX_FREE) goto free_all;
    return; //if CFG_TOYBOX_FREE is not set, exit.
  }

  dev = TT.devices;
  dev->prev->next = NULL; //break double list to traverse.
  for (; dev; dev = dev->next) {
    for (finfo = filesys_info; finfo; finfo = finfo->next)
      if (!strcmp(finfo->device, dev->data)
          || !strcmp(finfo->mountpt, dev->data)) break;
    if (!finfo) { //if not present, fill def values.
      mt.mnt_fsname = dev->data;
      mt.mnt_dir = "";
      mt.mnt_type = "auto";
      mt.mnt_opts = "";
      mt.mnt_passno = -1;
      finfo = create_db(&mt);
    }
    do_fsck(finfo);
    finfo->flag |= FLAG_DONE;
    if ((toys.optflags & FLAG_s) || (TT.nr_run && (TT.nr_run >= TT.max_nr_run)))
      toys.exitval |= wait_for(0);
  }
  if (TT.sig_num) kill_all();
  toys.exitval |= wait_for(1);
  finfo = filesys_info;

free_all:
  if (CFG_TOYBOX_FREE) {
    struct f_sys_info *finfo, *temp;

    llist_traverse(TT.devices, llist_free_double);
    free(TT.arr_type);
    free(TT.arr_flag);
    for (finfo = filesys_info; finfo;) {
      temp = finfo->next;
      free(finfo->device);
      free(finfo->mountpt);
      free(finfo->type);
      free(finfo->opts);
      free(finfo);
      finfo = temp;
    }
  }
}
