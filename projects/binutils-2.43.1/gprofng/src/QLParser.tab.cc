// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

// "%code top" blocks.
#line 28 "QLParser.yy"

#include <stdio.h>
#include <string.h>
#include <string>

#line 45 "QLParser.tab.cc"




#include "QLParser.tab.hh"


// Unqualified %code blocks.
#line 42 "QLParser.yy"

namespace QL
{
  static QL::Parser::symbol_type yylex (QL::Result &result);

  static Expression *
  processName (std::string str)
  {
    const char *name = str.c_str();
    int propID = dbeSession->getPropIdByName (name);
    if (propID != PROP_NONE)
      return new Expression (Expression::OP_NAME,
		      new Expression (Expression::OP_NUM, (uint64_t) propID));

    // If a name is not statically known try user defined objects
    Expression *expr = dbeSession->findObjDefByName (name);
    if (expr != NULL)
      return expr->copy();

    throw Parser::syntax_error ("Name not found");
  }
}

#line 78 "QLParser.tab.cc"


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif



// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 67 "QLParser.yy"
namespace QL {
#line 152 "QLParser.tab.cc"

  /// Build a parser object.
  Parser::Parser (QL::Result &result_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      result (result_yyarg)
  {}

  Parser::~Parser ()
  {}

  Parser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  Parser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  Parser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  Parser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  Parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  Parser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  Parser::symbol_kind_type
  Parser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  Parser::stack_symbol_type::stack_symbol_type ()
  {}

  Parser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_exp: // exp
      case symbol_kind::S_term: // term
        value.YY_MOVE_OR_COPY< Expression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_NAME: // NAME
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_NUM: // NUM
      case symbol_kind::S_FNAME: // FNAME
      case symbol_kind::S_JGROUP: // JGROUP
      case symbol_kind::S_JPARENT: // JPARENT
      case symbol_kind::S_QSTR: // QSTR
        value.YY_MOVE_OR_COPY< uint64_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  Parser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s)
  {
    switch (that.kind ())
    {
      case symbol_kind::S_exp: // exp
      case symbol_kind::S_term: // term
        value.move< Expression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_NAME: // NAME
        value.move< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_NUM: // NUM
      case symbol_kind::S_FNAME: // FNAME
      case symbol_kind::S_JGROUP: // JGROUP
      case symbol_kind::S_JPARENT: // JPARENT
      case symbol_kind::S_QSTR: // QSTR
        value.move< uint64_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  Parser::stack_symbol_type&
  Parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_exp: // exp
      case symbol_kind::S_term: // term
        value.copy< Expression * > (that.value);
        break;

      case symbol_kind::S_NAME: // NAME
        value.copy< std::string > (that.value);
        break;

      case symbol_kind::S_NUM: // NUM
      case symbol_kind::S_FNAME: // FNAME
      case symbol_kind::S_JGROUP: // JGROUP
      case symbol_kind::S_JPARENT: // JPARENT
      case symbol_kind::S_QSTR: // QSTR
        value.copy< uint64_t > (that.value);
        break;

      default:
        break;
    }

    return *this;
  }

  Parser::stack_symbol_type&
  Parser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_exp: // exp
      case symbol_kind::S_term: // term
        value.move< Expression * > (that.value);
        break;

      case symbol_kind::S_NAME: // NAME
        value.move< std::string > (that.value);
        break;

      case symbol_kind::S_NUM: // NUM
      case symbol_kind::S_FNAME: // FNAME
      case symbol_kind::S_JGROUP: // JGROUP
      case symbol_kind::S_JPARENT: // JPARENT
      case symbol_kind::S_QSTR: // QSTR
        value.move< uint64_t > (that.value);
        break;

