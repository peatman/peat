#!/usr/bin/perl

#
# This file is part of PEAT (Performance Estimation & Analysing Toolkit).
# Copyright (C) 2026 by Alex Knyazev
#
# PEAT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PEAT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
#

use strict;
use Env;
use Switch;
#use List::Util qw(min);

my %mpiFuncID;
####################################################################################
# { MPID_REQUEST_RELEASE_ID COMMA "int" COMMA "MPID_Request_Release" COMMA "MPID_Request* request" COMMA "" COMMA   {{PEAT_PROP_BUILTIN PROPSEP T_PROPERTY_NONE}} }  COMMA 


sub make_func_id {
my $lineref=shift;
my @line=@$lineref;
my $func_name_id=uc $line[2];
my $func_id=$line[0];
if ($mpiFuncID{$func_id} ne "" ) {
print <<EOF;
    ${func_name_id}_ID = $mpiFuncID{$func_id},
EOF
} else {
print <<EOF;
    ${func_name_id}_ID,
EOF
}
}

my $stat_count=1;
sub make_stat_func_id {
my $lineref=shift;
my @line=@$lineref;
my $func_name_id=uc $line[2];
my $func_id=$line[0];
print <<EOF;
    ${func_name_id}_ID_THROUGH,
EOF
$stat_count++;
}

sub make_stat_func_id_2 { # new version based on 'enum' file
my $func_name_id=shift;
print OUT_TH <<EOF;
    ${func_name_id}_THROUGH,
EOF
$stat_count++;
}


sub make_func_name {
my $lineref=shift;
my @line=@$lineref;
my $func_name_id=$line[2]; #uc is removed
my $func_id=$line[0];
print OUT_NAME <<EOF;
    "${func_name_id}",
EOF
}

sub make_func_id_trans {
my $lineref=shift;
my @line=@$lineref;
my $func_name_id=uc $line[2];
my $func_id=$line[0];
#print STDERR "$func_id:$mpiFuncID{$func_id}:$func_name_id\n";
print OUT_TR <<EOF;
    $mpiFuncID{$func_id},
EOF
}


my %mpi_func_IDs;

sub store_func_id {
my $lineref=shift;
my @line=@$lineref;
my $func_id=$line[0];
   $mpi_func_IDs{$func_id}=\@line;
}

