#!/usr/bin/env perl

use strict;
use warnings;

# type codes:
#   0 -> normal file
#   1 -> hardlink
#   2 -> symlink
#   3 -> character special
#   4 -> block special
#   5 -> directory
my @devfiles = (
    # filename  mode      type link target        major  minor
    ["",        oct(755), 5,   '',                undef, undef],
    ["console", oct(666), 3,   '',                5,     1],
    ["fd",      oct(777), 2,   '/proc/self/fd',   undef, undef],
    ["full",    oct(666), 3,   '',                1,     7],
    ["null",    oct(666), 3,   '',                1,     3],
    ["ptmx",    oct(666), 3,   '',                5,     2],
    ["pts/",    oct(755), 5,   '',                undef, undef],
    ["random",  oct(666), 3,   '',                1,     8],
    ["shm/",    oct(755), 5,   '',                undef, undef],
    ["stderr",  oct(777), 2,   '/proc/self/fd/2', undef, undef],
    ["stdin",   oct(777), 2,   '/proc/self/fd/0', undef, undef],
    ["stdout",  oct(777), 2,   '/proc/self/fd/1', undef, undef],
    ["tty",     oct(666), 3,   '',                5,     0],
    ["urandom", oct(666), 3,   '',                1,     9],
    ["zero",    oct(666), 3,   '',                1,     5],
);

my $mtime = time;
if (exists $ENV{SOURCE_DATE_EPOCH}) {
    $mtime = $ENV{SOURCE_DATE_EPOCH} + 0;
}

foreach my $file (@devfiles) {
    my ($fname, $mode, $type, $linkname, $devmajor, $devminor) = @{$file};
    my $entry = pack(
        'a100 a8 a8 a8 a12 a12 A8 a1 a100 a8 a32 a32 a8 a8 a155 x12',
        "./dev/$fname",
        sprintf('%07o',  $mode),
        sprintf('%07o',  0),        # uid
        sprintf('%07o',  0),        # gid
        sprintf('%011o', 0),        # size
        sprintf('%011o', $mtime),
        '',                         # checksum
        $type,
        $linkname,
        "ustar  ",
        '',                         # username
        '',                         # groupname
        defined($devmajor) ? sprintf('%07o', $devmajor) : '',
        defined($devminor) ? sprintf('%07o', $devminor) : '',
        '',                         # prefix
    );

    # compute and insert checksum
    substr($entry, 148, 7)
      = sprintf("%06o\0", unpack("%16C*", $entry));
    print $entry;
}
