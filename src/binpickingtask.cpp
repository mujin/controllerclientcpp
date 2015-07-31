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
#include "common.h"
#include "controllerclientimpl.h"
#include <boost/thread.hpp> // for sleep
#include <boost/property_tree/ptree.hpp>
#include "mujincontrollerclient/binpickingtask.h"

#ifdef MUJIN_USEZMQ
#include "mujincontrollerclient/zmq.hpp"
#endif

namespace mujinclient {

BinPickingResultResource::BinPickingResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

BinPickingResultResource::~BinPickingResultResource()
{
}

BinPickingTaskResource::BinPickingTaskResource(ControllerClientPtr pcontroller, const std::string& pk, const std::string& scenepk) : TaskResource(pcontroller,pk), _robotControllerUri(""), _robotDeviceIOUri(""), _zmqPort(-1), _heartbeatPort(-1), _bIsInitialized(false)
{
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

    /// HACK until can think of proper way to send sceneparams
    std::stringstream ss;
    ss.str("");
    ss << "{";
    ss << GetJsonString("scenetype", "mujincollada") << ", ";

    std::string scenebasename = pcontroller->GetNameFromPrimaryKey_UTF8(scenepk);
    ss << GetJsonString("sceneuri", std::string("mujin:/")+scenebasename) << ", ";

    // should stop sending scenefilename since it is a hack!
    std::string MUJIN_MEDIA_ROOT_DIR = "/var/www/media/u";
    char* pMUJIN_MEDIA_ROOT_DIR = getenv("MUJIN_MEDIA_ROOT_DIR");
    if( !!pMUJIN_MEDIA_ROOT_DIR ) {
        MUJIN_MEDIA_ROOT_DIR = pMUJIN_MEDIA_ROOT_DIR;
    }

    std::string scenefilename = MUJIN_MEDIA_ROOT_DIR + std::string("/") + pcontroller->GetUserName() + std::string("/") + scenebasename;
    ss << GetJsonString("scenefilename", scenefilename);
    // ss << GetJsonString("scale", "[1.0, 1.0,1.0]");
    ss << "}";
    _sceneparams_json = ss.str();
}

BinPickingTaskResource::~BinPickingTaskResource()
{
    _bShutdownHeartbeatMonitor = true;
    if (!!_pHeartbeatMonitorThread) {
        _pHeartbeatMonitorThread->join();
    }
}

void BinPickingTaskResource::Initialize(const std::string& robotControllerUri, const std::string& robotDeviceIOUri, const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout, const std::string& userinfo)
{
    _robotControllerUri = robotControllerUri;
    _robotDeviceIOUri = robotDeviceIOUri;
    _zmqPort = zmqPort;
    _heartbeatPort = heartbeatPort;
    _bIsInitialized = true;
    _zmqcontext = zmqcontext;
    _userinfo_json = userinfo;
}

boost::property_tree::ptree BinPickingResultResource::GetResultPtree() const
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    return pt.get_child("output");
}

std::string BinPickingTaskResource::GetJsonString(const std::string& str)
{
    return "\""+str+"\"";
}

std::string BinPickingTaskResource::GetJsonString (const std::vector<Real>& vec)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
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

std::string BinPickingTaskResource::GetJsonString (const std::vector<int>& vec)
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

std::string BinPickingTaskResource::GetJsonString(const std::vector<std::string>& vec)
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

std::string BinPickingTaskResource::GetJsonString(const Transform& transform)
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

std::string BinPickingTaskResource::GetJsonString(const DetectedObject& obj)
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
    ss << ", " <<GetJsonString("sensortimestamp") << ": " << obj.timestamp << ", ";
    ss << GetJsonString("extra") << ": " << obj.extra;
    ss << "}";
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const PointCloudObstacle& obj)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    // "\"name\": __dynamicobstacle__, \"pointsize\": 0.005, \"points\": []
    ss << GetJsonString("pointcloudid") << ": " << GetJsonString(obj.name) << ", ";
    ss << GetJsonString("pointsize") << ": " << obj.pointsize <<", ";
    std::vector<Real> points;
    points.resize(obj.points.size());
    for (unsigned int i=0; i<points.size(); i++) {
        //points[i] = obj.points[i]*1000.0f; // convert from meter to milimeter
        points[i] = obj.points[i]; // send in meter
    }
    ss << GetJsonString("points") << ": " << GetJsonString(points);
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const SensorOcclusionCheck& check)
{
    std::stringstream ss;
    ss << GetJsonString("bodyname") << ": " << GetJsonString(check.bodyname) << ", ";
    ss << GetJsonString("cameraname") << ": " << GetJsonString(check.cameraname) << ", ";
    ss << GetJsonString("starttime") << ": " << check.starttime <<", ";
    ss << GetJsonString("endtime") << ": " << check.endtime;
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const std::string& key, const std::string& value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << GetJsonString(value);
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const std::string& key, const int value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const std::string& key, const unsigned long long value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const std::string& key, const Real value)
{
    std::stringstream ss;
    ss << GetJsonString(key) << ": " << value;
    return ss.str();
}

