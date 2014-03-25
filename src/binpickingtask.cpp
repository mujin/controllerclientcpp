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
#include "common.h"
#include "controllerclientimpl.h"
#include <boost/thread.hpp> // for sleep
#include <mujincontrollerclient/binpickingtask.h>

namespace mujinclient {

BinPickingTaskParameters::BinPickingTaskParameters()
{
    SetDefaults();
}

BinPickingTaskParameters::~BinPickingTaskParameters()
{
}

void BinPickingTaskParameters::SetDefaults()
{
    command = "GetJointValues";
    robottype = "densowave";
    robotcontrollerip = "";
    robotcontrollerport = 0;
    envclearance = 20;
    speed = 0.5;
    targetname = "";
}

void BinPickingTaskParameters::GetJsonString (const std::vector<Real>& vec, std::string& str) const
{
    std::stringstream _ss;
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
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString (const std::vector<int>& vec, std::string& str) const
{
    std::stringstream _ss;
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
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString(const Transform& transform, std::string& str) const
{
    std::stringstream _ss;
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
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString(const DetectedObject& obj, std::string& str) const
{
    std::stringstream _ss;
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
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString(const PointCloudObstacle& obj, std::string& str) const
{
    std::stringstream _ss;
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
    std::string pointsstr;
    GetJsonString(points, pointsstr);
    _ss << "points: " << pointsstr;
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString(const SensorOcclusionCheck& check, std::string& str) const
{
    std::stringstream _ss;
    _ss.str(""); _ss.clear();
    _ss << "bodyname: " << "\"" << check.bodyname << "\", ";
    _ss << "sensorname: " << "\"" << check.sensorname << "\", ";
    _ss << "starttime: " << check.starttime <<", ";
    _ss << "endtime: " << check.endtime;
    str = _ss.str();
}

void BinPickingTaskParameters::GetJsonString(std::string &str) const
{
    std::stringstream _ss;
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << "\"command\": " << "\"" << command << "\", ";
    if (command == "GetJointValues") {
        _ss << "\"robottype\": " << "\"" << robottype << "\", ";
        _ss << "\"controllerip\": " << "\"" << robotcontrollerip << "\", ";
        _ss << "\"controllerport\": " << "\"" << robotcontrollerport << "\", ";
    }
    else if (command == "MoveJoints") {
        _ss << "\"robottype\": " << "\"" << robottype << "\", ";
        _ss << "\"controllerip\": " << "\"" << robotcontrollerip << "\", ";
        _ss << "\"controllerport\": " << "\"" << robotcontrollerport << "\", ";
        std::string goaljointsstr;
        GetJsonString(goaljoints, goaljointsstr);
        _ss << "\"goaljoints\": " << goaljointsstr << ", ";
        std::string jointindicesstr;
        GetJsonString(jointindices, jointindicesstr);
        _ss << "\"jointindices\": " << jointindicesstr << ", ";
        _ss << "\"envclearance\": " << envclearance << ", ";
        _ss << "\"speed\": " << speed << ", ";
    }
    else if (command == "GetTransform") {
        _ss << "\"targetname\": " << "\"" << targetname << "\", ";
    }
    else if (command == "SetTransform") {
        _ss << "\"targetname\": " << "\"" << targetname << "\", ";
        std::string transformstr;
        GetJsonString(transform, transformstr);
        _ss << transformstr << ", ";
    }
    else if (command == "GetManipTransformToRobot") {
    }
    else if (command == "GetManipTransform") {
    }
    else if (command == "GetRobotTransform") {
    }
    else if (command == "InitZMQ") {
        _ss << "\"port\": " << zmqport;        
    }
    else if (command == "UpdateObjects") {
        _ss << "\"objectname\": " << "\"" << targetname << "\", ";
        _ss << "\"object_uri\": " << "\"mujin:/" << targetname << ".mujin.dae\", ";
        _ss << "\"envstate\": {";
        for (unsigned int i=0; i<detectedobjects.size(); i++) {
            std::string detectedobjectstr;
            GetJsonString(detectedobjects[i], detectedobjectstr);
            _ss << detectedobjectstr;
            if (i!=detectedobjects.size()-1) {
                _ss << ", ";
            }
        }
        _ss << "}";
    }
    else if (command == "AddPointCloudObstacle") {
        std::string pointcloudobstaclestr;
        GetJsonString(pointcloudobstacle, pointcloudobstaclestr);
        _ss << pointcloudobstaclestr;
    }
    else if (command == "IsRobotOccludingBody") {
        std::string sensorocclusioncheckstr;
        GetJsonString(sensorocclusioncheck, sensorocclusioncheckstr);
        _ss << sensorocclusioncheckstr;
    }
    else if (command == "GetPickedPositions") {
    }
    _ss << "}";
    str = _ss.str();
}

BinPickingResultResource::BinPickingResultResource()
{
}

BinPickingResultResource::~BinPickingResultResource()
{
}

void BinPickingResultResource::GetResultGetJointValues(const boost::property_tree::ptree& output, BinPickingResultResource::ResultGetJointValues& result) const
{
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, output) {
        if( value.first == "robottype") {
            result.robottype = value.second.data();
        }
        else if (value.first == "jointnames") {
            result.jointnames.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                result.jointnames[i++] = boost::lexical_cast<std::string>(v.second.data());
            }
        }
        else if (value.first == "currentjointvalues" ) {
            result.currentjointvalues.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                result.currentjointvalues[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "tools") {
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                std::string first = v.first;
                BOOST_FOREACH(const boost::property_tree::ptree::value_type& value2, output) {
                    if( value2.first == "translate") {
                        unsigned int i = 0;
                        if ( value2.second.size() != 3 ) {
                            throw MujinException("the length of translation is invalid", MEC_Timeout);
                        }
                        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value2.second) {
                            result.tools[first].translate[i++] = boost::lexical_cast<Real>(v.second.data());
                        }
                    }
                    else if (value2.first == "quaternion") {
                        unsigned int i = 0;
                        if ( value2.second.size() != 4 ) {
                            throw MujinException("the length of quaternion is invalid", MEC_Timeout);
                        }
                        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value2.second) {
                            result.tools[first].quaternion[i++] = boost::lexical_cast<Real>(v.second.data());
                        }
                    }
                }
            }
        }
    }
}

void BinPickingResultResource::GetResultMoveJoints(const boost::property_tree::ptree& output, BinPickingResultResource::ResultMoveJoints& result) const
{
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, output) {
        if (value.first == "robottype" ) {
            result.robottype = value.second.data();
        }
        else if (value.first == "timedjointvalues") {
            result.timedjointvalues.resize(value.second.size());
            unsigned int i = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, value.second) {
                result.timedjointvalues[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "numpoints" ) {
            result.numpoints = boost::lexical_cast<int>(value.second.data());
        }
        /*
          else if (value.first == "elapsedtime" ) {
          //TODO lexical_cast doesn't work with such kind of string: "4.99999999999998e-06"
          result.elapsedtime = boost::lexical_cast<int>(value.second.data());
          }
        */
    }
}

void BinPickingResultResource::GetResultTransform(const boost::property_tree::ptree& output, Transform& transform) const
{
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, output) {
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

void BinPickingResultResource::GetResultIsRobotOccludingBody(const boost::property_tree::ptree& output, bool result) const
{
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, output) {
        if( value.first == "occluded") {
            result = boost::lexical_cast<int>(value.second.data())==1;
            return;
        }
    }
    throw MujinException("Output does not have \"occluded\" attribute!", MEC_Failed);
}

void BinPickingResultResource::GetResultGetPickedPositions(const boost::property_tree::ptree& output, BinPickingResultResource::ResultPickedPositions& pickedpositions) const
{
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& value, output) {
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
                pickedpositions.transforms.push_back(transform);
                pickedpositions.timestamps.push_back((unsigned long long)(boost::lexical_cast<Real>(iter->second.data())*1000.0f)); // convert from second to milisecond
            }
        }
    }
}

