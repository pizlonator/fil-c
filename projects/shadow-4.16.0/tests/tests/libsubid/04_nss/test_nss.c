#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <prototypes.h>
#include <stdbool.h>
#include <dlfcn.h>

extern bool nss_is_initialized();
extern struct subid_nss_ops *get_subid_nss_handle();

void test1() {
	// nsswitch1 has no subid: entry
	setenv("LD_LIBRARY_PATH", ".", 1);
	printf("Test with no subid entry\n");
	nss_init("./nsswitch1.conf");
	if (!nss_is_initialized() || get_subid_nss_handle())
		exit(1);
	// second run should change nothing
	printf("Test with no subid entry, second run\n");
	nss_init("./nsswitch1.conf");
	if (!nss_is_initialized() || get_subid_nss_handle())
		exit(1);
}

void test2() {
	// nsswitch2 has a subid: files entry
	printf("test with 'files' subid entry\n");
	nss_init("./nsswitch2.conf");
	if (!nss_is_initialized() || get_subid_nss_handle())
		exit(1);
	// second run should change nothing
	printf("test with 'files' subid entry, second run\n");
	nss_init("./nsswitch2.conf");
	if (!nss_is_initialized() || get_subid_nss_handle())
		exit(1);
}

void test3() {
	// nsswitch3 has a subid: testnss entry
	printf("test with 'test' subid entry\n");
	nss_init("./nsswitch3.conf");
	if (!nss_is_initialized() || !get_subid_nss_handle())
		exit(1);
	// second run should change nothing
	printf("test with 'test' subid entry, second run\n");
	nss_init("./nsswitch3.conf");
	if (!nss_is_initialized() || !get_subid_nss_handle())
		exit(1);
}

const char *Prog;

int main(int argc, char *argv[])
{
	int which;

	Prog = Basename(argv[0]);

	if (argc < 1)
		exit(1);

	which = atoi(argv[1]);
	switch(which) {
	case 1: test1(); break;
	case 2: test2(); break;
	case 3: test3(); break;
	default: exit(1);
	}

	printf("nss parsing tests done\n");
	exit(0);
}
