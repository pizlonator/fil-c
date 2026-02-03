// This file is part of The New Aspell Copyright (C)
// 2002,2003,2004,2011,2019,2023 by Kevin Atkinson
// under the GNU LGPL license version 2.0 or 2.1.  You should have received a
// copy of the LGPL license along with this library if you did not you can
// find it at http://www.gnu.org/.

//
// NOTE: This program currently uses a very ugly mix of the internal
//       API and the external C interface.  The eventual goal is to
//       use only the external C++ interface, however, the external
//       C++ interface is currently incomplete.  The C interface is
//       used in some places because without the strings will not get
//       converted properly when the encoding is not the same as the
//       internal encoding used by Aspell.
// 

#include <ctype.h>
#include "settings.h"

#ifdef USE_LOCALE
# include <locale.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

#include "aspell.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef USE_FILE_INO
# include <unistd.h>
# include <fcntl.h>
#endif

#include "asc_ctype.hpp"
#include "check_funs.hpp"
#include "config.hpp"
#include "convert.hpp"
#include "document_checker.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "info.hpp"
#include "iostream.hpp"
#include "posib_err.hpp"
#include "speller.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_map.hpp"
#include "word_list.hpp"
#include "istream_enumeration.hpp"

#include "string_list.hpp"
#include "speller_impl.hpp"
#include "data.hpp"

#include "hash-t.hpp"
#include "hash_fun.hpp"

#include "gettext.h"

using namespace acommon;

using aspeller::Conv;

// action functions declarations

void print_ver();
void print_help(bool verbose = false);
void config();

void check();
void pipe();
void convt();
void normlz();
void filter();
void list();
void dicts();
void modes();
void filters();

void clean();
void master();
void personal();
void repl();
void soundslike();
void munch();
void expand();
void combine();
void munch_list();
void dump_affix();

void print_error(ParmString msg)
{
  CERR.printf(_("Error: %s\n"), msg.str());
}

void print_error(ParmString msg, ParmString str)
{
  CERR.put(_("Error: "));
  CERR.printf(msg.str(), str.str());
  CERR.put('\n');
}

#define EXIT_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  } while(false)
#define EXIT_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr< type > pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  else {var=pe.data;}\
  } while(false)
#define BREAK_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  } while(false)
#define BREAK_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr< type > pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  else {var=pe.data;}\
  } while(false)


/////////////////////////////////////////////////////////
//
// Command line options functions and classes
// (including main)
//

typedef Vector<String> Args;
typedef Config         Options;
enum Action {do_create, do_merge, do_dump, do_test, do_other};

Args              args;
StackPtr<Options> options;
Action            action  = do_other;

struct PossibleOption {
  const char * name;
  char         abrv;
  int          num_arg;
  bool         is_command;
};

#define OPTION(name,abrv,num)         {name,abrv,num,false}
#define COMMAND(name,abrv,num)        {name,abrv,num,true}
#define ISPELL_COMP(abrv,num)         {"",abrv,num,false}

const PossibleOption possible_options[] = {
  OPTION("master",           'd', 1),
  OPTION("personal",         'p', 1),
  OPTION("ignore",           'W', 1),
  OPTION("lang",             'l', 1),
  OPTION("backup",           'b', 0),
  OPTION("dont-backup",      'x', 0),
  OPTION("run-together",     'C', 0),
  OPTION("dont-run-together",'B', 0),
  OPTION("guess",            'm', 0),
  OPTION("dont-guess",       'P', 0),
  
  COMMAND("usage",     '?',  0),
  COMMAND("help",      '\0', 0),
  COMMAND("version",   'v',  0),
  COMMAND("config",    '\0', 0),
  COMMAND("dicts",     '\0', 0),
  COMMAND("check",     'c',  0),
  COMMAND("pipe",      'a',  0),
  COMMAND("list",      '\0', 0),
  COMMAND("conv",      '\0', 2),
  COMMAND("norm",      '\0', 1),
  COMMAND("filter",    '\0', 0),
  COMMAND("soundslike",'\0', 0),
  COMMAND("munch",     '\0', 0),
  COMMAND("expand",    '\0', 0),
  COMMAND("combine",   '\0', 0),
  COMMAND("munch-list",'\0', 0),
  COMMAND("clean",     '\0', 0),
  COMMAND("filters",   '\0', 0),
  COMMAND("modes",     '\0', 0),

  COMMAND("dump",   '\0', 1),
  COMMAND("create", '\0', 1),
  COMMAND("merge",  '\0', 1),

  ISPELL_COMP('S',0), ISPELL_COMP('w',1), ISPELL_COMP('T',1),

  {"",'\0'}, {"",'\0'}
};

const PossibleOption * possible_options_end = possible_options + sizeof(possible_options)/sizeof(PossibleOption) - 2;

struct ModeAbrv {
  char abrv;
  const char * mode;
  const char * desc;
};
static const ModeAbrv mode_abrvs[] = {
  {'e', "mode=email", N_("enter Email mode.")},
  {'H', "mode=html",  N_("enter HTML mode.")},
  {'t', "mode=tex",   N_("enter TeX mode.")},
  {'n', "mode=nroff", N_("enter Nroff mode.")},
  {'M', "mode=markdown", N_("enter Markdown mode.")},
};

static const ModeAbrv *  mode_abrvs_end = mode_abrvs + sizeof(mode_abrvs)/sizeof(ModeAbrv);

const PossibleOption * find_option(char c) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end && i->abrv != c) 
    ++i;
  return i;
}

static inline bool str_equal(const char * begin, const char * end, 
			     const char * other) 
{
  while(begin != end && *begin == *other)
    ++begin, ++other;
  return (begin == end && *other == '\0');
}

static const PossibleOption * find_option(const char * begin, const char * end) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
	 && !str_equal(begin, end, i->name))
    ++i;
  return i;
}

static const PossibleOption * find_option(const char * str) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
	 && strcmp(str, i->name) != 0)
    ++i;
  return i;
}

static void line_buffer() {
#ifndef WIN32
  // set up stdin and stdout to be line buffered
  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0); 
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
#endif
}

Conv dconv;
Conv uiconv;

int main (int argc, const char *argv[]) 
{
  options = new_config(); // this needs to be here because of a bug
                          // with static initlizers on Darwin.
#ifdef USE_LOCALE
  setlocale (LC_ALL, "");
#endif
  aspell_gettext_init();

  options->set_committed_state(false);

  if (argc == 1) {print_help(); return 0;}

  int i = 1;
  const PossibleOption * o;
  const char           * parm;

  //
  // process command line options by setting the appropriate options
  // in "options" and/or pushing non-options onto "argv"
  //
  PossibleOption other_opt = OPTION("",'\0',0);
  String option_name;
  while (i != argc) {
    if (argv[i][0] == '-') {
      bool have_parm = false;
      if (argv[i][1] == '-') {
	// a long arg
	const char * c = argv[i] + 2;
	while(*c != '=' && *c != '\0') ++c;
	o = find_option(argv[i] + 2, c);
	if (o == possible_options_end) {
	  option_name.assign(argv[i] + 2, c - argv[i] - 2);
          other_opt.name    = option_name.c_str();
          other_opt.num_arg = -1;
          o = &other_opt;
	}
	if (*c == '=') {have_parm = true; ++c;}
	parm = c;
      } else {
	// a short arg
	const ModeAbrv * j = mode_abrvs;
	while (j != mode_abrvs_end && j->abrv != argv[i][1]) ++j;
	if (j == mode_abrvs_end) {
	  o = find_option(argv[i][1]);
	  if (argv[i][1] == 'v' && argv[i][2] == 'v')
	    // Hack for -vv
	    parm = argv[i] + 3;
	  else
	    parm = argv[i] + 2;
	} else { // mode option
	  other_opt.name = "mode";
	  other_opt.num_arg = 1;
	  o = &other_opt;
	  parm = j->mode + 5;
	}
        if (*parm) have_parm = true;
      }
      if (o == possible_options_end) {
	print_error(_("Invalid Option: %s"), argv[i]);
	return 1;
      }
      int num_parms;
      if (o->num_arg == 0) {
        num_parms = 0;
	if (parm[0] != '\0') {
	  print_error(_("'%s' does not take any parameters."),
		      String(argv[i], parm - argv[i]));
	  return 1;
	}
	i += 1;
      } else if (have_parm) {
        num_parms = 1;
        i += 1;
      } else if (i + 1 == argc || argv[i+1][0] == '-') {
        if (o->num_arg == -1) {
          num_parms = 0;
          i += 1;
        } else {
          print_error(_("You must specify a parameter for \"%s\"."), argv[i]);
          return 1;
        }
      } else {
        num_parms = o->num_arg;
        parm = argv[i + 1];
        i += 2;
      }
      if (o->is_command) {
	args.push_back(o->name);
	if (o->num_arg == 1)
	  args.push_back(parm);
      } else if (o->name[0] != '\0') {
        Config::Entry * entry = new Config::Entry;
        entry->key = o->name;
        entry->value = parm;
        entry->need_conv = true;
        if (num_parms == -1) {
          entry->place_holder = args.size();
          args.push_back(parm);
        }
        options->set(entry);
      }
    } else {
      args.push_back(argv[i]);
      i += 1;
    }
  }

  EXIT_ON_ERR(options->read_in_settings());

  const char * codeset = 0;
#ifdef HAVE_LANGINFO_CODESET
  codeset = nl_langinfo(CODESET);
  if (ascii_encoding(*options, codeset)) codeset = 0;
#endif

// #ifdef USE_LOCALE
//   if (!options->have("encoding") && codeset)
//     EXIT_ON_ERR(options->replace("encoding", codeset));
// #endif

  Vector<int> to_remove;
  EXIT_ON_ERR(options->commit_all(&to_remove, codeset));
  for (int i = to_remove.size() - 1; i >= 0; --i) {
    args.erase(args.begin() + to_remove[i]);
  }

  if (args.empty()) {
    print_error(_("You must specify an action"));
    return 1;
  }

  String action_str = args.front();
  args.pop_front();
  const PossibleOption * action_opt = find_option(action_str.str());
  if (!action_opt->is_command) {
    print_error(_("Unknown Action: %s"),  action_str);
    return 1;
  } else if (action_opt->num_arg == 1 && args.empty()) {
    print_error(_("You must specify a parameter for \"%s\"."), action_str);
    return 1;
  } else if (action_opt->num_arg > (int)args.size()) {
    CERR.printf(_("Error: You must specify at least %d parameters for \"%s\".\n"), 
                action_opt->num_arg, action_str.str());
    return 1;
  }

  //
  // perform the requested action
  //
  if (action_str == "usage")
    print_help();
  else if (action_str == "help")
    print_help(true);
  else if (action_str == "version")
    print_ver();
  else if (action_str == "config")
    config();
  else if (action_str == "dicts")
    dicts();
  else if (action_str == "check")
    check();
  else if (action_str == "pipe")
    pipe();
  else if (action_str == "list")
    list();
  else if (action_str == "conv")
    convt();
  else if (action_str == "norm")
    normlz();
  else if (action_str == "filter")
    filter();
  else if (action_str == "soundslike")
    soundslike();
  else if (action_str == "munch")
    munch();
  else if (action_str == "expand")
    expand();
  else if (action_str == "combine")
    combine();
  else if (action_str == "munch-list")
    munch_list();
  else if (action_str == "clean")
    clean();
  else if (action_str == "filters")
    filters();
  else if (action_str == "modes")
    modes();
  else if (action_str == "dump")
    action = do_dump;
  else if (action_str == "create")
    action = do_create;
  else if (action_str == "merge")
    action = do_merge;
  else
    abort(); // this should not happen

  if (action != do_other) {
    if (args.empty()) {
      print_error(_("Unknown Action: %s"),  action_str);
      return 1;
    }
    String what_str = args.front();
    args.pop_front();
    if (what_str == "config")
      config();
    else if (what_str == "dicts")
      dicts();
    else if (what_str == "filters")
      filters();
    else if (what_str == "modes")
      modes();
    else if (what_str == "master")
      master();
    else if (what_str == "personal")
      personal();
    else if (what_str == "repl")
      repl();
    else if (what_str == "affix")
      dump_affix();
    else {
      print_error(_("Unknown Action: %s"),
		  String(action_str + " " + what_str));
      return 1;
    }
  }

  return 0;

}


