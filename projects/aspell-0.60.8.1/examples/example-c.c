/* This file is part of The New Aspell
 * Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/. 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aspell.h"

static void print_word_list(AspellSpeller * speller, 
			    const AspellWordList *wl,
			    char delem) 
{
  if (wl == 0) {
    printf("Error: %s\n", aspell_speller_error_message(speller));
  } else {
    AspellStringEnumeration * els = aspell_word_list_elements(wl);
    const char * word;
    while ( (word = aspell_string_enumeration_next(els)) != 0) {
      fputs(word, stdout);
      putc(delem, stdout);
    }
    delete_aspell_string_enumeration(els);
  }
}

#define check_for_error(speller)                                  \
  if (aspell_speller_error(speller) != 0) {                       \
    printf("Error: %s\n", aspell_speller_error_message(speller)); \
    break;                                                        \
  }

#define check_for_config_error(config)                            \
  if (aspell_config_error(config) != 0) {                         \
    printf("Error: %s\n", aspell_config_error_message(config));   \
    break;                                                        \
  }

static void check_document(AspellSpeller * speller, const char * file);

int main(int argc, const char *argv[]) 
{
  AspellCanHaveError * ret;
  AspellSpeller * speller;
  int have;
  char word[81];
  char * p;
  char * word_end;
  AspellConfig * config;

  if (argc < 2) {
    printf("Usage: %s <language> [<size>|- [[<jargon>|- [<encoding>]]]\n", argv[0]);
    return 1;
  }

  config = new_aspell_config();

  aspell_config_replace(config, "lang", argv[1]);

  if (argc >= 3 && argv[2][0] != '-' && argv[2][1] != '\0')
    aspell_config_replace(config, "size", argv[2]);

  if (argc >= 4 && argv[3][0] != '-')
    aspell_config_replace(config, "jargon", argv[3]);

  if (argc >= 5 && argv[4][0] != '-')
    aspell_config_replace(config, "encoding", argv[4]);

  ret = new_aspell_speller(config);

  delete_aspell_config(config);

  if (aspell_error(ret) != 0) {
    printf("Error: %s\n",aspell_error_message(ret));
    delete_aspell_can_have_error(ret);
    return 2;
  }
  speller = to_aspell_speller(ret);
  config = aspell_speller_config(speller);

  fputs("Using: ",                                      stdout);
  fputs(aspell_config_retrieve(config, "lang"),         stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "jargon"),       stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "size"),         stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "module"),       stdout);
  fputs("\n\n",                                         stdout);

  puts("Type \"h\" for help.\n");

  while (fgets(word, 80, stdin) != 0) {

    /* remove trailing spaces */

    word_end = strchr(word, '\0') - 1;
    while (word_end != word && (*word_end == '\n' || *word_end == ' ')) 
      --word_end;
    ++word_end;
    *word_end = '\0';
    
    putchar('\n');
    switch (word[0]) {
    case '\0':
      break;
    case 'h':
      puts(
	"Usage: \n"
	"  h(elp)      help\n"
	"  c <word>    check if a word is the correct spelling\n"
	"  s <word>    print out a list of suggestions for a word\n"
	"  a <word>    add a word to the personal word list\n"
	"  i <word>    ignore a word for the rest of the session\n"
        "  d <file>    spell checks a document\n"
	"  p           dumps the personal word list\n"
	"  P           dumps the session word list\n"
	"  m           dumps the main  word list\n"
        "  o <option> <value> sets a config option\n"
	"  r <option>         retrieves a config option\n"
        "  l <option>         retrieves a config option as a list\n"
	"  S           saves all word lists\n"
	"  C           clear the current session word list\n"
	"  x           quit\n"	);
      break;
    case 'p':
      print_word_list(speller, 
		      aspell_speller_personal_word_list(speller), '\n');
      break;
    case 'P':
      print_word_list(speller, 
		      aspell_speller_session_word_list(speller), '\n');
      break;
    case 'm':
      print_word_list(speller, 
		      aspell_speller_main_word_list(speller), '\n');
      break;
    case 'S':
      aspell_speller_save_all_word_lists(speller);
      check_for_error(speller);
      break;
    case 'C': 
      aspell_speller_clear_session(speller);
      check_for_error(speller);
      break;
    case 'x':
      goto END;
    case 'c':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	have = aspell_speller_check(speller, word + 2, -1);
	if (have == 1) 
	  puts("correct");
	else if (have == 0)
	  puts("incorrect");
	else
	  printf("Error: %s\n", aspell_speller_error_message(speller));
      }
      break;
    case 's':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	print_word_list(speller, 
			aspell_speller_suggest(speller, word + 2, -1), '\n');
      }
      break;
    case 'a':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	aspell_speller_add_to_personal(speller, word + 2, -1);
	check_for_error(speller);
      }
      break;
    case 'i':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	aspell_speller_add_to_session(speller, word + 2, -1);
	check_for_error(speller);
      }
      break;
    case 'o':
      word[80] = '\0'; /* to make sure strchr doesn't run off end of string */
      p = strchr(word + 3, ' ');
      if (strlen(word) < 3 || p == 0) {
	printf("Usage: %c <option> <value>\n", word[0]);
      } else {
	*p = '\0';
	++p;
	aspell_config_replace(config, word + 2, p);
	check_for_config_error(config);
      }
      break;
    case 'r':
      if (strlen(word) < 3) {
	printf("Usage: %c <option>\n", word[0]);
      } else {
	const char * val = aspell_config_retrieve(config, word + 2);
	check_for_config_error(config);
	if (val)
	  printf("%s = \"%s\"\n", word + 2, val);
      }
      break;
    case 'l':
      if (strlen(word) < 3) {
	printf("Usage: %c <option>\n", word[0]);
      } else {
	AspellStringList * lst = new_aspell_string_list();
	AspellMutableContainer * lst0 
	  = aspell_string_list_to_mutable_container(lst);
	AspellStringEnumeration * els;
	const char * val;
	aspell_config_retrieve_list(config, word + 2, lst0);
	check_for_config_error(config);
	els = aspell_string_list_elements(lst);
	printf("%s:\n", word + 2);
	while ( (val = aspell_string_enumeration_next(els)) != 0)
	  printf("  %s\n", val);
	delete_aspell_string_enumeration(els);
	delete_aspell_string_list(lst);
      }
      break;
    case 'd':
      if (strlen(word) < 3) {
	printf("Usage: %c <file>\n", word[0]);
      } else {
	check_document(speller, word + 2);
	printf("\n");
      }
      break;
    default:
      printf("Unknown Command: %s\n", word);
    }
    putchar('\n');
  }
 END:
  delete_aspell_speller(speller);
  return 0;
}

