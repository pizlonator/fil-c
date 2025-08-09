/* lsof.c - list open files.
 *
 * Copyright 2015 The Android Open Source Project

USE_LSOF(NEWTOY(lsof, "lp*t", TOYFLAG_USR|TOYFLAG_BIN))

config LSOF
  bool "lsof"
  default n
  help
    usage: lsof [-lt] [-p PID1,PID2,...] [FILE...]

    List all open files belonging to all active processes, or processes using
    listed FILE(s).

    -l	list uids numerically
    -p	for given comma-separated pids only (default all pids)
    -t	terse (pid only) output
*/

#define FOR_lsof
#include "toys.h"

GLOBALS(
  struct arg_list *p;

  struct stat *sought_files;
  struct double_list *all_sockets, *files;
  int last_shown_pid, shown_header;
)

struct proc_info {
  char cmd[17];
  int pid, uid;
};

struct file_info {
  char *next, *prev;

  // For output.
  struct proc_info pi;
  char *name, fd[8], rw, locks, type[10], device[32], size_off[32], node[32];

  // For filtering.
  struct dev_ino di;
};

static void print_info(void *data)
{
  struct file_info *fi = data;

  // Filter matches
  if (toys.optc) {
    int i;

    for (i = 0; i<toys.optc; i++)
      if (same_dev_ino(TT.sought_files+i, &fi->di)) break;
    if (i==toys.optc) return;
  }

  if (FLAG(t)) {
    if (fi->pi.pid != TT.last_shown_pid)
      printf("%d\n", TT.last_shown_pid = fi->pi.pid);
  } else {
    if (!TT.shown_header) {
      // TODO: llist_traverse to measure the columns first.
      printf("%-9s %5s %10.10s %4s   %7s %18s %9s %10s %s\n", "COMMAND", "PID",
        "USER", "FD", "TYPE", "DEVICE", "SIZE/OFF", "NODE", "NAME");
      TT.shown_header = 1;
    }

    printf("%-9s %5d %10.10s %4s%c%c %7s %18s %9s %10s %s\n",
           fi->pi.cmd, fi->pi.pid, getusername(fi->pi.uid),
           fi->fd, fi->rw, fi->locks, fi->type, fi->device, fi->size_off,
           fi->node, fi->name);
  }
}

static void free_info(void *data)
{
  free(((struct file_info *)data)->name);
  free(data);
}

static void fill_flags(struct file_info *fi)
{
  FILE* fp;
  long long pos;
  unsigned flags;

  snprintf(toybuf, sizeof(toybuf), "/proc/%d/fdinfo/%s", fi->pi.pid, fi->fd);
  if (!(fp = fopen(toybuf, "r"))) return;

  if (fscanf(fp, "pos: %lld flags: %o", &pos, &flags) == 2) {
    flags &= O_ACCMODE;
    if (flags == O_RDONLY) fi->rw = 'r';
    else if (flags == O_WRONLY) fi->rw = 'w';
    else fi->rw = 'u';

    snprintf(fi->size_off, sizeof(fi->size_off), "0t%lld", pos);
  }
  fclose(fp);
}

static void scan_proc_net_file(char *path, int family, char type,
    void (*fn)(char *, int, char))
{
  FILE *fp = fopen(path, "r");
  char *line = NULL;
  size_t line_length = 0;

  if (!fp) return;

  if (getline(&line, &line_length, fp) <= 0) return; // Skip header.

  while (getline(&line, &line_length, fp) > 0) {
    fn(line, family, type);
  }

  free(line);
  fclose(fp);
}

static struct file_info *add_socket(ino_t inode, const char *type)
{
  struct file_info *fi = xzalloc(sizeof(struct file_info));

  dlist_add_nomalloc(&TT.all_sockets, (struct double_list *)fi);
  fi->di.ino = inode;
  strcpy(fi->type, type);
  return fi;
}

