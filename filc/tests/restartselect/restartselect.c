/* This is a configure test in OpenSSH. At some point it started timing out, so now it's a
   regression test! */

# include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdfil.h>
static void sighandler(int sig) { }
int main()
{
    int r;
    pid_t pid;
    struct sigaction sa;

    zprintf("Starting restartselect test.\n");
    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESTART;
    (void)sigaction(SIGTERM, &sa, NULL);
    if ((pid = fork()) == 0) { /* child */
        pid = getppid();
        sleep(1);
        zprintf("Sending signal.\n");
        kill(pid, SIGTERM);
        sleep(1);
        if (getppid() == pid) { /* if parent did not exit, shoot it */
            /* FIXME: This can happen due to a rare race, and then the test claims to fail. */
            zprintf("Killing parent.\n");
            kill(pid, SIGKILL);
        }
        exit(0);
    } else { /* parent */
        zprintf("In parent.\n");
        r = select(0, NULL, NULL, NULL, NULL);
        zprintf("Returned r = %d, error = %s\n", r, strerror(errno));
    }
    exit(r == -1 ? 0 : 1);
}

