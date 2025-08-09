/* readelf.c - display information about ELF files.
 *
 * Copyright 2019 The Android Open Source Project
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/nm.html

USE_READELF(NEWTOY(readelf, "<1(dyn-syms)Aadehlnp:SsWx:", TOYFLAG_USR|TOYFLAG_BIN))

config READELF
  bool "readelf"
  default y
  help
    usage: readelf [-AadehlnSs] [-p SECTION] [-x SECTION] [file...]

    Displays information about ELF files.

    -A	Show architecture-specific info
    -a	Equivalent to -AdhlnSs
    -d	Show dynamic section
    -e	Headers (equivalent to -hlS)
    -h	Show ELF header
    -l	Show program headers
    -n	Show notes
    -p S	Dump strings found in named/numbered section
    -S	Show section headers
    -s	Show symbol tables (.dynsym and .symtab)
    -x S	Hex dump of named/numbered section

    --dyn-syms	Show just .dynsym symbol table
*/

#define FOR_readelf
#include "toys.h"

GLOBALS(
  char *x, *p;

  char *elf, *shstrtab, *f;
  unsigned long long shoff, phoff, size, shstrtabsz;
  int bits, endian, shnum, shentsize, phentsize;
)

// Section header.
struct sh {
  unsigned type, link, info;
  unsigned long long flags, addr, offset, size, addralign, entsize;
  char *name;
};

// Program header.
struct ph {
  unsigned type, flags;
  unsigned long long offset, vaddr, paddr, filesz, memsz, align;
};

static long long elf_get(char **p, int len)
{
  long long result;

  if (*p+len-TT.elf>TT.size)
    perror_exit("Access off end: %td[%d] of %lld\n", *p-TT.elf, len, TT.size);

  result = ((TT.endian == 2) ? peek_be : peek_le)(*p, len);
  *p += len;
  return result;
}

static unsigned long long elf_long(char **p)
{
  return elf_get(p, 4*(TT.bits+1));
}

static unsigned elf_int(char **p)
{
  return elf_get(p, 4);
}

static unsigned short elf_short(char **p)
{
  return elf_get(p, 2);
}

static int fits(char *what, int n, unsigned long long off, unsigned long long size)
{
  if (off > TT.size || size > TT.size || off > TT.size-size) {
    if (n == -1) *toybuf = 0;
    else snprintf(toybuf, sizeof(toybuf), " %d", n);
    printf("%s%s's offset %llu + size %llu > file size %llu\n",
      what, toybuf, off, size, TT.size);
    return 0;
  }
  return 1;
}

static int get_sh(unsigned i, struct sh *s)
{
  char *shdr = TT.elf+TT.shoff+i*TT.shentsize;
  unsigned name_offset;

  if (i >= TT.shnum || shdr > TT.elf+TT.size-TT.shentsize) {
    printf("No shdr %d\n", i);
    return 0;
  }

  name_offset = elf_int(&shdr);
  s->type = elf_int(&shdr);
  s->flags = elf_long(&shdr);
  s->addr = elf_long(&shdr);
  s->offset = elf_long(&shdr);
  s->size = elf_long(&shdr);
  s->link = elf_int(&shdr);
  s->info = elf_int(&shdr);
  s->addralign = elf_long(&shdr);
  s->entsize = elf_long(&shdr);

  if (s->type != 8 && !fits("section header", i, s->offset, s->size)) return 0;

  if (!TT.shstrtab) s->name = "?";
  else {
    s->name = TT.shstrtab + name_offset;
    if (name_offset > TT.shstrtabsz || s->name >= TT.elf+TT.size) {
      printf("Bad name for sh %d\n", i);
      return 0;
    }
  }

  return 1;
}

static int find_section(char *spec, struct sh *s)
{
  char *end;
  unsigned i;

  if (!spec) return 0;

  // Valid section number?
  i = estrtol(spec, &end, 0);
  if (!errno && !*end && i<TT.shnum) return get_sh(i, s);

  // Search the section names.
  for (i=0; i<TT.shnum; i++)
    if (get_sh(i, s) && !strcmp(s->name, spec)) return 1;

  error_msg("%s: no section '%s", TT.f, spec);
  return 0;
}

