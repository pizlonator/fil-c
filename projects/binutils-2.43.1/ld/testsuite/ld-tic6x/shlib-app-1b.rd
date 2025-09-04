There are 18 section headers, starting at offset .*:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.hash             HASH            00008000 001000 000040 04   A  2   0  4
  \[ 2\] \.dynsym           DYNSYM          [0-9a-f]* [0-9a-f]* 0000b0 10   A  3   6  4
  \[ 3\] \.dynstr           STRTAB          [0-9a-f]* [0-9a-f]* 000023 00   A  0   0  1
  \[ 4\] \.rela\.got         RELA            [0-9a-f]* [0-9a-f]* 000018 0c  AI  2  11  4
  \[ 5\] \.rela\.neardata    RELA            [0-9a-f]* [0-9a-f]* 000018 0c  AI  2  12  4
  \[ 6\] \.rela\.bss         RELA            [0-9a-f]* [0-9a-f]* 00000c 0c  AI  2  13  4
  \[ 7\] \.dynamic          DYNAMIC         [0-9a-f]* [0-9a-f]* 0000b8 08  WA  3   0  4
  \[ 8\] \.rela\.plt         RELA            10000000 002000 00000c 0c  AI  2  11  4
  \[ 9\] \.plt              PROGBITS        [0-9a-f]* [0-9a-f]* 000040 18  AX  0   0 32
  \[10\] \.text             PROGBITS        [0-9a-f]* [0-9a-f]* 000040 00  AX  0   0 32
  \[11\] \.got              PROGBITS        [0-9a-f]* [0-9a-f]* 000020 00  WA  0   0  4
  \[12\] \.neardata         PROGBITS        [0-9a-f]* [0-9a-f]* 00000c 00  WA  0   0  4
  \[13\] \.bss              NOBITS          [0-9a-f]* [0-9a-f]* 000004 00  WA  0   0  4
  \[14\] \.c6xabi\.attributes C6000_ATTRIBUTES 00000000 [0-9a-f]* 000019 00      0   0  1
  \[15\] \.symtab           SYMTAB          00000000 [0-9a-f]* 0001b0 10     16  22  4
  \[16\] \.strtab           STRTAB .*
  \[17\] \.shstrtab         STRTAB .*
Key to Flags:
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x10000060
There are 4 program headers, starting at offset 52

Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x001000 0x00008000 0x00008000 0x00208 0x00208 RW  0x1000
  LOAD           0x002000 0x10000000 0x10000000 0x000cc 0x000d0 RWE 0x1000
  DYNAMIC        0x[0-9a-f]* 0x[0-9a-f]* 0x[0-9a-f]* 0x000b8 0x000b8 RW  0x4
  GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x20000 RW  0x8

 Section to Segment mapping:
  Segment Sections\.\.\.
   00     \.hash \.dynsym \.dynstr \.rela\.got \.rela\.neardata \.rela\.bss \.dynamic 
   01     \.rela\.plt \.plt \.text \.got \.neardata \.bss 
   02     \.dynamic 
   03     

Dynamic section at offset 0x[0-9a-f]* contains 18 entries:
  Tag        Type                         Name/Value
 0x00000001 \(NEEDED\)                     Shared library: \[tmpdir/libtestb\.so\]
 0x00000004 \(HASH\)                       0x8000
 0x00000005 \(STRTAB\)                     0x[0-9a-f]*
 0x00000006 \(SYMTAB\)                     0x[0-9a-f]*
 0x0000000a \(STRSZ\)                      35 \(bytes\)
 0x0000000b \(SYMENT\)                     16 \(bytes\)
 0x00000015 \(DEBUG\)                      0x0
 0x00000003 \(PLTGOT\)                     0x100000ac
 0x00000002 \(PLTRELSZ\)                   12 \(bytes\)
 0x00000014 \(PLTREL\)                     RELA
 0x00000017 \(JMPREL\)                     0x10000000
 0x00000007 \(RELA\)                       0x[0-9a-f]*
 0x00000008 \(RELASZ\)                     72 \(bytes\)
 0x00000009 \(RELAENT\)                    12 \(bytes\)
 0x70000000 \(C6000_DSBT_BASE\)            0x100000a0
 0x70000001 \(C6000_DSBT_SIZE\)            0x3
 0x70000003 \(C6000_DSBT_INDEX\)           0x0
 0x00000000 \(NULL\)                       0x0

Relocation section '\.rela\.got' at offset 0x[0-9a-f]* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000b8  [0-9a-f]* R_C6000_ABS32          100000c0   b \+ 0
100000bc  [0-9a-f]* R_C6000_ABS32          100000cc   a \+ 0

Relocation section '\.rela\.neardata' at offset 0x[0-9a-f]* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000c4  [0-9a-f]* R_C6000_ABS32          00000000   g1 \+ 0
100000c8  [0-9a-f]* R_C6000_ABS32          00000000   g2 \+ 0

Relocation section '\.rela\.bss' at offset 0x[0-9a-f]* contains 1 entry:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000cc  [0-9a-f]* R_C6000_COPY           100000cc   a \+ 0

Relocation section '\.rela\.plt' at offset 0x[0-9a-f]* contains 1 entry:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000b4  [0-9a-f]* R_C6000_JUMP_SLOT      00000000   sub0 \+ 0

Symbol table '\.dynsym' contains 11 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    9.*
     2: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   10.*
     3: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   11.*
     4: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   12.*
     5: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   13.*
     6: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT   12 b
     7: 00000000     0 NOTYPE  WEAK   DEFAULT  UND g1
     8: 00000000     0 OBJECT  WEAK   DEFAULT  UND g2
     9: 00000000     0 FUNC    GLOBAL DEFAULT  UND sub0
    10: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT   13 a

Symbol table '\.symtab' contains 27 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    1.*
     2: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    2.*
     3: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    3.*
     4: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    4.*
     5: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    5.*
     6: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    6.*
     7: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    7.*
     8: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    8.*
     9: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    9.*
    10: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   10.*
    11: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   11.*
    12: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   12.*
    13: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   13.*
    14: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   14.*
    15: [0-9a-f]*     0 FILE    LOCAL  DEFAULT  ABS .*shlib-app-1\.o
    16: [0-9a-f]*     0 NOTYPE  LOCAL  DEFAULT   10 fish
    17: [0-9a-f]*     8 OBJECT  LOCAL  DEFAULT   12 w
    18: [0-9a-f]*     0 FILE    LOCAL  DEFAULT  ABS 
    19: [0-9a-f]*     0 OBJECT  LOCAL  DEFAULT    7 _DYNAMIC
    20: [0-9a-f]*     0 OBJECT  LOCAL  DEFAULT   11 _GLOBAL_OFFSET_TABLE_
    21: [0-9a-f]*     0 NOTYPE  LOCAL  DEFAULT   11 __c6xabi_DSBT_BASE
    22: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT   12 b
    23: [0-9a-f]*     0 NOTYPE  WEAK   DEFAULT  UND g1
    24: [0-9a-f]*     0 OBJECT  WEAK   DEFAULT  UND g2
    25: [0-9a-f]*     0 FUNC    GLOBAL DEFAULT  UND sub0
    26: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT   13 a
