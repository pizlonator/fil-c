/* init.c - init program.
 *
 * Copyright 2012 Harvind Singh <harvindsingh1981@gmail.com>
 * Copyright 2013 Kyungwan Han  <asura321@gmail.com>
 *
 * No Standard

USE_INIT(NEWTOY(init, "", TOYFLAG_SBIN))

config INIT
  bool "init"
  default n
  help
    usage: init

    System V style init.

    First program to run (as PID 1) when the system comes up, reading
    /etc/inittab to determine actions.
*/

#include "toys.h"
#include <sys/reboot.h>

struct action_list_seed {
  struct action_list_seed *next;
  pid_t pid;
  uint8_t action;
  char *terminal_name;
  char *command;
} *action_list_pointer = NULL;
int caught_signal;

//INITTAB action defination
#define SYSINIT     0x01
#define WAIT        0x02
#define ONCE        0x04
#define RESPAWN     0x08
#define ASKFIRST    0x10
#define CTRLALTDEL  0x20
#define SHUTDOWN    0x40
#define RESTART     0x80

static void initialize_console(void)
{
  int fd;
  char *p = getenv("CONSOLE");

  if (!p) p = getenv("console");
  if (!p) {
    fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
      while (fd < 2) fd = dup(fd);
      while (fd > 2) close(fd--);
    }
  } else {
    fd = open(p, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) printf("Unable to open console %s\n",p);
    else {
      dup2(fd,0);
      dup2(fd,1);
      dup2(fd,2);
    }
  }

  if (!getenv("TERM")) putenv("TERM=linux");
}

static void reset_term(int fd)
{
  struct termios terminal;

  tcgetattr(fd, &terminal);
  terminal.c_cc[VINTR] = 3;    //ctrl-c
  terminal.c_cc[VQUIT] = 28;   /*ctrl-\*/
  terminal.c_cc[VERASE] = 127; //ctrl-?
  terminal.c_cc[VKILL] = 21;   //ctrl-u
  terminal.c_cc[VEOF] = 4;     //ctrl-d
  terminal.c_cc[VSTART] = 17;  //ctrl-q
  terminal.c_cc[VSTOP] = 19;   //ctrl-s
  terminal.c_cc[VSUSP] = 26;   //ctrl-z

  terminal.c_line = 0;
  terminal.c_cflag &= CRTSCTS|PARODD|PARENB|CSTOPB|CSIZE|CBAUDEX|CBAUD;
  terminal.c_cflag |= CLOCAL|HUPCL|CREAD;

  //enable start/stop input and output control + map CR to NL on input
  terminal.c_iflag = IXON|IXOFF|ICRNL;

  //Map NL to CR-NL on output
  terminal.c_oflag = ONLCR|OPOST;
  terminal.c_lflag = IEXTEN|ECHOKE|ECHOCTL|ECHOK|ECHOE|ECHO|ICANON|ISIG;
  tcsetattr(fd, TCSANOW, &terminal);
}

static void add_new_action(int action, char *command, char *term)
{
  struct action_list_seed *x,**y;

  y = &action_list_pointer;
  x = *y;
  while (x) {
    if (!(strcmp(x->command, command)) && !(strcmp(x->terminal_name, term))) {
      *y = x->next; //remove from the list
      while(*y) y = &(*y)->next; //traverse through list till end
      x->next = NULL;
      break;
    }
    y = &(x)->next;
    x = *y;
  }

  //create a new node
  if (!x) {
    x = xzalloc(sizeof(*x));
    x->command = xstrdup(command);
    x->terminal_name = xstrdup(term);
  }
  x->action = action;
  *y = x;
}

static void parse_inittab(void)
{
  char *line = 0;
  size_t allocated_length = 0;
  ssize_t line_length = 0;
  int line_number = 0;
  char *act_name = "sysinit\0wait\0once\0respawn\0askfirst\0ctrlaltdel\0"
                    "shutdown\0restart\0";
  FILE *fp = fopen("/etc/inittab", "r");

  if (!fp) {
    error_msg("Unable to open /etc/inittab. Using Default inittab");
    add_new_action(SYSINIT, "/etc/init.d/rcS", "");
    add_new_action(RESPAWN, "/sbin/getty -n -l /bin/sh -L 115200 tty1 vt100", "");
    return;
  }

  while ((line_length = getline(&line, &allocated_length, fp)) > 0) {
    char *p = line, *x, *tty_name = 0, *command = 0, *extracted_token, *tmp;
    int action = 0, token_count = 0, i;

    if (p[line_length - 1] == '\n') p[line_length - 1] = '\0';
    if ((x = strchr(p, '#'))) *x = '\0';
    line_number++;
    action = 0;

    while ((extracted_token = strsep(&p,":"))) {
      token_count++;
      switch (token_count) {
        case 1:
          if (*extracted_token) {
            if (!strncmp(extracted_token, "/dev/", 5))
              tty_name = xmprintf("%s",extracted_token);
            else tty_name = xmprintf("/dev/%s",extracted_token);
          } else tty_name = xstrdup("");
          break;
        case 2:
          break;
        case 3:
          for (tmp = act_name, i = 0; *tmp; i++, tmp += strlen(tmp) +1) {
            if (!strcmp(tmp, extracted_token)) {
              action = 1 << i;
              break;
            }
          }
          if (!*tmp) error_msg("Invalid action at line number %d ---- ignoring",line_number);
          break;
        case 4:
          command = xstrdup(extracted_token);
          break;
        default:
          error_msg("Bad inittab entry at line %d", line_number);
          break;
      }
    }  //while token

    if (token_count == 4 && action) add_new_action(action, command, tty_name);
    free(tty_name);
    free(command);
  }
  free(line);
  fclose(fp);
}

