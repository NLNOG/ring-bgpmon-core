#!/usr/bin/perl

use strict;
use Getopt::Long;

$| = 1;

my $list_url = "http://archive.routeviews.org/COLLECTOR/bgpdata/";
my $file_url; 
my $collector = "";

#List of collectors (basically, the only valid arguments to this script)
#"." (which will resolve to bgpdata in the wget below), "route-views.eqix",
#"route-views.isc","route-views.kixp","route-views.linx",
#"route-views.saopaulo","route-views.sydney","route-views.wide","route-views4",
#"route-views6"
my $result = GetOptions( "collector=s" => \$collector);
die "Unable to parse command line arguments\n" unless $result;

## now process the updates
$list_url =~ s/COLLECTOR/$collector/;
if ($collector == "") {
    $list_url = "http://archive.routeviews.org/bgpdata/";
}

my $use_url;
my @years = ("2011", "2010", "2009", "2008", "2007", "2006", "2005", "2004", "2003", "2002", "2001");
my @months = ("12", "11", "10", "09", "08", "07", "06", "05", "04", "03", "02", "01");
my @html_index;

foreach my $year (@years) {
    foreach my $month (@months) {
        $use_url = $list_url . $year . "." . $month . "/UPDATES/";
        print $use_url;
        print "\n";
        @html_index = `curl $use_url`;
        foreach my $line (@html_index){
            if($line =~ /a href="(.*?bz2)"/){
                my $filename = $1;
                $file_url = $use_url . $filename;
                print $file_url;
                print "\n";
                `wget $file_url`;
                `bunzip2 $filename`;
                $filename =~ s/\.bz2//;
                `mrtfeeder -f $filename -d localhost`;
                `rm -f $filename`;
             }
        }
    }
}
