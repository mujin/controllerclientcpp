// -*- coding: utf-8 -*-
/** \example mujincreateinstobject.cpp

    Shows how to create empty inst object in a scene
    example1: mujincreateinstobject --controller_hostname=yourhost # creates an inst object at origin
    example2: mujincreateinstobject --controller_hostname=yourhost --name work_0x1a --translate 100 200 -300
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
        ("controller_username_password", bpo::value<string>()->default_value("testuser:pass"), "username and password to the mujin controller, e.g. username:password")
        ("task_scenepk", bpo::value<string>()->default_value(""), "scene pk of the binpicking task on the mujin controller, e.g. officeboltpicking.mujin.dae.")
        ("heartbeat_port", bpo::value<unsigned int>()->default_value(11001), "port of the binpicking task's heartbeat signal on the mujin controller")
        ("objectname", bpo::value<string>()->default_value(""), "object name to query")
        ("linkname", bpo::value<string>()->default_value("base"), "link name to query")
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

int main(int argc, char ** argv)
{
    // parsing options
    bpo::variables_map opts;
    if (!ParseOptions(argc, argv, opts)) {
        // parsing option failed
        return 1;
    }

    const string controllerUsernamePass = opts["controller_username_password"].as<string>();
    const string hostname = opts["controller_hostname"].as<string>();
    const unsigned int controllerPort = opts["controller_port"].as<unsigned int>();
    stringstream urlss;
    urlss << "http://" << hostname << ":" << controllerPort;

    const unsigned int heartbeatPort = opts["heartbeat_port"].as<unsigned int>();
    string taskScenePk = opts["task_scenepk"].as<string>();
    
    const bool needtoobtainfromheatbeat = taskScenePk.empty();
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
            boost::this_thread::sleep(boost::posix_time::seconds(0.1));
        }
        if (heartbeat.empty()) {
            throw MujinException(boost::str(boost::format("Failed to obtain heartbeat from %s. Is controller running?")%endpoint.str()));
        }
        
        if (taskScenePk.empty()) {
            taskScenePk = utils::GetScenePkFromHeatbeat(heartbeat);
            cout << "task_scenepk: " << taskScenePk << " is obtained from heatbeat\n";
        }
    }

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, urlss.str());
    cout << "connected to mujin controller at " << urlss.str() << endl;
    SceneResourcePtr scene(new SceneResource(controllerclient,taskScenePk));
    std::vector<SceneResource::InstObjectPtr> instobjects;
    scene->GetInstObjects(instobjects);
    std::string object_name=opts["objectname"].as<string>();
    std::vector<SceneResource::InstObjectPtr>::iterator it1 = std::find_if(instobjects.begin(),instobjects.end(),boost::bind(&SceneResource::InstObject::name,_1)==object_name);
    cout << "there are " << instobjects.size() << " objects." << endl;
    assert(it1!=instobjects.end()); // existence check
    ObjectResourcePtr object(new ObjectResource(controllerclient, (*it1)->object_pk)); 
    std::vector<ObjectResource::LinkResourcePtr> objectlinks;
    object->GetLinks(objectlinks);
    std::string link_name=opts["linkname"].as<string>();
    std::vector<ObjectResource::LinkResourcePtr>::iterator it2 = std::find_if(objectlinks.begin(),objectlinks.end(),boost::bind(&ObjectResource::LinkResource::name,_1)==link_name);
    assert(it2!=objectlinks.end()); // existence check
    cout << "there are " << objectlinks.size() << " links." << endl;
    std::vector<ObjectResource::GeometryResourcePtr> geometries;
    (*it2)->GetGeometries(geometries);
    cout << "there are " << geometries.size() << " geometries." << endl;
    std::vector<ObjectResource::IkParamResourcePtr> ikparams;
    object->GetIkParams(ikparams);
    cout << "there are " << ikparams.size() << " ikparams." << endl;

    return 0;
}
