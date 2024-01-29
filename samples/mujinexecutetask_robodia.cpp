// -*- coding: utf-8 -*-
/** \example mujinexecutetask_robodia.cpp

    Shows how to import a scene and query a list of the instance objects inside the scene. Note that querying only works for MUJIN COLLADA scenes.
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

        controller->SyncUpload_UTF8("../share/mujincontrollerclient/robodia_demo1/robodia_demo1.xml", "mujin:/robodia_demo1/", "cecrobodiaxml");

        std::string sceneuri = "mujin:/robodia_demo1/robodia_demo1.xml";
        SceneResourcePtr scene = controller->RegisterScene_UTF8(sceneuri, "cecrobodiaxml");

        std::vector<SceneResource::InstObjectPtr> instobjects;
        scene->GetInstObjects(instobjects);
        std::cout << "scene instance objects: ";
        for(size_t i = 0; i < instobjects.size(); ++i) {
            std::cout << instobjects[i]->name << ", ";
        }
        std::cout << std::endl;

        // execute a task

        TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8("task0", "itlplanning");
        ITLPlanningTaskParameters info;
        info.optimizationvalue = 0.2; // set the optimization value [0,1]
        info.vrcruns = 0; // turn off VRC since this environment does not have VRC files
        info.program = "settool(Tcp_HAND2)\n\
move(p[3_home])\n\
move(p[3_home_R])\n\
move(p[3_s1_R])\n\
movel(p[3_s2_R])\n\
movel(p[3_s1_R])\n\
move(p[3_home_R])\n\
move(p[3_home_L])\n\
move(p[3_e1_L])\n\
movel(p[3_e2_L])\n\
movel(p[3_e1_L])\n\
move(p[3_home_L])\n\
move(p[3_home])\n\
";
        task->SetTaskParameters(info);

        // if necessary, can cancel all current jobs. this is not required
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
            task->GetRunTimeStatus(status);
            std::cout << "current job status=" << status.code << ": " << status.message << std::endl;
            if( status.code == JSC_Succeeded ) {
                break;
            }
            else if( status.code == JSC_Unknown ) {
                // wait 30s, then fail
                if( iterations > 3 ) {
                    std::cout << "task won't start for some reason" << std::endl;
                    return 1;
                }
            }
            else if( status.code == JSC_Aborted || status.code == JSC_Preempted ) {
                std::cout << "task failed execution " << std::endl;
                return 1;
            }
            else if( status.code != JSC_Active && status.code != JSC_Pending && status.code != JSC_Succeeded ) {
                std::cout << "unexpected job status so quitting " << std::endl;
                return 1;
            }

            boost::this_thread::sleep(boost::posix_time::milliseconds(5000)); // 5s
            ++iterations;
            if( iterations > maxiterations ) {
                controller->CancelAllJobs();
                throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
            }
        }

        std::string cecrobodiazipdata;
        result->GetAllRawProgramData(cecrobodiazipdata, "cecrobodiasim");
        std::cout << "got robodia simulation file, size=" << cecrobodiazipdata.size() << " bytes" << std::endl;
        // get any other programs
        RobotControllerPrograms programs;
        result->GetPrograms(programs);
        std::cout << "found " << programs.programs.size() << " programs" << std::endl;
        for(std::map<std::string, RobotProgramData>::iterator it = programs.programs.begin(); it != programs.programs.end(); ++it ) {
            std::cout << "[" << it->first << "]" << std::endl << it->second.programdata << std::endl << std::endl;
        }
        std::cout << "final task_time is " << result->Get<std::string>("task_time") << std::endl;
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    // destroy all mujin controller resources
    DestroyControllerClient();
}
