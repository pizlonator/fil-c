/* file.c - describe file type
 *
 * Copyright 2016 The Android Open Source Project
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/file.html

USE_FILE(NEWTOY(file, "<1b(brief)hLs[!hL]", TOYFLAG_USR|TOYFLAG_BIN))

config FILE
  bool "file"
  default y
  help
    usage: file [-bhLs] [FILE...]

    Examine the given files and describe their content types.

    -b	Brief (no filename)
    -h	Don't follow symlinks (default)
    -L	Follow symlinks
    -s	Show block/char device contents
*/

#define FOR_file
#include "toys.h"

GLOBALS(
  int max_name_len;
  off_t len;
)

// We don't trust elf.h to be there, and two codepaths for 32/64 is awkward
// anyway, so calculate struct offsets manually. (It's a fixed ABI.)
static void do_elf_file(int fd)
{
  unsigned endian = toybuf[5], bits = toybuf[4]-1, i, j, dynamic = 0,
           stripped = 1, phentsize, phnum, shsize, shnum, bail = 0, arch;
  long long (*elf_int)(void *ptr, unsigned size) = (endian==2)?peek_be:peek_le;
  char *map = MAP_FAILED;
  unsigned long phoff, shoff;

  printf("ELF ");

  // executable type
  i = elf_int(toybuf+16, 2);
  if (i == 1) printf("relocatable");
  else if (i == 2) printf("executable");
  else if (i == 3) printf("shared object");
  else if (i == 4) printf("core dump");
  else {
    printf("(bad type %d)", i);
    bail++;
  }
  if (elf_int(toybuf+36+12*!!bits, 4) & 0x8000) printf(" (fdpic)");
  printf(", ");

  // "64-bit"
  if (bits&~1) {
    printf("(bad class %d) ", bits);
    bail++;
  } else printf("%d-bit ", 32<<bits);

  // "LSB"
  if (endian == 1) printf("LSB ");
  else if (endian == 2) printf("MSB ");
  else {
    printf("(bad endian %d) ", endian);
    bail++;
  }

  // "x86".
  printf("%s", elf_arch_name(arch = elf_int(toybuf+18, 2)));

  // If what we've seen so far doesn't seem consistent, bail.
  if (bail) goto bad;

  elf_print_flags(arch, elf_int(toybuf+36+12*bits, 4));

  // Stash what we need from the header; it's okay to reuse toybuf after this.
  phentsize = elf_int(toybuf+42+12*bits, 2);
  phnum = elf_int(toybuf+44+12*bits, 2);
  phoff = elf_int(toybuf+28+4*bits, 4+4*bits);
  shsize = elf_int(toybuf+46+12*bits, 2);
  shnum = elf_int(toybuf+48+12*bits, 2);
  shoff = elf_int(toybuf+32+8*bits, 4+4*bits);

  // With binutils, phentsize seems to only be non-zero if phnum is non-zero.
  // Such ELF files are rare, but do exist. (Android's crtbegin files, say.)
  if (phnum && (phentsize != 32+24*bits)) {
    printf(", bad phentsize %d?", phentsize);
    goto bad;
  }
  if (phoff>TT.len || phnum*phentsize>TT.len-phoff) {
    printf(", bad phoff %lu?", phoff);
    goto bad;
  }
  if (shoff>TT.len || shnum*shsize>TT.len-shoff) {
    printf(", bad shoff %lu?", phoff);
    goto bad;
  }

  // Parsing ELF means following tables that may point to data earlier in
  // the file, so sequential reading involves buffering unknown amounts of
  // data. Just skip it if we can't mmap.
  if (MAP_FAILED == (map = mmap(0, TT.len, PROT_READ, MAP_SHARED, fd, 0))) {
    perror_msg("mmap");
    goto bad;
  }

  // Read the phdrs for dynamic vs static. (Note: fields reordered on 64 bit)
  for (i = 0; i<phnum; i++) {
    char *phdr = map+phoff+i*phentsize;
    unsigned p_type = elf_int(phdr, 4);
    unsigned long long p_offset, p_filesz;

    // TODO: what does PT_DYNAMIC without PT_INTERP mean?
    if (p_type-2>2) continue; // 2 = PT_DYNAMIC, 3 = PT_INTERP, 4 = PT_NOTE
    dynamic |= p_type==2;
    p_offset = elf_int(phdr+(4<<bits), 4<<bits);
    p_filesz = elf_int(phdr+(16<<bits), 4<<bits);
    if (p_type==3) {
      if (p_filesz>TT.len || p_offset>TT.len-p_filesz) {
        printf(", bad phdr %d?", i);
        goto bad;
      }
      // TODO: if (int)<0 prints endlessly, could go off end of map?
      printf(", dynamic (%.*s)", (int)p_filesz, map+p_offset);
    }
  }
  if (!dynamic) printf(", static");

  // We need to read the shdrs for stripped/unstripped and any notes.
  // Notes are in program headers *and* section headers, but some files don't
  // contain program headers, so check here. (Note: fields reordered on 64 bit)
  for (i = 0; i<shnum; i++) {
    char *shdr = map+shoff+i*shsize;
    unsigned long sh_offset;
    int sh_type, sh_size;

    if (shdr>map+TT.len-(8+(4<<bits))) {
      printf(", bad shdr %d?", i);
      goto bad;
    }
    sh_type = elf_int(shdr+4, 4);
    sh_offset = elf_int(shdr+8+(8<<bits), 4<<bits);
    sh_size = elf_int(shdr+8+(12<<bits), 4);
    if (sh_type == 8 /*SHT_NOBITS*/) sh_size = 0;
    if (sh_offset>TT.len || sh_size>TT.len-sh_offset) {
      printf(", bad shdr %d?", i);
      goto bad;
    }

    if (sh_type == 2 /*SHT_SYMTAB*/) {
      stripped = 0;
      break;
    } else if (sh_type == 7 /*SHT_NOTE*/) {
      char *note = map+sh_offset;

      // An ELF note is a sequence of entries, each consisting of an
      // ndhr followed by n_namesz+n_descsz bytes of data (each of those
      // rounded up to the next 4 bytes, without this being reflected in
      // the header byte counts themselves).
      while (sh_size >= 3*4) { // Don't try to read a truncated entry.
        unsigned n_namesz, n_descsz, n_type, notesz;

        if (note>map+TT.len-3*4) {
          printf(", bad note %d?", i);
          goto bad;
        }

        n_namesz = elf_int(note, 4);
        n_descsz = elf_int(note+4, 4);
        n_type = elf_int(note+8, 4);
        notesz = 3*4 + ((n_namesz+3)&~3) + ((n_descsz+3)&~3);

        // Are the name/desc sizes consistent, and does the claimed size of
        // the note actually fit in the section?
        if (notesz<n_namesz || notesz<n_descsz || notesz>sh_size) {
          printf(", bad note %d size?", i);
          goto bad;
        }

        if (n_namesz==4 && !smemcmp(note+12, "GNU", 4) && n_type==3) {
          printf(", BuildID=");
          for (j = 0; j<n_descsz; j++) printf("%02x", note[16+j]);
        } else if (n_namesz==8 && !smemcmp(note+12, "Android", 8)) {
          if (n_type==1 /*.android.note.ident*/ && n_descsz >= 4) {
            printf(", for Android %d", (int)elf_int(note+20, 4));
            // NDK r14 and later also include NDK version info. OS binaries
            // and binaries built by older NDKs don't have this.
            if (n_descsz >= 4+64+64)
              printf(", built by NDK %.64s (%.64s)", note+24, note+24+64);
          }
        }

        note += notesz;
        sh_size -= notesz;
      }
    }
  }
  printf(", %sstripped", stripped ? "" : "not ");
bad:
  xputc('\n');

  if (map != MAP_FAILED) munmap(map, TT.len);
}