BinPickingTaskResource::ResultGetJointValues::~ResultGetJointValues()
{
}

void BinPickingTaskResource::ResultGetJointValues::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if( value->first == "robottype") {
            robottype = value->second.data();
        }
        else if (value->first == "jointnames") {
            jointnames.resize(value->second.size());
            unsigned int i = 0;
            FOREACH(v, value->second) {
                jointnames[i++] = boost::lexical_cast<std::string>(v->second.data());
            }
        }
        else if (value->first == "currentjointvalues" ) {
            currentjointvalues.resize(value->second.size());
            unsigned int i = 0;
            FOREACH(v, value->second) {
                currentjointvalues[i++] = boost::lexical_cast<Real>(v->second.data());
            }
        }
        else if (value->first == "tools") {
            FOREACH(v, value->second) {
                std::string first = v->first;
                FOREACH(value2, _pt) {
                    if( value2->first == "translate") {
                        unsigned int i = 0;
                        if ( value2->second.size() != 3 ) {
                            throw MujinException("the length of translation is invalid", MEC_Timeout);
                        }
                        FOREACH(v, value2->second) {
                            tools[first].translate[i++] = boost::lexical_cast<Real>(v->second.data());
                        }
                    }
                    else if (value2->first == "quaternion") {
                        unsigned int i = 0;
                        if ( value2->second.size() != 4 ) {
                            throw MujinException("the length of quaternion is invalid", MEC_Timeout);
                        }
                        FOREACH(v, value2->second) {
                            tools[first].quaternion[i++] = boost::lexical_cast<Real>(v->second.data());
                        }
                    }
                }
            }
        }
    }
}

BinPickingTaskResource::ResultMoveJoints::~ResultMoveJoints()
{
}

void BinPickingTaskResource::ResultMoveJoints::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if (value->first == "robottype" ) {
            robottype = value->second.data();
        }
        else if (value->first == "timedjointvalues") {
            timedjointvalues.resize(value->second.size());
            unsigned int i = 0;
            FOREACH(v, value->second) {
                timedjointvalues[i++] = boost::lexical_cast<Real>(v->second.data());
            }
        }
        else if (value->first == "numpoints" ) {
            numpoints = boost::lexical_cast<int>(value->second.data());
        }
        /*
           else if (value.first == "elapsedtime" ) {
           //TODO lexical_cast doesn't work with such kind of string: "4.99999999999998e-06"
           elapsedtime = boost::lexical_cast<int>(value.second.data());
           }
         */
    }
}

BinPickingTaskResource::ResultTransform::~ResultTransform()
{
}

void BinPickingTaskResource::ResultTransform::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if( value->first == "translation") {
            unsigned int i = 0;
            if ( value->second.size() != 3 ) {
                throw MujinException("the length of translation is invalid", MEC_Failed);
            }
            FOREACH(v, value->second) {
                transform.translate[i++] = boost::lexical_cast<Real>(v->second.data());
            }
        }
        else if (value->first == "quaternion") {
            unsigned int i = 0;
            if ( value->second.size() != 4 ) {
                throw MujinException("the length of quaternion is invalid", MEC_Failed);
            }
            FOREACH(v, value->second) {
                transform.quaternion[i++] = boost::lexical_cast<Real>(v->second.data());
            }
        }
    }
}

BinPickingTaskResource::ResultGetInstObjectAndSensorInfo::~ResultGetInstObjectAndSensorInfo()
{
}

