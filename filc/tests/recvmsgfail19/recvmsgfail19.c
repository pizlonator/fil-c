#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include "utils.h"

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
    char* buf = opaque(malloc(100));
    buf -= (uintptr_t)buf;
    buf += UINTPTR_MAX - 10;
    vec.iov_base = buf;
    vec.iov_len = 50;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;

    recvmsg(socks[1], &msg, 0);
    return 0;
}