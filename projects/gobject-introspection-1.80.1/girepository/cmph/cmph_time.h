#ifdef ELAPSED_TIME_IN_SECONDS
#undef ELAPSED_TIME_IN_SECONDS
#endif

#ifdef ELAPSED_TIME_IN_uSECONDS
#undef ELAPSED_TIME_IN_uSECONDS
#endif

#ifdef __GNUC__
	#include <sys/time.h>
	#ifndef WIN32
		#include <sys/resource.h>
	#endif
#endif

#ifdef __GNUC__
	#ifndef __CMPH_TIME_H__
		#define __CMPH_TIME_H__
		static inline void elapsed_time_in_seconds(double * elapsed_time)
		{
			struct timeval e_time;
			if (gettimeofday(&e_time, NULL) < 0) {
				return;
			}
			*elapsed_time =  (double)e_time.tv_sec + ((double)e_time.tv_usec/1000000.0);
		}
		static inline void dummy_elapsed_time_in_seconds(double * elapsed_time)
		{
                  (void) elapsed_time;
		}
		static inline void elapsed_time_in_useconds(cmph_uint64 * elapsed_time)
		{
			struct timeval e_time;
			if (gettimeofday(&e_time, NULL) < 0) {
				return;
			}
			*elapsed_time =  (cmph_uint64)(e_time.tv_sec*1000000 + e_time.tv_usec);
		}
		static inline void dummy_elapsed_time_in_useconds(cmph_uint64 * elapsed_time)
		{
                  (void) elapsed_time;
		}	
	#endif
#endif

#ifdef CMPH_TIMING
	  #ifdef __GNUC__
		  #define ELAPSED_TIME_IN_SECONDS elapsed_time_in_seconds
		  #define ELAPSED_TIME_IN_uSECONDS elapsed_time_in_useconds
	  #else
		  #define ELAPSED_TIME_IN_SECONDS dummy_elapsed_time_in_seconds
		  #define ELAPSED_TIME_IN_uSECONDS dummy_elapsed_time_in_useconds
	  #endif
#else
	  #ifdef __GNUC__
		  #define ELAPSED_TIME_IN_SECONDS
		  #define ELAPSED_TIME_IN_uSECONDS
	  #else
		  #define ELAPSED_TIME_IN_SECONDS dummy_elapsed_time_in_seconds
		  #define ELAPSED_TIME_IN_uSECONDS dummy_elapsed_time_in_useconds
	  #endif
#endif
