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
use lib("../../common");
use t_comm_func;

####################################################################################

sub make_dup_call {
my $funcname=shift;
my $funcpars=shift;
$funcpars=t_comm_func::remove_typenames($funcpars);
if ($funcpars eq 'void' || $funcpars eq '') {
  return "${funcname}_internal(return_addr)";
}else{
  return "${funcname}_internal($funcpars,return_addr)";
}
}

my $print_extended_stuff=0;
my $use_pragma_weak=0;
my $parse_mpi_internal=0;
my $mpifuncs;
my $mpilayer;
my $mpilevel=1;
my $funccount=0;
my $parse_gpu=0;
my $skip_core=0;
sub make_dup;
my @fab_layers; 
my $is_shim_layer=0; # set by parse_file_fab: standalone shim file being generated, vs. legacy core-embedded append
my $fab_reentrant_declared=0; # set once the t_fab_reentrant guard has been printed into the current output file

#my $prolog="
#$line[4]=~s/ //g;
#$line[5]=~s/ //g;
#$line[6]=~s/ //g;
#$line[7]=~s/ //g;
#$line[8]=~s/ //g;
#";

sub make_class_request {
my $line=shift;
my @line=@$line;

my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $declare_input_request;
my $declare_ignore_status;
my $if_ignore_status;
my $declare_l_flag="int l_flag=0;" if $properties =~/FLAG/;
my $event_flag="event->requests.flag=1;"; # for wait there no flag
$event_flag="event->requests.flag=l_flag;" if $properties =~/FLAG/;
my $event_request;
my $event_status;
my $record_input_request;
my $event_record_input_request;
my $event_rbuf;
my $event_sbuf;
my $event_record_input_request_addr;
my $mpi_internal="mpi_internal_" if $properties =~/MPI_INTERNAL/; # MPI-internal helper variants (T_Type_size_mpi_internal, mpi_internal_get_rank_from_req, stack_unwind_mpi_internal)
my $global_mpi_time;

$global_mpi_time="global_mpi_time+=tsc_mpi_exit-tsc_mpi_entry;" if !($properties=~/MPI_INTERNAL/);
#if ($properties=~/INPUT_REQUEST_POINTER_MULTIPLE/) { 
#   $record_input_request="__msl_record_requests(tid,*requests,inum);";
#   $record_input_request="__msl_record_drequests(tid,*requests,inum);" if $properties =~/MPI_INTERNAL/;
#   $event_record_input_request="event->requests.input_request_num=inum;";
#   $event_rbuf="event->requests.requests.virt_addr=*requests;event->requests.requests.size=inum*sizeof(MPI_Request);" if $print_extended_stuff;
#   $event_record_input_request_addr="event->requests.request_addr=(PEAT_Addr)*requests;";
#}
my $declare_local_comms;
my $save_local_comms;
my $save_input_request;
my $reused="if (event==NULL) reused=__msl_reuse_event(tid,level,&event,&counters,$funcid_through); if (!reused) " if $properties =~/COMPRESSIBLE/ ;
my $skip_parse_update;
if ($properties =~/FLAG/) {
   my $add_l_flag="l_flag=((*flag)>0) && (!is_empty(status));" if $properties =~/STATUS/;
   $add_l_flag="l_flag=((*flag)>0) && (!is_all_empty(inum,statuses));" if $properties =~/MSTATUSES/;
   $skip_parse_update="l_flag=*flag;";
   $skip_parse_update="$add_l_flag l_skip_parse=(l_flag==0) && ((gt_info(compress_events) & 2)!=0); if (l_skip_parse) l_skip_parse=__msl_discard_event(tid, level, &event, children_base);" if $properties =~/COMPRESSIBLE/ ;
} else {
   $skip_parse_update="l_skip_parse=(*onum!=MPI_UNDEFINED)  && ((gt_info(compress_events) & 2)!=0); if (l_skip_parse) l_skip_parse=__msl_discard_event(tid, level, &event, children_base);" if $properties =~/COMPRESSIBLE/ ;
}

if ($properties=~/INPUT_REQUEST_MULTIPLE/) { 
   $declare_input_request="MPI_Request t_req[inum];";
   $save_input_request="__msl_dup_requests(t_req, requests, inum);"; 
   $save_input_request="
   #ifndef _OPENMPI_
       __msl_cvt_drequests(t_req,(const MPID_Request **) requests, inum);
   #else
       mpi_internal_get_requests(inum, requests, t_req); // maps OpenMPI's shared inline-completion sentinel to MPI_REQUEST_NULL per entry
   #endif
   " if $properties =~/MPI_INTERNAL/;

   $save_local_comms="mpi_internal_get_comms(inum,requests,local_comms);" if $properties =~/MPI_INTERNAL/;
   $record_input_request="__msl_record_requests(tid,level,t_req,inum);";

   $event_record_input_request="event->requests.input_request_num=inum;";
   $event_rbuf="event->requests.requests.virt_addr=requests;event->requests.requests.size=inum*sizeof(MPI_Request);" if $print_extended_stuff;
   $event_record_input_request_addr="event->requests.request_addr=(PEAT_Addr)requests;";
   $declare_local_comms="
   #ifndef _OPENMPI_ 
       MPID_Comm *local_comms[inum];
   #else
       MPI_Comm local_comms[inum];
   #endif
   " if $properties =~/MPI_INTERNAL/;
}

if ($properties=~/INPUT_REQUEST_SINGLE/) {
   $declare_input_request="MPI_Request t_req;";
   $declare_local_comms="
   #ifndef _OPENMPI_ 
       MPID_Comm *local_comm;
   #else
       MPI_Comm local_comm;
   #endif" if $properties =~/MPI_INTERNAL/;
   $save_input_request="t_req=*request;";
   $save_input_request="
   #ifndef _OPENMPI_
       t_req=__msl_get_drequest((const MPID_Request *) request);
   #else
       t_req=mpi_internal_get_request(*request); // maps OpenMPI's shared inline-completion sentinel to MPI_REQUEST_NULL
   #endif
   " if $properties =~/MPI_INTERNAL/;
   $save_input_request="
   #ifndef _OPENMPI_
       t_req=request;
   #else
       t_req=mpi_internal_get_request(request); // maps OpenMPI's shared inline-completion sentinel to MPI_REQUEST_NULL
   #endif
   " if $properties =~/PEAT_PROP_REQUEST_DIRECT/;

   $save_local_comms="
   #ifndef _OPENMPI_ 
       local_comm=mpi_internal_get_comm(request);
   #else
       local_comm=mpi_internal_get_comm(*request);
   #endif
   " if $properties =~/MPI_INTERNAL/;
   $save_local_comms="local_comm=mpi_internal_get_comm(request);" if $properties =~/MPI_INTERNAL/ && $properties =~/PEAT_PROP_REQUEST_DIRECT/;

   $record_input_request="__msl_record_request(tid,level,t_req);";

   $event_record_input_request="event->requests.input_request_num=1;";
   $event_rbuf="event->requests.requests.virt_addr=request;event->requests.requests.size=sizeof(MPI_Request);" if $print_extended_stuff;
   $event_record_input_request_addr="event->requests.request_addr=(PEAT_Addr)request;";
   if ($properties =~/PEAT_PROP_REQUEST_DIRECT/) { # MPI_Request by value (MPI_Pready/MPI_Parrived): no address to record, keep the handle value
       $event_rbuf="event->requests.requests.virt_addr=(void *)(intptr_t)request;event->requests.requests.size=sizeof(MPI_Request);" if $print_extended_stuff;
       $event_record_input_request_addr="event->requests.request_addr=(PEAT_Addr)(intptr_t)request;";
   }
}

if ($properties=~/MSTATUSES/) {
   $event_sbuf="event->requests.statuses.virt_addr=status;event->requests.statuses.size=inum*sizeof(MPI_Status);" if $print_extended_stuff;
   if ($properties=~/INDICES/) {   # wait/test SOME
    $event_status="
    if (*onum!=MPI_UNDEFINED) {
        event->requests.flag=1;
        event->requests.triggered_request_num=*onum;
        // __msl_record_ints(tid,level,indx,(*onum));
        long long TSC[inum*4];
        for (i=0;i<*onum;i++) { 
            TSC[i*4+0]=indx[i];
            TSC[i*4+1]=statuses[indx[i]].MPI_TAG;
            TSC[i*4+2]=${mpi_internal}get_rank_from_comm(local_comms[indx[i]],statuses[indx[i]].MPI_SOURCE); ;
            TSC[i*4+3]=MPI_STATUS_GET_COUNT_R(statuses[indx[i]]);
        }
        __msl_record_longs(tid,level,TSC,(*onum)*4);
    }";
   } else { # wait/test ALL
    $event_status="
    if (event->requests.flag) { 
        event->requests.triggered_request_num=inum;
        long long TSC[inum*4];
        for (i=0;i<inum;i++) { 
            TSC[i*4+0]=i;
            TSC[i*4+1]=statuses[i].MPI_TAG;
            TSC[i*4+2]=${mpi_internal}get_rank_from_comm(local_comms[i],statuses[i].MPI_SOURCE);
            TSC[i*4+3]=MPI_STATUS_GET_COUNT_R(statuses[i]);
        }
        __msl_record_longs(tid,level,TSC,inum*4);
    }";
   };
    $if_ignore_status="if (statuses==MPI_STATUSES_IGNORE) statuses=local_statuses;";
    $declare_ignore_status="MPI_Status local_statuses[inum];"
} else {
    if ($properties=~/STATUS/ ) { # wait test, ANY
        my $lcomm=($properties=~/INPUT_REQUEST_SINGLE/) ? "local_comm" : "local_comms[temp_index]";
        $event_sbuf="event->requests.statuses.virt_addr=status;event->requests.statuses.size=sizeof(MPI_Status);" if $print_extended_stuff;
        $event_status="
        if (event->requests.flag) { 
            event->requests.triggered_request_num=1;
            long long TSC[4];
            TSC[0]=temp_index;
            TSC[1]=status->MPI_TAG;
            TSC[2]=${mpi_internal}get_rank_from_comm($lcomm,status->MPI_SOURCE);
            TSC[3]=MPI_STATUS_GET_COUNT_P(status);
           __msl_record_longs(tid,level,TSC,4);
        }";
        $event_status="
        temp_index=*indx;
        $event_status 
        " if ($properties=~/INDEX/); #append index into extra info

        $if_ignore_status="if (status==MPI_STATUS_IGNORE) status=&local_status;";
        $declare_ignore_status="MPI_Status local_status;"
   } else {
        $event_status="
        {
            event->requests.triggered_request_num=1;
            long long TSC[4];
            MPI_Status status;int flag=0;
            PMPI_Request_get_status(t_req,&flag,&status);
            if (flag) {
                TSC[0]=0;
                TSC[1]=status.MPI_TAG;
                TSC[2]=${mpi_internal}get_rank_from_comm(local_comm,status.MPI_SOURCE); ;
                TSC[3]=MPI_STATUS_GET_COUNT_R(status);
            } else {
                TSC[0]=0;
                TSC[1]=-1;
                TSC[2]=-2;
                TSC[3]=-1;
            }
           __msl_record_longs(tid,level,TSC,4);
       }
       " if $properties =~/MPI_INTERNAL/ && $properties =~/INPUT_REQUEST_SINGLE/ && !($properties =~/PERSISTENT_REQUEST_START/); #only apply to single-request MPIC_XXX / ompi_coll_p2p completion routines. Exclude MPI_STARTXXX.
   }

}
my $print_code=`cat pinc/request.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}

sub make_class_p2p {
my ($line,$basic_struct)=@_;
my @line=@$line;
$basic_struct="p2p_pr_pb" if $basic_struct eq "";
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $datasize;
my $event_buf;
my $internal_comm_tracking;

my $mpi_internal="mpi_internal_" if $properties =~/MPI_INTERNAL/; # MPI-internal helper variants
my $reused="reused=__msl_reuse_event(tid,level,&event,&counters,$funcid_through); if (!reused) " if $properties =~/PEAT_PROP_COMPRESSIBLE/ ;
my $skip_parse_update="l_skip_parse=(*flag==0) && ((gt_info(compress_events) & 2)!=0); if (l_skip_parse) l_skip_parse=__msl_discard_event(tid, level, &event, children_base);" if $properties =~/PEAT_PROP_COMPRESSIBLE/ ;
my $global_mpi_time;
$global_mpi_time="global_mpi_time+=tsc_mpi_exit-tsc_mpi_entry;" if !($properties=~/MPI_INTERNAL/);

if ($properties =~/DATA_TXRX/) {
my $comm="comm";
#$comm="MPI_COMM_WORLD" if $properties =~/MPI_INTERNAL/;
#$comm="(long long) comm" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/ ;
#$comm="mpi_internal_get_top_comm_handle_from_comm(comm)" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;
$comm="mpi_internal_get_comm_handle_from_comm(comm)" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;
#$comm="last_coll_comm" if $properties =~/MPI_INTERNAL/;
my $stag="stag";
my $dest="dest";

$dest="mpi_internal_get_rank_from_comm(comm,dest)" if $properties =~/MPI_INTERNAL/;

my $sbuf_bottom;
my $rbuf_bottom;
if ($funcid=~/REPLACE/) {
    $datasize="${mpi_internal}T_Type_size(type,&s_data_size);r_data_size=s_data_size;s_count=r_count=count;";
    $event_buf="
        event[1]->p2p_pr_pb.buf.virt_addr=buf;
        event[1]->p2p_pr_pb.buf.size=event[1]->p2p_pr_pb.transfer_bytes;
        event[0]->p2p_pr_pb.buf.virt_addr=buf;
        event[0]->p2p_pr_pb.buf.size=event[0]->p2p_pr_pb.transfer_bytes;
    " if $print_extended_stuff;

} else {
    $datasize="${mpi_internal}T_Type_size(stype,&s_data_size);${mpi_internal}T_Type_size(rtype,&r_data_size);s_count=scount;r_count=rcount;";
    $event_buf="
        event[1]->p2p_pr_pb.buf.virt_addr=rbuf;
        event[1]->p2p_pr_pb.buf.size=event[1]->p2p_pr_pb.transfer_bytes;
        event[0]->p2p_pr_pb.buf.virt_addr=sbuf;
        event[0]->p2p_pr_pb.buf.size=event[0]->p2p_pr_pb.transfer_bytes;
    " if $print_extended_stuff;
}
my $status_peer="src >=0 ? src : status->MPI_SOURCE";

$status_peer="mpi_internal_get_rank_from_comm(comm,$status_peer)" if $properties =~ /MPI_INTERNAL/;
# $internal_comm_tracking="track_internal_comm(event[0]->$basic_struct.comm,tsc_mpi_exit);" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;

# blocking: receive side is described by the returned status
my $declare_status="MPI_Status local_status;";
my $fix_status="if (status==MPI_STATUS_IGNORE) status=&local_status;";
my $recv_tag="status->MPI_TAG";
my $recv_peer=$status_peer;
my $recv_bytes="MPI_STATUS_GET_COUNT_P(status)";
my $event_request="";
if ($properties =~/NON_BLOCK/) { # MPI_Isendrecv*: no status yet, record the posted receive and the request
    $declare_status="";
    $fix_status="";
    $recv_tag="rtag";
    $recv_peer="src";
    $recv_peer="mpi_internal_get_rank_from_comm(comm,src)" if $properties =~ /MPI_INTERNAL/;
    $recv_bytes="(r_count*r_data_size)";
    $event_request="event[1]->p2p_pr_pb.request=*request;";
}

my $print_code=`cat pinc/sendrecv.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
} else {
my $event_request;
my $event_status;
my $if_ignore_status;
my $declare_ignore_status;
my $tag_peer;
my $pre_event_message;
my $post_event_message;
my $comm;
my $transfer_bytes;
my $get_datasize;
my $event_flag;
my $pre_handle;
my $post_handle;
my $offset;
my $buf_bottom;
my $rbuf_bottom;
my $cbuf_bottom;
my $event_rbuf;
my $event_cbuf;
my $event_status_begin="{";
my $event_status_end="}";

$event_request="event->$basic_struct.request=*request;" if $properties=~/REQUEST/;
$event_request="event->$basic_struct.request=mpi_internal_get_request(*request);" if $properties=~/REQUEST/ && $properties=~/MPI_INTERNAL/; #casting from MPID_Request*
$tag_peer="event->$basic_struct.tag=tag;" if $properties=~/TAG/;
$tag_peer="$tag_peer event->$basic_struct.peer=peer_rank;" if $properties=~/PEER/;
#$internal_comm_tracking="track_internal_comm(event->$basic_struct.comm,tsc_mpi_exit);" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;

# MPIC_Probe is the one mpi_internal helper that takes the MPI_Comm handle (not MPIR_Comm*), so probe-class mpi_internal
# functions use comm and ranks as they are, without the MPID->MPI comm / rank translation.
my $mpi_internal_mpid_comm = ($properties=~/MPI_INTERNAL/ && $properties!~/CLASS_PROBE/);
$tag_peer="$tag_peer event->$basic_struct.peer=mpi_internal_get_rank_from_comm(comm,peer_rank);" if $properties=~/PEER/ && $mpi_internal_mpid_comm;

$pre_handle="event->$basic_struct.window=win;" if $properties=~/HWINDOW/ && $properties=~/CLASS_OSC/;
$pre_handle="event->$basic_struct.window=*win;" if $properties=~/PHWINDOW/ && $properties=~/CLASS_OSC/;
$pre_handle="event->$basic_struct.fh=fh;" if $properties=~/FILE_HANDLE/ && $properties=~/CLASS_FIO/;
$pre_handle="event->$basic_struct.fh=*fh;" if $properties=~/FILE_POINTER_INPUT/ && $properties=~/CLASS_FIO/;
$post_handle="event->$basic_struct.fh=*fh;" if $properties=~/FILE_POINTER_OUTPUT/ && $properties=~/CLASS_FIO/;

$offset="event->$basic_struct.offset=offset;" if $properties=~/OFFSET/ && $properties=~/CLASS_FIO/;
$pre_event_message="event->$basic_struct.message=*message;" if $properties=~/MESSAGE/ && $properties=~/CLASS_P2P/;
$post_event_message="event->$basic_struct.message=*message;" if $properties=~/MESSAGE/ && $properties=~/CLASS_PROBE/;

$comm="event->$basic_struct.comm=comm;" if $properties=~/COMM/;
#$comm="event->$basic_struct.comm=(long long) comm;" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/ ;
#$comm="event->$basic_struct.comm=MPI_COMM_WORLD;" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;

#$comm="event->$basic_struct.comm=mpi_internal_get_top_comm_handle_from_comm(comm);" if $properties=~/COMM/ && $mpi_internal_mpid_comm;
$comm="event->$basic_struct.comm=mpi_internal_get_comm_handle_from_comm(comm);" if $properties=~/COMM/ && $mpi_internal_mpid_comm;

#$comm="event->$basic_struct.comm=last_coll_comm;" if $properties=~/COMM/ && $properties=~/MPI_INTERNAL/;
$get_datasize="${mpi_internal}T_Type_size(type,&data_size);"if $properties=~/BYTES/;
$transfer_bytes="event->$basic_struct.transfer_bytes=(count*data_size);"if $properties=~/BYTES/;
$transfer_bytes="event->$basic_struct.transfer_bytes=((long long)partitions*(long long)count*data_size);" if $properties=~/BYTES/ && $properties=~/PARTITIONS/; 
$transfer_bytes="event->$basic_struct.transfer_bytes=size;"if $properties=~/SIZE/;
$event_buf="
        event->$basic_struct.buf.virt_addr=buf;
        event->$basic_struct.buf.size=event->$basic_struct.transfer_bytes;
" if $print_extended_stuff && ($properties=~/DATA_TX/ || $properties=~/DATA_RX/|| $properties=~/DATA_TXRX/ || $properties=~/BUF/  );

$event_rbuf="
        event->$basic_struct.rbuf.virt_addr=rbuf;
        event->$basic_struct.rbuf.size=event->$basic_struct.transfer_bytes;
" if $print_extended_stuff && $properties=~/RBUF/;

$event_cbuf="
        event->$basic_struct.cbuf.virt_addr=cbuf;
        event->$basic_struct.cbuf.size=event->$basic_struct.transfer_bytes;
" if $print_extended_stuff && $properties=~/CBUF/;

if ($properties=~/STATUS/) {
    $event_status_begin="if (*flag) {" if $properties=~/FLAG/;
    $event_status="event->$basic_struct.transfer_bytes=MPI_STATUS_GET_COUNT_P(status);";
    $event_status="$event_status event->$basic_struct.tag=status->MPI_TAG;" if $properties=~/TAG/;
    if ($mpi_internal_mpid_comm) {
        $event_status="$event_status event->$basic_struct.peer=mpi_internal_get_rank_from_comm(comm,peer_rank>=0 ? peer_rank : status->MPI_SOURCE);" if $properties=~/PEER/;
    } else  {
        $event_status="$event_status event->$basic_struct.peer=peer_rank>=0 ? peer_rank : status->MPI_SOURCE;" if $properties=~/PEER/;
    }

    $event_status_end="} // if (*flag)" if $properties=~/FLAG/; 

#    $event_status_end="$event_status_end
#    if (t_mpi_rank==1) {
#    if (event->$basic_struct.peer<0 ) {
#        fprintf(stderr,\"Incorrect rank translation: %d->%d on comm %lx\\n\",peer_rank, event->$basic_struct.peer,comm);
#        event->$basic_struct.peer=mpi_internal_get_rank_from_comm_v(comm,peer_rank>=0 ? peer_rank : status->MPI_SOURCE);
#        mpi_internal_print_comm_tree(comm,0);
#        //MPI_Abort(MPI_COMM_WORLD,0);
#    } else {
#        fprintf(stderr,\"Correct rank translation: %d->%d on comm %lx\\n\",peer_rank, event->$basic_struct.peer,comm);
#        event->$basic_struct.peer=mpi_internal_get_rank_from_comm_v(comm,peer_rank>=0 ? peer_rank : status->MPI_SOURCE);
#        mpi_internal_print_comm_tree(comm,0);
#    }
#    }
#    " if $funcid=~/MPIC_RECV_ID/;# ||  $funcid=~/MPIC_SEND_ID/; 

    $if_ignore_status="if (status==MPI_STATUS_IGNORE) status=&local_status;";
    $declare_ignore_status="MPI_Status local_status;";
    undef $tag_peer; undef $transfer_bytes;
}
$event_flag="event->$basic_struct.flag=*flag;" if ($properties=~/FLAG/);

my $print_code=`cat pinc/rt.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}
}

sub make_class_buf_local {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $count="event->buf_local.count=count*data_size;";
my $type="T_Type_size(type,&data_size);";
my $size="event->buf_local.size=size;";
my $sbuf_bottom;
my $rbuf_bottom;
my $event_sbuf="event->buf_local.buf[0].virt_addr=buf;event->buf_local.buf[0].size=event->buf_local.count;" if $print_extended_stuff;
my $event_rbuf="event->buf_local.buf[1].virt_addr=outbuf;event->buf_local.buf[1].size=event->buf_local.size;" if $print_extended_stuff;
my $print_code=`cat pinc/buf_local.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}

