// -*- coding: utf-8 -*-
/** \example mujinjog.cpp

    Shows how to jog robot in joint and tool spaces.
    TODO: add example set of options
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
        ("controller_ip", bpo::value<string>()->required(), "ip of the mujin controller, e.g. 127.0.0.1")
        ("controller_hostname", bpo::value<string>()->required(), "hostname of the mujin controller, e.g. controller20")
        ("run_with_controllerui", bpo::value<bool>()->default_value(true), "true if connecting to controller where controller ui is running ")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller, e.g. 80")
        ("controller_username_password", bpo::value<string>()->required(), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<string>()->default_value("en_US"), "locale to use for the mujin controller client")
        ("task_scenepk", bpo::value<string>()->required(), "scene pk of the binpicking task on the mujin controller, e.g. officeboltpicking.mujin.dae")
        ("robotname", bpo::value<string>()->required(), "robot name, e.g. VP-5243I")
        ("taskparameters", bpo::value<string>()->default_value("{}"), "binpicking task parameters, e.g. {'robotname': 'robot', 'robots':{'robot': {'externalCollisionIO':{}, 'gripperControlInfo':{}, 'robotControllerUri': '', robotDeviceIOUri': '', 'toolname': 'tool'}}}")
        ("zmq_port", bpo::value<unsigned int>()->default_value(11000), "port of the binpicking task on the mujin controller")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")
        ("jointmode", bpo::value<bool>()->required(), "mode to do jogging. true indicates joint mode and tool mode otherwise")
        ("axis", bpo::value<unsigned int>()->required(), "axis to do jogging on. For joint mode, 0 indicates J1 and 5 indicates J6. For tool mode, 0 indicates translation in X, 5 indicates rotation in Z")
        ("move_in_positive", bpo::value<bool>()->required(), "whether to move in increasing or decreasing direction")
        ("jog_duration", bpo::value<double>()->default_value(1.0), "duration of jogging")
        ("speed", bpo::value<double>()->default_value(0.1), "speed to move at, a value between 0 and 1. ")
        ("print_period_ms", bpo::value<unsigned int>()->default_value(0), "how oftern to print robot state in millisecond. 0 indicates printing only before and after jogging")
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
    catch(...) {
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

/// \brief initialize BinPickingTask and establish communication with controller
/// \param opts options parsed from command line
/// \param pBinPickingTask bin picking task to be initialized
void InitializeTask(const bpo::variables_map& opts,
                    BinPickingTaskResourcePtr& pBinpickingTask)
{
    const string controllerIp = opts["controller_ip"].as<string>();
    const string controllerHostname = opts["controller_hostname"].as<string>();
    const bool runwithcontrollerui = opts["run_with_controllerui"].as<bool>();
    const string slavename = controllerHostname + (runwithcontrollerui ? "_slave0" : "_slave1");
    const unsigned int controllerPort = opts["controller_port"].as<unsigned int>();
    const string controllerUsernamePass = opts["controller_username_password"].as<string>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    const string taskScenePk = opts["task_scenepk"].as<string>();
    const string taskparameters = opts["taskparameters"].as<string>();
    const string locale = opts["locale"].as<string>();
    const unsigned int taskHeartbeatPort = opts["heartbeat_port"].as<unsigned int>();
    const unsigned int taskZmqPort = opts["zmq_port"].as<unsigned int>();

    //    cout << taskparameters << endl;
    string tasktype = "binpicking";

    // connect to mujin controller
    stringstream url;
    url << "http://"<< controllerIp << ":" << controllerPort;
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, url.str());

    cout << "connected to mujin controller at " << url.str() << endl;

    SceneResourcePtr scene(new SceneResource(controllerclient, taskScenePk));

    // initialize binpicking task
    pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+string("task1"), tasktype, TRO_EnableZMQ);
    const string userinfo = "{\"username\": \"" + controllerclient->GetUserName() + "\", ""\"locale\": \"" + locale + "\"}";
    cout << "initialzing binpickingtask with userinfo=" + userinfo << " taskparameters=" << taskparameters << endl;

    boost::shared_ptr<zmq::context_t> zmqcontext(new zmq::context_t(1));
    pBinpickingTask->Initialize(taskparameters, taskZmqPort, taskHeartbeatPort, zmqcontext, false, 10, controllerCommandTimeout, userinfo, slavename);
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

/// \brief jogs robot either in joint space or tool space
/// \param pTask task
/// \param mode whether to move in joints or tool
/// \param axis axis to jog
/// \param moveInPositive if true, moves in increasing direction,  otherwise move in decreasing direction
/// \param duration time in second to do jogging
/// \param speed speed at which to jog
/// \param acc acceleration at which to jog
/// \param robotname name of robot
/// \param period period in ms at which to print current robot state
/// \param timeout timeout for controller command
void Run(BinPickingTaskResourcePtr& pTask,
         const string& mode,
         unsigned int axis,
         bool moveInPositive,
         unsigned int duration,
         double speed,
         double acc,
         const string& robotname,
         unsigned int printPeriodMs,
         double timeout)
{
    // print state
    BinPickingTaskResource::ResultGetBinpickingState result;
    pTask->GetPublishedTaskState(result, robotname, "mm", timeout);
    cout << "Starting:\n" << ConvertStateToString(result) << endl;

    const size_t dof = mode == "joints" ? result.currentJointValues.size() : 6;
    vector<int> movejointsigns(dof, 0);

    // start moving
    movejointsigns[axis] = moveInPositive ? 1 : -1;
    try {
        pTask->SetJogModeVelocities(mode, movejointsigns, robotname, "", speed, acc, timeout);
    }
    catch (...)
    {
        // stop for safety
        movejointsigns[axis] = 0;
        pTask->SetJogModeVelocities(mode, movejointsigns, robotname, "", speed, acc, timeout);
    }
    
    // let it jog for specified duration
    if (printPeriodMs == 0) {
        boost::this_thread::sleep(boost::posix_time::seconds(duration));
    }
    else {
        unsigned int it_sleep = 0;
        for (; it_sleep < duration*1000; it_sleep += printPeriodMs) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(printPeriodMs));
            pTask->GetPublishedTaskState(result, robotname, "mm", timeout);
            cout << ConvertStateToString(result) << endl;
        }

        if (const int delta = it_sleep - duration*1000 > 0) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(delta));
        }
    }

    // stop
    movejointsigns[axis] = 0;
    pTask->SetJogModeVelocities(mode, movejointsigns, robotname, "", speed, acc, timeout);

    // print state
    pTask->GetPublishedTaskState(result, robotname, "mm", timeout);
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
    const unsigned int axis = opts["axis"].as<unsigned int>();
    const bool moveinpositive = opts["move_in_positive"].as<bool>();
    const unsigned int printPeriodMs = opts["print_period_ms"].as<unsigned int>();
    const double speed = opts["speed"].as<double>();
    const double acc = speed * 0.7;
    const double timeout = opts["controller_command_timeout"].as<double>();
    const double jogduration = opts["jog_duration"].as<double>();
    const string mode =  opts["jointmode"].as<bool>() ? "joints" : "tool";

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
    catch (...) {
        stringstream errss;
        errss << "Caught unknown exception!";
        cerr << errss.str() << endl;
    }

    if (!succesfullyInitialized) {
        // task initialization faliled
        return 2;
    }

    // do interesting part
    try {
        Run(pBinpickingTask, mode, axis, moveinpositive, jogduration, speed, acc, robotname, printPeriodMs, timeout);
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
