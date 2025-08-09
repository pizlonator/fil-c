/* sh.c - toybox shell
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * This shell aims for bash compatibility. The bash man page is at:
 * http://man7.org/linux/man-pages/man1/bash.1.html
 *
 * The POSIX-2008/SUSv4 shell spec is at:
 * http://opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
 * and http://opengroup.org/onlinepubs/9699919799/utilities/sh.html
 *
 * deviations from posix: don't care about $LANG or $LC_ALL
 * deviations from bash:
 *   redirect+expansion in one pass so we can't report errors between them.
 *   Trailing redirects error at runtime, not parse time.

 * builtins: alias bg command fc fg getopts jobs newgrp read umask unalias wait
 *          disown suspend source pushd popd dirs logout times trap cd hash exit
 *           unset local export readonly set : . let history declare ulimit type
 * "special" builtins: break continue eval exec return shift
 * external with extra shell behavior: kill pwd time test

 * * ? [ # ~ = % [[ ]] function select exit label:

 * TODO: case, wildcard +(*|?), job control (find_plus_minus), ${x//}, $(())

 * TODO: support case in $() because $(case a in a) ;; ; esac) stops at first )
 * TODO: test exit from "trap EXIT" doesn't recurse
 * TODO: ! history expansion
 * TODO: getuid() vs geteuid()
 * TODO: test that $PS1 color changes work without stupid \[ \] hack
 * TODO: Handle embedded NUL bytes in the command line? (When/how?)
 * TODO: set -e -o pipefail, shopt -s nullglob
 * TODO: utf8 isspace
 *
 * bash man page:
 * control operators || & && ; ;; ;& ;;& ( ) | |& <newline>
 * reserved words
 *   ! case  coproc  do done elif else esac fi for  function  if  in  select
 *   then until while { } time [[ ]]
 *
 * Flow control statements:
 *
 * if/then/elif/else/fi, for select while until/do/done, case/esac,
 * {/}, [[/]], (/), function assignment

USE_SH(NEWTOY(break, ">1", TOYFLAG_NOFORK))
USE_SH(NEWTOY(cd, ">1LP[-LP]", TOYFLAG_NOFORK))
USE_SH(NEWTOY(continue, ">1", TOYFLAG_NOFORK))
USE_SH(NEWTOY(declare, "pAailunxr", TOYFLAG_NOFORK))
 // TODO tpgfF
USE_SH(NEWTOY(eval, 0, TOYFLAG_NOFORK))
USE_SH(NEWTOY(exec, "^cla:", TOYFLAG_NOFORK))
USE_SH(NEWTOY(exit, 0, TOYFLAG_NOFORK))
USE_SH(NEWTOY(export, "np", TOYFLAG_NOFORK))
USE_SH(NEWTOY(jobs, "lnprs", TOYFLAG_NOFORK))
USE_SH(NEWTOY(local, 0, TOYFLAG_NOFORK))
USE_SH(NEWTOY(return, ">1", TOYFLAG_NOFORK))
USE_SH(NEWTOY(set, 0, TOYFLAG_NOFORK))
USE_SH(NEWTOY(shift, ">1", TOYFLAG_NOFORK))
USE_SH(NEWTOY(source, "<1", TOYFLAG_NOFORK))
USE_SH(OLDTOY(., source, TOYFLAG_NOFORK))
USE_SH(NEWTOY(unset, "fvn[!fv]", TOYFLAG_NOFORK))
USE_SH(NEWTOY(wait, "n", TOYFLAG_NOFORK))

USE_SH(NEWTOY(sh, "0^(noediting)(noprofile)(norc)sc:i", TOYFLAG_BIN))
USE_SH(OLDTOY(toysh, sh, TOYFLAG_BIN))
USE_SH(OLDTOY(bash, sh, TOYFLAG_BIN))
// Login lies in argv[0], so add some aliases to catch that
USE_SH(OLDTOY(-sh, sh, 0))
USE_SH(OLDTOY(-toysh, sh, 0))
USE_SH(OLDTOY(-bash, sh, 0))

config SH
  bool "sh (toysh)"
  default n
  help
    usage: sh [-c command] [script]

    Command shell.  Runs a shell script, or reads input interactively
    and responds to it. Roughly compatible with "bash". Run "help" for
    list of built-in commands.

    -c	command line to execute
    -i	interactive mode (default when STDIN is a tty)
    -s	don't run script (args set $* parameters but read commands from stdin)

    Command shells parse each line of input (prompting when interactive), perform
    variable expansion and redirection, execute commands (spawning child processes
    and background jobs), and perform flow control based on the return code.

    Parsing:
      syntax errors

    Interactive prompts:
      line continuation

    Variable expansion:
      Note: can cause syntax errors at runtime

    Redirection:
      HERE documents (parsing)
      Pipelines (flow control and job control)

    Running commands:
      process state
      builtins
        cd [[ ]] (( ))
        ! : [ # TODO: help for these?
        true false help echo kill printf pwd test
      child processes

    Job control:
      &    Background process
      Ctrl-C kill process
      Ctrl-Z suspend process
      bg fg jobs kill

    Flow control:
    ;    End statement (same as newline)
    &    Background process (returns true unless syntax error)
    &&   If this fails, next command fails without running
    ||   If this succeeds, next command succeeds without running
    |    Pipelines! (Can of worms...)
    for {name [in...]}|((;;)) do; BODY; done
    if TEST; then BODY; fi
    while TEST; do BODY; done
    case a in X);; esac
    [[ TEST ]]
    ((MATH))

    Job control:
    &    Background process
    Ctrl-C kill process
    Ctrl-Z suspend process
    bg fg jobs kill

# These are here for the help text, they're not selectable and control nothing
config BREAK
  bool
  default n
  depends on SH
  help
    usage: break [N]

    End N levels of for/while/until loop immediately (default 1).

config CD
  bool
  default n
  depends on SH
  help
    usage: cd [-PL] [-] [path]

    Change current directory. With no arguments, go $HOME. Sets $OLDPWD to
    previous directory: cd - to return to $OLDPWD.

    -P	Physical path: resolve symlinks in path
    -L	Local path: .. trims directories off $PWD (default)

config CONTINUE
  bool
  default n
  depends on SH
  help
    usage: continue [N]

    Start next entry in for/while/until loop (or Nth outer loop, default 1).

config DECLARE
  bool
  default n
  depends on SH
  help
    usage: declare [-pAailunxr] [NAME...]

    Set or print variable attributes and values.

    -p	Print variables instead of setting
    -A	Associative array
    -a	Indexed array
    -i	Integer
    -l	Lower case
    -n	Name reference (symlink)
    -r	Readonly
    -u	Uppercase
    -x	Export

config EXIT
  bool
  default n
  depends on SH
  help
    usage: exit [status]

    Exit shell.  If no return value supplied on command line, use value
    of most recent command, or 0 if none.

config SET
  bool
  default n
  depends on SH
  help
    usage: set [+a] [+o OPTION] [VAR...]

    Set variables and shell attributes. Use + to disable and - to enable.
    NAME=VALUE arguments assign to the variable, any leftovers set $1, $2...
    With no arguments, prints current variables.

    -f	NAME is a function
    -v	NAME is a variable
    -n	don't follow name reference

    OPTIONs:
      history - enable command history

config UNSET
  bool
  default n
  depends on SH
  help
    usage: unset [-fvn] NAME...

    -f	NAME is a function
    -v	NAME is a variable
    -n	dereference NAME and unset that

config EVAL
  bool
  default n
  depends on SH
  help
    usage: eval COMMAND...

    Execute (combined) arguments as a shell command.

config EXEC
  bool
  default n
  depends on SH
  help
    usage: exec [-cl] [-a NAME] COMMAND...

    -a	set argv[0] to NAME
    -c	clear environment
    -l	prepend - to argv[0]

config EXPORT
  bool
  default n
  depends on SH
  help
    usage: export [-n] [NAME[=VALUE]...]

    Make variables available to child processes. NAME exports existing local
    variable(s), NAME=VALUE sets and exports.

    -n	Unexport. Turn listed variable(s) into local variables.

    With no arguments list exported variables/attributes as "declare" statements.

config JOBS
  bool
  default n
  depends on SH
  help
    usage: jobs [-lnprs] [%JOB | -x COMMAND...]

    List running/stopped background jobs.

    -l Include process ID in list
    -n Show only new/changed processes
    -p Show process IDs only
    -r Show running processes
    -s Show stopped processes

config LOCAL
  bool
  default n
  depends on SH
  help
    usage: local [NAME[=VALUE]...]

    Create a local variable that lasts until return from this function.
    With no arguments lists local variables in current function context.
    TODO: implement "declare" options.

config RETURN
  bool
  default n
  depends on SH
  help
    usage: return [#]

    Return from function/source with specified value or last command's exit val.

config SHIFT
  bool
  default n
  depends on SH
  help
    usage: shift [N]

    Skip N (default 1) positional parameters, moving $1 and friends along the list.
    Does not affect $0.

config SOURCE
  bool
  default n
  depends on SH
  help
    usage: source FILE [ARGS...]

    Read FILE and execute commands. Any ARGS become positional parameters.

config WAIT
  bool
  default n
  depends on SH
  help
    usage: wait [-n] [ID...]

    Wait for background processes to exit, returning its exit code.
    ID can be PID or job, with no IDs waits for all backgrounded processes.

    -n	Wait for next process to exit
*/

#define FOR_sh
#include "toys.h"

GLOBALS(
  union {
    struct {
      char *c;
    } sh;
    struct {
      char *a;
    } exec;
  };

  // keep SECONDS here: used to work around compiler limitation in run_command()
  long long SECONDS;
  char *isexec, *wcpat;
  unsigned options, jobcnt;
  int hfd, pid, bangpid, recursion;
  jmp_buf forkchild;

  // Callable function array
  struct sh_function {
    char *name;
    struct sh_pipeline {  // pipeline segments: linked list of arg w/metadata
      struct sh_pipeline *next, *prev, *end;
      int count, here, type;
      long lineno;
      struct sh_arg {
        char **v;
        int c;
      } arg[1];
    } *pipeline;
    unsigned long refcount;
  } **functions;
  long funcslen;

  // runtime function call stack. TT.ff is current function, returns to ->next
  struct sh_fcall {
    struct sh_fcall *next, *prev;

    // Each level has its own local variables, root (TT.ff->prev) is globals
    struct sh_vars {
      long flags;
      char *str;
    } *vars;
    long varslen, varscap, shift, lineno;

    struct sh_function *function;
    FILE *source;
    char *ifs, *name, *_;
    struct sh_pipeline *pl;
    struct sh_arg arg;
    struct arg_list *delete;

    // Runtime stack of nested if/else/fi and for/do/done contexts.
    struct sh_blockstack {
      struct sh_blockstack *next;
      struct sh_pipeline *start, *middle;
      struct sh_process *pp;       // list of processes piping in to us
      int run, loop, *urd, pout, pipe;
      struct sh_arg farg;          // for/select arg stack, case wildcard deck
      struct arg_list *fdelete;    // farg's cleanup list
      char *fvar;                  // for/select's iteration variable name
    } *blk;

// TODO ctrl-Z suspend should stop script
    struct sh_process {
      struct sh_process *next, *prev; // | && ||
      struct arg_list *delete;   // expanded strings
      // undo redirects, a=b at start, child PID, exit status, has !, job #
      int *urd, envlen, pid, exit, flags, job, dash, refcount;
      long long when; // when job backgrounded/suspended
      struct sh_arg *raw, arg;
    } *pp;
  } *ff;

  // job list, command line for $*, scratch space for do_wildcard_files()
  struct sh_arg jobs, *wcdeck;
)

// functions contain pipelines contain functions: prototype because loop
static void free_pipeline(void *pipeline);
// recalculate needs to get/set variables, but setvar_found calls recalculate
static struct sh_vars *setvar(char *str);

// ordered for greedy matching, so >&; becomes >& ; not > &;
// making these const means I need to typecast the const away later to
// avoid endless warnings.
static const char *redirectors[] = {"<<<", "<<-", "<<", "<&", "<>", "<", ">>",
  ">&", ">|", ">", "&>>", "&>", 0};

// The order of these has to match the string in set_main()
#define OPT_B	0x100
#define OPT_C	0x200
#define OPT_x	0x400
#define OPT_u	0x800

// only export $PWD and $OLDPWD on first cd
#define OPT_cd  0x80000000

// struct sh_process->flags
#define PFLAG_NOT    1

static void sherror_msg(char *msg, ...)
{
  va_list va;
  struct sh_fcall *ff;

  va_start(va, msg);
// TODO $ sh -c 'x() { ${x:?blah}; }; x'
// environment: line 1: x: blah
  for (ff = TT.ff; !ff->source || !ff->name; ff = ff->next);
  if (!FLAG(i) || ff!=TT.ff->prev)
    fprintf(stderr, "%s: line %ld: ", ff->name,
      ff->pl ? ff->pl->lineno : ff->lineno);
  verror_msg(msg, 0, va);
  va_end(va);
}

static void syntax_err(char *s)
{
// TODO: script@line only for script not interactive.
  sherror_msg("syntax error: %s", s);
  toys.exitval = 2;
  if (!(TT.options&FLAG_i)) xexit();
}

void debug_show_fds()
{
  int x = 0, fd = open("/proc/self/fd", O_RDONLY);
  DIR *X = fdopendir(fd);
  struct dirent *DE;
  char *s, *ss = 0, buf[4096], *sss = buf;

  if (!X) return;
  for (; (DE = readdir(X));) {
    if (atoi(DE->d_name) == fd) continue;
    s = xreadlink(ss = xmprintf("/proc/self/fd/%s", DE->d_name));
    if (s && *s != '.') sss += sprintf(sss, ", %s=%s"+2*!x++, DE->d_name, s);
    free(s); free(ss);
  }
  *sss = 0;
  dprintf(2, "%d fd:%s\n", getpid(), buf);
  closedir(X);
}

static char **nospace(char **ss)
{
  while (isspace(**ss)) ++*ss;

  return ss;
}

// append to array with null terminator and realloc as necessary
static void arg_add(struct sh_arg *arg, char *data)
{
  // expand with stride 32. Micro-optimization: don't realloc empty stack
  if (!(arg->c&31) && (arg->c || !arg->v))
    arg->v = xrealloc(arg->v, sizeof(char *)*(arg->c+33));
  arg->v[arg->c++] = data;
  arg->v[arg->c] = 0;
}

// add argument to an arg_list
static void *push_arg(struct arg_list **list, void *arg)
{
  struct arg_list *al;

  if (list) {
    al = xmalloc(sizeof(struct arg_list));
    al->next = *list;
    al->arg = arg;
    *list = al;
  }

  return arg;
}

static void arg_add_del(struct sh_arg *arg, char *data,struct arg_list **delete)
{
  arg_add(arg, push_arg(delete, data));
}

// Assign one variable from malloced key=val string, returns var struct
// TODO implement remaining types
#define VAR_NOFREE    (1<<10)
#define VAR_WHITEOUT  (1<<9)
#define VAR_DICT      (1<<8)
#define VAR_ARRAY     (1<<7)
#define VAR_INT       (1<<6)
#define VAR_TOLOWER   (1<<5)
#define VAR_TOUPPER   (1<<4)
#define VAR_NAMEREF   (1<<3)
#define VAR_EXPORT    (1<<2)
#define VAR_READONLY  (1<<1)
#define VAR_MAGIC     (1<<0)

// return length of valid variable name
static char *varend(char *s)
{
  if (isdigit(*s)) return s;
  while (*s>' ' && (*s=='_' || !ispunct(*s))) s++;

  return s;
}

// TODO: this has to handle VAR_NAMEREF, but return dangling symlink
// Also, unset -n, also "local ISLINK" to parent var.
// Return sh_vars * or 0 if not found.
// Sets *pff to fcall (only if found), only returns whiteouts when pff not NULL
static struct sh_vars *findvar(char *name, struct sh_fcall **pff)
{
  int len = varend(name)-name;
  struct sh_fcall *ff = TT.ff;

  // advance through locals to global context, ignoring whiteouts
  if (len) do {
    struct sh_vars *var = ff->vars+ff->varslen;

    if (var) while (var--!=ff->vars) {
      if (strncmp(var->str, name, len) || var->str[len]!='=') continue;
      if (pff) *pff = ff;
      else if (var->flags&VAR_WHITEOUT) return 0;

      return var;
    }
  } while ((ff = ff->next)!=TT.ff);

  return 0;
}

// get value of variable starting at s.
static char *getvar(char *s)
{
  struct sh_vars *var = findvar(s, 0);

  if (!var) return 0;

  if (var->flags & VAR_MAGIC) {
    char c = *var->str;

    if (c == 'S') sprintf(toybuf, "%lld", (millitime()-TT.SECONDS)/1000);
    else if (c == 'R') sprintf(toybuf, "%ld", random()&((1<<16)-1));
    else if (c == 'L') sprintf(toybuf, "%ld", TT.ff->pl->lineno);
    else if (c == 'G') sprintf(toybuf, "TODO: GROUPS");
    else if (c == 'B') sprintf(toybuf, "%d", getpid());
    else if (c == 'E') {
      struct timespec ts;

      clock_gettime(CLOCK_REALTIME, &ts);
      sprintf(toybuf, "%lld%c%06ld", (long long)ts.tv_sec, (s[5]=='R')*'.',
              ts.tv_nsec/1000);
    }

    return toybuf;
  }

  return varend(var->str)+1;
}

// Append variable to ff->vars, returning *struct. Does not check duplicates.
static struct sh_vars *addvar(char *s, struct sh_fcall *ff)
{
  if (ff->varslen == ff->varscap) {
    ff->varscap += 32;
    ff->vars = xrealloc(ff->vars, (ff->varscap)*sizeof(*ff->vars));
  }
  if (!s) return ff->vars;
  ff->vars[ff->varslen].flags = 0;
  ff->vars[ff->varslen].str = s;

  return ff->vars+ff->varslen++;
}

