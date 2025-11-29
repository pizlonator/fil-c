/*
 * Copyright (c) 2021 Philip Zander
 * Copyright (c) 2018 Microsoft
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* NOTE: When compiling the 32-bit version of the library, in addition to being
 * compiled as a regular source file, this file is also included by
 * gdkkeys-win32-impl-wow64.c to generate an alternate version of the code
 * intended for running on a 64-bit kernel. Because of the way keyboard layout
 * DLLs work on Windows, we have to generate two versions and decide at runtime
 * which code path to execute. You can read more about the specifics below, in
 * the section about KBD_LONG_POINTER. */

#include "gdkkeys-win32.h"

#ifndef GDK_WIN32_COMPILE_FOR_WOW64
  #define GDK_WIN32_COMPILE_FOR_WOW64 0
#endif
 
/* This is our equivalent of the KBD_LONG_POINTER macro in Microsoft's kbd.h.
 *
 * A KBD_LONG_POINTER represents a pointer native to the *host*.
 * I.e. 32 bits on 32-bit Windows and 64 bits on 64-bit Windows.
 *
 * This is *not* the same as the the bitness of the application, since it is
 * possible to execute 32-bit binaries on either a 32-bit *or* a 64-bit host.
 * On a 64-bit host, KBD_LONG_PTR will be 64-bits, even if the application 
 * itself is 32-bit. (Whereas on a 32-bit host, it will be 32-bit.)
 *
 * For clarity, here is an overview of the bit-size of KBD_LONG_POINTER on all
 * possible host & app combinations:
 *
 *      Host  32  64
 *  App +-----------
 *  32  |     32  64
 *  64  |     -   64
 *
 * In the official MS headers, KBD_LONG_POINTER is implemented via a macro
 * which expands to the attribute `__ptr64` if the keyboard driver is 
 * compiled for a 64 bit host. Unfortunately, `__ptr64` is only 
 * supported by MSVC. We use a union here as a workaround.
 *
 * For all KBD_LONG_POINTERs, we define an alias starting with "KLP".
 * Our naming schema (inspired by the Windows headers) is thus the following:
 * - FOO:    The type FOO itself
 * - PFOO:   Regular pointer to the type FOO
 * - KLPFOO: Keyboard Long Pointer to the type FOO
 */

#if GDK_WIN32_COMPILE_FOR_WOW64
  #define DEFINE_KBD_LONG_POINTER(type)          \
    typedef union {                              \
      P##type ptr;                               \
      UINT64  _align;                            \
    } KLP##type
#else
  #define DEFINE_KBD_LONG_POINTER(type)          \
    typedef union {                              \
      P##type ptr;                               \
    } KLP##type
#endif

DEFINE_KBD_LONG_POINTER (USHORT);
DEFINE_KBD_LONG_POINTER (VOID);

/* Driver definitions
 * See
 *   https://github.com/microsoft/windows-rs/blob/0.28.0/crates/deps/sys/src/Windows/Win32/UI/Input/KeyboardAndMouse/mod.rs
 *
 * For more information on how these structures work, see also:
 *   https://github.com/microsoft/Windows-driver-samples/tree/f0adcda012820b1cd44a8b3a1953baf478029738/input/layout
 */

typedef struct
{
  BYTE                        Vk;
  BYTE                        ModBits;
} VK_TO_BIT, *PVK_TO_BIT;

DEFINE_KBD_LONG_POINTER (VK_TO_BIT);

typedef struct
{
  KLPVK_TO_BIT                pVkToBit;
  WORD                        wMaxModBits;
  BYTE                        ModNumber[1];
} MODIFIERS, *PMODIFIERS;

DEFINE_KBD_LONG_POINTER (MODIFIERS);

typedef struct
{
  BYTE                        Vsc;
  USHORT                      Vk;
} VSC_VK, *PVSC_VK;

DEFINE_KBD_LONG_POINTER (VSC_VK);

