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

#include "binpickingtaskzmq.h"
#include <boost/property_tree/json_parser.hpp>
#include "mujinzmq.h"

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

BinPickingTaskZmq::BinPickingTaskZmq(const std::string& taskname, const std::string& robotcontrollerip, const int robotcontrollerport, ControllerClientPtr controller, SceneResourcePtr scene, const std::string& host, const int port) : BinPickingTaskResource::BinPickingTaskResource(taskname, robotcontrollerip, robotcontrollerport, controller, scene)
{
    tasktype = MUJIN_BINPICKING_TASKTYPE_ZMQ;
    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(host, port));
    if (!_zmqmujincontrollerclient) {
        throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%host%port), MEC_Failed);
    }
}

BinPickingTaskZmq::~BinPickingTaskZmq()
{
}

boost::property_tree::ptree BinPickingTaskZmq::ExecuteCommand(const std::string& command, const int timeout /* [sec] */, const bool getresult)
{
    boost::property_tree::ptree pt;
    if (getresult) {
        boost::property_tree::read_json(_zmqmujincontrollerclient->Call(command), pt);
    } else {
        _zmqmujincontrollerclient->Call(command);
    }
    return pt;
}

} // end namespace mujinclient
