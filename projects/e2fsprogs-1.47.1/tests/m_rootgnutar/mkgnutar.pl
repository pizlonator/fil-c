#!/usr/bin/env perl

use strict;
use warnings;

use Getopt::Long;
use Fcntl ':mode';

my ($directory, $mtime, $nopadding, $norec, $verbose);
GetOptions(
    "directory=s"  => \$directory,
    "mtime=i"      => \$mtime,
    "nopadding"    => \$nopadding,
    "no-recursion" => \$norec,
    "verbose"      => \$verbose,
);

chdir($directory) || die "cannot chdir";

my $num_entries = 0;

sub recurse_dir {
    my $path    = shift;
    my @results = ("$path/");
    opendir my $dh, $path or die "cannot open $path";
    while (my $entry = readdir $dh) {
        next if $entry eq ".";
        next if $entry eq "..";
        if (-d "$path/$entry") {
            push @results, (&recurse_dir("$path/$entry"));
        } else {
            push @results, "$path/$entry";
        }
    }
    closedir $dh;
    return @results;
}

my @entries;
if (!-e $ARGV[0]) {
    die "does not exist: $ARGV[0]";
} elsif (-d $ARGV[0] && !$norec) {
    @entries = sort (recurse_dir($ARGV[0]));
} else {
    @entries = ($ARGV[0]);
}

foreach my $fname (@entries) {
    if ($verbose) {
        print STDERR "$fname\n";
    }
    my (
        $dev,  $ino,   $mode,   $nlink, $uid,     $gid, $rdev,
        $size, $atime, $mtime_, $ctime, $blksize, $blocks
    ) = lstat($fname);
    if (!defined $mode) {
        die "failed to stat $fname";
    }
    my $content = "";
    my $type;
    my $linkname = "";
    my $username = $ENV{LOGNAME} || $ENV{USER} || getpwuid($<);
    if (S_ISLNK($mode)) {
        $type     = 2;
        $linkname = readlink $fname;
    } elsif (S_ISREG($mode)) {
        $type = 0;
        open(my $fh, '<', $fname);
        $content = do { local $/; <$fh> };
        close($fh);
    } elsif (S_ISDIR($mode)) {
        $type = 5;
    }
    my $entry = pack(
        'a100 a8 a8 a8 a12 a12 A8 a1 a100 a6 a2 a32 a32 a8 a8 a155 x12',
        $fname,
        sprintf('%07o',  $mode & 07777),
        sprintf('%07o',  $<),                 # uid
        sprintf('%07o',  $(),                 # gid
        sprintf('%011o', length $content),    # size
        sprintf('%011o', $mtime),
         # mtime
        '',                                   # checksum
        $type,
        $linkname,                            # linkname
        "ustar ",                             # magic
        " ",                                  # version
        "$username",                          # username
        "$username",                          # groupname
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
        my $num_blocks = int((length $content) / 512);
        if ((length $content) % 512 != 0) {
            $num_blocks += 1;
        }
        print $content;
        print(("\x00") x ($num_blocks * 512 - (length $content)));
        $num_entries += $num_blocks;
    }
}

if (!$nopadding) {
    # https://www.gnu.org/software/tar/manual/html_node/Standard.html
    #
    # Physically, an archive consists of a series of file entries terminated
    # by an end-of-archive entry, which consists of two 512 blocks of zero
    # bytes.  At the end of the archive file there are two 512-byte blocks
    # filled with binary zeros as an end-of-file marker.
    print(pack 'a512', '');
    print(pack 'a512', '');
    $num_entries += 2;

    # https://www.gnu.org/software/tar/manual/html_section/tar_76.html
    #
    # Some devices requires that all write operations be a multiple of a
    # certain size, and so, tar pads the archive out to the next record
    # boundary.
    #
    # The default blocking factor is 20. With a block size of 512 bytes, we
    # get a record size of 10240.
    my $num_records = int($num_entries * 512 / 10240);
    if (($num_entries * 512) % 10240 != 0) {
        $num_records += 1;
    }
    for (my $i = $num_entries ; $i < $num_records * 10240 / 512 ; $i++) {
        print(pack 'a512', '');
    }
}
