#!/usr/bin/env perl

use strict;
use warnings;

my @entries = (
    # filename            mode      type content
    ['./PaxHeaders/file', oct(644), 'x', "57 SCHILY.xattr.security.capability=\x01\0\0\x02\0\x20\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x0a"],
    ['file',              oct(644), 0,   ''],
);

my $num_entries = 0;

foreach my $file (@entries) {
    my ($fname, $mode, $type, $content) = @{$file};
    my $entry = pack(
        'a100 a8 a8 a8 a12 a12 A8 a1 a100 a6 a2 a32 a32 a8 a8 a155 x12',
        $fname,
        sprintf('%07o',  $mode),
        sprintf('%07o',  0),                  # uid
        sprintf('%07o',  0),                  # gid
        sprintf('%011o', length $content),    # size
        sprintf('%011o', 0),                  # mtime
        '',                                   # checksum
        $type,
        '',                                   # linkname
        "ustar",                              # magic
        "00",                                 # version
        '',                                   # username
        '',                                   # groupname
        '',                                   # dev major
        '',                                   # dev minor
        '',                                   # prefix
    );

    # compute and insert checksum
    substr($entry, 148, 7)
      = sprintf("%06o\0", unpack("%16C*", $entry));
    print $entry;
    $num_entries += 1;

    if (length $content) {
        print(pack 'a512', $content);
        $num_entries += 1;
    }
}

# https://www.gnu.org/software/tar/manual/html_node/Standard.html
#
# Physically, an archive consists of a series of file entries terminated by an
# end-of-archive entry, which consists of two 512 blocks of zero bytes. At the
# end of the archive file there are two 512-byte blocks filled with binary
# zeros as an end-of-file marker.

print(pack 'a512', '');
print(pack 'a512', '');
$num_entries += 2;

# https://www.gnu.org/software/tar/manual/html_section/tar_76.html
#
# Some devices requires that all write operations be a multiple of a certain
# size, and so, tar pads the archive out to the next record boundary.
#
# The default blocking factor is 20. With a block size of 512 bytes, we get a
# record size of 10240.

for (my $i = $num_entries ; $i < 20 ; $i++) {
    print(pack 'a512', '');
}
