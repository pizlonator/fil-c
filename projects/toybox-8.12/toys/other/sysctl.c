/* sysctl.c - A utility to read and manipulate the sysctl parameters.
 *
 * Copyright 2014 Bilal Qureshi <bilal.jmi@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_SYSCTL(NEWTOY(sysctl, "^neNqwpaA[!ap][!aq][!aw][+aA]", TOYFLAG_SBIN))

config SYSCTL
  bool "sysctl"
  default y
  help
    usage: sysctl [-aeNnqw] [-p [FILE] | KEY[=VALUE]...]

    Read/write system control data (under /proc/sys).

    -a	Show all values
    -e	Don't warn about unknown keys
    -N	Don't print key values
    -n	Don't print key names
    -p	Read values from FILE (default /etc/sysctl.conf)
    -q	Don't show value after write
    -w	Only write values (object to reading)
*/
#define FOR_sysctl
#include "toys.h"

// Null terminate at =, return value
static char *split_key(char *key)
{
  char *value = strchr(key, '=');

  if (value) *(value++)=0;

  return value;
}

static void replace_char(char *str, char old, char new)
{
  for (; *str; str++) if (*str == old) *str = new;
}

static void key_error(char *key)
{
  if (errno != ENOENT) perror_msg("key '%s'", key);
  else if (!FLAG(e)) error_msg("unknown key '%s'", key);
}

static int write_key(char *path, char *key, char *value)
{
  int fd = open(path, O_WRONLY);

  if (fd < 0) {
    key_error(key);

    return 0;
  }
  xwrite(fd, value, strlen(value));
  xclose(fd);

  return 1;
}

// Display all keys under a path
static int do_show_keys(struct dirtree *dt)
{
  char *path, *data, *key;

  if (!dirtree_notdotdot(dt)) return 0; // Skip . and ..
  if (S_ISDIR(dt->st.st_mode)) return DIRTREE_RECURSE;

  path = dirtree_path(dt, 0);
  data = readfile(path, 0, 0);
  replace_char(key = path + 10, '/', '.'); // skip "/proc/sys/"
  if (!data) key_error(key);
  else {
    // Print the parts that aren't switched off by flags.
    if (!FLAG(n)) xprintf("%s", key);
    if (!FLAG(N) && !FLAG(n)) xprintf(" = ");
    for (key = data+strlen(data); key > data && isspace(*--key); *key = 0);
    if (!FLAG(N)) xprintf("%s", data);
    if (!FLAG(N) || !FLAG(n)) xputc('\n');
  }

  free(data);
  free(path);

  return 0;
}

// Read/write entries under a key. Accepts "key=value" in key if !value
static void process_key(char *key, char *value)
{
  char *path;

  if (!value) value = split_key(key);
  if (FLAG(w) && !value) return error_msg("'%s' not key=value", key);
  path = xmprintf("/proc/sys/%s", key);
  replace_char(path, '.', '/');
  // Note: failure to assign to a non-leaf node suppresses the display.
  if (!(value && (!write_key(path, key, value) || FLAG(q)))) {
    if (!access(path, R_OK)) dirtree_read(path, do_show_keys);
    else key_error(key);
  }
  free(path);
}

void sysctl_main()
{
  char **args = 0;

  // Display all keys
  if (FLAG(a)) dirtree_read("/proc/sys", do_show_keys);

  // read file
  else if (FLAG(p)) {
    FILE *fp = xfopen(*toys.optargs ? *toys.optargs : "/etc/sysctl.conf", "r");
    size_t len;

    for (;;) {
      char *line = 0, *key, *val;

      if (-1 == (len = getline(&line, &len, fp))) break;
      key = line;
      while (isspace(*key)) key++;
      if (*key == '#' || *key == ';' || !*key) continue;
      while (len && isspace(line[len-1])) line[--len] = 0;
      if (!(val = split_key(line))) {
        error_msg("'%s' not key=value", line);
        continue;
      }

      // Trim whitespace around =
      len = (val-line)-1;
      while (len && isspace(line[len-1])) line[--len] = 0;
      while (isspace(*val)) val++;;

      process_key(key, val);
      free(line);
    }
    fclose(fp);

  // Loop through arguments, displaying or assigning as appropriate
  } else {
    if (!*toys.optargs) help_exit("Needs 1 arg");
    for (args = toys.optargs; *args; args++) process_key(*args, 0);
  }
}
