#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ZASSERT(server_fd > 2);
    
    int on = 666;
    socklen_t onlen = sizeof(on);
    getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on - 1000, &onlen);
    return 0;
}