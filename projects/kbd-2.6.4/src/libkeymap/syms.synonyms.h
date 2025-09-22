struct syn const synonyms[] = {
	{ "Control_h", "BackSpace" },
	{ "Control_i", "Tab" },
	{ "Control_j", "Linefeed" },
	{ "Home", "Find" },
	/* Unfortunately Delete already denotes ASCII 0177 */
	/* { "Delete", "Remove" }, */
	{ "End", "Select" },
	{ "PageUp", "Prior" },
	{ "PageDown", "Next" },
	{ "multiplication", "multiply" },
	{ "pound", "sterling" },
	{ "pilcrow", "paragraph" },
	{ "Oslash", "Ooblique" },
	{ "Shift_L", "ShiftL" },
	{ "Shift_R", "ShiftR" },
	{ "Control_L", "CtrlL" },
	{ "Control_R", "CtrlR" },
	{ "AltL", "Alt" },
	{ "AltR", "AltGr" },
	{ "Alt_L", "Alt" },
	{ "Alt_R", "AltGr" },
	{ "AltGr_L", "Alt" },
	{ "AltGr_R", "AltGr" },
	{ "AltLLock", "Alt_Lock" },
	{ "AltRLock", "AltGr_Lock" },
	{ "SCtrl", "SControl" },
	{ "Spawn_Console", "KeyboardSignal" },
	{ "Uncaps_Shift", "CapsShift" },
	/* the names of the Greek letters are spelled differently
   in the iso-8859-7 and the Unicode standards */
	{ "lambda", "lamda" },
	{ "Lambda", "Lamda" },
	{ "xi", "ksi" },
	{ "Xi", "Ksi" },
	{ "chi", "khi" },
	{ "Chi", "Khi" },
	/* diacriticals */
	{ "tilde", "asciitilde" },
	{ "circumflex", "asciicircum" },
	/* dead_ogonek, dead_caron, dead_breve and dead_doubleacute didn't exist in
	 * the kernel, so they were introduced as aliases for other dead keys. For
	 * backward compatilibty they remain as aliases and the real names are
	 * prefixed with dead_k (kernel).
	 */
	{ "dead_ogonek", "dead_cedilla" },
	{ "dead_caron", "dead_circumflex" },
	{ "dead_breve", "dead_tilde" },
	{ "dead_doubleacute", "dead_tilde" },
	/* turkish */
	{ "Idotabove", "Iabovedot" },
	{ "dotlessi", "idotless" },
	/* cyrillic */
	{ "no-break_space", "nobreakspace" },
	{ "paragraph_sign", "section" },
	{ "soft_hyphen", "hyphen" },
	{ "bielorussian_cyrillic_capital_letter_i", "ukrainian_cyrillic_capital_letter_i" },
	{ "cyrillic_capital_letter_kha", "cyrillic_capital_letter_ha" },
	{ "cyrillic_capital_letter_ge", "cyrillic_capital_letter_ghe" },
	{ "cyrillic_capital_letter_ia", "cyrillic_capital_letter_ya" },
	{ "cyrillic_capital_letter_iu", "cyrillic_capital_letter_yu" },
	{ "cyrillic_capital_letter_yeri", "cyrillic_capital_letter_yeru" },
	{ "cyrillic_capital_letter_reversed_e", "cyrillic_capital_letter_e" },
	{ "cyrillic_capital_letter_ii", "cyrillic_capital_letter_i" },
	{ "cyrillic_capital_letter_short_ii", "cyrillic_capital_letter_short_i" },
	{ "bielorussian_cyrillic_small_letter_i", "ukrainian_cyrillic_small_letter_i" },
	{ "cyrillic_small_letter_kha", "cyrillic_small_letter_ha" },
	{ "cyrillic_small_letter_ge", "cyrillic_small_letter_ghe" },
	{ "cyrillic_small_letter_ia", "cyrillic_small_letter_ya" },
	{ "cyrillic_small_letter_iu", "cyrillic_small_letter_yu" },
	{ "cyrillic_small_letter_yeri", "cyrillic_small_letter_yeru" },
	{ "cyrillic_small_letter_reversed_e", "cyrillic_small_letter_e" },
	{ "cyrillic_small_letter_ii", "cyrillic_small_letter_i" },
	{ "cyrillic_small_letter_short_ii", "cyrillic_small_letter_short_i" },
	{ "ukrainian_cyrillic_small_letter_ghe_with_upturn", "cyrillic_small_letter_ghe_with_upturn" },
	{ "ukrainian_cyrillic_capital_letter_ghe_with_upturn", "cyrillic_capital_letter_ghe_with_upturn" },
	/* iso-8859-7 */
	{ "rightanglequote", "guillemotright" }
};
