// -*- coding: utf-8 -*-
/** \example mujingrabrelease.cpp

    Shows how to grabrelease robot in joint and tool spaces.
    example1: mujingrabrelease --controller_hostname=yourhost --robotname=yourrobot --targetname work_a_1 # grab work_a_1
    example2: mujingrabrelease --controller_hostname=yourhost --robotname=yourrobot --targetname work_a_1 --movealong 2 --moveby -100  # grab work_a_1 and move -100 along axis 2 and releaes
 */

#include <mujincontrollerclient/binpickingtask.h>

#include <boost/program_options.hpp>
#include <signal.h>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#undef GetUserName // clashes with ControllerClient::GetUserName
#endif // defined(_WIN32) || defined(_WIN64)

using namespace mujinclient;
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
        ("taskparameters", bpo::value<string>()->default_value("{}"), "binpicking task parameters, e.g. {'robotname': 'robot', 'robots':{'robot': {'externalCollisionIO':{}, 'gripperControlInfo':{}, 'robotControllerUri': '', robotDeviceIOUri': '', 'toolname': 'tool'}}}")
        ("zmq_port", bpo::value<unsigned int>()->default_value(11000), "port of the binpicking task on the mujin controller")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")
        ("toolname", bpo::value<string>()->default_value(""), "tool name, e.g. flange")
        ("goaltype", bpo::value<string>()->default_value("transform6d"), "mode to move tool with. Either transform6d or translationdirection5d")
        ("targetname", bpo::value<string>(), "name of target to grab, e.g. work1")
        ("goals", bpo::value<vector<double> >()->multitoken(), "goal to move tool to, \'X Y Z RX RY RZ\'. Units are in mm and deg.")
        ("movealong", bpo::value<int>()->default_value(0), "axis along which to move once grabbed")
        ("moveby", bpo::value<double>()->default_value(100), "distance to move along axis once grabbed ")
        ("speed", bpo::value<double>()->default_value(0.1), "speed to move at")
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


void GrabRelease(BinPickingTaskResourcePtr& pBinpickingTask,
                 const string& goaltype,
                 vector<double>& goals,
                 double speed,
                 const string& robotname,
                 const string& toolname,
                 const string& targetname,
                 int movealong,
                 double moveby)
{
    cout << "moving tool to "
         << goals[0] << ", " << goals[1] << ", " << goals[2] << ", "
         << goals[3] << ", " << goals[4] << ", " << goals[5] << endl;
    pBinpickingTask->MoveToHandPosition(goaltype, goals, robotname, toolname, speed, 30);

    pBinpickingTask->Grab(targetname);

    goals[movealong] += moveby;
    cout << "after grabbing " << targetname << ", moving tool to "
         << goals[0] << ", " << goals[1] << ", " << goals[2] << ", "
         << goals[3] << ", " << goals[4] << ", " << goals[5] << endl;
    pBinpickingTask->MoveToHandPosition(goaltype, goals, robotname, toolname, speed, 30);

    pBinpickingTask->Release(targetname);
    goals[movealong] -= moveby;
    cout << "after releasing " << targetname << ", moving tool to "
         << goals[0] << ", " << goals[1] << ", " << goals[2] << ", "
         << goals[3] << ", " << goals[4] << ", " << goals[5] << endl;
    pBinpickingTask->MoveToHandPosition(goaltype, goals, robotname, toolname, speed, 30);
}

/// \brief initialize BinPickingTask and establish communication with controller
/// \param opts options parsed from command line
/// \param pBinPickingTask bin picking task to be initialized
void InitializeTask(const bpo::variables_map& opts,
                    BinPickingTaskResourcePtr& pBinpickingTask)
{
    const string controllerUsernamePass = opts["controller_username_password"].as<string>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    const string taskparameters = opts["taskparameters"].as<string>();
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
    pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+string("temp task"), tasktype, TRO_EnableZMQ);
    const string userinfo = "{\"username\": \"" + controllerclient->GetUserName() + "\", ""\"locale\": \"" + locale + "\"}";
    cout << "initializing binpickingtask with userinfo=" + userinfo << " taskparameters=" << taskparameters << endl;

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
    pBinpickingTask->Initialize(taskparameters, taskZmqPort, heartbeatPort, zmqcontext, false, 10, controllerCommandTimeout, userinfo, slaverequestid);
}

int main(int argc, char ** argv)
{
    // parsing options
    bpo::variables_map opts;
    if (!ParseOptions(argc, argv, opts)) {
        // parsing option failed
        return 1;
    }

    const string toolname = opts["toolname"].as<string>();
    const string targetname = opts["targetname"].as<string>();
    vector<double> goals = opts["goals"].as<vector<double> >();
    const double speed = opts["speed"].as<double>();
    const string goaltype = opts["goaltype"].as<string>();
    const int movealong =  opts["movealong"].as<int>();
    const double moveby =  opts["moveby"].as<double>();

    // initializing
    BinPickingTaskResourcePtr pBinpickingTask;
    InitializeTask(opts, pBinpickingTask);


    GrabRelease(pBinpickingTask,
                goaltype, goals, speed, s_robotname, toolname, targetname,
                movealong, moveby);

    return 0;
}
