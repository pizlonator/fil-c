#include <string/memset.c>
#if IS_IN(rtld)
strong_alias (memset, __memset_ultra1)
#endif
