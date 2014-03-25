###############################################################################
# Find libzmq
#
# This sets the following variables:
# libzmq_FOUND - True if ZMQ was found.
# libzmq_INCLUDE_DIRS - Directories containing the ZMQ include files.
# libzmq_LIBRARIES - Libraries needed to use ZMQ.

find_package(PkgConfig)
if( PKG_CONFIG_EXECUTABLE )
  pkg_check_modules(libzmq libzmq)
endif()


set(MSVC_BINARIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../msvc_binaries")
if( NOT libzmq_FOUND)
  if( MSVC )
    # have to install dlls manually
    if( MSVC70 OR MSVC71 )
      set(ZMQ_MSVC_PREFIX "v70")
    elseif( MSVC80 )
      set(ZMQ_MSVC_PREFIX "v80")
    elseif( MSVC90 )
      set(ZMQ_MSVC_PREFIX "v90")
    else()
      set(ZMQ_MSVC_PREFIX "v100")
    endif()
    message(STATUS "setting local libzmq library from msvc_binaries/${CMAKE_SYSTEM_PROCESSOR}")
    set(libzmq_INCLUDE_DIRS "${MSVC_BINARIES_DIR}/${CMAKE_SYSTEM_PROCESSOR}/${MSVC_PREFIX}/include")
    set(libzmq_LIBRARIES "${MSVC_BINARIES_DIR}/${CMAKE_SYSTEM_PROCESSOR}/${MSVC_PREFIX}/lib/libzmq-${ZMQ_MSVC_PREFIX}-mt-3_2_3.lib")
    message(STATUS "zmq library: ${libzmq_LIBRARIES}")
	set(libzmq_LIBRARY_DIRS "${MSVC_BINARIES_DIR}/${CMAKE_SYSTEM_PROCESSOR}/${MSVC_PREFIX}/lib")
    set(libzmq_FOUND TRUE)
  else()
    message(FATAL_ERROR "could not find libzmq library")
  endif()
else()
  include_directories(${libzmq_INCLUDE_DIRS})
  message(STATUS "libzmq found (include: ${libzmq_INCLUDE_DIRS}, lib: ${libzmq_LIBRARY_DIRS})")
endif()
