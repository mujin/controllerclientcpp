// -*- coding: utf-8 -*-

#include "mujincontrollerclient/mujinzmqclient.h"

#include "logging.h"

MUJIN_LOGGER("mujin.controllerclientcpp.mujinzmqclient");

using namespace mujinzmq;

namespace mujinclient {

ZmqMujinControllerClient::ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port) : ZmqClient(host, port)
{
    _InitializeSocket(context);
}

} // end namespace mujinclient
