#include <mujincontrollerclient/binpickingtask.h>
#include <iostream>

#ifndef USE_LOG4CPP // logging

#define BINPICKINGSAMPLE_LOG_INFO(msg) std::cout << msg << std::endl;
#define BINPICKINGSAMPLE_LOG_ERROR(msg) std::cerr << msg << std::endl;

#else

#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Appender.hh>
#include "log4cpp/OstreamAppender.hh"
#include <log4cpp/SyslogAppender.hh>

LOG4CPP_LOGGER_N(mujincontrollerclientbpslogger, "mujincontrollerclient.samples.binpicking");

#define BINPICKINGSAMPLE_LOG_INFO(msg) LOG4CPP_INFO_S(mujincontrollerclientbpslogger) << msg;
#define BINPICKINGSAMPLE_LOG_ERROR(msg) LOG4CPP_ERROR_S(mujincontrollerclientbpslogger) << msg;

#endif // logging


using namespace mujinclient;

#include <boost/program_options.hpp>

int main(int argc, char ** argv)
{
#ifdef USE_LOG4CPP
    std::string logPropertiesFilename = "sample.properties";
    try{
        log4cpp::PropertyConfigurator::configure(logPropertiesFilename);
    } catch (log4cpp::ConfigureFailure& e) {
        log4cpp::Appender *consoleAppender = new log4cpp::OstreamAppender("console", &std::cout);
        std::string pattern = "%d %c [%p] %m%n";
        log4cpp::PatternLayout* patternLayout0 = new log4cpp::PatternLayout();
        patternLayout0->setConversionPattern(pattern);
        consoleAppender->setLayout(patternLayout0);

        log4cpp::Category& rootlogger = log4cpp::Category::getRoot();
        rootlogger.setPriority(log4cpp::Priority::INFO);
        rootlogger.addAppender(consoleAppender);
        rootlogger.error("failed to load logger properties at %s, using default logger. error: %s", logPropertiesFilename.c_str(), e.what());
    }

#endif

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
        std::stringstream ss;
        ss << "Usage: " << argv[0] << " [OPTS]" << std::endl;
        ss << std::endl;
        ss << desc << std::endl;
        BINPICKINGSAMPLE_LOG_INFO(ss.str());
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
    binpickingzmq->Initialize(url_ss.str(), "", binpickingTaskZmqPort, binpickingTaskHeartbeatPort, zmqcontext);

    Transform t;
    binpickingzmq->GetTransform(testobjectname,t);
    std::stringstream ss;
    ss  << "testobject " << testobjectname << " pose: [" << t.quaternion[0] << ", " << t.quaternion[1] << ", " << t.quaternion[2] << ", " << t.quaternion[3] << ", " << t.translate[0] << ", " << t.translate[1] << ", " << t.translate[2] << "]";
    BINPICKINGSAMPLE_LOG_INFO(ss.str());

#ifdef USE_LOG4CPP
    log4cpp::Category::shutdown();
#endif
}
