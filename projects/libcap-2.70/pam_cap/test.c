#include <stdio.h>
#include <stdlib.h>
#include <security/pam_modules.h>

int main(int argc, char **argv)
{
    if (pam_sm_authenticate(NULL, 0, 0, NULL) != PAM_SUCCESS) {
	printf("failed to authenticate\n");
	exit(1);
    }
    exit(0);
}
