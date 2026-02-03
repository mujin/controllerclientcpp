// -*- coding: utf-8 -*-
// Copyright (C) 2012-2020 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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
#include <boost/thread.hpp>
#ifdef MUJIN_USEZMQ
#include "zmq.hpp"
#endif

namespace mujinclient {

/// Margins of the container to be cropped (or enlarged if negative), in order to define 3D container region under (calibration & shape) uncertainty - for pointcloud processing.
struct CropContainerMarginsXYZXYZ
{
    double minMargins[3] = {0, 0, 0}; ///< XYZ of how much to crop from min margins (value > 0 means crop inside)
    double maxMargins[3] = {0, 0, 0}; ///< XYZ of how much to crop from min margins (value > 0 means crop inside)
};

typedef boost::shared_ptr<CropContainerMarginsXYZXYZ> CropContainerMarginsXYZXYZPtr;

class MUJINCLIENT_API BinPickingResultResource : public PlanningResultResource
{
public:
    BinPickingResultResource(ControllerClientPtr controller, const std::string& pk);

    ~BinPickingResultResource();

    void GetResultJson(rapidjson::Document& pt) const;
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
        Transform transform; ///< pose of the object in the world coordinate system
        std::string confidence;
        unsigned long long timestamp = 0; ///< sensor timestamp in ms (from Linux epoch)
        std::string extra;            // (OPTIONAL) "extra": {"type":"randombox", "length":100, "width":100, "height":100}
        bool isPickable = false; ///< whether the object is pickable
    };

    struct MUJINCLIENT_API PointCloudObstacle
    {
        std::string name;
        std::vector<float> points; ///< consecutive x,y,z values in meter
        Real pointsize = 0; ///< size of each point in meter
    };

    struct MUJINCLIENT_API SensorOcclusionCheck
    {
        std::string bodyname; ///< name of body whose visibility is to be checked
        std::string cameraname; ///< name of camera
        unsigned long long starttime = 0; ///< milisecond
        unsigned long long endtime = 0; ///< milisecond
    };

    struct MUJINCLIENT_API ResultBase
    {
        virtual ~ResultBase() {}
        virtual void Parse(const rapidjson::Value& pt) = 0;
    };
    typedef boost::shared_ptr<ResultBase> ResultBasePtr;

    struct MUJINCLIENT_API AddPointOffsetInfo
    {
        AddPointOffsetInfo() : zOffsetAtBottom(0), zOffsetAtTop(0) {
        }
        double zOffsetAtBottom = 0;
        double zOffsetAtTop = 0;
    };
    typedef boost::shared_ptr<AddPointOffsetInfo> AddPointOffsetInfoPtr;

    struct MUJINCLIENT_API ResultGetJointValues : public ResultBase
    {
        virtual ~ResultGetJointValues();
        void Parse(const rapidjson::Value& pt);
        std::string robottype;
        std::vector<std::string> jointnames;
        std::vector<Real> currentjointvalues;
        std::map<std::string, Transform> tools;
    };

    struct MUJINCLIENT_API ResultMoveJoints : public ResultBase
    {
        virtual ~ResultMoveJoints();
        void Parse(const rapidjson::Value& pt);
        std::string robottype;
        int numpoints = 0;
        std::vector<Real>   timedjointvalues;
        //Real elapsedtime;
    };

    struct MUJINCLIENT_API ResultTransform : public ResultBase
    {
        virtual ~ResultTransform();
        void Parse(const rapidjson::Value& pt);
        Transform transform;
    };

    struct MUJINCLIENT_API ResultComputeIkParamPosition : public ResultBase
    {
        void Parse(const rapidjson::Value& pt);
        std::vector<Real> translation;
        std::vector<Real> quaternion = {1, 0, 0, 0};
        std::vector<Real> direction;
        Real angleXZ = 0;
        Real angleYX = 0;
        Real angleZY = 0;
        Real angleX = 0;
        Real angleY = 0;
        Real angleZ = 0;
    };

    struct MUJINCLIENT_API ResultComputeIKFromParameters : public ResultBase
    {
        void Parse(const rapidjson::Value& pt);
        std::vector<std::vector<Real> > dofvalues;
        std::vector<std::string> extra;
    };

    class CopyableRapidJsonDocument : public rapidjson::Document
    {
public:
        // Since ResultGetBinpickingState needs to be copyable while rapidjson::Document is not, there needs to be a small wrapper
        CopyableRapidJsonDocument& operator=(const CopyableRapidJsonDocument& other) {
            SetNull();
            GetAllocator().Clear();
            CopyFrom(other, GetAllocator());
            return *this;
        }
    };

