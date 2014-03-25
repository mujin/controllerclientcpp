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
#include <mujincontrollerclient/binpickingtaskhttp.h>

namespace mujinclient {

BinPickingResultHttpResource::BinPickingResultHttpResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

BinPickingResultHttpResource::~BinPickingResultHttpResource()
{
}

void BinPickingResultHttpResource::GetResultPtree(boost::property_tree::ptree& output) const
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    output = pt.get_child("output");
}

BinPickingTaskHttpResource::BinPickingTaskHttpResource(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene) : TaskResource(controller,_GetOrCreateTaskAndGetPk(scene, taskname))
{
    _taskname = taskname;
    _robotcontrollerip = robotcontrollerip;
    _robotcontrollerport = robotcontrollerport;
    _controller = controller;
    _scene = scene;
    _sscmd << std::setprecision(7);
}

BinPickingTaskHttpResource::~BinPickingTaskHttpResource()
{
}

std::string BinPickingTaskHttpResource::_GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname)
{
    TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8(taskname,"binpicking");
    std::string pk = task->Get("pk");
    return pk;
}


void BinPickingTaskHttpResource::GetTaskParameters(BinPickingTaskParameters& params) const
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskHttpResource::SetTaskParameters(const BinPickingTaskParameters& taskparameters)
{
    GETCONTROLLERIMPL();

    std::string taskparametersstr;
    taskparameters.GetJsonString(taskparametersstr);
    std::string taskgoalput = boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\": %s }")% taskparametersstr);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), taskgoalput, pt);
}

PlanningResultResourcePtr BinPickingTaskHttpResource::GetResult()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk,errormessage")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    BinPickingResultHttpResourcePtr result;
    if( objects.size() > 0 ) {
        std::string pk = objects.begin()->second.get<std::string>("pk");
        result.reset(new BinPickingResultHttpResource(_controller, pk));
        boost::optional<std::string> erroptional = objects.begin()->second.get_optional<std::string>("errormessage");
        if (!!erroptional && erroptional.get().size() > 0 ) {
            throw MujinException(erroptional.get(), MEC_BinPickingError);
        }
    }
    return result;
}

void BinPickingTaskHttpResource::GetJointValues(const int timeout, BinPickingResultResource::ResultGetJointValues& result)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetJointValues";
    param.robotcontrollerip = _robotcontrollerip;
    param.robotcontrollerport = _robotcontrollerport;
    param.robottype = "densowave";
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
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

void BinPickingTaskHttpResource::MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real speed, const int timeout, BinPickingResultResource::ResultMoveJoints& result)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "MoveJoints";
    param.robotcontrollerip = _robotcontrollerip;
    param.robotcontrollerport = _robotcontrollerport;
    param.robottype = "densowave";
    param.goaljoints = jointvalues;
    param.jointindices = jointindices;
    param.envclearance = 20;
    param.speed = speed;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
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

void BinPickingTaskHttpResource::GetTransform(const std::string& targetname, Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetTransform";
    param.targetname = targetname;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
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

void BinPickingTaskHttpResource::SetTransform(const std::string& targetname, const Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "SetTransform";
    param.targetname = targetname;
    param.transform = transform;
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
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

void BinPickingTaskHttpResource::GetManipTransformToRobot(Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetManipTransformToRobot";
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
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

void BinPickingTaskHttpResource::GetManipTransform(Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetManipTransform";
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
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

void BinPickingTaskHttpResource::GetRobotTransform(Transform& transform)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "GetRobotTransform";
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetResultTransform(transform);
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

void BinPickingTaskHttpResource::GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    SceneResource::InstObjectPtr sensorinstobject;
    if (!_scene->FindInstObject(sensorname, sensorinstobject)) {
        throw MujinException("Could not find instobject with name: " + sensorname+".", MEC_Failed);
    }

    RobotResourcePtr sensorrobot;
    std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
    sensorrobot.reset(new RobotResource(_controller,sensorinstobject->object_pk));
    sensorrobot->GetAttachedSensors(attachedsensors);
    if (attachedsensors.size() == 0) {
        throw MujinException("Could not find attached sensor. Is calibration done for sensor: " + sensorname + "?", MEC_Failed);
    }
    result = attachedsensors[0]->sensordata;
}

void BinPickingTaskHttpResource::DeleteObject(const std::string& name)
{
    //TODO needs to robot.Release(name)
    std::vector<SceneResource::InstObjectPtr> instobjects;
    _scene->GetInstObjects(instobjects);

    for(unsigned int i = 0; i < instobjects.size(); ++i) {
        unsigned int found_at = instobjects[i]->name.find(name);
        if (found_at != std::string::npos) {
            instobjects[i]->Delete();
            break;
        }
    }
}

void BinPickingTaskHttpResource::UpdateObjects(const std::string& basename, const std::vector<BinPickingTaskParameters::DetectedObject>& detectedobjects)
{
    std::vector<SceneResource::InstObjectPtr> oldinstobjects;
    std::vector<SceneResource::InstObjectPtr> oldtargets;

    // get all instobjects from mujin controller
    _scene->GetInstObjects(oldinstobjects);
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

    for (unsigned int obj_i = 0; obj_i < detectedobjects.size(); ++obj_i) {
        Transform objecttransform;
        // detectedobject->translation is in meter, need to convert to milimeter
        objecttransform.translate[0] = detectedobjects[obj_i].translation[0] * 1000.0f;
        objecttransform.translate[1] = detectedobjects[obj_i].translation[1] * 1000.0f;
        objecttransform.translate[2] = detectedobjects[obj_i].translation[2] * 1000.0f;
        objecttransform.quaternion[0] = detectedobjects[obj_i].quaternion[0];
        objecttransform.quaternion[1] = detectedobjects[obj_i].quaternion[1];
        objecttransform.quaternion[2] = detectedobjects[obj_i].quaternion[2];
        objecttransform.quaternion[3] = detectedobjects[obj_i].quaternion[3];

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
        _scene->SetInstObjectsState(instobject_update_pool, state_update_pool);
    }
    // create new objects
    for(unsigned int i = 0; i < name_create_pool.size(); i++) {
        _scene->CreateInstObject(name_create_pool[i], ("mujin:/"+basename+".mujin.dae"), transform_create_pool[i].quaternion, transform_create_pool[i].translate);
    }
}

void BinPickingTaskHttpResource::InitializeZMQ(const int zmqport)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;
    BinPickingTaskParameters param;

    param.command = "InitZMQ";
    param.zmqport = zmqport;
    this->SetTaskParameters(param);
    this->Execute();

    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
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

void BinPickingTaskHttpResource::AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name)
{
    GETCONTROLLERIMPL();
    BinPickingResultHttpResourcePtr resultresource;

    _sscmd.str(""); _sscmd.clear();
    _sscmd << std::setprecision(std::numeric_limits<Real>::digits10+1);

    _sscmd << boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\":{\"command\":\"AddPointCloudObstacle\", \"pointsize\":%f, \"name\": \"%s\"")%pointsize%name);
    if( vpoints.size() > 0 ) {
        _sscmd << ", \"points\":[" << vpoints.at(0);
        for(unsigned int i = 1; i < vpoints.size(); ++i) {
            _sscmd << "," << vpoints[i];
        }
        _sscmd << "]";
    }
    _sscmd << "}}";
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), _sscmd.str(), pt);

    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<BinPickingResultHttpResource>(this->GetResult());
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

void BinPickingTaskHttpResource::IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskHttpResource::GetPickedPositions(BinPickingResultResource::ResultPickedPositions& result)
{
    throw MujinException("not implemented yet");
}

} // end namespace mujinclient
