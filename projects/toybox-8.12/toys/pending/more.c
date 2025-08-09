/* more.c - View FILE (or stdin) one screenfull at a time.
 *
 * Copyright 2013 Bilal Qureshi <bilal.jmi@gmail.com>
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/more.html

USE_MORE(NEWTOY(more, 0, TOYFLAG_USR|TOYFLAG_BIN))

config MORE
  bool "more"
  default n
  help
    usage: more [FILE...]

    View FILE(s) (or stdin) one screenfull at a time.
*/

#define FOR_more
#include "toys.h"

GLOBALS(
  struct termios inf;
  int cin_fd;
)

static void signal_handler(int sig)
{
  // Reset the terminal whether we were signalled or exited normally.
  tcsetattr(TT.cin_fd, TCSANOW, &TT.inf);

  // If we were actually signalled, move to a new line and re-raise the signal.
  if (sig != 0) {
    xputc('\n');
    signal(sig, SIG_DFL);
    raise(sig);
    _exit(sig | 128);
  }
}

static void show_file_header(const char *name)
{
  printf("::::::::::::::\n%s\n::::::::::::::\n", name);
}

static int prompt(FILE *cin, const char* fmt, ...)
{
  int input_key;
  va_list ap;

  printf("\33[7m"); // Reverse video before printing the prompt.

  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);

  while (1) {
    fflush(NULL);
    input_key = tolower(getc(cin));
    printf("\33[0m\33[1K\r"); // Reset all attributes, erase to start of line.
    if (strchr(" \nrq", input_key)) {
      fflush(NULL);
      return input_key;
    }
    printf("\33[7m(Enter:Next line Space:Next page Q:Quit R:Show the rest)");
  }
}

static int more_directory(char *path, struct stat *st)
{
  if (!stat(path, st) && S_ISDIR(st->st_mode)) {
    printf("\n*** %s: directory ***\n\n", path);
    return 1;
  }
  return 0;
}

static void do_cat_operation(int fd, char *name)
{
  struct stat st;

  if (!more_directory(name, &st)) {
    show_file_header(name);
    fflush(stdout);
    xsendfile(fd, 1);
  }
}

void more_main()
{
  int ch, input_key = 0, show_prompt;
  unsigned rows = 24, cols = 80, row = 0, col = 0;
  struct stat st;
  struct termios newf;
  FILE *fp, *cin;

  if (!isatty(1) || !(cin = fopen("/dev/tty", "r"))) {
    loopfiles(toys.optargs, do_cat_operation);
    return;
  }

  TT.cin_fd = fileno(cin);
  tcgetattr(TT.cin_fd, &TT.inf);

  //Prepare terminal for input
  memcpy(&newf, &TT.inf, sizeof(struct termios));
  newf.c_lflag &= ~(ICANON | ECHO);
  newf.c_cc[VMIN] = 1;
  newf.c_cc[VTIME] = 0;
  tcsetattr(TT.cin_fd, TCSANOW, &newf);

  sigatexit(signal_handler);

  do {
    char *filename = *toys.optargs;

    st.st_size = show_prompt = col = row = 0;
    if (!filename) fp = stdin;
    else {
      if (more_directory(filename, &st)) goto next_file;
      if (!(fp = fopen(filename, "r"))) {
        perror_msg("%s", filename);
        goto next_file;
      }
    }

    terminal_size(&cols, &rows);
    rows--;

    if (toys.optc > 1) {
      show_file_header(filename);
      row += 3;
    }

    while ((ch = getc(fp)) != EOF) {
      if (input_key != 'r' && show_prompt) {
        if (st.st_size)
          input_key = prompt(cin, "--More--(%d%% of %lld bytes)",
              (int) (100 * ( (double) ftell(fp) / (double) st.st_size)),
              (long long)st.st_size);
        else
          input_key = prompt(cin, "--More--");
        if (input_key == 'q') goto stop;

        col = row = show_prompt = 0;
        terminal_size(&cols, &rows);
        rows--;
      }

      putchar(ch);
      if (ch == '\t') col = (col | 0x7) + 1;
      else col++;
      if (col == cols) putchar(ch = '\n');
      if (ch == '\n') {
        col = 0;
        if (++row >= rows || input_key == '\n') show_prompt = 1;
      }
    }
    fclose(fp);

next_file:
    if (*toys.optargs && *++toys.optargs) {
      input_key = prompt(cin, "--More--(Next file: %s)", *toys.optargs);
      if (input_key == 'q') goto stop;
    }
  } while (*toys.optargs);

stop:
  tcsetattr(TT.cin_fd, TCSANOW, &TT.inf);
  fclose(cin);
  // Even if optarg not found, exit value still 0
  toys.exitval = 0;
}