    struct MUJINCLIENT_API ResultGetBinpickingState : public ResultBase
    {
        /// \brief holds published occlusion results of camera and container pairs
        struct OcclusionResult
        {
            mujin::SensorSelectionInfo sensorSelectionInfo;
            std::string bodyname;
            int isocclusion = 0;
        };

        ResultGetBinpickingState();
        virtual ~ResultGetBinpickingState();
        void Parse(const rapidjson::Value& pt);
        std::string statusPickPlace;
        std::string statusDescPickPlace;
        std::string statusPhysics;
        bool isDynamicEnvironmentStateEmpty = true;

        int pickAttemptFromSourceId = 0;
        unsigned long long timestamp = 0;  ///< ms
        unsigned long long lastGrabbedTargetTimeStamp = 0;   ///< ms
        //bool isRobotOccludingSourceContainer; ///?
        std::vector<OcclusionResult> vOcclusionResults;
        std::vector<mujin::LocationTrackingInfo> activeLocationTrackingInfos;
        std::vector<mujin::LocationExecutionInfo> locationExecutionInfos;

        bool isGrabbingTarget = false;
        bool isGrabbingLastTarget = false;

        bool hasRobotExecutionStarted=false;
        int orderNumber = -1; ///< -1 if not initiaized
        int numLeftInOrder = -1; ///< -1 if not initiaized
        int numLeftInSupply = -1; ///< -1 if not initiaized
        int placedInDest = -1; ///< -1 if not initiaized
        std::string cycleIndex; ///< index of the published cycle that pickworker is currently executing

        /**
         * Information needed to register a new object using a min viable region
         */
        struct RegisterMinViableRegionInfo
        {
            RegisterMinViableRegionInfo();
            RegisterMinViableRegionInfo(const RegisterMinViableRegionInfo& rhs);

            RegisterMinViableRegionInfo& operator=(const RegisterMinViableRegionInfo& rhs);

            void SerializeJSON(rapidjson::Value& rInfo, rapidjson::Document::AllocatorType& allocator) const;
            void DeserializeJSON(const rapidjson::Value& rInfo);

            /// \brief scales all translational components by this value
            void ConvertLengthUnitScale(double fUnitScale);

            struct MinViableRegionInfo
            {
                MinViableRegionInfo();
                std::array<double, 2> size2D = {0, 0}; ///< width and length on the MVR
                std::array<double, 3> maxPossibleSize = {0, 0, 0}; ///< the max possible size of actual item
                std::array<double, 3> maxPossibleSizeOriginal = {0, 0, 0}; ///< the maxPossibleSize that has originally been given from vision
                uint8_t cornerMaskOriginal = 0; ///< the cornerMask that has originally been given from vision
                uint8_t cornerMask = 0; ///< Represents the corner(s) used for corner based detection. 4 bit. -x-y = 1, +x-y = 2, -x+y = 4, +x+y = 8
            } minViableRegion;

            std::string locationName; ///< The name of the location where the minViableRegion was triggered for
            std::array<double, 3> translation = {0, 0, 0}; ///< Translation of the 2D MVR plane (height = 0)
            std::array<double, 4> quaternion = {1, 0, 0, 0}; ///< Rotation of the 2D MVR plane (height = 0)
            double objectWeight = 0; ///< If non-zero, use this weight fo registration. unit is kg. zero means unknown.
            std::string objectType; ///< The type of the object
            uint64_t sensorTimeStampMS = 0; ///< Same as DetectedObject's timestamp sent to planning
            double robotDepartStopTimestamp = 0; ///< Force capture after robot stops
            std::array<double, 3> liftedWorldOffset = {0, 0, 0}; ///< [dx, dy, dz], mm in world frame
            std::array<double, 3> maxCandidateSize = {0, 0, 0}; ///< the max candidate size expecting
            std::array<double, 3> minCandidateSize = {0, 0, 0}; ///< the min candidate size expecting
            double transferSpeedPostMult = 0; ///< transfer speed multiplication factor
            rapidjson::Document targetTemplateSceneData; ///< targetTemplateSceneData (a openrave scene that is used as a template for the target object)
            rapidjson::Document graspModelInfo; ///< Parameters used for grasping model generation for the object
            rapidjson::Document registrationVisionInfo; ///< Parameters used for registration vision for the object
            double minCornerVisibleDist = 0; ///< how much distance along with uncertain edge from uncertain corner robot exposes to camera
            double minCornerVisibleInsideDist = 0; ///< how much distance inside MVR robot exposes to camera
            double maxCornerAngleDeviation = 0; ///< how much angle deviation around uncertain corner is considered to expose to camera
            uint8_t occlusionFreeCornerMask = 0; ///< mask of corners that robot exposes to camera. 4 bit. -x-y = 1, +x-y = 2, -x+y = 4, +x+y = 8
            mujin::MinViableRegionRegistrationMode registrationMode; ///< lift, drag or perpendicular drag
            bool skipAppendingToObjectSet = true; ///<  if true, skip appending newly created registration data into an active object set
            double maxPossibleSizePadding = 0; ///< how much to additionally expose max possible size region to vision
            std::vector<double> fullDofValues; ///< robot configuration state on capturing
            std::vector<int8_t> connectedBodyActiveStates; ///< robot configuration state on capturing
            bool IsEmpty() const {
                return sensorTimeStampMS == 0;
            }
        } registerMinViableRegionInfo;

