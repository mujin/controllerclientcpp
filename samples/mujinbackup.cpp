// -*- coding: utf-8 -*-
/** \example mujinbackup.cpp

    Shows how to save/restore backup
    example1 for saving backup: mujinbackup --controller_hostname=yourhost --filename=backup.tar.gz --save_config=1 --save_media=1 --backupscenepks=scenename.mujin.msgpack
    example2 for restoring backup: mujinbackup --controller_hostname=yourhost --filename=backup.tar.gz --restore --restore_config=1 --restore_media=0 # restore only config even though backup.tar.gz has media backup
 */

#include <mujincontrollerclient/mujincontrollerclient.h>

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
/// \return true if non-help options are parsed successfully.
bool ParseOptions(int argc, char ** argv, bpo::variables_map& opts)
{
    // parse command line arguments
    bpo::options_description desc("Options");

    desc.add_options()
        ("help,h", "produce help message")
        ("controller_hostname", bpo::value<string>()->required(), "hostname or ip of the mujin controller, e.g. controllerXX or 192.168.0.1")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller")
        ("controller_username_password", bpo::value<string>()->default_value("testuser:pass"), "username and password to the mujin controller, e.g. username:password")
        ("filename", bpo::value<string>()->required(), "backup file name")
        ("restore", bpo::value<unsigned int>()->default_value(0), "if 1, will restore")
        ("save_config", bpo::value<unsigned int>()->default_value(1))
        ("save_media", bpo::value<unsigned int>()->default_value(1))
        ("backupscenepks", bpo::value<string>()->default_value(""), "comma separated list of scenes for backup")
        ("restore_config", bpo::value<unsigned int>()->default_value(1))
        ("restore_media", bpo::value<unsigned int>()->default_value(1))
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
    const unsigned int save_config = opts["save_config"].as<unsigned int>();
    const unsigned int save_media = opts["save_media"].as<unsigned int>();
    const string backupscenepks = opts["backupscenepks"].as<string>();
    const unsigned int restore_config = opts["restore_config"].as<unsigned int>();
    const unsigned int restore_media = opts["restore_media"].as<unsigned int>();
    const unsigned int restore = opts["restore"].as<unsigned int>();
    const string filename = opts["filename"].as<string>();
    stringstream urlss;
    urlss << "http://" << hostname << ":" << controllerPort;

    // connect to mujin controller
    ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, urlss.str());
    cerr << "connected to mujin controller at " << urlss.str() << endl;

    vector<unsigned char>buf;
    if(restore){
        ifstream fin(filename.c_str(), std::ios::in | std::ios::binary);
        controllerclient->RestoreBackup(fin,restore_config,restore_media);
    }else{
        ofstream fout(filename.c_str(), std::ios::out | std::ios::binary);
        controllerclient->SaveBackup(fout,save_config,save_media,backupscenepks);
    }

    return 0;
}
