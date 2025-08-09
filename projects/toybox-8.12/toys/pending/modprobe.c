/* modprobe.c - modprobe utility.
 *
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_MODPROBE(NEWTOY(modprobe, "alrqvsDbd*", TOYFLAG_SBIN))

config MODPROBE
  bool "modprobe"
  default n
  help
    usage: modprobe [-alrqvsDb] [-d DIR] MODULE [symbol=value][...]

    modprobe utility - inserts modules and dependencies.

    -a  Load multiple MODULEs
    -b  Apply blacklist to module names too
    -D  Show dependencies
    -d  Load modules from DIR, option may be used multiple times
    -l  List (MODULE is a pattern)
    -q  Quiet
    -r  Remove MODULE (stacks) or do autoclean
    -s  Log to syslog
    -v  Verbose
*/
#define FOR_modprobe
#include "toys.h"

GLOBALS(
  struct arg_list *dirs;

  struct arg_list *probes, *dbase[256];
  char *cmdopts;
  int nudeps, symreq;
)

#define MODNAME_LEN 256

// Modules flag definations
#define MOD_ALOADED   0x0001
#define MOD_BLACKLIST 0x0002
#define MOD_FNDDEPMOD 0x0004
#define MOD_NDDEPS    0x0008

// Current probing modules info
struct module_s {
  uint32_t flags;
  char *cmdname, *name, *depent, *opts;
  struct arg_list *rnames, *dep;
};

// Converts path name FILE to module name.
static char *path2mod(char *file, char *mod)
{
  int i;
  char *from;

  if (!file) return NULL;
  if (!mod) mod = xmalloc(MODNAME_LEN);

  from = getbasename(file);

  for (i = 0; i < (MODNAME_LEN-1) && from[i] && from[i] != '.'; i++)
    mod[i] = (from[i] == '-') ? '_' : from[i];
  mod[i] = '\0';
  return mod;
}

// Add options in opts from toadd.
static char *add_opts(char *opts, char *toadd)
{
  if (toadd) {
    int optlen = 0;

    if (opts) optlen = strlen(opts);
    opts = xrealloc(opts, optlen + strlen(toadd) + 2);
    sprintf(opts + optlen, " %s", toadd);
  }
  return opts;
}

// Remove first element from the list and return it.
static void *llist_popme(struct arg_list **head)
{
  char *data = NULL;
  struct arg_list *temp = *head;

  if (temp) {
    data = temp->arg;
    *head = temp->next;
    free(temp);
  }
  return data;
}

// Add new node at the beginning of the list.
static void llist_add(struct arg_list **old, void *data)
{
  struct arg_list *new = xmalloc(sizeof(struct arg_list));

  new->arg = (char*)data;
  new->next = *old;
  *old = new;
}

// Add new node at tail of list.
static void llist_add_tail(struct arg_list **head, void *data)
{
  while (*head) head = &(*head)->next;
  *head = xzalloc(sizeof(struct arg_list));
  (*head)->arg = (char*)data;
}

// Reverse list order.
static struct arg_list *llist_rev(struct arg_list *list)
{
  struct arg_list *rev = NULL;

  while (list) {
    struct arg_list *next = list->next;

    list->next = rev;
    rev = list;
    list = next;
  }
  return rev;
}

/*
 * Returns struct module_s from the data base if found, NULL otherwise.
 * if add - create module entry, add it to data base and return the same mod.
 */
static struct module_s *get_mod(char *mod, uint8_t add)
{
  char name[MODNAME_LEN];
  struct module_s *modentry;
  struct arg_list *temp;
  unsigned i, hash = 0;

  path2mod(mod, name);
  for (i = 0; name[i]; i++) hash = ((hash*31) + hash) + name[i];
  hash %= ARRAY_LEN(TT.dbase);
  for (temp = TT.dbase[hash]; temp; temp = temp->next) {
    modentry = (struct module_s *) temp->arg;
    if (!strcmp(modentry->name, name)) return modentry;
  }
  if (!add) return NULL;
  modentry = xzalloc(sizeof(*modentry));
  modentry->name = xstrdup(name);
  llist_add(&TT.dbase[hash], modentry);
  return modentry;
}

/*
 * Read a line from file with \ continuation and skip commented lines.
 * Return the line in allocated string (*li)
 */
