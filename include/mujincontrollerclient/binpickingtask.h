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

enum MUJINCLIENT_API BinPickingTaskType
{
    MUJIN_BINPICKING_TASKTYPE_HTTP,
    MUJIN_BINPICKING_TASKTYPE_ZMQ
};

class MUJINCLIENT_API BinPickingTask
{
public:
    virtual ~BinPickingTask();

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

    struct ResultBase
    {
        virtual ~ResultBase();

        boost::property_tree::ptree _pt; ///< property tree of result, if user ever wants it for logging purposes

        virtual void Parse(const boost::property_tree::ptree& pt) = 0;
    };
    typedef boost::shared_ptr<ResultBase> ResultBasePtr;

    struct ResultGetJointValues : public ResultBase
    {
        virtual ~ResultGetJointValues();
        void Parse(const boost::property_tree::ptree& pt);
        std::string robottype;
        std::vector<std::string> jointnames;
        std::vector<Real> currentjointvalues;
        std::map<std::string, Transform> tools;
    };

    struct ResultMoveJoints : public ResultBase
    {
        virtual ~ResultMoveJoints();
        void Parse(const boost::property_tree::ptree& pt);
        std::string robottype;
        int numpoints;
        std::vector<Real>   timedjointvalues;
        //Real elapsedtime;
    };

    struct ResultTransform : public ResultBase
    {
        virtual ~ResultTransform();
        void Parse(const boost::property_tree::ptree& pt);
        Transform transform;
    };

    struct ResultIsRobotOccludingBody: public ResultBase
    {
        virtual ~ResultIsRobotOccludingBody();
        void Parse(const boost::property_tree::ptree& pt);
        bool result;
    };

    struct ResultGetPickedPositions : public ResultBase
    {
        virtual ~ResultGetPickedPositions();
        void Parse(const boost::property_tree::ptree& pt);
        std::vector<Transform> transforms; // in meter
        std::vector<unsigned long long> timestamps; // in milisecond
    };

    virtual boost::property_tree::ptree ExecuteCommand(const std::string& command, const int timeout /* [sec] */=10, const bool getresult=true) = 0;

    void GetJointValues(ResultGetJointValues& result);
    void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real envclearance, const Real speed /* 0.1-1 */, ResultMoveJoints& result);
    void GetTransform(const std::string& targetname, Transform& result);
    void SetTransform(const std::string& targetname, const Transform& transform);
    void GetManipTransformToRobot(Transform& result);
    void GetManipTransform(Transform& result);
    virtual void GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
    virtual void DeleteObject(const std::string& name);

    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param detectedobjects list of detected object info including transform and confidence
     */
    virtual void UpdateObjects(const std::string& basename, const std::vector<DetectedObject>& detectedobjects);

    /// \brief Establish ZMQ connection to the task
    void InitZMQ(const int zmqport);

    /** \brief Dynamically add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param pointsize size of each point in meter
        \param name name of the obstacle
     */
    void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name);

    /// \brief Check if robot is occluding the object in the view of sensor between starttime and endtime
    void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool result);

    /// \brief Get the picked positions with corresponding timestamps
    void GetPickedPositions(ResultGetPickedPositions& result);

    BinPickingTaskType tasktype;
    std::string _robotcontrollerip; ///< robot controller ip
    int _robotcontrollerport; ///< robot controller port

private:
    std::stringstream _ss;
    std::string GetJsonString(const std::vector<Real>& vec);
    std::string GetJsonString(const std::vector<int>& vec);
    std::string GetJsonString(const Transform& transform);
    std::string GetJsonString(const DetectedObject& obj);
    std::string GetJsonString(const PointCloudObstacle& obj);
    std::string GetJsonString(const SensorOcclusionCheck& check);
};

typedef boost::shared_ptr<BinPickingTask> BinPickingTaskPtr;
typedef boost::weak_ptr<BinPickingTask> BinPickingTaskWeakPtr;

MUJINCLIENT_API BinPickingTaskPtr CreateBinPickingTask(const BinPickingTaskType& tasktype, const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene, const std::string& controllerip, const int zmqport);
MUJINCLIENT_API void DestroyBinPickingTask();

}

#endif
