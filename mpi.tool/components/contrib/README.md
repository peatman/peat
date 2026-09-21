# contrib

Third-party components used by peat.

This directory contains codes that complements `peat` but is not part of `peat` itself. 
Code in this directory may have a different licence and copyright than `peat` and should be explained in each subdirectory.

## Contents

- `getopt/` - Windows-compatible `getopt()` and `getopt_long()` implementation for command-line option parsing. See [getopt/README.md](getopt/README.md).
- `zlib/` - zlib compression library source used by tools that need compression support.  See [zlib/README](zlib/README). Modified to work with more than 8192 simultaneously opened files on windows.
- `psm3/` - psm3 header to compile fab_layer shim library.
- `libfabric/` - patched version of libfabric to trace ofi calls (available only with patched MPIs).

These directories contain upstream or separately maintained code. Keep local changes focused on integration requirements and preserve the original component documentation.


# configure and build zlib in windows
# assuming QT_DIR=C:/Qt

"${QT_DIR}/Tools/CMake_64/bin/cmake.exe" \
    -S ${PEAT_TOP}/mpi.tool/components/contrib/zlib \
    -B ${PEAT_TOP}/mpi.tool/components/contrib/zlib/zlib-mingw-build \
    -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${QT_DIR}/Tools/llvm-mingw1706_64/bin/clang.exe \
    -DCMAKE_MAKE_PROGRAM=${QT_DIR}/Tools/llvm-mingw1706_64/bin/mingw32-make.exe \
    -DCMAKE_INSTALL_PREFIX=${PEAT_TOP}/mpi.tool/components/contrib/zlib/zlib/install/mingw \
    -DZLIB_BUILD_TESTING=OFF \
    -DZLIB_BUILD_SHARED=ON \
    -DZLIB_BUILD_STATIC=ON \
    -DZLIB_INSTALL=ON

# 2. build
"${QT_DIR}/Tools/CMake_64/bin/cmake.exe" --build C:/Users/mgs/zlib-mingw-build -j8

# 3. install into contrib/zlib/install/mingw
"${QT_DIR}/Tools/CMake_64/bin/cmake.exe" --install C:/Users/mgs/zlib-mingw-build

# for MSVC build use built-in MSVC cmake. 
# Install it into contrib/zlib/install/msvc.