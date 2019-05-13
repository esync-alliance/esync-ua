cmake_minimum_required(VERSION 3.6)
project(libxl4ua)

include(FindPkgConfig)
include(FindLibXml2)
include(FindOpenSSL)
include(FindLibZip.cmake)
include(CMakeForceCompiler)

set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_VISIBILITY_PRESET hidden)

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
        influence_pkg_spec(ua-shared ${PREFIX})
        influence_pkg_spec(ua-static ${PREFIX}_STATIC)
        influence_pkg_spec(updateagent ${PREFIX})
    endif()
endfunction(influence_pkg)

set(BUILD_CONFIG $ENV{BUILD_CONFIG})
if (NOT BUILD_CONFIG)
    set(BUILD_CONFIG "config.cmk")
endif()

message(STATUS "Using configuration file ${BUILD_CONFIG}")

include(${BUILD_CONFIG})

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

set(BCH ${CMAKE_BINARY_DIR}/build_config.h)

configure_file(${CMAKE_SOURCE_DIR}/build_config.h.in ${BCH})
configure_file(${CMAKE_SOURCE_DIR}/json-c-rename.h ${CMAKE_BINARY_DIR} COPYONLY)

set(JSON_C ${CMAKE_SOURCE_DIR}/json-c-build)

include_directories(SYSTEM ${XL4BUS_DIR})
include_directories(SYSTEM ${XL4BUS_DIR}/include)
include_directories(SYSTEM ${XL4BUS_DIR}/../src/include)
include_directories(SYSTEM ${JSON_C})
include_directories(SYSTEM ${LIBZIP_INCLUDE_DIR})
include_directories(SYSTEM ${LIBXML2_INCLUDE_DIR})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/src/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR})
link_directories(${XL4BUS_DIR})

add_custom_command(OUTPUT PHONY ${CMAKE_BINARY_DIR}/ua_version.h COMMAND ${CMAKE_SOURCE_DIR}/make_version.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

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
    src/handler.h
    src/utils.h
    src/xl4busclient.h
    src/common.h
    src/misc.h
    src/delta.h
	src/xml.h
	src/updater.h
    src/debug.h
    src/uthash.h
    src/utlist.h
    src/debug.h
    src/include/xl4ua.h
)

if (NOT LIBXML2_FOUND)
    message(FATAL_ERROR "I sorta need libxml2 dev package")
endif()

if (NOT OPENSSL_FOUND)
    message(FATAL_ERROR "Though OpenSSL has been hammered at a lot recently, I still need it")
endif()

if (NOT LIBZIP_FOUND STREQUAL "YES")
    message(FATAL_ERROR "libzip library is required")
endif()

set(JSON_C_LIB ${JSON_C}/.libs/libjson-c.a)
if (NOT EXISTS ${JSON_C_LIB})
    message(FATAL_ERROR "You must build json-c sources first!")
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
    target_link_libraries(ua-shared ${JSON_C_LIB})
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

    add_executable(tmpl-updateagent
            src/test_agents/updateagent.c
            src/test_agents/tmpl_updateagent.c
            src/test_agents/tmpl_updateagent.h
    )

    targets_link_libraries(tmpl-updateagent deltapatcher LIBRARIES ${USE_LIB} -lxl4bus -lm ${LIBZIP_LIBRARY} ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})

    if (XL4_PROVIDE_THREADS)
        targets_link_libraries(tmpl-updateagent deltapatcher LIBRARIES Threads::Threads)
    else()
        message(FATAL_ERROR "Binaries require threading support that is not enabled. You probably want to disable binaries being built as well.")
    endif()
endif()

set(CMAKE_VERBOSE_MAKEFILE on)

if(UNIT_TEST)
	find_library(CMOCKA_LIB NAMES cmocka PATHS /usr/local/lib)
	add_executable(	ual_test ${LIB_SOURCE}
						src/test_agents/tmpl_updateagent.c
						src/test_agents/tmpl_updateagent.h
						unit_tests/test_setup.c
						unit_tests/test_setup.h
						unit_tests/ual_test.c
					)
	set(GCC_WRAP_FLAGS ${GCC_WRAP_FLAGS} -Wl,--wrap=xl4bus_client_init,--wrap=xl4bus_client_stop,--wrap=xl4bus_client_send_msg,--wrap=randstring )
	target_compile_definitions(ual_test PRIVATE WITH_UNIT_TEST)
	target_compile_options(ual_test PUBLIC -fprofile-arcs -ftest-coverage --coverage )
	target_link_libraries(ual_test xl4bus pthread gcov ${GCC_WRAP_FLAGS} ${CMOCKA_LIB} ${JSON_C_LIB} ${LIBZIP_LIBRARY} ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})

endif()


