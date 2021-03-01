# -*- coding: utf-8 -*-
# Copyright (C) 2012-2013 MUJIN Inc. <rosen.diankov@mujin.co.jp>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required (VERSION 2.8.0)
project(mujincontrollerclient)
set( CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE )

# Define here the needed parameters
# make sure to change the version in docs/Makefile
set (MUJINCLIENT_VERSION_MAJOR 0)
set (MUJINCLIENT_VERSION_MINOR 37)
set (MUJINCLIENT_VERSION_PATCH 3)
set (MUJINCLIENT_VERSION ${MUJINCLIENT_VERSION_MAJOR}.${MUJINCLIENT_VERSION_MINOR}.${MUJINCLIENT_VERSION_PATCH})
set (MUJINCLIENT_SOVERSION ${MUJINCLIENT_VERSION_MAJOR}.${MUJINCLIENT_VERSION_MINOR})
set (CLIENT_SOVERSION ${MUJINCLIENT_VERSION_MAJOR}.${MUJINCLIENT_VERSION_MINOR})
message(STATUS "Compiling MUJIN Controller Client C++ Version ${MUJINCLIENT_VERSION}, soversion=${CLIENT_SOVERSION}")

set(MUJINCLIENT_CMAKE_INSTALL_DIR "mujincontrollerclient-${MUJINCLIENT_VERSION_MAJOR}.${MUJINCLIENT_VERSION_MINOR}" CACHE STRING "Directory to install the cmake config files.")
set(MUJINCLIENT_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR} CACHE STRING "The target processor architecture to build for, this is combined with the generator toolchain")

message(STATUS "Using cmake version ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" )
# http://www.cmake.org/cmake/help/cmake-2.6.html#policy:CMP0002
cmake_policy(SET CMP0002 NEW)
# http://www.cmake.org/cmake/help/cmake-2.6.html#policy:CMP0003
cmake_policy(SET CMP0003 NEW)

option(OPT_SAMPLES "Build the samples" ON)
option(OPT_BUILD_TESTS "Build the tests" OFF)
option(OPT_BUILD_STATIC "Build static libraries for the client" ON)
option(OPT_LOG4CXX "Use log4cxx for logging" ON)

# Use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# When building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(LIB_SUFFIX CACHE STRING "suffix for the library directory need for x86-64 systems that use lib64 ")

# The RPATH to be used when installing
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")

# Add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CheckCXXCompilerFlag)

add_definitions("-DBOOST_SPIRIT_THREADSAFE") # for json parsing

# have to include before boost since the boost headers can be located in a previous installed version of this library
set(MUJINCLIENT_INCLUDE_LOCAL_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${MUJINCLIENT_INCLUDE_LOCAL_DIRS})

if( MSVC )
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_DEPRECATE)
  
  #gives linking problems
  #string(REGEX REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  #string(REGEX REPLACE "/MDd" "/MD" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

  # /EHc- allow extern "C" functions to throw exceptions
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc- ")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /EHc- ")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /EHc- ")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /EHc- ")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /EHc- ")

  # untar the source files
  set(EXTRA_MSVC_DEPEND msvc_boost)
  foreach(MSVC_LIBRARY ${EXTRA_MSVC_DEPEND})
    if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY}" )
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY}.tgz" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY}/boost.patch
      COMMAND ${CMAKE_COMMAND} -E chdir
      ARGS "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY}.tgz"
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY}.tgz)
    add_custom_target(${MSVC_LIBRARY} ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MSVC_LIBRARY})
  endforeach()

  check_include_file(stdint.h HAVE_STDINT_H)
  if( NOT HAVE_STDINT_H )
    #install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/msvc_include/stdint.h DESTINATION include/mujinclient-${MUJINCLIENT_VERSION_MAJOR}.${MUJINCLIENT_VERSION_MINOR} COMPONENT ${COMPONENT_PREFIX}dev)
    #include_directories(${CMAKE_CURRENT_SOURCE_DIR}/msvc_include)
  endif()
  
  # have to install dlls manually
  if( MSVC70 OR MSVC71 )
    set(MSVC_PREFIX "vc70")
    set(MSVC_PREFIX2 "v70")
  elseif( MSVC80 )
    set(MSVC_PREFIX "vc80")
    set(MSVC_PREFIX2 "v80")
  elseif( MSVC90 )
    set(MSVC_PREFIX "vc90")
    set(MSVC_PREFIX2 "v90")
  else()
    set(MSVC_PREFIX "vc100")
    set(MSVC_PREFIX2 "v100")
  endif()
  set(MUJINCLIENT_LIBRARY_SUFFIX "${MUJINCLIENT_SOVERSION}-${MSVC_PREFIX}-mt" CACHE STRING "Suffix to append to library names")
  
  # install all DLLs
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}/bin/" DESTINATION bin FILES_MATCHING PATTERN "*.dll")

  # force multi-threaded DLL boost
  set(Boost_USE_MULTITHREAD ON)
  set(Boost_USE_STATIC_LIBS OFF)
  set(Boost_USE_STATIC_RUNTIME OFF)
  set(Boost_CFLAGS "-DBOOST_ALL_DYN_LINK -DBOOST_ALL_NO_LIB")