typedef struct
{
  BYTE                        Vk;
  BYTE                        Vsc;
} VK_VSC, *PVK_VSC;

DEFINE_KBD_LONG_POINTER (VK_VSC);

typedef struct
{
  BYTE                        VirtualKey;
  BYTE                        Attributes;
  WCHAR                       wch[1];
} VK_TO_WCHARS, *PVK_TO_WCHARS;

DEFINE_KBD_LONG_POINTER (VK_TO_WCHARS);

typedef struct
{
  KLPVK_TO_WCHARS             pVkToWchars;
  BYTE                        nModifications;
  BYTE                        cbSize;
} VK_TO_WCHAR_TABLE, *PVK_TO_WCHAR_TABLE;

DEFINE_KBD_LONG_POINTER (VK_TO_WCHAR_TABLE);

typedef struct
{
  DWORD                       dwBoth;
  WCHAR                       wchComposed;
  USHORT                      uFlags;
} DEADKEY, *PDEADKEY;

DEFINE_KBD_LONG_POINTER (DEADKEY);

typedef struct
{
  KLPMODIFIERS                pCharModifiers;
  KLPVK_TO_WCHAR_TABLE        pVkToWcharTable;
  KLPDEADKEY                  pDeadKey;
  KLPVOID                     pKeyNames;
  KLPVOID                     pKeyNamesExt;
  KLPVOID                     pKeyNamesDead;
  KLPUSHORT                   pusVSCtoVK;
  BYTE                        bMaxVSCtoVK;
  KLPVSC_VK                   pVSCtoVK_E0;
  KLPVSC_VK                   pVSCtoVK_E1;
  DWORD                       fLocaleFlags;
  BYTE                        nLgMaxd;
  BYTE                        cbLgEntry;
  KLPVOID                     pLigature;
} KBDTABLES, *PKBDTABLES;

DEFINE_KBD_LONG_POINTER (KBDTABLES);

/* End of declarations */

static BYTE
keystate_to_modbits (GdkWin32KeymapLayoutInfo *info,
                     const BYTE                keystate[256])
{
  PKBDTABLES tables = (PKBDTABLES) info->tables;
  PVK_TO_BIT vk_to_bit;
  BYTE       result = 0;
  int        i;

  if (tables == NULL)
    return 0;

  vk_to_bit = tables->pCharModifiers.ptr->pVkToBit.ptr;

  for (i = 0; vk_to_bit[i].Vk != 0; ++i)
    if (keystate[vk_to_bit[i].Vk] & 0x80)
      result |= vk_to_bit[i].ModBits;

  return result;
}

static BYTE
modbits_to_level (GdkWin32KeymapLayoutInfo *info,
                  BYTE                      modbits)
{
  PKBDTABLES tables = (PKBDTABLES) info->tables;
  PMODIFIERS modifiers;

  if (tables == NULL)
    return 0;

  modifiers = tables->pCharModifiers.ptr;
  if (modbits > modifiers->wMaxModBits)
    return 0;

  return modifiers->ModNumber[modbits];
}

#define POPCOUNT(b) (!!(b & 0x01) + !!(b & 0x02) + !!(b & 0x04) + !!(b & 0x08) + \
                     !!(b & 0x10) + !!(b & 0x20) + !!(b & 0x40) + !!(b & 0x80))

