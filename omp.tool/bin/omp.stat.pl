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
use Term::ReadLine;

sub print_nested;
####################################################################################
sub remove_commas {
my $number=shift;
$number=~ s/,//g;
return $number;
}

####################################################################################
sub max{
my $number1=shift;
my $number2=shift;
if ($number1>$number2) {return $number1;} else {return $number2};
}

####################################################################################
sub min{
my $number1=shift;
my $number2=shift;
if ($number1<$number2) {return $number1;} else {return $number2};
}

####################################################################################
#control switches 
my $single_file=0;
my $interval=0.001;
my $scm=1;
my $start_time=0;
my $stop_time=0;
my $print_num=0;
my $print_kst=0;
my $print_csv=0;
my $print_content=1;
my $adv_parsing_content=0;
my $def_freq=2600000000;
my $inp_freq=0;
my $separator=',';
my $scale=1;
my $printed_scale='Ratio';
my $def_bin_file;
my $input_mask;
my $strip_path=0;
my $statfile_n;
my $statfile_s=0;
my $eventfile_s=0;
my $eventfile_n;
my $regionfile_s=0;
my $regionfile_n;
my $taskfile_s=0;
my $taskfile_n;
my $nestedfile_s=0;
my $nestedfile_n;

####################################################################################
my @bin_addrs=();
my $first_start_event=-1;
my $last_stop_event=-1;
my $last_start_od=-1;
my $last_stop_od=-1;

my @total_microtask_activity=();
my @total_task_activity=();
my @total_task_fork_activity=();
my @total_task_join_activity=();
my @total_task_plain_activity=();
my @total_crit_sec_activity=();
my @total_single_activity=();
my @total_critical_barrier_activity=();
my @total_plain_barrier_activity=();
my @total_fork_barrier_activity=();
my @total_join_barrier_activity=();

my @total_rank_time=();

my @microtask_activity=();
my @microtask_activity_s=();
my @microtask_start_addr=();
my @microtask_start_tsc=();
my @microtask_addr=();


my @task_activity=();
my @task_activity_s=();
my @task_start_addr=();
my @task_start_tsc=();
my @task_addr=();

my @tasks_on_fork=();
my @tasks_on_join=();
my @tasks_on_plain=();
my @tasks_on_taskwait=();
my @tasks_on_par_reg=();

my @crit_sec_activity=();
my @crit_sec_activity_s=();
my @crit_sec_start_addr=();
my @crit_sec_start_tsc=();
my @crit_sec_addr=();

my @single_activity=();
my @single_activity_s=();
my @single_start_addr=();
my @single_start_tsc=();
my @single_addr=();

my @critical_barrier_activity=();
my @critical_barrier_activity_s=();
my @critical_barrier_start_addr=();
my @critical_barrier_start_tsc=();
my @critical_barrier_addr=();

my @plain_barrier_activity=();
my @plain_barrier_activity_s=();
my @plain_barrier_start_addr=();
my @plain_barrier_start_tsc=();
my @plain_barrier_addr=();

my @fork_barrier_activity=();
my @fork_barrier_activity_s=();
my @fork_barrier_start_addr=();
my @fork_barrier_start_tsc=();
my @fork_barrier_addr=();

my @join_barrier_activity=();
my @join_barrier_activity_s=();
my @join_barrier_start_addr=();
my @join_barrier_start_tsc=();
my @join_barrier_addr=();

my @max_thread_id=();
my $max_rank_id=1;
my @proc_freq=();
my @proc_tsc_interval=();
#my @proc_bin_file=();

my $pmf=0;
my $pmr=0;
my %pid2rank=();
my %addr2line=();
my %handledTID;
my @rank_start_tsc;
my @rank_end_tsc;

my @rank_start_time;
my @rank_end_time;
my $print_warnings=0;
####################################################################################
sub printw {
my $message=shift;
if ($print_warnings) {
print STDERR $message;
}
}
####################################################################################
sub addthreadid {
my $tid=shift;
if (!exists  $handledTID{$tid}) {
#   print STDERR "Adding $tid";
   $handledTID{ "$tid" } = $tid ;
}
}

