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

class MUJINCLIENT_API BinPickingResultResource : public PlanningResultResource
{
public:
    BinPickingResultResource(ControllerClientPtr controller, const std::string& pk);

    ~BinPickingResultResource();

    boost::property_tree::ptree GetResultPtree() const;
};

class MUJINCLIENT_API BinPickingTaskResource : public TaskResource
{
public:
    BinPickingTaskResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~BinPickingTaskResource();

    struct DetectedObject
    {
        std::string name;
        Transform transform;
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

    struct ResultIsRobotOccludingBody : public ResultBase
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

    virtual void Initialize(const std::string& robotcontrollerip, const int robotcontrollerport, const int zmqport);

    virtual boost::property_tree::ptree ExecuteCommand(const std::string& command, const double timeout /* second */=0.0, const bool getresult=true);

    virtual void GetJointValues(ResultGetJointValues& result, const double timeout /* second */=0);
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real envclearance, const Real speed /* 0.1-1 */, ResultMoveJoints& result, const double timeout /* second */=0);
    virtual void GetTransform(const std::string& targetname, Transform& result, const double timeout /* second */=0);
    virtual void SetTransform(const std::string& targetname, const Transform& transform, const double timeout /* second */=0);
    virtual void GetManipTransformToRobot(Transform& result, const double timeout /* second */=0);
    virtual void GetManipTransform(Transform& result, const double timeout /* second */=0);

    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param transformsworld list of transforms in world frame
        \param confidence list of confidence of each detection
        \param unit unit of detectedobject info
     */
    virtual void UpdateObjects(const std::string& basename, const std::vector<Transform>& transformsworld, const std::vector<Real>& confidence, const std::string& unit="m", const double timeout /* second */=0);

    /// \brief Establish ZMQ connection to the task
    virtual void InitializeZMQ(const double timeout /* second */=0);

    /** \brief Add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param pointsize size of each point in meter
        \param name name of the obstacle
     */
    virtual void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name, const double timeout /* second */=0);

    /** \brief Visualize point cloud on controller
        \param pointslist vector of x,y,z coordinates vector in meter
        \param pointsize size of each point in meter
        \param names vector of names for each point cloud
     */
    virtual void VisualizePointCloud(const std::vector<std::vector<Real> >& pointslist, const Real pointsize, const std::vector<std::string>& names, const double timeout /* second */=0);

    /** \brief Clear visualization made by VisualizePointCloud.
     */
    virtual void ClearVisualization(const double timeout /* second */=0);

    /// \brief Check if robot is occluding the object in the view of sensor between starttime and endtime
    virtual void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool result, const double timeout /* second */=0);

    /// \brief Get the picked positions with corresponding timestamps
    virtual void GetPickedPositions(ResultGetPickedPositions& result, const double timeout /* second */=0);

    virtual PlanningResultResourcePtr GetResult();

protected:
    std::string _robotcontrollerip;
    int _robotcontrollerport;
    int _zmqport;
    bool _bIsInitialized;

private:

    std::stringstream _ss;
    std::string GetJsonString(const std::string& string);
    std::string GetJsonString(const std::vector<Real>& vec);
    std::string GetJsonString(const std::vector<int>& vec);
    std::string GetJsonString(const Transform& transform);
    std::string GetJsonString(const DetectedObject& obj);
    std::string GetJsonString(const PointCloudObstacle& obj);
    std::string GetJsonString(const SensorOcclusionCheck& check);
};


namespace utils {
void GetSensorData(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
void DeleteObject(SceneResourcePtr scene, const std::string& name);
void UpdateObjects(SceneResourcePtr scene, const std::string& basename, const std::vector<BinPickingTaskResource::DetectedObject>& detectedobjects, const std::string& unit="m");
};

} // namespace mujinclient

#endif
