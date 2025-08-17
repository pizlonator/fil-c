#include <sys/xattr.h>
#include "syscall.h"

ssize_t getxattr(const char *path, const char *name, void *value, size_t size)
{
	return zsys_getxattr(path, name, value, size);
}

ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size)
{
	return zsys_lgetxattr(path, name, value, size);
}

ssize_t fgetxattr(int filedes, const char *name, void *value, size_t size)
{
	return zsys_fgetxattr(filedes, name, value, size);
}

ssize_t listxattr(const char *path, char *list, size_t size)
{
	return zsys_listxattr(path, list, size);
}

ssize_t llistxattr(const char *path, char *list, size_t size)
{
	return zsys_llistxattr(path, list, size);
}

ssize_t flistxattr(int filedes, char *list, size_t size)
{
	return zsys_flistxattr(filedes, list, size);
}

int setxattr(const char *path, const char *name, const void *value, size_t size, int flags)
{
	return zsys_setxattr(path, name, value, size, flags);
}

int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags)
{
	return zsys_lsetxattr(path, name, value, size, flags);
}

int fsetxattr(int filedes, const char *name, const void *value, size_t size, int flags)
{
	return zsys_fsetxattr(filedes, name, value, size, flags);
}

int removexattr(const char *path, const char *name)
{
	return zsys_removexattr(path, name);
}

int lremovexattr(const char *path, const char *name)
{
	return zsys_lremovexattr(path, name);
}

int fremovexattr(int fd, const char *name)
{
	return zsys_fremovexattr(fd, name);
}
