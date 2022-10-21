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
#include "common.h"
#include "controllerclientimpl.h"
#if BOOST_VERSION > 104800
#include <boost/algorithm/string/replace.hpp>
#endif
#include <boost/thread.hpp> // for sleep
#include "mujincontrollerclient/binpickingtask.h"

#ifdef MUJIN_USEZMQ
#include "mujincontrollerclient/zmq.hpp"
#endif

#ifdef _WIN32
#include <float.h>
#define isnan _isnan
#endif

#include <cmath>

#include "logging.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "mujincontrollerclient/mujinjson.h"

MUJIN_LOGGER("mujin.controllerclientcpp.binpickingtask");

namespace mujinclient {
using namespace utils;
namespace mujinjson = mujinjson_external;
using namespace mujinjson;

BinPickingResultResource::BinPickingResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

BinPickingResultResource::~BinPickingResultResource()
{
}

BinPickingTaskResource::BinPickingTaskResource(ControllerClientPtr pcontroller, const std::string& pk, const std::string& scenepk, const std::string& tasktype) : TaskResource(pcontroller,pk), _zmqPort(-1), _heartbeatPort(-1), _tasktype(tasktype), _bIsInitialized(false)
{
    _callerid = str(boost::format("controllerclientcpp%s_web")%MUJINCLIENT_VERSION_STRING);
    _scenepk = scenepk;
    // get hostname from uri
    GETCONTROLLERIMPL();
    const std::string baseuri = controller->GetBaseUri();
    std::string::const_iterator uriend = baseuri.end();
    // query start
    std::string::const_iterator querystart = std::find(baseuri.begin(), uriend, '?');
    // protocol
    std::string protocol;
    std::string::const_iterator protocolstart = baseuri.begin();
    std::string::const_iterator protocolend = std::find(protocolstart, uriend, ':');
    if (protocolend != uriend) {
        std::string p = &*(protocolend);
        if ((p.length() > 3) & (p.substr(0,3) == "://")) {
            protocol = std::string(protocolstart, protocolend);
            protocolend +=3;
        } else {
            protocolend = baseuri.begin();
        }
    } else {
        protocolend = baseuri.begin();
    }
    // host
    std::string::const_iterator hoststart = protocolend;
    std::string::const_iterator pathstart = std::find(hoststart, uriend, '/');
    std::string::const_iterator hostend = std::find(protocolend, (pathstart != uriend) ? pathstart : querystart, ':');
    _mujinControllerIp = std::string(hoststart, hostend);

    {
        /// HACK until can think of proper way to send sceneparams
        std::string scenebasename = pcontroller->GetNameFromPrimaryKey_UTF8(scenepk);

        _rSceneParams.SetObject();
        mujinjson::SetJsonValueByKey(_rSceneParams, "scenetype", "mujin");
        mujinjson::SetJsonValueByKey(_rSceneParams, "sceneuri", std::string("mujin:/")+scenebasename);

        // should stop sending scenefilename since it is a hack!
        std::string MUJIN_MEDIA_ROOT_DIR = "/var/www/media/u";
        char* pMUJIN_MEDIA_ROOT_DIR = getenv("MUJIN_MEDIA_ROOT_DIR");
        if( !!pMUJIN_MEDIA_ROOT_DIR ) {
            MUJIN_MEDIA_ROOT_DIR = pMUJIN_MEDIA_ROOT_DIR;
        }

        std::string scenefilename = MUJIN_MEDIA_ROOT_DIR + std::string("/") + pcontroller->GetUserName() + std::string("/") + scenebasename;
        mujinjson::SetJsonValueByKey(_rSceneParams, "scenefilename", scenefilename);
        _sceneparams_json = mujinjson::DumpJson(_rSceneParams);
    }
}

BinPickingTaskResource::~BinPickingTaskResource()
{
    _bShutdownHeartbeatMonitor = true;
    if (!!_pHeartbeatMonitorThread) {
        _pHeartbeatMonitorThread->join();
    }
}

void BinPickingTaskResource::Initialize(const std::string& defaultTaskParameters, const double timeout, const std::string& userinfo, const std::string& slaverequestid)
{
    if( defaultTaskParameters.size() > 0 ) {
        _mapTaskParameters.clear();
        rapidjson::Document d;
        d.Parse(defaultTaskParameters.c_str());
        for (rapidjson::Value::ConstMemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it) {
            rapidjson::StringBuffer stringbuffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(stringbuffer);
            it->value.Accept(writer);
            _mapTaskParameters[it->name.GetString()] = std::string(stringbuffer.GetString(), stringbuffer.GetSize());
        }
    }

    _bIsInitialized = true;
    ParseJson(_rUserInfo, userinfo);
    _userinfo_json = userinfo;
    _slaverequestid = slaverequestid;
}

void BinPickingTaskResource::SetCallerId(const std::string& callerid)
{
    _callerid = callerid;
}

const std::string& BinPickingTaskResource::_GetCallerId() const
{
    return _callerid;
}

#ifdef MUJIN_USEZMQ
void BinPickingTaskResource::Initialize(const std::string& defaultTaskParameters, const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout, const std::string& userinfo, const std::string& slaverequestid)
{

    if( defaultTaskParameters.size() > 0 ) {
        _mapTaskParameters.clear();
        rapidjson::Document d;
        d.Parse(defaultTaskParameters.c_str());
        for (rapidjson::Value::ConstMemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it) {
            rapidjson::StringBuffer stringbuffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(stringbuffer);
            it->value.Accept(writer);
            _mapTaskParameters[it->name.GetString()] = std::string(stringbuffer.GetString(), stringbuffer.GetSize());
        }
    }

    _zmqPort = zmqPort;
    _heartbeatPort = heartbeatPort;
    _bIsInitialized = true;
    _zmqcontext = zmqcontext;
    ParseJson(_rUserInfo, userinfo);
    _userinfo_json = userinfo;
    _slaverequestid = slaverequestid;
}
#endif

void BinPickingResultResource::GetResultJson(rapidjson::Document& pt) const
{
    GETCONTROLLERIMPL();
    //rapidjson::Document d(rapidjson::kObjectType);
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    // in this way we don't copy
    rapidjson::Value v;
    v.Swap(pt["output"]);
    v.Swap(pt);
}

std::string utils::GetJsonString(const std::string& str)
{
    std::string newstr = str;
#if BOOST_VERSION > 104800
    boost::replace_all(newstr, "\"", "\\\"");
#else
    std::vector< std::pair<std::string, std::string> > serachpairs(1);
    serachpairs[0].first = "\""; serachpairs[0].second = "\\\"";
    SearchAndReplace(newstr, str, serachpairs);
#endif
    return "\""+newstr+"\"";
}

std::string utils::GetJsonString (const std::vector<float>& vec)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<float>::digits10+1);
    ss << "[";
    for (unsigned int i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i != vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString (const std::vector<double>& vec)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<double>::digits10+1);
    ss << "[";
    for (unsigned int i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i != vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString (const std::vector<int>& vec)
{
    std::stringstream ss;
    ss << "[";
    for (unsigned int i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i != vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString(const std::vector<std::string>& vec)
{
    std::stringstream ss;
    ss << "[";
    for (unsigned int i = 0; i < vec.size(); ++i) {
        ss << GetJsonString(vec[i]);
        if (i != vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString(const Transform& transform)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    // \"translation\":[%.15f, %.15f, %.15f], \"quaternion\":[%.15f, %.15f, %.15f, %.15f]
    ss << GetJsonString("translation") << ": [";
    for (unsigned int i=0; i<3; i++) {
        ss << transform.translate[i];
        if (i!=3-1) {
            ss << ", ";
        }
    }
    ss << "], ";
    ss << GetJsonString("quaternion") << ": [";
    for (unsigned int i=0; i<4; i++) {
        ss << transform.quaternion[i];
        if (i!=4-1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString(const BinPickingTaskResource::DetectedObject& obj)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    //"{\"name\": \"obj\",\"translation_\":[100,200,300],\"quat_\":[1,0,0,0],\"confidence\":0.5}"
    ss << "{";
    ss << GetJsonString("name") << ": " << GetJsonString(obj.name) << ", ";
    ss << GetJsonString("object_uri") << ": " << GetJsonString(obj.object_uri) << ", ";
    ss << GetJsonString("translation_") << ": [";
    for (unsigned int i=0; i<3; i++) {
        ss << obj.transform.translate[i];
        if (i!=3-1) {
            ss << ", ";
        }
    }
    ss << "], ";
    ss << GetJsonString("quat_") << ": [";
    for (unsigned int i=0; i<4; i++) {
        ss << obj.transform.quaternion[i];
        if (i!=4-1) {
            ss << ", ";
        }
    }
    ss << "], ";
    ss << GetJsonString("confidence") << ": " << obj.confidence;
    ss << ", " << GetJsonString("sensortimestamp") << ": " << obj.timestamp;
    ss << ", " << GetJsonString("isPickable") << ": " << obj.isPickable;
    if( obj.extra.size() > 0 ) {
        ss << ", " << GetJsonString("extra") << ": " << obj.extra;
    }
    ss << "}";
    return ss.str();
}

std::string utils::GetJsonString(const BinPickingTaskResource::PointCloudObstacle& obj)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<float>::digits10+1); // want to control the size of the JSON file outputted
    // "\"name\": __dynamicobstacle__, \"pointsize\": 0.005, \"points\": []
    ss << GetJsonString("pointcloudid") << ": " << GetJsonString(obj.name) << ", ";
    ss << GetJsonString("pointsize") << ": " << obj.pointsize <<", ";

    ss << GetJsonString("points") << ": " << "[";
    bool bwrite = false;
    for (unsigned int i = 0; i < obj.points.size(); i+=3) {
        if( !isnan(obj.points[i]) && !isnan(obj.points[i+1]) && !isnan(obj.points[i+2]) ) { // sometimes point clouds can have NaNs, although it's a bug on detectors sending bad point clouds, these points can usually be ignored.
            if( bwrite ) {
                ss << ",";
            }
            ss << obj.points[i] << "," << obj.points[i+1] << "," << obj.points[i+2];
            bwrite = true;
        }
    }
    ss << "]";
    return ss.str();
}

std::string utils::GetJsonString(const BinPickingTaskResource::SensorOcclusionCheck& check)
{
    std::stringstream ss;
    ss << GetJsonString("bodyname") << ": " << GetJsonString(check.bodyname) << ", ";
    ss << GetJsonString("cameraname") << ": " << GetJsonString(check.cameraname) << ", ";
    ss << GetJsonString("starttime") << ": " << check.starttime <<", ";
    ss << GetJsonString("endtime") << ": " << check.endtime;
    return ss.str();
}

std::string utils::GetJsonString(const std::string& key, const std::string& value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << GetJsonString(value);
    return ss.str();
}

std::string utils::GetJsonString(const std::string& key, const int value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

std::string utils::GetJsonString(const std::string& key, const unsigned long long value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

std::string utils::GetJsonString(const std::string& key, const Real value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

BinPickingTaskResource::ResultGetJointValues::~ResultGetJointValues()
{
}

void BinPickingTaskResource::ResultGetJointValues::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];

    LoadJsonValueByKey(v, "robottype", robottype);
    LoadJsonValueByKey(v, "jointnames", jointnames);
    LoadJsonValueByKey(v, "currentjointvalues", currentjointvalues);
    tools.clear();
    if (v.HasMember("tools")) {
        const rapidjson::Value &toolsjson = v["tools"];
        for (rapidjson::Document::ConstMemberIterator it = toolsjson.MemberBegin(); it != toolsjson.MemberEnd(); it++) {
            Transform transform;
            LoadJsonValueByKey(it->value, "translate", transform.translate);
            LoadJsonValueByKey(it->value, "quaternion", transform.quaternion);
            tools[it->name.GetString()] = transform;
        }
    }
}

BinPickingTaskResource::ResultMoveJoints::~ResultMoveJoints()
{
}

void BinPickingTaskResource::ResultMoveJoints::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];
    LoadJsonValueByKey(v, "robottype", robottype);
    LoadJsonValueByKey(v, "timedjointvalues", timedjointvalues);
    LoadJsonValueByKey(v, "numpoints", numpoints);
}

BinPickingTaskResource::ResultTransform::~ResultTransform()
{
}

void BinPickingTaskResource::ResultTransform::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];

    LoadJsonValueByKey(v, "translation", transform.translate);
    LoadJsonValueByKey(v, "quaternion", transform.quaternion);
}

BinPickingTaskResource::ResultInstObjectInfo::~ResultInstObjectInfo()
{
}

void BinPickingTaskResource::ResultInstObjectInfo::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& rOutput = pt["output"];

    LoadJsonValueByKey(rOutput, "translation", instobjecttransform.translate);
    LoadJsonValueByKey(rOutput, "quaternion", instobjecttransform.quaternion);
    instobjectobb.Parse(rOutput["obb"]);
    instobjectinnerobb.Parse(rOutput["innerobb"]);

    if( rOutput.HasMember("geometryInfos") ) {
        mujinjson::SaveJsonValue(rGeometryInfos, rOutput["geometryInfos"]);
    }

    if( rOutput.HasMember("ikparams") ) {
        mujinjson::SaveJsonValue(rIkParams, rOutput["ikparams"]);
    }
}

BinPickingTaskResource::ResultGetInstObjectAndSensorInfo::~ResultGetInstObjectAndSensorInfo()
{
}

void BinPickingTaskResource::ResultGetInstObjectAndSensorInfo::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& output = pt["output"];

    mrGeometryInfos.clear();

    const rapidjson::Value& instobjects = output["instobjects"];
    for (rapidjson::Document::ConstMemberIterator it = instobjects.MemberBegin(); it != instobjects.MemberEnd(); it++) {
        std::string objname = it->name.GetString();
        Transform transform;
        ResultOBB resultobb, resultinnerobb;
        LoadJsonValueByKey(it->value, "translation", transform.translate);
        LoadJsonValueByKey(it->value, "quaternion", transform.quaternion);
        resultobb.Parse(it->value["obb"]);
        resultinnerobb.Parse(it->value["innerobb"]);

        minstobjecttransform[objname] = transform;
        minstobjectobb[objname] = resultobb;
        minstobjectinnerobb[objname] = resultinnerobb;

        if( it->value.HasMember("geometryInfos") ) {
            boost::shared_ptr<rapidjson::Document> pr(new rapidjson::Document());;
            mujinjson::SaveJsonValue(*pr, it->value["geometryInfos"]);
            mrGeometryInfos[objname] = pr;
        }
    }

    const rapidjson::Value& sensors = output["sensors"];
    for (rapidjson::Document::ConstMemberIterator it = sensors.MemberBegin(); it != sensors.MemberEnd(); it++) {
        std::string sensorname = it->name.GetString();
        Transform transform;
        RobotResource::AttachedSensorResource::SensorData sensordata;
        LoadJsonValueByKey(it->value, "translation", transform.translate);
        LoadJsonValueByKey(it->value, "quaternion", transform.quaternion);

        const rapidjson::Value &sensor = it->value["sensordata"];
        LoadJsonValueByKey(sensor, "distortion_coeffs", sensordata.distortion_coeffs);
        LoadJsonValueByKey(sensor, "intrinsic", sensordata.intrinsic);

        std::vector<int> imagedimensions;
        LoadJsonValueByKey(sensor, "image_dimensions", imagedimensions);
        if (imagedimensions.size() == 2) {
            imagedimensions.push_back(1);
        }
        if (imagedimensions.size() != 3) {
            throw MujinException("the length of image_dimensions is invalid", MEC_Failed);
        }
        for (int i = 0; i < 3; i++) {
            sensordata.image_dimensions[i] = imagedimensions[i];
        }

        LoadJsonValueByKey(sensor, "extra_parameters", sensordata.extra_parameters);
        LoadJsonValueByKey(sensor, "distortion_model", sensordata.distortion_model);
        LoadJsonValueByKey(sensor, "focal_length", sensordata.focal_length);
        LoadJsonValueByKey(sensor, "measurement_time", sensordata.measurement_time);

        msensortransform[sensorname] = transform;
        msensordata[sensorname] = sensordata;
    }
}

