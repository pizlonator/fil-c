#ifndef _NSCD_H_
#define _NSCD_H_

/*
 * nscd_flush_cache - flush specified service buffer in nscd cache
 */
#ifdef	USE_NSCD
extern int nscd_flush_cache (const char *service);
#else
#define nscd_flush_cache(service) (0)
#endif

#endif
