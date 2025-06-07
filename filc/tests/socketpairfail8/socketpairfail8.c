#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int* p = (int*)zweak_map_new();
    socketpair(AF_UNIX, SOCK_STREAM, 0, p);
    return 0;
}