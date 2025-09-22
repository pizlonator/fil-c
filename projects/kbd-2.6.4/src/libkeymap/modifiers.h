#ifndef _MODIFIERS_H
#define _MODIFIERS_H

#include <linux/keyboard.h>

#define M_PLAIN 0
#define M_SHIFT (1 << KG_SHIFT)
#define M_CTRL (1 << KG_CTRL)
#define M_ALT (1 << KG_ALT)
#define M_ALTGR (1 << KG_ALTGR)
#define M_SHIFTL (1 << KG_SHIFTL)
#define M_SHIFTR (1 << KG_SHIFTR)
#define M_CTRLL (1 << KG_CTRLL)
#define M_CTRLR (1 << KG_CTRLR)
#define M_CAPSSHIFT (1 << KG_CAPSSHIFT)

typedef struct {
	const char *name;
	const int bit;
} modifier_t;

extern const modifier_t modifiers[];

#endif /* _MODIFIERS_H */
