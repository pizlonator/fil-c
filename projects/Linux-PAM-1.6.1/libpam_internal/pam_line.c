/* pam_line.c -- routine to parse configuration lines */

#include "config.h"

#include "security/_pam_macros.h"
#include "pam_line.h"

static int _pam_line_buffer_add(struct pam_line_buffer *buffer, char *start,
				char *end)
{
    size_t len = end - start;

    D(("assembled: [%zu/%zu] '%s', adding [%zu] '%s'",
	buffer->len, buffer->size,
	buffer->assembled == NULL ? "" : buffer->assembled, len, start));

    if (start == end)
	return 0;

    if (buffer->assembled == NULL && buffer->chunk == start) {
	/* no extra allocation needed, just move chunk to assembled */
	buffer->assembled = buffer->chunk;
	buffer->len = len;
	buffer->size = buffer->chunk_size;

	buffer->chunk = NULL;
	buffer->chunk_size = 0;

	D(("exiting with quick exchange"));
	return 0;
    }

    if (buffer->len + len + 1 > buffer->size) {
	size_t size;
	char *p;

	size = buffer->len + len + 1;
	if ((p = realloc(buffer->assembled, size)) == NULL)
		return -1;

	buffer->assembled = p;
	buffer->size = size;
    }

    memcpy(buffer->assembled + buffer->len, start, len);
    buffer->len += len;
    buffer->assembled[buffer->len] = '\0';

    D(("exiting"));
    return 0;
}

static inline int _pam_line_buffer_add_eol(struct pam_line_buffer *buffer,
					   char *start, char *end)
{
    if (buffer->assembled != NULL || (*start != '\0' && *start != '\n'))
	return _pam_line_buffer_add(buffer, start, end);
    return 0;
}

void _pam_line_buffer_clear(struct pam_line_buffer *buffer)
{
    pam_overwrite_n(buffer->assembled, buffer->size);
    _pam_drop(buffer->assembled);
    pam_overwrite_n(buffer->chunk, buffer->chunk_size);
    _pam_drop(buffer->chunk);
    buffer->chunk_size = 0;
    buffer->len = 0;
    buffer->size = 0;
}

void _pam_line_buffer_init(struct pam_line_buffer *buffer)
{
    buffer->assembled = NULL;
    buffer->chunk = NULL;
    _pam_line_buffer_clear(buffer);
}

static void _pam_line_buffer_purge(struct pam_line_buffer *buffer)
{
    pam_overwrite_n(buffer->chunk, buffer->chunk_size);
    _pam_drop(buffer->chunk);
    buffer->chunk_size = 0;
}

static void _pam_line_buffer_shift(struct pam_line_buffer *buffer)
{
    if (buffer->assembled == NULL)
	return;

    _pam_line_buffer_purge(buffer);
    buffer->chunk = buffer->assembled;
    buffer->chunk_size = buffer->size;

    buffer->assembled = NULL;
    buffer->size = 0;
    buffer->len = 0;
}

static inline int _pam_line_buffer_valid(struct pam_line_buffer *buffer)
{
    return buffer->assembled != NULL && *buffer->assembled != '\0';
}

/*
 * Trim string to relevant parts of a configuration line.
 *
 * Preceding whitespaces are skipped and comment (#) marks the end of
 * configuration line.
 *
 * Returns start of configuration line.
 */
static inline char *_pam_str_trim(char *str)
{
    /* skip leading spaces */
    str += strspn(str, " \t");
    /*
     * we are only interested in characters before the first '#'
     * character
     */
    str[strcspn(str, "#")] = '\0';

    return str;
}

/*
 * Remove escaped newline from end of string.
 *
 * Configuration lines may span across multiple lines in a file
 * by ending a line with a backslash (\).
 *
 * If an escaped newline is encountered, the backslash will be
 * replaced with "repl" and the newline itself removed.
 * Then the variable "end" will point to the new end of line.
 *
 * Returns 0 if escaped newline was found and replaced, 1 otherwise.
 */
static inline int _pam_str_unescnl(char *start, char **end, char repl)
{
    int ret = 1;
    char *p = *end;

    /*
     * Check for backslash by scanning back from the end of
     * the entered line, the '\n' should be included since
     * normally a line is terminated with this character.
     */
    while (p > start && ((*--p == ' ') || (*p == '\t') || (*p == '\n')))
	;
    if (*p == '\\') {
	*p = repl;          /* replace backslash with replacement char */
	if (repl != '\0') {
	    *++p = '\0';    /* truncate the line here if repl is not NUL */
	}
	*end = p;
	ret = 0;
    }

    return ret;
}

/*
 * Prepare line from file for configuration line parsing.
 *
 * A configuration line may span across multiple lines in a file.
 * Remove comments and skip preceding whitespaces.
 *
 * Returns 0 if line spans across multiple lines, 1 if
 * end of line is encountered.
 */
static inline int _pam_str_prepare(char *line, ssize_t len,
				   char **start, char **end, char repl)
{
    int ret;

    *start = line;
    *end = line + len;

    ret = _pam_str_unescnl(*start, end, repl) || strchr(*start, '#') != NULL;

    *start = _pam_str_trim(*start);

    return ret;
}

/*
 * This is where we read a line of the PAM config file. The line may be
 * preceded by lines of comments and also extended with "\\\n"
 *
 * The "repl" argument is used as replacement char for the backslash used
 * in newline escaping, i.e. in "\\\n".
 *
 * Returns 0 on EOF, 1 on successful line parsing, or -1 on error.
 */
int _pam_line_assemble(FILE *f, struct pam_line_buffer *buffer, char repl)
{
    int ret = 0;

    /* loop broken with a 'break' when a non-'\\n' ended line is read */

    D(("called."));

    _pam_line_buffer_shift(buffer);

    for (;;) {
	char *start, *end;
	ssize_t n;
	int eol;

	if ((n = getline(&buffer->chunk, &buffer->chunk_size, f)) == -1) {
	    if (ret) {
		/* Incomplete read */
		ret = -1;
	    } else {
		/* EOF */
		ret = 0;
	    }
	    break;
	}

	eol = _pam_str_prepare(buffer->chunk, n, &start, &end, repl);

	if (eol) {
	    if (_pam_line_buffer_add_eol(buffer, start, end)) {
		ret = -1;
		break;
	    }
	    if (_pam_line_buffer_valid(buffer)) {
		/* Successfully parsed a line */
		ret = 1;
		break;
	    }
	    /* Start parsing next line */
	    _pam_line_buffer_shift(buffer);
	    ret = 0;
	} else {
	    /* Configuration line spans across multiple lines in file */
	    if (_pam_line_buffer_add(buffer, start, end)) {
		ret = -1;
		break;
	    }
	    /* Keep parsing line */
	    ret = 1;
	}
    }

    if (ret == 1)
	_pam_line_buffer_purge(buffer);
    else
	_pam_line_buffer_clear(buffer);

    return ret;
}
