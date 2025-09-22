#ifndef UTIL_LINUX_SCRIPT_PLAYUTILS_H
#define UTIL_LINUX_SCRIPT_PLAYUTILS_H

#include "c.h"
#include "debug.h"

#define SCRIPTREPLAY_DEBUG_INIT	(1 << 1)
#define SCRIPTREPLAY_DEBUG_TIMING (1 << 2)
#define SCRIPTREPLAY_DEBUG_LOG	(1 << 3)
#define SCRIPTREPLAY_DEBUG_MISC	(1 << 4)
#define SCRIPTREPLAY_DEBUG_ALL	0xFFFF

UL_DEBUG_DECLARE_MASK(scriptreplay);

#define DBG(m, x)       __UL_DBG(scriptreplay, SCRIPTREPLAY_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(scriptreplay, SCRIPTREPLAY_DEBUG_, m, x)

/* CR to '\n' mode */
enum {
	REPLAY_CRMODE_AUTO	= 0,
	REPLAY_CRMODE_NEVER,
	REPLAY_CRMODE_ALWAYS
};

struct replay_setup;
struct replay_step;

void replay_init_debug(void);
struct replay_setup *replay_new_setup(void);
void replay_free_setup(struct replay_setup *stp);

int replay_set_default_type(struct replay_setup *stp, char type);
int replay_set_crmode(struct replay_setup *stp, int mode);
int replay_set_timing_file(struct replay_setup *stp, const char *filename);
const char *replay_get_timing_file(struct replay_setup *setup);
int replay_get_timing_line(struct replay_setup *setup);
int replay_associate_log(struct replay_setup *stp, const char *streams, const char *filename);

int replay_set_delay_min(struct replay_setup *stp, const struct timeval *tv);
int replay_set_delay_max(struct replay_setup *stp, const struct timeval *tv);
int replay_set_delay_div(struct replay_setup *stp, const double divi);

struct timeval *replay_step_get_delay(struct replay_step *step);
const char *replay_step_get_filename(struct replay_step *step);
int replay_step_is_empty(struct replay_step *step);
int replay_get_next_step(struct replay_setup *stp, char *streams, struct replay_step **xstep);

int replay_emit_step_data(struct replay_setup *stp, struct replay_step *step, int fd);

#endif /* UTIL_LINUX_SCRIPT_PLAYUTILS_H */
