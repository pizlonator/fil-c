#include "settings.h"

#include "config.hpp"
#include "indiv_filter.hpp"
#include "iostream.hpp"
#include "string_map.hpp"
#include "asc_ctype.hpp"

#include <typeinfo>

//#define DEBUG_FILTER

namespace {

using namespace acommon;

struct Iterator;

struct Block {
  Block * next;
  Block() : next() {}
  enum KeepOpenState {NEVER, MAYBE, YES};
  virtual KeepOpenState proc_line(Iterator &) = 0;
  virtual void dump() const = 0;
  virtual bool leaf() const = 0;
  virtual ~Block() {}
};

struct DocRoot : Block {
  KeepOpenState proc_line(Iterator &) {return YES;}
  void dump() const {CERR.printf("DocRoot\n");}
  bool leaf() const {return false;}
};

struct MultilineInlineState;

class MarkdownFilter : public IndividualFilter {
public:
  MarkdownFilter() : root(), back(&root), prev_blank(true), inline_state() {
    name_ = "markdown-filter";
    order_num_ = 0.30; // need to be before SGML filter
  }
  PosibErr<bool> setup(Config *);
  void reset();
  ~MarkdownFilter();

  void process(FilterChar * & start, FilterChar * & stop);

private:
  void dump() {
    CERR.printf(">>>blocks\n");
    for (Block * cur = &root; cur; cur = cur->next) {
      cur->dump();
    }
    CERR.printf("<<<blocks\n");
  }

  StringMap block_start_tags;
  StringMap raw_start_tags;
  
  DocRoot root;
  Block * back;
  bool prev_blank;
  MultilineInlineState * inline_state;
 
  void kill(Block * blk) {
    Block * cur = &root;
    while (cur->next && cur->next != blk)
      cur = cur->next;
    back = cur;
    Block * next = cur->next;
    cur->next = NULL;
    cur = next;
    while (cur) {
      next = cur->next;
      delete cur;
      cur = next;
    }
  }

  void add(Block * blk) {
    back->next = blk;
    back = blk;
  }

