# ldd.m4
# serial 1
dnl Copyright (C) 2006, 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Sets LDDPROG to a command and LDDPOSTPROC to a filter command, such that
#   $LDDPROG program $LDDPOSTPROC
# outputs a whitespace-separated list of the dynamically linked dependencies
# of the program, as library names (no full pathnames), or nothing if the
# program is statically linked or if the service is not supported on the given
# system.

dnl From Bruno Haible.

AC_DEFUN([gl_LDD],
[
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_REQUIRE([AC_PROG_CC])
  dnl Default values.
  LDDPROG=':'
  LDDPOSTPROC=
  dnl First try objdump, since it works when cross-compiling.
  AC_CHECK_TOOL([OBJDUMP], [objdump], [false])
changequote(,)dnl
  if test "$OBJDUMP" != "false"; then
    LDDPROG="$OBJDUMP -p"
    dnl The output of "LC_ALL=C objdump -p program" of a program or library
    dnl looks like this:
    dnl
    dnl libnet.so:     file format elf32-i386
    dnl
    dnl Program Header:
    dnl     LOAD off    0x00000000 vaddr 0x00000000 paddr 0x00000000 align 2**12
    dnl          filesz 0x0001391d memsz 0x0001391d flags r-x
    dnl     LOAD off    0x00013920 vaddr 0x00014920 paddr 0x00014920 align 2**12
    dnl          filesz 0x00001874 memsz 0x0001b020 flags rw-
    dnl  DYNAMIC off    0x00015104 vaddr 0x00016104 paddr 0x00016104 align 2**2
    dnl          filesz 0x00000090 memsz 0x00000090 flags rw-
    dnl
    dnl Dynamic Section:
    dnl   NEEDED      libroot.so
    dnl   SONAME      libnet.so
    dnl   SYMBOLIC    0x0
    dnl   INIT        0x2aec
    dnl   FINI        0x12a2c
    dnl   HASH        0x94
    dnl   STRTAB      0x1684
    dnl   SYMTAB      0x774
    dnl   STRSZ       0xbd5
    dnl   SYMENT      0x10
    dnl   PLTGOT      0x15f20
    dnl   PLTRELSZ    0x320
    dnl   PLTREL      0x11
    dnl   JMPREL      0x27cc
    dnl   REL         0x225c
    dnl   RELSZ       0x570
    dnl   RELENT      0x8
    LDDPOSTPROC="2>/dev/null | sed -n -e 's,^  NEEDED *\\([^ ].*\\)\$,\\1,p'"
  else
    if test "$cross_compiling" = no; then
      dnl Not cross-compiling. Try system dependent vendor tools.
      case "$host_os" in
        aix*)
          LDDPROG="dump -H"
          dnl The output of "LC_ALL=C dump -H program" looks like this:
          dnl
          dnl program:
          dnl
          dnl                         ***Loader Section***
          dnl                       Loader Header Information
          dnl VERSION#         #SYMtableENT     #RELOCent        LENidSTR
          dnl 0x00000001       0x00000005       0x0000000d       0x0000001e
          dnl
          dnl #IMPfilID        OFFidSTR         LENstrTBL        OFFstrTBL
          dnl 0x00000002       0x00000134       0x0000000d       0x00000152
          dnl
          dnl
          dnl                         ***Import File Strings***
          dnl INDEX  PATH                          BASE                MEMBER
          dnl 0      /usr/lib:/lib
          dnl 1                                    libc.a              shr.o
          dnl
          LDDPOSTPROC="2>/dev/null | sed -e '/^[^0-9]/d' -e '/^0x/d' | sed -n -e 's,^[0-9]*         *\\([^ 	]*\\).*\$,\\1,p' | sed -e 's,^.*/,,'"
          ;;
        darwin*)
          LDDPROG="otool -L"
          dnl The output of "otool -L program" looks like this:
          dnl program:
          dnl         /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 71.1.4)
          LDDPOSTPROC="2>/dev/null | sed -n -e 's,^	\\([^ 	]*\\).*\$,\\1,p' | sed -e 's,^.*/,,'"
          ;;
        hpux*)
          LDDPROG="chatr"
          dnl The output of "LC_ALL=C chatr program" looks like this:
          dnl program:
          dnl          shared executable
          dnl          shared library dynamic path search:
          dnl              SHLIB_PATH     disabled  second
          dnl              embedded path  disabled  first  Not Defined
          dnl          shared library list:
          dnl              dynamic   /usr/lib/libc.2
          dnl          shared library binding:
          dnl              deferred
          dnl          global hash table disabled
          dnl          plabel caching disabled
          dnl          global hash array size:1103
          dnl          global hash array nbuckets:3
          dnl          shared vtable support disabled
          dnl          static branch prediction disabled
          dnl          executable from stack: D (default)
          dnl          kernel assisted branch prediction enabled
          dnl          lazy swap allocation disabled
          dnl          text segment locking disabled
          dnl          data segment locking disabled
          dnl          third quadrant private data space disabled
          dnl          fourth quadrant private data space disabled
          dnl          third quadrant global data space disabled
          dnl          data page size: D (default)
          dnl          instruction page size: D (default)
          dnl          nulptr references disabled
          dnl          shared library private mapping disabled
          dnl          shared library text merging disabled
          dnl
          dnl or like this:
          dnl a.out:
          dnl          32-bit ELF executable
          dnl          shared library dynamic path search:
          dnl              LD_LIBRARY_PATH    enabled  first
          dnl              SHLIB_PATH         enabled  second
          dnl              embedded path      enabled  third  /usr/lib/hpux32:/opt/langtools/lib/hpux32
          dnl          shared library list:
          dnl              libc.so.1
          dnl          shared library binding:
          dnl              deferred
          dnl          global hash table disabled
          dnl          global hash table size 1103
          dnl          shared library mapped private disabled
          dnl          shared library segment merging disabled
          dnl          shared vtable support disabled
          dnl          explicit unloading disabled
          dnl          segments:
          dnl              index type     address      flags size
          dnl                  7 text     04000000     z---c-    D (default)
          dnl                  8 data     40000000     ---m--    D (default)
          dnl          executable from stack: D (default)
          dnl          kernel assisted branch prediction enabled
          dnl          lazy swap allocation for dynamic segments disabled
          dnl          nulptr references disabled
          dnl          address space model: default
          dnl          caliper dynamic instrumentation disabled
          dnl
          LDDPOSTPROC="2>/dev/null | sed -e '1,/shared library list:/d' -e '/shared library binding:/,\$d' | sed -e 's,^.*[ 	]\\([^ 	][^ 	]*\\)\$,\\1,' | sed -e 's,^.*/,,'"
          ;;
        irix*)
          LDDPROG="elfdump -Dl"
          dnl The output of "elfdump -Dl program" looks like this:
          dnl
          dnl program:
          dnl
          dnl                    **** MIPS LIBLIST INFORMATION ****
          dnl .liblist :
          dnl [INDEX] Timestamp               Checksum        Flags   Name            Version
          dnl [1]     Oct  2 05:19:12 1999    0x867bf7a8      -----   libc.so.1       sgi1.0
          dnl
          LDDPOSTPROC="2>/dev/null | sed -n -e 's,^[[][0-9]*[]].*	0x[^	]*	[^	][^	]*	\\([^	][^	]*\\).*\$,\\1,p' | sed -e 's,^.*/,,'"
          ;;
        linux* | gnu* | kfreebsd*-gnu | knetbsd*-gnu) # glibc-based systems
          LDDPROG="ldd"
          dnl The output of "ldd program" looks like this:
          dnl         libc.so.6 => /lib/libc.so.6 (0x4002d000)
          dnl         /lib/ld-linux.so.2 (0x40000000)
          LDDPOSTPROC="2>/dev/null | sed -n -e 's,^	\\([^ 	][^ 	]*\\).*\$,\\1,p' | sed -e 's,^.*/,,'"
          ;;
        osf*)
          LDDPROG="odump -Dl"
          dnl The output of "odump -Dl program" looks like this:
          dnl
          dnl                         ***LIBRARY LIST SECTION***
          dnl         Name             Time-Stamp        CheckSum   Flags Version
          dnl program:
          dnl         libc.so      Dec 30 00:09:30 1997 0x5e955f9b     0 osf.1
          dnl
          LDDPOSTPROC="2>/dev/null | sed -n -e 's,^	\\([^ 	][^ 	]*\\).*,\\1,p' | sed -e '/^Name\$/d' | sed -e 's,^.*/,,'"
          ;;
        solaris*)
          LDDPROG="ldd"
          dnl The output of "ldd program" looks like this:
          dnl         libc.so.1 =>     /usr/lib/libc.so.1
          dnl         libdl.so.1 =>    /usr/lib/libdl.so.1
          dnl         /usr/platform/SUNW,Ultra-5_10/lib/libc_psr.so.1
          dnl The first sed collects the indented lines.
          dnl The second sed extracts the left-hand part.
          dnl The third sed removes directory specifications.
          LDDPOSTPROC="2>/dev/null | sed -n -e 's,^	\\([^ ].*\\)\$,\\1,p' | sed -e 's, =>.*\$,,' | sed -e 's,^.*/,,'"
          ;;
      esac
    fi
  fi
  dnl Avoid locale dependencies.
  if test "$LDDPROG" != ":"; then
    LDDPROG="LC_ALL=C $LDDPROG"
  fi
changequote([,])dnl
  AC_SUBST([LDDPROG])
  AC_SUBST([LDDPOSTPROC])
])
