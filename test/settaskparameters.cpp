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
        TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8("task0", "itlplanning");
        ITLPlanningTaskParameters info;
        info.program = "settool(1)\n\
move(translation(0,0,20)*p[Work0/2])\n\
movel(p[Work0/2])\n\
movel(translation(0,0,20)*p[Work0/2])\n\
move(translation(0,0,20)*p[Work0/3])\n\
movel(p[Work0/3])\n\
movel(translation(0,0,20)*p[Work0/3])\n\
";
        task->SetTaskParameters(info);

        ITLPlanningTaskParameters newinfo;
        task->GetTaskParameters(newinfo);

        // compare the infos
        BOOST_ASSERT(info.startfromcurrent == newinfo.startfromcurrent);
        BOOST_ASSERT(info.returntostart == newinfo.returntostart);
        BOOST_ASSERT(info.vrcruns == newinfo.vrcruns);
        BOOST_ASSERT(info.ignorefigure == newinfo.ignorefigure);
        BOOST_ASSERT(info.unit == newinfo.unit);
        BOOST_ASSERT(info.optimizationvalue == newinfo.optimizationvalue);
        BOOST_ASSERT(info.program == newinfo.program);
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
