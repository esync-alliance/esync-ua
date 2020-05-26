cmake_minimum_required(VERSION 3.4.3)
project(libxl4ua)

set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(TARGET $ENV{PORT})

function(set_empty OUTPUT VAL)
    if ((NOT DEFINED ${OUTPUT}) OR "${${OUTPUT}}" STREQUAL "")
        set(${OUTPUT} "${VAL}" PARENT_SCOPE)
    endif()
endfunction(set_empty)

function(targets_link_libraries)
    set(LIB_MODE "zz")
    foreach(arg ${ARGN})
        if (${LIB_MODE} STREQUAL TRUE)
            foreach(arg2 ${ARGN})
                message("Applying ${arg2} to target ${arg}")
                if ("${arg2}" STREQUAL "LIBRARIES")
                    break()
                endif()
                if (TARGET ${arg2})
                    target_link_libraries(${arg2} ${arg})
                endif()
            endforeach()
        elseif("${arg}" STREQUAL "LIBRARIES")
            message("Switching to targeting")
            set(LIB_MODE TRUE)
        else()
            message("Delaying target ${arg}")
        endif()
    endforeach()
    message("end targets_link_libraries")
endfunction(targets_link_libraries)

if ("${TARGET}" STREQUAL "")
    set(TARGET "linux_port")
endif()

