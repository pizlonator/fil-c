/* mdev.c - Populate /dev directory and handle hotplug events
 *
 * Copyright 2005, 2008 Rob Landley <rob@landley.net>
 * Copyright 2005 Frank Sorenson <frank@tuxrocks.com>

USE_MDEV(NEWTOY(mdev, "s", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_UMASK))

config MDEV
  bool "mdev"
  default n
  help
    usage: mdev [-s]

    Create devices in /dev using information from /sys.

    -s	Scan all entries in /sys to populate /dev

config MDEV_CONF
  bool "Configuration file for mdev"
  default y
  depends on MDEV
  help
    The mdev config file (/etc/mdev.conf) contains lines that look like:
    hd[a-z][0-9]* 0:3 660
    (sd[a-z]) root:disk 660 =usb_storage

    Each line must contain three whitespace separated fields. The first
    field is a regular expression matching one or more device names,
    the second and third fields are uid:gid and file permissions for
    matching devices. Fourth field is optional. It could be used to change
    device name (prefix '='), path (prefix '=' and postfix '/') or create a
    symlink (prefix '>').
*/

#include "toys.h"
#include <stdbool.h>

// mknod in /dev based on a path like "/sys/block/hda/hda1"
static void make_device(char *path)
{
  char *device_name = 0, *custom_name = 0, *s, *temp;
  bool create_symlink = false;
  int major = 0, minor = 0, type, len, fd, mode = 0660;
  uid_t uid = 0;
  gid_t gid = 0;

  if (path) {

    // Try to read major/minor string, returning if we can't
    temp = strrchr(path, '/');
    fd = open(path, O_RDONLY);
    *temp = 0;
    len = read(fd, toybuf, 64);
    close(fd);
    if (len<1) return;
    toybuf[len] = 0;

    // Determine device type, major and minor

    type = path[5]=='c' ? S_IFCHR : S_IFBLK;
    sscanf(toybuf, "%u:%u", &major, &minor);
  } else {
    // if (!path), do hotplug

    if (!(temp = getenv("MODALIAS"))) xrun((char *[]){"modprobe", temp, 0});
    if (!(temp = getenv("SUBSYSTEM"))) return;
    type = strcmp(temp, "block") ? S_IFCHR : S_IFBLK;
    if (!(temp = getenv("MAJOR"))) return;
    sscanf(temp, "%u", &major);
    if (!(temp = getenv("MINOR"))) return;
    sscanf(temp, "%u", &minor);
    if (!(path = getenv("DEVPATH"))) return;
    device_name = getenv("DEVNAME");
  }
  if (!device_name)
    device_name = strrchr(path, '/') + 1;

  // as in linux/drivers/base/core.c, device_get_devnode()
  while ((temp = strchr(device_name, '!'))) {
    *temp = '/';
  }

  // If we have a config file, look up permissions for this device

  if (CFG_MDEV_CONF) {
    char *conf, *pos, *end;
    bool optional_field_valid = false;

    // mmap the config file
    if (-1!=(fd = open("/etc/mdev.conf", O_RDONLY))) {
      int line = 0;

      len = fdlength(fd);
      conf = xmmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

      // Loop through lines in mmaped file
      for (pos = conf; pos-conf<len;) {
        int field;
        char *end2;

        line++;
        // find end of this line
        for(end = pos; end-conf<len && *end!='\n'; end++);

        // Four fields (last is optional): regex, uid:gid, mode [, name|path ]
        // For example: (sd[a-z])1  root:disk 660 =usb_storage_p1
        for (field = 4; field; field--) {
          // Skip whitespace
          while (pos<end && isspace(*pos)) pos++;
          if (pos==end || *pos=='#') break;
          for (end2 = pos;
            end2<end && !isspace(*end2) && *end2!='#'; end2++);
          switch(field) {
            // Regex to match this device
            case 4:
            {
              char *regex = strndup(pos, end2-pos);
              regex_t match;
              regmatch_t off;
              int result;

              // Is this it?
              xregcomp(&match, regex, REG_EXTENDED);
              result=regexec(&match, device_name, 1, &off, 0);
              regfree(&match);
              free(regex);

              // If not this device, skip rest of line
              if (result || off.rm_so
                || off.rm_eo!=strlen(device_name))
                  goto end_line;

              break;
            }
            // uid:gid
            case 3:
            {
              char *s2;

              // Find :
              for(s = pos; s<end2 && *s!=':'; s++);
              if (s==end2) goto end_line;

              // Parse UID
              uid = strtoul(pos,&s2,10);
              if (s!=s2) {
                struct passwd *pass;
                char *str = strndup(pos, s-pos);
                pass = getpwnam(str);
                free(str);
                if (!pass) goto end_line;
                uid = pass->pw_uid;
              }
              s++;
              // parse GID
              gid = strtoul(s,&s2,10);
              if (end2!=s2) {
                struct group *grp;
                char *str = strndup(s, end2-s);
                grp = getgrnam(str);
                free(str);
                if (!grp) goto end_line;
                gid = grp->gr_gid;
              }
              break;
            }
            // mode
            case 2:
            {
              char *beg_pos = pos;
              mode = strtoul(pos, &pos, 8);
              if (pos == beg_pos) {
                // The line is bad because mode setting could not be
                // converted to numeric value.
                goto end_line;
              }
              break;
            }
            // Try to look for name or path (optional field)
            case 1:
            {
              if(pos < end2){
                //char *name = strndup(pos, end2-pos);
                char *name = malloc(end2-pos+1);
                if(name){
                  strncpy(name, pos, end2-pos+1);
                  name[end2-pos] = '\0';
                  switch(name[0]){
                    case '>':
                      create_symlink = true;
                    case '=':
                      custom_name = strdup(&name[1]);
                      break;
                    case '!':
                      device_name = NULL;
                      break;
                    default:
                      free(name);
                      goto end_line;
                  }
                  free(name);
                  optional_field_valid = true;
                }
              }
              goto found_device;
            }
          }
          pos=end2;
        }
end_line:
        // Did everything parse happily?
        // Note: Last field is optional.
        if ((field>1 || (field==1 && !optional_field_valid)) && field!=4)
          error_exit("Bad line %d", line);
        // Next line
        pos = ++end;
      }
found_device:
      munmap(conf, len);
    }
    close(fd);
  }

  if(device_name) {
    if(custom_name) {
      sprintf(toybuf, "/dev/%s", custom_name);
      if(custom_name[strlen(custom_name) - 1] == '/') {
        DIR *dir;
        dir = opendir(toybuf);
        if(dir) closedir(dir);
        else mkdir(toybuf, 0755);
      }
    }
    else
      sprintf(toybuf, "/dev/%s", device_name);

      if ((temp=getenv("ACTION")) && !strcmp(temp, "remove")) {
        unlink(toybuf);
        return;
      }

      if (strchr(device_name, '/')) mkpath(toybuf);
      if (mknod(toybuf, mode | type, dev_makedev(major, minor)) &&
          errno != EEXIST)
        perror_exit("mknod %s failed", toybuf);
      if(create_symlink){
        char *symlink_name = malloc(sizeof("/dev/") + strlen(device_name) + 1);
        if(symlink_name == NULL)
          perror_exit("malloc failed while creating symlink to %s", toybuf);
        sprintf(symlink_name, "/dev/%s", device_name);
        if(symlink(toybuf, symlink_name)){
          free(symlink_name);
          perror_exit("symlink creation failed for %s", toybuf);
        }
        free(symlink_name);
      }

      if (type == S_IFBLK) close(open(toybuf, O_RDONLY)); // scan for partitions

      if (CFG_MDEV_CONF) mode=chown(toybuf, uid, gid);
  }
}

static int callback(struct dirtree *node)
{
  // Entries in /sys/class/block aren't char devices, so skip 'em.  (We'll
  // get block devices out of /sys/block.)
  if(!strcmp(node->name, "block")) return 0;

  // Does this directory have a "dev" entry in it?
  // This is path based because the hotplug callbacks are
  if (S_ISDIR(node->st.st_mode) || S_ISLNK(node->st.st_mode)) {
    int len=4;
    char *dev = dirtree_path(node, &len);
    strcpy(dev+len, "/dev");
    if (!access(dev, R_OK)) make_device(dev);
    free(dev);
  }

  // Circa 2.6.25 the entries more than 2 deep are all either redundant
  // (mouse#, event#) or unnamed (every usb_* entry is called "device").

  if (node->parent && node->parent->parent) return 0;
  return DIRTREE_RECURSE|DIRTREE_SYMFOLLOW;
}

void mdev_main(void)
{
  // Handle -s

  if (toys.optflags) {
    dirtree_read("/sys/class", callback);
    if (!access("/sys/block", R_OK)) dirtree_read("/sys/block", callback);
  } else { // hotplug support
    make_device(NULL);
  }
}
