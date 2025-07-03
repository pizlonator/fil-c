#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Use a special pointer (zweak_map_new()) as writefds
    select(1, NULL, (fd_set*)zweak_map_new(), NULL, NULL);
    
    return 0;
}
