#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pfd[2];
    ZASSERT(pipe(pfd) == 0);

    pid_t pid = fork();
    ZASSERT(pid >= 0);
    if (pid == 0) {
        close(pfd[0]);
        ZASSERT(dup2(pfd[1], STDOUT_FILENO) == STDOUT_FILENO);
        close(pfd[1]);

        /* First write to non-TTY stdout triggers __stdout_write's TIOCGWINSZ
           probe; errno must not be clobbered with ENOTTY on success. */
        errno = EINVAL;
        ZASSERT(printf("probe\n") == 6);
        ZASSERT(fflush(stdout) == 0);
        ZASSERT(errno == EINVAL);
        _exit(0);
    }

    close(pfd[1]);
    int status = 0;
    ZASSERT(waitpid(pid, &status, 0) == pid);
    close(pfd[0]);
    ZASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("stdouterrno ok\n");
    return 0;
}