/////////////////////////////////////////////////////////
//
// Action Functions
//
//

  
static Convert * setup_conv(const aspeller::Language * lang,
                            Config * config)
{
  if (config->retrieve("encoding") != "none") {
    PosibErr<Convert *> pe = new_convert_if_needed(*config,
                                                   lang->charmap(),
                                                   config->retrieve("encoding"),
                                                   NormTo);
    if (pe.has_err()) {print_error(pe.get_err()->mesg); exit(1);}
    return pe.data;
  } else {
    return 0;
  }
}

static Convert * setup_conv(Config * config,
                            const aspeller::Language * lang)
{
  if (config->retrieve("encoding") != "none") {
    PosibErr<Convert *> pe = new_convert_if_needed(*config,
                                                   config->retrieve("encoding"),
                                                   lang->charmap(),
                                                   NormFrom);
    if (pe.has_err()) {print_error(pe.get_err()->mesg); exit(1);}
    return pe.data;
  } else {
    return 0;
  }
}

void setup_display_conv()
{
  const char * gettext_enc = 0;
  const char * env_enc = 0;
  String doc_enc = options->retrieve("encoding");
  String enc;
#ifdef ENABLE_NLS
  gettext_enc = bind_textdomain_codeset("aspell", 0);
  if (ascii_encoding(*options,gettext_enc)) gettext_enc = 0;
#endif
#ifdef HAVE_LANGINFO_CODESET
  env_enc = nl_langinfo(CODESET);
  if (ascii_encoding(*options, env_enc)) env_enc = 0;
#endif
  if (gettext_enc && env_enc && strcmp(gettext_enc,env_enc) != 0) 
  {
    fputs(("Error: bind_textdomain_codeset != nl_langinfo(CODESET)\n"), stderr);
    exit(-1);
  }
  if (gettext_enc)
    enc = gettext_enc;
  else if (env_enc)
    enc = env_enc;
  else
    enc = doc_enc;

  EXIT_ON_ERR(dconv.setup(*options, doc_enc, enc, NormNone));
  EXIT_ON_ERR(uiconv.setup(*options, enc, doc_enc, NormNone));
}


///////////////////////////
//
// config
//

void config ()
{
  if (args.size() == 0) {
    load_all_filters(options);
    options->write_to_stream(COUT);
  } else {
    EXIT_ON_ERR_SET(options->retrieve_any(args[0]), String, value);
    COUT << value << "\n";
  }
}

///////////////////////////
//
// dicts
//

void dicts() 
{
  const DictInfoList * dlist = get_dict_info_list(options);

  StackPtr<DictInfoEnumeration> dels(dlist->elements());

  const DictInfo * entry;

  while ( (entry = dels->next()) != 0) 
    puts(entry->name);
}

///////////////////////////
//
// list available (filters/filter modes)
//

void list_available(PosibErr<StringPairEnumeration *> (*fun)(Config *))
{
  EXIT_ON_ERR_SET(fun(options), StringPairEnumeration *, els);
  StringPair sp;
  while (!els->at_end()) {
    sp = els->next();
    printf("%-14s %s\n", sp.first, gt_(sp.second));
  }
  delete els;
}

void filters()
{
  load_all_filters(options);
  list_available(available_filters);
}

void modes()
{
  list_available(available_filter_modes);
}

///////////////////////////
//
// pipe
//

// precond: strlen(str) > 0
char * trim_wspace (char * str)
{
  int last = strlen(str) - 1;
  while (asc_isspace(str[0])) {
    ++str;
    --last;
  }
  while (last > 0 && asc_isspace(str[last])) {
    --last;
  }
  str[last + 1] = '\0';
  return str;
}

bool get_word_pair(char * line, char * & w1, char * & w2)
{
  w2 = strchr(line, ',');
  if (!w2) {
    print_error(_("Invalid Input"));
    return false;
  }
  *w2 = '\0';
  ++w2;
  w1 = trim_wspace(line);
  w2 = trim_wspace(w2);
  return true;
}

void print_elements(const AspellWordList * wl) {
  AspellStringEnumeration * els = aspell_word_list_elements(wl);
  int count = 0;
  const char * w;
  String line;
  while ( (w = aspell_string_enumeration_next(els)) != 0 ) {
    ++count;
    line += w;
    line += ", ";
  }
  line.resize(line.size() - 2);
  COUT.printf("%u: %s\n", count, line.c_str());
}

struct StatusFunInf 
{
  aspeller::SpellerImpl * real_speller;
  Conv oconv;
  bool verbose;
  StatusFunInf(Convert * c) : oconv(c) {}
};

void status_fun(void * d, Token, int correct)
{
  StatusFunInf * p = static_cast<StatusFunInf *>(d);
  if (p->verbose && correct) {
    const CheckInfo * ci = p->real_speller->check_info();
    if (ci->compound)
      COUT.put("-\n");
    else if (ci->pre_flag || ci->suf_flag)
      COUT.printf("+ %s\n", p->oconv(ci->word.str, ci->word.len));
    else
      COUT.put("*\n");
  }
}

DocumentChecker * new_checker(AspellSpeller * speller, 
			      StatusFunInf & status_fun_inf) 
{
  EXIT_ON_ERR_SET(new_document_checker(reinterpret_cast<Speller *>(speller)),
		  StackPtr<DocumentChecker>, checker);
  checker->set_status_fun(status_fun, &status_fun_inf);
  return checker.release();
}

#define BREAK_ON_SPELLER_ERR\
  do {if (aspell_speller_error(speller)) {\
    print_error(aspell_speller_error_message(speller)); break;\
  } } while (false)

