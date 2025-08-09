// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       fuzz_common.h
/// \brief      Common macros and functions needed by the fuzz targets
//
//  Authors:    Maksym Vatsyk
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "lzma.h"

// Some header values can make liblzma allocate a lot of RAM
// (up to about 4 GiB with liblzma 5.2.x). We set a limit here to
// prevent extreme allocations when fuzzing.
#define MEM_LIMIT (300 << 20) // 300 MiB


static void
fuzz_code(lzma_stream *stream, const uint8_t *inbuf, size_t inbuf_size) {
	// Output buffer for decompressed data. This is write only; nothing
	// cares about the actual data written here.
	uint8_t outbuf[4096];

	// Give the whole input buffer at once to liblzma.
	// Output buffer isn't initialized as liblzma only writes to it.
	stream->next_in = inbuf;
	stream->avail_in = inbuf_size;
	stream->next_out = outbuf;
	stream->avail_out = sizeof(outbuf);

	lzma_ret ret;
	while ((ret = lzma_code(stream, LZMA_FINISH)) == LZMA_OK) {
		if (stream->avail_out == 0) {
			// outbuf became full. We don't care about the
			// uncompressed data there, so we simply reuse
			// the outbuf and overwrite the old data.
			stream->next_out = outbuf;
			stream->avail_out = sizeof(outbuf);
		}
	}

	// LZMA_PROG_ERROR should never happen as long as the code calling
	// the liblzma functions is correct. Thus LZMA_PROG_ERROR is a sign
	// of a bug in either this function or in liblzma.
	if (ret == LZMA_PROG_ERROR) {
		fprintf(stderr, "lzma_code() returned LZMA_PROG_ERROR\n");
		abort();
	}
}
