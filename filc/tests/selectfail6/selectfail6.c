#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Use a special pointer (zweak_map_new()) as readfds
    select(1, (fd_set*)zweak_map_new(), NULL, NULL, NULL);
    
    return 0;
}
