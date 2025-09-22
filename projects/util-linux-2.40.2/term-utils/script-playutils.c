#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "c.h"
#include "xalloc.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "script-playutils.h"

UL_DEBUG_DEFINE_MASK(scriptreplay);
UL_DEBUG_DEFINE_MASKNAMES(scriptreplay) = UL_DEBUG_EMPTY_MASKNAMES;

#define DBG(m, x)       __UL_DBG(scriptreplay, SCRIPTREPLAY_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(scriptreplay, SCRIPTREPLAY_DEBUG_, m, x)

/*
 * The script replay is driven by timing file where each entry describes one
 * step in the replay. The timing step may refer input or output (or
 * signal, extra information, etc.)
 *
 * The step data are stored in log files, the right log file for the step is
 * selected from replay_setup.
 */
enum {
	REPLAY_TIMING_SIMPLE,		/* timing info in classic "<delta> <offset>" format */
	REPLAY_TIMING_MULTI		/* multiple streams in format "<type> <delta> <offset|etc> */
};

struct replay_log {
	const char	*streams;	/* 'I'nput, 'O'utput or both */
	const char	*filename;
	FILE		*fp;

	unsigned int	noseek : 1;	/* do not seek in this log */
};

struct replay_step {
	char	type;		/* 'I'nput, 'O'utput, ... */
	size_t	size;

	char	*name;		/* signals / headers */
	char	*value;

	struct timeval delay;
	struct replay_log *data;
};

struct replay_setup {
	struct replay_log	*logs;
	size_t			nlogs;

	struct replay_step	step;	/* current step */

	FILE			*timing_fp;
	const char		*timing_filename;
	int			timing_format;
	int			timing_line;

	struct timeval		delay_max;
	struct timeval		delay_min;
	double			delay_div;

	char			default_type;	/* type for REPLAY_TIMING_SIMPLE */
	int			crmode;
};

void replay_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(scriptreplay, SCRIPTREPLAY_DEBUG_, 0, SCRIPTREPLAY_DEBUG);
}

static int ignore_line(FILE *f)
{
	int c;

	while((c = fgetc(f)) != EOF && c != '\n');
	if (ferror(f))
		return -errno;

	DBG(LOG, ul_debug("  ignore line"));
	return 0;
}

/* incretemt @a by @b */
static inline void timerinc(struct timeval *a, struct timeval *b)
{
	struct timeval res;

	timeradd(a, b, &res);
	a->tv_sec = res.tv_sec;
	a->tv_usec = res.tv_usec;
}

struct replay_setup *replay_new_setup(void)
{
	return  xcalloc(1, sizeof(struct replay_setup));
}

void replay_free_setup(struct replay_setup *stp)
{
	if (!stp)
		return;

	free(stp->logs);
	free(stp->step.name);
	free(stp->step.value);
	free(stp);
}

/* if timing file does not contains types of entries (old format) than use this
 * type as the default */
int replay_set_default_type(struct replay_setup *stp, char type)
{
	assert(stp);
	stp->default_type = type;

	return 0;
}

int replay_set_crmode(struct replay_setup *stp, int mode)
{
	assert(stp);
	stp->crmode = mode;

	return 0;
}

int replay_set_delay_min(struct replay_setup *stp, const struct timeval *tv)
{
	stp->delay_min.tv_sec = tv->tv_sec;
	stp->delay_min.tv_usec = tv->tv_usec;
	return 0;
}

int replay_set_delay_max(struct replay_setup *stp, const struct timeval *tv)
{
	stp->delay_max.tv_sec = tv->tv_sec;
	stp->delay_max.tv_usec = tv->tv_usec;
	return 0;
}

int replay_set_delay_div(struct replay_setup *stp, const double divi)
{
	stp->delay_div = divi;
	return 0;
}

static struct replay_log *replay_new_log(struct replay_setup *stp,
					 const char *streams,
					 const char *filename,
					 FILE *f)
{
	struct replay_log *log;

	assert(stp);
	assert(streams);
	assert(filename);

	stp->logs = xreallocarray(stp->logs, stp->nlogs + 1,  sizeof(*log));
	log = &stp->logs[stp->nlogs];
	stp->nlogs++;

	memset(log, 0, sizeof(*log));
	log->filename = filename;
	log->streams = streams;
	log->fp = f;

	return log;
}

int replay_set_timing_file(struct replay_setup *stp, const char *filename)
{
	int c, rc = 0;

	assert(stp);
	assert(filename);

	stp->timing_filename = filename;
	stp->timing_line = 0;

	stp->timing_fp = fopen(filename, "r");
	if (!stp->timing_fp)
		rc = -errno;
	else {
		/* detect timing file format */
		c = fgetc(stp->timing_fp);
		if (c != EOF) {
			if (isdigit((unsigned int) c))
				stp->timing_format = REPLAY_TIMING_SIMPLE;
			else
				stp->timing_format = REPLAY_TIMING_MULTI;
			ungetc(c, stp->timing_fp);
		} else if (ferror(stp->timing_fp))
			rc = -errno;
	}

	if (rc && stp->timing_fp) {
		fclose(stp->timing_fp);
		stp->timing_fp = NULL;
	}

	/* create quasi-log for signals, headers, etc. */
	if (rc == 0 && stp->timing_format == REPLAY_TIMING_MULTI) {
		struct replay_log *log = replay_new_log(stp, "SH",
						filename, stp->timing_fp);
		if (!log)
			rc = -ENOMEM;
		else {
			log->noseek = 1;
			DBG(LOG, ul_debug("associate file '%s' for streams 'SH'", filename));
		}
	}

	DBG(TIMING, ul_debug("timing file set to '%s' [rc=%d]", filename, rc));
	return rc;
}

