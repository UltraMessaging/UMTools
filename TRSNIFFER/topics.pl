#!/usr/bin/env perl
# topics.pl

use strict;
use warnings;
use Getopt::Std;
use File::Basename;
use Carp;

$| = 1;  # flush stdout

# globals
my $tool = basename($0);

# process options.
use vars qw($opt_h $opt_o);
getopts('ho:') || mycroak("getopts failure");

if (defined($opt_h)) {
  help();
}

my $out_fd;
if (defined($opt_o)) {
  open($out_fd, ">", $opt_o) or mycroak("Error opening '$opt_o': $!");
} else {
  $out_fd = *STDOUT;
}

# Main loop; read each line in each file.
my %otid_map;

while (<>) {
  chomp;  # remove trailing \n

  if (/OTID_SOURCE: ([^ ]*) (.*)$/) {
    my ($otid, $topic) = ($1, $2);
    if (!defined($otid_map{$otid})) {
      $otid_map{$otid} = $topic;
      print $out_fd "$otid $topic\n";
    }
  }

} continue {  # This continue clause makes "$." give line number within file.
  close ARGV if eof;
}

# All done.
exit(0);


# End of main program, start subroutines.


sub help {
  my($err_str) = @_;

  if (defined $err_str) {
    print "$tool: $err_str\n\n";
  }
  print <<__EOF__;
Usage: $tool [-h] [-o out_file] [file ...]
Where ('R' indicates required option):
    -h - help
    -o out_file - output file (default: STDOUT).
    file ... - zero or more input files.  If omitted, inputs from stdin.

__EOF__

  exit(0);
}  # help