static void do_regular_file(int fd, char *name)
{
  char *s = toybuf;
  unsigned len, magic;
  int ii;

  // zero through elf shnum, just in case
  memset(s, 0, 80);
  if ((len = readall(fd, s, sizeof(toybuf)-8))<0) perror_msg_raw(name);

  if (!len) xputs("empty");
  // 45 bytes: https://www.muppetlabs.com/~breadbox/software/tiny/teensy.html
  else if (len>=45 && strstart(&s, "\177ELF")) do_elf_file(fd);
  else if (strstart(&s, "!<arch>\n")) xputs("ar archive");
  else if (*s=='%' && 2==sscanf(s, "%%PDF%d.%u", &ii, &magic))
    xprintf("PDF document, version %d.%u\n", -ii, magic);
  else if (len>28 && strstart(&s, "\x89PNG\x0d\x0a\x1a\x0a")) {
    // PNG is big-endian: https://www.w3.org/TR/PNG/#7Integers-and-byte-order
    int chunk_length = peek_be(s, 4);

    xprintf("PNG image data");

    // The IHDR chunk comes first: https://www.w3.org/TR/PNG/#11IHDR
    s += 4;
    if (chunk_length == 13 && strstart(&s, "IHDR")) {
      // https://www.w3.org/TR/PNG/#6Colour-values
      char *c = 0, *colors[] = {"grayscale", 0, "color RGB", "indexed color",
                                "grayscale with alpha", 0, "color RGBA"};

      if (s[9]<ARRAY_LEN(colors)) c = colors[s[9]];
      xprintf(", %d x %d, %d-bit/%s, %sinterlaced", (int)peek_be(s, 4),
        (int)peek_be(s+4, 4), s[8], c ? : "unknown", s[12] ? "" : "non-");
    }

    xputc('\n');

  // https://www.w3.org/Graphics/GIF/spec-gif89a.txt
  } else if (len>16 && (strstart(&s, "GIF87a") || strstart(&s, "GIF89a")))
    xprintf("GIF image data, version %3.3s, %d x %d\n",
      s-3, (int)peek_le(s, 2), (int)peek_le(s+2, 2));

  // https://en.wikipedia.org/wiki/JPEG#Syntax_and_structure
  else if (len>32 && !smemcmp(s, "\xff\xd8", 2)) {
    char *types[] = {"baseline", "extended sequential", "progressive"};
    int marker;

    printf("JPEG image data");
    while (s < toybuf+len-8) { // TODO: refill for files with lots of EXIF data?
      marker = peek_be(s, 2);
      if (marker >= 0xffd0 && marker <= 0xffd9) s += 2; // No data.
      else if (marker >= 0xffc0 && marker <= 0xffc2) {
        xprintf(", %s, %dx%d", types[marker-0xffc0], (int) peek_be(s+7, 2),
                (int) peek_be(s+5, 2));
        break;
      } else s += peek_be(s + 2, 2) + 2;
    }
    xputc('\n');

  } else if (len>8 && strstart(&s, "\xca\xfe\xba\xbe")) {
    unsigned count = peek_be(s, 4), i, arch;

    // 0xcafebabe can be a Java class file or a Mach-O universal binary.
    // Java major version numbers start with 0x2d for JDK 1.1, and realistically
    // you're never going to see more than 2 architectures in a binary anyway...
    if (count<0x2d && len>=(count*20)) {
      // https://eclecticlight.co/2020/07/28/universal-binaries-inside-fat-headers/
      xprintf("Mach-O universal binary with %u architecture%s:",
        count, count == 1 ? "" : "s");
      for (i = 0, s += 4; i < count; i++, s += 20) {
        arch = peek_be(s, 4);
        if (arch == 0x00000007) name = "i386";
        else if (arch == 0x01000007) name = "x86_64";
        else if (arch == 0x0000000c) name = "arm";
        else if (arch == 0x0100000c) name = "arm64";
        else name = "unknown";
        xprintf(" [%s]", name);
      }
      xprintf("\n");
    } else {
      // https://en.wikipedia.org/wiki/Java_class_file#General_layout
      xprintf("Java class file, version %d.%d (Java 1.%d)\n",
        (int)peek_be(s+2, 2), (int)peek_be(s, 2), (int)peek_be(s+2, 2)-44);
    }

  // https://source.android.com/devices/tech/dalvik/dex-format#dex-file-magic
  } else if (len>8 && strstart(&s, "dex\n") && !s[3])
    xprintf("Android dex file, version %s\n", s);

  // https://people.freebsd.org/~kientzle/libarchive/man/cpio.5.txt
  // the lengths for cpio are size of header + 9 bytes, since any valid
  // cpio archive ends with a record for "TARGET!!!"
  else if (len>6 && strstart(&s, "07070")) {
    char *cpioformat = "unknown type";

    if (*s == '7') cpioformat = "pre-SVR4 or odc";
    else if (*s == '1') cpioformat = "SVR4 with no CRC";
    else if (*s == '2') cpioformat = "SVR4 with CRC";
    xprintf("ASCII cpio archive (%s)\n", cpioformat);
  } else if (len>33 && ((magic=peek(&s,2))==0143561 || magic==070707)) {
    if (magic == 0143561) printf("byte-swapped ");
    xputs("cpio archive");
  // tar archive (old, ustar/pax, or gnu)
  } else if (len>500 && is_tar_header(s))
    xprintf("%s tar archive%s\n", s[257] ? "POSIX" : "old",
      (s[262]!=' ' || s[263]!=' ')?"":" (GNU)");
  // zip/jar/apk archive, ODF/OOXML document, or such
  else if (len>5 && strstart(&s, "PK\03\04")) {
    xprintf("Zip archive data");
    if (*s) xprintf(", requires at least v%d.%d to extract", *s/10, *s%10);
    xputc('\n');
  } else if (len>9 && strstart(&s, "7z\xbc\xaf\x27\x1c")) {
    xprintf("7-zip archive data");
    if (*s || s[1]) xprintf(", version %d.%d", *s, s[1]);
    xputc('\n');
  } else if (len>4 && strstart(&s, "BZh") && isdigit(*s))
    xprintf("bzip2 compressed data, block size = %c00k\n", *s);
  else if (len>31 && peek_be(s, 7) == 0xfd377a585a0000ULL)
    xputs("xz compressed data");
  else if (len>10 && strstart(&s, "\x28\xb5\x2f\xfd"))
    xputs("zstd compressed data");
  else if (len>10 && strstart(&s, "\x1f\x8b")) xputs("gzip compressed data");
  else if (len>32 && !smemcmp(s+1, "\xfa\xed\xfe", 3)) {
    int bit = (*s==0xce) ? 32 : 64;
    char *what = 0;

    xprintf("Mach-O %d-bit ", bit);

    if (s[4] == 7) what = (bit==32)?"x86":"x86-";
    else if (s[4] == 12) what = "arm";
    else if (s[4] == 18) what = "ppc";
    if (what) xprintf("%s%s ", what, (bit==32)?"":"64");
    else xprintf("(bad arch %d) ", s[4]);

    if (s[12] == 1) what = "object";
    else if (s[12] == 2) what = "executable";
    else if (s[12] == 6) what = "shared library";
    else what = NULL;
    if (what) xprintf("%s\n", what);
    else xprintf("(bad type %d)\n", s[9]);
  } else if (len>36 && !smemcmp(s, "OggS\x00\x02", 6)) {
    xprintf("Ogg data");
    // https://wiki.xiph.org/MIMETypesCodecs
    if (!smemcmp(s+28, "CELT    ", 8)) xprintf(", celt audio");
    else if (!smemcmp(s+28, "CMML    ", 8)) xprintf(", cmml text");
    else if (!smemcmp(s+28, "BBCD", 5)) xprintf(", dirac video");
    else if (!smemcmp(s+28, "\177FLAC", 5)) xprintf(", flac audio");
    else if (!smemcmp(s+28, "\x8bJNG\r\n\x1a\n", 8)) xprintf(", jng video");
    else if (!smemcmp(s+28, "\x80kate\0\0", 8)) xprintf(", kate text");
    else if (!smemcmp(s+28, "OggMIDI", 8)) xprintf(", midi text");
    else if (!smemcmp(s+28, "\x8aMNG\r\n\x1a\n", 8)) xprintf(", mng video");
    else if (!smemcmp(s+28, "OpusHead", 8)) xprintf(", opus audio");
    else if (!smemcmp(s+28, "PCM     ", 8)) xprintf(", pcm audio");
    else if (!smemcmp(s+28, "\x89PNG\r\n\x1a\n", 8)) xprintf(", png video");
    else if (!smemcmp(s+28, "Speex   ", 8)) xprintf(", speex audio");
    else if (!smemcmp(s+28, "\x80theora", 7)) xprintf(", theora video");
    else if (!smemcmp(s+28, "\x01vorbis", 7)) xprintf(", vorbis audio");
    else if (!smemcmp(s+28, "YUV4MPEG", 8)) xprintf(", yuv4mpeg video");
    xputc('\n');
  } else if (len>32 && !smemcmp(s, "RIF", 3) && !smemcmp(s+8, "WAVEfmt ", 8)) {
    // https://en.wikipedia.org/wiki/WAV
    int le = (s[3] == 'F');
    int format = le ? peek_le(s+20, 2) : peek_be(s+20, 2);
    int channels = le ? peek_le(s+22, 2) : peek_be(s+22, 2);
    int hz = le ? peek_le(s+24, 4) : peek_be(s+24, 4);
    int bits = le ? peek_le(s+34, 2) : peek_be(s+34, 2);

    xprintf("WAV audio, %s, ", le ? "LE" : "BE");
    if (bits) xprintf("%d-bit, ", bits);
    if (channels==1||channels==2) xprintf("%s, ",(channels==1)?"mono":"stereo");
    else xprintf("%d-channel, ", channels);
    xprintf("%d Hz, ", hz);
    // See https://tools.ietf.org/html/rfc2361, though there appear to be bugs
    // in the RFC. This assumes wikipedia's example files are more correct.
    if (format == 0x01) xprintf("PCM");
    else if (format == 0x03) xprintf("IEEE float");
    else if (format == 0x06) xprintf("A-law");
    else if (format == 0x07) xprintf("µ-law");
    else if (format == 0x11) xprintf("ADPCM");
    else if (format == 0x22) xprintf("Truespeech");
    else if (format == 0x31) xprintf("GSM");
    else if (format == 0x55) xprintf("MP3");
    else if (format == 0x70) xprintf("CELP");
    else if (format == 0xfffe) xprintf("extensible");
    else xprintf("unknown format %d", format);
    xputc('\n');
  } else if (len>12 && peek_be(s, 4)==0x10000) xputs("TrueType font");
  else if (len>12 && !smemcmp(s, "ttcf", 5)) {
    xprintf("TrueType font collection, version %d, %d fonts\n",
            (int)peek_be(s+4, 2), (int)peek_be(s+8, 4));

  // https://docs.microsoft.com/en-us/typography/opentype/spec/otff
  } else if (len>12 && strstart(&s, "OTTO")) xputs("OpenType font");
  else if (strstart(&s, "BC\xc0\xde")) xputs("LLVM IR bitcode");
  else if (strstart(&s,"-----BEGIN CERTIFICATE-----")) xputs("PEM certificate");

  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms680547(v=vs.85).aspx
  else if (len>0x70 && !smemcmp(s, "MZ", 2) &&
      (magic=peek_le(s+0x3c,4))<len-4 && !smemcmp(s+magic, "\x50\x45\0", 4)) {

    // Linux kernel images look like PE files.
    // https://www.kernel.org/doc/Documentation/arm64/booting.txt
    // I've only ever seen LE, 4KiB pages, so ignore flags for now.
    if (!smemcmp(s+0x38, "ARMd", 4)) return xputs("Linux arm64 kernel image");
    else if (!smemcmp(s+0x202, "HdrS", 4)) {
      // https://www.kernel.org/doc/Documentation/x86/boot.txt
      unsigned ver_off = peek_le(s+0x20e, 2);

      xprintf("Linux x86-64 kernel image");
      if ((0x200 + ver_off) < len) {
        s += 0x200 + ver_off;
      } else {
        if (lseek(fd, ver_off - len + 0x200, SEEK_CUR)<0 ||
            (len = readall(fd, s, sizeof(toybuf)))<0)
          return perror_msg_raw(name);
      }
      xprintf(", version %s\n", s);
      return;
    }

    xprintf("MS PE32%s executable %s", (peek_le(s+magic+24, 2)==0x20b)?"+":"",
        (peek_le(s+magic+22, 2)&0x2000)?"(DLL) ":"");
    if (peek_le(s+magic+20, 2)>70) {
      char *types[] = {0, "native", "GUI", "console", "OS/2", "driver", "CE",
          "EFI", "EFI boot", "EFI runtime", "EFI ROM", "XBOX", 0, "boot"}, *nn;
      unsigned type = peek_le(s+magic+92, 2);

      nn = (type<ARRAY_LEN(types)) ? types[type] : 0;
      xprintf("(%s) ", nn ? : "unknown");
    }
    xprintf("x86%s\n", (peek_le(s+magic+4, 2)==0x14c) ? "" : "-64");

    // https://en.wikipedia.org/wiki/BMP_file_format
  } else if (len>0x32 && !smemcmp(s, "BM", 2) && !peek_be(s+6, 4)) {
    xprintf("BMP image, %d x %d, %d bpp\n", (int)peek_le(s+18, 4),
            (int)peek_le(s+22,4), (int)peek_le(s+28, 2));

    // https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf.data-file-format.txt
  } else if (len>=104 && strstart(&s, "PERFILE2")) xputs("Linux perf data");

    // https://android.googlesource.com/platform/system/core/+/master/libsparse/sparse_format.h
  else if (len>28 && peek_le(s, 4) == 0xed26ff3a) {
    xprintf("Android sparse image v%d.%d, %d %d-byte blocks (%d chunks)\n",
        (int)peek_le(s+4, 2), (int)peek_le(s+6, 2), (int)peek_le(s+16, 4),
        (int)peek_le(s+12, 4), (int)peek_le(s+20, 4));

    // https://android.googlesource.com/platform/system/tools/mkbootimg/+/refs/heads/master/include/bootimg/bootimg.h
  } else if (len>1632 && !smemcmp(s, "ANDROID!", 8)) {
    xprintf("Android boot image v%d\n", (int)peek_le(s+40, 4));

    // https://source.android.com/devices/architecture/dto/partitions
  } else if (len>32 && peek_be(s, 4) == 0xd7b7ab1e) {
    xprintf("Android DTB/DTBO v%d, %d entries\n", (int)peek_be(s+28, 4),
            (int)peek_be(s+16, 4));

    // frameworks/base/core/java/com/android/internal/util/BinaryXmlSerializer.java
  } else if (len>4 && !smemcmp(s, "ABX", 3)) {
    xprintf("Android Binary XML v%d\n", s[3]);

    // https://webassembly.github.io/spec/core/binary/modules.html#binary-module
  } else if (len>8 && !smemcmp(s, "\0asm", 4)) {
    xprintf("wasm binary module version %d\n", (int)peek_le(s+4, 4));

    // Text files, including shell scripts.
  } else {
    char *what = 0;
    int i, bytes;

    // If shell script, report which interpreter
    if (len>3 && strstart(&s, "#!")) {
      // Whitespace is allowed between the #! and the interpreter
      while (isspace(*s)) s++;
      if (strstart(&s, "/usr/bin/env")) while (isspace(*s)) s++;
      for (what = s; *s && !isspace(*s); s++);
      strcpy(s, " script");

    // Distinguish ASCII text, UTF-8 text, or data
    } else for (i = 0; i<len; ++i) {
      if (!(isprint(s[i]) || isspace(s[i]))) {
        unsigned wc;

        if ((bytes = utf8towc(&wc, s+i, len-i))>0 && wcwidth(wc)>=0) {
          i += bytes-1;
          if (!what) what = "UTF-8 text";
        } else {
          what = "data";
          break;
        }
      }
    }
    xputs(what ? what : "ASCII text");
  }
}