sub make_class_local {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $print_code=`cat pinc/local.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}

sub make_class_collective {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $root="event->collective.root=-1;";
my $event_request;
my $s_count;
my $r_count;
my $comm_size;
my $comm="comm";
#my $comm="event->collective.comm=comm;";
my $amiroot;
$amiroot="PMPI_Comm_rank(comm,&amiroot);amiroot=(amiroot==root);" if ($properties=~/ROOT/);
$root="event->collective.root=root;" if ($properties=~/ROOT/) ;
$event_request="event->collective.request=*request;" if $properties=~/REQUEST/;
#defaults for barrier
my $s_type_size;
my $r_type_size;
my $buf_in_place;
my $sbuf_bottom;
my $rbuf_bottom;
my $event_sbuf;
my $event_rbuf;

if ($properties=~/DATA_COLLECTIVE/) {
if ($properties=~/TYPEMATCH/) {
    $s_type_size="T_Type_size(type,&s_type_size);r_type_size=s_type_size;" if $properties=~/SCALAR_TYPE/;
#    $s_type_size="event->collective.sr_type_sizes[0]=comm_size; { int type_sizes[comm_size]; for (i=0;i<comm_size;i++) T_Type_size(types[i],&type_sizes[i]); __msl_record_ints(tid,level,s_type_sizes,comm_size);}" if $properties=~/VECTOR_TYPE/; # shouldn't happen
    undef $r_type_size;
} else {
    $s_type_size="T_Type_size(stype,&s_type_size);" if $properties=~/SCALAR_TYPE/;
    $r_type_size="T_Type_size(rtype,&r_type_size);" if $properties=~/SCALAR_TYPE/;
    #$s_type_size="event->collective.sr_type_sizes[0]=comm_size; { int type_sizes[comm_size]; for (i=0;i<comm_size;i++) T_Type_size(stypes[i],&type_sizes[i]); __msl_record_ints(tid,level,type_sizes,comm_size);}" if $properties=~/VECTOR_TYPE/;
    #$r_type_size="event->collective.sr_type_sizes[1]=comm_size; { int type_sizes[comm_size]; for (i=0;i<comm_size;i++) T_Type_size(rtypes[i],&type_sizes[i]); __msl_record_ints(tid,level,type_sizes,comm_size);}" if $properties=~/VECTOR_TYPE/;
}
if ($properties=~/VECTOR/) {
    # entries of the per-peer arrays: comm size, remote size on an inter-communicator, or the
    # topology's out/in degree for neighbourhood collectives (see determine_comm_size in t_mpidef.h)
    my $size_kind="T_COMM_SIZE_PEERS";
    $size_kind="T_COMM_SIZE_NEIGHBORS" if $line[2]=~/Neighbor_/i;
    $size_kind="T_COMM_SIZE_LOCAL" if $properties=~/COUNTMATCH/; # MPI_Reduce_scatter: recvcounts covers the local group
    $comm_size="determine_comm_size(comm, $size_kind, &comm_size, &comm_rsize);";

    if ($properties=~/COUNTMATCH/) {
        $s_count="event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)count*(long long)s_type_size));" if $properties=~/SCALAR_SEND/;
        $s_count="event->collective.sr_bytes[0]=comm_size;
        { 
            long long s_bytes[comm_size]; 
            for (i=0;i<comm_size;i++) { 
                s_bytes[i]=(long long)counts[i]*(long long)s_type_size; 
#ifdef _TRACTOR_EXTRA_FIELDS_
                ts_bytes+=s_bytes[i]; 
#endif
            }
            __msl_record_longs(tid,level,s_bytes,comm_size);
        }" if $properties=~/VECTOR_SEND/;
        undef $r_count;
    } else {
        if ($properties=~/ROOT_SEND/) {
	        $s_count="if (amiroot) { event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)scount*(long long)s_type_size));} else event->collective.sr_bytes[0]=0;" if $properties=~/SCALAR_SEND/;
	} else {
	        $s_count="event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)scount*(long long)s_type_size));" if $properties=~/SCALAR_SEND/;
	}
        if ($properties=~/ROOT_RECV/) {
	        $r_count="if (amiroot) { event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)rcount*(long long)r_type_size));} else event->collective.sr_bytes[1]=0;" if $properties=~/SCALAR_RECV/;
	} else {
	        $r_count="event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)rcount*(long long)r_type_size));" if $properties=~/SCALAR_RECV/;
	}
        my $s_type_size_v;
        my $r_type_size_v;
        $s_type_size_v="T_Type_size(stypes[i],&s_type_size);" if $properties=~/VECTOR_TYPE/;
        $r_type_size_v="T_Type_size(rtypes[i],&r_type_size);" if $properties=~/VECTOR_TYPE/;
        if ($properties=~/ROOT_SEND/) {
        $s_count="
        if (amiroot) {
            event->collective.sr_bytes[0]=comm_size;
            { 
                long long s_bytes[comm_size]; 
                for (i=0;i<comm_size;i++) {
                    $s_type_size_v s_bytes[i]=(long long)scounts[i]*(long long)s_type_size;
#ifdef _TRACTOR_EXTRA_FIELDS_
                    ts_bytes+=s_bytes[i]; 
#endif
                } 
                __msl_record_longs(tid,level,s_bytes,comm_size);
            }
        } else event->collective.sr_bytes[0]=0;
        " if $properties=~/VECTOR_SEND/;
        } else {
        $s_count="
        event->collective.sr_bytes[0]=comm_size;
        { 
            long long s_bytes[comm_size]; 
            for (i=0;i<comm_size;i++) {
                $s_type_size_v s_bytes[i]=(long long)scounts[i]*(long long)s_type_size;
#ifdef _TRACTOR_EXTRA_FIELDS_
                ts_bytes+=s_bytes[i]; 
#endif
            } 
            __msl_record_longs(tid,level,s_bytes,comm_size);
        }" if $properties=~/VECTOR_SEND/;

        }
        if ($properties=~/ROOT_RECV/) {
        $r_count="
        if (amiroot) {
            event->collective.sr_bytes[1]=comm_rsize;
            { 
                long long r_bytes[comm_rsize]; 
                for (i=0;i<comm_rsize;i++) {
                    $r_type_size_v r_bytes[i]=(long long)rcounts[i]*(long long)r_type_size;
#ifdef _TRACTOR_EXTRA_FIELDS_
                    tr_bytes+=r_bytes[i]; 
#endif
                } 
                __msl_record_longs(tid,level,r_bytes,comm_rsize);
            }
        } else event->collective.sr_bytes[1]=0;" if $properties=~/VECTOR_RECV/;
        } else {
        $r_count="
        event->collective.sr_bytes[1]=comm_rsize;
        { 
            long long r_bytes[comm_rsize]; 
            for (i=0;i<comm_rsize;i++) {
                $r_type_size_v r_bytes[i]=(long long)rcounts[i]*(long long)r_type_size;
#ifdef _TRACTOR_EXTRA_FIELDS_
                tr_bytes+=r_bytes[i]; 
#endif
            } 
            __msl_record_longs(tid,level,r_bytes,comm_rsize);
        }" if $properties=~/VECTOR_RECV/;
        }
    }

} else {
    if ($properties=~/COUNTMATCH/) {
        if ($properties=~/SCALAR_SEND/ && $properties=~/SCALAR_RECV/) { 
            $s_count="event->collective.sr_bytes[0]=1; __msl_record_long(tid,level,((long long)count*(long long)s_type_size));";
            $r_count="event->collective.sr_bytes[1]=1; __msl_record_long(tid,level,((long long)count*(long long)s_type_size));";
        } 
        if ($properties=~/ROOT_SEND/) {
           $s_count="if (amiroot) { event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)count*(long long)s_type_size));} else event->collective.sr_bytes[0]=0;" ;
           $r_count="if (!amiroot) { event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)count*(long long)r_type_size));} else event->collective.sr_bytes[1]=0;" if $properties=~/SCALAR_SEND/;
        }
        if ($properties=~/ROOT_RECV/) {
           $s_count="if (!amiroot) { event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)count*(long long)s_type_size));} else event->collective.sr_bytes[0]=0;" if $properties=~/SCALAR_SEND/;
           $r_count="if (amiroot) { event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)count*(long long)r_type_size));} else event->collective.sr_bytes[1]=0;" if $properties=~/SCALAR_RECV/;
        }
    } else {
        if ($properties=~/ROOT_SEND/) {
            $s_count="if (amiroot) { event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)scount*(long long)s_type_size));} else event->collective.sr_bytes[0]=0;" if $properties=~/SCALAR_SEND/;
        } else {
            $s_count="event->collective.sr_bytes[0]=1;__msl_record_long(tid,level,((long long)scount*(long long)s_type_size));" if $properties=~/SCALAR_SEND/;
        }
        if ($properties=~/ROOT_RECV/) {
            $r_count="if (amiroot) { event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)rcount*(long long)r_type_size));} else event->collective.sr_bytes[1]=0;" if $properties=~/SCALAR_RECV/;
        } else {
            $r_count="event->collective.sr_bytes[1]=1;__msl_record_long(tid,level,((long long)rcount*(long long)r_type_size));" if $properties=~/SCALAR_RECV/;
       }
    }
}
if ($properties=~/SAMEBUF/) {
    $event_sbuf="event->collective.buf[0].virt_addr=buf;event->collective.buf[0].size=((long long)count*(long long)s_type_size);" if $print_extended_stuff;
    $event_rbuf="event->collective.buf[1].virt_addr=buf;event->collective.buf[1].size=((long long)count*(long long)s_type_size);" if $print_extended_stuff;
} else {
    if ($properties=~/COUNTMATCH/) {
        $event_sbuf="event->collective.buf[0].virt_addr=sbuf;event->collective.buf[0].size=((long long)count*(long long)s_type_size);" if $print_extended_stuff && $properties=~/SCALAR_SEND/;
        $event_rbuf="event->collective.buf[1].virt_addr=rbuf;event->collective.buf[1].size=((long long)count*(long long)r_type_size);" if $print_extended_stuff && $properties=~/SCALAR_RECV/;
    } else {
        $event_sbuf="event->collective.buf[0].virt_addr=sbuf;event->collective.buf[0].size=((long long)scount*(long long)s_type_size);" if $print_extended_stuff && $properties=~/SCALAR_SEND/;
        $event_rbuf="event->collective.buf[1].virt_addr=rbuf;event->collective.buf[1].size=((long long)rcount*(long long)r_type_size);" if $print_extended_stuff && $properties=~/SCALAR_RECV/;
    }
    $event_sbuf="event->collective.buf[0].virt_addr=sbuf;event->collective.buf[0].size=ts_bytes;" if $print_extended_stuff && $properties=~/VECTOR_SEND/;
    $event_rbuf="event->collective.buf[1].virt_addr=rbuf;event->collective.buf[1].size=tr_bytes;" if $print_extended_stuff && $properties=~/VECTOR_RECV/;
}
}
my $print_code=`cat pinc/collective.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}

