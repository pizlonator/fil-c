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
    struct iovec vec;
    
    memset(&msg, 0, sizeof(msg));
    char* buf = malloc(16);
    vec.iov_base = buf + 1000;  // Way out of bounds
    vec.iov_len = 10;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;

    recvmsg(socks[1], &msg, 0);
    return 0;
}