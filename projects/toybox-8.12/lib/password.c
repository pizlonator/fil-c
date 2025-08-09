/* password.c - password read/update helper functions.
 *
 * Copyright 2012 Ashwini Kumar <ak.ashwini@gmail.com>
 */

#include "toys.h"

// generate $id$salt$hash given a password and algorithm. Generates salt if NULL
//char *toy_crypt(char *pass, char *salt, char *algo)
// generate ID prefix and random salt for given encryption algorithm.
int get_salt(char *salt, char *algo, int rand)
{
  struct {
    char *type, id, len;
  } al[] = {{"des", 0, 2}, {"md5", 1, 8}, {"sha256", 5, 16}, {"sha512", 6, 16}};
  char *s;
  int i, len;

  for (i = 0; i < ARRAY_LEN(al); i++) {
    for (s = al[i].type, len = 0; algo[len]; len++) {
      while (ispunct(algo[len])) len++;
      if (tolower(algo[len]) != tolower(*s++)) break;
    }
    if (algo[len]) continue;

    len = al[i].len;
    s = salt + (al[i].id ? sprintf(salt, "$%c$", '0'+al[i].id) : 0);

    // Read appropriate number of random bytes for salt
    if (rand) xgetrandom(libbuf, ((len*6)+7)/8);

    // Grab 6 bit chunks and convert to characters in ./0-9a-zA-Z
    for (i = 0; i<len; i++) {
      int bitpos = i*6, bits = bitpos/8;

      bits = ((libbuf[i]+(libbuf[i+1]<<8)) >> (bitpos&7)) & 0x3f;
      bits += 46;
      if (bits > 57) bits += 7;
      if (bits > 90) bits += 6;

      s[i] = bits;
    }
    s[len] = 0;

    return s-salt;
  }

  return -1;
}

// Prompt with mesg, read password into buf, return 0 for success 1 for fail
int read_password(char *buf, int buflen, char *mesg)
{
  struct termios oldtermio;
  struct sigaction sa = {.sa_handler = generic_signal}, oldsa;
  int i, tty = tty_fd(), ret = 1;

  // Set NOP signal handler to return from the read.
  fflush(0);
  sigaction(SIGINT, &sa, &oldsa);
  tcflush(tty, TCIFLUSH);
  xset_terminal(tty, 1, 0, &oldtermio);
  dprintf(tty, "%s", mesg);

  // Loop assembling password. (Too long = fail)
  for (i = 0; i<buflen-1; i++) {
    // tty closed, or EOF or ctrl-D at start, or ctrl-C anywhere: fail.
    if ((ret = read(tty, buf+i, 1))<0 || (!ret&&!i) || *buf==4 || buf[i]==3)
      break;
    // EOF or newline: return success
    else if (!ret || buf[i]=='\n' || buf[i]=='\r') {
      ret = 0;
      break;
    } else if (buf[i] == 8 || buf[i] == 127) i -= 2-!i;
  }

  // Restore terminal/signal state, terminate string
  tcsetattr(0, TCSANOW, &oldtermio);
  sigaction(SIGINT, &oldsa, 0);
  xputc('\n');
  fflush(0);
  buf[i*!ret] = 0;

  return ret;
}

// Read array of colon separated strings from file with given first entry
char **get_userline(char *filename, char *username)
{
  FILE *fp = xfopen(filename, "r");
  int len = strlen(username);
  char *line = 0, **data;
  size_t n = 0;

  while (getline(&line, &n, fp)) {
    if (!strncmp(line, username, len) && line[len]==':') {
      data = xzalloc(10*sizeof(char *));
      for (len = 0; len<9; len++) {
        data[len] = line;
        if (!(line = strchr(line, ':'))) break;
        *line++ = 0;
      }
      return data;
    }
    memset(line, 0, strlen(line));
    free(line);
    line = 0;
    n = 0;
  }

  return 0;
}

// update colon-separated text files ala /etc/{passwd,shadow,group,gshadow}
// returns 1 for success, 0 for failure

// username = string match for first entry in line
// entry = new entry (NULL deletes matching line, contains : adds/replaces line)
// pos = if no : in "entry", which field to replace (0 is first)
// filename+ = new copy being written, filename- = backup of old version

int update_password(char *filename, char *username, char *entry, int pos)
{
  char *ff = xmprintf("%s-", filename), *line = 0, *start, *end, *out, *oo;
  FILE *ofp;
  int len = strlen(username)*!strchr(username,':'), rc = 0, ret = 0, found = 0,
      nfd;
  struct flock lock = {.l_type = F_WRLCK};
  struct stat st;
  long long ll = 0;

  // Open old filename ("r" won't let us lock) and get blocking lock
  if (!(ofp = fopen(filename, "w+")) || 0>fcntl(fileno(ofp), F_SETLK, &lock)\
      || fstat(fileno(ofp), &st))
  {
    perror_msg_raw(filename);
    goto free_storage;
  }

  // Delete old backup, link new backup. (Failure here isn't fatal.)
  unlink(ff);
  if (0>link(filename, ff)) perror_msg_raw(ff);

  // Open new file to copy entries to
  ff[strlen(ff)-1] = '+';
  if (-1 == (nfd = xcreate(ff, O_CREAT|O_EXCL|O_WRONLY, st.st_mode))) {
    perror_msg_raw(ff);
    goto free_storage;
  }

  // Loop through lines
  for (; getline(&line, (void *)&ll, ofp)!=-1; memset(line, 0, strlen(line))) {
    // find matching line
    oo = 0;
    start = end = chomp(line);
    if (strncmp(line, username, len) || !(line[len] && line[len]!=':'))
      out = line;
    else {
      found++;

    // Delete or replace whole line?
      if (!entry) out = 0;
      else if (strchr(entry, ':')) out = entry;

      // Replace entry at pos
      else {
        for (;; pos--, start = ++end) {
          while (*end && *end != ':') end++;
          if (!pos || !*end) break;
        }
        if (pos>=0) out = line;
        else oo = out = xmprintf("%*s%s%s\n", (int)(start-line),line,entry,end);
      }
    }
    if (out) {
      rc = dprintf(nfd, "%s\n", out);
      free(out);
      if (rc<0) {
        perror_msg_raw(ff);
        goto free_storage;
      }
      free(oo);
    }
  }
  free(line);
  if (!found && entry && strchr(entry, ':')) dprintf(nfd, "%s\n", entry);
  fsync(nfd);
  close(nfd);  // automatically unlocks

  if (!found || rename(ff, filename)) {
    if (found) perror_msg("%s -> %s", ff, filename);
    else if (entry) error_msg("No %s in %s", username, filename);
  } else ret = 1, *ff = 0;

free_storage:
  if (ofp) fclose(ofp);
  if (ff && *ff) unlink(ff);
  free(ff);

  return ret;
}