BinPickingTaskResource::ResultGetBinpickingState::ResultGetBinpickingState() :
    statusPickPlace(""),
    statusDescPickPlace(""),
    statusPhysics(""),
    isDynamicEnvironmentStateEmpty(false),
    pickAttemptFromSourceId(-1),
    timestamp(0),
    lastGrabbedTargetTimeStamp(0),
    isGrabbingTarget(true),
    isGrabbingLastTarget(true),
    hasRobotExecutionStarted(false),
    orderNumber(-1),
    numLeftInOrder(-1),
    numLeftInSupply(-1),
    placedInDest(-1)
{
}

BinPickingTaskResource::ResultGetBinpickingState::~ResultGetBinpickingState()
{
}

void BinPickingTaskResource::ResultGetBinpickingState::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];

    statusPickPlace = GetJsonValueByKey<std::string>(v, "statusPickPlace", "unknown");
    statusDescPickPlace = GetJsonValueByKey<std::string>(v, "statusDescPickPlace", "unknown");
    cycleIndex = GetJsonValueByKey<std::string>(v, "statusPickPlaceCycleIndex", "");
    statusPhysics = GetJsonValueByKey<std::string>(v, "statusPhysics", "unknown");
    pickAttemptFromSourceId = GetJsonValueByKey<int>(v, "pickAttemptFromSourceId", -1);
    //isContainerEmptyMap.clear();
    //LoadJsonValueByKey(v, "isContainerEmptyMap", isContainerEmptyMap);
    //lastInsideSourceTimeStamp = (unsigned long long)(GetJsonValueByKey<double>(v, "lastInsideSourceTimeStamp", 0) * 1000.0); // s -> ms
    //lastInsideDestTimeStamp = (unsigned long long)(GetJsonValueByKey<double>(v, "lastInsideDestTimeStamp", 0) * 1000.0); // s -> ms
    timestamp = (unsigned long long)(GetJsonValueByKey<double>(v, "timestamp", 0) * 1000.0); // s -> ms
    lastGrabbedTargetTimeStamp = (unsigned long long)(GetJsonValueByKey<double>(v, "lastGrabbedTargetTimeStamp", 0) * 1000.0); // s -> ms

    vOcclusionResults.clear();
    if( v.HasMember("occlusionResults") && v["occlusionResults"].IsArray() ) {
        vOcclusionResults.resize(v["occlusionResults"].Size());
        for(int iocc = 0; iocc < (int)vOcclusionResults.size(); ++iocc) {
            vOcclusionResults[iocc].cameraname = GetJsonValueByKey<std::string,std::string>(v["occlusionResults"][iocc], "cameraname", std::string());
            vOcclusionResults[iocc].bodyname = GetJsonValueByKey<std::string,std::string>(v["occlusionResults"][iocc], "bodyname", std::string());
            vOcclusionResults[iocc].isocclusion = GetJsonValueByKey<int,int>(v["occlusionResults"][iocc], "isocclusion", -1);
        }
    }

    isGrabbingTarget = GetJsonValueByKey<bool>(v, "isGrabbingTarget", true);
    isGrabbingLastTarget = GetJsonValueByKey<bool>(v, "isGrabbingLastTarget", true);
    hasRobotExecutionStarted = GetJsonValueByKey<bool>(v, "hasRobotExecutionStarted", false);
    orderNumber = GetJsonValueByPath<int>(v, "/orderstate/orderNumber", -1);
    numLeftInOrder = GetJsonValueByPath<int>(v, "/orderstate/numLeftInOrder", -1);
    numLeftInSupply = GetJsonValueByPath<int>(v, "/orderstate/numLeftInSupply", -1);
    placedInDest = GetJsonValueByPath<int>(v, "/orderstate/placedInDest", -1);

    registerMinViableRegionInfo.locationName = GetJsonValueByPath<std::string>(v, "/registerMinViableRegionInfo/locationName", std::string());
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/translation_", registerMinViableRegionInfo.translation_);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/quat_", registerMinViableRegionInfo.quat_);
    registerMinViableRegionInfo.objectWeight = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/objectWeight", 0);
    registerMinViableRegionInfo.sensortimestamp = GetJsonValueByPath<uint64_t>(v, "/registerMinViableRegionInfo/sensortimestamp", 0);
    registerMinViableRegionInfo.robotDepartStopTimestamp = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/robotDepartStopTimestamp", 0);
    registerMinViableRegionInfo.transferSpeedPostMult = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/transferSpeedPostMult", 1.0);
    {
        registerMinViableRegionInfo.graspModelInfo.SetNull();
        registerMinViableRegionInfo.graspModelInfo.GetAllocator().Clear();
        const rapidjson::Value* graspModelInfoJson = rapidjson::Pointer("/registerMinViableRegionInfo/graspModelInfo").Get(v);
        if( !!graspModelInfoJson && graspModelInfoJson->IsObject() ) {
            registerMinViableRegionInfo.graspModelInfo.CopyFrom(*graspModelInfoJson, registerMinViableRegionInfo.graspModelInfo.GetAllocator());
        }
    }
    registerMinViableRegionInfo.minCornerVisibleDist = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/minCornerVisibleDist", 30);
    registerMinViableRegionInfo.minCornerVisibleInsideDist = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/minCornerVisibleInsideDist", 0);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/minViableRegion/size2D", registerMinViableRegionInfo.minViableRegion.size2D);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/minViableRegion/maxPossibleSize", registerMinViableRegionInfo.minViableRegion.maxPossibleSize);
    registerMinViableRegionInfo.minViableRegion.cornerMask = GetJsonValueByPath<uint64_t>(v, "/registerMinViableRegionInfo/minViableRegion/cornerMask", 0);
    registerMinViableRegionInfo.occlusionFreeCornerMask = GetJsonValueByPath<uint64_t>(v, "/registerMinViableRegionInfo/occlusionFreeCornerMask", 0);
    registerMinViableRegionInfo.maxPossibleSizePadding = GetJsonValueByPath<double>(v, "/registerMinViableRegionInfo/maxPossibleSizePadding", 30);
    registerMinViableRegionInfo.waitForTriggerOnCapturing = GetJsonValueByPath<bool>(v, "/registerMinViableRegionInfo/waitForTriggerOnCapturing", false);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/liftedWorldOffset", registerMinViableRegionInfo.liftedWorldOffset);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/minCandidateSize", registerMinViableRegionInfo.minCandidateSize);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/maxCandidateSize", registerMinViableRegionInfo.maxCandidateSize);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/fullDofValues", registerMinViableRegionInfo.fullDofValues);
    LoadJsonValueByPath(v, "/registerMinViableRegionInfo/connectedBodyActiveStates", registerMinViableRegionInfo.connectedBodyActiveStates);

    removeObjectFromObjectListInfo.timestamp = GetJsonValueByPath<double>(v, "/removeObjectFromObjectList/timestamp", 0);
    removeObjectFromObjectListInfo.objectPk = GetJsonValueByPath<std::string>(v, "/removeObjectFromObjectList/objectPk", "");

    triggerDetectionCaptureInfo.timestamp = GetJsonValueByPath<double>(v, "/triggerDetectionCaptureInfo/timestamp", 0);
    triggerDetectionCaptureInfo.triggerType = GetJsonValueByPath<std::string>(v, "/triggerDetectionCaptureInfo/triggerType", "");
    triggerDetectionCaptureInfo.locationName = GetJsonValueByPath<std::string>(v, "/triggerDetectionCaptureInfo/locationName", "");
    triggerDetectionCaptureInfo.targetupdatename = GetJsonValueByPath<std::string>(v, "/triggerDetectionCaptureInfo/targetupdatename", "");

    activeLocationTrackingInfos.clear();
    if( v.HasMember("activeLocationTrackingInfos") && v["activeLocationTrackingInfos"].IsArray()) {
        const rapidjson::Value& rLocationTrackingInfo = v["activeLocationTrackingInfos"];
        activeLocationTrackingInfos.resize(rLocationTrackingInfo.Size());
        for(int iitem = 0; iitem < (int)rLocationTrackingInfo.Size(); ++iitem) {
            const rapidjson::Value& rInfo = rLocationTrackingInfo[iitem];
            activeLocationTrackingInfos[iitem].locationName = GetJsonValueByKey<std::string,std::string>(rInfo, "locationName", std::string());
            activeLocationTrackingInfos[iitem].containerId = GetJsonValueByKey<std::string,std::string>(rInfo, "containerId", std::string());
            activeLocationTrackingInfos[iitem].containerName = GetJsonValueByKey<std::string,std::string>(rInfo, "containerName", std::string());
            activeLocationTrackingInfos[iitem].containerUsage = GetJsonValueByKey<std::string,std::string>(rInfo, "containerUsage", std::string());
            activeLocationTrackingInfos[iitem].cycleIndex = GetJsonValueByKey<std::string,std::string>(rInfo, "cycleIndex", std::string());
        }
    }

    locationExecutionInfos.clear();
    if( v.HasMember("locationExecutionInfos") && v["locationExecutionInfos"].IsArray()) {
        const rapidjson::Value& rLocationTrackingInfo = v["locationExecutionInfos"];
        locationExecutionInfos.resize(rLocationTrackingInfo.Size());
        for(int iitem = 0; iitem < (int)rLocationTrackingInfo.Size(); ++iitem) {
            const rapidjson::Value& rInfo = rLocationTrackingInfo[iitem];
            locationExecutionInfos[iitem].locationName = GetJsonValueByKey<std::string,std::string>(rInfo, "locationName", std::string());
            locationExecutionInfos[iitem].containerId = GetJsonValueByKey<std::string,std::string>(rInfo, "containerId", std::string());
            locationExecutionInfos[iitem].forceRequestStampMS = GetJsonValueByKey<uint64_t, uint64_t>(rInfo, "forceRequestStampMS", 0);
            locationExecutionInfos[iitem].lastInsideContainerStampMS = GetJsonValueByKey<uint64_t, uint64_t>(rInfo, "lastInsideContainerStampMS", 0);
            locationExecutionInfos[iitem].needContainerState = GetJsonValueByKey<std::string,std::string>(rInfo, "needContainerState", std::string());
        }
    }

    pickPlaceHistoryItems.clear();
    if( v.HasMember("pickPlaceHistoryItems") && v["pickPlaceHistoryItems"].IsArray() ) {
        pickPlaceHistoryItems.resize(v["pickPlaceHistoryItems"].Size());
        for(int iitem = 0; iitem < (int)pickPlaceHistoryItems.size(); ++iitem) {
            const rapidjson::Value& rItem = v["pickPlaceHistoryItems"][iitem];
            pickPlaceHistoryItems[iitem].pickPlaceType = GetJsonValueByKey<std::string,std::string>(rItem, "pickPlaceType", std::string());
            pickPlaceHistoryItems[iitem].locationName = GetJsonValueByKey<std::string,std::string>(rItem, "locationName", std::string());
            pickPlaceHistoryItems[iitem].eventTimeStampUS = GetJsonValueByKey<unsigned long long>(rItem, "eventTimeStampUS", 0);
            pickPlaceHistoryItems[iitem].object_uri = GetJsonValueByKey<std::string,std::string>(rItem, "object_uri", std::string());

            pickPlaceHistoryItems[iitem].objectpose = Transform();
            if( rItem.HasMember("objectpose") ) {
                const rapidjson::Value& rObjectPose = rItem["objectpose"];
                if( rObjectPose.IsArray() && rObjectPose.Size() == 7 ) {
                    LoadJsonValue(rObjectPose[0], pickPlaceHistoryItems[iitem].objectpose.quaternion[0]);
                    LoadJsonValue(rObjectPose[1], pickPlaceHistoryItems[iitem].objectpose.quaternion[1]);
                    LoadJsonValue(rObjectPose[2], pickPlaceHistoryItems[iitem].objectpose.quaternion[2]);
                    LoadJsonValue(rObjectPose[3], pickPlaceHistoryItems[iitem].objectpose.quaternion[3]);
                    LoadJsonValue(rObjectPose[4], pickPlaceHistoryItems[iitem].objectpose.translate[0]);
                    LoadJsonValue(rObjectPose[5], pickPlaceHistoryItems[iitem].objectpose.translate[1]);
                    LoadJsonValue(rObjectPose[6], pickPlaceHistoryItems[iitem].objectpose.translate[2]);
                }
            }

            pickPlaceHistoryItems[iitem].sensorTimeStampUS = GetJsonValueByKey<unsigned long long>(rItem, "sensorTimeStampUS", 0);
        }
    }
}

