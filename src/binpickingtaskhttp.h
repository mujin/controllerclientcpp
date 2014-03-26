// -*- coding: utf-8 -*-
// Copyright (C) 2012-2014 MUJIN Inc. <rosen.diankov@mujin.co.jp>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file mujincontrollerclient.h
    \brief  Defines the public headers of the MUJIN Controller Client
 */
#ifndef MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_HTTP_H
#define MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_HTTP_H

#include <mujincontrollerclient/binpickingtask.h>

namespace mujinclient {

class MUJINCLIENT_API BinPickingResultResource : public PlanningResultResource
{
public:
    BinPickingResultResource(ControllerClientPtr controller, const std::string& pk);

    ~BinPickingResultResource();

    boost::property_tree::ptree GetResultPtree() const;
};

typedef boost::shared_ptr<BinPickingResultResource> BinPickingResultResourcePtr;
typedef boost::weak_ptr<BinPickingResultResource> BinPickingResultResourceWeakPtr;

class MUJINCLIENT_API BinPickingTaskResource : public BinPickingTask, public TaskResource
{
public:
    BinPickingTaskResource(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene);

    ~BinPickingTaskResource();

    PlanningResultResourcePtr GetResult();
    boost::property_tree::ptree ExecuteCommand(const std::string& command, const int timeout /* [sec] */, const bool getresult=true);

    void GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
    void DeleteObject(const std::string& name);
    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param detectedobjects list of detected object info including transform and confidence
     */
    void UpdateObjects(const std::string& basename, const std::vector<DetectedObject>& detectedobjects);

private:
    std::string _GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname);
    std::string _taskname;
    ControllerClientPtr _controller;
    SceneResourcePtr _scene;
};

typedef boost::shared_ptr<BinPickingTaskResource> BinPickingTaskResourcePtr;
typedef boost::weak_ptr<BinPickingTaskResource> BinPickingTaskResourceWeakPtr;

} // namespace mujinclient
#endif
