#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <selinux/selinux.h>

int main(void)
{
	return !is_selinux_enabled();
}
