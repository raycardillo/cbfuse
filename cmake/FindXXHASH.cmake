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

# Try to find the XXHASH library and define:
#
# XXHASH_FOUND        - True if library was found.
# XXHASH_INCLUDE_DIRS - Include directories.
# XXHASH_LIBRARIES    - Libraries.
#
# XXHASH::XXHASH

# check if already in cache, be silent
if (XXHASH_INCLUDE_DIRS AND XXHASH_LIBRARIES)
    SET (XXHASH_FIND_QUIETLY TRUE)
endif ()

if (APPLE)
    set (XXHASH_NAMES libxxhash.dylib xxhash xxHash)
else ()
    set (XXHASH_NAMES libxxhash.a xxhash xxHash)
endif ()

# find include
find_path (
    XXHASH_INCLUDE_DIRS xxhash.h
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES include
    REQUIRED)

# find lib
find_library (
    XXHASH_LIBRARIES
    NAMES ${XXHASH_NAMES}
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES lib
    REQUIRED)

include ("FindPackageHandleStandardArgs")
find_package_handle_standard_args (
    "XXHASH" DEFAULT_MSG
    XXHASH_INCLUDE_DIRS XXHASH_LIBRARIES)

mark_as_advanced (XXHASH_INCLUDE_DIRS XXHASH_LIBRARIES)

if (XXHASH_FOUND AND NOT TARGET XXHASH::XXHASH)
  add_library(XXHASH::XXHASH STATIC IMPORTED)
  set_target_properties(XXHASH::XXHASH PROPERTIES
    IMPORTED_LOCATION "${XXHASH_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${XXHASH_INCLUDE_DIRS}")
  target_compile_definitions(XXHASH::XXHASH INTERFACE XXHASH_FOUND)
else()
  message(WARNING "Notice: XXHASH not found, no XXHASH support")
  add_library(XXHASH::XXHASH INTERFACE IMPORTED)
endif()