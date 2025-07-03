#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Use a special pointer (zweak_map_new()) as exceptfds
    select(1, NULL, NULL, (fd_set*)zweak_map_new(), NULL);
    
    return 0;
}
