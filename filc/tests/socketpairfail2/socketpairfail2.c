#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    socketpair(AF_UNIX, SOCK_STREAM, 0, (int*)0x1010101010101010UL);
    return 0;
}