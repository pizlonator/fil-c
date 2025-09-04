/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* config/applet/usage bits are in bc.c */

//#include "libbb.h"
//#include "common_bufsiz.h"
#include <math.h>

#if 0
typedef unsigned data_t;
#define DATA_FMT ""
#elif 0
typedef unsigned long data_t;
#define DATA_FMT "l"
#else
typedef unsigned long long data_t;
#define DATA_FMT "ll"
#endif

struct globals {
	unsigned pointer;
	unsigned base;
	double stack[1];
} FIX_ALIASING;
enum { STACK_SIZE = (COMMON_BUFSIZE - offsetof(struct globals, stack)) / sizeof(double) };
#define G (*(struct globals*)bb_common_bufsiz1)
#define pointer   (G.pointer   )
#define base      (G.base      )
#define stack     (G.stack     )
#define INIT_G() do { \
	setup_common_bufsiz(); \
	base = 10; \
} while (0)

static unsigned check_under(void)
{
	unsigned p = pointer;
	if (p == 0)
		bb_simple_error_msg_and_die("stack underflow");
	return p - 1;
}

static void push(double a)
{
	if (pointer >= STACK_SIZE)
		bb_simple_error_msg_and_die("stack overflow");
	stack[pointer++] = a;
}

static double pop(void)
{
	unsigned p = check_under();
	pointer = p;
	return stack[p];
}

static void add(void)
{
	push(pop() + pop());
}

static void sub(void)
{
	double subtrahend = pop();

	push(pop() - subtrahend);
}

static void mul(void)
{
	push(pop() * pop());
}

#if ENABLE_FEATURE_DC_LIBM
static void power(void)
{
	double topower = pop();

	push(pow(pop(), topower));
}
#endif

static void divide(void)
{
	double divisor = pop();

	push(pop() / divisor);
}

static void mod(void)
{
	data_t d = pop();

	/* compat with dc (GNU bc 1.07.1) 1.4.1:
	 * $ dc -e '4 0 % p'
	 * dc: remainder by zero
	 * 0
	 */
	if (d == 0) {
		bb_simple_error_msg("remainder by zero");
		pop();
		push(0);
		return;
	}
	/* ^^^^ without this, we simply get SIGFPE and die */

	push((data_t) pop() % d);
}

static void and(void)
{
	push((data_t) pop() & (data_t) pop());
}

static void or(void)
{
	push((data_t) pop() | (data_t) pop());
}

static void eor(void)
{
	push((data_t) pop() ^ (data_t) pop());
}

static void not(void)
{
	push(~(data_t) pop());
}

static void set_output_base(void)
{
	static const char bases[] ALIGN1 = { 2, 8, 10, 16, 0 };
	unsigned b = (unsigned)pop();

	base = *strchrnul(bases, b);
	if (base == 0) {
		bb_error_msg("error, base %u is not supported", b);
		base = 10;
	}
}

static void print_base(double print)
{
	data_t x, i;

	x = (data_t) print;
	if (base == 10) {
		if (x == print) /* exactly representable as unsigned integer */
			printf("%"DATA_FMT"u\n", x);
		else
			printf("%g\n", print);
		return;
	}

	switch (base) {
	case 16:
		printf("%"DATA_FMT"x\n", x);
		break;
	case 8:
		printf("%"DATA_FMT"o\n", x);
		break;
	default: /* base 2 */
		i = MAXINT(data_t) - (MAXINT(data_t) >> 1);
		/* i is 100000...00000 */
		do {
			if (x & i)
				break;
			i >>= 1;
		} while (i > 1);
		do {
			bb_putchar('1' - !(x & i));
			i >>= 1;
		} while (i);
		bb_putchar('\n');
	}
}

static void print_stack_no_pop(void)
{
	unsigned i = pointer;
	while (i)
		print_base(stack[--i]);
}

static void print_no_pop(void)
{
	print_base(stack[check_under()]);
}

struct op {
	const char name[4];
	void (*function) (void);
};

static const struct op operators[] ALIGN_PTR = {
#if ENABLE_FEATURE_DC_LIBM
	{"^",   power},
//	{"exp", power},
//	{"pow", power},
#endif
	{"%",   mod},
//	{"mod", mod},
	// logic ops are not standard, remove?
	{"and", and},
	{"or",  or},
	{"not", not},
	{"xor", eor},
	{"+",   add},
//	{"add", add},
	{"-",   sub},
//	{"sub", sub},
	{"*",   mul},
//	{"mul", mul},
	{"/",   divide},
//	{"div", divide},
	{"p", print_no_pop},
	{"f", print_stack_no_pop},
	{"o", set_output_base},
};

/* Feed the stack machine */
static void stack_machine(const char *argument)
{
	char *end;
	double number;
	const struct op *o;

 next:
//TODO: needs setlocale(LC_NUMERIC, "C")?
	number = strtod(argument, &end);
	if (end != argument) {
		argument = end;
		push(number);
		goto next;
	}

	/* We might have matched a digit, eventually advance the argument */
	argument = skip_whitespace(argument);

	if (*argument == '\0')
		return;

	o = operators;
	do {
		char *after_name = is_prefixed_with(argument, o->name);
		if (after_name) {
			argument = after_name;
			o->function();
			goto next;
		}
		o++;
	} while (o != operators + ARRAY_SIZE(operators));

	bb_error_msg_and_die("syntax error at '%s'", argument);
}

static void process_file(FILE *fp)
{
	char *line;
	while ((line = xmalloc_fgetline(fp)) != NULL) {
		stack_machine(line);
		free(line);
	}
}

int dc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dc_main(int argc UNUSED_PARAM, char **argv)
{
	bool script = 0;

	INIT_G();

	/* Run -e'SCRIPT' and -fFILE in order of appearance, then handle FILEs */
	for (;;) {
		int n = getopt(argc, argv, "e:f:");
		if (n <= 0)
			break;
		switch (n) {
		case 'e':
			script = 1;
			stack_machine(optarg);
			break;
		case 'f':
			script = 1;
			process_file(xfopen_for_read(optarg));
			break;
		default:
			bb_show_usage();
		}
	}
	argv += optind;

	if (*argv) {
		do
			process_file(xfopen_for_read(*argv++));
		while (*argv);
	} else if (!script) {
		/* Take stuff from stdin if no args are given */
		process_file(stdin);
	}

	return EXIT_SUCCESS;
}