static void check_document(AspellSpeller * speller, const char * filename)
{
  /* For readablity this function does not worry about buffer overrun.
     This is meant as an illustrative example only.  Please do not
     attempt to spell check your documents with this function. */

  AspellCanHaveError * ret;
  AspellDocumentChecker * checker;
  AspellToken token;
  FILE * doc, * out;
  char line[256], repl[256], checked_filename[256];
  int diff;
  unsigned int repl_len;
  char * word_begin;

  /* Open the file */
  doc = fopen(filename, "r");
  if (doc <= 0) {
    printf("Error: Unable to open the file \"%s\" for reading.", filename);
    return;
  }

  /* Open filename.checked for writing the results */
  strcpy(checked_filename, filename);
  strcat(checked_filename, ".checked");
  out = fopen(checked_filename, "w");
  if (out <= 0) {
    printf("Error: Unable to open the file \"%s\" for writing.", 
	   checked_filename);
    fclose(doc);
    return;
  }

  /* Set up the document checker */
  ret = new_aspell_document_checker(speller);
  if (aspell_error(ret) != 0) {
    printf("Error: %s\n",aspell_error_message(ret));
    fclose(out);
    fclose(doc);
    return;
  }
  checker = to_aspell_document_checker(ret);

  while (fgets(line, 256, doc)) 
  {
    /* First process the line */
    aspell_document_checker_process(checker, line, -1);

    diff = 0;

    /* Now find the misspellings in the line */
    while (token = aspell_document_checker_next_misspelling(checker),
	   token.len != 0)
    {
      /* Print out the misspelling and get a replacement from the user */

      /* Pay particular attention to how token.offset and diff is used */
	 
      word_begin = line + token.offset + diff;
      printf("%.*s*%.*s*%s",
	     (int)(token.offset + diff), line,
	     (int)token.len, word_begin,
	     word_begin + token.len);

      printf("Suggestions: ");
      print_word_list(speller, 
		      aspell_speller_suggest(speller, word_begin, token.len), 
		      ' ');
      printf("\n");

      printf("Replacement? ");
      fgets(repl, 256, stdin);
      printf("\n");
      if (repl[0] == '\n') continue; /* ignore the current misspelling */
      repl_len = strlen(repl) - 1;
      repl[repl_len] = '\0';

      /* Replace the misspelled word with the replacement */
      diff += repl_len - token.len;
      memmove(word_begin + repl_len, word_begin + token.len,
	      strlen(word_begin + token.len) + 1);
      memcpy(word_begin, repl, repl_len);
    }

    /* print the line to filename.checked */
    fputs(line, out);
  }

  delete_aspell_document_checker(checker);

  fclose(out);
  fclose(doc);

  printf("Done.  Results saved to \"%s\".", checked_filename);
}