// Recursively calculate string into dd, returns 0 if failed, ss = error point
// Recursion resolves operators of lower priority level to a value
// Loops through operators at same priority
#define NO_ASSIGN 128
static int recalculate(long long *dd, char **ss, int lvl)
{
  long long ee, ff;
  char *var = 0, *val, cc = **nospace(ss);
  int ii, noa = lvl&NO_ASSIGN;
  lvl &= NO_ASSIGN-1;

  // Unary prefixes can only occur at the start of a parse context
  if (cc=='!' || cc=='~') {
    ++*ss;
    if (!recalculate(dd, ss, noa|15)) return 0;
    *dd = (cc=='!') ? !*dd : ~*dd;
  } else if (cc=='+' || cc=='-') {
    // Is this actually preincrement/decrement? (Requires assignable var.)
    if (*++*ss==cc) {
      val = (*ss)++;
      nospace(ss);
      if (*ss==(var = varend(*ss))) {
        *ss = val;
        var = 0;
      }
    }
    if (!var) {
      if (!recalculate(dd, ss, noa|15)) return 0;
      if (cc=='-') *dd = -*dd;
    }
  } else if (cc=='(') {
    ++*ss;
    if (!recalculate(dd, ss, noa|1)) return 0;
    if (**ss!=')') return 0;
    else ++*ss;
  } else if (isdigit(cc)) {
    *dd = strtoll(*ss, ss, 0);
    if (**ss=='#') {
      if (!*++*ss || isspace(**ss) || ispunct(**ss)) return 0;
      *dd = strtoll(val = *ss, ss, *dd);
      if (val == *ss) return 0;
    }
  } else if ((var = varend(*ss))==*ss) {
    // At lvl 0 "" is ok, anything higher needs a non-empty equation
    if (lvl || (cc && cc!=')')) return 0;
    *dd = 0;

    return 1;
  }

  // If we got a variable, evaluate its contents to set *dd
  if (var) {
    // Recursively evaluate, catching x=y; y=x; echo $((x))
    if (TT.recursion==100) {
      sherror_msg("recursive occlusion");
      --TT.recursion;

      return 0;
    }
    val = getvar(var = *ss) ? : "";
    ii = recalculate(dd, &val, noa);
    TT.recursion--;
    if (!ii) return 0;
    if (*val) {
      sherror_msg("bad math: %s @ %d", var, (int)(val-var));

      return 0;
    }
    val = *ss = varend(var);

    // Operators that assign to a varible must be adjacent to one:

    // Handle preincrement/predecrement (only gets here if var set before else)
    if (cc=='+' || cc=='-') {
      if (cc=='+') ee = ++*dd;
      else ee = --*dd;
    } else cc = 0;

    // handle postinc/postdec
    if ((**nospace(ss)=='+' || **ss=='-') && (*ss)[1]==**ss) {
      ee = ((cc = **ss)=='+') ? 1+*dd : -1+*dd;
      *ss += 2;

    // Assignment operators: = *= /= %= += -= <<= >>= &= ^= |=
    } else if (lvl<=2 && (*ss)[ii = (-1 != stridx("*/%+-", **ss))
               +2*!smemcmp(*ss, "<<", 2)+2*!smemcmp(*ss, ">>", 2)]=='=')
    {
      // TODO: assignments are lower priority BUT must go after variable,
      // come up with precedence checking tests?
      cc = **ss;
      *ss += ii+1;
      if (!recalculate(&ee, ss, noa|1)) return 0; // TODO lvl instead of 1?
      if (cc=='*') *dd *= ee;
      else if (cc=='+') *dd += ee;
      else if (cc=='-') *dd -= ee;
      else if (cc=='<') *dd <<= ee;
      else if (cc=='>') *dd >>= ee;
      else if (cc=='&') *dd &= ee;
      else if (cc=='^') *dd ^= ee;
      else if (cc=='|') *dd |= ee;
      else if (!cc) *dd = ee;
      else if (!ee) {
        sherror_msg("%c0", cc);

        return 0;
      } else if (cc=='/') *dd /= ee;
      else if (cc=='%') *dd %= ee;
      ee = *dd;
    }
    if (cc && !noa) setvar(xmprintf("%.*s=%lld", (int)(val-var), var, ee));
  }

  // x**y binds first
  if (lvl<=14) while (strstart(nospace(ss), "**")) {
    if (!recalculate(&ee, ss, noa|15)) return 0;
    if (ee<0) sherror_msg("** < 0");
    for (ff = *dd, *dd = 1; ee; ee--) *dd *= ff;
  }

  // w*x/y%z bind next
  if (lvl<=13) while ((cc = **nospace(ss)) && strchr("*/%", cc)) {
    ++*ss;
    if (!recalculate(&ee, ss, noa|14)) return 0;
    if (cc=='*') *dd *= ee;
    else if (!ee) {
      sherror_msg("%c0", cc);

      return 0;
    } else if (cc=='%') *dd %= ee;
    else *dd /= ee;
  }

  // x+y-z
  if (lvl<=12) while ((cc = **nospace(ss)) && strchr("+-", cc)) {
    ++*ss;
    if (!recalculate(&ee, ss, noa|13)) return 0;
    if (cc=='+') *dd += ee;
    else *dd -= ee;
  }

  // x<<y >>

  if (lvl<=11) while ((cc = **nospace(ss)) && strchr("<>", cc) && cc==(*ss)[1]){
    *ss += 2;
    if (!recalculate(&ee, ss, noa|12)) return 0;
    if (cc == '<') *dd <<= ee;
    else *dd >>= ee;
  }

  // x<y <= > >=
  if (lvl<=10) while ((cc = **nospace(ss)) && strchr("<>", cc)) {
    if ((ii = *++*ss=='=')) ++*ss;
    if (!recalculate(&ee, ss, noa|11)) return 0;
    if (cc=='<') *dd = ii ? (*dd<=ee) : (*dd<ee);
    else *dd = ii ? (*dd>=ee) : (*dd>ee);
  }

  if (lvl<=9) while ((cc = **nospace(ss)) && strchr("=!", cc) && (*ss)[1]=='='){
    *ss += 2;
    if (!recalculate(&ee, ss, noa|10)) return 0;
    *dd = (cc=='!') ? *dd != ee : *dd == ee;
  }

  if (lvl<=8) while (**nospace(ss)=='&' && (*ss)[1]!='&') {
    ++*ss;
    if (!recalculate(&ee, ss, noa|9)) return 0;
    *dd &= ee;
  }

  if (lvl<=7) while (**nospace(ss)=='^') {
    ++*ss;
    if (!recalculate(&ee, ss, noa|8)) return 0;
    *dd ^= ee;
  }

  if (lvl<=6) while (**nospace(ss)=='|' && (*ss)[1]!='|') {
    ++*ss;
    if (!recalculate(&ee, ss, noa|7)) return 0;
    *dd |= ee;
  }

  if (lvl<=5) while (strstart(nospace(ss), "&&")) {
    if (!recalculate(&ee, ss, noa|6|NO_ASSIGN*!*dd)) return 0;
    *dd = *dd && ee;
  }

  if (lvl<=4) while (strstart(nospace(ss), "||")) {
    if (!recalculate(&ee, ss, noa|5|NO_ASSIGN*!!*dd)) return 0;
    *dd = *dd || ee;
  }

  // ? : slightly weird: recurses with lower priority instead of looping
  // because a ? b ? c : d ? e : f : g == a ? (b ? c : (d ? e : f) : g)
  if (lvl<=3) if (**nospace(ss)=='?') {
    ++*ss;
    if (**nospace(ss)==':' && *dd) ee = *dd;
    else if (!recalculate(&ee, ss, noa|1|NO_ASSIGN*!*dd) || **nospace(ss)!=':')
      return 0;
    ++*ss;
    if (!recalculate(&ff, ss, noa|1|NO_ASSIGN*!!*dd)) return 0;
    *dd = *dd ? ee : ff;
  }

  // lvl<=2 assignment would go here, but handled above because variable

  // , (slightly weird, replaces dd instead of modifying it via ee/ff)
  if (lvl<=1) while (**nospace(ss)==',') {
    ++*ss;
    if (!recalculate(dd, ss, noa|2)) return 0;
  }

  return 1;
}

// Return length of utf8 char @s fitting in len, writing value into *cc
static int getutf8(char *s, int len, int *cc)
{
  unsigned wc;

  if (len<0) wc = len = 0;
  else if (1>(len = utf8towc(&wc, s, len))) wc = *s, len = 1;
  if (cc) *cc = wc;

  return len;
}

// utf8 strchr: return wide char matched at wc from chrs, or 0 if not matched
// if len, save length of next wc (whether or not it's in list)
static int utf8chr(char *wc, char *chrs, int *len)
{
  unsigned wc1, wc2;
  int ll;

  if (len) *len = 1;
  if (!*wc) return 0;
  if (0<(ll = utf8towc(&wc1, wc, 99))) {
    if (len) *len = ll;
    while (*chrs) {
      if(1>(ll = utf8towc(&wc2, chrs, 99))) chrs++;
      else {
        if (wc1 == wc2) return wc1;
        chrs += ll;
      }
    }
  }

  return 0;
}

// Update $IFS cache in function call stack after variable assignment
static void cache_ifs(char *s, struct sh_fcall *ff)
{
  if (strstart(&s, "IFS="))
    do ff->ifs = s; while ((ff = ff->next) != TT.ff->prev);
}

// declare -aAilnrux
// ft
// TODO VAR_ARRAY VAR_DICT

// Assign new name=value string for existing variable. s takes x=y or x+=y
static struct sh_vars *setvar_found(char *s, int freeable, struct sh_vars *var)
{
  char *vs = var->str, *ss, *sss, *sd, buf[24];
  long ii, jj, kk, flags = var->flags&~VAR_WHITEOUT;
  long long ll;
  int cc, vlen = varend(s)-s;

  if (flags&VAR_READONLY) {
    sherror_msg("%.*s: read only", vlen, s);
    goto bad;
  }

  // If += has no old value (addvar placeholder or empty old var) yank the +
  if (s[vlen]=='+' && (var->str==s || !strchr(var->str, '=')[1])) {
    ss = xmprintf("%.*s%s", vlen, s, s+vlen+1);
    if (vs!=s && freeable++) free(s);
    s = ss;
  }

  // Handle VAR_NAMEREF mismatch by replacing name
  if (strncmp(var->str, s, vlen)) {
    ss = s+vlen+(s[vlen]=='+')+1;
    ss = xmprintf("%.*s%s", (vlen = varend(var->str)-var->str)+1, var->str, ss);
    if (vs!=s && freeable++) free(s);
    s = ss;
  }

  // utf8 aware case conversion, two pass (measure, allocate, convert) because
  // unicode IS stupid enough for upper/lower case to be different utf8 byte
  // lengths, for example lowercase of U+023a (c8 ba) is U+2c65 (e2 b1 a5)
  if (flags&(VAR_TOUPPER|VAR_TOLOWER)) {
    for (jj = kk = 0, sss = 0; jj<2; jj++, sss = sd = xmalloc(vlen+kk+2)) {
      sd = jj ? stpncpy(sss, s, vlen+1) : (void *)&sss;
      for (ss = s+vlen+1; (ii = getutf8(ss, 4, &cc)); ss += ii) {
        kk += wctoutf8(sd, (flags&VAR_TOUPPER) ? towupper(cc) : towlower(cc));
        if (jj) {
          sd += kk;
          kk = 0;
        }
      }
    }
    *sd = 0;
    if (vs!=s && freeable++) free(s);
    s = sss;
  }

  // integer variables treat += differently
  ss = s+vlen+(s[vlen]=='+')+1;
  if (flags&VAR_INT) {
    sd = ss;
    if (!recalculate(&ll, &sd, 0) || *sd) {
      sherror_msg("bad math: %s @ %d", ss, (int)(sd-ss));

      goto bad;
    }

    sprintf(buf, "%lld", ll);
    if (flags&VAR_MAGIC) {
      if (*s == 'S') {
        ll *= 1000;
        TT.SECONDS = (s[vlen]=='+') ? TT.SECONDS+ll : millitime()-ll;
      } else if (*s == 'R') srandom(ll);
      if (freeable) free(s);

      // magic can't be whiteout or nofree, and keeps old string
      return var;
    } else if (s[vlen]=='+' || strcmp(buf, ss)) {
      if (s[vlen]=='+') ll += atoll(strchr(var->str, '=')+1);
      ss = xmprintf("%.*s=%lld", vlen, s, ll);
      if (vs!=s && freeable++) free(s);
      s = ss;
    }
  } else if (s[vlen]=='+' && !(flags&VAR_MAGIC)) {
    ss = xmprintf("%s%s", var->str, ss);
    if (vs!=s && freeable++) free(s);
    s = ss;
  }

  // Replace old string with new one, adjusting nofree status
  if (flags&VAR_NOFREE) flags ^= VAR_NOFREE;
  else free(var->str);
  if (!freeable) flags |= VAR_NOFREE;
  var->str = s;
  var->flags = flags;

  return var;
bad:
  if (freeable) free(s);

  return 0;
}

// Creates new variables (local or global) and handles +=
// returns 0 on error, else sh_vars of new entry. Adds at ff if not found.
static struct sh_vars *setvar_long(char *s, int freeable, struct sh_fcall *ff)
{
  struct sh_vars *vv = 0, *was;
  char *ss;

  if (!s) return 0;
  ss = varend(s);
  if (ss[*ss=='+']!='=') {
    sherror_msg("bad setvar %s\n", s);
    if (freeable) free(s);

    return 0;
  }

  // Add if necessary, set value, and remove again if we added but set failed
  if (!(was = vv = findvar(s, &ff))) (vv = addvar(s, ff))->flags = VAR_NOFREE;
  if (!setvar_found(s, freeable, vv)) {
    if (!was) memmove(vv, vv+1, sizeof(ff->vars)*(--ff->varslen-(vv-ff->vars)));

    return 0;
  }
  cache_ifs(vv->str, ff);

  return vv;
}

// Set variable via a malloced "name=value" (or "name+=value") string.
// Returns sh_vars * or 0 for failure (readonly, etc)
static struct sh_vars *setvar(char *str)
{
  return setvar_long(str, 1, TT.ff->prev);
}


// returns whether variable found (whiteout doesn't count)
static int unsetvar(char *name)
{
  struct sh_fcall *ff;
  struct sh_vars *var = findvar(name, &ff);
  int len = varend(name)-name;

  if (!var || (var->flags&VAR_WHITEOUT)) return 0;
  if (var->flags&VAR_READONLY) sherror_msg("readonly %.*s", len, name);
  else {
    // turn local into whiteout
    if (ff != TT.ff->prev) {
      var->flags = VAR_WHITEOUT;
      if (!(var->flags&VAR_NOFREE))
        (var->str = xrealloc(var->str, len+2))[len+1] = 0;
    // free from global context
    } else {
      if (!(var->flags&VAR_NOFREE)) free(var->str);
      memmove(var, var+1, sizeof(ff->vars)*(ff->varslen-- -(var-ff->vars)));
    }
    if (!strcmp(name, "IFS"))
      do ff->ifs = " \t\n"; while ((ff = ff->next) != TT.ff->prev);
  }

  return 1;
}

static struct sh_vars *setvarval(char *name, char *val)
{
  return setvar(xmprintf("%s=%s", name, val));
}

// TODO: keep variable arrays sorted for binary search

// create array of variables visible in current function.
static struct sh_vars **visible_vars(void)
{
  struct sh_arg arg;
  struct sh_fcall *ff;
  struct sh_vars *vv;
  unsigned ii, jj, len;

  arg.c = 0;
  arg.v = 0;

  // Find non-duplicate entries: TODO, sort and binary search
  for (ff = TT.ff; ; ff = ff->next) {
    if (ff->vars) for (ii = ff->varslen; ii--;) {
      vv = ff->vars+ii;
      len = 1+(varend(vv->str)-vv->str);
      for (jj = 0; ;jj++) {
        if (jj == arg.c) arg_add(&arg, (void *)vv);
        else if (strncmp(arg.v[jj], vv->str, len)) continue;

        break;
      }
    }
    if (ff->next == TT.ff) break;
  }

  return (void *)arg.v;
}

// malloc declare -x "escaped string"
static char *declarep(struct sh_vars *var)
{
  char *types = "rxnuliaA", *esc = "$\"\\`", *in, flags[16], *out = flags, *ss;
  int len;

  for (len = 0; types[len]; len++) if (var->flags&(2<<len)) *out++ = types[len];
  if (out==flags) *out++ = '-';
  *out = 0;
  len = out-flags;

  for (in = var->str; *in; in++) len += !!strchr(esc, *in);
  len += in-var->str;
  ss = xmalloc(len+15);

  len = varend(var->str)-var->str;
  out = ss + sprintf(ss, "declare -%s %.*s", flags, len, var->str);
  if (var->flags != VAR_MAGIC)  {
    out = stpcpy(out, "=\"");
    for (in = var->str+len+1; *in; *out++ = *in++)
      if (strchr(esc, *in)) *out++ = '\\';
    *out++ = '"';
  }
  *out = 0;

  return ss;
}

// Skip past valid prefix that could go before redirect
static char *skip_redir_prefix(char *word)
{
  char *s = word;

  if (*s == '{') {
    if (*(s = varend(s+1)) == '}' && s != word+1) s++;
    else s = word;
  } else while (isdigit(*s)) s++;

  return s;
}

// parse next word from command line. Returns end, or 0 if need continuation
// caller eats leading spaces. early = skip one quote block (or return start)
static char *parse_word(char *start, int early)
{
  int ii, qq, qc = 0, quote = 0;
  char *end = start, *ss;

  // Handle redirections, <(), (( )) that only count at the start of word
  ss = skip_redir_prefix(end); // 123<<file- parses as 2 args: "123<<" "file-"
  if (strstart(&ss, "<(") || strstart(&ss, ">(")) {
    toybuf[quote++]=')';
    end = ss;
  } else if ((ii = anystart(ss, (void *)redirectors))) return ss+ii;
  if (strstart(&end, "((")) toybuf[quote++] = 254;

  // Loop to find end of this word
  while (*end) {
    // If we're stopping early and already handled a symbol...
    if (early && end!=start && !quote) break;

    // barf if we're near overloading quote stack (nesting ridiculously deep)
    if (quote>4000) {
      syntax_err("bad quote depth");
      return (void *)1;
    }

    // Are we in a quote context?
    if ((qq = quote ? toybuf[quote-1] : 0)) {
      ii = *end++;
      if ((qq==')' || qq>=254) && (ii=='(' || ii==')')) { // parentheses nest
        if (ii=='(') qc++;
        else if (qc) qc--;
        else if (qq>=254) {
          // (( can end with )) or retroactively become two (( if we hit one )
          if (ii==')' && *end==')') quote--, end++;
          else if (qq==254) return start+1;
          else if (qq==255) toybuf[quote-1] = ')';
        } else if (ii==')') quote--;
      } else if (ii==(qq&127)) quote--;        // matching end quote
      else if (qq!='\'') end--, ii = 0;  // single quote claims everything
      if (ii) continue;                  // fall through for other quote types

    // space and flow control chars only end word when not quoted in any way
    } else {
      if (isspace(*end)) break;
      ss = end + anystart(end, (char *[]){";;&", ";;", ";&", ";", "||",
        "|&", "|", "&&", "&", "(", ")", 0});
      if (ss==end) ss += anystart(end, (void *)redirectors);
      if (ss!=end) return (end==start) ? ss : end;
    }

    // start new quote context? (' not special within ")
    if (strchr("'\"`"+(qq=='"'), ii = *end++)) toybuf[quote++] = ii;

    // \? $() ${} $[] ?() *() +() @() !()
    else {
      if (ii=='$' && qq != 0247 && -1!=(qq = stridx("({['", *end))) {
        if (strstart(&end, "((")) {
          end--;
          toybuf[quote++] = 255;
        } else toybuf[quote++] = ")}]\247"[qq]; // last is '+128
      } else if (*end=='(' && strchr("?*+@!", ii)) toybuf[quote++] = ')';
      else {
        if (ii!='\\') end--;
        else if (!end[*end=='\n']) return (*end && !early) ? 0 : end;
        if (early && !quote) return end;
      }
      end++;
    }
  }

  return (quote && !early) ? 0 : end;
}

// Return next available high (>=10) file descriptor
static int next_hfd()
{
  int hfd;

  for (; TT.hfd<=99999; TT.hfd++) if (-1 == fcntl(TT.hfd, F_GETFL)) break;
  hfd = TT.hfd;
  if (TT.hfd>99999) {
    hfd = -1;
    if (!errno) errno = EMFILE;
  }

  return hfd;
}

// Perform a redirect, saving displaced filehandle to a high (>10) fd
// rd is an int array: [0] = count, followed by from/to pairs to restore later.
// If from >= 0 dup from->to after saving to. If from == -1 just save to.
// if from == -2 schedule "to" to be closed by unredirect.
static int save_redirect(int **rd, int from, int to)
{
  int cnt, hfd, *rr;

//dprintf(2, "%d redir %d to %d\n", getpid(), from, to);
  if (from == to) return 0;
  // save displaced to, copying to high (>=10) file descriptor to undo later
  // except if we're saving to environment variable instead (don't undo that)
  if (from>-2) {
    if ((hfd = next_hfd())==-1) return 1;
    if (hfd != dup2(to, hfd)) hfd = -1;
    else fcntl(hfd, F_SETFD, FD_CLOEXEC);

    // dup "to"
    if (from >= 0 && to != dup2(from, to)) {
      if (hfd >= 0) close(hfd);

      return 1;
    }
  } else {
    hfd = to;
    to = -1;
  }

  // Append undo information to redirect list so we can restore saved hfd later.
  if (!((cnt = *rd ? **rd : 0)&31)) *rd = xrealloc(*rd, (cnt+33)*2*sizeof(int));
  *(rr = *rd) = ++cnt;
  rr[2*cnt-1] = hfd;
  rr[2*cnt] = to;

  return 0;
}

// restore displaced filehandles, closing high filehandles they were copied to
static void unredirect(int *urd)
{
  int *rr = urd+1, i;

  if (!urd) return;

  for (i = 0; i<*urd; i++, rr += 2) if (rr[0] != -1) {
    // No idea what to do about fd exhaustion here, so Steinbach's Guideline.
    dup2(rr[0], rr[1]);
    close(rr[0]);
  }
  free(urd);
}

// TODO: waitpid(WNOHANG) to clean up zombies and catch background& ending
static void subshell_callback(char **argv)
{
  struct sh_fcall *ff;

  // Don't leave open filehandles to scripts in children
  for (ff = TT.ff; ff!=TT.ff->prev; ff = ff->next)
    if (ff->source) fclose(ff->source);

  // This depends on environ having been replaced by caller
  environ[1] = xmprintf("@%d,%d", getpid(), getppid());
  environ[2] = xmprintf("$=%d", TT.pid);
// TODO: test $$ in (nommu)
}

// TODO what happens when you background a function?
// turn a parsed pipeline back into a string.
static char *pl2str(struct sh_pipeline *pl, int one)
{
  struct sh_pipeline *end = 0, *pp;
  int len QUIET, i;
  char *ss;

  // Find end of block (or one argument)
  if (one) end = pl->next;
  else for (end = pl, len = 0; end; end = end->next)
    if (end->type == 1) len++;
    else if (end->type == 3 && --len<0) break;

  // measure, then allocate
  for (ss = 0;; ss = xmalloc(len+1)) {
    for (pp = pl; pp != end; pp = pp->next) {
      if (pp->type == 'F') continue; // TODO fix this
      for (i = len = 0; i<=pp->arg->c; i++)
        len += snprintf(ss+len, ss ? INT_MAX : 0, " %s"+!i,
           pp->arg->v[i] ? : ";"+(pp->next==end));
    }
    if (ss) return ss;
  }

// TODO test output with case and function
// TODO add HERE documents back in
// TODO handle functions
}

static struct sh_blockstack *clear_block(struct sh_blockstack *blk)
{
  memset(blk, 0, sizeof(*blk));
  blk->start = TT.ff->pl;
  blk->run = 1;
  blk->pout = -1;

  return blk;
}

// when ending a block, free, cleanup redirects and pop stack.
static struct sh_pipeline *pop_block(void)
{
  struct sh_pipeline *pl = 0;
  struct sh_blockstack *blk = TT.ff->blk;

