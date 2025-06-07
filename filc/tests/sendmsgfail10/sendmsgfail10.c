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
    
    char* ctrl = malloc(100);
    msg.msg_control = ctrl + 1000;
    msg.msg_controllen = 50;
    
    sendmsg(socks[0], &msg, 0);
    return 0;
}