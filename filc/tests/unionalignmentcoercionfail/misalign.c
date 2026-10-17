const void *misalign(const void *pointer)
{
    return (const unsigned char *)pointer + 1;
}