        struct RemoveObjectFromObjectListInfo {
            RemoveObjectFromObjectListInfo();
            double timestamp; // timestamp this request was sent. If non-zero, then valid.
            std::string objectPk; // objectPk to remove from the current object set vision is using
            bool IsEmpty() const {
                return objectPk.empty();
            }
        };
        std::vector<RemoveObjectFromObjectListInfo> removeObjectFromObjectListInfos;

        struct TriggerDetectionCaptureInfo {
            TriggerDetectionCaptureInfo();
            double timestamp = 0; ///< timestamp this request was sent. If non-zero, then valid.
            std::string triggerType; ///< The type of trigger this is. For now can be: "phase1Detection", "phase2Detection"
            std::string locationName; ///< The name of the location for this detection trigger.
            std::string targetUpdateNamePrefix; ///< if not empty, use this new targetUpdateNamePrefix for the triggering, otherwise do not change the original targetUpdateNamePrefix
        } triggerDetectionCaptureInfo;

        std::vector<mujin::PickPlaceHistoryItem> pickPlaceHistoryItems; ///< history of pick/place actions that occurred in planning. Events should be sorted in the order they happen, ie event [0] happens before event [1], meaning event[0].eventTimeStampUS is before event[1].eventTimeStampUS
        CopyableRapidJsonDocument rUnitInfo; ///< the unitInfo struct that specifies what the units for the data in this struct are
    };

    struct MUJINCLIENT_API ResultIsRobotOccludingBody : public ResultBase
    {
        virtual ~ResultIsRobotOccludingBody();
        void Parse(const rapidjson::Value& pt);
        bool result;
    };

    struct MUJINCLIENT_API ResultGetPickedPositions : public ResultBase
    {
        void Parse(const rapidjson::Value& pt);
        std::vector<Transform> transforms;
        std::vector<unsigned long long> timestamps; // in millisecond
    };

    struct MUJINCLIENT_API ResultAABB : public ResultBase
    {
        void Parse(const rapidjson::Value& pt);
        std::array<double, 3> pos;
        std::array<double, 3> extents;
    };

    struct MUJINCLIENT_API ResultOBB : public ResultBase
    {
        void Parse(const rapidjson::Value& pt);
        bool operator!=(const ResultOBB& other) const {
            return !mujin::FuzzyEquals(translation, other.translation) ||
                !mujin::FuzzyEquals(quaternion, other.quaternion) ||
                !mujin::FuzzyEquals(extents, other.extents);
        }
        bool operator==(const ResultOBB& other) const {
            return !operator!=(other);
        }
        std::array<double, 3> translation;
        std::array<double, 4> quaternion = {1, 0, 0, 0};
        std::array<double, 3> extents;
    };

    struct MUJINCLIENT_API ResultInstObjectInfo : public ResultBase
    {
        virtual ~ResultInstObjectInfo();
        void Parse(const rapidjson::Value& pt);
        Transform instobjecttransform;
        ResultOBB instobjectobb;
        ResultOBB instobjectinnerobb;
        rapidjson::Document rGeometryInfos; ///< for every object, list of all the geometry infos
        rapidjson::Document rIkParams; ///< for every object, dict of serialized ikparams
    };