sub make_class_osc {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $global_mpi_time;
$global_mpi_time="global_mpi_time+=tsc_mpi_exit-tsc_mpi_entry;" if !($properties=~/MPI_INTERNAL/);

if ($properties=~/CREATE_ALLOC/) {
my $size;
$size="event->one_sided_comm.size=size;" if $properties=~/SIZE/;
my $comm="comm";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $basic_struct="one_sided_comm";
my $event_handle="event->one_sided_comm.window=*win;";
my $event_buf="event->one_sided_comm.buf.virt_addr=buf;event->one_sided_comm.buf.size=size;" if $print_extended_stuff && !($properties=~/IMP_ADDRESS/);
my $event_buf="MPI_Aint addr;PMPI_Get_address(MPI_BOTTOM,&addr);event->one_sided_comm.buf.virt_addr=(void *)addr;event->one_sided_comm.buf.size=0;" if $print_extended_stuff && ($properties=~/IMP_ADDRESS/);
my $print_code=`cat pinc/create_alloc.code.inc`;

eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
} else {
make_class_p2p($line,"one_sided_comm");
}
}

sub make_class_fio {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $properties=$line[6];
my $c_prototype="$retcode $funcname($funcpars)";
$funcpars=t_comm_func::remove_typenames($funcpars);
my $c_call="$funcname($funcpars)";
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
if ($properties=~/CREATE_ALLOC/) {
my $size;
my $comm="comm";
#$size="event->one_sided_comm.size=size;" if $properties=~/SIZE/;
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $basic_struct="mpi_fio";
my $event_handle="event->mpi_fio.fh=*fh;";
my $event_buf;

my $print_code=`cat pinc/create_alloc.code.inc`;
eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
} else {
make_class_p2p($line,"mpi_fio");
}
}

