#ifndef UTIL_LINUX_SU_COMMON_H
#define UTIL_LINUX_SU_COMMON_H

enum {
	SU_MODE,
	RUNUSER_MODE
};

extern int su_main(int argc, char **argv, int mode);

#endif /* UTIL_LINUX_SU_COMMON */