      default:
        break;
    }

    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  Parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  Parser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " (";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  Parser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  Parser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  Parser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  Parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  Parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  Parser::debug_level_type
  Parser::debug_level () const
  {
    return yydebug_;
  }

  void
  Parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  Parser::state_type
  Parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  Parser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  Parser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  Parser::operator() ()
  {
    return parse ();
  }

  int
  Parser::parse ()
  {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            symbol_type yylookahead (yylex (result));
            yyla.move (yylookahead);
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        goto yydefault;
      }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_exp: // exp
      case symbol_kind::S_term: // term
        yylhs.value.emplace< Expression * > ();
        break;

      case symbol_kind::S_NAME: // NAME
        yylhs.value.emplace< std::string > ();
        break;

      case symbol_kind::S_NUM: // NUM
      case symbol_kind::S_FNAME: // FNAME
      case symbol_kind::S_JGROUP: // JGROUP
      case symbol_kind::S_JPARENT: // JPARENT
      case symbol_kind::S_QSTR: // QSTR
        yylhs.value.emplace< uint64_t > ();
        break;

      default:
        break;
    }



      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 2: // S: %empty
#line 120 "QLParser.yy"
                                { result.out = new Expression (Expression::OP_NUM, (uint64_t) 1); }
#line 619 "QLParser.tab.cc"
    break;

  case 3: // S: exp
#line 121 "QLParser.yy"
                                { result.out = yystack_[0].value.as < Expression * > (); }
#line 625 "QLParser.tab.cc"
    break;

  case 4: // exp: exp DEG exp
#line 123 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_DEG, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 631 "QLParser.tab.cc"
    break;

  case 5: // exp: exp MUL exp
#line 124 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_MUL, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 637 "QLParser.tab.cc"
    break;

  case 6: // exp: exp DIV exp
#line 125 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_DIV, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 643 "QLParser.tab.cc"
    break;

  case 7: // exp: exp REM exp
#line 126 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_REM, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 649 "QLParser.tab.cc"
    break;

  case 8: // exp: exp ADD exp
#line 127 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_ADD, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 655 "QLParser.tab.cc"
    break;

  case 9: // exp: exp MINUS exp
#line 128 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_MINUS, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 661 "QLParser.tab.cc"
    break;

  case 10: // exp: exp LS exp
#line 129 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_LS, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 667 "QLParser.tab.cc"
    break;

  case 11: // exp: exp RS exp
#line 130 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_RS, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 673 "QLParser.tab.cc"
    break;

  case 12: // exp: exp LT exp
#line 131 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_LT, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 679 "QLParser.tab.cc"
    break;

  case 13: // exp: exp LE exp
#line 132 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_LE, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 685 "QLParser.tab.cc"
    break;

  case 14: // exp: exp GT exp
#line 133 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_GT, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 691 "QLParser.tab.cc"
    break;

  case 15: // exp: exp GE exp
#line 134 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_GE, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 697 "QLParser.tab.cc"
    break;

  case 16: // exp: exp EQ exp
#line 135 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_EQ, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 703 "QLParser.tab.cc"
    break;

  case 17: // exp: exp NE exp
#line 136 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_NE, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 709 "QLParser.tab.cc"
    break;

  case 18: // exp: exp BITAND exp
#line 137 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_BITAND, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 715 "QLParser.tab.cc"
    break;

  case 19: // exp: exp BITXOR exp
#line 138 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_BITXOR, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 721 "QLParser.tab.cc"
    break;

  case 20: // exp: exp BITOR exp
#line 139 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_BITOR, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 727 "QLParser.tab.cc"
    break;

  case 21: // exp: exp AND exp
#line 140 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_AND, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 733 "QLParser.tab.cc"
    break;

  case 22: // exp: exp OR exp
#line 141 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_OR, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 739 "QLParser.tab.cc"
    break;

  case 23: // exp: exp NEQV exp
#line 142 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_NEQV, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 745 "QLParser.tab.cc"
    break;

  case 24: // exp: exp EQV exp
#line 143 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_EQV, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 751 "QLParser.tab.cc"
    break;

  case 25: // exp: exp QWE exp COLON exp
#line 145 "QLParser.yy"
          {
	     yylhs.value.as < Expression * > () = new Expression (Expression::OP_QWE, yystack_[4].value.as < Expression * > (),
				  new Expression (Expression::OP_COLON, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()));
	  }
