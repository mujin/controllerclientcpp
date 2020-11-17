#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#undef GetUserName // clashes with ControllerClient::GetUserName
#endif // defined(_WIN32) || defined(_WIN64)


#ifndef MUJIN_TIME
#define MUJIN_TIME
#include <time.h>

#ifndef _WIN32
#if !(defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0))
#include <sys/time.h>
#endif // !(defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0))
#else
#include <sys/timeb.h>    // ftime(), struct timeb
inline void usleep(unsigned long microseconds) {
    Sleep((microseconds+999)/1000);
}
#endif  // _WIN32

#ifdef _WIN32
inline unsigned long long GetMilliTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (unsigned long long)((count.QuadPart * 1000) / freq.QuadPart);
}

#else

inline void GetWallTime(unsigned int& sec, unsigned int& nsec)
{
#if defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0)
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    sec  = start.tv_sec;
    nsec = start.tv_nsec;
#else
    struct timeval timeofday;
    gettimeofday(&timeofday,NULL);
    sec  = timeofday.tv_sec;
    nsec = timeofday.tv_usec * 1000;
#endif //defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0)
}

inline unsigned long long GetMilliTime()
{
    unsigned int sec,nsec;
    GetWallTime(sec,nsec);
    return (unsigned long long)sec*1000 + (unsigned long long)nsec/1000000;
}
#endif // _WIN32
#endif // MUJIN_TIME

using namespace mujinclient;

#include <boost/program_options.hpp>


