/* LLVMLookupIntrinsicID takes an explicit length and must work on buffers that
   are not NUL-terminated, including ones abutting a PROT_NONE guard page. */
#include <filc_llvm_jit.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    char* region = (char*)mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ZASSERT(region != MAP_FAILED);
    ZASSERT(mprotect(region + page, page, PROT_NONE) == 0);

    const char* ref = "llvm.memcpy";
    size_t len = strlen(ref);
    char* exact = region + page - len;
    memcpy(exact, ref, len);

    unsigned id_ref = LLVMLookupIntrinsicID(ref, len);
    unsigned id_exact = LLVMLookupIntrinsicID(exact, len);
    ZASSERT(id_ref != 0);
    ZASSERT(id_exact == id_ref);

    const char* miss = "llvm.not.an.intrinsic";
    size_t miss_len = strlen(miss);
    char* exact_miss = region + page - miss_len;
    memcpy(exact_miss, miss, miss_len);
    ZASSERT(LLVMLookupIntrinsicID(exact_miss, miss_len) == 0);

    ZASSERT(munmap(region, page * 2) == 0);
    printf("unterminated ok\n");
    return 0;
}
