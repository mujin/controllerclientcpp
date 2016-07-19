// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>
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
    SceneResourcePtr scene, newscene;
    std::string robotname = "VP-5243I";
    scene.reset(new SceneResource(controller,scenepk));
    std::vector<SceneResource::InstObjectPtr> instobjects;

    std::cout << "scenename: " << scene->Get("name") << std::endl;

    //newscene = scene->Copy("irex2013_copied");
    //std::cout << "newname: " << newscene->Get("name") << std::endl;

    scene->GetInstObjects(instobjects);
    int robotinstindex = -1;
    for(size_t i = 0; i < instobjects.size(); ++i) {
        //std::cout << "==============" << std::endl;
        //instobjects[i]->Print();
        if (instobjects[i]->name == robotname)
        {
            robotinstindex = i;
        }
    }
    if (robotinstindex == -1) {
        std::cout << "no such kind of robot: " << robotname << std::endl;
        exit(1);
    }
    std::cout << "links: " << std::endl;
    for (size_t i = 0; i < instobjects[robotinstindex]->links.size(); i++) {
        std::cout << "\tname: " << instobjects[robotinstindex]->links[i].name << std::endl;
        std::cout << "\tquaternion: ";
        for (int j = 0; j < 4; j++) {
            std::cout << instobjects[robotinstindex]->links[i].quaternion[j] << ", ";
        }
        std::cout << std::endl;
        std::cout << "\ttranslate: ";
        for (int j = 0; j < 3; j++) {
            std::cout << instobjects[robotinstindex]->links[i].translate[j] << ", ";
        }
        std::cout << std::endl;
    }

    std::cout << "instobjects[robotinstindex]->tools: " << std::endl;
    for (size_t i = 0; i < instobjects[robotinstindex]->tools.size(); i++) {
        std::cout << "tool" << i << std::endl;
        std::cout << "\tname: " << instobjects[robotinstindex]->tools[i].name << std::endl;

        std::cout << "\tquaternion: ";
        for (int j = 0; j < 4; j++) {
            std::cout << instobjects[robotinstindex]->tools[i].quaternion[j] << ", ";
        }
        std::cout << std::endl;

        std::cout << "\ttranslate: ";
        for (int j = 0; j < 3; j++) {
            std::cout << instobjects[robotinstindex]->tools[i].translate[j] << ", ";
        }
        std::cout << std::endl;

        std::cout << "\tdirection: ";
        for (int j = 0; j < 3; j++) {
            std::cout << instobjects[robotinstindex]->tools[i].direction[j] << ", ";
        }
        std::cout << std::endl;
    }

    /*
       ObjectResourcePtr object;
       RobotResourcePtr robot;
       robot.reset(new RobotResource(controller, instobjects[robotinstindex]->object_pk));
       object.reset(new ObjectResource(controller, instobjects[objectinstndex]->object_pk));

       std::vector<RobotResource::ToolResourcePtr> tools;
       std::vector<ObjectResource::LinkResourcePtr> robotlinks, objectlinks;
       robot->GetTools(tools);
       size_t flangeindex;
       for(size_t i = 0; i < tools.size(); ++i) {
        if(tools[i]->name == "FlangeBase"){
            flangeindex = i;
        }
        //std::cout << "------" << std::endl;
        //std::cout << tools[i]->name << std::endl;
        //std::cout << tools[i]->frame_origin<< std::endl;
        //std::cout << tools[i]->frame_tip<< std::endl;
        //std::cout << tools[i]->pk<< std::endl;
       }
       robot->GetLinks(robotlinks);
       object->GetLinks(objectlinks);

       for(size_t i = 0; i < robotlinks.size(); ++i) {
        //std::cout << "------" << std::endl;
        //std::cout << robotlinks[i]->name << std::endl;
        //std::cout << robotlinks[i]->pk<< std::endl;
        //for (size_t j = 0; j < robotlinks[i]->attachmentpks.size(); j++) {
        //    std::cout << robotlinks[i]->attachmentpks[j] << std::endl;
        //}
       }

       std::cout << "original grabs: " << instobjects[robotinstindex]->grabs.size() << std::endl;
       for(size_t i = 0; i < instobjects[robotinstindex]->grabs.size(); ++i) {
        std::cout << instobjects[robotinstindex]->grabs[i].Serialize() << std::endl;
       }

       instobjects[robotinstindex]->GrabObject(instobjects[objectinstndex], objectlinks[0]->pk, robotlinks[0]->pk);
       newscene->GetInstObjects(instobjects);
       std::cout << "new grabs: " << instobjects[robotinstindex]->grabs.size() << std::endl;
       for(size_t i = 0; i < instobjects[robotinstindex]->grabs.size(); ++i) {
        std::cout << instobjects[robotinstindex]->grabs[i].Serialize() << std::endl;
       }
     */
    ///newscene->Delete();
}
