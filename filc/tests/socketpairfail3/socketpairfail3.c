#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int* p = malloc(2 * sizeof(int));
    socketpair(AF_UNIX, SOCK_STREAM, 0, p - 1000);
    return 0;
}