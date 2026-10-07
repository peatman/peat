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

package t_comm_func;

####################################################################################
sub remove_commas {
my $number=shift;
$number=~ s/,//g;
return $number;
}

sub remove_retcode_extern {
my $funcpars=shift;
$funcpars=~s/extern\**//g;
return $funcpars;
}

sub remove_typenames_retcode {
my $funcpars=shift;
$funcpars=~s/extern\**//g;
$funcpars=~s/ZE_APIEXPORT\**//g;
$funcpars=~s/ZE_APICALL\**//g;
$funcpars=~s/CL_API_ENTRY\**//g;
$funcpars=~s/CL_API_CALL\**//g;

$funcpars=~s/CL_EXT_PREFIX__VERSION_1_1_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_1_2_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_2_1_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_2_2_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_1_1_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_1_2_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_2_1_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_2_2_DEPRECATED\** //g;
$funcpars=~s/ //g;
return $funcpars;
}
sub remove_typenames {
my $funcpars=shift;
$funcpars=~s/\*/ /g;

#$funcpars=~s/ze_api_version_t\** //g;
#$funcpars=~s/ze_driver_properties_t\** //g;
#$funcpars=~s/ze_driver_ipc_properties_t\** //g;
#$funcpars=~s/ze_device_compute_properties_t\** //g;
#$funcpars=~s/ze_command_queue_group_properties_t\** //g;
#$funcpars=~s/ze_device_memory_properties_t\** //g;
#$funcpars=~s/ze_device_memory_access_properties_t\** //g;
#$funcpars=~s/ze_device_cache_properties_t\** //g;
#$funcpars=~s/ze_device_image_properties_t\** //g;
#$funcpars=~s/ze_device_external_memory_properties_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_context_desc_t\** //g;
#$funcpars=~s/ze_init_flags_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;
#$funcpars=~s/ze_bool_t\** //g;

$funcpars=~s/MPI_Datarep_conversion_function\** //g;
$funcpars=~s/MPI_Datarep_extent_function\** //g;
# MPI-3/4/5 types (added 2026-09-29)
$funcpars=~s/MPI_Comm_copy_attr_function\** //g;
$funcpars=~s/MPI_Comm_delete_attr_function\** //g;
$funcpars=~s/MPI_Count\** //g;
$funcpars=~s/MPI_Datarep_conversion_function_c\** //g;
$funcpars=~s/MPI_F08_status\** //g;
$funcpars=~s/MPI_File_errhandler_function\** //g;
$funcpars=~s/MPI_Fint\** //g;
$funcpars=~s/MPI_Grequest_cancel_function\** //g;
$funcpars=~s/MPI_Grequest_free_function\** //g;
$funcpars=~s/MPI_Grequest_query_function\** //g;
$funcpars=~s/MPI_Session\** //g;
$funcpars=~s/MPI_Session_errhandler_function\** //g;
$funcpars=~s/MPI_T_cb_safety\** //g;
$funcpars=~s/MPI_T_cvar_handle\** //g;
$funcpars=~s/MPI_T_enum\** //g;
$funcpars=~s/MPI_T_event_cb_function\** //g;
$funcpars=~s/MPI_T_event_dropped_cb_function\** //g;
$funcpars=~s/MPI_T_event_free_cb_function\** //g;
$funcpars=~s/MPI_T_event_instance\** //g;
$funcpars=~s/MPI_T_event_registration\** //g;
$funcpars=~s/MPI_T_pvar_handle\** //g;
$funcpars=~s/MPI_T_pvar_session\** //g;
$funcpars=~s/MPI_T_source_order\** //g;
$funcpars=~s/MPI_Type_copy_attr_function\** //g;
$funcpars=~s/MPI_Type_delete_attr_function\** //g;
$funcpars=~s/MPI_User_function_c\** //g;
$funcpars=~s/MPI_Win_copy_attr_function\** //g;
$funcpars=~s/MPI_Win_delete_attr_function\** //g;
$funcpars=~s/MPI_Win_errhandler_function\** //g;
$funcpars=~s/MPI_Datatype\** //g;
$funcpars=~s/MPI_Message\** //g;
$funcpars=~s/MPID_Request\** //g;
$funcpars=~s/MPIO_Request\** //g;
$funcpars=~s/MPI_Request\** //g;
$funcpars=~s/MPI_Offset\** //g;
$funcpars=~s/MPI_Status\** //g;
$funcpars=~s/MPI_Group\** //g;
$funcpars=~s/MPID_Comm\** //g;
$funcpars=~s/MPI_Comm\** //g;
$funcpars=~s/MPI_Info\** //g;
$funcpars=~s/MPI_File\** //g;
$funcpars=~s/MPI_Win\** //g;
$funcpars=~s/MPI_Op\** //g;
$funcpars=~s/MPI_Comm_errhandler_function\** //g;
$funcpars=~s/MPI_Handler_function\** //g;
$funcpars=~s/MPI_Delete_function\** //g;
$funcpars=~s/MPI_User_function\** //g;
$funcpars=~s/MPI_Errhandler\** //g;
$funcpars=~s/MPI_Aarray_of_displacements\** //g;
$funcpars=~s/MPI_Adisplacement\** //g;
$funcpars=~s/MPI_Aint\** //g;
$funcpars=~s/MPI_Copy_function\** //g;
$funcpars=~s/MTIOMPIDconst\** //g;
$funcpars=~s/MTIconst\** //g;
$funcpars=~s/psmi_mq_status_copy_user_t\** //g;
$funcpars=~s/smp_ctrl_avail_flag_t\** //g;
$funcpars=~s/MPIDI_CH3_Pkt_t\** //g;
$funcpars=~s/MPIDI_VC_t\** //g;
$funcpars=~s/MPIDI_PG_t\** //g;

$funcpars=~s/extern\** //g;
$funcpars=~s/CL_API_ENTRY\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_1_1_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_1_2_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_2_1_DEPRECATED\** //g;
$funcpars=~s/CL_EXT_PREFIX__VERSION_2_2_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_1_1_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_1_2_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_2_1_DEPRECATED\** //g;
$funcpars=~s/CL_API_PREFIX__VERSION_2_2_DEPRECATED\** //g;
$funcpars=~s/cl_platform_id\** //g;
$funcpars=~s/cl_ulong\** //g;
$funcpars=~s/cl_uint\** //g;
$funcpars=~s/cl_long\** //g;
$funcpars=~s/cl_int\** //g;
$funcpars=~s/cl_device_partition_property\** //g;
$funcpars=~s/cl_command_queue_info\** //g;    
$funcpars=~s/cl_device_id\** //g;
$funcpars=~s/cl_device_info\** //g;
$funcpars=~s/cl_context_properties\** //g;
$funcpars=~s/cl_context\** //g;


$funcpars=~s/cl_kernel_work_group_info\** //g;
$funcpars=~s/cl_kernel_sub_group_info\** //g;
$funcpars=~s/cl_kernel_exec_info\** //g;
$funcpars=~s/cl_kernel_arg_info\** //g;
$funcpars=~s/cl_kernel_info\** //g;
$funcpars=~s/cl_kernel\** //g;
$funcpars=~s/cl_command_queue\** //g;
$funcpars=~s/cl_platform_info\** //g;
$funcpars=~s/cl_device_type\** //g;    
$funcpars=~s/cl_mem_flags\** //g;    
$funcpars=~s/cl_context_info\** //g;    
$funcpars=~s/cl_mem_properties\** //g;    
$funcpars=~s/cl_image_format\** //g;    
$funcpars=~s/cl_image_desc\** //g;    
$funcpars=~s/cl_queue_properties\** //g;    
$funcpars=~s/cl_profiling_info\** //g;    
$funcpars=~s/cl_bool\** //g;    
$funcpars=~s/cl_command_queue_properties\** //g;    
$funcpars=~s/cl_addressing_mode\** //g;    
$funcpars=~s/cl_filter_mode\** //g;    
$funcpars=~s/cl_mem_migration_flags\** //g;    
$funcpars=~s/cl_map_flags\** //g;    
$funcpars=~s/cl_queue_properties\** //g;    
$funcpars=~s/cl_program_build_info\** //g;    
$funcpars=~s/cl_program_info\** //g;    
$funcpars=~s/cl_program\** //g;    
$funcpars=~s/cl_mem_properties'\** //g;    
$funcpars=~s/cl_mem_object_type\** //g;    
$funcpars=~s/cl_mem_info\** //g;    
$funcpars=~s/cl_image_info\** //g;    
$funcpars=~s/cl_svm_mem_flags\** //g;    
$funcpars=~s/cl_pipe_properties\** //g;    
$funcpars=~s/cl_pipe_info\** //g;    
$funcpars=~s/cl_buffer_create_type\** //g;    
$funcpars=~s/cl_sampler_info\** //g;    
$funcpars=~s/cl_sampler_properties\** //g;    
$funcpars=~s/cl_sampler\** //g;    



$funcpars=~s/cl_event_info\** //g;    
$funcpars=~s/cl_event\** //g;    


$funcpars=~s/cl_mem\** //g;    

$funcpars=~s/CL_CALLBACK\** //g;
$funcpars=~s/CL_API_CALL\** //g;

$funcpars=~s/psm2_ep_t\** //g;
$funcpars=~s/psm2_mq_t\** //g;
$funcpars=~s/psm2_epaddr_t\** //g;
$funcpars=~s/psm2_mq_tag_t\** //g;
$funcpars=~s/psm2_mq_req_t\** //g;
$funcpars=~s/psm2_mq_status_t\** //g;
$funcpars=~s/psm2_mq_status2_t\** //g;
$funcpars=~s/psm2_error_t\** //g;
$funcpars=~s/psm2_epid_t\** //g;
$funcpars=~s/psm2_uuid_t\** //g;

$funcpars=~s/ucp_params_t\** //g;
$funcpars=~s/ucp_config_t\** //g;
$funcpars=~s/ucp_context_h\** //g;
$funcpars=~s/ucp_context_attr_t\** //g;
$funcpars=~s/ucp_worker_params_t\** //g;
$funcpars=~s/ucp_worker_h\** //g;
$funcpars=~s/ucp_ep_params_t\** //g;
$funcpars=~s/ucp_ep_h\** //g;
$funcpars=~s/ucp_send_callback_t\** //g;
$funcpars=~s/ucp_tag_recv_callback_t\** //g;
$funcpars=~s/ucp_tag_recv_info_t\** //g;
$funcpars=~s/ucp_datatype_t\** //g;
$funcpars=~s/ucp_tag_t\** //g;
$funcpars=~s/ucp_tag_message_h\** //g;
$funcpars=~s/fi_addr_t\** //g;
$funcpars=~s/fid_t\** //g;
$funcpars=~s/volatile //g;
$funcpars=~s/unsigned\** //g;
$funcpars=~s/uint64_t\** //g;
$funcpars=~s/uint32_t\** //g;
$funcpars=~s/uint16_t\** //g;
$funcpars=~s/uint8_t\** //g;
$funcpars=~s/int64_t\** //g;
$funcpars=~s/int32_t\** //g;
$funcpars=~s/int16_t\** //g;
$funcpars=~s/int8_t\** //g;
$funcpars=~s/pid_t\** //g;
$funcpars=~s/vbuf\** //g;
$funcpars=~s/ssize_t\** //g;
$funcpars=~s/size_t\** //g;
$funcpars=~s/const //g;
$funcpars=~s/struct\** [a-z|0-9|_]*\** //g;
$funcpars=~s/enum\** [a-z|0-9|_]*\**//g;
$funcpars=~s/union\** [a-z|0-9|_]*\**//g;
$funcpars=~s/void\** //g;
$funcpars=~s/char\** //g;
$funcpars=~s/int\** //g;
$funcpars=~s/long\** //g;
$funcpars=~s/ze_[^ ]*_t\** //g;

#while ($funcpars=~m/ze_.+_t\** /) {
##print STDERR "reducing $funcpars...";
#$funcpars=~s/ze_[^ ]*_t\** //;
##print STDERR "after reducing = $funcpars\n";
#}


$funcpars=~s/\[.*\]//g;
$funcpars=~s/ //g;
$funcpars="" if $funcpars eq "void";
return $funcpars;
}


sub purify_val{
my $code=shift;
#$code=~s/const //g;
$code=~s/^ *//g;
$code=~s/ *$//g;
return $code;
}

sub refine_params{ # can do only one func
my $funcpars=shift;
$funcpars=~s/\(//;
$funcpars=~s/\)//;
$funcpars=~s/\(.*\)//;

return $funcpars;
}

1;
