// -*- coding: utf-8 -*-
/** \example mujinmovetool.cpp

    Shows how to move tool to a specified position and orientation in two ways.
    example: ./mujinmovetool --controller_hostname=controllerXX --controller_url=http://controllerXX:80 --controller_username_password=youruser:yourpassword --task_scenepk=yourfile.mujin.dae --robotname=yourrobot --goals=100 200 300 0 90 180 --goaltype=transform6d --movelinear=true --speed=0.1

    tips:
     1. if solution is not found, try to set goal close to current tool position
     2. if solution is not found, try to set --movelinear==false

 */

#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>

using namespace mujinclient;

#include <boost/program_options.hpp>

namespace bpo = boost::program_options;
using namespace std;

/// \brief parse command line options and store in a map
/// \param argc number of arguments
/// \param argv arguments
/// \param opts map where parsed options are stored
/// \return true if non-help options are parsed succesfully.
bool ParseOptions(int argc, char ** argv, bpo::variables_map& opts)
{
    // parse command line arguments
    bpo::options_description desc("Options");

    desc.add_options()
        ("help,h", "produce help message")
        ("controller_url", bpo::value<string>()->required(), "ip of the mujin controller, e.g. http://controller20:80")
        ("controller_hostname", bpo::value<string>()->required(), "hostname of the mujin controller, e.g. controller")
        ("run_with_controllerui", bpo::value<bool>()->default_value(true), "true if connecting to controller where controller ui is running ")
        ("controller_username_password", bpo::value<string>()->required(), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<string>()->default_value("en_US"), "locale to use for the mujin controller client")
        ("task_scenepk", bpo::value<string>()->required(), "scene pk of the binpicking task on the mujin controller, e.g. officeboltpicking.mujin.dae")
        ("robotname", bpo::value<string>()->required(), "robot name, e.g. VP-5243I")
        ("toolname", bpo::value<string>()->default_value(""), "tool name, e.g. flange")
        ("taskparameters", bpo::value<string>()->default_value("{}"), "binpicking task parameters, e.g. {'robotname': 'robot', 'robots':{'robot': {'externalCollisionIO':{}, 'gripperControlInfo':{}, 'robotControllerUri': '', robotDeviceIOUri': '', 'toolname': 'tool'}}}")
        ("zmq_port", bpo::value<unsigned int>()->default_value(11000), "port of the binpicking task on the mujin controller")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")
        ("goaltype", bpo::value<string>()->default_value("transform6d"), "mode to move tool with. Either transform6d or translationdirection5d")
        ("goals", bpo::value<vector<double> >()->multitoken(), "goal to move tool to, \'X Y Z RX RY RZ\'. Units are in mm and deg.")
        ("speed", bpo::value<double>()->default_value(0.1), "speed to move at")
        ("movelinear", bpo::value<bool>()->default_value(false), "whether to move tool linearly")
        ;

    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc, bpo::command_line_style::unix_style ^ bpo::command_line_style::allow_short), opts);
    }
    catch (const exception& ex) {
        stringstream errss;
        errss << "Caught exception " << ex.what();
        cerr << errss.str() << endl;
        return false;
    }

    bool badargs = false;
    try {
        bpo::notify(opts);
    }
    catch(const exception& ex) {
        stringstream errss;
        errss << "Caught exception " << ex.what();
        cerr << errss.str() << endl;
        badargs = true;
    }

    if(opts.count("help") || badargs) {
        cout << "Usage: " << argv[0] << " [OPTS]" << endl;
        cout << endl;
        cout << desc << endl;
        return false;
    }
    return true;
}

/// \brief initialize BinPickingTask and establish communication with server
/// \param opts options parsed from command line
/// \param pBinPickingTask bin picking task to be initialized
void InitializeTask(const bpo::variables_map& opts,
                    BinPickingTaskResourcePtr& pBinpickingTask)
{
    const string controllerUrl = opts["controller_url"].as<string>();
    const string controllerHostname = opts["controller_hostname"].as<string>();
    const bool runwithcontrollerui = opts["run_with_controllerui"].as<bool>();
    const string slavename = controllerHostname + (runwithcontrollerui ? "_slave0" : "_slave1");
    const string controllerUsernamePass = opts["controller_username_password"].as<string>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    const string taskScenePk = opts["task_scenepk"].as<string>();
    const string taskparameters = opts["taskparameters"].as<string>();
    const string locale = opts["locale"].as<string>();
    const unsigned int taskHeartbeatPort = opts["heartbeat_port"].as<unsigned int>();
    const unsigned int taskZmqPort = opts["zmq_port"].as<unsigned int>();

    //    cout << taskparameters << endl;
    const string tasktype = "realtimeitlplanning";

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, controllerUrl);

    cout << "connected to mujin controller at " << controllerUrl << endl;

    SceneResourcePtr scene(new SceneResource(controllerclient, taskScenePk));

    // initialize binpicking task
    pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+string("task1"), tasktype, TRO_EnableZMQ);
    const string userinfo = "{\"username\": \"" + controllerclient->GetUserName() + "\", ""\"locale\": \"" + locale + "\"}";
    cout << "initialzing binpickingtask in UpdateEnvironmentThread with userinfo=" + userinfo << " taskparameters=" << taskparameters << endl;

    boost::shared_ptr<zmq::context_t> zmqcontext(new zmq::context_t(1));
    pBinpickingTask->Initialize("", taskZmqPort, taskHeartbeatPort, zmqcontext, false, 10, controllerCommandTimeout, userinfo, slavename);
}

