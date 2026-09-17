# msvc - Windows build solution

Part of the PEAT.

## What it does

Visual Studio 2022+ solution (`mpi.tool.sln`) providing the Windows/MSVC counterpart to the Linux `Makefile`/`Build.sh` scripts used by the individual components. Builds `pet`, `mat`, `urpu` and `ctt` as x64 console applications from their respective `../<component>/src` sources.

Each project ships two configuration pairs:
- `Debug`/`Release` - plain serial build (matches the Linux `TARGET=serial` build).
- `Debug.MPI`/`Release.MPI` - MPI-parallelized build (`PEAT_MPI` preprocessor define), matching the Linux targets. tested only with  Intel MPI from Intel OneAPI.

## Layout

- `mpi.tool.sln` - the Visual Studio solution, references all four project files.
- `mat.vcxproj`,  `ctt.vcxproj` - per-component project files.
 