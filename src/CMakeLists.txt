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
include_directories(${MUJINCLIENT_INCLUDE_LOCAL_DIRS} ${CURL_INCLUDE_DIRS})
link_directories(${MUJINCLIENT_LINK_DIRS})

set(SOURCE_FILES common.cpp controllerclientimpl.cpp mujincontrollerclient.cpp ${mujin_header_files} common.h controllerclientimpl.h utf8.h)

add_library(libmujincontrollerclient SHARED ${SOURCE_FILES})
if( EXTRA_MSVC_DEPEND )
  message(STATUS "adding msvc_boost dependency")
  add_dependencies(libmujincontrollerclient ${EXTRA_MSVC_DEPEND})
endif()
set_target_properties(libmujincontrollerclient PROPERTIES COMPILE_FLAGS "${EXTRA_COMPILE_FLAGS} ${Boost_CFLAGS} -DMUJINCLIENT_DLL_EXPORTS -DMUJINCLIENT_DLL" LINK_FLAGS ""
  OUTPUT_NAME mujincontrollerclient${MUJINCLIENT_LIBRARY_SUFFIX}
  SOVERSION 0 # always have it 0 since we're including the soversion as part of the library name
  VERSION ${MUJINCLIENT_VERSION}
  CLEAN_DIRECT_OUTPUT 1
  )
target_link_libraries(libmujincontrollerclient ${CURL_LIBRARIES} ${Boost_THREAD_LIBRARY} ${Boost_DATE_TIME_LIBRARY}  ${EXTRA_LIBRARIES})
if( MSVC )
  install(TARGETS libmujincontrollerclient RUNTIME DESTINATION bin LIBRARY DESTINATION bin ARCHIVE DESTINATION lib${LIB_SUFFIX})
else()
  install(TARGETS libmujincontrollerclient DESTINATION lib${LIB_SUFFIX})
endif()

if( OPT_BUILD_STATIC )
  # visual studio needs static lib built
  if( MSVC )
    # static version needs to have different name
    set(LIBMUJINCONTROLLERCLIENT_NAME libmujincontrollerclient${MUJINCLIENT_LIBRARY_SUFFIX})
  else()
    set(LIBMUJINCONTROLLERCLIENT_NAME mujincontrollerclient${MUJINCLIENT_LIBRARY_SUFFIX})
  endif()

  add_library(libmujincontrollerclient_static STATIC ${SOURCE_FILES})
  set_target_properties(libmujincontrollerclient_static PROPERTIES OUTPUT_NAME ${LIBMUJINCONTROLLERCLIENT_NAME}
                                           SOVERSION 0 # always have it 0 since we're including the soversion as part of the library name
                                           VERSION ${MUJINCLIENT_VERSION}
                                           CLEAN_DIRECT_OUTPUT 1
                                           COMPILE_FLAGS "${EXTRA_COMPILE_FLAGS} ${Boost_CFLAGS} -DMUJINCLIENT_DLL_EXPORTS -DMUJINCLIENT_DLL"
                                           LINK_FLAGS "")
  
  target_link_libraries(libmujincontrollerclient_static ${CURL_LIBRARIES} ${EXTRA_LIBRARIES})
  install(TARGETS libmujincontrollerclient_static DESTINATION lib${LIB_SUFFIX})
endif()