sub make_class_fab {
my $line=shift;
my $fab_layer=shift;
my @line=@$line;
my $funcretcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $c_prototype="$funcretcode $funcname($funcpars)";
# all layers are compiled into one library now; only the layer matching the
# runtime fab_layer (T_FAB_LAYER env var) actually records fab events
my %fab_layer_enum_of = (
    "psm2"  => "T_FAB_LAYER_PSM2",
    "psm3"  => "T_FAB_LAYER_PSM3",
    "verbs" => "T_FAB_LAYER_VERBS",
    "smpi"  => "T_FAB_LAYER_VERBS",
    "ucx"   => "T_FAB_LAYER_UCX",
    "ofi"   => "T_FAB_LAYER_OFI",
);
my $fab_layer_enum = $fab_layer_enum_of{$fab_layer};
# only meaningful for the legacy core-embedded path (verbs/smpi mixed in with everything
# else in one binary); a standalone shim only ever contains one layer's functions and is
# only dlopen()'d once fab_layer already matches it, so the check would always be true
# there anyway - and it needlessly requires T_FAB_LAYER_*/fab_layer to be visible/defined.
# my $fab_layer_check = ($fab_layer_enum && !$is_shim_layer) ? " || (gt_info(fab_layer)!=$fab_layer_enum)" : "";
#my $t_prototype;
my $properties=$line[6];

#my $funcretcode=t_comm_func::remove_typenames_retcode($retcode);
my $t_func_pointer;
my $rtdl; 
my $funcname_t="t_$funcname";
#psm2_error_t(*t_psm_profiler::t_psm2_mq_send)(psm2_mq_t, psm2_epaddr_t, uint32_t, uint64_t,const void *, uint32_t) = 0;
#psm2_error_t(*t_psm_profiler::t_psm2_mq_send2)(psm2_mq_t, psm2_epaddr_t, uint32_t,psm2_mq_tag_t *, const void *, uint32_t ) = 0;
#print STDERR "Writing Headers $funcname on layer $fab_layer\n";

#print STDERR "----$c_prototype, $properties----\n";
#die;
$t_func_pointer="$funcretcode (*t_$funcname)($funcpars) = 0;"  if ($properties=~/PSM2|PSM3|UCX|OFI/);
$t_func_pointer="$funcretcode $funcname_t($funcpars);" if ($properties=~/T_PREFIX/); 
$rtdl="if(!$funcname_t) { $funcname_t = reinterpret_cast<$funcretcode (*)($funcpars)>(dlsym(RTLD_NEXT, \"$funcname\")); }\n" if ($properties=~/PSM2|PSM3|UCX|OFI/); 

$c_prototype="$funcretcode mv2_$funcname($funcpars)" if ($properties=~/MV2_PREFIX/ );
$c_prototype="$funcretcode mv2_proxy_$funcname($funcpars)" if ($properties=~/MV2_PROXY/ );

#print "static $funcretcode (*$funcname_t)($funcpars);\n" if ($properties=~/PSM2/); 
#print OUT_CPP "$funcretcode (*t_fab_profiler::$funcname_t)($funcpars) = 0;\n" if ($properties=~/PSM2/); 
#print "$funcretcode (*$funcname_t)($funcpars) = 0;\n" if ($properties=~/PSM2/); 
   

$funcpars=t_comm_func::remove_typenames($funcpars);
$funcretcode=t_comm_func::purify_val($funcretcode);

my $return;
my $declare;
my $call;
if ($funcretcode ne "void") {
  $call="rv = ";
  $declare="$funcretcode rv;";
  $return="return rv;";
}
#$call="$call t_" if $properties =~ /SMPI/;
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $print_code;

$funcname=$funcname_t if ($properties=~/T_PREFIX/); 

if ($properties =~ /VERBS/ || $properties =~ /SMPI/) {
    if ($funcid =~ /POLL/) {
        $print_code=`cat pinc/fab.poll.direct.code.inc`;
    } else {
        $print_code=`cat pinc/fab.direct.code.inc`;
    }
} else {
    if ($funcid =~ /IPEEK/ ) { #|| $funcid eq "FI_CQ_READ_ID") {
        $print_code=`cat pinc/fab.ipeek.code.inc`;
    } else {
       $print_code=`cat pinc/fab.code.inc`;
    }
}
eval $print_code;
print STDERR "Function $prototype:$@ on layer $fab_layer generated\n";
$funccount++;
}