static int get_ph(int i, struct ph *ph)
{
  char *phdr = TT.elf+TT.phoff+i*TT.phentsize;

  if (phdr > TT.elf+TT.size-TT.phentsize) {
    printf("Bad phdr %d\n", i);
    return 0;
  }

  // Elf64_Phdr reordered fields.
  ph->type = elf_int(&phdr);
  if (TT.bits) {
    ph->flags = elf_int(&phdr);
    ph->offset = elf_long(&phdr);
    ph->vaddr = elf_long(&phdr);
    ph->paddr = elf_long(&phdr);
    ph->filesz = elf_long(&phdr);
    ph->memsz = elf_long(&phdr);
    ph->align = elf_long(&phdr);
  } else {
    ph->offset = elf_int(&phdr);
    ph->vaddr = elf_int(&phdr);
    ph->paddr = elf_int(&phdr);
    ph->filesz = elf_int(&phdr);
    ph->memsz = elf_int(&phdr);
    ph->flags = elf_int(&phdr);
    ph->align = elf_int(&phdr);
  }

  if (!fits("program header", i, ph->offset, ph->filesz)) return 0;
  return 1;
}

#define MAP(...) __VA_ARGS__
#define DECODER(name, values) \
  static char *name(int type) { \
    static char unknown[20]; \
    struct {int v; char *s;} a[] = values; \
    int i; \
    \
    for (i=0; i<ARRAY_LEN(a); i++) if (type==a[i].v) return a[i].s; \
    sprintf(unknown, "0x%x", type); \
    return unknown; \
  }

DECODER(dt_type, MAP({{0,"x(NULL)"},{1,"N(NEEDED)"},{2,"b(PLTRELSZ)"},
  {3,"x(PLTGOT)"},{4,"x(HASH)"},{5,"x(STRTAB)"},{6,"x(SYMTAB)"},{7,"x(RELA)"},
  {8,"b(RELASZ)"},{9,"b(RELAENT)"},{10,"b(STRSZ)"},{11,"b(SYMENT)"},
  {12,"x(INIT)"},{13,"x(FINI)"},{14,"S(SONAME)"},{15,"R(RPATH)"},
  {16,"x(SYMBOLIC)"},{17,"x(REL)"},{18,"b(RELSZ)"},{19,"b(RELENT)"},
  {20,"P(PLTREL)"},{21,"x(DEBUG)"},{22,"x(TEXTREL)"},{23,"x(JMPREL)"},
  {24,"d(BIND_NOW)"},{25,"x(INIT_ARRAY)"},{26,"x(FINI_ARRAY)"},
  {27,"b(INIT_ARRAYSZ)"},{28,"b(FINI_ARRAYSZ)"},{29,"R(RUNPATH)"},
  {30,"f(FLAGS)"},{32,"x(PREINIT_ARRAY)"},{33,"x(PREINIT_ARRAYSZ)"},
  {35,"b(RELRSZ)"},{36,"x(RELR)"},{37,"b(RELRENT)"},
  {0x6000000f,"x(ANDROID_REL)"},{0x60000010,"b(ANDROID_RELSZ)"},
  {0x60000011,"x(ANDROID_RELA)"},{0x60000012,"b(ANDROID_RELASZ)"},
  {0x6fffe000,"x(ANDROID_RELR)"},{0x6fffe001,"b(ANDROID_RELRSZ)"},
  {0x6fffe003,"x(ANDROID_RELRENT)"},{0x6ffffef5,"x(GNU_HASH)"},
  {0x6ffffef6,"x(TLSDESC_PLT)"},{0x6ffffef7,"x(TLSDESC_GOT)"},
  {0x6ffffff0,"x(VERSYM)"},{0x6ffffff9,"d(RELACOUNT)"},
  {0x6ffffffa,"d(RELCOUNT)"},{0x6ffffffb,"F(FLAGS_1)"},
  {0x6ffffffc," (VERDEF)"},{0x6ffffffd,"d(VERDEFNUM)"},
  {0x6ffffffe,"x(VERNEED)"},{0x6fffffff,"d(VERNEEDNUM)"}}))