void pipe() 
{
  line_buffer();

  bool terse_mode = true;
  bool do_time = options->retrieve_bool("time");
  bool suggest = options->retrieve_bool("suggest");
  bool include_guesses = options->retrieve_bool("guess");
  clock_t start,finish;

  if (!options->have("mode") && !options->have("filter")) {
    PosibErrBase err(options->replace("mode", "nroff"));
    if (err.has_err()) 
      CERR.printf(_("WARNING: Unable to enter Nroff mode: %s\n"),
                  err.get_err()->mesg);
  }

  start = clock();
  
  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);
  aspeller::SpellerImpl * real_speller = reinterpret_cast<aspeller::SpellerImpl *>(speller);
  Config * config = real_speller->config();
  Conv iconv(setup_conv(config, &real_speller->lang()));
  Conv oconv(setup_conv(&real_speller->lang(), config));
  MBLen mb_len;
  if (!config->retrieve_bool("byte-offsets")) 
    mb_len.setup(*config, config->retrieve("encoding"));
  if (do_time)
    COUT << _("Time to load word list: ")
         << (clock() - start)/(double)CLOCKS_PER_SEC << "\n";
  StatusFunInf status_fun_inf(setup_conv(&real_speller->lang(), config));
  status_fun_inf.real_speller = real_speller;
  bool & print_star = status_fun_inf.verbose;
  print_star = true;
  StackPtr<DocumentChecker> checker(new_checker(speller, status_fun_inf));
  int c;
  const char * w;
  CharVector buf;
  char * line;
  char * line0;
  char * word;
  char * word2;
  int    ignore;
  PosibErrBase err;

  print_ver();

  for (;;) {
    buf.clear();
    fflush(stdout);
    while (c = getchar(), c != '\n' && c != EOF)
      buf.push_back(static_cast<char>(c));
    buf.push_back('\n'); // always add new line so strlen > 0
    buf.push_back('\0');
    //CERR.printf("%s", buf.data());
    line = buf.data();
    ignore = 0;
    switch (line[0]) {
    case '\n':
      if (c != EOF) continue;
      else          break;
    case '*':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '&':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal
	(speller, 
	 real_speller->to_lower(word), -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '@':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_session(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '#':
      aspell_speller_save_all_word_lists(speller);
      BREAK_ON_SPELLER_ERR;
      break;
    case '+':
      word = trim_wspace(line + 1);
      err = config->replace("mode", word);
      if (err.get_err())
	config->replace("mode", "tex");
      reload_filters(real_speller);
      checker.del();
      checker = new_checker(speller, status_fun_inf);
      break;
    case '-':
      config->remove("filter");
      reload_filters(real_speller);
      checker.del();
      checker = new_checker(speller, status_fun_inf);
      break;
    case '~':
      break;
    case '!':
      terse_mode = true;
      print_star = false;
      break;
    case '%':
      terse_mode = false;
      print_star = true;
      break;
    case '$':
      if (line[1] == '$') {
	switch(line[2]) {
	case 'r':
	  switch(line[3]) {
	  case 'a':
	    if (get_word_pair(line + 4, word, word2))
	      aspell_speller_store_replacement(speller, word, -1, word2, -1);
	    break;
	  }
	  break;
	case 'c':
	  switch (line[3]) {
	  case 's':
	    if (get_word_pair(line + 4, word, word2))
	      BREAK_ON_ERR(err = config->replace(word, word2));
            if (strcmp(word,"suggest") == 0)
              suggest = config->retrieve_bool("suggest");
            else if (strcmp(word,"time") == 0)
              do_time = config->retrieve_bool("time");
            else if (strcmp(word,"guess") == 0)
              include_guesses = config->retrieve_bool("guess");
	    break;
	  case 'r':
	    word = trim_wspace(line + 4);
	    BREAK_ON_ERR_SET(config->retrieve(word), String, ret);
            COUT.printl(ret);
	    break;
	  }
	  break;
	case 'p':
	  switch (line[3]) {
	  case 'p':
	    print_elements(aspell_speller_personal_word_list(speller));
	    break;
	  case 's':
	    print_elements(aspell_speller_session_word_list(speller));
	    break;
	  }
	  break;
	case 'l':
	  COUT.printl(config->retrieve("lang"));
	  break;
	}
	break;
      } else {
	// continue on (no break)
      }
    case '^':
      ignore = 1;
    default:
      line0 = line;
      line += ignore;
      checker->process(line, strlen(line));
      while (Token token = checker->next_misspelling()) {
	word = line + token.offset;
	word[token.len] = '\0';
        const char * cword = iconv(word);
        String guesses, guess;
        const CheckInfo * ci = real_speller->check_info();
        aspeller::CasePattern casep 
          = real_speller->lang().case_pattern(cword);
        while (ci) {
          guess.clear();
          if (ci->pre_add && ci->pre_add[0])
            guess.append(ci->pre_add, ci->pre_add_len).append('+');
          guess.append(ci->word.str, ci->word.len);
          if (ci->pre_strip_len > 0) 
            guess.append('-').append(ci->word.str, ci->pre_strip_len);
          if (ci->suf_strip_len > 0) 
            guess.append('-').append(ci->word.str + ci->word.len - ci->suf_strip_len, 
                                     ci->suf_strip_len);
          if (ci->suf_add && ci->suf_add[0])
            guess.append('+').append(ci->suf_add, ci->suf_add_len);
          guess.ensure_null_end();
          real_speller->lang().fix_case(casep, guess.data(), guess.data());
          guesses << ", " << oconv(guess.str());
          ci = ci->next;
        }
	start = clock();
        const AspellWordList * suggestions = 0;
        if (suggest) 
          suggestions = aspell_speller_suggest(speller, word, -1);
	finish = clock();
        unsigned offset = mb_len(line0, token.offset + ignore);
	if (suggestions && !aspell_word_list_empty(suggestions)) 
        {
          COUT.printf("& %s %u %u:", word, 
                      aspell_word_list_size(suggestions), offset);
	  AspellStringEnumeration * els 
	    = aspell_word_list_elements(suggestions);
	  if (options->retrieve_bool("reverse")) {
	    Vector<String> sugs;
	    sugs.reserve(aspell_word_list_size(suggestions));
	    while ( ( w = aspell_string_enumeration_next(els)) != 0)
	      sugs.push_back(w);
	    Vector<String>::reverse_iterator i = sugs.rbegin();
	    while (true) {
              COUT.printf(" %s", i->c_str());
	      ++i;
	      if (i == sugs.rend()) break;
              COUT.put(',');
	    }
	  } else {
	    while ( ( w = aspell_string_enumeration_next(els)) != 0) {
              COUT.printf(" %s%s", w, 
                          aspell_string_enumeration_at_end(els) ? "" : ",");
	    }
	  }
	  delete_aspell_string_enumeration(els);
          if (include_guesses)
            COUT.put(guesses);
	  COUT.put('\n');
	} else {
          if (guesses.empty())
            COUT.printf("# %s %u\n", word, offset);
          else
            COUT.printf("? %s 0 %u: %s\n", word, offset,
                        guesses.c_str() + 2);
	}
	if (do_time)
          COUT.printf(_("Suggestion Time: %f\n"), 
                      (finish-start)/(double)CLOCKS_PER_SEC);
      }
      COUT.put('\n');
    }
    if (c == EOF) break;
  }

  delete_aspell_speller(speller);
}

///////////////////////////
//
// check
//

enum UserChoice {None, Ignore, IgnoreAll, Replace, ReplaceAll, 
		 Add, AddLower, Exit, Abort};

struct Mapping {
  char primary[9];
  UserChoice reverse[256];
  void to_aspell();
  void to_ispell();
  char & operator[] (UserChoice c) {return primary[c];}
  UserChoice & operator[] (char c) 
    {return reverse[static_cast<unsigned char>(c)];}
};

void abort_check();

void setup_display_conv();

void check()
{
  String file_name;
  String new_name;
  FILE * in = 0;
  FILE * out = 0;
  Mapping mapping;
  bool changed = false;

  if (args.size() == 0) {
    print_error(_("You must specify a file name."));
    exit(-1);
  } else if (args.size() > 1) {
    print_error(_("Only one file name may be specified."));
    exit(-1);
  }
  
  file_name = args[0];
  new_name = file_name;
  new_name += ".new";

  in = fopen(file_name.c_str(), "r");
  if (!in) {
    print_error(_("Could not open the file \"%s\" for reading"), file_name);
    exit(-1);
  }
    
  if (!options->have("mode"))
    EXIT_ON_ERR(set_mode_from_extension(options, file_name));
    
  String m = options->retrieve("keymapping");
  if (m == "aspell")
    mapping.to_aspell();
  else if (m == "ispell")
    mapping.to_ispell();
  else {
    print_error(_("Invalid keymapping: %s"), m);
    exit(-1);
  }

  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }

  {
    struct stat st;
    fstat(fileno(in), &st);
    if (!S_ISREG(st.st_mode)) {
      print_error(_("\"%s\" is not a regular file"), file_name);
      exit(-1);
    }
#ifdef USE_FILE_INO
    int fd = open(new_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd >= 0) out = fdopen(fd, "w");
#else
    out = fopen(new_name.c_str(), "w");
#endif
  }
  if (!out) {
    print_error(_("Could not open the file \"%s\" for writing. File not saved."), file_name);
    exit(-1);
  }

  setup_display_conv();

  AspellSpeller * speller = to_aspell_speller(ret);

  state = new CheckerString(speller,in,out,64);
 
  word_choices = new Choices;

  menu_choices = new Choices;
  menu_choices->push_back(Choice(mapping[Ignore],     _("Ignore")));
  menu_choices->push_back(Choice(mapping[IgnoreAll],  _("Ignore all")));
  menu_choices->push_back(Choice(mapping[Replace],    _("Replace")));
  menu_choices->push_back(Choice(mapping[ReplaceAll], _("Replace all")));
  menu_choices->push_back(Choice(mapping[Add],        _("Add")));
  menu_choices->push_back(Choice(mapping[AddLower],   _("Add Lower")));
  menu_choices->push_back(Choice(mapping[Abort],      _("Abort")));
  menu_choices->push_back(Choice(mapping[Exit],       _("Exit")));

  String word0, new_word;
  Vector<String> sug_con;
  StackPtr<StringMap> replace_list(new_string_map());
  const char * w;

  begin_check();

  while (state->next_misspelling()) {

    char * word = state->get_real_word(word0);

    //
    // check if it is in the replace list
    //

    if ((w = replace_list->lookup(word)) != 0) {
      state->replace(w);
      continue;
    }

    //
    // print the line with the misspelled word highlighted;
    //

    display_misspelled_word();

    //
    // print the suggestions and menu choices
    //

    const AspellWordList * suggestions = aspell_speller_suggest(speller, word, -1);
    AspellStringEnumeration * els = aspell_word_list_elements(suggestions);
    sug_con.resize(0);
    while (sug_con.size() != 10 
           && (w = aspell_string_enumeration_next(els)) != 0)
      sug_con.push_back(w);
    delete_aspell_string_enumeration(els);

    // disable suspend
    unsigned int suggestions_size = sug_con.size();
    unsigned int suggestions_mid = suggestions_size / 2;
    if (suggestions_size % 2) suggestions_mid++; // if odd
    word_choices->resize(0);
    for (unsigned int j = 0; j != suggestions_mid; ++j) {
      word_choices->push_back(Choice('0' + j+1, sug_con[j]));
      if (j + suggestions_mid != suggestions_size) 
        word_choices
          ->push_back(Choice(j+suggestions_mid+1 == 10 
                             ? '0' 
                             : '0' + j+suggestions_mid+1,
                             sug_con[j+suggestions_mid]));
    }
    //enable suspend
    display_menu();

  choice_prompt:

    prompt("? ");

  choice_loop:

    //
    // Handle the users choice
    //

    int choice;
    get_choice(choice);
      
    if (choice == '0') choice = '9' + 1;
    
    switch (mapping[choice]) {
    case Exit:
      goto exit_loop;
    case Abort: {
      prompt(_("Are you sure you want to abort (y/n)? "));
      get_choice(choice);
      /* TRANSLATORS: The user may input any of these characters to say "yes".
         MUST ONLY CONSIST OF ASCII CHARACTERS. */
      const char * yes_characters = _("Yy");
      if (strchr(yes_characters, choice) != 0)
        goto abort_loop;
      goto choice_prompt;
    }
    case Ignore:
      break;
    case IgnoreAll:
      aspell_speller_add_to_session(speller, word, -1);
      break;
    case Add:
      aspell_speller_add_to_personal(speller, word, -1);
      break;
    case AddLower: 
    {
      // Emulate the c function add_to_personal, but add extra step to
      // convert word to lowercase.  Yeah its a bit of a hack.
      Speller * sp = reinterpret_cast<Speller *>(speller);
      sp->temp_str_0.clear();
      sp->to_internal_->convert(word, -1, sp->temp_str_0);
      char * lower = sp->to_lower(sp->temp_str_0.mstr());
      PosibErr<void> ret = sp->add_to_personal(MutableString(lower));
      sp->err_.reset(ret.release_err());
      break;
    }
    case Replace:
    case ReplaceAll:
      // the string new_word is in the encoding of the document
      prompt(_("With: "));
      get_line(new_word);
      if (new_word.size() == 0)
        goto choice_prompt;
      if (new_word[0] >= '1' && new_word[0] < (char)suggestions_size + '1')
        new_word = sug_con[new_word[0]-'1'];
      state->replace(new_word);
      changed = true;
      if (mapping[choice] == ReplaceAll && (strcmp(word,new_word.str()) != 0))
        replace_list->replace(word, new_word);
      break;
    default:
      // the replasments are in the encoding of the document
      if (choice >= '1' && choice < (char)suggestions_size + '1') { 
        state->replace(sug_con[choice-'1']);
        changed = true;
      } else {
        error(_("Sorry that is an invalid choice!"));
        goto choice_loop;
      }
    }
  }
exit_loop:
  {
    aspell_speller_save_all_word_lists(speller);
    state.del(); // to close the file handles
    delete_aspell_speller(speller);

    if (changed) {

      bool keep_backup = options->retrieve_bool("backup");
      if (keep_backup) {
        String backup_name = file_name;
        backup_name += ".bak";
        rename_file(file_name, backup_name);
      }
      rename_file(new_name, file_name);

    } else {

      remove_file(new_name);

    }
    
    //end_check();
    
    return;
  }
abort_loop:
  {
    state->abort(); // to close the file handles
    delete_aspell_speller(speller);

    remove_file(new_name);

    return;
  }
}