sub make_class_gpu {
my $line=shift;
my @line=@$line;
my $retcode=$line[1];
my $funcname=$line[2];
my $funcpars=$line[3];
my $funcoutpars=$line[5];
my $properties=$line[6];
$retcode=t_comm_func::remove_retcode_extern($retcode);
#print STDERR "Generating $funcname\n";
my $c_prototype="$retcode $funcname($funcpars)";
my $funcretcode=t_comm_func::remove_typenames_retcode($retcode);
my $t_func_pointer="$retcode (*t_$funcname)($funcpars) = 0;";
my $rtdl="if(!t_$funcname) { t_$funcname = reinterpret_cast<$retcode (*)($funcpars)>(dlsym(RTLD_NEXT, \"$funcname\")); }\n" ; 
#print STDERR "Include written\n";

#if ($funcname =~/clCreateContext/) {
#print STDERR "$funcpars\n";die;
#}
$funcpars=t_comm_func::refine_params($funcpars);
my @funcpars_with_type=split(',',$funcpars);
$funcpars=t_comm_func::remove_typenames($funcpars);
my @funcpars=split(',',$funcpars);
my @funcoutpars=split(',',$funcoutpars);
my $num_params=@funcpars;
my $num_out_params=@funcoutpars;
my $num_extra_params="0";
   $num_extra_params="numWaitEvents" if $funcpars =~ /numWaitEvents/;
my $record_params;
if ($num_params>0) {
  if ($num_params==1) {
    $record_params="__msl_record_long(tid,level,(long long)$funcpars);";
  } else {
    $record_params="{
long long params[$num_params];
";
    for (my $i=0;$i<$num_params;$i++) {
      $funcpars[$i]="&$funcpars[$i]" if (($funcpars_with_type[$i]=~/ze_ipc_event_pool_handle_t/ ||  $funcpars_with_type[$i]=~/ze_ipc_mem_handle_t/) &&  (!($funcpars_with_type[$i]=~/\*/))); #supply adress instead of real structure 
      $record_params="$record_params
      params[$i]=(long long) $funcpars[$i];";
    }
    $record_params="$record_params
__msl_record_longs(tid,level,params,$num_params);
};
";
  }
}
my $record_out_params;
if ($num_out_params>0) {
  if ($num_out_params==1) {
    $record_out_params="__msl_record_long(tid,level,(long long)$funcoutpars);";
  } else {
    $record_out_params="{
long long params[$num_out_params];
";
    for (my $i=0;$i<$num_out_params;$i++) {
      $record_out_params="$record_out_params
      params[$i]=(long long) $funcoutpars[$i];";
    }
    $record_out_params="$record_out_params
__msl_record_longs(tid,level,params,$num_out_params);
};
";
  }
}

