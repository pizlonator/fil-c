# Linux-specific (and language-independent) keys
#
# This file describes function keys, console switching keys,
# keypad, cursor keys, and SysRq, ScrollLock, Pause
#
# It does not use the AltGr modifier, so that the user can opt
# to have both Alt keys produce the same effect.
# As a consequence, the Hex_* keys are missing, as are
#	control altgr keycode  83 = Boot
#	control altgr keycode 111 = Boot
#
# One culture-specific item that will be overridden in many keymaps
# is the KP_Period: many countries have a KP_Comma instead.
#
# One user-preference item will be the function of the Delete key.
#
# Old keyboards have 10 function keys, and keymaps have
# sometimes used shift+F1 = F11, and sometimes shift+F1 = F13.
# Here we assume an enhanced keyboard (with 12 function keys).
#
plain keycode  59 = F1
plain keycode  60 = F2
plain keycode  61 = F3
plain keycode  62 = F4
plain keycode  63 = F5
plain keycode  64 = F6
plain keycode  65 = F7
plain keycode  66 = F8
plain keycode  67 = F9
plain keycode  68 = F10
plain keycode  87 = F11
plain keycode  88 = F12
shift keycode  59 = F13
shift keycode  60 = F14
shift keycode  61 = F15
shift keycode  62 = F16
shift keycode  63 = F17
shift keycode  64 = F18
shift keycode  65 = F19
shift keycode  66 = F20
shift keycode  67 = F21
shift keycode  68 = F22
shift keycode  87 = F23
shift keycode  88 = F24
control keycode  59 = F25
control keycode  60 = F26
control keycode  61 = F27
control keycode  62 = F28
control keycode  63 = F29
control keycode  64 = F30
control keycode  65 = F31
control keycode  66 = F32
control keycode  67 = F33
control keycode  68 = F34
control keycode  87 = F35
control keycode  88 = F36
#
# The canonical assignment for F37-F48 uses
#	control shift keycode  59 = F37
# etc. If you want that, load linux-keys-extd.inc
#
# Both Alt+Fn and Control+Alt+Fn switch to Console n
# (in order to please Linux / dosemu / X).
# Here Consoles 1..12. For 13..24 load linux-keys-extd.inc
#
alt keycode  59 = Console_1
alt keycode  60 = Console_2
alt keycode  61 = Console_3
alt keycode  62 = Console_4
alt keycode  63 = Console_5
alt keycode  64 = Console_6
alt keycode  65 = Console_7
alt keycode  66 = Console_8
alt keycode  67 = Console_9
alt keycode  68 = Console_10
alt keycode  87 = Console_11
alt keycode  88 = Console_12
control alt keycode  59 = Console_1
control alt keycode  60 = Console_2
control alt keycode  61 = Console_3
control alt keycode  62 = Console_4
control alt keycode  63 = Console_5
control alt keycode  64 = Console_6
control alt keycode  65 = Console_7
control alt keycode  66 = Console_8
control alt keycode  67 = Console_9
control alt keycode  68 = Console_10
control alt keycode  87 = Console_11
control alt keycode  88 = Console_12

#
# Keypad keys
#
keycode  69 = Num_Lock
keycode  98 = KP_Divide
keycode  55 = KP_Multiply
keycode  74 = KP_Subtract
keycode  78 = KP_Add
keycode  96 = KP_Enter
#
keycode  71 = KP_7
keycode  72 = KP_8
keycode  73 = KP_9
keycode  75 = KP_4
keycode  76 = KP_5
keycode  77 = KP_6
keycode  79 = KP_1
keycode  80 = KP_2
keycode  81 = KP_3
keycode  82 = KP_0
#
keycode  83 = KP_Period			# or KP_Comma
control alt keycode  83 = Boot
#
alt keycode  71 = Ascii_7
alt keycode  72 = Ascii_8
alt keycode  73 = Ascii_9
alt keycode  75 = Ascii_4
alt keycode  76 = Ascii_5
alt keycode  77 = Ascii_6
alt keycode  79 = Ascii_1
alt keycode  80 = Ascii_2
alt keycode  81 = Ascii_3
alt keycode  82 = Ascii_0
#
# The four cursor keys
#
keycode 103 = Up
keycode 105 = Left
keycode 106 = Right
keycode 108 = Down
alt keycode 103 = KeyboardSignal
alt keycode 105 = Decr_Console
alt keycode 106 = Incr_Console
#
# The six edit keys
#
keycode 110 = Insert
keycode 102 = Home		# Find
keycode 104 = PageUp		# Prior
keycode 111 = Remove
keycode 107 = End		# Select
keycode 109 = PageDown		# Next
shift keycode 104 = Scroll_Backward
shift keycode 109 = Scroll_Forward
control	alt keycode 111 = Boot
#
# The three system keys
#
keycode  84 = Last_Console		# Alt+SysRq/PrintScrn
#keycode  99 = VoidSymbol		# SysRq/PrintScrn
keycode  99 = Compose			# SysRq/PrintScrn
alt	keycode  99 = Control_backslash
control	keycode  99 = Control_backslash

plain   keycode  70 = Scroll_Lock
shift	keycode  70 = Show_Memory
control keycode  70 = Show_State
alt	keycode  70 = Show_Registers

keycode 101 = Break			# Ctrl+Break/Pause
keycode 119 = Pause			# Break/Pause

