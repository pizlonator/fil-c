// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// DWARF CFI (.eh_frame/.debug_frame) unwinding for guest stack traces: the
// primary unwinder for crash-time traces (same mechanism gdb uses on the
// core), so -fomit-frame-pointer guests produce exact, ordered backtraces
// instead of falling back to the approximate stack scan in stack_trace.cpp.
//
// Self-contained on purpose: no bfd.h (BFD stays in stack_trace.cpp, whose
// Makefile rule has a special forced-include). The ELF section contents are
// read directly from the on-disk files named in a snapshot of
// Emulator::guest_maps: guest VA - load bias == ELF file vaddr (the loader
// records the exact load bias), so FDE pc ranges -- decoded with the same
// pcrel bases the linker used -- can be matched against guest PCs without
// re-deriving segment layout.
//
// All failure paths return false; guest-memory reads go through
// mem_copy_guarded, so garbage CFI or unmapped saved-register slots can
// never kill the process while it is writing its crash dump.

#include "dwarf_unwind.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "mem.h"

// DWARF constants: glibc's <elf.h> defines the ELF structures and SHT_*
// values but ships no CFI opcode, pointer-encoding, or expression-opcode
// macros (they live in the compiler's dwarf headers), so define the standard
// names here. Guarded, so a future <elf.h> that provides them wins.
#ifndef DW_EH_PE_absptr
#define DW_EH_PE_absptr 0x00
#endif
#ifndef DW_EH_PE_uleb128
#define DW_EH_PE_uleb128 0x01
#endif
#ifndef DW_EH_PE_udata2
#define DW_EH_PE_udata2 0x02
#endif
#ifndef DW_EH_PE_udata4
#define DW_EH_PE_udata4 0x03
#endif
#ifndef DW_EH_PE_udata8
#define DW_EH_PE_udata8 0x04
#endif
#ifndef DW_EH_PE_sleb128
#define DW_EH_PE_sleb128 0x09
#endif
#ifndef DW_EH_PE_sdata2
#define DW_EH_PE_sdata2 0x0a
#endif
#ifndef DW_EH_PE_sdata4
#define DW_EH_PE_sdata4 0x0b
#endif
#ifndef DW_EH_PE_sdata8
#define DW_EH_PE_sdata8 0x0c
#endif
#ifndef DW_EH_PE_pcrel
#define DW_EH_PE_pcrel 0x10
#endif
#ifndef DW_EH_PE_textrel
#define DW_EH_PE_textrel 0x20
#endif
#ifndef DW_EH_PE_datarel
#define DW_EH_PE_datarel 0x30
#endif
#ifndef DW_EH_PE_funcrel
#define DW_EH_PE_funcrel 0x40
#endif
#ifndef DW_EH_PE_aligned
#define DW_EH_PE_aligned 0x50
#endif
#ifndef DW_EH_PE_indirect
#define DW_EH_PE_indirect 0x80
#endif
#ifndef DW_EH_PE_omit
#define DW_EH_PE_omit 0xff
#endif

#ifndef DW_CFA_nop
#define DW_CFA_nop 0x00
#endif
#ifndef DW_CFA_set_loc
#define DW_CFA_set_loc 0x01
#endif
#ifndef DW_CFA_advance_loc1
#define DW_CFA_advance_loc1 0x02
#endif
#ifndef DW_CFA_advance_loc2
#define DW_CFA_advance_loc2 0x03
#endif
#ifndef DW_CFA_advance_loc4
#define DW_CFA_advance_loc4 0x04
#endif
#ifndef DW_CFA_offset_extended
#define DW_CFA_offset_extended 0x05
#endif
#ifndef DW_CFA_restore_extended
#define DW_CFA_restore_extended 0x06
#endif
#ifndef DW_CFA_undefined
#define DW_CFA_undefined 0x07
#endif
#ifndef DW_CFA_same_value
#define DW_CFA_same_value 0x08
#endif
#ifndef DW_CFA_register
#define DW_CFA_register 0x09
#endif
#ifndef DW_CFA_remember_state
#define DW_CFA_remember_state 0x0a
#endif
#ifndef DW_CFA_restore_state
#define DW_CFA_restore_state 0x0b
#endif
#ifndef DW_CFA_def_cfa
#define DW_CFA_def_cfa 0x0c
#endif
#ifndef DW_CFA_def_cfa_register
#define DW_CFA_def_cfa_register 0x0d
#endif
#ifndef DW_CFA_def_cfa_offset
#define DW_CFA_def_cfa_offset 0x0e
#endif
#ifndef DW_CFA_def_cfa_expression
#define DW_CFA_def_cfa_expression 0x0f
#endif
#ifndef DW_CFA_expression
#define DW_CFA_expression 0x10
#endif
#ifndef DW_CFA_offset_extended_sf
#define DW_CFA_offset_extended_sf 0x11
#endif
#ifndef DW_CFA_def_cfa_sf
#define DW_CFA_def_cfa_sf 0x12
#endif
#ifndef DW_CFA_def_cfa_offset_sf
#define DW_CFA_def_cfa_offset_sf 0x13
#endif
#ifndef DW_CFA_val_offset
#define DW_CFA_val_offset 0x14
#endif
#ifndef DW_CFA_val_offset_sf
#define DW_CFA_val_offset_sf 0x15
#endif
#ifndef DW_CFA_val_expression
#define DW_CFA_val_expression 0x16
#endif
#ifndef DW_CFA_GNU_window_save
#define DW_CFA_GNU_window_save 0x1d
#endif
#ifndef DW_CFA_GNU_args_size
#define DW_CFA_GNU_args_size 0x2e
#endif
#ifndef DW_CFA_GNU_negative_offset_sf
#define DW_CFA_GNU_negative_offset_sf 0x2f
#endif

