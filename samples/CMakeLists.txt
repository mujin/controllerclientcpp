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

build_sample(mujinexecutetask)
build_sample(mujinexecutetask_fast)
build_sample(mujinexecutetask_robodia)
build_sample(mujinideal_densowave)
build_sample(mujinimportscene)
build_sample(mujinshowresults)
build_sample(mujinimportscene_robodia)
build_sample(mujinbackup)
build_sample(mujinuploadfile)
build_sample(mujindownloadfile)
build_sample(mujindeletefile)
build_sample(mujinupgrade)
build_sample(mujinreboot)
build_sample(mujindownloaddebug)
build_sample(mujindownloaddebugall)
if (libzmq_FOUND)
  build_sample(mujinbinpickingtask)
  build_sample(mujinjog)
  build_sample(mujinupdategeometry)
  build_sample(mujinmovetoollinear)
  build_sample(mujintransform)
  build_sample(mujinsetdofvalues)
  build_sample(mujincreateikparam)
  build_sample(mujingetpublishedtaskstate)
  build_sample(mujingrabrelease)
  build_sample(mujinpointcloud)
  build_sample(mujincreateinstobject)
  build_sample(mujinmovetohandposition)
  build_sample(mujinmoveandcancel)
  build_sample(mujinplanexecute)
  build_sample(mujincreateitltask)
  build_sample(mujinlistobjects)
  build_sample(mujinqueryobject)
endif()
build_sample(mujinupdateenvironmentstate)
