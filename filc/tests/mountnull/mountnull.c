/* mount(2) with NULL source and NULL fs_type must pass through the runtime's
   syscall checking instead of panicking. Remounts, bind mounts and propagation
   changes pass NULL for both. The syscall itself must still fail gracefully
   (we point it at a target that cannot be a mountpoint). */

#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <sys/mount.h>

int main(void)
{
    errno = 0;
    int rc = mount(NULL, "/nonexistent-filc-mount-test-target", NULL, MS_SILENT, NULL);
    printf("mount null rc %d errno %d\n", rc, errno);
    ZASSERT(rc == -1);
    ZASSERT(errno != 0);
    printf("mount null ok\n");
    return 0;
}