####################################################################################
sub getrankfrompid {
my $pid=shift;
if ($single_file==1) { return 0;}
if ($pmr==1) {
#Treat 'pid' as MPI rank
   if ($max_rank_id<=$pid) {$max_rank_id=$pid+1}; # dangerous but necessary
   return $pid;
}else{
#This is not an actual MPI rank. It's just called 'rank'
if (exists  $pid2rank{$pid}) {
   return $pid2rank { $pid };
} else {
   $pid2rank{ "$pid" } = $max_rank_id ;
   return $max_rank_id++;
}
}
}
####################################################################################
sub getlinefromaddr {
my $addr=shift;
my $rank=shift;
#print STDERR "Entering getlinefromaddr\n";
if ( 0+$print_content&0+8 ) {
#chomp($addr);
my ($bin_file,$minaddr,$maxaddr)=();
my $newaddr=-1;
my $line=0;
my $newaddr_valid=-1;
$addr=~ s/\(nil\)/0/g;
$addr=hex $addr;

if (exists  $addr2line{"$addr:$rank"}) {
#   print STDERR "(cached)".$addr2line { "$addr:$rank" }."\n";
   return $addr2line { "$addr:$rank" };
} else {
#  adjust addr and binfile
foreach my $value(@{$bin_addrs[$rank]}) {
     ($bin_file,$minaddr,$maxaddr)=split(":", $value);
     if ($addr>=$minaddr && $addr<=$maxaddr) {
        if ($bin_file =~/\.so/ ) { 
#           print STDERR "Shared object: $bin_file \n";
           $addr-=$minaddr;
        }
        $newaddr=sprintf("%x", $addr);
        $newaddr_valid=1;
#        print STDERR "Hit to $bin_file with $newaddr ($addr $minaddr $maxaddr) \n";
        last;
     }
}
   if ($newaddr_valid>0) {
   my $addr2line_sc="addr2line -e $bin_file $newaddr 2>/dev/null";
#   print STDERR "(calling addr2line...$addr2line_sc)\n";
   $line=`$addr2line_sc`;
   chomp($line);
#   print STDERR "(returned $line)\n";
   if ($strip_path>0) {
      my @path=split('/',$line);
      $line=$path[$#path];
   }
   }else{
     $line=$addr;
   }

   $addr2line{ "$addr:$rank" } = $line;
   return $line;
}
}else{
return $addr;
}
}

####################################################################################
sub getpidfromfilename {
my $filename=shift;
my @pids = split('\.',$filename);
return $pids[$#pids-1];
}

sub getthreadfromfilename {
my $filename=shift;
my @pids = split('\.',$filename);
return $pids[$#pids];
}

# example of events
# Debug mode (abandoned)
# OMPStatEvent: Thread 21 21 : Application_Starts at 2974629724805006 on -1
my $events_start=8;
my $split_sign=7;
my @debug_par=("OMPStatEvent:",2,5,7,4,10,3,"/[\s]+/","Application_Starts","Application_Ends",
                                               "Microtask_Starts","Microtask_Ends",
                                               "Critical_Section_Starts","Critical_Section_Ends",
                                               "Critical_Barrier_Starts","Critical_Barrier_Ends",
                                               "Plain_Barrier_Starts","Plain_Barrier_Ends",
                                               "Fork_Barrier_Starts","Fork_Barrier_Ends",
                                               "Join_Barrier_Starts","Join_Barrier_Ends",
                                               "Single_Section_Starts","Single_Section_Ends",
                                               "Calibration_Starts","TSC_Calibration",
                                               "Frequency","Executable_File",
                                               "Serialized_Microtask_Starts","Serialized_Microtask_Ends",
                                               "Serialized_Team_Microtask_Starts","Serialized_Team_Microtask_Ends",
                                               "Fork_Call",
                                               "Start_On_Demand","Stop_On_Demand",
                                               "Task_Starts","Task_Ends",
                                               "TaskWait_Starts","TaskWait_Ends"
);
# Release mode                                  
#OSE:MS:3:0:0:0:18880196756035:(nil)
# id evt uid  tid gtid gmtid  tsc            addr
#OSE: MS: 13: 3: 3:   1:    18880196919040:(nil)


#tid=$par[1];
#event=$par[2];
#tsc=$par[3];
#addr=$par[5];
#gtid=$par[6];

my @release_par=("OSE",3,1,6,4,7,4,":","AS","AE",
                                   "MS","ME",
                                   "CSS","CSE",
                                   "CBS","CBE",
                                   "PBS","PBE",
                                   "FBS","FBE",
                                   "JBS","JBE",
                                   "SS","SE",
                                   "CLS","TSCC",
                                   "F","E",
                                   "SMS","SME",
                                   "FC",
                                   "STOD","SPOD",
                                   "TS","TE",
                                   "TWS","TWE"
);

my @par=@release_par;

####################################################################################
sub calibrate_counter {
my $lineref=shift;
my @line=@$lineref;
}


####################################################################################
sub set_freq{
   my ($lineref,$rank)=@_;
   my @line=@$lineref;
   if ($inp_freq>0) {
      $proc_freq[$rank]=$inp_freq;
   }else{
      chomp($line[$par[4]]);  # for freq it's different
      $proc_freq[$rank]=$line[$par[4]];
   }
   $proc_tsc_interval[$rank]=$proc_freq[$rank]*$interval;
print STDERR "freq $rank:$proc_freq[$rank]\n";
}

####################################################################################
sub set_def_freq{
   my $rank=shift;
   if ($inp_freq>0) {
      $proc_freq[$rank]=$inp_freq;
   }else{
      $proc_freq[$rank]=$def_freq;
   }
   $proc_tsc_interval[$rank]=$proc_freq[$rank]*$interval;
}

####################################################################################
#sub set_binary_file{
#   my ($lineref,$rank)=@_;
#   my @line=@$lineref;
#   chomp($line[$par[3]]);
#   $proc_bin_file[$rank]=$line[$par[3]];
#   print ("bin $rank:$proc_bin_file[$rank]");
#}

#sub set_def_binary_file{
#   my $rank=shift;
#   $proc_bin_file[$rank]=$def_bin_file;
#}

####################################################################################
my %fork_calls=();
my @fork_calls_tscs=();
my @fork_calls_addrs=();
my @fork_calls_lines=();
my @fork_calls_ranks=();
my @fork_calls_parse_lines=();
my $rank_fork_index=0;
my $max_fork_index;
my @fork_duration=();
my @microtask_duration=();
my @join_duration=();


my %tasks=();
my @task_tscs=();
my @task_addrs=();
my @task_lines=();
my @task_ranks=();
my @associated_task_num=();
my @task_thread=();
my $rank_task_index=0;
my $max_task_index;
my @task_duration=();
my @task_fork_duration=();


sub associate_fork_call {
my ($rank,$gtid,$addr,$tsc,$line)=@_;
my $index;
#print STDERR "quering for $rank$tsc...";
   if (exists $fork_calls{"$rank$tsc"}) {
      $index=$fork_calls { "$rank$tsc" };
   } else {
      $fork_calls{ "$rank$tsc" } = $rank_fork_index;
      $index=$rank_fork_index;
      $rank_fork_index++;
      chomp($addr);
      $fork_calls_tscs[$rank]->[$index]=$tsc;
      $fork_calls_addrs[$rank]->[$index]=$addr;
      $fork_calls_lines[$rank]->[$index]=getlinefromaddr($addr,$rank);
      $fork_calls_parse_lines[$rank]->[$index]=$line;
   }
#   print STDERR "resulting $index\n";
   return $index;
}

sub associate_task {
my ($rank,$gtid,$addr,$tsc,$fork_index)=@_;
my $index;
   if (exists $tasks{"$rank$fork_index"}) {
      $index=$tasks { "$rank$fork_index" };
      $associated_task_num[$rank]->[$index]++;
#     print STDERR "cached...\n";
   } else {
#      print STDERR "quering for $rank$fork_index...";
      $index=$rank_task_index;
      $tasks{ "$rank$fork_index" } = $rank_task_index;
      $associated_task_num[$rank]->[$index]=1;
      $rank_task_index++;
#      print STDERR "new...\n";
#      my $line=<STDIN>;
   }
      $task_ranks[$index] = $rank; 
      $task_thread[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=$gtid;
      chomp($addr);
      $task_tscs[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=$tsc;
      $task_addrs[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=$addr;
          $task_lines[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=getlinefromaddr($addr,$rank);
#   print STDERR "resulting $index\n";
   return $index;
}

sub process_fork_call {
my ($lineref,$rank,$fork_line)=@_;
my @line=@$lineref;
my $tid=$line[$par[1]];
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $addr=$line[$par[5]];
#print STDERR "associate fork call:$tid $gtid $addr $tsc $fork_line\n";
   return associate_fork_call($rank,$gtid,$addr,$tsc,$fork_line);
}

sub process_task_fork {
my ($lineref,$rank,$fork_index,$task_line)=@_;
my @line=@$lineref;
my $tid=$line[$par[1]];
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $addr=$line[$par[5]];
#print STDERR "associate fork call:$tid $gtid $addr $tsc $fork_line\n";
   return associate_task($rank,$gtid,$addr,$tsc,$fork_index,$task_line);
}

sub calc_fork_duration {
#assuming we're processing MS event 
my ($lineref,$rank,$index)=@_;
if ($index<0) {return;}
my @line=@$lineref;
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $duration=$tsc-$fork_calls_tscs[$rank]->[$index];
$fork_duration[$rank]->[$index][$gtid]=$duration;
#print STDERR "fork_duration $index $gtid $fork_duration[$index]->[$gtid]\n";
}

sub calc_task_fork_duration {
#assuming we're processing TS event 
my ($lineref,$rank,$index,$last_tsc)=@_;
if ($index<0) {return;}
my @line=@$lineref;
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $duration=$tsc-$last_tsc;
$task_fork_duration[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=$duration;
#print STDERR "task_fork_duration $index $associated_task_num[$rank]->[$index] $tsc $last_tsc $duration\n";
#my $line =<STDIN>;
}

sub calc_microtask_duration {
#assuming we're processing ME event 
my ($lineref,$rank,$index)=@_;
if ($index<0) {return;}
my @line=@$lineref;
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $duration=$tsc-($fork_calls_tscs[$rank]->[$index]+$fork_duration[$rank]->[$index][$gtid]);
$microtask_duration[$rank]->[$index][$gtid]=$duration;
#print STDERR "microtask_duration $index $gtid $microtask_duration[$rank]->[$index][$gtid]\n";
}

sub calc_task_duration {
#assuming we're processing TE event 
my ($lineref,$rank,$index,$last_tsc)=@_;
if ($index<0) {return;}
my @line=@$lineref;
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $duration=$tsc-$last_tsc;
$task_duration[$rank]->[$index][$associated_task_num[$rank]->[$index]-1]=$duration;
#print STDERR "microtask_duration $index $gtid $microtask_duration[$rank]->[$index][$gtid]\n";
}

sub calc_join_duration {
#assuming we're processing JBE event 
my ($lineref,$rank,$index)=@_;
if ($index<0) {return;}
my @line=@$lineref;
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $duration=$tsc-($fork_calls_tscs[$rank]->[$index]+$fork_duration[$rank]->[$index][$gtid]+$microtask_duration[$rank]->[$index][$gtid]);
$join_duration[$rank]->[$index][$gtid]=$duration;
#print STDERR "join_duration $index $gtid $join_duration[$rank]->[$index][$gtid]\n";
}

####################################################################################
my @nested_regions_list=();
my @nested_regions_occurences=();
my $min_chain=0;
sub process_region {
my ($rank,$tid,$gtid,$address,$level,$type,$chain_ref,$key)=@_;
   $$key="$$key$address";
   my $last=pop @$chain_ref;
   my @entry=split(":",$last);
   if ($entry[0]==$level && $entry[1]==$address &&  $entry[2]==$type &&  $entry[3]==$tid &&  $entry[4]==$gtid) {  
     $entry[5]++;
     push @$chain_ref,"$level:$address:$type:$tid:$gtid:$entry[5]";
   }else{
     push @$chain_ref,$last;
     push @$chain_ref,"$level:$address:$type:$tid:$gtid:1";
   }
}

sub confirm_region {
my ($rank,$tid,$gtid,$address,$level,$type,$chain_ref,$key)=@_;
my @chain=@$chain_ref;
if (exists $nested_regions_list[$rank]{$$key}) {
   $nested_regions_occurences[$rank]{$$key}=$nested_regions_occurences[$rank]{$$key}+1;
}else{
   $nested_regions_list[$rank]{$$key}=\@chain;
   $nested_regions_occurences[$rank]{$$key}=1;
}


}

####################################################################################
sub event_start {
my ($lineref,$start_tsc,$rank,$cur_start_tsc_ref,$cur_addr_ref,$update_fse)=@_;

my @line=@$lineref;
my $tid=$line[$par[1]];
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $rtsc=$tsc-$start_tsc;
my $addr=$line[$par[5]];

$$cur_start_tsc_ref[$gtid]=$rtsc;

     if ($max_thread_id[$rank] < $gtid) {
         $max_thread_id[$rank] = $gtid;
     }
chomp($addr);
$addr=getlinefromaddr($addr,$rank);
$$cur_addr_ref[$gtid]=$addr;
#print "Event start called $tsc $rtsc $addr $line[$par[2]]\n";

if ($update_fse>0 && ($first_start_event==-1 || $first_start_event>$rtsc/$proc_freq[$rank]) && $rtsc/$proc_freq[$rank]>-$start_time) {$first_start_event=$rtsc/$proc_freq[$rank];}
}

####################################################################################
sub event_end {
my ($lineref,$start_tsc,$rank,$cur_start_tsc_ref,$cur_addr_ref,$start_addr_ref,$activity_ref,$activity_s_ref,$update_fse,$total_activity_refs,$total_activity_mults)=@_;

my @line=@$lineref;
my $tid=$line[$par[1]];
my $gtid=$line[$par[6]];
my $tsc=$line[$par[3]];
my $thread_start_tsc=max($$cur_start_tsc_ref[$gtid],$last_start_od-$start_tsc);
#my $thread_start_tsc=$$cur_start_tsc_ref[$gtid];
my $thread_end_tsc=$tsc-$start_tsc;

my $start_index=int($thread_start_tsc/$proc_tsc_interval[$rank]);
my $end_index=int($thread_end_tsc/$proc_tsc_interval[$rank]);
my $j=$start_index;
#print STDERR "\n--->$lineref ::: $line[0] $line[1] $line[2] $line[3] $line[4] ::: $total_activity_refs ::: $total_activity_mults<---\n";
my @activities=@$total_activity_refs;
my @mults=@$total_activity_mults;
my $i;

     if ($max_thread_id[$rank] < $gtid) {
         $max_thread_id[$rank] = $gtid;
     }

     do {
       $$activity_ref[$rank]->[$gtid][$j]=1;
       $$activity_s_ref[$rank]->[$gtid][$j]=1;
       $$start_addr_ref[$rank]->[$gtid][$j]=$$cur_addr_ref[$gtid];
       $j++;
     } while($j < $end_index);
#     if ($thread_end_tsc<$$cur_start_tsc_ref[$gtid]) {
#        print STDERR "\n$thread_end_tsc $$cur_start_tsc_ref[$gtid] $gtid $tid $tsc $line[0] $line[1] $line[2] $line[3] $line[4] $line[5] \n"; die;
#     }
     if ($update_fse>0 && ($last_stop_event<$thread_end_tsc/$proc_freq[$rank]) && $thread_end_tsc/$proc_freq[$rank]>-$start_time) {$last_stop_event=$thread_end_tsc/$proc_freq[$rank];}
     for ($i=0;$i<@activities;$i++) {
        my $cur_act=$activities[$i];
        my $cur_mult=$mults[$i];
#        print STDERR "+ $cur_mult + \n";
        $$cur_act[$rank]->[$gtid]=$$cur_act[$rank]->[$gtid]+($thread_end_tsc-$$cur_start_tsc_ref[$gtid])*$cur_mult;
     }  
}


####################################################################################
sub parse_map_file{
my $file=shift;
my @line=();
my @minmaxaddr=();
my $exepath=0;
my $rank=0;
my @local_bin_addr=();
if ($pmf) {
  my $pid=getpidfromfilename($file);
  $rank=getrankfrompid($pid);
#  print("rank:$rank");
}
my $thread=getthreadfromfilename($file);
#print STDERR "----$file-----\n";
open(IN,"maps.$file") or return -1;
print STDERR "Parsing map file maps.$file\n";
while( <IN> ) {
  chomp($_);
  @line = split(" ",$_);
  $exepath=$line[5];
  @minmaxaddr = split("-",$line[0]);
  if ($exepath ne "") {
     $minmaxaddr[0]= hex $minmaxaddr[0];
     $minmaxaddr[1]= hex $minmaxaddr[1];
     push  @local_bin_addr, "$exepath:$minmaxaddr[0]:$minmaxaddr[1]";
#     push  @bin_addrs[$rank], "$exepath:$minmaxaddr[0]:$minmaxaddr[1]";
  }
}
$bin_addrs[$rank]=\@local_bin_addr;
#foreach my $value(@{$bin_addrs[$rank]}) {
#     print STDERR "0 $value\n";
#}
close (IN);
return $rank;
}
####################################################################################
sub parse_file{
my $file=shift;
my $totallinecount=shift;
my $include_thread_0=shift;
my @line;
my $event;
my $rank=0;
my $linecount=0;
my $next_percentage=10;
my $cur_percentage=0;
my $inside_microtask=0;
my $inside_fork_barrier=0;
my $inside_join_barrier=0;
my $inside_plain_barrier=0;
my $inside_taskwait_barrier=0;
my $fork_index=-1;
my $task_fork_index=-1;
my $cur_fork_index=0;
my $task_index=-1;
my $cur_task_index=0;
my $need_restart=0;
my $tsc;
my $last_join=0;
my $last_fork=0;
my $last_plain=0;
my $last_task=0;
my $nested_parreg_key;
my @nested_parreg_chain;
$last_start_od=-1;
$last_stop_od=-1;
%handledTID=();
if ($pmf) {
  my $pid=getpidfromfilename($file);
  $rank=getrankfrompid($pid);
#  print("rank:$rank");
}
my $thread=getthreadfromfilename($file);
if ($include_thread_0==0 && $thread==0) {return $rank;}
if ($proc_freq[$rank]<=0) {set_def_freq($rank)};
#if ($def_bin_file ne "") {set_def_binary_file($rank)};
#print STDERR "----$file-----\n";
open(IN,$file) or die "Can't open ouput file: $file !\n" ;
print STDERR "Parsing file $file 00\% ";

$rank_fork_index=0;
$fork_index=-1;
$rank_task_index=0;
$task_index=-1;

if ($totallinecount>0) {
while( <IN> ) {
  chomp($_);
  @line = split("$par[$split_sign]",$_);
#print STDERR "Parsing line: $_\n";
$cur_percentage=int($linecount*100 / $totallinecount);
$linecount++;
if ($cur_percentage>=$next_percentage) {print STDERR "\b\b\b\b$cur_percentage\% ";$next_percentage=$cur_percentage+10;}

      if ($line[0] ne "$par[0]") {next;} # application or debug output, skip it

#      print STDERR "Parsing line: $_  +++ $line[0] <--> $par[0]\n";

restart:
      $tsc=$line[$par[3]];
      if ($include_thread_0==0 && $thread>0) {
         if ($need_restart==0) {
             my $old_fork_index=$cur_fork_index;
             while ($tsc>$fork_calls_tscs[$rank]->[$cur_fork_index] && $fork_calls_tscs[$rank]->[$cur_fork_index]>0) {
                 $cur_fork_index++;
#                 print STDERR "adjusting fork index $cur_fork_index in progress, $tsc $fork_calls_tscs[$rank]->[$cur_fork_index]\n";
             };
             if ($cur_fork_index>0) {$cur_fork_index--;}
             $need_restart=$cur_fork_index!=$old_fork_index;
#                 print STDERR "adjusting fork index $cur_fork_index done $tsc $fork_calls_tscs[$rank]->[$cur_fork_index]\n";
             if ($need_restart==1) {
                 @line = split("$par[$split_sign]",$fork_calls_parse_lines[$rank]->[$cur_fork_index]);
                 $line[$par[1]]=$thread; # adjust tid
                 $line[$par[6]]=$thread; # adjust gtid
             }
         }else{
               @line = split("$par[$split_sign]",$_); #not pretty but simple
#               print STDERR "parsing $_ after restart\n";
               $need_restart=0;
         }
      }else{
               @line = split("$par[$split_sign]",$_); #not pretty but simple
               $need_restart=0;
      }
      $event=$line[$par[2]];

      switch ($event) {
#application starts
         case "$par[$events_start+0]" {
               $rank_start_tsc[$rank]->[$line[$par[6]]]=$line[$par[3]]; 
               $rank_start_time[$rank]->[$line[$par[6]]]=$rank_start_tsc[$rank]->[$line[$par[6]]]/$proc_freq[$rank];
               addthreadid($line[$par[6]]);
         }
#application ends
         case "$par[$events_start+1]" {
# fork barrier ends
               if ($inside_fork_barrier==1) {
                   my @activities=(\@total_fork_barrier_activity);
                   my @mults=(1);
                   $inside_fork_barrier=0;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@fork_barrier_start_tsc,\@fork_barrier_addr,\@fork_barrier_start_addr,\@fork_barrier_activity,
                   \@fork_barrier_activity_s,0,\@activities,\@mults);
               }

               $rank_end_tsc[$rank]->[$line[$par[6]]]=$line[$par[3]]; 
               $rank_end_time[$rank]->[$line[$par[6]]]=$rank_end_tsc[$rank]->[$line[$par[6]]]/$proc_freq[$rank];
               addthreadid($line[$par[6]]);
         } 
#microtask starts
         case "$par[$events_start+2]" {
# fork barrier ends
               if ($inside_fork_barrier==1) {
                  my @activities=(\@total_fork_barrier_activity);
                  my @mults=(1);

                  $inside_fork_barrier=0;
                  event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                  \@fork_barrier_start_tsc,\@fork_barrier_addr,\@fork_barrier_start_addr,
                  \@fork_barrier_activity,\@fork_barrier_activity_s,0,\@activities,\@mults);}

               if ($inside_microtask>0) {
                 printw "Warning! Nested parallel region detected! Started new team.\n";
#                 print STDERR "$_.\n";
#                 die;
                 if ($line[$par[6]]!=$line[$par[1]]) { # tid!=gtid ->new worker thread 
                   event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@microtask_start_tsc,\@microtask_addr,1);
                 }
               }else{
                 calc_fork_duration(\@line,$rank,$fork_index);
                 event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@microtask_start_tsc,\@microtask_addr,1);
               }
               process_region ($rank,$line[$par[1]],$line[$par[6]],$line[$par[5]],$inside_microtask,'M',\@nested_parreg_chain,\$nested_parreg_key);
               $inside_microtask++;
         }
#microtask ends
         case "$par[$events_start+3]" {
              if ($line[$par[6]]!=$line[$par[1]] || $inside_microtask==1) { # tid!=gtid ->new worker thread 
                my @activities=(\@total_microtask_activity);
                my @mults=(1);
                event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
               \@microtask_start_tsc,\@microtask_addr,\@microtask_start_addr,\@microtask_activity,
               \@microtask_activity_s,1,\@activities,\@mults);
                if ($inside_microtask==1) {calc_microtask_duration(\@line,$rank,$fork_index);}
              }
              if ($inside_microtask>0) {$inside_microtask--;}
# join barrier starts
               {$inside_join_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@join_barrier_start_tsc,\@join_barrier_addr,0);$last_join=$line[$par[3]];}
              if ($inside_microtask==0) {
                 confirm_region($rank,$line[$par[1]],$line[$par[6]],$line[$par[5]],$inside_microtask,'M',\@nested_parreg_chain,\$nested_parreg_key);
                 undef $nested_parreg_key;
                 @nested_parreg_chain=();
              }
         }
#serialized microtask starts
         case "$par[$events_start+20]" {
# fork barrier ends
               if ($inside_fork_barrier==1) {
                   my @activities=(\@total_fork_barrier_activity);
                   my @mults=(1);
                   $inside_fork_barrier=0;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@fork_barrier_start_tsc,\@fork_barrier_addr,\@fork_barrier_start_addr,\@fork_barrier_activity,
                   \@fork_barrier_activity_s,0,\@activities,\@mults);
               }

               process_region ($rank,$line[$par[1]],$line[$par[6]],$line[$par[5]],$inside_microtask,'S',\@nested_parreg_chain,\$nested_parreg_key);
               if ($inside_microtask==0) {
                 event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@microtask_start_tsc,\@microtask_addr,1);
                 $inside_microtask=1;
                 calc_fork_duration(\@line,$rank,$fork_index);
               }else{
                 printw "Warning! Nested parallel region detected. Serialized!\n";
                 $inside_microtask++;
               }
#               goto fork_barrier_ends;
         }
#serialized microtask ends
         case "$par[$events_start+21]" {
               if ($inside_microtask==1) {
                   my @activities=(\@total_microtask_activity);
                   my @mults=(1);
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@microtask_start_tsc,\@microtask_addr,\@microtask_start_addr,\@microtask_activity,
                   \@microtask_activity_s,1,\@activities,\@mults);
                   calc_microtask_duration(\@line,$rank,$fork_index);
                   $inside_microtask=0;
               }else{
                   $inside_microtask--;
               }
# join barrier starts
               {$inside_join_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@join_barrier_start_tsc,\@join_barrier_addr,0);$last_join=$line[$par[3]];}
               if ($inside_microtask==0) {
                  confirm_region($rank,$line[$par[1]],$line[$par[6]],$line[$par[5]],$inside_microtask,'M',\@nested_parreg_chain,\$nested_parreg_key);
                  undef $nested_parreg_key;
                  @nested_parreg_chain=();
               }
         }
#task starts
         case "$par[$events_start+25]" {
               event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@task_start_tsc,\@task_addr,0);
               $task_index=process_task_fork(\@line,$rank,$task_fork_index,$_);
               if ($inside_fork_barrier) {
                   calc_task_fork_duration(\@line,$rank,$task_index,$last_fork);
               }
               if ($inside_join_barrier) {
                   calc_task_fork_duration(\@line,$rank,$task_index,$last_join);
               }
               if ($inside_microtask) {
               if ($inside_plain_barrier) {
                   calc_task_fork_duration(\@line,$rank,$task_index,$last_plain);
               } else { 
               if ($inside_taskwait_barrier) {
                   calc_task_fork_duration(\@line,$rank,$task_index,$last_plain);
               }else{
                   calc_task_fork_duration(\@line,$rank,$task_index,$last_plain);
               }
               }
               }
               $last_task=$line[$par[3]];
#               if ($need_restart>0) { goto restart;}
         }
#task ends
         case "$par[$events_start+26]" {
#print STDERR "task ends $inside_fork_barrier $inside_join_barrier $inside_plain_barrier $inside_microtask\n";
               if ($inside_fork_barrier) {
                  my @activities=(\@total_task_activity,\@total_task_fork_activity,\@total_fork_barrier_activity,\@total_microtask_activity);
                  my @mults=(1,1,-((($scm+0)&8)>0),(($scm+0)&16)>0);
#print STDERR "task ends $mults[0] $mults[1] $mults[2] $mults[3]\n";
                  $tasks_on_fork[$rank]->[$line[$par[6]]]++;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@task_start_tsc,\@task_addr,\@task_start_addr,\@task_activity,\@task_activity_s,0,
                   \@activities,\@mults);
#my $line=<STDIN>;
                   $last_fork=$line[$par[3]];
               }
               if ($inside_join_barrier) {
                  my @activities=(\@total_task_activity,\@total_task_join_activity,\@total_join_barrier_activity,\@total_microtask_activity);
                  my @mults=(1,1,-((($scm+0)&8)>0),(($scm+0)&16)>0);
                  $tasks_on_join[$rank]->[$line[$par[6]]]++;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@task_start_tsc,\@task_addr,\@task_start_addr,\@task_activity,\@task_activity_s,0,
                   \@activities,\@mults);
                  $last_join=$line[$par[3]];
               }
               if ($inside_microtask) {
               if ($inside_plain_barrier) {
                  my @activities=(\@total_task_activity,\@total_task_plain_activity,\@total_plain_barrier_activity,\@total_microtask_activity);
                  my @mults=(1,1,-((($scm+0)&8)>0),(($scm+0)&16)>0);
                  $tasks_on_plain[$rank]->[$line[$par[6]]]++;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@task_start_tsc,\@task_addr,\@task_start_addr,\@task_activity,\@task_activity_s,0,
                   \@activities,\@mults);
                  $last_plain=$line[$par[3]];
               }else {
               if ($inside_taskwait_barrier){
                  my @activities=(\@total_task_activity,\@total_task_plain_activity,\@total_plain_barrier_activity,\@total_microtask_activity);
                  my @mults=(1,1,-((($scm+0)&8)>0),(($scm+0)&16)>0);
                  $tasks_on_taskwait[$rank]->[$line[$par[6]]]++;
                   event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                   \@task_start_tsc,\@task_addr,\@task_start_addr,\@task_activity,\@task_activity_s,0,
                   \@activities,\@mults);
                  $last_plain=$line[$par[3]];
               }else  {
                    my @activities=(\@total_task_activity);
                    my @mults=(1);
                    $tasks_on_par_reg[$rank]->[$line[$par[6]]]++;
                     event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
                     \@task_start_tsc,\@task_addr,\@task_start_addr,\@task_activity,\@task_activity_s,0,
                     \@activities,\@mults);
                    $last_plain=$line[$par[3]];
                  }
               }
               }
               calc_task_duration(\@line,$rank,$task_index,$last_task);
         }
