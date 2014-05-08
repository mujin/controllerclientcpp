// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>
#include <mujincontrollerclient/binpickingtask.h>
#include <mujincontrollerclient/handeyecalibrationtask.h>

#include <boost/thread/thread.hpp> // for sleep
#include <boost/program_options.hpp>

#include <iostream>

using namespace mujinclient;

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
        ("binpicking_task_zmq_port", bpo::value<unsigned int>()->default_value(7100), "port of the binpicking task on the mujin controller, e.g. 7101")
        ("binpicking_task_heartbeat_port", bpo::value<unsigned int>()->default_value(7100), "port of the binpicking task's heartbeat signal on the mujin controller, e.g. 7101")
        ("binpicking_task_scenepk", bpo::value<std::string>()->default_value("irex2013.mujin.dae"), "scene pk of the binpicking task on the mujin controller, e.g. irex2013.mujin.dae")
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

    // connect to mujin controller
    std::stringstream url_ss;
    url_ss << "http://"<< controllerIp << ":" << controllerPort;
    ControllerClientPtr controller = CreateControllerClient(controllerUsernamePass, url_ss.str());
    std::cout << "connected to mujin controller at " << url_ss.str() << std::endl;
    SceneResourcePtr scene(new SceneResource(controller,binpickingTaskScenePk));
    BinPickingTaskResourcePtr binpicking = scene->GetOrCreateBinPickingTaskFromName_UTF8("binpickingtask1");

    HandEyeCalibrationTaskResourcePtr calib;
    binpicking->Initialize(robotControllerIp, robotControllerPort, binpickingTaskZmqPort, binpickingTaskHeartbeatPort);
    calib.reset(new HandEyeCalibrationTaskResource(std::string("calibtask1"), controller,scene));
    BinPickingTaskResource::ResultGetJointValues jointvaluesresult;
    binpicking->GetJointValues(jointvaluesresult);

    HandEyeCalibrationTaskParameters calibparam;
    calibparam.SetDefaults();
    calibparam.cameraname = "camera2";
    calibparam.numsamples = 15;
    calibparam.distances[0] = 0.6;
    calibparam.distances[1] = 0.9;
    calibparam.distances[2] = 0.1;
    calibparam.transform.translate[0] = -170;
    calibparam.transform.translate[1] = 0;
    calibparam.transform.translate[2] = 0;
    calibparam.transform.quaternion[0] = 0.70710678;
    calibparam.transform.quaternion[1] = 0;
    calibparam.transform.quaternion[2] = 0;
    calibparam.transform.quaternion[3] = -0.70710678;

    HandEyeCalibrationResultResource::CalibrationResult result;
    calib->ComputeCalibrationPoses(30, calibparam, result);
    std::cout << "poses: " << std::endl;
    for (int i = 0; i < result.poses.size(); i++) {
        for (int j = 0; j < 7; j++) {
            std::cout << result.poses[i][j] << ", ";
        }
        std::cout << std::endl;
    }
    std::cout << "configs: " << std::endl;
    for (int i = 0; i < result.configs.size(); i++) {
        for (int j = 0; j < result.configs[i].size(); j++) {
            std::cout << result.configs[i][j] << ", ";
        }
        std::cout << std::endl;
    }
    std::cout << "jointindices: " << std::endl;
    for (int i = 0; i < result.jointindices.size(); i++) {
        std::cout << result.jointindices[i] << ", ";
    }
    std::cout << std::endl;
    DestroyControllerClient();
}
