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
#ifndef MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_H
#define MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_H

#include <mujincontrollerclient/mujincontrollerclient.h>
#include <boost/property_tree/ptree.hpp>

namespace mujinclient {

/// \brief holds information about the binpicking task parameters
class MUJINCLIENT_API BinPickingTaskParameters
{

public:
    BinPickingTaskParameters();

    virtual ~BinPickingTaskParameters();

    void SetDefaults();

    struct DetectedObject
    {
        std::string name;
        std::vector<Real> translation; ///< in meter
        std::vector<Real> quaternion;
        Real confidence;
    };

    struct PointCloudObstacle
    {
        std::string name;
        std::vector<Real> points; ///< consecutive x,y,z values in meter
        Real pointsize; ///< size of each point in meter
    };

    struct SensorOcclusionCheck
    {
        std::string bodyname; ///< name of body whose visibility is to be checked
        std::string sensorname; ///< name of sensor
        unsigned long long starttime; ///< milisecond
        unsigned long long endtime; ///< milisecond
    };

    void GetJsonString(std::string& str) const;

    std::string command; ///< command to call
    std::string robottype; ///< the type of robot
    std::string robotcontrollerip; ///< the ip of the computer on which the robot controller runs
    int robotcontrollerport; ///< the port of the computer on which the robot controller runs
    std::vector<Real> goaljoints; ///< the joint values of goal point
    std::vector<int> jointindices; ///< the joint indices correspond to joint values
    int zmqport; ///< zmq port
    Real envclearance;
    Real speed;
    std::string targetname;
    Transform transform;
    std::vector<DetectedObject> detectedobjects;
    PointCloudObstacle pointcloudobstacle;
    SensorOcclusionCheck sensorocclusioncheck;

private:
    void GetJsonString(const std::vector<Real>& vec, std::string& str) const;
    void GetJsonString(const std::vector<int>& vec, std::string& str) const;
    void GetJsonString(const Transform& transform, std::string& str) const;
    void GetJsonString(const DetectedObject& obj, std::string& str) const;
    void GetJsonString(const PointCloudObstacle& obj, std::string& str) const;
    void GetJsonString(const SensorOcclusionCheck& check, std::string& str) const;

};

typedef boost::shared_ptr<BinPickingTaskParameters> BinPickingTaskParametersPtr;
typedef boost::weak_ptr<BinPickingTaskParameters> BinPickingTaskParametersWeakPtr;

class MUJINCLIENT_API BinPickingResultResource
{
public:

    BinPickingResultResource();

    virtual ~BinPickingResultResource();

    struct ResultGetJointValues
    {
        std::string robottype;
        std::vector<std::string> jointnames;
        std::vector<Real> currentjointvalues;
        std::map<std::string, Transform> tools;
    };

    struct ResultMoveJoints
    {
        std::string robottype;
        int numpoints;
        std::vector<Real>   timedjointvalues;
        //Real elapsedtime;
    };

    struct ResultPickedPositions
    {
        std::vector<Transform> transforms; // in meter
        std::vector<unsigned long long> timestamps; // in milisecond
    };

    void GetResultGetJointValues(ResultGetJointValues& result) const;
    void GetResultMoveJoints(ResultMoveJoints& result ) const;
    void GetResultTransform(Transform& result) const;
    void GetResultIsRobotOccludingBody(bool result) const;
    void GetResultGetPickedPositions(ResultPickedPositions& result) const;

protected:
    virtual void GetResultPtree(boost::property_tree::ptree& pt) const = 0;

    void GetResultGetJointValues(const boost::property_tree::ptree& output, ResultGetJointValues& result) const;
    void GetResultMoveJoints(const boost::property_tree::ptree& output, ResultMoveJoints& result) const;
    void GetResultTransform(const boost::property_tree::ptree& output, Transform& result) const;
    void GetResultIsRobotOccludingBody(const boost::property_tree::ptree& output, bool result) const;
    void GetResultGetPickedPositions(const boost::property_tree::ptree& output, ResultPickedPositions& result) const;
};

typedef boost::shared_ptr<BinPickingResultResource> BinPickingResultResourcePtr;
typedef boost::weak_ptr<BinPickingResultResource> BinPickingResultResourceWeakPtr;

class MUJINCLIENT_API BinPickingTaskResource
{
public:
    //BinPickingTaskResource(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene);
    BinPickingTaskResource();

    virtual ~BinPickingTaskResource();

    /// \brief Get the task info for tasks of type <b>binpicking</b>
    virtual void GetTaskParameters(BinPickingTaskParameters& taskparameters) const = 0; // TODO: do we need this?
    /// \brief Set the task info for tasks of type <b>binpicking</b>
    virtual void SetTaskParameters(const BinPickingTaskParameters& taskparameters) = 0;

    virtual PlanningResultResourcePtr GetResult() = 0;
    virtual void GetJointValues(const int timeout /* [sec] */, BinPickingResultResource::ResultGetJointValues& result) = 0;
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real speed /* 0.1-1 */, const int timeout /* [sec] */, BinPickingResultResource::ResultMoveJoints& result) = 0;
    virtual void GetTransform(const std::string& targetname, Transform& result) = 0;
    virtual void SetTransform(const std::string& targetname, const Transform& transform) = 0;
    virtual void GetManipTransformToRobot(Transform& result) = 0;
    virtual void GetManipTransform(Transform& result) = 0;
    virtual void GetRobotTransform(Transform& result) = 0;
    virtual void GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result) = 0;
    virtual void DeleteObject(const std::string& name) = 0;
    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param detectedobjects list of detected object info including transform and confidence
     */
    virtual void UpdateObjects(const std::string& basename, const std::vector<BinPickingTaskParameters::DetectedObject>& detectedobjects) = 0;

    /// \brief Establish ZMQ connection to the task
    virtual void InitializeZMQ(const int zmqport) = 0;

    /** \brief Dynamically add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param pointsize size of each point in meter
        \param name name of the obstacle
     */
    virtual void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name) = 0;

    /// \brief Check if robot is occluding the object in the view of sensor between starttime and endtime
    virtual void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool result) = 0;

    /// \brief Get the picked positions with corresponding timestamps
    virtual void GetPickedPositions(BinPickingResultResource::ResultPickedPositions& result) = 0;

};

typedef boost::shared_ptr<BinPickingTaskResource> BinPickingTaskResourcePtr;
typedef boost::weak_ptr<BinPickingTaskResource> BinPickingTaskResourceWeakPtr;

}

#endif