int main(int argc, char ** argv)
{
    // parse command line arguments
    namespace bpo = boost::program_options;
    bpo::options_description desc("Options");

    desc.add_options()
        ("help,h", "produce help message")
        ("controller_ip", bpo::value<std::string>()->required(), "ip of the mujin controller, e.g. controller")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller, e.g. 80")
        ("controller_username_password", bpo::value<std::string>()->required(), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<std::string>()->default_value("en_US"), "locale to use for the mujin controller client")
        ("binpicking_task_scenepk", bpo::value<std::string>()->required(), "scene pk of the binpicking task on the mujin controller, e.g. officeboltpicking.mujin.dae")
        ("taskparameters", bpo::value<std::string>()->required(), "binpicking task parameters, e.g. {\"robotname\": \"robot\", \"toolname\": \"tool\"}")
        ("slaverequestid", bpo::value<std::string>()->required(), "slaverequestid, e.g. hostname_slave0")
        ("robotname", bpo::value<std::string>()->required(), "robot name, e.g. VS060A3-AV6-NNN-NNN")
        ("regionname", bpo::value<std::string>()->required(), "regionname, e.g. smallcontainer")
        ("objectupdatename", bpo::value<std::string>()->required(), "target object name, e.g. detected")
        ("objecturi", bpo::value<std::string>()->required(), "target object uri, e.g. mujin:/bolt0.mujin.dae")
        ("objectconfidence", bpo::value<std::string>()->default_value("{\"global_confidence\":1.0}"), "target object confidence")
        ("objectextra", bpo::value<std::string>()->default_value(""), "target object extras, e.g. {\"randombox\": {\"height\":100,\"width\":100,\"length\":100}}")
        ("waitinterval", bpo::value<unsigned int>()->default_value(500), "update interval in ms")
        ("pointsfilename", bpo::value<std::string>()->required(), "path to text file containing commaseparated point xyz positions in millimeter")
        ("pointsize", bpo::value<double>()->default_value(0.005), "pointcloud pointsize in millimeter")
        ("obstaclename", bpo::value<std::string>()->default_value("__dynamicobstacle__"), "pointcloud obstacle name")
    ;

    bpo::variables_map opts;
    bpo::store(bpo::parse_command_line(argc, argv, desc), opts);
    bool badargs = false;
    try {
        bpo::notify(opts);
    }
    catch(...) {
        badargs = true;
    }
    if(opts.count("help") || badargs) {
        std::cout << "Usage: " << argv[0] << " [OPTS]" << std::endl;
        std::cout << std::endl;
        std::cout << desc << std::endl;
        return (1);
    }

    const std::string controllerIp = opts["controller_ip"].as<std::string>();
    const unsigned int controllerPort = opts["controller_port"].as<unsigned int>();
    const std::string controllerUsernamePass = opts["controller_username_password"].as<std::string>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    const std::string binpickingTaskScenePk = opts["binpicking_task_scenepk"].as<std::string>();
    const std::string robotname = opts["robotname"].as<std::string>();
    const std::string objectupdatename = opts["objectupdatename"].as<std::string>();
    const std::string objecturi = opts["objecturi"].as<std::string>();
    const std::string objectconfidence = opts["objectconfidence"].as<std::string>();
    const std::string objectextra = opts["objectextra"].as<std::string>();
    const std::string regionname = opts["regionname"].as<std::string>();
    const std::string taskparameters = opts["taskparameters"].as<std::string>();
    const std::string slaverequestid = opts["slaverequestid"].as<std::string>();
    const unsigned int waitinterval = opts["waitinterval"].as<unsigned int>();
    const std::string pointsfilename = opts["pointsfilename"].as<std::string>();
    const double pointsize = opts["pointsize"].as<double>();
    const std::string obstaclename = opts["obstaclename"].as<std::string>();
    const std::string locale = opts["locale"].as<std::string>();

    std::string tasktype = "binpicking";
    try {
        // connect to mujin controller
        std::stringstream url_ss;
        url_ss << "http://"<< controllerIp << ":" << controllerPort;
        ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, url_ss.str());
        SceneResourcePtr scene(new SceneResource(controllerclient, binpickingTaskScenePk));

        // initialize binpicking task
        BinPickingTaskResourcePtr pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+std::string("task1"), tasktype);
        std::string userinfo_json = "{\"username\": \"" + controllerclient->GetUserName() + "\", \"locale\": \"" + locale + "\"}";
        std::cout << "initialzing binpickingtask in UpdateEnvironmentThread with userinfo=" + userinfo_json << " taskparameters=" << taskparameters << " slaverequestid=" << slaverequestid << std::endl;
        pBinpickingTask->Initialize(taskparameters, controllerCommandTimeout, userinfo_json, slaverequestid);

        // populate dummy data
        std::vector<BinPickingTaskResource::DetectedObject> detectedobjects;
        std::vector<float> points;
        std::string resultstate;

        // create object
        BinPickingTaskResource::DetectedObject detectedobject;
        detectedobject.name = str(boost::format("%s_%d") % objectupdatename % 0);
        detectedobject.object_uri = objecturi;
        Transform transform;
        transform.quaternion[0] = 1; transform.quaternion[1] = 0; transform.quaternion[2] = 0; transform.quaternion[3] = 0;
        transform.translate[0] = 450; transform.translate[1] = 0; transform.translate[2] = 60;  // in milimeter
        detectedobject.transform = transform;
        detectedobject.confidence = objectconfidence;
        detectedobject.timestamp = GetMilliTime();
        detectedobject.extra = objectextra;

        // load pointcloud from file, assuming comma seprated xyz coordinates in millimeter
        std::ifstream pointsfile(pointsfilename.c_str());
        while (pointsfile) {
            std::string s;
            if (!std::getline(pointsfile, s)) {
                break;
            }
            std::istringstream ss(s);
            while (ss) {
                std::string s;
                if (!std::getline(ss, s, ',')) {
                    break;
                }
                points.push_back(boost::lexical_cast<double>(s));
            }
        }
        std::cout << "loaded " << points.size() / 3 << " points from " << pointsfilename << std::endl;

        // update environment loop
        std::string inputdataunit = "mm";
        while (1) {
            try {
                pBinpickingTask->UpdateEnvironmentState(objectupdatename, detectedobjects, points, resultstate, pointsize, obstaclename, inputdataunit, controllerCommandTimeout);
                std::cout << "UpdateEnvironmentState with " << detectedobjects.size() << " objects " << (points.size()/3.) << " points" << std::endl;
            }
            catch(const std::exception& ex) {
                std::cerr << "Failed to update environment state: " << ex.what() << "." << std::endl;

            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(waitinterval));
        }
    }
    catch(const std::exception& ex) {
        std::stringstream errss;
        errss << "Caught exception " << ex.what();
        std::cerr << errss.str() << std::endl;
    }
    catch (...) {
        std::stringstream errss;
        errss << "Caught unknown exception!";
        std::cerr << errss.str() << std::endl;
    }
}
