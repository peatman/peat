# Building tractor

- **Linux x86_64 only** 

## 1. What gets built

`src/Makefile` produces:
- `libtractor<suffix>.so` - the interception library, installed to `mpi.tool/lib/<COMPILER>/` (`gnu`, `intel`, `nvc` or `clang`)
- `ptpd<suffix>` - the PTP daemon binary, installed to `mpi.tool/bin/<COMPILER>/`
- optionally `libtractor_transport_{psm2,psm3,ucx,ofi}<suffix>.so` shims (`TARGET=transport_layers`)

## 2. Platform & toolchain prerequisites

- One of:
  - **GNU**: `gcc`/`g++`/`gfortran`. [env.sh](../../scripts/env/env.sh) expects a
    self-built toolchain at `$HOME/gcc/<version>/bin/gccvars.sh` (default version
    `14.1.0`) - if you use your distro's gcc instead, just make sure
    `CC=gcc CXX=g++ FC=gfortran` are on `PATH` and skip/edit that block.
  - **Intel oneAPI**: `icx`/`icpx`/`ifx` via `/opt/intel/oneapi/<version>/setvars.sh`.
  - **NVHPC** (`nvc`/`nvc++`/`nvfortran`): needed to run OpenACC code on an NVIDIA GPU. The CUDA toolkit it compiles with must support
    the architecture of the GPU (newer toolkits drop old architectures); select a suitable one with `compiler=nvc:<CUDA version>`.
  - **Clang/LLVM** (`clang`/`clang++`): the system-wide installation, one version. There is no Fortran front end, `gfortran`
    is used for Fortran. Clang has no OpenACC runtime or `acc_prof.h`, so OpenACC is never built with it.
- **Perl** - required at build time to run the code generators (`mpi_layer.pl`,
  `../../common/mpi_func_ids.pl`, `func_split`, etc.). The generators need the `Switch` module, which is no longer
  part of core Perl: `libswitch-perl` (Debian/Ubuntu), `perl-Switch` (Fedora/RHEL), or `cpan Switch`.
- **`omp-tools.h`** (OpenMP Tools/OMPT header) - needed with `OMPT=on` (the default). GCC's libgomp does not ship it;
  it comes with LLVM's OpenMP (`openmp` on Arch, `libomp-dev` on Debian/Ubuntu, under `/usr/lib/llvm-*`). If it is not
  on the default include path, point `CPATH` at a directory containing it, or build with `OMPT=off`.
- **GNU Make**, `cpp` (comes with gcc), `bash`/`sh`.

## 3. Required third-party libraries

| Library | Why | Notes |
|---|---|---|
| **zlib** (`-lz`, `zlib.h`) | trace compression | system `zlib1g-dev`/`zlib-devel`, or build [../contrib/zlib](../contrib/zlib) yourself |
| **hwloc** (headers + `libhwloc`) | node/CPU topology capture | `Makefile` references `$(HWLOC_PATH)/include` and `$(HWLOC_PATH)/lib` 
| **An MPI implementation** | tractor links against `MPICC`/`MPICXX`/`MPIFC` | see section 5 below |

`../contrib/libfabric` and `../contrib/psm3` in this repo are vendored **sources for building those libraries yourself**, not prebuilt - see section 4.

## 4. Optional libraries (only needed for specific Build.sh switches)

| Feature flag | Extra dependency | Notes |
|---|---|---|
| `PARSE_GPU=on` | OpenCL (`CL/cl.h`, `-lOpenCL`), Level Zero (`level_zero/ze_api.h`, `-lze_loader`), OpenACC (`acc_prof.h`) | GPU call interception; each API is only built when it is found, see section 8 |
| `HWCOUNTERS=RDPMC` | Linux kernel headers (`linux/perf_event.h`) | no PAPI needed - uses `perf_event_open` directly |
| `TRANSPORT=on` / `TARGET=transport_layers` | libfabric, PSM2, PSM3, UCX headers (+libs for the standalone shims) | PSM2/PSM3/UCX/OFI calls are `dlsym(RTLD_NEXT,...)`-intercepted, so only headers are strictly needed to build; the real `.so` must be on `LD_LIBRARY_PATH` at run time. Build [../contrib/libfabric](../contrib/libfabric) yourself (or use system libfabric); headers for [../contrib/psm3](../contrib/psm3) are bundled but the actual PSM3/UCX/PSM2 runtime libs are not - install separately. |
| `TARGET=mvapich2` (verbs/smpi) | `libibverbs`-dev | `VERBSPATH` defaults to `/usr` in `Build.sh`; install e.g. `libibverbs-dev`/`rdma-core-devel` |

