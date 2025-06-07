#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char* p = malloc(16);
    memset(p, 'a', 15);  // Fill with 'a', leave last byte uninitialized
    p[15] = '\0';        // Null terminate at end of buffer
    
    // Try to open with a path that's exactly at buffer boundary
    // This might cause issues if openat() tries to read past the string
    openat(AT_FDCWD, p, O_RDONLY);
    return 0;
}