  // when ending a block, free, cleanup redirects and pop stack.
  if (blk->pout != -1) close(blk->pout);
  unredirect(blk->urd);
  llist_traverse(blk->fdelete, llist_free_arg);
  free(blk->farg.v);
  if (TT.ff->blk->next) {
    pl = blk->start->end;
    free(llist_pop(&TT.ff->blk));
  } else clear_block(blk);

  return pl;
}

// Push a new empty block to the stack
static void add_block(void)
{
  struct sh_blockstack *blk = clear_block(xmalloc(sizeof(*blk)));

  blk->next = TT.ff->blk;
  TT.ff->blk = blk;
}

// Add entry to runtime function call stack
static struct sh_fcall *call_function(void)
{
  // dlist in reverse order: TT.ff = current function, TT.ff->prev = globals
  dlist_add_nomalloc((void *)&TT.ff, xzalloc(sizeof(struct sh_fcall)));
  TT.ff = TT.ff->prev;
  add_block();

// TODO caller needs to set pl, vars, func
  // default $* is to copy previous
  TT.ff->arg.v = TT.ff->next->arg.v;
  TT.ff->arg.c = TT.ff->next->arg.c;
  TT.ff->ifs = TT.ff->next->ifs;

  return TT.ff;
}

static void free_function(struct sh_function *funky)
{
  if (!funky || --funky->refcount) return;

  free(funky->name);
  llist_traverse(funky->pipeline, free_pipeline);
  free(funky);
}

static int free_process(struct sh_process *pp)
{
  int rc;

  if (!pp) return 127;
  rc = pp->exit;
  if (!--pp->refcount) {
    llist_traverse(pp->delete, llist_free_arg);
    free(pp);
  }

  return rc;
}

// Clean up and pop TT.ff
static void end_fcall(void)
{
  struct sh_fcall *ff = TT.ff;

  // forked child does NOT clean up
  if (ff->pp == (void *)1) _exit(toys.exitval);

  // Free local vars then update $_ in other vars
  while (ff->varslen)
    if (!(ff->vars[--ff->varslen].flags&VAR_NOFREE))
      free(ff->vars[ff->varslen].str);
  free(ff->vars);
  ff->vars = 0;
  if (ff->_) setvarval("_", ff->_);

  // Free the rest
  llist_traverse(ff->delete, llist_free_arg);
  ff->delete = 0;
  while (pop_block());
  free(ff->blk);
  free_function(ff->function);
  if (ff->pp) {
    unredirect(ff->pp->urd);
    ff->pp->urd = 0;
    free_process(ff->pp);
  }
  free(dlist_pop(&TT.ff));
}

// TODO check every caller of run_subshell for error, or syntax_error() here
// from pipe() failure

// TODO need CLOFORK? CLOEXEC doesn't help if we don't exec...

// Pass environment and command string to child shell, return PID of child
static int run_subshell(char *str, int len)
{
  pid_t pid;
//dprintf(2, "%d run_subshell %.*s\n", getpid(), len, str); debug_show_fds();
  // The with-mmu path is significantly faster.
  if (CFG_TOYBOX_FORK) {
    if ((pid = fork())<0) perror_msg("fork");
    else if (!pid) {
      call_function()->pp = (void *)1;
      if (str) {
        TT.ff->source = fmemopen(str, len, "r");
        longjmp(TT.forkchild, 1);
      }
    }

  // On nommu vfork, exec /proc/self/exe, and pipe state data to ourselves.
  } else {
    int pipes[2];
    unsigned i;
    char **oldenv = environ, *ss = str ? : pl2str(TT.ff->pl->next, 0);
    struct sh_vars **vv;

    // open pipe to child
    if (pipe(pipes) || 254 != dup2(pipes[0], 254)) return 1;
    close(pipes[0]);
    fcntl(pipes[1], F_SETFD, FD_CLOEXEC);

    // vfork child with clean environment
    environ = xzalloc(4*sizeof(char *));
    *environ = getvar("PATH") ? : "PATH=";
    pid = xpopen_setup(0, 0, subshell_callback);
// TODO what if pid -1? Handle process exhaustion.
    // free entries added to end of environment by callback (shared heap)
    free(environ[1]);
    free(environ[2]);
    free(environ);
    environ = oldenv;

    // marshall context to child
    close(254);
    // TODO: need ff->name and ff->source's lineno
    dprintf(pipes[1], "%lld %u %ld %u %u\n", TT.SECONDS,
      TT.options, TT.ff->lineno, TT.pid, TT.bangpid);

    for (i = 0, vv = visible_vars(); vv[i]; i++)
      dprintf(pipes[1], "%u %lu\n%.*s", (unsigned)strlen(vv[i]->str),
              vv[i]->flags, (int)strlen(vv[i]->str), vv[i]->str);
    free(vv);

    // send command
    dprintf(pipes[1], "0 0\n%.*s\n", len, ss);
    if (!str) free(ss);
    close(pipes[1]);
  }

  return pid;
}

// Call subshell with either stdin/stdout redirected, return other end of pipe
static int pipe_subshell(char *s, int len, int out)
{
  int pipes[2], *uu = 0, in = !out;

  // Grab subshell data
  if (pipe(pipes)) {
    perror_msg("%.*s", len, s);

    return -1;
  }

  // Perform input or output redirect and launch process (ignoring errors)
  save_redirect(&uu, pipes[in], in);
  close(pipes[in]);
  fcntl(pipes[!in], F_SETFD, FD_CLOEXEC);
  run_subshell(s, len);
  fcntl(pipes[!in], F_SETFD, 0);
  unredirect(uu);

  return pipes[out];
}

// grab variable or special param (ala $$) up to len bytes. Return value.
// set *used to length consumed. Does not handle $* and $@
char *getvar_special(char *str, int len, int *used, struct arg_list **delete)
{
  char *s = 0, *ss, cc = *str;
  unsigned uu;

  *used = 1;
  if (cc == '-') {
    s = ss = xmalloc(8);
    if (TT.options&FLAG_i) *ss++ = 'i';
    if (TT.options&OPT_B) *ss++ = 'B';
    if (TT.options&FLAG_s) *ss++ = 's';
    if (TT.options&FLAG_c) *ss++ = 'c';
    *ss = 0;
  } else if (cc == '?') s = xmprintf("%d", toys.exitval);
  else if (cc == '$') s = xmprintf("%d", TT.pid);
  else if (cc == '#') s = xmprintf("%d", TT.ff->arg.c ? TT.ff->arg.c-1 : 0);
  else if (cc == '!') s = xmprintf("%d"+2*!TT.bangpid, TT.bangpid);
  else {
    delete = 0;
    for (*used = uu = 0; *used<len && isdigit(str[*used]); ++*used)
      uu = (10*uu)+str[*used]-'0';
    if (*used) {
      if (uu) uu += TT.ff->shift;
      if (uu<TT.ff->arg.c) s = TT.ff->arg.v[uu];
    } else if ((*used = varend(str)-str)) return getvar(str);
  }
  if (s) push_arg(delete, s);

  return s;
}

#define WILD_SHORT 1 // else longest match
#define WILD_CASE  2 // case insensitive
#define WILD_ANY   4 // advance through pattern instead of str
// Returns length of str matched by pattern, or -1 if not all pattern consumed
static int wildcard_matchlen(char *str, int len, char *pattern, int plen,
  struct sh_arg *deck, int flags)
{
  struct sh_arg ant = {0};    // stack: of str offsets
  long ss, pp, dd, best = -1;
  int i, j, c, not;

  // Loop through wildcards in pattern.
  for (ss = pp = dd = 0; ;) {
    if ((flags&WILD_ANY) && best!=-1) break;

    // did we consume pattern?
    if (pp==plen) {
      if (ss>best) best = ss;
      if (ss==len || (flags&WILD_SHORT)) break;
    // attempt literal match?
    } else if (dd>=deck->c || pp!=(long)deck->v[dd]) {
      if (ss<len) {
        if (flags&WILD_CASE) {
          ss += getutf8(str+ss, len-ss, &c);
          c = towupper(c);
          pp += getutf8(pattern+pp, pp-plen, &i);
          i = towupper(i);
        } else c = str[ss++], i = pattern[pp++];
        if (c==i) continue;
      }

    // Wildcard chars: |+@!*?()[]
    } else {
      c = pattern[pp++];
      dd++;
      if (c=='?' || ((flags&WILD_ANY) && c=='*')) {
        ss += (i = getutf8(str+ss, len-ss, 0));
        if (i) continue;
      } else if (c=='*') {

        // start with zero length match, don't record consecutive **
        if (dd==1 || pp-2!=(long)deck->v[dd-1] || pattern[pp-2]!='*') {
          arg_add(&ant, (void *)ss);
          arg_add(&ant, 0);
        }

        continue;
      } else if (c == '[') {
        pp += (not = !!strchr("!^", pattern[pp]));
        ss += getutf8(str+ss, len-ss, &c);
        for (i = 0; pp<(long)deck->v[dd]; i = 0) {
          pp += getutf8(pattern+pp, plen-pp, &i);
          if (pattern[pp]=='-') {
            ++pp;
            pp += getutf8(pattern+pp, plen-pp, &j);
            if (not^(i<=c && j>=c)) break;
          } else if (not^(i==c)) break;
        }
        if (i) {
          pp = 1+(long)deck->v[dd++];

          continue;
        }

      // ( preceded by +@!*?

      } else { // TODO ( ) |
        dd++;
        continue;
      }
    }

    // match failure
    if (flags&WILD_ANY) {
      ss = 0;
      if (plen==pp) break;
      continue;
    }

    // pop retry stack or return failure (TODO: seek to next | in paren)
    while (ant.c) {
      if ((c = pattern[(long)deck->v[--dd]])=='*') {
        if (len<(ss = (long)ant.v[ant.c-2]+(long)++ant.v[ant.c-1])) ant.c -= 2;
        else {
          pp = (long)deck->v[dd++]+1;
          break;
        }
      } else if (c == '(') dprintf(2, "TODO: (");
    }

    if (!ant.c) break;
  }
  free (ant.v);

  return best;
}

static int wildcard_match(char *s, char *p, struct sh_arg *deck, int flags)
{
  return wildcard_matchlen(s, strlen(s), p, strlen(p), deck, flags);
}


// TODO: test that * matches ""

// skip to next slash in wildcard path, passing count active ranges.
// start at pattern[off] and deck[*idx], return pattern pos and update *idx
char *wildcard_path(char *pattern, int off, struct sh_arg *deck, int *idx,
  int count)
{
  char *p, *old;
  int i = 0, j = 0;

  // Skip [] and nested () ranges within deck until / or NUL
  for (p = old = pattern+off;; p++) {
    if (!*p) return p;
    while (*p=='/') {
      old = p++;
      if (j && !count) return old;
      j = 0;
    }

    // Got wildcard? Return start of name if out of count, else skip [] ()
    if (*idx<deck->c && p-pattern == (long)deck->v[*idx]) {
      if (!j++ && !count--) return old;
      ++*idx;
      if (*p=='[') p = pattern+(long)deck->v[(*idx)++];
      else if (*p=='(') while (*++p) if (p-pattern == (long)deck->v[*idx]) {
        ++*idx;
        if (*p == ')') {
          if (!i) break;
          i--;
        } else if (*p == '(') i++;
      }
    }
  }
}

// TODO ** means this directory as well as ones below it, shopt -s globstar

// Filesystem traversal callback
// pass on: filename, portion of deck, portion of pattern,
// input: pattern+offset, deck+offset. Need to update offsets.
int do_wildcard_files(struct dirtree *node)
{
  struct dirtree *nn;
  char *pattern, *patend;
  int lvl, ll = 0, ii = 0, rc;
  struct sh_arg ant;

  // Top level entry has no pattern in it
  if (!node->parent) return DIRTREE_RECURSE;

  // Find active pattern range
  for (nn = node->parent; nn; nn = nn->parent) if (nn->parent) ii++;
  pattern = wildcard_path(TT.wcpat, 0, TT.wcdeck, &ll, ii);
  while (*pattern=='/') pattern++;
  lvl = ll;
  patend = wildcard_path(TT.wcpat, pattern-TT.wcpat, TT.wcdeck, &ll, 1);

  // Don't include . entries unless explicitly asked for them
  if (*node->name=='.' && *pattern!='.') return 0;

  // Don't descend into non-directory (was called with DIRTREE_SYMFOLLOW)
  if (*patend && !S_ISDIR(node->st.st_mode) && *node->name) return 0;

  // match this filename from pattern to p in deck from lvl to ll
  ant.c = ll-lvl;
  ant.v = TT.wcdeck->v+lvl;
  for (ii = 0; ii<ant.c; ii++) TT.wcdeck->v[lvl+ii] -= pattern-TT.wcpat;
  rc = wildcard_matchlen(node->name, strlen(node->name), pattern,
    patend-pattern, &ant, 0);
  for (ii = 0; ii<ant.c; ii++) TT.wcdeck->v[lvl+ii] += pattern-TT.wcpat;

  // Return failure or save exact match.
  if (rc<0 || node->name[rc]) return 0;
  if (!*patend) return DIRTREE_SAVE;

  // Are there more wildcards to test children against?
  if (TT.wcdeck->c!=ll) return DIRTREE_RECURSE;

  // No more wildcards: check for child and return failure if it isn't there.
  pattern = xmprintf("%s%s", node->name, patend);
  rc = faccessat(dirtree_parentfd(node), pattern, F_OK, AT_SYMLINK_NOFOLLOW);
  free(pattern);
  if (rc) return 0;

  // Save child and self. (Child could be trailing / but only one saved.)
  while (*patend=='/' && patend[1]) patend++;
  node->child = xzalloc(sizeof(struct dirtree)+1+strlen(patend));
  node->child->parent = node;
  strcpy(node->child->name, patend);

  return DIRTREE_SAVE;
}

// Record active wildcard chars in output string
// *new start of string, oo offset into string, deck is found wildcards,
static void collect_wildcards(char *new, long oo, struct sh_arg *deck)
{
  long bracket, *vv;
  char cc = new[oo];

  // Record unescaped/unquoted wildcard metadata for later processing

  if (!deck->c) arg_add(deck, 0);
  vv = (long *)deck->v;

  // vv[0] used for paren level (bottom 16 bits) + bracket start offset<<16

  // at end loop backwards through live wildcards to remove pending unmatched (
  if (!cc) {
    long ii = 0, jj = 65535&*vv, kk;

    for (kk = deck->c; jj;) {
      if (')' == (cc = new[vv[--kk]])) ii++;
      else if ('(' == cc) {
        if (ii) ii--;
        else {
          memmove(vv+kk, vv+kk+1, sizeof(long)*(deck->c-- -kk));
          jj--;
        }
      }
    }
    if (deck->c) memmove(vv, vv+1, sizeof(long)*deck->c--);

    return;
  }

  // Start +( range, or remove first char that isn't wildcard without (
  if (deck->c>1 && vv[deck->c-1] == oo-1 && strchr("+@!*?", new[oo-1])) {
    if (cc == '(') {
      vv[deck->c-1] = oo;
      return;
    } else if (!strchr("*?", new[oo-1])) deck->c--;
  }

  // fall through to add wildcard, popping parentheses stack as necessary
  if (strchr("|+@!*?", cc));
  else if (cc == ')' && (65535&*vv)) --*vv;

  // complete [range], discard wildcards within, add [, fall through to add ]
  else if (cc == ']' && (bracket = *vv>>16)) {

    // don't end range yet for [] or [^]
    if (bracket+1 == oo || (bracket+2 == oo && strchr("!^", new[oo-1]))) return;
    while (deck->c>1 && vv[deck->c-1]>=bracket) deck->c--;
    *vv &= 65535;
    arg_add(deck, (void *)bracket);

  // Not a wildcard
  } else {
    // [ is speculative, don't add to deck yet, just record we saw it
    if (cc == '[' && !(*vv>>16)) *vv = (oo<<16)+(65535&*vv);
    return;
  }

  // add active wildcard location
  arg_add(deck, (void *)oo);
}

// wildcard expand data against filesystem, and add results to arg list
// Note: this wildcard deck has extra argument at start (leftover from parsing)
static void wildcard_add_files(struct sh_arg *arg, char *pattern,
  struct sh_arg *deck, struct arg_list **delete)
{
  struct dirtree *dt;
  char *pp;
  int ll = 0;

  // fast path: when no wildcards, add pattern verbatim
  collect_wildcards("", 0, deck);
  if (!deck->c) return arg_add(arg, pattern);

  // Traverse starting with leading patternless path.
  pp = wildcard_path(TT.wcpat = pattern, 0, TT.wcdeck = deck, &ll, 0);
  pp = (pp==pattern) ? 0 : xstrndup(pattern, pp-pattern);
  dt = dirtree_flagread(pp, DIRTREE_STATLESS|DIRTREE_SYMFOLLOW,
    do_wildcard_files);
  free(pp);
  deck->c = 0;

  // If no match save pattern, else free tree saving each path found.
  if (!dt) return arg_add(arg, pattern);
  while (dt) {
    while (dt->child) dt = dt->child;
    arg_add(arg, push_arg(delete, dirtree_path(dt, 0)));
    do {
      pp = (void *)dt;
      if ((dt = dt->parent)) dt->child = dt->child->next;
      free(pp);
    } while (dt && !dt->child);
  }
// TODO: test .*/../
}

// Copy string until } including escaped }
// if deck collect wildcards, and store terminator at deck->v[deck->c]
char *slashcopy(char *s, char *c, struct sh_arg *deck)
{
  char *ss;
  long ii, jj;

  for (ii = 0; !strchr(c, s[ii]); ii++) if (s[ii] == '\\') ii++;
  ss = xmalloc(ii+1);
  for (ii = jj = 0; !strchr(c, s[jj]); ii++)
    if ('\\'==(ss[ii] = s[jj++])) ss[ii] = s[jj++];
    else if (deck) collect_wildcards(ss, ii, deck);
  ss[ii] = 0;
  if (deck) {
    arg_add(deck, 0);
    deck->v[--deck->c] = (void *)jj;
    collect_wildcards("", 0, deck);
  }

  return ss;
}

#define NO_QUOTE (1<<0)    // quote removal
#define NO_PATH  (1<<1)    // path expansion (wildcards)
#define NO_SPLIT (1<<2)    // word splitting
#define NO_BRACE (1<<3)    // {brace,expansion}
#define NO_TILDE (1<<4)    // ~username/path
#define NO_NULL  (1<<5)    // Expand to "" instead of NULL
#define SEMI_IFS (1<<6)    // Use ' ' instead of IFS to combine $*
// expand str appending to arg using above flag defines, add mallocs to delete
// if ant not null, save wildcard deck there instead of expanding vs filesystem
// returns 0 for success, 1 for error.
// If measure stop at *measure and return input bytes consumed in *measure
static int expand_arg_nobrace(struct sh_arg *arg, char *str, unsigned flags,
  struct arg_list **delete, struct sh_arg *ant, long *measure)
{
  char cc, qq = flags&NO_QUOTE, sep[6], *new = str, *s, *ss = ss, *ifs, *slice;
  int ii = 0, oo = 0, xx, yy, dd, jj, kk, ll, mm;
  struct sh_arg deck = {0};

  // Tilde expansion
  if (!(flags&NO_TILDE) && *str == '~') {
    struct passwd *pw = 0;

    ss = 0;
    while (str[ii] && str[ii]!=':' && str[ii]!='/') ii++;
    if (ii==1) {
      if (!(ss = getvar("HOME")) || !*ss) pw = bufgetpwuid(getuid());
    } else {
      // TODO bufgetpwnam
      pw = getpwnam(s = xstrndup(str+1, ii-1));
      free(s);
    }
    if (pw) {
      ss = pw->pw_dir;
      if (!ss || !*ss) ss = "/";
    }
    if (ss) {
      oo = strlen(ss);
      s = xmprintf("%s%s", ss, str+ii);
      if (str != new) free(new);
      new = s;
    }
  }

