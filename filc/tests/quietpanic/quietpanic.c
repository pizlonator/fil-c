#include <stdfil.h>
#include <stdbool.h>

int main()
{
    ZASSERT(!zget_quiet_panic());
    zset_quiet_panic(true);
    ZASSERT(zget_quiet_panic());
    zset_quiet_panic(false);
    ZASSERT(!zget_quiet_panic());
    zset_quiet_panic(true);
    ZASSERT(zget_quiet_panic());
    zprintf("got this far\n");
    zerror("some kind of error");
    return 0;
}


