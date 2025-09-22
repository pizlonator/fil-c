%{
/* Parser for i386 CPU description.
   Copyright (C) 2004, 2005, 2007, 2008, 2009 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <obstack.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libeu.h>
#include <system.h>

#include "i386_mne.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* The error handler.  */
static void yyerror (const char *s);

extern int yylex (void);
extern int i386_lineno;
extern char *infname;


struct known_bitfield
{
  char *name;
  unsigned long int bits;
  int tmp;
};


struct bitvalue
{
  enum bittype { zeroone, field, failure } type;
  union
  {
    unsigned int value;
    struct known_bitfield *field;
  };
  struct bitvalue *next;
};


struct argname
{
  enum nametype { string, nfield } type;
  union
  {
    char *str;
    struct known_bitfield *field;
  };
  struct argname *next;
};


struct argument
{
  struct argname *name;
  struct argument *next;
};


struct instruction
{
  /* The byte encoding.  */
  struct bitvalue *bytes;

  /* Prefix possible.  */
  int repe;
  int rep;

  /* Mnemonic.  */
  char *mnemonic;

  /* Suffix.  */
  enum { suffix_none = 0, suffix_w, suffix_w0, suffix_W, suffix_tttn,
	 suffix_w1, suffix_W1, suffix_D } suffix;

  /* Flag set if modr/m is used.  */
  int modrm;

  /* Operands.  */
  struct operand
  {
    char *fct;
    char *str;
    int off1;
    int off2;
    int off3;
  } operands[3];

  struct instruction *next;
};


struct synonym
{
  char *from;
  char *to;
};


struct suffix
{
  char *name;
  int idx;
};


struct argstring
{
  char *str;
  int idx;
  int off;
};


static struct known_bitfield ax_reg =
  {
    .name = "ax", .bits = 0, .tmp = 0
  };

static struct known_bitfield dx_reg =
  {
    .name = "dx", .bits = 0, .tmp = 0
  };

static struct known_bitfield di_reg =
  {
    .name = "es_di", .bits = 0, .tmp = 0
  };

static struct known_bitfield si_reg =
  {
    .name = "ds_si", .bits = 0, .tmp = 0
  };

static struct known_bitfield bx_reg =
  {
    .name = "ds_bx", .bits = 0, .tmp = 0
  };


static int bitfield_compare (const void *p1, const void *p2);
static void new_bitfield (char *name, unsigned long int num);
static void check_bits (struct bitvalue *value);
static int check_duplicates (struct bitvalue *val);
static int check_argsdef (struct bitvalue *bitval, struct argument *args);
static int check_bitsused (struct bitvalue *bitval,
			   struct known_bitfield *suffix,
			   struct argument *args);
static struct argname *combine (struct argname *name);
static void fillin_arg (struct bitvalue *bytes, struct argname *name,
			struct instruction *instr, int n);
static void find_numbers (void);
static int compare_syn (const void *p1, const void *p2);
static int compare_suf (const void *p1, const void *p2);
static void instrtable_out (void);
#if 0
static void create_mnemonic_table (void);
#endif

static void *bitfields;
static struct instruction *instructions;
static size_t ninstructions;
static void *synonyms;
static void *suffixes;
static int nsuffixes;
static void *mnemonics;
size_t nmnemonics;
extern FILE *outfile;

/* Number of bits used mnemonics.  */
#if 0
static size_t best_mnemonic_bits;
#endif
%}

%union {
  unsigned long int num;
  char *str;
  char ch;
  struct known_bitfield *field;
  struct bitvalue *bit;
  struct argname *name;
  struct argument *arg;
}

%token kMASK
%token kPREFIX
%token kSUFFIX
%token kSYNONYM
%token <str> kID
%token <num> kNUMBER
%token kPERCPERC
%token <str> kBITFIELD
%token <ch> kCHAR
%token kSPACE

%type <bit> bit byte bytes
%type <field> bitfieldopt
%type <name> argcomp arg
%type <arg> args optargs

%defines

%%

spec:		  masks kPERCPERC '\n' instrs
		    {
		      if (error_message_count != 0)
			error (EXIT_FAILURE, 0,
			       "terminated due to previous error");

		      instrtable_out ();
		    }
		;

masks:		  masks '\n' mask
		| mask
		;

mask:		  kMASK kBITFIELD kNUMBER
		    { new_bitfield ($2, $3); }
		| kPREFIX kBITFIELD
		    { new_bitfield ($2, -1); }
		| kSUFFIX kBITFIELD
		    { new_bitfield ($2, -2); }
		| kSYNONYM kBITFIELD kBITFIELD
		    {
		      struct synonym *newp = xmalloc (sizeof (*newp));
		      newp->from = $2;
		      newp->to = $3;
		      if (tfind (newp, &synonyms, compare_syn) != NULL)
			error (0, 0,
			       "%d: duplicate definition for synonym '%s'",
			       i386_lineno, $2);
		      else if (tsearch ( newp, &synonyms, compare_syn) == NULL)
			error (EXIT_FAILURE, 0, "tsearch");
		    }
		|
		;

instrs:		  instrs '\n' instr
		| instr
		;

