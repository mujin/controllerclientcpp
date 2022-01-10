// -*- coding: utf-8 -*-
/** \example mujinbackup.cpp

    Shows how to upgrade controller's software.
    example1: mujinupgrade --controller_hostname=yourhost --filename=software.bin
 */

#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/program_options.hpp>
#include <signal.h>
#include <iostream>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#undef GetUserName // clashes with ControllerClient::GetUserName
#define sleep(n) Sleep((n)*1000)
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
        ("filename", bpo::value<string>(), "backup file name")
        ("autorestart", bpo::value<unsigned int>()->default_value(1))
        ("uploadonly", bpo::value<unsigned int>()->default_value(0), "if 1, will only upload. continue upgrade process without specifying filename.")
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
    const unsigned int autorestart = opts["autorestart"].as<unsigned int>();
    const unsigned int uploadonly = opts["uploadonly"].as<unsigned int>();
    stringstream urlss;
    urlss << "http://" << hostname << ":" << controllerPort;

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, urlss.str());
    cerr << "connected to mujin controller at " << urlss.str() << endl;

    if(opts.count("filename")){
        const string filename = opts["filename"].as<string>();
        ifstream fin(filename.c_str(), std::ios::in | std::ios::binary);
        controllerclient->Upgrade(fin,autorestart,uploadonly);
    }else{
        stringstream ssempty;
        controllerclient->Upgrade(ssempty,autorestart,uploadonly);
    }
    cerr << "upload done." << endl;

    if(!uploadonly){
        for(;;){
            string status="";
            double progress=0;
            int cnt=0;
            bool received=false;
            for(;;){
                try{
                    received=controllerclient->GetUpgradeStatus(status,progress);
                }catch(mujinclient::MujinException e){
                    // is 120sec proper?
                    if(!autorestart || ++cnt==60){
                        throw e;
                    }
                    sleep(2);
                    continue;
                }
                break;
            }
            cerr << "status: " << status << " progress: " << progress << endl;
            if(!received || status=="done" || status=="error")break;
            sleep(1); // long interval with autorestart could cause disconnect before receiving "done".
        }
    }

    return 0;
}
