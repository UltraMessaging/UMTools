#!/usr/bin/env perl
# dbg_timestamp.pl

use strict;
use warnings;
use Getopt::Std;
use File::Basename;

# globals
my $tool = basename($0);
my $usage_str = "$tool [-h] [-H hour] [file ...]";

# process options.
use vars qw($opt_h $opt_H);
getopts('hH:') || usage();  # if -h had a value, it would be "h:"

if (! defined($opt_H)) {
  $opt_H = (localtime)[2];
}

if (defined($opt_h)) {
  help();  # if -h had a value, it would be in $opt_h
}

my $start_hr=$opt_H;
my $num_rollovers = 0;
my $prev_sec;

# Main loop; read each line in each file.
while (<>) {
  chomp;  # remove trailing \n
  if (/^\[([^:]+):([^\|]+)\|([^\.]+)\.([^\]]+)\]:(.*)$/) {
    my ($pid, $tid, $sec, $usec, $rest) = ($1, $2, $3, $4, $5);
    if (! defined($prev_sec)) {
      $prev_sec = $sec;
    }
    if ($sec < $prev_sec) {  # Rollover!
      $num_rollovers++;
    }
    $prev_sec = $sec;

    my $hr = $start_hr + $num_rollovers;
    my $min = $sec / 60;
    $sec = $sec % 60;
    my $ts = sprintf("%02d:%02d:%02d.%06d", $hr, $min, $sec, $usec);
    print "[$pid:$tid|$ts]:$rest\n";
  }
  else {
    print "$_\n";
  }
} continue {  # This continue clause makes "$." give line number within file.
  close ARGV if eof;
}

# All done.
exit(0);


# End of main program, start subroutines.


sub assrt {
  my ($assertion, $msg) = @_;

  if (! ($assertion)) {
    if (!defined($msg)) { $msg = ""; }
    mycroak($msg);
  }
}  # assrt


sub mycroak {
  my ($msg) = @_;

  croak("Error, $ARGV:$., $msg");
}  # mycroak


sub usage {
  my($err_str) = @_;

  if (defined $err_str) {
    print STDERR "$tool: $err_str\n\n";
  }
  print STDERR "Usage: $usage_str\n\n";

  exit(1);
}  # usage


sub help {
  my($err_str) = @_;

  if (defined $err_str) {
    print "$tool: $err_str\n\n";
  }
  print <<__EOF__;
Usage: $usage_str
Where:
    -h - help
    -H hour - starting hour to insert into timestamp.
    file ... - zero or more input files.  If omitted, inputs from stdin.

__EOF__

  exit(0);
}  # help
