#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdfil.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    connect(fd, (struct sockaddr*)0x1010101010101010UL, sizeof(struct sockaddr_in));
    return 0;
}