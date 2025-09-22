#
# hypermap.map 1994/11/11
# Michael Shields <shields@tembel.org>
#
# A keymap redesigned for sanity.
#

#
# This keymap is a ground-up reimplementation of the keyboard map,
# intended to be consistent and rational.  It uses an m4 metalanguage to
# declare the key mappings.  Usage is `m4 hypermap.map | loadkeys'.
#
# The modifier flags used are `shift' (1), `control' (4), and `alt' (8).
# Left and right modifiers are not distinguished.
#
# In general, Meta is always distinguished, and M-S-KEY is distinct from
# M-KEY.  This is good news for Emacs users.  C-S-KEY is consistently
# folded into C-KEY.
#
# Shift is more loosely interpreted than the other modifiers; usually if
# S-KEY has no special meaning, the action will be the same as KEY.
# However, if M-KEY or H-KEY is undefined, nothing happens.
#
# Because Caps Lock's position is so out of proportion to its utility,
# it's been totally redefined to a new sort of modifier, which I've
# arbitrarily named `Hyper'.  Shift is ignored with Hyper.  Hyper
# provides dead keys:
#	H-`	dead accent grave
#	H-'	dead acute accent
#	H-^	dead circumflex
#	H-t	dead tilde
#	H-d	dead diaeresis
# and ISO-8859-1 symbols (some more mnemonic than others):
#	H-SPC	nobreakspace
#	H-!	inverted bang
#	H-h	cents (`hundredths')
#	H-#	pounds sterling
#	H-$	currency
#	H-y	yen
#	H-|	broken bar
#	H-s	section (the galaxy symbol)
#	H-c	copyright
#	H-a	feminine ordinal
#	H-<	left guillemot
#	H-]	not sign (it's angular)
#	H--	soft hyphen
#	H-r	registered trademark symbol
#	H-=	macron
#	H-0	degrees
#	H-[	plus/minus (near + and -)
#	H-k	superior 2 (jkl form a series)
#	H-l	superior 3
#	H-u	mu
#	H-p	pilcrow
#	H-:	centered dot
#	H-j	superior 1
#	H-o	masculine ordinal
#	H->	right guillemot
#	H-q	1/4 fraction (qwe form a series)
#	H-w	1/2 fraction
#	H-e	3/4 fraction
#	H-?	inverted question mark
#	H-x	multiplication symbol
#	H-%	division symbol
# You can also use Hyper plus Alt to type characters by decimal code on
# the keypad, as with Alt alone, or in hex on the main keyboard.  And
# H-TAB is a Caps Lock.
#
# Function keys work as marked, unless with Alt, in which case they
# switch to the console with the same number.  Shift adds 12 and Hyper
# adds 24 to the numbers (i.e., they logically switch to other banks).
# Thus you can easily address up to 48.  Control is ignored, for
# consistency with X.
#
# Finally, you can change the behavior of the Caps Lock and Controls
# with m4 command-line options.  The default values are equivalent to
# `-DCAPSLOCK_K=Hyper -DLEFTCTRL_K=Control -DRIGHTCTRL_K=Control'.  You
# can remove the Hyper code entirely with `-DCAPSLOCK_K=Caps_Lock'.
#
# This file is arranged vaguely by key position on the classic PC layout.
#