/*
 * vk_to_char_fuzzy:
 *
 * For a given key and modifier state, return the best-fit character and the
 * modifiers used to produce it. Note that not all modifiers need to be used,
 * because some modifier combination aren't actually mapped in the keyboard
 * layout (for example the Ctrl key typically has no effect, unless used in
 * combination with Alt). Such modifiers will not be consumed.
 *
 * 'Best-fit' means 'consume as many modifiers as possibe'.
 *
 * For example (assuming a neutral lock state):
 *
 * - a                -> 'a', consumed_mod_bits: []
 * - Shift + a        -> 'A', consumed_mod_bits: [Shift]
 * - Ctrl + a         -> 'a', consumed_mod_bits: []
 * - Ctrl + Shift + a -> 'A', consumed_mod_bits: [Shift]
 *
 * If capslock is active, the result could be:
 *
 * - a                -> 'A', consumed_mod_bits: [Shift]
 * - Shift + a        -> 'a', consumed_mod_bits: []
 * - Ctrl + a         -> 'a', consumed_mod_bits: []
 * - Ctrl + Shift + a -> 'A', consumed_mod_bits: [Shift]
 *
 * The held down modifiers are supplied in `mod_bits` as a bitmask of KBDSHIFT,
 * KBDCTRL, KBDALT etc.
 *
 * The toggled modifiers are supplied in `lock_state` as a bitmask of CAPLOK and KANALOK.
 *
 * If the key combination results in a dead key, `is_dead` will be set to TRUE,
 * otherwise it will be set to FALSE.
 */
static WCHAR
vk_to_char_fuzzy (GdkWin32KeymapLayoutInfo *info,
                  BYTE                      mod_bits,
                  BYTE                      lock_bits,
                  BYTE                     *consumed_mod_bits,
                  gboolean                 *is_dead,
                  BYTE                      vk)
{
  PKBDTABLES         tables = (PKBDTABLES) info->tables;

  PVK_TO_WCHAR_TABLE wch_tables;
  PVK_TO_WCHAR_TABLE wch_table;
  PVK_TO_WCHARS      entry;

  int                table_index;
  int                entry_index;
  int                n_levels;
  int                entry_size;

  /* Initialize with defaults */
  if (consumed_mod_bits)
    *consumed_mod_bits = 0;
  if (is_dead)
    *is_dead = FALSE;

  if (tables == NULL)
    return WCH_NONE;

  wch_tables = tables->pVkToWcharTable.ptr;

  table_index = info->vk_lookup_table[vk].table;
  entry_index = info->vk_lookup_table[vk].index;

  if (table_index == -1 || entry_index == -1)
    return WCH_NONE;

  wch_table = &wch_tables[table_index];

  n_levels = wch_table->nModifications;
  entry_size = wch_table->cbSize;

  entry = (PVK_TO_WCHARS) ((PBYTE) wch_table->pVkToWchars.ptr
                           + entry_size*entry_index);

  if (entry->VirtualKey == vk)
    {
      gboolean have_sgcaps    = FALSE;
      WCHAR    best_char      = WCH_NONE;
      BYTE     best_modifiers = 0;
      int      best_score     = -1;
      gboolean best_is_dead   = FALSE;
      int      level;

      /* Take toggled keys into account. For example, capslock normally inverts the
       * state of KBDSHIFT (with some exceptions). */

      /* Key supporting capslock */
      if ((entry->Attributes & CAPLOK) &&
          /* Ignore capslock if any modifiers other than shift are pressed.
           * E.g. on the German layout, CapsLock + AltGr + q is the same as
           * AltGr + q ('@'), but NOT the same as Shift + AltGr + q (not mapped). */
          !(mod_bits & ~KBDSHIFT) &&
          (lock_bits & CAPLOK))
        mod_bits ^= KBDSHIFT;

      /* Key supporting combination of capslock + altgr */
      if ((entry->Attributes & CAPLOKALTGR) &&
          (mod_bits & KBDALTGR) &&
          (lock_bits & CAPLOK))
        mod_bits ^= KBDSHIFT;

      /* In the Swiss German layout, CapsLock + key is different from Shift + key
       * for some keys. For such keys, the characters for active capslock are
       * in the next entry. */
      if ((entry->Attributes & SGCAPS) &&
          (lock_bits & CAPLOK))
        have_sgcaps = TRUE;

      /* I'm not totally sure how kanalok behaves, for now I assume that there
       * aren't any special cases. */
      if ((entry->Attributes & KANALOK) &&
          (lock_bits & KANALOK))
        mod_bits ^= KBDKANA;

      /* We try to find the entry with the most matching modifiers */
      for (level = 0; level < n_levels; ++level)
        {
          BYTE     candidate_modbits = info->level_to_modbits[level];
          gboolean candidate_is_dead = FALSE;
          WCHAR    c;
          int      score;

          if (candidate_modbits & ~mod_bits)
            continue;

          /* Some keys have bogus mappings for the control key, e.g.  Ctrl +
           * Backspace = Delete, Ctrl + [ = 0x1B or even  Ctrl + Shift + 6 =
           * 0x1E on a US keyboard. So we have to ignore all cases of
           * Ctrl that aren't part of AltGr.
           */
          if ((candidate_modbits & KBDCTRL) && !(candidate_modbits & KBDALT))
            continue;

          c = entry->wch[level];
          if (c == WCH_DEAD || have_sgcaps)
            {
              /* Next entry contains the undead/capslocked keys */
              PVK_TO_WCHARS next_entry;
              next_entry = (PVK_TO_WCHARS) ((PBYTE) wch_table->pVkToWchars.ptr
                                            + entry_size * (entry_index + 1));
              c = next_entry->wch[level];
              candidate_is_dead = TRUE;
            }

          if (c == WCH_DEAD || c == WCH_LGTR || c == WCH_NONE)
            continue;

          score = POPCOUNT (candidate_modbits & mod_bits);
          if (score > best_score)
            {
              best_score = score;
              best_char = c;
              best_modifiers = candidate_modbits;
              best_is_dead = candidate_is_dead;
            }
        }

      if (consumed_mod_bits)
        *consumed_mod_bits = best_modifiers;

      if (is_dead)
        *is_dead = best_is_dead;

      return best_char;
    }

  return WCH_NONE;
}