void BinPickingResultResource::GetResultGetJointValues(BinPickingResultResource::ResultGetJointValues& result) const
{
    boost::property_tree::ptree pt;
    GetResultPtree(pt);
    GetResultGetJointValues(pt, result);
}

void BinPickingResultResource::GetResultMoveJoints(BinPickingResultResource::ResultMoveJoints& result) const
{
    boost::property_tree::ptree pt;
    GetResultPtree(pt);
    GetResultMoveJoints(pt,result);
}

void BinPickingResultResource::GetResultTransform(Transform& result) const
{
    boost::property_tree::ptree pt;
    GetResultPtree(pt);
    GetResultTransform(pt,result);
}

void BinPickingResultResource::GetResultIsRobotOccludingBody(bool result) const
{
    boost::property_tree::ptree pt;
    GetResultPtree(pt);
    GetResultIsRobotOccludingBody(pt,result);
}

void BinPickingResultResource::GetResultGetPickedPositions(BinPickingResultResource::ResultPickedPositions& result) const
{
    boost::property_tree::ptree pt;
    GetResultPtree(pt);
    GetResultGetPickedPositions(pt,result);
}

BinPickingTaskResource::BinPickingTaskResource()
{
}

BinPickingTaskResource::~BinPickingTaskResource()
{
}

} // end namespace mujinclient
