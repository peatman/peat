# PET - Performance Estimation Tool

Part of peat.

## What it does

Consumes `tractor` traces together with network/hardware profiles to model and estimate MPI application performance (per-rank/thread timelines, communicator, GPU, OpenMP and fabric events), 
helping estimate behaviour under different transport/network configurations.

Available on Linux (GNU Makefile) and Windows (MSVC solution).
Used for estimation purposes. Usualy you don't need this utility for performance analysis.

## Layout

- `pet_core.cpp`/`.h` - core estimation engine and orchestration.
- `pet_node.cpp`/`.h`, `pet_rank.cpp`/`.h`, `pet_thread.cpp`/`.h` - per-node/rank/thread modelling.
- `pet_event.h`, `pet_fabric_event.h`, `pet_defines.h` - event and fabric-related type definitions.
- `te_timeline.cpp`/`.h`, `timeline.cpp`/`.h`, `te_event.h` - timeline construction from trace events.
- `pet_events/` - per-event-kind estimation handlers: `collective.cpp`/`coll.cpp`, `comm.cpp`, `completion.cpp`, `gpu.cpp`, `local.cpp`/`localbuf.cpp`, `ompt.cpp` (OpenMP regions), `start.cpp`, `tar.cpp`/`tor.cpp`, `base.cpp`.
- `collstorage.cpp`/`.h`, `events.cpp`, `stat.cpp`/`.h`, `requests.cpp`/`.h` - collective/statistic/request bookkeeping.
- `main.cpp` - CLI entry point, argument parsing and driving the estimation run.
- `profiles/` - network/fabric profile definition files (e.g. `8260-2993-ib-200-verbs.in`, `8260-2993-opa-100-psm2.in`, `8260-split.in`) describing hardware characteristics used by the estimation model.
- `Build.sh`/`Makefile` - build scripts producing the `pet` binary.

Note:
It's not activelly being developed at the moment, some bugs recently found in other components may not be fixed here: it's only maintained in compilable state. 
Recent finding revealed that its whole paradigm MAY be (not necessary is) broken in case (f.i.) of NonBlocking collectives followed by Blocking collectives: 
it causes functions to overlap which pet currently doesn't tolerate with. Maybe it will be fixed in the future in case there will be interest in this part from community. 


PET usage guideline:
usage: pet <options> 
Options:

Long format 
--options=value or (depending on format)
--options=comma_separated_values

short format 
-o value or 
-o comma_separated_values

Options list:
--verbose - produce debug output
--func-id-print - print func ids at the begining
--parallel-progress - enables parallel progress for messages in progress engines if not specified handling progress will be limited to MPI functons
--use-up - turns on unreliable protocol (usualy UD). In this case ack packets will be produced.
--input= (-i) - name of input file mask (tractor_t is the default)
--transport-file= (-t) - name of transport profile file (required)
--user-region-file= (-u) - name of user region file (optional)
--gather-transport-stat= (-g) write new transport profile at the end (useful for correction)
--transport-selection= (-s) [min,avg,max] select values from (transport) profile
--config-file (-c)  - specifies config file (where options may be specified instead of writing them into command line)
--function-split-file= (-f) - name of function split file (not used yet)
--CPI-scale=('a') mpi_scale[,user_region_scale] - CPI for mpi function and for user regions missing in user regiosn file
--working-dir= (-w) specifies working directory if omitted current directory will be used
--eager-thresholds=(-e) <smp_eager_threshold,netmod_eager_threshold>
--target-cpu-freq=freq specifies target CPU frequency,
--target-ppn=<ppn,numa_pp> specifies target Processes Per Node and Processes Per NUMA Node