## 5. MPI implementation - build first

`tractor` is built *against* one MPI at a time
(`TARGET=impi|mpich3|mpich4|mpich5|mvapich2|mvapich4|openmpi`). Build/install the MPI(s) you need before building tractor:

| MPI | Build script | Notes |
|---|---|---|
| Intel MPI | n/a (use oneAPI installer) | `TARGET=impi`; closed source, so there is no patch (Fortran calls: see "Fortran" below) |
| Open MPI 5.0.10 | build/install normally (no build script here) | `TARGET=openmpi`; patches in [../../patches/openmpi-5.0.10](../../patches/openmpi-5.0.10): `openmpi-5.0.10.patch` (trace hooks, for the "engineering" build), `openmpi-5.0.10.fortran.patch` (Fortran interception), `openmpi-5.0.10.ze.patch` (Level Zero). Untested, experimental. The engineering features (`PARSE_MPI_INTERNAL`, `PARSE_COLT`) need the patched MPI; see [../../patches/README.md](../../patches/README.md) |
| MPICH 4.3.2 | [../../scripts/mpich-4.3.2/Build-{ofi,tcp,ucx}-m.sh](../../scripts/mpich-4.3.2) | `TARGET=mpich4`; patches in [../../patches/mpich-4.3.2](../../patches/mpich-4.3.2): `mpich-4.3.2.patch` for the "engineering" (`PARSE_COLT`) build, `mpich-4.3.2.fortran.patch` for Fortran interception |
| MPICH 5.0.1 / 5.0.2 | [../../scripts/mpich-5.0.1](../../scripts/mpich-5.0.1) | `TARGET=mpich5`; patches in [../../patches/mpich-5.0.2](../../patches/mpich-5.0.2) ([../../patches/mpich-5.0.1](../../patches/mpich-5.0.1) is deprecated): `.patch` for the "engineering" build, `.fortran.patch` for Fortran interception |
| MVAPICH2 2.3.5 | [../../scripts/mvapich-2.3.5/Build-ibv-m.sh](../../scripts/mvapich-2.3.5/Build-ibv-m.sh) | `TARGET=mvapich2`; patch [../../patches/mvapich-2.3.5/mvapich2-2.3.5.patch](../../patches/mvapich-2.3.5/mvapich2-2.3.5.patch) (+ `src/` verbs interceptor sources) needed for verbs/smpi transport-API interception; also `mvapich2-2.3.5.fortran.patch` (Fortran) and `mvapich2-2.3.5.transport.patch` |
| MVAPICH 4.1 | [../../scripts/mvapich-4.1](../../scripts/mvapich-4.1) | `TARGET=mvapich4`; patches in [../../patches/mvapich-4.1](../../patches/mvapich-4.1): `mvapich-4.1.patch`, `mvapich-4.1.fortran.patch` (Fortran interception) |

These `Build-*.sh` scripts are examples - edit `--prefix=$HOME/<mpi>/<version>` paths for your system.

The patches are independent (`<name>.patch` trace hooks, `<name>.fortran.patch` Fortran, and for some MPIs `.transport.patch`/`.autoconf.patch`); apply
them with `patch -p1` from the top of the MPI source tree, in any order. [../../patches/README.md](../../patches/README.md) lists them per MPI.

**Fortran:** apply `<name>.fortran.patch` to every MPI you build, otherwise Fortran calls are not intercepted. The patched Fortran
wrappers call the C `MPI_*` entry points (which tractor wraps) instead of `PMPI_*` only when the redirect variable is set to anything
except an empty string or `0`, so export it when tracing Fortran programs: `MPICH_FORTRAN_MPI_REDIRECT` (MPICH),
`MV2_FORTRAN_MPI_REDIRECT` (MVAPICH2), `MVP_FORTRAN_MPI_REDIRECT` (MVAPICH 4), `OMPI_FORTRAN_MPI_REDIRECT` (Open MPI). Intel MPI cannot be
patched; see [README.md](README.md) for the custom `libmpifort` used with it.

