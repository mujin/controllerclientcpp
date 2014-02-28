// -*- coding: utf-8 -*-
/** \example mujinideal_densowave.cpp

    Ideal usage of MUJIN Controller on DensoWave Wincaps RC8 files running on Windows.
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
        ControllerClientPtr controller, controller2;
        // licensekey "username:password"
        //controller = CreateControllerClient(licensekey);
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

        controller->SetDefaultSceneType("wincaps");
        controller->SetDefaultTaskType("itlplanning");
        controller->SetLanguage("ja");

        std::string sceneuri = "mujin:/vs060a3_test0/test0.WPJ";
        std::string scenepk = controller->GetScenePrimaryKeyFromURI_UTF8(sceneuri);

        // upload a Wincaps file
        controller->SyncUpload_UTF8("../share/mujincontrollerclient/densowave_wincaps_data/vs060a3_test0/test0.WPJ", "mujin:/vs060a3_test0/", "wincaps");

        // Register the scene, this also imports all PAC Script programs into "itlplanning" tasks
        SceneResourcePtr scene = controller->RegisterScene_UTF8(sceneuri, "wincaps");

        // test0.WPJ should have a grabtest.pcs PAC script registered
        // Query the task and execute it
        TaskResourcePtr task = scene->GetOrCreateTaskFromName_UTF8("grabtest", "itlplanning");

        // [Optional] get the task parameters and set optimiation value to 0.5
        ITLPlanningTaskParameters taskparameters;
        task->GetTaskParameters(taskparameters);
        taskparameters.optimizationvalue = 0.5;
        task->SetTaskParameters(taskparameters);

        // start task execution
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

        std::string errormessage = result->Get("errormessage");
        if( errormessage.size() > 0 ) {
            std::cout << "task failed with: " << errormessage << std::endl;
            return 2;
        }

        // task succeed, so get the results
        RobotControllerPrograms programs;
        result->GetPrograms(programs);
        std::cout << "found " << programs.programs.size() << " programs" << std::endl;
        for(std::map<std::string, RobotProgramData>::iterator it = programs.programs.begin(); it != programs.programs.end(); ++it ) {
            std::cout << "[" << it->first << "]" << std::endl << it->second.programdata << std::endl << std::endl;
        }
        std::string sOriginalTaskTime = result->Get("task_time");
        std::cout << "final task_time is " << sOriginalTaskTime << std::endl;

        OptimizationResourcePtr optimization = task->GetOrCreateOptimizationFromName_UTF8("myopt0","robotplacement");

        RobotPlacementOptimizationParameters optparams;
        optparams.targetname = ""; // no name means the robot
        optparams.framename = "0 robot"; // use the robot frame
        optparams.unit = "mm";
        optparams.minrange[0] = -100; // -X
        optparams.maxrange[0] = 100; // +X
        optparams.stepsize[0] = 50; // dX
        // don't move in y
        optparams.minrange[1] = 0; // -Y
        optparams.maxrange[1] = 0; // +Y
        optparams.stepsize[1] = 0; // dY
        // don't move in z
        optparams.minrange[2] = 0; // -Z
        optparams.maxrange[2] = 0; // +Z
        optparams.stepsize[2] = 0; // dZ
        // 4 angles, each 90 degrees apart
        optparams.minrange[3] = -180; // -angle
        optparams.maxrange[3] = 90; // +angle
        optparams.stepsize[3] = 90; // dangle
        optparams.ignorebasecollision = 0;
        optparams.topstorecandidates = 20; // store only the top 20 candidates
        optimization->SetOptimizationParameters(optparams);

        optimization->Execute();

        std::vector< PlanningResultResourcePtr > results;

        iterations = 0;
        while(1) {
            optimization->GetRunTimeStatus (status);

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

            optimization->GetResults(results, 0, 1);
            if( results.size() > 0 ) {
                std::cout << "top result task_time=" << results.at(0)->Get("task_time") << " seconds, original task = " << sOriginalTaskTime << " seconds" << std::endl;
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(5000)); // 5s

            ++iterations;
            if( iterations > maxiterations ) {
                controller->CancelAllJobs();
                throw MujinException("operation timed out, cancelling all jobs and quitting", MEC_Timeout);
            }
        }

        std::cout << "optimiation completed!" << std::endl;
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    DestroyControllerClient();
}
