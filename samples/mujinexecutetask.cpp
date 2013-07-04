// -*- coding: utf-8 -*-
/** \example mujinexecutetask.cpp

    Shows how to import a scene and execute a task on a specific scene and get the results. If the scene does not exist, will import it first.
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

        std::string sceneuri = "mujin:/densowave_wincaps_data/vs060a3_test0.mujin.dae";
        std::string scenepk = controller->GetScenePrimaryKeyFromURI_UTF8(sceneuri);
        SceneResourcePtr scene;
        try {
            scene.reset(new SceneResource(controller,scenepk));
            scene->Get("name");
        }
        catch(const MujinException& ex) {
            // failed to get name, so need to improt scene first
            scene = controller->ImportSceneToCOLLADA_UTF8("mujin:/densowave_wincaps_data/vs060a3_test0/test0.WPJ", "wincaps", sceneuri);
        }

        TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8("task0", "itlplanning");
        ITLPlanningTaskParameters info;
        info.optimizationvalue = 0.2; // set the optimization value [0,1]
        info.program = "settool(1)\n\
move(translation(0,0,20)*p[Work0/2])\n\
movel(p[Work0/2])\n\
movel(translation(0,0,20)*p[Work0/2])\n\
move(translation(0,0,20)*p[Work0/3])\n\
movel(p[Work0/3])\n\
movel(translation(0,0,20)*p[Work0/3])\n\
move(translation(0,0,20)*p[Work0/1])\n\
movel(p[Work0/1])\n\
movel(translation(0,0,20)*p[Work0/1])\n\
move(translation(0,0,20)*p[Work0/3])\n\
movel(p[Work0/3])\n\
";
        task->SetTaskParameters(info);

        // if necessary, can cancel all current jobs. this is not required
        controller->CancelAllJobs();

        task->Execute();

        std::cout << "waiting for task result" << std::endl;

        // query the results until they are complete, should take 100s
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
