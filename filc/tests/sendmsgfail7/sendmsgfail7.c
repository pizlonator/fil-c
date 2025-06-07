#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr* msg = (struct msghdr*)zweak_map_new();
    sendmsg(socks[0], msg, 0);
    return 0;
}