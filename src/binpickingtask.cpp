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

void BinPickingTaskParameters::SetDefaults()
{
    command = "GetJointValues";
    robottype = "densowave";
    controllerip = "";
    controllerport = 0;
    envclearance = 20;
    speed = 0.5;
    targetname = "";
}

std::string BinPickingTaskParameters::GenerateJsonString (const std::vector<Real>& vec) const
{
    std::stringstream ss; ss << std::setprecision(std::numeric_limits<Real>::digits10+1);
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

std::string BinPickingTaskParameters::GenerateJsonString (const std::vector<int>& vec) const
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

BinPickingResultResource::BinPickingResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

void BinPickingResultResource::GetResultGetJointValues(ResultGetJointValues& result)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    boost::property_tree::ptree& output = pt.get_child("output");
    BOOST_FOREACH(boost::property_tree::ptree::value_type& value, output) {
        if( value.first == "robottype") {
            result.robottype = value.second.data();
        }
        else if (value.first == "jointnames") {
            result.jointnames.resize(value.second.size());
            size_t i = 0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
                result.jointnames[i++] = boost::lexical_cast<std::string>(v.second.data());
            }
        }
        else if (value.first == "currentjointvalues" ) {
            result.currentjointvalues.resize(value.second.size());
            size_t i = 0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
                result.currentjointvalues[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "tools") {
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
                std::string first = v.first;
                BOOST_FOREACH(boost::property_tree::ptree::value_type &x, v.second) {
                    result.tools[first].push_back(boost::lexical_cast<Real>(x.second.data()));
                }
            }
        }
    }
}

void BinPickingResultResource::GetResultMoveJoints(BinPickingResultResource::ResultMoveJoints& result)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    boost::property_tree::ptree& output = pt.get_child("output");
    BOOST_FOREACH(boost::property_tree::ptree::value_type& value, output) {
        if (value.first == "robottype" ) {
            result.robottype = value.second.data();
        }
        else if (value.first == "timedjointvalues") {
            result.timedjointvalues.resize(value.second.size());
            size_t i = 0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
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
void BinPickingResultResource::GetResultTransform(Transform& transform)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    boost::property_tree::ptree& output = pt.get_child("output");
    BOOST_FOREACH(boost::property_tree::ptree::value_type& value, output) {
        if( value.first == "translation") {
            size_t i = 0;
            if ( value.second.size() != 3 ) {
                throw MujinException("the length of translation is invalid", MEC_Timeout);
            }
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
                transform.translate[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
        else if (value.first == "quaternion") {
            size_t i = 0;
            if ( value.second.size() != 4 ) {
                throw MujinException("the length of quaternion is invalid", MEC_Timeout);
            }
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second) {
                transform.quaternion[i++] = boost::lexical_cast<Real>(v.second.data());
            }
        }
    }
}

BinPickingTaskResource::BinPickingTaskResource(const std::string& taskname, const std::string& controllerip, const int controllerport, ControllerClientPtr controller, SceneResourcePtr scene) : TaskResource(controller,_GetOrCreateTaskAndGetPk(scene, taskname))
{
    _taskname = taskname;
    _controllerip = controllerip;
    _controllerport = controllerport;
}

std::string BinPickingTaskResource::_GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname)
{
    TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8(taskname,"binpicking");
    std::string pk = task->Get("pk");
    return pk;
}

void BinPickingTaskResource::GetJointValues(int timeout, BinPickingResultResource::ResultGetJointValues& result)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetJointValues";
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    param.robottype = "densowave";
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultGetJointValues(result);
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > timeout ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, double speed, int timeout, BinPickingResultResource::ResultMoveJoints& result)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "MoveJoints";
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    param.robottype = "densowave";
    param.goaljoints = jointvalues;
    param.jointindices = jointindices;
    param.envclearance = 20;
    param.speed = speed;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultMoveJoints(result);
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > timeout ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

Transform BinPickingTaskResource::GetTransform(const std::string& targetname)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;

    Transform transform;
    param.command = "GetTransform";
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    param.targetname = targetname;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
            return transform;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > 10 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::SetTransform(const std::string& targetname, const Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "SetTransform";
    param.targetname = targetname;
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    param.transform = transform;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > 10 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

Transform BinPickingTaskResource::GetManipTransformToRobot()
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;
    Transform transform;

    param.command = "GetManipTransformToRobot";
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
            return transform;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > 10 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

Transform BinPickingTaskResource::GetRobotTransform()
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;
    Transform transform;

    param.command = "GetRobotTransform";
    param.controllerip = _controllerip;
    param.controllerport = _controllerport;
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
            return transform;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > 10 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::InitializeZMQ(int zmqport)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "InitZMQ";
    param.port = zmqport;
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            // success
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        ++iterations;
        if( iterations > 20 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}
void BinPickingTaskResource::AddPointCloudObstacle(const std::vector<Real>& vpoints, Real pointsize, const std::string& name)
{
    GETCONTROLLERIMPL();
    BinPickingResultResourcePtr resultresource;

    std::stringstream ss; ss << std::setprecision(std::numeric_limits<Real>::digits10+1);

    ss << boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\":{\"command\":\"AddPointCloudObstacle\", \"pointsize\":%f, \"name\": \"%s\"")%pointsize%name);
    if( vpoints.size() > 0 ) {
        ss << ", \"points\":[" << vpoints.at(0);
        for(size_t i = 1; i < vpoints.size(); ++i) {
            ss << "," << vpoints[i];
        }
        ss << "]";
    }
    ss << "}}";
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), ss.str(), pt);

    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(this->GetResult());
        if( !!resultresource ) {
            return;
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > 10 ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::GetTaskParameters(BinPickingTaskParameters& taskparameters)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskResource::SetTaskParameters(const BinPickingTaskParameters& taskparameters)
{
    GETCONTROLLERIMPL();

    std::string taskgoalput = boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\":{\"command\":\"%s\", \"robottype\":\"%s\", \"controllerip\":\"%s\", \"controllerport\":%d, \"goaljoints\":%s, \"jointindices\":%s, \"envclearance\":%f, \"speed\": %.15f, \"targetname\": \"%s\", \"translation\":[%.15f, %.15f, %.15f], \"quaternion\":[%.15f, %.15f, %.15f, %.15f],\"port\": %d} }")%taskparameters.command%taskparameters.robottype%taskparameters.controllerip%taskparameters.controllerport%taskparameters.GenerateJsonString(taskparameters.goaljoints)%taskparameters.GenerateJsonString(taskparameters.jointindices)%taskparameters.envclearance%taskparameters.speed%taskparameters.targetname%taskparameters.transform.translate[0]%taskparameters.transform.translate[1]%taskparameters.transform.translate[2]%taskparameters.transform.quaternion[0]%taskparameters.transform.quaternion[1]%taskparameters.transform.quaternion[2]%taskparameters.transform.quaternion[3]%taskparameters.port);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), taskgoalput, pt);
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
        result.reset(new BinPickingResultResource(GetController(), pk));
        boost::optional<std::string> erroptional = objects.begin()->second.get_optional<std::string>("errormessage");
        if (!!erroptional && erroptional.get().size() > 0 ) {
            throw MujinException(erroptional.get(), MEC_BinPickingError);
        }
    }
    return result;
}

} // end namespace mujinclient