else()
  set(MUJINCLIENT_LIBRARY_SUFFIX "${MUJINCLIENT_SOVERSION}" CACHE STRING "Suffix to append to library names")
endif()

set(MUJINCLIENT_LINK_DIRS)

find_package(PkgConfig)
set(MUJINCLIENT_LOG4CXX 0)
if (OPT_LOG4CXX)
  pkg_check_modules(LOG4CXX liblog4cxx)
  if (LOG4CXX_FOUND)
    if( MSVC )
      set(MUJINCLIENT_LOG4CXX_INCLUDE_DIRS "/I\"${LOG4CXX_INCLUDEDIR}\"")
      set(MUJINCLIENT_LOG4CXX_LIB_DIRS "/LIBPATH:\"${LOG4CXX_LIBDIR}\"")
    else()
      set(MUJINCLIENT_LOG4CXX_INCLUDE_DIRS "-I${LOG4CXX_INCLUDEDIR}")
      set(MUJINCLIENT_LOG4CXX_LIB_DIRS "-L${LOG4CXX_LIBDIR}")
    endif()
    set(MUJINCLIENT_LOG4CXX_LIBRARY "-l${LOG4CXX_LIBRARIES}")

    set(MUJINCLIENT_LINK_DIRS ${MUJINCLIENT_LINK_DIRS} ${LOG4CXX_LIBDIR})
    set(MUJINCLIENT_LOG4CXX 1)
  endif()
endif()

# only look for installed boost if not an old version of msvc
if(NOT MSVC71 AND NOT MSVC80)
  if( NOT $ENV{BOOST_INCLUDEDIR} STREQUAL "" )
    set(Boost_INCLUDE_DIR $ENV{BOOST_INCLUDEDIR})
  endif()
  if( NOT $ENV{BOOST_LIBRARYDIR} STREQUAL "" )
    set(Boost_LIBRARY_DIRS $ENV{BOOST_LIBRARYDIR})
  endif()
  set(Boost_ADDITIONAL_VERSIONS "1.49" "1.48" "1.47" "1.46.1" "1.45" "1.44" "1.43" "1.42" "1.41" "1.40" "1.39" "1.38" "1.37.0" "1.37" "1.35.0" "1.34.1" "1.34.0" "1.34" "1.33.1" "1.33.0" "1.33" ${Boost_ADDITIONAL_VERSIONS})
  find_package(Boost COMPONENTS filesystem system thread date_time program_options)
endif()