DECODER(et_type, MAP({{0,"NONE (None)"},{1,"REL (Relocatable file)"},
  {2,"EXEC (Executable file)"},{3,"DYN (Shared object file)"},
  {4,"CORE (Core file)"}}))

DECODER(nt_type_core, MAP({{1,"NT_PRSTATUS"},{2,"NT_FPREGSET"},
  {3,"NT_PRPSINFO"},{5,"NT_PLATFORM"},{6,"NT_AUXV"},
  {0x46494c45,"NT_FILE"},{0x53494749,"NT_SIGINFO"}}))

DECODER(nt_type_linux, MAP({{0x200,"NT_386_TLS"},{0x202, "NT_X86_XSTATE"},
  {0x400,"NT_ARM_VFP"},{0x401,"NT_ARM_TLS"},{0x405,"NT_ARM_SVE"}}))

DECODER(os_abi, MAP({{0,"UNIX - System V"}}))

DECODER(ph_type, MAP({{0,"NULL"},{1,"LOAD"},{2,"DYNAMIC"},{3,"INTERP"},
  {4,"NOTE"},{5,"SHLIB"},{6,"PHDR"},{7,"TLS"},{0x6474e550,"GNU_EH_FRAME"},
  {0x6474e551,"GNU_STACK"},{0x6474e552,"GNU_RELRO"},{0x70000001,"EXIDX"}}))

DECODER(sh_type, MAP({{0,"NULL"},{1,"PROGBITS"},{2,"SYMTAB"},{3,"STRTAB"},
  {4,"RELA"},{5,"HASH"},{6,"DYNAMIC"},{7,"NOTE"},{8,"NOBITS"},{9,"REL"},
  {10,"SHLIB"},{11,"DYNSYM"},{14,"INIT_ARRAY"},{15,"FINI_ARRAY"},
  {16,"PREINIT_ARRAY"},{17,"GROUP"},{18,"SYMTAB_SHNDX"},{19,"RELR"},
  {0x60000001,"ANDROID_REL"},{0x60000002,"ANDROID_RELA"},
  {0x6fffff00,"ANDROID_RELR"},{0x6ffffff6,"GNU_HASH"},
  {0x6ffffffd,"VERDEF"},{0x6ffffffe,"VERNEED"},
  {0x6fffffff,"VERSYM"},{0x70000001,"ARM_EXIDX"},
  {0x70000003,"ATTRIBUTES"}}))

DECODER(stb_type, MAP({{0,"LOCAL"},{1,"GLOBAL"},{2,"WEAK"}}))

DECODER(stt_type, MAP({{0,"NOTYPE"},{1,"OBJECT"},{2,"FUNC"},{3,"SECTION"},
  {4,"FILE"},{5,"COMMON"},{6,"TLS"},{10,"GNU_IFUNC"}}))

DECODER(stv_type, MAP({{0,"DEFAULT"},{1,"INTERNAL"},{2,"HIDDEN"},
  {3,"PROTECTED"}}))

DECODER(riscv_attr_tag, MAP({{4,"stack_align"},{5,"arch"},
  {6,"unaligned_access"},{8,"priv_spec"},{10,"priv_spec_minor"},
  {12,"priv_spec_revision"},{14,"atomic_abi"},{16,"x3_reg_usage"}}))

static void show_symbols(struct sh *table, struct sh *strtab)
{
  char *symtab = TT.elf+table->offset, *ndx;
  int numsym = table->size/(TT.bits ? 24 : 16), i;

  if (!numsym) return;

  xputc('\n');
  printf("Symbol table '%s' contains %d entries:\n"
         "   Num:    %*s  Size Type    Bind   Vis      Ndx Name\n",
         table->name, numsym, 5+8*TT.bits, "Value");
  for (i=0; i<numsym; i++) {
    unsigned st_name = elf_int(&symtab), st_value, st_shndx, st_info, st_other;
    unsigned long st_size;
    char *name, buf[16];

    // The various fields were moved around for 64-bit.
    if (TT.bits) {
      st_info = *symtab++;
      st_other = *symtab++;
      st_shndx = elf_short(&symtab);
      st_value = elf_long(&symtab);
      st_size = elf_long(&symtab);
    } else {
      st_value = elf_int(&symtab);
      st_size = elf_int(&symtab);
      st_info = *symtab++;
      st_other = *symtab++;
      st_shndx = elf_short(&symtab);
    }

    // TODO: why do we trust name to be null terminated?
    name = TT.elf + strtab->offset + st_name;
    if (name >= TT.elf+TT.size) name = "???";

    if (!st_shndx) ndx = "UND";
    else if (st_shndx==0xfff1) ndx = "ABS";
    else sprintf(ndx = buf, "%d", st_shndx);

    // TODO: look up and show any symbol versions with @ or @@.

    printf("%6d: %0*x %5lu %-7s %-6s %-9s%3s %s\n", i, 8*(TT.bits+1),
      st_value, st_size, stt_type(st_info & 0xf), stb_type(st_info >> 4),
      stv_type(st_other & 3), ndx, name);
  }
}

