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

namespace mujinclient {

BinPickingResultResource::BinPickingResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

BinPickingResultResource::~BinPickingResultResource()
{
}

BinPickingTaskResource::BinPickingTaskResource(ControllerClientPtr controller, const std::string& pk) : TaskResource(controller,pk), _robotcontrollerip(""), _robotcontrollerport(-1), _zmqport(-1), _bIsInitialized(false)
{
}

BinPickingTaskResource::~BinPickingTaskResource()
{
}

void BinPickingTaskResource::Initialize(const std::string& robotcontrollerip, const int robotcontrollerport, const int zmqport)
{
    _robotcontrollerip = robotcontrollerip;
    _robotcontrollerport = robotcontrollerport;
    _zmqport = zmqport;
    _bIsInitialized = true;
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
    if( vec.size() > 0 ) {
        for (unsigned int i = 0; i < vec.size(); i++) {
            ss << vec[i];
            if( i != vec.size()-1) {
                ss << ", ";
            }
        }
    }
    ss << "]";
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString (const std::vector<int>& vec)
{
    std::stringstream ss;
    ss << "[";
    if( vec.size() > 0 ) {
        for (unsigned int i = 0; i < vec.size(); i++) {
            ss << vec[i];
            if( i != vec.size()-1) {
                ss << ", ";
            }
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
    ss << "}";
    return ss.str();
}

std::string BinPickingTaskResource::GetJsonString(const PointCloudObstacle& obj)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
    // "\"name\": __dynamicobstacle__, \"pointsize\": 0.005, \"points\": []
    ss << GetJsonString("name") << ": " << GetJsonString(obj.name) << ", ";
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
    ss << GetJsonString("sensorname") << ": " << GetJsonString(check.sensorname) << ", ";
    ss << GetJsonString("starttime") << ": " << check.starttime <<", ";
    ss << GetJsonString("endtime") << ": " << check.endtime;
    return ss.str();
}

BinPickingTaskResource::ResultBase::~ResultBase()
{
}

BinPickingTaskResource::ResultGetJointValues::~ResultGetJointValues()
{
}

void BinPickingTaskResource::ResultGetJointValues::Parse(const boost::property_tree::ptree& pt)
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

BinPickingTaskResource::ResultMoveJoints::~ResultMoveJoints()
{
}

void BinPickingTaskResource::ResultMoveJoints::Parse(const boost::property_tree::ptree& pt)
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

BinPickingTaskResource::ResultTransform::~ResultTransform()
{
}

void BinPickingTaskResource::ResultTransform::Parse(const boost::property_tree::ptree& pt)
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

BinPickingTaskResource::ResultIsRobotOccludingBody::~ResultIsRobotOccludingBody()
{
}

void BinPickingTaskResource::ResultIsRobotOccludingBody::Parse(const boost::property_tree::ptree& pt)
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

BinPickingTaskResource::ResultGetPickedPositions::~ResultGetPickedPositions()
{
}

void BinPickingTaskResource::ResultGetPickedPositions::Parse(const boost::property_tree::ptree& pt)
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

void BinPickingTaskResource::GetJointValues(ResultGetJointValues& result, const double timeout)
{
    std::string command = "GetJointValues";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("robottype") << ": " << GetJsonString(robottype) << ", ";
    _ss << GetJsonString("controllerip") << ": " << GetJsonString(_robotcontrollerip) << ", ";
    _ss << GetJsonString("controllerport") << ": " << _robotcontrollerport;
    _ss << "}";

    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::MoveJoints(const std::vector<Real>& goaljoints, const std::vector<int>& jointindices, const Real envclearance, const Real speed, ResultMoveJoints& result, const double timeout)
{
    std::string command = "MoveJoints";
    std::string robottype = "densowave";

    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("robottype") << ": " << GetJsonString(robottype) << ", ";
    _ss << GetJsonString("controllerip") << ": " << GetJsonString(_robotcontrollerip) << ", ";
    _ss << GetJsonString("controllerport") << ": " << _robotcontrollerport << ", ";
    _ss << GetJsonString("goaljoints") << ": " << GetJsonString(goaljoints) << ", ";
    _ss << GetJsonString("jointindices") << ": " << GetJsonString(jointindices) << ", ";
    _ss << GetJsonString("envclearance") << ": " << envclearance << ", ";
    _ss << GetJsonString("speed") << ": " << speed; // << ", ";
    _ss << "}";
    result.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::GetTransform(const std::string& targetname, Transform& transform, const double timeout)
{
    std::string command = "GetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("targetname") << ": " << GetJsonString(targetname);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::SetTransform(const std::string& targetname, const Transform& transform, const double timeout)
{
    std::string command = "SetTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("targetname") << ": " << GetJsonString(targetname) << ", ";
    _ss << GetJsonString(transform);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::GetManipTransformToRobot(Transform& transform, const double timeout)
{
    std::string command = "GetManipTransformToRobot";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::GetManipTransform(Transform& transform, const double timeout)
{
    std::string command = "GetManipTransform";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command);
    _ss << "}";
    ResultTransform result;
    result.Parse(ExecuteCommand(_ss.str(), timeout));
    transform = result.transform;
}

void BinPickingTaskResource::InitializeZMQ(const double timeout)
{
    std::string command = "InitializeZMQ";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("port") << ": " << _zmqport;
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::UpdateObjects(const std::string& basename, const std::vector<Transform>& transformsworld, const std::vector<Real>& confidence, const std::string& unit, const double timeout)
{
    std::string command = "UpdateObjects";
    std::string targetname = basename;
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    _ss << GetJsonString("objectname") << ": " << GetJsonString(targetname) << ", ";
    _ss << GetJsonString("object_uri") << ": " << GetJsonString("mujin:/"+targetname+".mujin.dae") << ", ";
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
    _ss << GetJsonString("unit") << ": " << GetJsonString(unit);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::AddPointCloudObstacle(const std::vector<Real>&vpoints, const Real pointsize, const std::string& name, const double timeout)
{
    std::string command = "AddPointCloudObstacle";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = name;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    _ss << GetJsonString(pointcloudobstacle);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::VisualizePointCloud(const std::vector<std::vector<Real> >& pointslist, const Real pointsize, const std::vector<std::string>& names, const double timeout)
{
    std::string command = "VisualizePointCloud";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
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
    _ss << GetJsonString("command") << ": " << GetJsonString(command);
    _ss << "}";
    ExecuteCommand(_ss.str(), timeout, false);
}

void BinPickingTaskResource::GetPickedPositions(ResultGetPickedPositions& r, const double timeout)
{
    std::string command = "GetPickedPositions";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command);
    _ss << "}";
    r.Parse(ExecuteCommand(_ss.str(), timeout));
}

void BinPickingTaskResource::IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool r, const double timeout)
{
    std::string command = "IsRobotOccludingBody";
    _ss.str(""); _ss.clear();
    _ss << "{";
    _ss << GetJsonString("command") << ": " << GetJsonString(command) << ", ";
    SensorOcclusionCheck check;
    check.bodyname = bodyname;
    check.sensorname = sensorname;
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

boost::property_tree::ptree BinPickingTaskResource::ExecuteCommand(const std::string& taskparameters, const double timeout, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    GETCONTROLLERIMPL();

    std::string taskgoalput = boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\": %s }")% taskparameters);
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

void utils::GetSensorData(ControllerClientPtr controller, SceneResourcePtr scene, const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    SceneResource::InstObjectPtr sensorinstobject;
    if (!scene->FindInstObject(sensorname, sensorinstobject)) {
        throw MujinException("Could not find instobject with name: " + sensorname+".", MEC_Failed);
    }

    RobotResourcePtr sensorrobot;
    std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
    sensorrobot.reset(new RobotResource(controller,sensorinstobject->object_pk));
    sensorrobot->GetAttachedSensors(attachedsensors);
    if (attachedsensors.size() == 0) {
        throw MujinException("Could not find attached sensor. Is calibration done for sensor: " + sensorname + "?", MEC_Failed);
    }
    result = attachedsensors[0]->sensordata;
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

void utils::UpdateObjects(SceneResourcePtr scene,const std::string& basename, const std::vector<BinPickingTaskResource::DetectedObject>& detectedobjects, const std::string& unit)
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

} // end namespace mujinclient
