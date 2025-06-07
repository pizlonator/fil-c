#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int* p = malloc(0);  // Zero-sized allocation, not enough space for 2 ints
    socketpair(AF_UNIX, SOCK_STREAM, 0, p);
    return 0;
}