static void reload_inittab(void)
{
  // Remove all inactive actions, then reload /etc/inittab
  struct action_list_seed **y;
  y = &action_list_pointer;
  while (*y) {
    if (!(*y)->pid) {
      struct action_list_seed *x = *y;
      free(x->terminal_name);
      free(x->command);
      *y = (*y)->next;
      free(x);
      continue;
    }
    y = &(*y)->next;
  }
  parse_inittab();
}

static void run_command(char *command)
{
  char *final_command[128];
  int hyphen = (command[0]=='-');

  command = command + hyphen;
  if (!strpbrk(command, "?<>'\";[]{}\\|=()*&^$!`~")) {
    char *next_command;
    char *extracted_command;
    int x = 0;

    next_command = strncpy(toybuf, command - hyphen, sizeof(toybuf));
    next_command[sizeof(toybuf) - 1] = toybuf[sizeof(toybuf) - 1 ] = '\0';
    command = next_command + hyphen;
    while ((extracted_command = strsep(&next_command," \t"))) {
      if (*extracted_command) {
        final_command[x] = extracted_command;
        x++;
      }
    }
    final_command[x] = NULL;
  } else {
    snprintf(toybuf, sizeof(toybuf), "exec %s", command);
    command = "-/bin/sh"+1;
    final_command[0] = ("-/bin/sh"+!hyphen);
    final_command[1] = "-c";
    final_command[2] = toybuf;
    final_command[3] = NULL;
  }
  if (hyphen) ioctl(0, TIOCSCTTY, 0);
  execvp(command, final_command);
  error_msg("unable to run %s",command);
}

//runs all same type of actions
static pid_t final_run(struct action_list_seed *x)
{
  pid_t pid;
  int fd;
  sigset_t signal_set;

  sigfillset(&signal_set);
  sigprocmask(SIG_BLOCK, &signal_set, NULL);
  if (x->action & ASKFIRST) pid = fork();
  else pid = vfork();

  if (pid > 0) {
    //parent process or error
    //unblock the signals
    sigfillset(&signal_set);
    sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

    return pid;
  } else if (pid < 0) {
    perror_msg("fork fail");
    sleep(1);
    return 0;
  }

  //new born child process
  sigset_t signal_set_c;
  sigfillset(&signal_set_c);
  sigprocmask(SIG_UNBLOCK, &signal_set_c, NULL);
  setsid(); //new session

  if (x->terminal_name[0]) {
    close(0);
    fd = open(x->terminal_name, (O_RDWR|O_NONBLOCK),0600);
    if (fd != 0) {
      error_msg("Unable to open %s,%s\n", x->terminal_name, strerror(errno));
      _exit(EXIT_FAILURE);
    } else {
      dup2(0, 1);
      dup2(0, 2);
    }
  }
  reset_term(0);
  run_command(x->command);
  _exit(-1);
}

static struct action_list_seed* mark_as_terminated_process(pid_t pid)
{
  struct action_list_seed *x;

  if (pid > 0) {
    for (x = action_list_pointer; x; x = x->next) {
      if (x->pid == pid) {
        x->pid = 0;
        return x;
      }
    }
  }

  return NULL;
}

static void waitforpid(pid_t pid)
{
  if (pid <= 0) return;

  while (!kill(pid, 0)) mark_as_terminated_process(wait(NULL));
}

static void run_action_from_list(int action)
{
  pid_t pid;
  struct action_list_seed *x = action_list_pointer;

  for (; x; x = x->next) {
    if (!(x->action & action)) continue;
    if (x->action & (SHUTDOWN|ONCE|SYSINIT|CTRLALTDEL|WAIT)) {
      pid = final_run(x);
      if (!pid) return;
      if (x->action & (SHUTDOWN|SYSINIT|CTRLALTDEL|WAIT)) waitforpid(pid);
    }
    if (x->action & (ASKFIRST|RESPAWN))
      if (!(x->pid)) x->pid = final_run(x);
  }
 }

static void set_default(void)
{
  sigset_t signal_set_c;

  xsignal_all_killers(SIG_DFL);
  sigfillset(&signal_set_c);
  sigprocmask(SIG_UNBLOCK,&signal_set_c, NULL);

  run_action_from_list(SHUTDOWN);
  error_msg("The system is going down NOW!");
  kill(-1, SIGTERM);
  error_msg("Sent SIGTERM to all processes");
  sync();
  sleep(1);
  kill(-1,SIGKILL);
  sync();
}

