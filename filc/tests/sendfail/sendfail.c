#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    send(socks[0], NULL, 100, 0);
    return 0;
}