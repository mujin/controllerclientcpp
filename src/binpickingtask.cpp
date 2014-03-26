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
#include <mujincontrollerclient/binpickingtask.h>
#include "binpickingtaskhttp.h"
#include "binpickingtaskzmq.h"

namespace mujinclient {

BinPickingTask::~BinPickingTask()
{
}

std::string BinPickingTask::GetJsonString (const std::vector<Real>& vec)
{
    _ss.str("");_ss.clear();
    _ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    _ss << "[";
    if( vec.size() > 0 ) {
        for (unsigned int i = 0; i < vec.size(); i++) {
            _ss << vec[i];
            if( i != vec.size()-1) {
                _ss << ", ";
            }
        }
    }
    _ss << "]";
    return _ss.str();
}

std::string BinPickingTask::GetJsonString (const std::vector<int>& vec)
{
    _ss.str(""); _ss.clear();
    _ss << "[";
    if( vec.size() > 0 ) {
        for (unsigned int i = 0; i < vec.size(); i++) {
            _ss << vec[i];
            if( i != vec.size()-1) {
                _ss << ", ";
            }
        }
    }
    _ss << "]";
    return _ss.str();
}

std::string BinPickingTask::GetJsonString(const Transform& transform)
{
    _ss.str(""); _ss.clear();
    _ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    // \"translation\":[%.15f, %.15f, %.15f], \"quaternion\":[%.15f, %.15f, %.15f, %.15f]
    _ss << "\"translation\": [";
    for (unsigned int i=0; i<3; i++) {
        _ss << transform.translate[i];
        if (i!=3-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    _ss << "\"quaternion\": [";
    for (unsigned int i=0; i<4; i++) {
        _ss << transform.quaternion[i];
        if (i!=4-1) {
            _ss << ", ";
        }
    }
    _ss << "]";
    return _ss.str();
}

std::string BinPickingTask::GetJsonString(const DetectedObject& obj)
{
    _ss.str(""); _ss.clear();
    _ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    //"{\"name\": \"obj\",\"translation_\":[100,200,300],\"quat_\":[1,0,0,0],\"confidence\":0.5}"
    _ss << "{";
    _ss << "\"name\": \"" << obj.name << "\", ";
    _ss << "\"translation_\": [";
    for (unsigned int i=0; i<3; i++) {
        _ss << obj.translation[i];
        if (i!=3-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    _ss << "\"quat_\": [";
    for (unsigned int i=0; i<4; i++) {
        _ss << obj.quaternion[i];
        if (i!=4-1) {
            _ss << ", ";
        }
    }
    _ss << "], ";
    _ss << "\"confidence\": " << obj.confidence;
    _ss << "}";
    return _ss.str();
}

std::string BinPickingTask::GetJsonString(const PointCloudObstacle& obj)
{
    _ss.str(""); _ss.clear();
    _ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    // "\"name\": __dynamicobstacle__, \"pointsize\": 0.005, \"points\": []
    _ss << "\"name\": " << "\"" << obj.name << "\", ";
    _ss << "\"pointsize\": " << obj.pointsize <<", ";
    std::vector<Real> points;
    points.resize(obj.points.size());
    for (unsigned int i=0; i<points.size(); i++) {
        points[i] = obj.points[i]*1000.0f; // convert from meter to milimeter
    }
    _ss << "points: " << GetJsonString(points);
    return _ss.str();
}

std::string BinPickingTask::GetJsonString(const SensorOcclusionCheck& check)
{
    _ss.str(""); _ss.clear();
    _ss << "bodyname: " << "\"" << check.bodyname << "\", ";
    _ss << "sensorname: " << "\"" << check.sensorname << "\", ";
    _ss << "starttime: " << check.starttime <<", ";
    _ss << "endtime: " << check.endtime;
    return _ss.str();
}

BinPickingTask::ResultBase::~ResultBase()
{
}

BinPickingTask::ResultGetJointValues::~ResultGetJointValues()
{
}

void BinPickingTask::ResultGetJointValues::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, _pt) {
        if( value.first == "robottype") {
            robottype = value.second.data();
        }
        else if (value.first == "jointnames") {
            jointnames.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                jointnames[i++] = boost::lexical_cast<std::string>(v.second.data());
            }
        }
        else if (value.first == "currentjointvalues" ) {
            currentjointvalues.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                currentjointvalues[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "tools") {
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                std::string first = v.first;
                BOOST_FOREACH(const boost::property_tree::ptree::value_type& value2, _pt) {
                    if( value2.first == "translate") {
                        unsigned int i = 0;
                        if ( value2.second.size() != 3 ) {
                            throw MujinException("the length of translation is invalid", MEC_Timeout);
                        }
                        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value2.second) {
                            tools[first].translate[i++] = boost::lexical_cast<Real>(v.second.data());
                        }
                    }
                    else if (value2.first == "quaternion") {
                        unsigned int i = 0;
                        if ( value2.second.size() != 4 ) {
                            throw MujinException("the length of quaternion is invalid", MEC_Timeout);
                        }
                        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value2.second) {
                            tools[first].quaternion[i++] = boost::lexical_cast<Real>(v.second.data());
                        }
                    }
                }
            }
        }
    }
}

