#define _GNU_SOURCE
#include <poll.h>
#include <stddef.h>
#include <stdfil.h>

int main()
{
    ppoll(NULL, 1, NULL, NULL);
    return 0;
}