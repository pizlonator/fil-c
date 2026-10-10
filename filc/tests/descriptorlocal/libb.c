__attribute__((noinline)) int which(void)
{
    return 2;
}

int (*whichptr(void))(void)
{
    return which;
}