void BinPickingTaskResource::ResultGetInstObjectAndSensorInfo::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");

    FOREACH(v0, _pt.get_child("instobjects")) {
        std::string objname = v0->first;
        Transform transform;
        FOREACH(value, v0->second) {
            if( value->first == "translation") {
                unsigned int i = 0;
                if ( value->second.size() != 3 ) {
                    throw MujinException("the length of translation is invalid", MEC_Failed);
                }
                FOREACH(v, value->second) {
                    transform.translate[i++] = boost::lexical_cast<Real>(v->second.data());
                }
            }
            else if (value->first == "quaternion") {
                unsigned int i = 0;
                if ( value->second.size() != 4 ) {
                    throw MujinException("the length of quaternion is invalid", MEC_Failed);
                }
                FOREACH(v, value->second) {
                    transform.quaternion[i++] = boost::lexical_cast<Real>(v->second.data());
                }
            }
        }
        minstobjecttransform[objname] = transform;
    }
    FOREACH(v0, _pt.get_child("sensors")) {
        std::string sensorname = v0->first;
        Transform transform;
        RobotResource::AttachedSensorResource::SensorData sensordata;
        FOREACH(value, v0->second) {
            if( value->first == "translation") {
                unsigned int i = 0;
                if ( value->second.size() != 3 ) {
                    throw MujinException("the length of translation is invalid", MEC_Failed);
                }
                FOREACH(v, value->second) {
                    transform.translate[i++] = boost::lexical_cast<Real>(v->second.data());
                }
            }
            else if (value->first == "quaternion") {
                unsigned int i = 0;
                if ( value->second.size() != 4 ) {
                    throw MujinException("the length of quaternion is invalid", MEC_Failed);
                }
                FOREACH(v, value->second) {
                    transform.quaternion[i++] = boost::lexical_cast<Real>(v->second.data());
                }
            }
            else if (value->first == "sensordata") {
                FOREACH(v1, value->second) {
                    if (v1->first == "distortion_coeffs") {
                        unsigned int i = 0;
                        if ( v1->second.size() != 5 ) {
                            throw MujinException("the length of distortion_coeffs is invalid", MEC_Failed);
                        }
                        FOREACH(v, v1->second) {
                            sensordata.distortion_coeffs[i++] = boost::lexical_cast<Real>(v->second.data());
                        }
                    }
                    else if (v1->first == "intrinsic") {
                        unsigned int i = 0;
                        if ( v1->second.size() != 6 ) {
                            throw MujinException("the length of intrinsic is invalid", MEC_Failed);
                        }
                        FOREACH(v, v1->second) {
                            sensordata.intrinsic[i++] = boost::lexical_cast<Real>(v->second.data());
                        }
                    }
                    else if (v1->first == "image_dimensions") {
                        unsigned int i = 0;
                        if ( v1->second.size() != 3 ) {
                            throw MujinException("the length of image_dimensions is invalid", MEC_Failed);
                        }
                        FOREACH(v, v1->second) {
                            sensordata.image_dimensions[i++] = boost::lexical_cast<int>(v->second.data());
                        }
                    }
                    else if (v1->first == "extra_parameters") {
                        unsigned int i = 0;
                        FOREACH(v, v1->second) {
                            sensordata.extra_parameters[i++] = boost::lexical_cast<Real>(v->second.data());
                        }
                    }
                }
                sensordata.distortion_model = value->second.get<std::string>("distortion_model");
                sensordata.focal_length = value->second.get<Real>("focal_length");
                sensordata.measurement_time = value->second.get<Real>("measurement_time");
            }
        }
        msensortransform[sensorname] = transform;
        msensordata[sensorname] = sensordata;
    }
}

BinPickingTaskResource::ResultGetBinpickingState::~ResultGetBinpickingState()
{
}

void BinPickingTaskResource::ResultGetBinpickingState::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");

    statusPickPlace = _pt.get<std::string>("statusPickPlace", "unknown");
    pickAttemptFromSourceId = _pt.get<int>("pickAttemptFromSourceId", -1);
    timestamp = (unsigned long long)(_pt.get<double>("timestamp", 0));
    isRobotOccludingSourceContainer = _pt.get<bool>("isRobotOccludingSourceContainer", true);
    forceRequestDetectionResults = _pt.get<bool>("forceRequestDetectionResults", true);
}

BinPickingTaskResource::ResultIsRobotOccludingBody::~ResultIsRobotOccludingBody()
{
}

