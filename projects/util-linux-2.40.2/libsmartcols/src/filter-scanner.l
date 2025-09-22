%{
#include "smartcolsP.h"
#include "filter-parser.h"	/* define tokens (T_*) */
%}

%option reentrant bison-bridge noyywrap noinput nounput

id	[a-zA-Z][a-zA-Z_.%:/\-0-9]*
int	[0-9]+
blank	[ \t]
str_qu	\"[^\"\n]*\"
str_ap	\'[^\'\n]*\'

%%

{blank}+	;	/* ignore */
[\n]+		;	/* ignore */

"("		return '(';
")"		return ')';
"'"		return '\'';

and|AND|"&&"	return T_AND;
or|OR|"||"	return T_OR;
"!"|not|NOT	return T_NEG;

eq|EQ|"=="	return T_EQ;
ne|NE|"!="	return T_NE;

le|LE|"<="	return T_LE;
lt|LT|"<"	return T_LT;

ge|GE|">="	return T_GE;
gt|GT|">"	return T_GT;

"=~"		return T_REG;
"!~"		return T_NREG;

false|FALSE	return T_FALSE;
true|TRUE	return T_TRUE;

{int}+\.{int}+ {
	yylval->param_float = strtold(yytext, NULL);
	return T_FLOAT;
}

{int}+ {
	yylval->param_number = (int64_t) strtoumax(yytext, NULL, 10);
	return T_NUMBER;
}

{id} {
	yylval->param_name = yytext;
	return T_HOLDER;
}

{str_ap}|{str_qu} {
	yylval->param_string = yytext;
	return T_STRING;
}


