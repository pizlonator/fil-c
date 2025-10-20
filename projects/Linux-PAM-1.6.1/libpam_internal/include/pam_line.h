/* pam_line.h -- routine to parse configuration lines */

#ifndef PAM_LINE_H
#define PAM_LINE_H

#include "pam_inline.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct pam_line_buffer {
	char *assembled;
	char *chunk;
	size_t chunk_size;
	size_t len;
	size_t size;
};

void _pam_line_buffer_clear(struct pam_line_buffer *buffer);

void _pam_line_buffer_init(struct pam_line_buffer *buffer);

int _pam_line_assemble(FILE *f, struct pam_line_buffer *buffer, char repl);

#endif /* PAM_LINE_H */
