// -*- coding: utf-8 -*-
/** \example mujincreateikparam.cpp

    Shows how to create ikparam using current coordinate
    example: mujincreateikparam --controller_hostname localhost --task_scenepk machine.mujin.dae --iktype Transform6D --object_name object --taskparameters '{"robotname":"robot","toolname":"tool"}'
 */

#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#undef GetUserName // clashes with ControllerClient::GetUserName
#endif // defined(_WIN32) || defined(_WIN64)


using namespace mujinclient;
namespace mujinjson = mujinclient::mujinjson_external;

#include <boost/program_options.hpp>
#include <boost/bind.hpp>

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
        ("slave_request_id", bpo::value<string>()->default_value(""), "request id of the mujin slave, e.g. controller20_slave0. If empty, uses  ")
        ("controller_username_password", bpo::value<string>()->default_value("testuser:pass"), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<string>()->default_value("en_US"), "locale to use for the mujin controller client")
        ("scenepk", bpo::value<string>()->default_value(""), "scene pk on the mujin controller, e.g. officeboltpicking.mujin.dae.")
        ("object_name", bpo::value<string>()->default_value(""), "object name to create ikparam for")
        ("update_external_reference", bpo::value<unsigned int>()->default_value(0), "if 1, update external reference")
        ("iktype", bpo::value<string>()->default_value("Transform6D"), "iktype")
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

    const double timeout = opts["controller_command_timeout"].as<double>();

    // get manip position
    const string object_name = opts["object_name"].as<string>();

    stringstream urlss;
    {
        const string hostname = opts["controller_hostname"].as<string>();
        const unsigned int controllerPort = opts["controller_port"].as<unsigned int>();
        urlss << "http://" << hostname << ":" << controllerPort;
    }
    ControllerClientPtr controllerclient = CreateControllerClient(opts["controller_username_password"].as<string>(), urlss.str());
    SceneResourcePtr scene(new SceneResource(controllerclient, opts["scenepk"].as<string>()));
    std::vector<SceneResource::InstObjectPtr> instobjects;
    scene->GetInstObjects(instobjects);
    std::vector<SceneResource::InstObjectPtr>::iterator it1 = std::find_if(instobjects.begin(),instobjects.end(),boost::bind(&SceneResource::InstObject::name,_1)==object_name);
    std::string object_pk = (*it1)->object_pk;
    if(opts["update_external_reference"].as<unsigned int>() && !(*it1)->reference_object_pk.empty()){
        object_pk = (*it1)->reference_object_pk;
    }
    ObjectResourcePtr object(new ObjectResource(controllerclient, object_pk));
    cout << "obtaining object done" << endl;

    map<string, vector<int>> params = object->Get<map<string, vector<int>>>("robot_motion_parameters/int_parameters");
    cout << "old pulseOffset =";
    for(auto &e: params["pulseOffset"]){cout<<" "<<e;}
    cout << endl;

    params["pulseOffset"][0]++;
    map<string, map<string, vector<int>>> setparams;
    setparams["int_parameters"] = params;
    object->SetJSON(mujinjson::GetJsonStringByKey("robot_motion_parameters",setparams));

    params = object->Get<map<string, vector<int>>>("robot_motion_parameters/int_parameters");
    cout << "new pulseOffset =";
    for(auto &e: params["pulseOffset"]){cout<<" "<<e;}
    cout << endl;

    return 0;
}