#taskwait barrier starts
         case "$par[$events_start+27]" {$inside_taskwait_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@plain_barrier_start_tsc,\@plain_barrier_addr,0);$last_plain=$line[$par[3]];}
#taskwait barrier ends
         case "$par[$events_start+28]" {
         my @activities=(\@total_plain_barrier_activity,\@total_microtask_activity);
         my @mults=(1,-(($inside_microtask>0)*((($scm+0)&4)>0)));
         $inside_taskwait_barrier=0;
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
        \@plain_barrier_start_tsc,\@plain_barrier_addr,\@plain_barrier_start_addr,\@plain_barrier_activity,
        \@plain_barrier_activity_s,0,\@activities,\@mults);
        if ($inside_microtask<1) {printw "Warning! Taskwait is outside parallel region!\n";}
}

#critical section starts
         case "$par[$events_start+4]" {event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@crit_sec_start_tsc,\@crit_sec_addr,0);}
#critical section ends
         case "$par[$events_start+5]" {
         my @activities=(\@total_crit_sec_activity);
         my @mults=(1);
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
         \@crit_sec_start_tsc,\@crit_sec_addr,\@crit_sec_start_addr,\@crit_sec_activity,\@crit_sec_activity_s,0,
         \@activities,\@mults);
         if ($inside_microtask<1) {printw "Warning! Critical Section is outside parallel region!\n";}
         }