#define U (unsigned char)

void Mapping::to_aspell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = 'i';
  reverse[U'i'] = Ignore;
  reverse[U' '] = Ignore;
  reverse[U'\n'] = Ignore;

  primary[IgnoreAll ] = 'I';
  reverse[U'I'] = IgnoreAll;

  primary[Replace   ] = 'r';
  reverse[U'r'] = Replace;

  primary[ReplaceAll] = 'R';
  reverse[U'R'] = ReplaceAll;

  primary[Add       ] = 'a';
  reverse[U'A'] = Add;
  reverse[U'a'] = Add;

  primary[AddLower  ] = 'l';
  reverse[U'L'] = AddLower;
  reverse[U'l'] = AddLower;

  primary[Abort     ] = 'b';
  reverse[U'b'] = Abort;
  reverse[U'B'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'x';
  reverse[U'x'] = Exit;
  reverse[U'X'] = Exit;
}

void Mapping::to_ispell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = ' ';
  reverse[U' '] = Ignore;
  reverse[U'\n'] = Ignore;

  primary[IgnoreAll ] = 'A';
  reverse[U'A'] = IgnoreAll;
  reverse[U'a'] = IgnoreAll;

  primary[Replace   ] = 'R';
  reverse[U'R'] = ReplaceAll;
  reverse[U'r'] = Replace;

  primary[ReplaceAll] = 'E';
  reverse[U'E'] = ReplaceAll;
  reverse[U'e'] = Replace;

  primary[Add       ] = 'I';
  reverse[U'I'] = Add;
  reverse[U'i'] = Add;

  primary[AddLower  ] = 'U';
  reverse[U'U'] = AddLower;
  reverse[U'u'] = AddLower;

  primary[Abort     ] = 'Q';
  reverse[U'Q'] = Abort;
  reverse[U'q'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'X';
  reverse[U'X'] = Exit;
  reverse[U'x'] = Exit;
}
#undef U

///////////////////////////
//
// list
//

void list()
{
  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);

  state = new CheckerString(speller,stdin,0,64);

  String word;
 
  while (state->next_misspelling()) {

    state->get_real_word(word);
    COUT.printl(word);

  }
  
  state.del(); // to close the file handles
  delete_aspell_speller(speller);
}

///////////////////////////
//
// convt
//

void convt()
{
  Conv conv;
  String buf1, buf2;
  const char * from = fix_encoding_str(args[0], buf1);
  const char * to   = fix_encoding_str(args[1], buf2);
  Normalize norm = NormNone;
  if (strcmp(from, "utf-8") == 0 && strcmp(to, "utf-8") != 0)
    norm = NormFrom;
  else if (strcmp(from, "utf-8") != 0 && strcmp(to, "utf-8") == 0)
    norm = NormTo;
  if (args.size() > 2) {
    for (String::iterator i = args[2].begin(); i != args[2].end(); ++i)
      *i = asc_tolower(*i);
    options->replace("normalize", "true");
    if (args[2] == "none")
      options->replace("normalize", "false");
    else if (args[2] == "internal")
      options->replace("norm-strict", "false");
    else if (args[2] == "strict")
      options->replace("norm-strict", "true");
    else
      EXIT_ON_ERR(options->replace("norm-form", args[2]));
  }
  EXIT_ON_ERR(conv.setup(*options, args[0], args[1], norm));
  String line;
  while (CIN.getline(line))
    COUT.printl(conv(line));
}

void normlz()
{
  options->replace("normalize", "true");
  const char * from = args.size() < 3 ? "utf-8" : args[0].str();
  const char * to   = args.size() < 3 ? "utf-8" : args[2].str();
  const char * intr = args.size() < 3 ? args[0].str() : args[1].str();
  String * form = (args.size() == 2   ? &args[1] 
                   : args.size() == 4 ? &args[3]
                   : 0);
  Normalize decode_norm = NormTo;
  if (form) {
    for (String::iterator i = form->begin(); i != form->end(); ++i)
      *i = asc_tolower(*i);
    if (*form == "internal") {
      options->replace("norm-strict", "false");
      decode_norm = NormNone;
    } else if (*form == "strict") {
      options->replace("norm-strict", "true");
      decode_norm = NormNone;
    }
    if (decode_norm == NormTo) EXIT_ON_ERR(options->replace("norm-form", *form));
  }
  Conv encode,decode;
  EXIT_ON_ERR(encode.setup(*options, from, intr, NormFrom));
  EXIT_ON_ERR(decode.setup(*options, intr, to, decode_norm));
  String line;
  while (CIN.getline(line))
    COUT.printl(decode(encode(line)));
}

///////////////////////////
//
// filter
//

void filter()
{
  line_buffer();

  String enc = options->retrieve("encoding");
  if (enc == "none")
    enc="utf-8";
  String encoding;
  fix_encoding_str(enc, encoding);

  EXIT_ON_ERR_SET(Decode::get_new(encoding, options.get()),
                  StackPtr<Decode>, iconv);
  EXIT_ON_ERR_SET(Encode::get_new(encoding, options.get()),
                  StackPtr<Encode>, oconv);

  StackPtr<Filter> filter(new Filter);
  EXIT_ON_ERR(setup_filter(*filter, options.get(), true, true, false));

  CharVector buf;
  FilterCharVector proc_str;

  int c;
  for (;;) {
    buf.clear();
    while (c = getchar(), c != '\n' && c != EOF)
      buf.push_back(static_cast<char>(c));
    if (c == EOF) break;
    buf.push_back('\n');
    buf.push_back('\0');
    proc_str.clear();
    iconv->decode(buf.data(), buf.size()-1, proc_str);
    proc_str.append(0);
    FilterChar * begin = proc_str.pbegin();
    FilterChar * end   = proc_str.pend() - 1;
    filter->process(begin, end);
    buf.clear();
    oconv->encode(begin,end,buf);
    if (buf.empty() || buf.back() != '\n')
      buf.push_back('\n');
    COUT.write(buf);
  }
}

///////////////////////////
//
// print_ver
//

void print_ver () {
  printf("@(#) International Ispell Version 3.1.20 " 
         "(but really Aspell %s)\n", aspell_version_string());
}

///////////////////////////////////////////////////////////////////////
//
// These functions use implementation details of the default speller
// module
//

///////////////////////////
//
// clean
//

void clean()
{
  using namespace aspeller;

  bool strict = args.size() != 0 && args[0] == "strict";
  
  Config * config = options;

  CachePtr<Language> lang;
  find_language(*config);
  PosibErr<Language *> res = new_language(*config);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  IstreamEnumeration in(CIN);
  WordListIterator wl_itr(&in, lang, &CERR);
  config->replace("validate-words", "true");
  config->replace("validate-affixes", "true");
  if (!strict)
    config->replace("clean-words", "true");
  config->replace("clean-affixes", "true");
  config->replace("skip-invalid-words", "true");
  wl_itr.init(*config);
  Conv oconv, oconv2;
  if (config->have("encoding")) {
    EXIT_ON_ERR(oconv.setup(*config, lang->charmap(), config->retrieve("encoding"), NormTo));
    EXIT_ON_ERR(oconv2.setup(*config, lang->charmap(), config->retrieve("encoding"), NormTo));
  } else {
    EXIT_ON_ERR(oconv.setup(*config, lang->charmap(), lang->data_encoding(), NormTo));
    EXIT_ON_ERR(oconv2.setup(*config, lang->charmap(), lang->data_encoding(), NormTo));
  }
  while (wl_itr.adv()) {
    if (*wl_itr->aff.str) 
      COUT.printf("%s/%s\n", oconv(wl_itr->word), oconv2(wl_itr->aff));
    else
      COUT.printl(oconv(wl_itr->word));
  }
}

