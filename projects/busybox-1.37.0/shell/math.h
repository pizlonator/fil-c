/* math.h - interface to shell math "library" -- this allows shells to share
 *          the implementation of arithmetic $((...)) expansions.
 *
 * This aims to be a POSIX shell math library as documented here:
 *	http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_04
 *
 * See math.c for internal documentation.
 */
/* The math library has just one function:
 *
 * arith_t arith(arith_state_t *state, const char *expr);
 *
 * The expr argument is the math string to parse.  All normal expansions must
 * be done already.  i.e. no dollar symbols should be present.
 *
 * The state argument is a pointer to a struct of hooks for your shell (see below),
 * and an error message string (NULL if no error).
 *
 * The function returns the answer to the expression.  So if you called it
 * with the expression:
 * "1 + 2 + 3"
 * you would obviously get back 6.
 */
/* To add support to a shell, you need to implement three functions:
 *
 * lookupvar() - look up and return the value of a variable
 *
 *	If the shell does:
 *		foo=123
 *	Then the code:
 *		const char *val = lookupvar("foo");
 *	will result in val pointing to "123"
 *
 * setvar() - set a variable to some value
 *
 *	If the arithmetic expansion does something like:
 *		$((i = 1))
 *	then the math code will make a call like so:
 *		setvar("i", "1");
 *	The storage for the first two parameters are not allocated, so your
 *	shell implementation will most likely need to strdup() them to save.
 */
#ifndef SHELL_MATH_H
#define SHELL_MATH_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#if ENABLE_FEATURE_SH_MATH_64
typedef long long arith_t;
# define ARITH_FMT "%lld"
#else
typedef long arith_t;
# define ARITH_FMT "%ld"
#endif

typedef const char* FAST_FUNC (*arith_var_lookup_t)(const char *name);
typedef void        FAST_FUNC (*arith_var_set_t)(const char *name, const char *val);

typedef struct arith_state_t {
	unsigned              evaluation_disabled;
	const char           *errmsg;
	void                 *list_of_recursed_names;
	arith_var_lookup_t    lookupvar;
	arith_var_set_t       setvar;
} arith_state_t;

arith_t FAST_FUNC arith(arith_state_t *state, const char *expr);

POP_SAVED_FUNCTION_VISIBILITY

#endif
