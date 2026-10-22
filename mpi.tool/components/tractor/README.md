# tractor - Trace Collector Runtime

Part of peat.

## What it does

A low-intrusion, `LD_PRELOAD`-based shared library that intercepts MPI (and optionally OpenMP/OMPT, and GPU (level zero and openCL)) calls and records them into the trace files 
consumed by `mat`, `ctt`, `urpu`, `pet` and `qTV`. Also drives the PTP Daemon (PTPD) used to synchronise timings across nodes. Supports Intel MPI, MPICH, MVAPICH and Open MPI on Linux.

See [doc/README.md](doc/README.md) for older build/usage/trace-format documentation (note: marked outdated there, kept for reference).

## Layout

- `src/tractor.cpp`/`.h` - main entry point of the interception library.
- `src/t_comm.cpp`/`.h` - communicator tracking.
- `src/t_alloc.h` - internal allocator/memory tracking used by the library itself.
- `src/t_base_event.cpp`/`.h`, `src/t_stat.cpp`/`.h` - base trace-event and statistic record types.
- `src/t_ompt.cpp` - OpenMP (OMPT) callback hooks for combined MPI+OpenMP tracing.
- `src/t_topo.cpp` - node/CPU topology capture via `hwloc`.
- `src/t_signals.cpp`/`.h` - signal handling for clean trace flushing on crash/termination.
- `src/elf_parser.cpp`/`.h` - ELF/debug-info symbol resolution for call-site addresses.
- `src/src-ptpd.cpp` - the PTP Daemon (PTPD) used to synchronise clocks across nodes.
- `src/mpi_layer.cpp.in`(`.end`), `src/mpi_layer.pl`, `src/mpi_layer_comm_begin/end.prolog/epilog.h` - templated/generated MPI interception layer.
- `src/pinc/` - per-call-category C++ code snippets included by the generated MPI layer (collectives, requests, local/buffer handling, GPU).
- `doc/` - readme and trace format description documents.
- `Build.sh`/`BuildAllg.sh`/`BuildAlli.sh` - build scripts for GNU/Intel toolchains across supported MPI.
 


Tractor usage guideline:
1. Required
1.1. General 
PEAT_ROOT=<PEAT_ROOT>

for out of the box mpich based mpi in order to trace fortran calls custom libmpifort (which calls MPI_xxx instead of PMPI_xxx) has to be compiled (for Intel MPI mpich 3.4.2 is ok). 
For patched mpich based mpi and for openmpi it's not needed.
export LD_LIBRARY_PATH=<PATH_TO_libmpifort.so_DIRECTORY>:$LD_LIBRARY_PATH


suffix={".impi","_dbg.impi",etc} 

LD_PRELOAD=$PEAT_ROOT/mpi.tool/lib/libtractor${suffix}.so

If you want to execute PTP Daemon please do it manually. (for trace estimation only - do not required for analysis)
1.2. PControl related.
export PEAT_TRACTOR_PCONTROL={0 or 1} 1 if pcontrol is used, 0 (default) pcontrol is not used.
export PEAT_TRACTOR_PCONTROL_FUNCS={comma separated list}  (no spaces or other delimiters allowed) a list of mpi functions with will be 
    traced even outside of any pcontrol (MPI_Init, MPI_Init_thread, MPI_Finalize  and MPI_Pcontrol are included anyway). 
export PEAT_TRACTOR_PC_LEVELS=<comma_separated_list> list of pcontrol levels to handle.

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!! No matter what PCONTROL options are used MPI_INIT,MPI_INIT_THREAD_MPI_FINALIZE and MPI_PCONTROL are recorded ANYWAY! !!!!!
!!! It's essential to use pinning that explicitly "nails" ranks to cores (i.e. DO NOT allow ranks to migrate accross cores !!!!
!!!!!!!!! otherwise there may be discrepancies in event timings (rdtsc returns different results for different cores) !!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

2. Optional:
export PEAT_TRACTOR_FILE=<trace_file_name_template>  - main file(s) template. If omitted tractor will use "tractor_t"
export PEAT_TRACTOR_DIR=<working_directory>  - Directory where trace files will be locared. If omitted tractor will use current one.
export PEAT_TRACTOR_TMP_DIR=<temp_directory>  - Directory for temporal files (realtime dumping). If omitted equals PEAT_TRACTOR_DIR. 
    Use as fast FS as possible. /tmp seems to be OK.

