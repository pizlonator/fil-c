# For keymaps in which AltGr_Lock is used to switch between two alphabets.
#
# A common construction is
#	alt keycode  42 = AltGr_Lock
#	altgr alt keycode  42 = AltGr_Lock
#
# All Linux keys have the same meaning with or without AltGr.
# No convention for Hex_* has been established.

include "linux-keys-bare"

keycode 100 = AltGr

altgr keycode  59 = F1
altgr keycode  60 = F2
altgr keycode  61 = F3
altgr keycode  62 = F4
altgr keycode  63 = F5
altgr keycode  64 = F6
altgr keycode  65 = F7
altgr keycode  66 = F8
altgr keycode  67 = F9
altgr keycode  68 = F10
altgr keycode  87 = F11
altgr keycode  88 = F12
altgr shift keycode  59 = F13
altgr shift keycode  60 = F14
altgr shift keycode  61 = F15
altgr shift keycode  62 = F16
altgr shift keycode  63 = F17
altgr shift keycode  64 = F18
altgr shift keycode  65 = F19
altgr shift keycode  66 = F20
altgr shift keycode  67 = F21
altgr shift keycode  68 = F22
altgr shift keycode  87 = F23
altgr shift keycode  88 = F24
altgr control keycode  59 = F25
altgr control keycode  60 = F26
altgr control keycode  61 = F27
altgr control keycode  62 = F28
altgr control keycode  63 = F29
altgr control keycode  64 = F30
altgr control keycode  65 = F31
altgr control keycode  66 = F32
altgr control keycode  67 = F33
altgr control keycode  68 = F34
altgr control keycode  87 = F35
altgr control keycode  88 = F36
#
altgr alt keycode  59 = Console_1
altgr alt keycode  60 = Console_2
altgr alt keycode  61 = Console_3
altgr alt keycode  62 = Console_4
altgr alt keycode  63 = Console_5
altgr alt keycode  64 = Console_6
altgr alt keycode  65 = Console_7
altgr alt keycode  66 = Console_8
altgr alt keycode  67 = Console_9
altgr alt keycode  68 = Console_10
altgr alt keycode  87 = Console_11
altgr alt keycode  88 = Console_12
altgr control alt keycode  59 = Console_1
altgr control alt keycode  60 = Console_2
altgr control alt keycode  61 = Console_3
altgr control alt keycode  62 = Console_4
altgr control alt keycode  63 = Console_5
altgr control alt keycode  64 = Console_6
altgr control alt keycode  65 = Console_7
altgr control alt keycode  66 = Console_8
altgr control alt keycode  67 = Console_9
altgr control alt keycode  68 = Console_10
altgr control alt keycode  87 = Console_11
altgr control alt keycode  88 = Console_12
#
altgr alt keycode  71 = Ascii_7
altgr alt keycode  72 = Ascii_8
altgr alt keycode  73 = Ascii_9
altgr alt keycode  75 = Ascii_4
altgr alt keycode  76 = Ascii_5
altgr alt keycode  77 = Ascii_6
altgr alt keycode  79 = Ascii_1
altgr alt keycode  80 = Ascii_2
altgr alt keycode  81 = Ascii_3
altgr alt keycode  82 = Ascii_0
#
altgr alt keycode 103 = KeyboardSignal
altgr alt keycode 105 = Decr_Console
altgr alt keycode 106 = Incr_Console
#
altgr shift keycode 104 = Scroll_Backward
altgr shift keycode 109 = Scroll_Forward
#
# altgr control keycode  83 = Boot
altgr control alt keycode  83 = Boot
# altgr control keycode 111 = Boot
altgr control alt keycode 111 = Boot
#
# an exception - altgr changes the function
#
altgr   keycode  70 = Show_Registers
