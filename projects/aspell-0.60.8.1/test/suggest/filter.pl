while (<>) {
    chomp;
    my @data = split "\t";
    die unless @data == 4;
    next if $data[2] == -1 && $data[3] == -1;
    print "$data[0]\t$data[1]\n";
}
