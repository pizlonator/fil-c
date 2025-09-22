#include <stdio.h>
#include <stdlib.h>
#include "fileutils.h"

int main(int argc, char *argv[])
{
	atexit(close_stdout);
	printf("Hello, World!\n");
	return EXIT_SUCCESS;
}
