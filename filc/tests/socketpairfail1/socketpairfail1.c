#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    socketpair(AF_UNIX, SOCK_STREAM, 0, NULL);
    return 0;
}