#critical barrier starts
         case "$par[$events_start+6]" {event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@critical_barrier_start_tsc,\@critical_barrier_addr,0);}
#critical barrier ends
         case "$par[$events_start+7]" {
         my @activities=(\@total_critical_barrier_activity,\@total_microtask_activity);
         my @mults=(1,-(($inside_microtask>0)*((($scm+0)&4)>0)));
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
         \@critical_barrier_start_tsc,\@critical_barrier_addr,\@critical_barrier_start_addr,
         \@critical_barrier_activity,\@critical_barrier_activity_s,0,
         \@activities,\@mults);
         }
#plain barrier starts
         case "$par[$events_start+8]" {$inside_plain_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@plain_barrier_start_tsc,\@plain_barrier_addr,0);$last_plain=$line[$par[3]];}
#plain barrier ends
         case "$par[$events_start+9]" {
         my @activities=(\@total_plain_barrier_activity,\@total_microtask_activity);
         my @mults=(1,-(($inside_microtask>0)*((($scm+0)&4)>0)));
         $inside_plain_barrier=0;
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
        \@plain_barrier_start_tsc,\@plain_barrier_addr,\@plain_barrier_start_addr,\@plain_barrier_activity,
        \@plain_barrier_activity_s,0,\@activities,\@mults);
        if ($inside_microtask<1) {printw "Warning! Barrier is outside parallel region!\n";}
}
#fork barrier starts
         case "$par[$events_start+10]" {$inside_fork_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@fork_barrier_start_tsc,\@fork_barrier_addr,0);$last_fork=$line[$par[3]];}
#fork barrier ends (deprecated)
         case "$par[$events_start+11]" {
         my @activities=(\@total_fork_barrier_activity,\@total_microtask_activity);
         my @mults=(1,-(($inside_microtask>0)*((($scm+0)&4)>0)));
         $inside_fork_barrier=0;
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
         \@fork_barrier_start_tsc,\@fork_barrier_addr,\@fork_barrier_start_addr,\@fork_barrier_activity,\@fork_barrier_activity_s,0,
         \@activities,\@mults);}
#join barrier starts (deprecated)
         case "$par[$events_start+12]" {$inside_join_barrier=1;event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@join_barrier_start_tsc,\@join_barrier_addr,0);}
#join barrier ends
         case "$par[$events_start+13]" {
         my @activities=(\@total_join_barrier_activity,\@total_microtask_activity);
         my @mults=(1,-(($inside_microtask>0)*((($scm+0)&4)>0)));
         $inside_join_barrier=0;
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
         \@join_barrier_start_tsc,\@join_barrier_addr,\@join_barrier_start_addr,\@join_barrier_activity,\@join_barrier_activity_s,0,
         \@activities,\@mults);
         if ($inside_microtask==0) {
            calc_join_duration(\@line,$rank,$fork_index);
            $fork_index=-1; #reset it
         }
         }
#single starts
         case "$par[$events_start+14]" {event_start(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,\@single_start_tsc,\@single_addr,0);}
#single ends
         case "$par[$events_start+15]" {
         my @activities=(\@total_single_activity);
         my @mults=(1);
         event_end(\@line,$rank_start_tsc[$rank]->[$line[$par[6]]],$rank,
         \@single_start_tsc,\@single_addr,\@single_start_addr,\@single_activity,\@single_activity_s,0,
         \@activities,\@mults);
         if ($inside_microtask<1) {printw "Warning! Single (or MASTER) is outside parallel region!\n";}
         }
#########
         case "$par[$events_start+16]" {if ($rank_end_tsc[$rank]->[$line[$par[1]]]<=0) {$rank_end_tsc[$rank]->[$line[$par[1]]]=$line[$par[3]]; $rank_end_time[$rank]->[$line[$par[1]]]=$rank_end_tsc[$rank]->[$line[$par[1]]]/$proc_freq[$rank];addthreadid($line[$par[1]]);} } # workaround since _end_application_ sometimes isn't called NOT ok to rewrite 
         case "$par[$events_start+17]" {calibrate_counter(\@line);}
         case "$par[$events_start+18]" {set_freq(\@line,$rank);}
#         case "$par[$events_start+19]" {set_binary_file(\@line,$rank);}
         case "$par[$events_start+22]" {
               if ($inside_microtask==0) {
                   $fork_index=process_fork_call(\@line,$rank,$_);$task_fork_index=$fork_index;
               }
#               if ($need_restart>0) { goto restart;}
         }
         case "$par[$events_start+23]" {$last_start_od=$line[$par[3]];$inside_microtask=0;}
         case "$par[$events_start+24]" {$last_stop_od=$line[$par[3]];} # technicaly "drop" code should be added here
#         else {;}
      }
      if ($need_restart>0) { goto restart;}
#if ($cur_percentage>30) {
#print_nested;
#die;
#}
}
}
close (IN);
print STDERR "\b\b\b\b100% Parsing completed\n";
#my $duration=($end_tsc-$start_tsc)/$proc_freq[$rank];
#print "file=$file rank=$rank $total_rank_time[$rank] $duration\n";
#print "$end_tsc:$start_tsc:$proc_freq[$rank]\n";
if ($max_fork_index<$rank_fork_index) {
    $max_fork_index=$rank_fork_index;
}
if ($max_task_index<$rank_task_index) {
    $max_task_index=$rank_task_index;
}
return $rank;
}

