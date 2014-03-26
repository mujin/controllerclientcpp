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
#include "binpickingtaskhttp.h"

namespace mujinclient {

BinPickingResultResource::BinPickingResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

BinPickingResultResource::~BinPickingResultResource()
{
}

boost::property_tree::ptree BinPickingResultResource::GetResultPtree() const
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    return pt.get_child("output");
}

BinPickingTaskResource::BinPickingTaskResource(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene) : TaskResource(controller,_GetOrCreateTaskAndGetPk(scene, taskname))
{
    tasktype = MUJIN_BINPICKING_TASKTYPE_HTTP;
    _taskname = taskname;
    _robotcontrollerip = robotcontrollerip;
    _robotcontrollerport = robotcontrollerport;
    _controller = controller;
    _scene = scene;
}

BinPickingTaskResource::~BinPickingTaskResource()
{
}

std::string BinPickingTaskResource::_GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname)
{
    TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8(taskname,"binpicking");
    std::string pk = task->Get("pk");
    return pk;
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
        result.reset(new BinPickingResultResource(_controller, pk));
        boost::optional<std::string> erroptional = objects.begin()->second.get_optional<std::string>("errormessage");
        if (!!erroptional && erroptional.get().size() > 0 ) {
            throw MujinException(erroptional.get(), MEC_BinPickingError);
        }
    }
    return result;
}

boost::property_tree::ptree BinPickingTaskResource::ExecuteCommand(const std::string& taskparameters, const int timeout, const bool getresult)
{
    GETCONTROLLERIMPL();

    std::string taskgoalput = boost::str(boost::format("{\"tasktype\": \"binpicking\", \"taskparameters\": %s }")% taskparameters);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), taskgoalput, pt);
    Execute();
 
    int iterations = 0;
    while (1) {
        BinPickingResultResourcePtr resultresource;
        resultresource = boost::dynamic_pointer_cast<BinPickingResultResource>(GetResult());
        if( !!resultresource ) {
            if (getresult) {
                return resultresource->GetResultPtree();
            }
            return boost::property_tree::ptree();
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        ++iterations;
        if( iterations > timeout ) {
            controller->CancelAllJobs();
            throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
        }
    }
}

void BinPickingTaskResource::GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
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

void BinPickingTaskResource::DeleteObject(const std::string& name)
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

void BinPickingTaskResource::UpdateObjects(const std::string& basename, const std::vector<DetectedObject>& detectedobjects)
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

} // end namespace mujinclient