static int notematch(int namesz, char **p, char *expected)
{
  if (namesz!=strlen(expected)+1 || strcmp(*p, expected)) return 0;
  *p += namesz;

  return 1;
}

static unsigned long long uleb(char **ptr, char *end)
{
  unsigned long long result = 0;
  int shift = 0;
  unsigned char b;

  do {
    if (*ptr >= end) error_exit("EOF in uleb128");
    b = **ptr;
    *ptr = *ptr + 1;
    result |= (b & 0x7f) << shift;
    shift += 7;
    if (shift > 56) error_exit("uleb128 too long");
  } while ((b & 0x80));
  return result;
}

static void show_attributes(unsigned long offset, unsigned long size)
{
  char *attr = TT.elf + offset, *end = TT.elf + offset + size;
  unsigned long long tag;
  unsigned len;

  // Attributes sections start with an 'A'...
  if (offset == size || *attr++ != 'A')
    return error_msg("%s: bad attributes @%lu", TT.f, offset);

  // ...followed by vendor-specific subsections.
  // TODO: there's a loop implied there, but i've never seen >1 subsection.

  // A subsection starts with a uint32 length and ASCII vendor name.
  len = elf_int(&attr);
  if (!memchr(attr, 0, 32)) return error_msg("%s: bad vendor name", TT.f);
  printf("\nAttribute Section: %s\n", attr);
  attr += strlen(attr) + 1;

  // ...followed by one or more sub-subsections.
  // TODO: there's a loop implied there, but i've never seen >1 sub-subsection.
  // A sub-subsection starts with a uleb128 tag and uint32 length.
  tag = uleb(&attr, end);
  len = elf_int(&attr);
  if (tag == 1) {
    printf("File Attributes\n");

    // ...followed by actual attribute tag/value pairs.
    while (attr < end) {
      tag = uleb(&attr, end);

      // TODO: arm tags don't seem to follow any pattern?
      printf("  Tag_RISCV_%s: ", riscv_attr_tag(tag));
      // Even riscv tags have uleb128 values, odd ones strings.
      if (!(tag & 1)) printf("%lld\n", uleb(&attr, end));
      else {
        printf("%s\n", attr);
        attr += strlen(attr) + 1;
      }
    }
  } else {
    // Do other tags exist?
    error_msg("%s: unknown attributes tag=%llx (size=%u)\n", TT.f, tag, len);
  }
}