  Block * start_block(Iterator & itr);
};

//
// Iterator class
//

inline void blank(FilterChar & chr) {
  if (!asc_isspace(chr))
    chr = ' ';
}

struct Iterator {
  FilterChar * line_start;
  FilterChar * i;
  FilterChar * end;
  int line_pos;
  int indent;
  Iterator(FilterChar * start, FilterChar * stop)
    : line_start(start), i(start), end(stop), line_pos(), indent() {}
  void * pos() {return i;}
  unsigned int operator[](int x) const {
    if (x < 0) {
      if (i + x >= line_start) return i[x];
      else return '\0';
    } else {
      if (i + x >= end) return '\0';
      if (*i == '\r' || *i == '\n') return '\0';
      else return i[x];
    }
  }
  bool prev_isspace() const {return i == line_start || asc_isspace(i[-1]);}
  bool escaped() const {return operator[](-1) == '\\';}
  unsigned int operator *() const {return operator[](0); }
  bool eol() const {return operator*() == '\0';}
  bool at_end() const {return i >= end;}
  int width() const {
    if (i == end) return 0;
    if (*i == '\t') return 4  - (line_pos % 4);
    return 1;
  }
  // u_eq = not escaped and equal
  bool u_eq(char chr) {
    return !escaped() && operator*() == chr;
  }
  bool eq(const char * str) {
    int i = 0;
    while (str[i] != '\0' && operator[](i) == str[i])
      ++i;
    return str[i] == '\0';
  }
  void inc() {
    indent = 0;
    if (eol()) return;
    line_pos += width();
    ++i;
  }
  void adv(int width = 1) {
    for (; width > 0; --width)
      inc();
    eat_space();
  }
  void blank_adv(int width = 1) {
    for (; !eol() && width > 0; --width) {
      blank(*i);
      inc();
    }
    eat_space();
  }
  void blank_rest() {
    while (!eol()) {
      blank(*i);
      inc();
    }
  }
  int eat_space();
  void next_line();
};

int Iterator::eat_space() {
  indent = 0;
  while (!eol()) {
    if (*i == ' ') {
      ++i;
      indent++;
      line_pos++;
    } else if (*i == '\t') {
      int w = width();
      ++i;
      indent += w;
      line_pos += w;
    } else {
      break;
    }
  }
  return indent;
}

void Iterator::next_line() {
  while (!eol())
    inc();
  if (!at_end() && *i == '\r') {
    ++i;
    if (!at_end() && *i == '\n') {
      ++i;
    }
  } else if (!at_end()) {
    ++i;
  }
  line_pos = 0;
  line_start = i;
}

//
// Markdown blocks
// 

struct BlockQuote : Block {
  static BlockQuote * start_block(Iterator & itr) {
    if (*itr == '>') {
      itr.blank_adv();
      return new BlockQuote();
    }
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (*itr == '>') {
      itr.blank_adv();
      return YES;
    } else if (itr.eol()) {
      return NEVER;
    }
    return MAYBE;
  }
  void dump() const {CERR.printf("BlockQuote\n");}
  bool leaf() const {return false;}
};

struct ListItem : Block {
  char marker; // '-' '+' or '*' for bullet lists; '.' or ')' for ordered lists
  int indent; // indention required in order to be considered part of
              // the same list item
  ListItem(char m, int i)
    : marker(m), indent(i) {}
  static ListItem * start_block(Iterator & itr) {
    char marker = '\0';
    int width = 0;
    if (*itr == '-' || *itr == '+' || *itr == '*') {
      marker = *itr;
      width = 1;
    } else if (asc_isdigit(*itr)) {
      width = 1;
      while (asc_isdigit(itr[width]))
        width += 1;
      if (itr[width] == '.' || itr[width] == ')') {
        width += 1;
        marker = *itr;
      }
    }
    if (marker != '\0') {
      itr.adv(width);
      if (itr.indent <= 4) {
        int indent = width + itr.indent;
        itr.indent = 0;
        return new ListItem(marker, indent);
      } else {
        int indent = 1 + itr.indent;
        itr.indent -= 1;
        return new ListItem(marker, indent);
      }
    }
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (!itr.eol() && itr.indent >= indent) {
      itr.indent -= indent;
      return YES;
    }
    return MAYBE;
  }
  void dump() const {CERR.printf("ListItem: '%c' %d\n", marker, indent);}
  bool leaf() const {return false;}
};

struct IndentedCodeBlock : Block {
  static IndentedCodeBlock * start_block(bool prev_blank, Iterator & itr) {
    if (prev_blank && !itr.eol() && itr.indent >= 4) {
      itr.blank_rest();
      return new IndentedCodeBlock();
    }
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (itr.indent >= 4) {
      itr.blank_rest();
      return YES;
    } else if (itr.eol()) {
      return YES;
    }
    return NEVER;
  }
  void dump() const {CERR.printf("IndentedCodeBlock\n");}
  bool leaf() const {return true;}
};

struct FencedCodeBlock : Block {
  char delem;
  int  delem_len;
  FencedCodeBlock(char d, int l) : delem(d), delem_len(l) {}
  static FencedCodeBlock * start_block(Iterator & itr) {
    if (*itr == '`' || *itr == '~') {
      char delem = *itr;
      int i = 1;
      while (itr[i] == delem)
        ++i;
      if (i < 3) return NULL;
      itr.blank_adv(i);
      itr.blank_rest(); // blank info string
      return new FencedCodeBlock(delem, i);
    }
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (*itr == '`' || *itr == '~') {
      char delem = *itr;
      int i = 1;
      while (itr[i] == delem)
        ++i;
      itr.blank_adv(i);
      if (i >= delem_len && itr.eol()) {
        return NEVER;
      }
    }
    itr.blank_rest();
    return YES;
  }
  bool blank_rest() const {
    return true;
  }
  void dump() const {CERR.printf("FencedCodeBlock: `%c` %d\n", delem, delem_len);}
  bool leaf() const {return true;}
};

struct SingleLineBlock : Block {
  static SingleLineBlock * start_block(Iterator & itr) {
    unsigned int chr = *itr;
    switch (chr) {
    case '-': case '_': case '*': {
      Iterator i = itr;
      i.adv();
      while (*i == *itr)
        i.adv();
      if (i.eol()) {
        itr = i; 
        return new SingleLineBlock();
      }
      if (chr != '-') // fall though on '-' case
        break;
    }
    case '=': {
      Iterator i = itr;
      i.inc();
      while (*i == *itr)
        i.inc();
      i.eat_space();
      if (i.eol()) {
        itr = i;
        return new SingleLineBlock();
      }
      break;
    }
    case '#':
      return new SingleLineBlock();
      break;

    }
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {return NEVER;}
  bool leaf() const {return true;}
  void dump() const {CERR.printf("SingleLineBlock\n");}
};

//
// MultilineInline 
// 

struct MultilineInline {
  virtual MultilineInline * close(Iterator & itr) = 0;
  virtual ~MultilineInline() {}
};

struct InlineCode : MultilineInline {
  int marker_len;
  MultilineInline * open(Iterator & itr) {
    if (itr.u_eq('`')) {
      int i = 1;
      while (itr[i] == '`')
        ++i;
      itr.blank_adv(i);
      marker_len = i;
      return close(itr);
    }
    return NULL;
  }
  MultilineInline * close(Iterator & itr) {
    while (!itr.eol()) {
      if (*itr == '`') {
        int i = 1;
        while (i < marker_len && itr[i] == '`')
          ++i;
        if (i == marker_len) {
          itr.blank_adv(i);
          return NULL;
        }
      }
      itr.blank_adv();
    }
    return this;
  }
};

//
// Html handling
//

struct HtmlComment : MultilineInline {
  MultilineInline * open(Iterator & itr) {
    if (itr.eq("<!--")) {
      itr.adv(4);
      return close(itr);
    }
    return NULL;
  }
  MultilineInline * close(Iterator & itr) {
    while (!itr.eol()) {
      if (itr.eq("-->")) {
        itr.adv(3);
        return NULL;
      }
      itr.inc();
    }
    return this;
  }
};

bool parse_tag_close(Iterator & itr) {
  if (*itr == '>') {
    itr.adv();
    return true;
  } else if (*itr == '/' && itr[1] == '>') {
    itr.adv(2);
    return true;
  }
  return false;
}

// note: does _not_ eat trialing whitespaceb
bool parse_tag_name(Iterator & itr, String & tag) {
  if (asc_isalpha(*itr)) {
    tag += asc_tolower(*itr);
    itr.inc();
    while (asc_isalpha(*itr) || asc_isdigit(*itr) || *itr == '-') {
      tag += asc_tolower(*itr);
      itr.inc();
    }
    return true;
  }
  return false;
}

struct HtmlTag : MultilineInline {
  HtmlTag(bool mlt) : start_pos(NULL), last(NULL,NULL), multi_line(mlt) {}
  void * start_pos; // used for caching
  Iterator last;    // ditto
  String name;
  bool closing;
  enum State {Invalid,Between,AfterName,AfterEq,InSingleQ,InDoubleQ,BeforeClose,Valid};
  State state;
  bool multi_line;
  void clear_cache() {
    start_pos = NULL;
  }
  void reset() {
    clear_cache();
    name.clear();
    closing = false;
    state = Invalid;
  }
  MultilineInline * open(const Iterator & itr0, Iterator & itr) {
    if (itr.pos() == start_pos) {
      itr = last;
      if (state != Invalid && state != Valid)
        return this;
      return NULL;
    }
    reset();
    start_pos = itr.pos();
    if (*itr == '<') {
      itr.inc();
      if (*itr == '/') {
        itr.inc();
        closing = true;
      }
      if (!parse_tag_name(itr, name))
        return invalid(itr0, itr);
      state = Between;
      if (itr.eol()) {
        return incomplete(itr0, itr);
      } else if (parse_tag_close(itr)) {
        return valid(itr0, itr);
      } else if (asc_isspace(*itr)) {
        return close(itr0, itr);
      } else {
        return invalid(itr0, itr);
      }
    }
    return invalid(itr0, itr);
  }
  MultilineInline * open(Iterator & itr) {
    Iterator itr0 = itr;
    return open(itr0, itr);
  }
  MultilineInline * close(const Iterator & itr0, Iterator & itr) {
    while (!itr.eol()) {
      if (state == Between || state == BeforeClose) {
        itr.eat_space();
        bool leading_space = itr.prev_isspace();
        
        if (parse_tag_close(itr))
          return valid(itr0, itr);

        if ((state == BeforeClose && !itr.eol())
            || (itr.line_pos != 0 && !leading_space))
          return invalid(itr0, itr);
      }

      state = parse_attribute(itr, state);
      if (state == Invalid)
        return invalid(itr0, itr);
    }
    return incomplete(itr0, itr);
  }
  MultilineInline * close(Iterator & itr) {
    Iterator itr0 = itr;
    return close(itr0, itr);
  }

