#
# More function keys and console assignments
# These are not in the main map in order to save keymaps
# Load after the main keymap, or include in the main keymap.
#
keymaps 5,9

control shift keycode  59 = F37
control shift keycode  60 = F38
control shift keycode  61 = F39
control shift keycode  62 = F40
control shift keycode  63 = F41
control shift keycode  64 = F42
control shift keycode  65 = F43
control shift keycode  66 = F44
control shift keycode  67 = F45
control shift keycode  68 = F46
control shift keycode  87 = F47
control shift keycode  88 = F48
#
alt shift keycode  59 = Console_13
alt shift keycode  60 = Console_14
alt shift keycode  61 = Console_15
alt shift keycode  62 = Console_16
alt shift keycode  63 = Console_17
alt shift keycode  64 = Console_18
alt shift keycode  65 = Console_19
alt shift keycode  66 = Console_20
alt shift keycode  67 = Console_21
alt shift keycode  68 = Console_22
alt shift keycode  87 = Console_23
alt shift keycode  88 = Console_24
#
# It would be reasonable to add lines like
#	control alt shift keycode  59 = Console_13
# but that would again require an extra key map, and is not worth it.
#
