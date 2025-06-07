#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    
    char* ctrl = opaque(malloc(100));
    ctrl -= (uintptr_t)ctrl;
    ctrl += UINTPTR_MAX - 10;
    msg.msg_control = ctrl;
    msg.msg_controllen = 50;
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}