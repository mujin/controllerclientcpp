// -*- coding: utf-8 -*-
/** \example mujinshowresults.cpp

    Shows how to get the result data data from an already generated task and optimization results.
 */
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <iostream>
#include <vector>

using namespace std;
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

        // get all supported keys
        std::vector<std::string> scenekeys;
        controller->GetScenePrimaryKeys(scenekeys);

        cout << "user has " << scenekeys.size() << " scenes: " << endl;
        for(size_t i = 0; i < scenekeys.size(); ++i) {
            cout << scenekeys[i] << endl;
        }

        SceneResourcePtr scene;
        // if YG_LAYOUT exists, open it, otherwise open the first file
        if( find(scenekeys.begin(),scenekeys.end(), string("YG_LAYOUT")) != scenekeys.end() ) {
            scene.reset(new SceneResource(controller, "YG_LAYOUT"));
        }
        else {
            cout << "opening scene " << scenekeys.at(0) << endl;
            scene.reset(new SceneResource(controller, scenekeys.at(0)));
        }

        // open the first task
        std::vector<std::string> taskkeys;
        scene->GetTaskPrimaryKeys(taskkeys);
        if( taskkeys.size() == 0 ) {
            std::cout << "no tasks for this scene" << std::endl;
            return 0;
        }
        TaskResourcePtr task(new TaskResource(controller, taskkeys.at(0)));

        cout << "got task " << task->Get("name") << endl;
        cout << "program is " << task->Get("taskgoalxml") << endl;

        PlanningResultResourcePtr result = task->GetResult();
        EnvironmentState envstate;
        if( !!result ) {
            cout << "result for task exists and can be completed in " << result->Get("task_time") << " seconds." << endl;
        }

        // get the first optimization
        std::vector<std::string> optimizationkeys;
        task->GetOptimizationPrimaryKeys(optimizationkeys);

        OptimizationResourcePtr optimization(new OptimizationResource(controller, optimizationkeys.at(0)));
        cout << "found optimization " << optimization->Get("name") << endl;

        std::vector<PlanningResultResourcePtr> results;
        optimization->GetResults(results,0,10);
        if( results.size() > 0 ) {
            cout << "the top results have times: ";
            for(size_t i = 0; i < results.size(); ++i) {
                cout << results[i]->Get("task_time") << ", ";
            }
            cout << endl;

            PlanningResultResourcePtr bestresult = results.at(0);
            bestresult->GetEnvironmentState(envstate);
            cout << "robot position of best result is: ";
            for(EnvironmentState::iterator it = envstate.begin(); it != envstate.end(); ++it) {
                InstanceObjectState objstate = it->second;
                // for now only output translate
                cout << it->first << "=(" << objstate.transform.translate[0] << ", " << objstate.transform.translate[1] << ", " << objstate.transform.translate[2] << "), ";
            }
            cout << endl;

            RobotControllerPrograms programs;
            result->GetPrograms(programs);
            std::cout << "found " << programs.programs.size() << " programs" << std::endl;
            for(std::map<std::string, RobotProgramData>::iterator it = programs.programs.begin(); it != programs.programs.end(); ++it ) {
                std::cout << "[" << it->first << "]" << std::endl << it->second.programdata << std::endl << std::endl;
            }
            std::cout << "final task_time is " << result->Get("task_time") << std::endl;
        }
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    // destroy all mujin controller resources
    DestroyControllerClient();
}