#ifndef DW_OP_addr
#define DW_OP_addr 0x03
#endif
#ifndef DW_OP_deref
#define DW_OP_deref 0x06
#endif
#ifndef DW_OP_const1u
#define DW_OP_const1u 0x08
#endif
#ifndef DW_OP_const1s
#define DW_OP_const1s 0x09
#endif
#ifndef DW_OP_const2u
#define DW_OP_const2u 0x0a
#endif
#ifndef DW_OP_const2s
#define DW_OP_const2s 0x0b
#endif
#ifndef DW_OP_const4u
#define DW_OP_const4u 0x0c
#endif
#ifndef DW_OP_const4s
#define DW_OP_const4s 0x0d
#endif
#ifndef DW_OP_const8u
#define DW_OP_const8u 0x0e
#endif
#ifndef DW_OP_const8s
#define DW_OP_const8s 0x0f
#endif
#ifndef DW_OP_constu
#define DW_OP_constu 0x10
#endif
#ifndef DW_OP_consts
#define DW_OP_consts 0x11
#endif
#ifndef DW_OP_minus
#define DW_OP_minus 0x1c
#endif
#ifndef DW_OP_mul
#define DW_OP_mul 0x1e
#endif
#ifndef DW_OP_and
#define DW_OP_and 0x1a
#endif
#ifndef DW_OP_or
#define DW_OP_or 0x21
#endif
#ifndef DW_OP_xor
#define DW_OP_xor 0x27
#endif
#ifndef DW_OP_plus
#define DW_OP_plus 0x22
#endif
#ifndef DW_OP_plus_uconst
#define DW_OP_plus_uconst 0x23
#endif
#ifndef DW_OP_shl
#define DW_OP_shl 0x24
#endif
#ifndef DW_OP_shr
#define DW_OP_shr 0x25
#endif
#ifndef DW_OP_lit0
#define DW_OP_lit0 0x30
#endif
#ifndef DW_OP_reg0
#define DW_OP_reg0 0x50
#endif
#ifndef DW_OP_breg0
#define DW_OP_breg0 0x70
#endif
#ifndef DW_OP_regx
#define DW_OP_regx 0x90
#endif
#ifndef DW_OP_bregx
#define DW_OP_bregx 0x92
#endif
#ifndef DW_OP_deref_size
#define DW_OP_deref_size 0x94
#endif
#ifndef DW_OP_nop
#define DW_OP_nop 0x96
#endif
#ifndef DW_OP_call_frame_cfa
#define DW_OP_call_frame_cfa 0x9c
#endif
#ifndef DW_OP_stack_value
#define DW_OP_stack_value 0x9f
#endif

namespace {

// ---------------------------------------------------------------------------
// Bounds-checked little-endian readers over a byte buffer. Every reader
// returns false (without advancing *pos) when the buffer is too short.
// ---------------------------------------------------------------------------

bool rd_u8(const uint8_t** pos, const uint8_t* end, uint8_t* out) {
    if (*pos >= end)
        return false;
    *out = **pos;
    (*pos)++;
    return true;
}

bool rd_u16(const uint8_t** pos, const uint8_t* end, uint16_t* out) {
    if (end - *pos < 2)
        return false;
    uint16_t v = (uint16_t)((uint16_t)(*pos)[0] | ((uint16_t)(*pos)[1] << 8));
    *out = v;
    *pos += 2;
    return true;
}

bool rd_u32(const uint8_t** pos, const uint8_t* end, uint32_t* out) {
    if (end - *pos < 4)
        return false;
    uint32_t v = (uint32_t)(*pos)[0] | ((uint32_t)(*pos)[1] << 8) |
                 ((uint32_t)(*pos)[2] << 16) | ((uint32_t)(*pos)[3] << 24);
    *out = v;
    *pos += 4;
    return true;
}

bool rd_u64(const uint8_t** pos, const uint8_t* end, uint64_t* out) {
    if (end - *pos < 8)
        return false;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)(*pos)[i] << (8 * i);
    *out = v;
    *pos += 8;
    return true;
}

bool rd_uleb128(const uint8_t** pos, const uint8_t* end, uint64_t* out) {
    uint64_t v = 0;
    int shift = 0;
    while (true) {
        if (*pos >= end || shift >= 64)
            return false;
        uint8_t b = *(*pos)++;
        v |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80))
            break;
        shift += 7;
    }
    *out = v;
    return true;
}

bool rd_sleb128(const uint8_t** pos, const uint8_t* end, int64_t* out) {
    uint64_t v = 0;
    int shift = 0;
    uint8_t b = 0;
    while (true) {
        if (*pos >= end || shift >= 64)
            return false;
        b = *(*pos)++;
        v |= (uint64_t)(b & 0x7f) << shift;
        shift += 7;
        if (!(b & 0x80))
            break;
    }
    // Sign-extend the final byte's high bit.
    if ((b & 0x40) && shift < 64)
        v |= (~0ULL) << shift;
    *out = (int64_t)v;
    return true;
}

// ---------------------------------------------------------------------------
// DW_EH_PE_* encoded pointer reader.
// ---------------------------------------------------------------------------

// Decode one encoded value at *pos within a section whose file vaddr is
// sec_vaddr; field_off is the offset (within the section) of the field being
// decoded -- the pcrel base. When fmt_only, the application nibble is
// ignored: the FDE pc_range and the 'P' personality payload are all decoded
// by libgcc/libunwind/binutils as a plain value of the encoded size (a
// pcrel-adjusted range or an indirect personality would be nonsense), and
// this way those two consumers tolerate any application nibble.
bool read_encoded(const uint8_t** pos, const uint8_t* end, uint64_t sec_vaddr,
                  size_t field_off, uint8_t enc, bool fmt_only,
                  uint64_t* out) {
    if (enc == DW_EH_PE_omit)
        return false;
    uint64_t v;
    switch (enc & 0x0f) {
    case DW_EH_PE_absptr:
        if (!rd_u64(pos, end, &v))
            return false;
        break;
    case DW_EH_PE_uleb128:
        if (!rd_uleb128(pos, end, &v))
            return false;
        break;
    case DW_EH_PE_udata2: {
        uint16_t t;
        if (!rd_u16(pos, end, &t))
            return false;
        v = t;
        break;
    }
    case DW_EH_PE_udata4: {
        uint32_t t;
        if (!rd_u32(pos, end, &t))
            return false;
        v = t;
        break;
    }
    case DW_EH_PE_udata8:
        if (!rd_u64(pos, end, &v))
            return false;
        break;
    case DW_EH_PE_sleb128: {
        int64_t t;
        if (!rd_sleb128(pos, end, &t))
            return false;
        v = (uint64_t)t;
        break;
    }
    case DW_EH_PE_sdata2: {
        uint16_t t;
        if (!rd_u16(pos, end, &t))
            return false;
        v = (uint64_t)(int64_t)(int16_t)t;
        break;
    }
    case DW_EH_PE_sdata4: {
        uint32_t t;
        if (!rd_u32(pos, end, &t))
            return false;
        v = (uint64_t)(int64_t)(int32_t)t;
        break;
    }
    case DW_EH_PE_sdata8: {
        uint64_t t;
        if (!rd_u64(pos, end, &t))
            return false;
        v = (uint64_t)(int64_t)t;
        break;
    }
    default:
        return false;
    }
    if (!fmt_only) {
        switch (enc & 0x70) {
        case 0x00:
            break;
        case DW_EH_PE_pcrel:
            v += sec_vaddr + (uint64_t)field_off;
            break;
        default: // textrel/datarel/funcrel/aligned: no relocatable base here
            return false;
        }
        if (enc & DW_EH_PE_indirect)
            return false;
    }
    *out = v;
    return true;
}

// ---------------------------------------------------------------------------
// Per-module CFI cache (like stack_trace.cpp's get_sym_tab: crash-time
// tracing is rare, one global lock, results cached by on-disk path).
// ---------------------------------------------------------------------------

