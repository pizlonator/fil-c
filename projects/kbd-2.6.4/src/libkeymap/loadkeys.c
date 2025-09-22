#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <unistd.h>

#include "keymap.h"

#include "libcommon.h"
#include "contextP.h"
#include "ksyms.h"

static int
defkeys(struct lk_ctx *ctx, int fd, int kbd_mode)
{
	struct kbentry ke;
	int ct = 0;
	int i, j, fail;

	if (ctx->flags & LK_FLAG_UNICODE_MODE) {
		/* temporarily switch to K_UNICODE while defining keys */
		if (ioctl(fd, KDSKBMODE, K_UNICODE)) {
			ERR(ctx, _("KDSKBMODE: %s: could not switch to Unicode mode"),
			    strerror(errno));
			goto fail;
		}
	}

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		int exist = lk_map_exists(ctx, i);

		if (exist) {
			for (j = 0; j < NR_KEYS; j++) {
				if (!lk_key_exists(ctx, i, j))
					continue;

				int value = lk_get_key(ctx, i, j);

				if (value < 0 || value > USHRT_MAX) {
					WARN(ctx, _("can not bind key %d to value %d because it is too large"), j, value);
					continue;
				}

				ke.kb_index = (unsigned char) j;
				ke.kb_table = (unsigned char) i;
				ke.kb_value = (unsigned short) value;

				fail = ioctl(fd, KDSKBENT, (unsigned long)&ke);

				if (fail) {
					if (errno == EPERM) {
						ERR(ctx, _("Keymap %d: Permission denied"), i);
						j = NR_KEYS;
						continue;
					}
					if (errno == EIO) {
						/*
						 * Such an error can be returned
						 * if the tty has been hungup
						 * while loadkeys is running.
						 */
						ERR(ctx, "%s", strerror(errno));
						goto fail;
					}
					ERR(ctx, "%s", strerror(errno));
				} else
					ct++;

				INFO(ctx, _("keycode %d, table %d = %d%s"),
				     j, i, ke.kb_value, fail ? _("    FAILED") : "");

				if (fail)
					WARN(ctx, _("failed to bind key %d to value %d"),
					     j, ke.kb_value);
			}

		} else if ((ctx->keywords & LK_KEYWORD_KEYMAPS) && !exist) {
			/* deallocate keymap */
			ke.kb_index = 0;
			ke.kb_table = (unsigned char) i;
			ke.kb_value = K_NOSUCHMAP;

			DBG(ctx, _("deallocate keymap %d"), i);

			if (ioctl(fd, KDSKBENT, (unsigned long)&ke)) {
				if (errno != EINVAL) {
					ERR(ctx, _("KDSKBENT: %s: could not deallocate keymap %d"),
					    strerror(errno), i);
					goto fail;
				}
				/* probably an old kernel */
				/* clear keymap by hand */
				for (j = 0; j < NR_KEYS; j++) {
					ke.kb_index = (unsigned char) j;
					ke.kb_table = (unsigned char) i;
					ke.kb_value = K_HOLE;

					if (ioctl(fd, KDSKBENT, (unsigned long)&ke)) {
						if (errno == EINVAL && i >= 16)
							break; /* old kernel */

						ERR(ctx, _("KDSKBENT: %s: cannot deallocate or clear keymap"),
						    strerror(errno));
						goto fail;
					}
				}
			}
		}
	}

	if ((ctx->flags & LK_FLAG_UNICODE_MODE) && ioctl(fd, KDSKBMODE, kbd_mode)) {
		ERR(ctx, _("KDSKBMODE: %s: could not return to original keyboard mode"),
		    strerror(errno));
		goto fail;
	}

	return ct;

fail:
	return -1;
}

static char *
ostr(struct lk_ctx *ctx, char *s)
{
	size_t lth = strlen(s);
	char *ns0  = malloc(4 * lth + 1);
	char *ns   = ns0;

	if (ns == NULL) {
		ERR(ctx, _("out of memory"));
		return NULL;
	}

	while (*s) {
		switch (*s) {
			case '\n':
				*ns++ = '\\';
				*ns++ = 'n';
				break;
			case '\033':
				*ns++ = '\\';
				*ns++ = '0';
				*ns++ = '3';
				*ns++ = '3';
				break;
			default:
				*ns++ = *s;
		}
		s++;
	}
	*ns = 0;
	return ns0;
}