#line 760 "QLParser.tab.cc"
    break;

  case 26: // exp: exp COMMA exp
#line 149 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_COMMA, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 766 "QLParser.tab.cc"
    break;

  case 27: // exp: exp IN exp
#line 150 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_IN, yystack_[2].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 772 "QLParser.tab.cc"
    break;

  case 28: // exp: exp SOME IN exp
#line 151 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_SOMEIN, yystack_[3].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 778 "QLParser.tab.cc"
    break;

  case 29: // exp: exp ORDR IN exp
#line 152 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_ORDRIN, yystack_[3].value.as < Expression * > (), yystack_[0].value.as < Expression * > ()); }
#line 784 "QLParser.tab.cc"
    break;

  case 30: // exp: term
#line 153 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = yystack_[0].value.as < Expression * > (); }
#line 790 "QLParser.tab.cc"
    break;

  case 31: // term: MINUS term
#line 156 "QLParser.yy"
          {
	     yylhs.value.as < Expression * > () = new Expression (Expression::OP_MINUS,
				  new Expression (Expression::OP_NUM, (uint64_t) 0), yystack_[0].value.as < Expression * > ());
	  }
#line 799 "QLParser.tab.cc"
    break;

  case 32: // term: NOT term
#line 160 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_NOT, yystack_[0].value.as < Expression * > ()); }
#line 805 "QLParser.tab.cc"
    break;

  case 33: // term: BITNOT term
#line 161 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_BITNOT, yystack_[0].value.as < Expression * > ()); }
#line 811 "QLParser.tab.cc"
    break;

  case 34: // term: "(" exp ")"
#line 162 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = yystack_[1].value.as < Expression * > (); }
#line 817 "QLParser.tab.cc"
    break;

  case 35: // term: FNAME "(" QSTR ")"
#line 164 "QLParser.yy"
          {
	    yylhs.value.as < Expression * > () = new Expression (Expression::OP_FUNC,
				 new Expression (Expression::OP_NUM, yystack_[3].value.as < uint64_t > ()),
				 new Expression (Expression::OP_NUM, yystack_[1].value.as < uint64_t > ()));
	  }
#line 827 "QLParser.tab.cc"
    break;

  case 36: // term: HASPROP "(" NAME ")"
#line 170 "QLParser.yy"
          {
	    yylhs.value.as < Expression * > () = new Expression (Expression::OP_HASPROP,
				 new Expression (Expression::OP_NUM, processName(yystack_[1].value.as < std::string > ())));
	  }
#line 836 "QLParser.tab.cc"
    break;

  case 37: // term: JGROUP "(" QSTR ")"
#line 175 "QLParser.yy"
          {
	    yylhs.value.as < Expression * > () = new Expression (Expression::OP_JAVA,
				 new Expression (Expression::OP_NUM, yystack_[3].value.as < uint64_t > ()),
				 new Expression (Expression::OP_NUM, yystack_[1].value.as < uint64_t > ()));
	  }
#line 846 "QLParser.tab.cc"
    break;

  case 38: // term: JPARENT "(" QSTR ")"
#line 181 "QLParser.yy"
          {
	     yylhs.value.as < Expression * > () = new Expression (Expression::OP_JAVA,
				  new Expression (Expression::OP_NUM, yystack_[3].value.as < uint64_t > ()),
				  new Expression (Expression::OP_NUM, yystack_[1].value.as < uint64_t > ()));
	  }
#line 856 "QLParser.tab.cc"
    break;

  case 39: // term: FILEIOVFD "(" QSTR ")"
#line 187 "QLParser.yy"
          {
	    yylhs.value.as < Expression * > () = new Expression (Expression::OP_FILE,
				 new Expression (Expression::OP_NUM, (uint64_t) 0),
				 new Expression (Expression::OP_NUM, yystack_[1].value.as < uint64_t > ()));
	  }
#line 866 "QLParser.tab.cc"
    break;

  case 40: // term: NUM
#line 192 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = new Expression (Expression::OP_NUM, yystack_[0].value.as < uint64_t > ()); }
#line 872 "QLParser.tab.cc"
    break;

  case 41: // term: NAME