BinPickingTask::ResultMoveJoints::~ResultMoveJoints()
{
}

void BinPickingTask::ResultMoveJoints::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, _pt) {
        if (value.first == "robottype" ) {
            robottype = value.second.data();
        }
        else if (value.first == "timedjointvalues") {
            timedjointvalues.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                timedjointvalues[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "numpoints" ) {
            numpoints = boost::lexical_cast<int>(value.second.data());
        }
        /*
          else if (value.first == "elapsedtime" ) {
          //TODO lexical_cast doesn't work with such kind of string: "4.99999999999998e-06"
          elapsedtime = boost::lexical_cast<int>(value.second.data());
          }
        */
    }
}

BinPickingTask::ResultTransform::~ResultTransform()
{
}

void BinPickingTask::ResultTransform::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, _pt) {
        if( value.first == "translation") {
            unsigned int i = 0;
            if ( value.second.size() != 3 ) {
                throw MujinException("the length of translation is invalid", MEC_Timeout);
            }
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                transform.translate[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "quaternion") {
            unsigned int i = 0;
            if ( value.second.size() != 4 ) {
                throw MujinException("the length of quaternion is invalid", MEC_Timeout);
            }
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                transform.quaternion[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
    }
}

BinPickingTask::ResultIsRobotOccludingBody::~ResultIsRobotOccludingBody()
{
}

void BinPickingTask::ResultIsRobotOccludingBody::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, _pt) {
        if( value.first == "occluded") {
            result = boost::lexical_cast<int>(value.second.data())==1;
            return;
        }
    }
    throw MujinException("Output does not have \"occluded\" attribute!", MEC_Failed);
}

BinPickingTask::ResultGetPickedPositions::~ResultGetPickedPositions()
{
}

void BinPickingTask::ResultGetPickedPositions::Parse(const boost::property_tree::ptree& pt)
{
    _pt = pt;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, _pt) {
        if( value.first == "positions") {
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                boost::property_tree::ptree::const_iterator iter = v.second.begin();
                Transform transform;
                // w,x,y,z
                for (unsigned int i=0; i<4; i++) {
                    transform.quaternion[i]= boost::lexical_cast<Real>(iter->second.data());
                    iter++;
                }
                // convert x,y,z in milimeter to meter
                for (unsigned int i=0; i<3; i++) {
                    transform.translate[i]= boost::lexical_cast<Real>(iter->second.data())/1000.0f; // convert from milimeter to meter
                    iter++;
                }
                transforms.push_back(transform);
                timestamps.push_back((unsigned long long)(boost::lexical_cast<Real>(iter->second.data())*1000.0f)); // convert from second to milisecond
            }
        }
    }
}

void BinPickingTask::GetJointValues(ResultGetJointValues& result)
{
    std::string command = "GetJointValues";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "\"robottype\": " << "\"" << robottype << "\", ";
    _ss << "\"controllerip\": " << "\"" << _robotcontrollerip << "\", ";
    _ss << "\"controllerport\": " << "\"" << _robotcontrollerport << "\", ";
    _ss << "}";

    result.Parse(ExecuteCommand(_ss.str()));
}