BinPickingTaskResource::ResultGetBinpickingState::RegisterMinViableRegionInfo::RegisterMinViableRegionInfo() :
    objectWeight(0.0),
    sensortimestamp(0),
    robotDepartStopTimestamp(0),
    transferSpeedPostMult(1.0),
    minCornerVisibleDist(30),
    minCornerVisibleInsideDist(0),
    occlusionFreeCornerMask(0),
    waitForTriggerOnCapturing(false),
    maxPossibleSizePadding(0)
{
    translation_.fill(0);
    quat_.fill(0);
    liftedWorldOffset.fill(0);
    maxCandidateSize.fill(0);
    minCandidateSize.fill(0);
}

BinPickingTaskResource::ResultGetBinpickingState::RegisterMinViableRegionInfo::MinViableRegionInfo::MinViableRegionInfo() :
    cornerMask(0)
{
    size2D.fill(0);
    maxPossibleSize.fill(0);
}

BinPickingTaskResource::ResultGetBinpickingState::RemoveObjectFromObjectListInfo::RemoveObjectFromObjectListInfo() :
    timestamp(0)
{
}

BinPickingTaskResource::ResultGetBinpickingState::TriggerDetectionCaptureInfo::TriggerDetectionCaptureInfo() :
    timestamp(0)
{
}

