Neo 2
=====

This directory includes various variants of Neo 2 (Often only referred to as Neo) - an Ergonomic Keyboard Layout, optimized for the German Language.
They have the following in common:

- Optimizations regarding Letter Frequency as well as Bi– and Trigrams given by the German Language (on Layer 1 and 2)
- Directly underneath your finger tips lie frequently used chars for Programming Languages (on Layer 3)
- Navigation Keys and numbers also lie directly under your finger tips! (on Layer 4)
- New chars can be entered, like: „ , ” , « , » , ∫ , √ , α , β , … (and massively more)

Links
=====

German website: https://neo-layout.org/


Notes:
=====
• Not using KP_-keysyms on fourth level as NumLock is not controllable
  (i.e. can't be forced to on whenever the keymaps are loaded).
  On console there is little need for KP_-keysyms.
• Mod4-Lock is not implemented (technical difficulties, not enough levels)
• Dead keys only when there exists a corresponding dead keysym on console
  (i.e circumflex, gravis, cedille, trema, akut and tilde) otherwise the
  undead character is produced.
• Using default compose as for now. Maximum number allowed compose
  sequences seems to be 256, not enough for the standard Neo2 sequences.
• Wrong Caps_Lock behaviour with ssharp (not returning to lower case when
  using Caps_Lock and Shift)
  See: https://bugzilla.kernel.org/show_bug.cgi?id=7063
