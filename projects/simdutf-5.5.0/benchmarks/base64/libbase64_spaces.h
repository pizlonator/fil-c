
// https://github.com/aklomp/base64/blob/b20a31a997e0b48274fa09e58b65ee9202531e4f/bin/base64.c#L392
static inline size_t libbase64_find_space(const char *p, const size_t avail) {
  for (size_t len = 0; len < avail; len++) {
    if (p[len] == '\n' || p[len] == '\r' || p[len] == ' ' || p[len] == '\t') {
      return len;
    }
  }

  return avail;
}

// Inspired by
// https://github.com/aklomp/base64/blob/b20a31a997e0b48274fa09e58b65ee9202531e4f/bin/base64.c#L405
static bool libbase64_space_decode(const char *start, size_t avail, char *outbuf,
                   size_t *outlen) {
  struct base64_state state;
  *outlen = 0;

  // Initialize the decoder's state structure.
  base64_stream_decode_init(&state, 0);

  while (avail > 0) {
    size_t len = libbase64_find_space(start, avail);
    if (len == 0) {
      start++;
      avail--;
      continue;
    }

    // Decode the chunk into the raw buffer.
    size_t outlen = 0;
    if (base64_stream_decode(&state, start, len, outbuf, &outlen) == 0) {
      // decoding error
      return false;
    }

    // Update the output buffer pointer and total size.
    outbuf += outlen;
    outlen += outlen;
    if(avail == len) {
      break;
    }

    start += len + 1;
    avail -= len + 1;
  }
  return true;
}