///////////////////////////
//
// master
//

void dump (aspeller::Dict * lws, Convert * conv) 
{
  using namespace aspeller;

  switch (lws->basic_type) {
  case Dict::basic_dict:
    {
      Dictionary * ws = static_cast<Dictionary *>(lws);
      StackPtr<WordEntryEnumeration> els(ws->detailed_elements());
      WordEntry * wi;
      while (wi = els->next(), wi) {
        wi->write(COUT,*ws->lang(), conv);
        COUT << '\n';
      }
    }
    break;
  case Dict::multi_dict:
    {
      StackPtr<DictsEnumeration> els(lws->dictionaries());
      Dict * ws;
      while (ws = els->next(), ws) 
	dump (ws, conv);
    }
    break;
  default:
    abort();
  }
}

void master () {
  using namespace aspeller;

  if (args.size() != 0) {
    options->replace("master", args[0].c_str());
  }

  Config * config = options;

  if (action == do_create) {
    
    find_language(*config);
    EXIT_ON_ERR(create_default_readonly_dict
                (new IstreamEnumeration(CIN),
                 *config));

  } else if (action == do_merge) {
    
    print_error(_("Can't merge a master word list yet. Sorry."));
    exit (1);
    
  } else if (action == do_dump) {

    EXIT_ON_ERR_SET(add_data_set(config->retrieve("master-path"), *config), Dict *, d);
    StackPtr<Convert> conv(setup_conv(d->lang(), config));
    dump(d, conv);
  }
}

///////////////////////////
//
// personal
//

void personal () {
  using namespace aspeller;

  if (args.size() != 0) {
    EXIT_ON_ERR(options->replace("personal", args[0]));
  }
  options->replace("module", "aspeller");
  if (action == do_create || action == do_merge) {
    CERR << _("Sorry \"create/merge personal\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    StackPtr<Speller> speller(new_speller(options));

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("personal-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""), 
		    speller->config()->retrieve("personal-path"));
        exit (1);
      }
      speller->personal_word_list().data->clear();
    }

    String word;
    while (CIN >> word) 
      speller->add_to_personal(word);

    speller->save_all_word_lists();
#endif

  } else { // action == do_dump

    // FIXME: This is currently broken

    Config * config = options;
    Dictionary * per = new_default_writable_dict(*config);
    EXIT_ON_ERR(per->load(config->retrieve("personal-path"), *config));
    StackPtr<WordEntryEnumeration> els(per->detailed_elements());
    StackPtr<Convert> conv(setup_conv(per->lang(), config));

    WordEntry * wi;
    while (wi = els->next(), wi) {
      wi->write(COUT,*(per->lang()), conv);
      COUT.put('\n');
    }
    delete per;
  }
}

///////////////////////////
//
// repl
//

void repl() {
  using namespace aspeller;

  if (args.size() != 0) {
    options->replace("repl", args[0].c_str());
  }

  if (action == do_create || action == do_merge) {

    CERR << _("Sorry \"create/merge repl\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    SpellerImpl speller(options);

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("repl-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""),
		    speller->config()->retrieve("repl-path"));
        exit (1);
      }
      speller->personal_repl().clear();
    }
    
    try {
      String word,repl;

      while (true) {
	get_word_pair(word,repl,':');
	EXIT_ON_ERR(speller->store_repl(word,repl,false));
      }

    } catch (bad_cin) {}

    EXIT_ON_ERR(speller->personal_repl().synchronize());

#endif

  } else if (action == do_dump) {

    // FIXME: This is currently broken

    ReplacementDict * repl = new_default_replacement_dict(*options);
    repl->load(options->retrieve("repl-path"), *options);
    StackPtr<WordEntryEnumeration> els(repl->detailed_elements());
    
    WordEntry * rl = 0;
    WordEntry words;
    Conv conv(setup_conv(repl->lang(), options));
    while ((rl = els->next())) {
      repl->repl_lookup(*rl, words);
      do {
        COUT << conv(rl->word) << ": " << conv(words.word) << "\n";
      } while (words.adv());
    }
    delete repl;
  }
}

//////////////////////////
//
// soundslike
//

void soundslike() {
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word;
  String sl;
  line_buffer();
  while (CIN.getline(word)) {
    const char * w = iconv(word);
    lang->LangImpl::to_soundslike(sl, w);
    printf("%s\t%s\n", word.str(), oconv(sl));
  }
}

//////////////////////////
//
// munch
//

void munch() 
{
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word;
  GuessInfo gi;
  line_buffer();
  while (CIN.getline(word)) {
    lang->munch(iconv(word), &gi);
    COUT << word;
    for (const aspeller::CheckInfo * ci = gi.head; ci; ci = ci->next)
    {
      COUT << ' ' << oconv(ci->word.str, ci->word.len) << '/';
      if (ci->pre_flag != 0) COUT << oconv(static_cast<char>(ci->pre_flag));
      if (ci->suf_flag != 0) COUT << oconv(static_cast<char>(ci->suf_flag));
    }
    COUT << '\n';
  }
}

//////////////////////////
//
// expand
//

void expand() 
{
  int level = 1;
  if (args.size() > 0)
    level = atoi(args[0].c_str()); //FIXME: More verbose
  int limit = INT_MAX;
  if (args.size() > 1)
    limit = atoi(args[1].c_str());
  
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word, buf;
  ObjStack exp_buf;
  WordAff * exp_list;
  line_buffer();
  while (CIN.getline(word)) {
    buf = word;
    char * w = iconv(buf.mstr(), buf.size());
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }
    exp_buf.reset();
    exp_list = lang->expand(w, af, exp_buf, limit);
    if (level <= 2) {
      if (level == 2) 
        COUT << word << ' ';
      WordAff * p = exp_list;
      while (p) {
        COUT << oconv(p->word);
        if (limit < INT_MAX && p->aff[0]) COUT << '/' << oconv((const char *)p->aff);
        p = p->next;
        if (p) COUT << ' ';
      }
      COUT << '\n';
    } else if (level >= 3) {
      double ratio = 0;
      if (level >= 4) {
        for (WordAff * p = exp_list; p; p = p->next)
          ratio += p->word.size;
        ratio /= exp_list->word.size; // it is assumed the first
                                      // expansion is just the root
      }
      for (WordAff * p = exp_list; p; p = p->next) {
        COUT << word << ' ' << oconv(p->word);
        if (limit < INT_MAX && p->aff[0]) COUT << '/' << oconv((const char *)p->aff);
        if (level >= 4) COUT.printf(" %f\n", ratio);
        else COUT << '\n';
      }
    }
  }
}

//////////////////////////
//
// combine
//

static void combine_aff(String & aff, const char * app)
{
  for (; *app; ++app) {
    if (!memchr(aff.c_str(),*app,aff.size()))
      aff.push_back(*app);
  }
}

static void print_wordaff(const String & base, const String & affs, Conv & oconv)
{
  if (base.empty()) return;
  COUT << oconv(base);
  if (affs.empty())
    COUT << '\n';
  else
    COUT.printf("/%s\n", oconv(affs));
}

static bool lower_equal(aspeller::Language * l, ParmString a, ParmString b)
{
  if (a.size() != b.size()) return false;
  if (l->to_lower(a[0]) != l->to_lower(b[0])) return false;
  return memcmp(a + 1, b + 1, a.size() - 1) == 0;
}

void combine()
{
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word;
  String base;
  String affs;
  line_buffer();
  while (CIN.getline(word)) {
    word = iconv(word);

    CharVector buf; buf.append(word.c_str(), word.size() + 1);
    char * w = buf.data();
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }

    if (lower_equal(lang, base, w)) {
      if (lang->is_lower(base.str())) {
        combine_aff(affs, af);
      } else {
        base = w;
        combine_aff(affs, af);
      }
    } else {
      print_wordaff(base, affs, oconv);
      base = w;
      affs = af;
    }

  }
  print_wordaff(base, affs, oconv);
}

//////////////////////////
//
// munch list
//

void munch_list_simple();
void munch_list_complete(bool, bool);

void munch_list()
{
  bool simple = false;
  bool multi = false;
  bool simplify = true;

  for (unsigned i = 0; i < args.size(); ++i) {
    if (args[i] == "simple")      simple = true;
    else if (args[i] == "single") multi = false;
    else if (args[i] == "multi")  multi = true;
    else if (args[i] == "keep")   simplify = false;
    else 
    {
      print_error(_("\"%s\" is not a valid flag for the \"munch-list\" command."),
                  args[i]);
      exit(1);
    }
  }
  if (simple)
    munch_list_simple();
  else
    munch_list_complete(multi, simplify);
}

//
// munch list (simple version)
//

// This version works the same way as the myspell "munch" program.
// However, because the results depends on the hash table used and the
// order of the word list it wonn't produce identical results.

struct SML_WordEntry {
  const char * word;
  char * aff;
  bool keep; // boolean
  SML_WordEntry(const char * w = 0) : word(w), aff(0), keep(false) {}
};

struct SML_Parms {
  typedef SML_WordEntry Value;
  typedef const char * Key;
  static const bool is_multi = false;
  acommon::hash<const char *> hash;
  bool equal(Key x, Key y) {return strcmp(x,y) == 0;}
  Key key(const Value & v) {return v.word;}
};

typedef HashTable<SML_Parms> SML_Table;

static inline void add_affix(SML_Table::iterator b, char aff)
{
  char * p = b->aff;
  if (p) {while (*p) {if (*p == aff) return; ++p;}}
  int s = p - b->aff;
  b->aff = (char *)realloc(b->aff, s + 2);
  b->aff[s + 0] = aff;
  b->aff[s + 1] = '\0';
}

