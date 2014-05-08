// -*- coding: utf-8 -*-
// Copyright (C) 2012-2014 MUJIN Inc. <rosen.diankov@mujin.co.jp>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common.h"
#include "controllerclientimpl.h"
#include "binpickingtaskzmq.h"
#include <boost/property_tree/json_parser.hpp>
#include "mujinzmq.h"

#include <algorithm> // find

namespace mujinclient {

class ZmqMujinControllerClient : public mujinzmq::ZmqClient
{

public:
    ZmqMujinControllerClient(const std::string& host, const int port);

    virtual ~ZmqMujinControllerClient();

    std::string Call(const std::string& msg);
};

ZmqMujinControllerClient::ZmqMujinControllerClient(const std::string& host, const int port) : ZmqClient(host, port)
{
    _InitializeSocket();
}

ZmqMujinControllerClient::~ZmqMujinControllerClient()
{
    // _DestroySocket() is called in  ~ZmqClient()
}

std::string ZmqMujinControllerClient::Call(const std::string& msg)
{
    //send
    //std::cout << "serialization done" << std::endl;
    zmq::message_t request (msg.size());
    memcpy ((void *) request.data (), msg.c_str(), msg.size());
    _socket->send(request);

    //recv
    zmq::message_t reply;

    _socket->recv (&reply);
    std::string replystring((char *) reply.data (), (size_t) reply.size());
    return replystring;
}

BinPickingTaskZmqResource::BinPickingTaskZmqResource(ControllerClientPtr c, const std::string& pk) : BinPickingTaskResource::BinPickingTaskResource(c, pk)
{
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::Initialize(const std::string& robotControllerIp, const int robotControllerPort, const int zmqPort, const int heartbeatPort, const bool initializezmq, const double reinitializetimeout, const double timeout)
{
    _robotControllerIp = robotControllerIp;
    _robotControllerPort = robotControllerPort;
    _zmqPort = zmqPort;
    _heartbeatPort = heartbeatPort;
    _bIsInitialized = true;

    if (initializezmq) {
        InitializeZMQ(reinitializetimeout, timeout);
    }

    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_mujinControllerIp, _zmqPort));
    if (!_zmqmujincontrollerclient) {
        throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
    }
}

boost::property_tree::ptree BinPickingTaskZmqResource::ExecuteCommand(const std::string& command, const double timeout /* [sec] */, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    // TODO need to implement timeout
    boost::property_tree::ptree pt;
    if (getresult) {
        std::stringstream result_ss;
        result_ss << _zmqmujincontrollerclient->Call(command);
        boost::property_tree::read_json(result_ss, pt);
    } else {
        _zmqmujincontrollerclient->Call(command);
    }
    return pt;
}

void BinPickingTaskZmqResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskZmqResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
    }
}


} // end namespace mujinclient
