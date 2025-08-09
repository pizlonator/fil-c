// Can't trust libc not to leak enviornment variable memory, so...

#include "toys.h"

// In libc, populated by start code, used by getenv() and exec() and friends.
extern char **environ;

// Returns the number of bytes taken by the environment variables. For use
// when calculating the maximum bytes of environment+argument data that can
// be passed to exec for find(1) and xargs(1).
long environ_bytes(void)
{
  long bytes = sizeof(char *);
  char **ev;

  for (ev = environ; *ev; ev++) bytes += sizeof(char *) + strlen(*ev) + 1;

  return bytes;
}

// This will clear the inherited environment if called first thing.
// Use this instead of envc so we keep track of what needs to be freed.
void xclearenv(void)
{
  if (toys.envc) {
    int i;

    for (i = 0; environ[i]; i++) if (i>=toys.envc) free(environ[i]);
  } else environ = xmalloc(256*sizeof(char *));
  toys.envc = 1;
  *environ = 0;
}

// Frees entries we set earlier. Use with libc getenv but not setenv/putenv.
// if name has an equals and !val, act like putenv (name=val must be malloced!)
// if !val unset name. (Name with = and val is an error)
// returns pointer to new name=value environment string, NULL if none
char *xsetenv(char *name, char *val)
{
  unsigned i, j = 0, len;
  char *new;

  // If we haven't snapshot initial environment state yet, do so now.
  if (!toys.envc) {

    // envc is size +1 so even if env empty it's nonzero after initialization
    while (environ[toys.envc++]);
    memcpy(new = xmalloc(((toys.envc|31)+1)*sizeof(char *)), environ,
      toys.envc*sizeof(char *));
    environ = (void *)new;
  }

  if (!(new = strchr(name, '='))) {
    len = strlen(name);
    if (val) new = xmprintf("%s=%s", name, val);
  } else {
    len = new-name;
    if (val) error_exit("xsetenv %s to %s", name, val);
    new = name;
  }

  for (i = 0; environ[i]; i++) {
    // Drop old entry, freeing as appropriate. Assumes no duplicates.
    if (!smemcmp(name, environ[i], len) && environ[i][len]=='=') {
      if (i<toys.envc-1) toys.envc--;
      else free(environ[i]);
      j++;
    }

    // move data down to fill hole, including null terminator
    if (j && !(environ[i] = environ[i+1])) break;
  }

  if (!new) return 0;

  // resize and null terminate if expanding
  if (!j && !environ[i]) {
    len = i+1;
    if (!(len&31)) environ = xrealloc(environ, (len+32)*sizeof(char *));
    environ[len] = 0;
  }

  return environ[i] = new;
}

void xunsetenv(char *name)
{
  if (strchr(name, '=')) error_exit("xunsetenv %s name has =", name);
  xsetenv(name, 0);
}

// remove entry and return pointer instead of freeing
char *xpop_env(char *name)
{
  int len, i;
  char *s = 0;

  for (len = 0; name[len] && name[len]!='='; len++);
  for (i = 0; environ[i]; i++) {
    if (!s && !strncmp(name, environ[i], len) && environ[i][len] == '=') {
      s = environ[i];
      if (toys.envc-1>i) {
        s = xstrdup(s);
        toys.envc--;
      }
    }
    if (s) environ[i] = environ[i+1];
  }

  return s;
}

// reset environment for a user, optionally clearing most of it
void reset_env(struct passwd *p, int clear)
{
  int i;

  if (clear) {
    char *s, *stuff[] = {"TERM", "DISPLAY", "COLORTERM", "XAUTHORITY"};

    for (i=0; i<ARRAY_LEN(stuff); i++)
      stuff[i] = (s = getenv(stuff[i])) ? xmprintf("%s=%s", stuff[i], s) : 0;
    xclearenv();
    for (i=0; i < ARRAY_LEN(stuff); i++) if (stuff[i]) xsetenv(stuff[i], 0);
    if (chdir(p->pw_dir)) {
      perror_msg("chdir %s", p->pw_dir);
      xchdir("/");
    }
  } else {
    char **ev1, **ev2;

    // remove LD_*, IFS, ENV, and BASH_ENV from environment
    for (ev1 = ev2 = environ;;) {
      while (*ev2 && (strstart(ev2, "LD_") || strstart(ev2, "IFS=") ||
        strstart(ev2, "ENV=") || strstart(ev2, "BASH_ENV="))) ev2++;
      if (!(*ev1++ = *ev2++)) break;
    }
  }

  setenv("PATH", _PATH_DEFPATH, 1);
  setenv("HOME", p->pw_dir, 1);
  setenv("SHELL", p->pw_shell, 1);
  setenv("USER", p->pw_name, 1);
  setenv("LOGNAME", p->pw_name, 1);
}