my $record_extra_params;
if ($num_extra_params eq "numWaitEvents") {
    $record_extra_params="if (numWaitEvents >0) {
long long extra_params[numWaitEvents];
";
    for (my $i=0;$i<$num_extra_params;$i++) {
      $record_extra_params="$record_extra_params
      extra_params[$i]=(long long) phWaitEvents[$i];";
    }
    $record_extra_params="$record_extra_params
__msl_record_longs(tid,level,extra_params,$num_extra_params);
}
";
}
my $c_call="$funcname($funcpars)";
my $return="return rv;";
my $Preturn="rv";
my $prototype=$c_prototype;
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
my $return;
my $declare;
my $call;
if ($funcretcode ne "void") {
  $call="rv = ";
  $declare="$funcretcode rv;";
  $return="return rv;";
}

my $print_code=`cat pinc/gpu_general.code.inc`;


eval $print_code;
print STDERR "Function $prototype:$@ generated\n";
}


sub make_internal_call {
my $line=shift;
my @line=@$line;
my $c_prototype="$line[1] $line[2]($line[3])";
my $properties=$line[6];
return if $properties=~/IGNORE/;
my $c_call=make_dup_call($line[2],$line[3]);
my $funcid_through="$line[0]_THROUGH";
my $funcid="$line[0]";
print <<EOF;
$c_prototype {
    void *return_addr[gt_info(num_addrs)];
    const int fidc=$funcid_through;
    stack_unwind(gt_info(max_ret_level),return_addr,gt_info(ret_levels));
    return ($c_call);
}
EOF
}

