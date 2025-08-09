/* acpi.c - show power state
 *
 * Written by Isaac Dunham, 2013
 *
 * No standard.

USE_ACPI(NEWTOY(acpi, "abctV", TOYFLAG_USR|TOYFLAG_BIN))

config ACPI
  bool "acpi"
  default y
  help
    usage: acpi [-abctV]

    Show status of power sources and thermal devices.

    -a	Show power adapters
    -b	Show batteries
    -c	Show cooling device state
    -t	Show temperatures
    -V	Show everything
*/

#define FOR_acpi
#include "toys.h"

static int read_int_at(int dirfd, char *name)
{
  int fd, ret=0;
  FILE *fil;

  if ((fd = openat(dirfd, name, O_RDONLY)) < 0) return -1;
  if (!fscanf(fil = xfdopen(fd, "r"), "%d", &ret)) perror_exit_raw(name);
  fclose(fil);

  return ret;
}

static int acpi_callback(struct dirtree *tree)
{
  int dfd, fd, len, on, bat = 0, ac = 0;
  char *cpath;

  errno = 0;

  if (*tree->name=='.') return 0;
  if (!tree->parent) return DIRTREE_RECURSE|DIRTREE_SYMFOLLOW;

  if (0<=(dfd = open((cpath = dirtree_path(tree, 0)), O_RDONLY))) {
    if ((fd = openat(dfd, "type", O_RDONLY)) < 0) goto done;
    len = readall(fd, toybuf, sizeof(toybuf));
    close(fd);
    if (len<1) goto done;

    if (!strncmp(toybuf, "Battery", 7)) {
      if (FLAG(b) || !toys.optflags) {
        int cap = 0, curr = 0, max = 0;

        if ((cap = read_int_at(dfd, "capacity"))<0) {
          if ((max = read_int_at(dfd, "charge_full"))>0 ||
              (max = read_int_at(dfd, "energy_full"))>0)
            curr = read_int_at(dfd, "charge_now");
          if (max>0 && curr>=0) cap = 100*curr/max;
        }
        if (cap>=0) printf("Battery %d: %d%%\n", bat++, cap);
      }
    } else if (FLAG(a) && (on = read_int_at(dfd, "online"))>=0)
      printf("Adapter %d: %s-line\n", ac++, on ? "on" : "off");
done:
    close(dfd);
  }
  free(cpath);
  return 0;
}

static int temp_callback(struct dirtree *tree)
{
  int dfd, temp, therm = 0;
  char *cpath;

  if (*tree->name=='.') return 0;
  if (!tree->parent || !tree->parent->parent)
    return DIRTREE_RECURSE|DIRTREE_SYMFOLLOW;
  errno = 0;

  if (0<=(dfd = open((cpath = dirtree_path(tree, 0)), O_RDONLY))) {
    if ((0 < (temp = read_int_at(dfd, "temp"))) || !errno) {
      // some tempertures are in milli-C, some in deci-C
      // reputedly some are in deci-K, but I have not seen them
      if ((temp>=1000 || temp<=-1000) && !(temp%100)) temp /= 100;
      printf("Thermal %d: %d.%d degrees C\n", therm++, temp/10, temp%10);
    }
    close(dfd);
  }
  free(cpath);

  return 0;
}

static int cool_callback(struct dirtree *tree)
{
  int dfd = 5, cur, max, cool = 0;
  char *cpath;

  errno = 0;
  memset(toybuf, 0, 257);

  if (*tree->name == '.') return 0;
  if (!tree->parent) return DIRTREE_RECURSE|DIRTREE_SYMFOLLOW;

  if (0<=(dfd = open((cpath = dirtree_path(tree, &dfd)), O_RDONLY))) {
    strcat(cpath, "/type");
    if (readfile(cpath, toybuf, 256) && !errno) {
      chomp(toybuf);
      cur = read_int_at(dfd, "cur_state");
      max = read_int_at(dfd, "max_state");
      printf("Cooling %d: %s ", cool++, toybuf);
      if (errno) printf("no state information\n");
      else printf("%d of %d\n", cur, max);
    }
    close(dfd);
  }
  free(cpath);

  return 0;
}

void acpi_main(void)
{
  if (FLAG(V)) toys.optflags = FLAG_a|FLAG_b|FLAG_c|FLAG_t;
  if (!toys.optflags) toys.optflags = FLAG_b;
  if (FLAG(a)|FLAG(b)) dirtree_read("/sys/class/power_supply", acpi_callback);
  if (FLAG(t)) dirtree_read("/sys/class", temp_callback);
  if (FLAG(c)) dirtree_read("/sys/class/thermal", cool_callback);
}