dnl Set default values.
ifdef(`CAPSLOCK_K', , `define(`CAPSLOCK_K', `Hyper')')
ifdef(`LEFTCTRL_K', , `define(`LEFTCTRL_K', `Control')')
ifdef(`RIGHTCTRL_K', , `define(`RIGHTCTRL_K', `Control')')

dnl Figure out if any key is a Hyper key.  If so, define `hyper', both
dnl for usefulness and `ifdef' testability.
ifelse(CAPSLOCK_K, `Hyper', `define(`hyper', `ctrlr')',
       LEFTCTRL_K, `Hyper', `define(`hyper', `ctrlr')',
       RIGHTCTRL_K, `Hyper', `define(`hyper', `ctrlr')')

define(`Hyper', `CtrlR')


keymaps 0,1,4,5,8,9,12,13`'ifdef(`hyper', `,128,129,132,133,136,137,140,141')


dnl General usage of these macros is MACRO(KEYCODE, UNSHIFTED, SHIFTED).

dnl We first undefine `shift', which only causes problems.
undefine(`shift')

define(`SIMPLE', `keycode $1 = $2 $2')

define(`KEY',
`			keycode $1 = $2 VoidSymbol
shift			keycode $1 = $3
		alt	keycode $1 = Meta_$2
shift		alt	keycode $1 = Meta_$3')

dnl This macro adds Control variations to a key.
define(`CONTROL',
`	control		keycode $1 = $2
shift	control		keycode $1 = $2
	control	alt	keycode $1 = Meta_$2
shift	control	alt	keycode $1 = Meta_$2')

dnl Add Hyper variations to a key.
define(`HYPER', ifdef(`hyper',
`			hyper	keycode $1 = $2
shift			hyper	keycode $1 = $2'))
define(`CONTROLHYPER', ifdef(`hyper',
`	control		hyper	keycode $1 = $2
shift	control		hyper	keycode $1 = $2'))
define(`METAHYPER', ifdef(`hyper',
`		alt	hyper	keycode $1 = $2
shift		alt	hyper	keycode $1 = $2'))
define(`CONTROLMETAHYPER', ifdef(`hyper', dnl Ludicrous.
`	control	alt	hyper	keycode $1 = $2
shift	control	alt	hyper	keycode $1 = $2'))

dnl Special case for letters.  Best to be explicit.
define(`LETTER',
`			keycode $1 = `+'$2 VoidSymbol
shift			keycode $1 = `+'translit($2, `a-z', `A-Z')
		alt	keycode $1 = `Meta_'$2
shift		alt	keycode $1 = `Meta_'translit($2, `a-z', `A-Z')
CONTROL($1, Control_$2)')

dnl For function keys.  Call here is FUNCTION(KEYCODE, FKEYNUM).
define(`BANKSIZE', 12)
define(`FUNCTION',
`			keycode $1 = `F'$2 VoidSymbol
shift			keycode $1 = `F'eval($2 + BANKSIZE)
	hyper		keycode $1 = `F'eval($2 + BANKSIZE * 2)
shift	hyper		keycode $1 = `F'eval($2 + BANKSIZE * 3)
		alt	keycode $1 = `Console_'$2
shift		alt	keycode $1 = `Console_'eval($2 + BANKSIZE)
	hyper	alt	keycode $1 = `Console_'eval($2 + BANKSIZE * 2)
shift	hyper	alt	keycode $1 = `Console_'eval($2 + BANKSIZE * 3)')

dnl For the keypad digits.  KPDIGIT(KEYCODE, DIGIT).
define(`KPDIGIT',
`			keycode $1 = KP_$2 VoidSymbol
shift			keycode $1 = KP_$2
		alt	keycode $1 = Ascii_$2
shift		alt	keycode $1 = Ascii_$2
METAHYPER($1, Ascii_$2)')

dnl And a special approximation:
define(`Meta_Return', `Meta_Control_m')


KEY(1, Escape, Escape)

FUNCTION(59, 1)
FUNCTION(60, 2)
FUNCTION(61, 3)
FUNCTION(62, 4)
FUNCTION(63, 5)
FUNCTION(64, 6)
FUNCTION(65, 7)
FUNCTION(66, 8)
FUNCTION(67, 9)
FUNCTION(68, 10)
FUNCTION(87, 11)
FUNCTION(88, 12)

KEY(2, one, exclam)
    HYPER(2, exclamdown)
    METAHYPER(2, Hex_1)
KEY(3, two, at)
    CONTROL(3, nul)
    METAHYPER(3, Hex_2)
KEY(4, three, numbersign)
    HYPER(4, pound)
    METAHYPER(4, Hex_3)
KEY(5, four, dollar)
    HYPER(5, currency)
    METAHYPER(5, Hex_4)
KEY(6, five, percent)
    HYPER(6, division)
    METAHYPER(6, Hex_5)
KEY(7, six, asciicircum)
    CONTROL(7, Control_asciicircum)
    HYPER(7, dead_circumflex)
    METAHYPER(7, Hex_6)
KEY(8, seven, ampersand)
    METAHYPER(8, Hex_7)
KEY(9, eight, asterisk)
    METAHYPER(9, Hex_8)
KEY(10, nine, parenleft)
    METAHYPER(10, Hex_9)
KEY(11, zero, parenright)
    HYPER(11, degree)
    METAHYPER(11, Hex_0)
KEY(12, minus, underscore)
    CONTROL(12, Control_underscore)
    HYPER(12, hyphen)
KEY(13, equal, plus)
    HYPER(13, macron)
KEY(14, Delete, Delete)
    CONTROL(14, BackSpace)

KEY(15, Tab, Tab)
    HYPER(15, Caps_Lock)
LETTER(16, q)
    HYPER(16, onequarter)
LETTER(17, w)
    HYPER(17, onehalf)
LETTER(18, e)
    HYPER(18, threequarters)
    METAHYPER(18, Hex_E)
LETTER(19, r)
    HYPER(19, registered)
LETTER(20, t)
    HYPER(20, dead_tilde)
LETTER(21, y)
    HYPER(21, yen)
LETTER(22, u)
    HYPER(22, mu)
LETTER(23, i)
LETTER(24, o)
    HYPER(24, masculine)
LETTER(25, p)
    HYPER(25, 182) # pilcrow
KEY(26, bracketleft, braceleft)
    CONTROL(26, Escape)
    HYPER(26, plusminus)
KEY(27, bracketright, braceright)
    CONTROL(27, Control_bracketright)
    HYPER(27, notsign)

KEY(28, Return, Return)

LETTER(30, a)
    HYPER(30, ordfeminine)
    METAHYPER(30, Hex_A)
LETTER(31, s)
    HYPER(31, section)
LETTER(32, d)
    HYPER(32, dead_diaeresis)
    METAHYPER(32, Hex_D)
LETTER(33, f)
    METAHYPER(33, Hex_F)
LETTER(34, g)
LETTER(35, h)
    HYPER(35, cent)
LETTER(36, j)
    HYPER(36, onesuperior)
LETTER(37, k)
    HYPER(37, twosuperior)
LETTER(38, l)
    HYPER(38, threesuperior)
KEY(39, semicolon, colon)
    HYPER(39, periodcentered)
KEY(40, apostrophe, quotedbl)
    HYPER(40, dead_acute)
KEY(41, grave, asciitilde)
    HYPER(41, dead_grave)

KEY(43, backslash, bar)
    CONTROL(43, Control_backslash)
    HYPER(43, brokenbar)
LETTER(44, z)
LETTER(45, x)
    HYPER(45, multiplication)
LETTER(46, c)
    HYPER(46, copyright)
    METAHYPER(46, Hex_C)
LETTER(47, v)
LETTER(48, b)
    METAHYPER(48, Hex_B)
LETTER(49, n)
LETTER(50, m)
KEY(51, comma, less)
    HYPER(51, guillemotleft)
KEY(52, period, greater)
    HYPER(52, guillemotright)
KEY(53, slash, question)
    HYPER(53, questiondown)

KEY(57, space, space)
    CONTROL(57, nul)
    HYPER(57, nobreakspace)

KPDIGIT(71, 7)
KPDIGIT(72, 8)
KPDIGIT(73, 9)
SIMPLE(74, KP_Subtract)
KPDIGIT(75, 4)
KPDIGIT(76, 5)
KPDIGIT(77, 6)
SIMPLE(78, KP_Add)
KPDIGIT(79, 1)
KPDIGIT(80, 2)
KPDIGIT(81, 3)
KPDIGIT(82, 0)
SIMPLE(83, KP_Period)
SIMPLE(96, KP_Enter)
SIMPLE(98, KP_Divide)
SIMPLE(55, KP_Multiply)
SIMPLE(118, KP_MinPlus)

# Modifiers.
keycode  42 = Shift
keycode  54 = Shift
keycode  56 = Alt
keycode 100 = Alt
keycode  58 = CAPSLOCK_K
keycode  29 = LEFTCTRL_K
keycode  97 = RIGHTCTRL_K

# Everyone needs a compose key.  This is C-..
control	keycode  52 = Compose

SIMPLE(69, Num_Lock)

keycode  70 = Scroll_Lock
	shift	keycode  70 = Show_Memory
	control	keycode  70 = Show_State
	alt	keycode  70 = Show_Registers

# SysRq.  I suppose there's some reason it's ^\.
SIMPLE(99, Control_backslash)

SIMPLE(119, Pause)
# Ctrl-Break seems to have low-level hardware behind it.
SIMPLE(101, Break)

SIMPLE(110, Insert)
SIMPLE(102, Find)
keycode 104 = Prior Scroll_Backward
SIMPLE(111, Remove)
SIMPLE(107, Select)
keycode 109 = Next Scroll_Forward

SIMPLE(103, Up)
SIMPLE(105, Left)
alt keycode 105 = Decr_Console
SIMPLE(106, Right)
alt keycode 106 = Incr_Console
SIMPLE(108, Down)

control	alt keycode  83 = Boot
control	alt keycode 111 = Boot


# Stock VT102 string definitions.
string F1 = "\033[[A"
string F2 = "\033[[B"
string F3 = "\033[[C"
string F4 = "\033[[D"
string F5 = "\033[[E"
string F6 = "\033[17~"
string F7 = "\033[18~"
string F8 = "\033[19~"
string F9 = "\033[20~"
string F10 = "\033[21~"
string F11 = "\033[23~"
string F12 = "\033[24~"
string F13 = "\033[25~"
string F14 = "\033[26~"
string F15 = "\033[28~"
string F16 = "\033[29~"
string F17 = "\033[31~"
string F18 = "\033[32~"
string F19 = "\033[33~"
string F20 = "\033[34~"
string Find = "\033[1~"
string Insert = "\033[2~"
string Remove = "\033[3~"
string Select = "\033[4~"
string Prior = "\033[5~"
string Next = "\033[6~"
string Macro = "\033[M"
string Pause = "\033[P"

# Stock ISO-8859-1 compositions.
changequote()dnl
compose '`' 'A' to 'À'
compose '`' 'a' to 'à'
compose '\'' 'A' to 'Á'
compose '\'' 'a' to 'á'
compose '^' 'A' to 'Â'
compose '^' 'a' to 'â'
compose '~' 'A' to 'Ã'
compose '~' 'a' to 'ã'
compose '"' 'A' to 'Ä'
compose '"' 'a' to 'ä'
compose 'O' 'A' to 'Å'
compose 'o' 'a' to 'å'
compose '0' 'A' to 'Å'
compose '0' 'a' to 'å'
compose 'A' 'A' to 'Å'
compose 'a' 'a' to 'å'
compose 'A' 'E' to 'Æ'
compose 'a' 'e' to 'æ'
compose ',' 'C' to 'Ç'
compose ',' 'c' to 'ç'
compose '`' 'E' to 'È'
compose '`' 'e' to 'è'
compose '\'' 'E' to 'É'
compose '\'' 'e' to 'é'
compose '^' 'E' to 'Ê'
compose '^' 'e' to 'ê'
compose '"' 'E' to 'Ë'
compose '"' 'e' to 'ë'
compose '`' 'I' to 'Ì'
compose '`' 'i' to 'ì'
compose '\'' 'I' to 'Í'
compose '\'' 'i' to 'í'
compose '^' 'I' to 'Î'
compose '^' 'i' to 'î'
compose '"' 'I' to 'Ï'
compose '"' 'i' to 'ï'
compose '-' 'D' to 'Ð'
compose '-' 'd' to 'ð'
compose '~' 'N' to 'Ñ'
compose '~' 'n' to 'ñ'
compose '`' 'O' to 'Ò'
compose '`' 'o' to 'ò'
compose '\'' 'O' to 'Ó'
compose '\'' 'o' to 'ó'
compose '^' 'O' to 'Ô'
compose '^' 'o' to 'ô'
compose '~' 'O' to 'Õ'
compose '~' 'o' to 'õ'
compose '"' 'O' to 'Ö'
compose '"' 'o' to 'ö'
compose '/' 'O' to 'Ø'
compose '/' 'o' to 'ø'
compose '`' 'U' to 'Ù'
compose '`' 'u' to 'ù'
compose '\'' 'U' to 'Ú'
compose '\'' 'u' to 'ú'
compose '^' 'U' to 'Û'
compose '^' 'u' to 'û'
compose '"' 'U' to 'Ü'
compose '"' 'u' to 'ü'
compose '\'' 'Y' to 'Ý'
compose '\'' 'y' to 'ý'
compose 'T' 'H' to 'Þ'
compose 't' 'h' to 'þ'
compose 's' 's' to 'ß'
compose '"' 'y' to 'ÿ'
compose 's' 'z' to 'ß'
compose 'i' 'j' to 'ÿ'

# ISO-8859-3 compositions for Esperanto.
compose 'C' 'x' to 'Æ' #198
compose 'G' 'x' to 'Ø' #216
compose 'H' 'x' to '¦' #166
compose 'J' 'x' to '¬' #172
compose 'S' 'x' to 'Þ' #222
compose 'U' 'x' to 'Ý' #221
compose 'c' 'x' to 'æ' #230
compose 'g' 'x' to 'ø' #248
compose 'h' 'x' to '¶' #182
compose 'j' 'x' to '¼' #188
compose 's' 'x' to 'þ' #254
compose 'u' 'x' to 'ý' #253
