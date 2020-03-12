# BWAP
This is the bwap-lib module, a library for dynamic page placement in NUMA nodes.

## What does it do?
The library will move pages between the NUMA nodes during your program's
execution in order to speed it up.

A simple explanation of the library is as follows:
1. **Place application pages according to a weighted interleaving strategy** by default (optimizing for bandwidth)
2. **Analyze the program's memory mapping** via the `/proc/self/maps` file. This includes the .data and BSS
segments, as well as dynamic memory mappings.
3. Use Hardware Performance Counters to **monitor the average resource stall rate**
4. **Move some pages from remote (non-worker) NUMA nodes into local (worker) NUMA nodes** -- this reduces the bandwidth but also places pages closer to where they are requested, which may result in a performance increase if latency is the
issue when accessing memory.
5. If we see a **drop in the average resource stall rate**, we **go back to step 4**. 
A lower stall rate means the CPU is less time idle waiting for a resource.
We assume that the loss in memory bandwidth is compensated with a lower access
latency.

## How do I use it?

### Pre-requisites

- `cmake` -- version 3.5 or newer
- A modern C++ compiler
  - We have used `gcc` 8 during our testing
  - `gcc` from version 6 compiles the program, but binaries haven't been tested
  - `clang` from version 6 compiles the program, but binaries haven't been tested
- `libnuma-dev` -- for the `numa.h` and `libnuma.h` headers
- `likwid` library for Performance monitoring: https://github.com/RRZE-HPC/likwid
- `libboost-all-dev` -- boost library

### Compiling

1. `cmake .` to generate a Makefile
2. `make` to build the library and tests

### Using

You can opt to use the library with or without modifying your program.

#### Without modifying the program
Preload the library to run alongside your program via `LD_PRELOAD`:

```LD_PRELOAD=/path/to/libunstickymem.so ./myProgram```

#### With program modification
1. Include the library header in your program:
  - `#include <unstickymem/unstickymem.h>`
2. Call at least one function (otherwise `gcc` won't bother to actually include
it with your executable)
  - See the available functions in [`unstickymem/unstickymem.h`](https://github.com/gureya/bwap/blob/master/include/unstickymem/unstickymem.h)
3. Compile your program

#### System installation
You can make the library generally available to any user in the system.
- `make install` installs the library and required header files in your system.

Run `make uninstall` to undo the effects of `make install`.

## Library Options
There are a few options that can change the behavior of the library.
These are specified via environment variables. check them here: https://github.com/gureya/bwap/blob/master/unstickymem.ini
More information on this is coming soon

## A tour of the source tree
- We are using the [`CMake`](https://cmake.org) build system for this library.
- `src` contains all source files
- `include` contains all header files. Each library uses its own subfolder in
order to reduce collisions when installed in a system (following the [Google
C++ Style Guide](https://google.github.io/styleguide/cppguide.html)).
- A few example programs are included in the `test` subfolder.

### Logical Components
- The higher-level logic is found in [`unstickymem.cpp`](https://github.com/gureya/bwap/blob/master/src/unstickymem/unstickymem.cpp).
- The logic to view/parse/modify the process memory map is in [`MemoryMap.cpp`] and [`MemorySegment.cpp`](https://github.com/gureya/bwap/tree/master/src/unstickymem/memory).
- The logic to deal with hardware performance counters is in [`PerformanceCounters.cpp`](https://github.com/gureya/bwap/blob/master/src/unstickymem/PerformanceCounters.cpp)
- Utility functions to simplify page placement and migration are in [`PagePlacement.cpp`](https://github.com/gureya/bwap/blob/master/src/unstickymem/PagePlacement.cpp)

## Issues / Feature Requests
If you found a bug or would like a feature added, please
[file a new Issue](https://github.com/gureya/bwap/issues)!

Check for issues with the
[`help-wanted`](https://github.com/gureya/bwap/issues)
tag -- these are usually ideal as first
issues or where development has been hampered.

For more information and results see our original paper: **Bandwidth-Aware Page Placement in NUMA** (https://arxiv.org/abs/2003.03304),
Accepted at 34th IEEE International Parallel & Distributed Processing Symposium (IPDPS), 2020