static int read_line(FILE *fl, char **li)
{
  char *nxtline = NULL, *line;
  ssize_t len, nxtlen;
  size_t linelen, nxtlinelen;

  for (;;) {
    line = NULL;
    linelen = nxtlinelen = 0;
    len = getline(&line, &linelen, fl);
    if (len <= 0) {
      free(line);
      return len;
    }
    // checking for commented lines.
    if (line[0] != '#') break;
    free(line);
  }
  for (;;) {
    if (line[len - 1] == '\n') len--;
    if (!len) {
      free(line);
      return len;
    } else if (line[len - 1] != '\\') break;

    len--;
    nxtlen = getline(&nxtline, &nxtlinelen, fl);
    if (nxtlen <= 0) break;
    if (linelen < len + nxtlen + 1) {
      linelen = len + nxtlen + 1;
      line = xrealloc(line, linelen);
    }
    memcpy(&line[len], nxtline, nxtlen);
    len += nxtlen;
  }
  line[len] = '\0';
  *li = xstrdup(line);
  free(line);
  if (nxtline) free(nxtline);
  return len;
}

/*
 * Action to be taken on all config files in default directories
 * checks for aliases, options, install, remove and blacklist
 */
static int config_action(struct dirtree *node)
{
  FILE *fc;
  char *filename, *tokens[3], *line, *linecp;
  struct module_s *modent;
  int tcount = 0;

  if (!dirtree_notdotdot(node)) return 0;
  if (S_ISDIR(node->st.st_mode)) return DIRTREE_RECURSE;

  if (!S_ISREG(node->st.st_mode)) return 0; // process only regular file
  filename = dirtree_path(node, NULL);
  if (!(fc = fopen(filename, "r"))) {
    free(filename);
    return 0;
  }
  for (line = linecp = NULL; read_line(fc, &line) >= 0;
      free(line), free(linecp), line = linecp = NULL) {
    char *tk = NULL;

    if (!strlen(line)) continue;
    linecp = xstrdup(line);
    for (tk = strtok(linecp, "# \t"), tcount = 0; tk;
        tk = strtok(NULL, "# \t"), tcount++) {
      tokens[tcount] = tk;
      if (tcount == 2) {
        tokens[2] = line + strlen(tokens[0]) + strlen(tokens[1]) + 2;
        break;
      }
    }
    // Every command requires at least one argument.
    if (tcount < 2) continue;
    // process the tokens[0] contains first word of config line.
    if (!strcmp(tokens[0], "alias")) {
      struct arg_list *temp;
      char alias[MODNAME_LEN], *realname;

      if (!tokens[2]) continue;
      path2mod(tokens[1], alias);
      for (temp = TT.probes; temp; temp = temp->next) {
        modent = (struct module_s *) temp->arg;
        if (fnmatch(alias, modent->name, 0)) continue;
        realname = path2mod(tokens[2], NULL);
        llist_add(&modent->rnames, realname);
        if (modent->flags & MOD_NDDEPS) {
          modent->flags &= ~MOD_NDDEPS;
          TT.nudeps--;
        }
        modent = get_mod(realname, 1);
        if (!(modent->flags & MOD_NDDEPS)) {
          modent->flags |= MOD_NDDEPS;
          TT.nudeps++;
        }
      }
    } else if (!strcmp(tokens[0], "options")) {
      if (!tokens[2]) continue;
      modent = get_mod(tokens[1], 1);
      modent->opts = add_opts(modent->opts, tokens[2]);
    } else if (!strcmp(tokens[0], "include"))
      dirtree_read(tokens[1], config_action);
    else if (!strcmp(tokens[0], "blacklist"))
      get_mod(tokens[1], 1)->flags |= MOD_BLACKLIST;
    else if (!strcmp(tokens[0], "install")) continue;
    else if (!strcmp(tokens[0], "remove")) continue;
    else if (!FLAG(q))
      error_msg("Invalid option %s found in file %s", tokens[0], filename);
  }
  fclose(fc);
  free(filename);
  return 0;
}

