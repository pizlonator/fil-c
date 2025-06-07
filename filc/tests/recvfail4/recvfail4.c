#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    char buf[100];
    unsigned index;
    for (index = 100; index--;)
        buf[index] = (char)(index + 66);

    ZASSERT(send(socks[0], buf, 100, 0) == 100);

    recv(socks[1], buf - 1000, 100, 0);
    return 0;
}