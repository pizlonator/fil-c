#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    char buf[100];
    unsigned index;
    for (index = 100; index--;)
        buf[index] = (char)(index + 42);

    ZASSERT(send(socks[0], buf, 100, 0) == 100);
    
    char* array[100];
    for (index = 100; index--;)
        array[index] = "hello";
    ZASSERT(recv(socks[1], array, 100, 0) == 100);
    for (index = 100; index--;) {
        ZASSERT(((char*)array)[index] == (char)(index + 42));
        ZASSERT(!strcmp((char*)zgetlower(array[index]), "hello"));
    }
    for (index = (100 + sizeof(char*) - 1) / sizeof(char*) + 1; index < 100; ++index)
        ZASSERT(!strcmp(array[index], "hello"));
    return 0;
}