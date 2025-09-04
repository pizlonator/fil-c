There are 15 section headers, starting at offset .*:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.hash             HASH            00008000 001000 000034 04   A  2   0  4
  \[ 2\] \.dynsym           DYNSYM          [0-9a-f]* [0-9a-f]* 000080 10   A  3   5  4
  \[ 3\] \.dynstr           STRTAB          [0-9a-f]* [0-9a-f]* 000008 00   A  0   0  1
  \[ 4\] \.rela\.got         RELA            [0-9a-f]* [0-9a-f]* 000024 0c  AI  2   8  4
  \[ 5\] \.rela\.neardata    RELA            [0-9a-f]* [0-9a-f]* 000030 0c  AI  2   9  4
  \[ 6\] \.dynamic          DYNAMIC         [0-9a-f]* [0-9a-f]* 000090 08  WA  3   0  4
  \[ 7\] \.text             PROGBITS        10000000 002000 0000c0 00  AX  0   0 32
  \[ 8\] \.got              PROGBITS        [0-9a-f]* [0-9a-f]* 000020 00  WA  0   0  4
  \[ 9\] \.neardata         PROGBITS        [0-9a-f]* [0-9a-f]* 000014 00  WA  0   0  4
  \[10\] \.bss              NOBITS          [0-9a-f]* [0-9a-f]* 000004 00  WA  0   0  4
  \[11\] \.c6xabi\.attributes C6000_ATTRIBUTES 00000000 [0-9a-f]* 000019 00      0   0  1
  \[12\] \.symtab           SYMTAB          00000000 [0-9a-f]* 0001c0 10     13  22  4
  \[13\] \.strtab           STRTAB .*
  \[14\] \.shstrtab         STRTAB .*
Key to Flags:
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x10000000
There are 4 program headers, starting at offset 52

Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x001000 0x00008000 0x00008000 0x001a0 0x001a0 RW  0x1000
  LOAD           0x002000 0x10000000 0x10000000 0x000f4 0x000f8 RWE 0x1000
  DYNAMIC        0x[0-9a-f]* 0x[0-9a-f]* 0x[0-9a-f]* 0x00090 0x00090 RW  0x4
  GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x20000 RW  0x8

 Section to Segment mapping:
  Segment Sections\.\.\.
   00     \.hash \.dynsym \.dynstr \.rela\.got \.rela\.neardata \.dynamic 
   01     \.text \.got \.neardata \.bss 
   02     \.dynamic 
   03     

Dynamic section at offset 0x[0-9a-f]* contains 13 entries:
  Tag        Type                         Name/Value
 0x00000004 \(HASH\)                       0x8000
 0x00000005 \(STRTAB\)                     0x[0-9a-f]*
 0x00000006 \(SYMTAB\)                     0x[0-9a-f]*
 0x0000000a \(STRSZ\)                      8 \(bytes\)
 0x0000000b \(SYMENT\)                     16 \(bytes\)
 0x00000015 \(DEBUG\)                      0x0
 0x00000007 \(RELA\)                       0x[0-9a-f]*
 0x00000008 \(RELASZ\)                     84 \(bytes\)
 0x00000009 \(RELAENT\)                    12 \(bytes\)
 0x70000000 \(C6000_DSBT_BASE\)            0x100000c0
 0x70000001 \(C6000_DSBT_SIZE\)            0x3
 0x70000003 \(C6000_DSBT_INDEX\)           0x0
 0x00000000 \(NULL\)                       0x0

Relocation section '\.rela\.got' at offset 0x[0-9a-f]* contains 3 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000d8  [0-9a-f]* R_C6000_ABS32          100000f4   \.bss \+ 0
100000d4  [0-9a-f]* R_C6000_ABS32          100000e8   b \+ 0
100000dc  [0-9a-f]* R_C6000_ABS32          100000e0   a \+ 0

Relocation section '\.rela\.neardata' at offset 0x[0-9a-f]* contains 4 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
100000e0  [0-9a-f]* R_C6000_ABS32          10000000   \.text \+ 8
100000e4  [0-9a-f]* R_C6000_ABS32          00000000   g1 \+ 0
100000ec  [0-9a-f]* R_C6000_ABS32          00000000   g1 \+ 0
100000f0  [0-9a-f]* R_C6000_ABS32          100000e0   \.neardata \+ 4

Symbol table '\.dynsym' contains 8 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    7.*
     2: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    8.*
     3: [0-9a-f]*     0 SECTION LOCAL  DEFAULT    9.*
     4: [0-9a-f]*     0 SECTION LOCAL  DEFAULT   10.*
     5: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT    9 b
     6: 00000000     0 NOTYPE  WEAK   DEFAULT  UND g1
     7: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT    9 a

Symbol table '\.symtab' contains 28 entries:
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
    12: [0-9a-f]*     0 FILE    LOCAL  DEFAULT  ABS .*shlib-1\.o
    13: [0-9a-f]*     0 FUNC    LOCAL  HIDDEN     7 sub1
    14: [0-9a-f]*     0 FILE    LOCAL  DEFAULT  ABS .*shlib-app-1\.o
    15: [0-9a-f]*     0 NOTYPE  LOCAL  DEFAULT    7 fish
    16: [0-9a-f]*     8 OBJECT  LOCAL  DEFAULT    9 w
    17: [0-9a-f]*     0 FILE    LOCAL  DEFAULT  ABS 
    18: [0-9a-f]*     0 OBJECT  LOCAL  DEFAULT    6 _DYNAMIC
    19: [0-9a-f]*     4 OBJECT  LOCAL  DEFAULT   10 c
    20: [0-9a-f]*     0 OBJECT  LOCAL  DEFAULT    8 _GLOBAL_OFFSET_TABLE_
    21: [0-9a-f]*     0 NOTYPE  LOCAL  DEFAULT    8 __c6xabi_DSBT_BASE
    22: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT    9 b
    23: [0-9a-f]*     0 NOTYPE  WEAK   DEFAULT  UND g1
    24: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT    9 g2
    25: [0-9a-f]*    52 FUNC    GLOBAL DEFAULT    7 sub0
    26: [0-9a-f]*     4 OBJECT  GLOBAL DEFAULT    9 a
    27: [0-9a-f]*    52 FUNC    GLOBAL DEFAULT    7 sub
