# contrib

Third-party components used by peat.

This directory contains codes that complements `peat` but is not part of `peat` itself. 
Code in this directory may have a different licence and copyright than `peat` and should be explained in each subdirectory.

## Contents

- `getopt/` - Windows-compatible `getopt()` and `getopt_long()` implementation for command-line option parsing. See [getopt/README.md](getopt/README.md).
- `zlib/` - zlib compression library source used by tools that need compression support.  See [zlib/README](zlib/README). Modified to work with more than 8192 simultaneously opened files on windows.

These directories contain upstream or separately maintained code. Keep local changes focused on integration requirements and preserve the original component documentation.