struct ModuleCFI {
    bool tried = false; // parse attempted
    bool ok = false;    // >= 1 section parsed
    // Section contents + their ELF file vaddrs (sh_addr).
    std::vector<uint8_t> eh_frame;
    uint64_t eh_frame_vaddr = 0;
    bool have_eh = false;
    std::vector<uint8_t> debug_frame;
    uint64_t debug_frame_vaddr = 0;
    bool have_dbg = false;
    struct FDE {
        uint64_t lo, hi; // file-vaddr range [lo, hi)
        uint64_t cie_off; // CIE offset within its section
        uint64_t ins_off, ins_end; // CFI instruction byte range within the
                                   // section
        bool from_debug; // parsed from .debug_frame
    };
    std::vector<FDE> fdes;
    struct CIE {
        uint64_t version;
        uint64_t code_align = 1;
        int64_t data_align = -8;
        uint32_t ra_reg = 16;
        uint8_t fde_encoding = DW_EH_PE_absptr;
        uint8_t lsda_encoding = DW_EH_PE_omit;
        bool has_z = false; // augmentation string contained 'z'
        uint64_t init_off = 0, init_end = 0; // initial instructions within
                                             // the section
        bool usable = true; // false for augmentations we do not support
    };
    std::map<uint64_t, CIE> cies_eh, cies_dbg; // keyed by section offset
};

static std::mutex g_cfi_mu;
static std::map<std::string, ModuleCFI> g_cfi_cache;

// True if the section name at strtab+name_off equals `want` (NUL-safe even
// when the string table is truncated).
bool section_name_is(const uint8_t* strtab, size_t strsz, uint32_t name_off,
                     const char* want) {
    if (name_off >= strsz)
        return false;
    const char* s = (const char*)strtab + name_off;
    size_t left = strsz - name_off;
    size_t wl = strlen(want);
    if (left <= wl)
        return false;
    return memcmp(s, want, wl + 1) == 0;
}

// Open the ELF object, mmap it, and copy the .eh_frame/.debug_frame section
// contents (with their sh_addr file vaddrs) into *m. Any failure leaves both
// have_* false; the caller caches the empty result either way so a bad file
// is not re-read for every frame.
void load_module_cfi(const std::string& path, ModuleCFI* m) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        (uint64_t)st.st_size < sizeof(Elf64_Ehdr)) {
        close(fd);
        return;
    }
    size_t sz = (size_t)st.st_size;
    void* map = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED)
        return;
    const uint8_t* base = (const uint8_t*)map;
    const uint8_t* eh = base + sz; // end-of-file sentinel

    // ELF64 little-endian only (guest objects are x86-64 Linux ELFs).
    if (memcmp(base, ELFMAG, SELFMAG) != 0 || base[EI_CLASS] != ELFCLASS64 ||
        base[EI_DATA] != ELFDATA2LSB)
        goto out;

    {
        const Elf64_Ehdr* h = (const Elf64_Ehdr*)base;
        uint64_t shoff = h->e_shoff;
        uint64_t shent = h->e_shentsize;
        uint64_t shnum = h->e_shnum;
        uint64_t shstrndx = h->e_shstrndx;
        if (shent < sizeof(Elf64_Shdr))
            goto out;

        // Extended section numbering keeps the real counts in section 0.
        Elf64_Shdr sh0;
        bool have_sh0 = false;
        if (shoff && shnum == 0 && shoff <= (uint64_t)(eh - base - sizeof(Elf64_Shdr))) {
            memcpy(&sh0, base + shoff, sizeof(sh0));
            have_sh0 = true;
            shnum = sh0.sh_size;
        }
        if (shstrndx == SHN_XINDEX && have_sh0)
            shstrndx = sh0.sh_link;
        if (!shnum || !shoff || shstrndx >= shnum)
            goto out;
        if (shoff > (uint64_t)(eh - base) ||
            shnum > ((uint64_t)(eh - base) - shoff) / shent)
            goto out;

        const Elf64_Shdr* shs = (const Elf64_Shdr*)(base + shoff);
        const Elf64_Shdr* strsh = shs + shstrndx;
        if (strsh->sh_type == SHT_NOBITS ||
            strsh->sh_offset > (uint64_t)(eh - base) ||
            strsh->sh_size > (uint64_t)(eh - base) - strsh->sh_offset)
            goto out;
        const uint8_t* strtab = base + strsh->sh_offset;
        size_t strsz = strsh->sh_size;

        for (uint64_t i = 0; i < shnum; i++) {
            const Elf64_Shdr* sh = shs + i;
            if (sh->sh_type == SHT_NOBITS || sh->sh_size == 0)
                continue;
            if (sh->sh_flags & SHF_COMPRESSED)
                continue;
            if (sh->sh_offset > (uint64_t)(eh - base) ||
                sh->sh_size > (uint64_t)(eh - base) - sh->sh_offset)
                continue;
            bool is_eh = section_name_is(strtab, strsz, sh->sh_name,
                                         ".eh_frame");
            bool is_dbg = !is_eh && section_name_is(strtab, strsz,
                                                    sh->sh_name,
                                                    ".debug_frame");
            if (!is_eh && !is_dbg)
                continue;
            if (is_eh ? m->have_eh : m->have_dbg)
                continue;
            std::vector<uint8_t>& dst = is_eh ? m->eh_frame : m->debug_frame;
            dst.assign(base + sh->sh_offset,
                       base + sh->sh_offset + sh->sh_size);
            if (is_eh) {
                m->eh_frame_vaddr = sh->sh_addr;
                m->have_eh = true;
            } else {
                m->debug_frame_vaddr = sh->sh_addr;
                m->have_dbg = true;
            }
        }
    }
out:
    munmap(map, sz);
}

// ---------------------------------------------------------------------------
// Section parsing: two passes (CIEs first, then FDEs) because an FDE's
// CIE_pointer may be a forward reference.
// ---------------------------------------------------------------------------

// Iterate the CFI units of one section. For each unit: unit_off = offset of
// the length field, hdr = 4 (32-bit DWARF) or 12 (64-bit), body = offset of
// the CIE_id/FDE CIE_pointer field, unit_end = end of the unit.
struct CfiUnit {
    uint64_t unit_off, hdr, body, unit_end;
};

bool next_cfi_unit(const uint8_t* base, size_t size, uint64_t* offp,
                   CfiUnit* u) {
    const uint8_t* p = base + *offp;
    const uint8_t* end = base + size;
    if (p >= end)
        return false;
    uint32_t len32;
    if (!rd_u32(&p, end, &len32))
        return false;
    uint64_t off = *offp;
    if (len32 == 0)
        return false; // zero length terminates the section
    uint64_t hdr, unit_len;
    if (len32 == 0xffffffffU) {
        uint64_t len64;
        if (!rd_u64(&p, end, &len64))
            return false;
        hdr = 12;
        unit_len = len64;
    } else {
        hdr = 4;
        unit_len = len32;
    }
    if (unit_len > (uint64_t)(end - (base + off)) - hdr)
        return false;
    u->unit_off = off;
    u->hdr = hdr;
    u->body = off + hdr;
    u->unit_end = off + hdr + unit_len;
    *offp = u->unit_end;
    return true;
}

