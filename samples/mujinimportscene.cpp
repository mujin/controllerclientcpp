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
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <iostream>

using namespace mujinclient;

int main(int argc, char ** argv)
{
    if( argc < 2 ) {
        printf("need username:password. Example: mujinclienttest myuser:mypass [url]\n\nurl - [optional] For example https://controller.mujin.co.jp/\n");
        return 1;
    }
    ControllerClientPtr controller;
    if( argc > 2 ) {
        controller = CreateControllerClient(argv[1], argv[2]);
    }
    else {
        controller = CreateControllerClient(argv[1]);
    }

    // try to import the scene, if it already exists delete it and import again
    try {
        controller->ImportScene("mujin:/EMU_MUJIN/EMU_MUJIN.WPJ", "wincaps", "mujin:/test.mujin.dae");
    }
    catch(const mujin_exception& ex) {
        if( ex.message().find("need to remove it first") != std::string::npos ) {
            std::cout << "file already imported, would you like to delete and re-import? (yes/no) ";
            std::string answer;
            std::cin >> answer;
            if( answer == std::string("yes") ) {
                std::cout << "try removing the file and importing again" << std::endl;
                SceneResource oldscene(controller,"test");
                oldscene.Delete();
                controller->ImportScene("mujin:/EMU_MUJIN/EMU_MUJIN.WPJ", "wincaps", "mujin:/test.mujin.dae");
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
    // destroy all mujin controller resources
    ControllerClientDestroy();
}
