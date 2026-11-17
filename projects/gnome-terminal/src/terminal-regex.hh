/*
 * Copyright © 2015 Egmont Koblinger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Mini style-guide:
 *
 * #define'd fragments should preferably have an outermost group, for the
 * exact same reason as why usually in C/C++ #define's the values are enclosed
 * in parentheses: that is, so that you don't get surprised when you use the
 * macro and append a quantifier.
 *
 * For repeated fragments prefer regex-style (?(DEFINE)(?<NAME>(...))) and use
 * as (?&NAME), so that the regex string and the compiled regex object is
 * smaller.
 *
 * Build small blocks, comment and unittest them heavily.
 *
 * Use free-spacing mode for improved readability. The hardest to read is
 * which additional characters belong to a "(?" prefix. To improve
 * readability, place a space after this, and for symmetry, before the closing
 * parenthesis. Also place a space around "|" characters. No space before
 * quantifiers. Try to be consistent with the existing style (yes I know the
 * existing style is not consistent either, but please do your best).
 *
 * See http://www.rexegg.com/regex-disambiguation.html for all the "(?"
 * syntaxes.
 */

#ifndef TERMINAL_REGEX_H
#define TERMINAL_REGEX_H

/* Lookbehind to see if there's a preceding apostrophe.
 * Unlike the other *_DEF macros which define regex subroutines,
 * this one is a named capture that defines APOS_START to either
 * an apostrophe or the empty string, depending on the character
 * preceding this APOS_START_DEF construct. */
#define APOS_START_DEF "(?<APOS_START>(?<='))?"

#define SCHEME "(?ix: news | telnet | nntp | https? | ftps? | sftp | webcal )"

#define USERCHARS "-+.[:alnum:]"
/* Nonempty username, e.g. "john.smith" */
#define USER "[" USERCHARS "]+"

#define PASSCHARS_CLASS "[-[:alnum:]\\Q,?;.:/!%$^*&~\"#'\\E]"
/* Optional colon-prefixed password. I guess empty password should be allowed, right? E.g. ":secret", ":", "" */
#define PASS "(?x: :" PASSCHARS_CLASS "* )?"

/* Optional at-terminated username (with perhaps a password too), e.g. "joe@", "pete:secret@", "" */
#define USERPASS "(?:" USER PASS "@)?"

/* S4: IPv4 segment (number between 0 and 255) with lookahead at the end so that we don't match "25" in the string "256".
   The lookahead could go to the last segment of IPv4 only but this construct allows nicer unittesting. */
#define S4_DEF "(?(DEFINE)(?<S4>(?x: (?: [0-9] | [1-9][0-9] | 1[0-9]{2} | 2[0-4][0-9] | 25[0-5] ) (?! [0-9] ) )))"

/* IPV4: Decimal IPv4, e.g. "1.2.3.4", with lookahead (implemented in S4) at the end so that we don't match "192.168.1.123" in the string "192.168.1.1234". */
#define IPV4_DEF S4_DEF "(?(DEFINE)(?<IPV4>(?x: (?: (?&S4) \\. ){3} (?&S4) )))"

/* IPv6, including embedded IPv4, e.g. "::1", "dead:beef::1.2.3.4".
 * Lookahead for the next char not being a dot or digit, so it doesn't get stuck matching "dead:beef::1" in "dead:beef::1.2.3.4".
 * This is not required since the surrounding brackets would trigger backtracking, but it allows nicer unittesting.
 * TODO: more strict check (right number of colons, etc.)
 * TODO: add zone_id: RFC 4007 section 11, RFC 6874 */

/* S6: IPv6 segment, S6C: IPv6 segment followed by a comma, CS6: comma followed by an IPv6 segment */
#define S6_DEF "(?(DEFINE)(?<S6>[[:xdigit:]]{1,4})(?<CS6>:(?&S6))(?<S6C>(?&S6):))"

/* No :: shorthand */
#define IPV6_FULL  "(?x: (?&S6C){7} (?&S6) )"
/* Begins with :: */
#define IPV6_LEFT  "(?x: : (?&CS6){1,7} )"
/* :: somewhere in the middle - use negative lookahead to make sure there aren't too many colons in total */
#define IPV6_MID   "(?x: (?! (?: [[:xdigit:]]*: ){8} ) (?&S6C){1,6} (?&CS6){1,6} )"
/* Ends with :: */
#define IPV6_RIGHT "(?x: (?&S6C){1,7} : )"
/* Is "::" and nothing more */
#define IPV6_NULL  "(?x: :: )"

/* The same ones for IPv4-embedded notation, without the actual IPv4 part */
#define IPV6V4_FULL  "(?x: (?&S6C){6} )"
#define IPV6V4_LEFT  "(?x: :: (?&S6C){0,5} )"  /* includes "::<ipv4>" */
#define IPV6V4_MID   "(?x: (?! (?: [[:xdigit:]]*: ){7} ) (?&S6C){1,4} (?&CS6){1,4} ) :"
#define IPV6V4_RIGHT "(?x: (?&S6C){1,5} : )"

/* IPV6: An IPv6 address (possibly with an embedded IPv4).
 * This macro defines both IPV4 and IPV6, since the latter one requires the former. */
#define IP_DEF IPV4_DEF S6_DEF "(?(DEFINE)(?<IPV6>(?x: (?: " IPV6_NULL " | " IPV6_LEFT " | " IPV6_MID " | " IPV6_RIGHT " | " IPV6_FULL " | (?: " IPV6V4_FULL " | " IPV6V4_LEFT " | " IPV6V4_MID " | " IPV6V4_RIGHT " ) (?&IPV4) ) (?! [.:[:xdigit:]] ) )))"

