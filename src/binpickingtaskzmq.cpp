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
#include <boost/property_tree/exceptions.hpp>
#include "mujincontrollerclient/mujinzmq.hpp"

#include <algorithm> // find

namespace mujinclient {

class ZmqMujinControllerClient : public mujinzmq::ZmqClient
{

public:
    ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port);

    virtual ~ZmqMujinControllerClient();

};

ZmqMujinControllerClient::ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port) : ZmqClient(host, port)
{
    _InitializeSocket(context);
}

ZmqMujinControllerClient::~ZmqMujinControllerClient()
{
    // _DestroySocket() is called in  ~ZmqClient()
}

BinPickingTaskZmqResource::BinPickingTaskZmqResource(ControllerClientPtr c, const std::string& pk, const std::string& scenepk) : BinPickingTaskResource::BinPickingTaskResource(c, pk, scenepk)
{
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::Initialize(const std::string& robotControllerUri, const std::string& robotDeviceIOUri,  const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout)
{
    _robotControllerUri = robotControllerUri;
    _robotDeviceIOUri = robotDeviceIOUri;
    _zmqPort = zmqPort;
    _heartbeatPort = heartbeatPort;
    _bIsInitialized = true;
    _zmqcontext = zmqcontext;
    if (initializezmq) {
        InitializeZMQ(reinitializetimeout, timeout);
    }

    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
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

        try{
            result_ss << _zmqmujincontrollerclient->Call(command);
        }
        catch (const MujinException& e) {
            BINPICKINGZMQ_LOG_ERROR(e.what());
            if (e.GetCode() == MEC_ZMQNoResponse) {
                BINPICKINGZMQ_LOG_INFO("reinitializing zmq connection with the slave");
                _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
                if (!_zmqmujincontrollerclient) {
                    throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
                }
            }
        }

        //std::cout << result_ss.str() << std::endl;
        try {
            boost::property_tree::read_json(result_ss, pt);
        } catch (boost::property_tree::ptree_error& e) {
            throw MujinException(boost::str(boost::format("Failed to parse json which is received from mujin controller: \nreceived message: %s \nerror message: %s")%result_ss.str()%e.what()), MEC_Failed);
        }
    } else {
        try{
            _zmqmujincontrollerclient->Call(command);
        }
        catch (const MujinException& e) {
            BINPICKINGZMQ_LOG_ERROR(e.what());
            if (e.GetCode() == MEC_ZMQNoResponse) {
                BINPICKINGZMQ_LOG_INFO("reinitializing zmq connection with the slave");
                _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
                if (!_zmqmujincontrollerclient) {
                    throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
                }
            }
        }
    }
    return pt;
}

void BinPickingTaskZmqResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        if (reinitializetimeout > 0 ) {
            _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskZmqResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
        }
    }
}

void BinPickingTaskZmqResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
    boost::shared_ptr<zmq::socket_t>  socket;
    socket.reset(new zmq::socket_t((*_zmqcontext.get()),ZMQ_SUB));
    std::stringstream ss;
    ss << _heartbeatPort;
    socket->connect (("tcp://"+ _mujinControllerIp+":"+ss.str()).c_str());
    socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);

    zmq::pollitem_t pollitem;
    memset(&pollitem, 0, sizeof(zmq::pollitem_t));
    pollitem.socket = socket->operator void*();
    pollitem.events = ZMQ_POLLIN;
    boost::property_tree::ptree pt;
    BinPickingTaskResource::ResultHeartBeat heartbeat;

    while (!_bShutdownHeartbeatMonitor) {
        unsigned long long lastheartbeat = GetMilliTime();
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(&reply);
                std::string replystring((char *) reply.data (), (size_t) reply.size());
                std::stringstream replystring_ss;
                replystring_ss << replystring;
                // std::cout << "got heartbeat: " << replystring << std::endl;
                try{
                    boost::property_tree::read_json(replystring_ss, pt);
                }
                catch (std::exception const &e) {
                    BINPICKINGZMQ_LOG_ERROR("HeartBeat reply is not JSON");
                    BINPICKINGZMQ_LOG_ERROR(e.what());
                    continue;
                }
                heartbeat.Parse(pt);
                {
                    boost::mutex::scoped_lock lock(_mutexTaskState);
                    _taskstate = heartbeat.taskstate;
                }
                //BINPICKING_LOG_INFO(replystring);

                // std::cout << heartbeat.status << " " << heartbeat.timestamp << " " << heartbeat.msg << std::endl;
                //if ((size_t)reply.size() == 1 && ((char *)reply.data())[0]==255) {
                if (heartbeat.status != std::string("lost") && heartbeat.status.size() > 1) {
                    lastheartbeat = GetMilliTime();
                }
            }
        }
        if (!_bShutdownHeartbeatMonitor) {
            std::stringstream ss;
            ss << (double)((GetMilliTime() - lastheartbeat)/1000.0f) << " seconds passed since last heartbeat signal, re-intializing ZMQ server.";
            BINPICKINGZMQ_LOG_INFO(ss.str());
        }
    }
}



} // end namespace mujinclient
