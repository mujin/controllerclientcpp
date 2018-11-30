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
include_directories(${MUJINCLIENT_INCLUDE_LOCAL_DIRS})
link_directories(${MUJINCLIENT_LINK_DIRS})

remove_definitions("-DBOOST_ENABLE_ASSERT_HANDLER")

macro(build_test name)
  include_directories(${LOG4CXX_INCLUDEDIR})
  link_directories(${LOG4CXX_LIBDIR})
  add_executable(${name} ${name}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${Boost_CFLAGS}" LINK_FLAGS "")
  add_dependencies(${name} libmujincontrollerclient)
  target_link_libraries (${name} libmujincontrollerclient ${Boost_THREAD_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${EXTRA_LIBRARIES} ${LOG4CXX_LIBRARIES})
endmacro(build_test)

build_test(settaskparameters)
build_test(showresults)
build_test(uploadregister)
build_test(uploadregistercec)
