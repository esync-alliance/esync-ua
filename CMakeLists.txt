cmake_minimum_required(VERSION 3.4.3)
project(libxl4ua VERSION 1.0.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(TARGET_PLATFORM_PORT_DIR $ENV{PORT})

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

if ("${TARGET_PLATFORM_PORT_DIR}" STREQUAL "")
    set(TARGET_PLATFORM_PORT_DIR "linux_port")
endif()

get_filename_component(PORT_DIR "${TARGET_PLATFORM_PORT_DIR}" REALPATH
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(ROOT_DIR "" REALPATH
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if((NOT IS_DIRECTORY "${PORT_DIR}") OR ("${PORT_DIR}" STREQUAL "${ROOT_DIR}"))
    message(FATAL_ERROR
        " Missing Port Directory:\n"
        "  [${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_PLATFORM_PORT_DIR}]\n")
endif()

set(LOAD_CFG ${PORT_DIR}/config.cmk)
if((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
    set(LOAD_CFG ${PORT_DIR}/config.cmk.tmpl)
    if ((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
        message(FATAL_ERROR "${PORT_DIR} folder must contain config.cmk file")
    endif()
endif()

include(${LOAD_CFG})

if(BUILD_DEPENDENCIES)
    if(NOT EXISTS ${PORT_DIR}/build_deps.done)
        message(STATUS "Building ${PROJECT_NAME} External Dependendcies...")
        execute_process(COMMAND ./build_deps.sh clean
            OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/CMakeExtDepsClean.log
            ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/CMakeExtDepsClean.log
            WORKING_DIRECTORY "${PORT_DIR}")
        execute_process(COMMAND ./build_deps.sh
            OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/CMakeExtDepsBuild.log
            ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/CMakeExtDepsBuild.log
            WORKING_DIRECTORY "${PORT_DIR}")
        message(STATUS "${PROJECT_NAME} External Dependencies Build Finished...")
    endif()
endif(BUILD_DEPENDENCIES)

set(LOAD_CFG ${PORT_DIR}/deps.cmk)
if((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
    set(LOAD_CFG ${PORT_DIR}/deps.cmk.tmpl)
    if ((NOT EXISTS ${LOAD_CFG}) OR (IS_DIRECTORY ${LOAD_CFG}))
        message(FATAL_ERROR "${PORT_DIR} folder must contain deps.cmk file")
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
    set(libName "ua")
else()
    message("Building libua with API version 2.0")
    set(LIBUA_API_VER "LIBUA_VER_2_0")
    add_definitions(-DLIBUA_VER_2_0)
    set(libName "ua_2")
endif()
if (CMAKE_CROSSCOMPILING)
    set(THREADS_PTHREAD_ARG 2)
endif()

set(BCH ${CMAKE_CURRENT_BINARY_DIR}/build_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_config.h.in ${BCH})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/json-c-rename.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

add_custom_command(OUTPUT PHONY ${CMAKE_CURRENT_BINARY_DIR}/ua_version.h COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make_version.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-D_GNU_SOURCE)

if(ESYNC_ALLIANCE)
	add_definitions(-DESYNC_ALLIANCE)
	set(UpdateAgentBinNAme "${binName}")
else()
	set(UpdateAgentBinNAme "tmpl-updateagent")
endif()

set(LIB_SOURCE ${LIB_SOURCE}
    PHONY
    ${BCH}
    ${CMAKE_CURRENT_BINARY_DIR}/ua_version.h
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

if (SUPPORT_LOGGING_INFO)
    set(LIB_SOURCE ${LIB_SOURCE}
    src/diagnostic.c
    src/diagnostic.h
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

if(ENABLE_YOCTO_BUILD)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
else(ENABLE_YOCTO_BUILD)
    if (DEBUG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
    else(DEBUG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    endif(DEBUG)
endif(ENABLE_YOCTO_BUILD)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wimplicit -Wreturn-type -Werror")

if (BUILD_SHARED)
    add_library(ua-shared SHARED ${LIB_SOURCE})
    set_target_properties(ua-shared PROPERTIES OUTPUT_NAME ${libName} CLEAN_DIRECT_OUTPUT 1)
    if (XL4_PROVIDE_THREADS)
        target_link_libraries(ua-shared Threads::Threads)
    endif()
    set(USE_LIB ua-shared)
    target_link_libraries(ua-shared ${SHARED_DEPS})
    install(TARGETS ua-shared LIBRARY DESTINATION lib)

    if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        add_dependencies(ua-shared xl4bus-shared)
    endif()

    if(ENABLE_YOCTO_BUILD)
        set_target_properties(ua-shared PROPERTIES
            SOVERSION   ${PROJECT_VERSION_MAJOR}
            VERSION     ${PROJECT_VERSION}
            )
    endif(ENABLE_YOCTO_BUILD)
endif()

if (BUILD_STATIC)
    if(ENABLE_YOCTO_BUILD)
        file(GLOB EXT_OBJ ${BITBAKE_STAGING_DIR}/updateagent/json-c/obj/*.o)
    else(ENABLE_YOCTO_BUILD)
        file(GLOB EXT_OBJ ${JSON_C}/*.o)
    endif(ENABLE_YOCTO_BUILD)

    add_library(ua-static STATIC ${LIB_SOURCE} ${EXT_OBJ})
    set_target_properties(ua-static PROPERTIES OUTPUT_NAME ${libName} CLEAN_DIRECT_OUTPUT 1)
    if (XL4_PROVIDE_THREADS)
        target_link_libraries(ua-static Threads::Threads)
    endif()
    if (NOT USE_LIB)
        set(USE_LIB ua-static)
    endif()
    target_compile_definitions(ua-static PUBLIC HIDE_SYM)
    install(TARGETS ua-static ARCHIVE DESTINATION lib)

    if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        add_dependencies(ua-static xl4bus-shared)
    endif()
endif()

if (BUILD_BINS)
    add_executable(deltapatcher ${LIB_SOURCE} src/tools/patcher.c)

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
    add_executable(${UpdateAgentBinNAme} ${TMPL_UA_SOURCE})
    targets_link_libraries(${UpdateAgentBinNAme} deltapatcher LIBRARIES ${USE_LIB} ${APP_DEPS})

    if (XL4_PROVIDE_THREADS)
        targets_link_libraries(${UpdateAgentBinNAme} deltapatcher LIBRARIES Threads::Threads)
    else()
        message(FATAL_ERROR "Binaries require threading support that is not enabled. You probably want to disable binaries being built as well.")
    endif()

    set(MAKE_BACKUP_SOURCE ${LIB_SOURCE_P}
        src/tools/make_backup.c
        src/xml.c
        src/xml.h
        src/misc.c
        src/misc.h
        src/common.c
        src/common.h
        src/delta.c
        src/delta.h
        src/handler.h
        src/debug.h
    )

    if (SUPPORT_LOGGING_INFO)
        set(MAKE_BACKUP_SOURCE ${MAKE_BACKUP_SOURCE}
        src/diagnostic.c
        src/diagnostic.h
        )
    endif()

    add_executable(make_backup ${MAKE_BACKUP_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/ua_version.h)
    target_link_libraries(make_backup ${APP_DEPS})

    if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        add_dependencies(make_backup xl4bus-shared)
    endif()

    if (XL4_PROVIDE_THREADS)
        targets_link_libraries(${UpdateAgentBinNAme} deltapatcher make_backup LIBRARIES Threads::Threads)
    else()
        message(FATAL_ERROR "Binaries require threading support that is not enabled. You probably want to disable binaries being built as well.")
    endif()

    if(WITH_BINTEST)
        install(TARGETS ${UpdateAgentBinNAme} RUNTIME DESTINATION bin)
        install(TARGETS make_backup RUNTIME DESTINATION bin)
    endif(WITH_BINTEST)
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

    if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        add_dependencies(ut_test xl4bus-shared)
    endif()
    if(NOT GCC_WRAP_FLAGS)
    set(GCC_WRAP_FLAGS ${GCC_WRAP_FLAGS} -Wl,--wrap=xl4bus_client_init,--wrap=xl4bus_client_stop,--wrap=xl4bus_client_send_msg,--wrap=reply_id_matched )
    endif()
    target_compile_definitions(ut_test PRIVATE WITH_UNIT_TEST)
    target_compile_options(ut_test PUBLIC -fprofile-arcs -ftest-coverage --coverage ${GCC_WRAP_FLAGS})
    target_link_libraries(ut_test pthread gcov ${GCC_WRAP_FLAGS} ${CMOCKA_LIB} ${APP_DEPS})
endif()

if(BUILD_PY_LIBUA AND NOT ENABLE_YOCTO_BUILD)
    include(FindPythonInterp)
    find_package(PythonInterp)
    if (PYTHONINTERP_FOUND)
        set(PY_LIBUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pylibua")
        set(SETUP_PY    "${CMAKE_CURRENT_SOURCE_DIR}/setup.py")
        file(GLOB pylibua_SRC
            "${PY_LIBUA_DIR}/*.h"
            "${PY_LIBUA_DIR}/*.c"
            "${PY_LIBUA_DIR}/*.py"
            "${PY_LIBUA_DIR}/*.i")

        if (PYTHON_VER3)
            set(PYTHON_BIN_PATH ${PYTHON3_EXECUTABLE})
        else()
            set(PYTHON_BIN_PATH ${PYTHON_EXECUTABLE})
        endif()

        if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
            set(XL4BUS_BIN_DIR ${XL4BUS_DIR})
            set(XL4BUS_SRC_DIR ${XL4BUS_DIR}/..)
        else()
            set(XL4BUS_BIN_DIR ${xl4bus_BINARY_DIR})
            set(XL4BUS_SRC_DIR ${xl4bus_SOURCE_DIR})
        endif()

        set(PYLIBUA_SETUP_ENV
            XL4BUS_BIN_DIR=${XL4BUS_BIN_DIR}
            XL4BUS_SRC_DIR=${XL4BUS_SRC_DIR}
            LIBUA_BUILD=${CMAKE_CURRENT_BINARY_DIR}
            LIBUA_API_VER=${LIBUA_API_VER})

        execute_process(COMMAND
            "${PYTHON_EXECUTABLE}" -c "if True:
                from distutils import sysconfig as sc
                print(sc.get_python_lib(prefix='${CMAKE_INSTALL_PREFIX}', plat_specific=True))"
            OUTPUT_VARIABLE PYTHON_LOCAL_SITE
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        set(PYLIBUA_INSTALL_ENV ${PYLIBUA_SETUP_ENV} "PYTHONPATH=${PYTHON_LOCAL_SITE}")
        set(PYLIBUA_INSTALL_EXTRA_OPT "--prefix ${CMAKE_INSTALL_PREFIX}")

        add_custom_target(pylibua ALL DEPENDS ${pylibua_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} -E env ${PYLIBUA_SETUP_ENV} ${PYTHON_BIN_PATH} ${SETUP_PY} build --build-base=${CMAKE_CURRENT_BINARY_DIR})

        add_dependencies(pylibua ua-shared ua-static)

        install(CODE "execute_process(
            COMMAND ${CMAKE_COMMAND} -E env ${PYLIBUA_INSTALL_ENV} ${PYTHON_BIN_PATH} ${SETUP_PY} install ${PYLIBUA_INSTALL_EXTRA_OPT} --record install_files.txt)")
    endif()
endif()
