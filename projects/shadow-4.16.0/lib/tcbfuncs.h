#ifndef _TCBFUNCS_H
#define _TCBFUNCS_H

#include <sys/types.h>

typedef enum {
	SHADOWTCB_FAILURE = 0,
	SHADOWTCB_SUCCESS = 1
} shadowtcb_status;

extern shadowtcb_status shadowtcb_drop_priv (void);
extern shadowtcb_status shadowtcb_gain_priv (void);
extern shadowtcb_status shadowtcb_set_user (const char *name);
extern shadowtcb_status shadowtcb_remove (const char *name);
extern shadowtcb_status shadowtcb_move (/*@null@*/const char *user_newname,
                                        uid_t user_newid);
extern shadowtcb_status shadowtcb_create (const char *name, uid_t uid);

#endif