static void show_notes(unsigned long offset, unsigned long size)
{
  char *note = TT.elf + offset, *end = TT.elf + offset + size;

  if (!fits("note", -1, offset, size)) return;

  printf("  %-20s%11s\tDescription\n", "Owner", "Data size");
  while (note < end) {
    char *p = note, *desc;
    unsigned namesz=elf_int(&p),descsz=elf_int(&p),type=elf_int(&p),j=0;

    if (namesz > size || descsz > size)
      return error_msg("%s: bad note @%lu", TT.f, offset);
    printf("  %-20.*s 0x%08x\t", namesz, p, descsz);
    if (notematch(namesz, &p, "GNU")) {
      if (type == 1) {
        printf("NT_GNU_ABI_TAG\tOS: %s, ABI: %u.%u.%u",
          !elf_int(&p)?"Linux":"?", elf_int(&p), elf_int(&p), elf_int(&p)), j=1;
      } else if (type == 3) {
        printf("NT_GNU_BUILD_ID\t");
        for (;j<descsz;j++) printf("%02x", *p++);
      } else if (type == 4) {
        printf("NT_GNU_GOLD_VERSION\t%.*s", descsz, p), j=1;
      } else if (type == 5) {
        printf("NT_GNU_PROPERTY_TYPE_0\n    Properties:");
        while (descsz - j > 0) {
          int pr_type = elf_int(&p);
          int pr_size = elf_int(&p), k, pr_data;

          j += 8;
          if (p > end) return error_msg("%s: bad property @%lu", TT.f, offset);
          if (pr_size != 4) {
            // Just hex dump anything we aren't familiar with.
            for (k=0;k<pr_size;k++) printf("%02x", *p++);
            xputc('\n');
            j += pr_size;
          } else {
            pr_data = elf_int(&p);
            elf_int(&p); // Skip padding.
            j += 8;
            if (pr_type == 0xc0000000) {
              printf("\tarm64 features:");
              if (pr_data & 1) printf(" bti");
              if (pr_data & 2) printf(" pac");
              xputc('\n');
            } else if (pr_type == 0xc0000002) {
              printf("\tx86 feature:");
              if (pr_data & 1) printf(" ibt");
              if (pr_data & 2) printf(" shstk");
              xputc('\n');
            } else if (pr_type == 0xc0008002) {
              printf("\tx86 isa needed: x86-64v%d", ffs(pr_data));
            } else {
              printf("\tother (%#x): %#x", pr_type, pr_data);
            }
          }
        }
      } else p -= 4;
    } else if (notematch(namesz, &p, "Android")) {
      if (type == 1) {
        printf("NT_VERSION\tAPI level %u", elf_int(&p)), j=1;
        if (descsz>=132) printf(", NDK %.64s (%.64s)", p, p+64);
      } else if (type == 5) {
        printf("NT_PAD_SEGMENT\tpad_segment=%u", elf_int(&p)), j=1;
      } else p -= 8;
    } else if (notematch(namesz, &p, "CORE")) {
      if (*(desc = nt_type_core(type)) != '0') printf("%s", desc), j=1;
    } else if (notematch(namesz, &p, "LINUX")) {
      if (*(desc = nt_type_linux(type)) != '0') printf("%s", desc), j=1;
    }

    // If we didn't do custom output above, show a hex dump.
    if (!j) {
      printf("0x%x\t", type);
      for (;j<descsz;j++) printf("%c%02x", j ? ' ' : '\t', *p++/*note[16+j]*/);
    }
    xputc('\n');
    note += 3*4 + ((namesz+3)&~3) + ((descsz+3)&~3);
  }
}

