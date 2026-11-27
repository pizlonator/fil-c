#include <stdarg.h>

const char* plugin_id(void)
{
    return "plugin_a";
}

int plugin_tag(int x)
{
    return x + 100;
}

int plugin_vsum(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int sum = 100;
    for (int i = 0; i < count; ++i)
        sum += va_arg(ap, int);
    va_end(ap);
    return sum;
}
