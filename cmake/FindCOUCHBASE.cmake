# Try to find the COUCHBASE library and define:
#
# COUCHBASE_FOUND        - True if library was found.
# COUCHBASE_INCLUDE_DIRS - Include directories.
# COUCHBASE_LIBRARIES    - Libraries.
#
# COUCHBASE::COUCHBASE

# check if already in cache, be silent
if (COUCHBASE_INCLUDE_DIRS AND COUCHBASE_LIBRARIES)
    SET (COUCHBASE_FIND_QUIETLY TRUE)
endif ()

if (APPLE)
    set (COUCHBASE_NAMES libcouchbase.dylib couchbase)
else ()
    set (COUCHBASE_NAMES libcouchbase.a couchbase)
endif ()

# find include
find_path (
    COUCHBASE_INCLUDE_DIRS libcouchbase/couchbase.h
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES include
    REQUIRED)

# find lib
find_library (
    COUCHBASE_LIBRARIES
    NAMES ${COUCHBASE_NAMES}
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES lib
    REQUIRED)

include ("FindPackageHandleStandardArgs")
find_package_handle_standard_args (
    "COUCHBASE" DEFAULT_MSG
    COUCHBASE_INCLUDE_DIRS COUCHBASE_LIBRARIES)

mark_as_advanced (COUCHBASE_INCLUDE_DIRS COUCHBASE_LIBRARIES)

if (COUCHBASE_FOUND AND NOT TARGET COUCHBASE::COUCHBASE)
  add_library(COUCHBASE::COUCHBASE STATIC IMPORTED)
  set_target_properties(COUCHBASE::COUCHBASE PROPERTIES
    IMPORTED_LOCATION "${COUCHBASE_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${COUCHBASE_INCLUDE_DIRS}")
  target_compile_definitions(COUCHBASE::COUCHBASE INTERFACE COUCHBASE_FOUND)
else()
  message(WARNING "Notice: COUCHBASE not found, no COUCHBASE support")
  add_library(COUCHBASE::COUCHBASE INTERFACE IMPORTED)
endif()