void munch_list_simple()
{
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word, buf;
  ObjStack exp_buf;
  WordAff * exp_list;
  GuessInfo gi;
  SML_Table table;
  ObjStack table_buf;

  // add words to dictionary
  while (CIN.getline(word)) {
    buf = word;
    char * w = iconv(buf.mstr(), buf.size());
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }
    exp_buf.reset();
    exp_list = lang->expand(w, af, exp_buf);
    for (WordAff * q = exp_list; q; q = q->next) {
      table.insert(SML_WordEntry(table_buf.dup(q->word)));
    }
  }

  // now try to munch each word in the dictionary
  SML_Table::iterator p = table.begin();
  SML_Table::iterator end = table.end();
  String flags;
  for (; p != end; ++p) 
  {
    const aspeller::CheckInfo * best = 0;
    unsigned min_base_size = INT_MAX;
    lang->munch(p->word, &gi);
    const aspeller::CheckInfo * ci = gi.head;
    while (ci)
    { {
      // check if the base word is in the dictionary
      SML_Table::iterator b = table.find(ci->word.str);
      if (b == table.end()) goto cont;

      // check if all the words once expanded are in the dictionary
      // this included the exiting flags due to pre-suf cross products
      if (b->aff) flags = b->aff;
      else        flags.clear();
      if (ci->pre_flag != 0) flags += ci->pre_flag;
      if (ci->suf_flag != 0) flags += ci->suf_flag;
      exp_buf.reset();
      exp_list = lang->expand(ci->word.str, flags, exp_buf);
      for (WordAff * q = exp_list; q; q = q->next) {
        if (!table.have(q->word)) goto cont;
      }

      // the base word and flags are valid, now keep the one with the
      // smallest base word
      if (ci->word.len < min_base_size) {
        min_base_size = ci->word.len;
        best = ci;
      }

    } cont:
      ci = ci->next;
    }
    // now add the base to the keep list if one exists
    // otherwise just keep the original word
    if (best) {
      SML_Table::iterator b = table.find(best->word.str);
      assert(b != table.end());
      if (best->pre_flag) add_affix(b, best->pre_flag);
      if (best->suf_flag) add_affix(b, best->suf_flag);
      b->keep = true;
    } else {
      p->keep = true;
    }
  }

  // Print the entries in the table marked as "to keep"
  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->keep) {
      COUT << oconv(p->word);
      if (p->aff) {
        COUT << '/' << oconv(p->aff);
      }
      COUT << '\n';
    }
  }

  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->aff) free(p->aff);
    p->aff = 0;
  }
}

//
// munch list (complete version)
//
//
// This version will produce a smaller list than the simple version.
// It is very close to the optimum result. 
//

//
// Hash table to store the words
//

struct CML_Entry {
  const char * word;
  char * aff;
  CML_Entry * parent;
  CML_Entry * next;
  int rank;
  CML_Entry(const char * w = 0) : word(w), aff(0), parent(0), next(0), rank(0) {}
};

struct CML_Parms {
  typedef CML_Entry Value;
  typedef const char * Key;
  static const bool is_multi = true;
  acommon::hash<const char *> hash;
  bool equal(Key x, Key y) {return strcmp(x,y) == 0;}
  Key key(const Value & v) {return v.word;}
};

typedef HashTable<CML_Parms> CML_Table;

//
// add an affix to a word but keep the prefixes and suffixes separate
//

static void add_affix(CML_Table::iterator b, char aff, bool prefix)
{
  char * p = b->aff;
  int s = 3;
  if (p) {
    while (*p) {
      if (*p == aff) return; 
      ++p;
    }
    s = (p - b->aff) + 2;
  }
  char * tmp = (char *)malloc(s);
  p = b->aff;
  char * q = tmp;
  if (p) {while (*p != '/') *q++ = *p++;}
  if (prefix) *q++ = aff;
  *q++ = '/';
  if (p) {p++; while (*p != '\0') *q++ = *p++;}
  if (!prefix) *q++ = aff;
  *q++ = '\0';
  assert(q - tmp == s);
  if (b->aff) free(b->aff);
  b->aff = tmp;
}

//
// Standard disjoint set algo with union by rank and path compression
//

static void link(CML_Entry * x, CML_Entry * y)
{
  if (x == y) return;
  if (x->rank > y->rank) {
    y->parent = x;
  } else {
    x->parent = y;
    if (x->rank == y->rank) y->rank++;
  }
}

static CML_Entry * find_set (CML_Entry * x) 
{
  if (x->parent)
    return x->parent = find_set(x->parent);
  else
    return x;
}

//
// Stuff to manage prefix-suffix combinations
//

struct PreSuf {
  String pre;
  String suf;
  String & get(int i) {return i == 0 ? pre : suf;}
  const String & get(int i) const {return i == 0 ? pre : suf;}
  PreSuf() : next(0) {}
  PreSuf * next;
};

class PreSufList {
public:
  PreSuf * head;
  PreSufList() : head(0) {}
  void add(PreSuf * to_add) {
    to_add->next = head;
    head = to_add;
  }
  void clear() {
    while (head) {
      PreSuf * tmp = head;
      head = head->next;
      delete tmp;
    }
  }
  void transfer(PreSufList & other) {
    clear();
    head = other.head;
    other.head = 0;
  }
  ~PreSufList() {
    clear();
  }
};


// Example of usage:
//   combine(in, res, 0)
//   Pre:  in =  [(ab, c) (ab, d) (c, de) (c, ef)]
//   Post: res = [(ab, cd), (c, def)]
static void combine(const PreSufList & in, PreSufList & res, int which)
{
  const PreSuf * i = in.head;
  while (i) { {
    const String & s = i->get(which);
    for (const PreSuf * j = in.head; j != i; j = j->next) {
      if (j->get(which) == s) goto cont;
    }
    PreSuf * tmp = new PreSuf;
    tmp->pre = i->pre;
    tmp->suf = i->suf;
    String & b = tmp->get(!which);
    for (const PreSuf * j = i->next; j; j = j->next) {
      if (j->get(which) != s) continue;
      const String & a = j->get(!which);
      for (String::const_iterator x = a.begin(); x != a.end(); ++x) {
        if (memchr(b.data(), *x, b.size())) continue;
        b += *x;
      }
    }
    res.add(tmp);
  } cont:
    i = i->next;
  }
}

//
// Stuff used when pruning the list of base words
//

struct Expansion {
  const char * word;
  char * aff; // modifying this will modify the affix entry in the hash table
  std::vector<bool> exp;
  std::vector<bool> orig_exp;
};

// static void dump(const Vector<Expansion *> & working, 
//                  const Vector<CML_Table::iterator> & entries)
// {
//   for (unsigned i = 0; i != working.size(); ++i) {
//     if (!working[i]) continue;
//     CERR.printf("%s/%s ", working[i]->word, working[i]->aff);
//     for (unsigned j = 0; j != working[i]->exp.size(); ++j) {
//       if (working[i]->exp[j])
//         CERR.printf("%s ", entries[j]->word);
//     }
//     CERR.put('\n');
//   }
//   CERR.put('\n');
// }

// standard set algorithms on a bit vector

static bool subset(const std::vector<bool> & smaller, 
                   const std::vector<bool> & larger)
{
  assert(smaller.size() == larger.size());
  unsigned s = larger.size();
  for (unsigned i = 0; i != s; ++i) {
    if (smaller[i] && !larger[i]) return false;
  }
  return true;
}

static void merge(std::vector<bool> & x, const std::vector<bool> & y)
{
  assert(x.size() == y.size());
  unsigned s = x.size();
  for (unsigned i = 0; i != s; ++i) {
    if (y[i]) x[i] = true;
  }
}

static void purge(std::vector<bool> & x, const std::vector<bool> & y)
{
  assert(x.size() == y.size());
  unsigned s = x.size();
  for (unsigned i = 0; i != s; ++i) {
    if (y[i]) x[i] = false;
  }
}

static inline unsigned count(const std::vector<bool> & x) {
  unsigned c = 0;
  for (unsigned i = 0; i != x.size(); ++i) {
    if (x[i]) ++c;
  }
  return c;
}

// 

struct WorkingLt {
  bool operator() (Expansion * x, Expansion * y) {

    // LARGEST number of expansions
    unsigned x_s = count(x->exp);
    unsigned y_s = count(y->exp);
    if (x_s != y_s) return x_s > y_s;

    // SMALLEST base word
    x_s = strlen(x->word);
    y_s = strlen(y->word);
    if (x_s != y_s) return x_s < y_s;

    // LARGEST affix string
    x_s = strlen(x->aff);
    y_s = strlen(y->aff);
    if (x_s != y_s) return x_s > y_s; 

    // 
    int cmp = strcmp(x->word, y->word);
    if (cmp != 0) return cmp < 0;

    //
    cmp = strcmp(x->aff, y->aff);
    return cmp < 0;
  }
};

//
// Finally the function that does the real work
//

