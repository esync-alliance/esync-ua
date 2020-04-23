cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME INTEGRITY)

set(LOAD_CFG ${CMAKE_SOURCE_DIR}/config.cmk)
if (NOT EXISTS ${LOAD_CFG})
  set(LOAD_CFG ${CMAKE_SOURCE_DIR}/../config.cmk.tmpl)
  if (NOT EXISTS ${LOAD_CFG})
    message(FATAL_ERROR "There is no ${LOAD_CFG} file")
  endif()
endif()
include(${LOAD_CFG})

if("${GHS_OS_DIR}" STREQUAL "")
  message(FATAL_ERROR "The GHS_OS_DIR is not found, please set GHS_OS in the config.cmk.default first!")
endif()

if("${GHS_BSP_NAME}" STREQUAL "")
  message(FATAL_ERROR "The GHS_BSP_NAME is not found, please set the GHS_BSP_NAME in the config.cmk.default first")
endif()

if("${BUILD_TOOL_DIR}" STREQUAL "")
  message(FATAL_ERROR "The BUILD_TOOL is not found, please set the BUILD_TOOL in the config.cmk.default  first!")
endif()
set(BUILD_TOOL ${BUILD_TOOL_DIR}/gbuild)

if ("${LIB_ZLIB_DIR}" STREQUAL "")
  set(LIB_ZLIB_DIR "${CMAKE_SOURCE_DIR}/../../zlib")
  if (NOT EXISTS ${LIB_ZLIB_DIR})
    message(FATAL_ERROR "You must set the path of zlib in the config.cmk or config.cmk.default")
  endif()
endif()
if ("${LIB_ZIP_DIR}" STREQUAL "")
  set(LIB_ZIP_DIR "${CMAKE_SOURCE_DIR}/../../libzip")
  if (NOT EXISTS ${LIB_ZIP_DIR})
    message(FATAL_ERROR "You must set the path of libzip in the config.cmk or config.cmk.default")
  endif()
endif()
if ("${LIB_ZLIB_BUILD_DIR}" STREQUAL "")
  set(LIB_ZLIB_BUILD_DIR "${CMAKE_SOURCE_DIR}/../zlib-build/build")
  if (NOT EXISTS ${LIB_ZLIB_BUILD_DIR})
    message(FATAL_ERROR "You must build zlip first")
  endif()
endif()

#Generate INTEGRITY project files
macro(configure_files srcDir dstDir)
  message(STATUS "Generating the gpj file FROM ${srcDir} TO  directory ${dstDir}")
  file(GLOB cfgFiles RELATIVE ${srcDir} ${srcDir}/*.in)

  foreach(cfgFile in ${cfgFiles})
    set(filePath ${srcDir}/${cfgFile})
    if(EXISTS ${filePath})
      # Remove the extension .in from input file for output file
      string(REGEX REPLACE ".in$" "" dstFile ${cfgFile})
      configure_file(${filePath} ${dstDir}/${dstFile} @ONLY)
    endif()
  endforeach()
endmacro(configure_files)

configure_files(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

configure_file(${CMAKE_SOURCE_DIR}/zip_get_encryption_implementation.c ${LIB_ZIP_DIR}/lib/zip_get_encryption_implementation.c @ONLY)

add_custom_target(build_zlib ALL
  COMMAND ${BUILD_TOOL} -top default.gpj libzip.gpj
  )
