// -*- coding: utf-8 -*-
// Copyright (C) 2012-2016 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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
#ifdef MUJIN_USEZMQ
#include "zmq.hpp"
#endif

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
    BinPickingTaskResource(ControllerClientPtr controller, const std::string& pk, const std::string& scenepk, const std::string& tasktype = "binpicking");
    virtual ~BinPickingTaskResource();
    
    struct MUJINCLIENT_API DetectedObject
    {
        std::string name;             // "name": "detectionresutl_1"
        std::string object_uri;       // "object_uri": "mujin:/box0.mujin.dae"
        Transform transform;  
        std::string confidence;
        unsigned long long timestamp;
        std::string extra;            // (OPTIONAL) "extra": {"type":"randombox", "length":100, "width":100, "height":100}
        bool isPickable; ///< whether the object is pickable
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

    struct MUJINCLIENT_API ResultGetBinpickingState : public ResultBase
    {
        ResultGetBinpickingState();
        virtual ~ResultGetBinpickingState();
        void Parse(const boost::property_tree::ptree& pt);
        std::string statusPickPlace;
        std::string statusDescPickPlace;
        std::string statusPhysics;
        bool isDynamicEnvironmentStateEmpty;
        bool isSourceContainerEmpty;
        int pickAttemptFromSourceId;
        unsigned long long timestamp;  ///< ms
        unsigned long long lastGrabbedTargetTimeStamp;   ///< ms
        bool isRobotOccludingSourceContainer;
        bool forceRequestDetectionResults;
        bool isGrabbingTarget;
        bool isGrabbingLastTarget;
        bool hasRobotExecutionStarted;
        int orderNumber;
        int numLeftInOrder;
        int numDetectionResults;
        int placedInDest;
        std::vector<double> currentJointValues;
        std::vector<double> currentToolValues;
        std::vector<std::string> jointNames;
        bool isControllerInError;
        std::string robotbridgestatus;
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

    struct MUJINCLIENT_API ResultOBB : public ResultBase
    {
        void Parse(const boost::property_tree::ptree& pt);
        std::vector<Real> translation;
        std::vector<Real> extents;
        std::vector<Real> rotationmat;  // row major
    };

    struct MUJINCLIENT_API ResultGetInstObjectAndSensorInfo : public ResultBase
    {
        virtual ~ResultGetInstObjectAndSensorInfo();
        void Parse(const boost::property_tree::ptree& pt);
        std::map<std::string, Transform> minstobjecttransform; ///< unit is m
        std::map<std::string, ResultOBB> minstobjectobb; ///< unit is m
        std::map<std::string, ResultOBB> minstobjectinnerobb; ///< unit is m
        std::map<std::string, Transform> msensortransform; ///< unit is m
        std::map<std::string, RobotResource::AttachedSensorResource::SensorData> msensordata;
    };

    struct MUJINCLIENT_API ResultHeartBeat : public ResultBase
    {
        virtual ~ResultHeartBeat();
        void Parse(const boost::property_tree::ptree& pt);
        std::string status;
        ResultGetBinpickingState taskstate;
        Real timestamp;
        std::string msg;
        std::string _slaverequestid;
    };

    /** \brief Initializes binpicking task.
        \param commandtimeout seconds until this command times out
        \param userinfo json string user info, such as locale
        \param slaverequestid id of mujincontroller planning slave to connect to
     */
    virtual void Initialize(const std::string& defaultTaskParameters, const double commandtimeout=5.0, const std::string& userinfo="{}", const std::string& slaverequestid="");

#ifdef MUJIN_USEZMQ
    /** \brief Initializes binpicking task.
        \param zmqPort port of the binpicking zmq server
        \param heartbeatPort port of the binpicking zmq server's heartbeat publisher
        \param zmqcontext zmq context
        \param initializezmq whether to call InitializeZMQ() in this call
        \param reinitializetimeout seconds until calling InitailizeZMQ() if heartbeat has not been received. If 0, do not reinitialize
        \param commandtimeout seconds until this command times out
        \param userinfo json string user info, such as locale
        \param slaverequestid id of mujincontroller planning slave to connect to
     */
    virtual void Initialize(const std::string& defaultTaskParameters, const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq=false, const double reinitializetimeout=10, const double commandtimeout=0, const std::string& userinfo="{}", const std::string& slaverequestid="");
#endif

    virtual boost::property_tree::ptree ExecuteCommand(const std::string& command, const double timeout /* second */=5.0, const bool getresult=true);

    virtual void GetJointValues(ResultGetJointValues& result, const double timeout /* second */=5.0);

    /// \brief Moves joints to specified value
    /// \param jointvalues goal joint values
    /// \param jointindices indices of joints to move
    /// \param envclearance environment clearance for collision avoidance in mm
    /// \param speed speed to move robot at
    /// \param result result of moving joints
    /// \param timeout timeout of communication
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real envclearance, const Real speed /* 0.1-1 */, ResultMoveJoints& result, const double timeout /* second */=5.0, std::string* pTraj = NULL);
    virtual void GetTransform(const std::string& targetname, Transform& result, const std::string& unit="mm", const double timeout /* second */=5.0);
    virtual void SetTransform(const std::string& targetname, const Transform& transform, const std::string& unit="mm", const double timeout /* second */=5.0);
    virtual void GetManipTransformToRobot(Transform& result, const std::string& unit="mm", const double timeout /* second */=5.0);
    virtual void GetManipTransform(Transform& result, const std::string& unit="mm", const double timeout /* second */=5.0);

    virtual void GetAABB(const std::string& targetname, ResultAABB& result, const std::string& unit="mm", const double timeout=0);
    virtual void GetOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname="", const std::string& unit="mm", const double timeout=0);
    virtual void GetInnerEmptyRegionOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname="", const std::string& unit="mm", const double timeout=0);

    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param transformsworld list of transforms in world frame
        \param confidence list of confidence of each detection
        \param state additional information about the objects
        \param unit unit of detectedobject info
        \param timeout seconds until this command times out
     */
    virtual void UpdateObjects(const std::string& basename, const std::vector<Transform>& transformsworld, const std::vector<std::string>& confidence, const std::string& state, const std::string& unit="mm", const double timeout /* second */=5.0);

    /** \brief Establish ZMQ connection to the task
        \param reinitializetimeout seconds to wait before re-initializing the ZMQ server after the heartbeat signal is lost
               if reinitializetimeout is 0, then this does not invoke heartbeat monitoring thread
        \param timeout seconds until this command times out
     */
    virtual void InitializeZMQ(const double reinitializetimeout = 0,const double timeout /* second */=5.0);

    /** \brief Add a point cloud collision obstacle with name to the environment.
        \param vpoints list of x,y,z coordinates in meter
        \param state additional information about the objects
        \param pointsize size of each point in meter
        \param name name of the obstacle
        \param isoccluded occlusion status of robot with the container: -1 if unknown, 0 if not occluding, 1 if robot is occluding region in camera
	\param regionname the name of the region for which the point cloud is supposed to be captured of. isoccluded maps to this region
        \param timeout seconds until this command times out
     */
    virtual void AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name,  const unsigned long long starttimestamp=0, const unsigned long long endtimestamp=0, const bool executionverification=false, const std::string& unit="mm", int isoccluded=-1, const std::string& regionname=std::string(), const double timeout /* second */=5.0);
    
    /// \param locationIOName the location IO name (1, 2, 3, 4, etc) used to tell mujin controller to notify  the IO signal with detected object info
    /// \param cameranames the names of the sensors mapped to the current region used for detetion. The sensor information is used to create shadow obstacles per each part, if empty, will not be able to create the correct shadow obstacles.
    virtual void UpdateEnvironmentState(const std::string& objectname, const std::vector<DetectedObject>& detectedobjects, const std::vector<Real>& vpoints, const std::string& resultstate, const Real pointsize, const std::string& pointcloudobstaclename, const std::string& unit="mm", const double timeout=0, const std::string& regionname=std::string(), const std::string& locationIOName="1", const std::vector<std::string>& cameranames=std::vector<std::string>());

    /// \brief removes objects by thier prefix
    /// \param prefix prefix of the objects to remove
    virtual void RemoveObjectsWithPrefix(const std::string& prefix, double timeout = 5.0);

    /** \brief Visualize point cloud on controller
        \param pointslist vector of x,y,z coordinates vector in meter
        \param pointsize size of each point in meter
        \param names vector of names for each point cloud
        \param unit of points
        \param timeout seconds until this command times out
     */
    virtual void VisualizePointCloud(const std::vector<std::vector<Real> >& pointslist, const Real pointsize, const std::vector<std::string>& names, const std::string& unit="m", const double timeout /* second */=5.0);

    /** \brief Clear visualization made by VisualizePointCloud.
     */
    virtual void ClearVisualization(const double timeout /* second */=5.0);

    /** \brief Check if robot is occluding the object in the view of sensor between starttime and endtime
        \param sensorname name of the sensor to be checked, example names: "sensor_kinbodyname/sensor_name" or "sensor_kinbodyname", in the latter case the first attached sensor will be used
        \param timeout seconds until this command times out
     */
    virtual void IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool& result, const double timeout /* second */=5.0);

    /** \brief Get the picked positions with corresponding timestamps
        \param timeout seconds until this command times out
     */
    virtual void GetPickedPositions(ResultGetPickedPositions& result, const std::string& unit="m", const double timeout /* second */=5.0);

    virtual PlanningResultResourcePtr GetResult();

    /** \brief Gets inst object
        \param unit input unit
        \param result unit is always in meter
     */
    virtual void GetInstObjectAndSensorInfo(const std::vector<std::string>& instobjectnames, const std::vector<std::string>& sensornames, ResultGetInstObjectAndSensorInfo& result, const std::string& unit="m", const double timeout /* second */=5.0);

    /// \brief Get state of bin picking
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetBinpickingState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit="m", const double timeout /* second */=5.0);

    /// \brief Get state of ITL which includes picking status
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetITLState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit="m", const double timeout /* second */=5.0);
    
    /// \brief Get published state of bin picking
    /// except for initial call, this returns cached value.
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetPublishedTaskState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit="m", const double timeout /* second */=5.0);

    /// \brief Jogs robot in joint or tool space
    /// \param jogtype type of jogging, either "joints" or "tool"
    /// \param goals where to move hand to [X, Y, Z, RX, RY, RZ] in mm and deg
    /// \param robotname name of the robot to move
    /// \param toolname name of the tool to move
    /// \param robotspeed speed at which to move. this is a ratio to maximum speed and thus valid range is 0 to 1.
    /// \param robotaccelmult acceleration multiplier at which to move. this is a ratio to maximum acceleration and thus valid range is 0 to 1.
    /// \param timeout timeout of communication
    virtual void SetJogModeVelocities(const std::string& jogtype, const std::vector<int>& movejointsigns, const std::string& robotname = "", const std::string& toolname = "", const double robotspeed = -1, const double robotaccelmult = -1.0, const double timeout=1);

    /// \brief Moves tool to specified posistion linearly
    /// \param goaltype whether to specify goal in full six degrees of freedom (transform6d) or three dimentional position and two dimentional angle (translationdirection5d)
    /// \param goals where to move tool to [X, Y, Z, RX, RY, RZ] in mm and deg
    /// \param robotname name of the robot to move
    /// \param toolname name of the tool to move
    /// \param workspeedlin linear speed at which to move tool in mm/s.
    /// \param workspeedrot rotational speed at which to move tool in deg/s
    /// \param timeout timeout of communication
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveToolLinear(const std::string& goaltype, const std::vector<double>& goals, const std::string& robotname = "", const std::string& toolname = "", const double workspeedlin = -1, const double workspeedrot = -1, bool checkEndeffectorCollision = false, const double timeout = 10, std::string* pTraj = NULL);

    /// \brief Moves hand to specified posistion
    /// \param goaltype whether to specify goal in full six degrees of freedom (transform6d) or three dimentional position and two dimentional angle (translationdirection5d)
    /// \param goals where to move hand to [X, Y, Z, RX, RY, RZ] in mm and deg
    /// \param robotname name of the robot to move
    /// \param toolname name of the tool to move
    /// \param robotspeed speed at which to move
    /// \param timeout timeout of communication
    /// \param envclearance environment clearance for collision avoidance in mm
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveToHandPosition(const std::string& goaltype, const std::vector<double>& goals, const std::string& robotname = "", const std::string& toolname = "", const double robotspeed = -1, const double timeout = 10, Real envclearance = -1.0, std::string* pTraj = NULL);

    /// \brief Executes a trajectory
    /// \param trajectory trajectory to execute
    /// \param filterTraj whether to filter trajectory so that it is smoother. For slow trajectory, filtering is not necessary.
    /// \param timeout timeout of executing trajectory
    virtual void ExecuteSingleXMLTrajectory(const std::string& trajectory, bool filterTraj = true, const double timeout = 10);

    /// \brief grabs object
    /// \param targetname name of the target to grab
    /// \param robotname name of the robot to grab with
    /// \param toolname name of the tool to grab with
    /// \param timeout timeout of communication
    virtual void Grab(const std::string& targetname, const std::string& robotname = "", const std::string& toolname = "", const double timeout = 1);

    /// \brief releases object
    /// \param targetname name of the target to release
    /// \param robotname name of the robot to release from
    /// \param toolname name of the tool to release from
    /// \param timeout timeout of communication
    virtual void Release(const std::string& targetname, const std::string& robotname = "", const std::string& toolname = "", const double timeout = 1);

    /** \brief Monitors heartbeat signals from a running binpicking ZMQ server, and reinitializes the ZMQ server when heartbeat is lost.
        \param reinitializetimeout seconds to wait before re-initializing the ZMQ server after the heartbeat signal is lost
        \param execfn function to use to execute the InitializeZMQ command
     */
    virtual void _HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout);

    /// \brief returns the slaverequestid used to communicate with the controller. If empty, then no id is used.
    virtual const std::string& GetSlaveRequestId() const;

