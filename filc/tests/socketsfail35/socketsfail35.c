#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    struct sockaddr_in addr;
    connect(fd, (struct sockaddr*)(&addr + 1000), sizeof(addr));  // Above bounds
    return 0;
}