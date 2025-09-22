#!/bin/sh

#
# This script checks for #ifdef HAVE_DECL_SYMBOL in code.
#
# Autoconf docs:
#
# Unlike the other autoconf ‘AC_CHECK_*S’ macros, when a symbol is not
# declared, HAVE_DECL_symbol is defined to ‘0’ instead of leaving
# HAVE_DECL_symbol undeclared. When you are sure that the check was performed,
# use HAVE_DECL_symbol in #if.
#

if [ ! -f ./configure ]; then
	echo "Not found configure script"
	exit 1
fi

for decl in $(awk '/HAVE_DECL_.*ac_have_decl/ { print $2 }' configure); do
	git grep -nE '[[:blank:]]*#[[:blank:]]*if(ndef|def)[[:blank:]]*'$decl;
done | sort -u
