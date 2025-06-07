#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr msg;
    struct iovec vec;
    
    memset(&msg, 0, sizeof(msg));
    char* buf = malloc(16);
    vec.iov_base = buf + 1000;  // Way out of bounds
    vec.iov_len = 10;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}