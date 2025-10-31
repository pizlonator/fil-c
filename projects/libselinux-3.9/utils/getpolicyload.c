#include <stdio.h>
#include <stdlib.h>

#include <selinux/avc.h>


int main(int argc __attribute__ ((unused)),
         char* argv[] __attribute__ ((unused))) {
	int rc;

	/*
	* Do not use netlink as fallback, since selinux_status_policyload(3)
	* works only after a first message has been received.
	*/
	rc = selinux_status_open(/*fallback=*/0);
	if (rc < 0) {
		fprintf(stderr, "%s:  failed to open SELinux status map:  %m\n", argv[0]);
		return EXIT_FAILURE;
	}

	rc = selinux_status_policyload();
	if (rc < 0)
		fprintf(stderr, "%s:  failed to read policyload from SELinux status page:  %m\n", argv[0]);
	else
		printf("%d\n", rc);

	selinux_status_close();

	return (rc < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