static void scan_unix(char *line, int af, char type)
{
  long inode;
  int path_pos;

  if (sscanf(line, "%*p: %*X %*X %*X %*X %*X %lu %n", &inode, &path_pos) >= 1) {
    struct file_info *fi = add_socket(inode, "unix");
    char *name = chomp(line + path_pos);

    fi->name = strdup(*name ? name : "socket");
  }
}

static void scan_netlink(char *line, int af, char type)
{
  unsigned state;
  long inode;
  char *netlink_states[] = {
    "ROUTE", "UNUSED", "USERSOCK", "FIREWALL", "SOCK_DIAG", "NFLOG", "XFRM",
    "SELINUX", "ISCSI", "AUDIT", "FIB_LOOKUP", "CONNECTOR", "NETFILTER",
    "IP6_FW", "DNRTMSG", "KOBJECT_UEVENT", "GENERIC", "DM", "SCSITRANSPORT",
    "ENCRYPTFS", "RDMA", "CRYPTO"
  };

  if (sscanf(line, "%*p %u %*u %*x %*u %*u %*u %*u %*u %lu", &state, &inode)<2)
    return;

  struct file_info *fi = add_socket(inode, "netlink");
  fi->name =
      strdup(state < ARRAY_LEN(netlink_states) ? netlink_states[state] : "?");
}

static void scan_ip(char *line, int af, char type)
{
  char *tcp_states[] = {
    "UNKNOWN", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT1", "FIN_WAIT2",
    "TIME_WAIT", "CLOSE", "CLOSE_WAIT", "LAST_ACK", "LISTEN", "CLOSING"
  };
  char local_ip[INET6_ADDRSTRLEN] = {0};
  char remote_ip[INET6_ADDRSTRLEN] = {0};
  struct in6_addr local, remote;
  int local_port, remote_port, state;
  long inode;
  int ok;

  if (af == 4) {
    ok = sscanf(line, " %*d: %x:%x %x:%x %x %*x:%*x %*X:%*X %*X %*d %*d %ld",
                &(local.s6_addr32[0]), &local_port,
                &(remote.s6_addr32[0]), &remote_port,
                &state, &inode) == 6;
  } else {
    ok = sscanf(line, " %*d: %8x%8x%8x%8x:%x %8x%8x%8x%8x:%x %x "
                "%*x:%*x %*X:%*X %*X %*d %*d %ld",
                &(local.s6_addr32[0]), &(local.s6_addr32[1]),
                &(local.s6_addr32[2]), &(local.s6_addr32[3]),
                &local_port,
                &(remote.s6_addr32[0]), &(remote.s6_addr32[1]),
                &(remote.s6_addr32[2]), &(remote.s6_addr32[3]),
                &remote_port, &state, &inode) == 12;
  }
  if (!ok) return;

  struct file_info *fi = add_socket(inode, af == 4 ? "IPv4" : "IPv6");
  inet_ntop(af, &local, local_ip, sizeof(local_ip));
  inet_ntop(af, &remote, remote_ip, sizeof(remote_ip));
  if (type == 't') {
    if (state < 0 || state > TCP_CLOSING) state = 0;
    fi->name = xmprintf(af == 4 ?
                        "TCP %s:%d->%s:%d (%s)" :
                        "TCP [%s]:%d->[%s]:%d (%s)",
                        local_ip, local_port, remote_ip, remote_port,
                        tcp_states[state]);
  } else {
    fi->name = xmprintf(af == 4 ? "%s %s:%d->%s:%d" : "%s [%s]:%d->[%s]:%d",
                        type == 'u' ? "UDP" : "RAW",
                        local_ip, local_port, remote_ip, remote_port);
  }
}

