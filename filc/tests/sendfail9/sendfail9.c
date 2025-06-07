#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    send(socks[0], (void*)0x4242424242424242UL, 50, 0);
    return 0;
}