void munch_list_complete(bool multi, bool simplify)
{
  using namespace aspeller;
  CachePtr<Language> lang;
  find_language(*options);
  PosibErr<Language *> res = new_language(*options);
  if (res.has_err()) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  Conv iconv(setup_conv(options, lang));
  Conv oconv(setup_conv(lang, options));
  String word, buf;
  ObjStack exp_buf;
  WordAff * exp_list;
  GuessInfo gi;
  CML_Table table;
  ObjStack table_buf;

  // add words to dictionary
  while (CIN.getline(word)) {
    buf = word;
    char * w = iconv(buf.mstr(), buf.size());
    char * af = strchr(w, '/');
    size_t s;
    if (af != 0) {
      s = af - w;
      *af++ = '\0';
    } else {
      s = strlen(w);
      af = w + s;
    }
    exp_buf.reset();
    exp_list = lang->expand(w, af, exp_buf);
    for (WordAff * q = exp_list; q; q = q->next) {
      if (!table.have(q->word)) // since it is a multi hash table
        table.insert(CML_Entry(table_buf.dup(q->word))).first;
    }
  }

  // Now try to munch each word in the dictionary.  This will also
  // group the base words into disjoint sets based on there expansion.
  CML_Table::iterator p = table.begin();
  CML_Table::iterator end = table.end();
  String flags;
  for (; p != end; ++p) 
  {
    lang->munch(p->word, &gi, false);
    const aspeller::CheckInfo * ci = gi.head;
    while (ci)
    { {
      // check if the base word is in the dictionary
      CML_Table::iterator b = table.find(ci->word.str);
      if (b == table.end()) goto cont;

      // check if all the words once expanded are in the dictionary
      char flags[2];
      assert(!(ci->pre_flag && ci->suf_flag));
      if      (ci->pre_flag != 0) flags[0] = ci->pre_flag;
      else if (ci->suf_flag != 0) flags[0] = ci->suf_flag;
      flags[1] = '\0';
      exp_buf.reset();
      exp_list = lang->expand(ci->word.str, flags, exp_buf);
      for (WordAff * q = exp_list; q; q = q->next) {
        if (!table.have(q->word)) goto cont;
      }

      // all the expansions are in the dictionary now add the affix to
      // the base word and figure out which disjoint set it belongs to
      add_affix(b, flags[0], ci->pre_flag != 0);
      CML_Entry * bs = find_set(&*b);
      for (WordAff * q = exp_list; q; q = q->next) {
        CML_Table::iterator w = table.find(q->word);
        assert(b != table.end());
        CML_Entry * ws = find_set(&*w);
        link(bs,ws);
      }

    } cont:
      ci = ci->next;
    }
  }

  // If a base word has both prefixes and suffixes try to combine them.
  // This can lead to multiple entries for the same base word.  If "multi"
  // is true, then include all the entries.  Otherwise, only include the
  // one with the largest number of expansions.  This is a greedy choice
  // that may not be optimal, but is close to it.
  p = table.begin();
  String pre,suf;
  CML_Entry * extras = 0;
  for (; p != end; ++p) 
  {
    pre.clear(); suf.clear();
    if (!p->aff) continue;
    char * s = p->aff;
    while (*s != '/') pre += *s++;
    ++s;
    while (*s != '\0') suf += *s++;
    if (pre.empty()) {

      strcpy(p->aff, suf.str());

    } else if (suf.empty()) {

      strcpy(p->aff, pre.str());

    } else {

      // Try all possible combinations and keep the ones which expand
      // to legal words.

      PreSufList cross,tmp1,tmp2;
      PreSuf * ps = 0;

      for (String::iterator pi = pre.begin(); pi != pre.end(); ++pi) {
        String::iterator si = suf.begin();
        while (si != suf.end()) { {
          char flags[3] = {*pi, *si, '\0'};
          exp_buf.reset();
          exp_list = lang->expand(p->word, flags, exp_buf);
          for (WordAff * q = exp_list; q; q = q->next) {
            if (!table.have(q->word)) goto cont2;
          }
          ps = new PreSuf;
          ps->pre += *pi;
          ps->suf += *si;
          cross.add(ps);
        } cont2:
          ++si;
        }
      }

      // Now combine the legal cross pairs with other ones when
      // possible.

      // final res = [ (pre, []) ([],suf),
      //               (cross | combine first | combine second)
      //               (cross | combine second | combine first)
      //             | combine first
      //             | combine second
      //
      // combine first [(ab, c) (ab, d) (c, de) (c, ef)]
      //   =  [(ab, cd), (c, def)]
      
      combine(cross, tmp1, 0); 
      combine(tmp1,  tmp2, 1);
      tmp1.clear();
      
      combine(cross, tmp1, 1);
      combine(tmp1,  tmp2, 0);
      tmp1.clear();

      cross.clear();

      ps = new PreSuf;
      ps->pre = pre;
      tmp2.add(ps);
      ps = new PreSuf;
      ps->suf = suf;
      tmp2.add(ps);

      combine(tmp2, tmp1, 0);
      combine(tmp1, cross, 1);

      if (multi) {

        // It is OK to have multiple entries with the same base word
        // so use them all.

        ps = cross.head;
        assert(ps);
        memcpy(p->aff, ps->pre.data(), ps->pre.size());
        memcpy(p->aff + ps->pre.size(), ps->suf.str(), ps->suf.size() + 1);
        
        ps = ps->next;
        CML_Entry * bs = find_set(&*p);
        for (; ps; ps = ps->next) {
          
          CML_Entry * tmp = new CML_Entry;
          tmp->word = p->word;
          tmp->aff = (char *)malloc(ps->pre.size() + ps->suf.size() + 1);
          memcpy(tmp->aff, ps->pre.data(), ps->pre.size());
          memcpy(tmp->aff + ps->pre.size(), ps->suf.str(), ps->suf.size() + 1);
          
          tmp->parent = bs;
          
          tmp->next = extras;
          extras = tmp;
        }

      } else {

        // chose the one which has the largest number of expansions

        int max_exp = 0;
        PreSuf * best = 0;
        String flags;

        for (ps = cross.head; ps; ps = ps->next) {
          flags  = ps->pre;
          flags += ps->suf;
          exp_buf.reset();
          exp_list = lang->expand(p->word, flags, exp_buf);
          int c = 0;
          for (WordAff * q = exp_list; q; q = q->next) ++c;
          if (c > max_exp) {max_exp = c; best = ps;}
        }

        memcpy(p->aff, best->pre.data(), best->pre.size());
        memcpy(p->aff + best->pre.size(), best->suf.str(), best->suf.size() + 1);
      }
    }
  }

  while (extras) {
    CML_Entry * tmp = extras;
    extras = extras->next;
    tmp->next = 0;
    table.insert(*tmp);
    delete tmp;
  }

  // Create a linked list for each disjoint set
  p = table.begin();
  for (; p != end; ++p) 
  {
    p->rank = -1;
    CML_Entry * bs = find_set(&*p);
    if (bs != &*p) {
      p->next = bs->next;
      bs->next = &*p;
    } 
  }

  // Now process each disjoint set independently
  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->parent) continue;

    Vector<CML_Table::iterator> entries;
    Vector<Expansion> expansions;
    Vector<Expansion *> to_keep;
    std::vector<bool> to_keep_exp;
    Vector<Expansion *> working;
    Vector<unsigned> to_remove;

    // First assign numbers to each unique word.  The rank field is
    // no longer used so use it to store the number.
    for (CML_Entry * q = &*p; q; q = q->next) {
      CML_Table::iterator e = table.find(q->word);
      if (e->rank == -1) {
        e->rank = entries.size();
        q->rank = entries.size();
        entries.push_back(e);
      } else {
        q->rank = e->rank;
      }
      if (q->aff) {
        Expansion tmp;
        tmp.word = q->word;
        tmp.aff  = q->aff;
        expansions.push_back(tmp);
      }
    }

    to_keep_exp.resize(entries.size());
    //for (int i = 0; i != to_keep_exp.size(); ++i) {
    //  printf(">>> %d %d\n", i, (int)to_keep_exp[i]);
    //}

    // Store the expansion of each base word in a bit vector and
    // add it to the working set
    for (Vector<Expansion>::iterator q = expansions.begin(); 
         q != expansions.end(); 
         ++q)
    {
      q->exp.resize(entries.size());
      exp_buf.reset();
      exp_list = lang->expand(q->word, q->aff, exp_buf);
      for (WordAff * i = exp_list; i; i = i->next) {
        CML_Table::iterator e = table.find(i->word);
        assert(0 <= e->rank && e->rank < (int)entries.size());
        q->exp[e->rank] = true;
      }
      q->orig_exp = q->exp;
      working.push_back(&*q);
    }
    
    unsigned prev_working_size = INT_MAX;

    // This loop will repeat until the working set is empty.  This
    // will produce optimum results in most cases.  Non optimum
    // results may be possible if step (4) is necessary, but in
    // practice this step is rarly necessary.
    do {
      prev_working_size = working.size();

      // Sort the list based on WorkingLt.  This is necessary every
      // time since the expansion list can change.
      std::sort(working.begin(), working.end(), WorkingLt());

      // (1) Eliminate any elements which are a subset of others
      for (unsigned i = 0; i != working.size(); ++i) {
        if (!working[i]) continue;
        for (unsigned j = i + 1; j != working.size(); ++j) {
          if (!working[j]) continue;
          if (subset(working[j]->exp, working[i]->exp)) {
            working[j] = 0;
          }
        }
      }

      // (2) Move any elements which expand to unique entree 
      // into the to_keep list
      to_remove.clear();
      for (unsigned i = 0; i != entries.size(); ++i) {
        int n = -1;
        for (unsigned j = 0; j != working.size(); ++j) {
          if (working[j] && working[j]->exp[i]) {
            if (n == -1) n = j;
            else         n = -2;
          }
        }
        if (n >= 0) to_remove.push_back(n);
      }
      for (unsigned i = 0; i != to_remove.size(); ++i) {
        unsigned n = to_remove[i];
        if (!working[n]) continue;
        to_keep.push_back(working[n]);
        merge(to_keep_exp, working[n]->exp);
        working[n] = 0;
      }

      // (3) Eliminate any elements which are a subset of all the
      // elements in the to_keep list
      for (unsigned i = 0; i != working.size(); ++i) {
        if (working[i] && subset(working[i]->exp, to_keep_exp)) {
          working[i] = 0;
        }
      }

      // Compact the working list
      {
        int i = 0, j = 0;
        while (j != (int)working.size()) {
          if (working[j]) {
            working[i] = working[j];
            ++i;
          }
          ++j;
        }
        working.resize(i);
      }

      // (4) If none of the entries in working have been removed via
      // the above methods then make a greedy choice and move the
      // first element into the to_keep list.
      if (working.size() > 0 && working.size() == prev_working_size)
      {
        to_keep.push_back(working[0]);
        //CERR.printf("Making greedy choice! Choosing %s/%s.\n",
        //            working[0]->word, working[0]->aff);
        merge(to_keep_exp, working[0]->exp);
        working.erase(working.begin(), working.begin() + 1);
      }

      // (5) Trim the expansion list for any elements left in the
      // working set by removing the expansions that already exist in
      // the to_keep list
      for (unsigned i = 0; i != working.size(); ++i) {
        purge(working[i]->exp, to_keep_exp);
      }

    } while (working.size() > 0);

    if (simplify) {

      // Remove unnecessary flags.  A flag is unnecessary if it does
      // does not expand to any new words, that is words that are not
      // already covered by an earlier entries in the list.

      for (unsigned i = 0; i != to_keep.size(); ++i) {
        to_keep[i]->exp = to_keep[i]->orig_exp;
      }
     
      std::sort(to_keep.begin(), to_keep.end(), WorkingLt());

      std::vector<bool> tally(entries.size());
      std::vector<bool> backup(entries.size());
      std::vector<bool> working(entries.size());
      String flags;
      
      for (unsigned i = 0; i != to_keep.size(); ++i) {

        backup = tally;

        merge(tally, to_keep[i]->exp);

        String flags_to_keep = to_keep[i]->aff;
        bool something_changed;
        do {
          something_changed = false;
          for (unsigned j = 0; j != flags_to_keep.size(); ++j) {
            flags.assign(flags_to_keep.data(), j);
            flags.append(flags_to_keep.data(j+1), 
                         flags_to_keep.size() - (j+1));
            working = backup;
            exp_buf.reset();
            exp_list = lang->expand(to_keep[i]->word, flags, exp_buf);
            for (WordAff * q = exp_list; q; q = q->next) {
              CML_Table::iterator e = table.find(q->word);
              working[e->rank] = true;
            }
            if (working == tally) {
              flags_to_keep = flags;
              something_changed = true;
              break;
            }
          }
        } while (something_changed);

        if (flags_to_keep != to_keep[i]->aff) {
          memcpy(to_keep[i]->aff, flags_to_keep.str(), flags_to_keep.size() + 1);
        }
      }
      
    }

    // Finally print the resulting list

    //printf("XXX %d %d\n", to_keep.size(), to_keep_exp.size());
    //for (int i = 0; i != to_keep_exp.size(); ++i) {
    //  printf(">>> %d %d\n", i, (int)to_keep_exp[i]);
    //}

    for (unsigned i = 0; i != to_keep.size(); ++i) {
      COUT << oconv(to_keep[i]->word);
      if (to_keep[i]->aff[0]) {
        COUT << '/';
        COUT << oconv(to_keep[i]->aff);
      }
      COUT << '\n';
    }
    for (unsigned i = 0; i != to_keep_exp.size(); ++i) {
      if (!to_keep_exp[i]) {
        assert(!entries[i]->aff);
        COUT.printf("%s\n", oconv(entries[i]->word));
      }
    }
  }

  p = table.begin();
  for (; p != end; ++p) 
  {
    if (p->aff) free(p->aff);
    p->aff = 0;
  }
}