// Is this unit a CIE? The CIE id is 0 in .eh_frame (4 or 8 bytes) and
// 0xffffffff/0xffffffffffffffff in .debug_frame.
bool unit_is_cie(bool from_debug, bool fmt64, uint64_t id) {
    if (from_debug) {
        if (fmt64)
            return id == 0xffffffffffffffffULL;
        return id == 0xffffffffU;
    }
    return id == 0;
}

// Parse the body of one CIE (after the CIE_id field). Returns false only on
// truncation; unsupported-but-parseable augmentations leave usable=false.
bool parse_cie(const uint8_t* base, const CfiUnit& u, ModuleCFI::CIE* cie) {
    const uint8_t* p = base + u.body + (u.hdr == 12 ? 8 : 4);
    const uint8_t* end = base + u.unit_end;
    cie->usable = true;
    uint64_t version;
    if (!rd_uleb128(&p, end, &version) || version < 1 || version > 5) {
        cie->usable = false;
        return true;
    }
    cie->version = version;
    // NUL-terminated augmentation string.
    const uint8_t* aug = p;
    while (p < end && *p)
        p++;
    if (p >= end) {
        cie->usable = false;
        return true;
    }
    size_t aug_len = (size_t)(p - aug);
    p++; // the NUL
    bool has_z = false;
    bool eh_aug = (aug_len >= 2 && aug[0] == 'e' && aug[1] == 'h');
    for (size_t i = 0; i < aug_len; i++)
        if (aug[i] == 'z')
            has_z = true;
    if (version >= 4) {
        // address_size + segment_size (we only handle 8-byte addresses).
        uint8_t addr_size, seg_size;
        if (!rd_u8(&p, end, &addr_size) || !rd_u8(&p, end, &seg_size)) {
            cie->usable = false;
            return true;
        }
        if (addr_size != 8)
            cie->usable = false;
    }
    uint64_t code_align;
    int64_t data_align;
    uint64_t ra_reg;
    if (!rd_uleb128(&p, end, &code_align) ||
        !rd_sleb128(&p, end, &data_align) ||
        !rd_uleb128(&p, end, &ra_reg)) {
        cie->usable = false;
        return true;
    }
    if (eh_aug) {
        // Legacy pre-DWARF3 "eh" augmentation: an extra encoded pointer we
        // do not model sits between the string and the factors above, so
        // everything past this point is untrustworthy.
        cie->usable = false;
        return true;
    }
    if (ra_reg >= 32)
        cie->usable = false;
    cie->code_align = code_align;
    cie->data_align = data_align;
    cie->ra_reg = (uint32_t)ra_reg;
    if (has_z) {
        cie->has_z = true;
        uint64_t aug_data_len;
        if (!rd_uleb128(&p, end, &aug_data_len) ||
            aug_data_len > (uint64_t)(end - p)) {
            cie->usable = false;
            return true;
        }
        const uint8_t* aug_end = p + aug_data_len;
        for (size_t i = 1; i < aug_len && cie->usable; i++) {
            switch (aug[i]) {
            case 'L': {
                uint8_t enc;
                if (!rd_u8(&p, end, &enc)) {
                    cie->usable = false;
                    return true;
                }
                cie->lsda_encoding = enc;
                break;
            }
            case 'P': {
                // Personality: one encoding byte + one encoded value. We
                // never need the value (only its size), so decode with the
                // format nibble only: an indirect personality then still
                // consumes exactly one field.
                uint8_t enc;
                if (!rd_u8(&p, end, &enc)) {
                    cie->usable = false;
                    return true;
                }
                uint64_t dummy;
                if (!read_encoded(&p, end, 0, 0, enc, true, &dummy))
                    cie->usable = false;
                break;
            }
            case 'R': {
                uint8_t enc;
                if (!rd_u8(&p, end, &enc)) {
                    cie->usable = false;
                    return true;
                }
                cie->fde_encoding = enc;
                break;
            }
            case 'S':
            case 'B':
            case 'G':
                break;
            default:
                // Unknown augmentation letter: its payload size is unknown,
                // so the rest of the CIE cannot be trusted.
                cie->usable = false;
                break;
            }
        }
        p = aug_end;
    }
    cie->init_off = (uint64_t)(p - base);
    cie->init_end = u.unit_end;
    return true;
}

// Parse every CIE and FDE of one section into the module's maps/vec.
void parse_section(const std::vector<uint8_t>& data, uint64_t sec_vaddr,
                   bool from_debug, std::map<uint64_t, ModuleCFI::CIE>* cies,
                   std::vector<ModuleCFI::FDE>* fdes) {
    if (data.empty())
        return;
    const uint8_t* base = data.data();
    size_t size = data.size();

    // Pass 1: CIEs.
    uint64_t off = 0;
    CfiUnit u;
    while (next_cfi_unit(base, size, &off, &u)) {
        const uint8_t* p = base + u.body;
        const uint8_t* end = base + u.unit_end;
        uint64_t id;
        bool fmt64 = u.hdr == 12;
        if (fmt64) {
            if (!rd_u64(&p, end, &id))
                break;
        } else {
            uint32_t id32;
            if (!rd_u32(&p, end, &id32))
                break;
            id = id32;
        }
        if (unit_is_cie(from_debug, fmt64, id)) {
            ModuleCFI::CIE cie;
            if (!parse_cie(base, u, &cie))
                break;
            (*cies)[u.unit_off] = cie;
        }
    }

    // Pass 2: FDEs.
    off = 0;
    while (next_cfi_unit(base, size, &off, &u)) {
        const uint8_t* p = base + u.body;
        const uint8_t* end = base + u.unit_end;
        uint64_t id;
        bool fmt64 = u.hdr == 12;
        if (fmt64) {
            if (!rd_u64(&p, end, &id))
                break;
        } else {
            uint32_t id32;
            if (!rd_u32(&p, end, &id32))
                break;
            id = id32;
        }
        uint64_t cie_field_off = u.body;
        // CIE_pointer: GCC's .eh_frame encodes it relative to this field
        // (cie = field_pos - value); the DWARF spec's .debug_frame encodes
        // the section-relative CIE offset. Try the section's convention
        // first, then the other.
        auto resolve = [&](uint64_t v) -> const ModuleCFI::CIE* {
            auto it = cies->find(v);
            return it == cies->end() ? nullptr : &it->second;
        };
        uint64_t cie_key;
        const ModuleCFI::CIE* cie;
        if (from_debug) {
            cie = resolve(id);
            cie_key = id;
            if (!cie) {
                cie_key = cie_field_off - id;
                cie = resolve(cie_key);
            }
        } else {
            cie = resolve(cie_field_off - id);
            cie_key = cie_field_off - id;
            if (!cie) {
                cie_key = id;
                cie = resolve(cie_key);
            }
        }
        if (!cie || !cie->usable)
            continue;
        // pc_begin (full encoding), pc_range (format only, no pcrel --
        // matching libgcc/libunwind/binutils).
        uint64_t pc_begin, pc_range;
        size_t field_off = (size_t)(p - base);
        if (!read_encoded(&p, end, sec_vaddr, field_off, cie->fde_encoding,
                          false, &pc_begin))
            continue;
        field_off = (size_t)(p - base);
        if (!read_encoded(&p, end, sec_vaddr, field_off, cie->fde_encoding,
                          true, &pc_range))
            continue;
        if (pc_range == 0)
            continue;
        // FDE augmentation data exists only when the CIE augmentation had
        // 'z'; the LSDA pointer it may carry is of no use here, so skip the
        // whole block by its declared length.
        if (cie->has_z) {
            uint64_t aug_len;
            if (!rd_uleb128(&p, end, &aug_len) ||
                aug_len > (uint64_t)(end - p))
                continue;
            p += aug_len;
        }
        ModuleCFI::FDE fde;
        fde.lo = pc_begin;
        fde.hi = pc_begin + pc_range;
        fde.cie_off = cie_key;
        fde.ins_off = (uint64_t)(p - base);
        fde.ins_end = u.unit_end;
        fde.from_debug = from_debug;
        fdes->push_back(fde);
    }
}