if( NOT MSVC )
  set(EXTRA_LIBRARIES ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
endif()

#rapidjson
find_package(RapidJSON QUIET)
if (NOT RapidJSON_FOUND)
  message(FATAL_ERROR "could not find RapidJSON")
else()
  include_directories(${RapidJSON_INCLUDE_DIRS})
  add_definitions("-DRAPIDJSON_ASSERT=BOOST_ASSERT")
endif()

if( Boost_FOUND )
  include_directories(${Boost_INCLUDE_DIRS})
  set(MUJINCLIENT_LINK_DIRS ${MUJINCLIENT_LINK_DIRS} ${Boost_LIBRARY_DIRS})
  set(EXTRA_MSVC_DEPEND) # reset extra depend
elseif(Boost_VERSION AND NOT "${Boost_VERSION}" STREQUAL "0")
  include_directories(${Boost_INCLUDE_DIRS})
  set(MUJINCLIENT_LINK_DIRS ${MUJINCLIENT_LINK_DIRS} ${Boost_LIBRARY_DIRS})
  set(EXTRA_MSVC_DEPEND msvc_boost) # reset extra depend
elseif( MSVC )
  # to facilitate compilation, visual studio libraries are included locally
  message(STATUS "using local boost libraries for MSVC")
  set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/msvc_boost")
  set(Boost_FOUND 1)
  set(Boost_INCLUDE_DIRS "${BOOST_ROOT}")
  set(Boost_INCLUDE_DIR "${BOOST_ROOT}")
  set(Boost_LIBRARY_DIRS "${BOOST_ROOT}/${MUJINCLIENT_TARGET_PROCESSOR}/lib")
  set(Boost_REGEX_FOUND 0)
  set(Boost_FILESYSTEM_FOUND 1)
  set(Boost_IOSTREAMS_FOUND 1)
  set(Boost_SYSTEM_FOUND 1)
  set(Boost_PYTHON_FOUND 0)
  set(Boost_THREAD_FOUND 1)
  set(Boost_DATE_TIME_FOUND 1)
  set(Boost_SIGNALS_FOUND 1)
  set(Boost_VERSION "104400")
  set(Boost_MAJOR_VERSION "1")
  set(Boost_MINOR_VERSION "44")
  # have to add to required flags for applications that will test linking with boost
  set(CMAKE_REQUIRED_FLAGS "-DBOOST_ALL_DYN_LINK -DBOOST_ALL_NO_LIB")
  set(Boost_DATE_TIME_LIBRARY "${Boost_LIBRARY_DIRS}/boost_date_time-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_DATE_TIME_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_date_time-${MSVC_PREFIX}-mt-1_44.dll")
  set(Boost_THREAD_LIBRARY "${Boost_LIBRARY_DIRS}/boost_thread-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_THREAD_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_thread-${MSVC_PREFIX}-mt-1_44.dll")
  set(Boost_SYSTEM_LIBRARY "${Boost_LIBRARY_DIRS}/boost_system-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_SYSTEM_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_system-${MSVC_PREFIX}-mt-1_44.dll")
  set(Boost_FILESYSTEM_LIBRARY "${Boost_LIBRARY_DIRS}/boost_filesystem-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_FILESYSTEM_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_filesystem-${MSVC_PREFIX}-mt-1_44.dll")
  set(Boost_IOSTREAMS_LIBRARY "${Boost_LIBRARY_DIRS}/boost_iostreams-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_IOSTREAMS_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_iostreams-${MSVC_PREFIX}-mt-1_44.dll")
  set(Boost_PROGRAM_OPTIONS_LIBRARY "${Boost_LIBRARY_DIRS}/boost_program_options-${MSVC_PREFIX}-mt-1_44.lib")
  set(Boost_PROGRAM_OPTIONS_LIBRARY_DLL "${Boost_LIBRARY_DIRS}/boost_program_options-${MSVC_PREFIX}-mt-1_44.dll")
  #install(DIRECTORY "${Boost_LIBRARY_DIRS}/" DESTINATION bin COMPONENT ${COMPONENT_PREFIX}base FILES_MATCHING PATTERN "*-${MSVC_PREFIX}-*.dll")
  #install(DIRECTORY "${Boost_LIBRARY_DIRS}/" DESTINATION lib${LIB_SUFFIX} COMPONENT ${COMPONENT_PREFIX}dev FILES_MATCHING PATTERN "*-${MSVC_PREFIX}-*.lib")
  install(FILES ${Boost_DATE_TIME_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} DESTINATION lib${LIB_SUFFIX} COMPONENT ${COMPONENT_PREFIX}dev)
  install(FILES ${Boost_DATE_TIME_LIBRARY_DLL} ${Boost_THREAD_LIBRARY_DLL} ${Boost_SYSTEM_LIBRARY_DLL} ${Boost_FILESYSTEM_LIBRARY_DLL} ${Boost_PROGRAM_OPTIONS_LIBRARY_DLL} DESTINATION bin COMPONENT ${COMPONENT_PREFIX}dev)
  install(DIRECTORY "${Boost_INCLUDE_DIR}/boost" DESTINATION include COMPONENT ${COMPONENT_PREFIX}dev)
  include_directories(${Boost_INCLUDE_DIRS})
  set(MUJINCLIENT_LINK_DIRS ${MUJINCLIENT_LINK_DIRS} ${Boost_LIBRARY_DIRS})
else()
  message(FATAL_ERROR "Could not find boost libraries!")
endif()

message(STATUS "found boost version: ${Boost_VERSION} at ${Boost_INCLUDE_DIRS}")
if( NOT Boost_THREAD_LIBRARY OR NOT Boost_DATE_TIME_LIBRARY )
  message(FATAL_ERROR "boost thread and date_time libraries are necessary")
endif()

add_definitions("-DBOOST_ENABLE_ASSERT_HANDLER")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR COMPILER_IS_CLANG )
  set(CMAKE_CXX_FLAGS_OPTIMIZED "-O3 -DNDEBUG -DBOOST_DISABLE_ASSERTS -D_SECURE_SCL=0") # this practically removes all checks making it a very dangerous options to play with
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "-g -D_DEBUG")

  add_definitions(-fPIC) # this is a very important switch and some libraries seem now to have it....
  set(EXTRA_COMPILE_FLAGS "-fPIC -fvisibility=hidden")
