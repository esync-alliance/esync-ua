cmake_minimum_required(VERSION 3.6)
project(updateagent)
include(FindLibZip.cmake)

function(influence_pkg_spec TGT PREFIX)

    if (TARGET ${TGT})
        target_compile_options(${TGT} PUBLIC ${${PREFIX}_CFLAGS})
        target_compile_options(${TGT} PUBLIC ${${PREFIX}_CFLAGS_OTHER})

        target_link_libraries(${TGT} ${${PREFIX}_LDFLAGS})
        target_link_libraries(${TGT} ${${PREFIX}_LDFLAGS_OTHER})
    endif()

endfunction(influence_pkg_spec)

function(influence_pkg PREFIX)
    if (${PREFIX}_FOUND)
        influence_pkg_spec(updateagent ${PREFIX})
    endif()
endfunction(influence_pkg)

# thanks to http://stackoverflow.com/questions/7172670
function(join OUTPUT GLUE)
    set(_TMP_RESULT "")
    set(_GLUE "") # effective glue is empty at the beginning
    foreach(arg ${ARGN})
        set(_TMP_RESULT "${_TMP_RESULT}${_GLUE}${arg}")
        set(_GLUE "${GLUE}")
    endforeach()
    set(${OUTPUT} "${_TMP_RESULT}" PARENT_SCOPE)
endfunction(join)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(BUILD_CONFIG $ENV{BUILD_CONFIG})
if (NOT BUILD_CONFIG)
    set(BUILD_CONFIG "config.cmk")
endif()

message(STATUS "Using configuration file ${BUILD_CONFIG}")

include(${BUILD_CONFIG})

configure_file(${CMAKE_SOURCE_DIR}/json-c-rename.h ${CMAKE_BINARY_DIR} COPYONLY)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/json-c-build)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR})

add_custom_command(OUTPUT PHONY ${CMAKE_BINARY_DIR}/ua_version.h COMMAND ${CMAKE_SOURCE_DIR}/make_version.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set(SOURCE_FILES
    PHONY
    src/updateagent.c
    src/handler.c
    src/utils.c
    src/uaclient.c

    src/updateagent.h
    src/handler.h
    src/utils.h
    src/uaclient.h
    src/config.h
    src/debug.h
)

add_executable(updateagent ${SOURCE_FILES})

if (DEBUG)
    set(myc "-g -ggdb -O0")
else()
    set(myc "-O3")
endif()

set(JSON_C_LIB ${CMAKE_SOURCE_DIR}/json-c-build/.libs/libjson-c.a)
if (NOT EXISTS ${JSON_C_LIB})
    message(FATAL_ERROR "You must build json-c sources first!")
endif()

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${myc} --std=c99 -isystem${XL4BUS_DIR}/../src/include -isystem${XL4BUS_DIR} ")

if (XL4BUS_RPATH)
    set(XL4BUS_RPATH "-Wl,-R${XL4BUS_DIR}")
else()
    set(XL4BUS_RPATH "")
endif()

set(USE_LIB  "-L${XL4BUS_DIR}" "${XL4BUS_RPATH}" "-lxl4bus" "-lz" "-lm" ${LIBZIP_LIBRARY} ${JSON_C_LIB})

target_link_libraries(updateagent ${USE_LIB})

set(CMAKE_VERBOSE_MAKEFILE on)

