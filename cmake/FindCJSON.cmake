# Try to find the CJSON library and define:
#
# CJSON_FOUND        - True if library was found.
# CJSON_INCLUDE_DIRS - Include directories.
# CJSON_LIBRARIES    - Libraries.
#
# CJSON::CJSON

# check if already in cache, be silent
if (CJSON_INCLUDE_DIRS AND CJSON_LIBRARIES)
    SET (CJSON_FIND_QUIETLY TRUE)
endif ()

if (APPLE)
    set (CJSON_NAMES libcjson.dylib libcjson_utils.dylib)
else ()
    set (CJSON_NAMES libcjson.a libcjson_utils.a)
endif ()

# find include
find_path (
    CJSON_INCLUDE_DIRS cjson/cJSON.h
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES include
    REQUIRED)

# find lib
find_library (
    CJSON_LIBRARIES
    NAMES ${CJSON_NAMES}
    PATHS /opt /opt/local /usr /usr/local /usr/pkg
    PATH_SUFFIXES lib
    REQUIRED)

include ("FindPackageHandleStandardArgs")
find_package_handle_standard_args (
    "CJSON" DEFAULT_MSG
    CJSON_INCLUDE_DIRS CJSON_LIBRARIES)

mark_as_advanced (CJSON_INCLUDE_DIRS CJSON_LIBRARIES)

if (CJSON_FOUND AND NOT TARGET CJSON::CJSON)
  add_library(CJSON::CJSON STATIC IMPORTED)
  set_target_properties(CJSON::CJSON PROPERTIES
    IMPORTED_LOCATION "${CJSON_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${CJSON_INCLUDE_DIRS}")
  target_compile_definitions(CJSON::CJSON INTERFACE CJSON_FOUND)
else()
  message(WARNING "Notice: CJSON not found, no CJSON support")
  add_library(CJSON::CJSON INTERFACE IMPORTED)
endif()