  // parameter/variable expansion and dequoting
  if (!ant) ant = &deck;
  for (; (cc = str[ii++]); str!=new && (new[oo] = 0)) {
    struct sh_arg aa = {0};
    int nosplit = 0;

    if (measure && cc==*measure) break;

    // skip literal chars
    if (!strchr("'\"\\$`"+2*(flags&NO_QUOTE), cc)) {
      if (str != new) new[oo] = cc;
      if (!(flags&NO_PATH) && !(qq&1)) collect_wildcards(new, oo, ant);
      oo++;
      continue;
    }

    // allocate snapshot if we just started modifying
    if (str == new) {
      new = xstrdup(new);
      new[oo] = 0;
    }
    ifs = slice = 0;

    // handle escapes and quoting
    if (cc == '"') qq++;
    else if (cc == '\'') {
      if (qq&1) new[oo++] = cc;
      else {
        qq += 2;
        while ((cc = str[ii++]) != '\'') new[oo++] = cc;
      }

    // both types of subshell work the same, so do $( here not in '$' below
// TODO $((echo hello) | cat) ala $(( becomes $( ( retroactively
    } else if (cc == '`' || (cc == '$' && (str[ii]=='(' || str[ii]=='['))) {
      off_t pp = 0;

      s = str+ii-1;
      kk = parse_word(s, 1)-s;
      if (str[ii] == '[' || *toybuf == 255) { // (( parsed together, not (( ) )
        struct sh_arg aa = {0};
        long long ll;

        // Expand $VARS in math string
        ss = str+ii+1+(str[ii]=='(');
        push_arg(delete, ss = xstrndup(ss, kk - (3+2*(str[ii]!='['))));
        expand_arg_nobrace(&aa, ss, NO_PATH|NO_SPLIT, delete, 0, 0);
        s = ss = (aa.v && *aa.v) ? *aa.v : "";
        free(aa.v);

        // Recursively calculate result
        if (!recalculate(&ll, &s, 0) || *s) {
          sherror_msg("bad math: %s @ %ld", ss, (long)(s-ss)+1);
          goto fail;
        }
        ii += kk-1;
        push_arg(delete, ifs = xmprintf("%lld", ll));
      } else {
        // Run subshell and trim trailing newlines
        s += (jj = 1+(cc == '$'));
        ii += --kk;
        kk -= jj;

        // Special case echo $(<input)
        for (ss = s; isspace(*ss); ss++);
        if (*ss != '<') ss = 0;
        else {
          while (isspace(*++ss));
          // Can't return NULL because guaranteed ) context end
          if (!(ll = parse_word(ss, 0)-ss)) ss = 0;
          else {
            jj = ll+(ss-s);
            while (isspace(s[jj])) jj++;
            if (jj != kk) ss = 0;
            else {
              jj = xcreate_stdio(ss = xstrndup(ss, ll), O_RDONLY|WARN_ONLY, 0);
              free(ss);
            }
          }
        }

// TODO what does \ in `` mean? What is echo `printf %s \$x` supposed to do?
        // This has to be async so pipe buffer doesn't fill up
        if (!ss) jj = pipe_subshell(s, kk, 0); // TODO $(true &&) syntax_err()
        if ((ifs = readfd(jj, 0, &pp)))
          for (kk = strlen(ifs); kk && ifs[kk-1]=='\n'; ifs[--kk] = 0);
        close(jj);
      }
    } else if (!str[ii]) new[oo++] = cc;
    else if (cc=='\\') {
      if (str[ii]=='\n') ii++;
      else new[oo++] = (!(qq&1) || strchr("\"\\$`", str[ii])) ? str[ii++] : cc;
    }

    // $VARIABLE expansions

    else if (cc == '$') {
      cc = *(ss = str+ii++);
      if (cc=='\'') {
        for (s = str+ii; *s != '\''; oo += wcrtomb(new+oo, unescape2(&s, 0),0));
        ii = s-str+1;

        continue;
      } else if (cc=='"' && !(qq&1)) {
        qq++;

        continue;
      } else if (cc == '{') {
        // Skip escapes to find }, parse_word() guarantees ${} terminates
        for (cc = *++ss; str[ii] != '}'; ii++) if (str[ii]=='\\') ii++;
        ii++;

        if (cc == '}') ifs = (void *)1;
        else if (strchr("#!", cc)) ss++;
        if (!(jj = varend(ss)-ss)) while (isdigit(ss[jj])) jj++;
        if (!jj && strchr("#$_*", *ss)) jj++;
        // parameter or operator? Maybe not a prefix: ${#-} vs ${#-x}
        if (!jj && strchr("-?@", *ss)) if (ss[++jj]!='}' && ss[-1]!='{') ss--;
        slice = ss+jj;        // start of :operation

        if (!jj) {
          // literal ${#} or ${!} wasn't a prefix
          if (strchr("#!", cc)) ifs = getvar_special(--ss, 1, &kk, delete);
          else ifs = (void *)1;  // unrecognized char ala ${~}
        } else if (ss[-1]=='{'); // not prefix, fall through
        else if (cc == '#') {  // TODO ${#x[@]}
          dd = !!strchr("@*", *ss);  // For ${#@} or ${#*} do normal ${#}
          if (!(ifs = getvar_special(ss-dd, jj, &kk, delete))) {
            if (TT.options&OPT_u) goto barf;
            ifs = "";
          }
          if (!dd) push_arg(delete, ifs = xmprintf("%zu", strlen(ifs)));
        // ${!@} ${!@Q} ${!x} ${!x@} ${!x@Q} ${!x#} ${!x[} ${!x[*]}
        } else if (cc == '!') {  // TODO: ${var[@]} array

          // special case: normal varname followed by @} or *} = prefix list
          if (ss[jj] == '*' || (ss[jj] == '@' && !isalpha(ss[jj+1]))) {
            struct sh_vars **vv = visible_vars();

            for (slice++, kk = 0; vv[kk]; kk++) {
              if (vv[kk]->flags&VAR_WHITEOUT) continue;
              if (!strncmp(s = vv[kk]->str, ss, jj))
                arg_add(&aa, push_arg(delete, s = xstrndup(s, stridx(s, '='))));
            }
            if (aa.c) push_arg(delete, aa.v);
            free(vv);

          // else dereference to get new varname, discarding if none, check err
          } else {
            // First expansion
            if (strchr("@*", *ss)) { // special case ${!*}/${!@}
              expand_arg_nobrace(&aa, "\"$*\"", NO_PATH|NO_SPLIT, delete, 0, 0);
              ifs = *aa.v;
              free(aa.v);
              memset(&aa, 0, sizeof(aa));
              jj = 1;
            } else ifs = getvar_special(ss, jj, &jj, delete);
            slice = ss+jj;

            // Second expansion
            if (!jj) ifs = (void *)1;
            else if (ifs && *(ss = ifs)) {
              if (strchr("@*", cc)) {
                aa.c = TT.ff->arg.c-1;
                aa.v = TT.ff->arg.v+1;
                jj = 1;
              } else ifs = getvar_special(ifs, strlen(ifs), &jj, delete);
              if (ss && ss[jj]) {
                ifs = (void *)1;
                slice = ss+strlen(ss);
              }
            }
          }
        }

        // Substitution error?
        if (ifs == (void *)1) {
barf:
          if (!(((unsigned long)ifs)>>1)) ifs = "bad substitution";
          sherror_msg("%.*s: %s", (int)(slice-ss), ss, ifs); // TODO: show ${}
          goto fail;
        }
      } else jj = 1;

      // Resolve unprefixed variables
      if (strchr("{$", ss[-1])) {
        if (strchr("@*", cc)) {
          aa.c = TT.ff->arg.c-1;
          aa.v = TT.ff->arg.v+1;
        } else {
          ifs = getvar_special(ss, jj, &jj, delete);
          if (!ifs && (TT.options&OPT_u)) goto barf;
          if (!jj) {
            if (ss[-1] == '{') goto barf;
            new[oo++] = '$';
            ii--;
            continue;
          } else if (ss[-1] != '{') ii += jj-1;
        }
      }
    }

    // combine before/ifs/after sections & split words on $IFS in ifs
    // keep oo bytes of str before (already parsed)
    // insert ifs (active for wildcards+splitting)
    // keep str+ii after (still to parse)

    // Fetch separator to glue string back together with
    *sep = 0;
    if (((qq&1) && cc=='*') || (flags&NO_SPLIT)) {
      unsigned wc;

      nosplit++;
      if (flags&SEMI_IFS) strcpy(sep, " ");
// TODO what if separator is bigger? Need to grab 1 column of combining chars
      else if (0<(dd = utf8towc(&wc, TT.ff->ifs, 4)))
        sprintf(sep, "%.*s", dd, TT.ff->ifs);
    }

    // when aa proceed through entries until NULL, else process ifs once
    mm = yy = 0;
    do {
      // get next argument
      if (aa.c) ifs = aa.v[mm++] ? : "";

      // Are we performing surgery on this argument?
      if (slice && *slice != '}') {
        dd = slice[xx = (*slice == ':')];
        if (!ifs || (xx && !*ifs)) {
          if (strchr("-?=", dd)) { // - use default = assign default ? error
            push_arg(delete, ifs = slashcopy(slice+xx+1, "}", 0));
            if (dd == '?' || (dd == '=' &&
              !(setvar(s = xmprintf("%.*s=%s", (int)(slice-ss), ss, ifs)))))
                goto barf; // TODO ? exits past "source" boundary
          }
        } else if (dd == '-'); // NOP when ifs not empty
        // use alternate value
        else if (dd == '+')
          push_arg(delete, ifs = slashcopy(slice+xx+1, "}", 0));
        else if (xx) { // ${x::}
          long long la = 0, lb = LLONG_MAX, lc = 1;

          ss = ++slice;
          nospace(&ss);
          if ((*ss==':' ? 1 : (lc = recalculate(&la, &ss, 0))) && *ss == ':') {
            ss++;
            if (**nospace(&ss)=='}') lb = 0;
            else lc = recalculate(&lb, &ss, 0);
          }
          if (!lc || *ss != '}') {
            // Find ${blah} context for error message
            while (*slice!='$') slice--;
            sherror_msg("bad %.*s @ %ld", (int)(strchr(ss, '}')+1-slice), slice,
              (long)(ss-slice));
            goto fail;
          }

          // This isn't quite what bash does, but close enough.
          if (!(lc = aa.c)) lc = strlen(ifs);
          else if (!la && !yy && strchr("@*", *slice)) {
            aa.v--; // ${*:0} shows $0 even though default is 1-indexed
            aa.c++;
            yy++;
          }
          if (la<0 && (la += lc)<0) continue;
          if (lb<0) lb = lc+lb-la;
          if (aa.c) {
            if (mm<la || mm>=la+lb) continue;
          } else if (la>=lc || lb<0) ifs = "";
          else if (la+lb>=lc) ifs += la;
          else if (!*delete || ifs != (*delete)->arg)
            push_arg(delete, ifs = xmprintf("%.*s", (int)lb, ifs+la));
          else {
            for (dd = 0; dd<lb ; dd++) if (!(ifs[dd] = ifs[dd+la])) break;
            ifs[dd] = 0;
          }
        } else if (strchr("#%^,", *slice)) {
          struct sh_arg wild = {0};
          char buf[8];

          s = slashcopy(slice+(xx = slice[1]==*slice)+1, "}", &wild);

          // ${x^pat} ${x^^pat} uppercase ${x,} ${x,,} lowercase (no pat = ?)
          if (strchr("^,", *slice)) {
            for (ss = ifs; *ss; ss += dd) {
              dd = getutf8(ss, 4, &jj);
              if (!*s || 0<wildcard_match(ss, s, &wild, WILD_ANY)) {
                ll = ((*slice=='^') ? towupper : towlower)(jj);

                // Of COURSE unicode case switch can change utf8 encoding length
                // Lower case U+0069 becomes u+0130 in turkish.
                // Greek U+0390 becomes 3 characters TODO test this
                if (ll != jj) {
                  yy = ss-ifs;
                  if (!*delete || (*delete)->arg!=ifs)
                    push_arg(delete, ifs = xstrdup(ifs));
                  if (dd != (ll = wctoutf8(buf, ll))) {
                    if (dd<ll)
                      ifs = (*delete)->arg = xrealloc(ifs, strlen(ifs)+1+dd-ll);
                    memmove(ifs+yy+dd-ll, ifs+yy+ll, strlen(ifs+yy+ll)+1);
                  }
                  memcpy(ss = ifs+yy, buf, dd = ll);
                }
              }
              if (!xx) break;
            }
          // ${x#y} remove shortest prefix ${x##y} remove longest prefix
          } else if (*slice=='#') {
            if (0<(dd = wildcard_match(ifs, s, &wild, WILD_SHORT*!xx)))
              ifs += dd;
          // ${x%y} ${x%%y} suffix
          } else if (*slice=='%') {
            for (ss = ifs+strlen(ifs), yy = -1; ss>=ifs; ss--) {
              if (0<(dd = wildcard_match(ss, s, &wild, WILD_SHORT*xx))&&!ss[dd])
              {
                yy = ss-ifs;
                if (!xx) break;
              }
            }

            if (yy != -1) {
              if (delete && *delete && (*delete)->arg==ifs) ifs[yy] = 0;
              else push_arg(delete, ifs = xstrndup(ifs, yy));
            }
          }
          free(s);
          free(wild.v);

        // ${x/pat/sub} substitute ${x//pat/sub} global ${x/#pat/sub} begin
        // ${x/%pat/sub} end ${x/pat} delete pat (x can be @ or *)
        } else if (*slice=='/') {
          struct sh_arg wild = {0};

          xx = !!strchr("/#%", slice[1]);
          s = slashcopy(ss = slice+xx+1, "/}", &wild);
          ss += (long)wild.v[wild.c];
          ss = (*ss == '/') ? slashcopy(ss+1, "}", 0) : 0;
          jj = ss ? strlen(ss) : 0;
          for (ll = 0; ifs[ll];) {
            // TODO nocasematch option
            if (0<(dd = wildcard_match(ifs+ll, s, &wild, 0))) {
              char *bird = 0;

              if (slice[1]=='%' && ifs[ll+dd]) {
                ll++;
                continue;
              }
              if (delete && *delete && (*delete)->arg==ifs) {
                if (jj==dd) memcpy(ifs+ll, ss, jj);
                else if (jj<dd) sprintf(ifs+ll, "%s%s", ss, ifs+ll+dd);
                else bird = ifs;
              } else bird = (void *)1;
              if (bird) {
                ifs = xmprintf("%.*s%s%s", ll, ifs, ss ? : "", ifs+ll+dd);
                if (bird != (void *)1) {
                  free(bird);
                  (*delete)->arg = ifs;
                } else push_arg(delete, ifs);
              }
              if (slice[1]!='/') break;
            } else ll++;
            if (slice[1]=='#') break;
          }

// ${x@QEPAa} Q=$'blah' E=blah without the $'' wrap, P=expand as $PS1
//   A=declare that recreates var a=attribute flags
//   x can be @*
//      } else if (*slice=='@') {

// TODO test x can be @ or *
        } else {
// TODO test ${-abc} as error
          ifs = slice;
          goto barf;
        }

// TODO: $((a=42)) can change var, affect lifetime
// must replace ifs AND any previous output arg[] within pointer strlen()
// also x=;echo $x${x:=4}$x
      }

      // Nothing left to do?
      if (!ifs) break;
      if (!*ifs && !qq) continue;

      // loop within current ifs checking region to split words
      do {

        // find end of (split) word
        if ((qq&1) || nosplit) ss = ifs+strlen(ifs);
        else for (ss = ifs; *ss; ss += kk)
          if (utf8chr(ss, TT.ff->ifs, &kk)) break;

        // when no prefix, not splitting, no suffix: use existing memory
        if (!oo && !*ss && !((mm==aa.c) ? str[ii] : nosplit)) {
          if (qq || ss!=ifs) {
            if (!(flags&NO_PATH))
              for (jj = 0; ifs[jj]; jj++) collect_wildcards(ifs, jj, ant);
            wildcard_add_files(arg, ifs, &deck, delete);
          }
          continue;
        }

        // resize allocation and copy next chunk of IFS-free data
        jj = (mm == aa.c) && !*ss;
        new = xrealloc(new, oo + (ss-ifs) + ((nosplit&!jj) ? strlen(sep) : 0) +
                       (jj ? strlen(str+ii) : 0) + 1);
        dd = sprintf(new + oo, "%.*s%s", (int)(ss-ifs), ifs,
          (nosplit&!jj) ? sep : "");
        if (flags&NO_PATH) oo += dd;
        else while (dd--) collect_wildcards(new, oo++, ant);
        if (jj) break;

        // If splitting, keep quoted, non-blank, or non-whitespace separator
        if (!nosplit) {
          if (qq || *new || *ss) {
            push_arg(delete, new = xrealloc(new, strlen(new)+1));
            wildcard_add_files(arg, new, &deck, delete);
            new = xstrdup(str+ii);
          }
          qq &= 1;
          oo = 0;
        }

        // Skip trailing seperator (combining whitespace)
        kk = 0;
        while ((jj = utf8chr(ss, TT.ff->ifs, &ll))) {
          if (!iswspace(jj) && kk++) break;
          ss += ll;
        }
      } while (*(ifs = ss));
    } while (!(mm == aa.c));
  }

// TODO globbing * ? [] +() happens after variable resolution

// TODO test word splitting completely eliminating argument when no non-$IFS data left
// wordexp keeps pattern when no matches

// TODO test NO_SPLIT cares about IFS, see also trailing \n

  // Record result.
  if (*new || qq) {
    if (str != new) push_arg(delete, new);
    wildcard_add_files(arg, new, &deck, delete);
    new = 0;
  }

  // return success after freeing
  arg = 0;

fail:
  if (str != new) free(new);
  free(deck.v);
  if (ant!=&deck && ant->v) collect_wildcards("", 0, ant);
  if (measure) *measure = --ii;

  return !!arg;
}

struct sh_brace {
  struct sh_brace *next, *prev, *stack;
  int active, cnt, idx, commas[];
};

static int brace_end(struct sh_brace *bb)
{
  return bb->commas[(bb->cnt<0 ? 0 : bb->cnt)+1];
}

// expand braces (ala {a,b,c}) and call expand_arg_nobrace() each permutation
static int expand_arg(struct sh_arg *arg, char *old, unsigned flags,
  struct arg_list **delete)
{
  struct sh_brace *bb = 0, *blist = 0, *bstk, *bnext;
  int i, j, k, x;
  char *s, *ss;

  // collect brace spans
  if ((TT.options&OPT_B) && !(flags&NO_BRACE)) for (i = 0; ; i++) {
    // skip quoted/escaped text
    while ((s = parse_word(old+i, 1)) != old+i) i += s-(old+i);

    // start a new span
    if (old[i] == '{') {
      dlist_add_nomalloc((void *)&blist,
        (void *)(bb = xzalloc(sizeof(struct sh_brace)+34*4)));
      bb->commas[0] = i;
    // end of string: abort unfinished spans and end loop
    } else if (!old[i]) {
      for (bb = blist; bb;) {
        if (!bb->active) {
          if (bb==blist) {
            dlist_pop(&blist);
            bb = blist;
          } else dlist_pop(&bb);
        } else bb = (bb->next==blist) ? 0 : bb->next;
      }
      break;
    // no active span?
    } else if (!bb) continue;
    // end current span
    else if (old[i] == '}') {
      bb->active = bb->commas[bb->cnt+1] = i;
      // Is this a .. span?
      j = 1+*bb->commas;
      if (!bb->cnt && i-j>=4) {
        // a..z span? Single digit numbers handled here too. TODO: utf8
        if (old[j+1]=='.' && old[j+2]=='.') {
          bb->commas[2] = old[j];
          bb->commas[3] = old[j+3];
          k = 0;
          if (old[j+4]=='}' ||
            (sscanf(old+j+4, "..%u}%n", bb->commas+4, &k) && k))
              bb->cnt = -1;
        }
        // 3..11 numeric span?
        if (!bb->cnt) {
          for (k=0, j = 1+*bb->commas; k<3; k++, j += x)
            if (!sscanf(old+j, "..%u%n"+2*!k, bb->commas+2+k, &x)) break;
          if (old[j]=='}') bb->cnt = -2;
        }
        // Increment goes in the right direction by at least 1
        if (bb->cnt) {
          if (!bb->commas[4]) bb->commas[4] = 1;
          if ((bb->commas[3]-bb->commas[2]>0) != (bb->commas[4]>0))
            bb->commas[4] *= -1;
        }
      }
      // discard commaless span that wasn't x..y
      if (!bb->cnt) free(dlist_pop((blist==bb) ? &blist : &bb));
      // Set bb to last unfinished brace (if any)
      for (bb = blist ? blist->prev : 0; bb && bb->active;
           bb = (bb==blist) ? 0 : bb->prev);
    // add a comma to current span
    } else if (old[i] == ',') {
      if (bb->cnt && !(bb->cnt&31)) {
        dlist_lpop(&blist);
        dlist_add_nomalloc((void *)&blist,
          (void *)(bb = xrealloc(bb, sizeof(struct sh_brace)+(bb->cnt+34)*4)));
      }
      bb->commas[++bb->cnt] = i;
    }
  }

// TODO NO_SPLIT with braces? (Collate with spaces?)
  // If none, pass on verbatim
  if (!blist) return expand_arg_nobrace(arg, old, flags, delete, 0, 0);

  // enclose entire range in top level brace.
  (bstk = xzalloc(sizeof(struct sh_brace)+8))->commas[1] = strlen(old)+1;
  bstk->commas[0] = -1;

