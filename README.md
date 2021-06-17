cbfuse
======

This is currently just a fun/academic/experimental project that I started to help me learn more about FUSE and to gain some first hand experience with libcouchbase (the C SDK for Couchbase) after seeing all the new features coming in [Couchbase Server 7.0](https://www.couchbase.com/products/server). The idea is to use a Couchbase Server as a distributed file store that presents itself as a local user based file system using [FUSE](https://github.com/libfuse/libfuse). However, I wasn't super focused on efficient file system design, have not put much thought into key access patterns, nor have I worried about distributed locks, etc. Those topics would have to be addressed for this to be a more robust and useful distributed filesystem.

If you're looking for an actual distributed file server built on top of Couchbase, check out [cbfs](https://github.com/couchbaselabs/cbfs) on [CouchbaseLabs](https://github.com/couchbaselabs) instead. 

### Implementation Notes:
- I am currently using the FUSE **high-level** operations to create a logical overlay of a filesystem.
- I have not fully tested FUSE in the normal **multi-threaded daemon** mode of operation (only tested with `-f -s` so far).
- All of the calls to Couchbase are currently **synchronous** and I haven't optimized batch calls or looked into transactions.
- Currently only developed and tested with **macOS** using `macFUSE` for convenience.
* Paths are currently limited to 250 characters (the length of a Couchbase key) but I have plans to expand that.
  * Here are some thoughts:
    1. Must try to take advantage of Couchbase keys for quick lookup (and future improvements I want to explore).
    1. Must only use more expensive operations/techniques when needed (e.g., when path is larger than 250 characters).
    1. Must support at least 4096 character upper limit (the current path limit for ext4 file systems).
    1. I want to avoid more time consuming lookup strategies that require multiple trips (e.g., path keys, collision documents).
    1. However, using a counter may be useful if the solution is fast and results in fewer calls and less complex keys.
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
  * REMINDER: This is all because I want to use the high-level operations which are heavily based on the `path` string. To avoid multiple calls, I need to obtain a unique key on the client without reverting to strategies that would require multiple trips. When using the high-level operations, this is true for all operations, but it's especially true for operations like `getattr` which are called very frequently. 

### General Development Notes
- Could be cleaner - this was just a quick mash up for experimentation and fun. I may improve it in the future if I have time or other ideas to explore.
- Tested primarily on **macOS Big Sur (11.4) (x86)**.
- I tried to keep the CMake config "clean" so it should only require a little TLC to build for other Unix based operating systems.
- FUSE on Windows is a different story and [Dokan](https://dokan-dev.github.io/) is probably a better approach (rewrite or using the FUSE wrapper utility).

### Development Build Environment Setup
- Intall cmake utility
  - `brew install cmake`
- Install Couchbase Server 7.0
  - https://www.couchbase.com/downloads
- Install Couchbase C library
  - `brew install libcouchbase`
  - IMPORTANT: You'll need v3.1.0 until the next 7.0 RC is released!! ([more info](https://issues.couchbase.com/browse/JSCBC-890?jql=text%20~%20%22LCB_ERR_KVENGINE_INVALID_PACKET%22))
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
- Setup Couchbase
  - Start the Couchbase server
  - Create a bucket (e.g., `cbfuse`)
  - Under the `_default` **Scope**, add the following **Collections**:
    - `stats` - _used for basic file stat attributes_
    - `blocks` - _used to store file data blocks_
    - `dentries` - _used to store directory entry info_
- Running a quick debug test
  - _This filesystem runs in the **foreground** and is **single-threaded**._
  - Mount the filesystem
    - `./cbfuse/cbfuse ~/cbfuse --cb_connect=couchbase://127.0.0.1/cbfuse --cb_username=raycardillo --cb_password=raycardillo`
  - Unmount the filesystem
    - `umount cbfuse`
