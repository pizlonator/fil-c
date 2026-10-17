/* Whole-object byval checks must not snapshot a larger union from a short
 * allocation. The typed descriptor also retains pointer-alignment checks. */
union Choice {
    int *pointer;
    unsigned long words[3];
};

__attribute__((noinline)) int consume(union Choice value)
{
    return *value.pointer;
}

static const unsigned long short_object = 0;
int main(void)
{
    return consume(*(const union Choice *)&short_object);
}