  // loop through each combination
  for (;;) {

    // Brace expansion can't be longer than original string. Keep start to {
    s = ss = xmalloc(bstk->commas[1]);

    // Append output from active braces to string
    for (bb = blist; bb; bb = (bnext == blist) ? 0 : bnext) {

      // If this brace already tip of stack, pop it. (We'll re-add in a moment.)
      if (bstk == bb) bstk = bstk->stack;
      // if bb is within bstk, save prefix text from bstk's "," to bb's "{"
      if (brace_end(bstk)>bb->commas[0]) {
        i = bstk->commas[bstk->idx]+1;
        s = stpncpy(s, old+i, bb->commas[0]-i);
      }
      else bstk = bstk->stack; // bb past bstk so done with old bstk, pop it
      // push self onto stack as active
      bb->stack = bstk;
      bb->active = 1;
      bstk = bnext = bb;

      // Find next active range: skip inactive spans from earlier/later commas
      while ((bnext = (bnext->next==blist) ? 0 : bnext->next)) {

        // past end of this brace (always true for a..b ranges)
        if ((i = bnext->commas[0])>brace_end(bb)) break;

        // in this brace but not this section
        if (i<bb->commas[bb->idx] || i>bb->commas[bb->idx+1]) {
          bnext->active = 0;
          bnext->stack = 0;

        // in this section
        } else break;
      }

      // is next span past this range?
      if (!bnext || bb->cnt<0 || bnext->commas[0]>bb->commas[bb->idx+1]) {

        // output uninterrupted span
        if (bb->cnt<0) {
          k = bb->commas[2]+bb->commas[4]*bb->idx;
          s += sprintf(s, (bb->cnt==-1) ? "\\%c"+!ispunct(k) : "%d", k);
        } else {
          i = bb->commas[bstk->idx]+1;
          s = stpncpy(s, old+i, bb->commas[bb->idx+1]-i);
        }

        // While not sibling, output tail and pop
        while (!bnext || bnext->commas[0]>brace_end(bstk)) {
          if (!(bb = bstk->stack)) break;
          i = brace_end(bstk)+1; // start of span
          j = bb->commas[bb->idx+1]; // enclosing comma span (can't be a..b)

          while (bnext) {
            if (bnext->commas[0]<j) {
              j = bnext->commas[0];// sibling
              break;
            } else if (brace_end(bb)>bnext->commas[0])
              bnext = (bnext->next == blist) ? 0 : bnext->next;
            else break;
          }
          s = stpncpy(s, old+i, j-i);

          // if next is sibling but parent _not_ a sibling, don't pop
          if (bnext && bnext->commas[0]<brace_end(bb)) break;
          bstk = bb;
        }
      }
    }

    // Save result, aborting on expand error
    if (expand_arg_nobrace(arg, push_arg(delete, ss), flags, delete, 0, 0)) {
      llist_traverse(blist, free);

      return 1;
    }

    // increment
    for (bb = blist->prev; bb; bb = (bb == blist) ? 0 : bb->prev) {
      if (!bb->stack) continue;
      else if (bb->cnt<0) {
        if (abs(bb->commas[2]-bb->commas[3]) < abs(++bb->idx*bb->commas[4]))
          bb->idx = 0;
        else break;
      } else if (++bb->idx > bb->cnt) bb->idx = 0;
      else break;
    }

    // if increment went off left edge, done expanding
    if (!bb) break;
  }
  llist_traverse(blist, free);

  return 0;
}

// Expand exactly one arg, returning NULL on error.
static char *expand_one_arg(char *new, unsigned flags)
{
  struct arg_list *del = 0, *dd;
  struct sh_arg arg = {0};
  char *s = 0;

  // TODO: ${var:?error} here?
  if (!expand_arg(&arg, new, flags|NO_PATH|NO_SPLIT, &del))
    if (!(s = *arg.v) && (flags&(SEMI_IFS|NO_NULL))) s = "";

  // Free non-returned allocations.
  while (del) {
    dd = del->next;
    if (del->arg != s) free(del->arg);
    free(del);
    del = dd;
  }
  free(arg.v);

  return s;
}

// TODO |&

// Expand arguments and perform redirections. Return new process object with
// expanded args. This can be called from command or block context.
static struct sh_process *expand_redir(struct sh_arg *arg, int skip, int *urd)
{
  struct sh_process *pp;
  char *s = s, *ss, *sss, *cv = 0;
  int j, to, from, here = 0;

  TT.hfd = 10;
  pp = xzalloc(sizeof(struct sh_process));
  pp->urd = urd;
  pp->raw = arg;
  pp->refcount = 1;

  // When redirecting, copy each displaced filehandle to restore it later.
  // Expand arguments and perform redirections
  for (j = skip; j<arg->c; j++) {
    int saveclose = 0, bad = 0;

    if (!strcmp(s = arg->v[j], "!")) {
      pp->flags ^= PFLAG_NOT;

      continue;
    }

    // Handle <() >() redirectionss
    if ((*s == '<' || *s == '>') && s[1] == '(') {
      int new = pipe_subshell(s+2, strlen(s+2)-1, *s == '>');

      // Grab subshell data
      if (new == -1) {
        pp->exit = 1;

        return pp;
      }
      save_redirect(&pp->urd, -2, new);

      // bash uses /dev/fd/%d which requires /dev/fd to be a symlink to
      // /proc/self/fd so we just produce that directly.
      arg_add_del(&pp->arg, ss = xmprintf("/proc/self/fd/%d", new),&pp->delete);

      continue;
    }

    // Is this a redirect? s = prefix, ss = operator
    ss = skip_redir_prefix(s);
    sss = ss + anystart(ss, (void *)redirectors);
    if (ss == sss) {
      // Nope: save/expand argument and loop
      if (expand_arg(&pp->arg, s, 0, &pp->delete)) {
        pp->exit = 1;

        return pp;
      }
      continue;
    } else if (j+1 >= arg->c) {
      // redirect needs one argument
      s = "\\n";
      break;
    }
    sss = arg->v[++j];

    // It's a redirect: for [to]<from s = start of [to], ss = <, sss = from
    if (isdigit(*s) && ss-s>5) break;

    // expand arguments for everything but HERE docs
    if (strncmp(ss, "<<", 2)) {
      struct sh_arg tmp = {0};

      if (!expand_arg(&tmp, sss, 0, &pp->delete) && tmp.c == 1) sss = *tmp.v;
      else {
        if (tmp.c > 1) sherror_msg("%s: ambiguous redirect", sss);
        s = 0;
      }
      free(tmp.v);
      if (!s) break;
    }

    // Parse the [fd] part of [fd]<name
    to = *ss != '<';
    if (isdigit(*s)) to = atoi(s);
    else if (*s == '{') {
      if (*varend(s+1) != '}') break;
      // when we close a filehandle, we _read_ from {var}, not write to it
      if ((!strcmp(ss, "<&") || !strcmp(ss, ">&")) && !strcmp(sss, "-")) {
        if (!(ss = getvar(s+1))) break;
        to = atoi(ss); // TODO trailing garbage?
        if (save_redirect(&pp->urd, -1, to)) break;
        close(to);

        continue;
      // record high file descriptor in {to}<from environment variable
      } else {
        // we don't save this, it goes in the env var and user can close it.
        if (-1 == (to = next_hfd())) break;
        cv = xmprintf("%.*s=%d", (int)(ss-s-2), s+1, to);
      }
    }

    // HERE documents?
    if (!strncmp(ss, "<<", 2)) {
      char *tmp = xmprintf("%s/sh-XXXXXX", getvar("TMPDIR") ? : "/tmp");
      int i, h, len, zap = (ss[2] == '-'), x = !sss[strcspn(sss, "\\\"'")];

      // store contents in open-but-deleted /tmp file: write then lseek(start)
      if ((from = mkstemp(tmp))>=0) {
        if (unlink(tmp)) bad++;
        else if (ss[2] == '<') { // not stored in arg[here]
          if (!(ss = expand_one_arg(sss, 0))) {
            s = 0;
            break;
          }
          len = strlen(ss);
          if (len != writeall(from, ss, len) || 1 != writeall(from, "\n", 1))
            bad++;
          if (ss != sss) free(ss);
        } else {
          struct sh_arg *hh = arg+ ++here;

          for (i = 0; i<hh->c; i++) {
            sss = ss = hh->v[i];
            while (zap && *ss == '\t') ss++;
// TODO audit this ala man page
            // expand_parameter, commands, and arithmetic
            if (x && !(sss = expand_one_arg(ss, ~SEMI_IFS))) {
              s = 0;
              break;
            }

            h = writeall(from, sss, len = strlen(sss));
            if (ss != sss) free(sss);
            if (len != h) break;
          }
          if (i != hh->c) bad++;
        }
        if (!bad && lseek(from, 0, SEEK_SET)) bad++;
        if (bad) close(from);
      } else bad++;
      free(tmp);
      if (bad) break;

    // from is fd<<2 (new fd to dup2() after vfork()) plus
    // 2 if we should close(from>>2) after dup2(from>>2, to),
    // 1 if we should close but dup for nofork recovery (ala <&2-)

    // Handle file descriptor duplication/close (&> &>> <& >& with number or -)
    // These redirect existing fd so nothing to open()
    } else if (*ss == '&' || ss[1] == '&') {

      // is there an explicit fd?
      for (ss = sss; isdigit(*ss); ss++);
      if (ss-sss>5 || (*ss && (*ss != '-' || ss[1]))) {
        if (*ss=='&') ss++;
        saveclose = 4;
        goto notfd;
      }

      from = (ss==sss) ? to : atoi(sss);
      saveclose = 2-(*ss == '-');
    } else {
notfd:
      // Permissions to open external file with: < > >> <& >& <> >| &>> &>
      if (!strcmp(ss, "<>")) from = O_CREAT|O_RDWR;
      else if (strstr(ss, ">>")) from = O_CREAT|O_APPEND|O_WRONLY;
      else {
        from = (*ss == '<') ? O_RDONLY : O_CREAT|O_WRONLY|O_TRUNC;
        if (!strcmp(ss, ">") && (TT.options&OPT_C)) {
          struct stat st;

          // Not _just_ O_EXCL: > /dev/null allowed
          if (stat(sss, &st) || !S_ISREG(st.st_mode)) from |= O_EXCL;
        }
      }

      // we expect /dev/fd/# and /dev/{stdin,stdout,stderr} to be in /dev

// TODO: /dev/{tcp,udp}/host/port

      // Open the file
      if (-1 == (from = xcreate_stdio(sss, from|WARN_ONLY, 0666))) {
        s = 0;

        break;
      } else if (from==to) saveclose |= 2;
    }

    // perform redirect, saving displaced "to".
    if (save_redirect(&pp->urd, from, to)) bad++;
    // Do we save displaced "to" in env variable instead of undo list?
    if (cv) {
      --*pp->urd;
      if (!setvar(cv)) bad++;
      cv = 0;
    }
    if ((saveclose&1) && save_redirect(&pp->urd, -1, from)) bad++;
    if ((saveclose&4) && save_redirect(&pp->urd, from, 2)) bad++;
    if (!(saveclose&2)) close(from);
    if (bad) break;
  }

  // didn't parse everything?
  if (j != arg->c) {
    if (s) syntax_err(s);
    if (!pp->exit) pp->exit = 1;
    free(cv);
  }

  return pp;
}

// Call binary, or run script via xexec("sh --")
static void sh_exec(char **argv)
{
  char *pp = getvar("PATH" ? : _PATH_DEFPATH), *ss = TT.isexec ? : *argv,
    **sss = 0, **oldenv = environ, **argv2;
  int norecurse = CFG_TOYBOX_NORECURSE || !toys.stacktop || TT.isexec;
  struct string_list *sl = 0;
  struct toy_list *tl = 0;

  if (getpid() != TT.pid) signal(SIGINT, SIG_DFL); // TODO: restore all?
  errno = ENOENT;
  if (strchr(ss, '/')) {
    if (access(ss, X_OK)) ss = 0;
  } else if (norecurse || !(tl = toy_find(ss)))
    for (sl = find_in_path(pp, ss); sl || (ss = 0); free(llist_pop(&sl)))
      if (!access(ss = sl->str, X_OK)) break;

  if (ss) {
    struct sh_vars **vv = visible_vars();
    struct sh_arg aa;
    unsigned uu, argc;

    // convert vars in-place and use original sh_arg alloc to add one more
    aa.v = environ = (void *)vv;
    for (aa.c = uu = 0; vv[uu]; uu++) {
      if ((vv[uu]->flags&(VAR_WHITEOUT|VAR_EXPORT))==VAR_EXPORT) {
        if (*(pp = vv[uu]->str)=='_' && pp[1]=='=') sss = aa.v+aa.c;
        aa.v[aa.c++] = pp;
      }
    }
    aa.v[aa.c] = 0;
    if (!sss) {
      if (aa.c<uu) aa.v[++aa.c] = 0;
      else arg_add(&aa, 0);
      sss = aa.v+aa.c-1;
    }
    *sss = xmprintf("_=%s", ss);

    // Don't leave open filehandles to scripts in children
    if (!TT.isexec) {
      struct sh_fcall *ff;

      for (ff = TT.ff; ff!=TT.ff->prev; ff = ff->next)
        if (ff->source) fclose(ff->source);
    }

    // Run builtin, exec command, or call shell script without #!
    toy_exec_which(tl, argv);
    execve(ss, argv, environ);
    // shell script without #!
    if (errno == ENOEXEC) {
      for (argc = 0; argv[argc]; argc++);
      argv2 = xmalloc((argc+3)*sizeof(char *));
      memcpy(argv2+3, argv+1, argc*sizeof(char *));
      argv2[0] = "sh";
      argv2[1] = "--";
      argv2[2] = ss;
      xexec(argv2);
      free(argv2);
    }
    environ = oldenv;
    free(*sss);
    free(aa.v);
  }

  perror_msg("%s", *argv);
  if (!TT.isexec) _exit(127);
  llist_traverse(sl, free);
}

// Execute a single command at TT.ff->pl
static struct sh_process *run_command(void)
{
  char *s, *ss, *sss;
  struct sh_arg *arg = TT.ff->pl->arg;
  int envlen, skiplen, funk = TT.funcslen, ii, jj, prefix = 0;
  struct sh_process *pp;

  // Count leading variable assignments
  for (envlen = skiplen = 0; envlen<arg->c; envlen++)
    if ((ss = varend(arg->v[envlen]))==arg->v[envlen] || ss[*ss=='+']!='=')
      break;

  // Was anything left after the assignments?
  if ((s = arg->v[envlen])) {
    // Skip [[ ]] and (( )) contents for now
    if (!smemcmp(s, "((", 2)) skiplen = 1;
    else if (!strcmp(s, "[[")) while (strcmp(arg->v[envlen+skiplen++], "]]"));
  }
  pp = expand_redir(arg, envlen+skiplen, 0);

// TODO: if error stops redir, expansion assignments, prefix assignments,
// what sequence do they occur in?
  // Handle expansions for (( )) and [[ ]]
  if (skiplen) {
    // Trailing redirects can't expand to any contents
    if (pp->arg.c) {
      syntax_err(*pp->arg.v);
      pp->exit = 1;
    }
    if (!pp->exit) {
      for (ii = 0; ii<skiplen; ii++)
// TODO: [[ ~ ] expands but ((~)) doesn't, what else?
        if (expand_arg(&pp->arg, arg->v[envlen+ii], NO_PATH|NO_SPLIT, &pp->delete))
          break;
      if (ii != skiplen) pp->exit = toys.exitval = 1;
    }
    if (pp->exit) return pp;

  // Are we calling a shell function?  TODO binary search
  } else if (pp->arg.c)
    if (!strchr(s, '/')) for (funk = 0; funk<TT.funcslen; funk++)
       if (!strcmp(s, TT.functions[funk]->name)) break;

  // If calling a function, or prefix assignment, or output is piped,
  // create new function context to hold local vars
  (call_function()->pp = pp)->refcount++;
  if (funk!=TT.funcslen || (envlen && pp->arg.c) || TT.ff->blk->pipe) prefix++;
// TODO function needs to run asynchronously in pipeline, and backgrounded

  // perform any assignments
  for (jj = 0; jj<envlen && !pp->exit; jj++) {
    struct sh_vars *vv;

    if ((sss = expand_one_arg(ss = arg->v[jj], SEMI_IFS))) {
      if (!prefix && sss==ss) sss = xstrdup(sss);
      if ((vv = setvar_long(sss, sss!=ss, prefix ? TT.ff : TT.ff->prev))) {
        if (prefix) vv->flags |= VAR_EXPORT;
        continue;
      }
    } else pp->exit = 1;
  }

  // Do the thing
  if (pp->exit) s = 0; // leave $_ alone
  else if (!pp->arg.c || envlen==arg->c) s = ""; // nothing to do but blank $_

// TODO: call functions() FUNCTION
// TODO what about "echo | x=1 | export fruit", must subshell? Test this.
//   Several NOFORK can just NOP in a pipeline? Except ${a?b} still errors

  // ((math))
  else if (!smemcmp(s = *pp->arg.v, "((", 2)) {
    char *ss = s+2;
    long long ll;

    funk = TT.funcslen;
    ii = strlen(s)-2;
    if (!recalculate(&ll, &ss, 0) || ss!=s+ii)
      sherror_msg("bad math: %.*s @ %ld", ii-2, s+2, (long)(ss-s)-2);
    else toys.exitval = !ll;
    pp->exit = toys.exitval;
    s = 0; // Really!

  // call shell function
  } else if (funk != TT.funcslen) {
    s = 0; // $_ set on return, not here
    (TT.ff->function = TT.functions[funk])->refcount++;
    TT.ff->pl = TT.ff->function->pipeline;
    TT.ff->arg = pp->arg;
  // call command from $PATH or toybox builtin
  } else {
    struct toy_list *tl = toy_find(*pp->arg.v);

    jj = tl ? tl->flags : 0;
    s = pp->arg.v[pp->arg.c-1];
//dprintf(2, "%d run command %p %s\n", getpid(), TT.ff, *pp->arg.v); debug_show_fds();
// TODO: figure out when can exec instead of forking, ala sh -c blah

    // Is this command a builtin that should run in this process?
    if ((jj&TOYFLAG_NOFORK) || ((jj&TOYFLAG_MAYFORK) && !prefix)) {
      sigjmp_buf rebound, *prebound = toys.rebound;
      char temp[jj = offsetof(struct toy_context, rebound)];

      // This fakes lots of what toybox_main() does.
      memcpy(&temp, &toys, jj);
      memset(&toys, 0, jj);

      // The compiler complains "declaration does not declare anything" if we
      // name the union in TT, only works WITHOUT name. So we can't
      // sizeof(union) instead offsetof() first thing after union to get size.
      memset(&TT, 0, offsetof(struct sh_data, SECONDS));
      if (!sigsetjmp(rebound, 1)) {
        toys.rebound = &rebound;
//dprintf(2, "%d builtin", getpid()); for (int xx = 0; xx<=pp->arg.c; xx++) dprintf(2, "{%s}", pp->arg.v[xx]); dprintf(2, "\n");
        toy_singleinit(tl, pp->arg.v);
        tl->toy_main();
        xexit();
      }
      toys.rebound = prebound;
      pp->exit = toys.exitval;
      clearerr(stdout);
      if (toys.optargs != toys.argv+1) push_arg(&pp->delete, toys.optargs);
      if (toys.old_umask) umask(toys.old_umask);
      memcpy(&toys, &temp, jj);
    // Run command in new child process
    } else if (-1==(pp->pid = xpopen_setup(pp->arg.v, 0, sh_exec)))
        perror_msg("%s: vfork", *pp->arg.v);
  }

  // cleanup
  // TODO test "_=hello echo thing" sequencing: parent is updated despite local
  if (!TT.ff->source && !TT.ff->pl) {
    if (s && !TT.ff->_) TT.ff->_ = s;
    end_fcall();
  }

  return pp;
}

// if then fi for while until select done done case esac break continue return

// Free one pipeline segment.
static void free_pipeline(void *pipeline)
{
  struct sh_pipeline *pl = pipeline;
  int i, j, k;

  if (!pl) return;

  // free either function or arguments and HERE doc contents
  if (pl->type == 'F') {
    free_function((void *)*pl->arg->v);
    *pl->arg->v = 0;
  }
  for (j=0; j<=pl->count; j++) {
    if (!pl->arg[j].v) continue;
    k = pl->arg[j].c-!!pl->count;
    for (i = 0; i<=k; i++) free(pl->arg[j].v[i]);
    free(pl->arg[j].v);
  }
  free(pl);
}

// Append a new pipeline to function, returning pipeline and pipeline's arg
static struct sh_pipeline *add_pl(struct sh_pipeline **ppl, struct sh_arg **arg)
{
  struct sh_pipeline *pl = xzalloc(sizeof(struct sh_pipeline));

  if (arg) *arg = pl->arg;
  pl->lineno = TT.ff->lineno;
  dlist_add_nomalloc((void *)ppl, (void *)pl);

  return pl->end = pl;
}

