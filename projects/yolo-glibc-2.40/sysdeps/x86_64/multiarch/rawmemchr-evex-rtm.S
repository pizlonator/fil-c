#define RAWMEMCHR	__rawmemchr_evex_rtm

#define USE_IN_RTM	1
#define SECTION(p)	p##.evex.rtm

#include "rawmemchr-evex.S"
