#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32
	#include <getopt.h>
#else
	#ifndef _GETOPT_
	#define _GETOPT_

	#include <stdio.h>                  /* for EOF */ 
	#include <string.h>                 /* for strchr() */ 

	char *optarg = NULL;    /* pointer to the start of the option argument  */ 
	int   optind = 1;       /* number of the next argv[] to be evaluated    */ 
	int   opterr = 1;       /* non-zero if a question mark should be returned */

	int getopt(int argc, char *argv[], char *opstring); 
	#endif //_GETOPT_
#endif //WIN32

#ifdef __cplusplus
}
#endif

