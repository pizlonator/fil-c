#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = NULL;
    msg.msg_iovlen = 1;  // Non-zero length with NULL pointer
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}