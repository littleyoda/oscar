#!/usr/bin/perl
## Copyright (c) 2024-2024 The OSCAR Team

## FindS Text files with the word "Copyright"  ending with with a year(4 digits) range like 2019-2022 and then
## followed by a string containg OSCAR.
## the script changes the 2 year to the current year.
## This script will search all file in the folder where the the script is run.
## 1) From The Git Top Level folder. The script will also access the translation files and Build file.
## 2) From The oscar folder (that contains oscar.pro). The script will also access the code files.


use strict;
use warnings;
use Time::HiRes;
use File::Find;
use File::Spec;
use Cwd;

my $current_year = (localtime)[5] + 1900;
my $top_dir = `git rev-parse --show-toplevel`;
chomp($top_dir);  # Remove the trailing newline
my $relativeSyncDir = "$top_dir/../";
my $verbose = 0  ;

my $filesChanged =0;
my $filesNotChanged =0;
my $filesWithoutCopyright =0;


sub processFile {
    my ($file_handle,$filename) = @_;
    my $relativeFileName = File::Spec->abs2rel($filename, $relativeSyncDir);
    my @lines;
    my $line;
    # Process each token here
    # Open the file for reading
    open($file_handle, '+<', $filename) or die "Could not open file for read/write:$filename";

    # Iterate over each line using a while loop
    ##@lines = ();
    my $fileModified=0;
    while ($line = <$file_handle> ) {
        ##if ($line =~ /(^\s*[*]?\s*Copyright\s+.c.\s+\d{4}-)(\d{4})( The OSCAR Team)$/i) 
        if ($fileModified == 0 && $line =~ /Copyright/i) {
            if ($line =~ /(^.*Copyright.*\b\d{4}-)(\d{4})(.*OSCAR.*$)/i) 
            {
                my $match1 = $1;  ## start of line that contains "CopyRight" and a year (4 digit number)
                my $match2 = $2;  ## year (4 digit number) 
                my $match3 = $3;  ## rest of characters that contains OSCAR
                if ($match2 == $current_year) {
                    ## no need to update so skip changing file;
                    $filesNotChanged++;
                    print "  Already Modified:  $relativeFileName\n" if ($verbose != 0) ;
                    return;
                }
                $match2 = $current_year;  ## year (4 digit number) 
                $line ="$match1$match2$match3\n";
                print "  Modified:          $relativeFileName\n" if ($verbose != 0) ;
                $fileModified=1;
            } else {
                ## file has copyright but no range data or not OSCAR.
            }
        }
        ##$line = $line."\n";
        push @lines , $line;
    };
    if ($fileModified==0) {
        $filesWithoutCopyright++;
        print "  No Match:          $relativeFileName\n" if ($verbose != 0) ;
        return;
    }
    $filesChanged++;
    truncate $file_handle, 0;
    seek $file_handle, 0, 0;

    print $file_handle @lines;

    $file_handle->sync();
    close $file_handle;

    #Time::HiRes::sleep(0.5);
}

sub handleFile {
    my $file = $File::Find::name;
    # Process the file here
    my $fh;
    processFile($fh,$file) if (-T $file) ;
}
sub help {
my $helpMsg = <<"END_MSG";  # Double-quoted heredoc

Help Menu
    $0 

    # uses the current year to modifed files with copyright.
    # This script modifies the first line with the following signature (case insensative). 
        YYYY and ZZZZ are sequences of 4 digits representing year
        asterisk " means any sequence of charaters.
    # signature: * Copyright * YYYY-ZZZZ * OSCAR *
    # The script will only change ZZZZ to the current year. No file size change.
    # No other lines will be modfied.

    --help          displays help message
    --execute       allows script to execute
    -v --verbose    displays each file modifed    
    --code          starts working at OSCAR-code/oscar
    --base          starts working at OSCAR-code
    <folderName>    starts working at OSCAR-code/folderName
    defaultFolder   starts working at the current folder.

END_MSG
    print $helpMsg;
    exit;
}

####################################################################################################
## find location to start.
my $start_dir = getcwd();
chomp($start_dir);  # Remove the trailing newline
my $options = "";
my $execute = undef;

while (my $arg = shift @ARGV) {
    $options = "$options $arg";
    if ($arg eq '--help') {
        # Handle help option
    } elsif ($arg eq '--verbose' || $arg eq '-v' ) {
        $verbose = 1 ;
    } elsif ($arg eq '--code') {
        ## starts form topLevelFolder/oscar
        $start_dir = "$top_dir/oscar";
    } elsif ($arg eq '--execute') {
        $execute = 1;
    } elsif ($arg eq '--base') {
        $start_dir = $top_dir;
    } else {
        my $tmp_dir = "$top_dir/$arg";
        if (-d $tmp_dir) {
            $start_dir = $tmp_dir;
        } else {
            print "Invalid Parameter:$arg\n";
            &help;
            exit 1;
        }
    }
}
my $relativeStartDir = File::Spec->abs2rel($start_dir, $relativeSyncDir);

if (!defined $execute) {
    print "Requires --execute parameter to execute script\n";
    help();
    exit 1;
}



# Call the find function to process all files recursively
finddepth(\&handleFile, $start_dir);

my $summary = <<"END_SUMMARY";  # Double-quoted heredoc
$0 $relativeStartDir $options
Summary of Text Files searched
Number of files with date modifed:          $filesChanged
Number of files with date already modifed:  $filesNotChanged
Number of files not matching signature:     $filesWithoutCopyright
END_SUMMARY

print $summary;



__END__

