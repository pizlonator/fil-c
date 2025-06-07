#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct addrinfo hints;
    struct addrinfo** result = malloc(sizeof(struct addrinfo*));
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    free(opaque(result));
    getaddrinfo("localhost", "80", &hints, result);  // Use-after-free
    return 0;
}