####################################################################################
sub parse_file{
my $file=shift;
my $mode=shift;
my @line;
my $linecount=0;

open(IN,$file) or die "Can't open input file: $file !" ;
print STDERR "Parsing file $file\n";

while( <IN> ) {
  chomp($_);
  my $line=$_;
  last if ($line =~/exit_parsing_here_all/);

  $line=~s/\/\/.*//g;
  $line=~s/\"//g;
  #$line=~s/ //g;
  $line=~s/{//g;

  @line = split(' COMMA ',$line);
  $line[0]=~s/ //g;
  if ($line[1] eq "") {next;}
  $linecount++;
  store_func_id(\@line) if ($mode==0);
  make_func_id(\@line) if ($mode==4);
  make_stat_func_id(\@line) if ($mode==1);
  make_func_name(\@line) if ($mode==2);
  make_func_id_trans(\@line) if ($mode==3);
}
close (IN);

return $linecount;
}


#./mpi_func_ids.pl  -l $(fab_layer) -i ../../common/mpifuncids.h ../../common/mpifunc.inc -o mpi_func_ids_through.h   mpi_func_names.h mpi_func_ids_trans.h  
my $mpifuncids;
my $mpifuncs;
my $mpifuncidsthrough;
my $mpifuncnames;
my $mpifuncidstrans;
my $mpifuncidstmp;

####################################################################################
sub parse_command_line {
   my $i;my $arg;
   for ($i=0; $i<=$#ARGV; $i++) {
      $arg=$ARGV[$i];
      switch ($arg) {
         case "-i" {$mpifuncids=$ARGV[$i+1]; $mpifuncs=$ARGV[$i+2];$i+=2}
         case "-t" {$mpifuncidstmp=$ARGV[$i+1];$i++}
         case "-o" {$mpifuncidsthrough=$ARGV[$i+1]; $mpifuncnames=$ARGV[$i+2];$mpifuncidstrans=$ARGV[$i+3];$i+=3}
         else {print "Unknown switch $ARGV[$i]\n";die;}
      }
   }
}

####################################################################################
########################### main program starts here ###############################
parse_command_line;
my $linecount;
if ($mpifuncidstmp ne "" ) {
open(OUT,">$mpifuncidstmp") or die ("Cannot open $mpifuncidstmp for writing");
select(OUT);
$linecount=parse_file("$mpifuncs",4);
close OUT;
exit;
}

$linecount=parse_file("$mpifuncs",0);

open(IN, "$mpifuncids") or die ("can't open file $mpifuncids for reading");
open(OUT_TH,">$mpifuncidsthrough") or die ("Cannot open $mpifuncidsthrough for writing");

print OUT_TH <<EOF;
#ifndef __MPI_FUNC_IDS_THROUGH_H__ 
#define __MPI_FUNC_IDS_THROUGH_H__ 
enum PEAT_MPI_Func_ID_Through {
    MPI_MIN_FUNCID_THROUGH,
EOF


open(OUT_NAME,">$mpifuncnames") or die ("Cannot open $mpifuncnames for writing");
print OUT_NAME <<EOF;
#ifndef __MPI_FUNC_NAMES_H__ 
#define __MPI_FUNC_NAMES_H__ 
#include "mpi_func_ids_through.h"
const char *mpi_function_names[MPI_MAX_FUNCID_THROUGH]={
    "UNKNOWN/USER CODE",
EOF

open(OUT_TR,">$mpifuncidstrans") or die ("Cannot open $mpifuncidstrans for writing");
print OUT_TR <<EOF;
#ifndef __MPI_FUNC_IDS_TRANS_H__ 
#define __MPI_FUNC_IDS_TRANS_H__  
#include "mpi_func_ids_through.h"
int mpi_func_ids_trans[MPI_MAX_FUNCID_THROUGH]={
    0,
EOF


print STDERR "Parsing file $mpifuncids\n";
my $enumCounter=0;
while( <IN> ) {
  chomp($_);
  my $line=$_;
  last if ($line =~/exit_parsing_here/);

  $line=~s/\/\/.*//g;
  $line=~s/,//g;
  $line=~s/ //g;
  if ($line =~ "^MPI" || $line =~ "^SMP" || $line =~ "^PSM" || $line =~ "^IBV" || $line =~ "^PROCESS" || $line =~ "^CL" || $line =~ "^ZE" || $line =~ "^OMPT" || $line =~ "^UCX" || $line =~ "^UCP" || $line =~ "^OFI" || $line =~ "^FI") {
    my @line1 = split('=',$line);
#    print STDERR "$line1[0]  $line1[1] \n";
    if ($line1[1] eq "") {
      $mpiFuncID{$line1[0]} = $enumCounter;
    } else {
      $mpiFuncID{$line1[0]} = $line1[1];
      $enumCounter = $line1[1];
    }
    make_stat_func_id_2($line1[0]);
    if (exists $mpi_func_IDs{$line1[0]} ) {
      make_func_name($mpi_func_IDs{$line1[0]});
      make_func_id_trans($mpi_func_IDs{$line1[0]});
    } else {
      my @line=($line1[0],$line1[0],$line1[0]);
      make_func_name(\@line);
      make_func_id_trans(\@line);
    }
#    print STDERR "$line1[0]  $mpiFuncID{$line1[0]} \n";
    $enumCounter++;
  }
}
close (IN);

print OUT_TH <<EOF;
    MPI_MAX_FUNCID_THROUGH
};
#endif /*__MPI_FUNC_IDS_THROUGH_H__*/
EOF

print OUT_NAME <<EOF;
};
#endif /*__MPI_FUNC_NAMES_H__*/
EOF

print OUT_TR <<EOF;
};
#endif /*__MPI_FUNC_IDS_TRANS_H__*/
EOF

close(OUT_TH);
close(OUT_NAME);
close(OUT_TR);


select(stdout);

print "Completed.$linecount\n"; 