####################################################################################
sub print_help {
  print STDERR "Usage:\n";
  print STDERR "$0 switches\n";
  print STDERR "--help, -h - print this help\n";
  print STDERR "-i file - specifies input files template\n";
  print STDERR "-is file - specifies single input file\n";
  print STDERR "-os file - specifies statistic (output) file (stdout is the default)\n";
  print STDERR "-oe file - specifies events (output) file (stdout is the default)\n";
  print STDERR "-or file - specifies regions (output) file (stdout is the default)\n";
  print STDERR "-ot file - specifies tasks (output) file (stdout is the default)\n";
  print STDERR "-on file - nested regions (output) file (stdout is the default)\n";
  print STDERR "-e file - specifies default binary file\n";
  print STDERR "-d interval - selects resolution interval\n";
  print STDERR "-cn level - selects minimum chain length for nested regions\n";
  print STDERR "-f - overrides default (2600000000) processor frequency\n";
  print STDERR "-p N - print threadid+1 (N=1) instead of 1 (N=0,default)\n";
  print STDERR "-pmr - preserve MPI ranks (treat numbers in filenames as MPI ranks)\n";
  print STDERR "-kst - produce kst text file (incompatible with -csv)\n";
  print STDERR "-csv - produce csv file (incompatible with -kst)\n";
  print STDERR "-s separator - use alternative separator(default is ',')\n";
  print STDERR "-sc scale - scale values to 0 scale (default is 1)\n";
  print STDERR "-st - strip paths from line numbers \n";
  print STDERR "-w - prints warnings \n";
  print STDERR "-pmf - process multiple files (usually from different MPI ranks)\n";
  print STDERR "-scm bitmask - statistic calculation method \n";
  print STDERR "\tbit 0 - based on resolution intervals (default)\n";
  print STDERR "\tbit 1 - based on original intervals (may not work if nested parallel regions occured, ignores -t as well)\n";
  print STDERR "\tbit 2 - subtract barriers from parallel regions\n";
  print STDERR "\tbit 3 - subtract task durations from barriers\n";
  print STDERR "\tbit 4 - add task durations to parallel regions\n";
  print STDERR "-pc bitmask - specifies basic printing content \n";
  print STDERR "\tbit 0 - total number of active threads per interval\n";
  print STDERR "\tbit 1 - detailed thread activity per interval\n";
  print STDERR "\tbit 2 - activity among all intervals\n";
  print STDERR "\tbit 3 - start address or source line number (if available)\n";
  print STDERR "\tbit 4 - thread histogram\n";
  print STDERR "\tbit 5 - detailed parallel region statistic\n";
  print STDERR "\tbit 6 - task statistic\n";
  print STDERR "\tbit 7 - \"nested\" regions\n";
  print STDERR "\trest bits are reserved\n";
  print STDERR "-apc bitmask - specifies additional parsing and printing content \n";
  print STDERR "\tbit 0 - single sections (includes master)\n";
  print STDERR "\tbit 1 - critical sections\n";
  print STDERR "\tbit 2 - critical barriers\n";
  print STDERR "\tbit 3 - plain barriers\n";
  print STDERR "\tbit 4 - fork barriers\n";
  print STDERR "\tbit 5 - join barriers\n";
  print STDERR "\tbit 6 - tasks\n";
  print STDERR "\trest bits are reserved\n";
  print STDERR "-t start stop - specifies time intervals (in seconds from the beginning and from ending) ";
  print STDERR "if start and stop are negatives then additionaly skip to/from time of first/last microtask event \n";
  exit 0;
}

####################################################################################
sub parse_command_line {
   my $i;my $arg;
   if ($#ARGV<0) {print_help;}
   for ($i=0; $i<=$#ARGV; $i++) {
      $arg=$ARGV[$i];
      switch ($arg) {
         case "-h" {print_help;}
         case "-help" {print_help;}
         case "-i" {$input_mask=$ARGV[$i+1];$i++;}
         case "-is" {$input_mask=$ARGV[$i+1];$single_file=1;$i++;}
         case "-os" {$statfile_n=$ARGV[$i+1];$statfile_s=1;$i++;}
         case "-oe" {$eventfile_n=$ARGV[$i+1];$eventfile_s=1;$i++;}
         case "-ot" {$taskfile_n=$ARGV[$i+1];$taskfile_s=1;$i++;}
         case "-on" {$nestedfile_n=$ARGV[$i+1];$nestedfile_s=1;$i++;}
         case "-or" {$regionfile_n=$ARGV[$i+1];$regionfile_s=1;$i++;}
         case "-e" {$def_bin_file=$ARGV[$i+1];$i++;}
         case "-d" {$interval=$ARGV[$i+1];$i++;}
         case "-cn" {$min_chain=max($ARGV[$i+1],0);$i++;}
         case "-f" {$inp_freq=$ARGV[$i+1];$i++;}
         case "-pc" {$print_content=$ARGV[$i+1];$i++;}
         case "-apc" {$adv_parsing_content=$ARGV[$i+1];$i++;}
         case "-scm" {$scm=$ARGV[$i+1];$i++;}
         case "-p" {$print_num=$ARGV[$i+1];$i++;}
         case "-kst" {$print_kst=1;}
         case "-csv" {$print_csv=1;}
         case "-s" {$separator=$ARGV[$i+1];$i++; if ($separator eq 'TAB' || $separator eq 'tab' || $separator eq '\\t') {$separator="\t";}}
         case "-sc" {
                        $scale=$ARGV[$i+1];
                        $i++;
                        switch ($scale) {
                            case "1" {$printed_scale='Ratio';}
                            case "100" {$printed_scale='Percent';}
                            else {$printed_scale='Scaled Ratio'};
                        }
                    }
         case "-st" {$strip_path=1;}
         case "-w" {$print_warnings=1;}
         case "-t" {$start_time=$ARGV[$i+1];$stop_time=$ARGV[$i+2];$i+=2;}
         case "-pmf" {$pmf=1;$max_rank_id=0;}
         case "-pmr" {$pmr=1;}
         else {print "Unknown switch $ARGV[$i]\n";print_help;}
      }
   }
if ($print_kst && $print_csv) {print STDERR "You cannot specify both csv and kst output at a time\n";print_help;}
if ($print_kst==0 && $print_csv==0) {$print_csv=1;}
#single section
if (!(0+$adv_parsing_content&1+0)) {
   $par[$events_start+14]="$par[$events_start+14]-NotParsed";
   $par[$events_start+15]="$par[$events_start+15]-NotParsed";
 }
#critical section
if (!(0+$adv_parsing_content&2+0)) {
   $par[$events_start+4]="$par[$events_start+4]-NotParsed";
   $par[$events_start+5]="$par[$events_start+5]-NotParsed";
}
#critical barriers
if (!(0+$adv_parsing_content&4+0)) {
   $par[$events_start+6]="$par[$events_start+6]-NotParsed";
   $par[$events_start+7]="$par[$events_start+7]-NotParsed";
}

#plain barriers
if (!(0+$adv_parsing_content&8+0)) {
   $par[$events_start+8]="$par[$events_start+8]-NotParsed";
   $par[$events_start+9]="$par[$events_start+9]-NotParsed";
}

#fork barriers
if (!(0+$adv_parsing_content&16+0)) {
   $par[$events_start+10]="$par[$events_start+10]-NotParsed";
   $par[$events_start+11]="$par[$events_start+11]-NotParsed";
}

#join barriers
if (!(0+$adv_parsing_content&32+0)) {
   $par[$events_start+12]="$par[$events_start+12]-NotParsed";
   $par[$events_start+13]="$par[$events_start+13]-NotParsed";
}

#tasks
if (!(0+$adv_parsing_content&64+0)) {
   $par[$events_start+25]="$par[$events_start+25]-NotParsed";
   $par[$events_start+26]="$par[$events_start+26]-NotParsed";
}

}


####################################################################################
sub print_total_activity {
my $message=shift;
my $duration=shift;
my $activity_s=shift;
my $cb_activity_s=shift;
my $pb_activity_s=shift;
my $t_activity_s=shift;
my $total_activity=shift;
my $total_cb_activity=shift;
my $total_pb_activity=shift;
my $total_t_activity=shift;
my $cb_multiplier=shift;
my $pb_multiplier=shift;
my $t_multiplier=shift;

if (0+$scm&1+0) {
  my $start_index=int($start_time/$interval);
  my $end_index=int(($duration-$stop_time)/$interval);
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid=max($max_thread_id[$rank],$max_tid);
  }
  print "Total $message (based on resolution intervals)\n";
  print "Thread $separator";
    for (my $j=0 ; $j <= $max_tid ; $j++) {
      print " $j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    print "Rank $rank $separator";
    for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
      my $local_sum=0;
      for (my $i=$start_index; $i < $end_index ; $i++) {
        $local_sum+=$$activity_s[$rank]->[$j][$i]+
        $$cb_activity_s[$rank]->[$j][$i]*(($$activity_s[$rank]->[$j][$i]>0)?$cb_multiplier:0)+
        $$pb_activity_s[$rank]->[$j][$i]*(($$activity_s[$rank]->[$j][$i]>0)?$pb_multiplier:0)+
        $$t_activity_s[$rank]->[$j][$i]*(($$activity_s[$rank]->[$j][$i]>0 || $t_multiplier>0)?$t_multiplier:0);
      }
      my $thread_activity_p=($local_sum/($end_index-$start_index))*$scale;
      print "$thread_activity_p $separator";
    }
  print "\n";
  }
}

if (0+$scm&2+0) {
   print "Total $message \n";
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
  print "Thread $separator";
    for (my $j=0 ; $j <= $max_tid ; $j++) {
      print "$j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    print "Rank $rank $separator";
    for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
      my $activity_p=(($$total_activity[$rank]->[$j]
#+
#      $$total_cb_activity[$rank]->[$j]*$cb_multiplier+
#      $$total_pb_activity[$rank]->[$j]*$pb_multiplier+
#      $$total_t_activity[$rank]->[$j]*$t_multiplier
)
      /$total_rank_time[$rank])*$scale;
      print "$activity_p $separator";
    }
  print "\n";
  }
}

} 
sub print_histo {
my $message=shift;
my $duration=shift;
my $activity_s=shift;
my @histo;

  my $start_index=int($start_time/$interval);
  my $end_index=int(($duration-$stop_time)/$interval);
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
  print "Distribution: $message (based on resolution intervals)\n";
  print "Threads $separator";
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      print "$j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    undef(@histo);
    print "Rank $rank $separator";
    for (my $i=$start_index; $i < $end_index ; $i++) {
      my $local_sum=0;
      for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
        $local_sum+=$$activity_s[$rank]->[$j][$i];
      }
      $histo[$local_sum]++;
    }
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      $histo[$j]/=($end_index-$start_index);
      $histo[$j]*=$scale;
      print "$histo[$j] $separator";
      }
  print "\n";
  }

}

sub print_histo2 {
my $message=shift;
my $duration=shift;
my $activity_s=shift;
my $csb_activity_s=shift;
my $psb_activity_s=shift;
my @histo;

  my $start_index=int($start_time/$interval);
  my $end_index=int(($duration-$stop_time)/$interval);
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
  print "Distribution: $message (based on resolution intervals)\n";
  print "Threads $separator";
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      print "$j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    undef(@histo);
    print "Rank $rank $separator";
    for (my $i=$start_index; $i < $end_index ; $i++) {
      my $local_sum=0;
      for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
        if ($$activity_s[$rank]->[$j][$i]>0 && $$csb_activity_s[$rank]->[$j][$i]==0 && $$psb_activity_s[$rank]->[$j][$i]==0) {
           $local_sum++;
        }
      }
      $histo[$local_sum]++;
    }
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      $histo[$j]/=($end_index-$start_index);
      $histo[$j]*=$scale;
      print "$histo[$j] $separator";
      }
  print "\n";
  }

}

