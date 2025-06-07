#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdfil.h>

int main()
{
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    getaddrinfo("localhost", "80", &hints, NULL);
    return 0;
}