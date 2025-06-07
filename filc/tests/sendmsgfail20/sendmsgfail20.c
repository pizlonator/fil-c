#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (struct iovec*)0x1010101010101010UL;
    msg.msg_iovlen = 1;
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}