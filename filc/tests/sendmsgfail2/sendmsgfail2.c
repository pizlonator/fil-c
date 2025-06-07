#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    sendmsg(socks[0], (struct msghdr*)0x1010101010101010UL, 0);
    return 0;
}