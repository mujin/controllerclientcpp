// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>
#include <mujincontrollerclient/binpickingtask.h>
#include <mujincontrollerclient/handeyecalibrationtask.h>

#include <boost/thread/thread.hpp> // for sleep

#include <iostream>

using namespace mujinclient;

int main(int argc, char ** argv)
{
    if( argc < 2 ) {
        std::cout << "need username:password. Example: mujinclienttest myuser:mypass [url]\n\nurl - [optional] For example https://controller.mujin.co.jp/" << std::endl;
        return 1;
    }
    try {
        ControllerClientPtr controller;
        if( argc >= 5 ) {
            controller = CreateControllerClient(argv[1], argv[2], argv[3], argv[4]);
        }
        if( argc == 4 ) {
            controller = CreateControllerClient(argv[1], argv[2], argv[3]);
        }
        else if( argc == 3 ) {
            controller = CreateControllerClient(argv[1], argv[2]);
        }
        else {
            controller = CreateControllerClient(argv[1]);
        }
        std::cout << "connected to controller v" << controller->GetVersion() << std::endl;

        std::string sceneuri = "mujin:/irex2013.mujin.dae";
        std::string scenepk = controller->GetScenePrimaryKeyFromURI_UTF8(sceneuri);
        SceneResourcePtr scene;
        scene.reset(new SceneResource(controller,scenepk));
        scene->Get("name");
        HandEyeCalibrationTaskResourcePtr calib;
        BinPickingTaskPtr binpicking = CreateBinPickingTask(MUJIN_BINPICKING_TASKTYPE_HTTP,"binpickingtask1","controller3",7123,controller,scene,"controller3",7100);
        calib.reset(new HandEyeCalibrationTaskResource(std::string("calibtask1"), controller,scene));
        BinPickingTask::ResultGetJointValues jointvaluesresult;
        binpicking->GetJointValues(jointvaluesresult);

        HandEyeCalibrationTaskParameters calibparam;
        calibparam.SetDefaults();
        calibparam.cameraname = "camera2";
        calibparam.numsamples = 15;
        calibparam.distances[0] = 0.6;
        calibparam.distances[1] = 0.9;
        calibparam.distances[2] = 0.1;
        calibparam.transform.translate[0] = -170;
        calibparam.transform.translate[1] = 0;
        calibparam.transform.translate[2] = 0;
        calibparam.transform.quaternion[0] = 0.70710678;
        calibparam.transform.quaternion[1] = 0;
        calibparam.transform.quaternion[2] = 0;
        calibparam.transform.quaternion[3] = -0.70710678;

        HandEyeCalibrationResultResource::CalibrationResult result;
        calib->ComputeCalibrationPoses(30, calibparam, result);
        std::cout << "poses: " << std::endl;
        for (int i = 0; i < result.poses.size(); i++) {
            for (int j = 0; j < 7; j++) {
                std::cout << result.poses[i][j] << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << "configs: " << std::endl;
        for (int i = 0; i < result.configs.size(); i++) {
            for (int j = 0; j < result.configs[i].size(); j++) {
                std::cout << result.configs[i][j] << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << "jointindices: " << std::endl;
        for (int i = 0; i < result.jointindices.size(); i++) {
            std::cout << result.jointindices[i] << ", ";
        }
        std::cout << std::endl;
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    DestroyControllerClient();
}