static void halt_poweroff_reboot_handler(int sig_no)
{
  unsigned int reboot_magic_no = 0;
  pid_t pid;

  set_default();

  switch (sig_no) {
    case SIGUSR1:
      error_msg("Requesting system halt");
      reboot_magic_no=RB_HALT_SYSTEM;
      break;
    case SIGUSR2:
      error_msg("Requesting system poweroff");
      reboot_magic_no=RB_POWER_OFF;
      break;
    case SIGTERM:
      error_msg("Requesting system reboot");
      reboot_magic_no=RB_AUTOBOOT;
      break;
    default:
      break;
  }

  sleep(1);
  pid = vfork();

  if (pid == 0) {
    reboot(reboot_magic_no);
    _exit(EXIT_SUCCESS);
  }

  while(1) sleep(1);
}

static void restart_init_handler(int sig_no)
{
  struct action_list_seed *x;
  pid_t pid;
  int fd;

  for (x = action_list_pointer; x; x = x->next) {
    if (!(x->action & RESTART)) continue;

    set_default();

    if (x->terminal_name[0]) {
      close(0);
      fd = open(x->terminal_name, (O_RDWR|O_NONBLOCK),0600);

      if (fd != 0) {
        error_msg("Unable to open %s,%s\n", x->terminal_name, strerror(errno));
        sleep(1);
        pid = vfork();

        if (pid == 0) {
          reboot(RB_HALT_SYSTEM);
          _exit(EXIT_SUCCESS);
        }

        while(1) sleep(1);
      } else {
        dup2(0, 1);
        dup2(0, 2);
        reset_term(0);
        run_command(x->command);
      }
    }
  }
}

static void catch_signal(int sig_no)
{
  caught_signal = sig_no;
  error_msg("signal seen: %d", sig_no);
}

static void pause_handler(int sig_no)
{
  int signal_backup,errno_backup;
  pid_t pid;

  errno_backup = errno;
  signal_backup = caught_signal;
  xsignal(SIGCONT, catch_signal);

  while(1) {
    if (caught_signal == SIGCONT) break;
    do pid = waitpid(-1,NULL,WNOHANG); while((pid==-1) && (errno=EINTR));
    mark_as_terminated_process(pid);
    sleep(1);
  }

  signal(SIGCONT, SIG_DFL);
  errno = errno_backup;
  caught_signal = signal_backup;
}

static int check_if_pending_signals(void)
{
  int signal_caught = 0;

  while(1) {
    int sig = caught_signal;
    if (!sig) return signal_caught;
    caught_signal = 0;
    signal_caught = 1;
    if (sig == SIGINT) run_action_from_list(CTRLALTDEL);
    else if (sig == SIGHUP) {
      error_msg("reloading inittab");
      reload_inittab();
    }
  }
}

void init_main(void)
{
  struct sigaction sig_act;

  if (getpid() != 1) error_exit("Already running");
  printf("Started init\n");
  initialize_console();
  reset_term(0);

  if (chdir("/")) perror_exit("Can't cd to /");
  setsid();

  putenv("HOME=/");
  putenv("PATH=/sbin:/usr/sbin:/bin:/usr/bin");
  putenv("SHELL=/bin/sh");
  putenv("USER=root");

  parse_inittab();
  xsignal(SIGUSR1, halt_poweroff_reboot_handler);//halt
  xsignal(SIGUSR2, halt_poweroff_reboot_handler);//poweroff
  xsignal(SIGTERM, halt_poweroff_reboot_handler);//reboot
  xsignal(SIGQUIT, restart_init_handler);//restart init
  memset(&sig_act, 0, sizeof(sig_act));
  sigfillset(&sig_act.sa_mask);
  sigdelset(&sig_act.sa_mask, SIGCONT);
  sig_act.sa_handler = pause_handler;
  sigaction(SIGTSTP, &sig_act, NULL);
  memset(&sig_act, 0, sizeof(sig_act));
  sig_act.sa_handler = catch_signal;
  sigaction(SIGINT, &sig_act, NULL);
  sigaction(SIGHUP, &sig_act, NULL);
  run_action_from_list(SYSINIT);
  check_if_pending_signals();
  run_action_from_list(WAIT);
  check_if_pending_signals();
  run_action_from_list(ONCE);
  while (1) {
    int suspected_WNOHANG = check_if_pending_signals();

    run_action_from_list(RESPAWN | ASKFIRST);
    suspected_WNOHANG = suspected_WNOHANG|check_if_pending_signals();
    sleep(1);//let cpu breath
    suspected_WNOHANG = suspected_WNOHANG|check_if_pending_signals();
    if (suspected_WNOHANG) suspected_WNOHANG=WNOHANG;

    while(1) {
      pid_t pid = waitpid(-1, NULL, suspected_WNOHANG);

      if (pid <= 0) break;
      mark_as_terminated_process(pid);
      suspected_WNOHANG = WNOHANG;
    }
  }
}
