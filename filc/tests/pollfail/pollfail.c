#include <poll.h>
#include <stddef.h>
#include <stdfil.h>

int main()
{
    poll(NULL, 1, 0);
    return 0;
}