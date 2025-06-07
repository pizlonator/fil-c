#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Cast an 8-byte string literal to int* (readonly memory)
    socketpair(AF_UNIX, SOCK_STREAM, 0, (int*)"12345678");
    return 0;
}