
foreach (@ARGV) {s|/+|/|g; s|/$||;}

($prefix, $data, $lib, $conf) = @ARGV;

sub def ( $ ) {
  return qq|"<prefix:$1>"| if $_[0] =~ m|^$prefix/?(.+)$|;
  return qq|"$_[0]"|;
}

printf qq|#define PREFIX "%s"\n|, $prefix;
printf qq|#define DATA_DIR %s\n|, def($data);
print  qq|#define DICT_DIR "<data-dir>"\n| if $lib eq $data;
printf qq|#define DICT_DIR %s\n|, def($lib) if $lib ne $data;
printf qq|#define CONF_DIR %s\n|, def($conf);