const char *replay_get_timing_file(struct replay_setup *setup)
{
	assert(setup);
	return setup->timing_filename;
}

int replay_get_timing_line(struct replay_setup *setup)
{
	assert(setup);
	return setup->timing_line;
}

int replay_associate_log(struct replay_setup *stp,
			const char *streams, const char *filename)
{
	FILE *f;
	int rc;

	assert(stp);
	assert(streams);
	assert(filename);

	/* open the file and skip the first line */
	f = fopen(filename, "r");
	rc = f == NULL ? -errno : ignore_line(f);

	if (rc == 0)
		replay_new_log(stp, streams, filename, f);
	else if (f)
		fclose(f);

	DBG(LOG, ul_debug("associate log file '%s', streams '%s' [rc=%d]", filename, streams, rc));
	return rc;
}

static int is_wanted_stream(char type, const char *streams)
{
	if (streams == NULL)
		return 1;
	if (strchr(streams, type))
		return 1;
	return 0;
}

static void replay_reset_step(struct replay_step *step)
{
	assert(step);

	step->size = 0;
	step->data = NULL;
	step->type = 0;
	timerclear(&step->delay);
}

struct timeval *replay_step_get_delay(struct replay_step *step)
{
	assert(step);
	return &step->delay;
}

/* current data log file */
const char *replay_step_get_filename(struct replay_step *step)
{
	assert(step);
	return step->data->filename;
}

int replay_step_is_empty(struct replay_step *step)
{
	assert(step);
	return step->size == 0 && step->type == 0;
}


static int read_multistream_step(struct replay_step *step, FILE *f, char type)
{
	int rc = 0;
	char nl;
	int64_t sec = 0, usec = 0;

	switch (type) {
	case 'O': /* output */
	case 'I': /* input */
		rc = fscanf(f, "%"SCNd64".%06"SCNd64" %zu%c\n",
				&sec, &usec, &step->size, &nl);
		if (rc != 4 || nl != '\n')
			rc = -EINVAL;
		else
			rc = 0;

		step->delay.tv_sec = (time_t) sec;
		step->delay.tv_usec = (suseconds_t) usec;
		break;

	case 'S': /* signal */
	case 'H': /* header */
	{
		char buf[BUFSIZ];

		rc = fscanf(f, "%"SCNd64".%06"SCNd64" ",
				&sec, &usec);
		if (rc != 2)
			break;

		step->delay.tv_sec = (time_t) sec;
		step->delay.tv_usec = (suseconds_t) usec;

		rc = fscanf(f, "%128s", buf);		/* name */
		if (rc != 1)
			break;
		step->name = strrealloc(step->name, buf);
		if (!step->name)
			err_oom();

		if (!fgets(buf, sizeof(buf), f)) {	/* value */
			rc = -errno;
			break;
		}
		if (*buf) {
			strrem(buf, '\n');
			step->value = strrealloc(step->value, buf);
			if (!step->value)
				err_oom();
		}
		rc = 0;
		break;
	}
	default:
		break;
	}

	DBG(TIMING, ul_debug(" read step delay & size [rc=%d]", rc));
	return rc;
}

static struct replay_log *replay_get_stream_log(struct replay_setup *stp, char stream)
{
	size_t i;

	for (i = 0; i < stp->nlogs; i++) {
		struct replay_log *log = &stp->logs[i];

		if (is_wanted_stream(stream, log->streams))
			return log;
	}
	return NULL;
}

static int replay_seek_log(struct replay_log *log, size_t move)
{
	if (log->noseek)
		return 0;
	DBG(LOG, ul_debug(" %s: seek ++ %zu", log->filename, move));
	return fseek(log->fp, move, SEEK_CUR) == (off_t) -1 ? -errno : 0;
}

/* returns next step with pointer to the right log file for specified streams (e.g.
 * "IOS" for in/out/signals) or all streams if stream is NULL.
 *
 * returns: 0 = success, <0 = error, 1 = done (EOF)
 */
