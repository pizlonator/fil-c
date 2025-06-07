#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    socklen_t onlen = sizeof(int);
    getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (void*)0x1010101010101010UL, &onlen);
    return 0;
}