/*	$OpenBSD: kbd.h,v 1.18 2006/07/27 19:59:29 deraadt Exp $	*/

/* This file is in the public domain. */

/*
 * kbd.h: type definitions for symbol.c and kbd.c for mg experimental
 */

struct map_element {
	KCHAR		 k_base;	/* first key in element		 */
	KCHAR		 k_num;		/* last key in element		 */
	PF		*k_funcp;	/* pointer to array of pointers	 */
					/* to functions			 */
	struct keymap_s *k_prefmap;	/* keymap of ONLY prefix key in	 */
					/* element			 */
};

/*
 * Predefined keymaps are NOT type KEYMAP because final array needs
 * dimension.  If any changes are made to this struct, they must be reflected
 * in all keymap declarations.
 */

#define KEYMAPE(NUM)	{						\
	short	map_num;			/* elements used */	\
	short	map_max;			/* elements allocated */\
	PF	map_default;			/* default function */	\
	struct map_element map_element[NUM];	/* really [e_max] */	\
}
typedef struct keymap_s KEYMAPE(1) KEYMAP;

/* Number of map_elements to grow an overflowed keymap by */
#define MAPGROW 3
#define MAPINIT (MAPGROW+1)

/* Max number of default bindings added to avoid creating new element */
#define MAPELEDEF 4

struct maps_s {
	KEYMAP		*p_map;
	const char	*p_name;
	struct maps_s	*p_next;
};

extern struct maps_s	*maps;
extern struct maps_s	fundamental_mode;
#define		fundamental_map (fundamental_mode.p_map)

int		 dobindkey(KEYMAP *, const char *, const char *);
KEYMAP		*name_map(const char *);
struct maps_s	*name_mode(const char *);
PF		 doscan(KEYMAP *, int, KEYMAP **);
void		 maps_init(void);
int		 maps_add(KEYMAP *, const char *);

extern struct map_element	*ele;
extern struct maps_s		*defb_modes[];