#line 193 "QLParser.yy"
                                { yylhs.value.as < Expression * > () = processName(yystack_[0].value.as < std::string > ()); }
#line 878 "QLParser.tab.cc"
    break;


#line 882 "QLParser.tab.cc"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        std::string msg = YY_("syntax error");
        error (YY_MOVE (msg));
      }


    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
              {
                yyn = yytable_[yyn];
                if (0 < yyn)
                  break;
              }
          }

        // Pop the current state because it cannot handle the error token.
        if (yystack_.size () == 1)
          YYABORT;

        yy_destroy_ ("Error: popping", yystack_[0]);
        yypop_ ();
        YY_STACK_PRINT ();
      }
    {
      stack_symbol_type error_token;


      // Shift the error token.
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  Parser::error (const syntax_error& yyexc)
  {
    error (yyexc.what ());
  }

#if YYDEBUG || 0
  const char *
  Parser::symbol_name (symbol_kind_type yysymbol)
  {
    return yytname_[yysymbol];
  }
#endif // #if YYDEBUG || 0









  const signed char Parser::yypact_ninf_ = -3;

  const signed char Parser::yytable_ninf_ = -1;

  const short
  Parser::yypact_[] =
  {
       0,     0,    -2,     1,    -3,     8,    13,    14,    -3,     0,
       0,     0,     2,   142,    -3,    50,     6,     9,    10,    11,
      12,    -3,    -3,    -3,    -3,     0,    38,    39,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    -3,    15,    21,    22,    49,    51,   188,     0,     0,
     221,    96,    95,    95,    95,    95,    95,    95,    95,   141,
     141,   141,   141,   141,   141,    17,    17,    17,    17,    17,
      17,    17,    17,    -3,    -3,    -3,    -3,    -3,   188,   188,
       0,   221
  };

  const signed char
  Parser::yydefact_[] =
  {
       2,     0,     0,     0,    40,     0,     0,     0,    41,     0,
       0,     0,     0,     3,    30,     0,     0,     0,     0,     0,
       0,    31,    32,    33,     1,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    34,     0,     0,     0,     0,     0,    27,     0,     0,
      26,     0,    21,    22,    24,    23,    18,    20,    19,    16,
      17,    12,    14,    13,    15,    10,    11,     8,     9,     5,
       6,     7,     4,    36,    39,    35,    37,    38,    28,    29,
       0,    25
  };

  const signed char
  Parser::yypgoto_[] =
  {
      -3,    -3,    -1,     4
  };

  const signed char
  Parser::yydefgoto_[] =
  {
       0,    12,    13,    14
  };

  const signed char
  Parser::yytable_[] =
  {
      15,    16,    24,     1,    17,     2,     3,     4,     5,     6,
       7,    18,     8,    21,    22,    23,    19,    20,    52,    83,
      53,    54,    55,    56,    57,    84,    85,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
       9,    58,    59,    86,    51,    87,     0,    88,    89,    10,
       0,    11,     0,    25,    26,    27,    28,     0,    29,     0,
       0,     0,    30,     0,    31,    50,    32,    33,    34,    35,
      36,     0,    37,     0,    38,     0,    39,     0,    40,    91,
      41,     0,    42,     0,    43,     0,    44,     0,    45,     0,
      46,     0,    47,     0,    48,     0,    49,     0,    50,    25,
      26,    27,    28,     0,    29,     0,    90,     0,    30,     0,
      31,     0,    32,    33,    34,    35,    36,    37,    37,    38,
      38,    39,    39,    40,    40,    41,    41,    42,    42,    43,
      43,    44,    44,    45,    45,    46,    46,    47,    47,    48,
      48,    49,    49,    50,    50,    25,    26,    27,    28,     0,
      29,     0,     0,     0,    30,     0,    31,     0,    32,    33,
      34,    35,    36,    -1,    37,    -1,    38,    -1,    39,    -1,
      40,    -1,    41,    -1,    42,    43,    43,    44,    44,    45,
      45,    46,    46,    47,    47,    48,    48,    49,    49,    50,
      50,    -1,    -1,    -1,    28,     0,    29,     0,     0,     0,
      30,     0,    31,     0,    32,    33,    34,    35,    36,     0,
      37,     0,    38,     0,    39,     0,    40,     0,    41,     0,
      42,     0,    43,     0,    44,     0,    45,     0,    46,    29,
      47,     0,    48,    30,    49,    31,    50,    32,    33,    34,
      35,    36,     0,    37,     0,    38,     0,    39,     0,    40,
       0,    41,     0,    42,     0,    43,     0,    44,     0,    45,
       0,    46,     0,    47,     0,    48,     0,    49,     0,    50
  };

  const signed char
  Parser::yycheck_[] =
  {
       1,     3,     0,     3,     3,     5,     6,     7,     8,     9,
      10,     3,    12,     9,    10,    11,     3,     3,    12,     4,
      11,    11,    11,    11,    25,     4,     4,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      50,    13,    13,     4,     4,     4,    -1,    58,    59,    59,
      -1,    61,    -1,    13,    14,    15,    16,    -1,    18,    -1,
      -1,    -1,    22,    -1,    24,    58,    26,    27,    28,    29,
      30,    -1,    32,    -1,    34,    -1,    36,    -1,    38,    90,
      40,    -1,    42,    -1,    44,    -1,    46,    -1,    48,    -1,
      50,    -1,    52,    -1,    54,    -1,    56,    -1,    58,    13,
      14,    15,    16,    -1,    18,    -1,    20,    -1,    22,    -1,
      24,    -1,    26,    27,    28,    29,    30,    32,    32,    34,
      34,    36,    36,    38,    38,    40,    40,    42,    42,    44,
      44,    46,    46,    48,    48,    50,    50,    52,    52,    54,
      54,    56,    56,    58,    58,    13,    14,    15,    16,    -1,
      18,    -1,    -1,    -1,    22,    -1,    24,    -1,    26,    27,
      28,    29,    30,    32,    32,    34,    34,    36,    36,    38,
      38,    40,    40,    42,    42,    44,    44,    46,    46,    48,
      48,    50,    50,    52,    52,    54,    54,    56,    56,    58,
      58,    13,    14,    15,    16,    -1,    18,    -1,    -1,    -1,
      22,    -1,    24,    -1,    26,    27,    28,    29,    30,    -1,
      32,    -1,    34,    -1,    36,    -1,    38,    -1,    40,    -1,
      42,    -1,    44,    -1,    46,    -1,    48,    -1,    50,    18,
      52,    -1,    54,    22,    56,    24,    58,    26,    27,    28,
      29,    30,    -1,    32,    -1,    34,    -1,    36,    -1,    38,
      -1,    40,    -1,    42,    -1,    44,    -1,    46,    -1,    48,
      -1,    50,    -1,    52,    -1,    54,    -1,    56,    -1,    58
  };

  const signed char
  Parser::yystos_[] =
  {
       0,     3,     5,     6,     7,     8,     9,    10,    12,    50,
      59,    61,    64,    65,    66,    65,     3,     3,     3,     3,
       3,    66,    66,    66,     0,    13,    14,    15,    16,    18,
      22,    24,    26,    27,    28,    29,    30,    32,    34,    36,
      38,    40,    42,    44,    46,    48,    50,    52,    54,    56,
      58,     4,    12,    11,    11,    11,    11,    65,    13,    13,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,     4,     4,     4,     4,     4,    65,    65,
      20,    65
  };

  const signed char
  Parser::yyr1_[] =
  {
       0,    63,    64,    64,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    66,    66,    66,    66,    66,    66,    66,    66,    66,
      66,    66
  };

  const signed char
  Parser::yyr2_[] =
  {
       0,     2,     0,     1,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     5,     3,     3,     4,     4,
       1,     2,     2,     2,     3,     4,     4,     4,     4,     4,
       1,     1
  };


#if YYDEBUG
  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
  const char*
  const Parser::yytname_[] =
  {
  "YYEOF", "error", "\"invalid token\"", "\"(\"", "\")\"", "HASPROP",
  "FILEIOVFD", "NUM", "FNAME", "JGROUP", "JPARENT", "QSTR", "NAME", "IN",
  "SOME", "ORDR", "COMMA", "\",\"", "QWE", "\"?\"", "COLON", "\":\"",
  "AND", "\"&&\"", "OR", "\"|\"", "EQV", "NEQV", "BITAND", "BITOR",
  "BITXOR", "\"^\"", "EQ", "\"=\"", "NE", "\"!=\"", "LT", "\"<\"", "GT",
  "\">\"", "LE", "\"<=\"", "GE", "\">=\"", "LS", "\"<<\"", "RS", "\">>\"",
  "ADD", "\"+\"", "MINUS", "\"-\"", "MUL", "\"*\"", "DIV", "\"/\"", "REM",
  "\"%\"", "DEG", "NOT", "\"!\"", "BITNOT", "\"~\"", "$accept", "S", "exp",
  "term", YY_NULLPTR
  };
#endif


#if YYDEBUG
  const unsigned char
  Parser::yyrline_[] =
  {
       0,   120,   120,   121,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   149,   150,   151,   152,
     153,   155,   160,   161,   162,   163,   169,   174,   180,   186,
     192,   193
  };

  void
  Parser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  Parser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


#line 67 "QLParser.yy"
} // QL
#line 1279 "QLParser.tab.cc"

