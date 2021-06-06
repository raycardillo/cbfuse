cbfuse
======

This is currently just an academic/experimental project that I started for fun and to help me learn more about FUSE and libcouchbase (the C SDK for Couchbase) after seeing all the new features coming in [Couchbase Server 7.0](https://www.couchbase.com/products/server). The idea is to use a Couchbase Server as a distributed file store that presents itself as a local user based file system using [FUSE](https://github.com/libfuse/libfuse).

If you're looking for an actual distributed file server built on top of Couchbase, check out [cbfs](https://github.com/couchbaselabs/cbfs) on [CouchbaseLabs](https://github.com/couchbaselabs) instead. 

### Implementation Notes:
- I am using the FUSE high-level operations to create a logical mapping of a filesystem.
- Currently only developed and tested on a mac using `macFUSE` for convenience right now.
* Paths are currently limited to 250 characters (the length of a Couchbase key) but I have plans to expand that.
  * Here are my thoughts:
    1. Must try to take advantage of Couchbase keys for quick lookup (and future improvements I want to explore).
    1. Must only use more expensive operations/techniques when needed (e.g., when path is larger than 250 characters).
    1. Must support at least 4096 character upper limit (the current path limit for ext4 file systems).
    1. I want to avoid more time consuming lookup strategies that require multiple trips (e.g., path keys, collision documents).
  * Current idea:
    - When path <= 250 characters:
      - Just use it because it's already unique.
    - When path > 250 characters:
      - XXH128 hash is performed over entire path and converted to a 22 character Base64 string.
      - The next 228 characters are samples to help add to the unique key property.
      - From a `path` of size `n` (where `n > 250`) and starting at [0] the samples are taken from:
        - 30 chars starting at: `[1]`
        - 48 chars starting at: `[n\*0.25]`
        - 50 chars starting at: `[n\*0.50]`
        - 50 chars starting at: `[n\*0.75]`
        - 50 chars starting at: `[n-51]`
      - Note that this strategy scales to try to find unique strings throughout. This is important because some storage patterns may have common sub-structures that are similar with unique paths earlier in the string (or visa-versa).
      - XXH128 itself has practically zero chance of collision (see: https://github.com/Cyan4973/xxHash/wiki/Collision-ratio-comparison).
      - The combination of XXH128 plus these character samples, with paths up to 4096, bounds the limits fairly well.
      - Collision should be impossible, but I'll leave the math to prove it as an exercise for the theoretical Computer Scientists.
  * REMINDER: This is all because I want to use the high-level operations which are heavily based on the `path` string. I need to be able to compute the unique key on the client without reverting to strategies that would require multiple trips. When using the high-level operations, this is true for all operations, but it's especially true for operations like `getattr` which are called very frequently. 

### General Development Notes
- Could be cleaner - this was just a quick mash up for experimentation and fun. I may improve it in the future if I have time or other ideas to explore.
- Tested primarily on **macOS Big Sur (11.4) (x86)**.
- I tried to keep things "clean" so it should only require a little TLC to build for other Unix based operating systems.
- FUSE on Windows is a different story and [Dokan](https://dokan-dev.github.io/) is probably a better approach (rewrite or using the FUSE wrapper utility).

### Development Build Environment Setup
- Intall cmake utility
  - `brew install cmake`
- Install Couchbase Server 7.0
  - https://www.couchbase.com/downloads
- Install Couchbase C library
  - `brew install libcouchbase`
  - IMPORTANT: You'll need v3.1.0 until the next 7.0 RC is released!!
  - I installed v3.1.0 by downloading source and doing a manual build.
- Install FUSE
  - `brew install macfuse`
  - Tested with macfuse (v4.1.2) == FUSE (v2.9)
- Install cJSON
  - `brew install cjson`
  - Tested with v1.7.14
- Install xxHash 
  - `brew install xxhash`
  - Tested with 0.8.0
- Install Visual Studio Code (optional)
  - A decent general purpose IDE with lots of useful extensions.
  - https://code.visualstudio.com/

### Getting Started
- Build cbfuse
  -  _see environment setup instructions above_
  - `mkdir build; cd build`
  - `cmake ..`
  - `cmake --build . --config Release`
- Running a quick debug test
  - `./cbfuse/cbfuse ~/cbfuse -f -s`
  - _note that this is running in the foreground and single threaded_
