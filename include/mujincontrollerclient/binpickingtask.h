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
#include <boost/thread.hpp>
#include "zmq.hpp"

#ifndef USE_LOG4CPP // logging

#define BINPICKING_LOG_INFO(msg) std::cout << msg << std::endl;
#define BINPICKING_LOG_ERROR(msg) std::cerr << msg << std::endl;

#else

#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>

LOG4CPP_LOGGER_N(mujincontrollerclientbinpickinglogger, "mujincontrollerclient.binpickingtask");

#define BINPICKING_LOG_INFO(msg) LOG4CPP_INFO_S(mujincontrollerclientbinpickinglogger) << msg;
#define BINPICKING_LOG_ERROR(msg) LOG4CPP_ERROR_S(mujincontrollerclientbinpickinglogger) << msg;

#endif // logging


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
    BinPickingTaskResource(ControllerClientPtr controller, const std::string& pk, const std::string& scenepk);
    virtual ~BinPickingTaskResource();
    
    struct MUJINCLIENT_API DetectedObject
    {
        std::string name;             // "name": "detectionresutl_1"
        std::string object_uri;       // "object_uri": "mujin:/box0.mujin.dae"
        Transform transform;  
        std::string confidence;
        unsigned long long timestamp;
        std::string extra;            // (OPTIONAL) "extra": {"type":"randombox", "length":100, "width":100, "height":100}
    };

    struct MUJINCLIENT_API PointCloudObstacle
    {
        std::string name;
        std::vector<Real> points; ///< consecutive x,y,z values in meter
        Real pointsize; ///< size of each point in meter
    };

    struct MUJINCLIENT_API SensorOcclusionCheck
    {
        std::string bodyname; ///< name of body whose visibility is to be checked
        std::string cameraname; ///< name of camera
        unsigned long long starttime; ///< milisecond
        unsigned long long endtime; ///< milisecond
    };

    struct MUJINCLIENT_API ResultBase
    {
        boost::property_tree::ptree _pt; ///< property tree of result, if user ever wants it for logging purposes

        virtual void Parse(const boost::property_tree::ptree& pt) = 0;
    };
    typedef boost::shared_ptr<ResultBase> ResultBasePtr;

    struct MUJINCLIENT_API ResultGetJointValues : public ResultBase
    {
        virtual ~ResultGetJointValues();
        void Parse(const boost::property_tree::ptree& pt);
        std::string robottype;
        std::vector<std::string> jointnames;
        std::vector<Real> currentjointvalues;
        std::map<std::string, Transform> tools;
    };

    struct MUJINCLIENT_API ResultMoveJoints : public ResultBase
    {
        virtual ~ResultMoveJoints();
        void Parse(const boost::property_tree::ptree& pt);
        std::string robottype;
        int numpoints;
        std::vector<Real>   timedjointvalues;
        //Real elapsedtime;
    };

    struct MUJINCLIENT_API ResultTransform : public ResultBase
    {
        virtual ~ResultTransform();
        void Parse(const boost::property_tree::ptree& pt);
        Transform transform;
    };

    struct MUJINCLIENT_API ResultGetInstObjectAndSensorInfo : public ResultBase
    {
        virtual ~ResultGetInstObjectAndSensorInfo();
        void Parse(const boost::property_tree::ptree& pt);
        std::map<std::string, Transform> minstobjecttransform;
        std::map<std::string, Transform> msensortransform;
        std::map<std::string, RobotResource::AttachedSensorResource::SensorData> msensordata;
    };

    struct MUJINCLIENT_API ResultGetBinpickingState : public ResultBase
    {
        virtual ~ResultGetBinpickingState();
        void Parse(const boost::property_tree::ptree& pt);
        std::string statusPickPlace;
        int pickAttemptFromSourceId;
        unsigned long long timestamp;
        bool isRobotOccludingSourceContainer;
        bool forceRequestDetectionResults;
    };

    struct MUJINCLIENT_API ResultIsRobotOccludingBody : public ResultBase
    {
        virtual ~ResultIsRobotOccludingBody();
        void Parse(const boost::property_tree::ptree& pt);
        bool result;
    };

    struct MUJINCLIENT_API ResultGetPickedPositions : public ResultBase
    {
        void Parse(const boost::property_tree::ptree& pt);
        std::vector<Transform> transforms;
        std::vector<unsigned long long> timestamps; // in millisecond
    };

    struct MUJINCLIENT_API ResultAABB : public ResultBase
    {
        void Parse(const boost::property_tree::ptree& pt);
        std::vector<Real> pos;
        std::vector<Real> extents;
    };

    struct MUJINCLIENT_API ResultHeartBeat : public ResultBase
    {
        virtual ~ResultHeartBeat();
        void Parse(const boost::property_tree::ptree& pt);
        std::string status;
        std::string taskstate;
        Real timestamp;
        std::string msg;
    };

    /** \brief Initializes binpicking task.
        \param robotControllerUri URI of the robot controller, e.g. tcp://127.0.0.1:1234?param0=0,param1=1
        \param robotDeviceIOUri URI of the robot IO device, e.g. tcp://127.0.0.1:2345?param0=0,param1=1
        \param zmqPort port of the binpicking zmq server
        \param heartbeatPort port of the binpicking zmq server's heartbeat publisher
        \param zmqcontext zmq context
        \param initializezmq whether to call InitializeZMQ() in this call
        \param reinitializetimeout seconds until calling InitailizeZMQ() if heartbeat has not been received. If 0, do not reinitialize
        \param timeout seconds until this command times out
        \param locale language of the log/status
     */
    virtual void Initialize(const std::string& robotControllerUri, const std::string& robotDeviceIOUri, const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq=false, const double reinitializetimeout=10, const double commandtimeout=0, const std::string& locale="");

    virtual boost::property_tree::ptree ExecuteCommand(const std::string& command, const double timeout /* second */=0.0, const bool getresult=true);

    virtual void GetJointValues(ResultGetJointValues& result, const double timeout /* second */=0);
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real envclearance, const Real speed /* 0.1-1 */, ResultMoveJoints& result, const double timeout /* second */=0);
    virtual void GetTransform(const std::string& targetname, Transform& result, const std::string& unit="mm", const double timeout /* second */=0);
    virtual void SetTransform(const std::string& targetname, const Transform& transform, const std::string& unit="mm", const double timeout /* second */=0);
    virtual void GetManipTransformToRobot(Transform& result, const std::string& unit="mm", const double timeout /* second */=0);
    virtual void GetManipTransform(Transform& result, const std::string& unit="mm", const double timeout /* second */=0);

    virtual void GetAABB(const std::string& targetname, ResultAABB& result, const std::string& unit="mm", const double timeout=0);

    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param transformsworld list of transforms in world frame
        \param confidence list of confidence of each detection
        \param state additional information about the objects
        \param unit unit of detectedobject info
        \param timeout seconds until this command times out
     */
    virtual void UpdateObjects(const std::string& basename, const std::vector<Transform>& transformsworld, const std::vector<std::string>& confidence, const std::string& state, const std::string& unit="m", const double timeout /* second */=0);

    /** \brief Establish ZMQ connection to the task
        \param reinitializetimeout seconds to wait before re-initializing the ZMQ server after the heartbeat signal is lost
               if reinitializetimeout is 0, then this does not invoke heartbeat monitoring thread
        \param timeout seconds until this command times out
     */
    virtual void InitializeZMQ(const double reinitializetimeout = 0,const double timeout /* second */=0);

    /** \brief Add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param state additional information about the objects
        \param pointsize size of each point in meter
        \param name name of the obstacle
        \param timeout seconds until this command times out
     */
    virtual void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name, const double timeout /* second */=0);

    virtual void UpdateEnvironmentState(const std::string& basename, const std::string& default_object_uri, const std::vector<DetectedObject>& detectedobjects, const std::vector<Real>& vpoints, const std::string& resultstate, const Real pointsize, const std::string& pointcloudobstaclename, const std::string& unit="m", const double timeout=0);

    /** \brief Visualize point cloud on controller
        \param pointslist vector of x,y,z coordinates vector in meter
        \param pointsize size of each point in meter
        \param names vector of names for each point cloud
        \param timeout seconds until this command times out
     */
    virtual void VisualizePointCloud(const std::vector<std::vector<Real> >& pointslist, const Real pointsize, const std::vector<std::string>& names, const double timeout /* second */=0);

    /** \brief Clear visualization made by VisualizePointCloud.
     */
    virtual void ClearVisualization(const double timeout /* second */=0);

    /** \brief Check if robot is occluding the object in the view of sensor between starttime and endtime
        \param sensorname name of the sensor to be checked, example names: "sensor_kinbodyname/sensor_name" or "sensor_kinbodyname", in the latter case the first attached sensor will be used
        \param timeout seconds until this command times out
     */
    virtual void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool& result, const double timeout /* second */=0);

    /** \brief Get the picked positions with corresponding timestamps
        \param timeout seconds until this command times out
     */
    virtual void GetPickedPositions(ResultGetPickedPositions& result, const std::string& unit="m", const double timeout /* second */=0);

    virtual PlanningResultResourcePtr GetResult();

    /** \brief Gets inst object 
     */
    virtual void GetInstObjectAndSensorInfo(const std::vector<std::string>& instobjectnames, const std::vector<std::string>& sensornames, ResultGetInstObjectAndSensorInfo& result, const std::string& unit="m", const double timeout /* second */=0);

    virtual void GetBinpickingState(ResultGetBinpickingState& result, const std::string& unit="m", const double timeout /* second */=0);
    virtual void GetPublishedTaskState(ResultGetBinpickingState& result, const std::string& unit="m", const double timeout /* second */=0);

    /** \brief Monitors heartbeat signals from a running binpicking ZMQ server, and reinitializes the ZMQ server when heartbeat is lost.
        \param reinitializetimeout seconds to wait before re-initializing the ZMQ server after the heartbeat signal is lost
        \param execfn function to use to execute the InitializeZMQ command
     */
    virtual void _HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout);

