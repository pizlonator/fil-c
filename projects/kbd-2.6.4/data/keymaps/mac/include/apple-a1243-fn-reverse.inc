# Apple USB keyboard model A1243 with keypad, aluminium enclosure
#
# Map feature and function keys to correspond to reversed Apple keys.
#
# idVendor     0x05ac Apple, Inc.
# idProduct    0x0221 Aluminum Keyboard (ISO)
#
# The top row keys can be used as function keys (F1-F19). They can also be
# used to control features such as volume adjustments by holding the Fn key.
# This behaviour is reversed in key map "apple-a1243-fn.inc", which is
# Apple default.

keymaps 0,1,4,8,12       # Plain Shift Control Alt Control+Alt

keycode 464 = VoidSymbol # Function key

# Both Alt+F<n> and Control+Alt+F<n> switch to Console_<n>.
keycode 224 = F1  F1  VoidSymbol Console_1  Console_1
keycode 225 = F2  F2  VoidSymbol Console_2  Console_2
keycode 120 = F3  F3  VoidSymbol Console_3  Console_3
keycode 204 = F4  F4  VoidSymbol Console_4  Console_4
keycode 229 = F5  F5  VoidSymbol Console_5  Console_5
keycode 230 = F6  F6  VoidSymbol Console_6  Console_6
keycode 165 = F7  F7  VoidSymbol Console_7  Console_7
keycode 164 = F8  F8  VoidSymbol Console_8  Console_8
keycode 163 = F9  F9  VoidSymbol Console_9  Console_9
keycode 113 = F10 F10 VoidSymbol Console_10 Console_10
keycode 114 = F11 F11 VoidSymbol Console_11 Console_11
keycode 115 = F12 F12 VoidSymbol Console_12 Console_12
keycode 183 = F13 F13 VoidSymbol Console_13 Console_13
keycode 184 = F14 F14 VoidSymbol Console_14 Console_14
keycode 185 = F15 F15 VoidSymbol Console_15 Console_15
keycode 186 = F16 F16 VoidSymbol Console_16 Console_16
keycode 187 = F17 F17 VoidSymbol Console_17 Console_17
keycode 188 = F18 F18 VoidSymbol Console_18 Console_18
keycode 189 = F19 F19 VoidSymbol Console_19 Console_19

# Feature keys
keycode  59 = VoidSymbol # Brightness Down
keycode  60 = VoidSymbol # Brightness Up
keycode  61 = VoidSymbol # Exposé / Mission Control
keycode  62 = VoidSymbol # Dashboard / Launchpad
keycode  63 = VoidSymbol # Unused
keycode  64 = VoidSymbol # Unused
keycode  65 = VoidSymbol # Rewind (Previous Track)
keycode  66 = VoidSymbol # Play / Pause
keycode  67 = VoidSymbol # Fast Forward (Next Track)
keycode  68 = VoidSymbol # Mute
keycode  87 = VoidSymbol # Volume Down
keycode  88 = VoidSymbol # Volume Up
keycode 161 = VoidSymbol # Eject
