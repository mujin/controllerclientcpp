// -*- coding: utf-8 -*-
/** \example mujinimportscene_robodia.cpp

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

        //controller->SyncUpload_UTF16(L"c:\\controllerclientcpp\\シームレステスト\\シームレステスト.xml", L"mujin:/シームレステスト/", "cecrobodiaxml");
        controller->SyncUpload_UTF8("../share/mujincontrollerclient/robodia_demo1/robodia_demo1.xml", "mujin:/robodia_demo1/", "cecrobodiaxml");

        // try to import the scene, if it already exists delete it and import again
        std::string sceneuri = "mujin:/robodia_demo1.mujin.dae";
        try {
            controller->ImportSceneToCOLLADA_UTF8("mujin:/robodia_demo1/robodia_demo1.xml", "cecrobodiaxml", sceneuri);
        }
        catch(const MujinException& ex) {
            if( ex.message().find("need to remove it first") != std::string::npos ) {
                std::cout << "file already imported, would you like to delete and re-import? (yes/no) ";
                std::string answer;
                std::cin >> answer;
                if( answer == std::string("yes") ) {
                    std::cout << "try removing the file and importing again" << std::endl;
                    SceneResource oldscene(controller,"robodia_demo1.mujin.dae");
                    oldscene.Delete();
                    controller->ImportSceneToCOLLADA_UTF8("mujin:/robodia_demo1/robodia_demo1.xml", "cecrobodiaxml", sceneuri);
                }
            }
            else {
                std::cout << "error importing scene" << ex.message() << std::endl;
            }
        }

        // create the resource and query info
        // have to convert the URI into a primary-key for creating the resources
        std::string scenepk = controller->GetScenePrimaryKeyFromURI_UTF8(sceneuri);
        SceneResource scene(controller,scenepk);
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
    DestroyControllerClient();
}
