# tractor/doc

Documentation for the tractor trace collector.

For build/usage instructions, see [../README.md](../README.md).

## Contents

- `Tractor.Trace.Format.Description.pdf` - trace format.

---

The remainder of this file is legacy documentation, kept for historical reference only; it does not reflect the current build/usage (see [../README.md](../README.md) instead).

0. Disclaimer
   This package provided �AS IS�. This is NOT an official product. Use it for your own risk.
   I OFFER NO WARRANTY EITHER EXPRESS OR IMPLIED. I SHALL NOT BE LIABLE 
   FOR ANY DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, 
   BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER LOSS) ARISING OUT OF THE USE OF OR 
   INABILITY TO USE THE SOFTWARE, EVEN IF I HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. 
   This tool was not deeply tested so crashes etc. might occur.

1. What it's for.
   This is simple easy-to-use, customisable, low intrusive tool to analyse MPI behaviour. 
   Available on Linux only. Supports Intel MPI, MVAPICH2, MVAPICH24, MPICH4, Open MPI (5x).
    
2. Package Content:
   Source, makefile.
   This file.

3. Current Directory Structure:
   [TOP]
   ../../common/mpifunc.inc
   ../../common/mpifuncids.h
        /src
                Makefile
                addr_entry.h, records.h, t_defines.h, t_extradef.h, t_mpidef.h, t_time.h
                tractor.cpp/.h        - main interception library entry point
                t_comm.cpp/.h         - communicator tracking
                t_fab.cpp/.h          - fabric-level (PSM2/PSM3/Verbs/UCX/OFI) event profiling
                t_alloc.cpp/.h        - internal allocator/memory tracking
                t_base_event.cpp/.h   - base trace-event/statistic record types
                t_stat.cpp/.h         - statistics collection
                t_globals.cpp         - global state
                t_ompt.cpp            - OpenMP (OMPT) callback hooks
                t_topo.cpp            - node/CPU topology capture (hwloc)
                t_signals.cpp/.h      - signal handling for clean trace flushing
                elf_parser.cpp/.h     - ELF/debug-info symbol resolution
                hwcounter.cpp/.h      - PAPI hardware performance counter sampling
                memprofile.cpp/.h     - memory allocation profiling hooks
                virt2phys.cpp         - virtual-to-physical address resolution
                src-ptpd.cpp          - the PTP Daemon (PTPD) executable
                mpi_layer.cpp.in(.end), mpi_layer.pl - templated/generated MPI interception layer
                mpi_layer_comm_begin/end.prolog/epilog.h
                cl_layer.pl, cl_proto.pl, ze_layer.pl, ze_proto.pl, ze_api.h - OpenCL/Level Zero (GPU) layer generators
                fab_func_layers.pl, funcids_convert.pl(.old) - fabric/function-ID table generators
                test_mpi.c/.f         - C/Fortran MPI test programs used to validate the built library
                /pinc
                    buf_local.code.inc, collective.code.inc, create_alloc.code.inc,
                    local.code.inc, request.code.inc, rt.code.inc, sendrecv.code.inc,
                    fab.code.inc, fab.direct.code.inc, fab.ipeek.code.inc, fab.poll.direct.code.inc,
                    gpu_general.code.inc
        /doc/
                README.md (this file)
                Tractor.Trace.Format.Description.pdf 