// Add a line of shell script to a shell function. Returns 0 if finished,
// 1 to request another line of input (> prompt), -1 for syntax err
static int parse_line(char *line, struct double_list **expect)
{
  char *start = line, *delete = 0, *end, *s, *ex, done = 0,
    *tails[] = {"fi", "done", "esac", "}", "]]", ")", 0};
  struct sh_pipeline *pl = TT.ff->pl ? TT.ff->pl->prev : 0, *pl2, *pl3;
  struct sh_arg *arg = 0;
  long i;

  // Resume appending to last statement?
  if (pl) {
    arg = pl->arg;

    // Extend/resume quoted block
    if (arg->c<0) {
      arg->c = (-arg->c)-1;
      if (start) {
        delete = start = xmprintf("%s%s", arg->v[arg->c], start);
        free(arg->v[arg->c]);
      } else start = arg->v[arg->c];
      arg->v[arg->c] = 0;

    // is a HERE document in progress?
    } else if (pl->count != pl->here) {
here_loop:
      // Back up to oldest unfinished pipeline segment.
      while (pl!=TT.ff->pl && pl->prev->count != pl->prev->here) pl = pl->prev;
      arg = pl->arg+1+pl->here;

      // Match unquoted EOF.
      if (!line) {
        sherror_msg("<<%s EOF", arg->v[arg->c]);
        goto here_end;
      }
      for (s = line, end = arg->v[arg->c]; *end; s++, end++) {
        end += strspn(end, "\\\"'\n");
        if (!*s || *s != *end) break;
      }

      // Add this line, else EOF hit so end HERE document
      if ((*s && *s!='\n') || *end) {
        end = arg->v[arg->c];
        arg_add(arg, xstrdup(line));
        arg->v[arg->c] = end;
      } else {
here_end:
        // End segment and advance/consume bridge segments
        arg->v[arg->c] = 0;
        if (pl->count == ++pl->here)
          while (pl->next!=TT.ff->pl && (pl = pl->next)->here == -1)
            pl->here = pl->count;
      }
      if (pl->here != pl->count) {
        if (!line) goto here_loop;
        else return 1;
      }
      start = 0;

    // Nope, new segment if not self-managing type
    } else if (pl->type < 128) pl = 0;
  }

  // Parse words, assemble argv[] pipelines, check flow control and HERE docs
  if (start) for (;;) {
    ex = *expect ? (*expect)->prev->data : 0;

    // Look for << HERE redirections in completed pipeline segment
    if (pl && pl->count == -1) {
      // find arguments of the form [{n}]<<[-] with another one after it
      for (arg = pl->arg, pl->count = i = 0; i<arg->c; i++) {
        s = skip_redir_prefix(arg->v[i]);
        if (strncmp(s, "<<", 2) || s[2]=='<') continue;
        if (i+1 == arg->c) goto flush;

        // Add another arg[] to the pipeline segment (removing/re-adding
        // to list because realloc can move pointer, and adjusing end pointers)
        dlist_lpop(&TT.ff->pl);
        pl2 = pl;
        pl = xrealloc(pl, sizeof(*pl)+(++pl->count+1)*sizeof(struct sh_arg));
        arg = pl->arg;
        dlist_add_nomalloc((void *)&TT.ff->pl, (void *)pl);
        for (pl3 = TT.ff->pl;;) {
          if (pl3->end == pl2) pl3->end = pl;
          if ((pl3 = pl3->next)==TT.ff->pl) break;
        }

        // queue up HERE EOF so input loop asks for more lines.
        memset(arg+pl->count, 0, sizeof(*arg));
        arg_add(arg+pl->count, arg->v[++i]);
        arg[pl->count].c--;
      }
      // Mark "bridge" segment when previous pl had HERE but this doesn't
      if (!pl->count && pl->prev->count != pl->prev->here) pl->here = -1;
      pl = 0;
    }
    if (done) break;
    s = 0;

    // skip leading whitespace/comment here to know where next word starts
    while (isspace(*start)) ++start;
    if (*start=='#') while (*start) ++start;

    // Parse next word and detect overflow (too many nested quotes).
    if ((end = parse_word(start, 0)) == (void *)1) goto flush;
//dprintf(2, "%d %p(%d) %s word=%.*s\n", getpid(), pl, pl ? pl->type : -1, ex, (int)(end-start), end ? start : "");

    // End function declaration?
    if (pl && pl->type == 'f' && arg->c == 1 && (end-start!=1 || *start!='(')) {
      // end (possibly multiline) function segment, expect function body next
      dlist_add(expect, 0);
      pl = 0;

      continue;
    }

    // Is this a new pipeline segment?
    if (!pl) pl = add_pl(&TT.ff->pl, &arg);

    // Do we need to request another line to finish word (find ending quote)?
    if (!end) {
      // Save unparsed bit of this line, we'll need to re-parse it.
      if (*start=='\\' && (!start[1] || start[1]=='\n')) start++;
      arg_add(arg, xstrndup(start, strlen(start)));
      arg->c = -arg->c;
      free(delete);

      return 1;
    }

    // Ok, we have a word. What does it _mean_?

    // case/esac parsing is weird (unbalanced parentheses!), handle first
    i = ex && !strcmp(ex, "esac") &&
        ((pl->type && pl->type != 3) || (*start==';' && end-start>1));
    if (i) {

      // Premature EOL in type 1 (case x\nin) or 2 (at start or after ;;) is ok
      if (end == start) {
        if (pl->type==128 && arg->c==2) break;  // case x\nin
        if (pl->type==129 && (!arg->c || (arg->c==1 && **arg->v==';'))) break;
        s = "newline";
        goto flush;
      }

      // type 0 means just got ;; so start new type 2
      if (!pl->type || pl->type==3) {
        // catch "echo | ;;" errors
        if (arg->v && arg->v[arg->c] && strcmp(arg->v[arg->c], "&")) goto flush;
        if (!arg->c) {
          if (pl->prev->type == 2) {
            // Add a call to "true" between empty ) ;;
            arg_add(arg, xstrdup(":"));
            pl = add_pl(&TT.ff->pl, &arg);
          }
          pl->type = 129;
        } else {
          // check for here documents
          pl->count = -1;
          continue;
        }
      }

    // Did we hit end of line or ) outside a function declaration?
    // ) is only saved at start of a statement, ends current statement
    } else if (end == start || (arg->c && *start == ')' && pl->type!='f')) {
//TODO: test ) within ]]
      // function () needs both parentheses or neither
      if (pl->type == 'f' && arg->c != 1 && arg->c != 3) {
        s = "function(";
        goto flush;
      }

      // "for" on its own line is an error.
      if (arg->c == 1 && !smemcmp(ex, "do\0A", 4)) {
        s = "newline";
        goto flush;
      }

      // Stop at EOL. Discard blank pipeline segment, else end segment
      if (end == start) done++;
      if (!pl->type && !arg->c) {
        free_pipeline(dlist_lpop(&TT.ff->pl));
        pl = TT.ff->pl ? TT.ff->pl->prev : 0;
      } else pl->count = -1;

      continue;
    }

    // Save word and check for flow control
    arg_add(arg, s = xstrndup(start, end-start));
    start = end;

    // Second half of case/esac parsing
    if (i) {
      // type 1 (128): case x [\n] in
      if (pl->type==128) {
        if (arg->c==2 && strchr("()|;&", *s)) goto flush;
        if (arg->c==3) {
          if (strcmp(s, "in")) goto flush;
          pl->type = 1;
          (pl = add_pl(&TT.ff->pl, &arg))->type = 129;
        }

        continue;

      // type 2 (129): [;;] [(] pattern [|pattern...] )
      } else {

        // can't start with line break or ";;" or "case ? in ;;" without ")"
        if (*s==';') {
          if (arg->c>1 || (arg->c==1 && pl->prev->type==1)) goto flush;
        } else pl->type = 2;
        i = arg->c - (**arg->v==';' && arg->v[0][1]);
        if (i==1 && !strcmp(s, "esac")) {
          // esac right after "in" or ";;" ends block, fall through
          if (arg->c>1) {
            arg->v[1] = 0;
            pl = add_pl(&TT.ff->pl, &arg);
            arg_add(arg, s);
          } else pl->type = 0;
        } else {
          if (arg->c>1) i -= *arg->v[1]=='(';
          if (i>0 && ((i&1)==!!strchr("|)", *s) || strchr(";(", *s)))
            goto flush;
          if (*s=='&' || !strcmp(s, "||")) goto flush;
          if (*s==')') pl = add_pl(&TT.ff->pl, &arg);

          continue;
        }
      }
    }

    // Are we starting a new [function] name [()] definition
    if (!pl->type || pl->type=='f') {
      if (!pl->type && arg->c==1 && !strcmp(s, "function")) {
        free(arg->v[--arg->c]);
        arg->v[arg->c] = 0;
        pl->type = 'f';
        continue;
      } else if (arg->c==2 && !strcmp(s, "(")) pl->type = 'f';
    }

    // one or both of [function] name[()]
    if (pl->type=='f') {
      if (arg->v[0][strcspn(*arg->v, "\"'`><;|&$")]) {
        s = *arg->v;
        goto flush;
      }
      if (arg->c == 2 && strcmp(s, "(")) goto flush;
      if (arg->c == 3) {
        if (strcmp(s, ")")) goto flush;
        dlist_add(expect, 0);
        pl = 0;
      }

      continue;

    // is it a line break token?
    } else if (strchr(";|&", *s) && strncmp(s, "&>", 2)) {
      arg->c--;

      // Connecting nonexistent statements is an error
      if (!arg->c || !smemcmp(ex, "do\0A", 4)) goto flush;

      // treat ; as newline so we don't have to check both elsewhere.
      if (!strcmp(s, ";")) {
        arg->v[arg->c] = 0;
        free(s);
        s = 0;
// TODO can't have ; between "for i" and in or do. (Newline yes, ; no. Why?)
        if (!arg->c && !smemcmp(ex, "do\0C", 4)) continue;

      // ;; and friends only allowed in case statements
      } else if (*s == ';') goto flush;
      pl->count = -1;

      continue;

    // a for/select must have at least one additional argument on same line
    } else if (!smemcmp(ex, "do\0A", 4)) {
      // Sanity check and break the segment
      if (strncmp(s, "((", 2) && *varend(s)) goto flush;
      pl->count = -1;
      (*expect)->prev->data = "do\0C";

      continue;

    // flow control is the first word of a pipeline segment
    } else if (arg->c>1) {
      // Except that [[ ]] is a type 0 segment
      if (ex && *ex==']' && !strcmp(s, ex)) free(dlist_lpop(expect));

      continue;
    }

    // The "test" part of for/select loops can have (at most) one "in" line,
    // for {((;;))|name [in...]} do
    if (!smemcmp(ex, "do\0C", 4)) {
      if (strcmp(s, "do")) {
        // can only have one "in" line between for/do, but not with for(())
        if (pl->prev->type == 's') goto flush;
        if (!strncmp(pl->prev->arg->v[1], "((", 2)) goto flush;
        else if (strcmp(s, "in")) goto flush;
        pl->type = 's';

        continue;
      }
    }

    // start of a new block?

    // for/select/case require var name on same line, can't break segment yet
    if (!strcmp(s, "for") || !strcmp(s, "select") || !strcmp(s, "case")) {
// TODO why !pl->type here
      if (!pl->type) pl->type = (*s == 'c') ? 128 : 1;
      dlist_add(expect, (*s == 'c') ? "esac" : "do\0A");

      continue;
    }

    end = 0;
    if (!strcmp(s, "if")) end = "then";
    else if (!strcmp(s, "while") || !strcmp(s, "until")) end = "do\0B";
    else if (!strcmp(s, "{")) end = "}";
    else if (!strcmp(s, "(")) end = ")";
    else if (!strcmp(s, "[[")) end = "]]";

    // Expecting NULL means any statement (don't care which).
    if (!ex && *expect) {
      if (pl->prev->type == 'f' && !end && smemcmp(s, "((", 2)) goto flush;
      free(dlist_lpop(expect));
    }

    // Did we start a new statement
    if (end) {
      if (*end!=']') pl->type = 1;
      else {
        // [[ ]] is a type 0 segment, not a flow control block
        dlist_add(expect, end);
        continue;
      }

      // Only innermost statement needed in { { { echo ;} ;} ;} and such
      if (*expect && !(*expect)->prev->data) free(dlist_lpop(expect));

    // if not looking for end of statement skip next few tests
    } else if (!ex);

    // If we got here we expect a specific word to end this block: is this it?
    else if (!strcmp(s, ex)) {
      // can't "if | then" or "while && do", only ; & or newline works
      if (strcmp(pl->prev->arg->v[pl->prev->arg->c] ? : "&", "&")) goto flush;

      // consume word, record block end in earlier !0 type (non-nested) blocks
      free(dlist_lpop(expect));
      if (3 == (pl->type = anystr(s, tails) ? 3 : 2)) {
        for (i = 0, pl2 = pl3 = pl; (pl2 = pl2->prev);) {
          if (pl2->type == 3) i++;
          else if (pl2->type) {
            if (!i) {
              if (pl2->type == 2) {
                pl2->end = pl3;
                pl3 = pl2;   // chain multiple gearshifts for case/esac
              } else pl2->end = pl;
            }
            if (pl2->type == 1 && --i<0) break;
          }
        }
      }

      // if it's a multipart block, what comes next?
      if (!strcmp(s, "do")) end = "done";
      else if (!strcmp(s, "then")) end = "fi\0A";

    // fi could have elif, which queues a then.
    } else if (!strcmp(ex, "fi")) {
      if (!strcmp(s, "elif")) {
        free(dlist_lpop(expect));
        end = "then";
      // catch duplicate else while we're here
      } else if (!strcmp(s, "else")) {
        if (ex[3] != 'A') {
          s = "2 else";
          goto flush;
        }
        free(dlist_lpop(expect));
        end = "fi\0B";
      }
    }

    // Queue up the next thing to expect, all preceded by a statement
    if (end) {
      if (!pl->type) pl->type = 2;

      dlist_add(expect, end);
      if (!anystr(end, tails)) dlist_add(expect, 0);
      pl->count = -1;
    }

    // syntax error check: these can't be the first word in an unexpected place
    if (!pl->type && anystr(s, (char *[]){"then", "do", "esac", "}", "]]", ")",
        "done", "fi", "elif", "else", 0})) goto flush;
  }
  free(delete);

  // Return now if line didn't tell us to DO anything.
  if (!TT.ff->pl) return 0;
  pl = TT.ff->pl->prev;

  // return if HERE document pending or more flow control needed to complete
  if (pl->count != pl->here) return 1;
  if (*expect) return 1;
  if (pl->arg->v[pl->arg->c] && strcmp(pl->arg->v[pl->arg->c], "&")) return 1;

  // Transplant completed function bodies into reference counted structures
  for (;;) {
    if (pl->type=='f') {
      struct sh_function *funky;

      // Create sh_function struct, attach to declaration's pipeline segment
      funky = xmalloc(sizeof(struct sh_function));
      funky->refcount = 1;
      funky->name = *pl->arg->v;
      *pl->arg->v = (void *)funky;
      pl->type = 'F'; // different cleanup

      // Transplant function body into new struct, re-circling both lists
      pl2 = pl->next;
      // Add NOP 'f' segment (TODO: remove need for this?)
      (funky->pipeline = add_pl(&pl2, 0))->type = 'f';
      // Find end of block
      for (i = 0, pl3 = pl2->next;;pl3 = pl3->next)
        if (pl3->type == 1) i++;
        else if (pl3->type == 3 && --i<0) break;
      // Chop removed segment out of old list.
      pl3->next->prev = pl;
      pl->next = pl3->next;
      // Terminate removed segment.
      pl2->prev = 0;
      pl3->next = 0;
    }
    if (pl == TT.ff->pl) break;
    pl = pl->prev;
  }

  // Don't need more input, can start executing.

  dlist_terminate(TT.ff->pl);
  return 0;

flush:
  if (s) syntax_err(s);

  return -1;
}

// Find + and - jobs. Returns index of plus, writes minus to *minus
int find_plus_minus(int *minus)
{
  long long when, then;
  int i, plus;

  if (minus) *minus = 0;
  for (then = i = plus = 0; i<TT.jobs.c; i++) {
    if ((when = ((struct sh_process *)TT.jobs.v[i])->when) > then) {
      then = when;
      if (minus) *minus = plus;
      plus = i;
    }
  }

  return plus;
}

char is_plus_minus(int i, int plus, int minus)
{
  return (i == plus) ? '+' : (i == minus) ? '-' : ' ';
}


// We pass in dash to avoid looping over every job each time
char *show_job(struct sh_process *pp, char dash)
{
  char *s = "Run", *buf = 0;
  int i, j, len, len2;

// TODO Terminated (Exited)
  if (pp->exit<0) s = "Stop";
  else if (pp->exit>126) s = "Kill";
  else if (pp->exit>0) s = "Done";
  for (i = len = len2 = 0;; i++) {
    len += snprintf(buf, len2, "[%d]%c  %-6s", pp->job, dash, s);
    for (j = 0; j<pp->raw->c; j++)
      len += snprintf(buf, len2, " %s"+!j, pp->raw->v[j]);
    if (!i) buf = xmalloc(len2 = len+1);
    else break;
  }

  return buf;
}

// Wait for pid to exit and remove from jobs table, returning process or 0.
struct sh_process *wait_job(int pid, int nohang)
{
  struct sh_process *pp = pp;
  int ii, status, minus, plus;

  if (TT.jobs.c<1) return 0;
  for (;;) {
    errno = 0;
    if (1>(pid = waitpid(pid, &status, nohang ? WNOHANG : 0))) {
      if (!nohang && errno==EINTR && !toys.signal) continue;
      return 0;
    }
    for (ii = 0; ii<TT.jobs.c; ii++) {
      pp = (void *)TT.jobs.v[ii];
      if (pp->pid == pid) break;
    }
    if (ii == TT.jobs.c) continue;
    if (pid<1) return 0;
    if (!WIFSTOPPED(status) && !WIFCONTINUED(status)) break;
  }
  plus = find_plus_minus(&minus);
  memmove(TT.jobs.v+ii, TT.jobs.v+ii+1, (TT.jobs.c--)-ii);
  pp->exit = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)+128;
  pp->dash = is_plus_minus(ii, plus, minus);

  return pp;
}

// wait for every process in a pipeline to end
static int wait_pipeline(struct sh_process *pp)
{
  int rc = 0;

  for (dlist_terminate(pp); pp; pp = pp->next) {
    if (pp->pid) {
      // TODO job control: not xwait, handle EINTR ourselves and check signals
      pp->exit = xwaitpid(pp->pid);
      pp->pid = 0;
    }
    // TODO handle set -o pipefail here
    rc = (pp->flags&PFLAG_NOT) ? !pp->exit : pp->exit;
  }

  while ((pp = wait_job(-1, 1)) && (TT.options&FLAG_i)) {
    char *s = show_job(pp, pp->dash);

    dprintf(2, "%s\n", s);
    free(s);
  }

  return rc;
}

// Print prompt to stderr, parsing escapes
// Truncated to 4k at the moment, waiting for somebody to complain.
static void do_prompt(char *prompt)
{
  char *s, *ss, c, cc, *pp = toybuf;
  int len, ll;

  if (!prompt) return;
  while ((len = sizeof(toybuf)-(pp-toybuf))>0 && *prompt) {
    c = *(prompt++);

    if (c=='!') {
      if (*prompt=='!') prompt++;
      else {
        pp += snprintf(pp, len, "%ld", TT.ff->lineno);
        continue;
      }
    } else if (c=='\\') {
      cc = *(prompt++);
      if (!cc) {
        *pp++ = c;
        break;
      }

      // \nnn \dD{}hHjlstT@AuvVwW!#$
      // Ignore bash's "nonprintable" hack; query our cursor position instead.
      if (cc=='[' || cc==']') continue;
      else if (cc=='$') *pp++ = getuid() ? '$' : '#';
      else if (cc=='h' || cc=='H') {
        *pp = 0;
        gethostname(pp, len);
        pp[len-1] = 0;
        if (cc=='h' && (s = strchr(pp, '.'))) *s = 0;
        pp += strlen(pp);
      } else if (cc=='s') {
        s = getbasename(*toys.argv);
        while (*s && len--) *pp++ = *s++;
      } else if (cc=='w') {
        if ((s = getvar("PWD"))) {
          if ((ss = getvar("HOME")) && strstart(&s, ss)) {
            *pp++ = '~';
            if (--len && *s!='/') *pp++ = '/';
            len--;
          }
          if (len>0) {
            ll = strlen(s);
            pp = stpncpy(pp, s, ll>len ? len : ll);
          }
        }
      } else if (!(c = unescape(cc))) {
        *pp++ = '\\';
        if (--len) *pp++ = c;
      } else *pp++ = c;
    } else *pp++ = c;
  }
  len = pp-toybuf;
  if (len>=sizeof(toybuf)) len = sizeof(toybuf);
  writeall(2, toybuf, len);
}

// returns NULL for EOF or error, else null terminated string.
static char *get_next_line(FILE *fp, int prompt)
{
  char *new;
  int len, cc;
  unsigned uu;

  if (!fp) return 0;
  if (prompt>2 || (fp==stdin && (TT.options&FLAG_i))) {
    char ps[16];

    sprintf(ps, "PS%d", prompt);
    do_prompt(getvar(ps));
  }

// TODO what should ctrl-C do? (also in "select")
// TODO line editing/history, should set $COLUMNS $LINES and sigwinch update
//  TODO: after first EINTR returns closed?
// TODO: ctrl-z during script read having already read partial line,
// SIGSTOP and SIGTSTP need SA_RESTART, but child proc should stop
// TODO if (!isspace(*new)) add_to_history(line);
// TODO: embedded nul bytes need signaling for the "tried to run binary" test.

  for (new = 0, len = 0;;) {
    errno = 0;
    if (!(cc = getc(fp))) {
      if (prompt!=1 || TT.ff->lineno) continue;
      cc = 255; // force invalid utf8 sequence detection
    }
    if (cc<0) {
      if (errno == EINTR) continue;
      break;
    }
    if (!(len&63)) new = xrealloc(new, len+65);
    if ((new[len++] = cc) == '\n') break;
  }
  if (!new) return new;
  new[len] = 0;

  // Check for binary file?
  if (prompt<3 && !TT.ff->lineno++ && TT.ff->name) {
    // A shell script's first line has no high bytes that aren't valid utf-8.
    for (len = 0; new[len]>6 && 0<(cc = utf8towc(&uu, new+len, 4)); len += cc);
    if (new[len]) {
      sherror_msg("'%s' is binary", TT.ff->name); // TODO syntax_err() exit?
      free(new);
      new = 0;
    }
  }
//dprintf(2, "%d get_next_line=%s\n", getpid(), new ? : "(null)");
  return new;
}

