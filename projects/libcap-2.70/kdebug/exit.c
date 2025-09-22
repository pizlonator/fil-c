/*
 * See https://stackoverflow.com/questions/42208228/how-to-automatically-close-the-execution-of-the-qemu-after-end-of-process
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <unistd.h>

#define SHUTDOWN_PORT 0x604
#define EXIT_PORT     0x501

static void clean_exit(void) {
    ioperm(SHUTDOWN_PORT, 16, 1);
    outw(0x2000, SHUTDOWN_PORT);
}

int main(int argc, char **argv) {
    int status;
    if (argc != 2) {
	clean_exit();
    }
    status = atoi(argv[1]);
    printf("exiting with status %d (in three seconds)\n", status);
    sleep(3);
    if (!status) {
	clean_exit();
    }
    ioperm(EXIT_PORT, 8, 1);
    /*
     * status returned is 1+(2*orig_status)
     */
    outb(status-1, EXIT_PORT);
    printf("didn't exit.. did you include '-device isa-debug-exit'"
	   " in qemu command?\n");
    exit(1);
}
