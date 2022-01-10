// -*- coding: utf-8 -*-
/** \example mujinmovetohandposition.cpp

    Shows how to move tool to a specified position and orientation while avoiding obstacles.
    example1: mujinmovetohandposition --controller_hostname=yourhost --robotname=yourrobot --goals=700 600 500 0 0 180
    example2: mujinmovetohandposition --controller_hostname=yourhost --robotname=yourrobot --goals=700 600 500 0 0 180 --goaltype=translationdirection5d
 */

#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#undef GetUserName // clashes with ControllerClient::GetUserName
#endif // defined(_WIN32) || defined(_WIN64)


using namespace mujinclient;

#include <boost/program_options.hpp>

namespace bpo = boost::program_options;
using namespace std;

static string s_robotname;

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
        ("controller_hostname", bpo::value<string>()->required(), "hostname or ip of the mujin controller, e.g. controllerXX or 192.168.0.1")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller")
        ("slave_request_id", bpo::value<string>()->default_value(""), "request id of the mujin slave, e.g. controller20_slave0. If empty, uses  ")
        ("controller_username_password", bpo::value<string>()->default_value("testuser:pass"), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<string>()->default_value("en_US"), "locale to use for the mujin controller client")
        ("task_scenepk", bpo::value<string>()->default_value(""), "scene pk of the binpicking task on the mujin controller, e.g. officeboltpicking.mujin.dae.")
        ("robotname", bpo::value<string>()->default_value(""), "robot name.")
        ("taskparameters", bpo::value<string>()->default_value("{}"), "binpicking task parameters, e.g. {\"robotname\": \"robot\", \"toolname\": \"tool\"}")
        ("zmq_port", bpo::value<unsigned int>()->default_value(11000), "port of the binpicking task on the mujin controller")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")

        ("toolname", bpo::value<string>()->default_value(""), "tool name, e.g. flange")
        ("goaltype", bpo::value<string>()->default_value("transform6d"), "mode to move tool with. Either transform6d or translationdirection5d")
        ("goals", bpo::value<vector<double> >()->multitoken(), "goal to move tool to, \'X Y Z RX RY RZ\'. Units are in mm and deg.")
        ("speed", bpo::value<double>()->default_value(0.1), "speed to move at")
        ("envclearance", bpo::value<double>()->default_value(-1), "environment clearcance for collision avoidance")
        ("instobjectname", bpo::value<string>()->default_value(""), "name of instobject which the ikparam belongs to")
        ("ikparamname", bpo::value<string>()->default_value(""), "name of ikparam which the robot move to")
        ;

    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc, bpo::command_line_style::unix_style ^ bpo::command_line_style::allow_short), opts);
    }
    catch (const exception& ex) {
        cerr << "Caught exception " << ex.what() << endl;
        return false;
    }

    bool badargs = false;
    try {
        bpo::notify(opts);
    }
    catch(const exception& ex) {
        cerr << "Caught exception " << ex.what() << endl;
        badargs = true;
    }

    if (!badargs) {
        bool goalsavailable = opts.find("goals") != opts.end() && opts["goals"].as<vector<double> >().size() == 6;
        bool ikparamavailable = opts.find("instobjectname") != opts.end() && opts.find("ikparamname") != opts.end();
        badargs = (!goalsavailable && !ikparamavailable) || (goalsavailable && ikparamavailable);
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
    const string controllerUsernamePass = opts["controller_username_password"].as<string>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    string taskparameters = opts["taskparameters"].as<string>();
    const string locale = opts["locale"].as<string>();
    const unsigned int taskZmqPort = opts["zmq_port"].as<unsigned int>();
    const string hostname = opts["controller_hostname"].as<string>();
    const unsigned int controllerPort = opts["controller_port"].as<unsigned int>();
    stringstream urlss;
    urlss << "http://" << hostname << ":" << controllerPort;

    const unsigned int heartbeatPort = opts["heartbeat_port"].as<unsigned int>();
    string slaverequestid = opts["slave_request_id"].as<string>();
    string taskScenePk = opts["task_scenepk"].as<string>();
    
    const bool needtoobtainfromheatbeat = taskScenePk.empty() || slaverequestid.empty();
    if (needtoobtainfromheatbeat) {
        stringstream endpoint;
        endpoint << "tcp://" << hostname << ":" << heartbeatPort;
        cout << "connecting to heartbeat at " << endpoint.str() << endl;
        string heartbeat;
        const size_t num_try_heartbeat(10);
        for (size_t it_try_heartbeat = 0; it_try_heartbeat < num_try_heartbeat; ++it_try_heartbeat) {
            heartbeat = utils::GetHeartbeat(endpoint.str());
            if (!heartbeat.empty()) {
                break;
            }
            cout << "Failed to get heart beat " << it_try_heartbeat << "/" << num_try_heartbeat << "\n";
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        if (heartbeat.empty()) {
            throw MujinException(boost::str(boost::format("Failed to obtain heartbeat from %s. Is controller running?")%endpoint.str()));
        }
        
        if (taskScenePk.empty()) {
            taskScenePk = utils::GetScenePkFromHeatbeat(heartbeat);
            cout << "task_scenepk: " << taskScenePk << " is obtained from heatbeat\n";
        }
        if (slaverequestid.empty()) {
            slaverequestid = utils::GetSlaveRequestIdFromHeatbeat(heartbeat);
            cout << "slave_request_id: " << slaverequestid << " is obtained from heatbeat\n";
        }
    }

    //    cout << taskparameters << endl;
    const string tasktype = "realtimeitlplanning";

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, urlss.str());

    cout << "connected to mujin controller at " << urlss.str() << endl;

    SceneResourcePtr scene(new SceneResource(controllerclient, taskScenePk));

    // initialize binpicking task
    pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+string("task1"), tasktype, TRO_EnableZMQ);
    const string userinfo = "{\"username\": \"" + controllerclient->GetUserName() + "\", ""\"locale\": \"" + locale + "\"}";
    cout << "initialzing binpickingtask with userinfo=" + userinfo << " taskparameters=" << taskparameters << endl;

    s_robotname = opts["robotname"].as<string>();
    if (s_robotname.empty()) {
        vector<SceneResource::InstObjectPtr> instobjects;
        scene->GetInstObjects(instobjects);
        for (vector<SceneResource::InstObjectPtr>::const_iterator it = instobjects.begin();
             it != instobjects.end(); ++it) {
            if (!(**it).dofvalues.empty()) {
                s_robotname = (**it).name;
                cout << "robot name: " << s_robotname << " is obtained from scene\n";
                break;
            }
        }
        if (s_robotname.empty()) {
            throw MujinException("Robot name was not given by command line option. Also, failed to obtain robot name from scene.\n");
        }
    }

    
    boost::shared_ptr<zmq::context_t> zmqcontext(new zmq::context_t(1));
    pBinpickingTask->Initialize(taskparameters, taskZmqPort, heartbeatPort, zmqcontext, false, controllerCommandTimeout, controllerCommandTimeout, userinfo, slaverequestid);
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
/// \param envclearance environment clearance when aboiding obstacles
/// \param checkcollision whether to move in line or not
void Run(BinPickingTaskResourcePtr& pTask,
         const string& goaltype,
         const vector<double>& goals,
         double speed,
         const string& robotname,
         const string& toolname,
         double envclearance,
         double timeout)
{
    // print state
    BinPickingTaskResource::ResultGetBinpickingState result;
    pTask->GetPublishedTaskState(result, robotname, "mm", 1.0);
    cout << "Starting:\n" << ConvertStateToString(result) << endl;

    // start moving
    pTask->MoveToHandPosition(goaltype, goals, robotname, toolname, speed, timeout, envclearance);

    // print state
    pTask->GetPublishedTaskState(result, robotname, "mm", 1.0);
    cout << "Finished:\n" << ConvertStateToString(result) << endl;
}

