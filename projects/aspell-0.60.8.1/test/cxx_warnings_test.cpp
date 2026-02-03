 #include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aspell.h>

const uint16_t test_word[] = {'c','a','f', 0x00E9, 0};
const uint16_t test_incorrect[] = {'c','a','f', 'e', 0};
//const uint16_t test_doc[] = {'T', 'h', 'e', ' ', 'c','a','f', 0x00E9, '.', 0};

int fail = 0;

void f1() {
  AspellConfig * spell_config = new_aspell_config();
  aspell_config_replace(spell_config, "master", "en_US-w_accents");
  aspell_config_replace(spell_config, "encoding", "ucs-2");
  AspellCanHaveError * possible_err = new_aspell_speller(spell_config);
  AspellSpeller * spell_checker = 0;
  if (aspell_error_number(possible_err) != 0) {
    fprintf(stderr, "%s", aspell_error_message(possible_err));
    exit(0);
  } else {
    spell_checker = to_aspell_speller(possible_err);
  }
  int correct = aspell_speller_check_w(spell_checker, test_word, -1);
  if (!correct) {
    fprintf(stderr, "%s", "fail: expected word to be correct\n");
    fail = 1;
  }
  correct = aspell_speller_check_w(spell_checker, test_incorrect, -1);
  if (correct) {
    fprintf(stderr, "%s", "fail: expected word to be incorrect\n");
    fail = 1;
  }
  const AspellWordList * suggestions = aspell_speller_suggest_w(spell_checker, test_incorrect, -1);
  AspellStringEnumeration * elements = aspell_word_list_elements(suggestions);
  const uint16_t * word = aspell_string_enumeration_next_w(uint16_t, elements);
  if (memcmp(word, test_word, sizeof(test_incorrect)) != 0) {
    fprintf(stderr, "%s", "fail: first suggesion is not what is expected\n");
    fail = 1;
  delete_aspell_string_enumeration(elements);
  }
  if (fail)
    printf("not ok\n");
  else
    printf("ok\n");
}

void f2() {
  AspellConfig * spell_config = new_aspell_config();
  aspell_config_replace(spell_config, "master", "en_US-w_accents");
  aspell_config_replace(spell_config, "encoding", "ucs-2");
  AspellCanHaveError * possible_err = new_aspell_speller(spell_config);
  AspellSpeller * spell_checker = 0;
  if (aspell_error_number(possible_err) != 0) {
    fprintf(stderr, "%s", aspell_error_message(possible_err));
    exit(0);
  } else {
    spell_checker = to_aspell_speller(possible_err);
  }
  int correct = aspell_speller_check_w(spell_checker, test_word, -1);
  if (!correct) {
    fprintf(stderr, "%s", "fail: expected word to be correct\n");
    fail = 1;
  }
  correct = aspell_speller_check_w(spell_checker, test_incorrect, -1);
  if (correct) {
    fprintf(stderr, "%s", "fail: expected word to be incorrect\n");
    fail = 1;
  }
  const AspellWordList * suggestions = aspell_speller_suggest_w(spell_checker, test_incorrect, -1);
  AspellStringEnumeration * elements = aspell_word_list_elements(suggestions);
  const uint16_t * word = aspell_string_enumeration_next_w(uint16_t, elements);
  if (memcmp(word, test_word, sizeof(test_incorrect)) != 0) {
    fprintf(stderr, "%s", "fail: first suggesion is not what is expected\n");
    fail = 1;
  delete_aspell_string_enumeration(elements);
  }
  if (fail)
    printf("not ok\n");
  else
    printf("ok\n");
}