**Open MPI engineering builds**: configure with `--with-ofi=$HOME/libfabric-eng --with-hwloc=internal --with-libevent=internal`
(and the other options of your usual configure). Without the `internal` switches configure picks up the system hwloc/libevent, puts
`-Wl,-rpath,/usr/lib` first in the RUNPATH and the library then loads the *system* `libopen-pal`/`libpmix`/`libfabric` (an
installed distro Open MPI) instead of its own. Check with `readelf -d libmpi.so | grep RUNPATH` and
`env -i ldd libmpi.so`. Do not use one source tree for two compilers at the same time, run `make distclean` before re-configuring
it, and make the build script stop when `./configure` fails: otherwise `make` and `make install` go on with the configuration
left from the previous run (and install it).

### mpivars.sh

`env.sh` sources `$HOMEINIT/<mpich|mvapich>/$mpi_version/bin/mpivars.sh` after
building each MPI. **This file is not generated by MPICH/MVAPICH's `make install`** - it's a generic template at
[../../scripts/env/mpivars.sh](../../scripts/env/mpivars.sh) that must be copied manually into each MPI install's `bin/` directory after installing:

```sh
cp mpi.tool/scripts/env/mpivars.sh $HOME/mpich/4.3.2g-mt-ofi/bin/mpivars.sh
cp mpi.tool/scripts/env/mpivars.sh $HOME/mvapich/4.1g-mt-ofi/bin/mpivars.sh
# one copy per installed MPI prefix/version
```

It derives `MPI_ROOT` from its own location (`bin/..`) and scrapes
`CC=`/`CXX=`/`FC=` out of the installed `mpicc`/`mpicxx`/`mpif90` wrapper
scripts, so it must live inside that specific MPI's `bin/` directory.

Intel MPI and Open MPI don't need this: Intel MPI ships its own `vars.sh`
(sourced directly by `env.sh`), and Open MPI's `PATH`/`LD_LIBRARY_PATH` are set
directly in `env.sh`.

## 6. Environment setup (env.sh)

`Build.sh` does `. env.sh` from the $PATH.  Copy or symlink it in before building.

`env.sh` expects (adjust or export overrides before running `Build.sh`):
- `HOMEINIT`/`WORKINIT` (default `$HOME`) - root under which it looks for   `gcc/<ver>`, `mpich/<ver>`, `mvapich/<ver>`, `openmpi/<ver>`, `libfabric`,
  `ucx`, `psm2`, `psm3`.
- `compiler=<oneAPI:VER|gcc|nvc[:VER[:VER]]|clang>` and `mpi_type=<impi|openmpi:VER|mvapich:VER|mpich:VER>` - normally set as
  env vars right before invoking `Build.sh` (see `BuildAllg.sh`/`BuildAlli.sh`/`BuildAlln.sh`/`BuildAllc.sh` for examples).
  - `oneAPI:<version>` sources that oneAPI's `setvars.sh`; `gcc` is the default (anything else); `clang` is handled like gcc
    (system-wide, one version).
  - `nvc` puts the NVHPC compilers in `PATH`. Each version is looked up as an NVHPC release
    (`$NVHPC_ROOT_PATH/Linux_x86_64/<version>`, default root `/opt/nvidia/hpc_sdk`) or, if there is none, as a CUDA toolkit
    (`/opt/cuda-<version>`, `/usr/local/cuda-<version>`; exported as `NVHPC_CUDA_HOME`): `nvc`, `nvc:<CUDA version>`,
    `nvc:<NVHPC version>`, `nvc:<NVHPC version>:<CUDA version>`. No NVHPC version means the newest installed.
  - For `nvc` and `clang`, `env.sh` also sets `MPICH_CC/CXX/FC`, `OMPI_CC/CXX/FC` and `I_MPI_CC/CXX/FC`, so the MPI wrappers
    (`mpicc` ...) and `mpivars.sh` use the selected compiler; `CC`/`CXX`/`FC` follow.