static void
init_vk_lookup_table (GdkWin32KeymapLayoutInfo *info)
{
  PKBDTABLES         tables = (PKBDTABLES) info->tables;
  PVK_TO_WCHAR_TABLE wch_tables;
  PMODIFIERS         modifiers;
  int                i, vk, table_idx;

  g_return_if_fail (tables != NULL);

  wch_tables = tables->pVkToWcharTable.ptr;

  /* Initialize empty table */
  memset (info->vk_lookup_table, -1, sizeof (info->vk_lookup_table));

  /* Initialize level -> modbits lookup table */
  memset (info->level_to_modbits, 0, sizeof(info->level_to_modbits));
  info->max_level = 0;

  modifiers = tables->pCharModifiers.ptr;
  for (i = 0; i <= modifiers->wMaxModBits; ++i)
    {
      if (modifiers->ModNumber[i] != SHFT_INVALID &&
          modifiers->ModNumber[i] != 0 /* Workaround for buggy layouts*/)
        {
          if (modifiers->ModNumber[i] > info->max_level)
              info->max_level = modifiers->ModNumber[i];
          info->level_to_modbits[modifiers->ModNumber[i]] = i;
        }
    }

  info->max_modbit_value = modifiers->wMaxModBits;

  /* For convenience, we add 256 identity-mapped entries corresponding to the VKs.
   * This allows us to return a pointer to them from the `gdk_keysym_to_key_entry`
   * function.
   */

  for (vk = 0; vk < 256; ++vk)
    {
      GdkWin32KeymapKeyEntry key_entry = {0};
      key_entry.vk       = vk;
      key_entry.mod_bits = 0;
      key_entry.next     = -1;
      g_array_append_val (info->key_entries, key_entry);
    }

  /* Special entry for ISO_Left_Tab */
  {
    GdkWin32KeymapKeyEntry key_entry = {0};
    key_entry.vk       = VK_TAB;
    key_entry.mod_bits = KBDSHIFT;
    key_entry.next     = -1;
    g_array_append_val (info->key_entries, key_entry);
  }

  /* Initialize generic vk <-> char tables */

  for (table_idx = 0; ; ++table_idx)
    {
      PVK_TO_WCHAR_TABLE wch_table = &wch_tables[table_idx];
      int entry_size;
      int n_levels;
      int entry_idx;

      if (wch_table->pVkToWchars.ptr == NULL)
        break;

      entry_size = wch_table->cbSize;
      n_levels   = wch_table->nModifications;

      for (entry_idx = 0; ; ++entry_idx)
        {
          PVK_TO_WCHARS entry;
          int           level;

          entry = (PVK_TO_WCHARS) ((PBYTE) wch_table->pVkToWchars.ptr
				   + entry_size * entry_idx);

          if (entry->VirtualKey == 0)
            break;

          /* Lookup table to find entry for a VK in O(1). */

          /* Only add the first entry, as some layouts (Swiss German) contain
           * multiple successive entries for the same VK (SGCAPS). */
          if (info->vk_lookup_table[entry->VirtualKey].table < 0)
            {
              info->vk_lookup_table[entry->VirtualKey].table = table_idx;
              info->vk_lookup_table[entry->VirtualKey].index = entry_idx;
            }

          /* Create reverse lookup entries to find a VK+modifier combinations
           * that results in a given character. */
          for (level = 0; level < n_levels; ++level)
            {
              GdkWin32KeymapKeyEntry key_entry = {0};
              WCHAR                  c         = entry->wch[level];
              int                    inserted_idx;
              gintptr                next_idx;

              key_entry.vk       = entry->VirtualKey;
              key_entry.mod_bits = info->level_to_modbits[level];

              /* There can be multiple combinations that produce the same character.
               * We store all of them in a linked list.
               * Check if we already have an entry for the character, so we can chain
               * them together. */
              if (g_hash_table_lookup_extended (info->reverse_lookup_table,
                                                GINT_TO_POINTER (c),
                                                NULL, (gpointer*)&next_idx))
                {
                  key_entry.next = next_idx;
                }
              else
                {
                  key_entry.next = -1;
                }

              /* We store the KeyEntry in an array. In the hash table we only store
               * the index. */

              g_array_append_val (info->key_entries, key_entry);
              inserted_idx = info->key_entries->len - 1;

              g_hash_table_insert (info->reverse_lookup_table,
                                   GINT_TO_POINTER (c),
                                   GINT_TO_POINTER (inserted_idx));
            }
        }
    }
}