export PEAT_TRACTOR_PRINT_FUNC_NAMES=0 - places function IDs instead of names in the trace file
export PEAT_TRACTOR_STORE_MAP=1 store map files (useful for debugging or manual symbol resolution)
export PEAT_TRACTOR_WRITE_TRACE=0 Do not write main trace file if you want only statistic.
export PEAT_TRACTOR_USE_REALTIME_DUMP=1 dump events once the buffer is full. Decreases tractor memory footprint but increases 
intrusiveness.
export PEAT_TRACTOR_RETURN_LEVELS={comma separated list} (no spaces or other delimiters allowed), Return levels for stack unwind 
    recording. Example: "0,3,7" will save 3 levels 0-th, 3-rd and 7-th. Default "0".
export PEAT_TRACTOR_INCLUDE_EVENTS={comma separated list} (no spaces or other delimiters allowed) a list of mpi functions with will be 
    traced other will be ignored (MPI_Init, MPI_Init_thread, MPI_Finalize  and MPI_Pcontrol are included anyway). 
export PEAT_TRACTOR_EXCLUDE_EVENTS={comma separated list} (no spaces or other delimiters allowed) a list of mpi functions with won't be 
    traced  (MPI_Init, MPI_Init_thread, MPI_Finalize  and MPI_Pcontrol are included anyway). 

export PEAT_TRACTOR_INCLUDE_FULL_STACK_EVENTS, PEAT_TRACTOR_EXCLUDE_FULL_STACK_EVENTS - select (include or exclude) events for full or partial (level 0) 
stack unwind (useful if some functions like MPI_Init is located on top and others deep down - avoids crashing) 

export PEAT_TRACTOR_RESOLVE=0 - do not resolve communicators and ranks i.e. write them "as is". Sometimes usefull for debugging purposes. 
export PEAT_TRACTOR_BOUNDARIES={comma separated list} (no spaces or other delimiters allowed) a list of boundaries which groups message 
    sizes (for statistic section in ".com" file). Default is {0,16,64,256,16384,262144,LLONG_MAX}.
export PEAT_TRACTOR_MIN_ADDRESS=<min_adress>
export PEAT_TRACTOR_MAX_ADDRESS=<max_adress>
    are system specific. Usualy user should leave them blank.
export PEAT_TRACTOR_USE_SIGNALS=1 - Install handlers for some system signals to catch exceptions or MPI daemon termination signals
export PEAT_TRACTOR_MAX_THREADS=<value> maximum allowed threads (default 6, safe selection is OMP_NUM_THREADS+5)
export PEAT_TRACTOR_FULL_OMPT_UNWIND full (1) or partial (1st caller) (0) unwind of OpenMP events

PTP Daemon can be executed either by hand using "&" or by tractor itself using values below

export PEAT_TRACTOR_PTPD_EXECUTABLE=<ptpd_daemon_executable> - if omitted tractor will attempt to identify correct daemon by itself
export PEAT_TRACTOR_PTPD_OUTPUT=<output_file> - if omitted tractor will use it own predefined name
export PEAT_TRACTOR_PTPD_NODEFILE=<nodefile> - the file must exist and contains correct nodes list. if omitted tractor will build it by itself.  
export PEAT_TRACTOR_PTPD_TIMENODE=<timenode_hostname> - if omitted timenode won't be used.
export PEAT_TRACTOR_PTPD_UI=<update_interval_in_microseconds> - default (if omitted) 100000 usec (0.1 sec)
export PEAT_TRACTOR_USE_PTPD=1 enables starting PTP Daemon by tractor (otherwise it has to be executed manually). 

Usualy PTPD is not required unless trace will later be used in qTV or PET, so you may switch it off. 
Tractor PTPD starting functionality was not thoroughly tested so use it with caution.

3. Executing 
a. without PTPD or with "auto" PTPD
mpiexec.hydra -launcher ssh -genvall -genv LD_PRELOAD $LD_PRELOAD <your_usual_parameters>
(adding -genvall and -launcher ssh is strongly suggested)

b. (with ptpd executed manually)
  mpiexec.hydra -launcher ssh -genvall -ppn 1 -f <PEAT_TRACTOR_PTPD_NODEFILE> -n `cat <PEAT_TRACTOR_PTPD_NODEFILE> | uniq |wc -l` <PEAT_TRACTOR_PTPD_EXECUTABLE> <PTPD_UI>  <PEAT_TRACTOR_PTPD_OUTPUT> <time_node_used> &
  mpiexec.hydra -launcher ssh -genvall -genv LD_PRELOAD $LD_PRELOAD <your_usual_parameters>
  where
  time_node_used = 1 if first entry in nodefile is time node host (otherwise 0).
  All other names between <> are corresponded to environment variables above. 
  After main program finishes PTPD daemon must be terminated.
  ssh `cat <PEAT_TRACTOR_PTPD_NODEFILE> | uniq | head -1` killall <PEAT_TRACTOR_PTPD_EXECUTABLE>
