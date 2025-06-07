#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(fd > 2);
    
    connect(fd, NULL, sizeof(struct sockaddr_in));
    return 0;
}