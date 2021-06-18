# 
# cbfuse implements a FUSE file-system using Couchbase as the data store.
# Copyright (c) 2021 Raymond Cardillo
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

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
  sync_remove.c
  stats.c
  dentries.c
  data.c
  cbfuse.c
)

configure_file(cbfuse.h.in cbfuse.h)

target_include_directories(cbfuse
  PUBLIC
    "${PROJECT_BINARY_DIR}"
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
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
