#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));
    
    int flags = fcntl(socks[1], F_GETFL);
    ZASSERT(flags != -1);
    ZASSERT(fcntl(socks[1], F_SETFL, flags | O_NONBLOCK) == 0);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    
    struct iovec* iov = malloc(sizeof(struct iovec));
    free(iov);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    recvmsg(socks[1], &msg, 0);
    return 0;
}