else()
  set(EXTRA_COMPILE_FLAGS "")
endif()


if( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX )
  add_definitions("-fno-strict-aliasing -Wall")
endif( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/mujincontrollerclient/config.h IMMEDIATE @ONLY)

find_package(CURL)
if( NOT CURL_FOUND )
  if( MSVC )
    message(STATUS "setting local curl library from msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}")
    set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}/include")
    set(CURL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}/lib/libcurl-${MSVC_PREFIX}-mt_imp.lib")
  else()
    message(FATAL_ERROR "could not find CURL library")
  endif()
endif()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(ZeroMQ QUIET)
if( ZeroMQ_FOUND )
  include_directories(${ZeroMQ_INCLUDE_DIR})
  if( MSVC )
    if( CMAKE_BUILD_TYPE )
      string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
    elseif( CMAKE_CONFIGURATION_TYPES )
      if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ";")
        message(FATAL_ERROR "Sorry. Due to libzmq implib naming, you can specify only one CMAKE_CONFIGURATION_TYPES (${CMAKE_CONFIGURATION_TYPES}).")
      endif()
      string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" BUILD_TYPE)
    else()
      message(FATAL_ERROR "CMAKE_BUILD_TYPE nor CMAKE_CONFIGURATION_TYPES not specified")
    endif()
    get_target_property(libzmq_LIBRARIES libzmq IMPORTED_IMPLIB_${BUILD_TYPE})
  else()
    set(libzmq_LIBRARIES "${ZeroMQ_LIBRARY}")
  endif()
  set(libzmq_FOUND 1)
else()
  find_package(PkgConfig)
  if( PKG_CONFIG_EXECUTABLE )
    pkg_check_modules(libzmq libzmq)
  endif()
  if (NOT libzmq_FOUND)
    if( MSVC )
      set(libzmq_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}/include")
      set(libzmq_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}/lib/libzmq-${MSVC_PREFIX2}-mt-4_0_4.lib")
      if(EXISTS ${libzmq_LIBRARIES} )
        message(STATUS "setting local zmq library from msvc_binaries/${MUJINCLIENT_TARGET_PROCESSOR}/${MSVC_PREFIX}")
        set(libzmq_FOUND 1)
      else()
        message(STATUS "could not find msvc zmq library ${libzmq_LIBRARIES}")
      endif()
    endif()
  endif()
endif()

if (libzmq_FOUND)
  include_directories(${libzmq_INCLUDE_DIRS})
  set(MUJINCLIENT_LINK_DIRS ${MUJINCLIENT_LINK_DIRS} ${libzmq_LIBRARY_DIRS})
else()
  message(WARNING "compiling without libzmq library")
endif()

file(GLOB mujin_header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/mujincontrollerclient/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/mujincontrollerclient/*.hpp)
install(FILES ${mujin_header_files} ${CMAKE_CURRENT_BINARY_DIR}/include/mujincontrollerclient/config.h DESTINATION include/mujincontrollerclient)

macro(build_sample name)
  include_directories(${LOG4CXX_INCLUDEDIR} ${libzmq_INCLUDE_DIRS})
  link_directories(${LOG4CXX_LIBDIR})
  add_executable(${name} ${name}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${Boost_CFLAGS}" LINK_FLAGS "")
  add_dependencies(${name} libmujincontrollerclient)
  target_link_libraries (${name} libmujincontrollerclient ${Boost_THREAD_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${EXTRA_LIBRARIES} ${LOG4CXX_LIBRARIES})
  install(TARGETS ${name} DESTINATION bin)
endmacro(build_sample)

add_subdirectory(src)
if( OPT_SAMPLES )
  add_subdirectory(samples)
endif()
if( OPT_BUILD_TESTS )
  add_subdirectory(test)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share/mujincontrollerclient DESTINATION share)

# add make uninstall capability
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

set(MujinControllerClient_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mujincontrollerclient-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/mujincontrollerclient-config.cmake" IMMEDIATE @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mujincontrollerclient-config-version.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/mujincontrollerclient-config-version.cmake" IMMEDIATE @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mujincontrollerclient-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/mujincontrollerclient-config-version.cmake" DESTINATION "lib${LIB_SUFFIX}/cmake/${MUJINCLIENT_CMAKE_INSTALL_DIR}")

