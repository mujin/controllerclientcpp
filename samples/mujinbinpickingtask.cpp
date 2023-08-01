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
        ("binpicking_task_zmq_port", bpo::value<unsigned int>()->required(), "port of the binpicking task on the mujin controller, e.g. 7100")
        ("binpicking_task_heartbeat_port", bpo::value<unsigned int>()->required(), "port of the binpicking task's heartbeat signal on the mujin controller, e.g. 7101")
        ("binpicking_task_scenepk", bpo::value<std::string>()->required(), "scene pk of the binpicking task on the mujin controller, e.g. irex2013.mujin.dae")
        ("robotname", bpo::value<std::string>()->required(), "robot name, e.g. VP-5243I")
        ("testobjectname", bpo::value<std::string>()->required(), "test object name in the scene, e.g. floor")
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
    const std::string binpickingTaskScenePk = opts["binpicking_task_scenepk"].as<std::string>();
    const std::string robotname = opts["robotname"].as<std::string>();
    const std::string testobjectname = opts["testobjectname"].as<std::string>();

    // connect to mujin controller
    std::stringstream url_ss;
    url_ss << "http://"<< controllerIp << ":" << controllerPort;
    ControllerClientPtr controller = CreateControllerClient(controllerUsernamePass, url_ss.str());
    SceneResourcePtr scene(new SceneResource(controller, binpickingTaskScenePk));
    BinPickingTaskResourcePtr binpickingzmq = scene->GetOrCreateBinPickingTaskFromName_UTF8("binpickingtask1", "binpicking", TRO_EnableZMQ);
    boost::shared_ptr<zmq::context_t> zmqcontext(new zmq::context_t(2));
    binpickingzmq->Initialize("", binpickingTaskZmqPort, binpickingTaskHeartbeatPort, zmqcontext);

    Transform t;
    binpickingzmq->GetTransform(testobjectname,t,"mm");
    std::cout  << "testobject " << testobjectname << " pose: [" << t.quaternion[0] << ", " << t.quaternion[1] << ", " << t.quaternion[2] << ", " << t.quaternion[3] << ", " << t.translate[0] << ", " << t.translate[1] << ", " << t.translate[2] << "]";
}