  MultilineInline * valid(const Iterator & itr0, Iterator & itr) {
    state = Valid;
    last = itr;
    return NULL;
  }
  MultilineInline * invalid(const Iterator & itr0, Iterator & itr) {
    state = Invalid;
    itr = itr0;
    last = itr;
    return NULL;
  }
  MultilineInline * incomplete(const Iterator & itr0, Iterator & itr) {
    last = itr;
    if (multi_line)
      return this;
    return invalid(itr0, itr);
  }

  // note: does _not_ eat trialing whitespace
  static State parse_attribute(Iterator & itr, State state) {
    switch (state) {
      // note: this switch is being used as a computed goto to make
      //   restoring state straightforward without restructuring the code
     case Between:
      if (asc_isalpha(*itr) || *itr == '_' || *itr == ':') {
        itr.inc();
        while (asc_isalpha(*itr) || asc_isdigit(*itr)
               || *itr == '_' || *itr == ':' || *itr == '.' || *itr == '-')
          itr.inc();
       case AfterName:
        itr.eat_space();
        if (itr.eol()) return AfterName;
        if (*itr != '=') return Invalid;
        itr.inc();
       case AfterEq:
        itr.eat_space();
        if (itr.eol()) return AfterEq;
        if (*itr == '\'') {
          itr.inc();
         case InSingleQ:
          while (!itr.eol() && *itr != '\'')
            itr.inc();
          if (itr.eol()) return InSingleQ;
          if (*itr != '\'') return Invalid;
          itr.inc();
        } else if (*itr == '"') {
          itr.inc();
         case InDoubleQ:
          while (!itr.eol() && *itr != '"')
            itr.inc();
          if (itr.eol()) return InDoubleQ;
          if (*itr != '"') return Invalid;
          itr.inc();
        } else {
          void * pos = itr.pos();
          while (!itr.eol() && !asc_isspace(*itr)
                 && *itr != '"' && *itr != '\'' && *itr != '='
                 && *itr != '<' && *itr != '>' && *itr != '`')
            itr.inc();
          if (pos == itr.pos()) return Invalid;
        }
        return Between;
      }
     case BeforeClose:
      return BeforeClose;
     default: //case Valid: case Invalid:
      // should not happen
      break;
    }
    // should not be here
    abort();
  }
};

struct HtmlBlock : Block {
  HtmlBlock(Iterator & itr) {
    proc_line(itr);
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (itr.eol()) return NEVER;
    while (!itr.eol()) itr.inc();
    return YES;
  }
  void dump() const {CERR.printf("HtmlBlock\n");}
  bool leaf() const {return true;}
};

struct RawHtmlBlock : Block {
  RawHtmlBlock(Iterator & itr, ParmStr tn) : done(false), tag(false), tag_name(tn) {
    proc_line(itr);
  }
  bool done;
  HtmlTag tag;
  String tag_name;
  KeepOpenState proc_line(Iterator & itr) {
    tag.reset();
    if (done) return NEVER;
    while (!itr.eol()) {
      tag.open(itr);
      if (tag.state == HtmlTag::Valid && tag.closing && tag.name == tag_name) {
        done = true;
        while (!itr.eol()) itr.inc();
        return NEVER;
      }
      itr.adv();
    }
    return YES;
  }
  void dump() const {CERR.printf("RawHtmlBlock: %s\n", tag_name.c_str());}
  bool leaf() const {return true;}
};

Block * start_html_block(Iterator & itr, HtmlTag & tag,
                         const StringMap & start_tags,
                         const StringMap & raw_tags) {
  Iterator itr0 = itr;
  tag.open(itr0, itr);
  if (!tag.closing && raw_tags.have(tag.name))
    return new RawHtmlBlock(itr,tag.name);
  if ((tag.state == HtmlTag::Valid && itr.eol())
      || start_tags.have(tag.name)) {
    return new HtmlBlock(itr);
  }
  itr = itr0;
  return NULL;
}

//
// Link handling
// 

struct Link : MultilineInline {
  Link(bool s) : skip_ref_labels(s) {reset();}
  enum State {Invalid, BeforeUrl, AfterUrl, InSingleQ, InDoubleQ, InParanQ, AfterQuote, Valid};
  State state;
  bool skip_ref_labels;
  struct LineState {
    Iterator itr0;
    FilterChar * blank_start;
    FilterChar * blank_stop;
    LineState(const Iterator & itr0)
      : itr0(itr0), blank_start(NULL), blank_stop(NULL) {}
  };
  void reset() {
    state = Invalid;
  }
  MultilineInline * open(Iterator & itr) {
    reset();
    if (itr.u_eq(']')) {
      // no space allowed between ']' and '(' or '[';
      if (itr[1] == '(') {
        itr.adv(2);
        return close(itr);
      } else if (skip_ref_labels && itr[1] == '[') {
        LineState st(itr);
        itr.adv(2);
        st.blank_start = itr.i;
        while (!itr.eol() && !itr.u_eq(']'))
          itr.adv();
        st.blank_stop = itr.i;
        if (!itr.eol())
          return valid(st,itr);
        else
          return invalid(st, itr);
      }
    }
    state = Invalid;
    return NULL;
  }
  