sub print_histo3 {
my $message=shift;
my $duration=shift;
my $activity_s=shift;
my $csb_activity_s=shift;
my $psb_activity_s=shift;
my $t_activity_s=shift;
my @histo;

  my $start_index=int($start_time/$interval);
  my $end_index=int(($duration-$stop_time)/$interval);
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
  print "Distribution: $message (based on resolution intervals)\n";
  print "Threads $separator";
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      print "$j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    undef(@histo);
    print "Rank $rank $separator";
    for (my $i=$start_index; $i < $end_index ; $i++) {
      my $local_sum=0;
      for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
        if ($$activity_s[$rank]->[$j][$i]>0 && $$csb_activity_s[$rank]->[$j][$i]==0 && $$psb_activity_s[$rank]->[$j][$i]==0 || $$t_activity_s[$rank]->[$j][$i]>0) {
           $local_sum++;
        }
      }
      $histo[$local_sum]++;
    }
    for (my $j=0 ; $j <= $max_tid+1 ; $j++) {
      $histo[$j]/=($end_index-$start_index);
      $histo[$j]*=$scale;
      print "$histo[$j] $separator";
      }
  print "\n";
  }

}

sub print_section_activity {
my $message=shift;
my $duration=shift;
my $activity_s=shift;
if (0+$scm&1+0) {
  my $start_index=int($start_time/$interval);
  my $end_index=int(($duration-$stop_time)/$interval);
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
  print "$message activity (G)\n";
  print "$separator";
    for (my $j=0 ; $j <= $max_tid ; $j++) {
      print "Thread $j $separator";
      }
  print "\n";

  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    print "Rank $rank $separator";
    for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
      my $local_sum=0;
      for (my $i=$start_index; $i < $end_index ; $i++) {
        $local_sum+=$$activity_s[$rank]->[$j][$i];
      }
      my $thread_activity_p=$local_sum/($end_index-$start_index);
      print "$thread_activity_p $separator";
    }
  print "\n";
  }
}

} 

####################################################################################
sub print_TOC {
my $name=shift;
my $rank=shift;
  print "$name ";
  if (0+$print_content&0+2) {
    for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
      print "$separator";
    }
   }
  if (0+$print_content&0+1) {
    print "$separator";
  }
  if (0+$print_content&0+8 ) {
    print "$separator";
  }
}
sub print_heading {
my $rank=shift;
  if (0+$print_content&0+2) {
    for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
#      print "Thread $j $separator";
      print "$j $separator";
    }
   }
  my $m=$max_thread_id[$rank]+1;
  if (0+$print_content&0+1) {
    print "Total $rank (max $m) $separator";
  }
  if (0+$print_content&0+8 ) {
    print "Address $separator";
  }
}

sub print_stat_for_event {
my ($i,$rank,$activityref,$activity_sref,$start_addrref)=@_;
my $local_sum=0;
my $print_addr="";
my $print_addr_violators="";
my $same_addr4all=1;
my @start_addr=@$start_addrref;
my @activity=@$activityref;
my @activity_s=@$activity_sref;
######
      for (my $j=0 ; $j <= $max_thread_id[$rank] ; $j++) {
        $local_sum+=$activity_s[$rank]->[$j][$i];
        if (0+$print_content&0+2) {
          my $multiplyer=($print_num == 1)?($j+1):1;
          my $print_value=$activity[$rank]->[$j][$i]*$multiplyer;
          print "$print_value $separator";
        }
        if (0+$print_content&0+8 && $same_addr4all > 0) {
          if ($activity[$rank]->[$j][$i] > 0) {
              if ($print_addr eq "") { $print_addr=$start_addr[$rank]->[$j][$i]; }
              if ($print_addr ne $start_addr[$rank]->[$j][$i]) 
                 {
                    $print_addr_violators="$print_addr_violators $j:$start_addr[$rank]->[$j][$i]";
                    $same_addr4all=0;
                 }
           }
        }
       }
      if (0+$print_content&0+1) {
        print "$local_sum $separator";
      }
      if (0+$print_content&0+8 ) {
        print "$print_addr $print_addr_violators $separator";
      }
}

####################################################################################
#my @fork_calls_tscs=();
#my @fork_calls_addrs=();
#my @fork_duration=();
#my @microtask_duration=();
#my @join_duration=();

sub  print_par_reg_stat {
my $duration=shift;
#my $i;
my $j;
my $k;
my $rank=-1;
my $tot_fork_duration_sec=0;
my $tot_join_duration_sec=0;
my $tot_microtask_duration_sec=0;
my $tot_fork_overhead=0;
my $tot_join_overhead=0;
#make first pass to gather statisctics
my %parregs=();
my $diff_par_regs=0;
          for ($rank=0;$rank<$max_rank_id;$rank++) {
          for ($j=0;$j<$max_fork_index;$j++) {
              if (!exists $parregs{"$fork_calls_addrs[$rank]->[$j]-$fork_calls_lines[$rank]->[$j]"}) {
                  $parregs{"$fork_calls_addrs[$rank]->[$j]-$fork_calls_lines[$rank]->[$j]"}=1;
                  $diff_par_regs++;
              }else{
                  $parregs{"$fork_calls_addrs[$rank]->[$j]-$fork_calls_lines[$rank]->[$j]"}++;
              }
          }
          }
          for ($rank=0;$rank<$max_rank_id;$rank++) {
              print "Rank $rank\n";
          for ($j=0;$j<$max_fork_index;$j++) {
              print "\n";
              print "$separator Cycles$separator Cycles$separator Cycles$separator $printed_scale$separator $printed_scale$separator $printed_scale\n";
              print "Thread$separator";
              print "Fork$separator Microtask$separator Join$separator Fork/Microtask $separator Join/Microtask$separator Total\n";
              my $max_fork_duration=0;
              my $max_join_duration=0;
              my $max_microtask_duration=0;
              my $max_fork_duration_tid=0;
              my $max_join_duration_tid=0;
              my $max_microtask_duration_tid=0;
              my $min_fork_duration=-1;
              my $min_join_duration=-1;
              my $min_microtask_duration=-1;
              my $min_fork_duration_tid=0;
              my $min_join_duration_tid=0;
              my $min_microtask_duration_tid=0;
              my $avg_fork_duration=0;
              my $avg_join_duration=0;
              my $avg_microtask_duration=0;
 
              my $max_fork_overhead;
              my $max_join_overhead;
              my $max_total_overhead;
              my $min_fork_overhead;
              my $min_join_overhead;
              my $min_total_overhead;
              my $avg_fork_overhead;
              my $avg_join_overhead;
              my $avg_total_overhead;
              my @cur_fork_duration=();
              my @cur_microtask_duration=();
              my @cur_join_duration=();

              for ($k=0 ; $k <= $max_thread_id[$rank] ; $k++) {
                  $cur_fork_duration[$k]=$fork_duration[$rank]->[$j][$k];
                  $cur_microtask_duration[$k]=$microtask_duration[$rank]->[$j][$k];
                  $cur_join_duration[$k]=$join_duration[$rank]->[$j][$k];
                  
                  if ($max_fork_duration<$cur_fork_duration[$k]) {$max_fork_duration=$cur_fork_duration[$k];$max_fork_duration_tid=$k;}
                  if ($max_join_duration<$cur_join_duration[$k]) {$max_join_duration=$cur_join_duration[$k];$max_join_duration_tid=$k;}
                  if ($max_microtask_duration<$cur_microtask_duration[$k]) {$max_microtask_duration=$cur_microtask_duration[$k];$max_microtask_duration_tid=$k;}
                  if ($min_fork_duration>$cur_fork_duration[$k] || $min_fork_duration<0) {$min_fork_duration=$cur_fork_duration[$k];$min_fork_duration_tid=$k;}
                  if ($min_join_duration>$cur_join_duration[$k] || $min_join_duration<0) {$min_join_duration=$cur_join_duration[$k];$min_join_duration_tid=$k;}
                  if ($min_microtask_duration>$cur_microtask_duration[$k] ||  $min_microtask_duration<0) {$min_microtask_duration=$cur_microtask_duration[$k];$min_microtask_duration_tid=$k;}
              }

              for ($k=0 ; $k <= $max_thread_id[$rank] ; $k++) {
                  my $cur_fork_duration=$cur_fork_duration[$k];
                  my $cur_microtask_duration=$cur_microtask_duration[$k];
                  my $cur_join_duration=$cur_join_duration[$k];
                  my $fork_overhead=$cur_microtask_duration>0?$cur_fork_duration/$cur_microtask_duration*$scale:$max_fork_duration/$max_microtask_duration*$scale;
                  my $join_overhead=$cur_microtask_duration>0?$cur_join_duration/$cur_microtask_duration*$scale:$max_join_duration/$max_microtask_duration*$scale;
                  my $total_overhead=$fork_overhead+$join_overhead;

                  print "$k$separator"; # Thread
                  print "$cur_fork_duration$separator$cur_microtask_duration$separator$cur_join_duration$separator$fork_overhead$separator$join_overhead$separator$total_overhead$separator\n";
                  $avg_fork_duration+=$cur_fork_duration>0?$cur_fork_duration:$max_fork_duration;
                  $avg_join_duration+=$cur_join_duration>0?$cur_join_duration:$max_join_duration;
                  $avg_microtask_duration+=$cur_microtask_duration>0?$cur_microtask_duration:$max_microtask_duration;
              }
              
              $avg_fork_duration/=($max_thread_id[$rank]+1);
              $avg_join_duration/=($max_thread_id[$rank]+1);
              $avg_microtask_duration/=($max_thread_id[$rank]+1);
              $tot_fork_duration_sec+=$avg_fork_duration/$proc_freq[$rank];
              $tot_join_duration_sec+=$avg_join_duration/$proc_freq[$rank];
              $tot_microtask_duration_sec+=$avg_microtask_duration/$proc_freq[$rank];

              $max_fork_overhead=$min_microtask_duration>0?$max_fork_duration/$min_microtask_duration*$scale:-1;
              $max_join_overhead=$min_microtask_duration>0?$max_join_duration/$min_microtask_duration*$scale:-1;
              $max_total_overhead=$max_fork_overhead+$max_join_overhead;
              $min_fork_overhead=$max_microtask_duration>0?$min_fork_duration/$max_microtask_duration*$scale:-1;
              $min_join_overhead=$max_microtask_duration>0?$min_join_duration/$max_microtask_duration*$scale:-1;
              $min_total_overhead=$min_fork_overhead+$min_join_overhead;
              $avg_fork_overhead=$avg_microtask_duration>0?$avg_fork_duration/$avg_microtask_duration*$scale:-1;
              $avg_join_overhead=$avg_microtask_duration>0?$avg_join_duration/$avg_microtask_duration*$scale:-1;
              $avg_total_overhead=$avg_fork_overhead+$avg_join_overhead;

              print "AVG$separator$avg_fork_duration$separator$avg_microtask_duration$separator$avg_join_duration$separator$avg_fork_overhead$separator$avg_join_overhead$separator$avg_total_overhead$separator\n";
              print "\n";
              print "$separator Fork$separator Thread$separator Microtask$separator Thread$separator Join$separator Thread$separator Fork/Microtask $separator Join/Microtask$separator Total$separator\n";
              
              print "MAX$separator$max_fork_duration$separator$max_fork_duration_tid$separator$max_microtask_duration$separator$max_microtask_duration_tid$separator$max_join_duration$separator$max_join_duration_tid$separator$max_fork_overhead$separator$max_join_overhead$separator$max_total_overhead$separator\n";
              print "MIN$separator$min_fork_duration$separator$min_fork_duration_tid$separator$min_microtask_duration$separator$min_microtask_duration_tid$separator$min_join_duration$separator$min_join_duration_tid$separator$min_fork_overhead$separator$min_join_overhead$separator$min_total_overhead$separator\n";
              print "ADDR$separator$fork_calls_addrs[$rank]->[$j]$separator SRC$separator$fork_calls_lines[$rank]->[$j]\n";
              print "\n";
          }
          }
          print "\n";
          print "Summary:\n";
          print "Total Parallel regions $separator$max_fork_index \n";
          print "Different Parallel regions $separator$diff_par_regs\n";
          print "Total Averaged Section Duration $separator$tot_microtask_duration_sec of $duration (sec)\n";
          $tot_microtask_duration_sec/=$max_fork_index>0?$max_fork_index:1;
          $tot_fork_duration_sec/=$max_fork_index>0?$max_fork_index:1;
          $tot_join_duration_sec/=$max_fork_index>0?$max_fork_index:1;
          $tot_fork_overhead=$tot_microtask_duration_sec>0?$tot_fork_duration_sec/$tot_microtask_duration_sec*$scale:-1;
          $tot_join_overhead=$tot_microtask_duration_sec>0?$tot_join_duration_sec/$tot_microtask_duration_sec*$scale:-1;
          print "\n";
          print "Average Section Duration $separator$tot_microtask_duration_sec (sec)\n";
          print "Average Fork Duration $separator$tot_fork_duration_sec (sec)\n";
          print "Average Join Duration $separator$tot_join_duration_sec (sec)\n";
          print "Average Fork Overhead $separator$tot_fork_overhead\n";
          print "Average Join Overhead $separator$tot_join_overhead\n";
          print "\n";
          print "Region Executions\n";
          print "Address $separator Source $separator Executions\n";
          while ( my ($address, $executions) = each(%parregs) ) {
              my ($addr,$line)=split('-',$address);
              print "$addr $separator $line $separator $executions\n";
          }


}


