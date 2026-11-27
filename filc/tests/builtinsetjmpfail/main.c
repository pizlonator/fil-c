void *buf[5];

int main(void)
{
    if (__builtin_setjmp(buf))
        return 1;
    __builtin_longjmp(buf, 1);
    return 0;
}
