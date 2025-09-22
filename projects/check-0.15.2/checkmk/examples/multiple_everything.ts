/* Multiple everything... */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

void print_message(const char *msg, size_t msgsz)
{
    int nc;

    nc = printf("%s", msg);
    fail_unless(nc == msgsz, "failed to print completely: %s",
                strerror(errno));
}

# suite A Suite

# tcase A Test Case

# test hello_world
    const char msg[] = "Hello, world!\n";
    print_message(msg, sizeof msg - 1);

# test neverending_story
    const char msg[] = "Bastian Balthazar Bux\n";
    print_message(msg, sizeof msg - 1);

# tcase Another Test Case

# test math_problem
    fail_unless(1 + 1 == 2, "Something's broken...");

# suite Another Suite

# tcase A Test Case for Another Suite

# test more_math
    fail_unless(2/2 == 1, "Another weird math result");

# tcase A Basket Case

# test weave
    int i;
    const char msg[] = "###\n";

    for (i=0; i != 3; ++i)
        print_message(row, sizeof row - 1);
