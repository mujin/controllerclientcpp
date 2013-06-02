// -*- coding: utf-8 -*-
/** \example mujinimportscene.cpp

    Shows how to import a scene and query a list of the instance objects inside the scene. Note that querying only works for MUJIN COLLADA scenes.
 */
#include <mujincontrollerclient/mujincontrollerclient.h>

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

        // try to import the scene, if it already exists delete it and import again
        try {
            controller->ImportSceneToCOLLADA_UTF8("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps", "mujin:/test.mujin.dae");
        }
        catch(const MujinException& ex) {
            if( ex.message().find("need to remove it first") != std::string::npos ) {
                std::cout << "file already imported, would you like to delete and re-import? (yes/no) ";
                std::string answer;
                std::cin >> answer;
                if( answer == std::string("yes") ) {
                    std::cout << "try removing the file and importing again" << std::endl;
                    SceneResource oldscene(controller,"test.mujin.dae");
                    oldscene.Delete();
                    controller->ImportSceneToCOLLADA_UTF8("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps", "mujin:/test.mujin.dae");
                }
            }
            else {
                std::cout << "error importing scene" << ex.message() << std::endl;
            }
        }

        // create the resource and query info
        SceneResource scene(controller,"test");
        std::vector<SceneResource::InstObjectPtr> instobjects;
        scene.GetInstObjects(instobjects);
        std::cout << "scene instance objects: ";
        for(size_t i = 0; i < instobjects.size(); ++i) {
            std::cout << instobjects[i]->name << ", ";
        }
        std::cout << std::endl;
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    // destroy all mujin controller resources
    ControllerClientDestroy();
}