  static State parse_url(LineState & st, Iterator & itr, char close) {
    if (itr.eol())
      return BeforeUrl;
    if (itr.u_eq('<')) {
      st.blank_start = itr.i;
      itr.adv();
      while (!itr.eol() && !itr.u_eq('>'))
        itr.adv();
      if (itr.eol())
        return Invalid;
      itr.adv();
      st.blank_stop = itr.i;
    } else {
      st.blank_start = itr.i;
      while (!itr.eol() && !itr.u_eq(close) && !asc_isspace(*itr))
        itr.inc();
      st.blank_stop = itr.i;
      itr.eat_space();
    }
    return AfterUrl;
  }
  static State parse_label(State state, Iterator & itr) {
    switch (state) {
    default:
      if (itr.u_eq('\'')) {
        itr.inc();
        state = InSingleQ;
      case InSingleQ:
        while (!itr.eol() && !itr.u_eq('\''))
          itr.inc();
        if (itr.eol())
          return state;
        itr.adv();
        state = AfterQuote;
      } else if (itr.u_eq('\"')) {
        itr.inc();
        state = InDoubleQ;
      case InDoubleQ:
        while (!itr.eol() && !itr.u_eq('"'))
          itr.inc();
        if (itr.eol())
          return state;
        itr.adv();
        state = AfterQuote;
      } else if (itr.u_eq('(')) {
        state = InParanQ;
      case InParanQ:
        while (!itr.eol() && !itr.u_eq(')'))
          itr.inc();
        if (itr.eol())
          return state;
        itr.adv();
        state = AfterQuote;
      }
    }
    return state;
  }
  Link * parse_url_label(Iterator & itr, char close) {
    LineState st(itr);
    itr.eat_space();
    switch (state) {
    default:
      state = parse_url(st,itr,close);
      if (state == Invalid) return invalid(st, itr);
      if (itr.eol()) return incomplete(st, itr);
    case AfterUrl:
      if (close != '\0' ? itr.u_eq(close) : itr.eol())
        return valid(st, itr);
    case InSingleQ: case InDoubleQ:
      state = parse_label(state, itr);
      if (state == Invalid) return invalid(st, itr);
      if (itr.eol()) return incomplete(st, itr);
    case AfterQuote:
      if (close != '\0' ? itr.u_eq(close) : itr.eol())
        return valid(st, itr);
      return invalid(st, itr);
    }    
  }
  MultilineInline * close(Iterator & itr) {
    return parse_url_label(itr, ')');
  }
  static void blank(const LineState & st) {
    for (FilterChar * i = st.blank_start; i != st.blank_stop; ++i) {
      ::blank(*i);
    }
  }
  Link * valid(const LineState & st, Iterator & itr) {
    itr.adv(); // skip over closing tag
    blank(st);
    state = Valid;
    return NULL;
  }
  Link * invalid(const LineState & st, Iterator & itr) {
    state = Invalid;
    itr = st.itr0;
    return NULL;
  }
  Link * incomplete(const LineState & st, Iterator & itr) {
    blank(st);
    return this;
  }
};

struct LinkRefDefinition : Block {
  Link link;
  Link * multiline;
  LinkRefDefinition() : link(false) {}
  static LinkRefDefinition * start_block(Iterator & itr, bool skip_ref_labels) {
    Link::LineState st(itr);
    if (*itr == '[') {
      itr.adv();
      st.blank_start = itr.i;
      if (*itr == ']') goto fail;
      while (!itr.eol() && !itr.u_eq(']')) {
        itr.adv();
      }
      st.blank_stop = itr.i;
      itr.inc();
      if (*itr != ':') goto fail;
      itr.adv();
      LinkRefDefinition * obj = new LinkRefDefinition();
      obj->multiline = obj->link.parse_url_label(itr, '\0');
      if (obj->link.state == Link::Invalid) {
        delete obj;
        goto fail;
      }
      if (skip_ref_labels)
        Link::blank(st);
      return obj;
    }
  fail:
    itr = st.itr0;
    return NULL;
  }
  KeepOpenState proc_line(Iterator & itr) {
    if (!multiline)
      return NEVER;
    void * pos = itr.pos();
    multiline = multiline->parse_url_label(itr, '\0');
    if (multiline)
      return MAYBE;
    return NEVER;
  }
  void dump() const {CERR.printf("LinkRefDefination\n");}
  bool leaf() const {return true;}
};

//
//
//

struct MultilineInlineState {
  MultilineInlineState(bool mlt, bool s) : ptr(), tag(mlt), link(s) {}
  MultilineInline * ptr;
  InlineCode inline_code;
  HtmlComment comment;
  HtmlTag tag;
  Link link;
  void clear_cache() {
    tag.clear_cache();
  }
  void reset() {
    tag.reset();
    link.reset();
  }
};

//
// MarkdownFilter implementation
//

PosibErr<bool> MarkdownFilter::setup(Config * cfg) {
  bool skip_ref_labels = cfg->retrieve_bool("f-markdown-skip-ref-labels");
  bool multiline_tags = cfg->retrieve_bool("f-markdown-multiline-tags");
  delete inline_state;
  inline_state = new MultilineInlineState(multiline_tags, skip_ref_labels);
  raw_start_tags.clear();
  cfg->retrieve_list("f-markdown-raw-start-tags",  &raw_start_tags);
  block_start_tags.clear();
  cfg->retrieve_list("f-markdown-block-start-tags", &block_start_tags);

  return true;
}

void MarkdownFilter::reset() {
  kill(root.next);
  prev_blank = true;
  inline_state->reset();
}


MarkdownFilter::~MarkdownFilter() {
  kill(root.next);
  delete inline_state;
}

void MarkdownFilter::process(FilterChar * & start, FilterChar * & stop) {
  inline_state->clear_cache();
  Iterator itr(start,stop);
  bool blank_line = false;
  while (!itr.at_end()) { 
    itr.eat_space();
    if (inline_state->ptr) {
      if (itr.eol())
        inline_state->ptr = NULL;
      else
        inline_state->ptr = inline_state->ptr->close(itr);
    } else {
      Block * blk = &root;
      Block::KeepOpenState keep_open;
      for (; blk; blk = blk->next) {
        keep_open = blk->proc_line(itr);
        if (keep_open != Block::YES)
          break;
      }

      blank_line = itr.eol();
      Block * nblk = blank_line || (keep_open == Block::YES && back->leaf())
        ? NULL
        : start_block(itr);
      
      if (nblk || keep_open == Block::NEVER || (prev_blank && !blank_line)) {
#ifdef DEBUG_FILTER
        CERR.printf("*** kill\n");
#endif
        kill(blk);
      } else {
        for (; blk; blk = blk->next) {
          keep_open = blk->proc_line(itr);
          if (keep_open == Block::NEVER) {
#ifdef DEBUG_FILTER
            CERR.printf("***** kill\n");
#endif          
            kill(blk);
            break;
          }
        }
      }

      if (nblk) {
#ifdef DEBUG_FILTER
        CERR.printf("*** new block\n");
#endif
        add(nblk);
        prev_blank = true;
      }

      while (nblk && !nblk->leaf()) {
        nblk = start_block(itr);
        if (nblk) {
#ifdef DEBUG_FILTER
          CERR.printf("*** new block\n");
#endif         
          add(nblk);
        }
      }

#ifdef DEBUG_FILTER
      dump();
#endif
    }
    // now process line, mainly blank inline code and handle html tags
      
    while (!itr.eol()) {
      void * pos = itr.pos();
#define TRY(what) \
  inline_state->ptr = inline_state->what.open(itr);  \
  if (inline_state->ptr) break; \
  if (itr.pos() != pos) continue
      TRY(inline_code);
      TRY(comment);
      TRY(tag);
      TRY(link);
#undef TRY
      if (*itr == '<' || *itr == '>')
        itr.blank_adv();
      else
        itr.adv();
    }

    itr.next_line();

    prev_blank = blank_line;
  }
}

Block * MarkdownFilter::start_block(Iterator & itr) {
  inline_state->tag.reset();
  Block * nblk = NULL;
  (nblk = IndentedCodeBlock::start_block(prev_blank, itr))
    || (nblk = FencedCodeBlock::start_block(itr))
    || (nblk = BlockQuote::start_block(itr))
    || (nblk = ListItem::start_block(itr))
    || (nblk = LinkRefDefinition::start_block(itr, inline_state->link.skip_ref_labels))
    || (nblk = SingleLineBlock::start_block(itr))
    || (nblk = start_html_block(itr, inline_state->tag, block_start_tags, raw_start_tags));
  return nblk;
}

} // anon namespace

C_EXPORT IndividualFilter * new_aspell_markdown_filter() 
{
  return new MarkdownFilter();
}

