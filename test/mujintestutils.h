// -*- coding: utf-8 -*-
#ifndef MUJINCONTROLLER_TESTUTILS_H
#define MUJINCONTROLLER_TESTUTILS_H

#include <mujincontrollerclient/mujincontrollerclient.h>
#include <iostream>

mujinclient::ControllerClientPtr CreateControllerFromCommandLine(int argc, char ** argv)
{
    mujinclient::ControllerClientPtr controller;
    if( argc >= 5 ) {
        controller = mujinclient::CreateControllerClient(argv[1], argv[2], argv[3], argv[4]);
    }
    if( argc == 4 ) {
        controller = mujinclient::CreateControllerClient(argv[1], argv[2], argv[3]);
    }
    else if( argc == 3 ) {
        controller = mujinclient::CreateControllerClient(argv[1], argv[2]);
    }
    else {
        controller = mujinclient::CreateControllerClient(argv[1]);
    }
    std::cout << "connected to controller v" << controller->GetVersion() << std::endl;
    return controller;
}

#endif