#line 195 "QLParser.yy"


namespace QL
{
  static Parser::symbol_type
  unget_ret (std::istream &in, char c, Parser::symbol_type tok)
  {
    in.putback (c);
    return tok;
  }

  static Parser::symbol_type
  yylex (QL::Result &result)
  {
    int base = 0;
    int c;

    do
      c = result.in.get ();
    while (result.in && (c == ' ' || c == '\t'));
    if (!result.in)
      return Parser::make_YYEOF ();

    switch (c)
      {
      case '\0':
      case '\n': return Parser::make_YYEOF ();
      case '(': return Parser::make_LPAR () ;
      case ')': return Parser::make_RPAR ();
      case ',': return Parser::make_COMMA ();
      case '%': return Parser::make_REM ();
      case '/': return Parser::make_DIV ();
      case '*': return Parser::make_MUL ();
      case '-': return Parser::make_MINUS ();
      case '+': return Parser::make_ADD ();
      case '~': return Parser::make_BITNOT ();
      case '^': return Parser::make_BITXOR ();
      case '?': return Parser::make_QWE ();
      case ':': return Parser::make_COLON ();
      case '|':
	c = result.in.get ();
	if (c == '|')
	  return Parser::make_OR ();
	else
	  return unget_ret (result.in, c, Parser::make_BITOR ());
      case '&':
	c = result.in.get ();
	if (c == '&')
	  return Parser::make_AND ();
	else
	  return unget_ret (result.in, c, Parser::make_BITAND ());
      case '!':
	c = result.in.get ();
	if (c == '=')
	  return Parser::make_NE ();
	else
	  return unget_ret (result.in, c, Parser::make_NOT ());
      case '=':
	c = result.in.get ();
	if (c == '=')
	  return Parser::make_EQ ();
	else
	  throw Parser::syntax_error ("Syntax error after =");
      case '<':
	c = result.in.get ();
	if (c == '=')
	  return Parser::make_LE ();
	else if (c == '<')
	  return Parser::make_LS ();
	else
	  return unget_ret (result.in, c, Parser::make_LT ());
      case '>':
	c = result.in.get ();
	if (c == '=')
	  return Parser::make_GE ();
	else if (c == '>')
	  return Parser::make_RS ();
	else
	  return unget_ret (result.in, c, Parser::make_GT ());
      case '"':
	{
	  int  maxsz = 16;
	  char *str = (char *) malloc (maxsz);
	  char *ptr = str;

	  for (;;)
	    {
	      c = result.in.get ();
	      if (!result.in)
		{
		  free (str);
		  throw Parser::syntax_error ("Unclosed \"");
		}

	      switch (c)
		{
		case '"':
		  *ptr = (char)0;
		  // XXX omazur: need new string type
		  return Parser::make_QSTR ((uint64_t) str);
		case 0:
		case '\n':
		  free (str);
		  throw Parser::syntax_error ("Multiline strings are not supported");
		default:
		  if (ptr - str >= maxsz)
		    {
		      size_t len = ptr - str;
		      maxsz = maxsz > 8192 ? maxsz + 8192 : maxsz * 2;
		      char *new_s = (char *) realloc (str, maxsz);
		      str = new_s;
		      ptr = str + len;
		    }
		  *ptr++ = c;
		}
	    }
	}
      default:
	if (c == '0')
	  {
	    base = 8;
	    c = result.in.get ();
	    if ( c == 'x' )
	      {
		base = 16;
		c = result.in.get ();
	      }
	  }
	else if (c >= '1' && c <='9')
	  base = 10;

	if (base)
	  {
	    uint64_t lval = 0;
	    for (;;)
	      {
		int digit = -1;
		switch (c)
		  {
		  case '0': case '1': case '2': case '3':
		  case '4': case '5': case '6': case '7':
		    digit = c - '0';
		    break;
		  case '8': case '9':
		    if (base > 8)
		      digit = c - '0';
		    break;
		  case 'a': case 'b': case 'c':
		  case 'd': case 'e': case 'f':
		    if (base == 16)
		      digit = c - 'a' + 10;
		    break;
		  case 'A': case 'B': case 'C':
		  case 'D': case 'E': case 'F':
		    if (base == 16)
		      digit = c - 'A' + 10;
		    break;
		  }
		if  (digit == -1)
		  {
		    result.in.putback (c);
		    break;
		  }
		lval = lval * base + digit;
		c = result.in.get ();
	      }
	    return Parser::make_NUM (lval);
	  }

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
	  {
	    char name[32];	// omazur XXX: accept any length
	    name[0] = (char)c;
	    for (size_t i = 1; i < sizeof (name); i++)
	      {
		c = result.in.get ();
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || (c == '_'))
		  name[i] = c;
		else
		  {
		    name[i] = (char)0;
		    result.in.putback (c);
		    break;
		  }
	      }

	    if (strcasecmp (name, NTXT ("IN")) == 0)
	      return Parser::make_IN ();
	    else if (strcasecmp (name, NTXT ("SOME")) == 0)
	      return Parser::make_SOME ();
	    else if (strcasecmp (name, NTXT ("ORDERED")) == 0)
	      return Parser::make_ORDR ();
	    else if (strcasecmp (name, NTXT ("TRUE")) == 0)
	      return Parser::make_NUM ((uint64_t) 1);
	    else if (strcasecmp (name, NTXT ("FALSE")) == 0)
	      return Parser::make_NUM ((uint64_t) 0);
	    else if (strcasecmp (name, NTXT ("FNAME")) == 0)
	      return Parser::make_FNAME (Expression::FUNC_FNAME);
	    else if (strcasecmp (name, NTXT ("HAS_PROP")) == 0)
	      return Parser::make_HASPROP ();
	    else if (strcasecmp (name, NTXT ("JGROUP")) == 0)
	      return Parser::make_JGROUP (Expression::JAVA_JGROUP);
	    else if (strcasecmp (name, NTXT ("JPARENT")) == 0 )
	      return Parser::make_JPARENT (Expression::JAVA_JPARENT);
	    else if (strcasecmp (name, NTXT ("DNAME")) == 0)
	      return Parser::make_FNAME (Expression::FUNC_DNAME);
	    else if (strcasecmp (name, NTXT ("FILEIOVFD")) == 0 )
	      return Parser::make_FILEIOVFD ();

	    std::string nm = std::string (name);
	    return Parser::make_NAME (nm);
	  }

	throw Parser::syntax_error ("Syntax error");
      }
  }
  void
  Parser::error (const std::string &)
  {
    // do nothing for now
  }
}

