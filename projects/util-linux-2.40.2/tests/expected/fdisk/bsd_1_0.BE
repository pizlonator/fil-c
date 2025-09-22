BSD_LABELSECTOR=1
BSD_LABELOFFSET=0
BSD_EXTRA=
BYTE_ORDER=BE

Initialize empty image
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000
Create new DOS partition table
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00  |................|
000001c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000

---layout----------
Disk <removed>: 10 MiB, 10485760 bytes, 20480 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / <removed> bytes
Disklabel type: dos
Disk identifier: <removed>
-------------------

Create 1st primary partition
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 20  |............... |
000001c0  21 00 83 41 01 00 00 08  00 00 00 08 00 00 00 00  |!..A............|
000001d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000

---layout----------
Disk <removed>: 10 MiB, 10485760 bytes, 20480 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / <removed> bytes
Disklabel type: dos
Disk identifier: <removed>

Device     Boot Start   End Sectors Id Type  Start-C/H/S End-C/H/S Attrs
<removed>1       2048  4095    2048 83 Linux     0/32/33    0/65/1 
-------------------

Create 2nd primary partition
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 20  |............... |
000001c0  21 00 83 41 01 00 00 08  00 00 00 08 00 00 00 41  |!..A...........A|
000001d0  02 00 83 46 05 01 00 10  00 00 00 40 00 00 00 00  |...F.......@....|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000
Set 2nd partition type
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 20  |............... |
000001c0  21 00 83 41 01 00 00 08  00 00 00 08 00 00 00 41  |!..A...........A|
000001d0  02 00 a5 46 05 01 00 10  00 00 00 40 00 00 00 00  |...F.......@....|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000

---layout----------
Disk <removed>: 10 MiB, 10485760 bytes, 20480 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / <removed> bytes
Disklabel type: dos
Disk identifier: <removed>

Device     Boot Start   End Sectors Id Type    Start-C/H/S End-C/H/S Attrs
<removed>1       2048  4095    2048 83 Linux       0/32/33    0/65/1 
<removed>2       4096 20479   16384 a5 FreeBSD      0/65/2    1/70/5 
-------------------

Create default BSD
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 20  |............... |
000001c0  21 00 83 41 01 00 00 08  00 00 00 08 00 00 00 41  |!..A...........A|
000001d0  02 00 a5 46 05 01 00 10  00 00 00 40 00 00 00 00  |...F.......@....|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00200200  82 56 45 57 00 06 00 00  00 00 00 00 00 00 00 00  |.VEW............|
00200210  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00200220  00 00 00 00 00 00 00 00  00 00 02 00 00 00 00 3f  |...............?|
00200230  00 00 00 ff 00 00 00 01  00 00 3e c1 00 00 3e c1  |..........>...>.|
00200240  00 00 00 00 00 00 00 00  0e 10 00 01 00 00 00 00  |................|
00200250  00 00 00 00 00 00 00 00  00 00 00 20 00 00 00 00  |........... ....|
00200260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00200280  00 00 00 00 82 56 45 57  62 33 00 04 00 00 20 00  |.....VEWb3.... .|
00200290  00 00 20 00 00 00 00 00  00 00 00 00 00 00 00 00  |.. .............|
002002a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
002002b0  00 00 00 00 00 00 40 00  00 00 10 00 00 00 00 00  |......@.........|
002002c0  00 00 00 00 00 00 3e c1  00 00 00 00 00 00 00 00  |......>.........|
002002d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000

---layout----------

Welcome to fdisk <removed>.
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.


Command (m for help): Entering nested BSD disklabel.

Command (m for help): Disk <removed>2: 10 MiB, 10485760 bytes, 20480 sectors
Geometry: 255 heads, 63 sectors/track, 1 cylinders
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / <removed> bytes
Disklabel type: bsd

Slice Start   End Sectors  Size Type     Fsize Bsize Cpg
c      4096 20479   16384    8M unused       0     0   0
d         0 16064   16065  7.8M unused       0     0   0

Partition table entries are not in disk order.

Command (m for help): Leaving nested disklabel.

Command (m for help): 
-------------------

00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 20  |............... |
000001c0  21 00 83 41 01 00 00 08  00 00 00 08 00 00 00 41  |!..A...........A|
000001d0  02 00 a5 46 05 01 00 10  00 00 00 40 00 00 00 00  |...F.......@....|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00200200  82 56 45 57 00 06 00 00  00 00 00 00 00 00 00 00  |.VEW............|
00200210  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00200220  00 00 00 00 00 00 00 00  00 00 02 00 00 00 00 3f  |...............?|
00200230  00 00 00 ff 00 00 00 01  00 00 3e c1 00 00 3e c1  |..........>...>.|
00200240  00 00 00 00 00 00 00 00  0e 10 00 01 00 00 00 00  |................|
00200250  00 00 00 00 00 00 00 00  00 00 00 20 00 00 00 00  |........... ....|
00200260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00200280  00 00 00 00 82 56 45 57  7d 32 00 04 00 00 20 00  |.....VEW}2.... .|
00200290  00 00 20 00 00 00 08 01  00 00 10 00 00 00 00 00  |.. .............|
002002a0  07 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
002002b0  00 00 00 00 00 00 40 00  00 00 10 00 00 00 00 00  |......@.........|
002002c0  00 00 00 00 00 00 3e c1  00 00 00 00 00 00 00 00  |......>.........|
002002d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00a00000

---layout----------

Welcome to fdisk <removed>.
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.


Command (m for help): Entering nested BSD disklabel.

Command (m for help): Disk <removed>2: 10 MiB, 10485760 bytes, 20480 sectors
Geometry: 255 heads, 63 sectors/track, 1 cylinders
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / <removed> bytes
Disklabel type: bsd

Slice Start   End Sectors  Size Type     Fsize Bsize Cpg
a      4096  6144    2049    1M 4.2BSD       0     0   0
c      4096 20479   16384    8M unused       0     0   0
d         0 16064   16065  7.8M unused       0     0   0

Partition table entries are not in disk order.

Command (m for help): Leaving nested disklabel.

Command (m for help): 
-------------------



Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.


Command (m for help): Entering nested BSD disklabel.

Command (m for help): 
00 unused           05 4.1BSD           09 4.4LFS           0d boot           
01 swap             06 Eighth Edition   0a unknown          0e ADOS           
02 Version 6        07 4.2BSD           0b HPFS             0f HFS            
03 Version 7        08 MS-DOS           0c ISO-9660         10 AdvFS          
04 System V       

Command (m for help): 
