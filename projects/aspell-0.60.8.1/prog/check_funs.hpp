// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef check_fun__hh
#define check_fun__hh

#include "checker_string.hpp"
#include "parm_string.hpp"
#include "stack_ptr.hpp"
#include "vector.hpp"

#define MENU_HEIGHT 9

using namespace acommon;

extern StackPtr<CheckerString> state;

extern const char * last_prompt;
struct Choice {
  char choice; 
  const char * desc;
  Choice() {}
  Choice(char c, ParmString d) : choice(c), desc(d) {}
};
typedef Vector<Choice> Choices;
extern StackPtr<Choices> word_choices;
extern StackPtr<Choices> menu_choices;

void get_choice(int & choice);
void get_line(String & line);
void begin_check();
void display_misspelled_word();
void display_menu();
void prompt(const char * prompt);
void error(const char * error);

#define control(key) (1 + (key-'a'))
  
#endif
