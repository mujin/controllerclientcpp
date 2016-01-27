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
#ifndef MUJIN_CONTROLLERCLIENT_HANDEYECALIBRATIONTASK_H
#define MUJIN_CONTROLLERCLIENT_HANDEYECALIBRATIONTASK_H

#include <mujincontrollerclient/mujincontrollerclient.h>

namespace mujinclient {

/// \brief holds information about the binpicking task parameters
class MUJINCLIENT_API HandEyeCalibrationTaskParameters
{
public:
    HandEyeCalibrationTaskParameters() {
        SetDefaults();
    }

    virtual void SetDefaults();

    std::string command;
    std::string cameraname;
    std::vector<Real> halconpatternparameters;

    std::vector<Real> conedirection;
    Real coneangle;
    int orientationdensity;
    std::vector<Real> distances;

    int numsamples;

    std::string toolname;
    Transform transform;
    std::string targetarea;
    std::string samplingmethod;

    /*
    goals["command"] = "ComputeCalibrationPoses"
    goals["cameraname"] = "camera2"
    goals["halconpatternparameters"] =  [7, 7, 0.015875, 0.5]
    goals["patternvisibility"] = {"conedirection": [0, 0, -1], "coneangle" : 45, "orientationdensity" : 1, "distances": [0.6, 0.9, 0.1]}
    goals["numsamples"] = 15
    goals["patternposition"] = {"toolname": "Flange", "translation": [-170,0,0], "axisangle": [0,0,-numpy.pi/2]}
    */
};

class HandEyeCalibrationTaskResource;
typedef boost::shared_ptr<HandEyeCalibrationTaskResource> HandEyeCalibrationTaskResourcePtr;
typedef boost::weak_ptr<HandEyeCalibrationTaskResource> HandEyeCalibrationTaskResourceWeakPtr;

class MUJINCLIENT_API HandEyeCalibrationResultResource : public PlanningResultResource
{
public:
    class CalibrationResult
    {
public:
        std::vector<std::vector<Real> > poses;
        std::vector<std::vector<Real> > configs;
        std::vector<int> jointindices;
    };

    HandEyeCalibrationResultResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~HandEyeCalibrationResultResource() {
    }

    void GetCalibrationPoses(HandEyeCalibrationResultResource::CalibrationResult& result);
};
typedef boost::shared_ptr<HandEyeCalibrationResultResource> HandEyeCalibrationResultResourcePtr;

class MUJINCLIENT_API HandEyeCalibrationTaskResource : public TaskResource
{
public:
    HandEyeCalibrationTaskResource(const std::string& taskname, ControllerClientPtr controller, SceneResourcePtr scene);

    virtual ~HandEyeCalibrationTaskResource() {
    }

    virtual void ComputeCalibrationPoses(int timeout, HandEyeCalibrationTaskParameters& params, HandEyeCalibrationResultResource::CalibrationResult& result);
    virtual PlanningResultResourcePtr GetResult();

    /// \brief Get the task info for tasks of type <b>binpicking</b>
    virtual void GetTaskParameters(HandEyeCalibrationTaskParameters& taskparameters);

    /// \brief Set the task info for tasks of type <b>binpicking</b>
    virtual void SetTaskParameters(const HandEyeCalibrationTaskParameters& taskparameters);
private:
    std::string _GetOrCreateTaskAndGetPk(SceneResourcePtr scene, const std::string& taskname);
    std::string _taskname;
};

}

#endif
