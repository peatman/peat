# msvc - Windows build solution

Part of the PEAT.

## What it does

Visual Studio 2022+ solution (`mpi.tool.sln`) providing the Windows/MSVC counterpart to the Linux `Makefile`/`Build.sh` scripts used by the individual components. Builds `pet`, `mat`, `urpu` and `ctt` as x64 console applications from their respective `../<component>/src` sources.

Each project ships two configuration pairs:
- `Debug`/`Release` - plain serial build (matches the Linux `TARGET=serial` build).
- `Debug.MPI`/`Release.MPI` - MPI-parallelised build (`PEAT_MPI` preprocessor define), matching the Linux targets. tested only with  Intel MPI from Intel OneAPI.
- pet can only be compiled with compiler which supports OpenMP 3.0 or later (Intel oneAPI supports 5.2) which rules out Microsoft one.

## Layout

- `mpi.tool.sln` - the Visual Studio solution, references all four project files.
- `pet.vcxproj`, `mat.vcxproj`, `urpu.vcxproj`, `ctt.vcxproj` - per-component project files.
 