instr:		  bytes ':' bitfieldopt kID bitfieldopt optargs
		    {
		      if ($3 != NULL && strcmp ($3->name, "RE") != 0
			  && strcmp ($3->name, "R") != 0)
			{
			  error (0, 0, "%d: only 'R' and 'RE' prefix allowed",
				 i386_lineno - 1);
			}
		      if (check_duplicates ($1) == 0
			  && check_argsdef ($1, $6) == 0
			  && check_bitsused ($1, $5, $6) == 0)
			{
			  struct instruction *newp = xcalloc (sizeof (*newp),
							      1);
			  if ($3 != NULL)
			    {
			      if (strcmp ($3->name, "RE") == 0)
				newp->repe = 1;
			      else if (strcmp ($3->name, "R") == 0)
				newp->rep = 1;
			    }

			  newp->bytes = $1;
			  newp->mnemonic = $4;
			  if (newp->mnemonic != (void *) -1l
			      && tfind ($4, &mnemonics,
					(int (*)(const void *, const void *)) strcmp) == NULL)
			    {
			      if (tsearch ($4, &mnemonics,
					   (int (*)(const void *, const void *)) strcmp) == NULL)
				error (EXIT_FAILURE, errno, "tsearch");
			      ++nmnemonics;
			    }

			  if ($5 != NULL)
			    {
			      if (strcmp ($5->name, "w") == 0)
				newp->suffix = suffix_w;
			      else if (strcmp ($5->name, "w0") == 0)
				newp->suffix = suffix_w0;
			      else if (strcmp ($5->name, "tttn") == 0)
				newp->suffix = suffix_tttn;
			      else if (strcmp ($5->name, "w1") == 0)
				newp->suffix = suffix_w1;
			      else if (strcmp ($5->name, "W") == 0)
				newp->suffix = suffix_W;
			      else if (strcmp ($5->name, "W1") == 0)
				newp->suffix = suffix_W1;
			      else if (strcmp ($5->name, "D") == 0)
				newp->suffix = suffix_D;
			      else
				error (EXIT_FAILURE, 0,
				       "%s: %d: unknown suffix '%s'",
				       infname, i386_lineno - 1, $5->name);

			      struct suffix search = { .name = $5->name };
			      if (tfind (&search, &suffixes, compare_suf)
				  == NULL)
				{
				  struct suffix *ns = xmalloc (sizeof (*ns));
				  ns->name = $5->name;
				  ns->idx = ++nsuffixes;
				  if (tsearch (ns, &suffixes, compare_suf)
				      == NULL)
				    error (EXIT_FAILURE, errno, "tsearch");
				}
			    }

			  struct argument *args = $6;
			  int n = 0;
			  while (args != NULL)
			    {
			      fillin_arg ($1, args->name, newp, n);

			      args = args->next;
			      ++n;
			    }

			  newp->next = instructions;
			  instructions = newp;
			  ++ninstructions;
			}
		    }
		|
		;

bitfieldopt:	  kBITFIELD
		    {
		      struct known_bitfield search;
		      search.name = $1;
		      struct known_bitfield **res;
		      res = tfind (&search, &bitfields, bitfield_compare);
		      if (res == NULL)
			{
			  error (0, 0, "%d: unknown bitfield '%s'",
				 i386_lineno, search.name);
			  $$ = NULL;
			}
		      else
			$$ = *res;
		    }
		|
		    { $$ = NULL; }
		;

bytes:		  bytes ',' byte
		    {
		      check_bits ($3);

		      struct bitvalue *runp = $1;
		      while (runp->next != NULL)
			runp = runp->next;
		      runp->next = $3;
		      $$ = $1;
		    }
		| byte
		    {
		      check_bits ($1);
		      $$ = $1;
		    }
		;

byte:		  byte bit
		    {
		      struct bitvalue *runp = $1;
		      while (runp->next != NULL)
			runp = runp->next;
		      runp->next = $2;
		      $$ = $1;
		    }
		| bit
		    { $$ = $1; }
		;

bit:		  '0'
		    {
		      $$ = xmalloc (sizeof (struct bitvalue));
		      $$->type = zeroone;
		      $$->value = 0;
		      $$->next = NULL;
		    }
		| '1'
		    {
		      $$ = xmalloc (sizeof (struct bitvalue));
		      $$->type = zeroone;
		      $$->value = 1;
		      $$->next = NULL;
		    }
		| kBITFIELD
		    {
		      $$ = xmalloc (sizeof (struct bitvalue));
		      struct known_bitfield search;
		      search.name = $1;
		      struct known_bitfield **res;
		      res = tfind (&search, &bitfields, bitfield_compare);
		      if (res == NULL)
			{
			  error (0, 0, "%d: unknown bitfield '%s'",
				 i386_lineno, search.name);
			  $$->type = failure;
			}
		      else
			{
			  $$->type = field;
			  $$->field = *res;
			}
		      $$->next = NULL;
		    }
		;

optargs:	  kSPACE args
		    { $$ = $2; }
		|
		    { $$ = NULL; }
		;

args:		  args ',' arg
		    {
		      struct argument *runp = $1;
		      while (runp->next != NULL)
			runp = runp->next;
		      runp->next = xmalloc (sizeof (struct argument));
		      runp->next->name = combine ($3);
		      runp->next->next = NULL;
		      $$ = $1;
		    }
		| arg
		    {
		      $$ = xmalloc (sizeof (struct argument));
		      $$->name = combine ($1);
		      $$->next = NULL;
		    }
		;

arg:		  arg argcomp
		    {
		      struct argname *runp = $1;
		      while (runp->next != NULL)
			runp = runp->next;
		      runp->next = $2;
		      $$ = $1;
		    }
		| argcomp
		    { $$ = $1; }
		;
