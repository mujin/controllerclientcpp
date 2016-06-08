#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>

using namespace mujinclient;

#include <boost/program_options.hpp>


int main(int argc, char ** argv)
{
    // parse command line arguments
    namespace bpo = boost::program_options;
    bpo::options_description desc("Options");

    desc.add_options()
        ("help,h", "produce help message")
        ("controller_ip", bpo::value<std::string>()->required(), "ip of the mujin controller, e.g. controller3")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller, e.g. 80")
        ("controller_username_password", bpo::value<std::string>()->required(), "username and password to the mujin controller, e.g. username:password")
        ("controller_command_timeout", bpo::value<double>()->default_value(10), "command timeout in seconds, e.g. 10")
        ("locale", bpo::value<std::string>()->default_value("en_US"), "locale to use for the mujin controller client")        
        ("binpicking_task_zmq_port", bpo::value<unsigned int>()->required(), "port of the binpicking task on the mujin controller, e.g. 7100")
        ("binpicking_task_heartbeat_port", bpo::value<unsigned int>()->required(), "port of the binpicking task's heartbeat signal on the mujin controller, e.g. 7101")
        ("binpicking_task_heartbeat_timeout", bpo::value<unsigned int>()->default_value(10), "binpicking task's heartbeat timeout in seconds, e.g. 10")
        ("binpicking_task_scenepk", bpo::value<std::string>()->required(), "scene pk of the binpicking task on the mujin controller, e.g. irex2013.mujin.dae")
        ("slaverequestid", bpo::value<std::string>()->required(), "slaverequestid")
        ("robotname", bpo::value<std::string>()->required(), "robot name, e.g. VP-5243I")
        ("regionname", bpo::value<std::string>()->required(), "regionname, e.g. sourcecontainer")
        ("targetupdatename", bpo::value<std::string>()->required(), "target object name, e.g. floor")
        ("waitinterval", bpo::value<unsigned int>()->default_value(100), "update interval in ms")
        ("pointsize", bpo::value<double>()->default_value(0.005), "pointcloud pointsize")
        ("obstaclename", bpo::value<std::string>()->required(), "pointcloud obstacle name")
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
    const unsigned int binpickingTaskZmqPort = opts["binpicking_task_zmq_port"].as<unsigned int>();
    const unsigned int binpickingTaskHeartbeatPort = opts["binpicking_task_heartbeat_port"].as<unsigned int>();
    const double binpickingTaskHeartbeatTimeout = opts["binpicking_task_heartbeat_timeout"].as<double>();
    const double controllerCommandTimeout = opts["controller_command_timeout"].as<double>();
    const std::string binpickingTaskScenePk = opts["binpicking_task_scenepk"].as<std::string>();
    const std::string robotname = opts["robotname"].as<std::string>();
    const std::string targetupdatename = opts["targetupdatename"].as<std::string>();
    const std::string regionname = opts["regionname"].as<std::string>();
    const std::string taskparameters = opts["taskparameters"].as<std::string>();
    const std::string slaverequestid = opts["slaverequestid"].as<std::string>();
    const unsigned int waitinterval = opts["waitinterval"].as<unsigned int>();
    const double pointsize = opts["pointsize"].as<double>();
    const std::string obstaclename = opts["obstaclename"].as<std::string>();
    const std::string locale = opts["locale"].as<std::string>();

    std::string tasktype = "binpicking";
    try {
        boost::shared_ptr<zmq::context_t> zmqcontext(new zmq::context_t(8));
        // connect to mujin controller
        std::stringstream url_ss;
        url_ss << "http://"<< controllerIp << ":" << controllerPort;
        ControllerClientPtr controllerclient = CreateControllerClient(controllerUsernamePass, url_ss.str());
        SceneResourcePtr scene(new SceneResource(controllerclient, binpickingTaskScenePk));
        // TODO made these options
        std::string resultstate;


        BinPickingTaskResourcePtr pBinpickingTask = scene->GetOrCreateBinPickingTaskFromName_UTF8(tasktype+std::string("task1"), tasktype, TRO_EnableZMQ);
        std::string userinfo_json = "{\"username\": \"" + controllerclient->GetUserName() + "\", \"locale\": \"" + locale + "\"}";
        std::cout << "initialzing binpickingtask in UpdateEnvironmentThread with userinfo " + userinfo_json << std::endl;

        pBinpickingTask->Initialize(taskparameters, binpickingTaskZmqPort, binpickingTaskHeartbeatPort, zmqcontext, false, binpickingTaskHeartbeatTimeout, controllerCommandTimeout, userinfo_json, slaverequestid);
        uint64_t starttime;
        uint64_t lastwarnedtimestamp1 = 0;

        // TODO populate these with dummy data
        std::vector<BinPickingTaskResource::DetectedObject> detectedobjects;
        std::vector<Real> totalpoints;
        std::map<std::string, std::vector<Real> > mResultPoints;
        std::vector<Real> newpoints;


        while (1) {
            bool bDetectedObjectsValid = false;

            //MUJIN_LOG_INFO(str(boost::format("updating environment with %d detected objects")%vDetectedObject.size()));

            // use the already acquired detection results without locking
            unsigned int nameind = 0;
            //DetectedObjectPtr detectedobject;
            // TransformMatrix O_T_region, O_T_baselinkcenter, tBaseLinkInInnerRegionTopCenter;
            // unsigned int numUpdatedRegions = 0;
            // for (unsigned int i=0; i<detectedobjects.size(); i++) {
            //     //Transform newtransform;
            //     //BinPickingTaskResource::DetectedObject detectedobject;
            //     //newtransform = vDetectedObject[i]->transform; // apply offset to result transform
            //     // overwrite name because we need to add id to the end
            //     std::stringstream name_ss;
            //     name_ss << targetupdatename << nameind;
            //     detectedobjects[i].name = name_ss.str();
            //     nameind++;

            //     // convert to mujinclient format
            //     Transform transform;
            //     transform.quaternion[0] = newtransform.rot[0];
            //     transform.quaternion[1] = newtransform.rot[1];
            //     transform.quaternion[2] = newtransform.rot[2];
            //     transform.quaternion[3] = newtransform.rot[3];
            //     transform.translate[0] = newtransform.trans[0];
            //     transform.translate[1] = newtransform.trans[1];
            //     transform.translate[2] = newtransform.trans[2];

            //     detectedobject.object_uri = vDetectedObject[i]->objecturi;
            //     detectedobject.transform = transform;
            //     detectedobject.confidence = vDetectedObject[i]->confidence;
            //     detectedobject.timestamp = vDetectedObject[i]->timestamp;
            //     detectedobject.extra = vDetectedObject[i]->extra;
            //     detectedobjects.push_back(detectedobject);
            // }
            // for(unsigned int i=0; i<cameranamestobeused.size(); i++) {
            //     std::string cameraname = cameranamestobeused[i];
            //     std::map<std::string, std::vector<Real> >::const_iterator itpoints = mResultPoints.find(cameraname);
            //     if( itpoints != mResultPoints.end() ) {
            //         // get point cloud obstacle
            //         newpoints.resize(itpoints->second.size());
            //         for (size_t j=0; j<itpoints->second.size(); j+=3) {
            //             Vector newpoint = Vector(itpoints->second.at(j), itpoints->second.at(j+1), itpoints->second.at(j+2));
            //             newpoints[j] = newpoint.x;
            //             newpoints[j+1] = newpoint.y;
            //             newpoints[j+2] = newpoint.z;
            //         }
            //         totalpoints.insert(totalpoints.end(), newpoints.begin(), newpoints.end());
            //     }
            // }
            try {
                //starttime = GetMilliTime();
                pBinpickingTask->UpdateEnvironmentState(targetupdatename, detectedobjects, totalpoints, resultstate, pointsize, obstaclename, "m", 10);
                std::stringstream ss;
                //ss << "UpdateEnvironmentState with " << detectedobjects.size() << " objects " << (totalpoints.size()/3.) << " points, took " << (GetMilliTime() - starttime) / 1000.0f << " secs";
                ss << "UpdateEnvironmentState with " << detectedobjects.size() << " objects " << (totalpoints.size()/3.) << " points";
                std::cout << ss.str() << std::endl;
            }
            catch(const std::exception& ex) {
                //if (GetMilliTime() - lastwarnedtimestamp1 > 1000.0) {
                //lastwarnedtimestamp1 = GetMilliTime();
                    std::stringstream ss;
                    ss << "Failed to update environment state: " << ex.what() << ".";
                    std::cout << ss.str() << std::endl;
                    //}
                boost::this_thread::sleep(boost::posix_time::milliseconds(waitinterval));
            }
        }
    }
    catch (const zmq::error_t& e) {
        std::stringstream errss;
        errss << "Caught zmq exception errornum=" << e.num();
        std::cerr << errss.str() << std::endl;
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
