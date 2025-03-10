int main()
{
    int x;
    // NOTE: If we ever implement coro_resume, then we should find some other intrinsic that we don't
    // support and add it here.
    //
    // Unless we really do implement all of them, but that seems unlikely. Surely there's some
    // inherently unsafe one?
    __builtin_coro_resume(&x);
    return 0;
}