// ---------------------------------------------------------------------------
// CFI evaluation: run the CIE initial instructions plus the FDE instructions
// of the FDE covering the target PC, producing the register rules in effect
// at that PC.
// ---------------------------------------------------------------------------

// One register's rule (where the CALLER's copy of the register lives).
struct RegRule {
    enum Kind {
        R_UNDEF,     // DW_CFA_undefined / not yet given a rule
        R_SAME,      // DW_CFA_same_value
        R_OFFSET,    // saved at CFA + a (factored by data_align already)
        R_VAL_OFFSET, // value is CFA + a
        R_REGISTER,  // value is the callee's register b
        R_EXPR       // location expression; val_expr: value, not address
    };
    Kind kind = R_UNDEF;
    int64_t a = 0;  // OFFSET/VAL_OFFSET: byte offset from CFA (factored)
    uint32_t b = 0; // REGISTER: the DWARF register holding the saved value
    const uint8_t* expr = nullptr;
    uint32_t expr_len = 0;
    bool val_expr = false; // EXPR came from DW_CFA_val_expression (result is
                           // the value, not an address)
};

struct CFIState {
    // CFA rule: def_cfa (register + unscaled offset) or def_cfa_expression.
    uint32_t cfa_reg = 7;
    int64_t cfa_off = 0;
    bool cfa_is_expr = false;
    const uint8_t* cfa_expr = nullptr;
    uint32_t cfa_expr_len = 0;
    RegRule regs[32]; // DWARF-numbered x86-64 registers; index 16 = return
                      // address
    std::vector<CFIState> saved; // DW_CFA_remember_state / restore_state
                                 // stack
};

// Guest-memory u64 read for the unwinder (guarded; see mem.h).
bool read64(uint64_t addr, uint64_t* out) {
    return mem_copy_guarded(addr, out, 8);
}