- `libfabric_eng='-eng'` - selects `$HOME/libfabric-eng` (the engineering libfabric, `OFIPATH`) instead of `$HOME/libfabric`.
- **`HWLOC_PATH`** - not set by `env.sh`; export it yourself.

`Build.sh` also hardcodes `PSM2PATH=$HOME/psm2`, `PSM3PATH=$HOME/psm3`, `UCXPATH=$HOME/ucx`, `OFI_PATH=$HOME/libfabric[-eng]`, `VERBSPATH=/usr` -
place those libraries there, or edit `Build.sh`.

## 7. Build steps

```sh
export HWLOC_PATH=/usr                         # or your hwloc prefix

# Option A: adapt and run the example "build everything" scripts
./BuildAllg.sh     # GNU toolchain, builds several TARGETs in parallel
./BuildAlli.sh     # Intel oneAPI toolchain
./BuildAlln.sh     # NVHPC toolchain
./BuildAllc.sh     # clang

# Option B: build one variant directly
#              mpi_type[:version]   TARGET   MEMPROFILE DIAG PARSE_GPU HWCOUNTERS MPIBUFFERS VIRT2PHYS
compiler=gcc COMPILER=gnu ./Build.sh mpich:4.3.2g-mt-ofi mpich4 off off off off off off
```

Output: `mpi.tool/lib/<COMPILER>/libtractor<suffix>.so` and `mpi.tool/bin/<COMPILER>/ptpd<suffix>`.

The `BuildAll*.sh` scripts are the same list of builds for each toolchain (GNU `g` MPIs; the Intel one uses the `x` MPIs);
a `.gpu` in the file name means `PARSE_GPU=on`, `.eng` the engineering MPI, `_dbg` a `DIAG=on` build.
`Build.sh` runs several `make` invocations that all fail the same way when something is wrong, check `Build.<COMPILER>.log`.

## 8. Key Makefile switches

Pass as `make` vars, or positional args to `Build.sh`.

| Switch | Values | Meaning |
|---|---|---|
| `TARGET` | `impi*, mpich3, mpich4, mpich5, mvapich2, mvapich4, openmpi, transport_layers` | MPI flavor |
| `COMPILER` | `gnu*, intel, nvc, clang` | toolchain (the directory name under `lib/` and `bin/`). `nvc` and `clang` take the GCC options; with `nvc` the `.c` sources are built with `nvc++` (done by the Makefile) |
| `TRANSPORT` | `on, off*` (mvapich2 also: `PSM2/PSM3/VERBS/UCX/OFI`, legacy) | transport-API interception |
| `PTPD` | `on*, off` | build PTP daemon support |
| `OMPT` | `on*, off` | OpenMP interception |
| `DIAG` | `off*, on` | debug build (`-O0`, `_dbg` suffix) |
| `PARSE_GPU` | `off*, on` | GPU call interception: OpenCL, Level Zero and OpenACC (profiling interface), each only when found - see "GPU APIs" below |
| `OCL_PARSE`, `LZE_PARSE`, `ACC_PARSE` | `auto*, on, off` | with `PARSE_GPU=on`: build OpenCL / Level Zero / OpenACC interception (`auto` when found, `on` = missing is an error) |
| `PARSE_COLT` | `off*, on` | collective-timeline engine ("engineering" build, needs patched MPI) |
| `HWCOUNTERS` | `off*, RDPMC` | hardware counters via `perf_event_open` |
| `MEMPROFILE` | `off*, ...` | allocation profiling |
| `VIRT2PHYS` | `off*, on` | virtual->physical address resolution |

Run `make install TARGET=... DIAG=on ...` directly from `src/` if you don't
want to go through `Build.sh`'s temp-directory copy dance.

### Compilers with CMake

CMake detects the compiler family from `CMAKE_CXX_COMPILER_ID` (`GNU` - gnu, `Intel`/`IntelLLVM` - intel, `NVHPC`/`PGI` - nvc,
`Clang` - clang) and installs to `lib/<family>`; `-DTRACTOR_COMPILER=gnu|intel|nvc|clang` overrides it. Select the toolchain with
`compiler=<name>` before sourcing `env.sh` (e.g. `compiler=nvc:<CUDA version>`, `compiler=clang`) and pass the MPI wrappers
(`-DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX`); `env.sh` points the wrappers to the selected compiler.

