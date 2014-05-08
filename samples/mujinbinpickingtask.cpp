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
        ("controller_ip", bpo::value<std::string>()->default_value("controller3"), "ip of the mujin controller, e.g. controller3")
        ("controller_port", bpo::value<unsigned int>()->default_value(80), "port of the mujin controller, e.g. 80")
        ("controller_username_password", bpo::value<std::string>()->required(), "username and password to the mujin controller, e.g. username:password")
        ("robot_controller_ip", bpo::value<std::string>()->default_value("192.168.13.201"), "ip of the robot controller, e.g. 192.168.13.201")
        ("robot_controller_port", bpo::value<unsigned int>()->default_value(5007), "port of the robot controller, e.g. 5007")
        ("binpicking_task_zmq_port", bpo::value<unsigned int>()->default_value(7100), "port of the binpicking task on the mujin controller, e.g. 7100")
        ("binpicking_task_heartbeat_port", bpo::value<unsigned int>()->default_value(7101), "port of the binpicking task's heartbeat signal on the mujin controller, e.g. 7101")
        ("binpicking_task_scenepk", bpo::value<std::string>()->default_value("irex2013.mujin.dae"), "scene pk of the binpicking task on the mujin controller, e.g. irex2013.mujin.dae")
        ("robotname", bpo::value<std::string>()->default_value("VP-5243I"), "robot name, e.g. VP-5243I")
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
    const std::string robotControllerIp = opts["robot_controller_ip"].as<std::string>();
    const unsigned int robotControllerPort = opts["robot_controller_port"].as<unsigned int>();
    const unsigned int binpickingTaskZmqPort = opts["binpicking_task_zmq_port"].as<unsigned int>();
    const unsigned int binpickingTaskHeartbeatPort = opts["binpicking_task_heartbeat_port"].as<unsigned int>();
    const std::string binpickingTaskScenePk = opts["binpicking_task_scenepk"].as<std::string>();
    const std::string robotname = opts["robotname"].as<std::string>();

    // connect to mujin controller
    std::stringstream url_ss;
    url_ss << "http://"<< controllerIp << ":" << controllerPort;
    ControllerClientPtr controller = CreateControllerClient(controllerUsernamePass, url_ss.str());
    std::cout << "connected to mujin controller at " << url_ss.str() << std::endl;
    SceneResourcePtr scene(new SceneResource(controller,binpickingTaskScenePk));
    BinPickingTaskResourcePtr binpickingzmq = scene->GetOrCreateBinPickingTaskFromName_UTF8("binpickingtask1", TRO_EnableZMQ);
    binpickingzmq->Initialize(robotControllerIp, robotControllerPort, binpickingTaskZmqPort, binpickingTaskHeartbeatPort);

    Transform t;
    std::cout << "testing binpickinghttp->GetTransform()" << std::endl;
    // TODO
    std::cout << "testing binpickingzmq->GetTransform()" << std::endl;
    binpickingzmq->GetTransform("camera3",t);
}
