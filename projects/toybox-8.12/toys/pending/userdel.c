/* userdel.c - delete a user
 *
 * Copyright 2014 Ashwini Kumar <ak.ashwini1981@gmail.com>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/userdel.html

USE_USERDEL(NEWTOY(userdel, "<1>1r", TOYFLAG_NEEDROOT|TOYFLAG_SBIN))
USE_USERDEL(OLDTOY(deluser, userdel, TOYFLAG_NEEDROOT|TOYFLAG_SBIN))

config USERDEL
  bool "userdel"
  default n
  help
    usage: userdel [-r] USER
    usage: deluser [-r] USER

    Delete USER from the SYSTEM

    -r	remove home directory
*/

#define FOR_userdel
#include "toys.h"

static void update_groupfiles(char *filename, char* username)
{
  char *filenamesfx = NULL, *sfx = NULL, *line = NULL;
  FILE *exfp, *newfp;
  int ulen = strlen(username);
  size_t allocated_length = 0;
  struct flock lock;

  filenamesfx = xmprintf("%s+", filename);
  sfx = strchr(filenamesfx, '+');
  exfp = xfopen(filename, "r+");

  *sfx = '-';
  unlink(filenamesfx);
  if (link(filename, filenamesfx)) error_msg("Can't create backup file");

  *sfx = '+';
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = lock.l_len = 0;

  if (fcntl(fileno(exfp), F_SETLK, &lock) < 0)
    perror_msg("Couldn't lock file %s",filename);

  lock.l_type = F_UNLCK; //unlocking at a later stage

  newfp = xfopen(filenamesfx, "w+");

  while (getline(&line, &allocated_length, exfp) > 0) {
    sprintf(toybuf, "%s:",username);
    if (strncmp(line, toybuf, ulen+1)) {
      char *n, *p = strrchr(line, ':');

      if (p && *++p && (n = strstr(p, username))) {
        do {
          if (n[ulen] == ',') {
            *n = '\0';
            n += ulen + 1;
            fprintf(newfp, "%s%s\n", line, n);
            break;
          } else if (!n[ulen]) {
            if (n[-1] == ',') n[-1] = *n = '\0';
            if (n[-1] == ':') *n = '\0';
            fprintf(newfp, "%s%s\n", line, n);
            break;
          } else n += ulen;
        } while (*n && (n=strstr(n, username)));
        if (!n) fprintf(newfp, "%s\n", line);
      } else fprintf(newfp, "%s\n", line);
    }
  }
  free(line);
  fcntl(fileno(exfp), F_SETLK, &lock);
  fclose(exfp);
  errno = 0;
  fflush(newfp);
  fsync(fileno(newfp));
  fclose(newfp);
  rename(filenamesfx, filename);
  if (errno){
    perror_msg("File Writing/Saving failed: ");
    unlink(filenamesfx);
  }
  free(filenamesfx);
}

void userdel_main(void)
{
  struct passwd *pwd = xgetpwnam(*toys.optargs);

  update_password("/etc/passwd", pwd->pw_name, 0, 0);
  update_password("/etc/shadow", pwd->pw_name, 0, 0);

  // delete the group named USER, and remove user from group.
  // could update_password() be used for this?
  // not a good idea, as update_passwd() updates one entry at a time
  // in this case it will be modifying the files as many times the
  // USER appears in group database files. So the customized version
  // of update_passwd() is here.
  update_groupfiles("/etc/group", *toys.optargs);
  update_groupfiles("/etc/gshadow", *toys.optargs);

  if (FLAG(r)) {
    char *arg[] = {"rm", "-fr", pwd->pw_dir, NULL, NULL};

    sprintf(toybuf, "/var/spool/mail/%s",pwd->pw_name);
    arg[3] = toybuf;
    xexec(arg);
  }
}
