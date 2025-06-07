#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    struct sockaddr_in addr;
    bind(server_fd, (struct sockaddr*)(&addr + 1000), sizeof(addr));
    return 0;
}