// Evaluate a DWARF expression subset, as used by DW_CFA_def_cfa_expression
// and DW_CFA_expression/val_expression rules (and nowhere else). `regs` is
// dwarf-numbered: 0..15 the frame's GPR values, 16 its rip. `have_cfa`
// enables DW_OP_call_frame_cfa. The result is (value, is_value): is_value is
// set by DW_OP_stack_value (and by a register reference, DW_OP_reg*/regx --
// whose meaning in a location expression is "the value is that register's
// content", never an address to dereference).
bool eval_expr(const uint8_t* p, const uint8_t* end, const uint64_t* regs,
               bool have_cfa, uint64_t cfa, uint64_t* out_value,
               bool* out_is_value) {
    constexpr size_t kMaxStack = 64;
    uint64_t stk[kMaxStack];
    size_t sp = 0;
    bool is_value = false;
    auto push = [&](uint64_t v) -> bool {
        if (sp >= kMaxStack)
            return false;
        stk[sp++] = v;
        return true;
    };
    auto pop = [&](uint64_t* v) -> bool {
        if (!sp)
            return false;
        *v = stk[--sp];
        return true;
    };

    while (p < end) {
        uint8_t op;
        if (!rd_u8(&p, end, &op))
            return false;
        if (op >= DW_OP_lit0 && op <= DW_OP_lit0 + 31) {
            if (!push((uint64_t)(op - DW_OP_lit0)))
                return false;
            continue;
        }
        if (op >= DW_OP_reg0 && op <= DW_OP_reg0 + 31) {
            uint32_t r = (uint32_t)(op - DW_OP_reg0);
            if (r >= 17)
                return false;
            if (!push(regs[r]))
                return false;
            is_value = true;
            continue;
        }
        if (op >= DW_OP_breg0 && op <= DW_OP_breg0 + 31) {
            uint32_t r = (uint32_t)(op - DW_OP_breg0);
            int64_t off;
            if (r >= 17 || !rd_sleb128(&p, end, &off))
                return false;
            if (!push(regs[r] + (uint64_t)off))
                return false;
            continue;
        }
        switch (op) {
        case DW_OP_nop:
            break;
        case DW_OP_addr: {
            uint64_t v;
            if (!rd_u64(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_const1u: {
            uint8_t v;
            if (!rd_u8(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_const1s: {
            uint8_t v;
            if (!rd_u8(&p, end, &v) || !push((uint64_t)(int64_t)(int8_t)v))
                return false;
            break;
        }
        case DW_OP_const2u: {
            uint16_t v;
            if (!rd_u16(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_const2s: {
            uint16_t v;
            if (!rd_u16(&p, end, &v) || !push((uint64_t)(int64_t)(int16_t)v))
                return false;
            break;
        }
        case DW_OP_const4u: {
            uint32_t v;
            if (!rd_u32(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_const4s: {
            uint32_t v;
            if (!rd_u32(&p, end, &v) || !push((uint64_t)(int64_t)(int32_t)v))
                return false;
            break;
        }
        case DW_OP_const8u:
        case DW_OP_const8s: {
            uint64_t v;
            if (!rd_u64(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_constu: {
            uint64_t v;
            if (!rd_uleb128(&p, end, &v) || !push(v))
                return false;
            break;
        }
        case DW_OP_consts: {
            int64_t v;
            if (!rd_sleb128(&p, end, &v) || !push((uint64_t)v))
                return false;
            break;
        }
        case DW_OP_regx: {
            uint64_t r;
            if (!rd_uleb128(&p, end, &r) || r >= 17)
                return false;
            if (!push(regs[r]))
                return false;
            is_value = true;
            break;
        }
        case DW_OP_bregx: {
            uint64_t r;
            int64_t off;
            if (!rd_uleb128(&p, end, &r) || r >= 17 ||
                !rd_sleb128(&p, end, &off))
                return false;
            if (!push(regs[r] + (uint64_t)off))
                return false;
            break;
        }
        case DW_OP_plus_uconst: {
            uint64_t v, c;
            if (!pop(&v) || !rd_uleb128(&p, end, &c) || !push(v + c))
                return false;
            break;
        }
        case DW_OP_plus:
        case DW_OP_minus:
        case DW_OP_and:
        case DW_OP_or:
        case DW_OP_xor:
        case DW_OP_mul:
        case DW_OP_shl:
        case DW_OP_shr: {
            uint64_t a, b;
            if (!pop(&a) || !pop(&b))
                return false;
            uint64_t r;
            switch (op) {
            case DW_OP_plus:
                r = b + a;
                break;
            case DW_OP_minus:
                r = b - a;
                break;
            case DW_OP_and:
                r = b & a;
                break;
            case DW_OP_or:
                r = b | a;
                break;
            case DW_OP_xor:
                r = b ^ a;
                break;
            case DW_OP_mul:
                r = b * a;
                break;
            case DW_OP_shl:
                if (a >= 64)
                    return false;
                r = b << a;
                break;
            default: // DW_OP_shr
                if (a >= 64)
                    return false;
                r = b >> a;
                break;
            }
            if (!push(r))
                return false;
            break;
        }
        case DW_OP_deref:
        case DW_OP_deref_size: {
            uint64_t addr;
            uint64_t v = 0;
            size_t n = 8;
            if (op == DW_OP_deref_size) {
                uint8_t sz;
                if (!rd_u8(&p, end, &sz) || sz == 0 || sz > 8)
                    return false;
                n = sz;
            }
            if (!pop(&addr))
                return false;
            if (n == 8) {
                if (!read64(addr, &v))
                    return false;
            } else {
                uint64_t b = 0;
                if (!mem_copy_guarded(addr, &b, n))
                    return false;
                for (size_t i = 0; i < n; i++)
                    v |= ((uint64_t)((uint8_t*)&b)[i]) << (8 * i);
            }
            if (!push(v))
                return false;
            break;
        }
        case DW_OP_call_frame_cfa:
            if (!have_cfa || !push(cfa))
                return false;
            break;
        case DW_OP_stack_value:
            is_value = true;
            break;
        default:
            // Unsupported (piece, bit_piece, fbreg, skip/bra,
            // call2/call4/call_ref, push_object_address, implicit_value,
            // ...): the result would be garbage, so fail the evaluation.
            return false;
        }
    }
    if (!sp)
        return false;
    *out_value = stk[sp - 1];
    *out_is_value = is_value;
    return true;
}

// Run one CFI instruction stream. With stop_at_target (the FDE's own
// instructions), the stream runs while the accumulated location `loc` is
// <= target (the row containing the target is the last row whose start is
// <= it, so its defining instructions are all applied); `loc` starts at
// pc_begin and is absolute. `initial` is the state established by the CIE's
// initial instructions -- DW_CFA_restore* revert a register to it.
bool run_cfi_stream(const uint8_t* p, const uint8_t* end,
                    const uint8_t* sec_base, uint64_t sec_vaddr,
                    uint8_t fde_encoding, uint64_t code_align,
                    int64_t data_align, uint64_t pc_begin, uint64_t target,
                    bool stop_at_target, CFIState* st,
                    const CFIState* initial) {
    uint64_t loc = pc_begin;
    while (p < end) {
        if (stop_at_target && loc > target)
            break;
        uint8_t op;
        if (!rd_u8(&p, end, &op))
            return false;
        if ((op & 0xc0) == 0x40) {
            loc += (uint64_t)(op & 0x3f) * code_align;
            continue;
        }
        if ((op & 0xc0) == 0x80) {
            uint32_t reg = (uint32_t)(op & 0x3f);
            uint64_t off;
            if (reg >= 32 || !rd_uleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_OFFSET;
            st->regs[reg].a = (int64_t)off * data_align;
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            continue;
        }
        if ((op & 0xc0) == 0xc0) {
            uint32_t reg = (uint32_t)(op & 0x3f);
            if (reg >= 32)
                return false;
            st->regs[reg] = initial->regs[reg];
            continue;
        }
        switch (op) {
        case DW_CFA_nop:
            break;
        case DW_CFA_set_loc: {
            // The set_loc operand is an absolute location encoded with the
            // FDE's address encoding (pcrel base: the field itself).
            uint64_t v;
            if (!read_encoded(&p, end, sec_vaddr, (size_t)(p - sec_base),
                              fde_encoding, false, &v))
                return false;
            loc = v;
            break;
        }
        case DW_CFA_advance_loc1: {
            uint8_t d;
            if (!rd_u8(&p, end, &d))
                return false;
            loc += (uint64_t)d * code_align;
            break;
        }
        case DW_CFA_advance_loc2: {
            uint16_t d;
            if (!rd_u16(&p, end, &d))
                return false;
            loc += (uint64_t)d * code_align;
            break;
        }
        case DW_CFA_advance_loc4: {
            uint32_t d;
            if (!rd_u32(&p, end, &d))
                return false;
            loc += (uint64_t)d * code_align;
            break;
        }
        case DW_CFA_offset_extended: {
            uint64_t reg, off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_uleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_OFFSET;
            st->regs[reg].a = (int64_t)off * data_align;
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            break;
        }
        case DW_CFA_restore_extended: {
            uint64_t reg;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32)
                return false;
            st->regs[reg] = initial->regs[reg];
            break;
        }
        case DW_CFA_undefined: {
            uint64_t reg;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32)
                return false;
            st->regs[reg].kind = RegRule::R_UNDEF;
            break;
        }
        case DW_CFA_same_value: {
            uint64_t reg;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32)
                return false;
            st->regs[reg].kind = RegRule::R_SAME;
            break;
        }
        case DW_CFA_register: {
            uint64_t r1, r2;
            if (!rd_uleb128(&p, end, &r1) || r1 >= 32 ||
                !rd_uleb128(&p, end, &r2) || r2 >= 32)
                return false;
            st->regs[r1].kind = RegRule::R_REGISTER;
            st->regs[r1].b = (uint32_t)r2;
            st->regs[r1].expr = nullptr;
            st->regs[r1].val_expr = false;
            break;
        }
        case DW_CFA_remember_state: {
            CFIState snap = *st;
            snap.saved.clear();
            st->saved.push_back(std::move(snap));
            break;
        }
        case DW_CFA_restore_state: {
            if (st->saved.empty())
                return false;
            std::vector<CFIState> outer = std::move(st->saved);
            CFIState snap = std::move(outer.back());
            outer.pop_back();
            *st = std::move(snap);
            st->saved = std::move(outer);
            break;
        }
        case DW_CFA_def_cfa: {
            uint64_t reg, off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_uleb128(&p, end, &off))
                return false;
            st->cfa_reg = (uint32_t)reg;
            st->cfa_off = (int64_t)off;
            st->cfa_is_expr = false;
            break;
        }
        case DW_CFA_def_cfa_sf: {
            uint64_t reg;
            int64_t off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_sleb128(&p, end, &off))
                return false;
            st->cfa_reg = (uint32_t)reg;
            st->cfa_off = off * data_align;
            st->cfa_is_expr = false;
            break;
        }
        case DW_CFA_def_cfa_register: {
            uint64_t reg;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32)
                return false;
            st->cfa_reg = (uint32_t)reg;
            st->cfa_is_expr = false;
            break;
        }
        case DW_CFA_def_cfa_offset: {
            uint64_t off;
            if (!rd_uleb128(&p, end, &off))
                return false;
            st->cfa_off = (int64_t)off; // unscaled
            st->cfa_is_expr = false;
            break;
        }
        case DW_CFA_def_cfa_offset_sf: {
            int64_t off;
            if (!rd_sleb128(&p, end, &off))
                return false;
            st->cfa_off = off * data_align;
            st->cfa_is_expr = false;
            break;
        }
        case DW_CFA_def_cfa_expression: {
            uint64_t len;
            if (!rd_uleb128(&p, end, &len) || len > (uint64_t)(end - p))
                return false;
            st->cfa_is_expr = true;
            st->cfa_expr = p;
            st->cfa_expr_len = (uint32_t)len;
            p += len;
            break;
        }
        case DW_CFA_expression:
        case DW_CFA_val_expression: {
            uint64_t reg, len;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_uleb128(&p, end, &len) || len > (uint64_t)(end - p))
                return false;
            st->regs[reg].kind = RegRule::R_EXPR;
            st->regs[reg].expr = p;
            st->regs[reg].expr_len = (uint32_t)len;
            st->regs[reg].val_expr = op == DW_CFA_val_expression;
            p += len;
            break;
        }
        case DW_CFA_offset_extended_sf: {
            uint64_t reg;
            int64_t off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_sleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_OFFSET;
            st->regs[reg].a = off * data_align;
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            break;
        }
        case DW_CFA_val_offset: {
            uint64_t reg, off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_uleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_VAL_OFFSET;
            st->regs[reg].a = (int64_t)off * data_align;
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            break;
        }
        case DW_CFA_val_offset_sf: {
            uint64_t reg;
            int64_t off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_sleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_VAL_OFFSET;
            st->regs[reg].a = off * data_align;
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            break;
        }
        case DW_CFA_GNU_args_size: {
            uint64_t sz;
            if (!rd_uleb128(&p, end, &sz))
                return false;
            break; // ignored
        }
        case DW_CFA_GNU_negative_offset_sf: {
            uint64_t reg;
            int64_t off;
            if (!rd_uleb128(&p, end, &reg) || reg >= 32 ||
                !rd_sleb128(&p, end, &off))
                return false;
            st->regs[reg].kind = RegRule::R_OFFSET;
            st->regs[reg].a = -(off * data_align);
            st->regs[reg].expr = nullptr;
            st->regs[reg].val_expr = false;
            break;
        }
        case DW_CFA_GNU_window_save:
            break; // SPARC-specific; nothing to do for x86-64
        default:
            return false; // unknown opcode: fail the whole step
        }
    }
    return true;
}

// Compute the CFI rules in effect at `target` (a file vaddr) from the FDE
// `fde` of module `m`. Fails when the CIE/FDE is malformed or uses something
// we do not support.
bool eval_fde(const ModuleCFI& m, const ModuleCFI::FDE& fde, uint64_t target,
              CFIState* out) {
    const std::vector<uint8_t>& sec =
        fde.from_debug ? m.debug_frame : m.eh_frame;
    const std::map<uint64_t, ModuleCFI::CIE>& cies =
        fde.from_debug ? m.cies_dbg : m.cies_eh;
    auto it = cies.find(fde.cie_off);
    if (it == cies.end() || !it->second.usable)
        return false;
    const ModuleCFI::CIE& cie = it->second;
    if (cie.ra_reg >= 32)
        return false;
    if (sec.empty() || fde.ins_end > sec.size() ||
        cie.init_end > sec.size())
        return false;
    const uint8_t* sec_base = sec.data();

    CFIState st;
    // The CIE's initial instructions define the state at the function
    // entry; they all run, regardless of where they place `loc`.
    if (!run_cfi_stream(sec_base + cie.init_off, sec_base + cie.init_end,
                        sec_base, (fde.from_debug ? m.debug_frame_vaddr
                                                  : m.eh_frame_vaddr),
                        cie.fde_encoding, cie.code_align, cie.data_align,
                        fde.lo, target, false, &st, &st))
        return false;
    // The initial rules DW_CFA_restore* revert to:
    CFIState initial = st;
    if (fde.ins_off > fde.ins_end || fde.ins_end > sec.size())
        return false;
    if (!run_cfi_stream(sec_base + fde.ins_off, sec_base + fde.ins_end,
                        sec_base, (fde.from_debug ? m.debug_frame_vaddr
                                                  : m.eh_frame_vaddr),
                        cie.fde_encoding, cie.code_align, cie.data_align,
                        fde.lo, target, true, &st, &initial))
        return false;
    *out = std::move(st);
    return true;
}

ModuleCFI* get_module_cfi(const std::string& path) {
    // caller holds g_cfi_mu
    auto it = g_cfi_cache.find(path);
    if (it != g_cfi_cache.end())
        return &it->second;
    ModuleCFI m;
    m.tried = true;
    load_module_cfi(path, &m);
    if (m.have_eh)
        parse_section(m.eh_frame, m.eh_frame_vaddr, false, &m.cies_eh,
                      &m.fdes);
    if (m.have_dbg)
        parse_section(m.debug_frame, m.debug_frame_vaddr, true, &m.cies_dbg,
                      &m.fdes);
    m.ok = m.have_eh || m.have_dbg;
    return &g_cfi_cache.emplace(path, std::move(m)).first->second;
}

// DWARF register numbers for x86-64: 0=rax 1=rdx 2=rcx 3=rbx 4=rsi 5=rdi
// 6=rbp 7=rsp 8..15=r8..r15, 16=rip (the return address column). Index into
// the CPU's ZG_*-ordered gpr array.
uint32_t dwarf_to_zg(uint32_t dwarf_reg) {
    static const uint32_t map[16] = {
        0, 2, 1, 3, 6, 7, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15
    };
    return map[dwarf_reg & 15];
}

} // namespace

bool zeg_unwind_step(const std::vector<Emulator::GuestMap>& maps,
                     const ZegUnwindState& st, ZegUnwindState& caller,
                     bool st_rip_is_ra) {
    // Passthrough mode installs no fault handler, so guarded guest reads
    // are not actually guarded: refuse to run there.
    if (mem_passthrough())
        return false;

    // Evaluate the CFI of the frame whose code is at st.rip; for return
    // addresses, the preceding byte belongs to the call, whose row is the
    // one in effect (also required when the call is the function's last
    // instruction: the next byte belongs to the FOLLOWING function).
    uint64_t cfi_pc;
    if (st_rip_is_ra) {
        if (!st.rip)
            return false;
        cfi_pc = st.rip - 1;
    } else {
        cfi_pc = st.rip;
    }
    if (!cfi_pc)
        return false;

    // Find the module containing the PC (same criteria as stack_trace.cpp's
    // find_module: exact load bias, real on-disk file).
    const std::string* path = nullptr;
    uint64_t bias = 0;
    for (const auto& gm : maps) {
        if (!gm.bias_valid || gm.path.empty() ||
            !(gm.start <= cfi_pc && cfi_pc < gm.end))
            continue;
        if (access(gm.path.c_str(), R_OK) != 0)
            continue;
        path = &gm.path;
        bias = gm.bias;
        break;
    }
    if (!path)
        return false;

    std::lock_guard<std::mutex> l(g_cfi_mu);
    ModuleCFI* m = get_module_cfi(*path);
    if (!m || !m->ok)
        return false;
    uint64_t file_pc = cfi_pc - bias;

    // Find the FDE covering the PC (linear scan; the parse kept only FDEs
    // with usable CIEs).
    const ModuleCFI::FDE* fde = nullptr;
    for (const auto& f : m->fdes) {
        if (f.lo <= file_pc && file_pc < f.hi) {
            fde = &f;
            break;
        }
    }
    if (!fde)
        return false;

    CFIState cfi;
    if (!eval_fde(*m, *fde, file_pc, &cfi))
        return false;

    // The RA column comes from the CIE (eval_fde required it to be < 32,
    // but re-resolve it here so the rule lookup is explicit).
    const std::map<uint64_t, ModuleCFI::CIE>& cies =
        fde->from_debug ? m->cies_dbg : m->cies_eh;
    auto cit = cies.find(fde->cie_off);
    if (cit == cies.end() || !cit->second.usable || cit->second.ra_reg >= 32)
        return false;
    uint32_t ra_reg = (uint32_t)cit->second.ra_reg;

    // The frame's CFA.
    uint64_t cfa;
    if (cfi.cfa_is_expr) {
        uint64_t regs[17];
        for (uint32_t d = 0; d < 16; d++)
            regs[d] = st.gpr[dwarf_to_zg(d)];
        regs[16] = st.rip;
        uint64_t v;
        bool is_val;
        // No CFA is available while computing it, so an expression that
        // calls DW_OP_call_frame_cfa fails the evaluation.
        if (!eval_expr(cfi.cfa_expr, cfi.cfa_expr + cfi.cfa_expr_len, regs,
                       false, 0, &v, &is_val))
            return false;
        cfa = v; // used as-is: no implicit deref
    } else {
        if (cfi.cfa_reg >= 16)
            return false;
        cfa = st.gpr[dwarf_to_zg(cfi.cfa_reg)] + (uint64_t)cfi.cfa_off;
    }
    if (cfa == 0 || cfa < 4096)
        return false;

    // The frame's return address (CIE's ra_reg column).
    auto get_rule_value = [&](const RegRule& rule, uint64_t* out) -> bool {
        switch (rule.kind) {
        case RegRule::R_UNDEF:
        case RegRule::R_SAME:
            return false;
        case RegRule::R_OFFSET:
            return read64(cfa + (uint64_t)rule.a, out);
        case RegRule::R_VAL_OFFSET:
            *out = cfa + (uint64_t)rule.a;
            return true;
        case RegRule::R_REGISTER:
            if (rule.b >= 16)
                return false;
            *out = st.gpr[dwarf_to_zg(rule.b)];
            return true;
        case RegRule::R_EXPR: {
            uint64_t regs[17];
            for (uint32_t d = 0; d < 16; d++)
                regs[d] = st.gpr[dwarf_to_zg(d)];
            regs[16] = st.rip;
            uint64_t v;
            bool is_val;
            if (!eval_expr(rule.expr, rule.expr + rule.expr_len, regs, true,
                           cfa, &v, &is_val))
                return false;
            if (rule.val_expr || is_val) {
                *out = v;
                return true;
            }
            return read64(v, out);
        }
        }
        return false;
    };

    uint64_t ra;
    if (!get_rule_value(cfi.regs[ra_reg], &ra))
        return false;
    if (ra == 0)
        return false;

    // The caller's register state. The caller's SP is the CFA: GCC/Clang
    // never emit rules for the SP register itself (dwarf 7) on x86-64, so
    // skip dwarf 7 in the rule loop below -- with the default "no rule"
    // state being R_UNDEF it would otherwise clobber the CFA with the
    // callee's (post-prologue, crash-time) SP, exactly like forgetting the
    // CFA entirely.
    caller = ZegUnwindState();
    caller.gpr[4 /* ZG_RSP */] = cfa;
    for (uint32_t d = 0; d < 16; d++) {
        if (d == 7)
            continue; // rsp: caller value is the CFA (set above)
        const RegRule& rule = cfi.regs[d];
        uint64_t v;
        switch (rule.kind) {
        case RegRule::R_UNDEF:
        case RegRule::R_SAME:
            // The callee never saved it: the caller's value is still live.
            caller.gpr[dwarf_to_zg(d)] = st.gpr[dwarf_to_zg(d)];
            break;
        case RegRule::R_OFFSET:
            if (!read64(cfa + (uint64_t)rule.a, &v))
                return false;
            caller.gpr[dwarf_to_zg(d)] = v;
            break;
        case RegRule::R_VAL_OFFSET:
            caller.gpr[dwarf_to_zg(d)] = cfa + (uint64_t)rule.a;
            break;
        case RegRule::R_REGISTER:
            if (rule.b >= 16)
                return false;
            caller.gpr[dwarf_to_zg(d)] = st.gpr[dwarf_to_zg(rule.b)];
            break;
        case RegRule::R_EXPR:
            if (!get_rule_value(rule, &v))
                return false;
            caller.gpr[dwarf_to_zg(d)] = v;
            break;
        }
    }
    caller.rip = ra;
    return true;
}

// --- --history=N/full stack recording ----------------------------------------

// Per-step frame cap for deep history (0 = full/unlimited, capped at 256
// like the crash-time stack trace). Set once by main from --history=N.
static size_t g_history_frames = 0;

void zeg_set_history_frames(size_t n) {
    g_history_frames = n;
}

void zeg_history_record_stack(std::vector<uint64_t>& out) {
    out.clear();
    out.push_back(zeg_history_pc);
    size_t cap = g_history_frames ? g_history_frames : 256;
    if (out.size() >= cap || mem_passthrough() || !zeg_history_emu)
        return;
    std::vector<Emulator::GuestMap> maps;
    {
        std::lock_guard<std::mutex> l(zeg_history_emu->maps_mu);
        maps = zeg_history_emu->guest_maps;
    }
    ZegUnwindState st;
    for (int i = 0; i < 16; i++)
        st.gpr[i] = zeg_history_regs[i];
    st.rip = zeg_history_pc;
    bool ra = false;
    while (out.size() < cap) {
        ZegUnwindState caller;
        if (!zeg_unwind_step(maps, st, caller, ra))
            break;
        ra = true;
        if (!caller.rip || caller.rip == st.rip)
            break;
        out.push_back(caller.rip);
        st = caller;
    }
}
