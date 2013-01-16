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
        if( argc > 2 ) {
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
        std::map<std::string, Transform> transforms;
        if( !!result ) {
            cout << "result for task exists and can be completed in " << result->Get("task_time") << " seconds." << endl;
        }

        // get the first optimization
        std::vector<std::string> optimizationkeys;
        task->GetOptimizationPrimaryKeys(optimizationkeys);

        OptimizationResourcePtr optimization(new OptimizationResource(controller, optimizationkeys.at(0)));
        cout << "found optimization " << optimization->Get("name") << endl;

        std::vector<PlanningResultResourcePtr> results;
        optimization->GetResults(10,results);
        if( results.size() > 0 ) {
            cout << "the top results have times: ";
            for(size_t i = 0; i < results.size(); ++i) {
                cout << results[i]->Get("task_time") << ", ";
            }
            cout << endl;

            PlanningResultResourcePtr bestresult = results.at(0);
            bestresult->GetTransforms(transforms);
            cout << "robot position of best result is: ";
            for(std::map<std::string, Transform>::iterator it = transforms.begin(); it != transforms.end(); ++it) {
                Transform tfirst = it->second;
                // for now only output translation
                cout << it->first << "=(" << tfirst.translation[0] << ", " << tfirst.translation[1] << ", " << tfirst.translation[2] << "), ";
            }
            cout << endl;
            std::string robotprogram = bestresult->Get("robot_programs");
            if( robotprogram.size() > 0 ) {
                cout << endl << "robot program is: " << endl << robotprogram << endl;
            }
            else {
                // output the trajectory in xml format
                cout << endl << "trajectory is: " << endl << bestresult->Get("trajectory") << endl;
            }
        }
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    // destroy all mujin controller resources
    ControllerClientDestroy();
}