void BinPickingTaskResource::ResultIsRobotOccludingBody::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if( value->first == "occluded") {
            result = boost::lexical_cast<int>(value->second.data())==1;
            return;
        }
    }
    throw MujinException("Output does not have \"occluded\" attribute!", MEC_Failed);
}

void BinPickingTaskResource::ResultGetPickedPositions::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if( value->first == "positions") {
            FOREACH(v, value->second) {
                boost::property_tree::ptree::const_iterator iter = v->second.begin();
                Transform transform;
                // w,x,y,z
                for (unsigned int i=0; i<4; i++) {
                    transform.quaternion[i]= boost::lexical_cast<Real>(iter->second.data());
                    iter++;
                }
                // x,y,z
                for (unsigned int i=0; i<3; i++) {
                    //transform.translate[i]= boost::lexical_cast<Real>(iter->second.data())/1000.0f; // convert from milimeter to meter
                    transform.translate[i]= boost::lexical_cast<Real>(iter->second.data());
                    iter++;
                }
                transforms.push_back(transform);
                //timestamps.push_back((unsigned long long)(boost::lexical_cast<Real>(iter->second.data())*1000.0f)); // convert from second to milisecond
                timestamps.push_back((unsigned long long)(boost::lexical_cast<Real>(iter->second.data())));
            }
        }
    }
}

void BinPickingTaskResource::ResultAABB::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if (value->first == "pos") {
            if (value->second.size() != 3) {
                throw MujinException("The length of pos is invalid.", MEC_Failed);
            }
            FOREACH(v,value->second) {
                pos.push_back(boost::lexical_cast<Real>(v->second.data()));
            }
        }
        if (value->first == "extents") {
            if (value->second.size() != 3) {
                throw MujinException("The length of extents is invalid.", MEC_Failed);
            }
            FOREACH(v,value->second) {
                extents.push_back(boost::lexical_cast<Real>(v->second.data()));
            }
        }
    }
}

void BinPickingTaskResource::ResultOBB::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt.get_child("output");
    FOREACH(value, _pt) {
        if (value->first == "translation") {
            if (value->second.size() != 3) {
                throw MujinException("The length of translation is invalid.", MEC_Failed);
            }
            FOREACH(v, value->second) {
                translation.push_back(boost::lexical_cast<Real>(v->second.data()));
            }
        }
        if (value->first == "extents") {
            if (value->second.size() != 3) {
                throw MujinException("The length of extents is invalid.", MEC_Failed);
            }
            FOREACH(v,value->second) {
                extents.push_back(boost::lexical_cast<Real>(v->second.data()));
            }
        }
        if (value->first == "rotationmat") {
            if (value->second.size() != 3) {
                throw MujinException("The row number of rotationmat is invalid.", MEC_Failed);
            }
            FOREACH(vr, value->second) {
                if (vr->second.size() != 3) {
                    throw MujinException("The column number of rotationmat is invalid.", MEC_Failed);
                }
                FOREACH(vc, vr->second) {
                    rotationmat.push_back(boost::lexical_cast<Real>(vc->second.data()));
                }
            }
        }
    }
}

BinPickingTaskResource::ResultHeartBeat::~ResultHeartBeat()
{
}

void BinPickingTaskResource::ResultHeartBeat::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    status = "";
    msg = "";
    timestamp = 0;
    taskstate = "";
    FOREACH(value, _pt) {
        if (value->first == "status") {
            status = std::string(value->second.data());
        }
        if (value->first == "msg") {
            msg = std::string(value->second.data());
        }
        if (value->first == "timestamp") {
            timestamp = boost::lexical_cast<Real>(value->second.data());
        }
        if (value->first == "taskstate") {
            taskstate = std::string(value->second.data());
        }
    }
}


