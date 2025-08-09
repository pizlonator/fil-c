#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#ifndef _MSC_VER
#include <iconv.h>
#endif


#include "xmldecl.h"

// Profiling	

#ifdef BUFFER_PROFILING
#include "../Profiling/BOM_Profiler.h"
BOM_Table * transcode_timer;
#endif


#define BUFFER_SIZE 2048

#define error_in_result(rslt) rslt == (size_t) -1
#define illegal_sequence_error(rslt) errno == EILSEQ
#define incomplete_sequence_error(rslt) errno == EINVAL


#define FAST_U8U16 1

// Include the conversion routines.
#include "libu8u16.c"

#include "xml_error.c"
#include "xmldecl.c"

unsigned char xml_decl_u16[12] = 
     {'\0', '<', '\0', '?', '\0', 'x', '\0', 'm', '\0', 'l', '\0', ' '};
unsigned char version[28] = 
     {'\0', 'v', '\0', 'e', '\0', 'r', '\0', 's', '\0', 'i', '\0', 'o', '\0', 'n', 
      '\0', '=', '\0', '"', '\0', '1', '\0', '.', '\0', '0', '\0', '"', '\0', ' '};
unsigned char encoding[34] = 
     {'\0', 'e', '\0', 'n', '\0', 'c', '\0', 'o', '\0', 'd',
      '\0', 'i', '\0', 'n', '\0', 'g', '\0', '=', '\0', '"', 
      '\0', 'u', '\0', 't', '\0', 'f', '\0', '-', '\0', '1', '\0', '6', '\0', '"'};
unsigned char standalone[34] = 
     {'\0', ' ', '\0', 's', '\0', 't', '\0', 'a', '\0', 'n',
      '\0', 'd', '\0', 'a', '\0', 'l', '\0', 'o', '\0', 'n', 
      '\0', 'e', '\0', '=', '\0', '"', '\0', 'y', '\0', 'e', '\0', 's', '\0', '"'};
unsigned char xml_decl_end[4] = {'\0', '?', '\0', '>'};



void do_UTF8toUTF16(FILE *infile, FILE *outfile) {

#ifdef __GNUC__
  unsigned char UTF8_buffer[BUFFER_SIZE] __attribute__((aligned(16)));
#endif
#ifdef _MSC_VER
  __declspec(align(16)) unsigned char UTF8_buffer[BUFFER_SIZE];
#endif
  unsigned short UTF16_buffer[BUFFER_SIZE];
  unsigned char * srcbuf_ptr = &UTF8_buffer[0];
  unsigned short * trgtbuf_ptr = &UTF16_buffer[0];
  size_t inbytes_left, outbytes_left;
  int file_position = 0;

  intptr_t chars_read, i, UTF16_chars;
    
   size_t rslt;

  chars_read = fread(&UTF8_buffer, 1, BUFFER_SIZE, infile);

  if (chars_read < 4) {
	fprintf(stderr, "Input document empty or too short\n");
	exit(-1);
  }

  Entity_Info * e = new Entity_Info;
  e->AnalyzeSignature(UTF8_buffer);
  XML_Decl_Parser decl_parser((unsigned char *)&UTF8_buffer);

  decl_parser.ReadXMLorTextDecl(*e);

  if (e->content_start != 0) {
	memmove(&UTF8_buffer[0], &UTF8_buffer[e->content_start], chars_read - e->content_start);
	if (chars_read == BUFFER_SIZE) {
		chars_read = BUFFER_SIZE - e->content_start + 
                             fread(&UTF8_buffer[BUFFER_SIZE-e->content_start], 1, e->content_start, infile);
	}
  }
  /* Write UTF-16 BOM */
  fputc(0xFE, outfile);
  fputc(0xFF, outfile);

  if (e->content_start > e->BOM_units) {
	// Duplicate original XML/text declaration with encoding="UTF-16"
	fwrite(&xml_decl_u16[0], 1, 12, outfile);
	if (e->version == XML_1_0)
		fwrite(&version[0], 1, 28, outfile);
	else if (e->version == XML_1_0) {
		fwrite(&version[0], 1, 23, outfile);
		fputc('1', outfile);
		fwrite(&version[24], 1, 4, outfile);
	}
	fwrite(&encoding[0], 1, 34, outfile);
	if (e->standalone == Standalone_yes)
		fwrite(&standalone[0], 1, 34, outfile);
	else if (e->standalone == Standalone_no) {
		fwrite(&standalone[0], 1, 27, outfile);
		fputc('n', outfile);
		fputc('\0', outfile);
		fputc('o', outfile);
		fwrite(&standalone[32], 1, 2, outfile);
	}
	fwrite(&xml_decl_end[0], 1, 4, outfile);
  }
  while (chars_read > 0) {
    srcbuf_ptr = &UTF8_buffer[0];
    trgtbuf_ptr = &UTF16_buffer[0];
    inbytes_left = chars_read;
    outbytes_left = BUFFER_SIZE*2;
   rslt = u8u16((char **) &srcbuf_ptr, 
                 &inbytes_left,
                 (char **) &trgtbuf_ptr, 
                 &outbytes_left);

    file_position += chars_read - inbytes_left;
    UTF16_chars = ((intptr_t) trgtbuf_ptr - (intptr_t) &UTF16_buffer[0])/2;
    fwrite(&UTF16_buffer, 2, UTF16_chars, outfile);
    if (error_in_result(rslt)) {
      if (illegal_sequence_error(rslt)) {
        int pos = 0;
        fprintf(stderr, "Illegal UTF-8 sequence at position %i in source.\n", file_position);
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
  fclose(infile);
  fclose(outfile);
}




int
main(int argc, char * argv[]) {
  if (argc < 2) {
    printf("Usage: %s <filename> [<filename.u16>].\n", argv[0]);
	  exit(-1);
  }
  char * filename = argv[1];
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

  return(0);
}