argcomp:	  kBITFIELD
		    {
		      $$ = xmalloc (sizeof (struct argname));
		      $$->type = nfield;
		      $$->next = NULL;

		      struct known_bitfield search;
		      search.name = $1;
		      struct known_bitfield **res;
		      res = tfind (&search, &bitfields, bitfield_compare);
		      if (res == NULL)
			{
			  if (strcmp ($1, "ax") == 0)
			    $$->field = &ax_reg;
			  else if (strcmp ($1, "dx") == 0)
			    $$->field = &dx_reg;
			  else if (strcmp ($1, "es_di") == 0)
			    $$->field = &di_reg;
			  else if (strcmp ($1, "ds_si") == 0)
			    $$->field = &si_reg;
			  else if (strcmp ($1, "ds_bx") == 0)
			    $$->field = &bx_reg;
			  else
			    {
			      error (0, 0, "%d: unknown bitfield '%s'",
				     i386_lineno, search.name);
			      $$->field = NULL;
			    }
			}
		      else
			$$->field = *res;
		    }
		| kCHAR
		    {
		      $$ = xmalloc (sizeof (struct argname));
		      $$->type = string;
		      $$->next = NULL;
		      $$->str = xmalloc (2);
		      $$->str[0] = $1;
		      $$->str[1] = '\0';
		    }
		| kID
		    {
		      $$ = xmalloc (sizeof (struct argname));
		      $$->type = string;
		      $$->next = NULL;
		      $$->str = $1;
		    }
		| ':'
		    {
		      $$ = xmalloc (sizeof (struct argname));
		      $$->type = string;
		      $$->next = NULL;
		      $$->str = xmalloc (2);
		      $$->str[0] = ':';
		      $$->str[1] = '\0';
		    }
		;

%%

static void
yyerror (const char *s)
{
  error (0, 0, _("while reading i386 CPU description: %s at line %d"),
         _(s), i386_lineno);
}


static int
bitfield_compare (const void *p1, const void *p2)
{
  struct known_bitfield *f1 = (struct known_bitfield *) p1;
  struct known_bitfield *f2 = (struct known_bitfield *) p2;

  return strcmp (f1->name, f2->name);
}


static void
new_bitfield (char *name, unsigned long int num)
{
  struct known_bitfield *newp = xmalloc (sizeof (struct known_bitfield));
  newp->name = name;
  newp->bits = num;
  newp->tmp = 0;

  if (tfind (newp, &bitfields, bitfield_compare) != NULL)
    {
      error (0, 0, "%d: duplicated definition of bitfield '%s'",
	     i386_lineno, name);
      free (name);
      free (newp);
      return;
    }

  if (tsearch (newp, &bitfields, bitfield_compare) == NULL)
    error (EXIT_FAILURE, errno, "%d: cannot insert new bitfield '%s'",
	   i386_lineno, name);
}


/* Check that the number of bits is a multiple of 8.  */
static void
check_bits (struct bitvalue *val)
{
  struct bitvalue *runp = val;
  unsigned int total = 0;

  while (runp != NULL)
    {
      if (runp->type == zeroone)
	++total;
      else if (runp->field == NULL)
	/* No sense doing anything, the field is not known.  */
	return;
      else
	total += runp->field->bits;

      runp = runp->next;
    }

  if (total % 8 != 0)
    {
      struct obstack os;
      obstack_init (&os);

      while (val != NULL)
	{
	  if (val->type == zeroone)
	    obstack_printf (&os, "%u", val->value);
	  else
	    obstack_printf (&os, "{%s}", val->field->name);
	  val = val->next;
	}
      obstack_1grow (&os, '\0');

      error (0, 0, "%d: field '%s' not a multiple of 8 bits in size",
	     i386_lineno, (char *) obstack_finish (&os));

      obstack_free (&os, NULL);
    }
}


static int
check_duplicates (struct bitvalue *val)
{
  static int testcnt;
  ++testcnt;

  int result = 0;
  while (val != NULL)
    {
      if (val->type == field && val->field != NULL)
	{
	  if (val->field->tmp == testcnt)
	    {
	      error (0, 0, "%d: bitfield '%s' used more than once",
		     i386_lineno - 1, val->field->name);
	      result = 1;
	    }
	  val->field->tmp = testcnt;
	}

      val = val->next;
    }

  return result;
}


static int
check_argsdef (struct bitvalue *bitval, struct argument *args)
{
  int result = 0;

  while (args != NULL)
    {
      for (struct argname *name = args->name; name != NULL; name = name->next)
	if (name->type == nfield && name->field != NULL
	    && name->field != &ax_reg && name->field != &dx_reg
	    && name->field != &di_reg && name->field != &si_reg
	    && name->field != &bx_reg)
	  {
	    struct bitvalue *runp = bitval;

	    while (runp != NULL)
	      if (runp->type == field && runp->field == name->field)
		break;
	      else
		runp = runp->next;

	    if (runp == NULL)
	      {
		error (0, 0, "%d: unknown bitfield '%s' used in output format",
		       i386_lineno - 1, name->field->name);
		result = 1;
	      }
	  }

      args = args->next;
    }

  return result;
}


static int
check_bitsused (struct bitvalue *bitval, struct known_bitfield *suffix,
		struct argument *args)
{
  int result = 0;

  while (bitval != NULL)
    {
      if (bitval->type == field && bitval->field != NULL
	  && bitval->field != suffix
	  /* {w} is handled special.  */
	  && strcmp (bitval->field->name, "w") != 0)
	{
	  struct argument *runp;
	  for (runp = args; runp != NULL; runp = runp->next)
	    {
	      struct argname *name = runp->name;

	      while (name != NULL)
		if (name->type == nfield && name->field == bitval->field)
		  break;
		else
		  name = name->next;

	      if (name != NULL)
		break;
	    }

#if 0
	  if (runp == NULL)
	    {
	      error (0, 0, "%d: bitfield '%s' not used",
		     i386_lineno - 1, bitval->field->name);
	      result = 1;
	    }
#endif
	}

      bitval = bitval->next;
    }

  return result;
}


static struct argname *
combine (struct argname *name)
{
  struct argname *last_str = NULL;
  for (struct argname *runp = name; runp != NULL; runp = runp->next)
    {
      if (runp->type == string)
	{
	  if (last_str == NULL)
	    last_str = runp;
	  else
	    {
	      last_str->str = xrealloc (last_str->str,
					strlen (last_str->str)
					+ strlen (runp->str) + 1);
	      strcat (last_str->str, runp->str);
	      last_str->next = runp->next;
	    }
	}
      else
	last_str = NULL;
    }
  return name;
}


