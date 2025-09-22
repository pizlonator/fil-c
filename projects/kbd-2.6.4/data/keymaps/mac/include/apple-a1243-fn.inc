# Apple USB keyboard model A1243 with keypad, aluminium enclosure
#
# Map feature and function keys to correspond to standard Apple keys.
#
# idVendor     0x05ac Apple, Inc.
# idProduct    0x0221 Aluminum Keyboard (ISO)
#
# The top row keys can be used to control features such as volume
# adjustments. They can also be used as function keys (F1-F19) by holding
# the Fn key. Apple default is to perform the feature indicated by the
# icon printed on the key. This behaviour is reversed in key map
# "apple-a1243-fn-reverse.inc".

keymaps 0,1,4,8,12       # Plain Shift Control Alt Control+Alt

keycode 464 = VoidSymbol # Function key

# Both Fn+Alt+F<n> and Fn+Control+Alt+F<n> switch to Console_<n>.
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
keycode 187 = F17 F17 VoidSymbol Console_17 Console_17
keycode 188 = F18 F18 VoidSymbol Console_18 Console_18
keycode 189 = F19 F19 VoidSymbol Console_19 Console_19

# Feature keys
keycode 224 = VoidSymbol # Brightness Down
keycode 225 = VoidSymbol # Brightness Up
keycode 120 = VoidSymbol # Exposé / Mission Control
keycode 204 = VoidSymbol # Dashboard / Launchpad
keycode 229 = VoidSymbol # Unused
keycode 230 = VoidSymbol # Unused
keycode 165 = VoidSymbol # Rewind (Previous Track)
keycode 164 = VoidSymbol # Play / Pause
keycode 163 = VoidSymbol # Fast Forward (Next Track)
keycode 113 = VoidSymbol # Mute
keycode 114 = VoidSymbol # Volume Down
keycode 115 = VoidSymbol # Volume Up
keycode 161 = VoidSymbol # Eject