### GPU APIs

With `PARSE_GPU=on` (CMake: `-DGPU=ON`) each GPU API is looked for with the compiler that builds the library, and only the ones
that are found are built; the choice is printed as `tractor: GPU parsing: OpenCL=.. LevelZero=.. OpenACC=..`. The headers
count as present when the compiler can include them (also through `CPATH`, the oneAPI `${CMPLR_ROOT}/include` for Intel, ...),
the libraries must be linkable. Each API has its own switch (CMake spells the values in upper case):

| Switch | Values | Needs |
|---|---|---|
| `OCL_PARSE` | `AUTO*, ON, OFF` | `CL/cl.h` and `libOpenCL` |
| `LZE_PARSE` | `AUTO*, ON, OFF` | `level_zero/ze_api.h` and `libze_loader` |
| `ACC_PARSE` | `AUTO*, ON, OFF` | `acc_prof.h` (GCC >= 9 or NVHPC); always off with the Intel compilers |

`AUTO` enables an API when it is found, `ON` makes a missing one an error, `OFF` skips it. If nothing is found the
GPU parsing is switched off with a warning. The `Makefile` uses the same names (`make PARSE_GPU=on OCL_PARSE=off ...`) and
probes by compiling a one-line program. OpenACC follows `PARSE_GPU`: it is never built with the Intel compilers (no `acc_prof.h`),
and not with clang either (the probe fails); `ACC=` cannot be set by hand.

### libfabric and MPI editions

The engineering libfabric exports functions (`fi_close`, ...) that the stock one only has as `static inline` in its headers,
and an engineering `libmpi` references them as undefined symbols. Both `Makefile` and CMake read the editions with `nm` and
stop with a "version mismatch" error when the MPI and `$OFIPATH` (system libfabric if unset) do not match. An MPI also counts as
engineering when it defines the patched `mpi_internal_comm_create`. The check is skipped for an MPI that does not link
libfabric (stock Open MPI, MVAPICH 2 verbs). `TARGET=transport_layers` needs the engineering libfabric (the shims redefine those
functions). The CMake check runs before `find_package(MPI)`, which is why the CMake call has to use the MPI compiler wrappers
(`-DCMAKE_C_COMPILER=$MPICC -DCMAKE_CXX_COMPILER=$MPICXX`, `Build-cmake.sh` does that).

## 9. Sanity check

After building, verify with `ldd` that `libtractor*.so` resolves
`libhwloc`/`libz`/MPI runtime, then smoke-test:

```sh
LD_PRELOAD=$PEAT_ROOT/mpi.tool/lib/<COMPILER>/libtractor<suffix>.so mpirun -n 2 ./your_mpi_app
```

The `com` file written by the library starts with `PEAT TRACTOR V<major>.<minor>. Build Date <date>(<compile date>) by <compiler>`
(compiler name and version), which tells which compiler built the library that was preloaded.

### Tests

- `tests/func-classes/run.sh [p2p|coll|comm|rma|fio|all] [iterations] [ranks]` - the MPI function classes under tractor. The
  toolchain is selected with `TOOLCHAIN=intel|gcc|nvc|clang` (default `intel`) and the libraries come from `lib/<toolchain>`
  (`TRACTOR_LIBDIR=<dir>` overrides, `mpi_type=` replaces the MPICH); the test programs are rebuilt when the toolchain changes.
  The function sets collected are the same for all four toolchains; only the counts of polling calls (`MPI_Test*`, `MPI_Iprobe`,
  ...) vary with timing.
- `tests/openacc/run.sh` - OpenACC under tractor with `gcc -fopenacc` (runs on the host), plus `acc_sim_test`, which raises the
  OpenACC device events itself. `tests/openacc/run_nvc.sh` builds `acc_test` with nvc (`NVC_COMPILER` selects the `compiler=` value, see the script for the default) and runs
  it on the GPU. Both default to `lib/gnu/libtractor.mpich5.gpu.so` (`TRACTOR_LIB=<path>` selects another library; the OpenACC
  support needs a `PARSE_GPU=on` build, `.gpu` in the name).

See [README.md](README.md) sections 1-3 for the full runtime env-var reference.