// Show matched modules else return -1 on failure.
static int depmode_read_entry(char *cmdname)
{
  char *line, *name;
  int ret = -1;
  FILE *fe = xfopen("modules.dep", "r");

  while (read_line(fe, &line) >= 0) {
    char *tmp = strchr(line, ':');

    if (tmp) {
      *tmp = '\0';
      name = basename(line);
      tmp = strchr(name, '.');
      if (tmp) *tmp = '\0';
      if (!cmdname || !fnmatch(cmdname, name, 0)) {
        if (tmp) *tmp = '.';
        if (FLAG(v)) puts(line);
        ret = 0;
      }
    }
    free(line);
  }
  fclose(fe);
  return ret;
}

// Finds dependencies for modules from the modules.dep file.
static void find_dep(void)
{
  char *line = NULL;
  struct module_s *mod;
  FILE *fe = xfopen("modules.dep", "r");

  for (; read_line(fe, &line) >= 0; free(line)) {
    char *tmp = strchr(line, ':');

    if (tmp) {
      *tmp = '\0';
      mod = get_mod(line, 0);
      if (!mod) continue;
      if ((mod->flags & MOD_ALOADED) && !(FLAG(r)|FLAG(D))) continue;

      mod->flags |= MOD_FNDDEPMOD;
      if ((mod->flags & MOD_NDDEPS) && !mod->dep) {
        TT.nudeps--;
        llist_add(&mod->dep, xstrdup(line));
        tmp++;
        if (*tmp) {
          char *tok;

          while ((tok = strsep(&tmp, " \t"))) {
            if (!*tok) continue;
            llist_add_tail(&mod->dep, xstrdup(tok));
          }
        }
      }
    }
  }
  fclose(fe);
}

// Remove a module from the Linux Kernel. if !modules does auto remove.
static int rm_mod(char *modules)
{
  char *s;

  if (modules && (s = strend(modules, ".ko"))) *s = 0;
  return syscall(__NR_delete_module, modules, O_NONBLOCK);
}

// Insert module; simpler than insmod(1) because we already flattened the array
// of flags, and don't need to support loading from stdin.
static int ins_mod(char *modules, char *flags)
{
  int fd = xopenro(modules), rc = syscall(__NR_finit_module, fd, flags, 0);

  xclose(fd);
  return rc;
}

// Add module in probes list, if not loaded.
static void add_mod(char *name)
{
  struct module_s *mod = get_mod(name, 1);

  if (!(FLAG(r)|FLAG(D)) && (mod->flags & MOD_ALOADED)) {
    if (FLAG(v)) printf("%s already loaded\n", name);
    return;
  }
  if (FLAG(v)) printf("queuing %s\n", name);
  mod->cmdname = name;
  mod->flags |= MOD_NDDEPS;
  llist_add_tail(&TT.probes, mod);
  TT.nudeps++;
  if (!strncmp(mod->name, "symbol:", 7)) TT.symreq = 1;
}

// Parse cmdline options suplied for module.
static char *add_cmdopt(char **argv)
{
  char *opt = xzalloc(1);
  int lopt = 0;

  while (*++argv) {
    char *fmt, *var, *val;

    var = *argv;
    opt = xrealloc(opt, lopt + 2 + strlen(var) + 2);
    // check for key=val or key = val.
    fmt = "%.*s%s ";
    for (val = var; *val && *val != '='; val++);
    if (*val && strchr(++val, ' ')) fmt = "%.*s\"%s\" ";
    lopt += sprintf(opt + lopt, fmt, (int) (val - var), var, val);
  }
  return opt;
}

