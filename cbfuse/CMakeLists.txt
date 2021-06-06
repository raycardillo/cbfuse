# Good reference for a similar CMake configuration:
# https://github.com/vgough/encfs/blob/master/CMakeLists.txt

# Find FUSE
find_package(FUSE 2.9 REQUIRED)
add_definitions(-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=29)

# Find Couchbase
find_package(COUCHBASE 3.1 REQUIRED)

# Find xxHash
find_package(XXHASH 0.8 REQUIRED)

# Find cJSON
find_package(CJSON 1.7.14 REQUIRED)

add_executable(cbfuse
  common.c
  sync_get.c
  sync_store.c
  attribs.c
  dentries.c
  cbfuse.c
)

configure_file(cbfuse.h.in cbfuse.h)

target_include_directories(cbfuse
  PUBLIC
    "${PROJECT_BINARY_DIR}"
  PRIVATE
    "${PROJECT_SOURCE_DIR}/contrib"
    "${FUSE_INCLUDE_DIRS}"
    "${COUCHBASE_INCLUDE_DIRS}"
    "${CJSON_INCLUDE_DIRS}"
    "${XXHASH_INCLUDE_DIRS}"
)

target_link_libraries(cbfuse
  PRIVATE
    CJSON::CJSON
    XXHASH::XXHASH
    FUSE::FUSE
    COUCHBASE::COUCHBASE
)