void BinPickingTask::MoveJoints(const std::vector<Real>& goaljoints, const std::vector<int>& jointindices, const Real envclearance, const Real speed, ResultMoveJoints& result)
{
    std::string command = "MoveJoints";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "\"robottype\": " << "\"" << robottype << "\", ";
    _ss << "\"controllerip\": " << "\"" << _robotcontrollerip << "\", ";
    _ss << "\"controllerport\": " << "\"" << _robotcontrollerport << "\", ";
    _ss << "\"goaljoints\": " << GetJsonString(goaljoints) << ", ";
    _ss << "\"jointindices\": " << GetJsonString(jointindices) << ", ";
    _ss << "\"envclearance\": " << envclearance << ", ";
    _ss << "\"speed\": " << speed << ", ";
    _ss << "}";
    result.Parse(ExecuteCommand(_ss.str()));
}

void BinPickingTask::GetTransform(const std::string& targetname, Transform& transform)
{
    std::string command = "GetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "\"targetname\": " << "\"" << targetname << "\", ";
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(),10));
    transform = result.transform;
}

void BinPickingTask::SetTransform(const std::string& targetname, const Transform& transform)
{
    std::string command = "SetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "\"targetname\": " << "\"" << targetname << "\", ";
    _ss << GetJsonString(transform) << ", ";
    _ss << "}";
    ExecuteCommand(_ss.str(), 10, false);
}

void BinPickingTask::GetManipTransformToRobot(Transform& transform)
{
    std::string command = "GetManipTransformToRobot";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), 10));
    transform = result.transform;
}

void BinPickingTask::GetManipTransform(Transform& transform)
{
    std::string command = "GetManipTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), 10));
    transform = result.transform;
}

void BinPickingTask::GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    throw MujinException("not implemented");
}

void BinPickingTask::DeleteObject(const std::string& name)
{
    throw MujinException("not implemented");
}

void BinPickingTask::InitZMQ(const int zmqport)
{
    std::string command = "InitZMQ";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "\"port\": " << zmqport;
    _ss << "}";
    ExecuteCommand(_ss.str(), 20, false);
}

void BinPickingTask::UpdateObjects(const std::string& basename, const std::vector<DetectedObject>& detectedobjects)
{
    std::string command = "UpdateObjects";
    std::string targetname = basename;
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"objectname\": " << "\"" << targetname << "\", ";
    _ss << "\"object_uri\": " << "\"mujin:/" << targetname << ".mujin.dae\", ";
    _ss << "\"envstate\": {";
    for (unsigned int i=0; i<detectedobjects.size(); i++) {
        _ss << GetJsonString(detectedobjects[i]);
        if (i!=detectedobjects.size()-1) {
            _ss << ", ";
        }
    }
    _ss << "}";
    _ss << "}";
    ExecuteCommand(_ss.str(), 20, false);
}

void BinPickingTask::AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name)
{
    std::string command = "AddPointCloudObstacle";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = name;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);
    _ss << "}";
    ExecuteCommand(_ss.str(), 10, false);
}

void BinPickingTask::GetPickedPositions(ResultGetPickedPositions& r)
{
    std::string command = "GetPickedPositions";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    _ss << "}";
    r.Parse(ExecuteCommand(_ss.str()));
}

void BinPickingTask::IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool r)
{
    std::string command = "IsRobotOccludingBody";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    SensorOcclusionCheck check;
    check.bodyname = bodyname;
    check.sensorname = sensorname;
    check.starttime = starttime;
    check.endtime = endtime;
    _ss << GetJsonString(check);
    _ss << "}";
    ResultIsRobotOccludingBody result;
    result.Parse(ExecuteCommand(_ss.str()));
    r = result.result;
}

BinPickingTaskPtr CreateBinPickingTask(const BinPickingTaskType& tasktype, const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene, const std::string& controllerip, const int zmqport)
{
    if (tasktype == MUJIN_BINPICKING_TASKTYPE_HTTP) {
        return BinPickingTaskPtr(new BinPickingTaskResource(taskname, robotcontrollerip, robotcontrollerport, controller, scene));
    } else if (tasktype == MUJIN_BINPICKING_TASKTYPE_ZMQ) {
        return BinPickingTaskPtr(new BinPickingTaskZmq(taskname, robotcontrollerip, robotcontrollerport, controller, scene, controllerip, zmqport));
    } else {
        std::stringstream ss;
        ss << tasktype;
        throw MujinException("tasktype " + ss.str() + " is not supported!", MEC_Failed);
    }
}

void DestroyBinPickingTask()
{
}

} // end namespace mujinclient