BinPickingTaskResource::ResultIsRobotOccludingBody::~ResultIsRobotOccludingBody()
{
}

void BinPickingTaskResource::ResultIsRobotOccludingBody::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];
    if (!v.IsObject() || !v.HasMember("occluded")) {
        throw MujinException("Output does not have \"occluded\" attribute!", MEC_Failed);
    }
    result = GetJsonValueByKey<int>(v, "occluded", 1) == 1;
}

void BinPickingTaskResource::ResultGetPickedPositions::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];
    for (rapidjson::Document::ConstMemberIterator value = v.MemberBegin(); value != v.MemberEnd(); ++value) {
        if (std::string(value->name.GetString()) == "positions" && value->value.IsArray()) {
            for (rapidjson::Document::ConstValueIterator it = value->value.Begin(); it != value->value.End(); ++it) {
                if (it->IsArray() && it->Size() >= 8) {
                    Transform transform;
                    for (int i = 0; i < 4; i++) {
                        transform.quaternion[i] = (*it)[i].GetDouble();
                    }
                    for (int i = 0; i < 3; i++) {
                        transform.translate[i] = (*it)[i + 4].GetDouble();
                    }
                    transforms.push_back(transform);
                    timestamps.push_back((*it)[7].GetInt64());
                }
            }
        }
    }
}

void BinPickingTaskResource::ResultAABB::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];
    LoadJsonValueByKey(v, "pos", pos);
    LoadJsonValueByKey(v, "extents", extents);
    if (pos.size() != 3) {
        throw MujinException("The length of pos is invalid.", MEC_Failed);
    }
    if (extents.size() != 3) {
        throw MujinException("The length of extents is invalid.", MEC_Failed);
    }
}

void BinPickingTaskResource::ResultOBB::Parse(const rapidjson::Value& pt)
{
    const rapidjson::Value&  v = (pt.IsObject()&&pt.HasMember("output") ? pt["output"] : pt);

    LoadJsonValueByKey(v, "translation", translation);
    LoadJsonValueByKey(v, "extents", extents);
    std::vector<std::vector<Real> > rotationmatrix2d;
    LoadJsonValueByKey(v, "rotationmat", rotationmatrix2d);
    if (translation.size() != 3) {
        throw MujinException("The length of translation is invalid.", MEC_Failed);
    }
    if (extents.size() != 3) {
        throw MujinException("The length of extents is invalid.", MEC_Failed);
    }
    if (rotationmatrix2d.size() != 3 || rotationmatrix2d[0].size() != 3 || rotationmatrix2d[1].size() != 3 || rotationmatrix2d[2].size() != 3) {
        throw MujinException("The row number of rotationmat is invalid.", MEC_Failed);
    }

    rotationmat.resize(9);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            rotationmat[i*3+j] = rotationmatrix2d[i][j];
        }
    }

    LoadJsonValueByKey(v, "quaternion", quaternion);
}

void BinPickingTaskResource::ResultComputeIkParamPosition::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v = pt["output"];
    LoadJsonValueByKey(v, "translation", translation);
    LoadJsonValueByKey(v, "quaternion", quaternion);
    LoadJsonValueByKey(v, "direction", direction);
    LoadJsonValueByKey(v, "angleXZ", angleXZ);
    LoadJsonValueByKey(v, "angleYX", angleYX);
    LoadJsonValueByKey(v, "angleZY", angleZY);
    LoadJsonValueByKey(v, "angleX", angleX);
    LoadJsonValueByKey(v, "angleY", angleY);
    LoadJsonValueByKey(v, "angleZ", angleZ);

    if (translation.size() != 3) {
        throw MujinException("The length of translation is invalid.", MEC_Failed);
    }
    if (quaternion.size() != 4) {
        throw MujinException("The length of quaternion is invalid.", MEC_Failed);
    }
    if (direction.size() != 3) {
        throw MujinException("The length of direction is invalid.", MEC_Failed);
    }
}

void BinPickingTaskResource::ResultComputeIKFromParameters::Parse(const rapidjson::Value& pt)
{
    BOOST_ASSERT(pt.IsObject() && pt.HasMember("output"));
    const rapidjson::Value& v_ = pt["output"];
    BOOST_ASSERT(v_.IsObject() && v_.HasMember("solutions"));
    const rapidjson::Value& v = v_["solutions"];
    for (rapidjson::Document::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
        std::vector<Real> dofvalues_;
        std::string extra_;
        LoadJsonValueByKey(*it, "dofvalues", dofvalues_);
        extra_ = DumpJson((*it)["extra"]);
        dofvalues.push_back(dofvalues_);
        extra.push_back(extra_);
    }
}

BinPickingTaskResource::ResultHeartBeat::~ResultHeartBeat()
{
}

void BinPickingTaskResource::ResultHeartBeat::Parse(const rapidjson::Value& pt)
{
    status = "";
    msg = "";
    timestamp = 0;
    LoadJsonValueByKey(pt, "status", status);
    LoadJsonValueByKey(pt, "message",msg);
    LoadJsonValueByKey(pt, "timestamp", timestamp);
    if (pt.HasMember("slavestates") && _slaverequestid.size() > 0) {
        rapidjson::Document d(rapidjson::kObjectType), taskstatejson;
        std::string key = "slaverequestid-" + _slaverequestid;
        //try {
        if (pt["slavestates"].HasMember(key.c_str()) && pt["slavestates"][key.c_str()].HasMember("taskstate")) {
            mujinjson::SaveJsonValue(taskstatejson, pt["slavestates"][key.c_str()]["taskstate"]);
            SetJsonValueByKey(d, "output", taskstatejson);
            taskstate.Parse(d);
        }
//        }
//        catch (const std::exception& ex){
//            MUJIN_LOG_WARN("parsing heartbeat at " << timestamp ": " << ex.what());
//        }
    }
}

