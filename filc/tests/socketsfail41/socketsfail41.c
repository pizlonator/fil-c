#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    void* special = zweak_map_new();
    connect(fd, (struct sockaddr*)special, sizeof(struct sockaddr_in));
    return 0;
}