int replay_get_next_step(struct replay_setup *stp, char *streams, struct replay_step **xstep)
{
	struct replay_step *step;
	int rc;
	struct timeval ignored_delay;

	assert(stp);
	assert(stp->timing_fp);
	assert(xstep);

	step = &stp->step;
	*xstep = NULL;

	timerclear(&ignored_delay);

	do {
		struct replay_log *log = NULL;

		rc = 1;	/* done */
		if (feof(stp->timing_fp))
			break;

		DBG(TIMING, ul_debug("reading next step"));

		replay_reset_step(step);
		stp->timing_line++;

		switch (stp->timing_format) {
		case REPLAY_TIMING_SIMPLE:
			/* old format is the same as new format, but without <type> prefix */
			rc = read_multistream_step(step, stp->timing_fp, stp->default_type);
			if (rc == 0)
				step->type = stp->default_type;
			break;
		case REPLAY_TIMING_MULTI:
			rc = fscanf(stp->timing_fp, "%c ", &step->type);
			if (rc != 1)
				rc = -EINVAL;
			else
				rc = read_multistream_step(step,
						stp->timing_fp,
						step->type);
			break;
		}

		if (rc) {
			if (rc < 0 && feof(stp->timing_fp))
				rc = 1;
			break;		/* error or EOF */
		}

		DBG(TIMING, ul_debug(" step entry is '%c'", step->type));

		log = replay_get_stream_log(stp, step->type);
		if (log) {
			if (is_wanted_stream(step->type, streams)) {
				step->data = log;
				*xstep = step;
				DBG(LOG, ul_debug(" use %s as data source", log->filename));
				goto done;
			}
			/* The step entry is unwanted, but we keep the right
			 * position in the log file although the data are ignored.
			 */
			replay_seek_log(log, step->size);
		} else
			DBG(TIMING, ul_debug(" not found log for '%c' stream", step->type));

		DBG(TIMING, ul_debug(" ignore step '%c' [delay=%"PRId64".%06"PRId64"]",
				step->type,
				(int64_t) step->delay.tv_sec,
				(int64_t) step->delay.tv_usec));

		timerinc(&ignored_delay, &step->delay);
	} while (rc == 0);

done:
	if (timerisset(&ignored_delay))
		timerinc(&step->delay, &ignored_delay);

	DBG(TIMING, ul_debug("reading next step done [rc=%d delay=%"PRId64".%06"PRId64
			     "(ignored=%"PRId64".%06"PRId64") size=%zu]",
			rc,
			(int64_t) step->delay.tv_sec, (int64_t) step->delay.tv_usec,
			(int64_t) ignored_delay.tv_sec, (int64_t) ignored_delay.tv_usec,
			step->size));

	/* normalize delay */
	if (stp->delay_div) {
		DBG(TIMING, ul_debug(" normalize delay: divide"));
		step->delay.tv_sec /= stp->delay_div;
		step->delay.tv_usec /= stp->delay_div;
	}
	if (timerisset(&stp->delay_max) &&
	    timercmp(&step->delay, &stp->delay_max, >)) {
		DBG(TIMING, ul_debug(" normalize delay: align to max"));
		step->delay.tv_sec = stp->delay_max.tv_sec;
		step->delay.tv_usec = stp->delay_max.tv_usec;
	}
	if (timerisset(&stp->delay_min) &&
	    timercmp(&step->delay, &stp->delay_min, <)) {
		DBG(TIMING, ul_debug(" normalize delay: align to min"));
		timerclear(&step->delay);
	}

	return rc;
}

/* return: 0 = success, <0 = error, 1 = done (EOF) */
int replay_emit_step_data(struct replay_setup *stp, struct replay_step *step, int fd)
{
	size_t ct;
	int rc = 0, cr2nl = 0;
	char buf[BUFSIZ];

	assert(stp);
	assert(step);
	switch (step->type) {
	case 'S':
		assert(step->name);
		assert(step->value);
		dprintf(fd, "%s %s\n", step->name, step->value);
		DBG(LOG, ul_debug("log signal emitted"));
		return 0;
	case 'H':
		assert(step->name);
		assert(step->value);
		dprintf(fd, "%10s: %s\n", step->name, step->value);
		DBG(LOG, ul_debug("log header emitted"));
		return 0;
	default:
		break;		/* continue with real data */
	}

	assert(step->size);
	assert(step->data);
	assert(step->data->fp);

	switch (stp->crmode) {
	case REPLAY_CRMODE_AUTO:
		if (step->type == 'I')
			cr2nl = 1;
		break;
	case REPLAY_CRMODE_NEVER:
		cr2nl = 0;
		break;
	case REPLAY_CRMODE_ALWAYS:
		cr2nl = 1;
		break;
	}

	for (ct = step->size; ct > 0; ) {
		size_t len, cc;

		cc = ct > sizeof(buf) ? sizeof(buf): ct;
		len = fread(buf, 1, cc, step->data->fp);

		if (!len) {
			DBG(LOG, ul_debug("log data emit: failed to read log %m"));
			break;
		}

		if (cr2nl) {
			size_t i;

			for (i = 0; i < len; i++) {
				if (buf[i] == 0x0D)
					buf[i] = '\n';
			}
		}

		ct -= len;
		cc = write(fd, buf, len);
		if (cc != len) {
			rc = -errno;
			DBG(LOG, ul_debug("log data emit: failed write data %m"));
			break;
		}
	}

	if (ct && ferror(step->data->fp))
		rc = -errno;
	if (ct && feof(step->data->fp))
		rc = 1;

	DBG(LOG, ul_debug("log data emitted [rc=%d size=%zu]", rc, step->size));
	return rc;
}