namespace {
void SetMapTaskParameters(std::stringstream &ss, const std::map<std::string, std::string> &params)
{
    ss << std::setprecision(std::numeric_limits<double>::digits10+1);
    ss.str("");
    ss.clear();
    ss << "{";
    FOREACHC(it, params) {
        ss << "\"" << it->first << "\":" << it->second << ", ";
    }
}

void SerializeGetStateCommand(std::stringstream &ss, const std::map<std::string, std::string> &params,
                              const std::string &functionname, const std::string &tasktype,
                              const std::string &robotname, const std::string &unit, const double timeout) {
    SetMapTaskParameters(ss, params);

    ss << GetJsonString("command", functionname) << ", ";
    ss << GetJsonString("tasktype", tasktype) << ", ";
    if (!robotname.empty()) {
        ss << GetJsonString("robotname", robotname) << ", ";
    }
    ss << GetJsonString("unit", unit);
    ss << "}";
}

void
GenerateMoveToolCommand(const std::string &movetype, const std::string &goaltype, const std::vector<double> &goals,
                        const std::string &robotname, const std::string &toolname, const double robotspeed,
                        Real envclearance, std::stringstream &ss,
                        const std::map<std::string, std::string> &params) {
    SetMapTaskParameters(ss, params);
    ss << GetJsonString("command", movetype) << ", ";
    ss << GetJsonString("goaltype", goaltype) << ", ";
    if (!robotname.empty()) {
        ss << GetJsonString("robotname", robotname) << ", ";
    }
    if (!toolname.empty()) {
        ss << GetJsonString("toolname", toolname) << ", ";
    }
    if (robotspeed >= 0) {
        ss << GetJsonString("robotspeed") << ": " << robotspeed << ", ";
    }
    if (envclearance >= 0) {
        ss << GetJsonString("envclearance") << ": " << envclearance << ", ";
    }
    ss << GetJsonString("goals") << ": " << GetJsonString(goals);
    ss << "}";

}

void
GenerateMoveToolByIkParamCommand(const std::string &movetype, const std::string &instobjectname, const std::string &ikparamname,
                                 const std::string &robotname, const std::string &toolname, const double robotspeed,
                                 Real envclearance, std::stringstream &ss,
                                 const std::map<std::string, std::string> &params) {
    SetMapTaskParameters(ss, params);
    ss << GetJsonString("command", movetype) << ", ";
    ss << "\"goaltype\": null,";
    ss << GetJsonString("instobjectname", instobjectname) << ", ";
    if (!robotname.empty()) {
        ss << GetJsonString("robotname", robotname) << ", ";
    }
    if (!toolname.empty()) {
        ss << GetJsonString("toolname", toolname) << ", ";
    }
    if (robotspeed >= 0) {
        ss << GetJsonString("robotspeed") << ": " << robotspeed << ", ";
    }
    if (envclearance >= 0) {
        ss << GetJsonString("envclearance") << ": " << envclearance << ", ";
    }
    ss << GetJsonString("ikparamname", ikparamname);
    ss << "}";

}

void SetTrajectory(const rapidjson::Value &pt,
                   std::string *pTraj) {
    if (!(pt.IsObject() && pt.HasMember("output") && pt["output"].HasMember("trajectory"))) {
        throw MujinException("trajectory is not available in output", MEC_Failed);
    }
    *pTraj = GetJsonValueByPath<std::string>(pt, "/output/trajectory");
}
}

void BinPickingTaskResource::GetJointValues(ResultGetJointValues& result, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetJointValues";
    std::string robottype = "densowave";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("robottype", robottype) << ", ";
    _ss << GetJsonString("unit", unit) << ", ";
    _ss << GetJsonString("tasktype", _tasktype);
    _ss << "}";

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::SetInstantaneousJointValues(const std::vector<Real>& jointvalues, const std::string& unit, const double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    {
        for(std::map<std::string,std::string>::iterator it=_mapTaskParameters.begin(); it!=_mapTaskParameters.end(); ++it) {
            rapidjson::Document t;
            ParseJson(t,it->second);
            SetJsonValueByKey(pt,it->first,t);
        }
    }
    SetJsonValueByKey(pt,"command","SetInstantaneousJointValues");
    SetJsonValueByKey(pt,"tasktype",_tasktype);
    SetJsonValueByKey(pt,"jointvalues",jointvalues);
    SetJsonValueByKey(pt,"unit",unit);
    rapidjson::Document d;
    ExecuteCommand(DumpJson(pt), d, timeout); // need to check return code
}

void BinPickingTaskResource::ComputeIkParamPosition(ResultComputeIkParamPosition& result, const std::string& name, const std::string& unit, const double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    {
        for(std::map<std::string,std::string>::iterator it=_mapTaskParameters.begin(); it!=_mapTaskParameters.end(); ++it) {
            rapidjson::Document t;
            ParseJson(t,it->second);
            SetJsonValueByKey(pt,it->first,t);
        }
    }
    SetJsonValueByKey(pt,"command","ComputeIkParamPosition");
    SetJsonValueByKey(pt,"tasktype",_tasktype);
    SetJsonValueByKey(pt,"name",name);
    SetJsonValueByKey(pt,"unit",unit);
    rapidjson::Document d;
    ExecuteCommand(DumpJson(pt), d, timeout);
    result.Parse(d);
}

void BinPickingTaskResource::ComputeIKFromParameters(ResultComputeIKFromParameters& result, const std::string& targetname, const std::vector<std::string>& ikparamnames, const int filteroptions, const int limit, const double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    {
        for(std::map<std::string,std::string>::iterator it=_mapTaskParameters.begin(); it!=_mapTaskParameters.end(); ++it) {
            rapidjson::Document t;
            ParseJson(t,it->second);
            SetJsonValueByKey(pt,it->first,t);
        }
    }
    SetJsonValueByKey(pt,"command","ComputeIKFromParameters");
    SetJsonValueByKey(pt,"tasktype",_tasktype);
    SetJsonValueByKey(pt,"targetname",targetname);
    SetJsonValueByKey(pt,"ikparamnames",ikparamnames);
    SetJsonValueByKey(pt,"filteroptions",filteroptions); // 0
    SetJsonValueByKey(pt,"limit",limit); // 0
    rapidjson::Document d;
    ExecuteCommand(DumpJson(pt), d, timeout);
    result.Parse(d);
}

void BinPickingTaskResource::MoveJoints(const std::vector<Real>& goaljoints, const std::vector<int>& jointindices, const Real envclearance, const Real speed, ResultMoveJoints& result, const double timeout, std::string* pTraj)
{
    _mapTaskParameters["execute"] = !!pTraj ? "0" : "1";
    SetMapTaskParameters(_ss, _mapTaskParameters);

    std::string command = "MoveJoints";
    std::string robottype = "densowave";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("robottype", robottype) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("goaljoints") << ": " << GetJsonString(goaljoints) << ", ";
    _ss << GetJsonString("jointindices") << ": " << GetJsonString(jointindices) << ", ";
    _ss << GetJsonString("envclearance",envclearance ) << ", ";
    _ss << GetJsonString("speed", speed);
    _ss << "}";

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
    if (!!pTraj) {
        SetTrajectory(pt, pTraj);
    }
}

void BinPickingTaskResource::GetTransform(const std::string& targetname, Transform& transform, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetTransform";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
    transform = result.transform;
}

void BinPickingTaskResource::SendMVRRegistrationResult(
    const rapidjson::Document &mvrResultInfo,
    double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "SendMVRRegistrationResult") << ", ";
    _ss << GetJsonString("mvrResultInfo", DumpJson(mvrResultInfo)) << ", ";
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);

}

void BinPickingTaskResource::SendRemoveObjectFromObjectListResult(
    const std::string& objectPk,
    bool success,
    double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "SendRemoveObjectFromObjectListResult") << ", ";
    _ss << GetJsonString("objectPk", objectPk) << ", ";
    _ss << GetJsonString("success", success) << ", ";
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
}


void BinPickingTaskResource::SendTriggerDetectionCaptureResult(const std::string& triggerType, const std::string& returnCode, double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "SendTriggerDetectionCaptureResult") << ", ";
    _ss << GetJsonString("triggerType", triggerType) << ", ";
    _ss << GetJsonString("returnCode", returnCode) << ", ";
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
}

void BinPickingTaskResource::SetTransform(const std::string& targetname, const Transform &transform, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "SetTransform";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString(transform) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout); // need to check return code
}

void BinPickingTaskResource::GetManipTransformToRobot(Transform& transform, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetManipTransformToRobot";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
    transform = result.transform;
}

void BinPickingTaskResource::GetManipTransform(Transform& transform, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetManipTransform";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
    transform = result.transform;
}

void BinPickingTaskResource::GetAABB(const std::string& targetname, ResultAABB& result, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetAABB";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::GetInnerEmptyRegionOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetInnerEmptyRegionOBB";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    if (linkname != "") {
        _ss << GetJsonString("linkname", linkname) << ", ";
    }
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::GetOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetOBB";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    if (linkname != "") {
        _ss << GetJsonString("linkname", linkname) << ", ";
    }
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
#ifdef MUJIN_USEZMQ
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        if ( reinitializetimeout > 0) {
            _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
        }
    }
#endif
}

