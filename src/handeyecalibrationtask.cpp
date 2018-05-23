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
#include <mujincontrollerclient/handeyecalibrationtask.h>
#include <mujincontrollerclient/mujinjson.h>
namespace mujinclient {
using namespace mujinjson;

void HandEyeCalibrationTaskParameters::SetDefaults()
{
    command = "ComputeCalibrationPoses";
    halconpatternparameters.resize(4);
    halconpatternparameters[0] = 7;
    halconpatternparameters[1] = 7;
    halconpatternparameters[2] = 0.015875;
    halconpatternparameters[3] = 0.5;
    conedirection.resize(3);
    conedirection[0] = 0;
    conedirection[1] = 0;
    conedirection[2] = -1;
    coneangle = 45;
    orientationdensity = 1;
    distances.resize(3);
    numsamples = 15;
    toolname = "Flange";
}

HandEyeCalibrationResultResource::HandEyeCalibrationResultResource(ControllerClientPtr controller, const std::string& pk) : PlanningResultResource(controller,"binpickingresult", pk)
{
}

void HandEyeCalibrationResultResource::GetCalibrationPoses(HandEyeCalibrationResultResource::CalibrationResult& result)
{
    GETCONTROLLERIMPL();
    rapidjson::Document pt(rapidjson::kObjectType);
    controller->CallGet(boost::str(boost::format("%s/%s/?format=json&limit=1")%GetResourceName()%GetPrimaryKey()), pt);
    rapidjson::Value& output = pt["output"];
    LoadJsonValueByKey(output, "poses", result.poses);
    LoadJsonValueByKey(output, "configs", result.configs);
    LoadJsonValueByKey(output, "jointindices", result.jointindices);
}

HandEyeCalibrationTaskResource::HandEyeCalibrationTaskResource(const std::string& taskname, ControllerClientPtr controller, SceneResourcePtr scene) : TaskResource(controller,_GetOrCreateTaskAndGetPk(scene, taskname))
{
    _taskname = taskname;
}

std::string HandEyeCalibrationTaskResource::_GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname)
{
    TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8(taskname,"handeyecalibration");
    std::string pk = task->Get<std::string>("pk");
    return pk;
}

void HandEyeCalibrationTaskResource::ComputeCalibrationPoses(int timeout, HandEyeCalibrationTaskParameters& param, HandEyeCalibrationResultResource::CalibrationResult& result)
{
    GETCONTROLLERIMPL();
    HandEyeCalibrationResultResourcePtr resultresource;

    param.command = "ComputeCalibrationPoses";
    this->SetTaskParameters(param);
    this->Execute();
    int iterations = 0;
    while (1) {
        resultresource = boost::dynamic_pointer_cast<HandEyeCalibrationResultResource>(this->GetResult());
        if( !!resultresource ) {
            resultresource->GetCalibrationPoses(result);
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

void HandEyeCalibrationTaskResource::GetTaskParameters(HandEyeCalibrationTaskParameters& taskparameters)
{
    throw MujinException("not implemented yet");
}

void HandEyeCalibrationTaskResource::SetTaskParameters(const HandEyeCalibrationTaskParameters& taskparameters)
{
    GETCONTROLLERIMPL();

    std::string taskgoalput
        = boost::str(boost::format("{\"tasktype\": \"handeyecalibration\",\"taskparameters\":{\
\"command\":\"%s\",\
\"cameraname\": \"%s\",\
\"halconpatternparameters\": [%d,%d,%.15f,%.15f],\
\"patternvisibility\": {\"conedirection\": [%.15f, %.15f, %.15f], \"coneangle\": %.15f, \"orientationdensity\": %d,\
\"distances\": [%.15f, %.15f, %.15f]}, \"numsamples\": %d,\
\"toolname\": \"%s\",\
\"targetarea\": \"%s\",\
\"samplingmethod\": \"%s\",\
\"patternposition\": {\"translation\":[%.15f, %.15f, %.15f], \"quaternion\":[%.15f, %.15f, %.15f, %.15f]}}}")%taskparameters.command%taskparameters.cameraname%taskparameters.halconpatternparameters[0]%taskparameters.halconpatternparameters[1]%taskparameters.halconpatternparameters[2]%taskparameters.halconpatternparameters[3]%taskparameters.conedirection[0]%taskparameters.conedirection[1]%taskparameters.conedirection[2]%taskparameters.coneangle%taskparameters.orientationdensity%taskparameters.distances[0]%taskparameters.distances[1]%taskparameters.distances[2]%taskparameters.numsamples%taskparameters.toolname%taskparameters.targetarea%taskparameters.samplingmethod%taskparameters.transform.translate[0]%taskparameters.transform.translate[1]%taskparameters.transform.translate[2]%taskparameters.transform.quaternion[0]%taskparameters.transform.quaternion[1]%taskparameters.transform.quaternion[2]%taskparameters.transform.quaternion[3]);
    rapidjson::Document d;
    controller->CallPutJSON(str(boost::format("task/%s/?format=json")%GetPrimaryKey()), taskgoalput,d);
}

PlanningResultResourcePtr HandEyeCalibrationTaskResource::GetResult()
{
    GETCONTROLLERIMPL();
    rapidjson::Document pt(rapidjson::kObjectType);
    controller->CallGet(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk")%GetPrimaryKey()), pt);
    HandEyeCalibrationResultResourcePtr result;
    if (pt.IsObject() && pt.HasMember("objects") && pt["objects"].IsArray() && pt["objects"].Size() > 0) {
        rapidjson::Value& objects = pt["objects"];
        std::string pk = GetJsonValueByKey<std::string>(objects[0], "pk");
        result.reset(new HandEyeCalibrationResultResource(GetController(), pk));
        std::string erroptional = GetJsonValueByKey<std::string>(objects[0], "errormessage");
        if (erroptional.size() > 0 ) {
            throw MujinException(erroptional, MEC_HandEyeCalibrationError);
        }
    }
    return result;
}

} // end namespace mujinclient