####################################################################################

sub  print_task_stat {
my $duration=shift;
#my $i;
my $j;
#my $k;
my $l;
my $rank=-1;
my $tot_fork_duration_sec=0;
my $tot_task_duration_sec=0;
my $tot_fork_overhead=0;
my $tot_on_fork=0;
my $tot_on_join=0;
my $tot_on_plain=0;
my $tot_on_taskwait=0;
my $tot_on_par_reg=0;
#make first pass to gather statisctics
          for ($rank=0;$rank<$max_rank_id;$rank++) {
              print "Rank $rank\n";
          for ($j=0;$j<$max_task_index;$j++) {
              if ($task_ranks[$j]!=$rank) {next;}
              print "Region $j\n";
              print "\n";
              print "$separator Cycles$separator Cycles$separator $printed_scale$separator $printed_scale\n";
              print "Thread$separator";
              print "Fork$separator Task$separator Fork/Task $separator ADDR $separator SRC $separator\n";

              #for ($k=0 ; $k <= $max_thread_id[$rank] ; $k++) {
                for ($l=0 ; $l < $associated_task_num[$rank]->[$j] ; $l++) {
                  my $cur_fork_duration=$task_fork_duration[$rank]->[$j][$l];
                  my $cur_task_duration=$task_duration[$rank]->[$j][$l];
                  my $fork_overhead=$cur_task_duration>0?$cur_fork_duration/$cur_task_duration*$scale:-1;
                  
                  print "$task_thread[$rank]->[$j][$l]$separator"; # Thread
                  print "$cur_fork_duration$separator$cur_task_duration$separator$fork_overhead$separator";
                  print "$task_addrs[$rank]->[$j][$l] $separator $task_lines[$rank]->[$j][$l]\n";
              }
              #}
          }
          }
          print "\n";
          print "Summary:\n";
          print "Total Tasks $separator$max_task_index \n";

          print "Task Executions\n";
          for ($rank=0;$rank<$max_rank_id;$rank++) {
          print "Rank $rank\n";
          print "Thread $separator Fork $separator Join $separator Plain $separator TaskWait $separator Other \n";
          for ($j=0;$j<=$max_thread_id[$rank];$j++) {
             $tasks_on_fork[$rank]->[$j]=$tasks_on_fork[$rank]->[$j]+0;
             $tasks_on_join[$rank]->[$j]=$tasks_on_join[$rank]->[$j]+0;
             $tasks_on_plain[$rank]->[$j]=$tasks_on_plain[$rank]->[$j]+0;
             $tasks_on_taskwait[$rank]->[$j]=$tasks_on_taskwait[$rank]->[$j]+0;
             $tasks_on_par_reg[$rank]->[$j]=$tasks_on_par_reg[$rank]->[$j]+0;
             print "$j $separator $tasks_on_fork[$rank]->[$j] $separator $tasks_on_join[$rank]->[$j] $separator $tasks_on_plain[$rank]->[$j] $separator $tasks_on_taskwait[$rank]->[$j]$separator $tasks_on_par_reg[$rank]->[$j]\n";
             $tot_on_fork+=$tasks_on_fork[$rank]->[$j];
             $tot_on_join+=$tasks_on_join[$rank]->[$j];
             $tot_on_plain+=$tasks_on_plain[$rank]->[$j];
             $tot_on_taskwait+=$tasks_on_taskwait[$rank]->[$j];
             $tot_on_par_reg+=$tasks_on_par_reg[$rank]->[$j];
          }
          }

          print "Total\n";
          print "Fork $separator Join $separator Plain $separator TaskWait $separator Other $separator Overall\n";
          my $total_tasks=$tot_on_fork+$tot_on_join+$tot_on_plain+$tot_on_taskwait+$tot_on_par_reg;
          print "$tot_on_fork $separator $tot_on_join $separator $tot_on_plain $separator $tot_on_taskwait $separator $tot_on_par_reg $separator $total_tasks\n";

}

####################################################################################
#my @nested_regions_list=();
#my @nested_regions_occurences=();
sub print_indent {
my $indent=shift;
my $i;
   for ($i=0;$i<$indent;$i++) {
      print "$separator";
   }
}
sub  print_nested {
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
      print "Rank$separator $rank\n\n";
      my $chain=0;
      while(my ($key,$value) = each %{$nested_regions_list[$rank]}) {
            my $occurences=$nested_regions_occurences[$rank]{$key}+0;
            my @entries=@$value;
            my $level;
            my $i;
            $i=@entries;
            if (@entries>$min_chain) {
            print "Chain $chain$separator Occurences $occurences\n";
            for ($i=0;$i<@entries;$i++) {
               my $line;
               my ($level,$addr,$type,$tid,$gtid,$occ)=split(':',$entries[$i]);
               $line=getlinefromaddr($addr,$rank);
               print_indent($level);
               print "$line $separator $occ $separator $type $separator $tid $separator $gtid $separator\n";
            }
            print "End of Chain $chain\n\n";
            }
      $chain++;
      }
  }

}
####################################################################################
sub print_stat {
my $duration=shift;
my $start_index=int($start_time/$interval);
my $end_index=int(($duration-$stop_time)/$interval);
my @local_sum;
print STDERR "Printing Statistics:\n";
if ($statfile_s) {
  open(STATFILE,">$statfile_n") or die ("Cannot open statistic file ($statfile_n) for writing");
  select(STATFILE);
}
  my $max_tid=0;
  for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
    $max_tid= max($max_thread_id[$rank],$max_tid);
  }
$max_tid++;
print "duration=$duration $separator resolution interval=$interval $separator Ranks=$max_rank_id $separator Threads=$max_tid\n";
print "start time=$start_time $separator stop time=$stop_time \n";

