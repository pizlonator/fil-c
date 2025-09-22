# Apple USB keyboard model A1048 with keypad, plastic enclosure
#
# idVendor     0x05ac Apple, Inc.
# idProduct    0x020c Extended Keyboard
#
# Base map without localization specific keys.

keymaps 0,1,4,8,9 # Plain Shift Control Alt Shift+Alt

keycode   1 = Escape
keycode  14 = Delete Delete Delete Delete       Delete
keycode  15 = Tab    Tab    Tab    Tab          Tab
keycode  28 = Return Return Return Return       Return
keycode  57 = space  space  space  nobreakspace nobreakspace

# Modifier keys
# Note: Apple keyboards normally do not distinguish
# between left and right modifiers keys.
keycode  29 = Control    # Left Control
keycode  97 = Control    # Right Control
keycode  42 = Shift      # Left Shift
keycode  54 = Shift      # Right Shift
keycode  56 = Alt        # Left Alt
keycode 100 = Alt        # Right Alt
keycode 125 = VoidSymbol # Left Command
keycode 126 = VoidSymbol # Right Command
keycode  58 = Caps_Lock

# Keypad keys
keycode  69 = U+2327 # Clear
keycode 117 = equal
keycode  98 = slash
keycode  55 = asterisk
keycode  74 = minus
keycode  78 = plus
keycode  94 = Return

keycode  82 = zero
keycode  79 = one
keycode  80 = two
keycode  81 = three
keycode  75 = four
keycode  76 = five
keycode  77 = six
keycode  71 = seven
keycode  72 = eight
keycode  73 = nine
keycode  83 = period

# Edit keys
keycode 110 = Help
keycode 111 = Remove
keycode 102 = Home
keycode 107 = End
keycode 104 = PageUp
keycode 109 = PageDown

keycode 105 = Left
keycode 103 = Up
keycode 108 = Down
keycode 106 = Right

# Function keys, both Alt+F<n> and Control+Alt+F<n> switch to Console_<n>
keycode  59 = F1  F1  VoidSymbol Console_1  Console_1
keycode  60 = F2  F2  VoidSymbol Console_2  Console_2
keycode  61 = F3  F3  VoidSymbol Console_3  Console_3
keycode  62 = F4  F4  VoidSymbol Console_4  Console_4
keycode  63 = F5  F5  VoidSymbol Console_5  Console_5
keycode  64 = F6  F6  VoidSymbol Console_6  Console_6
keycode  65 = F7  F7  VoidSymbol Console_7  Console_7
keycode  66 = F8  F8  VoidSymbol Console_8  Console_8
keycode  67 = F9  F9  VoidSymbol Console_9  Console_9
keycode  68 = F10 F10 VoidSymbol Console_10 Console_10
keycode  87 = F11 F11 VoidSymbol Console_11 Console_11
keycode  88 = F12 F12 VoidSymbol Console_12 Console_12
keycode 183 = F13 F13 VoidSymbol Console_13 Console_13
keycode 184 = F14 F14 VoidSymbol Console_14 Console_14
keycode 185 = F15 F15 VoidSymbol Console_15 Console_15
keycode 186 = F16 F16 VoidSymbol Console_16 Console_16

# Feature keys
keycode 113 = VoidSymbol # Mute
keycode 114 = VoidSymbol # Volume Down
keycode 115 = VoidSymbol # Volume Up
keycode 161 = VoidSymbol # Eject
