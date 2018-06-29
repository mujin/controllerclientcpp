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
#include "mujincontrollerclient/mujinzmq.h"

#include <algorithm> // find

#include "logging.h"
#include "mujincontrollerclient/mujinjson.h"

MUJIN_LOGGER("mujin.controllerclientcpp.binpickingtask.zmq");

using namespace mujinzmq;

namespace mujinclient {
using namespace utils;
namespace mujinjson = mujinjson_external;
using namespace mujinjson;

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

BinPickingTaskZmqResource::BinPickingTaskZmqResource(ControllerClientPtr c, const std::string& pk, const std::string& scenepk, const std::string& tasktype) : BinPickingTaskResource(c, pk, scenepk, tasktype)
{
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::Initialize(const std::string& defaultTaskParameters,  const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout, const std::string& userinfo, const std::string& slaverequestid)
{
    BinPickingTaskResource::Initialize(defaultTaskParameters, zmqPort, heartbeatPort, zmqcontext, initializezmq, reinitializetimeout, timeout, userinfo, slaverequestid);
    
    if (initializezmq) {
        InitializeZMQ(reinitializetimeout, timeout);
    }

    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
    if (!_zmqmujincontrollerclient) {
        throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
    }
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        if (reinitializetimeout > 0 ) {
            _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskZmqResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
        }
    }
}

void BinPickingTaskZmqResource::ExecuteCommand(const std::string& taskparameters, rapidjson::Document &pt, const double timeout /* [sec] */, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    if (!_zmqmujincontrollerclient) {
        MUJIN_LOG_ERROR("zmqcontrollerclient is not initialized! initialize");
        _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
    }

    std::stringstream ss;
    ss << "{\"fnname\": \"";
    if (_tasktype == "binpicking") {
        ss << "binpicking.";
    }
    ss << "RunCommand\", \"taskparams\": {\"tasktype\": \"" << _tasktype << "\", ";

    ss << "\"taskparameters\": " << taskparameters << ", ";
    ss << "\"sceneparams\": " << _sceneparams_json << "}, ";
    ss << "\"userinfo\": " << _userinfo_json;
    if (_slaverequestid != "") {
        ss << ", " << GetJsonString("slaverequestid", _slaverequestid);
    }
    ss << "}";
    std::string command = ss.str();
    //    std::cout << "Sending " << command << " from " << __func__ << std::endl;

    //std::string task = "{\"tasktype\": \"binpicking\", \"taskparameters\": " + command + "}";
    if (getresult) {
        std::stringstream result_ss;

        try{
            result_ss << _zmqmujincontrollerclient->Call(command, timeout);
        }
        catch (const MujinException& e) {
            MUJIN_LOG_ERROR(e.what());
            if (e.GetCode() == MEC_ZMQNoResponse) {
                MUJIN_LOG_INFO("reinitializing zmq connection with the slave");
                _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
                if (!_zmqmujincontrollerclient) {
                    throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
                }
            } else if (e.GetCode() == MEC_Timeout) {
                std::string errstr;
                if (taskparameters.size()>1000) {
                    errstr = taskparameters.substr(0, 1000);
                } else {
                    errstr = taskparameters;
                }
                throw MujinException(boost::str(boost::format("Timed out receiving response of command with taskparameters=%s...")%errstr));
            }
        }

        ParseJson(pt, result_ss.str());
        if( pt.IsObject() && pt.HasMember("error")) {
            std::string error = GetJsonValueByKey<std::string>(pt["error"], "errorcode");
            std::string description = GetJsonValueByKey<std::string>(pt["error"], "description");
            if ( error.size() > 0 ) {
                std::string serror;
                if ( description.size() > 0 ) {
                    serror = description;
                }
                else {
                    serror = error;
                }
                if( serror.size() > 1000 ) {
                    MUJIN_LOG_ERROR(str(boost::format("truncated original error message from %d")%serror.size()));
                    serror = serror.substr(0,1000);
                }
                throw MujinException(str(boost::format("Error when calling binpicking.RunCommand: %s")%serror), MEC_BinPickingError);
            }
        }
    } else {
        try{
            _zmqmujincontrollerclient->Call(command, timeout);
            // TODO since not getting response, internal zmq will be in a bad state, have to recreate
        }
        catch (const MujinException& e) {
            MUJIN_LOG_ERROR(e.what());
            if (e.GetCode() == MEC_ZMQNoResponse) {
                MUJIN_LOG_INFO("reinitializing zmq connection with the slave");
                _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
                if (!_zmqmujincontrollerclient) {
                    throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
                }
            } else if (e.GetCode() == MEC_Timeout) {
                std::string errstr;
                if (command.size()>1000) {
                    errstr = command.substr(0, 1000);
                } else {
                    errstr = command;
                }
                throw MujinException(boost::str(boost::format("Timed out receiving response of command with taskparameters=%s...")%errstr));
            }
        }
    }
}

void BinPickingTaskZmqResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
}

void BinPickingTaskZmqResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
    boost::shared_ptr<zmq::socket_t>  socket;
    boost::property_tree::ptree pt;
    BinPickingTaskResource::ResultHeartBeat heartbeat;
    heartbeat._slaverequestid = _slaverequestid;
    while (!_bShutdownHeartbeatMonitor) {
        if (!!socket) {
            socket->close();
            socket.reset();
        }
        socket.reset(new zmq::socket_t((*_zmqcontext.get()),ZMQ_SUB));
        std::stringstream ss;
        ss << _heartbeatPort;
        socket->connect (("tcp://"+ _mujinControllerIp+":"+ss.str()).c_str());
        socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);

        zmq::pollitem_t pollitem;
        memset(&pollitem, 0, sizeof(zmq::pollitem_t));
        pollitem.socket = socket->operator void*();
        pollitem.events = ZMQ_POLLIN;
        unsigned long long lastheartbeat = GetMilliTime();
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(&reply);
                std::string replystring((char *) reply.data (), (size_t) reply.size());
                rapidjson::Document pt(rapidjson::kObjectType);
                try{
                    std::stringstream replystring_ss(replystring);
                    ParseJson(pt, replystring_ss.str());
                    heartbeat.Parse(pt);
                    {
                        boost::mutex::scoped_lock lock(_mutexTaskState);
                        _taskstate = heartbeat.taskstate;
                    }
                    //BINPICKING_LOG_ERROR(replystring);
                    
                    if (heartbeat.status != std::string("lost") && heartbeat.status.size() > 1) {
                        lastheartbeat = GetMilliTime();
                    }
                }
                catch (std::exception const &e) {
                    MUJIN_LOG_ERROR("HeartBeat reply is not JSON");
                    MUJIN_LOG_ERROR(e.what());
                    continue;
                }
            }
        }
        if (!_bShutdownHeartbeatMonitor) {
            std::stringstream ss;
            ss << (double)((GetMilliTime() - lastheartbeat)/1000.0f) << " seconds passed since last heartbeat signal, re-intializing ZMQ server.";
            MUJIN_LOG_INFO(ss.str());
        }
    }
}

    

} // end namespace mujinclient
