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

# Try to find the FUSE library and define:
#
# FUSE_FOUND        - True if library was found.
# FUSE_INCLUDE_DIRS - Include directories.
# FUSE_LIBRARIES    - Libraries.
#
# FUSE::FUSE

# check if already in cache, be silent
if (FUSE_INCLUDE_DIRS AND FUSE_LIBRARIES)
    SET (FUSE_FIND_QUIETLY TRUE)
endif ()

if (APPLE)
    set (FUSE_NAMES libosxfuse.dylib fuse)
    set (FUSE_SUFFIXES osxfuse fuse)
else ()
    set (FUSE_NAMES fuse refuse)
    set (FUSE_SUFFIXES fuse refuse)
endif ()

# find include
find_path (
    FUSE_INCLUDE_DIRS fuse.h
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES include ${FUSE_SUFFIXES}
    REQUIRED)

# find lib
find_library (
    FUSE_LIBRARIES
    NAMES ${FUSE_NAMES}
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES lib
    REQUIRED)

include ("FindPackageHandleStandardArgs")
find_package_handle_standard_args (
    "FUSE" DEFAULT_MSG
    FUSE_INCLUDE_DIRS FUSE_LIBRARIES)

mark_as_advanced (FUSE_INCLUDE_DIRS FUSE_LIBRARIES)

if (FUSE_FOUND AND NOT TARGET FUSE::FUSE)
  add_library(FUSE::FUSE STATIC IMPORTED)
  set_target_properties(FUSE::FUSE PROPERTIES
    IMPORTED_LOCATION "${FUSE_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${FUSE_INCLUDE_DIRS}")
  target_compile_definitions(FUSE::FUSE INTERFACE FUSE_FOUND)
else()
  message(WARNING "Notice: FUSE not found, no FUSE support")
  add_library(FUSE::FUSE INTERFACE IMPORTED)
endif()