#define _GNU_SOURCE

#include <pthread.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdfil.h>
#include <stdbool.h>

int port;
int server_fd;

static void init_port(void)
{
    for (port = 6666; ; port++) {
        struct sockaddr_in addr;
        socklen_t slen = sizeof(addr);
        bzero(&addr,slen);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        ZASSERT(server_fd > 2);
        int on = 1;
        ZASSERT(!setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
        on = 666;
        socklen_t onlen = sizeof(on);
        ZASSERT(!getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, &onlen));
        ZASSERT(on == 1);
        if (bind(server_fd, (struct sockaddr*)&addr, slen) < 0 ||
            listen(server_fd, 5) < 0) {
            zprintf("bind error: %s\n", strerror(errno));
            ZASSERT(errno == EADDRINUSE);
            close(server_fd);
            continue;
        }
        break;
    }
}

static void sendloop(int fd, char* data, size_t size)
{
    while (size) {
        ssize_t result = sendto(fd, data, size, 0, NULL, 0);
        zprintf("Wrote %ld bytes.\n", result);
        ZASSERT(result);
        if (result == -1) {
            ZASSERT(errno == EINTR);
            continue;
        }
        ZASSERT(result > 0);
        ZASSERT(result <= size);
        data += result;
        size -= result;
    }
}

static size_t recvloop(int fd, char* data, size_t size)
{
    size_t bytes_read = 0;
    while (size) {
        ssize_t result = recvfrom(fd, data, size, 0, NULL, NULL);
        zprintf("Read %ld bytes.\n", result);
        if (!result)
            return bytes_read;
        if (result == -1) {
            ZASSERT(errno == EINTR);
            continue;
        }
        ZASSERT(result > 0);
        ZASSERT(result <= size);
        data += result;
        size -= result;
        bytes_read += result;
    }
    return bytes_read;
}

static void* thread_main(void* arg)
{
    struct sockaddr_in addr;
    socklen_t slen = sizeof(addr);
    int fd = accept4(server_fd, (struct sockaddr*)&addr, &slen, 0);
    ZASSERT(fd > 2);
    int value;
    ZASSERT(recvloop(fd, (char*)&value, sizeof(value)) == sizeof(value));
    value += 666;
    sendloop(fd, (char*)&value, sizeof(value));
    close(fd);
    return NULL;
}

int main()
{
    init_port();
    pthread_t t;
    pthread_create(&t, NULL, thread_main, NULL);
    
    struct addrinfo hints;
    struct addrinfo *result;
    
    bzero(&hints,sizeof(hints));
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    
    ZASSERT(!getaddrinfo("localhost", zasprintf("%d", port), &hints, &result));

    struct addrinfo* cur;
    int fd;
    bool succeeded = false;
    for (cur = result; cur; cur = cur->ai_next) {
        fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        ZASSERT(fd > 2);

        if (!connect(fd, cur->ai_addr, cur->ai_addrlen)) {
            succeeded = true;
            break;
        }
        
        close(fd);
    }

    ZASSERT(succeeded);

    int value = 42;
    sendloop(fd, (char*)&value, sizeof(value));
    ZASSERT(recvloop(fd, (char*)&value, sizeof(value)) == sizeof(value));
    ZASSERT(value == 42 + 666);
    close(fd);

    pthread_join(t, NULL);

    return 0;
}