protected:
    std::stringstream _ss;

    std::map<std::string, std::string> _mapTaskParameters; ///< set of key value pairs that should be included
    std::string _mujinControllerIp;
    boost::mutex _mutexTaskState;
    ResultGetBinpickingState _taskstate;
#ifdef MUJIN_USEZMQ
    boost::shared_ptr<zmq::context_t> _zmqcontext;
#endif
    int _zmqPort;
    int _heartbeatPort;
    std::string _userinfo_json;  ///< userinfo json
    std::string _sceneparams_json; ///\ parameters of the scene to run tasks on the backend zmq slave
    std::string _slaverequestid; ///< to ensure the same slave is used for binpicking task
    std::string _scenepk; ///< scene pk
    const std::string _tasktype; ///< the specific task type to create internally. As long as the task supports the binpicking interface, it can be used.
    boost::shared_ptr<boost::thread> _pHeartbeatMonitorThread;

    bool _bIsInitialized;
    bool _bShutdownHeartbeatMonitor;

};

namespace utils {
std::string GetJsonString(const std::string& string);
std::string GetJsonString(const std::vector<Real>& vec);
std::string GetJsonString(const std::vector<int>& vec);
std::string GetJsonString(const std::vector<std::string>& vec);
std::string GetJsonString(const Transform& transform);
std::string GetJsonString(const BinPickingTaskResource::DetectedObject& obj);
std::string GetJsonString(const BinPickingTaskResource::PointCloudObstacle& obj);
std::string GetJsonString(const BinPickingTaskResource::SensorOcclusionCheck& check);

std::string GetJsonString(const std::string& key, const std::string& value);
std::string GetJsonString(const std::string& key, const int value);
std::string GetJsonString(const std::string& key, const unsigned long long value);
std::string GetJsonString(const std::string& key, const Real value);

void GetAttachedSensors(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, std::vector<RobotResource::AttachedSensorResourcePtr>& result);
void GetSensorData(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
/** \brief Gets transform of attached sensor in sensor body frame
  */
void GetSensorTransform(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, Transform& result, const std::string& unit="m");
void DeleteObject(SceneResourcePtr scene, const std::string& name);
void UpdateObjects(SceneResourcePtr scene, const std::string& basename, const std::vector<BinPickingTaskResource::DetectedObject>& detectedobjects, const std::string& unit="m");


#ifdef MUJIN_USEZMQ
/// \brief get heatbeat
/// \param endopoint endpoint to get heartbeat from. looks like protocol://hostname:port (ex. tcp://localhost:11001)
/// \return heartbeat as string
MUJINCLIENT_API std::string GetHeartbeat(const std::string& endpoint);

MUJINCLIENT_API std::string GetScenePkFromHeatbeat(const std::string& heartbeat);
MUJINCLIENT_API std::string GetSlaveRequestIdFromHeatbeat(const std::string& heartbeat);
#endif
} // namespace utils

} // namespace mujinclient

#endif
