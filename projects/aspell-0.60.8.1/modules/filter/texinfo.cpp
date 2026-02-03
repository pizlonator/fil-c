// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "string.hpp"
#include "indiv_filter.hpp"
#include "string_map.hpp"
#include "vector.hpp"

namespace {

  using namespace acommon;

  class TexInfoFilter : public IndividualFilter 
  {
  private:
    struct Command {
      bool ignore;
      Command(bool i = false) : ignore(i) {}
    };

    struct Table {
      String name;
      bool ignore_item;
      Table(ParmStr n) : name(n), ignore_item(false) {}
    };

    String last_command;

    String env_command;
    int env_ignore;

    int ignore;
    bool in_line_command;
    bool seen_input;
    Vector<Command> stack;
    Vector<Table>   table_stack;

    StringMap to_ignore;
    StringMap to_ignore_env;

    void reset_stack();
    
  public:
    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  //
  //
  //

  PosibErr<bool> TexInfoFilter::setup(Config * opts) 
  {
    name_ = "texinfo-filter";
    order_num_ = 0.35;
    
    to_ignore.clear();
    opts->retrieve_list("f-texinfo-ignore", &to_ignore);
    opts->retrieve_list("f-texinfo-ignore-env", &to_ignore_env);
    
    reset();
    return true;
  }
  
  void TexInfoFilter::reset_stack() 
  {
    stack.clear();
    stack.push_back(Command());
    in_line_command = false;
    ignore = 0;
  }

  void TexInfoFilter::reset() 
  {
    reset_stack();
    seen_input = false;
    env_command.clear();
    env_ignore = 0;
    table_stack.clear();
    table_stack.push_back(Table(""));
  }

  void TexInfoFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;

    while (cur != stop) {

      if (*cur == ' ') {

        ++cur;

      } else if (*cur == '@') {

        *cur = ' ';
        ++cur;
        if (cur == stop) break;
        if (asc_isalpha(*cur)) {

          bool was_table = last_command == table_stack.back().name
            && (last_command == "table" || last_command == "ftable" 
                || last_command == "vtable");

          last_command.clear();
          while (cur != stop && asc_isalpha(*cur)) {
            last_command += *cur;
            *cur = ' ';
            ++cur;
          }

          if (env_ignore) {

            // do nothing

          } else if (was_table) {

            if (to_ignore.have(last_command))
              table_stack.back().ignore_item = true;

          } else {
            
            if (cur == stop || *cur != '{')
              in_line_command = true;
            else
              ++cur;
            
            if (to_ignore.have(last_command) 
                || (table_stack.back().ignore_item 
                    && (last_command == "item" || last_command == "itemx")))
            {
              stack.push_back(Command(true));
              ++ignore;
            } else {
              stack.push_back(Command(false));
            }
            
            if (last_command == "end") {
              // do nothing as end command is special
            } else if (env_command.empty() && to_ignore_env.have(last_command)) {
              env_command = last_command;
              env_ignore = 1;
            } else if (env_command == last_command) {
              env_ignore++;
            } else if (last_command.suffix("table")) {
              table_stack.push_back(Table(last_command));
            }

          }

        } else {

          *cur = ' ';
          ++cur;
      }
        
      } else if (!seen_input && *cur == '\\' && stop - cur >= 6 
                 && cur[1] == 'i' && cur[2] == 'n' && cur[3] == 'p' 
                 && cur[4] == 'u' && cur[5] == 't' /* i.e '\input' */) {
        
        last_command.clear();
        for (int i = 0; i != 6; ++i)
          *cur++ = ' ';
        stack.push_back(Command(true));
        ++ignore;
        in_line_command = true;
        seen_input = true;

      } else if (last_command == "end") {

        last_command.clear();

        while (cur != stop && asc_isalpha(*cur)) {
          last_command += *cur;
          *cur = ' ';
          ++cur;
        }

        if (last_command == env_command) {

          --env_ignore;
          if (env_ignore <= 0) {
            env_ignore = 0;
            env_command.clear();
          }

        } else if (last_command == table_stack.back().name) {

          table_stack.pop_back();
          if (table_stack.empty()) table_stack.push_back(Table(""));

        }

        last_command.clear();
        
      } else {

        last_command.clear();
        if (*cur == '{') {
          stack.push_back(Command());
        } else if (*cur == '}') {
          if (stack.back().ignore) {
            --ignore;
            if (ignore < 0) ignore = 0;
          }
          stack.pop_back();
          if (stack.empty())
            stack.push_back(Command());
        } else if (in_line_command && *cur == '\n') {
          reset_stack();
        } else if (ignore || env_ignore) {
          *cur = ' ';
        }
        ++cur;

      }

    }
  }

}

C_EXPORT 
IndividualFilter * new_aspell_texinfo_filter() {return new TexInfoFilter;}

