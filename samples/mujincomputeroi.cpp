// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>
#include <mujincontrollerclient/binpickingtask.h>
#include <mujincontrollerclient/handeyecalibrationtask.h>
#include <geometry.h>

#include <boost/thread/thread.hpp> // for sleep

#include <iostream>

using namespace mujinclient;

int main(int argc, char ** argv)
{
    if( argc < 3 ) {
        std::cout << "need cameraname username:password. Example: mujinclienttest camera3 myuser:mypass [url]\n\nurl - [optional] For example https://controller.mujin.co.jp/" << std::endl;
        return 1;
    }
    try {
        ControllerClientPtr controller;
        const std::string cameraname(argv[1]);
        if( argc >= 6 ) {
            controller = CreateControllerClient(argv[2], argv[3], argv[4], argv[5]);
        }
        if( argc == 5 ) {
            controller = CreateControllerClient(argv[2], argv[3], argv[4]);
        }
        else if( argc == 4 ) {
            controller = CreateControllerClient(argv[2], argv[3]);
        }
        else {
            controller = CreateControllerClient(argv[2]);
        }
        std::cout << "connected to controller v" << controller->GetVersion() << std::endl;

        std::string sceneuri = "mujin:/irex2013.mujin.dae";
        std::string scenepk = controller->GetScenePrimaryKeyFromURI_UTF8(sceneuri);
        SceneResourcePtr scene;
        scene.reset(new SceneResource(controller,scenepk));
        scene->Get("name");
        SceneResource::InstObjectPtr camera, container;
        RobotResourcePtr camerarobot;
        if (!(scene->FindInstObject(cameraname,camera))) {
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

        geometry::TransformMatrix<double> Tcamtoworld, Tcontainertoworld, Tworldtocam;
        geometry::TransformMatrix<double> Tcontainerpointstoworld[4];
        geometry::TransformMatrix<double> Tcontainerpointstocam[4];
        double global3droi[6]= { 0.0, 0.33, 0.0, 0.69, 0.0, 0.21};
        for (auto& var : global3droi) {
            var *= 1000.0;
        }

        Tcamtoworld = geometry::matrixFromQuat(geometry::Vector<double>(camera->quaternion[0], camera->quaternion[1], camera->quaternion[2], camera->quaternion[3]));
        Tcamtoworld.trans[0] = camera->translate[0];
        Tcamtoworld.trans[1] = camera->translate[1];
        Tcamtoworld.trans[2] = camera->translate[2];
        Tworldtocam = Tcamtoworld.inverse();

        Tcontainertoworld = geometry::matrixFromQuat(geometry::Vector<double>(container->quaternion[0], container->quaternion[1], container->quaternion[2], container->quaternion[3]));
        Tcontainertoworld.trans[0] = container->translate[0];
        Tcontainertoworld.trans[1] = container->translate[1];
        Tcontainertoworld.trans[2] = container->translate[2];

        for (auto& Tcontainerpointtoworld : Tcontainerpointstoworld) {
            Tcontainerpointtoworld = Tcontainertoworld;
        }
        Tcontainerpointstoworld[0].trans[0] += global3droi[0];
        Tcontainerpointstoworld[0].trans[1] += global3droi[2];

        Tcontainerpointstoworld[1].trans[0] += global3droi[1];
        Tcontainerpointstoworld[1].trans[1] += global3droi[2];

        Tcontainerpointstoworld[2].trans[0] += global3droi[0];
        Tcontainerpointstoworld[2].trans[1] += global3droi[3];

        Tcontainerpointstoworld[3].trans[0] += global3droi[1];
        Tcontainerpointstoworld[3].trans[1] += global3droi[3];
        for (auto& Tcontainerpointtoworld : Tcontainerpointstoworld) {
            Tcontainerpointtoworld.trans[2] += global3droi[5];
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
            auto x = Tpointtocam.trans[0] / Tpointtocam.trans[2];
            auto y = Tpointtocam.trans[1] / Tpointtocam.trans[2];
            auto px = intrinsic[0] * x  + intrinsic[1] * y + intrinsic[2];
            auto py = intrinsic[3] * x  + intrinsic[4] * y + intrinsic[5];
            std::cout << "=============" << std::endl;
            std::cout << "px: " << px << std::endl;
            std::cout << "py: " << py << std::endl;
            pxlist.push_back(px);
            pylist.push_back(py);
        }

        // need to make sure roi is within image boundary
        const int image_width  = attachedsensors[0]->sensordata.image_dimensions[0];
        const int image_height  = attachedsensors[0]->sensordata.image_dimensions[1];
        double image_roi[4];
        image_roi[0] = std::max(int(*std::min_element(pxlist.begin(),pxlist.end())), 0);
        image_roi[1] = std::min(int(*std::max_element(pxlist.begin(),pxlist.end())), image_width);
        image_roi[2] = std::max(int(*std::min_element(pylist.begin(),pylist.end())), 0);
        image_roi[3] = std::min(int(*std::max_element(pylist.begin(),pylist.end())), image_height);
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
