#ifndef ICONV_UTF8LUT_HEADER
#define ICONV_UTF8LUT_HEADER

//cross-platform export/import macro for shared library
#ifdef _WIN32
    #if defined(ICONV_UTF8LUT_BUILD) || defined(iconv_u8l_EXPORTS)
        #define ICONV_UTF8LUT_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #else
        #define ICONV_UTF8LUT_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
#else
    #define ICONV_UTF8LUT_EXPORT __attribute__ ((visibility ("default")))
#endif

#include <stddef.h>

//global var "errno" used to report errors
#include <errno.h>
//any valid iconv_t descriptor is actually a pointer to BaseBufferProcessor object
typedef void *iconv_t;

#ifdef __cplusplus
extern "C" {
#endif

    // see manual at http://man7.org/linux/man-pages/man3/iconv_open.3.html
    // Supported conversion modes are: from "UTF-8" to ("UTF-16LE" or "UTF-32LE") and vice versa.
    //
    ICONV_UTF8LUT_EXPORT iconv_t iconv_open(const char *tocode, const char *fromcode);

    // see manual at http://man7.org/linux/man-pages/man3/iconv.3.html
    //
    // Breaking differences from the official iconv interface are:
    //  1. If input is fully converted without errors, then just "1" is returned
    //     (instead of the total number of characters converted).
    //  2. If output buffer is not big enough to hold converted data for any valid input of size *inbytesleft (plus overhead),
    //     then E2BIG verdict may be returned with considerable amount of input data left unprocessed (usually up to 64KB)
    //     (while original interface guarantees that maximal possible number of input characters is processed).
    //     In order to avoid this problem, allocate sufficiently large output buffer (see also p.3 below).
    //
    // Interface extension:
    //  3. If inbuf and *inbuf are NOT null, and either outbuf or *outbuf is null, then:
    //     maximal possible size of converted output (plus overhead) for input data of size *inbytesleft is stored into *outbytesleft.
    //     If you start conversion with the same value of *inbytesleft, and such a value of *outbytesleft (or greater),
    //     then the verdict E2BIG will surely NOT happen.
    //     Note: inbuf parameter is NOT used in this case, and no actual conversion happens.
    //
    // Here is a code sample showing how to allocate output buffer, so that E2BIG verdict does NOT happen (see p.2 and p.3):
    //     size_t inbufsize = {...}, outbufsize;
    //     char *outbuf = (char*)0xDEADBEEF;
    //     iconv(
    //       cd,           //conversion descriptor
    //       &outbuf,      //not used, but must be non-NULL
    //       &inbufsize,   //size of input buffer
    //       NULL,         //must be NULL
    //       &outbufsize   //correct size of output buffer would be here
    //     );
    //     outbuf = malloc(outbufsize);
    //     ...
    //     char *inbuf = {...};
    //     iconv(cd, &inbuf, &inbufsize, &outbuf, &outbufsize);
    //     ...
    //  
    ICONV_UTF8LUT_EXPORT size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);

    // see manual at http://man7.org/linux/man-pages/man3/iconv_close.3.html
    //
    ICONV_UTF8LUT_EXPORT int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif
