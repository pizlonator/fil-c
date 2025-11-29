Title: Keyboard Codes, Groups, And Modifiers

## Functions for manipulating keyboard codes

Key values are the codes which are sent whenever a key is pressed or released.
They are included in the data contained in a key press or release `GdkEvent`.
The complete list of key values can be found in the [`gdk/gdkkeysyms.h`](https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gdk/gdkkeysyms.h) header
file.

Key values are regularly updated from the upstream X.org X11 implementation,
so new values are added regularly. They will be prefixed with `GDK_KEY_` rather
than `XF86XK_` or `XK_` (for older symbols).

Key values can be converted into a string representation using
[`func@Gdk.keyval_name`]. The reverse function, converting a string to a key
value, is provided by [`func@Gdk.keyval_from_name`].

The case of key values can be determined using [`func@Gdk.keyval_is_upper`]
and [`func@Gdk.keyval_is_lower`]. Key values can be converted to upper or lower
case using [`func@Gdk.keyval_to_upper`] and [`func@Gdk.keyval_to_lower`].

When it makes sense, key values can be converted to and from Unicode characters
with [`func@Gdk.keyval_to_unicode`] and [`func@Gdk.unicode_to_keyval`].

## Key groups

At the lowest level, physical keys on the keyboard are represented by
numeric keycodes, and GDK knows how to translate these keycodes into
key values according to the configured keyboard layout and the current
state of the keyboard. In the GDK api, the mapping from keycodes to key
values is available via [method@Gdk.Display.map_keycode], and the reverse
mapping is available via [method@Gdk.Display.map_keyval]. The results of
these functions are returned in [struct@Gdk.KeymapKey] structures.

You can think of a [struct@Gdk.KeymapKey] as a representation of a symbol
printed on a physical keyboard key. That is, it contains three pieces of
information:

1. first, it contains the hardware keycode; this is an identifying number
   for a physical key
1. second, it contains the “level” of the key. The level indicates which
   symbol on the key will be used, in a vertical direction. So on a standard
   US keyboard, the key with the number “1“ on it also has the exclamation
   point (”!”) character on it. The level indicates whether to use the “1”
   or the “!” symbol. The letter keys are considered to have a lowercase
   letter at level 0, and an uppercase letter at level 1, though normally
   only the uppercase letter is printed on the key
1. third, the [struct@Gdk.KeymapKey] contains a group; groups are not used on
   standard US keyboards, but are used in many other countries. On a
   keyboard with groups, there can be 3 or 4 symbols printed on a single
   key. The group indicates movement in a horizontal direction. Usually
   groups are used for two different languages. In group 0, a key might
   have two English characters, and in group 1 it might have two Hebrew
   characters. The Hebrew characters will be printed on the key next to
   the English characters.

When GDK creates a key event in order to deliver a key press or release,
it first converts the current keyboard state into an effective group and
level. This is done via a set of rules that varies widely according to
type of keyboard and user configuration. The input to this translation
consists of the hardware keycode pressed, the active modifiers, and the
active group. It then applies the appropriate rules, and returns the
group/level to be used to index the keymap, along with the modifiers
which did not affect the group and level. i.e. it returns “unconsumed
modifiers.” The keyboard group may differ from the effective group used
for lookups because some keys don't have multiple groups - e.g. the
<kbd>Enter</kbd> key is always in group 0 regardless of keyboard state.

The results of the translation, including the keyval, are all included
in the key event and can be obtained via [class@Gdk.KeyEvent] getters.

### Consumed modifiers

The `consumed_modifiers` in a key event are modifiers that should be masked
out from `@state` when comparing this key press to a hot key. For instance,
on a US keyboard, the `plus` symbol is shifted, so when comparing a key
press to a `<Control>plus` accelerator `<Shift>` should be masked out.

```c
// We want to ignore irrelevant modifiers like ScrollLock
#define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK)
state = gdk_event_get_modifier_state (event);
gdk_keymap_translate_keyboard_state (keymap,
                                     gdk_key_event_get_keycode (event),
                                     state,
                                     gdk_key_event_get_group (event),
                                     &keyval, NULL, NULL, &consumed);
if (keyval == GDK_PLUS &&
    (state & ~consumed & ALL_ACCELS_MASK) == GDK_CONTROL_MASK)
  // Control was pressed
```

An older interpretation of `consumed_modifiers` was that it contained
all modifiers that might affect the translation of the key; this allowed
accelerators to be stored with irrelevant consumed modifiers, by doing:

```c
// XXX Don’t do this XXX
if (keyval == accel_keyval &&
    (state & ~consumed & ALL_ACCELS_MASK) == (accel_mods & ~consumed))
  // Accelerator was pressed
```

However, this did not work if multi-modifier combinations were used in the
keymap, since, for instance, `<Control>` would be masked out even if only
`<Control><Alt>` was used in the keymap. To support this usage as well as
well as possible, all single modifier combinations that could affect the key
for any combination of modifiers will be returned in `consumed_modifiers`;
multi-modifier combinations are returned only when actually found in `state`.
When you store accelerators, you should always store them with consumed
modifiers removed. Store `<Control>plus`, not `<Control><Shift>plus`.
