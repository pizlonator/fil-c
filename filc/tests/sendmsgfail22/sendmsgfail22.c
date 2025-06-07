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
    
    struct iovec* iov = malloc(sizeof(struct iovec));
    msg.msg_iov = iov - 1000;
    msg.msg_iovlen = 1;
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}