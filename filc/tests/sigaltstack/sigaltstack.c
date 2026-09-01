/* Registers an alternate signal stack, reads it back, then disables it.
   sigaltstack routes through syscall(SYS_sigaltstack) in libc. */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>

int main()
{
    size_t size = SIGSTKSZ;
    void* mem = malloc(size);
    ZASSERT(mem);

    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = mem;
    ss.ss_size = size;
    ss.ss_flags = 0;
    ZASSERT(sigaltstack(&ss, NULL) == 0);

    /* Read the current alternate stack back and check it matches. */
    stack_t old;
    memset(&old, 0, sizeof(old));
    ZASSERT(sigaltstack(NULL, &old) == 0);
    ZASSERT(old.ss_size == size);
    ZASSERT(!(old.ss_flags & SS_DISABLE));
    ZASSERT(old.ss_sp == mem);

    /* Disable it again. */
    stack_t dis;
    memset(&dis, 0, sizeof(dis));
    dis.ss_flags = SS_DISABLE;
    ZASSERT(sigaltstack(&dis, NULL) == 0);

    free(mem);
    return 0;
}
