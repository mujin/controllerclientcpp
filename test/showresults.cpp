// -*- coding: utf-8 -*-
/** \example mujinexecutetask.cpp

    Shows how to quickly register a scene and execute a task and get the results. Because the scene is directly used instead of imported.
 */
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/thread/thread.hpp> // for sleep

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

        SceneResourcePtr scene = controller->RegisterScene("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps");
        TaskResourcePtr task = scene->GetTaskFromName_UTF8("task0");
        PlanningResultResourcePtr result = task->GetResult();
        RobotControllerPrograms programs;
        result->GetPrograms(programs);
        std::cout << "found " << programs.programs.size() << " programs" << std::endl;
        for(std::map<std::string, RobotProgramData>::iterator it = programs.programs.begin(); it != programs.programs.end(); ++it ) {
            std::cout << "[" << it->first << "]" << std::endl << it->second.programdata << std::endl << std::endl;
        }
        std::cout << "final task_time is " << result->Get("task_time") << std::endl;

    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