static int
deffuncs(struct lk_ctx *ctx, int fd)
{
	unsigned int i;
	int ct = 0;
	char *ptr, *s;
	struct kbsentry kbs;

	for (i = 0; i < MAX_NR_FUNC; i++) {
		kbs.kb_func = (unsigned char) i;

		ptr = lk_array_get_ptr(ctx->func_table, i);

		if (ptr) {
			strcpy((char *)kbs.kb_string, ptr);
			if (ioctl(fd, KDSKBSENT, (unsigned long)&kbs)) {
				s = ostr(ctx, (char *)kbs.kb_string);
				if (s == NULL)
					return -1;
				ERR(ctx, _("failed to bind string '%s' to function %s"),
				    s, get_sym(ctx, KT_FN, kbs.kb_func));
				free(s);
			} else {
				ct++;
			}
		} else if (ctx->flags & LK_FLAG_CLEAR_STRINGS) {
			kbs.kb_string[0] = 0;

			if (ioctl(fd, KDSKBSENT, (unsigned long)&kbs)) {
				ERR(ctx, _("failed to clear string %s"),
				    get_sym(ctx, KT_FN, kbs.kb_func));
			} else {
				ct++;
			}
		}
	}
	return ct;
}

static int
defdiacs(struct lk_ctx *ctx, int fd)
{
	unsigned int i, j, count;
	struct lk_kbdiacr *ptr;

	if (ctx->accent_table->count > MAX_DIACR) {
		count = MAX_DIACR;
		ERR(ctx, _("too many compose definitions"));
	} else {
		count = (unsigned int) ctx->accent_table->count;
	}
#ifdef KDSKBDIACRUC
	if (ctx->flags & LK_FLAG_PREFER_UNICODE) {
		struct kbdiacrsuc kdu;

		kdu.kb_cnt = count;

		for (i = 0, j = 0; i < ctx->accent_table->total && j < count; i++) {
			ptr = lk_array_get_ptr(ctx->accent_table, i);
			if (!ptr)
				continue;

			kdu.kbdiacruc[j].diacr  = ptr->diacr;
			kdu.kbdiacruc[j].base   = ptr->base;
			kdu.kbdiacruc[j].result = ptr->result;
			j++;
		}

		if (ioctl(fd, KDSKBDIACRUC, (unsigned long)&kdu)) {
			ERR(ctx, "KDSKBDIACRUC: %s", strerror(errno));
			return -1;
		}
	} else
#endif
	{
		struct kbdiacrs kd;

		kd.kb_cnt = count;

		for (i = 0, j = 0; i < ctx->accent_table->total && j < count; i++) {
			ptr = lk_array_get_ptr(ctx->accent_table, i);
			if (!ptr)
				continue;

			if (ptr->diacr > UCHAR_MAX ||
			    ptr->base > UCHAR_MAX ||
			    ptr->result > UCHAR_MAX) {
				ERR(ctx, _("unable to load compose definitions because some of them are too large"));
				return -1;
			}

			kd.kbdiacr[j].diacr  = (unsigned char) ptr->diacr;
			kd.kbdiacr[j].base   = (unsigned char) ptr->base;
			kd.kbdiacr[j].result = (unsigned char) ptr->result;
			j++;
		}

		if (ioctl(fd, KDSKBDIACR, (unsigned long)&kd)) {
			ERR(ctx, "KDSKBDIACR: %s", strerror(errno));
			return -1;
		}
	}

	return (int) count;
}

int lk_load_keymap(struct lk_ctx *ctx, int fd, int kbd_mode)
{
	int keyct, funcct, diacct;

	if (lk_add_constants(ctx) < 0)
		return -1;

	if ((keyct = defkeys(ctx, fd, kbd_mode)) < 0 || (funcct = deffuncs(ctx, fd)) < 0)
		return -1;

	INFO(ctx, P_("\nChanged %d key", "\nChanged %d keys", (unsigned int) keyct), keyct);
	INFO(ctx, P_("Changed %d string", "Changed %d strings", (unsigned int) funcct), funcct);

	if (ctx->accent_table->count > 0 || ctx->flags & LK_FLAG_CLEAR_COMPOSE) {
		diacct = defdiacs(ctx, fd);

		if (diacct < 0)
			return -1;

		INFO(ctx, P_("Loaded %d compose definition",
		             "Loaded %d compose definitions", (unsigned int) diacct),
		     diacct);

	} else {
		INFO(ctx, _("(No change in compose definitions)"));
	}

	return 0;
}