void file_main(void)
{
  char **arg;

  for (arg = toys.optargs; *arg; ++arg)
    TT.max_name_len = maxof(strlen(*arg), TT.max_name_len);

  // Can't use loopfiles here because it doesn't call function when can't open
  for (arg = toys.optargs; *arg; arg++) {
    char *name = *arg, *what = "unknown";
    struct stat sb;
    int fd = !strcmp(name, "-");

    if (!FLAG(b))
      xprintf("%s: %*s", name, (int)(TT.max_name_len - strlen(name)), "");

    sb.st_size = 0;
    if (!fd && (FLAG(L) ? stat : lstat)(name, &sb)) {
      xprintf("cannot open: %s\n", strerror(errno));

      continue;
    }

    if (!fd && !FLAG(s) && (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode))) {
      sprintf(what = toybuf, "%s special (%u/%u)",
          S_ISBLK(sb.st_mode) ? "block" : "character",
          dev_major(sb.st_rdev), dev_minor(sb.st_rdev));
    } else if (fd || S_ISREG(sb.st_mode)) {
      TT.len = sb.st_size;
      // This test identifies an empty file we don't have permission to read
      if (!fd && !sb.st_size) what = "empty";
      else if ((fd = openro(name, O_RDONLY)) != -1) {
        do_regular_file(fd, name);
        if (fd) close(fd);

        continue;
      }
    } else if (S_ISFIFO(sb.st_mode)) what = "fifo";
    else if (S_ISDIR(sb.st_mode)) what = "directory";
    else if (S_ISSOCK(sb.st_mode)) what = "socket";
    else if (S_ISLNK(sb.st_mode)) {
      char *lnk = xreadlink(name);

      sprintf(what = toybuf, "%ssymbolic link to %s",
          stat(name, &sb) ? "broken " : "", lnk);
      free(lnk);
    }
    xputs(what);
  }
}
