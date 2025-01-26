int punk(void);

asm(".filc_rename punk, sybir_punk");

int punk(void)
{
    return 13;
}

asm(".filc_alias sybir_punk, punk");

int funk(void)
{
    return punk();
}

