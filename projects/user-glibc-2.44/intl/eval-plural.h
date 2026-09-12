/* Plural expression evaluation.
   Copyright (C) 2000-2026 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Ulrich Drepper and Bruno Haible.  */

#ifndef STATIC
#define STATIC static
#endif

/* While the bison parser is able to support expressions of a maximum depth
   of YYMAXDEPTH = 10000, the runtime evaluation of a parsed plural expression
   has a smaller maximum recursion depth.
   If we did not limit the recursion depth, a program that just invokes
   ngettext() on a thread other than the main thread could get a crash by
   stack overflow in the following circumstances:
     - On systems with glibc, after the stack size has been reduced,
       e.g. on x86_64 systems after "ulimit -s 260".
       This stack size is only sufficient for ca. 3919 recursions.
       Cf. <https://unix.stackexchange.com/questions/620720/>
     - On systems with musl libc, because there the thread stack size (for a
       thread other than the main thread) by default is only 128 KB, see
       <https://wiki.musl-libc.org/functional-differences-from-glibc.html#Thread-stack-size>.
     - On AIX 7 systems, because there the thread stack size (for a thread
       other than the main thread) by default is only 96 KB, see
       <https://www.ibm.com/docs/en/aix/7.1?topic=programming-threads-library-options>.
       This stack size is only sufficient for between 887 and 1363 recursions,
       depending on the compiler and compiler optimization options.
   A maximum depth of 100 is a large enough for all practical needs
   and also small enough to avoid stack overflow even with small thread stack
   sizes.  */
#ifndef EVAL_MAXDEPTH
# define EVAL_MAXDEPTH 100
#endif

/* A shorthand that denotes a successful evaluation result with a value V.  */
#define OK(v) (struct eval_result) { .status = PE_OK, .value = (v) }

/* Evaluates a plural expression PEXP for n=N, up to ALLOWED_DEPTH.  */
static struct eval_result
plural_eval_recurse (const struct expression *pexp, unsigned long int n,
		     unsigned int allowed_depth)
{
  if (allowed_depth == 0)
    /* The allowed recursion depth is exhausted.  */
    return (struct eval_result) { .status = PE_STACKOVF };
  allowed_depth--;

  switch (pexp->nargs)
    {
    case 0:
      switch (pexp->operation)
	{
	case var:
	  return OK (n);
	case num:
	  return OK (pexp->val.num);
	default:
	  break;
	}
      /* NOTREACHED */
      break;
    case 1:
      {
	/* pexp->operation must be lnot.  */
	struct eval_result arg =
	  plural_eval_recurse (pexp->val.args[0], n, allowed_depth);
	if (arg.status != PE_OK)
	  return arg;
	return OK (! arg.value);
      }
    case 2:
      {
	struct eval_result leftarg =
	  plural_eval_recurse (pexp->val.args[0], n, allowed_depth);
	if (leftarg.status != PE_OK)
	  return leftarg;
	if (pexp->operation == lor)
	  {
	    if (leftarg.value)
	      return OK (1);
	    struct eval_result rightarg =
	      plural_eval_recurse (pexp->val.args[1], n, allowed_depth);
	    if (rightarg.status != PE_OK)
	      return rightarg;
	    return OK (rightarg.value ? 1 : 0);
	  }
	else if (pexp->operation == land)
	  {
	    if (!leftarg.value)
	      return OK (0);
	    struct eval_result rightarg =
	      plural_eval_recurse (pexp->val.args[1], n, allowed_depth);
	    if (rightarg.status != PE_OK)
	      return rightarg;
	    return OK (rightarg.value ? 1 : 0);
	  }
	else
	  {
	    struct eval_result rightarg =
	      plural_eval_recurse (pexp->val.args[1], n, allowed_depth);
	    if (rightarg.status != PE_OK)
	      return rightarg;

	    switch (pexp->operation)
	      {
	      case mult:
		return OK (leftarg.value * rightarg.value);
	      case divide:
		if (rightarg.value == 0)
		  return (struct eval_result) { .status = PE_INTDIV };
		return OK (leftarg.value / rightarg.value);
	      case module:
		if (rightarg.value == 0)
		  return (struct eval_result) { .status = PE_INTDIV };
		return OK (leftarg.value % rightarg.value);
	      case plus:
		return OK (leftarg.value + rightarg.value);
	      case minus:
		return OK (leftarg.value - rightarg.value);
	      case less_than:
		return OK (leftarg.value < rightarg.value);
	      case greater_than:
		return OK (leftarg.value > rightarg.value);
	      case less_or_equal:
		return OK (leftarg.value <= rightarg.value);
	      case greater_or_equal:
		return OK (leftarg.value >= rightarg.value);
	      case equal:
		return OK (leftarg.value == rightarg.value);
	      case not_equal:
		return OK (leftarg.value != rightarg.value);
	      default:
		break;
	      }
	  }
	/* NOTREACHED */
	break;
      }
    case 3:
      {
	/* pexp->operation must be qmop.  */
	struct eval_result boolarg =
	  plural_eval_recurse (pexp->val.args[0], n, allowed_depth);
	if (boolarg.status != PE_OK)
	  return boolarg;
	return plural_eval_recurse (pexp->val.args[boolarg.value ? 1 : 2], n,
				    allowed_depth);
      }
    }
  /* NOTREACHED */
  return (struct eval_result) { .status = PE_ASSERT };
}

/* Evaluates a plural expression PEXP for n=N.  */
STATIC
struct eval_result
plural_eval (const struct expression *pexp, unsigned long int n)
{
  return plural_eval_recurse (pexp, n, EVAL_MAXDEPTH);
}

#undef OK