/// \brief run hand moving task
/// \param pTask task
/// \param instobjectname name of instobject which the ikparam belongs to
/// \param ikparamname name of ikparam which the robot move to
/// \param speed speed to move at
/// \param robotname robot name
/// \param toolname tool name
/// \param envclearance environment clearance when aboiding obstacles
/// \param checkcollision whether to move in line or not
void Run(BinPickingTaskResourcePtr& pTask,
         const string& instobjectname,
         const string& ikparamname,
         double speed,
         const string& robotname,
         const string& toolname,
         double envclearance,
         double timeout)
{
    // print state
    BinPickingTaskResource::ResultGetBinpickingState result;
    pTask->GetPublishedTaskState(result, robotname, "mm", 1.0);
    cout << "Starting:\n" << ConvertStateToString(result) << endl;

    // start moving
    pTask->MoveToHandPosition(instobjectname, ikparamname, robotname, toolname, speed, timeout, envclearance);

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
    const double speed = opts["speed"].as<double>();
    const double timeout = opts["controller_command_timeout"].as<double>();
    const double envclearance = opts["envclearance"].as<double>();

    // initializing
    BinPickingTaskResourcePtr pBinpickingTask;
    InitializeTask(opts, pBinpickingTask);

    // do interesting part
    if(opts.find("instobjectname") != opts.end() && opts.find("ikparamname") != opts.end()) {
        const string instobjectname = opts["instobjectname"].as<string>();
        const string ikparamname = opts["ikparamname"].as<string>();
        Run(pBinpickingTask, instobjectname, ikparamname, speed, s_robotname, toolname, envclearance, timeout);
    } else {
        const vector<double> goals = opts["goals"].as<vector<double> >();
        const string goaltype = opts["goaltype"].as<string>();
        Run(pBinpickingTask, goaltype, goals, speed, s_robotname, toolname, envclearance, timeout);
    }

    return 0;
}