/// \brief convert state of bin picking task to string
/// \param state state to convert to string
/// \return state converted to string
string ConvertStateToString(const BinPickingTaskResource::ResultGetBinpickingState& state)
{
    if (state.currentJointValues.empty() || state.currentToolValues.size() < 6) {
        stringstream ss;
        ss << "Failed to obtain robot state: joint values have "
           << state.currentJointValues.size() << " elements and tool values have "
           << state.currentToolValues.size() << " elements\n";

        throw std::runtime_error(ss.str());
    }

    stringstream ss;
    ss << state.timestamp << " (ms): joint:";
    for (size_t i = 0; i < state.currentJointValues.size(); ++i) {
        ss << state.jointNames[i] << "=" << state.currentJointValues[i] << " ";
    }

    ss << "X=" << state.currentToolValues[0] << " ";
    ss << "Y=" << state.currentToolValues[1] << " ";
    ss << "Z=" << state.currentToolValues[2] << " ";
    ss << "RX=" << state.currentToolValues[3] << " ";
    ss << "RY=" << state.currentToolValues[4] << " ";
    ss << "RZ=" << state.currentToolValues[5];
    return ss.str();
}

/// \brief run hand moving task
/// \param pTask task
/// \param goaltype whether to specify goal in 6 dimension(transform6d) or 5 dimension(translationdirection5d)
/// \param goals goal of the hand
/// \param speed speed to move at
/// \param robotname robot name
/// \param toolname tool name
/// \param whether to move in line or not
void Run(BinPickingTaskResourcePtr& pTask,
         const string& goaltype,
         const vector<double>& goals,
         double speed,
         const string& robotname,
         const string& toolname,
         bool movelinear)
{
    // print state
    BinPickingTaskResource::ResultGetBinpickingState result;
    pTask->GetPublishedTaskState(result, robotname, "mm", 1.0);
    cout << "Starting:\n" << ConvertStateToString(result) << endl;

    // start moving
    if (movelinear) {
        pTask->MoveToolLinear(goaltype, goals, robotname, toolname, speed);
    }
    else {
        pTask->MoveToHandPosition(goaltype, goals, robotname, toolname, speed);
    }

    // print state
    pTask->GetPublishedTaskState(result, robotname, "mm", 1.0);
    cout << "Finished:\n" << ConvertStateToString(result) << endl;
}

int main(int argc, char ** argv)
{
    // parsing options
    bpo::variables_map opts;
    if (!ParseOptions(argc, argv, opts)) {
        // parsing option failed
        return 1;
    }
    const string robotname = opts["robotname"].as<string>();
    const string toolname = opts["toolname"].as<string>();
    const vector<double> goals = opts["goals"].as<vector<double> >();
    const string goaltype = opts["goaltype"].as<string>();
    const double speed = opts["speed"].as<double>();
    const bool movelinearly = opts["movelinear"].as<bool>();

    // initializing
    BinPickingTaskResourcePtr pBinpickingTask;
    bool succesfullyInitialized = false;
    try {

        InitializeTask(opts, pBinpickingTask);
        succesfullyInitialized = true;
    }
    catch(const exception& ex) {
        stringstream errss;
        errss << "Caught exception " << ex.what();
        cerr << errss.str() << endl;
    }

    if (!succesfullyInitialized) {
        // task initialization faliled
        return 2;
    }

    // do interesting part
    try {
        Run(pBinpickingTask, goaltype, goals, speed, robotname, toolname, movelinearly);
    }
    catch (const exception& ex) {
        stringstream errss;
        errss << "Caught exception " << ex.what();
        cerr << errss.str() << endl;

        // task execution failed
        return 3;
    }

    return 0;
}