// Probes a single module and loads all its dependencies.
static void go_probe(struct module_s *m)
{
  int rc = 0, first = 1;

  if (!(m->flags & MOD_FNDDEPMOD)) {
    if (!FLAG(q)) error_msg("module %s not found in modules.dep", m->name);
    return;
  }
  if (FLAG(v)) printf("go_prob'ing %s\n", m->name);
  if (!FLAG(r)) m->dep = llist_rev(m->dep);

  while (m->dep) {
    struct module_s *m2;
    char *fn, *options;

    rc = 0;
    fn = llist_popme(&m->dep);
    m2 = get_mod(fn, 1);
    // are we removing ?
    if (FLAG(r)) {
      if (m2->flags & MOD_ALOADED) {
        if (rm_mod(m2->name)) {
          if (first) {
            perror_msg("can't unload module %s", m2->name);
            break;
          }
        } else m2->flags &= ~MOD_ALOADED;
      }
      first = 0;
      continue;
    }
// TODO how does free work here without leaking?
    options = m2->opts;
    m2->opts = NULL;
    if (m == m2) options = add_opts(options, TT.cmdopts);

    // are we only checking dependencies ?
    if (FLAG(D)) {
      if (FLAG(v))
        printf(options ? "insmod %s %s\n" : "insmod %s\n", fn, options);
      if (options) free(options);
      continue;
    }
    if (m2->flags & MOD_ALOADED) {
      if (FLAG(v)) printf("%s already loaded\n", fn);
      if (options) free(options);
      continue;
    }
    // none of above is true insert the module.
    errno = 0;
    rc = ins_mod(fn, options ? : "");
    if (FLAG(v))
      printf("loaded %s '%s': %s\n", fn, options, strerror(errno));
    if (errno == EEXIST) rc = 0;
    free(options);
    if (rc) {
      perror_msg("can't load module %s (%s)", m2->name, fn);
      break;
    }
    m2->flags |= MOD_ALOADED;
  }
}

void modprobe_main(void)
{
  char **argv = toys.optargs, *procline = NULL;
  FILE *fs;
  struct module_s *module;
  struct arg_list *dirs;

  if (toys.optc<1 && !FLAG(r) == !FLAG(l)) help_exit("bad syntax");
  // Check for -r flag without arg if yes then do auto remove.
  if (FLAG(r) && !toys.optc) {
    if (rm_mod(0)) perror_exit("rmmod");
    return;
  }

  if (!TT.dirs) {
    struct utsname uts;

    uname(&uts);
    TT.dirs = xzalloc(sizeof(struct arg_list));
    TT.dirs->arg = xmprintf("/lib/modules/%s", uts.release);
  }

  // modules.dep processing for dependency check.
  if (FLAG(l)) {
    for (dirs = TT.dirs; dirs; dirs = dirs->next) {
      xchdir(dirs->arg);
      if (!depmode_read_entry(*toys.optargs)) return;
    }
    error_exit("no module found.");
  }

  // Read /proc/modules to get loaded modules.
  fs = fopen("/proc/modules", "r");

  while (fs && read_line(fs, &procline) > 0) {
    *strchr(procline, ' ') = 0;
    get_mod(procline, 1)->flags = MOD_ALOADED;
    free(procline);
    procline = NULL;
  }
  if (fs) fclose(fs);
  if (FLAG(a) || FLAG(r)) for (; *argv; argv++) add_mod(*argv);
  else {
    add_mod(*argv);
    TT.cmdopts = add_cmdopt(argv);
  }
  if (!TT.probes) {
    if (FLAG(v)) puts("All modules loaded");
    return;
  }
  dirtree_flagread("/etc/modprobe.conf", DIRTREE_SHUTUP, config_action);
  dirtree_flagread("/etc/modprobe.d", DIRTREE_SHUTUP, config_action);

  for (dirs = TT.dirs; dirs; dirs = dirs->next) {
    xchdir(dirs->arg);
    if (TT.symreq) dirtree_read("modules.symbols", config_action);
    if (TT.nudeps) dirtree_read("modules.alias", config_action);
  }

  for (dirs = TT.dirs; dirs; dirs = dirs->next) {
    xchdir(dirs->arg);
    find_dep();
  }

  while ((module = llist_popme(&TT.probes))) {
    if (!module->rnames) {
      if (FLAG(v)) puts("probing by module name");
      /* This is not an alias. Literal names are blacklisted
       * only if '-b' is given.
       */
      if (!FLAG(b) || !(module->flags & MOD_BLACKLIST))
        go_probe(module);
      continue;
    }
    do { // Probe all real names for the alias.
      char *real = ((struct arg_list *)llist_pop(&module->rnames))->arg;
      struct module_s *m2 = get_mod(real, 0);

      if (FLAG(v))
        printf("probing alias %s by realname %s\n", module->name, real);
      if (!m2) continue;
      if (!(m2->flags & MOD_BLACKLIST)
          && (!(m2->flags & MOD_ALOADED) || FLAG(r) || FLAG(D)))
        go_probe(m2);
      free(real);
    } while (module->rnames);
  }
}