static void scan_elf()
{
  struct sh dynamic = {}, dynstr = {}, dynsym = {}, shstr = {}, strtab = {},
    symtab = {}, s;
  struct ph ph;
  char *hdr = TT.elf;
  int type, machine, version, flags, entry, ehsize, phnum, shstrndx, i, j, w;

  if (TT.size < 45 || smemcmp(hdr, "\177ELF", 4))
    return error_msg("%s: not ELF", TT.f);

  TT.bits = hdr[4] - 1;
  TT.endian = hdr[5];
  if (TT.bits<0 || TT.bits>1 || TT.endian<1 || TT.endian>2 || hdr[6]!=1)
    return error_msg("%s: bad ELF", TT.f);

  hdr += 16; // EI_NIDENT
  type = elf_short(&hdr);
  machine = elf_short(&hdr);
  version = elf_int(&hdr);
  entry = elf_long(&hdr);
  TT.phoff = elf_long(&hdr);
  TT.shoff = elf_long(&hdr);
  flags = elf_int(&hdr);
  ehsize = elf_short(&hdr);
  TT.phentsize = elf_short(&hdr);
  phnum = elf_short(&hdr);
  TT.shentsize = elf_short(&hdr);
  TT.shnum = elf_short(&hdr);
  shstrndx = elf_short(&hdr);

  if (toys.optc > 1) printf("\nFile: %s\n", TT.f);

  if (FLAG(h)) {
    printf("ELF Header:\n");
    printf("  Magic:   ");
    for (i=0; i<16; i++) printf("%02x%c", TT.elf[i], (i==15) ? '\n' : ' ');
    printf("  Class:                             ELF%d\n", TT.bits?64:32);
    printf("  Data:                              2's complement, %s endian\n",
           (TT.endian==2)?"big":"little");
    printf("  Version:                           1 (current)\n");
    printf("  OS/ABI:                            %s\n", os_abi(TT.elf[7]));
    printf("  ABI Version:                       %d\n", TT.elf[8]);
    printf("  Type:                              %s\n", et_type(type));
    printf("  Machine:                           %s\n", elf_arch_name(machine));
    printf("  Version:                           0x%x\n", version);
    printf("  Entry point address:               0x%x\n", entry);
    printf("  Start of program headers:          %llu (bytes into file)\n",
           TT.phoff);
    printf("  Start of section headers:          %llu (bytes into file)\n",
           TT.shoff);
    printf("  Flags:                             0x%x", flags);
    elf_print_flags(machine, flags); putchar('\n');
    printf("  Size of this header:               %d (bytes)\n", ehsize);
    printf("  Size of program headers:           %d (bytes)\n", TT.phentsize);
    printf("  Number of program headers:         %d\n", phnum);
    printf("  Size of section headers:           %d (bytes)\n", TT.shentsize);
    printf("  Number of section headers:         %d\n", TT.shnum);
    printf("  Section header string table index: %d\n", shstrndx);
  }
  if (TT.phoff > TT.size) return error_msg("%s: bad phoff", TT.f);
  if (TT.shoff > TT.size) return error_msg("%s: bad shoff", TT.f);

  // Set up the section header string table so we can use section header names.
  // Core files have shstrndx == 0.
  TT.shstrtab = 0;
  TT.shstrtabsz = 0;
  if (shstrndx) {
    if (!get_sh(shstrndx, &shstr) || shstr.type != 3 /*SHT_STRTAB*/)
      return error_msg("%s: bad shstrndx", TT.f);
    TT.shstrtab = TT.elf+shstr.offset;
    TT.shstrtabsz = shstr.size;
  }

  w = 8<<TT.bits;
  if (FLAG(S)) {
    if (!TT.shnum) printf("\nThere are no sections in this file.\n");
    else {
      if (!FLAG(h))
        printf("There are %d section headers, starting at offset %#llx:\n",
               TT.shnum, TT.shoff);
      printf("\nSection Headers:\n"
             "  [Nr] %-17s %-15s %-*s %-6s %-6s ES Flg Lk Inf Al\n",
             "Name", "Type", w, "Address", "Off", "Size");
    }
  }
  // We need to iterate through the section headers even if we're not
  // dumping them, to find specific sections.
  for (i=0; i<TT.shnum; i++) {
    if (!get_sh(i, &s)) continue;
    if (s.type == 2 /*SHT_SYMTAB*/) symtab = s;
    else if (s.type == 6 /*SHT_DYNAMIC*/) dynamic = s;
    else if (s.type == 11 /*SHT_DYNSYM*/) dynsym = s;
    else if (s.type == 3 /*SHT_STRTAB*/) {
      if (!strcmp(s.name, ".strtab")) strtab = s;
      else if (!strcmp(s.name, ".dynstr")) dynstr = s;
    }

    if (FLAG(S)) {
      char sh_flags[12] = {}, *p = sh_flags;

      for (j=0; j<12; j++) if (s.flags&(1<<j)) *p++ = "WAXxMSILOGTC"[j];
      printf("  [%2d] %-17s %-15s %0*llx %06llx %06llx %02llx %3s %2d %2d %2lld\n",
             i, s.name, sh_type(s.type), w, s.addr, s.offset, s.size,
             s.entsize, sh_flags, s.link, s.info, s.addralign);
    }
  }
  if (FLAG(S) && TT.shnum)
    printf("Key:\n  (W)rite, (A)lloc, e(X)ecute, (M)erge, (S)trings, (I)nfo\n"
           "  (L)ink order, (O)S, (G)roup, (T)LS, (C)ompressed, x=unknown\n");

  if (FLAG(l)) {
    xputc('\n');
    if (!phnum) printf("There are no program headers in this file.\n");
    else {
      if (!FLAG(h))
        printf("Elf file type is %s\nEntry point %#x\n"
          "There are %d program headers, starting at offset %lld\n\n",
          et_type(type), entry, phnum, TT.phoff);
      printf("Program Headers:\n"
             "  %-14s %-8s %-*s   %-*s   %-7s %-7s Flg Align\n", "Type",
             "Offset", w, "VirtAddr", w, "PhysAddr", "FileSiz", "MemSiz");
      for (i = 0; i<phnum; i++) {
        if (!get_ph(i, &ph)) continue;
        printf("  %-14s 0x%06llx 0x%0*llx 0x%0*llx 0x%05llx 0x%05llx %c%c%c %#llx\n",
               ph_type(ph.type), ph.offset, w, ph.vaddr, w, ph.paddr,
               ph.filesz, ph.memsz, (ph.flags&4)?'R':' ', (ph.flags&2)?'W':' ',
               (ph.flags&1)?'E':' ', ph.align);
        if (ph.type == 3 /*PH_INTERP*/ && ph.filesz<TT.size &&
            ph.offset<TT.size && ph.filesz - 1 < TT.size - ph.offset) {
// TODO: ph.filesz of 0 prints unlimited length string
          printf("      [Requesting program interpreter: %*s]\n",
                 (int) ph.filesz-1, TT.elf+ph.offset);
        }
      }

      printf("\n Section to Segment mapping:\n  Segment Sections...\n");
      for (i=0; i<phnum; i++) {
        if (!get_ph(i, &ph)) continue;
        printf("   %02d     ", i);
        for (j=0; j<TT.shnum; j++) {
          if (!get_sh(j, &s)) continue;
          if (!*s.name) continue;
          if (s.offset >= ph.offset && s.offset+s.size <= ph.offset+ph.filesz)
            printf("%s ", s.name);
        }
        xputc('\n');
      }
    }
  }

  // binutils ld emits a bunch of extra DT_NULL entries, so binutils readelf
  // uses two passes here! We just tell the truth, which matches -h.
  if (FLAG(d)) {
    char *dyn = TT.elf+dynamic.offset, *end = dyn+dynamic.size;

    xputc('\n');
    if (!dynamic.size) printf("There is no dynamic section in this file.\n");
    else if (!dynamic.entsize) printf("Bad dynamic entry size 0!\n");
    else {
      printf("Dynamic section at offset 0x%llx contains %lld entries:\n"
             "  %-*s %-20s %s\n", dynamic.offset, dynamic.size/dynamic.entsize,
             w+2, "Tag", "Type", "Name/Value");
      while (dyn < end) {
        unsigned long long tag = elf_long(&dyn), val = elf_long(&dyn);
        char *type = dt_type(tag);

        printf(" 0x%0*llx %-20s ", w, tag, type+(*type!='0'));
        if (*type == 'd') printf("%lld\n", val);
        else if (*type == 'b') printf("%lld (bytes)\n", val);
// TODO: trusting this %s to be null terminated
        else if (*type == 's') printf("%s\n", TT.elf+dynstr.offset+val);
        else if (*type == 'f' || *type == 'F') {
          struct bitname { int bit; char *s; }
            df_names[] = {{0, "ORIGIN"},{1,"SYMBOLIC"},{2,"TEXTREL"},
              {3,"BIND_NOW"},{4,"STATIC_TLS"},{}},
            df_1_names[]={{0,"NOW"},{1,"GLOBAL"},{2,"GROUP"},{3,"NODELETE"},
              {5,"INITFIRST"},{27,"PIE"},{}},
            *names = *type == 'f' ? df_names : df_1_names;
          int mask;

          if (*type == 'F') printf("Flags: ");
          for (j=0; names[j].s; j++)
            if (val & (mask=(1<<names[j].bit)))
              printf("%s%s", names[j].s, (val &= ~mask) ? " " : "");
          if (val) printf("0x%llx", val);
          xputc('\n');
        } else if (*type == 'N' || *type == 'R' || *type == 'S') {
          char *s = TT.elf+dynstr.offset+val;

          if (dynstr.offset>TT.size || val>TT.size || dynstr.offset>TT.size-val)
            s = "???";
          printf("%s: [%s]\n", *type=='N' ? "Shared library" :
            (*type=='R' ? "Library runpath" : "Library soname"), s);
        } else if (*type == 'P') {
          j = strlen(type = dt_type(val));
          if (*type != '0') type += 2, j -= 3;
          printf("%*.*s\n", j, j, type);
        } else printf("0x%llx\n", val);
      }
    }
  }

  if (FLAG(dyn_syms)) show_symbols(&dynsym, &dynstr);
  if (FLAG(s)) show_symbols(&symtab, &strtab);

  if (FLAG(n)) {
    int found = 0;

    for (i=0; i<TT.shnum; i++) {
      if (!get_sh(i, &s)) continue;
      if (s.type == 7 /*SHT_NOTE*/) {
        printf("\nDisplaying notes found in: %s\n", s.name);
        show_notes(s.offset, s.size);
        found = 1;
      }
    }
    for (i=0; !found && i<phnum; i++) {
      if (!get_ph(i, &ph)) continue;
      if (ph.type == 4 /*PT_NOTE*/) {
        printf("\n"
          "Displaying notes found at file offset 0x%llx with length 0x%llx:\n",
          ph.offset, ph.filesz);
        show_notes(ph.offset, ph.filesz);
      }
    }
  }

  // TODO: ARC/ARM/CSKY have these too.
  if (FLAG(A) && machine == 243) { // RISCV
    for (i=0; i<TT.shnum; i++) {
      if (!get_sh(i, &s)) continue;
      if (s.type == 0x70000003 /*SHT_RISCV_ATTRIBUTES*/) {
        show_attributes(s.offset, s.size);
      }
    }
  }

  if (find_section(TT.x, &s)) {
    char *p = TT.elf+s.offset;
    long offset = 0;

    printf("\nHex dump of section '%s':\n", s.name);
    while (offset < s.size) {
      int space = 2*16 + 16/4;

      printf("  0x%08lx ", offset);
      for (i=0; i<16 && offset < s.size; offset++)
        space -= printf("%02x%s", *p++, " "+!!(++i%4));
      printf("%*s", space, "");
      for (p -= i; i; i--, p++) putchar((*p>=' ' && *p<='~') ? *p : '.');
      xputc('\n');
    }
    xputc('\n');
  }

  if (find_section(TT.p, &s)) {
    char *begin = TT.elf+s.offset, *end = begin + s.size, *p = begin;
    int any = 0;

    printf("\nString dump of section '%s':\n", s.name);
    for (; p < end; p++) {
      if (isprint(*p)) {
        printf("  [%6tx]  ", p-begin);
        while (p < end && isprint(*p)) putchar(*p++);
        xputc('\n');
        any=1;
      }
    }
    if (!any) printf("  No strings found in this section.\n");
    xputc('\n');
  }
}

void readelf_main(void)
{
  char **arg;
  int all = FLAG_A|FLAG_d|FLAG_h|FLAG_l|FLAG_n|FLAG_S|FLAG_s|FLAG_dyn_syms;

  if (FLAG(a)) toys.optflags |= all;
  if (FLAG(e)) toys.optflags |= FLAG_h|FLAG_l|FLAG_S;
  if (FLAG(s)) toys.optflags |= FLAG_dyn_syms;
  if (!(toys.optflags & (all|FLAG_p|FLAG_x))) help_exit("needs a flag");

  for (arg = toys.optargs; *arg; arg++) {
    int fd = open(TT.f = *arg, O_RDONLY);
    struct stat sb;

    if (fd == -1) perror_msg_raw(TT.f);
    else {
      if (fstat(fd, &sb)) perror_msg_raw(TT.f);
      else if (!sb.st_size) error_msg("%s: empty", TT.f);
      else if (!S_ISREG(sb.st_mode)) error_msg("%s: not a regular file",TT.f);
      else {
        TT.elf = xmmap(0, TT.size=sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
        scan_elf();
        munmap(TT.elf, TT.size);
      }
      close(fd);
    }
  }
}
