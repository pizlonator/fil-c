#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#ifndef UNICODE_CONVERT
#ifndef _MSC_VER
#include <iconv.h>
#endif
#endif

#ifdef _MSC_VER
//#undef BUFFER_PROFILING
#undef STD_ICONV
#endif
// Profiling

#ifdef BUFFER_PROFILING
#include "../Profiling/BOM_Profiler.h"
BOM_Table * transcode_timer;
#endif

/* Counting the number of blocks classified by the
   maxiumum UTF-8 sequence length within the block. */
#ifdef BLOCK_COUNTING
int block_counts[4] = {0,0,0,0};
#endif

#define BUFFER_SIZE 2048
#define U16BUFFER_UNITS (BUFFER_SIZE + 16)

#ifdef UNICODE_ORG_CONVERT
#undef STD_ICONV
#undef FAST_U8U16
#define error_in_result(rslt) rslt != conversionOK
#define illegal_sequence_error(rslt) rslt == sourceIllegal
#define incomplete_sequence_error(rslt) rslt == sourceExhausted
#endif

#ifndef UNICODE_ORG_CONVERT
#define error_in_result(rslt) rslt == (size_t) -1
#define illegal_sequence_error(rslt) u8u16_errno == EILSEQ
#define incomplete_sequence_error(rslt) u8u16_errno == EINVAL


#ifdef STD_ICONV
#undef FAST_U8U16
#endif
#ifndef STD_ICONV
#define FAST_U8U16 1
#endif
#endif

// Include the conversion routines.
#ifdef UNICODE_ORG_CONVERT
//#include "Unicode.org/ConvertUTF.h"
#include "Unicode.org/ConvertUTF.c"
#endif
#ifdef FAST_U8U16
#include "libu8u16.c"
#endif