#define obstack_grow_str(ob, str) obstack_grow (ob, str, strlen (str))


static void
fillin_arg (struct bitvalue *bytes, struct argname *name,
	    struct instruction *instr, int n)
{
  static struct obstack ob;
  static int initialized;
  if (! initialized)
    {
      initialized = 1;
      obstack_init (&ob);
    }

  struct argname *runp = name;
  int cnt = 0;
  while (runp != NULL)
    {
      /* We ignore strings in the function name.  */
      if (runp->type == string)
	{
	  if (instr->operands[n].str != NULL)
	    error (EXIT_FAILURE, 0,
		   "%d: cannot have more than one string parameter",
		   i386_lineno - 1);

	  instr->operands[n].str = runp->str;
	}
      else
	{
	  assert (runp->type == nfield);

	  /* Construct the function name.  */
	  if (cnt++ > 0)
	    obstack_1grow (&ob, '$');

	  if (runp->field == NULL)
	    /* Add some string which contains invalid characters.  */
	    obstack_grow_str (&ob, "!!!INVALID!!!");
	  else
	    {
	      char *fieldname = runp->field->name;

	      struct synonym search = { .from = fieldname };

	      struct synonym **res = tfind (&search, &synonyms, compare_syn);
	      if (res != NULL)
		fieldname = (*res)->to;

	      obstack_grow_str (&ob, fieldname);
	    }

	  /* Now compute the bit offset of the field.  */
	  struct bitvalue *b = bytes;
	  int bitoff = 0;
	  if (runp->field != NULL)
	    while (b != NULL)
	      {
		if (b->type == field && b->field != NULL)
		  {
		    if (strcmp (b->field->name, runp->field->name) == 0)
		      break;
		    bitoff += b->field->bits;
		  }
		else
		  ++bitoff;

		b = b->next;
	      }
	  if (instr->operands[n].off1 == 0)
	    instr->operands[n].off1 = bitoff;
	  else if (instr->operands[n].off2 == 0)
	    instr->operands[n].off2 = bitoff;
	  else if (instr->operands[n].off3 == 0)
	    instr->operands[n].off3 = bitoff;
	  else
	    error (EXIT_FAILURE, 0,
		   "%d: cannot have more than three fields in parameter",
		   i386_lineno - 1);

	  if  (runp->field != NULL
	       && strncasecmp (runp->field->name, "mod", 3) == 0)
	    instr->modrm = 1;
	}

      runp = runp->next;
    }
  if (obstack_object_size (&ob) == 0)
    obstack_grow_str (&ob, "string");
  obstack_1grow (&ob, '\0');
  char *fct = obstack_finish (&ob);

  instr->operands[n].fct = fct;
}


#if 0
static void
nameout (const void *nodep, VISIT value, int level)
{
  if (value == leaf || value == postorder)
    printf ("  %s\n", *(const char **) nodep);
}
#endif


static int
compare_argstring (const void *p1, const void *p2)
{
  const struct argstring *a1 = (const struct argstring *) p1;
  const struct argstring *a2 = (const struct argstring *) p2;

  return strcmp (a1->str, a2->str);
}


static int maxoff[3][3];
static int minoff[3][3] = { { 1000, 1000, 1000 },
			    { 1000, 1000, 1000 },
			    { 1000, 1000, 1000 } };
static int nbitoff[3][3];
static void *fct_names[3];
static int nbitfct[3];
static int nbitsuf;
static void *strs[3];
static int nbitstr[3];
static int total_bits = 2;	// Already counted the rep/repe bits.