    struct MUJINCLIENT_API ResultGetInstObjectAndSensorInfo : public ResultBase
    {
        virtual ~ResultGetInstObjectAndSensorInfo();
        void Parse(const rapidjson::Value& pt);
        std::map<std::string, std::string> muri;
        std::map<std::string, Transform> minstobjecttransform;
        std::map<std::string, ResultOBB> minstobjectobb;
        std::map<std::string, ResultOBB> minstobjectinnerobb;
        std::map<mujin::SensorSelectionInfo, Transform> msensortransform;
        std::map<mujin::SensorSelectionInfo, RobotResource::AttachedSensorResource::SensorData> msensordata;
        std::map<std::string, boost::shared_ptr<rapidjson::Document> > mrGeometryInfos; ///< for every object, list of all the geometry infos
        std::map<std::string, std::string> mObjectType;
    };

    struct MUJINCLIENT_API ResultHeartBeat : public ResultBase
    {
        virtual ~ResultHeartBeat();
        void Parse(const rapidjson::Value& pt);
        std::string status;
        ResultGetBinpickingState taskstate;
        Real timestamp = 0;
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

    virtual void SetCallerId(const std::string& callerid);

    virtual void ExecuteCommand(const std::string& command, rapidjson::Document&d, const double timeout /* second */=5.0, const bool getresult=true);

    /// \brief executes command directly from rapidjson::Value struct.
    ///
    /// \param rTaskParameters will be destroyed after the call due to r-value moves
    virtual void ExecuteCommand(rapidjson::Value& rTaskParameters, rapidjson::Document& rOutput, const double timeout /* second */=5.0);

    virtual void GetJointValues(ResultGetJointValues& result, const std::string& unit, const double timeout /* second */=5.0);
    virtual void SetInstantaneousJointValues(const std::vector<Real>& jointvalues, const std::string& unit, const double timeout /* second */=5.0);
    virtual void ComputeIkParamPosition(ResultComputeIkParamPosition& result, const std::string& name, const std::string& unit, const double timeout /* second */=5.0);
    virtual void ComputeIKFromParameters(ResultComputeIKFromParameters& result, const std::string& targetname, const std::vector<std::string>& ikparamnames, const int filteroptions, const int limit=0, const double timeout /* second */=5.0);

    /// \brief Moves joints to specified value
    /// \param jointvalues goal joint values
    /// \param jointindices indices of joints to move
    /// \param envclearance environment clearance for collision avoidance in mm
    /// \param speed speed to move robot at
    /// \param result result of moving joints
    /// \param timeout timeout of communication
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real envclearance, const Real speed /* 0.1-1 */, ResultMoveJoints& result, const double timeout /* second */=5.0, std::string* pTraj = NULL);
    virtual void GetTransform(const std::string& targetname, Transform& result, const std::string& unit, const double timeout /* second */=5.0);
    virtual void SetTransform(const std::string& targetname, const Transform& transform, const std::string& unit, const double timeout /* second */=5.0);

    virtual void GetManipTransformToRobot(Transform& result, const std::string& unit, const double timeout /* second */=5.0);
    virtual void GetManipTransform(Transform& result, const std::string& unit, const double timeout /* second */=5.0);

