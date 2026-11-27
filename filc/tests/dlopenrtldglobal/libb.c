#include <stdarg.h>

const char* plugin_id(void)
{
    return "plugin_b";
}

int plugin_tag(int x)
{
    return x + 200;
}

int plugin_vsum(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int sum = 200;
    for (int i = 0; i < count; ++i)
        sum += va_arg(ap, int);
    va_end(ap);
    return sum;
}
