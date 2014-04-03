// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>
#include <mujincontrollerclient/binpickingtask.h>
#include <mujincontrollerclient/handeyecalibrationtask.h>

#include <boost/thread/thread.hpp> // for sleep
#include <Eigen/Dense>
#include <Eigen/Geometry>

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
        SceneResource::InstObjectPtr camera, container;
        RobotResourcePtr camerarobot;
        if (!(scene->FindInstObject("camera5",camera))) {
            std::cerr << "camera is not in the scene" << std::endl;
            exit(1);
        }

        if (!(scene->FindInstObject("container1",container))) {
            std::cerr << "container is not in the scene" << std::endl;
            exit(1);
        }
        camerarobot.reset(new RobotResource(controller, camera->object_pk));
        std::vector<RobotResource::AttachedSensorResourcePtr> attachedsensors;
        camerarobot->GetAttachedSensors(attachedsensors);
        if (attachedsensors.size() == 0) {
            std::cerr << "no sensor attached!" << std::endl;
            exit(1);
        }

        /*{{{*/
        /*
        std::cout << attachedsensors[0]->name << std::endl;
        std::cout << attachedsensors[0]->frame_origin << std::endl;
        std::cout << attachedsensors[0]->pk << std::endl;

        std::cout << attachedsensors[0]->quaternion[0]<<" ";
        std::cout << attachedsensors[0]->quaternion[1]<<" ";
        std::cout << attachedsensors[0]->quaternion[2]<<" ";
        std::cout << attachedsensors[0]->quaternion[3]<<" ";
        std::cout << std::endl;
        std::cout << attachedsensors[0]->translate[0]<< " ";
        std::cout << attachedsensors[0]->translate[1]<< " ";
        std::cout << attachedsensors[0]->translate[2]<< " ";
        std::cout << std::endl;
        std::cout << attachedsensors[0]->sensortype << std::endl;
        std::cout << attachedsensors[0]->sensordata.focal_length << std::endl;
        */

        std::cout << "intrinsic: " << std::endl;
        for (auto i : attachedsensors[0]->sensordata.intrinsic) {
            std::cout << i << ", ";
        }
        std::cout  << std::endl;
        /*
        for (auto i : attachedsensors[0]->sensordata.distortion_coeffs) {
            std::cout << i << std::endl;
        }
        *//*}}}*/

        Eigen::Matrix4d Tcamtoworld, Tcontainertoworld, Tworldtocam;
        Eigen::Matrix4d Tcontainerpointstoworld[4];
        Eigen::Matrix4d Tcontainerpointstocam[4];
        double global3droi[6]= { 0.0, 0.33, 0.0, 0.69, 0.0, 0.21};
        for (auto& var : global3droi) {
            var *= 1000.0;
        }

        Tcamtoworld       = Eigen::Matrix4d::Identity();
        Tcontainertoworld = Eigen::Matrix4d::Identity();
        
        Tcamtoworld.block(0,0,3,3) = Eigen::Quaternion<double>(camera->quaternion[0], camera->quaternion[1], camera->quaternion[2], camera->quaternion[3]).toRotationMatrix();
        Tcamtoworld(0,3) = camera->translate[0];
        Tcamtoworld(1,3) = camera->translate[1];
        Tcamtoworld(2,3) = camera->translate[2];

        Tworldtocam = Tcamtoworld.inverse();
        Tcontainertoworld.block(0,0,3,3) = Eigen::Quaternion<double>(container->quaternion[0], container->quaternion[1], container->quaternion[2], container->quaternion[3]).toRotationMatrix();
        Tcontainertoworld(0,3) = container->translate[0];
        Tcontainertoworld(1,3) = container->translate[1];
        Tcontainertoworld(2,3) = container->translate[2];

        for (auto& Tcontainerpointtoworld : Tcontainerpointstoworld) {
            Tcontainerpointtoworld = Tcontainertoworld;
        }
        Tcontainerpointstoworld[0](0,3) += global3droi[0];
        Tcontainerpointstoworld[0](1,3) += global3droi[2];

        Tcontainerpointstoworld[1](0,3) += global3droi[1];
        Tcontainerpointstoworld[1](1,3) += global3droi[2];

        Tcontainerpointstoworld[2](0,3) += global3droi[0];
        Tcontainerpointstoworld[2](1,3) += global3droi[3];

        Tcontainerpointstoworld[3](0,3) += global3droi[1];
        Tcontainerpointstoworld[3](1,3) += global3droi[3];
        for (auto& Tcontainerpointtoworld : Tcontainerpointstoworld) {
            Tcontainerpointtoworld(2,3) += global3droi[5];
            //std::cout << "=====" << std::endl;
            //std::cout << Tcontainerpointtoworld<< std::endl;
        }


        for (size_t i = 0; i < 4; i++) {
            Tcontainerpointstocam[i] = Tworldtocam * Tcontainerpointstoworld[i];
            //std::cout << "=====" << std::endl;
            //std::cout << Tcontainerpointstocam[i]<< std::endl;
        }
        Real* intrinsic =  attachedsensors[0]->sensordata.intrinsic;
        std::vector<double> pxlist, pylist;
        for (auto& Tpointtocam : Tcontainerpointstocam) {
            auto x = Tpointtocam(0,3) / Tpointtocam(2,3);
            auto y = Tpointtocam(1,3) / Tpointtocam(2,3);
            auto px = intrinsic[0] * x  + intrinsic[1] * y + intrinsic[2];
            auto py = intrinsic[3] * x  + intrinsic[4] * y + intrinsic[5];
            //std::cout << "=============" << std::endl;
            //std::cout << "px: " << px << std::endl;
            //std::cout << "py: " << py << std::endl;
            pxlist.push_back(px);
            pylist.push_back(py);
        }
        double image_roi[4];
        image_roi[0] = *std::min_element(pxlist.begin(),pxlist.end());
        image_roi[1] = *std::max_element(pxlist.begin(),pxlist.end());
        image_roi[2] = *std::min_element(pylist.begin(),pylist.end());
        image_roi[3] = *std::max_element(pylist.begin(),pylist.end());
        std::cout << "image_roi:" << std::endl;
        for (auto& roielem : image_roi) {
            std::cout << roielem << ", ";
        }
        std::cout << std::endl;


        //BinPickingTaskPtr binpicking = CreateBinPickingTask(MUJIN_BINPICKING_TASKTYPE_HTTP,"binpickingtask1","192.168.13.201",5007,controller,scene,"controller16",7100);
        //BinPickingTask::ResultGetJointValues jointvaluesresult;
        //binpicking->GetJointValues(jointvaluesresult);

    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    DestroyControllerClient();
}