void BinPickingTaskResource::UpdateObjects(const std::string& objectname, const std::vector<DetectedObject>& detectedobjects, const std::string& resultstate, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    static const std::string command = "UpdateObjects";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("objectname", objectname) << ", ";
    _ss << GetJsonString("envstate") << ": [";
    for (unsigned int i=0; i<detectedobjects.size(); i++) {
        _ss << GetJsonString(detectedobjects[i]);
        if (i!=detectedobjects.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    if (resultstate.size() == 0) {
        _ss << GetJsonString("detectionResultState") << ": {}, ";
    }
    else {
        _ss << GetJsonString("detectionResultState") << ": " << resultstate << ", ";
    }
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout); // need to check return code
}

void BinPickingTaskResource::AddPointCloudObstacle(const std::vector<float>&vpoints, const Real pointsize, const std::string& name,  const unsigned long long starttimestamp, const unsigned long long endtimestamp, const bool executionverification, const std::string& unit, int isoccluded, const std::string& locationName, const double timeout, bool clampToContainer, CropContainerMarginsXYZXYZPtr pCropContainerMargins, AddPointOffsetInfoPtr pAddPointOffsetInfo)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "AddPointCloudObstacle";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("isoccluded", isoccluded) << ", ";
    _ss << GetJsonString("locationName", locationName) << ", ";
    _ss << GetJsonString("clampToContainer", clampToContainer) << ", ";
    if( !!pCropContainerMargins ) {
        _ss << "\"cropContainerMarginsXYZXYZ\":[" << pCropContainerMargins->minMargins[0] << ", " << pCropContainerMargins->minMargins[1] << ", " << pCropContainerMargins->minMargins[2] << ", " << pCropContainerMargins->maxMargins[0] << ", " << pCropContainerMargins->maxMargins[1] << ", " << pCropContainerMargins->maxMargins[2] << "], ";
    }
    if( !!pAddPointOffsetInfo ) {
        _ss << "\"addPointOffsetInfo\":{";
        _ss << "\"zOffsetAtBottom\": " << pAddPointOffsetInfo->zOffsetAtBottom << ", ";
        _ss << "\"zOffsetAtTop\": " << pAddPointOffsetInfo->zOffsetAtTop << ", ";
        _ss << "\"use\": true";
        _ss << "}, ";
    }
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = name;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);

    // send timestamp regardless of the pointcloud definition
    _ss << ", \"starttimestamp\": " << starttimestamp;
    _ss << ", \"endtimestamp\": " << endtimestamp;
    _ss << ", \"executionverification\": " << (int) executionverification;

    _ss << ", " << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document rResult;
    ExecuteCommand(_ss.str(), rResult, timeout); // need to check return code
}

void BinPickingTaskResource::UpdateEnvironmentState(const std::string& objectname, const std::vector<DetectedObject>& detectedobjects, const std::vector<float>& vpoints, const std::string& state, const Real pointsize, const std::string& pointcloudobstaclename, const std::string& unit, const double timeout, const std::string& locationName, const std::vector<std::string>& cameranames, CropContainerMarginsXYZXYZPtr pCropContainerMargins)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "UpdateEnvironmentState";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("objectname", objectname) << ", ";
    _ss << GetJsonString("locationName", locationName) << ", ";
    _ss << "\"cameranames\":" << GetJsonString(cameranames) << ", ";
    _ss << GetJsonString("envstate") << ": [";
    for (unsigned int i=0; i<detectedobjects.size(); i++) {
        _ss << GetJsonString(detectedobjects[i]);
        if (i!=detectedobjects.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    if (state.size() == 0) {
        _ss << GetJsonString("detectionResultState") << ": {}, ";
    } else {
        _ss << GetJsonString("detectionResultState") << ": " << state << ", ";
    }
    _ss << GetJsonString("unit", unit) << ", ";
    if( !!pCropContainerMargins ) {
        _ss << "\"cropContainerMarginsXYZXYZ\":[" << pCropContainerMargins->minMargins[0] << ", " << pCropContainerMargins->minMargins[1] << ", " << pCropContainerMargins->minMargins[2] << ", " << pCropContainerMargins->maxMargins[0] << ", " << pCropContainerMargins->maxMargins[1] << ", " << pCropContainerMargins->maxMargins[2] << "], ";
    }
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = pointcloudobstaclename;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);

    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(),d, timeout); // need to check return code
}

void BinPickingTaskResource::RemoveObjectsWithPrefix(const std::string& prefix, double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "RemoveObjectsWithPrefix";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("prefix", prefix);
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(),d, timeout);

}
void BinPickingTaskResource::VisualizePointCloud(const std::vector<std::vector<float> >&pointslist, const Real pointsize, const std::vector<std::string>&names, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "VisualizePointCloud";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("pointslist") << ": [";
    for (unsigned int i=0; i< pointslist.size(); i++) {
        _ss << GetJsonString(pointslist[i]);
        if (i<pointslist.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    _ss << GetJsonString("pointsize") << ": " << pointsize << ", ";
    _ss << GetJsonString("names") << ": [";
    for (unsigned int i=0; i< names.size(); i++) {
        _ss << GetJsonString(names[i]);
        if (i<names.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "]" << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document d(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(),d, timeout); // need to check return code
}

void BinPickingTaskResource::ClearVisualization(const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "ClearVisualization";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout); // need to check return code
}

void BinPickingTaskResource::GetPickedPositions(ResultGetPickedPositions& r, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetPickedPositions";
    _ss << GetJsonString("command", command) << ",";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    r.Parse(pt);
}

void BinPickingTaskResource::IsRobotOccludingBody(const std::string& bodyname, const std::string& cameraname, const unsigned long long starttime, const unsigned long long endtime, bool& r, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "IsRobotOccludingBody";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    SensorOcclusionCheck check;
    check.bodyname = bodyname;
    check.cameraname = cameraname;
    check.starttime = starttime;
    check.endtime = endtime;
    _ss << GetJsonString(check);
    _ss << "}";
    ResultIsRobotOccludingBody result;
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
    r = result.result;
}

PlanningResultResourcePtr BinPickingTaskResource::GetResult()
{
    GETCONTROLLERIMPL();
    rapidjson::Document pt(rapidjson::kObjectType);
    controller->CallGet(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk,errormessage")%GetPrimaryKey()), pt);
    BinPickingResultResourcePtr result;
    if(pt.IsObject() && pt.HasMember("objects") && pt["objects"].IsArray() && pt["objects"].Size() > 0) {
        rapidjson::Value& objects = pt["objects"];
        std::string pk = GetJsonValueByKey<std::string>(objects[0], "pk", pk);
        result.reset(new BinPickingResultResource(controller, pk));
        std::string erroptional = GetJsonValueByKey<std::string>(objects[0], "errormessage");
        if (erroptional.size() > 0  && erroptional != "succeed") {
            throw MujinException(erroptional, MEC_BinPickingError);
        }
    }
    return result;
}

void BinPickingTaskResource::GetInstObjectAndSensorInfo(const std::vector<std::string>& instobjectnames, const std::vector<std::string>& sensornames, ResultGetInstObjectAndSensorInfo& result, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetInstObjectAndSensorInfo";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", _tasktype) << ", ";
    _ss << GetJsonString("instobjectnames") << ": " << GetJsonString(instobjectnames) << ", ";
    _ss << GetJsonString("sensornames") << ": " << GetJsonString(sensornames) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    try {
        result.Parse(pt);
    }
    catch(const std::exception& ex) {
        MUJIN_LOG_ERROR(str(boost::format("got error when parsing result: %s")%DumpJson(pt)));
        throw;
    }
}

void BinPickingTaskResource::GetInstObjectInfoFromURI(const std::string& objecturi, const Transform& instobjecttransform, ResultInstObjectInfo& result, const std::string& unit, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    std::string command = "GetInstObjectInfoFromURI";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("unit", unit) << ", ";
    _ss << "\"objecturi\":" << GetJsonString(objecturi) << ", ";
    _ss << "\"instobjectpose\":[";
    _ss << instobjecttransform.quaternion[0] << ", " << instobjecttransform.quaternion[1] << ", " << instobjecttransform.quaternion[2] << ", " << instobjecttransform.quaternion[3] << ", ";
    _ss << instobjecttransform.translate[0] << ", " << instobjecttransform.translate[1] << ", " << instobjecttransform.translate[2];
    _ss << "]";
    _ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    try {
        result.Parse(pt);
    }
    catch(const std::exception& ex) {
        MUJIN_LOG_ERROR(str(boost::format("got error when parsing result: %s")%DumpJson(pt)));
        throw;
    }
}

void BinPickingTaskResource::GetPublishedTaskState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout)
{
    ResultGetBinpickingState taskstate;
    {
        boost::mutex::scoped_lock lock(_mutexTaskState);
        taskstate =_taskstate;
    }

    if (taskstate.timestamp == 0) {
        if (_tasktype == "binpicking") {
            MUJIN_LOG_INFO("Have not received published message yet, getting published state from GetBinpickingState()");
            GetBinpickingState(result, robotname, unit, timeout);
        }
        else {
            MUJIN_LOG_INFO("Have not received published message yet, getting published state from GetITLState()");
            GetITLState(result, robotname, unit, timeout);
        }
        {
            boost::mutex::scoped_lock lock(_mutexTaskState);
            _taskstate = result;
        }
    } else {
        result = taskstate;
    }
}

void BinPickingTaskResource::GetBinpickingState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout)
{
    SerializeGetStateCommand(_ss, _mapTaskParameters, "GetState", _tasktype, robotname, unit, timeout);
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::GetITLState(ResultGetBinpickingState& result, const std::string& robotname, const std::string& unit, const double timeout)
{
    SerializeGetStateCommand(_ss, _mapTaskParameters, "GetState", _tasktype, robotname, unit, timeout);
    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    result.Parse(pt);
}

void BinPickingTaskResource::SetJogModeVelocities(const std::string& jogtype, const std::vector<int>& movejointsigns, const std::string& robotname, const std::string& toolname, const double robotspeed, const double robotaccelmult, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", std::string("SetJogModeVelocities")) << ", ";
    _ss << GetJsonString("jogtype", jogtype) << ", ";

    if (!robotname.empty()) {
        _ss << GetJsonString("robotname", robotname) << ", ";
    }
    if (!toolname.empty()) {
        _ss << GetJsonString("toolname", toolname) << ", ";
    }
    if (robotspeed >= 0) {
        _ss << GetJsonString("robotspeed") << ": " << robotspeed << ", ";
    }
    if (robotaccelmult >= 0) {
        _ss << GetJsonString("robotaccelmult") << ": " << robotaccelmult << ", ";
    }
    _ss << GetJsonString("movejointsigns") << ": " << GetJsonString(movejointsigns);
    _ss << "}";
    //std::cout << "Sending\n" << _ss.str() << " from " << __func__ << std::endl;
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout);
}

void BinPickingTaskResource::MoveToolLinear(const std::string& goaltype, const std::vector<double>& goals, const std::string& robotname, const std::string& toolname, const double workspeedlin, const double workspeedrot, bool checkEndeffectorCollision, const double timeout, std::string* pTraj)
{
    _mapTaskParameters["execute"] = !!pTraj ? "0" : "1";

    const std::string ignorethresh = checkEndeffectorCollision ? "0.0" : "1000.0"; // zero to not ignore collision at any time, large number (1000) to ignore collision of end effector for first and last part of trajectory as well as ignore collision of robot at initial part of trajectory
    _mapTaskParameters["workignorefirstcollisionee"] = ignorethresh;
    _mapTaskParameters["workignorelastcollisionee"] = ignorethresh;
    _mapTaskParameters["workignorefirstcollision"] = ignorethresh;
    if (workspeedlin > 0 && workspeedrot > 0) {
        std::stringstream ss;
        ss << "[" << workspeedrot << ", " << workspeedlin << "]";
        _mapTaskParameters["workspeed"] = ss.str();
    }
    GenerateMoveToolCommand("MoveToolLinear", goaltype, goals, robotname, toolname, -1, -1, _ss, _mapTaskParameters);

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    if (!!pTraj) {
        SetTrajectory(pt, pTraj);
    }
}

void BinPickingTaskResource::MoveToolLinear(const std::string& instobjectname, const std::string& ikparamname, const std::string& robotname, const std::string& toolname, const double workspeedlin, const double workspeedrot, bool checkEndeffectorCollision, const double timeout, std::string* pTraj)
{
    _mapTaskParameters["execute"] = !!pTraj ? "0" : "1";

    const std::string ignorethresh = checkEndeffectorCollision ? "0.0" : "1000.0"; // zero to not ignore collision at any time, large number (1000) to ignore collision of end effector for first and last part of trajectory as well as ignore collision of robot at initial part of trajectory
    _mapTaskParameters["workignorefirstcollisionee"] = ignorethresh;
    _mapTaskParameters["workignorelastcollisionee"] = ignorethresh;
    _mapTaskParameters["workignorefirstcollision"] = ignorethresh;
    if (workspeedlin > 0 && workspeedrot > 0) {
        std::stringstream ss;
        ss << "[" << workspeedrot << ", " << workspeedlin << "]";
        _mapTaskParameters["workspeed"] = ss.str();
    }
    GenerateMoveToolByIkParamCommand("MoveToolLinear", instobjectname, ikparamname, robotname, toolname, -1, -1, _ss, _mapTaskParameters);

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    if (!!pTraj) {
        SetTrajectory(pt, pTraj);
    }
}

void BinPickingTaskResource::MoveToHandPosition(const std::string& goaltype, const std::vector<double>& goals, const std::string& robotname, const std::string& toolname, const double robotspeed, const double timeout, Real envclearance, std::string* pTraj)
{
    _mapTaskParameters["execute"] = !!pTraj ? "0" : "1";

    GenerateMoveToolCommand("MoveToHandPosition", goaltype, goals, robotname, toolname, robotspeed, envclearance, _ss, _mapTaskParameters);

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    if (!!pTraj) {
        SetTrajectory(pt, pTraj);
    }
}

void BinPickingTaskResource::MoveToHandPosition(const std::string& instobjectname, const std::string& ikparamname, const std::string& robotname, const std::string& toolname, const double robotspeed, const double timeout, Real envclearance, std::string* pTraj)
{
    _mapTaskParameters["execute"] = !!pTraj ? "0" : "1";

    GenerateMoveToolByIkParamCommand("MoveToHandPosition", instobjectname, ikparamname, robotname, toolname, robotspeed, envclearance, _ss, _mapTaskParameters);

    rapidjson::Document pt(rapidjson::kObjectType);
    ExecuteCommand(_ss.str(), pt, timeout);
    if (!!pTraj) {
        SetTrajectory(pt, pTraj);
    }
}

void BinPickingTaskResource::GetGrabbed(std::vector<std::string>& grabbed, const std::string& robotname, const double timeout)
{
    grabbed.clear();
    rapidjson::Document pt(rapidjson::kObjectType);
    {
        for(std::map<std::string,std::string>::iterator it=_mapTaskParameters.begin(); it!=_mapTaskParameters.end(); ++it) {
            rapidjson::Document t;
            ParseJson(t,it->second);
            SetJsonValueByKey(pt,it->first,t);
        }
    }
    SetJsonValueByKey(pt,"command","GetGrabbed");
    SetJsonValueByKey(pt,"tasktype",_tasktype);
    if (!robotname.empty()) {
        SetJsonValueByKey(pt, "robotname", robotname);
    }
    rapidjson::Document d;
    ExecuteCommand(DumpJson(pt), d, timeout); // need to check return code
    BOOST_ASSERT(d.IsObject() && d.HasMember("output"));
    const rapidjson::Value& v = d["output"];
    if(v.HasMember("names") && !v["names"].IsNull()) {
        LoadJsonValueByKey(v, "names", grabbed);
    }
}

void BinPickingTaskResource::ExecuteSingleXMLTrajectory(const std::string& trajectory, bool filterTraj, const double timeout)
{
    _ss.str(""); _ss.clear();
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "ExecuteSingleXMLTrajectory") << ", "
        << GetJsonString("filtertraj", filterTraj ? 1 : 0) << ", "
        << GetJsonString("trajectory", trajectory) << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout);
}