static void
find_numbers (void)
{
  int nfct_names[3] = { 0, 0, 0 };
  int nstrs[3] = { 0, 0, 0 };

  /* We reverse the order of the instruction list while processing it.
     Later phases need it in the order in which the input file has
     them.  */
  struct instruction *reversed = NULL;

  struct instruction *runp = instructions;
  while (runp != NULL)
    {
      for (int i = 0; i < 3; ++i)
	if (runp->operands[i].fct != NULL)
	  {
	    struct argstring search = { .str = runp->operands[i].fct };
	    if (tfind (&search, &fct_names[i], compare_argstring) == NULL)
	      {
		struct argstring *newp = xmalloc (sizeof (*newp));
		newp->str = runp->operands[i].fct;
		newp->idx = 0;
		if (tsearch (newp, &fct_names[i], compare_argstring) == NULL)
		  error (EXIT_FAILURE, errno, "tsearch");
		++nfct_names[i];
	      }

	    if (runp->operands[i].str != NULL)
	      {
		search.str = runp->operands[i].str;
		if (tfind (&search, &strs[i], compare_argstring) == NULL)
		  {
		    struct argstring *newp = xmalloc (sizeof (*newp));
		    newp->str = runp->operands[i].str;
		    newp->idx = 0;
		    if (tsearch (newp, &strs[i], compare_argstring) == NULL)
		      error (EXIT_FAILURE, errno, "tsearch");
		    ++nstrs[i];
		  }
	      }

	    maxoff[i][0] = MAX (maxoff[i][0], runp->operands[i].off1);
	    maxoff[i][1] = MAX (maxoff[i][1], runp->operands[i].off2);
	    maxoff[i][2] = MAX (maxoff[i][2], runp->operands[i].off3);

	    if (runp->operands[i].off1 > 0)
	      minoff[i][0] = MIN (minoff[i][0], runp->operands[i].off1);
	    if (runp->operands[i].off2 > 0)
	      minoff[i][1] = MIN (minoff[i][1], runp->operands[i].off2);
	    if (runp->operands[i].off3 > 0)
	      minoff[i][2] = MIN (minoff[i][2], runp->operands[i].off3);
	  }

      struct instruction *old = runp;
      runp = runp->next;

      old->next = reversed;
      reversed = old;
    }
  instructions = reversed;

  int d;
  int c;
  for (int i = 0; i < 3; ++i)
    {
      // printf ("min1 = %d, min2 = %d, min3 = %d\n", minoff[i][0], minoff[i][1], minoff[i][2]);
      // printf ("max1 = %d, max2 = %d, max3 = %d\n", maxoff[i][0], maxoff[i][1], maxoff[i][2]);

      if (minoff[i][0] == 1000)
	nbitoff[i][0] = 0;
      else
	{
	  nbitoff[i][0] = 1;
	  d = maxoff[i][0] - minoff[i][0];
	  c = 1;
	  while (c < d)
	    {
	      ++nbitoff[i][0];
	      c *= 2;
	    }
	  total_bits += nbitoff[i][0];
	}

      if (minoff[i][1] == 1000)
	nbitoff[i][1] = 0;
      else
	{
	  nbitoff[i][1] = 1;
	  d = maxoff[i][1] - minoff[i][1];
	  c = 1;
	  while (c < d)
	    {
	      ++nbitoff[i][1];
	      c *= 2;
	    }
	  total_bits += nbitoff[i][1];
	}

      if (minoff[i][2] == 1000)
	nbitoff[i][2] = 0;
      else
	{
	  nbitoff[i][2] = 1;
	  d = maxoff[i][2] - minoff[i][2];
	  c = 1;
	  while (c < d)
	    {
	      ++nbitoff[i][2];
	      c *= 2;
	    }
	  total_bits += nbitoff[i][2];
	}
      // printf ("off1 = %d, off2 = %d, off3 = %d\n", nbitoff[i][0], nbitoff[i][1], nbitoff[i][2]);

      nbitfct[i] = 1;
      d = nfct_names[i];
      c = 1;
      while (c < d)
	{
	  ++nbitfct[i];
	  c *= 2;
	}
      total_bits += nbitfct[i];
      // printf ("%d fct[%d], %d bits\n", nfct_names[i], i, nbitfct[i]);

      if (nstrs[i] != 0)
	{
	  nbitstr[i] = 1;
	  d = nstrs[i];
	  c = 1;
	  while (c < d)
	    {
	      ++nbitstr[i];
	      c *= 2;
	    }
	  total_bits += nbitstr[i];
	}

      // twalk (fct_names[i], nameout);
    }

  nbitsuf = 0;
  d = nsuffixes;
  c = 1;
  while (c < d)
    {
      ++nbitsuf;
      c *= 2;
    }
  total_bits += nbitsuf;
  // printf ("%d suffixes, %d bits\n", nsuffixes, nbitsuf);
}


static int
compare_syn (const void *p1, const void *p2)
{
  const struct synonym *s1 = (const struct synonym *) p1;
  const struct synonym *s2 = (const struct synonym *) p2;

  return strcmp (s1->from, s2->from);
}


static int
compare_suf (const void *p1, const void *p2)
{
  const struct suffix *s1 = (const struct suffix *) p1;
  const struct suffix *s2 = (const struct suffix *) p2;

  return strcmp (s1->name, s2->name);
}


static int count_op_str;
static int off_op_str;
static void
print_op_str (const void *nodep, VISIT value,
	      int level __attribute__ ((unused)))
{
  if (value == leaf || value == postorder)
    {
      const char *str = (*(struct argstring **) nodep)->str;
      fprintf (outfile, "%s\n  \"%s",
	       count_op_str == 0 ? "" : "\\0\"", str);
      (*(struct argstring **) nodep)->idx = ++count_op_str;
      (*(struct argstring **) nodep)->off = off_op_str;
      off_op_str += strlen (str) + 1;
    }
}


static void
print_op_str_idx (const void *nodep, VISIT value,
		  int level __attribute__ ((unused)))
{
  if (value == leaf || value == postorder)
    printf ("  %d,\n", (*(struct argstring **) nodep)->off);
}


static void
print_op_fct (const void *nodep, VISIT value,
	      int level __attribute__ ((unused)))
{
  if (value == leaf || value == postorder)
    {
      fprintf (outfile, "  FCT_%s,\n", (*(struct argstring **) nodep)->str);
      (*(struct argstring **) nodep)->idx = ++count_op_str;
    }
}

