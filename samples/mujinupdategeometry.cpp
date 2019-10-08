// -*- coding: utf-8 -*-
/** \example mujinupdategeometry.cpp

    Shows how to add stl mesh to a scene
    example1: mujinjog --controller_hostname=yourhost --filename=yourstlfile
 */

#include <mujincontrollerclient/binpickingtask.h>

#include <boost/program_options.hpp>
#include <signal.h>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#undef GetUserName // clashes with ControllerClient::GetUserName
#endif // defined(_WIN32) || defined(_WIN64)

using namespace mujinclient;
namespace mujinjson = mujinclient::mujinjson_external;
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
        ("taskparameters", bpo::value<string>()->default_value("{}"), "binpicking task parameters, e.g. {'robotname': 'robot', 'robots':{'robot': {'externalCollisionIO':{}, 'gripperControlInfo':{}, 'robotControllerUri': '', robotDeviceIOUri': '', 'toolname': 'tool'}}}")
        ("zmq_port", bpo::value<unsigned int>()->default_value(11000), "port of the binpicking task on the mujin controller")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")
        ("objectname", bpo::value<string>(), "object name to which to add mesh")
        ("linkname", bpo::value<string>(), "link to which to add mesh")
        ("filename", bpo::value<string>(), "stl file to import mesh from")
        ("geometryname", bpo::value<string>()->default_value("default name"), "geometry name")
        ("count", bpo::value<int>()->default_value(1), "times to add/delete")
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
    const string geometryName = opts["geometryname"].as<string>();
    stringstream urlss;
    urlss << "http://" << hostname << ":" << controllerPort;

    const unsigned int heartbeatPort = opts["heartbeat_port"].as<unsigned int>();
    //string slaverequestid = opts["slave_request_id"].as<string>();
    string taskScenePk = opts["task_scenepk"].as<string>();
    
    const bool needtoobtainfromheatbeat = taskScenePk.empty(); //|| slaverequestid.empty();
    if (needtoobtainfromheatbeat) {
        stringstream endpoint;
        endpoint << "tcp://" << hostname << ":" << heartbeatPort;
        cout << "connecting to heartbeat at " << endpoint.str() << endl;
        string heartbeat;
        const size_t num_try_heartbeat(5);
        for (size_t it_try_heartbeat = 0; it_try_heartbeat < num_try_heartbeat; ++it_try_heartbeat) {
            heartbeat = utils::GetHeartbeat(endpoint.str());
            if (!heartbeat.empty()) {
                break;
            }
            cout << "Failed to get heart beat " << it_try_heartbeat << "/" << num_try_heartbeat << "\n";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
        if (heartbeat.empty()) {
            throw MujinException(boost::str(boost::format("Failed to obtain heartbeat from %s. Is controller running?")%endpoint.str()));
        }
        
        if (taskScenePk.empty()) {
            taskScenePk = utils::GetScenePkFromHeatbeat(heartbeat);
            cout << "task_scenepk: " << taskScenePk << " is obtained from heatbeat\n";
        }
        //if (slaverequestid.empty()) {
        //    slaverequestid = utils::GetSlaveRequestIdFromHeatbeat(heartbeat);
        //    cout << "slave_request_id: " << slaverequestid << " is obtained from heatbeat\n";
        //}
    }

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, urlss.str());

    cout << "connected to mujin controller at " << urlss.str() << endl;

    SceneResourcePtr scene(new SceneResource(controllerclient, taskScenePk));

    ifstream meshStream(opts["filename"].as<string>().c_str(), ios::binary);
    meshStream.unsetf(std::ios::skipws); // need this to not skip white space
    vector<unsigned char> meshData;
    copy(std::istream_iterator<unsigned char>(meshStream),
         std::istream_iterator<unsigned char>(),
         back_inserter(meshData));

int count = opts["count"].as<int>();
for(int i=0;i<count;i++){
    std::cout << i << endl;
    const Real testTranslate[3]={123,456,789};
    const Real testQuaternion[4]={sqrt(0.5),0,0,sqrt(0.5)};
    SceneResource::InstObjectPtr instobject = scene->CreateInstObject("hello","",testQuaternion,testTranslate);
    // https://redmine.mujin.co.jp/issues/5514
    instobject->SetJSON(mujinjson::GetJsonStringByKey("translate",testTranslate));
    instobject->SetJSON(mujinjson::GetJsonStringByKey("quaternion",testQuaternion));
    ObjectResourcePtr object(new ObjectResource(controllerclient, instobject->object_pk));

    //ObjectResource::IkParamResourcePtr ik=object->AddIkParam("hello","Transform6D");
    //const Real testTranslation[3]={123,456,789};
    //const Real testQuaternion[4]={sqrt(0.5),0,0,sqrt(0.5)};
    //ik->SetJSON(mujinjson::GetJsonStringByKey("translation",testTranslation));
    //ik->SetJSON(mujinjson::GetJsonStringByKey("quaternion",testQuaternion));

    std::vector<ObjectResource::LinkResourcePtr> objectlinks;
    object->GetLinks(objectlinks);
    std::vector<ObjectResource::LinkResourcePtr>::iterator it2 = std::find_if(objectlinks.begin(),objectlinks.end(),boost::bind(&ObjectResource::LinkResource::name,_1)=="base");
    ObjectResource::LinkResourcePtr link = (*it2)->AddChildLink("hello",testQuaternion,testTranslate);

    ObjectResource::GeometryResourcePtr geometry=link->AddGeometryFromRawSTL(meshData,"hello","mm",50);

    geometry->SetJSON(mujinjson::GetJsonStringByKey("translate",testTranslate));
    geometry->SetJSON(mujinjson::GetJsonStringByKey("quaternion",testQuaternion));

    boost::this_thread::sleep(boost::posix_time::seconds(30));
    //cout << "Deleted geometry\n";
    //std::string cmd="ls -l";
    //system(cmd.c_str());
    instobject->Delete();
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    //system(cmd.c_str());
}
}

int main(int argc, char ** argv)
{
    // parsing options
    bpo::variables_map opts;
    if (!ParseOptions(argc, argv, opts)) {
        // parsing option failed
        return 1;
    }
    // initializing
    BinPickingTaskResourcePtr pBinpickingTask;
    InitializeTask(opts, pBinpickingTask);

    // do interesting part
    return 0;
}
