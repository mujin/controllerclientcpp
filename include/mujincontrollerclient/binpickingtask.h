// -*- coding: utf-8 -*-
// Copyright (C) 2012-2013 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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
#ifndef MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_H
#define MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_H

#include <mujincontrollerclient/mujincontrollerclient.h>

namespace mujinclient {

/// \brief holds information about the binpicking task parameters
class BinPickingTaskParameters
{
public:
    BinPickingTaskParameters() {
        SetDefaults();
    }

    virtual void SetDefaults();

    std::string GenerateJsonString (const std::vector<Real>& vec) const;
    std::string GenerateJsonString (const std::vector<int>& vec) const;

    std::string command; ///< command to call
    std::string robottype; ///< the type of robot
    std::string controllerip; ///< the ip of the computer on which the robot controller runs
    int controllerport; ///< the port of the computer on which the robot controller runs
    std::vector<Real> goaljoints; ///< the joint values of goal point
    std::vector<int>    jointindices;
    int port;
    Real envclearance;
    Real speed;
    std::string targetname;
    Transform transform;
};

class BinPickingTaskResource;
typedef boost::shared_ptr<BinPickingTaskResource> BinPickingTaskResourcePtr;
typedef boost::weak_ptr<BinPickingTaskResource> BinPickingTaskResourceWeakPtr;

class MUJINCLIENT_API BinPickingResultResource : public PlanningResultResource
{
public:
    BinPickingResultResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~BinPickingResultResource() {
    }
    class ResultGetJointValues
    {
public:
        std::string robottype;
        std::vector<std::string> jointnames;
        std::vector<Real> currentjointvalues;
        std::map<std::string, Transform> tools;
    };

    class ResultMoveJoints
    {
public:
        std::string robottype;
        int numpoints;
        std::vector<Real>   timedjointvalues;
        //Real elapsedtime;
    };

    void GetResultGetJointValues(ResultGetJointValues& result);
    void GetResultMoveJoints(ResultMoveJoints& result);
    void GetResultTransform(Transform& transform);
};
typedef boost::shared_ptr<BinPickingResultResource> BinPickingResultResourcePtr;

class MUJINCLIENT_API BinPickingTaskResource : public TaskResource
{
public:
    BinPickingTaskResource(const std::string& taskname, const std::string& controllerip, const int controllerport, ControllerClientPtr controller, SceneResourcePtr scene);

    virtual ~BinPickingTaskResource() {
    }

    virtual PlanningResultResourcePtr GetResult();
    virtual void GetJointValues(int timeout /* [sec] */, BinPickingResultResource::ResultGetJointValues& result);
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, Real speed /* 0.1-1 */, int timeout /* [sec] */, BinPickingResultResource::ResultMoveJoints& result);
    virtual Transform GetTransform(const std::string& targetname);
    virtual void SetTransform(const std::string& targetname, const Transform& transform);
    virtual Transform GetManipTransformToRobot();
    virtual Transform GetManipTransform();
    virtual Transform GetRobotTransform();
    virtual void InitializeZMQ(int zmqport);

    /// \brief Dynamically add a point cloud collision obstacle with name to the environment.
    virtual void AddPointCloudObstacle(const std::vector<Real>& vpoints, Real pointsize, const std::string& name);

    /// \brief Get the task info for tasks of type <b>binpicking</b>
    virtual void GetTaskParameters(BinPickingTaskParameters& taskparameters);

    /// \brief Set the task info for tasks of type <b>binpicking</b>
    virtual void SetTaskParameters(const BinPickingTaskParameters& taskparameters);
private:
    std::string _GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname);
    std::string _controllerip;
    int _controllerport;
    std::string _taskname;
};

}

#endif
