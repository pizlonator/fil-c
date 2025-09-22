/*
 * Unprivileged program that binds to port 80. It does this by
 * leveraging a file capable shared library.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "capso.h"

int main(int argc, char **argv) {
    int f = bind80("127.0.0.1");
    if (f < 0) {
	perror("unable to bind to port 80");
	exit(1);
    }
    if (listen(f, 10) == -1) {
	perror("unable to listen to port 80");
	exit(1);
    }
    printf("Webserver code to use filedes = %d goes here.\n"
	   "(Sleeping for 60s... Try 'netstat -tlnp|grep :80')\n", f);
    fflush(stdout);
    sleep(60);
    close(f);
    printf("Done.\n");
}