//////////////////////////
//
// dump affix
//

void dump_affix()
{
  FStream in;
  EXIT_ON_ERR(aspeller::open_affix_file(*options, in));
  
  String line;
  while (in.getline(line))
    COUT << line << '\n';
}



///////////////////////////////////////////////////////////////////////


///////////////////////////
//
// print_help
//

void print_help_line(char abrv, char dont_abrv, const char * name, 
		     KeyInfoType type, const char * desc, bool no_dont = false) 
{
  String command;
  if (abrv != '\0') {
    command += '-';
    command += abrv;
    if (dont_abrv != '\0') {
      command += '|';
      command += '-';
      command += dont_abrv;
    }
    command += ',';
  }
  command += "--";
  if (type == KeyInfoBool && !no_dont) command += "[dont-]";
  if (type == KeyInfoList) command += "add|rem-";
  command += name;
  if (type == KeyInfoString || type == KeyInfoList) 
    command += "=<str>";
  if (type == KeyInfoInt)
    command += "=<int>";
  const char * tdesc = _(desc);
  char buf[120];
  int len = snprintf(buf, 120, "  %-27s", command.c_str());
  if (len == 29)
    printf("%s %s\n", buf, tdesc);
  else
    printf("%s\n%30s%s\n", buf, "", tdesc);
}

namespace acommon {
  PosibErr<ConfigModule *> get_dynamic_filter(Config * config, ParmStr value);
}

static const char * usage_text[] = 
{
  /* TRANSLATORS: These should all be formated to fit in 80 column or
     less */
  N_("Usage: aspell [options] <command>"),
  N_("<command> is one of:"),
  N_("  -?|usage         display a brief usage message"),
  N_("  help             display a detailed help message"),
  N_("  -c|check <file>  spellchecks a file"),
  N_("  -a|pipe          \"ispell -a\" compatibility mode"),
  N_("  [dump] config    dumps the current configuration to stdout"),
  N_("  config <key>     prints the current value of an option"),
  N_("  [dump] dicts | filters | modes"),
  N_("    lists available dictionaries / filters / filter modes"),
  N_("[options] is any of the following:")
};
static const unsigned usage_text_size = sizeof(usage_text)/sizeof(const char *);

static const char * help_text[] = 
{
  usage_text[0],
  "",
  usage_text[1],
  usage_text[2],
  usage_text[3],
  usage_text[4],
  usage_text[5],
  N_("  list             produce a list of misspelled words from standard input"),
  usage_text[6],
  usage_text[7],
  N_("  soundslike       returns the sounds like equivalent for each word entered"),
  N_("  munch            generate possible root words and affixes"),
  N_("  expand [1-4]     expands affix flags"),
  N_("  clean [strict]   cleans a word list so that every line is a valid word"),
  N_("  filter           filters input as if it was being spellchecked"),
  N_("  -v|version       prints a version line"),
  N_("  munch-list [simple] [single|multi] [keep]"),
  N_("    reduce the size of a word list via affix compression"),
  N_("  conv <from> <to> [<norm-form>]"),
  N_("    converts from one encoding to another"),
  N_("  norm (<norm-map> | <from> <norm-map> <to>) [<norm-form>]"),
  N_("    perform Unicode normalization"),
  usage_text[8],
  usage_text[9],
  N_("  dump|create|merge master|personal|repl [<name>]"),
  N_("    dumps, creates or merges a master, personal, or replacement dictionary."),
  "",
  /* TRANSLATORS: "none", "internal" and "strict" are literal values
     and should not be translated. */
  N_("  <norm-form>      normalization form to use, either none, internal, or strict"),
  "",
  usage_text[10],
  ""
};
static const unsigned help_text_size = sizeof(help_text)/sizeof(const char *);

void print_help (bool verbose) {
  load_all_filters(options);
  if (verbose) {
    printf(_("\n"
             "Aspell %s.  Copyright 2000-2023 by Kevin Atkinson.\n"
             "\n"), aspell_version_string());
    for (unsigned i = 0; i < help_text_size; ++i)
      puts(gt_(help_text[i]));
  } else {
    for (unsigned i = 0; i < usage_text_size; ++i)
      puts(gt_(usage_text[i]));
  }
  StackPtr<KeyInfoEnumeration> els(options->possible_elements(true,false));
  const KeyInfo * k;
  while (k = els->next(), k) {
    if (k->desc == 0 || k->flags & KEYINFO_HIDDEN) continue;
    if (!verbose && !(k->flags & KEYINFO_COMMON)) continue;
    const PossibleOption * o = find_option(k->name);
    const char * name = k->name;
    print_help_line(o->abrv, 
		    strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
		    name, k->type, k->desc);
    if (verbose && strcmp(name, "mode") == 0) {
      for (const ModeAbrv * j = mode_abrvs;
           j != mode_abrvs_end;
           ++j)
      {
        print_help_line(j->abrv, '\0', j->mode, KeyInfoBool, j->desc, true);
      }
    }
  }

  if (verbose) {
    //
    putchar('\n');
    putchar('\n');
    puts(
      _("Available Dictionaries:\n"
        "    Dictionaries can be selected directly via the \"-d\" or \"master\"\n"
        "    option.  They can also be selected indirectly via the \"lang\",\n"
        "    \"variety\", and \"size\" options.\n"));
    
    const DictInfoList * dlist = get_dict_info_list(options);
    
    StackPtr<DictInfoEnumeration> dels(dlist->elements());
    
    const DictInfo * entry;
    
    while ( (entry = dels->next()) != 0) 
    {
      printf("  %s\n", entry->name);
    }


    //
    putchar('\n');
    putchar('\n');
    fputs(
      _("Available Filters (and associated options):\n"
        "    Filters can be added or removed via the \"filter\" option.\n"),
      stdout);
    for (Vector<ConfigModule>::const_iterator m = options->filter_modules.begin();
         m != options->filter_modules.end();
         ++m)
    {
      printf(_("\n  %s filter: %s\n"), m->name, gt_(m->desc));
      for (k = m->begin; k != m->end; ++k) {
        const PossibleOption * o = find_option(k->name);
        const char * name = k->name;
        const KeyInfo * ok = options->keyinfo(name + 2);
        if (k == ok) name += 2;
        print_help_line(o->abrv, 
                        strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
                        name, k->type, k->desc);
      }
    }

    //
    putchar('\n');
    putchar('\n');
    puts(  
      /* TRANSLATORS: This should be formated to fit in 80 column or less */
      _("Available Filter Modes:\n"
        "    Filter Modes are reconfigured combinations of filters optimized for\n"
        "    files of a specific type. A mode is selected via the \"mode\" option.\n"
        "    This will happen implicitly if Aspell is able to identify the file\n"
        "    type from the extension, and possibility the contents, of the file.\n"));
    
    EXIT_ON_ERR_SET(available_filter_modes(options), StringPairEnumeration *, els);
    StringPair sp;
    while (!els->at_end()) {
      sp = els->next();
      printf("  %-14s %s\n", sp.first, gt_(sp.second));
    }
    delete els;
  }
}