#ugly but simple check
sub check_mpi_v {
my $properties=shift;
my $class=1;
$class=1 if $properties =~/MPI1/;
$class=2 if $properties =~/MPI2/;
$class=3 if $properties =~/MPI3/;
$class=4 if $properties =~/MPI4/;
$class=5 if $properties =~/MPI5/;
return ($class>$mpilevel);
}

# MPI_VERSION / MPI_SUBVERSION guard derived from the PEAT_PROP_MPIx tag, so a wrapper is
# only compiled when the mpi.h being built against actually declares the function
sub mpi_version_guard {
my $properties=shift;
return "#if MPI_VERSION >= 5\n" if $properties =~/PEAT_PROP_MPI5\b/;
return "#if (MPI_VERSION > 4) || (MPI_VERSION == 4 && MPI_SUBVERSION >= 1)\n" if $properties =~/PEAT_PROP_MPI41\b/;
return "#if MPI_VERSION >= 4\n" if $properties =~/PEAT_PROP_MPI4\b/;
return "";
}

sub make_c_func {
my $lineref=shift;
my $fab_layer=shift;
my $guard=mpi_version_guard($$lineref[6]);
print $guard if $guard ne "";
make_c_func_body($lineref,$fab_layer);
print "#endif /* MPI version guard */\n" if $guard ne "";
return;
}

sub make_c_func_body {
my $lineref=shift;
my $fab_layer=shift;

# divide functions into 'classes' 
# clases are:
# p2p transfer + persistant init (CLASS_P2P)
# waitxxx/testxxx/startxxx (CLASS_REQUEST)
# probexxx (CLASS_PROBE)
# collective (CLASS_COLLECTIVE)
# One sided communication (CLASS_OSC)
# buffered local (CLASS_BLOCAL)
# simple local (CLASS_LOCAL just register occurence)
my $properties=$$lineref[6];
return if $properties=~/BUILTIN/;
return if $parse_mpi_internal==0 && $properties =~/MPI_INTERNAL/;
return if $properties =~/SKIP/;
return if check_mpi_v($properties);
$funccount++;
if ($properties =~/CALL_INTERNAL/) { make_internal_call($lineref);return;} 
if ($properties =~/CLASS_P2P/ || $properties =~/CLASS_PROBE/) { make_class_p2p($lineref);return;} 
if ($properties =~/CLASS_REQUEST/) { make_class_request($lineref);return;} 
if ($properties =~/CLASS_COLLECTIVE/) { make_class_collective($lineref);return;} 
if ($properties =~/CLASS_BUF_LOCAL/) { make_class_buf_local($lineref);return;} 
if ($properties =~/CLASS_OSC/) { make_class_osc($lineref);return;} 
if ($properties =~/CLASS_FIO/) { make_class_fio($lineref);return;} 
if ($properties =~/CLASS_CL/) { make_class_gpu($lineref);return;} 
if ($properties =~/CLASS_ZE/) { make_class_gpu($lineref);return;} 
if ($properties =~/CLASS_PSM2/) { make_class_fab($lineref,$fab_layer);return;} 
if ($properties =~/CLASS_PSM3/) { make_class_fab($lineref,$fab_layer);return;} 
if ($properties =~/CLASS_VERBS/) { make_class_fab($lineref,$fab_layer);return;} 
if ($properties =~/CLASS_SMPI/) { make_class_fab($lineref,$fab_layer);return;} 
if ($properties =~/CLASS_UCX/) { make_class_fab($lineref,$fab_layer);return;} 
if ($properties =~/CLASS_OFI/) { make_class_fab($lineref,$fab_layer);return;} 
make_class_local($lineref); #any other functions
return;
}



