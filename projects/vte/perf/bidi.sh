# BiDi support in VTE is implemented according to
# https://terminal-wg.pages.freedesktop.org/bidi/
#
# The following aliases allow you to quickly change the BiDi mode.
#
# These are not meant to be standardized command names. Customize them
# according to your liking, and don't rely on them being available for others.

# Bi-Directional Support Mode (BDSM):
# In implicit mode the terminal emulator performs BiDi.
# In explicit mode the terminal emulator lays out the characters in linear
# order, and the application running inside is expected to do BiDi.
alias implicit='printf "\e[8h"'
alias explicit='printf "\e[8l"'

# Select Character Path (SCP):
# Defines the paragraph direction for explicit mode, and for implicit mode
# without autodetection. Defines the fallback paragraph direction (in case
# autodetection fails) for implicit mode with autodetection.
alias ltr='printf "\e[1 k"'
alias rtl='printf "\e[2 k"'
# alias defaultdir='printf "\e[ k"' # currently the same as ltr

# Autodetection:
# Whether in implicit mode the paragraph direction is autodetected (possibly
# falling back to the value set by SCP), or taken strictly from SCP.
alias   autodir='printf "\e[?2501h"'
alias noautodir='printf "\e[?2501l"'

# Box mirroring:
# Whether box drawing characters are added to the set of mirrorable glyphs.
alias   boxmirror='printf "\e[?2500h"'
alias noboxmirror='printf "\e[?2500l"'

# Keyboard arrow swapping:
# Whether the left and right arrows of the keyboard are swapped whenever the
# cursor stands within an RTL paragraph.
alias   kbdswap='printf "\e[?1243h"'
alias nokbdswap='printf "\e[?1243l"'
