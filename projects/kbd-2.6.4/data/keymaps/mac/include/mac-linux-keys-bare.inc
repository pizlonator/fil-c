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
plain keycode 122 = F1
plain keycode 120 = F2
plain keycode  99 = F3
plain keycode 118 = F4
plain keycode  96 = F5
plain keycode  97 = F6
plain keycode  98 = F7
plain keycode 100 = F8
plain keycode 101 = F9
plain keycode 109 = F10
plain keycode 103 = F11
plain keycode 111 = F12
shift keycode 122 = F13
shift keycode 120 = F14
shift keycode  99 = F15
shift keycode 118 = F16
shift keycode  96 = F17
shift keycode  97 = F18
shift keycode  98 = F19
shift keycode 100 = F20
shift keycode 101 = F21
shift keycode 109 = F22
shift keycode 103 = F23
shift keycode 111 = F24
control keycode 122 = F25
control keycode 120 = F26
control keycode  99 = F27
control keycode 118 = F28
control keycode  96 = F29
control keycode  97 = F30
control keycode  98 = F31
control keycode 100 = F32
control keycode 101 = F33
control keycode 109 = F34
control keycode 103 = F35
control keycode 111 = F36
#
# The canonical assignment for F37-F48 uses
#	control shift keycode 122 = F37
# etc. If you want that, load linux-keys-extd.inc
#
# Both Alt+Fn and Control+Alt+Fn switch to Console n
# (in order to please Linux / dosemu / X).
# Here Consoles 1..12. For 13..24 load linux-keys-extd.inc
#
alt keycode 122 = Console_1
alt keycode 120 = Console_2
alt keycode  99 = Console_3
alt keycode 118 = Console_4
alt keycode  96 = Console_5
alt keycode  97 = Console_6
alt keycode  98 = Console_7
alt keycode 100 = Console_8
alt keycode 101 = Console_9
alt keycode 109 = Console_10
alt keycode 103 = Console_11
alt keycode 111 = Console_12
control alt keycode 122 = Console_1
control alt keycode 120 = Console_2
control alt keycode  99 = Console_3
control alt keycode 118 = Console_4
control alt keycode  96 = Console_5
control alt keycode  97 = Console_6
control alt keycode  98 = Console_7
control alt keycode 100 = Console_8
control alt keycode 101 = Console_9
control alt keycode 109 = Console_10
control alt keycode 103 = Console_11
control alt keycode 111 = Console_12

#
# Keypad keys
#
keycode  71 = Num_Lock
keycode  75 = KP_Divide
keycode  67 = KP_Multiply
keycode  78 = KP_Subtract
keycode  69 = KP_Add
keycode  76 = KP_Enter
keycode  81 = equal
#
keycode  89 = KP_7
keycode  91 = KP_8
keycode  92 = KP_9
keycode  86 = KP_4
keycode  87 = KP_5
keycode  88 = KP_6
keycode  83 = KP_1
keycode  84 = KP_2
keycode  85 = KP_3
keycode  82 = KP_0
#
keycode  65 = KP_Period			# or KP_Comma
control alt keycode  65 = Boot
#
alt keycode  89 = Ascii_7
alt keycode  91 = Ascii_8
alt keycode  92 = Ascii_9
alt keycode  86 = Ascii_4
alt keycode  87 = Ascii_5
alt keycode  88 = Ascii_6
alt keycode  83 = Ascii_1
alt keycode  84 = Ascii_2
alt keycode  85 = Ascii_3
alt keycode  82 = Ascii_0
#
# The four cursor keys
#
keycode  62 = Up
keycode  59 = Left
keycode  60 = Right
keycode  61 = Down
alt keycode  62 = KeyboardSignal
alt keycode  59 = Decr_Console
alt keycode  60 = Incr_Console
#
# The six edit keys
#
keycode 114 = Insert
keycode 115 = Home		# Find
keycode 116 = PageUp		# Prior
keycode 117 = Remove
keycode 119 = End		# Select
keycode 121 = PageDown		# Next
shift keycode 116 = Scroll_Backward
shift keycode 121 = Scroll_Forward
control	alt keycode 117 = Boot
#
# The three system keys
#
# what is that? olh@suse.de
#keycode  84 = Last_Console		# Alt+SysRq/PrintScrn
#
# what is that? olh@suse.de
#keycode  99 = Control_backslash		# SysRq/PrintScrn

plain   keycode 107 = Scroll_Lock
shift	keycode 107 = Show_Memory
control keycode 107 = Show_State
alt	keycode 107 = Show_Registers

control keycode 113 = Break		# Ctrl+Break/Pause
keycode 113 = Pause			# Break/Pause

# generic keys for all keyboards
keycode  36 = Return
        alt     keycode  36 = Meta_Control_m
keycode  48 = Tab
	shift	keycode  48 = Meta_Tab
	alt keycode  48 = Meta_Tab
keycode  49 = space space space nul
keycode  51 = Delete  Remove
	alt	keycode  51 = Meta_Delete
	shift 	alt	keycode  51 = Meta_Delete
        control	keycode  51 = Remove
keycode  53 = Escape
        alt keycode  53 =        Meta_Escape
        shift   alt keycode  53 =        Meta_Escape
keycode  54 = Control
keycode  55 = Alt             # Command/Apple key
keycode  56 = Shift
keycode  57 = Caps_Lock
keycode  58 = AltGr           # Alt/Option key