void do_UTF8toUTF16(FILE *infile, FILE *outfile) {

#ifdef __GNUC__
  unsigned char UTF8_buffer[BUFFER_SIZE] __attribute__((aligned(16)));
#endif
#ifdef _MSC_VER
  __declspec(align(16)) unsigned char UTF8_buffer[BUFFER_SIZE];
#endif
  unsigned short UTF16_buffer[U16BUFFER_UNITS];
  unsigned char * srcbuf_ptr = &UTF8_buffer[0];
  unsigned short * trgtbuf_ptr = &UTF16_buffer[0];
  size_t inbytes_left, outbytes_left;
  int file_position = 0;

  intptr_t chars_read, i, UTF16_chars;
  int u8u16_errno;
    
#ifdef UNICODE_ORG_CONVERT
   ConversionResult rslt;
#endif
#ifdef STD_ICONV
   iconv_t cd = iconv_open("UTF-16BE", "UTF-8");
   size_t rslt;
#endif
#ifdef FAST_U8U16
   size_t rslt;
#endif

  chars_read = fread(&UTF8_buffer, 1, BUFFER_SIZE, infile);

  while (chars_read > 0) {
#ifdef BUFFER_PROFILING
    start_BOM_interval(transcode_timer);
#endif 
    srcbuf_ptr = &UTF8_buffer[0];
    trgtbuf_ptr = &UTF16_buffer[0];
    inbytes_left = chars_read;
    outbytes_left = U16BUFFER_UNITS*2;
#ifdef STD_ICONV
    rslt = iconv(cd,
                 (char **) &srcbuf_ptr, 
                 &inbytes_left,
                 (char **) &trgtbuf_ptr, 
                 &outbytes_left);
    u8u16_errno = errno;
#endif
#ifdef UNICODE_ORG_CONVERT
    rslt = ConvertUTF8toUTF16((const UTF8**) &srcbuf_ptr, 
                              srcbuf_ptr + chars_read,
                              &trgtbuf_ptr, 
                              &UTF16_buffer[BUFFER_SIZE],
                              strictConversion);
    inbytes_left = chars_read - (srcbuf_ptr - &UTF8_buffer[0]);
#endif
#ifdef FAST_U8U16
#ifndef BUFFERED_U8U16
    rslt = u8u16((char **) &srcbuf_ptr, 
                 &inbytes_left,
                 (char **) &trgtbuf_ptr, 
                 &outbytes_left);
#endif
#ifdef BUFFERED_U8U16
    rslt = buffered_u8u16((char **) &srcbuf_ptr, 
                 &inbytes_left,
                 (char **) &trgtbuf_ptr, 
                 &outbytes_left);
#endif
    u8u16_errno = errno;
#endif

#ifdef BUFFER_PROFILING
     end_BOM_interval(transcode_timer, chars_read - inbytes_left);
#endif 
    file_position += chars_read - inbytes_left;
    UTF16_chars = ((intptr_t) trgtbuf_ptr - (intptr_t) &UTF16_buffer[0])/2;
    fwrite(&UTF16_buffer, 2, UTF16_chars, outfile);
    if (error_in_result(rslt)) {
      if (illegal_sequence_error(rslt)) {
        int pos = 0;
        fprintf(stderr, "Illegal UTF-8 sequence at position %i in source.\n", file_position);
#ifdef DEBUG_ERROR
        for (pos =0; (pos < 5) && (pos < inbytes_left); pos++) {
          fprintf(stderr, " %02X", (unsigned char) srcbuf_ptr[pos]);
        }
        fprintf(stderr, "\n");
#endif
        fclose(infile);
        fclose(outfile);
        exit(-1);
      }
      if (!incomplete_sequence_error(rslt)) {
        fprintf(stderr, "Unknown error %i at position %i in source.\n", errno, file_position);
        fclose(infile);
        fclose(outfile);
        exit(-1);
      }
      // errno == EINVAL  or rslt == sourceExhausted
      // Incomplete sequence at end of input buffer. 
      if (chars_read < BUFFER_SIZE) {
        int pos = 0;
        fprintf(stderr, "EOF with incomplete UTF-8 sequence at position %i in source.\n", file_position);
#ifdef DEBUG_ERROR
        for (pos =0; (pos < 5) && (pos < inbytes_left); pos++) {
          fprintf(stderr, " %02X", (unsigned char) srcbuf_ptr[pos]);
        }
        fprintf(stderr, "\n");
#endif
        fclose(infile);
        fclose(outfile);
        exit(-1);
      }
      // Move unprocessed characters to beginning.
      for (i = 0; i < inbytes_left; i++) {
        UTF8_buffer[i] = UTF8_buffer[chars_read - inbytes_left + i];
      }

    }
    chars_read = fread(&UTF8_buffer[inbytes_left], 1, BUFFER_SIZE-inbytes_left, infile);
    chars_read += inbytes_left;
  }
#ifdef STD_ICONV
  iconv_close(cd);
#endif
  fclose(infile);
  fclose(outfile);
}




int
main(int argc, char * argv[]) {
  if (argc < 2) {
    printf("Usage: %s <filename.u8> [<filename.u16>].\n", argv[0]);
	  exit(-1);
  }
  char * filename = argv[1];
#ifdef BUFFER_PROFILING
  transcode_timer = init_BOM_timer();
#endif 
  FILE *infile, *outfile;
  infile = fopen(filename, "rb");
  if (!infile) {
      fprintf(stderr, "Error: cannot open %s for input.\n", filename);
      exit(-1);
  }

  if (argc < 3) outfile = stdout;
  else {
    outfile = fopen(argv[2], "wb");
    if (!outfile) {
      fprintf(stderr, "Error: cannot open %s for writing.\n", argv[2]);
      exit(-1);
    }
  }

  do_UTF8toUTF16(infile, outfile);

#ifdef BUFFER_PROFILING
  printf("Buffer conversion timing.\n");
  dump_BOM_table(transcode_timer);
#endif
#ifdef BLOCK_COUNTING
printf("%i bytes in pure ASCII blocks.\n", block_counts[0]);
printf("%i bytes in blocks confined to two-byte subplane.\n", block_counts[1]);
printf("%i bytes in blocks confined to basic multilingual plane.\n", block_counts[2]);
printf("%i bytes in blocks containing 4-byte UTF-8 sequences.\n", block_counts[3]);
#endif
  return(0);
}