get_filename_component(TARGET "${TARGET}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
get_filename_component(ROOT_DIR "" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")

set(LOAD_CFG ${TARGET}/config.cmk)
if((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
    set(LOAD_CFG ${TARGET}/config.cmk.tmpl)
    if ((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
        message(FATAL_ERROR "${TARGET} folder must contain config.cmk file")
    endif()
endif()

include(${LOAD_CFG})

set(LOAD_CFG ${TARGET}/deps.cmk)
if((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
    set(LOAD_CFG ${TARGET}/deps.cmk.tmpl)
    if ((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
        message(FATAL_ERROR "${TARGET} folder must contain deps.cmk file")
    endif()
endif()

include(${LOAD_CFG})

set_empty(BUILD_SHARED TRUE)
set_empty(BUILD_STATIC TRUE)
set_empty(BUILD_BINS TRUE)
set_empty(WITH_EFENCE FALSE)

set_empty(XL4_PROVIDE_THREADS 1)
set_empty(XL4_PROVIDE_DEBUG 1)
set_empty(XL4_PROVIDE_PRINTF 0)
set_empty(XL4_SUPPORT_THREADS 1)
set_empty(XL4_PROVIDE_GETTIMEOFDAY 1)
set_empty(XL4_HAVE_GETTIMEOFDAY 1)
set_empty(XL4_HAVE_STD_MALLOC 1)
set_empty(HAVE_LIMITS_H 1)
if (USE_LEGACY_API)
    message("Building libua with legacy API")
else()
    message("Building libua with API version 2.0")
    set(LIBUA_API_VER "LIBUA_VER_2_0")
    add_definitions(-DLIBUA_VER_2_0)
endif()
if (CMAKE_CROSSCOMPILING)
    set(THREADS_PTHREAD_ARG 2)
endif()

set(BCH ${CMAKE_BINARY_DIR}/build_config.h)
configure_file(${CMAKE_SOURCE_DIR}/build_config.h.in ${BCH})
configure_file(${CMAKE_SOURCE_DIR}/json-c-rename.h ${CMAKE_BINARY_DIR} COPYONLY)

add_custom_command(OUTPUT PHONY ${CMAKE_BINARY_DIR}/ua_version.h COMMAND ${CMAKE_SOURCE_DIR}/make_version.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_definitions(-D_GNU_SOURCE)

if(ESYNC_ALLIANCE)
	add_definitions(-DESYNC_ALLIANCE)
endif()

set(LIB_SOURCE ${LIB_SOURCE}
    PHONY
    ${BCH}
    ${CMAKE_BINARY_DIR}/ua_version.h
    src/handler.c
    src/utils.c
    src/xl4busclient.c
    src/common.c
    src/misc.c
    src/delta.c
    src/xml.c
    src/updater.c
    src/component.c
    src/handler.h
    src/utils.h
    src/xl4busclient.h
    src/common.h
    src/misc.h
    src/delta.h
    src/xml.h
    src/updater.h
    src/component.h
    src/debug.h
    src/uthash.h
    src/utlist.h
    src/debug.h
)

if (USE_LEGACY_API)
    set(LIB_SOURCE ${LIB_SOURCE} src/include/xl4ua.h)
else()
    set(LIB_SOURCE ${LIB_SOURCE} src/include/esyncua.h)
endif()


if (SUPPORT_UA_DOWNLOAD)
    set(LIB_SOURCE ${LIB_SOURCE}
        src/Crc32.c
        src/Crc32.h
        src/base64.c
        src/base64.h
        src/ua_download.c
        src/ua_download.h
        src/eua_json.h
    )
endif()

if (SHELL_COMMAND_DISABLE)
    set(LIB_SOURCE ${LIB_SOURCE}
        src/delta_utils/delta_utils.h
        src/delta_utils/xzdec.c
        src/delta_utils/espatch.c
        )
endif()

if (XL4_PROVIDE_THREADS)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads MODULE REQUIRED)
endif()

if (DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

if (BUILD_SHARED)
    add_library(ua-shared SHARED ${LIB_SOURCE})
    set_target_properties(ua-shared PROPERTIES OUTPUT_NAME ua CLEAN_DIRECT_OUTPUT 1)
    if (XL4_PROVIDE_THREADS)
        target_link_libraries(ua-shared Threads::Threads)
    endif()
    set(USE_LIB ua-shared)
    target_link_libraries(ua-shared ${SHARED_DEPS})
    install(TARGETS ua-shared LIBRARY DESTINATION lib)
endif()

if (BUILD_STATIC)
    file(GLOB EXT_OBJ ${JSON_C}/*.o)
    add_library(ua-static STATIC ${LIB_SOURCE} ${EXT_OBJ})
    set_target_properties(ua-static PROPERTIES OUTPUT_NAME ua CLEAN_DIRECT_OUTPUT 1)
    if (XL4_PROVIDE_THREADS)
        target_link_libraries(ua-static Threads::Threads)
    endif()
    if (NOT USE_LIB)
        set(USE_LIB ua-static)
    endif()
    target_compile_definitions(ua-static PUBLIC HIDE_SYM)
    install(TARGETS ua-static ARCHIVE DESTINATION lib)
endif()

if (BUILD_BINS)
    add_executable(deltapatcher ${LIB_SOURCE} src/patcher.c)

    set(TMPL_UA_SOURCE
        src/test_agents/main_updateagent.c
        src/test_agents/tmpl_updateagent.c
        src/test_agents/tmpl_updateagent.h
        )
    if(TMPL_UA_SUPPORT_SCP_TRANSFER)
    set(TMPL_UA_SOURCE  ${TMPL_UA_SOURCE}
        src/test_agents/util.c
        src/test_agents/util.h
        )
    endif()
    add_executable(agent ${TMPL_UA_SOURCE})
    targets_link_libraries(agent deltapatcher LIBRARIES ${USE_LIB} ${APP_DEPS})

    if (XL4_PROVIDE_THREADS)
        targets_link_libraries(agent deltapatcher LIBRARIES Threads::Threads)
    else()
        message(FATAL_ERROR "Binaries require threading support that is not enabled. You probably want to disable binaries being built as well.")
    endif()
endif()

set(CMAKE_VERBOSE_MAKEFILE on)

install(FILES src/delta_utils/delta_utils.h DESTINATION include)
if (USE_LEGACY_API)
    install(FILES src/include/xl4ua.h DESTINATION include)
else()
    install(FILES src/include/esyncua.h DESTINATION include)
endif()

if(UNIT_TEST)
    find_library(CMOCKA_LIB NAMES cmocka PATHS /usr/local/lib)
    add_executable(ut_test ${LIB_SOURCE}
                    unit_tests/ut_updateagent.c
                    unit_tests/ut_updateagent.h
                    unit_tests/test_setup.c
                    unit_tests/test_setup.h
                    unit_tests/ut_main.c
                    )
    if(NOT GCC_WRAP_FLAGS)
    set(GCC_WRAP_FLAGS ${GCC_WRAP_FLAGS} -Wl,--wrap=xl4bus_client_init,--wrap=xl4bus_client_stop,--wrap=xl4bus_client_send_msg,--wrap=reply_id_matched )
    endif()
    target_compile_definitions(ut_test PRIVATE WITH_UNIT_TEST)
    target_compile_options(ut_test PUBLIC -fprofile-arcs -ftest-coverage --coverage ${GCC_WRAP_FLAGS})
    target_link_libraries(ut_test pthread gcov ${GCC_WRAP_FLAGS} ${CMOCKA_LIB} ${APP_DEPS})
endif()


if(BUILD_PY_LIBUA)
	include(FindPythonInterp)
	find_package(PythonInterp)
    if (PYTHONINTERP_FOUND)
		set(PY_LIBUA_DIR "${CMAKE_SOURCE_DIR}/pylibua")
		set(SETUP_PY    "${CMAKE_SOURCE_DIR}/setup.py")
        file(GLOB pylibua_SRC
            "${PY_LIBUA_DIR}/*.h"
            "${PY_LIBUA_DIR}/*.c"
            "${PY_LIBUA_DIR}/*.py"
            "${PY_LIBUA_DIR}/*.i")

        add_custom_target(pylibua ALL DEPENDS ${pylibua_SRC}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} -E env XL4BUS_DIR=${XL4BUS_DIR} LIBUA_BUILD=${CMAKE_BINARY_DIR} LIBUA_API_VER=${LIBUA_API_VER} ${PYTHON_EXECUTABLE} ${SETUP_PY} develop --user )

		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E env XL4BUS_DIR=${XL4BUS_DIR} LIBUA_BUILD=${CMAKE_BINARY_DIR} ${PYTHON_EXECUTABLE} ${SETUP_PY} install --record install_files.txt)")
	endif()
endif()
