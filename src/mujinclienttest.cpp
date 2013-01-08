// -*- coding: utf-8 -*-
// Copyright (C) 2012 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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

using namespace std;
using namespace mujinclient;

int main(int argc, char ** argv)
{
    if( argc < 2 ) {
        printf("need username:password. Example: mujinclienttest myuser:mypass\n");
    }
    ControllerClientPtr controller = CreateControllerClient(argv[1]);

    std::vector<std::string> scenekeys;
    controller->GetScenePrimaryKeys(scenekeys);

    cout << "user has " << scenekeys.size() << " environments: " << endl;
    for(size_t i = 0; i < scenekeys.size(); ++i) {
        cout << scenekeys[i] << endl;
    }

    SceneResourcePtr scene(new SceneResource(controller, "YG_LAYOUT"));
    TaskResourcePtr task = scene->GetOrCreateTaskFromName("task0");
    cout << "got task " << task->Get("name") << endl;
    cout << "program is " << task->Get("taskgoalxml") << endl;
    //task->Execute();

    PlanningResultResourcePtr result = task->GetTaskResult();
    if( !!result ) {
        cout << "result exists and can be completed in " << result->Get("task_time") << " seconds." << endl;
    }
    ControllerClientDestroy();
}