/*
 TODO: "echo | read i" is backgroundable with ctrl-Z despite read = builtin.
       probably have to inline run_command here to do that? Implicit ()
       also "X=42 | true; echo $X" doesn't get X.
       I.E. run_subshell() here sometimes? (But when?)
 TODO If we just started a new pipeline, implicit parentheses (subshell)
 TODO can't free sh_process delete until ready to dispose else no debug output
 TODO: a | b | c needs subshell for builtins?
        - anything that can produce output
        - echo declare dirs
      (a; b; c) like { } but subshell
      when to auto-exec? ps vs sh -c 'ps' vs sh -c '(ps)'
*/

// run a parsed shell function. Handle flow control blocks and characters,
// setup pipes and block redirection, break/continue, call builtins, functions,
// vfork/exec external commands. Return when out of input.
static void run_lines(void)
{
  char *ctl, *s, *ss, **vv;
  struct sh_process *pp, *pplist = 0; // processes piping into current level
  long i, j, k;

  // iterate through pipeline segments
  for (;;) {
    if (!TT.ff->pl) {
      if (TT.ff->source) break;
      end_fcall();
// TODO can we move advance logic to start of loop to avoid straddle?
      goto advance;
    }

    ctl = TT.ff->pl->end->arg->v[TT.ff->pl->end->arg->c];
    s = *TT.ff->pl->arg->v;
    ss = TT.ff->pl->arg->v[1];
//dprintf(2, "%d s=%s ss=%s ctl=%s type=%d pl=%p ff=%p\n", getpid(), (TT.ff->pl->type == 'F') ? ((struct sh_function *)s)->name : s, ss, ctl, TT.ff->pl->type, TT.ff->pl, TT.ff);
    if (!pplist) TT.hfd = 10;

    // Skip disabled blocks, handle pipes and backgrounding
    if (TT.ff->pl->type<2) {
      if (!TT.ff->blk->run) {
        TT.ff->pl = TT.ff->pl->end->next;

        continue;
      }

      if (TT.options&OPT_x) {
        char *ss, *ps4 = getvar("PS4");
        struct sh_fcall *ff;

        // duplicate first char of ps4 call depth times
        if (ps4 && *ps4) {

          for (ff = TT.ff, i = 0; ff != TT.ff->prev; ff = ff->next)
            if (ff->source && ff->name) i++;
          j = getutf8(ps4, k = strlen(ps4), 0);
          ss = xmalloc(i*j+k+1);
          for (k = 0; k<i; k++) memcpy(ss+k*j, ps4, j);
          strcpy(ss+k*j, ps4+j);
          do_prompt(ss);
          free(ss);

          // TODO resolve variables
          ss = pl2str(TT.ff->pl, 1);
          dprintf(2, "%s\n", ss);
          free(ss);
        }
      }

      // pipe data into and out of this segment, I.E. leading/trailing |
      unredirect(TT.ff->blk->urd);
      TT.ff->blk->urd = 0;
      TT.ff->blk->pipe = 0;

      // Consume pipe from previous segment as stdin.
      if (TT.ff->blk->pout != -1) {
        TT.ff->blk->pipe++;
        if (save_redirect(&TT.ff->blk->urd, TT.ff->blk->pout, 0)) break;
        close(TT.ff->blk->pout);
        TT.ff->blk->pout = -1;
      }

      // Create output pipe and save next process's stdin in pout
      if (ctl && *ctl == '|' && ctl[1] != '|') {
        int pipes[2] = {-1, -1};

        TT.ff->blk->pipe++;
        if (pipe(pipes)) {
          perror_msg("pipe");

          break;
        }
        if (save_redirect(&TT.ff->blk->urd, pipes[1], 1)) {
          close(pipes[0]);
          close(pipes[1]);

          break;
        }
        if (pipes[1] != 1) close(pipes[1]);
        fcntl(TT.ff->blk->pout = *pipes, F_SETFD, FD_CLOEXEC);
        if (ctl[1] == '&') save_redirect(&TT.ff->blk->urd, 1, 2);
      }
    }

    // If executable segment parse and run next command saving resulting process
    if (!TT.ff->pl->type) {
      if ((pp = run_command())) dlist_add_nomalloc((void *)&pplist, (void *)pp);

    // Start of flow control block?
    } else if (TT.ff->pl->type == 1) {

// TODO test cat | {thingy} is new PID: { is ( for |

      // perform/save trailing redirects
      pp = expand_redir(TT.ff->pl->end->arg, 1, TT.ff->blk->urd);
      TT.ff->blk->urd = pp->urd;
      pp->urd = 0;
      if (pp->arg.c) syntax_err(*pp->arg.v);
      llist_traverse(pp->delete, llist_free_arg);
      pp->delete = 0;
      if (pp->exit || pp->arg.c) {
        free(pp);
        toys.exitval = 1;

        break;
      }
      add_block();

// TODO test background a block: { abc; } &

      // If we spawn a subshell, pass data off to child process
      if (TT.ff->blk->next->pipe || !strcmp(s, "(") || (ctl && !strcmp(ctl, "&"))) {
        if (!(pp->pid = run_subshell(0, -1))) {
          // zap forked child's cleanup context and advance to next statement
          pplist = 0;
          while (TT.ff->blk->next) TT.ff->blk = TT.ff->blk->next;
          TT.ff->blk->pout = -1;
          TT.ff->blk->urd = 0;
          TT.ff->pl = TT.ff->next->pl->next;

          continue;
        }
        TT.ff->pl = TT.ff->pl->end;
        pop_block();
        dlist_add_nomalloc((void *)&pplist, (void *)pp);

      // handle start of block in this process
      } else {
        free(pp);

        // What flow control statement is this?

        // {/} if/then/elif/else/fi, while until/do/done - no special handling

        // for/select/do/done: populate blk->farg with expanded args (if any)
        if (!strcmp(s, "for") || !strcmp(s, "select")) {
          if (TT.ff->blk->loop); // skip init, not first time through loop

          // in (;;)
          else if (!strncmp(TT.ff->blk->fvar = ss, "((", 2)) {
            char *in = ss+2, *out;
            long long ll;

            TT.ff->blk->loop = 1;
            for (i = 0; i<3; i++) {
              if (i==2) k = strlen(in)-2;
              else {
                // perform expansion but immediately discard it to find ;
                k = ';';
                pp = xzalloc(sizeof(*pp));
                if (expand_arg_nobrace(&pp->arg, ss+2, NO_PATH|NO_SPLIT,
                    &pp->delete, 0, &k)) break;
                free_process(pp);
                if (in[k] != ';') break;
              }
              (out = xmalloc(k+1))[k] = 0;
              memcpy(out, in, k);
              arg_add(&TT.ff->blk->farg, push_arg(&TT.ff->blk->fdelete, out));
              in += k+1;
            }
            if (i!=3) {
              syntax_err(ss);
              break;
            }
            in = out = *TT.ff->blk->farg.v;
            if (!recalculate(&ll, &in, 0) || *in) {
              sherror_msg("bad math: %s @ %ld", in, (long)(in-out));
              break;
            }

          // in LIST
          } else if (TT.ff->pl->next->type == 's') {
            for (i = 1; i<TT.ff->pl->next->arg->c; i++)
              if (expand_arg(&TT.ff->blk->farg, TT.ff->pl->next->arg->v[i],
                             0, &TT.ff->blk->fdelete)) break;
            if (i != TT.ff->pl->next->arg->c) TT.ff->pl = pop_block();

          // in without LIST. (This expansion can't return error.)
          } else expand_arg(&TT.ff->blk->farg, "\"$@\"", 0,
                            &TT.ff->blk->fdelete);

          // TODO: ls -C style output
          if (*s == 's') for (i = 0; i<TT.ff->blk->farg.c; i++)
            dprintf(2, "%ld) %s\n", i+1, TT.ff->blk->farg.v[i]);

        // TODO: bash man page says it performs <(process substituion) here?!?
        } else if (!strcmp(s, "case")) {
          if (!(TT.ff->blk->fvar = expand_one_arg(ss, NO_NULL))) break;
          if (ss != TT.ff->blk->fvar)
            push_arg(&TT.ff->blk->fdelete, TT.ff->blk->fvar);
        }

// TODO [[/]] ((/)) function/}
      }

    // gearshift from block start to block body (end of flow control test)
    } else if (TT.ff->pl->type == 2) {
      int match, err;

      TT.ff->blk->middle = TT.ff->pl;

      // ;; end, ;& continue through next block, ;;& test next block
      if (!strcmp(*TT.ff->blk->start->arg->v, "case")) {
        if (!strcmp(s, ";;")) {
          while (TT.ff->pl->type!=3) TT.ff->pl = TT.ff->pl->end;
          continue;
        } else if (strcmp(s, ";&")) {
          struct sh_arg arg = {0}, arg2 = {0};

          for (err = 0, vv = 0;;) {
            if (!vv) {
              vv = TT.ff->pl->arg->v + (**TT.ff->pl->arg->v == ';');
              if (!*vv) {
                // TODO syntax err if not type==3, catch above
                TT.ff->pl = TT.ff->pl->next;
                break;
              } else vv += **vv == '(';
            }
            arg.c = arg2.c = 0;
            if ((err = expand_arg_nobrace(&arg, *vv++, NO_SPLIT,
              &TT.ff->blk->fdelete, &arg2, 0))) break;
            s = arg.c ? *arg.v : "";
            match = wildcard_match(TT.ff->blk->fvar, s, &arg2, 0);
            if (match>=0 && !s[match]) break;
            else if (**vv++ == ')') {
              vv = 0;
              if ((TT.ff->pl = TT.ff->pl->end)->type!=2) break;
            }
          }
          free(arg.v);
          free(arg2.v);
          if (err) break;
          if (TT.ff->pl->type==3) continue;
        }

      // Handle if/else/elif statement
      } else if (!strcmp(s, "then")) {
do_then:
        TT.ff->blk->run = TT.ff->blk->run && !toys.exitval;
        toys.exitval = 0;
      } else if (!strcmp(s, "else") || !strcmp(s, "elif"))
        TT.ff->blk->run = !TT.ff->blk->run;

      // Loop
      else if (!strcmp(s, "do")) {
        struct sh_blockstack *blk = TT.ff->blk;

        ss = *blk->start->arg->v;
        if (!strcmp(ss, "while")) goto do_then;
        else if (!strcmp(ss, "until")) {
          blk->run = blk->run && toys.exitval;
          toys.exitval = 0;
        } else if (!strcmp(ss, "select")) {
          if (!(ss = get_next_line(stdin, 3))) {
            TT.ff->pl = pop_block();
            printf("\n");
          } else {
            match = atoi(ss);
            i = *s;
            free(ss);
            if (!i) {
              TT.ff->pl = blk->start;
              continue;
            } else setvarval(blk->fvar, (match<1 || match>blk->farg.c)
                                        ? "" : blk->farg.v[match-1]);
          }
        } else if (blk->loop >= blk->farg.c) TT.ff->pl = pop_block();
        else if (!strncmp(blk->fvar, "((", 2)) {
          char *aa, *bb;
          long long ll;

          for (i = 2; i; i--) {
            if (TT.ff->blk->loop == 1) {
              TT.ff->blk->loop++;
              i--;
            }
            aa = bb = TT.ff->blk->farg.v[i];
            if (!recalculate(&ll, &aa, 0) || *aa) {
              sherror_msg("bad math: %s @ %ld", aa, (long)(aa-bb));
              break;
            }
            if (i==1 && !ll) TT.ff->pl = pop_block();
          }
        } else setvarval(blk->fvar, blk->farg.v[blk->loop++]);
      }

    // end of block
    } else if (TT.ff->pl->type == 3) {
      // If we end a block we're not in, exit subshell
      if (!TT.ff->blk->next) xexit();

      // repeating block?
      if (TT.ff->blk->run && !strcmp(s, "done")) {
        TT.ff->pl = (**TT.ff->blk->start->arg->v == 'w')
          ? TT.ff->blk->start->next : TT.ff->blk->middle;
        continue;
      }

      // cleans up after trailing redirections/pipe
      pop_block();

    // declare a shell function
    } else if (TT.ff->pl->type == 'F') {
      struct sh_function *funky = (void *)*TT.ff->pl->arg->v;

// TODO binary search
      for (i = 0; i<TT.funcslen; i++)
        if (!strcmp(TT.functions[i]->name, funky->name)) break;
      if (i == TT.funcslen) {
        struct sh_arg arg = {(void *)TT.functions, TT.funcslen};

        arg_add(&arg, (void *)funky); // TODO possibly an expand@31 function?
        TT.functions = (void *)arg.v;
        TT.funcslen++;
      } else {
        free_function(TT.functions[i]);
        TT.functions[i] = funky;
      }
      TT.functions[i]->refcount++;
    }

    // Three cases: 1) background & 2) pipeline | 3) last process in pipeline ;
    // If we ran a process and didn't pipe output, background or wait for exit
    if (pplist && TT.ff->blk->pout == -1) {
      if (ctl && !strcmp(ctl, "&")) {
        if (!TT.jobs.c) TT.jobcnt = 0;
        pplist->job = ++TT.jobcnt;
        arg_add(&TT.jobs, (void *)pplist);
        if (TT.options&FLAG_i) dprintf(2, "[%u] %u\n", pplist->job,pplist->pid);
      } else {
        toys.exitval = wait_pipeline(pplist);
        llist_traverse(pplist, (void *)free_process);
      }
      pplist = 0;
    }
advance:
    if (!TT.ff || !TT.ff->pl) break;
    // for && and || skip pipeline segment(s) based on return code
    if (!TT.ff->pl->type || TT.ff->pl->type == 3) {
      for (;;) {
        ctl = TT.ff->pl->arg->v[TT.ff->pl->arg->c];
        if (!ctl || strcmp(ctl, toys.exitval ? "&&" : "||")) break;
        if ((TT.ff->pl = TT.ff->pl->next)->type) TT.ff->pl = TT.ff->pl->end;
      }
    }
    TT.ff->pl = TT.ff->pl->next;
  }

  // clean up any unfinished stuff
  if (pplist) {
    toys.exitval = wait_pipeline(pplist);
    llist_traverse(pplist, (void *)free_process);
  }
}

// set variable
static struct sh_vars *initvar(char *name, char *val)
{
  return addvar(xmprintf("%s=%s", name, val ? : ""), TT.ff);
}

static struct sh_vars *initvardef(char *name, char *val, char *def)
{
  return initvar(name, (!val || !*val) ? def : val);
}

// export existing "name" or assign/export name=value string (making new copy)
static void set_varflags(char *str, unsigned set_flags, unsigned unset_flags)
{
  struct sh_vars *shv = 0;
  struct sh_fcall *ff;
  char *s;

  // Make sure variable exists and is updated
  if (strchr(str, '=')) shv = setvar(xstrdup(str));
  else if (!(shv = findvar(str, &ff))) {
    if (!set_flags) return;
    shv = addvar(str = xmprintf("%s=", str), TT.ff->prev);
    shv->flags = VAR_WHITEOUT;
  } else if (shv->flags&VAR_WHITEOUT) shv->flags |= VAR_EXPORT;
  if (!shv || (shv->flags&VAR_EXPORT)) return;

  // Resolve magic for export (bash bug compatibility, really should be dynamic)
  if (shv->flags&VAR_MAGIC) {
    s = shv->str;
    shv->str = xmprintf("%.*s=%s", (int)(varend(str)-str), str, getvar(str));
    if (!(shv->flags&VAR_NOFREE)) free(s);
    else shv->flags ^= VAR_NOFREE;
  }
  shv->flags |= set_flags;
  shv->flags &= ~unset_flags;
}

static void export(char *str)
{
  set_varflags(str, VAR_EXPORT, 0);
}

FILE *fpathopen(char *name)
{
  int fd = open(name, O_RDONLY|O_CLOEXEC), ii;
  struct string_list *sl = 0;
  char *pp = getvar("PATH") ? : _PATH_DEFPATH;

  if (fd==-1) {
    for (sl = find_in_path(pp, name); sl; free(llist_pop(&sl)))
      if (-1==(fd = open(sl->str, O_RDONLY|O_CLOEXEC))) break;
    if (sl) llist_traverse(sl, free);
  }
  if (fd != -1) {
    dup2(fd, ii = next_hfd());
    fcntl(ii, F_SETFD, FD_CLOEXEC);
    close(fd);
    fd = ii;
  }

  return fd==-1 ? 0 : fdopen(fd, "r");
}

// On nommu we had to exec(), so parent environment is passed via a pipe.
static void nommu_reentry(void)
{
  struct stat st;
  int ii, pid, ppid, len;
  unsigned long ll;
  char *s = 0;

  // Sanity check
  if (!fstat(254, &st) && S_ISFIFO(st.st_mode)) {
    for (ii = len = 0; (s = environ[ii]); ii++) {
      if (*s!='@') continue;
      sscanf(s, "@%u,%u%n", &pid, &ppid, &len);
      break;
    }
  }
  if (!s || s[len] || pid!=getpid() || ppid!=getppid()) error_exit(0);

  // NOMMU subshell commands come from pipe from parent
  TT.ff->source = fdopen(254, "r");

  // But first, we have to marshall context across the pipe into child

// TODO signal setup before this so fscanf can't EINTR.
// TODO marshall TT.jobcnt TT.funcslen: child needs jobs and function list
// TODO marshall functions (including signal handlers?)
// TODO test: call function from subshell, send signal to subshell/background

  // Marshall magics: $SECONDS $- $LINENO $$ $!
  if (5!=fscanf(TT.ff->source, "%lld %u %ld %u %u%*[^\n]", &TT.SECONDS,
      &TT.options, &TT.ff->lineno, &TT.pid, &TT.bangpid)) error_exit(0);

  // Read named variables: type, len, var=value\0
  for (;;) {
    len = ll = 0;
    (void)fscanf(TT.ff->source, "%u %lu%*[^\n]", &len, &ll);
    fgetc(TT.ff->source); // Discard the newline fscanf didn't eat.
    if (!len) break;
    (s = xmalloc(len+1))[len] = 0;
    for (ii = 0; ii<len; ii += pid)
      if (1>(pid = fread(s+ii, 1, len-ii, TT.ff->source))) error_exit(0);
    set_varflags(s, ll, 0);
  }
}