void BinPickingTaskResource::GetJointValues(ResultGetJointValues& result, const double timeout)
{
    std::string command = "GetJointValues";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("robottype", robottype) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("robotControllerUri", _robotControllerUri) << ", ";
    _ss << GetJsonString("robotDeviceIOUri", _robotDeviceIOUri);
    _ss << "}";

    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::MoveJoints(const std::vector<Real>& goaljoints, const std::vector<int>& jointindices, const Real envclearance, const Real speed, ResultMoveJoints& result, const double timeout)
{
    std::string command = "MoveJoints";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("robottype", robottype) << ", ";
    _ss << GetJsonString("robotControllerUri", _robotControllerUri) << ", ";
    _ss << GetJsonString("robotDeviceIOUri", _robotDeviceIOUri) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("goaljoints") << ": " << GetJsonString(goaljoints) << ", ";
    _ss << GetJsonString("jointindices") << ": " << GetJsonString(jointindices) << ", ";
    _ss << GetJsonString("envclearance",envclearance ) << ", ";
    _ss << GetJsonString("speed", speed);
    _ss << "}";
    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::GetTransform(const std::string& targetname, Transform& transform, const std::string& unit, const double timeout)
{
    std::string command = "GetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::SetTransform(const std::string& targetname, const Transform &transform, const std::string& unit, const double timeout)
{
    std::string command = "SetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString(transform) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::GetManipTransformToRobot(Transform& transform, const std::string& unit, const double timeout)
{
    std::string command = "GetManipTransformToRobot";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("robotControllerUri", _robotControllerUri) << ", ";
    _ss << GetJsonString("robotDeviceIOUri", _robotDeviceIOUri) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::GetManipTransform(Transform& transform, const std::string& unit, const double timeout)
{
    std::string command = "GetManipTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("robotControllerUri", _robotControllerUri) << ", ";
    _ss << GetJsonString("robotDeviceIOUri", _robotDeviceIOUri) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::GetAABB(const std::string& targetname, ResultAABB& result, const std::string& unit, const double timeout)
{
    std::string command = "GetAABB";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::GetInnerEmptyRegionOBB(ResultOBB& result, const std::string& targetname, const std::string& linkname, const std::string& unit, const double timeout)
{
    std::string command = "GetInnerEmptyRegionOBB";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("targetname", targetname) << ", ";
    if (linkname != "") {
        _ss << GetJsonString("linkname", linkname) << ", ";
    }
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    result.Parse(ExecuteCommand(_ss.str(), timeout));
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

void BinPickingTaskResource::UpdateObjects(const std::string& basename, const std::vector<Transform>&transformsworld, const std::vector<std::string>&confidence, const std::string& state, const std::string& unit, const double timeout)
{
    std::string command = "UpdateObjects";
    std::string targetname = basename;
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("objectname", targetname) << ", ";
    _ss << GetJsonString("object_uri", "mujin:/"+targetname+".mujin.dae") << ", ";
    std::vector<DetectedObject> detectedobjects;
    for (unsigned int i=0; i<transformsworld.size(); i++) {
        DetectedObject detectedobject;
        std::stringstream name_ss;
        name_ss << basename << "_" << i;
        detectedobject.name = name_ss.str();
        detectedobject.transform = transformsworld[i];
        detectedobject.confidence = confidence[i];
        detectedobjects.push_back(detectedobject);
    }

    _ss << GetJsonString("envstate") << ": [";
    for (unsigned int i=0; i<detectedobjects.size(); i++) {
        _ss << GetJsonString(detectedobjects[i]);
        if (i!=detectedobjects.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    _ss << GetJsonString("state") << ": " << GetJsonString(state) << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::AddPointCloudObstacle(const std::vector<Real>&vpoints, const Real pointsize, const std::string& name, const double timeout)
{
    std::string command = "AddPointCloudObstacle";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = name;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::UpdateEnvironmentState(const std::string& basename, const std::string& default_object_uri, const std::vector<DetectedObject>& detectedobjects, const std::vector<Real>& vpoints, const std::string& state, const Real pointsize, const std::string& pointcloudobstaclename, const std::string& unit, const double timeout)
{
    std::string command = "UpdateEnvironmentState";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("objectname", basename) << ", ";
    _ss << GetJsonString("object_uri", default_object_uri) << ", ";
    
    _ss << GetJsonString("envstate") << ": [";  
    for (unsigned int i=0; i<detectedobjects.size(); i++) {
        _ss << GetJsonString(detectedobjects[i]);
        if (i!=detectedobjects.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";

    _ss << GetJsonString("detectionResultState") << ": " << state << ", ";
    _ss << GetJsonString("unit", unit) << ", ";
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = pointcloudobstaclename;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);
    
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::VisualizePointCloud(const std::vector<std::vector<Real> >&pointslist, const Real pointsize, const std::vector<std::string>&names, const double timeout)
{
    std::string command = "VisualizePointCloud";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
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
    _ss << "]";
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::ClearVisualization(const double timeout)
{
    std::string command = "ClearVisualization";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json;
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::GetPickedPositions(ResultGetPickedPositions& r, const std::string& unit, const double timeout)
{
    std::string command = "GetPickedPositions";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ",";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    r.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::IsRobotOccludingBody(const std::string& bodyname, const std::string& cameraname, const unsigned long long starttime, const unsigned long long endtime, bool& r, const double timeout)
{
    std::string command = "IsRobotOccludingBody";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << GetJsonString("robotControllerUri", _robotControllerUri) << ", ";
    _ss << GetJsonString("robotDeviceIOUri", _robotDeviceIOUri) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    SensorOcclusionCheck check;
    check.bodyname = bodyname;
    check.cameraname = cameraname;
    check.starttime = starttime;
    check.endtime = endtime;
    _ss << GetJsonString(check);
    _ss << "}";
    ResultIsRobotOccludingBody result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    r = result.result;
}

PlanningResultResourcePtr BinPickingTaskResource::GetResult()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk,errormessage")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    BinPickingResultResourcePtr result;
    if( objects.size() > 0 ) {
        std::string pk = objects.begin()->second.get<std::string>("pk");
        result.reset(new BinPickingResultResource(controller, pk));
        boost::optional<std::string> erroptional = objects.begin()->second.get_optional<std::string>("errormessage");
        if (!!erroptional && erroptional.get().size() > 0 ) {
            throw MujinException(erroptional.get(), MEC_BinPickingError);
        }
    }
    return result;
}

void BinPickingTaskResource::GetInstObjectAndSensorInfo(const std::vector<std::string>& instobjectnames, const std::vector<std::string>& sensornames, ResultGetInstObjectAndSensorInfo& result, const std::string& unit, const double timeout)
{
    std::string command = "GetInstObjectAndSensorInfo";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << GetJsonString("instobjectnames") << ": " << GetJsonString(instobjectnames) << ", ";
    _ss << GetJsonString("sensornames") << ": " << GetJsonString(sensornames) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    boost::property_tree::ptree pt = ExecuteCommand(_ss.str(), timeout);
    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::GetPublishedTaskState(ResultGetBinpickingState& result, const std::string& unit, const double timeout)
{
    std::string taskstate;
    {
        boost::mutex::scoped_lock lock(_mutexTaskState);
        taskstate =_taskstate;
    }

    if (taskstate == "") {
        GetBinpickingState(result, unit, timeout);
    } else {
        boost::property_tree::ptree pt;
        std::stringstream ss;
        ss << "{\"output\":" << taskstate << "}";
        boost::property_tree::read_json(ss, pt);
        result.Parse(pt);
    }
}

void BinPickingTaskResource::GetBinpickingState(ResultGetBinpickingState& result, const std::string& unit, const double timeout)
{
    std::string taskstate;
    std::string command = "GetBinpickingState";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command", command) << ", ";
    _ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
    _ss << "\"sceneparams\": " << _sceneparams_json << ", ";
    _ss << "\"userinfo\": " << _userinfo_json << ", ";
    _ss << GetJsonString("unit", unit);
    _ss << "}";
    boost::property_tree::ptree pt = ExecuteCommand(_ss.str(), timeout);
    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

boost::property_tree::ptree BinPickingTaskResource::ExecuteCommand(const std::string& taskparameters, const double timeout, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    GETCONTROLLERIMPL();

    std::string taskgoalput = "{\"tasktype\": \"binpicking\", \"taskparameters\": " + taskparameters + "}";
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), taskgoalput, pt);
    Execute();

    double secondspassed = 0;
    while (1) {
        BinPickingResultResourcePtr resultresource;
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(GetResult());
        if( !!resultresource ) {
            if (getresult) {
                return resultresource->GetResultPtree();
            }
            return boost::property_tree::ptree();
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        secondspassed+=0.1;
        if( timeout != 0 && secondspassed > timeout ) {
            controller->CancelAllJobs();
            std::stringstream ss;
            ss << secondspassed;
            throw MujinException("operation timed out after " +ss.str() + " seconds, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void utils::GetAttachedSensors(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, std::vector<RobotResource::AttachedSensorResourcePtr>& attachedsensors)
{
    SceneResource::InstObjectPtr sensorinstobject;
    if (!scene->FindInstObject(bodyname, sensorinstobject)) {
        throw MujinException("Could not find instobject with name: " + bodyname+".", MEC_Failed);
    }

    RobotResourcePtr sensorrobot;
    sensorrobot.reset(new RobotResource(controller,sensorinstobject->object_pk));
    sensorrobot->GetAttachedSensors(attachedsensors);
    if (attachedsensors.size() == 0) {
        throw MujinException("Could not find attached sensor. Is calibration done for sensor: " + bodyname + "?", MEC_Failed);
    }
}

void utils::GetSensorData(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
    utils::GetAttachedSensors(controller, scene, bodyname, attachedsensors);
    for (size_t i=0; i<attachedsensors.size(); ++i) {
        if (attachedsensors.at(i)->name == sensorname) {
            result = attachedsensors.at(i)->sensordata;
            return;
        }
    }
    throw MujinException("Could not find attached sensor " + sensorname + " on " + bodyname + ".", MEC_Failed); 
}

void utils::GetSensorTransform(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& bodyname, const std::string& sensorname, Transform& result, const std::string& unit)
{
    SceneResource::InstObjectPtr cameraobj;
    if (scene->FindInstObject(bodyname, cameraobj)) {
        for (size_t i=0; i<cameraobj->attachedsensors.size(); ++i) {
            if (cameraobj->attachedsensors.at(i).name == sensorname) {
                Transform transform;
                std::copy(cameraobj->attachedsensors.at(i).quaternion, cameraobj->attachedsensors.at(i).quaternion+4, transform.quaternion);
                std::copy(cameraobj->attachedsensors.at(i).translate, cameraobj->attachedsensors.at(i).translate+3, transform.translate);
                if (unit == "m") {
                    transform.translate[0] *= 0.001;
                    transform.translate[1] *= 0.001;
                    transform.translate[2] *= 0.001;
                }
                  
                result = transform;
                return;
            }
        }
        throw MujinException("Could not find attached sensor " + sensorname + " on " + bodyname + ".", MEC_Failed);
    } else {
        throw MujinException("Could not find camera body " + bodyname +".", MEC_Failed);
    }
}

void utils::DeleteObject(SceneResourcePtr scene, const std::string& name)
{
    //TODO needs to robot.Release(name)
    std::vector<SceneResource::InstObjectPtr> instobjects;
    scene->GetInstObjects(instobjects);

    for(unsigned int i = 0; i < instobjects.size(); ++i) {
        unsigned int found_at = instobjects[i]->name.find(name);
        if (found_at != std::string::npos) {
            instobjects[i]->Delete();
            break;
        }
    }
}

void utils::UpdateObjects(SceneResourcePtr scene,const std::string& basename, const std::vector<BinPickingTaskResource::DetectedObject>&detectedobjects, const std::string& unit)
{
    std::vector<SceneResource::InstObjectPtr> oldinstobjects;
    std::vector<SceneResource::InstObjectPtr> oldtargets;

    // get all instobjects from mujin controller
    scene->GetInstObjects(oldinstobjects);
    for(unsigned int i = 0; i < oldinstobjects.size(); ++i) {
        unsigned int found_at = oldinstobjects[i]->name.find(basename);
        if (found_at != std::string::npos) {
            oldtargets.push_back(oldinstobjects[i]);
        }
    }

    std::vector<InstanceObjectState> state_update_pool;
    std::vector<SceneResource::InstObjectPtr> instobject_update_pool;
    std::vector<Transform> transform_create_pool;
    std::vector<std::string> name_create_pool;

    Real conversion = 1.0f;
    if (unit=="m") {
        // detectedobject->translation is in meter, need to convert to millimeter
        conversion = 1000.0f;
    }

    for (unsigned int obj_i = 0; obj_i < detectedobjects.size(); ++obj_i) {
        Transform objecttransform;
        objecttransform.translate[0] = detectedobjects[obj_i].transform.translate[0] * conversion;
        objecttransform.translate[1] = detectedobjects[obj_i].transform.translate[1] * conversion;
        objecttransform.translate[2] = detectedobjects[obj_i].transform.translate[2] * conversion;
        objecttransform.quaternion[0] = detectedobjects[obj_i].transform.quaternion[0];
        objecttransform.quaternion[1] = detectedobjects[obj_i].transform.quaternion[1];
        objecttransform.quaternion[2] = detectedobjects[obj_i].transform.quaternion[2];
        objecttransform.quaternion[3] = detectedobjects[obj_i].transform.quaternion[3];

        int nearest_index = 0;
        double maxdist = (std::numeric_limits<double>::max)();
        if ( oldtargets.size() == 0 ) {
            // create new object
            transform_create_pool.push_back(objecttransform);
            std::string name = boost::str(boost::format("%s_%d")%basename%obj_i);
            name_create_pool.push_back(name);
        } else {
            // update existing object
            for (unsigned int j = 0; j < oldtargets.size(); ++j) {
                double dist=0;
                for (int x = 0; x < 3; x++) {
                    dist += std::pow(objecttransform.translate[x] - oldtargets[j]->translate[x], 2);
                }
                if ( dist < maxdist ) {
                    nearest_index = j;
                    maxdist = dist;
                }
            }
            instobject_update_pool.push_back(oldtargets[nearest_index]);
            InstanceObjectState state;
            state.transform = objecttransform;
            state_update_pool.push_back(state);
            oldtargets.erase(oldtargets.begin() + nearest_index);
        }
    }
    // remove non-existent oldtargets
    for(unsigned int i = 0; i < oldtargets.size(); i++) {
        oldtargets[i]->Delete();
    }

    // update existing objects
    if (instobject_update_pool.size() != 0 ) {
        scene->SetInstObjectsState(instobject_update_pool, state_update_pool);
    }
    // create new objects
    for(unsigned int i = 0; i < name_create_pool.size(); i++) {
        scene->CreateInstObject(name_create_pool[i], ("mujin:/"+basename+".mujin.dae"), transform_create_pool[i].quaternion, transform_create_pool[i].translate);
    }
}

void BinPickingTaskResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
#ifdef MUJIN_USEZMQ
    boost::shared_ptr<zmq::socket_t>  socket;
    socket.reset(new zmq::socket_t((*_zmqcontext.get()),ZMQ_SUB));
    std::stringstream ss;
    ss << _heartbeatPort;
    socket->connect (("tcp://"+ _mujinControllerIp+":"+ss.str()).c_str());
    socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::pollitem_t pollitem;
    memset(&pollitem, 0, sizeof(zmq::pollitem_t));
    pollitem.socket = socket->operator void*();
    pollitem.events = ZMQ_POLLIN;

    while (!_bShutdownHeartbeatMonitor) {
        unsigned long long lastheartbeat = GetMilliTime();
        //unsigned long long initialtimestamp = GetMilliTime();
        std::string command = "InitializeZMQ";
        //ss.str(""); ss.clear();
        //ss << initialtimestamp;
        //std::string heartbeatMessage = ss.str();
        ss.str(""); ss.clear();
        ss << "{";
        ss << GetJsonString("command",command) << ", ";
        // ss << GetJsonString("tasktype", std::string("binpicking")) << ", ";
        ss << "\"sceneparams\": " << _sceneparams_json << ", ";
        ss << "\"userinfo\": " << _userinfo_json << ", ";
        ss << GetJsonString("port", _zmqPort) << ", ";
        ss << GetJsonString("heartbeatPort", _heartbeatPort);
        //ss << GetJsonString("heartbeatMessage", initialtimestamp);
        ss << "}";
        BinPickingTaskResource::ExecuteCommand(ss.str(), commandtimeout, false); // need to execute this command via http
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(&reply);
                std::string replystring((char *) reply.data (), (size_t) reply.size());
                //if ((size_t)reply.size() == 1 && ((char *)reply.data())[0]==255) {
                if (replystring == "255") {
                    lastheartbeat = GetMilliTime();
                }
            }
        }
        if (!_bShutdownHeartbeatMonitor) {
            std::stringstream ss;
            ss << (double)((GetMilliTime() - lastheartbeat)/1000.0f) << " seconds passed since last heartbeat signal, re-intializing ZMQ server.";
            BINPICKING_LOG_INFO(ss.str());
        }
    }
#else
    BINPICKING_LOG_ERROR("cannot create heartbeat monitor since not compiled with libzmq");
#endif
}


} // end namespace mujinclient
