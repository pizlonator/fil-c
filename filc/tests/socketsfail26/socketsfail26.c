#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    bind(server_fd, (struct sockaddr*)0x1010101010101010UL, sizeof(struct sockaddr_in));
    return 0;
}