// init locals, sanitize environment, handle nommu subshell handoff
static void subshell_setup(void)
{
  int ii, from, uid = getuid();
  struct passwd *pw = getpwuid(uid);
  char *s, *ss, *magic[] = {"SECONDS", "RANDOM", "LINENO", "GROUPS", "BASHPID",
    "EPOCHREALTIME", "EPOCHSECONDS"},
    *readonly[] = {xmprintf("EUID=%d", geteuid()), xmprintf("UID=%d", uid),
                   xmprintf("PPID=%d", getppid())};
  struct sh_vars *shv;
  struct utsname uu;

  // Find input source
  if (TT.sh.c) {
    TT.ff->source = fmemopen(TT.sh.c, strlen(TT.sh.c), "r");
    TT.ff->name = "-c";
  } else if (TT.options&FLAG_s) TT.ff->source = stdin;
  else if (!(TT.ff->source = fpathopen(TT.ff->name = *toys.optargs)))
    perror_exit_raw(*toys.optargs);

  // Add additional input sources (in reverse order so they pop off stack right)

  // /etc/profile, ~/.bashrc...

  // Initialize magic and read only local variables
  for (ii = 0; ii<ARRAY_LEN(magic) && (s = magic[ii]); ii++)
    initvar(s, "")->flags = VAR_MAGIC+VAR_INT*('G'!=*s)+VAR_READONLY*('B'==*s);
  for (ii = 0; ii<ARRAY_LEN(readonly); ii++)
    addvar(readonly[ii], TT.ff)->flags = VAR_READONLY|VAR_INT;

  // Add local variables that can be overwritten
  initvar("PATH", _PATH_DEFPATH);
  if (!pw) pw = (void *)toybuf; // first use, so still zeroed
  sprintf(toybuf+1024, "%u", uid);
  initvardef("HOME", pw->pw_dir, "/");
  initvardef("SHELL", pw->pw_shell, "/bin/sh");
  initvardef("USER", pw->pw_name, toybuf+1024);
  initvardef("LOGNAME", pw->pw_name, toybuf+1024);
  gethostname(toybuf, sizeof(toybuf)-1);
  initvar("HOSTNAME", toybuf);
  uname(&uu);
  initvar("HOSTTYPE", uu.machine);
  sprintf(toybuf, "%s-unknown-linux", uu.machine);
  initvar("MACHTYPE", toybuf);
  initvar("OSTYPE", uu.sysname);
  // sprintf(toybuf, "%s-toybox", TOYBOX_VERSION);
  // initvar("BASH_VERSION", toybuf); TODO
  initvar("OPTERR", "1"); // TODO: test if already exported?
  if (readlink0("/proc/self/exe", s = toybuf, sizeof(toybuf))||(s=getenv("_")))
    initvar("BASH", s);
  initvar("PS2", "> ");
  initvar("PS3", "#? ");
  initvar("PS4", "+ ");

  // Ensure environ copied and toys.envc set, and clean out illegal entries
  for (from = 0; (s = environ[from]); from++) {
    if (*varend(s) != '=') continue;
    if (!(shv = findvar(s, 0))) addvar(s, TT.ff)->flags = VAR_EXPORT|VAR_NOFREE;
    else if (shv->flags&VAR_READONLY) continue;
    else {
      if (!(shv->flags&VAR_NOFREE)) {
        free(shv->str);
        shv->flags ^= VAR_NOFREE;
      }
      shv->flags |= VAR_EXPORT;
      shv->str = s;
    }
    cache_ifs(s, TT.ff); // TODO: replace with set(get("IFS")) after loop
  }

  // set _ to path to this shell
  s = toys.argv[0];
  ss = 0;
  if (!strchr(s, '/')) {
    if ((ss = getcwd(0, 0))) {
      s = xmprintf("%s/%s", ss, s);
      free(ss);
      ss = s;
    } else if (*toybuf) s = toybuf; // from /proc/self/exe
  }
  setvarval("_", s)->flags |= VAR_EXPORT;
  free(ss);

  // TODO: this is in pipe, not environment
  if (!(ss = getvar("SHLVL"))) export("SHLVL=1"); // Bash 5.0
  else {
    char buf[16];

    sprintf(buf, "%u", atoi(ss+6)+1);
    setvarval("SHLVL", buf)->flags |= VAR_EXPORT;
  }
  if (TT.options&FLAG_i) {
    if (!getvar("PS1")) setvarval("PS1", "$ "); // "\\s-\\v$ "
    // TODO Set up signal handlers and grab control of this tty.
    // ^C SIGINT ^\ SIGQUIT ^Z SIGTSTP SIGTTIN SIGTTOU SIGCHLD
    // setsid(), setpgid(), tcsetpgrp()...
    xsignal(SIGINT, SIG_IGN);
  }

  // Add additional input sources (in reverse order so they pop off stack right)

  // /etc/profile, ~/.bashrc...

  // set/update PWD, but don't let it overwrite $_
  call_function()->source = fmemopen("cd .", 4, "r");
  addvar("_=", TT.ff)->flags = VAR_NOFREE;
}

void sh_main(void)
{
// TODO should expect also move into TT.ff like pl did
  struct double_list *expect = 0;
  char *new;
  unsigned more = 0;

//dprintf(2, "%d main", getpid()); for (unsigned uu = 0; toys.argv[uu]; uu++) dprintf(2, " %s", toys.argv[uu]); dprintf(2, "\n");

  signal(SIGPIPE, SIG_IGN);
  TT.options = OPT_B;
  TT.pid = getpid();
  srandom(TT.SECONDS = millitime());

  // TODO euid stuff?
  // TODO login shell?
  // TODO read profile, read rc, if (!FLAG(noprofile)) { }

  // If not reentering, figure out if this is an interactive shell.
  if (toys.stacktop) {
    if (!FLAG(c)) {
      if (toys.optc==1) toys.optflags |= FLAG_s;
      if (FLAG(s) && isatty(0)) toys.optflags |= FLAG_i;
    }
    if (toys.optc>1) {
      toys.optargs++;
      toys.optc--;
    }
    TT.options |= toys.optflags&0xff;
  }

  // Create initial function context
  call_function()->arg = (struct sh_arg){.v = toys.optargs, .c = toys.optc};
  TT.ff->ifs = " \t\n";
  TT.ff->name = FLAG(i) ? toys.which->name : "main";

  // Set up environment variables and queue up initial command input source
  if (CFG_TOYBOX_FORK || toys.stacktop) subshell_setup();
  else nommu_reentry();

  // Note: run_command() blanks argument sections of TT and this,
  // so parse everything we need from shell command line before here.

// TODO fix/catch O_NONBLOCK on input?

  // Main execution loop: read input and execute lines, with or without prompts.
  if (CFG_TOYBOX_FORK) setjmp(TT.forkchild);
  for (;;) {
    // if this fcall has source but not dlist_terminate()d pl, get line & parse
    if (TT.ff->source && (!TT.ff->pl || TT.ff->pl->prev)) {
      new = get_next_line(TT.ff->source, more+1);
      more = parse_line(new, &expect);
      free(new);
      if (more==1) {
        if (new) continue;
        syntax_err("unexpected end of file");
      }
      // at EOF or error, close source and signal run_lines to pop fcall
      if (!new && TT.ff->source) {
        fclose(TT.ff->source);
        TT.ff->source = 0;
      }
    }

    // TODO: source <(echo 'echo hello\') vs source <(echo -n 'echo hello\')
    // prints "hello" vs "hello\"
    if (!more) run_lines();
    if (!TT.ff) break;
    more = 0;
    llist_traverse(TT.ff->pl, free_pipeline);
    TT.ff->pl = 0;
    llist_traverse(expect, free);
    expect = 0;
  }

  // exit signal.
}

// TODO: ./blah.sh one two three: put one two three in scratch.arg

/********************* shell builtin functions *************************/

// Note: "break &" in bash breaks in the child, this breaks in the parent.
void break_main(void)
{
  unsigned ii = *toys.optargs ? atolx_range(*toys.optargs, 1, INT_MAX) : 1,
    jj = ii;
  struct sh_fcall *ff = TT.ff->next;
  struct sh_blockstack *blk = ff->blk;

  // Search for target.
  for (;;) {
    if (blk->middle && !strcmp(*blk->middle->arg->v, "do") && !--ii) break;
    if ((blk = blk->next)) continue;
    if (ff==TT.ff->prev || ff->function) break;
    ff = ff->next;
  }
  // We try to continue/break N levels deep, but accept fewer.
  if (ii==jj) error_exit("need for/while/until");

  // Unroll to target
  while (TT.ff->blk != blk) if (!pop_block()) end_fcall();
  TT.ff->pl = *toys.which->name=='c' ? TT.ff->blk->start : pop_block();
}

#define FOR_cd
#include "generated/flags.h"
void cd_main(void)
{
  char *from, *to = 0, *dd = *toys.optargs ? : (getvar("HOME") ? : "/"),
       *pwd = FLAG(P) ? 0 : getvar("PWD"), *zap = 0;
  struct stat st1, st2;

  // TODO: CDPATH? Really?

  // For cd - use $OLDPWD as destination directory
  if (!strcmp(dd, "-") && (!(dd = getvar("OLDPWD")) || !*dd))
    return error_msg("No $OLDPWD");

  if (*dd == '/') pwd = 0;

  // Did $PWD move out from under us?
  if (pwd && !stat(".", &st1))
    if (stat(pwd, &st2) || st1.st_dev!=st2.st_dev || st1.st_ino!=st2.st_ino)
      pwd = 0;

  // Handle logical relative path
  if (pwd) {
    zap = xmprintf("%s/%s", pwd, dd);

    // cancel out . and .. in the string
    for (from = to = zap; *from;) {
      if (*from=='/' && from[1]=='/') from++;
      else if (*from!='/' || from[1]!='.') *to++ = *from++;
      else if (!from[2] || from[2]=='/') from += 2;
      else if (from[2]=='.' && (!from[3] || from[3]=='/')) {
        from += 3;
        while (to>zap && *--to != '/');
      } else *to++ = *from++;
    }
    if (to == zap) to++;
    if (to-zap>1 && to[-1]=='/') to--;
    *to = 0;
  }

  // If logical chdir doesn't work, fall back to physical
  if (!zap || chdir(zap)) {
    free(zap);
    if (chdir(dd)) return perror_msg("%s", dd);
    if (!(dd = getcwd(0, 0))) dd = xstrdup("(nowhere)");
  } else dd = zap;

  if ((pwd = getvar("PWD"))) setvarval("OLDPWD", pwd);
  setvarval("PWD", dd);
  free(dd);

  if (!(TT.options&OPT_cd)) {
    export("OLDPWD");
    export("PWD");
    TT.options |= OPT_cd;
  }
}

void continue_main(void)
{
  break_main();
}

void exit_main(void)
{
  toys.exitval = *toys.optargs ? atoi(*toys.optargs) : 0;
  toys.rebound = 0;
  // TODO trap EXIT, sigatexit
  xexit();
}

// lib/args.c can't +prefix & "+o history" needs space so parse cmdline here
void set_main(void)
{
  char *cc, *ostr[] = {"braceexpand", "noclobber", "xtrace"};
  int ii, jj, kk, oo = 0, dd = 0;

  // display visible variables
  if (!*toys.optargs) {
    struct sh_vars **vv = visible_vars();

// TODO escape properly
    for (ii = 0; vv[ii]; ii++)
      if (!(vv[ii]->flags&VAR_WHITEOUT)) printf("%s\n", vv[ii]->str);
    free(vv);

    return;
  }

  // Handle options
  for (ii = 0;; ii++) {
    if ((cc = toys.optargs[ii]) && !(dd = stridx("-+", *cc)+1) && oo--) {
      for (jj = 0; jj<ARRAY_LEN(ostr); jj++) if (!strcmp(cc, ostr[jj])) break;
      if (jj != ARRAY_LEN(ostr)) {
        if (dd==1) TT.options |= OPT_B<<kk;
        else TT.options &= ~(OPT_B<<kk);

        continue;
      }
      error_exit("bad -o %s", cc);
    }
    if (oo>0) for (jj = 0; jj<ARRAY_LEN(ostr); jj++)
      printf("%s\t%s\n", ostr[jj], TT.options&(OPT_B<<jj) ? "on" : "off");
    oo = 0;
    if (!cc || !dd) break;
    for (jj = 1; cc[jj]; jj++) {
      if (cc[jj] == 'o') oo++;
      else if (-1 != (kk = stridx("BCxu", cc[jj]))) {
        if (*cc == '-') TT.options |= OPT_B<<kk;
        else TT.options &= ~(OPT_B<<kk);
      } else error_exit("bad -%c", toys.optargs[ii][1]);
    }
  }

  // handle positional parameters
  if (cc) {
    struct arg_list *al, **head;
    struct sh_fcall *ff = TT.ff->next;
    struct sh_arg *arg = &ff->arg;

    // Make sure we have a deletion list at correct persistence level
    if (!ff->pp) {
      ff->pp = TT.ff->pp;
      TT.ff->pp = 0;
    }

    // Was this memory already scheduled for deletion by a previous "set"?
    for (al = *(head = &ff->pp->delete); al; al = *(head = &al->next))
      if (al->arg == (void *)arg->v) break;

    // free last set's memory (if any) so it doesn't accumulate in loop
    cc = *arg->v;
    if (al) for (jj = arg->c; jj; jj--) {
      *head = al->next;
      free(al->arg);
      free(al);
      al = *head;
    }

    // Add copies of each new argument, scheduling them for deletion.
    *arg = (struct sh_arg){0, 0};
    arg_add(arg, cc);
    while (toys.optargs[ii])
      arg_add(arg, push_arg(&ff->pp->delete, xstrdup(toys.optargs[ii++])));
    push_arg(&ff->pp->delete, arg->v);
  }
}

// TODO need test: unset clears var first and stops, function only if no var.
#define FOR_unset
#include "generated/flags.h"

void unset_main(void)
{
  char **arg, *s;
  int ii;

  for (arg = toys.optargs; *arg; arg++) {
    s = varend(*arg);
    if (s == *arg || *s) {
      error_msg("bad '%s'", *arg);
      continue;
    }

    // TODO -n and name reference support
    // unset variable
    if (!FLAG(f) && unsetvar(*arg)) continue;
    // unset function TODO binary search
    for (ii = 0; ii<TT.funcslen; ii++)
      if (!strcmp(*arg, TT.functions[ii]->name)) break;
    if (ii != TT.funcslen) {
      free_function(TT.functions[ii]);
      memmove(TT.functions+ii, TT.functions+ii+1, TT.funcslen+1-ii);
    }
  }
}

#define FOR_export
#include "generated/flags.h"

void export_main(void)
{
  char **arg, *eq;

  // list existing variables?
  if (!toys.optc) {
    struct sh_vars **vv = visible_vars();
    unsigned uu;

    for (uu = 0; vv[uu]; uu++) {
      if ((vv[uu]->flags&(VAR_WHITEOUT|VAR_EXPORT))==VAR_EXPORT) {
        xputs(eq = declarep(vv[uu]));
        free(eq);
      }
    }
    free(vv);

    return;
  }

  // set/move variables
  for (arg = toys.optargs; *arg; arg++) {
    eq = varend(*arg);
    if (eq == *arg || (*eq && eq[*eq=='+'] != '=')) {
      error_msg("bad %s", *arg);
      continue;
    }

    if (FLAG(n)) set_varflags(*arg, 0, VAR_EXPORT);
    else export(*arg);
  }
}

#define FOR_declare
#include "generated/flags.h"

void declare_main(void)
{
  unsigned uu, fl = toys.optflags&(FLAG(p)-1);
  char *ss, **arg;
// TODO: need a show_vars() to collate all the visible_vars() loop output
// TODO: -g support including -gp
// TODO: dump everything key=value and functions too
  if (!toys.optc) {
    struct sh_vars **vv = visible_vars();

    for (uu = 0; vv[uu]; uu++) {
      if ((vv[uu]->flags&VAR_WHITEOUT) || (fl && !(vv[uu]->flags&fl))) continue;
      xputs(ss = declarep(vv[uu]));
      free(ss);
    }
    free(vv);
  } else if (FLAG(p)) for (arg = toys.optargs; *arg; arg++) {
    struct sh_vars *vv = *varend(ss = *arg) ? 0 : findvar(ss, 0);

    if (!vv) error_msg("%s: not found", ss);
    else {
      xputs(ss = declarep(vv));
      free(ss);
    }
  } else for (arg = toys.optargs; *arg; arg++) {
    ss = varend(*arg);
    if (ss == *arg || (*ss && ss[*ss=='+'] != '=')) {
      error_msg("bad %s", *arg);
      continue;
    }
    set_varflags(*arg, toys.optflags<<1, 0); // TODO +x unset
  }
}

void eval_main(void)
{
  char *s;

  // borrow the $* expand infrastructure to add sh_fcall->source with no ->name
  TT.ff->arg = (struct sh_arg){.v = toys.argv, .c = toys.optc+1};
  TT.ff->lineno = TT.ff->next->lineno;
  s = push_arg(&TT.ff->pp->delete,
    TT.ff->_ = expand_one_arg("\"$*\"", SEMI_IFS));
  TT.ff->source = fmemopen(s, strlen(s), "r");
  TT.ff->arg.v = TT.ff->next->arg.v;
  TT.ff->arg.c = TT.ff->next->arg.c;
}

#define FOR_exec
#include "generated/flags.h"

void exec_main(void)
{
  char *ee[1] = {0}, **old = environ;

  // discard redirects and return if nothing to exec
  free(TT.ff->pp->urd);
  TT.ff->pp->urd = 0;
  if (!toys.optc) return;

//TODO zap isexec
  // exec, handling -acl
  TT.isexec = *toys.optargs;
  if (FLAG(c)) environ = ee;
  if (TT.exec.a || FLAG(l))
    *toys.optargs = xmprintf("-%s"+!FLAG(l), TT.exec.a?:TT.isexec);
  sh_exec(toys.optargs);

  // report error (usually ENOENT) and return
  if (*toys.optargs != TT.isexec) free(*toys.optargs);
  TT.isexec = 0;
  toys.exitval = 127;
  environ = old;
}

// Return T.jobs index or -1 from identifier
// Note, we don't return "ambiguous job spec", we return the first hit or -1.
// TODO %% %+ %- %?ab
int find_job(char *s)
{
  char *ss;
  long ll = strtol(s, &ss, 10);
  int i, j;

  if (!TT.jobs.c) return -1;
  if (!*s || (!s[1] && strchr("%+-", *s))) {
    int minus, plus = find_plus_minus(&minus);

    return (*s == '-') ? minus : plus;
  }

  // Is this a %1 numeric jobspec?
  if (s != ss && !*ss)
    for (i = 0; i<TT.jobs.c; i++)
      if (((struct sh_process *)TT.jobs.v[i])->job == ll) return i;

  // Match start of command or %?abc
  for (i = 0; i<TT.jobs.c; i++) {
    struct sh_process *pp = (void *)TT.jobs.v[i];

    if (strstart(&s, *pp->arg.v)) return i;
    if (*s != '?' || !s[1]) continue;
    for (j = 0; j<pp->arg.c; j++) if (strstr(pp->arg.v[j], s+1)) return i;
  }

  return -1;
}

void jobs_main(void)
{
  int i, j, minus, plus = find_plus_minus(&minus);
  char *s;

// TODO -lnprs

  for (i = 0;;i++) {
    if (toys.optc) {
      if (!(s = toys.optargs[i])) break;
      if ((j = find_job(s+('%' == *s))) == -1) {
        error_msg("%s: no such job", s);

        continue;
      }
    } else if ((j = i) >= TT.jobs.c) break;

    s = show_job((void *)TT.jobs.v[i], is_plus_minus(i, plus, minus));
    printf("%s\n", s);
    free(s);
  }
}

#define FOR_local
#include "generated/flags.h"

void local_main(void)
{
  struct sh_fcall *ff, *ff2;
  struct sh_vars *var;
  char **arg, *eq;

  // find local variable context
  for (ff = TT.ff;; ff = ff->next) {
    if (ff == TT.ff->prev) return error_msg("not in function");
    if (ff->function) break;
  }

  // list existing vars (todo:
  if (!toys.optc) {
    for (var = ff->vars; var; var++) xputs(var->str); // TODO escape
    return;
  }

  // set/move variables
  for (arg = toys.optargs; *arg; arg++) {
    if ((eq = varend(*arg)) == *arg || (*eq && *eq != '=')) {
      error_msg("bad %s", *arg);
      continue;
    }

    if ((var = findvar(*arg, &ff2)) && ff==ff2 && !*eq) continue;
    if (var && (var->flags&VAR_READONLY)) {
      error_msg("%.*s: readonly variable", (int)(varend(*arg)-*arg), *arg);
      continue;
    }

    // Add local inheriting global status and setting whiteout if blank.
    if (!var || ff!=ff2) {
      int flags = var ? var->flags&VAR_EXPORT : 0;

      var = addvar(xmprintf("%s%s", *arg, *eq ? "" : "="), ff);
      var->flags = flags|(VAR_WHITEOUT*!*eq);
    }

    // TODO accept declare options to set more flags
    // TODO, integer, uppercase take effect. Setvar?
  }
}

void return_main(void)
{
  struct sh_fcall *ff;
  char *ss;

  if (*toys.optargs) {
    toys.exitval = estrtol(*toys.optargs, &ss, 0);
    if (errno || *ss) return error_msg("NaN");
  }

  // Do we have a non-transparent function context in the call stack?
  for (ff = TT.ff; !ff->function && !ff->source; ff = ff->next)
    if (ff==TT.ff->prev) return error_msg("not function or source");

  while (TT.ff!=ff) end_fcall();
  TT.ff->pl = 0;
}

void shift_main(void)
{
  long long by = 1;

  if (toys.optc) by = atolx(*toys.optargs);
  by += TT.ff->next->shift;
  if (by<0 || by>=TT.ff->next->arg.c) toys.exitval++;
  else TT.ff->next->shift = by;
}

// TODO add tests: sh -c "source input four five" one two three
void source_main(void)
{
  int ii;

  if (!(TT.ff->source = fpathopen(*toys.optargs)))
    return perror_msg_raw(*toys.optargs);

  // lifetime of optargs handled by TT.ff->pp
  TT.ff->_ = toys.optargs[toys.optc-1];
  TT.ff->name = *toys.optargs;
  *toys.optargs = *toys.argv;
  TT.ff->arg.v = toys.argv; // $0 is shell name, not source file name. Bash!
  for (ii = 0; toys.argv[ii]; ii++);
  TT.ff->arg.c = ii;
}

#define FOR_wait
#include "generated/flags.h"

void wait_main(void)
{
  struct sh_process *pp;
  int ii, jj;
  long long ll;
  char *s;

  // TODO does -o pipefail affect return code here
  if (FLAG(n)) toys.exitval = free_process(wait_job(-1, 0));
  else if (!toys.optc) while (TT.jobs.c) {
    if (!(pp = wait_job(-1, 0))) break;
  } else for (ii = 0; ii<toys.optc; ii++) {
    ll = estrtol(toys.optargs[ii], &s, 10);
    if (errno || *s) {
      if (-1 == (jj = find_job(toys.optargs[ii]))) {
        error_msg("%s: bad pid/job", toys.optargs[ii]);
        continue;
      }
      ll = ((struct sh_process *)TT.jobs.v[jj])->pid;
    }
    if (!(pp = wait_job(ll, 0))) {
      if (toys.signal) toys.exitval = 128+toys.signal;
      break;
    }
    toys.exitval = free_process(pp);
  }
}