static gboolean
load_layout_dll (const char               *dll,
                 GdkWin32KeymapLayoutInfo *info)
{
    typedef KLPKBDTABLES (*KbdLayerDescriptor)(VOID);

    HMODULE             lib;
    KbdLayerDescriptor  func;
    KLPKBDTABLES        tables;

    g_return_val_if_fail (dll != NULL, FALSE);

    lib = LoadLibraryA (dll);
    if (lib == NULL)
      goto fail1;

    func = (KbdLayerDescriptor) GetProcAddress (lib, "KbdLayerDescriptor");
    if (func == NULL)
      goto fail2;

    tables = func();

    info->lib = lib;
    info->tables = (PKBDTABLES) tables.ptr;

    return TRUE;

fail2:
    FreeLibrary (lib);
fail1:
    return FALSE;
}

#if GDK_WIN32_COMPILE_FOR_WOW64
  #define GDK_WIN32_KEYMAP_IMPL_NAME gdkwin32_keymap_impl_wow64
#else
  #define GDK_WIN32_KEYMAP_IMPL_NAME gdkwin32_keymap_impl
#endif

const GdkWin32KeymapImpl GDK_WIN32_KEYMAP_IMPL_NAME =
  {
    load_layout_dll,
    init_vk_lookup_table,
    keystate_to_modbits,
    modbits_to_level,
    vk_to_char_fuzzy
  };
