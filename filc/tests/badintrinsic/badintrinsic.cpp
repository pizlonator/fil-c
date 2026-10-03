int main()
{
    int x;
    // NOTE: This used to use coro_resume, but LLVM's coroutine lowering now runs right before the
    // pizlonator, so coro intrinsics never reach the pizlonator anymore. Instead we use clear_cache,
    // which is an intrinsic that the pizlonator does not support. If we ever implement clear_cache,
    // then we should find some other intrinsic that we don't support and add it here.
    //
    // Unless we really do implement all of them, but that seems unlikely. Surely there's some
    // inherently unsafe one?
    __builtin___clear_cache((char*)&x, (char*)&x + sizeof(int));
    return 0;
}