4. Usage Model
   4.1 Build process.
      4.1.1 
         a. Assuming  that all paths(Compiler,MPI,PAPI) are properly set.  
         b. Go to src directory and execute 'make'. libtractor.<specifiers>so should be built. After make install all required library are installed into PEAT_ROOT/lib and all binaries are in PEAT_ROOT/bin.
      4.1.2 Build switches (specified after make command, * - indicates default value, see src/Makefile for the authoritative list)
        TARGET=[impi*,mpich4,mpich5,mvapich2,mvapich4,openmpi] compile under/for a certain MPI flavor. TARGET=fab_layer builds special libraries which can be loaded in runtime depending on switch (see 4.2).
        COMPILER=[gnu*,intel] toolchain used to build.
        FABRIC=[on,off*] build in 'fab' interception (psm2,psm3,ucx,ofi).
        PTPD=[on*,off] build in support for the Precision Time Protocol Daemon (adds -D_TRACTOR_USE_PTPD_).
        OMPT=[on*,off] build in OpenMP (OMPT) callback hooks for combined MPI+OpenMP tracing.
        DIAG=[off*,on] enable diagnostic/debug messages during collection (DEBUG purposes). Adds _dbg to the library name, forces -O0.
        PARSE_GPU=[off*,on] build in OpenCL/Level Zero (GPU) interception layers.
        PARSE_COLT=[off*,on] enable collective-timeline parsing engine.
        FABRIC=[off*,PSM2,PSM3,VERBS,UCX,OFI] (mvapich2 target only) build in fabric-level (PSM2/PSM3/Verbs/UCX/OFI) event profiling via t_fab.
        USE_SIGNALS=[on*,off] install signal handlers for clean trace flushing on crash/termination.
        USE_PRAGMA_WEAK=[on*,off] use weak symbols for the generated MPI interception layer.
        MAXMPI=[3*,...] MPI standard level targeted by the generated layer.
        example: make install TARGET=mpich4 DIAG=on; 

   4.2 Running   
       4.2.1 Tractor itself
       a. Put and propagate to all ranks:

       export LD_PRELOAD=<PEAT_ROOT>/lib/<compiler>/libtractor<specifiers>.so
       where specifiers are $(SUFFIX)$(DBG).$(TARGET)$(EXTRANAMELIB)
       SUFFIX  exactly the same as one in 4.1.2 (usually empty)
       DBG={_dbg,<empty>}
       TARGET  exactly the same as one in 4.1.2
       EXTRANAMELIB  accumulates suffixes from optional switches:  .gpu (PARSE_GPU), .eng (PARSE_COLT/fab layer)

       Environment variables (all prefixed PEAT_TRACTOR_, read by tractor::parse_env_vars(), * - indicates default value):
       export PEAT_TRACTOR_FILE=<template>            - template for trace file names (default: tractor_t)
       export PEAT_TRACTOR_DIR=<path>                  - directory trace files are written to (default: .)
       export PEAT_TRACTOR_TMP_DIR=<path>              - directory for temporary files (default: .)
       export PEAT_TRACTOR_ONE_TIME_ALLOC=<value>      - size of preallocated event buffers
       export PEAT_TRACTOR_MAX_TID_NUM=<value>         - max number of (OpenMP) thread IDs tracked per rank
       export PEAT_TRACTOR_MAX_THREADS=<value>         - max number of threads (default: 6)
       export PEAT_TRACTOR_CALLS_LIMIT=<value>         - stop recording new events once this many have been recorded (default: unlimited)
       export PEAT_TRACTOR_WRITE_TRACE=1|0             - write MAIN/ext trace files (default: on); when off, only overall statistics are kept
       export PEAT_TRACTOR_WRITE_ON_FINALIZE=1|0       - only flush/write trace at MPI_Finalize instead of incrementally (default: off)
       export PEAT_TRACTOR_USE_REALTIME_DUMP=1|0       - dump events to disk as they occur instead of buffering (default: off)
       export PEAT_TRACTOR_STORE_MAP=1|0               - also store a copy of /proc/self/maps per rank for address resolving (default: off)
       export PEAT_TRACTOR_RESOLVE=1|0                 - resolve communicators/ranks while running (default: on)
       export PEAT_TRACTOR_CORRECT_EVENTS=1|0          - apply runtime timer-correction to events (default: off)
       export PEAT_TRACTOR_COMPRESS_EVENTS=<value>     - compress recorded events (default: off/0)
       export PEAT_TRACTOR_PRINT_ALL_STAT=1|0          - print statistics for all functions even with zero occurrences (default: off)
       export PEAT_TRACTOR_PRINT_FUNC_NAMES=1|0        - print MPI function names during the run (default: off)
       export PEAT_TRACTOR_USE_SIGNALS=1|0             - install signal handlers for clean trace flushing on crash/termination (default: off)
       export PEAT_TRACTOR_PARSE_OPENMP=1|0            - parse OpenMP (OMPT) events when built with OMPT support (default: on)
       export PEAT_TRACTOR_FULL_OMPT_UNWIND=<value>    - depth of OMPT call-stack unwinding (default: 0)
       export PEAT_TRACTOR_ABORT_SLEEP=<seconds>       - seconds to sleep before aborting on fatal error, to allow ranks to flush their files (default: 300)
       export PEAT_TRACTOR_COLLECT_FAB=1|0             - collect fabric-level (PSM2/PSM3/Verbs/UCX/OFI) events, when PEAT_TRACTOR_WRITE_TRACE is on
       export PEAT_TRACTOR_FAB_LAYER=ucx|ofi|psm2|psm3 - collect fabric-level (PSM2/PSM3/Verbs/UCX/OFI) events, when PEAT_TRACTOR_COLLECT_FAB is on (only for libraries with .eng suffix)
       export PEAT_TRACTOR_INCLUDE_EVENTS / PEAT_TRACTOR_EXCLUDE_EVENTS="comma_separated_ids" - include/exclude particular MPI functions from recording (see mpifuncids.h for IDs). If both are specified, PEAT_TRACTOR_EXCLUDE_EVENTS is ignored.
       export PEAT_TRACTOR_INCLUDE_FULL_STACK_EVENTS / PEAT_TRACTOR_EXCLUDE_FULL_STACK_EVENTS="comma_separated_ids" - same, but controls which events get full-stack (multi-level) return-address resolution.
       export PEAT_TRACTOR_BOUNDARIES=<value>          - restrict tracing to a boundary range
       export PEAT_TRACTOR_PCONTROL=<value>            - enable MPI_Pcontrol-gated tracing; >0 activates PEAT_TRACTOR_PCONTROL_FUNCS filtering
       export PEAT_TRACTOR_PCONTROL_FUNCS="comma_separated_ids" - functions counted even outside of MPI_Pcontrol region

       PTPD-related (only meaningful when built with PTPD=on):
       export PEAT_TRACTOR_USE_PTPD=1|0                - enable PTP Daemon (default: off)
       export PEAT_TRACTOR_PTPD_EXECUTABLE=<PEAT_ROOT>/bin/<compiler>/ptpd<specifiers> - PTP daemon executable. Library will try to determine it if not specified. Aborts if it fails.
       export PEAT_TRACTOR_PTPD_NODEFILE=<path>         - make sure that tractor is allowed to WRITE into that file. ptpd-nodes-$(PEAT_TRACTOR_FILE) is the default.
       export PEAT_TRACTOR_PTPD_OUTPUT=<path>           - make sure that tractor is allowed to WRITE into that file. ptpd-$(PEAT_TRACTOR_FILE) is the default.
       export PEAT_TRACTOR_PTPD_UI=<update_interval_in_microseconds> - default is 100000 (0.1 sec).
       export PEAT_TRACTOR_PTPD_TIMENODE=<hostname>     - if specified tractor will use this dedicated node as ptpd root (otherwise the 1st node is used).

       Next switches are for special purposes and not intended for ordinary usage

       export PEAT_TRACTOR_RETURN_LEVELS=<comma_separated_values>             - set level(s) for return address capture
       export PEAT_TRACTOR_PC_LEVELS=X                                        - set level(s) for PControl
       export PEAT_TRACTOR_MIN_ADDRESS=<hexadecimal_value>
       export PEAT_TRACTOR_MAX_ADDRESS=<hexadecimal_value>
          The library will only obtain the deeper return level if the shallower one belongs to the main binary or
          falls between PEAT_TRACTOR_MIN_ADDRESS (0 is the default) and PEAT_TRACTOR_MAX_ADDRESS (0x7FFFFFFFFFFFFFFF is the default)
        4.2.2 PTP Daemon (PTPD):
        There is 2 way to execute PTPD: 1 automatically through tractor or manually using '&'.
        For 'automatic' way several environment variables started with T_PTPD should be set, plus PEAT_TRACTOR_USE_PTPD=1. If dedicated time node is planned to use specify its name in PEAT_TRACTOR_PTPD_TIMENODE.
        For 'manual'way a few important things should be kept in mind:
            a. PTPD should be run on exactly the same nodes with exactly the same order(except dedicated timenode if in use). Typical command is: uniq OLD_NODEFILE > NEW_NODEFILE.
            b. Timenode if in use shold be place FIRST in nodefile. Typical command is: cat PTPD_NODEFILE | sed 1d > TRACTOR_NODEFILE ; 
            c. Daemon must be terminated by sending SIGTERM signal to its 'main' process (located on timenode).
 
        example(LSF):
           automatic:
           export PEAT_TRACTOR_USE_PTPD=1
           export PEAT_TRACTOR_PTPD_TIMENODE=`cat $LSB_DJOB_HOSTFILE | head -1`
           cat $LSB_DJOB_HOSTFILE | uniq | sed 1d > newnodes.txt
           export PEAT_TRACTOR_PTPD_UI=1000000
           mpiexec.hydra -bootstrap ssh  -genvall -perhost 4 -f newnodes.txt -genv LD_PRELOAD ../../../lib/intel/libtractor.impi.so -np 33 /panfs/projects/innl/aknyaze1/IMB_3.1/src/IMB-MPI1 >IMB-MPI1.debug.log #-iter 10,4,1 >IMB-MPI1.debug.log
        manual:
           cat $LSB_DJOB_HOSTFILE | uniq | sed 1d > newnodes.txt
           mpiexec.hydra -bootstrap ssh  -genvall -perhost 1 -f $LSB_DJOB_HOSTFILE -n `cat $LSB_DJOB_HOSTFILE | uniq |wc -l` ../../../bin/intel/ptpd.impi 1000000 ptpd-IMB-MPI1.log 1 &
           mpiexec.hydra -bootstrap ssh  -genvall -perhost 4 -f newnodes.txt -genv LD_PRELOAD ../../../lib/intel/libtractor.impi.so -np 33 /panfs/projects/innl/aknyaze1/IMB_3.1/src/IMB-MPI1 >IMB-MPI1.debug.log #-iter 10,4,1 >IMB-MPI1.debug.log
           ssh `cat $LSB_DJOB_HOSTFILE | head -1` killall ptpd.impi 

         