####################################################################################
sub parse_file{
my $file=shift;
my @line;
my $event;
my $rank=0;
my $linecount=0;
my $next_percentage=10;
my $cur_percentage=0;
my @fw_underscores=("","_");
my @bw_underscores=("","_","__");
my @uppercase=(0,1);

open(IN,$file) or die "Can't open input file: $file !" ;
print STDERR "Parsing file $file\n";

while( <IN> ) {
  chomp($_);
  my $line="$_ "; #cpp removes trailing spaces
  last if ($line =~/exit_parsing_here/);

  $line=~s/\/\/*//g;
  $line=~s/\"//g;
  $line=~s/{//g;
  $line=~s/}//g;
  #$line=~s/ +/ /g;

  @line = split(' COMMA ',$line);
  $line[0]=~s/ //g;
  $line[2]=~s/ //g;
  if ($line[2] eq "") {next;}
  if ($line[6] =~ /COMMA/) {
      print STDERR "Separator detected in $line[6] while parsing \n$line\n";
      die;
  }
  $linecount++;
  make_c_func(\@line);
}

if ($parse_gpu == 1) {
  while( <IN> ) {
    chomp($_);
    my $line=$_;
    last if ($line =~/cl_layer_starts_here/);
  }     
 
  print "#ifdef GPUPARSE\n";
  print "#include \"CL/cl.h\"\n";
  print "#include \"level_zero/ze_api.h\"\n";
#  print "void init_gpu_syms(void);\n";

#  print "int clv30=CL_VERSION_3_0;\n";
#  print "int clv22=CL_VERSION_2_2;\n";
#  print "int clv20=CL_VERSION_2_0;\n";
#  print "int clv12=CL_VERSION_1_2;\n";
#  print "int clv11=CL_VERSION_1_1;\n";
#  print "int clv10=CL_VERSION_1_0;\n";


  while( <IN> ) {
    chomp($_);
    my $line=$_;
    last if ($line =~/exit_parsing_here/);

    $line=~s/\/\/*//g;
    $line=~s/\"//g;
    $line=~s/{//g;
    $line=~s/}//g;
    #$line=~s/ +/ /g;

    @line = split(' COMMA ',$line);
    $line[0]=~s/ //g;
    $line[2]=~s/ //g;
    if ($line[2] eq "") {next;}
    $linecount++;
    make_c_func(\@line);
  }
#  print "static int gpu_layer_initialized=0;\n";
#  print "void init_gpu_syms(void) {\n";
#  print "int local_init=0;\n";
#  print "if (gpu_layer_initialized) return;\n";
#  print "#include \"tgpu.h\"\n";
#  print "gpu_layer_initialized=!local_init;\n";
#  print "debug_printf(\"local_init=%d zeInit=%p\\n\",local_init,t_zeInit);\n";
#  print "}\n";

  print "#endif // GPUPARSE\n";
}

close (IN);

return $linecount;
}

####################################################################################
sub parse_file_fab{
my $file=shift;
my $fab_layer=shift;
my $is_shim=shift; # standalone shim file: always built for exactly one layer, so skip the _FAB_PROFILE_ guard (it would just leave the shim empty)
my @line;
my $event;
my $rank=0;
my $linecount=0;
my $next_percentage=10;
my $cur_percentage=0;

open(IN,$file) or die "Can't open input file: $file !" ;
print STDERR "Parsing file $file for layer $fab_layer\n";

$is_shim_layer=$is_shim;

while( <IN> ) {
  chomp($_);
  my $line=$_;
  last if ($line =~/${fab_layer}_layer_starts_here/);
 }
  print "#if defined(_FAB_PROFILE_)\n" unless $is_shim;
  print "#include \"t_fab.h\"\n";
  if ($is_shim) { $fab_reentrant_declared=0; }
  if (!$fab_reentrant_declared) {
    print "static thread_local bool t_fab_reentrant = false;\n";
    $fab_reentrant_declared=1;
  }

while( <IN> ) {
  chomp($_);
  my $line=$_;
  last if ($line =~/exit_parsing_here/);

  $line=~s/\/\/*//g;
  $line=~s/\"//g;
  $line=~s/{//g;
  $line=~s/}//g;
  #$line=~s/ +/ /g;

  @line = split(' COMMA ',$line);
  $line[0]=~s/ //g;
  $line[2]=~s/ //g;

  if ($line[1] eq "") {next;}
  #print STDERR "$line[0] $line[1] $line[2]\n";
  $linecount++;
  make_c_func(\@line,$fab_layer);
}
close (IN);
  print "#endif /* defined(_FAB_PROFILE_) */\n" unless $is_shim;

return $linecount;
}



####################################################################################
sub parse_command_line {
   my $i;my $arg;
   for ($i=0; $i<=$#ARGV; $i++) {
      $arg=$ARGV[$i];
      switch ($arg) {
         case "-i" {$mpifuncs=$ARGV[$i+1]; $i++}
         case "-o" {$mpilayer=$ARGV[$i+1]; $i++}
         case "-m" {$mpilevel=$ARGV[$i+1]; $i++}
         case "-t" {$parse_mpi_internal=1 if $ARGV[$i+1] eq "on"; $i++}
         case "-c" {$parse_gpu=1 if $ARGV[$i+1] eq "on"; $i++}
         case "-e" {$print_extended_stuff=1 if $ARGV[$i+1] eq "on"; $i++}
         case "-p" {$use_pragma_weak=1 if $ARGV[$i+1] eq "on";$i++}
         case "-l" {my $fab_layer = $ARGV[$i+1]; push (@fab_layers,$fab_layer); $i++} 
         case "-s" {$skip_core=1 if $ARGV[$i+1] eq "on"; $i++} # skip core parse_file(): only fab shim files are wanted
         else {print "Unknown switch $ARGV[$i]\n";die;}
      }
   }
   die if $mpifuncs eq "" || $mpilayer eq "";
}


####################################################################################
########################### main program starts here ###############################

my $linecount=0;
parse_command_line;
if ($skip_core) {
   # nothing needs the core mpi_layer.cpp content (e.g. TARGET=fab_layers only
   # wants the per-layer files below); still touch it so Make's timestamp
   # dependency is satisfied without paying for the full parse_file() pass
   `touch $mpilayer`;
   open(OUT,">>$mpilayer") or die ("Cannot open $mpilayer for writing");
} else {
   `cp $mpilayer.in $mpilayer`;
   open(OUT,">>$mpilayer") or die ("Cannot open $mpilayer for writing");
   select(OUT);
   $linecount=parse_file($mpifuncs);
}
select(OUT);
foreach my $fab_layer_spec(@fab_layers) {
   # "-l layer" appends into the main $mpilayer (legacy single-fabric build);
   # "-l layer=outfile" writes a standalone shim source for that one layer,
   # so it can be built into its own libtractor_fab_<layer>.so and dlopen'd
   # at runtime depending on T_FAB_LAYER, instead of being baked into the core lib.
   my ($fab_layer,$fab_outfile) = split('=',$fab_layer_spec,2);
   if (defined $fab_outfile) {
      open(FABOUT,">$fab_outfile") or die ("Cannot open $fab_outfile for writing");
      select(FABOUT);
      print "#include \"mpi.h\"\n#include \"mpi_func_ids.h\"\n#include \"tractor.h\"\n";
      print "#if defined(__CPLUSPLUS__) || defined(__cplusplus)\nextern \"C\" {\n#endif\n";
      print "#include \"t_extradef.h\"\n";
      print "extern long long global_tool_overhead;\n";
      $linecount+=parse_file_fab($mpifuncs,$fab_layer,1);
      print "#if defined(__CPLUSPLUS__) || defined(__cplusplus)\n}\n#endif\n";
      select(OUT);
      close(FABOUT);
   } else {
      $linecount+=parse_file_fab($mpifuncs,$fab_layer,0);
   }
}

select(stdout);
`cat $mpilayer.in.end >> $mpilayer` unless $skip_core;

print "Completed. Parsed: $linecount, Generated $funccount.\n"; 