void BinPickingTaskResource::Grab(const std::string& targetname, const std::string& robotname, const std::string& toolname, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "Grab") << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    if (!robotname.empty()) {
        _ss << GetJsonString("robotname", robotname) << ", ";
    }
    if (!toolname.empty()) {
        _ss << GetJsonString("toolname", toolname) << ", ";
    }
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout);
}

void BinPickingTaskResource::Release(const std::string& targetname, const std::string& robotname, const std::string& toolname, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "Release") << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    if (!robotname.empty()) {
        _ss << GetJsonString("robotname", robotname) << ", ";
    }
    if (!toolname.empty()) {
        _ss << GetJsonString("toolname", toolname) << ", ";
    }
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout);
}

void BinPickingTaskResource::GetRobotBridgeIOVariableString(const std::vector<std::string>& ionames, std::vector<std::string>& iovalues, const double timeout)
{
    SetMapTaskParameters(_ss, _mapTaskParameters);
    _ss << GetJsonString("command", "GetRobotBridgeIOVariableString") << ", ";
    _ss << "\"ionames\":" << GetJsonString(ionames);
    _ss << "}";
    rapidjson::Document d;
    ExecuteCommand(_ss.str(), d, timeout);
    BOOST_ASSERT(d.IsObject() && d.HasMember("output"));
    const rapidjson::Value& rIOOutputs = d["output"];

    // process
    iovalues.resize(ionames.size());
    for(size_t index = 0; index < ionames.size(); ++index) {
        if( rIOOutputs.HasMember(ionames[index].c_str()) ) {
            iovalues[index] = mujinjson::GetStringJsonValueByKey(rIOOutputs, ionames[index].c_str(), std::string());
        }
        else {
            MUJIN_LOG_DEBUG(str(boost::format("could not read ioname %s")%ionames[index]));
        }
    }
}

const std::string& BinPickingTaskResource::GetSlaveRequestId() const
{
    return _slaverequestid;
}

