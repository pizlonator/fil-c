#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    char buf[100];
    unsigned index;
    for (index = 100; index--;)
        buf[index] = (char)(index + 42);

    ZASSERT(send(socks[0], buf, strlen(buf), 0) > 0);

    recv(socks[1], (void*)0x1010101010101010UL, 50, 0);
    return 0;
}