static void
instrtable_out (void)
{
  find_numbers ();

#if 0
  create_mnemonic_table ();

  fprintf (outfile, "#define MNEMONIC_BITS %zu\n", best_mnemonic_bits);
#else
  fprintf (outfile, "#define MNEMONIC_BITS %ld\n",
	   lrint (ceil (log2 (MNE_COUNT))));
#endif
  fprintf (outfile, "#define SUFFIX_BITS %d\n", nbitsuf);
  for (int i = 0; i < 3; ++i)
    {
      fprintf (outfile, "#define FCT%d_BITS %d\n", i + 1, nbitfct[i]);
      if (nbitstr[i] != 0)
	fprintf (outfile, "#define STR%d_BITS %d\n", i + 1, nbitstr[i]);
      fprintf (outfile, "#define OFF%d_1_BITS %d\n", i + 1, nbitoff[i][0]);
      fprintf (outfile, "#define OFF%d_1_BIAS %d\n", i + 1, minoff[i][0]);
      if (nbitoff[i][1] != 0)
	{
	  fprintf (outfile, "#define OFF%d_2_BITS %d\n", i + 1, nbitoff[i][1]);
	  fprintf (outfile, "#define OFF%d_2_BIAS %d\n", i + 1, minoff[i][1]);
	}
      if (nbitoff[i][2] != 0)
	{
	  fprintf (outfile, "#define OFF%d_3_BITS %d\n", i + 1, nbitoff[i][2]);
	  fprintf (outfile, "#define OFF%d_3_BIAS %d\n", i + 1, minoff[i][2]);
	}
    }

  fputs ("\n#include <i386_data.h>\n\n", outfile);


#define APPEND(a, b) APPEND_ (a, b)
#define APPEND_(a, b) a##b
#define EMIT_SUFFIX(suf) \
  fprintf (outfile, "#define suffix_%s %d\n", #suf, APPEND (suffix_, suf))
  EMIT_SUFFIX (none);
  EMIT_SUFFIX (w);
  EMIT_SUFFIX (w0);
  EMIT_SUFFIX (W);
  EMIT_SUFFIX (tttn);
  EMIT_SUFFIX (D);
  EMIT_SUFFIX (w1);
  EMIT_SUFFIX (W1);

  fputc_unlocked ('\n', outfile);

  for (int i = 0; i < 3; ++i)
    {
      /* Functions.  */
      count_op_str = 0;
      fprintf (outfile, "static const opfct_t op%d_fct[] =\n{\n  NULL,\n",
	       i + 1);
      twalk (fct_names[i], print_op_fct);
      fputs ("};\n", outfile);

      /* The operand strings.  */
      if (nbitstr[i] != 0)
	{
	  count_op_str = 0;
	  off_op_str = 0;
	  fprintf (outfile, "static const char op%d_str[] =", i + 1);
	  twalk (strs[i], print_op_str);
	  fputs ("\";\n", outfile);

	  fprintf (outfile, "static const uint8_t op%d_str_idx[] = {\n",
		   i + 1);
	  twalk (strs[i], print_op_str_idx);
	  fputs ("};\n", outfile);
	}
    }


  fputs ("static const struct instr_enc instrtab[] =\n{\n", outfile);
  struct instruction *instr;
  for (instr = instructions; instr != NULL; instr = instr->next)
    {
      fputs ("  {", outfile);
      if (instr->mnemonic == (void *) -1l)
	fputs (" .mnemonic = MNE_INVALID,", outfile);
      else
	fprintf (outfile, " .mnemonic = MNE_%s,", instr->mnemonic);
      fprintf (outfile, " .rep = %d,", instr->rep);
      fprintf (outfile, " .repe = %d,", instr->repe);
      fprintf (outfile, " .suffix = %d,", instr->suffix);
      fprintf (outfile, " .modrm = %d,", instr->modrm);

      for (int i = 0; i < 3; ++i)
	{
	  int idx = 0;
	  if (instr->operands[i].fct != NULL)
	    {
	      struct argstring search = { .str = instr->operands[i].fct };
	      struct argstring **res = tfind (&search, &fct_names[i],
					      compare_argstring);
	      assert (res != NULL);
	      idx = (*res)->idx;
	    }
	  fprintf (outfile, " .fct%d = %d,", i + 1, idx);

	  idx = 0;
	  if (instr->operands[i].str != NULL)
	    {
	      struct argstring search = { .str = instr->operands[i].str };
	      struct argstring **res = tfind (&search, &strs[i],
					      compare_argstring);
	      assert (res != NULL);
	      idx = (*res)->idx;
	    }
	  if (nbitstr[i] != 0)
	    fprintf (outfile, " .str%d = %d,", i + 1, idx);

	  fprintf (outfile, " .off%d_1 = %d,", i + 1,
		   MAX (0, instr->operands[i].off1 - minoff[i][0]));

	  if (nbitoff[i][1] != 0)
	    fprintf (outfile, " .off%d_2 = %d,", i + 1,
		     MAX (0, instr->operands[i].off2 - minoff[i][1]));

	  if (nbitoff[i][2] != 0)
	    fprintf (outfile, " .off%d_3 = %d,", i + 1,
		     MAX (0, instr->operands[i].off3 - minoff[i][2]));
	}

      fputs (" },\n", outfile);
    }
  fputs ("};\n", outfile);

  fputs ("static const uint8_t match_data[] =\n{\n", outfile);
  size_t cnt = 0;
  for (instr = instructions; instr != NULL; instr = instr->next, ++cnt)
    {
      /* First count the number of bytes.  */
      size_t totalbits = 0;
      size_t zerobits = 0;
      bool leading_p = true;
      size_t leadingbits = 0;
      struct bitvalue *b = instr->bytes;
      while (b != NULL)
	{
	  if (b->type == zeroone)
	    {
	      ++totalbits;
	      zerobits = 0;
	      if (leading_p)
		++leadingbits;
	    }
	  else
	    {
	      totalbits += b->field->bits;
	      /* We must always count the mod/rm byte.  */
	      if (strncasecmp (b->field->name, "mod", 3) == 0)
		zerobits = 0;
	      else
		zerobits += b->field->bits;
	      leading_p = false;
	    }
	  b = b->next;
	}
      size_t nbytes = (totalbits - zerobits + 7) / 8;
      assert (nbytes > 0);
      size_t leadingbytes = leadingbits / 8;

      fprintf (outfile, "  %#zx,", nbytes | (leadingbytes << 4));

      /* Now create the mask and byte values.  */
      uint8_t byte = 0;
      uint8_t mask = 0;
      int nbits = 0;
      b = instr->bytes;
      while (b != NULL)
	{
	  if (b->type == zeroone)
	    {
	      byte = (byte << 1) | b->value;
	      mask = (mask << 1) | 1;
	      if (++nbits == 8)
		{
		  if (leadingbytes > 0)
		    {
		      assert (mask == 0xff);
		      fprintf (outfile, " %#" PRIx8 ",", byte);
		      --leadingbytes;
		    }
		  else
		    fprintf (outfile, " %#" PRIx8 ", %#" PRIx8 ",",
			     mask, byte);
		  byte = mask = nbits = 0;
		  if (--nbytes == 0)
		    break;
		}
	    }
	  else
	    {
	      assert (leadingbytes == 0);

	      unsigned long int remaining = b->field->bits;
	      while (nbits + remaining > 8)
		{
		  fprintf (outfile, " %#" PRIx8 ", %#" PRIx8 ",",
			   mask << (8 - nbits), byte << (8 - nbits));
		  remaining = nbits + remaining - 8;
		  byte = mask = nbits = 0;
		  if (--nbytes == 0)
		    break;
		}
	      byte <<= remaining;
	      mask <<= remaining;
	      nbits += remaining;
	      if (nbits == 8)
		{
		  fprintf (outfile, " %#" PRIx8 ", %#" PRIx8 ",", mask, byte);
		  byte = mask = nbits = 0;
		  if (--nbytes == 0)
		    break;
		}
	    }
	  b = b->next;
	}

      fputc_unlocked ('\n', outfile);
    }
  fputs ("};\n", outfile);
}


#if 0
static size_t mnemonic_maxlen;
static size_t mnemonic_minlen;
static size_t
which_chars (const char *str[], size_t nstr)
{
  char used_char[256];
  memset (used_char, '\0', sizeof (used_char));
  mnemonic_maxlen = 0;
  mnemonic_minlen = 10000;
  for (size_t cnt = 0; cnt < nstr; ++cnt)
    {
      const unsigned char *cp = (const unsigned char *) str[cnt];
      mnemonic_maxlen = MAX (mnemonic_maxlen, strlen ((char *) cp));
      mnemonic_minlen = MIN (mnemonic_minlen, strlen ((char *) cp));
      do
        used_char[*cp++] = 1;
      while (*cp != '\0');
    }
  size_t nused_char = 0;
  for (size_t cnt = 0; cnt < 256; ++cnt)
    if (used_char[cnt] != 0)
      ++nused_char;
  return nused_char;
}


static const char **mnemonic_strs;
static size_t nmnemonic_strs;
static void
add_mnemonics (const void *nodep, VISIT value,
	       int level __attribute__ ((unused)))
{
  if (value == leaf || value == postorder)
    mnemonic_strs[nmnemonic_strs++] = *(const char **) nodep;
}


struct charfreq
{
  char ch;
  int freq;
};
static struct charfreq pfxfreq[256];
static struct charfreq sfxfreq[256];


static int
compare_freq (const void *p1, const void *p2)
{
  const struct charfreq *c1 = (const struct charfreq *) p1;
  const struct charfreq *c2 = (const struct charfreq *) p2;

  if (c1->freq > c2->freq)
    return -1;
  if (c1->freq < c2->freq)
    return 1;
  return 0;
}


static size_t
compute_pfxfreq (const char *str[], size_t nstr)
{
  memset (pfxfreq, '\0', sizeof (pfxfreq));

  for (size_t i = 0; i < nstr; ++i)
    pfxfreq[i].ch = i;

  for (size_t i = 0; i < nstr; ++i)
    ++pfxfreq[*((const unsigned char *) str[i])].freq;

  qsort (pfxfreq, 256, sizeof (struct charfreq), compare_freq);

  size_t n = 0;
  while (n < 256 && pfxfreq[n].freq != 0)
    ++n;
  return n;
}


struct strsnlen
{
  const char *str;
  size_t len;
};

static size_t
compute_sfxfreq (size_t nstr, struct strsnlen *strsnlen)
{
  memset (sfxfreq, '\0', sizeof (sfxfreq));

  for (size_t i = 0; i < nstr; ++i)
    sfxfreq[i].ch = i;

  for (size_t i = 0; i < nstr; ++i)
    ++sfxfreq[((const unsigned char *) strchrnul (strsnlen[i].str, '\0'))[-1]].freq;

  qsort (sfxfreq, 256, sizeof (struct charfreq), compare_freq);

  size_t n = 0;
  while (n < 256 && sfxfreq[n].freq != 0)
    ++n;
  return n;
}


static void
create_mnemonic_table (void)
{
  mnemonic_strs = xmalloc (nmnemonics * sizeof (char *));

  twalk (mnemonics, add_mnemonics);

  (void) which_chars (mnemonic_strs, nmnemonic_strs);

  size_t best_so_far = 100000000;
  char *best_prefix = NULL;
  char *best_suffix = NULL;
  char *best_table = NULL;
  size_t best_table_size = 0;
  size_t best_table_bits = 0;
  size_t best_prefix_bits = 0;

  /* We can precompute the prefix characters.  */
  size_t npfx_char = compute_pfxfreq (mnemonic_strs, nmnemonic_strs);

  /* Compute best size for string representation including explicit NUL.  */
  for (size_t pfxbits = 0; (1u << pfxbits) < 2 * npfx_char; ++pfxbits)
    {
      char prefix[1 << pfxbits];
      size_t i;
      for (i = 0; i < (1u << pfxbits) - 1; ++i)
	prefix[i] = pfxfreq[i].ch;
      prefix[i] = '\0';

      struct strsnlen strsnlen[nmnemonic_strs];

      for (i = 0; i < nmnemonic_strs; ++i)
	{
	  if (strchr (prefix, *mnemonic_strs[i]) != NULL)
	    strsnlen[i].str = mnemonic_strs[i] + 1;
	  else
	    strsnlen[i].str = mnemonic_strs[i];
	  strsnlen[i].len = strlen (strsnlen[i].str);
	}

      /* With the prefixes gone, try to combine strings.  */
      size_t nstrsnlen = 1;
      for (i = 1; i < nmnemonic_strs; ++i)
	{
	  size_t j;
	  for (j = 0; j < nstrsnlen; ++j)
	    if (strsnlen[i].len > strsnlen[j].len
		&& strcmp (strsnlen[j].str,
			   strsnlen[i].str + (strsnlen[i].len
					      - strsnlen[j].len)) == 0)
	      {
		strsnlen[j] = strsnlen[i];
		break;
	      }
	    else if (strsnlen[i].len < strsnlen[j].len
		     && strcmp (strsnlen[i].str,
				strsnlen[j].str + (strsnlen[j].len
						   - strsnlen[i].len)) == 0)
	      break;
;
	  if (j == nstrsnlen)
	      strsnlen[nstrsnlen++] = strsnlen[i];
	}

      size_t nsfx_char = compute_sfxfreq (nstrsnlen, strsnlen);

      for (size_t sfxbits = 0; (1u << sfxbits) < 2 * nsfx_char; ++sfxbits)
	{
	  char suffix[1 << sfxbits];

	  for (i = 0; i < (1u << sfxbits) - 1; ++i)
	    suffix[i] = sfxfreq[i].ch;
	  suffix[i] = '\0';

	  size_t newlen[nstrsnlen];

	  for (i = 0; i < nstrsnlen; ++i)
	    if (strchr (suffix, strsnlen[i].str[strsnlen[i].len - 1]) != NULL)
	      newlen[i] = strsnlen[i].len - 1;
	    else
	      newlen[i] = strsnlen[i].len;

	  char charused[256];
	  memset (charused, '\0', sizeof (charused));
	  size_t ncharused = 0;

	  const char *tablestr[nstrsnlen];
	  size_t ntablestr = 1;
	  tablestr[0] = strsnlen[0].str;
	  size_t table = newlen[0] + 1;
	  for (i = 1; i < nstrsnlen; ++i)
	    {
	      size_t j;
	      for (j = 0; j < ntablestr; ++j)
		if (newlen[i] > newlen[j]
		    && memcmp (tablestr[j],
			       strsnlen[i].str + (newlen[i] - newlen[j]),
			       newlen[j]) == 0)
		  {
		    table += newlen[i] - newlen[j];
		    tablestr[j] = strsnlen[i].str;
		    newlen[j] = newlen[i];
		    break;
		  }
		else if (newlen[i] < newlen[j]
		     && memcmp (strsnlen[i].str,
				tablestr[j] + (newlen[j] - newlen[i]),
				newlen[i]) == 0)
		  break;

	      if (j == ntablestr)
		{
		  table += newlen[i] + 1;
		  tablestr[ntablestr] = strsnlen[i].str;
		  newlen[ntablestr] = newlen[i];

		  ++ntablestr;
		}

	      for (size_t x = 0; x < newlen[j]; ++x)
		if (charused[((const unsigned char *) tablestr[j])[x]]++ == 0)
		  ++ncharused;
	    }

	  size_t ncharused_bits = 0;
	  i = 1;
	  while (i < ncharused)
	    {
	      i *= 2;
	      ++ncharused_bits;
	    }

	  size_t table_bits = 0;
	  i = 1;
	  while (i < table)
	    {
	      i *= 2;
	      ++table_bits;
	    }

	  size_t mnemonic_bits = table_bits + pfxbits + sfxbits;
	  size_t new_total = (((table + 7) / 8) * ncharused_bits + ncharused
			      + (pfxbits == 0 ? 0 : (1 << pfxbits) - 1)
			      + (sfxbits == 0 ? 0 : (1 << sfxbits) - 1)
			      + (((total_bits + mnemonic_bits + 7) / 8)
				 * ninstructions));

	  if (new_total < best_so_far)
	    {
	      best_so_far = new_total;
	      best_mnemonic_bits = mnemonic_bits;

	      free (best_suffix);
	      best_suffix = xstrdup (suffix);

	      free (best_prefix);
	      best_prefix = xstrdup (prefix);
	      best_prefix_bits = pfxbits;

	      best_table_size = table;
	      best_table_bits = table_bits;
	      char *cp = best_table = xrealloc (best_table, table);
	      for (i = 0; i < ntablestr; ++i)
		{
		  assert (cp + newlen[i] + 1 <= best_table + table);
		  cp = mempcpy (cp, tablestr[i], newlen[i]);
		  *cp++ = '\0';
		}
	      assert (cp == best_table + table);
	    }
	}
    }

  fputs ("static const char mnemonic_table[] =\n\"", outfile);
  for (size_t i = 0; i < best_table_size; ++i)
    {
      if (((i + 1) % 60) == 0)
	fputs ("\"\n\"", outfile);
      if (!isascii (best_table[i]) || !isprint (best_table[i]))
	fprintf (outfile, "\\%03o", best_table[i]);
      else
	fputc (best_table[i], outfile);
    }
  fputs ("\";\n", outfile);

  if (best_prefix[0] != '\0')
    fprintf (outfile,
	     "static const char prefix[%zu] = \"%s\";\n"
	     "#define PREFIXCHAR_BITS %zu\n",
	     strlen (best_prefix), best_prefix, best_prefix_bits);
  else
    fputs ("#define NO_PREFIX\n", outfile);

  if (best_suffix[0] != '\0')
    fprintf (outfile, "static const char suffix[%zu] = \"%s\";\n",
	     strlen (best_suffix), best_suffix);
  else
    fputs ("#define NO_SUFFIX\n", outfile);

  for (size_t i = 0; i < nmnemonic_strs; ++i)
    {
      const char *mne = mnemonic_strs[i];

      size_t pfxval = 0;
      char *cp = strchr (best_prefix, *mne);
      if (cp != NULL)
	{
	  pfxval = 1 + (cp - best_prefix);
	  ++mne;
	}

      size_t l = strlen (mne);

      size_t sfxval = 0;
      cp = strchr (best_suffix, mne[l - 1]);
      if (cp != NULL)
	{
	  sfxval = 1 + (cp - best_suffix);
	  --l;
	}

      char *off = memmem (best_table, best_table_size, mne, l);
      while (off[l] != '\0')
	{
	  off = memmem (off + 1, best_table_size, mne, l);
	  assert (off != NULL);
	}

      fprintf (outfile, "#define MNE_%s %#zx\n",
	       mnemonic_strs[i],
	       (off - best_table)
	       + ((pfxval + (sfxval << best_prefix_bits)) << best_table_bits));
    }
}
#endif
