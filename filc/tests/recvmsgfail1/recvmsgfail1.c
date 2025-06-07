#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));
    
    // Set O_NONBLOCK to avoid hanging if safety checks pass
    int flags = fcntl(socks[1], F_GETFL);
    ZASSERT(flags != -1);
    ZASSERT(fcntl(socks[1], F_SETFL, flags | O_NONBLOCK) == 0);

    recvmsg(socks[1], NULL, 0);
    return 0;
}