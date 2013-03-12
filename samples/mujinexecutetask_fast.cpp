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

        SceneResourcePtr scene = controller->RegisterScene("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps");

        TaskResourcePtr task = scene->GetOrCreateTaskFromName("task0", "itlplanning");
        ITLPlanningTaskInfo info;
        info.program = "settool(1)\n\
move(translation(0,0,20)*p[Work0/2])\n\
movel(p[Work0/2])\n\
movel(translation(0,0,20)*p[Work0/2])\n\
move(translation(0,0,20)*p[Work0/3])\n\
movel(p[Work0/3])\n\
movel(translation(0,0,20)*p[Work0/3])\n\
";
        task->SetTaskInfo(info);

        // cancel all current jobs
        controller->CancelAllJobs();

        task->Execute();

        std::cout << "waiting for task result" << std::endl;

        // query the results until they are complete, should take several seconds
        PlanningResultResourcePtr result;
        JobStatus status;
        int iterations = 0, maxiterations = 4000;
        while (1) {
            result = task->GetResult();
            if( !!result ) {
                break;
            }
//        task->GetRunTimeStatus(status);
//        std::cout << "current job status is: " << status.code << std::cout;
//        if( status.code != JSC_Active || status.code != JSC_Pending || status.code != JSC_Succeeded ) {
//            std::cout << "unexpected job status so quitting" << std::cout;
//        }
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            ++iterations;
            if( iterations > maxiterations ) {
                controller->CancelAllJobs();
                throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
            }
        }

//        std::string robotprogram = result->Get("robot_programs");
//        if( robotprogram.size() > 0 ) {
//            std::cout << std::endl << "robot program is: " << std::endl << robotprogram << std::endl;
//        }
//        else {
//            // output the trajectory in xml format
//            std::cout << std::endl << "trajectory is: " << std::endl << result->Get("trajectory") << std::endl;
//        }

        std::cout << "final task_time is " << result->Get("task_time") << std::endl;
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
