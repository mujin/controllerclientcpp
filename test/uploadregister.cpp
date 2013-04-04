// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <iostream>

using namespace mujinclient;

int main(int argc, char ** argv)
{
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

        controller->SyncUpload("../densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "mujin:/testupload/", "wincaps");
        //SceneResourcePtr scene = controller->RegisterScene("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps");
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
