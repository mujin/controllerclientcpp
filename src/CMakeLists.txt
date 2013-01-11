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

add_library(mujincontrollerclient SHARED mujincontrollerclient.cpp)
set_target_properties(mujincontrollerclient PROPERTIES COMPILE_FLAGS "${EXTRA_COMPILE_FLAGS} ${Boost_CFLAGS} -DMUJINCLIENT_DLL_EXPORTS -DMUJINCLIENT_DLL" LINK_FLAGS ""
  OUTPUT_NAME mujincontrollerclient${MUJINCLIENT_LIBRARY_SUFFIX}
  SOVERSION 0 # always have it 0 since we're including the soversion as part of the library name
  VERSION ${MUJINCLIENT_VERSION}
  CLEAN_DIRECT_OUTPUT 1
  )
target_link_libraries(mujincontrollerclient ${CURL_LIBRARIES})
install(TARGETS mujincontrollerclient RUNTIME DESTINATION bin LIBRARY DESTINATION bin ARCHIVE DESTINATION lib${LIB_SUFFIX})

macro(build_sample name)
  add_executable(${name} ${name}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${Boost_CFLAGS}" LINK_FLAGS "")
  add_dependencies(${name} mujincontrollerclient)
  target_link_libraries (${name} mujincontrollerclient ${Boost_THREAD_LIBRARY} ${Boost_DATETIME_LIBRARY})
  install(TARGETS ${name} DESTINATION bin)
endmacro(build_sample)

build_sample(mujinclienttest)
build_sample(mujinimportscene)
