#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr* msg = malloc(sizeof(struct msghdr));
    sendmsg(socks[0], msg - 1000, 0);
    return 0;
}