static int find_socket(struct file_info *fi, long inode)
{
  static int cached;
  if (!cached) {
    scan_proc_net_file("/proc/net/tcp", 4, 't', scan_ip);
    scan_proc_net_file("/proc/net/tcp6", 6, 't', scan_ip);
    scan_proc_net_file("/proc/net/udp", 4, 'u', scan_ip);
    scan_proc_net_file("/proc/net/udp6", 6, 'u', scan_ip);
    scan_proc_net_file("/proc/net/raw", 4, 'r', scan_ip);
    scan_proc_net_file("/proc/net/raw6", 6, 'r', scan_ip);
    scan_proc_net_file("/proc/net/unix", 0, 0, scan_unix);
    scan_proc_net_file("/proc/net/netlink", 0, 0, scan_netlink);
    cached = 1;
  }
  void* list = TT.all_sockets;

  while (list) {
    struct file_info *s = (struct file_info *)llist_pop(&list);

    if (s->di.ino == inode) {
      fi->name = s->name ? strdup(s->name) : NULL;
      strcpy(fi->type, s->type);
      return 1;
    }
    if (list == TT.all_sockets) break;
  }

  return 0;
}

static void fill_stat(struct file_info *fi, const char *path)
{
  struct stat sb;
  long dev;

  if (stat(path, &sb)) return;

  // Fill TYPE.
  switch ((sb.st_mode & S_IFMT)) {
    case S_IFBLK: strcpy(fi->type, "BLK"); break;
    case S_IFCHR: strcpy(fi->type, "CHR"); break;
    case S_IFDIR: strcpy(fi->type, "DIR"); break;
    case S_IFIFO: strcpy(fi->type, "FIFO"); break;
    case S_IFLNK: strcpy(fi->type, "LINK"); break;
    case S_IFREG: strcpy(fi->type, "REG"); break;
    case S_IFSOCK: strcpy(fi->type, "sock"); break;
    default:
      snprintf(fi->type, sizeof(fi->type), "%04o", sb.st_mode & S_IFMT);
      break;
  }

  if (S_ISSOCK(sb.st_mode)) find_socket(fi, sb.st_ino);

  // Fill DEVICE.
  dev = (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) ? sb.st_rdev : sb.st_dev;
  if (!S_ISSOCK(sb.st_mode))
    snprintf(fi->device, sizeof(fi->device), "%d,%d",
             dev_major(dev), dev_minor(dev));

  // Fill SIZE/OFF.
  if (S_ISREG(sb.st_mode) || S_ISDIR(sb.st_mode))
    snprintf(fi->size_off, sizeof(fi->size_off), "%lld",
             (long long)sb.st_size);

  // Fill NODE.
  snprintf(fi->node, sizeof(fi->node), "%ld", (long)sb.st_ino);

  // Stash st_dev and st_ino for filtering.
  fi->di.dev = sb.st_dev;
  fi->di.ino = sb.st_ino;
}

struct file_info *new_file_info(struct proc_info *pi, const char *fd)
{
  struct file_info *fi = xzalloc(sizeof(struct file_info));

  dlist_add_nomalloc(&TT.files, (struct double_list *)fi);

  fi->pi = *pi;

  // Defaults.
  strcpy(fi->fd, fd);
  strcpy(fi->type, "unknown");
  fi->rw = fi->locks = ' ';

  return fi;
}

static void visit_symlink(struct proc_info *pi, char *name, char *path)
{
  struct file_info *fi = new_file_info(pi, "");

  // Get NAME.
  if (name) { // "/proc/pid/[cwd]".
    snprintf(fi->fd, sizeof(fi->fd), "%s", name);
    snprintf(toybuf, sizeof(toybuf), "/proc/%d/%s", pi->pid, path);
  } else { // "/proc/pid/fd/[3]"
    snprintf(fi->fd, sizeof(fi->fd), "%s", path);
    fill_flags(fi); // Clobbers toybuf.
    snprintf(toybuf, sizeof(toybuf), "/proc/%d/fd/%s", pi->pid, path);
  }
  // TODO: code called by fill_stat would be easier to write if we didn't
  // rely on toybuf being preserved here.
  fill_stat(fi, toybuf);
  if (!fi->name) { // We already have a name for things like sockets.
    fi->name = xreadlink(toybuf);
    if (!fi->name) {
      fi->name = xmprintf("%s (readlink: %s)", toybuf, strerror(errno));
    }
  }
}