/* Either an alphanumeric character or dash; or if [negative lookahead] not ASCII
 * then any graphical Unicode character.
 * A segment can consist entirely of numbers.
 * (Note: PCRE doesn't support character class subtraction/intersection.) */
#define HOSTNAMESEGMENTCHARS_CLASS "(?x: [-[:alnum:]] | (?! [[:ascii:]] ) [[:graph:]] )"

/* A hostname of at least 1 component. The last component cannot be entirely numbers.
 * E.g. "foo", "example.com", "1234.com", but not "foo.123" */
#define HOSTNAME1 "(?x: (?: " HOSTNAMESEGMENTCHARS_CLASS "+ \\. )* " HOSTNAMESEGMENTCHARS_CLASS "* (?! [0-9] ) " HOSTNAMESEGMENTCHARS_CLASS "+ )"

/* A hostname of at least 2 components. The last component cannot be entirely numbers.
 * E.g. "example.com", "1234.com", but not "1234.56" */
#define HOSTNAME2 "(?x: (?: " HOSTNAMESEGMENTCHARS_CLASS "+ \\.)+ " HOSTNAME1 " )"

/* For URL: Hostname, IPv4, or bracket-enclosed IPv6, e.g. "example.com", "1.2.3.4", "[::1]" */
#define URL_HOST "(?x: " HOSTNAME1 " | (?&IPV4) | \\[ (?&IPV6) \\] )"

/* For e-mail: Hostname of at least two segments, or bracket-enclosed IPv4 or IPv6, e.g. "example.com", "[1.2.3.4]", "[::1]".
 * Technically an e-mail with a single-component hostname might be valid on a local network, but let's avoid tons of false positives (e.g. in a typical shell prompt). */
#define EMAIL_HOST "(?x: " HOSTNAME2 " | \\[ (?: (?&IPV4) | (?&IPV6) ) \\] )"

/* Number between 1 and 65535, with lookahead at the end so that we don't match "6789" in the string "67890",
   and in turn we don't eventually match "http://host:6789" in "http://host:67890". */
#define N_1_65535 "(?x: (?: [1-9][0-9]{0,3} | [1-5][0-9]{4} | 6[0-4][0-9]{3} | 65[0-4][0-9]{2} | 655[0-2][0-9] | 6553[0-5] ) (?! [0-9] ) )"

/* Optional colon-prefixed port, e.g. ":1080", "" */
#define PORT "(?x: \\:" N_1_65535 " )?"

/* Omit the parentheses, see below */
#define PATHCHARS_CLASS "[-[:alnum:]\\Q_$.+!*,:;@&=?/~#|%'\\E]"
/* Chars to end a URL. Apostrophe only allowed if there wasn't one in front of the URL, see bug 448044 */
#define PATHTERM_CLASS        "[-[:alnum:]\\Q_$+*@&=/~#|%'\\E]"
#define PATHTERM_NOAPOS_CLASS "[-[:alnum:]\\Q_$+*@&=/~#|%\\E]"

/* Recursive definition of PATH that allows parentheses and square brackets only if balanced, see bug 763980. */
#define PATH_INNER_DEF "(?(DEFINE)(?<PATH_INNER>(?x: (?: " PATHCHARS_CLASS "* (?: \\( (?&PATH_INNER) \\) | \\[ (?&PATH_INNER) \\] ) )* " PATHCHARS_CLASS "* )))"
/* Same as above, but the last character (if exists and is not a parenthesis) must be from PATHTERM_CLASS. */
#define PATH_DEF "(?(DEFINE)(?<PATH>(?x: (?: " PATHCHARS_CLASS "* (?: \\( (?&PATH_INNER) \\) | \\[ (?&PATH_INNER) \\] ) )* (?: " PATHCHARS_CLASS "* (?(<APOS_START>)" PATHTERM_NOAPOS_CLASS "|" PATHTERM_CLASS ") )? )))"

#define URLPATH "(?x: [/?#](?&PATH) )?"
#define VOIP_PATH "(?x: [;?](?&PATH) )?"

/* Now let's put these fragments together */

#define DEFS APOS_START_DEF IP_DEF PATH_INNER_DEF PATH_DEF

#define REGEX_URL_AS_IS  DEFS SCHEME "://" USERPASS URL_HOST PORT URLPATH
/* TODO: also support file:/etc/passwd */
#define REGEX_URL_FILE   DEFS "(?ix: file:/ (?: / (?: " HOSTNAME1 " )? / )? (?! / ) )(?&PATH)"
/* Lookbehind so that we don't catch "abc.www.foo.bar", bug 739757. Lookahead for www/ftp for convenience (so that we can reuse HOSTNAME1). */
#define REGEX_URL_HTTP   DEFS "(?<!(?:" HOSTNAMESEGMENTCHARS_CLASS "|[.]))(?=(?i:www|ftp))" HOSTNAME1 PORT URLPATH
#define REGEX_URL_VOIP   DEFS "(?i:h323:|sips?:)" USERPASS URL_HOST PORT VOIP_PATH
#define REGEX_EMAIL      DEFS "(?i:mailto:)?" USER "@" EMAIL_HOST
#define REGEX_NEWS_MAN   "(?i:news:|man:|info:)[-[:alnum:]\\Q^_{|}~!\"#$%&'()*+,./;:=?`\\E]+"

#endif /* !TERMINAL_REGEX_H */
