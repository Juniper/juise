#!/usr/bin/perl
#

@in = <STDIN>;

open(OUT, ">test-03.out");
print OUT @in;

$out = <<EOF;
<?xml version="1.0"?>
<some-type-of-data>
  <details/>
  <more-details/>
</some-type-of-data>
EOF

print $out;

exit(0);