protected:
    std::stringstream _ss;
    std::string GetJsonString(const std::string& string);
    std::string GetJsonString(const std::vector<Real>& vec);
    std::string GetJsonString(const std::vector<int>& vec);
    std::string GetJsonString(const std::vector<std::string>& vec);
    std::string GetJsonString(const Transform& transform);
    std::string GetJsonString(const DetectedObject& obj);
    std::string GetJsonString(const PointCloudObstacle& obj);
    std::string GetJsonString(const SensorOcclusionCheck& check);

    std::string GetJsonString(const std::string& key, const std::string& value);
    std::string GetJsonString(const std::string& key, const int value);
    std::string GetJsonString(const std::string& key, const unsigned long long value);
    std::string GetJsonString(const std::string& key, const Real value);

    std::string _robotControllerUri;
    std::string _robotDeviceIOUri;
    std::string _mujinControllerIp;
    boost::mutex _mutexTaskState;
    std::string _taskstate;
    boost::shared_ptr<zmq::context_t> _zmqcontext;
    int _zmqPort;
    int _heartbeatPort;
    std::string _userinfo_json;  ///< userinfo json
    std::string _sceneparams_json; ///\ parameters of the scene to run tasks on the backend zmq slave
    boost::shared_ptr<boost::thread> _pHeartbeatMonitorThread;

    bool _bIsInitialized;
    bool _bShutdownHeartbeatMonitor;

};

namespace utils {

void GetAttachedSensors(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, std::vector<RobotResource::AttachedSensorResourcePtr>& result);
void GetSensorData(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
/** \brief Gets transform of attached sensor in sensor body frame
  */
void GetSensorTransform(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, Transform& result, const std::string& unit="m");
void DeleteObject(SceneResourcePtr scene, const std::string& name);
void UpdateObjects(SceneResourcePtr scene, const std::string& basename, const std::vector<BinPickingTaskResource::DetectedObject>& detectedobjects, const std::string& unit="m");

}; // namespace utils

} // namespace mujinclient

#endif