    virtual void GetAABB(const std::string& targetname, ResultAABB& result, const std::string& unit, const double timeout=0);
    virtual void GetOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname, const std::string& unit, const double timeout=0);
    virtual void GetInnerEmptyRegionOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname, const std::string& unit, const double timeout=0);

    /** \brief Update objects in the scene
        \param basename base name of the object. e.g. objects will have name basename_0, basename_1, etc
        \param transformsworld list of transforms in world frame
        \param confidence list of confidence of each detection
        \param state additional information about the objects
        \param unit unit of detectedobject info
        \param timeout seconds until this command times out
     */
    virtual void UpdateObjects(const std::string& objectname, const std::vector<DetectedObject>& detectedobjects, const std::string& resultstate, const std::string& unit, const double timeout /* second */=5.0);

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
        \param locationName the name of the region for which the point cloud is supposed to be captured of. isoccluded maps to this region
        \param timeout seconds until this command times out
        \param clampToContainer if true, then planning will clamp the points to the container walls specified by locationName. Otherwise, will use all the points
        \param rExtraParameters extra parameters to be inserted
     */
    virtual void AddPointCloudObstacle(const std::vector<float>& vpoints, const Real pointsize, const std::string& name,  const unsigned long long starttimestamp, const unsigned long long endtimestamp, const bool executionverification, const std::string& unit, int isoccluded=-1, const std::string& locationName=std::string(), const double timeout /* second */=5.0, bool clampToContainer=true, CropContainerMarginsXYZXYZPtr pCropContainerMargins=CropContainerMarginsXYZXYZPtr(), AddPointOffsetInfoPtr pAddPointOffsetInfo=AddPointOffsetInfoPtr());

    /// \param cameranames the names of the sensors mapped to the current region used for detetion. The sensor information is used to create shadow obstacles per each part, if empty, will not be able to create the correct shadow obstacles.
    virtual void UpdateEnvironmentState(const std::string& objectname, const std::vector<DetectedObject>& detectedobjects, const std::vector<float>& vpoints, const std::string& resultstate, const Real pointsize, const std::string& pointcloudobstaclename, const std::string& unit, const double timeout=0, const std::string& locationName=std::string(), const std::vector<std::string>& cameranames=std::vector<std::string>(), CropContainerMarginsXYZXYZPtr pCropContainerMargins=CropContainerMarginsXYZXYZPtr());

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
    virtual void VisualizePointCloud(const std::vector<std::vector<float> >& pointslist, const Real pointsize, const std::vector<std::string>& names, const std::string& unit, const double timeout /* second */=5.0);

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
    virtual void GetPickedPositions(ResultGetPickedPositions& result, const std::string& unit, const double timeout /* second */=5.0);

    virtual PlanningResultResourcePtr GetResult();

    /** \brief Gets inst object and sensor info of existing objects in the scene
        \param unit input unit
        \param result unit is always in meter
     */
    virtual void GetInstObjectAndSensorInfo(const std::vector<std::string>& instobjectnames, const std::vector<mujin::SensorSelectionInfo>& sensornames, ResultGetInstObjectAndSensorInfo& result, const std::string& unit, const double timeout /* second */=5.0);

    /** \brief Gets inst object info for a particular URI
        \param unit input unit
        \param result unit is always in meter
     */
    virtual void GetInstObjectInfoFromURI(const std::string& objecturi, const Transform& instobjecttransform, ResultInstObjectInfo& result, const std::string& unit, const double timeout /* second */=5.0);

    /// \brief Get state of bin picking
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetBinpickingState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout /* second */=5.0);

    /// \brief Get state of ITL which includes picking status
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetITLState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout /* second */=5.0);

    /// \brief Get published state of bin picking
    /// except for initial call, this returns cached value.
    /// \param result state of bin picking
    /// \param robotname name of robot
    /// \param unit unit to receive values in, either "m" (indicates radian for angle) or "mm" (indicates degree for angle)
    /// \param timeout timeout of communication
    virtual void GetPublishedTaskState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout /* second */=5.0);

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


    /// \brief Moves tool to specified posistion linearly
    /// \param instobjectname name of instobject which the ikparam belongs to
    /// \param ikparamname name of ikparam which the robot move to
    /// \param robotname name of the robot to move
    /// \param toolname name of the tool to move
    /// \param workspeedlin linear speed at which to move tool in mm/s.
    /// \param workspeedrot rotational speed at which to move tool in deg/s
    /// \param timeout timeout of communication
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveToolLinear(const std::string& instobjectname, const std::string& ikparamname, const std::string& robotname = "", const std::string& toolname = "", const double workspeedlin = -1, const double workspeedrot = -1, bool checkEndeffectorCollision = false, const double timeout = 10, std::string* pTraj = NULL);


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

    /// \brief Moves hand to specified posistion
    /// \param instobjectname name of instobject which the ikparam belongs to
    /// \param ikparamname name of ikparam which the robot move to
    /// \param robotname name of the robot to move
    /// \param toolname name of the tool to move
    /// \param robotspeed speed at which to move
    /// \param timeout timeout of communication
    /// \param envclearance environment clearance for collision avoidance in mm
    /// \param pTraj if not NULL, planned trajectory is set but not executed. Otherwise, trajectory is planed and executed but not set.
    virtual void MoveToHandPosition(const std::string& instobjectname, const std::string& ikparamname, const std::string& robotname = "", const std::string& toolname = "", const double robotspeed = -1, const double timeout = 10, Real envclearance = -1.0, std::string* pTraj = NULL);

    virtual void GetGrabbed(std::vector<std::string>& grabbed, const std::string& robotname = "", const double timeout = 10);

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

    /// \brief calls planning GetRobotBridgeIOVariableString and returns the contents of the signal in a string with correct endianness
    virtual void GetRobotBridgeIOVariableString(const std::vector<std::string>& ionames, std::vector<std::string>& iovalues, const double timeout=10);

    /** \brief Monitors heartbeat signals from a running binpicking ZMQ server, and reinitializes the ZMQ server when heartbeat is lost.
        \param reinitializetimeout seconds to wait before re-initializing the ZMQ server after the heartbeat signal is lost
        \param execfn function to use to execute the InitializeZMQ command
     */
    virtual void _HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout);

    /// \brief returns the slaverequestid used to communicate with the controller. If empty, then no id is used.
    virtual const std::string& GetSlaveRequestId() const;

    virtual void SendMVRRegistrationResult(
        const rapidjson::Document &mvrResultInfo,
        double timeout /* second */=5.0);

    // send result of RemoveObjectsFromObjectList request
    virtual void SendRemoveObjectsFromObjectListResult(const std::vector<ResultGetBinpickingState::RemoveObjectFromObjectListInfo>& removeObjectFromObjectListInfos, const bool success, const double timeout /* second */=5.0);

    // send result of RemoveObjectFromObjectList request
    virtual void SendTriggerDetectionCaptureResult(const std::string& triggerType, const std::string& returnCode, double timeout /* second */=5.0);

