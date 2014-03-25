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

class MUJINCLIENT_API BinPickingResultHttpResource : public BinPickingResultResource, public PlanningResultResource
{
public:
    BinPickingResultHttpResource(ControllerClientPtr controller, const std::string& pk);

    ~BinPickingResultHttpResource();

private:
    void GetResultPtree(boost::property_tree::ptree& pt) const;
};

typedef boost::shared_ptr<BinPickingResultHttpResource> BinPickingResultHttpResourcePtr;
typedef boost::weak_ptr<BinPickingResultHttpResource> BinPickingResultHttpResourceWeakPtr;

class MUJINCLIENT_API BinPickingTaskHttpResource : public BinPickingTaskResource, public TaskResource
{
public:
    BinPickingTaskHttpResource(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene);

    ~BinPickingTaskHttpResource();

    /// \brief Get the task info for tasks of type <b>binpicking</b>
    void GetTaskParameters(BinPickingTaskParameters& params) const;
    /// \brief Set the task info for tasks of type <b>binpicking</b>
    void SetTaskParameters(const BinPickingTaskParameters& taskparameters);

    PlanningResultResourcePtr GetResult();
    void GetJointValues(const int timeout /* [sec] */, BinPickingResultResource::ResultGetJointValues& result);
    void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real speed /* 0.1-1 */, const int timeout /* [sec] */, BinPickingResultResource::ResultMoveJoints& result);
    void GetTransform(const std::string& targetname, Transform& result);
    void SetTransform(const std::string& targetname, const Transform& transform);
    void GetManipTransformToRobot(Transform& result);
    void GetManipTransform(Transform& result);
    void GetRobotTransform(Transform& result);
    void GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
    void DeleteObject(const std::string& name);
    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param detectedobjects list of detected object info including transform and confidence
     */
    void UpdateObjects(const std::string& basename, const std::vector<BinPickingTaskParameters::DetectedObject>& detectedobjects);

    /// \brief Establish ZMQ connection to the task
    void InitializeZMQ(const int zmqport);

    /** \brief Dynamically add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param pointsize size of each point in meter
        \param name name of the obstacle
     */
    void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name);

    void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool result);
    void GetPickedPositions(BinPickingResultResource::ResultPickedPositions& result);

private:
    std::string _GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname);
    std::string _robotcontrollerip; ///< robot controller ip
    int _robotcontrollerport; ///< robot controller port
    std::string _taskname;
    ControllerClientPtr _controller;
    SceneResourcePtr _scene;
    std::stringstream _sscmd;
};

typedef boost::shared_ptr<BinPickingTaskHttpResource> BinPickingTaskHttpResourcePtr;
typedef boost::weak_ptr<BinPickingTaskHttpResource> BinPickingTaskHttpResourceWeakPtr;

} // namespace mujinclient
#endif