void BinPickingTaskResource::ExecuteCommand(const std::string& taskparameters, rapidjson::Document& rResult, const double timeout, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    GETCONTROLLERIMPL();

    std::stringstream ss; ss << std::setprecision(std::numeric_limits<double>::digits10+1);
    ss << "{\"tasktype\": \"" << _tasktype << "\", \"taskparameters\": " << taskparameters << ", ";
    ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    ss << "\"userinfo\": " << _userinfo_json << ", ";
    if (_slaverequestid != "") {
        ss << GetJsonString("slaverequestid", _slaverequestid) << ", ";
    }
    ss << "\"stamp\": " << (GetMilliTime()*1e-3) << ", ";
    ss << "\"callerid\": \"" << _GetCallerId() << "\"";
    ss << "}";
    rapidjson::Document pt(rapidjson::kObjectType);
    controller->CallPutJSON(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), ss.str(), pt);
    Execute();

    double secondspassed = 0;
    while (1) {
        BinPickingResultResourcePtr resultresource;
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(GetResult());
        if( !!resultresource ) {
            if (getresult) {
                resultresource->GetResultJson(rResult);
            }
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        secondspassed+=0.1;
        if( timeout != 0 && secondspassed > timeout ) {
            controller->CancelAllJobs();
            std::stringstream sss; sss << std::setprecision(std::numeric_limits<double>::digits10+1);
            sss << secondspassed;
            throw MujinException("operation timed out after " +sss.str() + " seconds, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::ExecuteCommand(rapidjson::Value& rTaskParameters, rapidjson::Document& rOutput, const double timeout)
{
    BOOST_ASSERT(0); // not supported
}

void utils::GetAttachedSensors(SceneResource& scene, const std::string& bodyname, std::vector<RobotResource::AttachedSensorResourcePtr>& attachedsensors)
{
    SceneResource::InstObjectPtr sensorinstobject;
    if (!scene.FindInstObject(bodyname, sensorinstobject)) {
        throw MujinException("Could not find instobject with name: " + bodyname+".", MEC_Failed);
    }

    RobotResourcePtr sensorrobot;
    sensorrobot.reset(new RobotResource(scene.GetController(),sensorinstobject->object_pk));
    sensorrobot->GetAttachedSensors(attachedsensors);
    if (attachedsensors.size() == 0) {
        throw MujinException("Could not find attached sensor. Is calibration done for sensor: " + bodyname + "?", MEC_Failed);
    }
}

void utils::GetSensorData(SceneResource& scene, const std::string& bodyname, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
    utils::GetAttachedSensors(scene, bodyname, attachedsensors);
    for (size_t i=0; i<attachedsensors.size(); ++i) {
        if (attachedsensors.at(i)->name == sensorname) {
            result = attachedsensors.at(i)->sensordata;
            return;
        }
    }
    throw MujinException("Could not find attached sensor " + sensorname + " on " + bodyname + ".", MEC_Failed);
}

void utils::GetSensorTransform(SceneResource& scene, const std::string& bodyname, const std::string& sensorname, Transform& result, const std::string& unit)
{
    std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
    utils::GetAttachedSensors(scene, bodyname, attachedsensors);
    for (size_t i=0; i<attachedsensors.size(); ++i) {
        if (attachedsensors.at(i)->name == sensorname) {
            Transform transform;
            std::copy(attachedsensors.at(i)->quaternion, attachedsensors.at(i)->quaternion+4, transform.quaternion);
            std::copy(attachedsensors.at(i)->translate, attachedsensors.at(i)->translate+3, transform.translate);
            if (unit == "m") { //?!
                transform.translate[0] *= 0.001;
                transform.translate[1] *= 0.001;
                transform.translate[2] *= 0.001;
            }

            result = transform;
            return;
        }
    }
    throw MujinException("Could not find attached sensor " + sensorname + " on " + bodyname + ".", MEC_Failed);
}

void utils::DeleteObject(SceneResource& scene, const std::string& name)
{
    //TODO needs to robot.Release(name)
    std::vector<SceneResource::InstObjectPtr> instobjects;
    scene.GetInstObjects(instobjects);

    for(unsigned int i = 0; i < instobjects.size(); ++i) {
        const std::size_t found_at = instobjects[i]->name.find(name);
        if (found_at != std::string::npos) {
            instobjects[i]->Delete();
            break;
        }
    }
}

void BinPickingTaskResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
#ifdef MUJIN_USEZMQ
    boost::shared_ptr<zmq::socket_t>  socket;

    while (!_bShutdownHeartbeatMonitor) {
        if (!!socket) {
            socket->close();
            socket.reset();
        }
        socket.reset(new zmq::socket_t((*_zmqcontext.get()),ZMQ_SUB));
        socket->setsockopt(ZMQ_TCP_KEEPALIVE, 1); // turn on tcp keepalive, do these configuration before connect
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_CNT, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
        std::stringstream ss; ss << std::setprecision(std::numeric_limits<double>::digits10+1);
        ss << _heartbeatPort;
        socket->connect(("tcp://"+ _mujinControllerIp+":"+ss.str()).c_str());
        socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);

        zmq::pollitem_t pollitem;
        memset(&pollitem, 0, sizeof(zmq::pollitem_t));
        pollitem.socket = socket->operator void*();
        pollitem.events = ZMQ_POLLIN;

        unsigned long long lastheartbeat = GetMilliTime();
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(&reply);
                std::string replystring((char *)reply.data (), (size_t)reply.size());
                //if ((size_t)reply.size() == 1 && ((char *)reply.data())[0]==255) {
                if (replystring == "255") {
                    lastheartbeat = GetMilliTime();
                }
            }
        }
        if (!_bShutdownHeartbeatMonitor) {
            std::stringstream sss; sss << std::setprecision(std::numeric_limits<double>::digits10+1);
            sss << (double)((GetMilliTime() - lastheartbeat)/1000.0f) << " seconds passed since last heartbeat signal, re-intializing ZMQ server.";
            MUJIN_LOG_INFO(sss.str());
        }
    }
#else
    MUJIN_LOG_ERROR("cannot create heartbeat monitor since not compiled with libzmq");
#endif
}

#ifdef MUJIN_USEZMQ
std::string utils::GetHeartbeat(const std::string& endpoint) {
    zmq::context_t zmqcontext(1);
    zmq::socket_t socket(zmqcontext, ZMQ_SUB);
    socket.connect(endpoint.c_str());
    socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::pollitem_t pollitem;
    memset(&pollitem, 0, sizeof(zmq::pollitem_t));
    pollitem.socket = socket;
    pollitem.events = ZMQ_POLLIN;

    zmq::poll(&pollitem,1, 50); // wait 50 ms for message
    if (!(pollitem.revents & ZMQ_POLLIN)) {
        return "";
    }

    zmq::message_t reply;
    socket.recv(&reply);
    const std::string received((char *)reply.data (), (size_t)reply.size());
#ifndef _WIN32
    return received;
#else
    // sometimes buffer can container \n or \\, which windows does not like
    std::string newbuffer;
    std::vector< std::pair<std::string, std::string> > serachpairs(2);
    serachpairs[0].first = "\n"; serachpairs[0].second = "";
    serachpairs[1].first = "\\"; serachpairs[1].second = "";
    SearchAndReplace(newbuffer, received, serachpairs);
    return newbuffer;
#endif
}


namespace {
std::string FindSmallestSlaveRequestId(const rapidjson::Value& pt) {
    // get all slave request ids
    std::vector<std::string> slavereqids;
    if (pt.IsObject() && pt.HasMember("slavestates")) {
        const rapidjson::Value& slavestates = pt["slavestates"];
        for (rapidjson::Document::ConstMemberIterator it = slavestates.MemberBegin(); it != slavestates.MemberEnd(); ++it) {
            static const std::string prefix("slaverequestid-");
            if (std::string(it->name.GetString()).substr(0,prefix.length()) == prefix) {
                // GetValueForSmallestSlaveRequestId uses slavereqid directly, so cannot substr here
                slavereqids.push_back(it->name.GetString());
            }
        }
    }

    // find numerically smallest suffix (find one with smallest ### in slave request id of form hostname_slave###)
    int smallest_suffix_index = -1;
    int smallest_suffix = INT_MAX;
    static const char searchstr('_');
    for (std::vector<std::string>::const_iterator it = slavereqids.begin();
         it != slavereqids.end(); ++it) {
        const size_t foundindex = it->rfind(searchstr);
        if (foundindex == std::string::npos) {
            continue;
        }

        if ((*it)[it->length()-1] < '0' || '9' < (*it)[it->length()-1]) {
            continue;
        }
        int suffix = 0;
        int po = 1;
        for (int i = it->length()-1; i >= 0 && '0' <= (*it)[i] && (*it)[i] <= '9'; i--, po *= 10) {
            suffix = suffix + ((*it)[i] - '0')*po;
        }

        if (smallest_suffix > suffix) {
            smallest_suffix = suffix;
            smallest_suffix_index = std::distance<std::vector<std::string>::const_iterator>(slavereqids.begin(), it);
        }
    }

    if (smallest_suffix_index == -1) {
        throw MujinException("Failed to find slave request id like hostname_slave### where ### is a number");
    }
    return slavereqids[smallest_suffix_index];
}

std::string GetValueForSmallestSlaveRequestId(const std::string& heartbeat,
                                              const std::string& key)
{

    rapidjson::Document pt(rapidjson::kObjectType);
    std::stringstream ss(heartbeat);
    ParseJson(pt, ss.str());
    try {
        const std::string slavereqid = FindSmallestSlaveRequestId(pt);
        std::string result;
        LoadJsonValue(pt["slavestates"][slavereqid.c_str()][key.c_str()], result);
        return result;
    }
    catch (const MujinException& ex) {
        throw MujinException(boost::str(boost::format("%s from heartbeat:\n%s")%ex.what()%heartbeat));
    }

}
}


std::string mujinclient::utils::GetScenePkFromHeartbeat(const std::string& heartbeat) {
    static const std::string prefix("mujin:/");
    return GetValueForSmallestSlaveRequestId(heartbeat, "currentsceneuri").substr(prefix.length());
}

std::string utils::GetSlaveRequestIdFromHeartbeat(const std::string& heartbeat) {
    rapidjson::Document pt;
    std::stringstream ss(heartbeat);
    ParseJson(pt, ss.str());
    try {
        static const std::string prefix("slaverequestid-");
        return FindSmallestSlaveRequestId(pt).substr(prefix.length());
    }
    catch (const MujinException& ex) {
        throw MujinException(boost::str(boost::format("%s from heartbeat:\n%s")%ex.what()%heartbeat));
    }
}
#endif

} // end namespace mujinclient