protected:
    const std::string& _GetCallerId() const;

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

    rapidjson::Document _rUserInfo;  ///< userinfo json
    rapidjson::Document _rSceneParams; ///\ parameters of the scene to run tasks on the backend zmq slave
    std::string _sceneparams_json, _userinfo_json;

    std::string _slaverequestid; ///< to ensure the same slave is used for binpicking task
    std::string _scenepk; ///< scene pk
    std::string _callerid; ///< string identifying the caller
    const std::string _tasktype; ///< the specific task type to create internally. As long as the task supports the binpicking interface, it can be used.
    boost::shared_ptr<boost::thread> _pHeartbeatMonitorThread;

    bool _bIsInitialized;
    bool _bShutdownHeartbeatMonitor;
};



namespace utils {
MUJINCLIENT_API std::string GetJsonString(const std::string& string);
MUJINCLIENT_API std::string GetJsonString(const std::vector<float>& vec);
MUJINCLIENT_API std::string GetJsonString(const std::vector<double>& vec);
MUJINCLIENT_API std::string GetJsonString(const std::vector<int>& vec);
MUJINCLIENT_API std::string GetJsonString(const std::vector<std::string>& vec);
MUJINCLIENT_API std::string GetJsonString(const Transform& transform);
MUJINCLIENT_API std::string GetJsonString(const BinPickingTaskResource::DetectedObject& obj);
MUJINCLIENT_API std::string GetJsonString(const BinPickingTaskResource::PointCloudObstacle& obj);
MUJINCLIENT_API std::string GetJsonString(const BinPickingTaskResource::SensorOcclusionCheck& check);
template <typename T, size_t N>
MUJINCLIENT_API std::string GetJsonString(const std::array<T, N>& a)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<T>::digits10+1);
    ss << "[";
    for (unsigned int i = 0; i < N; ++i) {
        ss << a[i];
        if (i != N - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

MUJINCLIENT_API std::string GetJsonString(const std::string& key, const std::string& value);
MUJINCLIENT_API std::string GetJsonString(const std::string& key, const int value);
MUJINCLIENT_API std::string GetJsonString(const std::string& key, const unsigned long long value);
MUJINCLIENT_API std::string GetJsonString(const std::string& key, const Real value);

MUJINCLIENT_API void GetAttachedSensors(SceneResource& scene, const std::string& bodyname, std::vector<RobotResource::AttachedSensorResourcePtr>& result);
MUJINCLIENT_API void GetSensorData(SceneResource& scene, const std::string& bodyname, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result);
/** \brief Gets transform of attached sensor in sensor body frame
 */
MUJINCLIENT_API void GetSensorTransform(SceneResource& scene, const std::string& bodyname, const std::string& sensorname, Transform& result, const std::string& unit);
MUJINCLIENT_API void DeleteObject(SceneResource& scene, const std::string& name);


#ifdef MUJIN_USEZMQ
/// \brief get heartbeat
/// \param endopoint endpoint to get heartbeat from. looks like protocol://hostname:port (ex. tcp://localhost:11001)
/// \return heartbeat as string
MUJINCLIENT_API std::string GetHeartbeat(const std::string& endpoint);

MUJINCLIENT_API std::string GetScenePkFromHeartbeat(const std::string& heartbeat);
MUJINCLIENT_API std::string GetSlaveRequestIdFromHeartbeat(const std::string& heartbeat);
#endif
} // namespace utils

} // namespace mujinclient

#endif