if (0+$print_content&0+4) {
print STDERR "Printing Summary...\n";
  print_total_activity("Parallel Regions",$duration,
  \@microtask_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
  \@total_microtask_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_activity,
  -((($scm+0)&4)>0),-((($scm+0)&4)>0),(($scm+0)&16)>0);
  if (0+$adv_parsing_content&0+1) { # single
    print_total_activity("Single Section",$duration,
    \@single_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_single_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_activity,
    0,0,0);
  }
  if (0+$adv_parsing_content&0+2) { #critical
    print_total_activity("Critical Section",$duration,
   \@crit_sec_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
   \@total_crit_sec_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_activity,
   0,0,0);
  }
  if (0+$adv_parsing_content&0+4) { #critical barrier
    print_total_activity("Critical Barrier",$duration,
    \@critical_barrier_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_critical_barrier_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_activity,
    0,0,0);
  }
  if (0+$adv_parsing_content&0+8) { #plain barrier
    print_total_activity("Plain Barrier",$duration,
    \@plain_barrier_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_plain_barrier_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_plain_activity,
    0,0,-((($scm+0)&8)>0));
  }
  if (0+$adv_parsing_content&0+16) { #fork barrier
    print_total_activity("Fork Barrier",$duration,
    \@fork_barrier_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_fork_barrier_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_fork_activity,
    0,0,-((($scm+0)&8)>0));
  }
  if (0+$adv_parsing_content&0+32) { #join barrier
    print_total_activity("Join Barrier",$duration,
    \@join_barrier_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_join_barrier_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_join_activity,
    0,0,-((($scm+0)&8)>0));
  }
  if (0+$adv_parsing_content&0+64) { #task
    print_total_activity("Task",$duration,
    \@task_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s,
    \@total_task_activity,\@total_critical_barrier_activity,\@total_plain_barrier_activity,\@total_task_activity,
    0,0,0);
  }
}
if (0+$print_content&0+16) {
  print STDERR "Printing Histograms...\n";
  print_histo("Parallel Regions",$duration,\@microtask_activity_s);
  print_histo2("Parallel Regions-Barries",$duration,\@microtask_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s);
  print_histo3("Parallel Regions-Barries+Tasks",$duration,\@microtask_activity_s,\@critical_barrier_activity_s,\@plain_barrier_activity_s,\@task_activity_s);
  if (0+$adv_parsing_content&0+1) { # single
    print_histo("Single Section",$duration,\@single_activity_s);
  }
  if (0+$adv_parsing_content&0+2) { #critical
    print_histo("Critical Section",$duration,\@crit_sec_activity_s);
  }
  if (0+$adv_parsing_content&0+4) { #critical barrier
    print_histo("Critical Barrier",$duration,\@critical_barrier_activity_s);
  }
  if (0+$adv_parsing_content&0+8) { #plain barrier
    print_histo("Plain Barrier",$duration,\@plain_barrier_activity_s);
  }
  if (0+$adv_parsing_content&0+16) { #fork barrier
    print_histo("Fork Barrier",$duration,\@fork_barrier_activity_s);
  }
  if (0+$adv_parsing_content&0+32) { #join barrier
    print_histo("Join Barrier",$duration,\@join_barrier_activity_s);
  }
  if (0+$adv_parsing_content&0+64) { #task
    print_histo("Task",$duration,\@task_activity_s);
  }

}
if ($statfile_s) {
  close(STATFILE);
  select(STDOUT);
}

if (0+$print_content&0+32) {
  if ($regionfile_s) {
    open(REGIONFILE,">$regionfile_n") or die ("Cannot open Region file ($regionfile_n) for writing");
    select(REGIONFILE);
  }
  print STDERR "Printing Parallel Region Statistic...\n";
  print_par_reg_stat($duration);
  if ($regionfile_s) {
    close(REGIONFILE);
    select(STDOUT);
  }

}

if (0+$print_content&0+64) {
  if ($taskfile_s) {
    open(TASKFILE,">$taskfile_n") or die ("Cannot open Task file ($taskfile_n) for writing");
    select(TASKFILE);
  }
  print STDERR "Printing Task Statistic...\n";
  print_task_stat($duration);
  if ($taskfile_s) {
    close(TASKFILE);
    select(STDOUT);
  }

}

if (0+$print_content&0+128) {
  if ($nestedfile_s) {
    open(NESTED,">$nestedfile_n") or die ("Cannot open Nested Regions file ($nestedfile_n) for writing");
    select(NESTED);
  }
  print STDERR "Printing Nested Region Statistic...\n";
  print_nested;
  if ($nestedfile_s) {
    close(NESTED);
    select(STDOUT);
  }

}


if (!(0+$print_content&0+2)) {
print STDERR "Printing Details...\n";

if ($eventfile_s) {
open(EVENTFILE,">$eventfile_n") or die ("Cannot open Events file ($eventfile_n) for writing");
select(EVENTFILE);
}

#--------------------------------------#
if ($print_csv && (0+$print_content&0+3)) {
#0-th raw is for interval in case of csv
print "$separator";
} 
#--------------------------------------#
for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
  print_TOC("Parallel Region",$rank);
  if (0+$adv_parsing_content&0+1) { # single
    print_TOC("Single Section",$rank);
  }
  if (0+$adv_parsing_content&0+2) { #critical
    print_TOC("Critical Section",$rank);
  }
  if (0+$adv_parsing_content&0+4) { #critical barrier
    print_TOC("Critical Barrier",$rank);
  }
  if (0+$adv_parsing_content&0+8) { #plain barrier
    print_TOC("Plain Barrier",$rank);
  }
  if (0+$adv_parsing_content&0+16) { #fork barrier
    print_TOC("Fork Barrier",$rank);
  }
  if (0+$adv_parsing_content&0+32) { #join barrier
    print_TOC("Join Barrier",$rank);
  }
  if (0+$adv_parsing_content&0+64) { #task barrier
    print_TOC("Task",$rank);
  }
}
print "\n";
#--------------------------------------#
if ($print_csv && 0+$print_content&0+3) {
#0-th raw is for interval in case of csv
print "$separator";
} 
#--------------------------------------#

for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
  print_heading($rank);
  if (0+$adv_parsing_content&0+1) { # single
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+2) { #critical
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+4) { #critical barrier 
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+8) { #plain barrier 
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+16) { #fork barrier 
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+32) { #join barrier 
    print_heading($rank);
  }
  if (0+$adv_parsing_content&0+64) { #task
    print_heading($rank);
  }
}

print "\n";
#--------------------------------------#
if ($print_kst && 0+$print_content&0+3) {
#print units for kst 
for (my $i=0; $i < $end_index ; $i++) {
  print "$i $separator";
}
print "\n";
}
#--------------------------------------#

for (my $i=$start_index; $i < $end_index ; $i++) {
#--------------------------------------#
if ($print_csv && 0+$print_content&0+3) {
    print "Interval $i $separator";
}
#--------------------------------------#
    for (my $rank=0 ; $rank < $max_rank_id ; $rank++) {
######
      print_stat_for_event($i,$rank,\@microtask_activity,\@microtask_activity_s,\@microtask_start_addr);
########
      if (0+$adv_parsing_content&0+1) {
         print_stat_for_event($i,$rank,\@single_activity,\@single_activity_s,\@single_start_addr);
      }
      if (0+$adv_parsing_content&0+2) {
         print_stat_for_event($i,$rank,\@crit_sec_activity,\@crit_sec_activity_s,\@crit_sec_start_addr);
      }
      if (0+$adv_parsing_content&0+4) {
         print_stat_for_event($i,$rank,\@critical_barrier_activity,\@critical_barrier_activity_s,\@critical_barrier_start_addr);
      }
      if (0+$adv_parsing_content&0+8) {
         print_stat_for_event($i,$rank,\@plain_barrier_activity,\@plain_barrier_activity_s,\@plain_barrier_start_addr);
      }
      if (0+$adv_parsing_content&0+16) {
         print_stat_for_event($i,$rank,\@fork_barrier_activity,\@fork_barrier_activity_s,\@fork_barrier_start_addr);
      }
      if (0+$adv_parsing_content&0+32) {
         print_stat_for_event($i,$rank,\@join_barrier_activity,\@join_barrier_activity_s,\@join_barrier_start_addr);
      }
      if (0+$adv_parsing_content&0+64) {
         print_stat_for_event($i,$rank,\@task_activity,\@task_activity_s,\@task_start_addr);
      }
    }
  print "\n";
  }
if ($eventfile_s) {
close(EVENTFILE);
select(STDOUT);
}
}

}


####################################################################################
########################### main program starts here ###############################
my $duration;
my $temp_duration;
my %file_lines;
my @input_files;
my @input_files_thread_0;

parse_command_line;
#### get file names #####
if ($single_file==1) {
#push (@input_files, $input_mask);
push (@input_files_thread_0, $input_mask);
foreach my $input_file(@input_files_thread_0) {
chomp $input_file;
my $lines=`cat $input_file | wc -l`;
chomp($lines);
print STDERR "File $input_file, total lines:$lines\n";
$file_lines{$input_file}=$lines;
}
}else{
if ($pmf==1) {
  push (@input_files, `ls $input_mask*`);
  push (@input_files_thread_0, `ls $input_mask*.0`);
}else{
  push (@input_files, `ls $input_mask.*`);
  push (@input_files_thread_0, `ls $input_mask.0`);
}
}
$duration=0;

foreach my $input_file(@input_files) {
chomp $input_file;
my $lines=`cat $input_file | wc -l`;
chomp($lines);
print STDERR "File $input_file, total lines:$lines\n";
$file_lines{$input_file}=$lines;
}

foreach my $input_file(@input_files_thread_0) {
      chomp $input_file;
      my $lines=$file_lines{$input_file};
      my $success=parse_map_file($input_file);
      my $rank=parse_file($input_file,$lines,1);
      while ( my ($key, $value) = each(%handledTID) ) {
        $temp_duration=$rank_end_time[$rank]->[$value]-$rank_start_time[$rank]->[$value];
        print STDERR "Rank $rank, Thread=$value, Duration $temp_duration\n";
        my $relative_start_time=$rank_start_time[$rank]->[$value]-$rank_start_time[0]->[0];
        my $relative_end_time=$rank_end_time[$rank]->[$value]-$rank_end_time[0]->[0];
        my $temp_total_rank_time=$rank_end_tsc[$rank]->[$value]-$rank_start_tsc[$rank]->[$value];
        if ($total_rank_time[$rank]<$temp_total_rank_time) {$total_rank_time[$rank]=$temp_total_rank_time;}

        print STDERR "Relative start  $relative_start_time, Relative end $relative_end_time\n";
        if ($temp_duration>$duration) {
            $duration=$temp_duration;
        }
      }
}

foreach my $input_file(@input_files) {
#while ( my ($input_file, $lines) = each(%file_lines) ) {
      chomp $input_file;
      my $lines=$file_lines{$input_file};
      my $rank=parse_file($input_file,$lines,0);
      while ( my ($key, $value) = each(%handledTID) ) {
        $temp_duration=$rank_end_time[$rank]->[$value]-$rank_start_time[$rank]->[$value];
        print STDERR "Rank $rank, Thread=$value, Duration $temp_duration\n";
        my $relative_start_time=$rank_start_time[$rank]->[$value]-$rank_start_time[0]->[0];
        my $relative_end_time=$rank_end_time[$rank]->[$value]-$rank_end_time[0]->[0];
        my $temp_total_rank_time=$rank_end_tsc[$rank]->[$value]-$rank_start_tsc[$rank]->[$value];
        if ($total_rank_time[$rank]<$temp_total_rank_time) {$total_rank_time[$rank]=$temp_total_rank_time;}

        print STDERR "Relative start  $relative_start_time, Relative end $relative_end_time\n";
        if ($temp_duration>$duration) {
            $duration=$temp_duration;
        }
      }
}
if ($start_time < 0 ) {$start_time=$first_start_event;}
if ($stop_time < 0 ) {$stop_time=$duration-$last_stop_event;}
print_stat($duration);