static void visit_maps(struct proc_info *pi)
{
  FILE *fp;
  unsigned long long offset;
  long inode;
  char *line = NULL, device[10]; // xxx:xxxxx\0
  size_t line_length = 0;
  struct file_info *fi;

  snprintf(toybuf, sizeof(toybuf), "/proc/%d/maps", pi->pid);
  fp = fopen(toybuf, "r");
  if (!fp) return;

  while (getline(&line, &line_length, fp) > 0) {
    int name_pos;

    if (sscanf(line, "%*x-%*x %*s %llx %9s %ld %n",
               &offset, device, &inode, &name_pos) >= 3) {
      // Ignore non-file maps.
      if (inode == 0 || !strcmp(device, "00:00")) continue;
      // TODO: show unique maps even if they have a non-zero offset?
      if (offset != 0) continue;

      fi = new_file_info(pi, "mem");
      fi->name = strdup(chomp(line + name_pos));
      fill_stat(fi, fi->name);
    }
  }
  free(line);
  fclose(fp);
}

static void visit_fds(struct proc_info *pi)
{
  DIR *dir;
  struct dirent *de;

  snprintf(toybuf, sizeof(toybuf), "/proc/%d/fd", pi->pid);
  if (!(dir = opendir(toybuf))) {
    struct file_info *fi = new_file_info(pi, "NOFD");

    fi->name = xmprintf("%s (opendir: %s)", toybuf, strerror(errno));
    return;
  }

  while ((de = readdir(dir))) {
    if (*de->d_name == '.') continue;
    visit_symlink(pi, NULL, de->d_name);
  }

  closedir(dir);
}

static void lsof_pid(int pid, struct stat *st)
{
  struct proc_info pi;
  struct stat sb;
  char *s;

  pi.pid = pid;

  // Skip nonexistent pids
  sprintf(toybuf, "/proc/%d/stat", pid);
  if (!readfile(toybuf, toybuf, sizeof(toybuf)-1) || !(s = strchr(toybuf, '(')))
    return;
  memcpy(pi.cmd, s+1, sizeof(pi.cmd)-1);
  pi.cmd[sizeof(pi.cmd)-1] = 0;
  if ((s = strrchr(pi.cmd, ')'))) *s = 0;

  // Get USER.
  if (!st) {
    snprintf(toybuf, sizeof(toybuf), "/proc/%d", pid);
    if (stat(toybuf, st = &sb)) return;
  }
  pi.uid = st->st_uid;

  visit_symlink(&pi, "cwd", "cwd");
  visit_symlink(&pi, "rtd", "root");
  visit_symlink(&pi, "txt", "exe");
  visit_maps(&pi);
  visit_fds(&pi);
}

static int scan_proc(struct dirtree *node)
{
  int pid;

  if (!node->parent) return DIRTREE_RECURSE|DIRTREE_SHUTUP;
  if ((pid = atol(node->name))) lsof_pid(pid, &node->st);

  return 0;
}

void lsof_main(void)
{
  struct arg_list *pp;
  int i, pid;

  // lsof will only filter on paths it can stat (because it filters by inode).
  if (toys.optc) {
    TT.sought_files = xmalloc(toys.optc*sizeof(struct stat));
    for (i = 0; i<toys.optc; ++i) xstat(toys.optargs[i], TT.sought_files+i);
  }

  if (!TT.p) dirtree_read("/proc", scan_proc);
  else for (pp = TT.p; pp; pp = pp->next) {
    char *start, *end, *next = pp->arg;

    while ((start = comma_iterate(&next, &i))) {
      if ((pid = strtol(start, &end, 10))<1 || (*end && *end!=','))
        error_msg("bad -p '%.*s'", (int)(end-start), start);
      lsof_pid(pid, 0);
    }
  }

  llist_traverse(TT.files, print_info);

  if (CFG_TOYBOX_FREE) {
    llist_traverse(TT.files, free_info);
    llist_traverse(TT.all_sockets, free_info);
  }
}
