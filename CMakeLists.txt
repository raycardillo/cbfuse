cmake_minimum_required( VERSION 3.20 )

# required to make sure cdefs and stat are restricted to POSIX standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
add_definitions("-D_POSIX_C_SOURCE=200112L")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

if (APPLE)
  # reserved for future use
else()
  message(WARNING "Only APPLE builds have been tested.")
endif()

if (NOT DEFINED LIB_INSTALL_DIR)
  set (LIB_INSTALL_DIR lib)
endif ()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

project( cbfuse VERSION 0.1 LANGUAGES C )

add_subdirectory( cbfuse )
